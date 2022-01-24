#pragma once
#include <cstdlib>
#include <new>

void* _rs_aligned_alloc(size_t allocsz, size_t allign);
void _rs_aligned_free(void* memptr);

namespace rs
{
    inline void* alloc64(size_t memsz)
    {
        return _rs_aligned_alloc(memsz, std::hardware_constructive_interference_size);
    }
    inline void free64(void* ptr)
    {
        _rs_aligned_free(ptr);
    }
}