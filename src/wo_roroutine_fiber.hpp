#pragma once

#include "wo_global_setting.hpp"

#include <cstdint>

#ifdef _WIN32
// include nothing
#else
#   include <ucontext.h>
#endif
namespace wo
{
    class fiber
    {
        bool    m_pure_fiber;
#ifdef WO_PLATRORM_OS_WINDOWS
        void* m_context;
#else
        ucontext_t m_context;
        void* m_fiber_stack;

        static void _fiber_func_invoker(
            uint32_t aimf_lo32,
            uint32_t aimf_hi32,
            uint32_t argp_lo32,
            uint32_t argp_hi32);

#endif
    public:
        fiber(const fiber&) = delete;
        fiber(fiber&&) = delete;
        fiber& operator = (const fiber&) = delete;
        fiber& operator = (fiber&&) = delete;

        fiber();
        fiber(void(*fiber_entry)(void*), void* argn);
        ~fiber();

        template<typename ARGT>
        fiber(void(*fiber_entry)(ARGT*), ARGT* argn)
            :fiber((void(*)(void*))fiber_entry, (void*)argn)
        {
            /* DO NOTHING */
        }
    public:
        bool switch_to(fiber* another_fiber);
    };
}