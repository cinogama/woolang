#pragma once

#include "wo_basic_type.hpp"

namespace wo
{
    struct runtime_env;
    void update_env_jit(runtime_env* env);
    void cleanup_env_jit(runtime_env* env);
}
