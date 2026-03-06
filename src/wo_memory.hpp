#pragma once

#include "woomem.h"
#include "wo_global_setting.hpp"

#include <stdint.h>
#include <stddef.h>

namespace wo
{
    namespace mem
    {
        void init(void);
        void shutdown(void);

        struct GCUnitProxy
        {
            using MarkingCallback = void(*)(GCunitBase*);
            using DestroyCallback = void(*)(GCunitBase*);

            /* OPTIONAL */ MarkingCallback m_marker;
            /* OPTIONAL */ DestroyCallback m_destroier;
        };
        struct GCunitBase
        {
            const GCUnitProxy* const m_proxy;
            GCunitBase(const GCUnitProxy* proxy)
                : m_proxy(proxy)
            {
            }
        };

        // Retry if allocate failed, never return nullptr;
        void* gc_alloc(size_t memsz, int attrib);
    }
}
