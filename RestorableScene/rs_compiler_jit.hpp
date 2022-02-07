#pragma once

#include "rs_basic_type.hpp"

namespace rs
{
    struct vmbase;

    struct jit_compiler_x86
    {
        using jit_packed_function = void(*)(vmbase*, value*, value*, value*);
                                    //       vmptr     bp      reg    const_global

        static jit_packed_function compile_jit(const byte_t* rt_ip);
    };

}