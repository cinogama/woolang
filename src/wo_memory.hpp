#pragma once
#include "wo_global_setting.hpp"

#include <stdint.h>
#include <stddef.h>

namespace wo
{
    void* alloc64(size_t memsz);
    void free64(void* ptr);
}
