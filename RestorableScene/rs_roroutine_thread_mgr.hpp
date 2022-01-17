#pragma once

// rs_roroutine_mgr is based on virtual machine's runtime
// rs_roroutine_thread_mgr will based on Fiber/UContext
//

#include "rs_roroutine_thread.hpp"
#include "rs_vm.hpp"
#include "rs_roroutine_vmpool.hpp"
#include "rs_shared_ptr.hpp"

#include <thread>
#include <atomic>

namespace rs
{
    class vmthread;

    struct RSCO_Waitter
    {
        vmthread* working_vmthread = nullptr;
        rs::vmbase* working_vm = nullptr;

        std::atomic_flag _completed = {};
        bool complete_flag = false;

        void abort();
    };

    class vmthread
    {
        fthread* m_fthread = nullptr;
        vmbase* m_virtualmachine = nullptr;

        std::atomic_bool m_finish_flag = false;

    public:
        ~vmthread()
        {
            if (m_fthread)
                delete m_fthread;

            rs_asure(m_virtualmachine->interrupt(vmbase::PENDING_INTERRUPT));
        }
        vmthread(rs::vmbase* _vm, rs_int_t vm_funcaddr, size_t argc, rs::shared_pointer<RSCO_Waitter> _waitter = nullptr)
        {
            m_virtualmachine = _vm;
            value* return_sp = _vm->co_pre_invoke(vm_funcaddr, argc);

            m_fthread = new fthread(
                [=]() {
                    do
                    {
                        m_virtualmachine->run();

                        if (m_virtualmachine->sp != return_sp
                            && !(m_virtualmachine->vm_interrupt & vmbase::ABORT_INTERRUPT))
                            fthread::yield();
                        else
                            break;
                    } while (true);

                    if (_waitter)
                    {
                        while (_waitter->_completed.test_and_set())
                            ;
                        _waitter->complete_flag = true;
                    }
                    m_finish_flag = true;
                }
            );
        }
        vmthread(rs::vmbase* _vm, rs_handle_t native_funcaddr, size_t argc, rs::shared_pointer<RSCO_Waitter> _waitter = nullptr)
        {
            m_virtualmachine = _vm;
            m_fthread = new fthread(
                [=]() {
                    m_virtualmachine->invoke(native_funcaddr, argc);

                    if (_waitter)
                    {
                        while (_waitter->_completed.test_and_set())
                            ;
                        _waitter->complete_flag = true;
                    }
                    m_finish_flag = true;
                }
            );
        }
        bool finished() const noexcept
        {
            return m_finish_flag;
        }

        void invoke_from(fiber* from_fib)
        {
            auto _old_this_thread_vm = rs::vmbase::_this_thread_vm;
            rs::vmbase::_this_thread_vm = m_virtualmachine;

            m_fthread->invoke_from(from_fib);

            rs::vmbase::_this_thread_vm = _old_this_thread_vm;
        }

        void join(fiber* from_fib)
        {
            auto _old_this_thread_vm = rs::vmbase::_this_thread_vm;
            rs::vmbase::_this_thread_vm = m_virtualmachine;

            m_fthread->join(from_fib);

            rs::vmbase::_this_thread_vm = _old_this_thread_vm;
        }

        void abort_for_exit(fiber* from_fib)
        {
            auto _old_this_thread_vm = rs::vmbase::_this_thread_vm;
            rs::vmbase::_this_thread_vm = m_virtualmachine;

            m_virtualmachine->interrupt(vmbase::ABORT_INTERRUPT);
            m_fthread->join(from_fib);

            rs::vmbase::_this_thread_vm = _old_this_thread_vm;
        }

        void schedule_yield()
        {
            m_virtualmachine->interrupt(vmbase::YIELD_INTERRUPT);
        }

        void schedule_abort()
        {
            m_virtualmachine->interrupt(vmbase::ABORT_INTERRUPT);
        }
    };

    inline void RSCO_Waitter::abort()
    {
        if (!_completed.test_and_set())
        {
            working_vmthread->schedule_abort();
            _completed.clear();
        }
    }

    class fvmscheduler
    {
        class fvmshcedule_queue_thread
        {
            std::thread                 _real_thread;

            std::list<vmthread*>        _jobs_queue;
            gcbase::rw_lock             _jobs_queue_mx;

            std::atomic<vmthread*>      _current_vmthread = nullptr;

            std::condition_variable_any _jobs_cv;

            std::atomic_bool            _shutdown_flag = false;
            std::atomic_bool            _pause_flag = false;

            static void _queue_thread_work(fvmshcedule_queue_thread* _this)
            {
                fiber _queue_main;
                for (; !_this->_shutdown_flag;)
                {
                    do
                    {
                        std::unique_lock ug1(_this->_jobs_queue_mx);
                        _this->_jobs_cv.wait(ug1,
                            [=]() {return !_this->_jobs_queue.empty()
                            || _this->_shutdown_flag; });

                        if (_this->_shutdown_flag)
                            goto thread_work_end;
                    } while (0);

                    vmthread* working_thread = nullptr;
                    do
                    {
                        std::lock_guard g1(_this->_jobs_queue_mx);
                        working_thread = _this->_jobs_queue.front();
                        _this->_jobs_queue.pop_front();
                    } while (0);

                    _this->_current_vmthread = working_thread;
                    if (_this->_pause_flag)
                    {
                        _this->_current_vmthread = nullptr;
                        continue;
                    }
                    working_thread->invoke_from(&_queue_main);
                    _this->_current_vmthread = nullptr;

                    if (!working_thread->finished())
                    {
                        // Not finished, put it back to queue
                        std::lock_guard g1(_this->_jobs_queue_mx);
                        _this->_jobs_queue.push_back(working_thread);
                    }
                    else
                    {
                        // Finished, destroy vmthread..
                        delete working_thread;
                    }
                }

            thread_work_end:
                _this->_clear_all_works(&_queue_main);
            }

            void _clear_all_works(fiber* _main_fiber)
            {
                std::lock_guard g1(_jobs_queue_mx);
                for (auto* working_thread : _jobs_queue)
                {
                    working_thread->abort_for_exit(_main_fiber);
                    delete working_thread;
                }
                _jobs_queue.clear();
            }

        public:
            ~fvmshcedule_queue_thread()
            {
                do
                {
                    std::lock_guard g1(_jobs_queue_mx);
                    _shutdown_flag = true;

                } while (0);

                _jobs_cv.notify_all();
                _real_thread.join();
            }

            fvmshcedule_queue_thread()
                :_real_thread(_queue_thread_work, this)
            {

            }

            template<typename FT>
            rs::shared_pointer<RSCO_Waitter> new_work(vmbase* _vm, FT funcaddr, size_t argc)
            {
                std::lock_guard g1(_jobs_queue_mx);
                if (_shutdown_flag)
                    return nullptr;

                rs::shared_pointer<RSCO_Waitter> waitter = new RSCO_Waitter;
                waitter->working_vm = _vm;
                waitter->working_vmthread = new vmthread(_vm, funcaddr, argc, waitter);

                _jobs_queue.push_back(waitter->working_vmthread);
                _jobs_cv.notify_one();

                return waitter;

            }

            size_t work_count()
            {
                std::shared_lock s1(_jobs_queue_mx);
                return _jobs_queue.size();
            }

            void schedule_yield()
            {
                auto* thvm = _current_vmthread.load();
                if (thvm)
                    thvm->schedule_yield();
            }

            void pause_immediately()
            {
                _pause_flag = true;
                schedule_yield();
            }

            void wait_until_paused()
            {
                while (_current_vmthread)
                    std::this_thread::yield();
            }

            void resume()
            {
                _pause_flag = false;
                _jobs_cv.notify_one();
            }

            void stop_after_paused()
            {
                rs_assert(_pause_flag && !_current_vmthread);
                fiber _stoppine_fiber;

                _clear_all_works(&_stoppine_fiber);
            }
        };

        std::vector<fvmshcedule_queue_thread> m_working_thread;
        std::atomic_bool            _shutdown_flag = false;
        std::thread                 _schedule_thread;

        gcbase::rw_lock             _schedule_mx;
        std::condition_variable_any _schedule_cv;

        static void _fvmscheduler_thread_work(fvmscheduler* _this)
        {
            using namespace std;
            for (; !_this->_shutdown_flag;)
            {
                do
                {
                    std::unique_lock ug1(_this->_schedule_mx);
                    _this->_schedule_cv.wait_for(ug1, 0.5s, [=]()->bool
                        {
                            return _this->_shutdown_flag;
                        });
                    if (_this->_shutdown_flag)
                        goto thread_work_end;

                } while (0);

                // YIELD ALL CO, LET WORKER CHANGE 
                for (auto& queueth_worker : _this->m_working_thread)
                    queueth_worker.schedule_yield();

                RSCO_WorkerPool::do_reduce();

            }
        thread_work_end:
            ;
        }

        ~fvmscheduler()
        {
            do
            {
                std::lock_guard g1(_schedule_mx);
                _shutdown_flag = true;

            } while (0);
            _schedule_thread.join();

            m_working_thread.clear();
        }
        fvmscheduler(size_t working_thread_count)
            :m_working_thread(working_thread_count)
            , _schedule_thread(_fvmscheduler_thread_work, this)
        {
            rs_assert(working_thread_count);
        }

        inline static fvmscheduler* _scheduler = nullptr;

    public:
        static void init(size_t working_thread_count = 1)
        {
            if (nullptr == _scheduler)
                _scheduler = new fvmscheduler(working_thread_count);
        }

        template<typename FT>
        static rs::shared_pointer<RSCO_Waitter> new_work(vmbase* _vm, FT funcaddr, size_t argc)
        {
            if (!_scheduler)
            {
                rs_fail(RS_FAIL_NOT_SUPPORT, "Coroutine scheduler not supported.");
                return nullptr;
            }

            std::shared_lock s1(_scheduler->_schedule_mx);
            if (_scheduler->_shutdown_flag)
                return nullptr;

            // Finding a minor thread_queue;
            fvmshcedule_queue_thread* minor_wthread = nullptr;
            size_t minor_wthread_job_count = SIZE_MAX;
            for (auto& wthread : _scheduler->m_working_thread)
            {
                if (size_t count = wthread.work_count();
                    count < minor_wthread_job_count)
                {
                    minor_wthread_job_count = count;
                    minor_wthread = &wthread;
                }
            }
            if (minor_wthread)
            {
                return minor_wthread->new_work(_vm, funcaddr, argc);
            }
            return nullptr;
        }

        static void pause_all()
        {
            if (!_scheduler)
            {
                rs_fail(RS_FAIL_NOT_SUPPORT, "Coroutine scheduler not supported.");
                return;
            }

            std::lock_guard g1(_scheduler->_schedule_mx);
            for (auto& wthread : _scheduler->m_working_thread)
                wthread.pause_immediately();
            for (auto& wthread : _scheduler->m_working_thread)
                wthread.wait_until_paused();
        }

        static void resume_all()
        {
            if (!_scheduler)
            {
                rs_fail(RS_FAIL_NOT_SUPPORT, "Coroutine scheduler not supported.");
                return;
            }

            std::lock_guard g1(_scheduler->_schedule_mx);
            for (auto& wthread : _scheduler->m_working_thread)
                wthread.resume();
        }

        static void stop_all()
        {
            if (!_scheduler)
            {
                rs_fail(RS_FAIL_NOT_SUPPORT, "Coroutine scheduler not supported.");
                return;
            }
            std::lock_guard g1(_scheduler->_schedule_mx);

            for (auto& wthread : _scheduler->m_working_thread)
                wthread.pause_immediately();
            for (auto& wthread : _scheduler->m_working_thread)
                wthread.wait_until_paused();

            for (auto& wthread : _scheduler->m_working_thread)
                wthread.stop_after_paused();

            for (auto& wthread : _scheduler->m_working_thread)
                wthread.resume();
        }
    };
}