#pragma once

#include "woort.h"

#include <unordered_map>

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

        // When true, mutable static (global / static-lifecycle) variables
        // allocated during IR generation use pvalue-indirect storage so that
        // closure FAR CALLs across REPL CodeEnvs share the same heap box.
        bool m_pvalue_indirect_for_mutable_statics = false;

        // Track whether builtins have been registered (only once per session).
        bool m_builtin_types_registered = false;
    };
}
#endif
