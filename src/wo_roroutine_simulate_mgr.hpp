#pragma once
// 'roroutine', what a cute name~

#if 0 
// Fiber worker enable, abondon this one.

#include "wo_vm.hpp"
#include "wo_roroutine_vmpool.hpp"

#include <mutex>
#include <shared_mutex>

/*

PENDING
* No code running, marked as PENDING_INTERRUPT

YIELD
* Has code, but not running..

RUNNING
* Has code, and now is running.


*/

class RSCO_Worker
{
    // Worker is real thread..
    struct launching_vm
    {
        wo::vmbase* vmbase;
        wo::value* return_sp;

        size_t      argc;
        wo_handle_t native_funcaddr;

        wo::shared_pointer<RSCO_Waitter> waitter;
    };

    wo::gcbase::rw_lock m_work_mx;
    std::list<launching_vm*> m_work_load;
    std::condition_variable_any m_worker_cv;
    std::thread m_work_thread;

    std::atomic<wo::vmbase*> m_executing_vm;
    bool pause_flag = false;

    std::atomic_bool m_thread_work_flag = true;

    static void default_worker_loop(RSCO_Worker* work)
    {
        using namespace std;

        for (; work->m_thread_work_flag;)
        {
            do
            {
                std::unique_lock ug1(work->m_work_mx);

                work->m_worker_cv.wait_for(ug1, 1s,
                    [&] {return !work->m_work_load.empty() && !work->pause_flag; });
            } while (0);

            work->execute_once();
        }
    }

public:
    ~RSCO_Worker()
    {
        m_thread_work_flag = false;
        m_work_thread.join();
    }

    RSCO_Worker()
        :m_work_thread(default_worker_loop, this)
        , m_executing_vm(nullptr)
    {

    }

    size_t size()
    {
        std::shared_lock s1(m_work_mx);
        return m_work_load.size();
    }

    void execute_once()
    {
        launching_vm* running_vm_work = nullptr;
        do
        {
            std::shared_lock s1(m_work_mx);

            if (m_work_load.empty())
                return; // TODO: STEAL SOME WORK FROM OTHER WORKER

            running_vm_work = m_work_load.front();
            m_work_load.pop_front();

        } while (0);

        if (running_vm_work->waitter->force_abort)
        {
            // WORK RUNNED OVER, PENDING IT AND REMOVE IT FROM LIST;
            std::lock_guard g1(m_work_mx);

            wo_asure(running_vm_work->vmbase
                ->interrupt(wo::vmbase::vm_interrupt_type::PENDING_INTERRUPT));

            running_vm_work->waitter->complete = true;

            delete running_vm_work;
        }
        else if (running_vm_work->return_sp) // INVOKING RSVM FUNC
        {
            m_executing_vm = running_vm_work->vmbase;
            running_vm_work->vmbase->run();
            // TODO: IF SOME WORK TOOK TOOO MUCH TIME, WORK WILL RUN AWAY
            m_executing_vm = nullptr;

            if (running_vm_work->vmbase->sp == running_vm_work->return_sp
                || (running_vm_work->vmbase->vm_interrupt & wo::vmbase::ABORT_INTERRUPT))
            {
                // WORK RUNNED OVER, PENDING IT AND REMOVE IT FROM LIST;
                std::lock_guard g1(m_work_mx);

                wo_asure(running_vm_work->vmbase
                    ->interrupt(wo::vmbase::vm_interrupt_type::PENDING_INTERRUPT));

                running_vm_work->waitter->complete = true;

                delete running_vm_work;
            }
            else
            {
                // WORK YIELD, MOVE IT TO LIST BACK
                std::lock_guard g1(m_work_mx);
                m_work_load.push_back(running_vm_work);
            }
        }
        else // INVOKING NATIVE FUNC
        {
            m_executing_vm = running_vm_work->vmbase;
            running_vm_work->vmbase->invoke(
                running_vm_work->native_funcaddr,
                running_vm_work->argc);
            // TODO: IF SOME WORK TOOK TOOO MUCH TIME, WORK WILL RUN AWAY
            m_executing_vm = nullptr;


            std::lock_guard g1(m_work_mx);

            wo_asure(running_vm_work->vmbase
                ->interrupt(wo::vmbase::vm_interrupt_type::PENDING_INTERRUPT));
            running_vm_work->waitter->complete = true;

            delete running_vm_work;
        }
    }

    wo::shared_pointer<RSCO_Waitter> launch_an_work(wo::vmbase* vmbase, wo_int_t called_func, size_t argc)
    {
        auto* return_sp = vmbase->co_pre_invoke(called_func, argc);
        wo::shared_pointer<RSCO_Waitter> waitter = new RSCO_Waitter;
        m_work_load.push_back(new launching_vm{ vmbase , return_sp, argc, 0 ,waitter });
        m_worker_cv.notify_all();
        return waitter;
    }

    wo::shared_pointer<RSCO_Waitter> launch_an_work(wo::vmbase* vmbase, wo_handle_t called_native_func, size_t argc)
    {
        wo::shared_pointer<RSCO_Waitter> waitter = new RSCO_Waitter;
        m_work_load.push_back(new launching_vm{ vmbase , nullptr, argc, called_native_func,waitter });

        return waitter;
    }

    void yield_for_wheel()
    {
        if (wo::vmbase* vmm = m_executing_vm)
        {
            vmm->interrupt(wo::vmbase::vm_interrupt_type::YIELD_INTERRUPT);
        }
    }

    void pause()
    {
        pause_flag = true;
        yield_for_wheel();
    }

    void resume()
    {
        pause_flag = false;
        m_worker_cv.notify_all();
    }

    void stop()
    {
        pause();
        wait_until_paused();
        do
        {
            std::lock_guard g1(m_work_mx);
            m_work_load.clear();

        } while (0);

        resume();
    }

    void wait_until_paused() const
    {
        while (m_executing_vm)
            std::this_thread::yield();
    }
};

class RSCO_Scheduler
{
    wo::gcbase::rw_lock m_alive_worker_mx;
    std::vector<wo::shared_pointer<RSCO_Worker>> m_alive_worker;
    std::thread m_scheduler_thread;
    std::condition_variable m_scheduler_waiter;
    std::atomic_bool m_scheduler_work_flag = true;

    RSCO_Scheduler()
        :m_alive_worker(4)
        , m_scheduler_thread(RSCO_Scheduler_default_scheduler)
    {
        for (auto& worker : m_alive_worker)
            worker = new RSCO_Worker;
    }

    ~RSCO_Scheduler()
    {
        m_scheduler_work_flag = false;
        m_scheduler_thread.join();
    }

    inline static RSCO_Scheduler& scheduler()
    {
        static RSCO_Scheduler rsco_scheduler;
        return rsco_scheduler;
    }

    static void RSCO_Scheduler_default_scheduler()
    {
        using namespace std;

        for (; scheduler().m_scheduler_work_flag;)
        {
            // YIELD ALL COROUTINE PER ONE SEC TO WHEEL
            do
            {
                std::mutex useless_mtx;
                std::unique_lock ug1(useless_mtx);
                scheduler().m_scheduler_waiter.wait_for(ug1, 1s,
                    []() {return !scheduler().m_scheduler_work_flag; });
            } while (0);

            do
            {
                std::shared_lock s1(scheduler().m_alive_worker_mx);
                for (auto& worker : scheduler().m_alive_worker)
                {
                    worker->yield_for_wheel();
                }
            } while (0);

            RSCO_WorkerPool::do_reduce();
        }

        scheduler().m_scheduler_work_flag = true;

        wo::wo_stdout << "RSCO_Scheduler_default_scheduler end" << wo::wo_endl;
    }
public:
    template<typename FTT>
    static wo::shared_pointer<RSCO_Waitter> launch(wo::vmbase* covmm, FTT called_func, size_t argc)
    {
        // Find a worker
        size_t aim_worker_load = SIZE_MAX;
        RSCO_Worker* aim_worker = nullptr;

        do
        {
            std::shared_lock s1(scheduler().m_alive_worker_mx);
            for (auto& alive_worker : scheduler().m_alive_worker)
            {
                auto worker_load = alive_worker->size();
                if (worker_load < aim_worker_load)
                {
                    aim_worker_load = worker_load;
                    aim_worker = alive_worker;
                }
            }
        } while (0);

        if (aim_worker)
            return aim_worker->launch_an_work(covmm, called_func, argc);
        else
            wo_fail(WO_FAIL_DEADLY, "No available worker.");

        return nullptr;
    }

    static void pause_all()
    {
        std::shared_lock s1(scheduler().m_alive_worker_mx);
        for (auto& worker : scheduler().m_alive_worker)
            worker->pause();
        for (auto& worker : scheduler().m_alive_worker)
            worker->wait_until_paused();
    }
    static void resume_all()
    {
        std::shared_lock s1(scheduler().m_alive_worker_mx);
        for (auto& worker : scheduler().m_alive_worker)
            worker->resume();
    }

    static void stop_all()
    {
        std::shared_lock s1(scheduler().m_alive_worker_mx);
        for (auto& worker : scheduler().m_alive_worker)
            worker->stop();
    }
};

#endif