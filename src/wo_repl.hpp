#pragma once

#include "wo_lang.hpp"
#include "wo_compiler_lexer.hpp"
#include "wo_compiler_parser.hpp"
#include "wo_const_string_pool.hpp"

#include <optional>
#include <unordered_set>
#include <vector>

// Factory declared in wo_api_impl.cpp, used to create wo_CompileErrors from a lexer.
wo_CompileErrors* _wo_make_compile_errors(std::optional<std::unique_ptr<wo::lexer>> lex);

struct _wo_ReplSession
{
    // Persistent compiler state: accumulates symbols/types across evaluations.
    std::unique_ptr<wo::LangContext> m_lang_context;

    // REPL-owned string pool. Detached from thread-local between evals;
    // installed via repl_tls_guard at each wo_repl_eval entry so that the
    // caller's TLS is not held for the session lifetime. Keeps all
    // wo_pstring_t values alive across the whole session.
    wo::wstring_pool* m_repl_pool = nullptr;

    // REPL-owned AST arena. Accumulates AST nodes across evals; installed
    // into thread-local via repl_tls_guard and extracted back on exit.
    // Needs to persist for template instantiation, error "defined here"
    // pointers, etc.
    wo::ast::AstAllocator m_repl_ast_arena;

    // Persistent VM: survives across evaluations, holds runtime state.
    woort_vm* m_vm;

    // Flat snapshot of ALL static storage slots from the last booted CodeEnv.
    // m_static_values[i] is the runtime value at woort_IRStaticIndex i.
    // Preserved across evals by bulk-reserving slots [0..N-1] in each new
    // IRCompiler, injecting the saved values into the new CodeEnv, then
    // re-snapshoting after boot. This uniformly handles globals, namespace-
    // scoped variables, and function-local statics without per-symbol
    // tracking: symbols retain their m_IR_storage across evals, and the
    // reserved range keeps their indices valid.
    std::vector<woort_Value> m_static_values;

    // CodeEnvs kept alive (m_hold stays true) so that function closures
    // defined in prior lines remain callable.
    std::vector<woort_CodeEnv*> m_cenv_history;

    size_t m_line_counter;
    size_t m_repl_seq_num;

    // Session-stable logical source identity. Every REPL eval's lexer is
    // constructed with this as `source_group`, while each gets a unique
    // `source_path` ("<repl N @ ptr>") for VFS source rendering. This way
    // every REPL snippet shares one semantic identity, so the compiler's
    // source-file-based mechanisms (using-namespace map, PRIVATE access
    // check, import visibility) work across evals as if they were in the
    // same file.
    wo_pstring_t m_repl_group_token;

    // All source paths imported by prior lines (stdlib, etc.).
    // Each new line inherits these so imports persist across the session.
    std::unordered_set<wo_pstring_t> m_known_imports;

    _wo_ReplSession();
    ~_wo_ReplSession();

    _wo_ReplSession(const _wo_ReplSession&) = delete;
    _wo_ReplSession(_wo_ReplSession&&) = delete;
    _wo_ReplSession& operator=(const _wo_ReplSession&) = delete;
    _wo_ReplSession& operator=(_wo_ReplSession&&) = delete;
};
