#pragma once

#include "wo_basic_type.hpp"

namespace wo
{
    struct runtime_env;
    void analyze_jit(byte_t* codebuf, runtime_env* env);
}
