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

    // Session-level string pool guard: keeps all wo_pstring_t values alive.
    std::unique_ptr<wo::start_string_pool_guard> m_string_pool_guard;

    // Session-level AST arena management.
    //   At construction, we swap in a fresh thread-local AST allocator so that
    //   AST nodes from every REPL line persist for the whole session (needed
    //   for template instantiation, error "defined here" pointers, etc.).
    wo::ast::AstAllocator m_previous_ast_context;
    bool m_need_restore_ast;

    // Persistent VM: survives across evaluations, holds runtime state.
    woort_vm* m_vm;

    // A session binding is a top-level variable whose value persists.
    struct SessionBinding
    {
        wo_pstring_t m_name;
        wo::lang_Symbol* m_symbol;
        wo::lang_TypeInstance* m_type;
        woort_Value m_value;
        // The static-slot index assigned in the *current* line's CodeEnv.
        woort_IRStaticIndex m_current_static_idx;
    };
    std::vector<SessionBinding> m_bindings;

    // CodeEnvs kept alive (m_hold stays true) so that function closures
    // defined in prior lines remain callable.
    std::vector<woort_CodeEnv*> m_cenv_history;

    size_t m_line_counter;
    size_t m_repl_seq_num;

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
