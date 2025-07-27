#pragma once
#include "wo_vm.hpp"
#include "wo_lang.hpp"

#include <mutex>
#include <chrono>
#include <algorithm>
#include <unordered_set>

wo_real_t _wo_inside_time_sec();

namespace wo
{
    class default_cli_debuggee_bridge : public wo::vm_debuggee_bridge_base
    {
        struct _env_context
        {
            struct breakpoint_info
            {
                std::string         m_filepath;
                size_t              m_row_no;
                std::vector<size_t> m_break_ips;
            };
            std::vector<breakpoint_info> break_point_traps;
            std::unordered_multiset<size_t> break_ips;
        };
        std::map<runtime_env*, _env_context> env_context;

        bool stop_attach_debuggee_for_exit_flag = false;
        bool stop_for_detach_debuggee = false;
        bool first_time_to_breakdown = true;

        struct cpu_profiler_record_infornmation
        {
            size_t m_inclusive;
            size_t m_exclusive;
            std::map<size_t, size_t> m_exclusive_record;
        };
        std::unordered_map<std::string, cpu_profiler_record_infornmation> profiler_records = {};
        bool                                    profiler_enabled = false;
        double                                  profiler_until = 0.;
        size_t                                  profiler_total_count = 0;
        std::unordered_map<wo::vmbase*, double> profiler_last_sampling_times = {};

    public:
        default_cli_debuggee_bridge() = default;

        void set_breakpoint_with_ips(wo::vmbase* vmm, const std::string& src_file, size_t rowno, std::vector<size_t> ips)
        {
            auto& context = env_context[vmm->env];
            for (size_t bip : ips)
                context.break_ips.insert(bip);
            context.break_point_traps.emplace_back(_env_context::breakpoint_info{ src_file, rowno, ips });
        }
        bool set_breakpoint(wo::vmbase* vmm, const std::string& src_file, size_t rowno)
        {
            auto breakip = vmm->env->program_debug_info->get_ip_by_src_location(
                src_file, rowno, true, false);

            if (!breakip.empty())
            {
                std::vector<size_t> breakips;
                for (auto bip : breakip)
                    breakips.push_back(bip);
                set_breakpoint_with_ips(vmm, src_file, rowno, breakips);
                return true;
            }
            return false;
        }
        bool clear_breakpoint(wo::vmbase* vmm, size_t breakid)
        {
            auto& context = env_context[vmm->env];
            if (context.break_point_traps.size() > breakid)
            {
                for (auto& bip : context.break_point_traps[breakid].m_break_ips)
                    context.break_ips.erase(bip);

                context.break_point_traps.erase(context.break_point_traps.begin() + breakid);
                return true;
            }
            return false;
        }
        void breakdown_immediately()
        {
            breakdown_temp_immediately = true;
        }
        void breakdown_at_vm_immediately(wo::vmbase* vmm)
        {
            focus_on_vm = vmm;
            breakdown_temp_for_stepir = true;
        }
    private:
        void command_help()
        {
            wo_stdout <<
                R"(Woolang debuggee tool command list:

COMMAND_NAME    SHORT_COMMAND   ARGUMENT    DESCRIBE
------------------------------------------------------------------------------
break           b               <file line>   Set a breakpoint at the specified
                                <funcname>  location.

callstack       cs/bt           [max = 8]     Get current VM's callstacks.

clear           cls                           Clean the screen.

continue        c                             Continue to run.

deletebreak     delbreak        <breakid>     Delete a breakpoint.

detach                                        Detach debuggee.

disassemble     dis             [funcname]    Dump current VM's running ir-codes.
                                    or
                                --all
                                    or
                                [offset length]

exit                                          Invoke _Exit(0) to shutdown.

frame           f               <frameid>     Switch to a call frame.

global          g               <offset>      Display global data.

halt                            [id]            Halt specified or current vm.

help            ?                             Get help informations.

list            l               <listitem>    List something, such as:
                                            break, var, vm(thread)

next            n                             Execute next line of src.

print           p               <varname>     Print the value.

profiler                        start         Collect runtime cost in following 
                                [time = 1.] 'time' sec(s), only collect current 
                                            env's profiler data.
                                    or
                                review <fn>   Get exclusive detail after profiler

quit                                          Stop all vm to exit.

return          r                             Execute to the return of this fun
                                            -ction.

source          src             [file name]   Get current source
                                [range = 5]

stackframe      sf                            Get current function's stack frame.

state           st              [id]          Get VM's register & interrupt state.

step            s                             Execute next line of src, will step
                                            in functions.

stepir          si                            Execute next command.

thread          vm              <id>          Continue and break at specified vm.
whereis                         <ipoffset>    Find the function that the ipoffset
                                            belong to.
)"
<< wo_endl;
        }

        static std::vector<std::string> get_and_split_line()
        {
            std::vector<std::string> result;

            std::string inputstr;
            std::getline(std::cin, inputstr);

            for (auto fnd = inputstr.begin(); fnd != inputstr.end(); fnd++)
            {
                auto readed_ch = *fnd;

                if ((readed_ch & 0x80) || !isspace(readed_ch & 0x7f))
                {
                    std::string read_word;

                    while (fnd != inputstr.end())
                    {
                        if ((readed_ch & 0x80) || !isspace(readed_ch & 0x7f))
                        {
                            readed_ch = *fnd;
                            read_word += readed_ch;
                            fnd++;
                        }
                        else
                            break;
                    }
                    fnd--;

                    result.push_back(read_word);
                }
            }

            return result;
        }

        template<typename T>
        static bool need_possiable_input(std::vector<std::string>& inputbuf, T& out)
        {
            using namespace std;

            if (inputbuf.size())
            {
                std::stringstream ss;
                ss << inputbuf.front();
                ss >> out;
                inputbuf.erase(inputbuf.begin());
                return true;
            }

            return false;
        }

        struct function_code_info
        {
            std::string func_sig;
            size_t command_ip_begin;
            size_t command_ip_end;
            size_t rt_ip_begin;
            size_t rt_ip_end;
        };
        std::vector<function_code_info> search_function_begin_rtip_scope_with_name(wo::vmbase* vmm, const std::string& funcname, bool fullmatch)
        {
            std::vector<function_code_info> result;
            for (auto& [funcsign_name, pos] : vmm->env->program_debug_info->_function_ip_data_buf)
            {
                if (fullmatch ? (funcsign_name == funcname) : (funcsign_name.rfind(funcname) != std::string::npos))
                {
                    auto begin_rt_ip = vmm->env->program_debug_info->get_runtime_ip_by_ip(pos.ir_begin);
                    auto end_rt_ip = vmm->env->program_debug_info->get_runtime_ip_by_ip(pos.ir_end);
                    result.push_back({ funcsign_name,pos.ir_begin,pos.ir_end, begin_rt_ip , end_rt_ip });
                }
            }
            return result;
        }

        bool breakdown_temp_immediately = false;
        bool breakdown_temp_for_stepir = false;
        bool breakdown_temp_for_return = false;
        size_t breakdown_temp_for_return_callstackdepth = 0;

        bool breakdown_temp_for_step = false;

        bool breakdown_temp_for_next = false;
        size_t breakdown_temp_for_next_callstackdepth = 0;

        size_t breakdown_temp_for_step_row_begin = 0;
        size_t breakdown_temp_for_step_row_end = 0;
        size_t breakdown_temp_for_step_col_begin = 0;
        size_t breakdown_temp_for_step_col_end = 0;
        std::string breakdown_temp_for_step_srcfile = "";
        std::string last_command = "";

        const wo::byte_t* current_runtime_ip;
        wo::value* current_frame_sp;
        wo::value* current_frame_bp;

        wo::vmbase* focus_on_vm = nullptr;

        static std::string _safe_cast_value_to_string(wo::value* val)
        {
            if (val->type >= wo::value::valuetype::need_gc_flag)
            {
                [[maybe_unused]] gcbase::unit_attrib* _attr;
                // NOTE: It's safe to get gcunit, all val here is read from vm, 
                //      it would not be free after fetch.
                wo::gcbase* gc_unit_base_addr = val->get_gcunit_and_attrib_ref(&_attr);

                if (gc_unit_base_addr == nullptr)
                    return "<released>";

                bool type_match = false;
                switch (val->type)
                {
                case wo::value::valuetype::string_type:
                    if (dynamic_cast<wo::string_t*>(gc_unit_base_addr) != nullptr)
                        type_match = true;
                    break;
                case wo::value::valuetype::dict_type:
                    if (dynamic_cast<wo::directory_t*>(gc_unit_base_addr) != nullptr)
                        type_match = true;
                    break;
                case wo::value::valuetype::array_type:
                    if (dynamic_cast<wo::array_t*>(gc_unit_base_addr) != nullptr)
                        type_match = true;
                    break;
                case wo::value::valuetype::gchandle_type:
                    if (dynamic_cast<wo::gchandle_t*>(gc_unit_base_addr) != nullptr)
                        type_match = true;
                    break;
                case wo::value::valuetype::closure_type:
                    if (dynamic_cast<wo::closure_t*>(gc_unit_base_addr) != nullptr)
                        type_match = true;
                    break;
                case wo::value::valuetype::struct_type:
                    if (dynamic_cast<wo::structure_t*>(gc_unit_base_addr) != nullptr)
                        type_match = true;
                    break;
                default:
                    return "<unexpected type>";
                }
                if (!type_match)
                    return "<released>";
            }
            else
            {
                if (val->type != wo::value::valuetype::invalid
                    && val->type != wo::value::valuetype::integer_type
                    && val->type != wo::value::valuetype::real_type
                    && val->type != wo::value::valuetype::handle_type
                    && val->type != wo::value::valuetype::bool_type)
                    return "<unexpected type>";
            }

            auto result = std::string("<")
                + wo_type_name((wo_type_t)val->type) + "> "
                + wo_cast_string(std::launder(reinterpret_cast<wo_value>(val)));

            return result;
        }

        void display_variable(wo::vmbase* vmm, wo::program_debug_data_info::function_symbol_infor::variable_symbol_infor& varinfo)
        {
            // auto real_offset = -varinfo.bp_offset;
            auto value_in_stack = current_frame_bp + varinfo.bp_offset;
            wo_stdout << varinfo.name << " define at line: " << varinfo.define_place << wo_endl;
            if (varinfo.bp_offset >= 0)
                wo_stdout << "[bp+" << varinfo.bp_offset << "]: ";
            else
                wo_stdout << "[bp-" << -varinfo.bp_offset << "]: ";

            if (value_in_stack <= current_frame_sp)
                wo_stdout << value_in_stack << " (not in stack)" << wo_endl;
            else
                wo_stdout << value_in_stack << " " << _safe_cast_value_to_string(value_in_stack) << wo_endl;
        }
        void display_variable(wo::vmbase* vmm, size_t global_offset)
        {
            wo_stdout << "g[" << global_offset << "]: ";
            if (global_offset < vmm->env->constant_and_global_value_takeplace_count)
            {
                auto* valueaddr = vmm->env->constant_and_global_storage + vmm->env->constant_value_count + global_offset;
                wo_stdout << valueaddr << " " << _safe_cast_value_to_string(valueaddr) << wo_endl;
            }
            else
                wo_stdout << "<out of range>" << wo_endl;
        }
        bool debug_command(vmbase* vmm)
        {
            // Clear stdout
            printf(ANSI_HIG "> " ANSI_HIY); fflush(stdout);

            // Clear stdin
            std::cin.clear();
            char _useless_for_clear = 0;
            while (std::cin.readsome(&_useless_for_clear, 1));
            (void)_useless_for_clear;

            // Receive command from stdin.
            auto inputbuf = get_and_split_line();

            std::string main_command;
            if (need_possiable_input(inputbuf, main_command))
                last_command = main_command;
            else
                main_command = last_command;

            if (!main_command.empty())
            {
                auto& context = env_context[vmm->env];

                printf(ANSI_RST);
                if (main_command == "?" || main_command == "help")
                    command_help();
                else if (main_command == "c" || main_command == "continue")
                {
                    printf(ANSI_HIG "Continue running...\n" ANSI_RST);
                    return false;
                }
                else if (main_command == "vm" || main_command == "thread")
                {
                    size_t vmid = 0;
                    bool continue_run = false;
                    if (need_possiable_input(inputbuf, vmid))
                    {
                        wo::assure_leave_this_thread_vm_shared_mutex::leave_context ctx;
                        if (wo::vmbase::_alive_vm_list_mx.try_lock_shared(&ctx))
                        {
                            if (vmid < wo::vmbase::_alive_vm_list.size())
                            {
                                auto vmidx = wo::vmbase::_alive_vm_list.begin();
                                for (size_t i = 0; i < vmid; ++i)
                                    ++vmidx;

                                focus_on_vm = *vmidx;
                                breakdown_temp_for_stepir = true;

                                printf(ANSI_HIG "Continue running and break at vm (%zu:%p)...\n" ANSI_RST, vmid, focus_on_vm);
                                continue_run = true;
                            }
                            else
                                printf(ANSI_HIR "You must input valid vm id.\n" ANSI_RST);

                            wo::vmbase::_alive_vm_list_mx.unlock_shared(ctx);
                        }
                    }
                    else
                        printf(ANSI_HIR "You must input vm id to break.\n" ANSI_RST);

                    if (continue_run)
                        return false;
                    goto continue_run_command;

                }
                else if (main_command == "st" || main_command == "state")
                {
                    wo::assure_leave_this_thread_vm_shared_lock sg1(wo::vmbase::_alive_vm_list_mx);

                    wo::vmbase* target_vm = nullptr;

                    size_t vmid = 0;
                    if (need_possiable_input(inputbuf, vmid))
                    {
                        if (vmid < wo::vmbase::_alive_vm_list.size())
                        {
                            auto vmidx = wo::vmbase::_alive_vm_list.begin();
                            for (size_t i = 0; i < vmid; ++i)
                                ++vmidx;

                            target_vm = *vmidx;
                        }
                        else
                            printf(ANSI_HIR "You must input valid vm id.\n" ANSI_RST);
                    }
                    else
                        target_vm = vmm;

                    if (target_vm != nullptr)
                    {
                        wo_stdout
                            << ANSI_HIG
                            << target_vm->env->real_register_count
                            << ANSI_HIY " register(s) in total:" ANSI_RST
                            << wo_endl;
                        printf("%-15s%-20s%-20s\n", "RegisterID", "Name", "Value");
                        for (size_t reg_idx = 0; reg_idx < target_vm->env->real_register_count; ++reg_idx)
                        {
                            printf("%-15zu", reg_idx);
                            switch (reg_idx)
                            {
                            case wo::opnum::reg::spreg::r0:
                            case wo::opnum::reg::spreg::r1:
                            case wo::opnum::reg::spreg::r2:
                            case wo::opnum::reg::spreg::r3:
                            case wo::opnum::reg::spreg::r4:
                            case wo::opnum::reg::spreg::r5:
                            case wo::opnum::reg::spreg::r6:
                            case wo::opnum::reg::spreg::r7:
                            case wo::opnum::reg::spreg::r8:
                            case wo::opnum::reg::spreg::r9:
                            case wo::opnum::reg::spreg::r10:
                            case wo::opnum::reg::spreg::r11:
                            case wo::opnum::reg::spreg::r12:
                            case wo::opnum::reg::spreg::r13:
                            case wo::opnum::reg::spreg::r14:
                            case wo::opnum::reg::spreg::r15:
                                printf("%-20s", ("R" + std::to_string(reg_idx - wo::opnum::reg::spreg::r0)).c_str()); break;
                            case wo::opnum::reg::spreg::t0:
                            case wo::opnum::reg::spreg::t1:
                            case wo::opnum::reg::spreg::t2:
                            case wo::opnum::reg::spreg::t3:
                            case wo::opnum::reg::spreg::t4:
                            case wo::opnum::reg::spreg::t5:
                            case wo::opnum::reg::spreg::t6:
                            case wo::opnum::reg::spreg::t7:
                            case wo::opnum::reg::spreg::t8:
                            case wo::opnum::reg::spreg::t9:
                            case wo::opnum::reg::spreg::t10:
                            case wo::opnum::reg::spreg::t11:
                            case wo::opnum::reg::spreg::t12:
                            case wo::opnum::reg::spreg::t13:
                            case wo::opnum::reg::spreg::t14:
                            case wo::opnum::reg::spreg::t15:
                                printf("%-20s", ("T" + std::to_string(reg_idx - wo::opnum::reg::spreg::t0)).c_str()); break;
                            case wo::opnum::reg::spreg::cr:
                                printf("%-20s", "OpTraceResult(CR)"); break;
                            case wo::opnum::reg::spreg::tc:
                                printf("%-20s", "ArgumentCount(TC)"); break;
                            case wo::opnum::reg::spreg::er:
                                printf("%-20s", "ExceptionInfo(ER)"); break;
                            case wo::opnum::reg::spreg::ni:
                                printf("%-20s", "NilConstant(NI)"); break;
                            case wo::opnum::reg::spreg::pm:
                                printf("%-20s", "PatternMatch(PM)"); break;
                            case wo::opnum::reg::spreg::tp:
                                printf("%-20s", "Temporary(TP)"); break;
                            default:
                                printf("%-20s", "---"); break;
                            }

                            printf("%-20s\n", _safe_cast_value_to_string(
                                &target_vm->register_storage[reg_idx]).c_str());
                        }

                    }
                }
                else if (main_command == "r" || main_command == "return")
                {
                    breakdown_temp_for_return = true;
                    breakdown_temp_for_return_callstackdepth = vmm->callstack_layer();
                    goto continue_run_command;
                }
                else if (main_command == "f" || main_command == "frame")
                {
                    size_t framelayer = 0;
                    if (need_possiable_input(inputbuf, framelayer))
                    {
                        current_runtime_ip = vmm->ip;
                        current_frame_sp = vmm->sp;
                        current_frame_bp = vmm->bp;

                        size_t current_frame = 0;
                        while (framelayer--)
                        {
                            auto tmp_sp = current_frame_bp + 1;
                            if ((tmp_sp->type & (~1)) == value::valuetype::callstack)
                            {
                                current_frame++;
                                current_frame_sp = tmp_sp;
                                current_frame_bp = vmm->sb - tmp_sp->vmcallstack.bp;
                                current_runtime_ip = vmm->env->rt_codes + tmp_sp->vmcallstack.ret_ip;
                            }
                            else
                                break;
                        }
                        printf("Now at: frame " ANSI_HIY "%zu\n" ANSI_RST, current_frame);

                    }
                    else
                        printf(ANSI_HIR "You must input frame number.\n" ANSI_RST);
                }
                else if (main_command == "dis" || main_command == "disassemble")
                {
                    std::string function_name_or_ip_offset;
                    if (need_possiable_input(inputbuf, function_name_or_ip_offset))
                    {
                        bool is_number = !function_name_or_ip_offset.empty();
                        for (char ch : function_name_or_ip_offset)
                        {
                            if (!isdigit(ch))
                            {
                                is_number = false;
                                break;
                            }
                        }

                        if (is_number)
                        {
                            size_t begin_offset = (size_t)atoll(function_name_or_ip_offset.c_str());
                            if (need_possiable_input(inputbuf, function_name_or_ip_offset))
                            {
                                size_t length = (size_t)atoll(function_name_or_ip_offset.c_str());
                                printf("Display +%04zu to +%04zu.\n",
                                    begin_offset,
                                    begin_offset + length);

                                vmm->dump_program_bin(
                                    std::min(begin_offset, vmm->env->rt_code_len),
                                    std::min(begin_offset + length, vmm->env->rt_code_len));
                            }
                            else
                                printf(ANSI_HIR "Missing length, command failed.\n" ANSI_RST);

                        }
                        else
                        {
                            if (function_name_or_ip_offset == "--all")
                            {
                                vmm->dump_program_bin();
                            }
                            else
                            {
                                if (vmm->env->program_debug_info == nullptr)
                                    printf(ANSI_HIR "No pdb found, command failed.\n" ANSI_RST);
                                else
                                {
                                    auto&& fndresult = search_function_begin_rtip_scope_with_name(vmm, function_name_or_ip_offset, false);
                                    wo_stdout << "Find " << fndresult.size() << " symbol(s):" << wo_endl;
                                    for (auto& funcinfo : fndresult)
                                    {
                                        wo_stdout << "In function: " << funcinfo.func_sig << wo_endl;
                                        vmm->dump_program_bin(funcinfo.rt_ip_begin, funcinfo.rt_ip_end);
                                    }
                                }
                            }
                        }
                    }
                    else
                    {
                        if (vmm->env->program_debug_info == nullptr)
                            printf(ANSI_HIR "No pdb found, command failed.\n" ANSI_RST);
                        else
                        {
                            auto&& funcname = vmm->env->program_debug_info->get_current_func_signature_by_runtime_ip(current_runtime_ip);
                            auto fnd = vmm->env->program_debug_info->_function_ip_data_buf.find(funcname);
                            if (fnd != vmm->env->program_debug_info->_function_ip_data_buf.end())
                            {
                                wo_stdout << "In function: " << funcname << wo_endl;
                                vmm->dump_program_bin(
                                    vmm->env->program_debug_info->get_runtime_ip_by_ip(fnd->second.ir_begin),
                                    vmm->env->program_debug_info->get_runtime_ip_by_ip(fnd->second.ir_end));
                            }
                            else
                            {
                                wo_stdout << "Unable to located function, display following 100 bytes." << wo_endl;
                                auto begin_offset = (size_t)(current_runtime_ip - vmm->env->rt_codes);
                                vmm->dump_program_bin(
                                    std::min(begin_offset, vmm->env->rt_code_len),
                                    std::min(begin_offset + 100, vmm->env->rt_code_len));
                            }
                        }
                    }
                }
                else if (main_command == "cs" || main_command == "bt" || main_command == "callstack")
                {
                    size_t max_layer;
                    if (!need_possiable_input(inputbuf, max_layer))
                        max_layer = 8;
                    vmm->dump_call_stack(max_layer, false);
                }
                else if (main_command == "profiler")
                {
                    if (vmm->env->program_debug_info == nullptr)
                        printf(ANSI_HIR "No pdb found, command failed.\n" ANSI_RST);
                    else
                    {
                        std::string command;
                        if (need_possiable_input(inputbuf, command))
                        {
                            if (command == "start")
                            {
                                double record_time;
                                if (!need_possiable_input(inputbuf, record_time))
                                    record_time = 1.0;

                                profiler_records.clear();
                                profiler_enabled = true;
                                profiler_last_sampling_times.clear();
                                profiler_until = _wo_inside_time_sec() + record_time;
                                profiler_total_count = 0;

                                return false;
                            }
                            else if (command == "review")
                            {
                                std::string specify_funcname;
                                if (!need_possiable_input(inputbuf, specify_funcname))
                                {
                                    printf(ANSI_HIR "You should input the function's name to review.\n" ANSI_RST);
                                }
                                else
                                {
                                    if (!profiler_records.empty())
                                    {
                                        for (auto& [funcname, record] : profiler_records)
                                        {
                                            if (funcname.find(specify_funcname) < funcname.size())
                                            {
                                                auto&& fndresult = search_function_begin_rtip_scope_with_name(vmm, funcname, true);

                                                if (fndresult.empty())
                                                    continue;

                                                wo_assert(fndresult.size() == 1);
                                                auto& func_info = fndresult.front();

                                                auto& src_begin = vmm->env->program_debug_info->get_src_location_by_runtime_ip(
                                                    vmm->env->rt_codes + func_info.rt_ip_begin);

                                                auto& src_end = vmm->env->program_debug_info->get_src_location_by_runtime_ip(
                                                    vmm->env->rt_codes + func_info.rt_ip_end);

                                                print_src_file(vmm, src_begin.source_file, 0, 0, 0, 0,
                                                    src_begin.begin_row_no, src_end.end_row_no, &record);
                                            }
                                        }
                                    }
                                    else
                                        printf(ANSI_HIR "No record, you should start profiler first.\n" ANSI_RST);
                                }
                            }
                            else
                                printf(ANSI_HIR "Unknown command for profiler, should be 'start' or 'review'.\n" ANSI_RST);
                        }
                        else
                            printf(ANSI_HIR "You should input sub command for profiler, should be 'start' or 'review'.\n" ANSI_RST);
                    }

                }
                else if (main_command == "break" || main_command == "b")
                {
                    if (vmm->env->program_debug_info == nullptr)
                        printf(ANSI_HIR "No pdb found, command failed.\n" ANSI_RST);
                    else
                    {
                        std::string filename_or_funcname;
                        if (need_possiable_input(inputbuf, filename_or_funcname))
                        {
                            size_t lineno;

                            bool result = false;

                            if (need_possiable_input(inputbuf, lineno))
                                result = set_breakpoint(vmm, filename_or_funcname, lineno -1);
                            else
                            {
                                for (auto ch : filename_or_funcname)
                                {
                                    if (ch < '0' || ch > '9')
                                    {
                                        auto&& fndresult = search_function_begin_rtip_scope_with_name(vmm, filename_or_funcname, false);
                                        wo_stdout << "Set breakpoint at " << fndresult.size() << " symbol(s):" << wo_endl;
                                        for (auto& funcinfo : fndresult)
                                        {
                                            wo_stdout << "In function: " << funcinfo.func_sig << wo_endl;

                                            auto frtip = vmm->env->rt_codes + vmm->env->program_debug_info->get_runtime_ip_by_ip(funcinfo.command_ip_begin);
                                            auto fip = vmm->env->program_debug_info->get_ip_by_runtime_ip(frtip);
                                            auto& srcinfo = vmm->env->program_debug_info->get_src_location_by_runtime_ip(frtip);
                                            set_breakpoint_with_ips(vmm, srcinfo.source_file, srcinfo.begin_row_no, { fip });
                                            //NOTE: some function's reserved stack op may be removed, so get real ir is needed..
                                        }
                                        goto need_next_command;
                                    }
                                }
                                ;
                                result = set_breakpoint(vmm,
                                    vmm->env->program_debug_info->get_src_location_by_runtime_ip(current_runtime_ip).source_file
                                    , (size_t)std::stoull(filename_or_funcname) - 1);

                            }

                            if (result)
                                wo_stdout << "OK!" << wo_endl;
                            else
                                wo_stdout << "FAIL!" << wo_endl;
                        }
                        else
                            printf(ANSI_HIR "You must input the file or function's name.\n" ANSI_RST);
                    }
                }
                else if (main_command == "halt")
                {
                    wo::assure_leave_this_thread_vm_shared_lock sg1(wo::vmbase::_alive_vm_list_mx);

                    wo::vmbase* target_vm = nullptr;

                    size_t vmid = 0;
                    if (need_possiable_input(inputbuf, vmid))
                    {
                        if (vmid < wo::vmbase::_alive_vm_list.size())
                        {
                            auto vmidx = wo::vmbase::_alive_vm_list.begin();
                            for (size_t i = 0; i < vmid; ++i)
                                ++vmidx;

                            target_vm = *vmidx;
                        }
                        else
                            printf(ANSI_HIR "You must input valid vm id.\n" ANSI_RST);
                    }
                    else
                        target_vm = vmm;

                    if (target_vm != nullptr)
                        target_vm->interrupt(wo::vmbase::vm_interrupt_type::ABORT_INTERRUPT);
                }
                else if (main_command == "quit")
                {
                    stop_attach_debuggee_for_exit_flag = true;
                    return false;
                }
                else if (main_command == "exit")
                {
                    _Exit(-1);
                    return false;
                }
                else if (main_command == "delbreak" || main_command == "deletebreak")
                {
                    size_t breakno;
                    if (need_possiable_input(inputbuf, breakno))
                    {
                        if (clear_breakpoint(vmm, breakno))
                            wo_stdout << "OK!" << wo_endl;
                        else
                            printf(ANSI_HIR "Unknown breakpoint id.\n" ANSI_RST);
                    }
                    else
                        printf(ANSI_HIR "You must input the breakpoint id.\n" ANSI_RST);
                }
                else if (main_command == "si" || main_command == "stepir")
                {
                    breakdown_temp_for_stepir = true;
                    goto continue_run_command;
                }
                else if (main_command == "cls" || main_command == "clear")
                {
#ifdef _WIN32
                    auto ignore = system("cls");
#else
                    auto ignore = system("clear");
#endif
                    (void)ignore;
                }
                else if (main_command == "s" || main_command == "step")
                {
                    if (vmm->env->program_debug_info == nullptr)
                        printf(ANSI_HIR "No pdb found, command failed.\n" ANSI_RST);
                    else
                    {
                        // Get current lineno

                        auto& loc = vmm->env->program_debug_info
                            ->get_src_location_by_runtime_ip(vmm->ip);

                        breakdown_temp_for_step = true;
                        breakdown_temp_for_step_row_begin = loc.begin_row_no;
                        breakdown_temp_for_step_row_end = loc.end_row_no;
                        breakdown_temp_for_step_col_begin = loc.begin_col_no;
                        breakdown_temp_for_step_col_end = loc.end_col_no;
                        breakdown_temp_for_step_srcfile = loc.source_file;

                        goto continue_run_command;
                    }
                }
                else if (main_command == "n" || main_command == "next")
                {
                    if (vmm->env->program_debug_info == nullptr)
                        printf(ANSI_HIR "No pdb found, command failed.\n" ANSI_RST);
                    else
                    {
                        // Get current lineno
                        auto& loc = vmm->env->program_debug_info
                            ->get_src_location_by_runtime_ip(vmm->ip);

                        breakdown_temp_for_next = true;
                        breakdown_temp_for_next_callstackdepth = vmm->callstack_layer();
                        breakdown_temp_for_step_row_begin = loc.begin_row_no;
                        breakdown_temp_for_step_row_end = loc.end_row_no;
                        breakdown_temp_for_step_col_begin = loc.begin_col_no;
                        breakdown_temp_for_step_col_end = loc.end_col_no;
                        breakdown_temp_for_step_srcfile = loc.source_file;

                        goto continue_run_command;
                    }
                }
                else if (main_command == "sf" || main_command == "stackframe")
                {
                    auto* currentsp = current_frame_sp;
                    while ((++currentsp) <= current_frame_bp)
                    {
                        wo_stdout
                            << "[bp-" << current_frame_bp - currentsp << "]: "
                            << currentsp << " " << _safe_cast_value_to_string(currentsp)
                            << wo_endl;
                    }

                }
                else if (main_command == "p" || main_command == "print")
                {
                    if (vmm->env->program_debug_info == nullptr)
                        printf(ANSI_HIR "No pdb found, command failed.\n" ANSI_RST);
                    else
                    {
                        std::string varname;
                        if (need_possiable_input(inputbuf, varname))
                        {
                            if (vmm->env)
                            {
                                auto&& funcname = vmm->env->program_debug_info->get_current_func_signature_by_runtime_ip(current_runtime_ip);
                                auto fnd = vmm->env->program_debug_info->_function_ip_data_buf.find(funcname);
                                if (fnd != vmm->env->program_debug_info->_function_ip_data_buf.end())
                                {
                                    if (auto vfnd = fnd->second.variables.find(varname);
                                        vfnd != fnd->second.variables.end())
                                    {
                                        for (auto& varinfo : vfnd->second)
                                        {
                                            display_variable(vmm, varinfo);
                                        }
                                    }
                                    else
                                        printf(ANSI_HIR "Cannot find '%s' in function '%s'.\n" ANSI_RST,
                                            varname.c_str(), funcname.c_str());
                                }
                                else
                                    printf(ANSI_HIR "Invalid function.\n" ANSI_RST);
                            }
                        }
                        else
                            printf(ANSI_HIR "You must input the variable name.\n" ANSI_RST);
                    }
                }
                else if (main_command == "src" || main_command == "source")
                {
                    if (vmm->env->program_debug_info == nullptr)
                        printf(ANSI_HIR "No pdb found, command failed.\n" ANSI_RST);
                    else
                    {
                        std::string filename;
                        size_t display_range = 5;
                        auto& loc = vmm->env->program_debug_info->get_src_location_by_runtime_ip(current_runtime_ip);
                        if (need_possiable_input(inputbuf, filename))
                        {
                            for (auto ch : filename)
                            {
                                if (ch < '0' || ch > '9')
                                {
                                    if (filename == loc.source_file)
                                        print_src_file(vmm, filename, loc.begin_row_no, loc.end_row_no, loc.begin_col_no, loc.end_col_no);
                                    else
                                        print_src_file(vmm, filename, 0, 0, 0, 0);
                                    goto need_next_command;
                                }
                            }
                            display_range = (size_t)std::stoull(filename);
                        }

                        print_src_file(vmm, loc.source_file, loc.begin_row_no, loc.end_row_no, loc.begin_col_no, loc.end_col_no,
                            (loc.begin_row_no < display_range / 2 ? 0 : loc.begin_row_no - display_range / 2), loc.end_row_no + display_range / 2);

                    }
                }
                else if (main_command == "global" || main_command == "g")
                {
                    size_t offset;
                    if (!need_possiable_input(inputbuf, offset))
                        printf(ANSI_HIR "Need to specify offset for command 'global'.\n" ANSI_RST);
                    else
                    {
                        display_variable(vmm, offset);
                    }
                }
                else if (main_command == "whereis")
                {
                    size_t offset;
                    if (!need_possiable_input(inputbuf, offset))
                        printf(ANSI_HIR "Need to specify ipoffset for command 'whereis'.\n" ANSI_RST);
                    else
                    {
                        if (vmm->env->program_debug_info == nullptr)
                            printf(ANSI_HIR "No pdb found, command failed.\n" ANSI_RST);
                        else
                        {
                            std::string func_name = vmm->env->program_debug_info
                                ->get_current_func_signature_by_runtime_ip(vmm->env->rt_codes + offset);

                            printf("The ip offset %zu is in function: `" ANSI_HIG "%s" ANSI_RST "`\n",
                                offset, func_name.c_str());
                        }
                    }
                }
                else if (main_command == "l" || main_command == "list")
                {
                    std::string list_what;
                    if (need_possiable_input(inputbuf, list_what))
                    {
                        if (list_what == "break")
                        {
                            if (vmm->env->program_debug_info == nullptr)
                                printf(ANSI_HIR "No pdb found, command failed.\n" ANSI_RST);
                            else
                            {
                                size_t id = 0;
                                for (auto& break_info : context.break_point_traps)
                                {
                                    wo_stdout << (id++) << " :\t" << break_info.m_filepath << " (" << break_info.m_row_no << ")" << wo_endl;;
                                }
                            }
                        }
                        else if (list_what == "var")
                        {
                            if (vmm->env->program_debug_info == nullptr)
                                printf(ANSI_HIR "No pdb found, command failed.\n" ANSI_RST);
                            else
                            {
                                auto&& funcname = vmm->env->program_debug_info->get_current_func_signature_by_runtime_ip(current_runtime_ip);
                                auto fnd = vmm->env->program_debug_info->_function_ip_data_buf.find(funcname);
                                if (fnd != vmm->env->program_debug_info->_function_ip_data_buf.end())
                                {
                                    for (auto& [varname, varinfos] : fnd->second.variables)
                                    {
                                        for (auto& varinfo : varinfos)
                                        {
                                            display_variable(vmm, varinfo);
                                        }
                                    }
                                }
                                else
                                    printf(ANSI_HIR "Invalid function.\n" ANSI_RST);
                            }
                        }
                        else if (list_what == "vm" || list_what == "thread")
                        {
                            wo::assure_leave_this_thread_vm_shared_mutex::leave_context ctx;
                            if (wo::vmbase::_alive_vm_list_mx.try_lock_shared(&ctx))
                            {
                                size_t vmcount = 0;
                                for (auto vms : wo::vmbase::_alive_vm_list)
                                {
                                    wo_stdout << ANSI_HIY "thread(vm) #" ANSI_HIG << vmcount << ANSI_RST " " << vms << " " << wo_endl;

                                    auto usage = ((double)(vms->sb - vms->sp)) / (double)vms->stack_size;
                                    wo_stdout << "stack usage: " << usage * 100. << "%" << wo_endl;

                                    if (vms->env->rt_codes == vms->ip
                                        || vms->ip == vms->env->rt_codes + vms->env->rt_code_len
                                        || vms->vm_interrupt & vmbase::PENDING_INTERRUPT)
                                        wo_stdout << "(pending)" << wo_endl;
                                    else if (vms->ip < vms->env->rt_codes
                                        || vms->ip > vms->env->rt_codes + vms->env->rt_code_len)
                                    {
                                        wo_stdout << "(leaving)" << wo_endl;
                                        vms->dump_call_stack(5, false);
                                    }
                                    else
                                    {
                                        wo_stdout << "(running)" << wo_endl;
                                        vms->dump_call_stack(5, false);
                                    }

                                    vmcount++;
                                }

                                wo::vmbase::_alive_vm_list_mx.unlock_shared(ctx);
                            }
                            else
                                printf(ANSI_HIR "Failed to get thread(virtual machine) list, may busy.\n" ANSI_RST);
                        }
                        else
                            printf(ANSI_HIR "Unknown type to list.\n" ANSI_RST);
                    }
                    else
                        printf(ANSI_HIR "You must input something to list.\n" ANSI_RST);
                }
                else if (main_command == "detach")
                {
                    printf(ANSI_HIG "Detach debuggee, continue run.\n" ANSI_RST);
                    stop_for_detach_debuggee = true;

                    wo::vmbase::attach_debuggee(nullptr)->_abandon();

                    return false;
                }
                else
                    printf(ANSI_HIR "Unknown debug command, please input 'help' for more informations.\n" ANSI_RST);
            }

        need_next_command:

            printf(ANSI_RST);
            std::cin.clear();
            while (std::cin.readsome(&_useless_for_clear, 1));

            return true;

        continue_run_command:

            printf(ANSI_RST);
            std::cin.clear();
            while (std::cin.readsome(&_useless_for_clear, 1));

            return false;
        }
        std::optional<size_t> print_src_file_print_lineno(
            wo::vmbase* vmm,
            const std::string& filepath,
            size_t current_row_no,
            cpu_profiler_record_infornmation* info)
        {
            auto& context = env_context[vmm->env];

            std::optional<size_t> breakpoint_found_id = std::nullopt;
            size_t finding_id = 0;
            for (auto& breakinfo : context.break_point_traps)
            {
                if (breakinfo.m_row_no == current_row_no && breakinfo.m_filepath == filepath)
                {
                    breakpoint_found_id = finding_id;
                    break;
                }
                ++finding_id;
            }

            if (breakpoint_found_id.has_value())
                printf(ANSI_BHIR ANSI_WHI "%-5zu " ANSI_RST "| ", current_row_no + 1);
            else
                printf(ANSI_HIM "%-5zu " ANSI_RST "| ", current_row_no + 1);

            if (info != nullptr)
            {
                auto fnd = info->m_exclusive_record.find(current_row_no);
                if (fnd != info->m_exclusive_record.end())
                {
                    auto deg = (double)fnd->second / (double)info->m_exclusive * 100.0;
                    printf(ANSI_HIM "%f%%" ANSI_RST, deg);
                }
            }

            return breakpoint_found_id;
        }
        void print_src_file(
            wo::vmbase* vmm,
            const std::string& filepath,
            size_t hightlight_range_begin_row,
            size_t hightlight_range_end_row,
            size_t hightlight_range_begin_col,
            size_t hightlight_range_end_col,
            size_t from = 0,
            size_t to = SIZE_MAX,
            cpu_profiler_record_infornmation* info = nullptr)
        {
            std::string srcfile, src_full_path;
            if (!wo::check_and_read_virtual_source(filepath, std::nullopt, &src_full_path, &srcfile))
                printf(ANSI_HIR "Cannot open source: '%s'.\n" ANSI_RST, filepath.c_str());
            else
            {
                wo_stdout << filepath << " from: " << from + 1 << " to: ";
                if (to == SIZE_MAX)
                    wo_stdout << "<file end>";
                else
                    wo_stdout << to + 1;

                wo_stdout << wo_endl;

                // print source;
                // here is a easy lexer..
                size_t current_row_no = 0;
                size_t current_col_no = 0;
                std::optional<size_t> last_line_is_breakline = std::nullopt;

                if (from <= current_row_no && current_row_no <= to)
                {
                    last_line_is_breakline = print_src_file_print_lineno(
                        vmm, filepath, current_row_no, info);
                }
                for (size_t index = 0; index < srcfile.size(); index++)
                {
                    const char* chs = srcfile.c_str() + index;
                    const size_t len = u8charnlen(chs, srcfile.length() - index);

                    if (len == 1 && (*chs == '\n' || *chs == '\r'))
                    {
                        current_col_no = 0;
                        ++current_row_no;

                        // If next line in range, display the line number & breakpoint state & profiler result.
                        if (from <= current_row_no && current_row_no <= to)
                        {
                            if (last_line_is_breakline.has_value())
                                printf("    " ANSI_HIR "# Breakpoint %zu" ANSI_RST, 
                                    last_line_is_breakline.value());

                            wo_stdout << ANSI_RST << wo_endl;
                            last_line_is_breakline = print_src_file_print_lineno(
                                vmm, filepath, current_row_no, info);
                        }

                        if (*chs == '\r' 
                            && index + 1 < srcfile.size() 
                            && srcfile[index + 1] == '\n')
                            index++;

                        continue;
                    }

                    if (from <= current_row_no && current_row_no <= to)
                    {
                        bool print_inv = false;

                        if (current_row_no >= hightlight_range_begin_row && current_row_no <= hightlight_range_end_row)
                        {
                            print_inv = true;

                            if ((current_row_no == hightlight_range_begin_row
                                && current_col_no < hightlight_range_begin_col)
                                || (current_row_no == hightlight_range_end_row
                                    && current_col_no >= hightlight_range_end_col))
                                print_inv = false;
                        }

                        if (print_inv)
                            printf(ANSI_INV);
                        else
                            printf(ANSI_RST);

                        wo_stdout << std::string_view(chs, len);
                    }
                    ++current_col_no;
                }
            }
            wo_stdout << ANSI_RST << wo_endl;
        }

        virtual void debug_interrupt(vmbase* vmm) override
        {
            do
            {
                do
                {
                    if (stop_attach_debuggee_for_exit_flag || stop_for_detach_debuggee)
                        return;

                } while (0);

                if (profiler_enabled)
                {
                    const auto sampling_interval = 0.001;
                    auto current_time = _wo_inside_time_sec();
                    if (current_time > profiler_until)
                    {
                        wo_stdout << ANSI_HIG "CPU Profiler Report" ANSI_RST << wo_endl;
                        wo_stdout << "======================================================" << wo_endl;
                        wo_stdout << "Total " ANSI_HIC << profiler_total_count << ANSI_RST " samples" << wo_endl;
                        wo_stdout << "------------------------------------------------------" << wo_endl;
                        wo_stdout << "Function Name\t\t\tInclusive Prop.\tExclusive Prop.\n" << wo_endl;
                        std::vector<std::pair<const std::string, cpu_profiler_record_infornmation>*> results;
                        for (auto& record : profiler_records)
                        {
                            results.push_back(&record);
                        }
                        std::sort(results.begin(), results.end(),
                            [](
                                std::pair<const std::string, cpu_profiler_record_infornmation>* a,
                                std::pair<const std::string, cpu_profiler_record_infornmation>* b)
                            {
                                return a->second.m_exclusive > b->second.m_exclusive;
                            });

                        for (auto* funcinfo : results)
                        {
                            printf(ANSI_HIG "%s" ANSI_RST "\n%-30s\t%f%%\t%f%%\n",
                                funcinfo->first.c_str(),
                                "",
                                100.0 * (double)funcinfo->second.m_inclusive / (double)profiler_total_count,
                                100.0 * (double)funcinfo->second.m_exclusive / (double)profiler_total_count
                            );
                        }
                        wo_stdout << wo_endl;

                        breakdown_temp_for_stepir = true;
                        profiler_enabled = false;
                    }
                    if (current_time > profiler_last_sampling_times[vmm] + sampling_interval)
                    {
                        profiler_last_sampling_times[vmm] = current_time + sampling_interval;
                        auto&& calls = vmm->dump_call_stack_func_info(SIZE_MAX, true, nullptr);

                        if (calls.empty() == false)
                        {
                            profiler_total_count += 1;

                            std::unordered_set<std::string> m_unique_callstack;
                            for (auto& info : calls)
                                m_unique_callstack.insert(info.m_func_name);

                            for (auto& funcname : m_unique_callstack)
                                profiler_records[funcname].m_inclusive++;

                            profiler_records[calls.front().m_func_name].m_exclusive++;
                            profiler_records[calls.front().m_func_name].m_exclusive_record[calls.front().m_row]++;
                        }
                    }

                    break;
                }

                const byte_t* next_execute_ip = vmm->ip;
                auto next_execute_ip_diff = vmm->ip - vmm->env->rt_codes;

                size_t command_ip = 0;
                const program_debug_data_info::location* loc = nullptr;
                if (vmm->env->program_debug_info != nullptr)
                {
                    command_ip = vmm->env->program_debug_info->get_ip_by_runtime_ip(next_execute_ip);
                    loc = &vmm->env->program_debug_info->get_src_location_by_runtime_ip(next_execute_ip);
                }
                else
                    loc = &wo::program_debug_data_info::FAIL_LOC;

                // check breakpoint..
                auto& context = env_context[vmm->env];

                if (stop_attach_debuggee_for_exit_flag || stop_for_detach_debuggee)
                    return;

                if ((
                    (breakdown_temp_for_stepir
                        || (breakdown_temp_for_step
                            && (loc->begin_row_no != breakdown_temp_for_step_row_begin
                                || loc->end_row_no != breakdown_temp_for_step_row_end
                                || loc->begin_col_no != breakdown_temp_for_step_col_begin
                                || loc->end_col_no != breakdown_temp_for_step_col_end
                                || loc->source_file != breakdown_temp_for_step_srcfile))
                        || (breakdown_temp_for_next
                            && vmm->callstack_layer() <= breakdown_temp_for_next_callstackdepth
                            && (loc->begin_row_no != breakdown_temp_for_step_row_begin
                                || loc->end_row_no != breakdown_temp_for_step_row_end
                                || loc->begin_col_no != breakdown_temp_for_step_col_begin
                                || loc->end_col_no != breakdown_temp_for_step_col_end
                                || loc->source_file != breakdown_temp_for_step_srcfile))
                        || (breakdown_temp_for_return
                            && vmm->callstack_layer() < breakdown_temp_for_return_callstackdepth)
                        ) && (focus_on_vm == vmm)
                    )
                    || context.break_ips.find(command_ip) != context.break_ips.end()
                    || breakdown_temp_immediately)
                {
                    breakdown_temp_for_stepir = false;
                    breakdown_temp_for_step = false;
                    breakdown_temp_for_next = false;
                    breakdown_temp_for_return = false;
                    breakdown_temp_immediately = false;
                    focus_on_vm = vmm;

                    current_frame_bp = vmm->bp;
                    current_frame_sp = vmm->sp;
                    current_runtime_ip = vmm->ip;

                    printf(ANSI_HIY "Breakdown: " ANSI_RST "+%04d: at " ANSI_HIG "%s" ANSI_RST "(" ANSI_HIY "%zu" ANSI_RST ", " ANSI_HIY "%zu" ANSI_RST ")\n"
                        "in function: " ANSI_HIG " %s\n" ANSI_RST
                        "in virtual-machine: " ANSI_HIG " %p\n" ANSI_RST,

                        (int)next_execute_ip_diff,
                        loc->source_file.c_str(),
                        loc->begin_row_no + 1,
                        loc->begin_col_no + 1,
                        vmm->env->program_debug_info == nullptr
                        ? "__unknown_func_without_pdb_"
                        : vmm->env->program_debug_info->get_current_func_signature_by_runtime_ip(next_execute_ip).c_str(),
                        vmm
                    );
                    if (vmm->env->program_debug_info != nullptr)
                    {
                        printf("-------------------------------------------\n");
                        auto& loc = vmm->env->program_debug_info->get_src_location_by_runtime_ip(current_runtime_ip);
                        print_src_file(
                            vmm,
                            loc.source_file,
                            loc.begin_row_no,
                            loc.end_row_no,
                            loc.begin_col_no,
                            loc.end_col_no,
                            std::max((size_t)2, loc.begin_row_no) - 2,
                            loc.end_row_no + 2);
                    }
                    printf("===========================================\n");

                    if (first_time_to_breakdown)
                    {
                        first_time_to_breakdown = false;
                        printf(ANSI_HIY "Note" ANSI_RST ": You can input '" ANSI_HIR "?" ANSI_RST "' for more informations.\n");
                    }

                    char _useless_for_clear = 0;
                    std::cin.clear();
                    while (std::cin.readsome(&_useless_for_clear, 1));

                    while (debug_command(vmm))
                    {
                        // ...?
                    }
                }

            } while (0);

            if (stop_attach_debuggee_for_exit_flag)
            {
                wo_abort_all_vm();
                return;
            }
        }
    };
}
