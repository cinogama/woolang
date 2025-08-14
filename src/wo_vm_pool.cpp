#include "wo_afx.hpp"
#include "wo_vm_pool.hpp"

namespace wo
{
    std::optional<std::unique_ptr<vmpool>>
        vmpool::global_vmpool_instance;

    vmpool::vmpool_for_spec_env::vmpool_for_spec_env(runtime_env* env)
        : m_env(env)
    {

    }
    vmpool::vmpool_for_spec_env::~vmpool_for_spec_env()
    {
        for (auto* free_vm : m_free_vm)
        {
            // Close all vm of current pool
            wo_close_vm((wo_vm)free_vm);
        }
    }
    bool vmpool::vmpool_for_spec_env::is_norefed() const noexcept
    {
        std::shared_lock sg1(m_guard);
        return m_free_vm.size() + 1 /* GC-VM */ == m_env->_running_on_vm_count;
    }
    bool vmpool::vmpool_for_spec_env::is_empty() const noexcept
    {
        std::shared_lock sg1(m_guard);
        return m_free_vm.empty();
    }
    vmbase* vmpool::vmpool_for_spec_env::try_borrow_vm() noexcept
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
    void vmpool::vmpool_for_spec_env::release_vm(vmbase* vm) noexcept
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

    bool vmpool::try_get_pool_by_env(
        runtime_env* env,
        const std::function<bool(vmpool_for_spec_env&)>& work)const noexcept
    {
        std::shared_lock sg1(m_pool_guard);
        auto fnd = m_pool.find(env);

        if (fnd == m_pool.end())
            return false;

        return work(*fnd->second);
    }

    vmbase* vmpool::borrow_vm_from_exists_vm(vmbase* vm) noexcept
    {
        vmbase* borrowed_vm = nullptr;
        bool result = try_get_pool_by_env(
            vm->env,
            [&borrowed_vm](vmpool_for_spec_env& pool)
            {
                borrowed_vm = pool.try_borrow_vm();
                return borrowed_vm != nullptr;
            });
        if (result)
        {
            wo_assert(borrowed_vm != nullptr);
            return borrowed_vm;
        }
        return (vmbase*)wo_sub_vm((wo_vm)vm);
    }
    void vmpool::release_vm(vmbase* vm)noexcept
    {
        bool result = try_get_pool_by_env(
            vm->env,
            [vm](vmpool_for_spec_env& pool)
            {
                pool.release_vm(vm);
                return true;
            });

        if (!result)
        {
            std::lock_guard g1(m_pool_guard);

            vmpool_for_spec_env* pool = nullptr;

            auto fnd = m_pool.find(vm->env);
            if (fnd == m_pool.end())
            {
                auto* env_ptr = vm->env.get();

                auto r = m_pool.insert(
                    std::make_pair(env_ptr, std::make_unique<vmpool_for_spec_env>(env_ptr)));

                wo_assert(r.second);
                pool = r.first->second.get();
            }
            else
                pool = fnd->second.get();

            pool->release_vm(vm);
        }
    }
    void vmpool::gc_check_and_release_norefed_vm() noexcept
    {
        std::lock_guard g1(m_pool_guard);

        auto iter = m_pool.begin();
        const auto end = m_pool.end();

        while (iter != end)
        {
            auto pool = iter->second.get();
            auto this_iter = iter++;

            if (pool->is_norefed())
            {
                // Remove unrefed vm pool.
                m_pool.erase(this_iter);
            }
        }
    }
}
