#include "wo_afx.hpp"

#include "wo_compiler_jit_v2.hpp"

namespace wo
{
    class jit_proxy
    {
    public:
        static void update_env(runtime_env* env)
        {

        }
        static void cleanup_env(runtime_env* env)
        {

        }
    };

    struct runtime_env;
    void update_env_jit_v2(runtime_env* env)
    {
        jit_proxy::update_env(env);
    }
    void cleanup_env_jit_v2(runtime_env* env)
    {
        jit_proxy::cleanup_env(env);
    }
}