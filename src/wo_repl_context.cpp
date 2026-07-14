#include "wo_afx.hpp"

#include "wo_repl_context.hpp"

#ifndef WO_DISABLE_COMPILER
namespace wo
{
    REPLContext::REPLContext()
        : m_pvalue_indirect_for_mutable_statics(false)
        , m_builtin_types_registered(false)
    {
    }

    void REPLContext::clean_emitted_script_funcs()
    {
        m_emitted_script_functions_for_REPL.clear();
    }

    void REPLContext::emit_script_func(ast::AstValueFunction* func)
    {
        m_emitted_script_functions_for_REPL.push_back(func);
    }
}
#endif
