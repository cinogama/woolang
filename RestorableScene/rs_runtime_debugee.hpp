#pragma once
#include "rs_vm.hpp"
#include "rs_lang.hpp"

#include <mutex>

namespace rs
{
    class default_debugee : public rs::debugee_base
    {
        lang* lang_context;
        std::mutex _mx;

    public:
        default_debugee(lang* _lang_context) :
            lang_context(_lang_context)
        {

        }

        std::map<std::string, std::map<size_t, bool>> break_point_traps;

        void set_breakpoint(const std::string & src_file, size_t rowno)
        {
            break_point_traps[src_file][rowno] = true;
        }
        void clear_breakpoint(const std::string& src_file, size_t rowno)
        {
            break_point_traps[src_file][rowno] = false;
        }

        virtual void debug_interrupt(vmbase* vmm) override
        {
            byte_t* next_execute_ip = vmm->ip;
            auto next_execute_ip_diff = vmm->ip - vmm->env->rt_codes;
            volatile value* current_bp = vmm->bp;

            auto& loc = lang_context->pdd_info.get_src_location(next_execute_ip, vmm->env.get());

            // check breakpoint..
            if (break_point_traps[loc.source_file][loc.row_no])
            {
                printf("Breakdown: +%04d: at %s(%zu, %zu)\n", (int)next_execute_ip_diff,
                    loc.source_file.c_str(), loc.row_no, loc.col_no);

                printf("===========================================\n");
                scanf("ok");
            }
           
        }
    };
}