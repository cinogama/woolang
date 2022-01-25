#pragma once
#include "rs_global_setting.hpp"

#include <cstdlib>
#include <new>

void* _rs_aligned_alloc(size_t allocsz, size_t allign);
void _rs_aligned_free(void* memptr);

namespace rs
{
    inline void* alloc64(size_t memsz)
    {
        return _rs_aligned_alloc(memsz, platform_info::CPU_CACHELINE_SIZE);
    }
    inline void free64(void* ptr)
    {
        _rs_aligned_free(ptr);
    }
}