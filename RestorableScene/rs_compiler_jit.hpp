#pragma once

#include "rs_basic_type.hpp"

namespace rs
{
    struct vmbase;

    struct jit_compiler_x86
    {
        using jit_packed_function = void(*)(vmbase*, value*, value*);
                                    //       vmptr     bp      reg

        static jit_packed_function compile_jit(const byte_t* rt_ip, vmbase* compile_vmptr);
    };

}