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
    };
} 