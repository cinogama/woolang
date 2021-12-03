#pragma once
#include "rs_vm.hpp"
#include "rs_lang.hpp"

#include <mutex>

namespace rs
{
    class default_debuggee : public rs::debuggee_base
    {
        std::recursive_mutex _mx;
        runtime_env* _env = nullptr;

        std::set<size_t> break_point_traps;
        std::map<std::string, std::map<size_t, bool>> template_breakpoint;

    public:
        default_debuggee()
        {

        }
        void set_breakpoint(const std::string& src_file, size_t rowno)
        {
            std::lock_guard g1(_mx);
            if (_env)
                break_point_traps.insert(_env->program_debug_info->get_ip_by_src_location(src_file, rowno));
            else
                template_breakpoint[src_file][rowno] = true;
        }
        void clear_breakpoint(const std::string& src_file, size_t rowno)
        {
            std::lock_guard g1(_mx);
            if (_env)
                break_point_traps.erase(_env->program_debug_info->get_ip_by_src_location(src_file, rowno));
            else
                template_breakpoint[src_file][rowno] = false;
        }

    private:
        virtual void debug_interrupt(vmbase* vmm) override
        {
            do
            {
                std::lock_guard g1(_mx);
                if (!_env)
                {
                    _env = vmm->env.get();

                    for (auto& [src_name, rowbuf] : template_breakpoint)
                        for (auto [row, breakdown] : rowbuf)
                            if (breakdown)
                                set_breakpoint(src_name, row);

                    template_breakpoint.clear();
                }

            } while (0);

            byte_t* next_execute_ip = vmm->ip;
            auto next_execute_ip_diff = vmm->ip - vmm->env->rt_codes;
            value* current_bp = vmm->bp;

            auto command_ip = vmm->env->program_debug_info->get_ip_by_runtime_ip(next_execute_ip);

            // check breakpoint..
            std::lock_guard g1(_mx);
            if (break_point_traps.find(command_ip) != break_point_traps.end())
            {
                block_other_vm_in_this_debuggee();

                auto& loc = vmm->env->program_debug_info->get_src_location_by_runtime_ip(next_execute_ip);

                printf("Breakdown: +%04d: at %s(%zu, %zu)\nin function: %s\n", (int)next_execute_ip_diff,
                    loc.source_file.c_str(), loc.row_no, loc.col_no,
                    vmm->env->program_debug_info->get_current_func_signature_by_runtime_ip(next_execute_ip).c_str()
                    );

                printf("===========================================\n");
                scanf("ok");

                unblock_other_vm_in_this_debuggee();
            }

        }
    };

    class c_style_debuggee_binder : public rs::debuggee_base
    {
        void* custom_items;
        rs_debuggee_handler_func c_debuggee_handler;

        c_style_debuggee_binder(rs_debuggee_handler_func func, void* custom)
        {
            c_debuggee_handler = func;
            custom_items = custom;
        }

        virtual void debug_interrupt(vmbase* vmm) override
        {
            c_debuggee_handler((rs_debuggee)this, (rs_vm)vmm, custom_items);
        }
    };
}