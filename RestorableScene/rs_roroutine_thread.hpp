#pragma once
#include "rs_roroutine_fiber.hpp"
#include "rs_assert.hpp"

#include<functional>
#include<thread>
#include<atomic>

namespace rs
{
    class fthread
    {
        inline static thread_local fthread* current_co = nullptr;

        fiber* m_scheduler_fiber = nullptr;
        fiber* m_fiber = nullptr;
        std::atomic_bool   m_finish_flag = false;
        std::atomic_bool   m_disable_yield_flag = false;
        std::function<void(void)> m_invoke_func;

        static void _invoke_fthread(fthread* _this)
        {
            _this->m_invoke_func();
            _this->m_finish_flag = true;

            yield(true);
        }

    public:
        template<typename FT, typename ... ARGTs>
        fthread(FT&& ft, ARGTs && ...  args)
        {
            m_invoke_func = std::bind(ft, std::forward(args)...);
            m_fiber = new fiber(_invoke_fthread, this);
        }

        ~fthread()
        {
            rs_assert(m_finish_flag);

            if (m_fiber)
                delete m_fiber;
        }

        void invoke_from(fiber* from_fib)
        {
            if (m_finish_flag)
                return;

            auto* origin_current_co = current_co;
            current_co = this;
            m_scheduler_fiber = from_fib;

            from_fib->switch_to(m_fiber);

            current_co = origin_current_co;
        }

        void join(fiber* from_fib)
        {
            m_disable_yield_flag = true;

            invoke_from(from_fib);
        }

    public:
        static void yield(bool force = false)
        {
            // Used for yield back to scheduler thread..
            if (current_co && current_co->m_scheduler_fiber)
            {
                if (force || !current_co->m_disable_yield_flag)
                    current_co->m_fiber->switch_to(current_co->m_scheduler_fiber);
            }
            else
                rs_error("non-fiber-thread cannot yield;");
        }
    };
}