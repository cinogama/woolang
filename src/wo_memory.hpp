#pragma once
#include "wo_global_setting.hpp"

#include <cstdlib>
#include <cstddef>
#include <new>

namespace wo
{
    inline void* alloc64(size_t memsz)
    {
        return malloc(memsz);
    }
    inline void free64(void* ptr)
    {
        free(ptr);
    }
}