#pragma once
#include "wo_global_setting.hpp"

#include <cstdlib>
#include <cstddef>
#include <new>

#ifndef WOMEM_STATIC_LIB
#   define WOMEM_STATIC_LIB
#endif
#include "woomem.h"

namespace wo
{
    void* alloc64(size_t memsz, womem_attrib_t attrib);
    void free64(void* ptr);
}