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
#include <list>
#include <mutex>
#include <condition_variable>

using namespace std::chrono_literals;

constexpr std::chrono::steady_clock::duration PRE_ACTIVE_TIME = 10'000'000ns; // 1'000'000'000ns is 1s

namespace rs
{
    class vmthread;

    class fvmscheduler;

    class fvmscheduler_fwaitable_base : public rs::fwaitable
    {
        friend class fvmscheduler;

        void be_awake()final;
    };


    struct RSCO_Waitter : public fvmscheduler_fwaitable_base
    {
        vmthread* working_vmthread = nullptr;
        rs::vmbase* working_vm = nullptr;

        std::atomic_flag _completed = {};
        bool complete_flag = false;

        void abort();
        void complete()
        {
            while (_completed.test_and_set())
                ;
            complete_flag = true;

            awake();
        }

        bool be_pending()override final
        {
            /* There is no need to do any thing */

            if (complete_flag)
                return false;

            return true;
        }
    };

    class vmthread
    {
        fthread* m_fthread = nullptr;
        vmbase* m_virtualmachine = nullptr;

        std::atomic_bool m_finish_flag = false;

    public:

        vmthread(const vmthread&) = delete;
        vmthread(vmthread&&) = delete;
        vmthread& operator = (const vmthread&) = delete;
        vmthread& operator = (vmthread&&) = delete;

        ~vmthread()
        {
            if (m_fthread)
                delete m_fthread;

            rs_asure(m_virtualmachine->interrupt(vmbase::PENDING_INTERRUPT));
        }
        vmthread(rs::vmbase* _vm, rs_int_t vm_funcaddr, size_t argc, rs::shared_pointer<RSCO_Waitter> _waitter = nullptr)
        {
            m_virtualmachine = _vm;
            if (_vm->co_pre_invoke(vm_funcaddr, argc))
            {
                m_fthread = new fthread(
                    [=]() {
                        m_virtualmachine->run();

                        if (_waitter)
                        {
                            _waitter->complete();
                        }
                        m_finish_flag = true;
                    }
                );
            }
        }
        vmthread(rs::vmbase* _vm, rs_handle_t native_funcaddr, size_t argc, rs::shared_pointer<RSCO_Waitter> _waitter = nullptr)
        {
            m_virtualmachine = _vm;
            m_fthread = new fthread(
                [=]() {
                    m_virtualmachine->invoke(native_funcaddr, argc);

                    if (_waitter)
                    {
                        _waitter->complete();
                    }
                    m_finish_flag = true;
                }
            );
        }
        bool finished() const noexcept
        {
            return m_finish_flag;
        }
        bool waitting() const
        {
            return m_fthread->state == fthread::fthread_state::WAITTING;
        }
        bool waitting_ready() const
        {
            return m_fthread->state == fthread::fthread_state::READY;
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

    class fvmscheduler_waitfortime : public fvmscheduler_fwaitable_base
    {
        friend class fvmscheduler;


        std::chrono::steady_clock::time_point awake_time_point;

        bool be_pending()override;
    };

    class fvmscheduler
    {
        friend class fvmscheduler_fwaitable_base;
        friend class fvmscheduler_waitfortime;

        class fvmshcedule_queue_thread
        {
            std::thread                 _real_thread;

            std::list<vmthread*>        _jobs_queue;
            std::list<vmthread*>        _pending_jobs_queue;
            gcbase::rw_lock             _jobs_queue_mx;

            std::atomic<vmthread*>      _current_vmthread = nullptr;

            std::condition_variable_any _jobs_cv;

            std::atomic_bool   _shutdown_flag = false;
            std::atomic_bool   _pause_flag = false;

            static void _queue_thread_work(fvmshcedule_queue_thread* _this)
            {
#ifdef RS_PLATRORM_OS_WINDOWS
                SetThreadDescription(GetCurrentThread(), L"rs_coroutine_worker_thread");
#endif
                fiber _queue_main;
                for (; !_this->_shutdown_flag;)
                {
                    do
                    {
                        std::unique_lock ug1(_this->_jobs_queue_mx);
                        _this->_jobs_cv.wait(ug1,
                            [=]() {

                                bool enable_execute_flag =
                                    (!_this->_jobs_queue.empty() && !_this->_pause_flag)
                                    || _this->_shutdown_flag;
                                return enable_execute_flag;
                            });

                        if (_this->_shutdown_flag)
                            goto thread_work_end;
                    } while (0);

                    vmthread* working_thread = nullptr;

                    if (_this->_pause_flag)
                    {
                        _this->_current_vmthread = nullptr;
                        continue;
                    }

                    do
                    {
                        std::lock_guard g1(_this->_jobs_queue_mx);
                        working_thread = _this->_jobs_queue.front();
                        _this->_jobs_queue.pop_front();
                    } while (0);

                    _this->_current_vmthread = working_thread;
                    working_thread->invoke_from(&_queue_main);
                    _this->_current_vmthread = nullptr;
                    if (working_thread->waitting())
                    {
                        std::lock_guard g1(_this->_jobs_queue_mx);
                        _this->_pending_jobs_queue.push_back(working_thread);
                    }
                    else if (!working_thread->finished())
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
            {
                _real_thread = std::move(std::thread(_queue_thread_work, this));
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

            void awake_all_avaliable_thread()
            {
                std::lock_guard g1(_jobs_queue_mx);

                bool has_ready_work = false;

                for (auto p1 = _pending_jobs_queue.begin();
                    p1 != _pending_jobs_queue.end();)
                {
                    auto p2 = p1++;
                    if ((*p2)->waitting_ready())
                    {
                        has_ready_work = true;
                        _jobs_queue.push_front(*p2);

                        _pending_jobs_queue.erase(p2);
                    }
                }

                if (has_ready_work)
                {
                    schedule_yield(); // make awaked thread work immediately
                    _jobs_cv.notify_all();
                }
            }
        };

        std::vector<fvmshcedule_queue_thread> m_working_thread;
        std::atomic_bool            _shutdown_flag = false;
        std::thread                 _schedule_thread;

        std::thread                 _schedule_timer_thread;

        gcbase::rw_lock             _schedule_mx;
        std::condition_variable_any _schedule_cv;

        std::atomic_flag            _awaking_flag = {};

        static void _fvmscheduler_thread_work(fvmscheduler* _this)
        {
#ifdef RS_PLATRORM_OS_WINDOWS
            SetThreadDescription(GetCurrentThread(), L"rs_coroutine_scheduler_thread");
#endif
            using namespace std;
            for (; !_this->_shutdown_flag;)
            {
                bool _awake_flag = false;
                do
                {
                    std::unique_lock ug1(_this->_schedule_mx);
                    _this->_schedule_cv.wait_for(ug1, 5s, [&]()->bool
                        {
                            _awake_flag = !_this->_awaking_flag.test_and_set();
                            return _this->_shutdown_flag || _awake_flag;
                        });
                    if (_this->_shutdown_flag)
                        goto thread_work_end;

                } while (0);

                if (_awake_flag)
                {
                    for (auto& queueth_worker : _this->m_working_thread)
                        queueth_worker.awake_all_avaliable_thread();
                }
                else
                {
                    // YIELD ALL CO, LET WORKER CHANGE 
                    for (auto& queueth_worker : _this->m_working_thread)
                        queueth_worker.schedule_yield();

                    RSCO_WorkerPool::do_reduce();
                }

            }
        thread_work_end:
            ;
        }

        std::chrono::steady_clock _hr_clock;
        std::chrono::steady_clock::time_point _hr_current_time_point;
        gcbase::rw_lock             _timer_mx;
        std::condition_variable_any _timer_cv;
        std::condition_variable_any _timer_list_cv;
        std::map<std::chrono::steady_clock::time_point,
            std::list<fvmscheduler_waitfortime*>> _hr_listening_awakeup_time_point;

        static void _fvmscheduler_timer_work(fvmscheduler* _this)
        {
#ifdef RS_PLATRORM_OS_WINDOWS
            SetThreadDescription(GetCurrentThread(), L"rs_coroutine_timer_thread");
#endif
            using namespace std;
            for (; !_this->_shutdown_flag;)
            {
                do
                {
                    do
                    {
                        std::unique_lock ug1(_this->_timer_mx);
                        _this->_timer_list_cv.wait(ug1,
                            [=]() {
                                return _this->_shutdown_flag || !_this->_hr_listening_awakeup_time_point.empty();
                            });
                        if (_this->_shutdown_flag)
                            goto thread_work_end;
                    } while (0);

                    rs_assert(!_this->_hr_listening_awakeup_time_point.empty());

                    std::unique_lock ug1(_this->_timer_mx);
                    _this->_timer_cv.wait_until(ug1,
                        _this->_hr_listening_awakeup_time_point.begin()->first - PRE_ACTIVE_TIME,
                        [=]()->bool {
                            return _this->_shutdown_flag;
                        });
                    if (_this->_shutdown_flag)
                        goto thread_work_end;

                    _this->_hr_current_time_point = _this->_hr_clock.now();

                    auto idx = _this->_hr_listening_awakeup_time_point.begin();
                    for (; idx != _this->_hr_listening_awakeup_time_point.end();
                        idx++)
                    {
                        if (idx->first - 2 * PRE_ACTIVE_TIME > _this->_hr_current_time_point)
                            break;
                        for (auto& waitter : idx->second)
                            waitter->awake();
                    }
                    _this->_hr_listening_awakeup_time_point.erase(
                        _this->_hr_listening_awakeup_time_point.begin(), idx);

                } while (0);
            }
        thread_work_end:
            ;
        }

        ~fvmscheduler()
        {
            do
            {
                std::lock_guard g1(_schedule_mx);
                std::unique_lock ug1(_timer_mx);
                _shutdown_flag = true;

            } while (0);

            _schedule_cv.notify_all();
            _timer_cv.notify_all();
            _timer_list_cv.notify_all();

            _schedule_thread.join();
            _schedule_timer_thread.join();

            m_working_thread.clear();
        }
        fvmscheduler(size_t working_thread_count)
            : m_working_thread(working_thread_count)
            , _hr_current_time_point(_hr_clock.now())
        {
            _schedule_thread = std::move(std::thread(_fvmscheduler_thread_work, this));
            _schedule_timer_thread = std::move(std::thread(_fvmscheduler_timer_work, this));

            rs_assert(working_thread_count);
        }

        inline static fvmscheduler* _scheduler = nullptr;

    public:
        static rs::shared_pointer<fvmscheduler_waitfortime> wait(double _tm)
        {
            using namespace std;

            fvmscheduler_waitfortime* wtime = new fvmscheduler_waitfortime;
            wtime->awake_time_point = _scheduler->_hr_clock.now();
            wtime->awake_time_point += (int64_t(_tm * 1000000000.)) * 1ns;

            return wtime;
        }

        inline static fvmscheduler* current_scheduler()
        {
            return _scheduler;
        }

        static void init(size_t working_thread_count = 4)
        {
            if (working_thread_count)
            {
                rs_assert(_scheduler == nullptr, "_scheduler should be nullptr here.");
                _scheduler = new fvmscheduler(working_thread_count);
            }
            else
                rs_warning("_scheduler disabled!");
        }

        static void shutdown()
        {
            if (_scheduler)
            {
                rs_coroutine_stopall();

                delete _scheduler;
                _scheduler = nullptr;

                RSCO_WorkerPool::shutdown();
            }
        }

        template<typename FT>
        static rs::shared_pointer<RSCO_Waitter> new_work(vmbase* _vm, FT funcaddr, size_t argc)
        {
            if (!_scheduler)
            {
                rs_fail(RS_FAIL_NOT_SUPPORT, "Coroutine scheduler not supported.");
                return nullptr;

            }

            static size_t using_machine_id = 0;

            std::shared_lock s1(_scheduler->_schedule_mx);
            if (_scheduler->_shutdown_flag)
                return nullptr;

            // Finding a minor thread_queue;
            fvmshcedule_queue_thread* minor_wthread =
                &(_scheduler->m_working_thread[using_machine_id++ % _scheduler->m_working_thread.size()]);

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

            // TODO: Here is not thread safe, some thread can't be pause, review!
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
    inline bool fvmscheduler_waitfortime::be_pending()
    {
        auto* _scheduler = fvmscheduler::current_scheduler();

        std::lock_guard g1(_scheduler->_timer_mx);

        if (this->awake_time_point <= _scheduler->_hr_current_time_point)
            return false;

        bool need_update_timer_flag = false;
        if (_scheduler->_hr_listening_awakeup_time_point.empty() ||
            this->awake_time_point < _scheduler->_hr_listening_awakeup_time_point.begin()->first)
            need_update_timer_flag = true;

        _scheduler->_hr_listening_awakeup_time_point[this->awake_time_point].push_back(this);

        if (need_update_timer_flag)
        {
            _scheduler->_timer_list_cv.notify_all();
            _scheduler->_timer_cv.notify_all();
        }
        return true;
    }
    inline void fvmscheduler_fwaitable_base::be_awake()
    {
        auto* _scheduler = fvmscheduler::current_scheduler();

        _scheduler->_awaking_flag.clear();
        _scheduler->_schedule_cv.notify_all();
    }
}