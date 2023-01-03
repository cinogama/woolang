#pragma once
#include "wo_vm.hpp"
#include "wo_lang.hpp"

#include <mutex>

namespace wo
{
    class default_debuggee : public wo::debuggee_base
    {
        std::recursive_mutex _mx;
        runtime_env* _env = nullptr;

        std::set<size_t> break_point_traps;
        std::map<std::wstring, std::map<size_t, bool>> template_breakpoint;

        inline static std::atomic_bool stop_attach_debuggee_for_exit_flag = false;

    public:
        default_debuggee()
        {

        }
        bool set_breakpoint(const std::wstring& src_file, size_t rowno)
        {
            std::lock_guard g1(_mx);
            if (_env)
            {
                auto breakip = _env->program_debug_info->get_ip_by_src_location(src_file, rowno);

                if (breakip < _env->rt_code_len)
                {
                    break_point_traps.insert(breakip);
                    return true;
                }
                return false;
            }
            else
                template_breakpoint[src_file][rowno] = true;

            return true;
        }
        void clear_breakpoint(const std::wstring& src_file, size_t rowno)
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
            wo_stdout <<
                R"(Woolang debuggee tool command list:

COMMAND_NAME    SHORT_COMMAND   ARGUMENT    DESCRIBE
------------------------------------------------------------------------------
break                           <file line>   Set a breakpoint at the specified
                                <funcname>  location.
callstack       cs              [max = 8]     Get current VM's callstacks.
continue        c                             Continue to run.
deletebreak     delbreak        <breakid>     Delete a breakpoint.
disassemble     dis             [funcname]    Get current VM's running ir-codes.
                                --all
frame           f               <frameid>     Switch to a call frame.
return          r                             Execute to the return of this fun
                                            -ction.
help            ?                             Get help informations.   
list            l               <listitem>    List something, such as:
                                            break, var, vm(thread)
next            n                             Execute next line of src.
print           p               <varname>     Print the value.
quit                                          Exit(0)
source          src             [file name]   Get current source
                                [range = 5]   
stackframe      sf                            Get current function's stack frame.
step            s                             Execute next line of src, will step
                                            in functions.
stepir          si                            Execute next command.
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
                if (!(readed_ch & 0x80 || isspace(readed_ch & 0x7f)))
                {
                    std::string read_word;

                    while (fnd != inputstr.end())
                    {
                        if (!(readed_ch & 0x80 || isspace(readed_ch & 0x7f)))
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
        std::wstring breakdown_temp_for_step_srcfile = L"";

        const wo::byte_t* current_runtime_ip;
        wo::value* current_frame_sp;
        wo::value* current_frame_bp;

        void display_variable(wo::vmbase* vmm, wo::program_debug_data_info::function_symbol_infor::variable_symbol_infor& varinfo)
        {
            // auto real_offset = -varinfo.bp_offset;
            auto value_in_stack = current_frame_bp - varinfo.bp_offset;
            wo_stdout << varinfo.name << " define at line: " << varinfo.define_place << wo_endl;
            if (varinfo.bp_offset >= 0)
                wo_stdout << "[bp-" << varinfo.bp_offset << "]: ";
            else
                wo_stdout << "[bp+" << -varinfo.bp_offset << "]: ";

            if (value_in_stack <= current_frame_sp)
                wo_stdout << value_in_stack << " nil (not in stack)" << wo_endl;
            else
                wo_stdout << value_in_stack << " " << wo_cast_string((wo_value)value_in_stack) << wo_endl;
        }

        bool debug_command(vmbase* vmm)
        {
            printf(ANSI_HIG "> " ANSI_HIY); fflush(stdout);
            std::string main_command;
            char _useless_for_clear = 0;
            auto&& inputbuf = get_and_split_line();
            if (need_possiable_input(inputbuf, main_command))
            {
                printf(ANSI_RST);
                if (main_command == "?" || main_command == "help")
                    command_help();
                else if (main_command == "c" || main_command == "continue")
                {
                    printf(ANSI_HIG "Continue running...\n" ANSI_RST);
                    return false;
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
                            if (tmp_sp->type == value::valuetype::callstack)
                            {
                                current_frame++;
                                current_frame_sp = tmp_sp;
                                current_frame_bp = vmm->stack_mem_begin - tmp_sp->bp;
                                current_runtime_ip = _env->rt_codes + tmp_sp->ret_ip;
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
                    std::string function_name;
                    if (need_possiable_input(inputbuf, function_name))
                    {
                        if (function_name == "--all")
                        {
                            vmm->dump_program_bin();
                        }
                        else
                        {
                            if (vmm->env->program_debug_info == nullptr)
                                printf(ANSI_HIR "No pdb found, command failed.\n" ANSI_RST);
                            else
                            {
                                auto&& fndresult = search_function_begin_rtip_scope_with_name(function_name);
                                wo_stdout << "Find " << fndresult.size() << " symbol(s):" << wo_endl;
                                for (auto& funcinfo : fndresult)
                                {
                                    wo_stdout << "In function: " << funcinfo.func_sig << wo_endl;
                                    vmm->dump_program_bin(funcinfo.rt_ip_begin, funcinfo.rt_ip_end);
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
                            auto&& funcname = _env->program_debug_info->get_current_func_signature_by_runtime_ip(current_runtime_ip);
                            auto fnd = _env->program_debug_info->_function_ip_data_buf.find(funcname);
                            if (fnd != _env->program_debug_info->_function_ip_data_buf.end())
                            {
                                wo_stdout << "In function: " << funcname << wo_endl;
                                vmm->dump_program_bin(
                                    _env->program_debug_info->get_runtime_ip_by_ip(fnd->second.ir_begin)
                                    , _env->program_debug_info->get_runtime_ip_by_ip(fnd->second.ir_end));
                            }
                            else
                                printf(ANSI_HIR "Invalid function.\n" ANSI_RST);
                        }
                    }
                }
                else if (main_command == "cs" || main_command == "callstack")
                {
                    size_t max_layer;
                    if (!need_possiable_input(inputbuf, max_layer))
                        max_layer = 8;
                    vmm->dump_call_stack(max_layer, false);
                }
                else if (main_command == "break")
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
                                result = set_breakpoint(str_to_wstr(filename_or_funcname), lineno);
                            else
                            {
                                for (auto ch : filename_or_funcname)
                                {
                                    if (!lexer::lex_isdigit(ch))
                                    {
                                        std::lock_guard g1(_mx);

                                        auto&& fndresult = search_function_begin_rtip_scope_with_name(filename_or_funcname);
                                        wo_stdout << "Set breakpoint at " << fndresult.size() << " symbol(s):" << wo_endl;
                                        for (auto& funcinfo : fndresult)
                                        {
                                            wo_stdout << "In function: " << funcinfo.func_sig << wo_endl;
                                            break_point_traps.insert(
                                                _env->program_debug_info->get_ip_by_runtime_ip(
                                                    _env->rt_codes + _env->program_debug_info->get_runtime_ip_by_ip(funcinfo.command_ip_begin)));
                                            //NOTE: some function's reserved stack op may be removed, so get real ir is needed..
                                        }
                                        goto need_next_command;
                                    }
                                }
                                ;
                                result = set_breakpoint(
                                    vmm->env->program_debug_info->get_src_location_by_runtime_ip(current_runtime_ip).source_file
                                    , std::stoull(filename_or_funcname));

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
                else if (main_command == "quit")
                {
                    // exit(0);
                    stop_attach_debuggee_for_exit_flag = true;
                    return false;
                }
                else if (main_command == "delbreak" || main_command == "deletebreak")
                {
                    size_t breakno;
                    if (need_possiable_input(inputbuf, breakno))
                    {
                        if (breakno < break_point_traps.size())
                        {
                            auto fnd = break_point_traps.begin();

                            for (size_t i = 0; i < breakno; i++)
                                fnd++;
                            break_point_traps.erase(fnd);
                            wo_stdout << "OK!" << wo_endl;
                        }
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
                        breakdown_temp_for_step_lineno = loc.begin_row_no;
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
                        breakdown_temp_for_step_lineno = loc.begin_row_no;
                        breakdown_temp_for_step_srcfile = loc.source_file;

                        goto continue_run_command;
                    }
                }
                else if (main_command == "sf" || main_command == "stackframe")
                {
                    auto* currentsp = current_frame_sp;
                    while ((++currentsp) <= current_frame_bp)
                    {
                        wo_stdout << "[bp-" << current_frame_bp - currentsp << "]: " << currentsp << " " << wo_cast_string((wo_value)currentsp) << wo_endl;
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
                        size_t display_rowno = loc.begin_row_no;
                        if (need_possiable_input(inputbuf, filename))
                        {
                            for (auto ch : filename)
                            {
                                if (!lexer::lex_isdigit(ch))
                                {
                                    print_src_file(filename, (str_to_wstr(filename) == loc.source_file ? loc.begin_row_no : 0));
                                    goto need_next_command;
                                }
                            }
                            display_range = std::stoull(filename);
                        }

                        print_src_file(wstr_to_str(loc.source_file), display_rowno,
                            (display_rowno < display_range / 2 ? 0 : display_rowno - display_range / 2), display_rowno + display_range / 2);

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
                                for (auto break_stip : break_point_traps)
                                {
                                    auto& file_loc =
                                        _env->program_debug_info->get_src_location_by_runtime_ip(_env->rt_codes +
                                            _env->program_debug_info->get_runtime_ip_by_ip(break_stip));
                                    wo_stdout << (id++) << " :\t" << wstr_to_str(file_loc.source_file) << " (" << file_loc.begin_row_no << "," << file_loc.begin_col_no << ")" << wo_endl;;
                                }
                            }
                        }
                        else if (list_what == "var")
                        {
                            if (vmm->env->program_debug_info == nullptr)
                                printf(ANSI_HIR "No pdb found, command failed.\n" ANSI_RST);
                            else
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
                        }
                        else if (list_what == "vm" || list_what == "thread")
                        {
                            if (wo::vmbase::_alive_vm_list_mx.try_lock_shared())
                            {
                                size_t vmcount = 0;
                                for (auto vms : wo::vmbase::_alive_vm_list)
                                {
                                    wo_stdout << ANSI_HIY "thread(vm) #" ANSI_HIG << vmcount << ANSI_RST " " << vms << " " << wo_endl;

                                    auto usage = ((double)(vms->stack_mem_begin - vms->sp)) / (double)vms->stack_size;
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

                                wo::vmbase::_alive_vm_list_mx.unlock_shared();
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
        size_t print_src_file_print_lineno(const std::string& filepath, size_t current_row_no)
        {
            auto ip = _env->program_debug_info->get_ip_by_src_location(str_to_wstr(filepath), current_row_no, true);
            std::lock_guard g1(_mx);

            if (auto fnd = break_point_traps.find(ip); fnd == break_point_traps.end())
            {
                printf("%-5zu: ", current_row_no);
                return SIZE_MAX;
            }
            else
            {
                printf(ANSI_BHIR ANSI_WHI "%-5zu: " ANSI_RST, current_row_no);

                size_t bid = 0;

                while (true)
                {
                    if (fnd == break_point_traps.begin())
                        return bid;
                    fnd--;
                    bid++;
                }
            }
        }
        void print_src_file(const std::string& filepath, size_t highlight = 0, size_t from = 0, size_t to = SIZE_MAX)
        {
            std::wstring srcfile, src_full_path;
            if (!wo::read_virtual_source(&srcfile, &src_full_path, wo::str_to_wstr(filepath), nullptr))
                printf(ANSI_HIR "Cannot open source: '%s'.\n" ANSI_RST, filepath.c_str());
            else
            {
                wo_stdout << filepath << " from: ";
                if (from == 0)
                    wo_stdout << "File begin";
                else
                    wo_stdout << from;
                wo_stdout << " to: ";
                if (to == SIZE_MAX)
                    wo_stdout << "File end";
                else
                    wo_stdout << to;
                wo_stdout << wo_endl;

                // print source;
                // here is a easy lexer..
                size_t current_row_no = 1;

                size_t last_line_is_breakline = SIZE_MAX;

                if (from <= current_row_no && current_row_no <= to)
                    last_line_is_breakline = print_src_file_print_lineno(filepath, current_row_no);
                for (size_t index = 0; index < srcfile.size(); index++)
                {
                    if (srcfile[index] == L'\n')
                    {
                        current_row_no++;
                        if (from <= current_row_no && current_row_no <= to)
                        {
                            if (last_line_is_breakline != SIZE_MAX)
                                printf("    " ANSI_HIR "# Breakpoint %zu" ANSI_RST, last_line_is_breakline);

                            wo_stdout << wo_endl; last_line_is_breakline = print_src_file_print_lineno(filepath, current_row_no);
                        }
                        continue;
                    }
                    else if (srcfile[index] == L'\r')
                    {
                        current_row_no++;
                        if (from <= current_row_no && current_row_no <= to)
                        {
                            if (last_line_is_breakline != SIZE_MAX)
                                printf("\t" ANSI_HIR "# Breakpoint %zu" ANSI_RST, last_line_is_breakline);

                            wo_stdout << wo_endl; last_line_is_breakline = print_src_file_print_lineno(filepath, current_row_no);
                        }
                        if (index + 1 < srcfile.size() && srcfile[index + 1] == L'\n')
                            index++;
                        continue;
                    }

                    if (current_row_no == highlight)
                        printf(ANSI_INV);

                    if (from <= current_row_no && current_row_no <= to)
                        wo_wstdout << srcfile[index];

                    if (current_row_no == highlight)
                        printf(ANSI_RST);
                }
            }
            wo_stdout << wo_endl;
        }

        virtual void debug_interrupt(vmbase* vmm) override
        {
            do
            {
                do
                {
                    std::lock_guard g1(_mx);
                    if (stop_attach_debuggee_for_exit_flag)
                        return;

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

                const byte_t* next_execute_ip = vmm->ip;
                auto next_execute_ip_diff = vmm->ip - vmm->env->rt_codes;

                current_frame_bp = vmm->bp;
                current_frame_sp = vmm->sp;
                current_runtime_ip = vmm->ip;


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
                std::lock_guard g1(_mx);

                if (stop_attach_debuggee_for_exit_flag)
                    return;

                if (breakdown_temp_for_stepir
                    || (breakdown_temp_for_step
                        && (loc->begin_row_no != breakdown_temp_for_step_lineno
                            || loc->source_file != breakdown_temp_for_step_srcfile))
                    || (breakdown_temp_for_next
                        && vmm->callstack_layer() <= breakdown_temp_for_next_callstackdepth
                        && (loc->begin_row_no != breakdown_temp_for_step_lineno
                            || loc->source_file != breakdown_temp_for_step_srcfile))
                    || (breakdown_temp_for_return
                        && vmm->callstack_layer() < breakdown_temp_for_return_callstackdepth)
                    || break_point_traps.find(command_ip) != break_point_traps.end())
                {
                    block_other_vm_in_this_debuggee();

                    breakdown_temp_for_stepir = false;
                    breakdown_temp_for_step = false;
                    breakdown_temp_for_next = false;
                    breakdown_temp_for_return = false;

                    printf("Breakdown: +%04d: at %s(%zu, %zu)\nin function: %s\n", (int)next_execute_ip_diff,
                        wstr_to_str(loc->source_file).c_str(), loc->begin_row_no, loc->begin_col_no,
                        vmm->env->program_debug_info == nullptr ?
                        "__unknown_func_without_pdb_" :
                        vmm->env->program_debug_info->get_current_func_signature_by_runtime_ip(next_execute_ip).c_str()
                    );

                    printf("===========================================\n");

                    while (debug_command(vmm))
                    {
                        // ...?
                    }

                    unblock_other_vm_in_this_debuggee();

                }

            } while (0);

            if (stop_attach_debuggee_for_exit_flag)
            {
                wo_abort_all_vm_to_exit();
                return;
            }
        }
    };

    class c_style_debuggee_binder : public wo::debuggee_base
    {
        void* custom_items;
        wo_debuggee_handler_func c_debuggee_handler;

        c_style_debuggee_binder(wo_debuggee_handler_func func, void* custom)
        {
            c_debuggee_handler = func;
            custom_items = custom;
        }

        virtual void debug_interrupt(vmbase* vmm) override
        {
            c_debuggee_handler((wo_debuggee)this, (wo_vm)vmm, custom_items);
        }
    };
}