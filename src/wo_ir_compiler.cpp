#include "wo_afx.hpp"

#include "wo_ir_compiler.hpp"

namespace wo
{
    IRFunction::IRFunction(IRCompiler* c, woort_IRFunction* irfunc)
        : m_ircompiler(c)
        , m_irfunction(irfunc)
    {
    }

    IRCompiler::IRCompiler()
    {
        m_ircompiler = woort_IRCompiler_create();

        // Assure nullptr same as NULL.
        static_assert(nullptr == NULL);
    }
    IRCompiler::~IRCompiler()
    {
        if (m_ircompiler != nullptr)
            abondon();        
    }

    void IRCompiler::abondon()
    {
        wo_assert(m_ircompiler != nullptr);

        woort_IRCompiler_close(m_ircompiler);
        m_ircompiler = nullptr;
    }
}