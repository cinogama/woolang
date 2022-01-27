#pragma once
#include "rs_roroutine_fiber.hpp"
#include "rs_assert.hpp"

#include<functional>
#include<thread>
#include<atomic>

namespace rs
{
    class fthread;

    class fwaitable
    {
        std::atomic_flag _fwaitable_pendable_flag = {};
        fthread* _fthread = nullptr;
    protected:
        virtual bool be_pending() = 0;
        virtual void be_awake() = 0;
    public:
        fwaitable() = default;
        fwaitable(const fwaitable&) = delete;
        fwaitable(fwaitable&&) = delete;
        fwaitable& operator = (const fwaitable&) = delete;
        fwaitable& operator = (fwaitable&&) = delete;


        virtual ~fwaitable() = default;

        bool pending(fthread* pending);
        void awake();
    };
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
        fthread(const fthread&) = delete;
        fthread(fthread&&) = delete;
        fthread& operator = (const fthread&) = delete;
        fthread& operator = (fthread&&) = delete;

        enum class fthread_state
        {
            NORMAL,     // Nothing to wait, just as a normal work~ 
            WAITTING,   // Have something to wait
            READY,      // Waitting items ready!
            YIELD,      // Take a break~
        };

        fthread_state state = fthread_state::NORMAL;

    public:
        template<typename FT, typename ... ARGTs>
        fthread(FT&& ft, ARGTs && ...  args)
        {
            m_invoke_func = std::bind(ft, std::forward(args)...);
            m_fiber = new fiber(_invoke_fthread, this);
        }

        ~fthread()
        {
            std::cout << "fthread freeed: " << this << std::endl;

            rs_assert(m_finish_flag);

            if (m_fiber)
                delete m_fiber;
        }

        void invoke_from(fiber* from_fib)
        {
            if (m_finish_flag)
                return;

            state = fthread_state::NORMAL;

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
                {
                    current_co->state = fthread_state::YIELD;
                    current_co->m_fiber->switch_to(current_co->m_scheduler_fiber);
                }
            }
            else
                rs_fail(RS_FAIL_NOT_SUPPORT, "non-fiber-thread cannot yield.");
        }
        template<typename T>
        static void wait(rs::shared_pointer<T> waitable)
        {
            static_assert(std::is_base_of<fwaitable, T>::value);

            if (current_co && current_co->m_scheduler_fiber)
            {
                if (waitable->pending(current_co))
                {
                    current_co->m_fiber->switch_to(current_co->m_scheduler_fiber);
                }
            }
            else
                rs_fail(RS_FAIL_NOT_SUPPORT, "non-fiber-thread cannot yield.");
        }
    };


    inline bool fwaitable::pending(fthread* pending)
    {
        if (!_fwaitable_pendable_flag.test_and_set())
        {
            _fthread = pending;
            if (be_pending())
            {
                pending->state = fthread::fthread_state::WAITTING;
                _fwaitable_pendable_flag.clear();
                return true;
            }
            else
            {
                _fwaitable_pendable_flag.clear();
            }
        }
        return false;
    }
    inline void fwaitable::awake()
    {
        while (!_fwaitable_pendable_flag.test_and_set())
            ;

        if (_fthread)
        {
            _fthread->state = fthread::fthread_state::READY;
            be_awake();
        }
    }
}