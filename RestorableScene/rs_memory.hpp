#pragma once
#include <cstdlib>

extern "C"
{
    void* _rs_aligned_alloc(size_t allocsz, size_t allign);
    void _rs_aligned_free(void* memptr);
}


namespace rs
{
    void alloc64(size_t memsz)
    {
        return malloc();
    }
}