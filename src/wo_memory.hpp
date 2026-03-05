#pragma once
#include "wo_global_setting.hpp"

#include <stdint.h>
#include <stddef.h>

namespace wo
{
    void* gc_alloc(size_t memsz);
}
