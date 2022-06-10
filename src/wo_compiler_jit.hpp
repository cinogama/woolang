#pragma once

#include "wo_basic_type.hpp"

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