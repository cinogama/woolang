#pragma once

#include "rs_vm.hpp"

#include <list>
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
                while (
                    !(vmm->vm_interrupt & rs::vmbase::vm_interrupt_type::LEAVE_INTERRUPT)
                    && (!(vmm->vm_interrupt & rs::vmbase::vm_interrupt_type::PENDING_INTERRUPT)
                        || !(vmm->vm_interrupt & rs::vmbase::vm_interrupt_type::ABORT_INTERRUPT)))
                    std::this_thread::yield();
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
                        vmm->finish_veh();
                        vmm->clear_interrupt(rs::vmbase::ABORT_INTERRUPT);
                        return vmm;
                    }
            } while (0);

            std::lock_guard g1(m_vm_list_mx);
            rs::vmbase* vmm = origin->make_machine(1024);
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
                env && env.used_count() == m_vm_list.size() + 2/*(GC_THREAD)*/)
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

    static void shutdown()
    {
        std::lock_guard gl(pool().m_pooled_vm_list_mx);
        
        pool().m_pooled_vm_list.clear();
    }
};

