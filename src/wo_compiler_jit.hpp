#pragma once

#include "wo_basic_type.hpp"

namespace wo
{
    struct runtime_env;
    struct paged_env_min_context;

    void analyze_jit(byte_t* codebuf, runtime_env* env);
    void free_jit(paged_env_min_context* min_env);
}
