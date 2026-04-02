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

    bool IRCompiler::is_abondoned() const
    {
        return m_ircompiler == nullptr;
    }

    IRFunction IRCompiler::add_function(uint32_t param_count)
    {
        if (m_ircompiler == nullptr)
            // IR 系列接口的设计原则是：压制内存申请失败，如同一切正常——直到最终提交代码时
            // 以失败结束
            return IRFunction(this, nullptr);

        woort_IRFunction* irfunc;
        if (!woort_IRCompiler_add_function(m_ircompiler, param_count, &irfunc))
        {
            abondon();
            return IRFunction(this, nullptr);
        }
        return IRFunction(this, irfunc);
    }

    woort_IRConstantIndex IRCompiler::alloc_constant()
    {
        if (m_ircompiler == nullptr)
            return 0;

        return woort_IRCompiler_add_constant(m_ircompiler);
    }

    woort_IRStaticIndex IRCompiler::alloc_static()
    {
        if (m_ircompiler == nullptr)
            return 0;

        return woort_IRCompiler_add_static(m_ircompiler);
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////

    woort_IRValue* IRFunction::load_constant(woort_IRConstantIndex cidx)
    {
        if (m_ircompiler->is_abondoned())
            return nullptr;

        woort_IRValue* const result = woort_IRFunction_load_const(m_irfunction, cidx);
        if (result == nullptr)
            m_ircompiler->abondon();
        
        return result;
    }

    woort_IRValue* IRFunction::new_value()
    {
        if (m_ircompiler->is_abondoned())
            return nullptr;

        woort_IRValue* const result = woort_IRFunction_new_vreg(m_irfunction);
        if (result == nullptr)
            m_ircompiler->abondon();

        return result;
    }

    woort_IRValue* IRFunction::argument(uint32_t aidx)
    {
        if (m_ircompiler->is_abondoned())
            return nullptr;

        woort_IRValue* const result = woort_IRFunction_get_argument(m_irfunction, aidx);
        if (result == nullptr)
            m_ircompiler->abondon();

        return result;
    }

    woort_IRLabel* IRFunction::new_label()
    {
        if (m_ircompiler->is_abondoned())
            return nullptr;

        woort_IRLabel* const result = woort_IRFunction_new_label(m_irfunction);
        if (result == nullptr)
            m_ircompiler->abondon();

        return result;
    }

    void IRFunction::mov(woort_IRValue* dst, woort_IRValue* src)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_MOV(m_irfunction, dst, src))
            m_ircompiler->abondon();
    }

    void IRFunction::load(woort_IRValue* dst, woort_IRStaticIndex src)
    {
        if (m_ircompiler->is_abondoned())
            return;

        if (!woort_IR_LOAD(m_irfunction, dst, src))
            m_ircompiler->abondon();
    }
}