#pragma once

#include "woort.h"

#include <optional>

namespace wo
{
    class IRCompiler;

    class IRFunction
    {
        IRCompiler* const m_ircompiler;
        /* OPTIONAL */ woort_IRFunction* m_irfunction;

        friend class IRCompiler;

    public:
        IRFunction(IRCompiler* c, /* OPTIONAL */ woort_IRFunction* irfunc);
        IRFunction(const IRFunction&) = default;
        IRFunction(IRFunction&&) = default;
        IRFunction& operator =(const IRFunction&) = default;
        IRFunction& operator =(IRFunction&&) = default;

    public:
        /*
        NOTE: 不可将值写入 load_constant 返回的 woort_IRValue 中
        */
        woort_IRValue* load_constant(woort_IRConstantIndex cidx);
        woort_IRValue* alloc_value();
        woort_IRValue* argument(uint32_t aidx);
    };

    class IRCompiler
    {
        /* OPTIONAL */ woort_IRCompiler* m_ircompiler;

        IRCompiler(const IRCompiler&) = delete;
        IRCompiler(IRCompiler&&) = delete;
        IRCompiler& operator = (const IRCompiler&) = delete;
        IRCompiler& operator = (IRCompiler&&) = delete;

    public:
        IRCompiler();
        ~IRCompiler();

        void abondon();

    public:
        bool is_abondoned() const;

        IRFunction add_function(uint32_t param_count);

        woort_IRConstantIndex alloc_constant();
        woort_IRStaticIndex alloc_static();
    };
} 