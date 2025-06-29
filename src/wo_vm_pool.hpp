#pragma once

#include "wo_vm.hpp"
#include "wo_gc.hpp"

#include <list>
#include <unordered_map>

namespace wo
{
    class vmpool
    {
        class vmpool_for_spec_env
        {
        private:
            mutable gcbase::rw_lock m_guard;
            std::list<vmbase*> m_free_vm;
        public:
            ~vmpool_for_spec_env()
            {
                for (auto* free_vm : m_free_vm)
                {
                    // Close all vm of current pool
                    wo_close_vm((wo_vm)free_vm);
                }
            }
            bool is_empty() const noexcept
            {
                std::shared_lock sg1(m_guard);
                return m_free_vm.empty();
            }
            vmbase* try_borrow_vm() noexcept
            {
                std::lock_guard g1(m_guard);
                if (!m_free_vm.empty())
                {
                    auto vm = m_free_vm.front();
                    m_free_vm.pop_front();

                    wo_assert(vm->check_interrupt(vmbase::vm_interrupt_type::LEAVE_INTERRUPT));
                    wo_assert(vm->bp == vm->sp && vm->bp == vm->sb);

                    wo_assure(vm->clear_interrupt(vmbase::vm_interrupt_type::PENDING_INTERRUPT));
                    (void)vm->clear_interrupt(vmbase::vm_interrupt_type::ABORT_INTERRUPT);
                    vm->ip = nullptr; // IP Should be set by other function like invoke/dispatch.

                    return vm;
                }
                return nullptr;
            }
            void release_vm(vmbase* vm) noexcept
            {
                std::lock_guard g1(m_guard);

                wo_assert(vm->check_interrupt(vmbase::vm_interrupt_type::LEAVE_INTERRUPT));
                wo_assure(vm->interrupt(vmbase::vm_interrupt_type::PENDING_INTERRUPT));

                // Clear LEAVE_INTERRUPT to make sure hangup correctly before clear stack when GC.
                wo_assure(wo_enter_gcguard(reinterpret_cast<wo_vm>(vm)));

                // Clear stack & register to make sure gc will not mark the useless data of current vm;
                vm->sp = vm->bp = vm->sb;

                const size_t register_count = vm->env->real_register_count;
                for (size_t regi = 0; regi < register_count; ++regi)
                {
                    vm->register_storage[regi].set_nil();
                }

                wo_assure(wo_leave_gcguard(reinterpret_cast<wo_vm>(vm)));
                m_free_vm.push_back(vm);
            }
        };

        mutable gcbase::rw_lock m_pool_guard;
        std::unordered_map<const runtime_env*, vmpool_for_spec_env*> m_pool;

        vmpool_for_spec_env* try_get_pool_by_env(const runtime_env* env)const noexcept
        {
            std::shared_lock sg1(m_pool_guard);
            auto fnd = m_pool.find(env);
            if (fnd == m_pool.end())
                return nullptr;
            return fnd->second;
        }
    public:
        ~vmpool()
        {
            for (auto&[env, pool] : m_pool)
                delete pool;
        }

        vmbase* borrow_vm_from_exists_vm(vmbase* vm) noexcept
        {
            if (vmpool_for_spec_env* pool = try_get_pool_by_env(vm->env);
                pool != nullptr && !pool->is_empty())
            {
                if (vmbase* borrowed_vm = pool->try_borrow_vm())
                    return borrowed_vm;
            }
            return (vmbase*)wo_sub_vm((wo_vm)vm);
        }

        void release_vm(vmbase* vm)noexcept
        {
            vmpool_for_spec_env* pool = nullptr;
            if (nullptr == (pool = try_get_pool_by_env(vm->env)))
            {
                std::lock_guard g1(m_pool_guard);
                auto fnd = m_pool.find(vm->env);
                if (fnd == m_pool.end())
                    pool = m_pool[vm->env] = new vmpool_for_spec_env;
                else
                    pool = fnd->second;
            }

            wo_assert(pool != nullptr);
            pool->release_vm(vm);
        }
    };
}
