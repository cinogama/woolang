#include <cstdlib>
#include <cstdint>

#include "wo_assert.hpp"

#define WO_NOT_ALLIGN 1

// From https://www.cnblogs.com/deatharthas/p/14502337.html

void* _wo_aligned_alloc(size_t allocsz, size_t allign)
{
    wo_assert(allocsz != 0);

#if WO_NOT_ALLIGN
    void* buf = malloc(allocsz + sizeof(size_t));
    *(size_t*)buf = (size_t)0x1234567887654321;
    return (void*)((size_t*)buf + 1);
#else
    size_t offset = allign - 1 + sizeof(void*);
    void* originalP = malloc(allocsz + offset);
    size_t originalLocation = reinterpret_cast<size_t>(originalP);
    size_t realLocation = (originalLocation + offset) & ~(allign - 1);
    void* realP = reinterpret_cast<void*>(realLocation);
    size_t originalPStorage = realLocation - sizeof(void*);
    *reinterpret_cast<void**>(originalPStorage) = originalP;
    return realP;
#endif //  WO_NOT_ALLIGN
}
void _wo_aligned_free(void* memptr)
{
#if WO_NOT_ALLIGN
    if (*((size_t*)memptr - 1) != (size_t)0x1234567887654321)
        wo_error("Freed a memory not belong to woolang or double free.");
    *((size_t*)memptr - 1) = (size_t)-1;
    return free((size_t*)memptr - 1);
#else
    size_t originalPStorage = reinterpret_cast<size_t>(memptr) - sizeof(void*);
    void** p = reinterpret_cast<void**>(originalPStorage);
    void* p2free = *p;
    *p = (void*)(~(intptr_t)p2free); // Debug flag.
    free(p2free);
#endif //  WO_NOT_ALLIGN
}
