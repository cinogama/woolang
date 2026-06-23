#include "wo_afx.hpp"

#include "wo_repl.hpp"
#include "wo_lang_grammar_loader.hpp"
#include "wo_source_file_manager.hpp"

#include <cstring>
#include <sstream>
#include <unordered_set>

// ===================================================================
//  Lifecycle
// ===================================================================

_wo_ReplSession::_wo_ReplSession()
    : m_vm(nullptr)
    , m_line_counter(0)
    , m_repl_seq_num(0)
    , m_repl_group_token(nullptr)
{
    // 1. Session-level string pool: all wo_pstring_t values stay valid.
    m_string_pool_guard = std::make_unique<wo::start_string_pool_guard>();

    // Allocate the session-stable logical source identity. Every REPL eval
    // carries this in source_location.source_group (not source_file, which
    // holds the unique per-snippet VFS path) so compiler-semantic
    // mechanisms (using-namespace, PRIVATE access, import visibility) treat
    // all snippets as the same file. The session pointer disambiguates
    // concurrent sessions in the same process.
    {
        char group_buf[48];
        (void)snprintf(group_buf, sizeof(group_buf), "<Repl[%p]>", this);
        m_repl_group_token = wo::wstring_pool::get_pstr(group_buf);
    }

    // 2. Session-level AST arena: install a fresh thread-local allocator
    //    so AST nodes persist across lines (save previous for restore).
    m_need_restore_ast =
        wo::ast::AstBase::exchange_this_thread_ast(m_previous_ast_context);

    // 3. Persistent compiler state.
    m_lang_context = std::make_unique<wo::LangContext>();

    // Pre-register builtin types at session creation so they are always
    // present before any wo_repl_eval snapshot/rollback.
    m_lang_context->pass_0_5_register_builtin_types();
    m_lang_context->m_builtin_types_registered = true;

    // 4. Persistent VM.
    m_vm = woort_vm_create();
}

_wo_ReplSession::~_wo_ReplSession()
{
    for (size_t i = 0; i <= m_repl_seq_num; ++i)
    {
        char repl_vfs_path[64];
        (void)snprintf(
            repl_vfs_path,
            sizeof(repl_vfs_path),
            "<Repl[%p]:%zu>",
            this,
            i);
        (void)woort_vfs_remove(repl_vfs_path);
    }

    // Drop all session CodeEnvs.
    for (woort_CodeEnv* cenv : m_cenv_history)
        woort_codeenv_drop(cenv);
    m_cenv_history.clear();

    // Destroy VM (swap out first to avoid GC deadlock).
    if (m_vm != nullptr)
    {
        woort_vm_close(m_vm);
        m_vm = nullptr;
    }

    // Destroy LangContext (releases symbol table, type table, etc.).
    m_lang_context.reset();

    // Clean session AST arena.
    wo::ast::AstBase::clean_this_thread_ast();
    if (m_need_restore_ast)
        wo::ast::AstBase::exchange_this_thread_ast(m_previous_ast_context);

    // End session string pool.
    m_string_pool_guard.reset();
}

// ===================================================================
//  Internal helpers
// ===================================================================

static void rollback_new_symbols(
    wo::lang_Scope* root_scope,
    const std::unordered_set<wo_pstring_t>& known_names)
{
    auto& syms = root_scope->m_defined_symbols;
    for (auto it = syms.begin(); it != syms.end(); )
    {
        if (known_names.find(it->first) == known_names.end())
            it = syms.erase(it);
        else
            ++it;
    }
}

// ===================================================================
//  Public API
// ===================================================================

wo_ReplSession* wo_repl_create(void)
{
    auto* s = new (std::nothrow) _wo_ReplSession();
    if (s == nullptr || s->m_vm == nullptr || s->m_lang_context == nullptr)
    {
        delete s;
        return nullptr;
    }
    return reinterpret_cast<wo_ReplSession*>(s);
}

void wo_repl_destroy(wo_ReplSession* session)
{
    delete reinterpret_cast<_wo_ReplSession*>(session);
}

wo_repl_result wo_repl_eval(
    wo_ReplSession* session,
    woort_U8CString src,
    wo_CompileErrors** out_errors)
{
    if (session == nullptr || src == nullptr)
        return WO_REPL_COMPILE_ERROR;

    auto* S = reinterpret_cast<_wo_ReplSession*>(session);
    auto* lc = S->m_lang_context.get();

    // --- 1. Reset IR context (fresh IRCompiler for this line) ---
    lc->m_ircontext.reset();

    // --- 2. Reserve static slots for carried-over values ---
    // Re-create the carried-over static slot range [0..N-1] in the new
    // IRCompiler so that symbols from prior evals (which retain their
    // m_IR_storage indices) point to valid slots. New statics allocated
    // during this line's compilation will get indices >= N.
    const size_t carry_count = S->m_static_values.size();
    for (size_t i = 0; i < carry_count; ++i)
        lc->m_ircontext.c().alloc_static();

    // --- 3. Reset scope stack to root ---
    while (lc->m_scope_stack.size() > 1)
        lc->m_scope_stack.pop();
    auto* root_ns = lc->m_root_namespace.get();
    lc->m_scope_stack.top() = root_ns->m_this_scope.get();
    auto* root_scope = root_ns->m_this_scope.get();

    // --- 4. Snapshot root scope symbol names (for rollback) ---
    std::unordered_set<wo_pstring_t> known_names;
    for (const auto& [name, _] : root_scope->m_defined_symbols)
        known_names.insert(name);

    // --- 5. Create lexer from source string ---
    // Per-eval VFS path (unique per line) so error rendering can show the
    // exact snippet. Same-file semantics across evals is handled via the
    // session-stable `source_group` token passed below, not via this path.
    char repl_vfs_path[64];
    (void)snprintf(
        repl_vfs_path, 
        sizeof(repl_vfs_path), 
        "<Repl[%p]:%zu>",
        session,
        session->m_repl_seq_num);
    
    // Register the current snippet in the VFS so that
    // wo_get_compile_error() can read it back and render the underlined
    // source span. Mirrors wo_load_binary(); enable_modify=true lets each
    // eval overwrite the previous entry in place.
    (void)woort_vfs_create(
        repl_vfs_path, src, std::strlen(src), /*enable_modify=*/true);

    wo_pstring_t path_pstr = wo::wstring_pool::get_pstr(
        std::string(WOORT_VFS_SCHEME) + repl_vfs_path);

    auto source_stream = std::make_unique<std::istringstream>(
        std::string(src));

    auto lex = std::make_unique<wo::lexer>(
        std::nullopt,
        path_pstr,
        std::optional<std::unique_ptr<std::istream>>(std::move(source_stream)),
        S->m_repl_group_token);

    // Inject known imports from prior lines (so stdlib etc. stays visible).
    lex->register_imported_sources(S->m_known_imports);

    // --- 6. Parse ---
    wo::ast::AstBase* ast_root = nullptr;
    bool is_incomplete = false;

    if (!lex->has_error())
    {
        ast_root = wo::get_grammar_instance()->gen(*lex, &is_incomplete);
        lex->drop_macro_vm_and_code_env();
    }

    if (ast_root == nullptr)
    {
        if (is_incomplete)
        {
            // Input is syntactically incomplete — parser hit EOF in a state
            // where EOF is not in the follow set. Wait for more input.
            lex.reset();
            return WO_REPL_INCOMPLETE_INPUT;
        }
    }

    // --- 7. Run compiler pipeline (pass0 -> pass1 -> passir) ---
    if (ast_root == nullptr 
        || lc->process(*lex, ast_root) != wo::compile_result::PROCESS_OK)
    {
        if (out_errors)
            *out_errors = _wo_make_compile_errors(std::move(lex));
        else
            lex.reset();

        rollback_new_symbols(root_scope, known_names);
        return WO_REPL_COMPILE_ERROR;
    }

    // --- Harvest imports from this line for future lines ---
    for (wo_pstring_t p : lex->get_linked_script_paths())
        S->m_known_imports.insert(p);

    // On success we can drop the lexer.
    lex.reset();

    // --- 8. Finalize -> CodeEnv ---
    auto cenv_opt = lc->m_ircontext.finalize();
    if (!cenv_opt.has_value())
    {
        rollback_new_symbols(root_scope, known_names);
        return WO_REPL_OUT_OF_MEMORY;
    }
    woort_CodeEnv* const cenv = cenv_opt.value();

    // --- 9. Inject carried-over static values into the new CodeEnv ---
    // Copy the saved snapshot into slots [0..N-1] so that symbols from
    // prior evals find their persisted values.
    if (carry_count > 0)
    {
        woort_CodeEnv_lock(cenv);
        for (size_t i = 0; i < carry_count; ++i)
            woort_CodeEnv_set_static_value(cenv, (woort_IRStaticIndex)i, &S->m_static_values[i]);
        woort_CodeEnv_unlock(cenv);
    }

    // --- 10. Boot the CodeEnv on the session VM ---
    woort_VMRuntime* const last_vm = woort_vm_swap(S->m_vm);

    woort_value v;
    woort_VmCallStatus status = WOORT_VM_CALL_STATUS_NORMAL;
    if (!woort_push_reserve(1, &v))
    {
        woort_panic(WOORT_PANIC_STACK_OVERFLOW, "Stack overflow.");
        (void)woort_vm_swap(last_vm);

        woort_codeenv_drop(cenv);
        return WO_REPL_RUNTIME_ERROR;
    }

    status = woort_bootup_codeenv(v, cenv);
    woort_pop(1);

    (void)woort_vm_swap(last_vm);

    if (status != WOORT_VM_CALL_STATUS_NORMAL)
    {
        // Runtime panic: the VM's ABORT flag is now sticky.
        woort_codeenv_drop(cenv);
        return WO_REPL_RUNTIME_ERROR;
    }

    // --- 11. Snapshot ALL static values from the booted CodeEnv ---
    // Read back every static slot (including newly declared ones) so the
    // full state is available for the next eval. Symbols retain their
    // m_IR_storage across evals — no clearing needed.
    woort_CodeEnv_lock(cenv);
    {
        const size_t total = woort_CodeEnv_get_static_storage_count(cenv);
        S->m_static_values.resize(total);
        for (size_t i = 0; i < total; ++i)
            woort_CodeEnv_get_static_value(cenv, (woort_IRStaticIndex)i, &S->m_static_values[i]);
    }
    woort_CodeEnv_unlock(cenv);

    // --- 12. Keep CodeEnv alive (for function closures) ---
    S->m_cenv_history.push_back(cenv);
    ++session->m_repl_seq_num;

    return WO_REPL_OK;
}
