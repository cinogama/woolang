#pragma once
// 'roroutine', what a cute name~

#include "rs_vm.hpp"

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

struct RSCO_Waitter
{
    rs::vmbase* working_vm = nullptr;

    bool complete = false;
    std::atomic_bool force_abort = false;
};

class RSCO_WorkerPool
{
    // RSCO_WorkerPool
    // WorkerPool is a container for getting usable vm.
    //
    // WorkerPool will drop all holding vm if there is 
    // no exists vm outside and holding vm is all in pending,
    // 
public:
    class RSCO_PoolVMList
    {
        rs::gcbase::rw_lock m_vm_list_mx;
        std::vector<rs::vmbase*> m_vm_list;

    public:
        ~RSCO_PoolVMList()
        {
            for (auto* vmm : m_vm_list)
            {
                rs_asure(vmm->clear_interrupt(rs::vmbase::vm_interrupt_type::PENDING_INTERRUPT));
                delete vmm;
            }
        }
        rs::vmbase* get_usable_vm(rs::vmbase* origin)
        {
            do
            {
                std::shared_lock s1(m_vm_list_mx);
                for (auto* vmm : m_vm_list)
                    if (vmm->clear_interrupt(rs::vmbase::vm_interrupt_type::PENDING_INTERRUPT))
                    {
                        vmm->sp = vmm->bp = vmm->stack_mem_begin;
                        return vmm;
                    }
            } while (0);

            std::lock_guard g1(m_vm_list_mx);
            rs::vmbase* vmm = origin->make_machine();
            return m_vm_list.emplace_back(vmm);
        }
        rs::shared_pointer<rs::runtime_env> get_one_env()
        {
            std::shared_lock s1(m_vm_list_mx);

            if (m_vm_list.size())
                return m_vm_list.front()->env;
            return nullptr;
        }
        bool need_recude()
        {
            std::shared_lock s1(m_vm_list_mx);
            if (auto env = get_one_env();
                env && env.used_count() == m_vm_list.size())
            {
                // If all vm is PENDING, return true
                for (auto* vm : m_vm_list)
                {
                    if (!(vm->vm_interrupt & rs::vmbase::vm_interrupt_type::PENDING_INTERRUPT))
                        return false;
                }
                return true;
            }
            return false;

        }
    };
private:
    rs::gcbase::rw_lock m_pooled_vm_list_mx;
    std::map<rs::runtime_env*, RSCO_PoolVMList> m_pooled_vm_list;

    inline static RSCO_WorkerPool& pool()
    {
        static RSCO_WorkerPool rsco_pool;
        return rsco_pool;
    }

public:

    static rs::vmbase* get_usable_vm(rs::vmbase* origin)
    {
        rs_assert(origin && origin->env);

        do
        {
            std::shared_lock sl(pool().m_pooled_vm_list_mx);
            if (auto fnd = pool().m_pooled_vm_list.find(origin->env);
                fnd != pool().m_pooled_vm_list.end())
            {
                return fnd->second.get_usable_vm(origin);
            }

        } while (0);

        std::lock_guard gl(pool().m_pooled_vm_list_mx);
        return pool().m_pooled_vm_list[origin->env].get_usable_vm(origin);
    }

    static void do_reduce()
    {
        // Searching in all vm, if vm's refcount == buffered vm count
        // all these vm will be reduced..

        std::lock_guard gl(pool().m_pooled_vm_list_mx);

        std::list<rs::runtime_env*> need_reduce_vm;

        for (auto& [env, vmpool] : pool().m_pooled_vm_list)
        {
            if (vmpool.need_recude())
                need_reduce_vm.push_back(env);
        }

        for (auto* removing_env : need_reduce_vm)
        {
            pool().m_pooled_vm_list.erase(removing_env);
        }

    }
};

class RSCO_Worker
{
    // Worker is real thread..
    struct launching_vm
    {
        rs::vmbase* vmbase;
        rs::value* return_sp;

        size_t      argc;
        rs_handle_t native_funcaddr;

        rs::shared_pointer<RSCO_Waitter> waitter;
    };

    rs::gcbase::rw_lock m_work_mx;
    std::list<launching_vm*> m_work_load;
    std::condition_variable_any m_worker_cv;
    std::thread m_work_thread;

    std::atomic<rs::vmbase*> m_executing_vm;
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

        } while (0);

        if (running_vm_work->waitter->force_abort)
        {
            // WORK RUNNED OVER, PENDING IT AND REMOVE IT FROM LIST;
            std::lock_guard g1(m_work_mx);

            rs_asure(running_vm_work->vmbase
                ->interrupt(rs::vmbase::vm_interrupt_type::PENDING_INTERRUPT));
            m_work_load.pop_front();

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
                || (running_vm_work->vmbase->vm_interrupt & rs::vmbase::ABORT_INTERRUPT))
            {
                // WORK RUNNED OVER, PENDING IT AND REMOVE IT FROM LIST;
                std::lock_guard g1(m_work_mx);

                rs_asure(running_vm_work->vmbase
                    ->interrupt(rs::vmbase::vm_interrupt_type::PENDING_INTERRUPT));
                m_work_load.pop_front();

                running_vm_work->waitter->complete = true;

                delete running_vm_work;
            }
            else
            {
                // WORK YIELD, MOVE IT TO LIST BACK
                std::lock_guard g1(m_work_mx);

                m_work_load.pop_front();
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

            rs_asure(running_vm_work->vmbase
                ->interrupt(rs::vmbase::vm_interrupt_type::PENDING_INTERRUPT));
            m_work_load.pop_front();

            running_vm_work->waitter->complete = true;

            delete running_vm_work;
        }
    }

    rs::shared_pointer<RSCO_Waitter> launch_an_work(rs::vmbase* vmbase, rs_int_t called_func, size_t argc)
    {
        auto* return_sp = vmbase->co_pre_invoke(called_func, argc);
        rs::shared_pointer<RSCO_Waitter> waitter = new RSCO_Waitter;
        m_work_load.push_back(new launching_vm{ vmbase , return_sp, argc, 0 ,waitter });
        m_worker_cv.notify_all();
        return waitter;
    }

    rs::shared_pointer<RSCO_Waitter> launch_an_work(rs::vmbase* vmbase, rs_handle_t called_native_func, size_t argc)
    {
        rs::shared_pointer<RSCO_Waitter> waitter = new RSCO_Waitter;
        m_work_load.push_back(new launching_vm{ vmbase , nullptr, argc, called_native_func,waitter });

        return waitter;
    }

    void yield_for_wheel()
    {
        if (rs::vmbase* vmm = m_executing_vm)
        {
            vmm->interrupt(rs::vmbase::vm_interrupt_type::YIELD_INTERRUPT);
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
    rs::gcbase::rw_lock m_alive_worker_mx;
    std::vector<rs::shared_pointer<RSCO_Worker>> m_alive_worker;
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

        rs::rs_stdout << "RSCO_Scheduler_default_scheduler end" << rs::rs_endl;
    }
public:
    template<typename FTT>
    static rs::shared_pointer<RSCO_Waitter> launch(rs::vmbase* covmm, FTT called_func, size_t argc)
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
            rs_fail(RS_FAIL_DEADLY, "No available worker.");

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

