#include "wo_afx.hpp"

#define WOMEM_IMPL

#include "woomem.h"
#include "wo_memory.hpp"

namespace wo
{
    void* gc_alloc(size_t memsz)
    {
        do
        {
            void* addr = woomem_alloc_attrib(memsz, WOOMEM_GC_UNIT_TYPE_NEED_SWEEP);
            if (addr != nullptr)
                return addr;

            // Alloc failed, retry.
            // 
            // TODO;
            abort();

        } while (1);
    }
}
