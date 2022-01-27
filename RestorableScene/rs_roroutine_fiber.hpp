#pragma once

#include "rs_global_setting.hpp"

#include <cstdint>

#ifdef _WIN32
// include nothing
#else
#   include <ucontext.h>
#endif
namespace rs
{
    class fiber
    {
        bool    m_pure_fiber;
#ifdef RS_PLATRORM_OS_WINDOWS
        void* m_context;
#else
        ucontext_t m_context;
        void* m_fiber_stack;

#endif
    public:
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