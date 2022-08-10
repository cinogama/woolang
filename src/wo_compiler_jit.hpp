#pragma once

#include "wo_basic_type.hpp"
#include "wo_compiler_ir.hpp"

namespace wo
{
    struct asmjit_jit_context;
    struct jit_compiler_x64
    {
        using jit_packed_func_t = wo_native_func; // x(vm, bp + 2, argc/* useless */)

        asmjit_jit_context* m_asmjit_context;

        jit_compiler_x64() noexcept;
        ~jit_compiler_x64() noexcept;

        void analyze_function(const byte_t* codebuf, runtime_env* env) noexcept;
        void analyze_jit(byte_t* codebuf, runtime_env* env) noexcept;
    };
}

#if WO_ENABLE_ASMJIT

namespace wo
{
    struct vmbase;

    struct jit_compiler_x86
    {
        using jit_packed_function = void(*)(vmbase*, value*, value*, value*);
                                    //       vmptr     bp      reg     cr

        static jit_packed_function compile_jit(const byte_t* rt_ip, vmbase* compile_vmptr);
    };

}

#endif