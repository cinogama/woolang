#pragma once

#include "wo_vm.hpp"
#include "wo_gc.hpp"

#include <forward_list>
#include <unordered_map>
#include <optional>
#include <memory>
#include <shared_mutex>

namespace wo
{
    class vmpool
    {
        class vmpool_for_spec_env
        {
        private:
            mutable std::shared_mutex m_guard;

            runtime_env* m_env;
            std::vector<vmbase*>  m_free_vm;
        public:
            vmpool_for_spec_env(runtime_env* env);
            ~vmpool_for_spec_env();

            vmpool_for_spec_env(const vmpool_for_spec_env&) = delete;
            vmpool_for_spec_env(vmpool_for_spec_env&&) = delete;
            vmpool_for_spec_env& operator = (const vmpool_for_spec_env&) = delete;
            vmpool_for_spec_env& operator = (vmpool_for_spec_env&&) = delete;

            bool is_norefed() const noexcept;
            bool is_empty() const noexcept;
            vmbase* try_borrow_vm() noexcept;
            void release_vm(vmbase* vm) noexcept;
        };

        mutable gcbase::rw_lock m_pool_guard;
        std::unordered_map<runtime_env*, std::unique_ptr<vmpool_for_spec_env>> m_pool;

        bool try_get_pool_by_env(
            runtime_env* env,
            const std::function<bool(vmpool_for_spec_env&)>& work)const noexcept;
    public:

        vmbase* borrow_vm_from_exists_vm(vmbase* vm) noexcept;
        void release_vm(vmbase* vm) noexcept;
        void drop_all_vm_in_shutdown() noexcept;
        void gc_check_and_release_norefed_vm() noexcept;

        static std::optional<std::unique_ptr<vmpool>>
            global_vmpool_instance;
    };
}
