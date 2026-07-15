#pragma once

#include "woort.h"

#include <unordered_map>
#include <vector>

#ifndef WO_DISABLE_COMPILER
namespace wo
{
    namespace ast
    {
        struct AstValueFunction;
    }

    struct REPLContext
    {
        // Maps script functions emitted in a prior eval to their bytecode
        // entry point in the prior CodeEnv. NOT cleared between REPL evals,
        // so it persists for the entire session lifetime. Empty in non-REPL
        // mode, so all lookups are no-ops there.
        std::unordered_map<ast::AstValueFunction*, const woort_Bytecode*>
            m_prior_function_bytecode;

        // All script functions actually emitted (push_function) in the
        // current compile session. Transient scratch: cleared at the start
        // of every REPL eval (see clean_emitted_script_funcs). Used by
        // IRCompiler::commit() to record their bytecode into
        // m_prior_function_bytecode, regardless of whether they also appear
        // in an immediate pool.
        std::vector<ast::AstValueFunction*> m_new_emitted_script_functions;

        // When true, mutable static (global / static-lifecycle) variables
        // allocated during IR generation use pvalue-indirect storage so that
        // closure FAR CALLs across REPL CodeEnvs share the same heap box.
        bool m_pvalue_indirect_for_mutable_statics;

        // Pointer to the session-owned REPL printer. The IR generated for
        // AstEchoForREPL passes this as the first argument to the
        // woostd_repl_print_* native functions so that echoed bare-expression
        // results are buffered here instead of written directly to stdout.
        // nullptr in non-REPL mode; set by _wo_ReplSession construction.
        woort_REPLPrinter* m_repl_printer;

        REPLContext();
        ~REPLContext() = default;

        // Reset the per-eval transient scratch. Called at the start of every
        // REPL eval (and also drops stale pointers from a failed prior eval
        // whose compilation aborted before reaching commit()).
        void clean_emitted_script_funcs();

        // Record a script function emitted in the current compile session.
        void emit_script_func(ast::AstValueFunction* func);
    };
}
#endif
