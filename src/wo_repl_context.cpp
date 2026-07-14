#include "wo_afx.hpp"

#include "wo_repl_context.hpp"

#ifndef WO_DISABLE_COMPILER
namespace wo
{
    REPLContext::REPLContext()
        : m_pvalue_indirect_for_mutable_statics(false)
    {
    }

    void REPLContext::clean_emitted_script_funcs()
    {
        m_new_emitted_script_functions.clear();
    }

    void REPLContext::emit_script_func(ast::AstValueFunction* func)
    {
        m_new_emitted_script_functions.push_back(func);
    }
}
#endif
