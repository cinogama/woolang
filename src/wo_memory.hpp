#pragma once
#include "wo_global_setting.hpp"

#include <cstdlib>
#include <cstddef>
#include <new>

void* _wo_aligned_alloc(size_t allocsz, size_t allign);
void _wo_aligned_free(void* memptr);

namespace wo
{
    inline void* alloc64(size_t memsz)
    {
        return _wo_aligned_alloc(memsz, platform_info::CPU_CACHELINE_SIZE);
    }
    inline void free64(void* ptr)
    {
        _wo_aligned_free(ptr);
    }
}