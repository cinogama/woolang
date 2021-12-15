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
        void breakdown_immediately()
        {
            breakdown_temp_for_stepir = true;
        }
    private:
        void command_help()
        {
            std::cout <<
                R"(RestorableScene debuggee tool command list:

COMMAND_NAME    SHORT_COMMAND   ARGUMENT    DESCRIBE
------------------------------------------------------------------------------
break                           <file line>   Set a breakpoint at the specified
                                <funcname>  location.
callstack       cs              [max = 8]     Get current VM's callstacks.
frame           f               <frameid>     Switch to a call frame.
continue        c                             Continue to run.
deletebreak     delbreak        <breakid>     Delete a breakpoint.
disassemble     dis             [funcname]    Get current VM's running ir-codes.
                                --all
return          r                             Execute to the return of this fun
                                            -ction.
help            ?                             Get help informations.   
list            l               <listitem>    List something, such as:
                                            break, var, 
next            n                             Execute next line of src.
print           p               <varname>     Print the value.
source          src             [file name]   Get current source
                                [range = 5]   
stackframe      sf                            Get current function's stack frame.
step            s                             Execute next line of src, will step
                                            in functions.
stepir          si                            Execute next command.
)"
<< std::endl;
        }
        template<typename T>
        static bool need_possiable_input(T& out, bool need_wait = false)
        {
            char _useless_for_clear = 0;

            std::string input_item = "";

            if (need_wait)
            {
                _useless_for_clear = getchar();
                input_item += _useless_for_clear;
            }

            std::cin.clear();
            while (std::cin.readsome(&_useless_for_clear, 1))
            {
                if (_useless_for_clear != ' ' && _useless_for_clear != '\t' && _useless_for_clear != '\n')
                {
                    input_item += _useless_for_clear;
                    while (std::cin.readsome(&_useless_for_clear, 1))
                    {
                        if (_useless_for_clear == ' ' || _useless_for_clear == '\t' || _useless_for_clear == '\n')
                            break;
                        input_item += _useless_for_clear;
                    }

                    std::stringstream ss;
                    ss << input_item;
                    ss >> out;
                    return true;
                }
                else if (need_wait && input_item.size())
                {
                    _useless_for_clear = input_item[0];
                    if (_useless_for_clear != ' ' && _useless_for_clear != '\t' && _useless_for_clear != '\n')
                    {
                        std::stringstream ss;
                        ss << input_item;
                        ss >> out;
                        return true;
                    }
                }
            }

            if (need_wait && input_item.size())
            {
                _useless_for_clear = input_item[0];
                if (_useless_for_clear != ' ' && _useless_for_clear != '\t' && _useless_for_clear != '\n')
                {
                    std::stringstream ss;
                    ss << input_item;
                    ss >> out;
                    return true;
                }

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
        std::vector<function_code_info> search_function_begin_rtip_scope_with_name(const std::string& funcname)
        {
            if (_env)
            {
                std::vector<function_code_info> result;
                for (auto& [funcsign_name, pos] : _env->program_debug_info->_function_ip_data_buf)
                {
                    if (funcsign_name.rfind(funcname) != std::string::npos)
                    {
                        auto begin_rt_ip = _env->program_debug_info->get_runtime_ip_by_ip(pos.ir_begin);
                        auto end_rt_ip = _env->program_debug_info->get_runtime_ip_by_ip(pos.ir_end);
                        result.push_back({ funcsign_name,pos.ir_begin,pos.ir_end, begin_rt_ip , end_rt_ip });
                    }
                }
                return result;
            }
            return {};
        }

        bool breakdown_temp_for_stepir = false;
        bool breakdown_temp_for_return = false;
        size_t breakdown_temp_for_return_callstackdepth = 0;

        bool breakdown_temp_for_step = false;

        bool breakdown_temp_for_next = false;
        size_t breakdown_temp_for_next_callstackdepth = 0;

        size_t breakdown_temp_for_step_lineno = 0;
        std::string breakdown_temp_for_step_srcfile = "";

        rs::byte_t* current_runtime_ip;
        rs::value* current_frame_sp;
        rs::value* current_frame_bp;

        void display_variable(rs::vmbase* vmm, rs::program_debug_data_info::function_symbol_infor::variable_symbol_infor& varinfo)
        {
            auto real_offset = -varinfo.bp_offset;
            auto value_in_stack = current_frame_bp - varinfo.bp_offset;
            std::cout << varinfo.name << " define at line: " << varinfo.define_place << std::endl;
            if (varinfo.bp_offset >= 0)
                std::cout << "[bp-" << varinfo.bp_offset << "]: ";
            else
                std::cout << "[bp+" << -varinfo.bp_offset << "]: ";

            if (value_in_stack <= current_frame_sp)
                std::cout << value_in_stack << " nil (not in stack)" << std::endl;
            else
                std::cout << value_in_stack << " " << rs_cast_string((rs_value)value_in_stack) << std::endl;
        }

        bool debug_command(vmbase* vmm)
        {
            printf(ANSI_HIG "> " ANSI_HIY); fflush(stdout);
            std::string main_command;
            char _useless_for_clear = 0;

            if (need_possiable_input(main_command, true))
            {
                printf(ANSI_RST);
                if (main_command == "?" || main_command == "help")
                    command_help();
                else if (main_command == "c" || main_command == "continue") 
                    return false;
                else if (main_command == "r" || main_command == "return")
                {
                    breakdown_temp_for_return = true;
                    breakdown_temp_for_return_callstackdepth = vmm->callstack_layer();
                    goto continue_run_command;
                }
                else if (main_command == "dis" || main_command == "disassemble")
                {
                    std::string function_name;
                    if (need_possiable_input(function_name))
                    {
                        if (function_name == "--all")
                        {
                            vmm->dump_program_bin();
                        }
                        else
                        {
                            auto&& fndresult = search_function_begin_rtip_scope_with_name(function_name);
                            std::cout << "Find " << fndresult.size() << " symbol(s):" << std::endl;
                            for (auto& funcinfo : fndresult)
                            {
                                std::cout << "In function: " << funcinfo.func_sig << std::endl;
                                vmm->dump_program_bin(funcinfo.rt_ip_begin, funcinfo.rt_ip_end);
                            }
                        }
                    }
                    else
                    {
                        auto&& funcname = _env->program_debug_info->get_current_func_signature_by_runtime_ip(current_runtime_ip);
                        auto fnd = _env->program_debug_info->_function_ip_data_buf.find(funcname);
                        if (fnd != _env->program_debug_info->_function_ip_data_buf.end())
                        {
                            std::cout << "In function: " << funcname << std::endl;
                            vmm->dump_program_bin(
                                _env->program_debug_info->get_runtime_ip_by_ip(fnd->second.ir_begin)
                                , _env->program_debug_info->get_runtime_ip_by_ip(fnd->second.ir_end));
                        }
                        else
                            printf(ANSI_HIR "Invalid function.\n" ANSI_RST);
                    }
                }
                else if (main_command == "cs" || main_command == "callstack")
                {
                    size_t max_layer;
                    if (!need_possiable_input(max_layer))
                        max_layer = 8;
                    vmm->dump_call_stack(max_layer, false);
                }
                else if (main_command == "break")
                {
                    std::string filename_or_funcname;
                    std::cin >> filename_or_funcname;

                    size_t lineno;
                    if (need_possiable_input<size_t>(lineno))
                        set_breakpoint(filename_or_funcname, lineno);
                    else
                    {
                        std::lock_guard g1(_mx);

                        auto&& fndresult = search_function_begin_rtip_scope_with_name(filename_or_funcname);
                        std::cout << "Set breakpoint at " << fndresult.size() << " symbol(s):" << std::endl;
                        for (auto& funcinfo : fndresult)
                        {
                            std::cout << "In function: " << funcinfo.func_sig << std::endl;
                            break_point_traps.insert(
                                _env->program_debug_info->get_ip_by_runtime_ip(
                                    _env->rt_codes + _env->program_debug_info->get_runtime_ip_by_ip(funcinfo.command_ip_begin)));
                            //NOTE: some function's reserved stack op may be removed, so get real ir is needed..
                        }
                    }
                    std::cout << "OK!" << std::endl;
                }
                else if (main_command == "delbreak" || main_command == "deletebreak")
                {
                    size_t breakno;
                    std::cin >> breakno;

                    if (breakno < break_point_traps.size())
                    {
                        auto fnd = break_point_traps.begin();

                        for (size_t i = 0; i < breakno; i++)
                            fnd++;
                        break_point_traps.erase(fnd);
                        std::cout << "OK!" << std::endl;
                    }
                    else
                        printf(ANSI_HIR "Unknown breakpoint id.\n" ANSI_RST);
                }
                else if (main_command == "si" || main_command == "stepir")
                {
                    breakdown_temp_for_stepir = true;
                    goto continue_run_command;
                }
                else if (main_command == "s" || main_command == "step")
                {
                    // Get current lineno

                    auto& loc = vmm->env->program_debug_info
                        ->get_src_location_by_runtime_ip(vmm->ip);

                    breakdown_temp_for_step = true;
                    breakdown_temp_for_step_lineno = loc.row_no;
                    breakdown_temp_for_step_srcfile = loc.source_file;

                    goto continue_run_command;
                }
                else if (main_command == "n" || main_command == "next")
                {
                    // Get current lineno

                    auto& loc = vmm->env->program_debug_info
                        ->get_src_location_by_runtime_ip(vmm->ip);

                    breakdown_temp_for_next = true;
                    breakdown_temp_for_next_callstackdepth = vmm->callstack_layer();
                    breakdown_temp_for_step_lineno = loc.row_no;
                    breakdown_temp_for_step_srcfile = loc.source_file;

                    goto continue_run_command;
                }
                else if (main_command == "sf" || main_command == "stackframe")
                {
                    auto* currentsp = current_frame_sp;
                    while ((++currentsp) <= current_frame_bp)
                    {
                        std::cout << "[bp-" << current_frame_bp - currentsp << "]: " << currentsp << " " << rs_cast_string((rs_value)currentsp) << std::endl;
                    }

                }
                else if (main_command == "p" || main_command == "print")
                {
                    std::string varname;
                    std::cin >> varname;

                    if (_env)
                    {
                        auto&& funcname = _env->program_debug_info->get_current_func_signature_by_runtime_ip(current_runtime_ip);
                        auto fnd = _env->program_debug_info->_function_ip_data_buf.find(funcname);
                        if (fnd != _env->program_debug_info->_function_ip_data_buf.end())
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
                else if (main_command == "src" || main_command == "source")
                {
                    std::string filename;
                    size_t display_range = 5;
                    auto& loc = vmm->env->program_debug_info->get_src_location_by_runtime_ip(current_runtime_ip);
                    size_t display_rowno = loc.row_no;
                    if (need_possiable_input(filename))
                    {
                        for (auto ch : filename)
                        {
                            if (!lexer::lex_isdigit(ch))
                            {
                                print_src_file(filename, (filename == loc.source_file ? loc.row_no : 0));
                                goto need_next_command;
                            }
                        }
                        display_range = std::stoll(filename);
                    }

                    print_src_file(loc.source_file, display_rowno,
                        (display_rowno < display_range / 2 ? 0 : display_rowno - display_range / 2), display_rowno + display_range / 2);


                }
                else if (main_command == "l" || main_command == "list")
                {
                    std::string list_what;
                    std::cin >> list_what;
                    if (list_what == "break")
                    {
                        size_t id = 0;
                        for (auto break_stip : break_point_traps)
                        {
                            auto& file_loc =
                                _env->program_debug_info->get_src_location_by_runtime_ip(_env->rt_codes +
                                    _env->program_debug_info->get_runtime_ip_by_ip(break_stip));
                            std::cout << (id++) << " :\t" << file_loc.source_file << " (" << file_loc.row_no << "," << file_loc.col_no << ")" << std::endl;;
                        }
                    }
                    else if (list_what == "var")
                    {
                        auto&& funcname = _env->program_debug_info->get_current_func_signature_by_runtime_ip(current_runtime_ip);
                        auto fnd = _env->program_debug_info->_function_ip_data_buf.find(funcname);
                        if (fnd != _env->program_debug_info->_function_ip_data_buf.end())
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
                    else
                        printf(ANSI_HIR "Unknown type to list.\n" ANSI_RST);
                }
                else
                    printf(ANSI_HIR "Unknown debug command, please input 'help' for more information.\n" ANSI_RST);
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
        void print_src_file_print_lineno(const std::string& filepath, size_t current_row_no)
        {
            auto ip = _env->program_debug_info->get_ip_by_src_location(filepath, current_row_no, true);
            std::lock_guard g1(_mx);

            if (break_point_traps.find(ip) == break_point_traps.end())
                printf("%-5zu: ", current_row_no);
            else
                printf(ANSI_BHIR ANSI_WHI "%-5zu: " ANSI_RST, current_row_no);
        }
        void print_src_file(const std::string& filepath, size_t highlight = 0, size_t from = 0, size_t to = SIZE_MAX)
        {
            std::wstring srcfile, src_full_path;
            if (!rs::read_virtual_source(&srcfile, &src_full_path, rs::str_to_wstr(filepath)))
                printf(ANSI_HIR "Cannot open source: '%s'.\n" ANSI_RST, filepath.c_str());
            else
            {
                std::cout << filepath << " from: ";
                if (from == 0)
                    std::cout << "File begin";
                else
                    std::cout << from;
                std::cout << " to: ";
                if (to == SIZE_MAX)
                    std::cout << "File end";
                else
                    std::cout << to;
                std::cout << std::endl;

                // print source;
                // here is a easy lexer..
                size_t current_row_no = 1;



                if (from <= current_row_no && current_row_no <= to)
                    print_src_file_print_lineno(filepath, current_row_no);
                for (size_t index = 0; index < srcfile.size(); index++)
                {
                    if (srcfile[index] == L'\n')
                    {
                        current_row_no++;
                        if (from <= current_row_no && current_row_no <= to)
                        {
                            std::cout << std::endl; print_src_file_print_lineno(filepath, current_row_no);
                        }
                        continue;
                    }
                    else if (srcfile[index] == L'\r')
                    {
                        current_row_no++;
                        if (from <= current_row_no && current_row_no <= to)
                        {
                            std::cout << std::endl; print_src_file_print_lineno(filepath, current_row_no);
                        }
                        if (index + 1 < srcfile.size() && srcfile[index + 1] == L'\n')
                            index++;
                        continue;
                    }

                    if (current_row_no == highlight)
                        printf(ANSI_INV);

                    if (from <= current_row_no && current_row_no <= to)
                        std::wcout << srcfile[index];

                    if (current_row_no == highlight)
                        printf(ANSI_RST);
                }
            }
            std::cout << std::endl;
        }

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

            current_frame_bp = vmm->bp;
            current_frame_sp = vmm->sp;
            current_runtime_ip = vmm->ip;

            auto command_ip = vmm->env->program_debug_info->get_ip_by_runtime_ip(next_execute_ip);
            auto& loc = vmm->env->program_debug_info->get_src_location_by_runtime_ip(next_execute_ip);

            // check breakpoint..
            std::lock_guard g1(_mx);
            if (breakdown_temp_for_stepir
                || (breakdown_temp_for_step
                    && (loc.row_no != breakdown_temp_for_step_lineno
                        || loc.source_file != breakdown_temp_for_step_srcfile))
                || (breakdown_temp_for_next
                    && vmm->callstack_layer() <= breakdown_temp_for_next_callstackdepth
                    && (loc.row_no != breakdown_temp_for_step_lineno
                        || loc.source_file != breakdown_temp_for_step_srcfile))
                || (breakdown_temp_for_return
                    && vmm->callstack_layer() < breakdown_temp_for_return_callstackdepth)
                || break_point_traps.find(command_ip) != break_point_traps.end())
            {
                breakdown_temp_for_stepir = false;
                breakdown_temp_for_step = false;
                breakdown_temp_for_next = false;
                breakdown_temp_for_return = false;
                block_other_vm_in_this_debuggee();

                printf("Breakdown: +%04d: at %s(%zu, %zu)\nin function: %s\n", (int)next_execute_ip_diff,
                    loc.source_file.c_str(), loc.row_no, loc.col_no,
                    vmm->env->program_debug_info->get_current_func_signature_by_runtime_ip(next_execute_ip).c_str()
                );

                printf("===========================================\n");

                while (debug_command(vmm))
                {
                    // ...?
                }

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