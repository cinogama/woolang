// wo_api_impl.cpp
#include "wo_vm.hpp"
#include "wo_source_file_manager.hpp"
#include "wo_compiler_parser.hpp"
#include "wo_stdlib.hpp"
#include "wo_lang_grammar_loader.hpp"
#include "wo_lang.hpp"
#include "wo_utf8.hpp"
#include "wo_runtime_debuggee.hpp"
#include "wo_global_setting.hpp"
#include "wo_io.hpp"
#include "wo_crc_64.hpp"
#include "wo_vm_pool.hpp"

#include <csignal>
#include <sstream>
#include <new>
#include <chrono>
#include <cstdarg>

// TODO LIST
// 1. ALL GC_UNIT OPERATE SHOULD BE ATOMIC

#include <atomic>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <string>

struct _wo_enter_gc_guard
{
    wo_vm _vm;
    _wo_enter_gc_guard() = delete;
    _wo_enter_gc_guard(const _wo_enter_gc_guard&) = delete;
    _wo_enter_gc_guard(_wo_enter_gc_guard&&) = delete;
    _wo_enter_gc_guard& operator = (const _wo_enter_gc_guard&) = delete;
    _wo_enter_gc_guard& operator = (_wo_enter_gc_guard&&) = delete;
    _wo_enter_gc_guard(wo_vm vm)
        : _vm(wo_enter_gcguard(vm) ? vm : nullptr)
    {
    }
    ~_wo_enter_gc_guard()
    {
        if (_vm)
            wo_leave_gcguard(_vm);
    }
};
struct _wo_in_thread_vm_guard
{
    wo_vm last_vm;

    _wo_in_thread_vm_guard() = delete;
    _wo_in_thread_vm_guard(const _wo_in_thread_vm_guard&) = delete;
    _wo_in_thread_vm_guard(_wo_in_thread_vm_guard&&) = delete;
    _wo_in_thread_vm_guard& operator = (const _wo_in_thread_vm_guard&) = delete;
    _wo_in_thread_vm_guard& operator = (_wo_in_thread_vm_guard&&) = delete;

    _wo_in_thread_vm_guard(wo_vm target_vm)
        : last_vm(wo_set_this_thread_vm(target_vm))
    {

    }
    ~_wo_in_thread_vm_guard()
    {
        wo_set_this_thread_vm(last_vm);
    }
};
struct loaded_lib_info
{
    void* m_lib_instance;
    size_t      m_use_count;
};

wo::vmpool* global_vm_pool = nullptr;

std::mutex loaded_named_libs_mx;
std::unordered_map<std::string, std::vector<loaded_lib_info>> loaded_named_libs;

void _default_fail_handler(wo_vm vm, wo_string_t src_file, uint32_t lineno, wo_string_t functionname, uint32_t rterrcode, wo_string_t reason)
{
    auto* cur_thread_vm = std::launder(reinterpret_cast<wo::vmbase*>(vm));
    bool leaved_flag = cur_thread_vm != nullptr
        ? wo_leave_gcguard(reinterpret_cast<wo_vm>(cur_thread_vm))
        : false;

    wo::wo_stderr << ANSI_HIR "WooLang Runtime happend a failure: "
        << ANSI_HIY << reason << " (Code: " << std::hex << rterrcode << std::dec << ")" << ANSI_RST << wo::wo_endl;
    wo::wo_stderr << "\tAt source: \t" << src_file << wo::wo_endl;
    wo::wo_stderr << "\tAt line: \t" << lineno << wo::wo_endl;
    wo::wo_stderr << "\tAt function: \t" << functionname << wo::wo_endl;
    wo::wo_stderr << wo::wo_endl;

    wo::wo_stderr << ANSI_HIR "callstack: " ANSI_RST << wo::wo_endl;

    if (cur_thread_vm != nullptr)
        cur_thread_vm->dump_call_stack(32, true, std::cerr);
    else
        wo::wo_stderr << ANSI_HIM "No woolang vm found in this thread." ANSI_RST << wo::wo_endl;

    wo::wo_stderr << wo::wo_endl;

    if ((rterrcode & WO_FAIL_TYPE_MASK) == WO_FAIL_MINOR)
    {
        wo::wo_stderr << ANSI_HIY "This is a minor failure, ignore it." ANSI_RST << wo::wo_endl;
        // Just ignore it..
    }
    else if ((rterrcode & WO_FAIL_TYPE_MASK) == WO_FAIL_MEDIUM)
    {
        // Just halt it..
        wo::wo_stderr << ANSI_HIY "This is a medium failure, halt." ANSI_RST << wo::wo_endl;

        if (cur_thread_vm != nullptr)
            wo_ret_halt(reinterpret_cast<wo_vm>(cur_thread_vm), reason);
    }
    else if ((rterrcode & WO_FAIL_TYPE_MASK) == WO_FAIL_HEAVY)
    {
        // Just halt it..
        wo::wo_stderr << ANSI_HIY "This is a heavy failure, halt." ANSI_RST << wo::wo_endl;
        if (cur_thread_vm != nullptr)
            wo_ret_halt(reinterpret_cast<wo_vm>(cur_thread_vm), reason);
    }
    // Real panic!
    else
    {
        wo::wo_stderr << ANSI_HIY "This failure may cause a crash or nothing happens." ANSI_RST << wo::wo_endl;
        if (wo::config::ENABLE_HALT_WHEN_PANIC)
        {
            // Halt directly, donot wait for input.
            if (cur_thread_vm != nullptr)
                wo_ret_halt(reinterpret_cast<wo_vm>(cur_thread_vm), reason);
        }
        else
        {
            wo::wo_stderr << "1) Abort program.(You can attatch debuggee.)" << wo::wo_endl;
            wo::wo_stderr << "2) Continue.(May cause unknown errors.)" << wo::wo_endl;
            wo::wo_stderr << "3) Halt (Not exactly safe, this vm will be abort.)" << wo::wo_endl;
            wo::wo_stderr << "4) Attach debuggee and break immediately." << wo::wo_endl;

            bool breakout = false;
            while (true)
            {
                char _useless_for_clear = 0;
                std::cin.clear();
                while (std::cin.readsome(&_useless_for_clear, 1));

                if (breakout)
                    break;

                int choice;
                wo::wo_stderr << "Please input your choice: " ANSI_HIY;
                std::cin >> choice;
                wo::wo_stderr << ANSI_RST;
                switch (choice)
                {
                case 1:
                    wo_error(reason);
                    breakout = true;
                    break;
                case 2:
                    breakout = true;
                    break;
                case 3:
                    if (cur_thread_vm != nullptr)
                    {
                        wo::wo_stderr << ANSI_HIR "Current virtual machine will abort." ANSI_RST << wo::wo_endl;
                        wo_ret_halt(reinterpret_cast<wo_vm>(cur_thread_vm), reason);
                        breakout = true;
                    }
                    else
                        wo::wo_stderr << ANSI_HIR "No virtual machine running in this thread." ANSI_RST << wo::wo_endl;
                    break;
                case 4:
                    if (cur_thread_vm != nullptr)
                    {
                        if (!wo_has_attached_debuggee())
                            wo_attach_default_debuggee();
                        wo_break_specify_immediately(vm);
                        breakout = true;
                    }
                    else
                        wo::wo_stderr << ANSI_HIR "No virtual machine running in this thread." ANSI_RST << wo::wo_endl;
                    break;
                default:
                    wo::wo_stderr << ANSI_HIR "Invalid choice" ANSI_RST << wo::wo_endl;
                }
            }
        }
    }

    if (leaved_flag)
    {
        wo_assert(cur_thread_vm != nullptr);
        wo_enter_gcguard(reinterpret_cast<wo_vm>(cur_thread_vm));
    }
}
static std::atomic<wo_fail_handler> _wo_fail_handler_function = &_default_fail_handler;

wo_fail_handler wo_regist_fail_handler(wo_fail_handler new_handler)
{
    return _wo_fail_handler_function.exchange(new_handler);
}
void wo_execute_fail_handler(wo_vm vm, wo_string_t src_file, uint32_t lineno, wo_string_t functionname, uint32_t rterrcode, wo_string_t reason)
{
    _wo_fail_handler_function.load()(
        vm, src_file, lineno, functionname, rterrcode, reason);
}
void wo_cause_fail(wo_string_t src_file, uint32_t lineno, wo_string_t functionname, uint32_t rterrcode, wo_string_t reasonfmt, ...)
{
    va_list v1, v2;
    va_start(v1, reasonfmt);
    va_copy(v2, v1);
    std::vector<char> buf(1 + vsnprintf(nullptr, 0, reasonfmt, v1));
    va_end(v1);
    std::vsnprintf(buf.data(), buf.size(), reasonfmt, v2);
    va_end(v2);

    wo_execute_fail_handler(
        reinterpret_cast<wo_vm>(wo::vmbase::_this_thread_vm),
        src_file, lineno, functionname, rterrcode, buf.data());
}

void _wo_ctrl_c_signal_handler(int)
{
    // CTRL + C
    wo::wo_stderr << ANSI_HIR "CTRL+C" ANSI_RST ": Trying to breakdown all virtual-machine by default debuggee immediately." << wo::wo_endl;

    if (!wo_has_attached_debuggee())
        wo_attach_default_debuggee();

    wo_break_immediately();

    wo_handle_ctrl_c(_wo_ctrl_c_signal_handler);
}

void wo_handle_ctrl_c(void(*handler)(int))
{
    signal(SIGINT, handler);
}

#undef wo_init

void wo_finish(void(*do_after_shutdown)(void*), void* custom_data)
{
    bool scheduler_need_shutdown = true;

    // Ready to shutdown all vm & coroutine.
    // 
    // Free all vm in pool, because vm in pool is PENDING, we can free them directly.
    // ATTENTION: If somebody using global_vm_pool when finish, here may crash or dead loop.
    if (global_vm_pool != nullptr)
    {
        delete global_vm_pool;
        global_vm_pool = nullptr;
    }

    time_t non_close_vm_last_warning_time = 0;
    size_t non_close_vm_last_warning_vm_count = 0;
    do
    {
        do
        {
            std::lock_guard g1(wo::vmbase::_alive_vm_list_mx);

            size_t not_close_vm_count = 0;
            for (auto& alive_vms : wo::vmbase::_alive_vm_list)
            {
                if (alive_vms->virtual_machine_type == wo::vmbase::vm_type::NORMAL)
                    not_close_vm_count++;

                alive_vms->interrupt(wo::vmbase::ABORT_INTERRUPT);
            }

            auto current_time = time(nullptr);
            if (non_close_vm_last_warning_time == 0 || current_time != non_close_vm_last_warning_time)
            {
                non_close_vm_last_warning_time = current_time;
                if (not_close_vm_count != 0 && not_close_vm_count != non_close_vm_last_warning_vm_count)
                {
                    non_close_vm_last_warning_vm_count = not_close_vm_count;
                    wo_warning((std::to_string(not_close_vm_count)
                        + " vm(s) have not been closed, please check.").c_str());
                }
            }

        } while (false);

        using namespace std;

        wo_gc_immediately(WO_TRUE);
        std::this_thread::sleep_for(10ms);

        std::lock_guard g1(wo::vmbase::_alive_vm_list_mx);
        if (wo::vmbase::_alive_vm_list.empty())
            break;

    } while (true);

    wo_gc_stop();

    if (do_after_shutdown != nullptr)
        do_after_shutdown(custom_data);

    std::lock_guard sg1(loaded_named_libs_mx);
    if (loaded_named_libs.empty() == false)
    {
        std::string not_unload_lib_warn = "Some of library(s) loaded by 'wo_load_lib' is not been unload after shutdown:";
        for (auto& [path, _] : loaded_named_libs)
            not_unload_lib_warn += "\n\t\t" + path;
        wo_warning(not_unload_lib_warn.c_str());
    }

    womem_shutdown();
    wo::debuggee_base::_free_abandons();

    wo::rslib_extern_symbols::free_wo_lib();
}

void wo_init(int argc, char** argv)
{
    const char* basic_env_local = "en_US.UTF-8";
    bool enable_std_package = true;
    bool enable_ctrl_c_to_debug = true;
    bool enable_gc = true;
    bool enable_vm_pool = true;

    wo::wo_init_args(argc, argv);

    wo::rslib_extern_symbols::init_wo_lib();

    for (int command_idx = 0; command_idx + 1 < argc; command_idx++)
    {
        std::string current_arg = argv[command_idx];
        if (current_arg.size() >= 2 && current_arg[0] == '-' && current_arg[1] == '-')
        {
            current_arg = current_arg.substr(2);
            if ("local" == current_arg)
                basic_env_local = argv[++command_idx];
            else if ("enable-std" == current_arg)
                enable_std_package = atoi(argv[++command_idx]);
            else if ("enable-shell" == current_arg)
                wo::config::ENABLE_SHELL_PACKAGE = atoi(argv[++command_idx]);
            else if ("enable-ctrlc-debug" == current_arg)
                enable_ctrl_c_to_debug = atoi(argv[++command_idx]);
            else if ("enable-gc" == current_arg)
                enable_gc = atoi(argv[++command_idx]);
            else if ("enable-ansi-color" == current_arg)
                wo::config::ENABLE_OUTPUT_ANSI_COLOR_CTRL = atoi(argv[++command_idx]);
            else if ("enable-jit" == current_arg)
                wo::config::ENABLE_JUST_IN_TIME = (bool)atoi(argv[++command_idx]);
            else if ("mem-chunk-size" == current_arg)
                wo::config::MEMORY_CHUNK_SIZE = (size_t)atoll(argv[++command_idx]);
            else if ("enable-pdb" == current_arg)
                wo::config::ENABLE_PDB_INFORMATIONS = (bool)atoi(argv[++command_idx]);
            else if ("enable-vm-pool" == current_arg)
                enable_vm_pool = (bool)atoi(argv[++command_idx]);
            else if ("enable-halt-when-panic" == current_arg)
                wo::config::ENABLE_HALT_WHEN_PANIC = (bool)atoi(argv[++command_idx]);
            else if ("update-grammar" == current_arg)
                wo::config::ENABLE_CHECK_GRAMMAR_AND_UPDATE = (bool)atoi(argv[++command_idx]);
            else if ("ignore-not-found-extern-func" == current_arg)
                wo::config::ENABLE_IGNORE_NOT_FOUND_EXTERN_SYMBOL = (bool)atoi(argv[++command_idx]);
            else
                wo::wo_stderr << ANSI_HIR "Woolang: " << ANSI_RST << "unknown setting --" << current_arg << wo::wo_endl;
        }
    }

    womem_init(wo::config::MEMORY_CHUNK_SIZE);

    wo::wo_init_locale(basic_env_local);
    wo::wstring_pool::init_global_str_pool();

    if (enable_vm_pool)
        global_vm_pool = new wo::vmpool;

#ifdef _DEBUG
    do
    {
        wo::start_string_pool_guard sg;

        std::wstring test_instance_a = L"Helloworld";
        std::wstring test_instance_b = test_instance_a;

        wo_assert(&test_instance_a != &test_instance_b);

        auto* p_a = wo::wstring_pool::get_pstr(test_instance_a);
        auto* p_b = wo::wstring_pool::get_pstr(test_instance_b);

        wo_assert(p_a == p_b);
    } while (0);
#endif

    if (enable_gc)
        wo::gc::gc_start(); // I dont know who will disable gc..

    if (enable_std_package)
    {
        wo_virtual_source(wo_stdlib_src_path, wo_stdlib_src_data, WO_FALSE);
        wo_virtual_source(wo_stdlib_debug_src_path, wo_stdlib_debug_src_data, WO_FALSE);
        wo_virtual_source(wo_stdlib_macro_src_path, wo_stdlib_macro_src_data, WO_FALSE);
        if (wo::config::ENABLE_SHELL_PACKAGE)
            wo_virtual_source(wo_stdlib_shell_src_path, wo_stdlib_shell_src_data, WO_FALSE);
    }

    if (enable_ctrl_c_to_debug)
        wo_handle_ctrl_c(_wo_ctrl_c_signal_handler);

    wo_assure(wo::get_wo_grammar()); // Create grammar when init.
}

#define WO_VAL(v) (std::launder(reinterpret_cast<wo::value*>(v)))
#define WO_VM(v) (std::launder(reinterpret_cast<wo::vmbase*>(v)))
#define CS_VAL(v) (reinterpret_cast<wo_value>(v))
#define CS_VM(v) (reinterpret_cast<wo_vm>(v))

wo_string_t wo_locale_name()
{
    return wo::wo_global_locale_name.c_str();
}

wo_string_t wo_exe_path()
{
    return wo::exe_path();
}

void wo_set_exe_path(wo_string_t path)
{
    wo::set_exe_path(path);

    // re-init wo lib handle;
    wo::rslib_extern_symbols::init_wo_lib();
}

wo_string_t wo_work_path()
{
    return wo::work_path();
}

wo_bool_t wo_set_work_path(wo_string_t path)
{
    return WO_CBOOL(wo::set_work_path(path));
}

wo_bool_t wo_equal_byte(wo_value a, wo_value b)
{
    auto left = WO_VAL(a), right = WO_VAL(b);
    return WO_CBOOL(left->type == right->type && left->handle == right->handle);
}

wo_ptr_t wo_safety_pointer(wo::gchandle_t* gchandle)
{
    wo::gcbase::gc_read_guard g1(gchandle);
    auto* pointer = gchandle->m_holding_handle;
    if (pointer == nullptr)
    {
        wo_fail(WO_FAIL_ACCESS_NIL, "Reading a closed gchandle.");
    }
    return pointer;
}

wo_type wo_valuetype(wo_value value)
{
    auto* _rsvalue = WO_VAL(value);

    return (wo_type)_rsvalue->type;
}
wo_integer_t wo_int(wo_value value)
{
    auto* _rsvalue = WO_VAL(value);
    if (_rsvalue->type != wo::value::valuetype::integer_type)
    {
        wo_fail(WO_FAIL_TYPE_FAIL, "This value is not an integer.");
        return wo_cast_int(value);
    }
    return _rsvalue->integer;
}
wo_char_t wo_char(wo_value value)
{
    return (wchar_t)wo_int(value);
}
wo_real_t wo_real(wo_value value)
{
    auto _rsvalue = WO_VAL(value);
    if (_rsvalue->type != wo::value::valuetype::real_type)
    {
        wo_fail(WO_FAIL_TYPE_FAIL, "This value is not an real.");
        return wo_cast_real(value);
    }
    return _rsvalue->real;
}
float wo_float(wo_value value)
{
    auto _rsvalue = WO_VAL(value);
    if (_rsvalue->type != wo::value::valuetype::real_type)
    {
        wo_fail(WO_FAIL_TYPE_FAIL, "This value is not an real.");
        return wo_cast_float(value);
    }
    return (float)_rsvalue->real;
}
wo_handle_t wo_handle(wo_value value)
{
    auto* _rsvalue = WO_VAL(value);
    if (_rsvalue->type != wo::value::valuetype::handle_type
        && _rsvalue->type != wo::value::valuetype::gchandle_type)
    {
        wo_fail(WO_FAIL_TYPE_FAIL, "This value is not a handle.");
        return wo_cast_handle(value);
    }
    return _rsvalue->type == wo::value::valuetype::handle_type ?
        (wo_handle_t)_rsvalue->handle
        :
        (wo_handle_t)wo_safety_pointer(_rsvalue->gchandle);
}
wo_ptr_t wo_pointer(wo_value value)
{
    auto* _rsvalue = WO_VAL(value);
    if (_rsvalue->type != wo::value::valuetype::handle_type
        && _rsvalue->type != wo::value::valuetype::gchandle_type)
    {
        wo_fail(WO_FAIL_TYPE_FAIL, "This value is not a handle.");
        return wo_cast_pointer(value);
    }
    return _rsvalue->type == wo::value::valuetype::handle_type ?
        (wo_ptr_t)_rsvalue->handle
        :
        (wo_ptr_t)wo_safety_pointer(_rsvalue->gchandle);
}
wo_string_t wo_string(wo_value value)
{
    auto* _rsvalue = WO_VAL(value);
    if (_rsvalue->type != wo::value::valuetype::string_type)
    {
        wo_fail(WO_FAIL_TYPE_FAIL, "This value is not a string.");
        return "<not string value>";
    }
    return _rsvalue->string->c_str();
}

const void* wo_buffer(wo_value value)
{
    return (const void*)wo_string(value);
}

wo_bool_t wo_bool(wo_value value)
{
    auto _rsvalue = WO_VAL(value);
    if (_rsvalue->type != wo::value::valuetype::bool_type)
    {
        wo_fail(WO_FAIL_TYPE_FAIL, "This value is not a boolean.");
        return wo_cast_bool(value);
    }
    return WO_CBOOL(_rsvalue->integer != 0);
}
//wo_value wo_value_of_gchandle(wo_value value)
//{
//    auto _rsvalue = WO_VAL(value);
//    if (_rsvalue->type != wo::value::valuetype::gchandle_type)
//    {
//        wo_fail(WO_FAIL_TYPE_FAIL, "This value is not a gchandle.");
//        return nullptr;
//    }
//    return CS_VAL(&_rsvalue->gchandle->holding_value);
//}

void wo_set_nil(wo_value value)
{
    WO_VAL(value)->set_nil();
}
void wo_set_int(wo_value value, wo_integer_t val)
{
    auto* _rsvalue = WO_VAL(value);
    _rsvalue->set_integer(val);
}
void wo_set_char(wo_value value, wo_char_t val)
{
    wo_set_int(value, (wo_integer_t)val);
}
void wo_set_real(wo_value value, wo_real_t val)
{
    auto* _rsvalue = WO_VAL(value);
    _rsvalue->set_real(val);
}
void wo_set_float(wo_value value, float val)
{
    auto* _rsvalue = WO_VAL(value);
    _rsvalue->set_real((wo_real_t)val);
}
void wo_set_handle(wo_value value, wo_handle_t val)
{
    auto* _rsvalue = WO_VAL(value);
    _rsvalue->set_handle(val);
}
void wo_set_pointer(wo_value value, wo_ptr_t val)
{
    if (val)
        WO_VAL(value)->set_handle((wo_handle_t)val);
    else
        wo_fail(WO_FAIL_ACCESS_NIL, "Cannot set a nullptr");
}
void wo_set_string(wo_value value, wo_vm vm, wo_string_t val)
{
    auto* _rsvalue = WO_VAL(value);

    _wo_enter_gc_guard g(vm);
    _rsvalue->set_string(val);
}
void wo_set_string_fmt(wo_value value, wo_vm vm, wo_string_t fmt, ...)
{
    va_list v1, v2;
    va_start(v1, fmt);
    va_copy(v2, v1);
    std::vector<char> buf(1 + vsnprintf(nullptr, 0, fmt, v1));
    va_end(v1);
    std::vsnprintf(buf.data(), buf.size(), fmt, v2);
    va_end(v2);
    wo_set_string(value, vm, buf.data());
}
void wo_set_buffer(wo_value value, wo_vm vm, const void* val, size_t len)
{
    auto* _rsvalue = WO_VAL(value);

    _wo_enter_gc_guard g(vm);
    _rsvalue->set_buffer(val, len);
}

void wo_set_bool(wo_value value, wo_bool_t val)
{
    auto* _rsvalue = WO_VAL(value);
    _rsvalue->set_bool(val != WO_FALSE);
}
void wo_set_gchandle(wo_value value, wo_vm vm, wo_ptr_t resource_ptr, wo_value holding_val, void(*destruct_func)(wo_ptr_t))
{
    wo_assert(resource_ptr != nullptr && destruct_func != nullptr);

    wo::gchandle_t* handle_ptr;
    {
        _wo_enter_gc_guard g(vm);
        handle_ptr = wo::gchandle_t::gc_new<wo::gcbase::gctype::young>();

        // NOTE: This function may defined in other libraries,
        //      so we need store gc vm for decrease.
        WO_VM(vm)->inc_destructable_instance_count();
        WO_VAL(value)->set_gcunit<wo::value::valuetype::gchandle_type>(handle_ptr);
    }

    handle_ptr->m_gc_vm = wo_gc_vm(vm);
    handle_ptr->m_holding_handle = resource_ptr;
    handle_ptr->m_destructor = destruct_func;
    if (holding_val)
    {
        wo::gcbase::unit_attrib* _;
        handle_ptr->m_holding_gcbase = WO_VAL(holding_val)->get_gcunit_with_barrier(&_);
    }
    else
        handle_ptr->m_holding_gcbase = nullptr;

}
void wo_set_val(wo_value value, wo_value val)
{
    auto* _rsvalue = WO_VAL(value);
    _rsvalue->set_val(WO_VAL(val));
}

void wo_set_dup(wo_value value, wo_vm vm, wo_value val)
{
    auto* _rsvalue = WO_VAL(value);

    _wo_enter_gc_guard g(vm);
    _rsvalue->set_dup(WO_VAL(val));
}

void wo_set_struct(wo_value value, wo_vm vm, uint16_t structsz)
{
    auto* _rsvalue = WO_VAL(value);

    _wo_enter_gc_guard g(vm);
    _rsvalue->set_gcunit<wo::value::valuetype::struct_type>(
        wo::struct_t::gc_new<wo::gcbase::gctype::young>(structsz));
}

void wo_set_arr(wo_value value, wo_vm vm, wo_int_t count)
{
    auto* _rsvalue = WO_VAL(value);

    _wo_enter_gc_guard g(vm);
    _rsvalue->set_gcunit<wo::value::valuetype::array_type>(
        wo::array_t::gc_new<wo::gcbase::gctype::young>((size_t)count));
}

void wo_set_map(wo_value value, wo_vm vm)
{
    auto _rsvalue = WO_VAL(value);

    _wo_enter_gc_guard g(vm);
    _rsvalue->set_gcunit<wo::value::valuetype::dict_type>(
        wo::dict_t::gc_new<wo::gcbase::gctype::young>());
}

wo_integer_t wo_cast_int(wo_value value)
{
    auto _rsvalue = WO_VAL(value);

    switch (_rsvalue->type)
    {
    case wo::value::valuetype::bool_type:
        return _rsvalue->integer == 0 ? 0 : 1;
    case wo::value::valuetype::integer_type:
        return _rsvalue->integer;
    case wo::value::valuetype::handle_type:
        return (wo_integer_t)_rsvalue->handle;
    case wo::value::valuetype::real_type:
        return (wo_integer_t)_rsvalue->real;
    case wo::value::valuetype::string_type:
        return (wo_integer_t)atoll(_rsvalue->string->c_str());
    default:
        wo_fail(WO_FAIL_TYPE_FAIL, "This value can not cast to integer.");
        return 0;
        break;
    }
}
wo_real_t wo_cast_real(wo_value value)
{
    auto _rsvalue = WO_VAL(value);

    switch (reinterpret_cast<wo::value*>(value)->type)
    {
    case wo::value::valuetype::bool_type:
        return _rsvalue->integer == 0 ? 0. : 1.;
    case wo::value::valuetype::integer_type:
        return (wo_real_t)_rsvalue->integer;
    case wo::value::valuetype::handle_type:
        return (wo_real_t)_rsvalue->handle;
    case wo::value::valuetype::real_type:
        return _rsvalue->real;
    case wo::value::valuetype::string_type:
        return atof(_rsvalue->string->c_str());
    default:
        wo_fail(WO_FAIL_TYPE_FAIL, "This value can not cast to real.");
        return 0;
        break;
    }
}

float wo_cast_float(wo_value value)
{
    return (float)wo_cast_real(value);
}

wo_handle_t wo_cast_handle(wo_value value)
{
    auto _rsvalue = WO_VAL(value);

    switch (reinterpret_cast<wo::value*>(value)->type)
    {
    case wo::value::valuetype::bool_type:
        return _rsvalue->integer == 0 ? 0 : 1;
    case wo::value::valuetype::integer_type:
        return (wo_handle_t)_rsvalue->integer;
    case wo::value::valuetype::handle_type:
        return _rsvalue->handle;
    case wo::value::valuetype::gchandle_type:
        return (wo_handle_t)wo_safety_pointer(_rsvalue->gchandle);
    case wo::value::valuetype::real_type:
        return (wo_handle_t)_rsvalue->real;
    case wo::value::valuetype::string_type:
        return (wo_handle_t)atoll(_rsvalue->string->c_str());
    default:
        wo_fail(WO_FAIL_TYPE_FAIL, "This value can not cast to handle.");
        return 0;
        break;
    }
}
wo_ptr_t wo_cast_pointer(wo_value value)
{
    return (wo_ptr_t)wo_cast_handle(value);
}

std::string _enstring(const std::string& sstr, bool need_wrap)
{
    if (need_wrap)
    {
        const char* str = sstr.c_str();
        std::string result;
        while (*str)
        {
            unsigned char uch = *str;
            if (iscntrl(uch))
            {
                char encode[8] = {};
                sprintf(encode, "\\u00%02x", (unsigned int)uch);

                result += encode;
            }
            else
            {
                switch (uch)
                {
                case '"':
                    result += R"(\")"; break;
                case '\\':
                    result += R"(\\)"; break;
                default:
                    result += *str; break;
                }
            }
            ++str;
        }
        return "\"" + result + "\"";
    }
    else
        return sstr;
}
std::string _destring(const std::string& dstr)
{
    const char* str = dstr.c_str();
    std::string result;
    if (*str == '"')
        ++str;
    while (*str)
    {
        char uch = *str;
        if (uch == '\\')
        {
            // Escape character 
            char escape_ch = *++str;
            switch (escape_ch)
            {
            case '\'':
            case '"':
            case '?':
            case '\\':
                result += escape_ch; break;
            case 'a':
                result += '\a'; break;
            case 'b':
                result += '\b'; break;
            case 'f':
                result += '\f'; break;
            case 'n':
                result += '\n'; break;
            case 'r':
                result += '\r'; break;
            case 't':
                result += L'\t'; break;
            case 'v':
                result += '\v'; break;
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
            {
                // oct 1byte 
                unsigned char oct_ascii = escape_ch - '0';
                for (int i = 0; i < 2; i++)
                {
                    unsigned char nextch = (unsigned char)*++str;
                    if (wo::lexer::lex_isodigit(nextch))
                    {
                        oct_ascii *= 8;
                        oct_ascii += wo::lexer::lex_hextonum(nextch);
                    }
                    else
                        break;
                }
                result += oct_ascii;
                break;
            }
            case 'X':
            case 'x':
            {
                // hex 1byte 
                unsigned char hex_ascii = 0;
                for (int i = 0; i < 2; i++)
                {
                    unsigned char nextch = (unsigned char)*++str;
                    if (wo::lexer::lex_isxdigit(nextch))
                    {
                        hex_ascii *= 16;
                        hex_ascii += wo::lexer::lex_hextonum(nextch);
                    }
                    else if (i == 0)
                        goto str_escape_sequences_fail;
                    else
                        break;
                }
                result += (char)hex_ascii;
                break;
            }
            case 'U':
            case 'u':
            {
                // hex 1byte 
                unsigned char hex_ascii = 0;
                for (int i = 0; i < 4; i++)
                {
                    unsigned char nextch = (unsigned char)*++str;
                    if (wo::lexer::lex_isxdigit(nextch))
                    {
                        hex_ascii *= 16;
                        hex_ascii += wo::lexer::lex_hextonum(nextch);
                    }
                    else if (i == 0)
                        goto str_escape_sequences_fail;
                    else
                        break;
                }
                result += (char)hex_ascii;
                break;
            }
            default:
            str_escape_sequences_fail:
                result += escape_ch;
                break;
            }
        }
        else if (uch == '"')
            break;
        else
            result += uch;
        ++str;
    }
    return result;
}
wo_bool_t _wo_cast_value(wo_vm vm, wo::value* value, wo::lexer* lex, wo::value::valuetype except_type);
wo_bool_t _wo_cast_array(wo_vm vm, wo::value* value, wo::lexer* lex)
{
    wo::array_t* rsarr = WO_VAL(wo_push_arr(vm, 0))->array;

    while (true)
    {
        auto lex_type = lex->peek(nullptr);
        if (lex_type == wo::lex_type::l_index_end)
        {
            lex->next(nullptr);
            break;
        }

        if (!_wo_cast_value(vm, value, lex, wo::value::valuetype::invalid)) // val!
            return WO_FALSE;
        rsarr->push_back(*value);

        if (lex->peek(nullptr) == wo::lex_type::l_comma)
            lex->next(nullptr);
    }
    value->set_gcunit<wo::value::valuetype::array_type>(rsarr);

    wo_pop_stack(vm);
    return WO_TRUE;
}
wo_bool_t _wo_cast_map(wo_vm vm, wo::value* value, wo::lexer* lex)
{
    wo::dict_t* rsmap = WO_VAL(wo_push_map(vm))->dict;

    while (true)
    {
        auto lex_type = lex->peek(nullptr);
        if (lex_type == wo::lex_type::l_right_curly_braces)
        {
            // end
            lex->next(nullptr);
            break;
        }

        if (!_wo_cast_value(vm, value, lex, wo::value::valuetype::invalid))// key!
            return WO_FALSE;
        auto& val_place = (*rsmap)[*value];

        lex_type = lex->next(nullptr);
        if (lex_type != wo::lex_type::l_typecast)
            //wo_fail(WO_FAIL_TYPE_FAIL, "Unexcept token while parsing map, here should be ':'.");
            return WO_FALSE;

        if (!_wo_cast_value(vm, &val_place, lex, wo::value::valuetype::invalid)) // value!
            return WO_FALSE;

        if (lex->peek(nullptr) == wo::lex_type::l_comma)
            lex->next(nullptr);
    }

    value->set_gcunit<wo::value::valuetype::dict_type>(rsmap);

    wo_pop_stack(vm);
    return WO_TRUE;
}
wo_bool_t _wo_cast_value(wo_vm vm, wo::value* value, wo::lexer* lex, wo::value::valuetype except_type)
{
    std::wstring wstr;
    auto lex_type = lex->next(&wstr);
    if (lex_type == wo::lex_type::l_left_curly_braces) // is map
    {
        if (!_wo_cast_map(vm, value, lex))
            return WO_FALSE;
    }
    else if (lex_type == wo::lex_type::l_index_begin) // is array
    {
        if (!_wo_cast_array(vm, value, lex))
            return WO_FALSE;
    }
    else if (lex_type == wo::lex_type::l_literal_string) // is string   
        value->set_string(wo::wstr_to_str(wstr).c_str());
    else if (lex_type == wo::lex_type::l_add
        || lex_type == wo::lex_type::l_sub
        || lex_type == wo::lex_type::l_literal_integer
        || lex_type == wo::lex_type::l_literal_real) // is integer
    {
        bool positive = true;
        if (lex_type == wo::lex_type::l_sub || lex_type == wo::lex_type::l_add)
        {
            if (lex_type == wo::lex_type::l_sub)
                positive = false;

            lex_type = lex->next(&wstr);
            if (lex_type != wo::lex_type::l_literal_integer
                && lex_type != wo::lex_type::l_literal_real)
                // wo_fail(WO_FAIL_TYPE_FAIL, "Unknown token while parsing.");
                return WO_FALSE;
        }

        if (lex_type == wo::lex_type::l_literal_integer) // is real
            value->set_integer(positive
                ? std::stoll(wo::wstr_to_str(wstr).c_str())
                : -std::stoll(wo::wstr_to_str(wstr).c_str()));
        else if (lex_type == wo::lex_type::l_literal_real) // is real
            value->set_real(positive
                ? std::stod(wo::wstr_to_str(wstr).c_str())
                : -std::stod(wo::wstr_to_str(wstr).c_str()));

    }
    else if (lex_type == wo::lex_type::l_nil) // is nil
        value->set_nil();
    else if (wstr == L"true")
        value->set_bool(true);// true
    else if (wstr == L"false")
        value->set_bool(false);// false
    else if (wstr == L"null")
        value->set_nil();// null
    else
        //wo_fail(WO_FAIL_TYPE_FAIL, "Unknown token while parsing.");
        return WO_FALSE;

    if (except_type != wo::value::valuetype::invalid && except_type != value->type)
        // wo_fail(WO_FAIL_TYPE_FAIL, "Unexcept value type after parsing.");
        return WO_FALSE;
    return WO_TRUE;

}
wo_bool_t wo_deserialize(wo_vm vm, wo_value value, wo_string_t str, wo_type except_type)
{
    wo::lexer lex(wo::str_to_wstr(str), "json");
    return _wo_cast_value(vm, WO_VAL(value), &lex, (wo::value::valuetype)except_type);
}

enum cast_string_mode
{
    FORMAT,
    FIT,
    SERIALIZE,
};

bool _wo_cast_string(
    wo::value* value,
    std::string* out_str,
    cast_string_mode mode,
    std::map<wo::gcbase*, int>* traveled_gcunit,
    int depth)
{
    switch (value->type)
    {
    case wo::value::valuetype::bool_type:
        *out_str += value->integer ? "true" : "false";
        return true;
    case wo::value::valuetype::integer_type:
        *out_str += std::to_string(value->integer);
        return true;
    case wo::value::valuetype::handle_type:
        *out_str += std::to_string(value->handle);
        return true;
    case wo::value::valuetype::real_type:
        *out_str += std::to_string(value->real);
        return true;
    case wo::value::valuetype::gchandle_type:
        *out_str += std::to_string((wo_handle_t)wo_safety_pointer(value->gchandle));
        return true;
    case wo::value::valuetype::string_type:
        *out_str += _enstring(*value->string, true);
        return true;
    case wo::value::valuetype::dict_type:
    {
        wo::dict_t* map = value->dict;
        wo::gcbase::gc_read_guard rg1(map);

        if ((*traveled_gcunit)[map] >= 1)
        {
            if (mode == cast_string_mode::SERIALIZE)
                return false;

            mode = cast_string_mode::FIT;
            if ((*traveled_gcunit)[map] >= 2)
            {
                *out_str += "{...}";
                return true;
            }
        }
        (*traveled_gcunit)[map]++;

        bool _fit_layout = (mode != cast_string_mode::FORMAT);

        *out_str += _fit_layout ? "{" : "{\n";
        bool first_kv_pair = true;
        for (auto& [v_key, v_val] : *map)
        {
            if (!first_kv_pair)
                *out_str += _fit_layout ? "," : ",\n";
            first_kv_pair = false;

            for (int i = 0; !_fit_layout && i <= depth; i++)
                *out_str += "    ";
            if (!_wo_cast_string(const_cast<wo::value*>(&v_key), out_str, mode, traveled_gcunit, depth + 1))
                return false;
            *out_str += _fit_layout ? ":" : " : ";
            if (!_wo_cast_string(&v_val, out_str, mode, traveled_gcunit, depth + 1))
                return false;

        }
        if (!_fit_layout)
            *out_str += "\n";
        for (int i = 0; !_fit_layout && i < depth; i++)
            *out_str += "    ";
        *out_str += "}";

        (*traveled_gcunit)[map]--;

        return true;
    }
    case wo::value::valuetype::array_type:
    {
        wo::array_t* arr = value->array;
        wo::gcbase::gc_read_guard rg1(value->array);
        if ((*traveled_gcunit)[arr] >= 1)
        {
            if (mode == cast_string_mode::SERIALIZE)
                return false;

            mode = cast_string_mode::FIT;
            if ((*traveled_gcunit)[arr] >= 2)
            {
                *out_str += "[...]";
                return true;
            }
        }
        (*traveled_gcunit)[arr]++;

        bool _fit_layout = (mode != cast_string_mode::FORMAT);

        *out_str += _fit_layout ? "[" : "[\n";
        bool first_value = true;
        for (auto& v_val : *arr)
        {
            if (!first_value)
                *out_str += _fit_layout ? "," : ",\n";
            first_value = false;

            for (int i = 0; !_fit_layout && i <= depth; i++)
                *out_str += "    ";
            if (!_wo_cast_string(&v_val, out_str, mode, traveled_gcunit, depth + 1))
                return false;
        }
        if (!_fit_layout)
            *out_str += "\n";
        for (int i = 0; !_fit_layout && i < depth; i++)
            *out_str += "    ";
        *out_str += "]";

        (*traveled_gcunit)[arr]--;
        return true;
    }
    case wo::value::valuetype::struct_type:
    {
        if (mode == cast_string_mode::SERIALIZE)
            return false;

        wo::struct_t* struc = value->structs;
        wo::gcbase::gc_read_guard rg1(struc);

        if ((*traveled_gcunit)[struc] >= 1)
        {
            mode = cast_string_mode::FIT;
            if ((*traveled_gcunit)[struc] >= 2)
            {
                *out_str += "struct{...}";
                return true;
            }
        }
        (*traveled_gcunit)[struc]++;

        bool _fit_layout = (mode != cast_string_mode::FORMAT);

        *out_str += _fit_layout ? "struct{" : "struct {\n";
        bool first_value = true;
        for (uint16_t i = 0; i < value->structs->m_count; ++i)
        {
            if (!first_value)
                *out_str += _fit_layout ? "," : ",\n";
            first_value = false;

            for (int i = 0; !_fit_layout && i <= depth; i++)
                *out_str += "    ";

            *out_str += "+" + std::to_string(i) + (_fit_layout ? "=" : " = ");
            if (!_wo_cast_string(&value->structs->m_values[i], out_str, mode, traveled_gcunit, depth + 1))
                return false;
        }
        if (!_fit_layout)
            *out_str += "\n";
        for (int i = 0; !_fit_layout && i < depth; i++)
            *out_str += "    ";
        *out_str += "}";

        (*traveled_gcunit)[struc]--;
        return true;
    }
    case wo::value::valuetype::closure_type:
        if (mode == cast_string_mode::SERIALIZE)
            return false;
        *out_str += "<closure function>";
        return true;
    case wo::value::valuetype::invalid:
        *out_str += "nil";
        return true;
    default:
        wo_fail(WO_FAIL_TYPE_FAIL, "This value can not cast to string.");
        *out_str += "nil";
        return true;
    }

}

wo_bool_t wo_serialize(wo_value value, wo_string_t* out_str)
{
    thread_local std::string _buf;
    _buf = "";

    std::map<wo::gcbase*, int> _tved_gcunit;
    if (_wo_cast_string(WO_VAL(value), &_buf, cast_string_mode::SERIALIZE, &_tved_gcunit, 0))
    {
        *out_str = _buf.c_str();
        return WO_TRUE;
    }
    return WO_FALSE;
}

wo_bool_t wo_cast_bool(wo_value value)
{
    auto _rsvalue = WO_VAL(value);
    switch (_rsvalue->type)
    {
    case wo::value::valuetype::bool_type:
    case wo::value::valuetype::integer_type:
        return WO_CBOOL(_rsvalue->integer != 0);
    case wo::value::valuetype::handle_type:
        return WO_CBOOL(_rsvalue->handle != 0);
    case wo::value::valuetype::real_type:
        return WO_CBOOL(_rsvalue->real != 0);
    case wo::value::valuetype::string_type:
        return WO_CBOOL(_rsvalue->string->compare("true") == 0);
    default:
        wo_fail(WO_FAIL_TYPE_FAIL, "This value can not cast to bool.");
        break;
    }
    return WO_FALSE;
}
wo_string_t wo_cast_string(wo_value value)
{
    thread_local std::string _buf;
    _buf = "";

    auto _rsvalue = WO_VAL(value);
    switch (_rsvalue->type)
    {
    case wo::value::valuetype::bool_type:
        _buf = _rsvalue->integer ? "true" : "false";
        return _buf.c_str();
    case wo::value::valuetype::integer_type:
        _buf = std::to_string(_rsvalue->integer);
        return _buf.c_str();
    case wo::value::valuetype::handle_type:
        _buf = std::to_string(_rsvalue->handle);
        return _buf.c_str();
    case wo::value::valuetype::gchandle_type:
        _buf = std::to_string((wo_handle_t)wo_safety_pointer(_rsvalue->gchandle));
        return _buf.c_str();
    case wo::value::valuetype::real_type:
        _buf = std::to_string(_rsvalue->real);
        return _buf.c_str();
    case wo::value::valuetype::string_type:
        return _rsvalue->string->c_str();
    case wo::value::valuetype::closure_type:
        return "<closure function>";
    case wo::value::valuetype::invalid:
        return "nil";
    default:
        break;
    }

    std::map<wo::gcbase*, int> _tved_gcunit;
    _wo_cast_string(WO_VAL(value), &_buf, cast_string_mode::FORMAT, &_tved_gcunit, 0);

    return _buf.c_str();
}

wo_string_t wo_type_name(wo_type type)
{
    switch ((wo::value::valuetype)type)
    {
    case wo::value::valuetype::bool_type:
        return "bool";
    case wo::value::valuetype::integer_type:
        return "int";
    case wo::value::valuetype::handle_type:
        return "handle";
    case wo::value::valuetype::real_type:
        return "real";
    case wo::value::valuetype::string_type:
        return "string";
    case wo::value::valuetype::array_type:
        return "array";
    case wo::value::valuetype::dict_type:
        return "dict";
    case wo::value::valuetype::gchandle_type:
        return "gchandle";
    case wo::value::valuetype::closure_type:
        return "closure";
    case wo::value::valuetype::struct_type:
        return "struct";
    case wo::value::valuetype::invalid:
        return "nil";
    default:
        return "unknown";
    }
}

wo_integer_t wo_argc(wo_vm vm)
{
    return WO_VM(vm)->tc->integer;
}
wo_result_t wo_ret_bool(wo_vm vm, wo_bool_t result)
{
    WO_VM(vm)->cr->set_bool(result != WO_FALSE);
    return wo_result_t::WO_API_NORMAL;
}
wo_result_t wo_ret_int(wo_vm vm, wo_integer_t result)
{
    WO_VM(vm)->cr->set_integer(result);
    return wo_result_t::WO_API_NORMAL;
}
wo_result_t  wo_ret_char(wo_vm vm, wo_char_t result)
{
    wo_ret_int(vm, (wo_integer_t)result);
    return wo_result_t::WO_API_NORMAL;
}
wo_result_t wo_ret_real(wo_vm vm, wo_real_t result)
{
    WO_VM(vm)->cr->set_real(result);
    return wo_result_t::WO_API_NORMAL;
}
wo_result_t wo_ret_float(wo_vm vm, float result)
{
    WO_VM(vm)->cr->set_real((wo_real_t)result);
    return wo_result_t::WO_API_NORMAL;
}
wo_result_t wo_ret_handle(wo_vm vm, wo_handle_t result)
{
    WO_VM(vm)->cr->set_handle(result);
    return wo_result_t::WO_API_NORMAL;
}
wo_result_t wo_ret_pointer(wo_vm vm, wo_ptr_t result)
{
    if (result)
    {
        WO_VM(vm)->cr->set_handle((wo_handle_t)result);
        return wo_result_t::WO_API_NORMAL;
    }
    return wo_ret_panic(vm, "Cannot return a nullptr");
}
wo_result_t wo_ret_string(wo_vm vm, wo_string_t result)
{
    _wo_enter_gc_guard g(vm);
    WO_VM(vm)->cr->set_string(result);
    return wo_result_t::WO_API_NORMAL;
}

wo_result_t wo_ret_string_fmt(wo_vm vm, wo_string_t fmt, ...)
{
    va_list v1, v2;
    va_start(v1, fmt);
    va_copy(v2, v1);
    std::vector<char> buf(1 + vsnprintf(nullptr, 0, fmt, v1));
    va_end(v1);
    std::vsnprintf(buf.data(), buf.size(), fmt, v2);
    va_end(v2);
    return wo_ret_string(vm, buf.data());
}

wo_result_t wo_ret_buffer(wo_vm vm, const void* result, size_t len)
{
    _wo_enter_gc_guard g(vm);
    WO_VM(vm)->cr->set_buffer(result, len);
    return wo_result_t::WO_API_NORMAL;
}
wo_result_t wo_ret_gchandle(wo_vm vm, wo_ptr_t resource_ptr, wo_value holding_val, void(*destruct_func)(wo_ptr_t))
{
    wo_set_gchandle(CS_VAL(WO_VM(vm)->cr), vm, resource_ptr, holding_val, destruct_func);
    return wo_result_t::WO_API_NORMAL;
}
wo_result_t wo_ret_val(wo_vm vm, wo_value result)
{
    wo_assert(result);
    WO_VM(vm)->cr->set_val(WO_VAL(result));
    return wo_result_t::WO_API_NORMAL;
}

wo_result_t wo_ret_dup(wo_vm vm, wo_value result)
{
    auto* val = WO_VAL(result);

    _wo_enter_gc_guard g(vm);
    WO_VM(vm)->cr->set_dup(val);

    return wo_result_t::WO_API_NORMAL;
}

wo_result_t wo_ret_halt(wo_vm vm, wo_string_t reasonfmt, ...)
{
    va_list v1, v2;
    va_start(v1, reasonfmt);
    va_copy(v2, v1);
    std::vector<char> buf(1 + vsnprintf(nullptr, 0, reasonfmt, v1));
    va_end(v1);
    std::vsnprintf(buf.data(), buf.size(), reasonfmt, v2);
    va_end(v2);

    auto* vmptr = WO_VM(vm);
    {
        _wo_enter_gc_guard g(vm);
        vmptr->er->set_string(buf.data());
    }
    vmptr->interrupt(wo::vmbase::vm_interrupt_type::ABORT_INTERRUPT);
    wo::wo_stderr << ANSI_HIR "Halt happend: " ANSI_RST << wo_cast_string((wo_value)vmptr->er) << wo::wo_endl;
    vmptr->dump_call_stack(32, true, std::cerr);
    return wo_result_t::WO_API_NORMAL;
}

wo_result_t wo_ret_panic(wo_vm vm, wo_string_t reasonfmt, ...)
{
    va_list v1, v2;
    va_start(v1, reasonfmt);
    va_copy(v2, v1);
    std::vector<char> buf(1 + vsnprintf(nullptr, 0, reasonfmt, v1));
    va_end(v1);
    std::vsnprintf(buf.data(), buf.size(), reasonfmt, v2);
    va_end(v2);

    auto* vmptr = WO_VM(vm);
    {
        _wo_enter_gc_guard g(vm);
        vmptr->er->set_string(buf.data());
    }
    wo_fail(WO_FAIL_USER_PANIC, vmptr->er->string->c_str());
    return wo_result_t::WO_API_NORMAL;
}

void wo_set_option_void(wo_value val, wo_vm vm)
{
    auto* wovm = WO_VM(vm);
    auto* target_val = WO_VAL(val);

    wo::struct_t* structptr;
    {
        _wo_enter_gc_guard g(vm);
        structptr = wo::struct_t::gc_new<wo::gcbase::gctype::young>(2);
        target_val->set_gcunit<wo::value::valuetype::struct_type>(structptr);
    }

    structptr->m_values[0].set_integer(1);
    structptr->m_values[1].set_nil();
}
void wo_set_option_bool(wo_value val, wo_vm vm, wo_bool_t result)
{
    auto* wovm = WO_VM(vm);
    auto* target_val = WO_VAL(val);

    wo::struct_t* structptr;
    {
        _wo_enter_gc_guard g(vm);
        structptr = wo::struct_t::gc_new<wo::gcbase::gctype::young>(2);
        target_val->set_gcunit<wo::value::valuetype::struct_type>(structptr);
    }

    structptr->m_values[0].set_integer(1);
    structptr->m_values[1].set_bool(result != WO_FALSE);

}
void wo_set_option_char(wo_value val, wo_vm vm, wo_char_t result)
{
    return wo_set_option_int(val, vm, (wo_integer_t)result);
}
void wo_set_option_int(wo_value val, wo_vm vm, wo_integer_t result)
{
    auto* wovm = WO_VM(vm);
    auto* target_val = WO_VAL(val);

    wo::struct_t* structptr;
    {
        _wo_enter_gc_guard g(vm);
        structptr = wo::struct_t::gc_new<wo::gcbase::gctype::young>(2);
        target_val->set_gcunit<wo::value::valuetype::struct_type>(structptr);
    }

    structptr->m_values[0].set_integer(1);
    structptr->m_values[1].set_integer(result);


}
void wo_set_option_real(wo_value val, wo_vm vm, wo_real_t result)
{
    auto* wovm = WO_VM(vm);
    auto* target_val = WO_VAL(val);

    wo::struct_t* structptr;
    {
        _wo_enter_gc_guard g(vm);
        structptr = wo::struct_t::gc_new<wo::gcbase::gctype::young>(2);
        target_val->set_gcunit<wo::value::valuetype::struct_type>(structptr);
    }

    structptr->m_values[0].set_integer(1);
    structptr->m_values[1].set_real(result);


}
void wo_set_option_float(wo_value val, wo_vm vm, float result)
{
    auto* wovm = WO_VM(vm);
    auto* target_val = WO_VAL(val);

    wo::struct_t* structptr;
    {
        _wo_enter_gc_guard g(vm);
        structptr = wo::struct_t::gc_new<wo::gcbase::gctype::young>(2);
        target_val->set_gcunit<wo::value::valuetype::struct_type>(structptr);
    }

    structptr->m_values[0].set_integer(1);
    structptr->m_values[1].set_real((wo_real_t)result);
}
void wo_set_option_handle(wo_value val, wo_vm vm, wo_handle_t result)
{
    auto* wovm = WO_VM(vm);
    auto* target_val = WO_VAL(val);

    wo::struct_t* structptr;
    {
        _wo_enter_gc_guard g(vm);
        structptr = wo::struct_t::gc_new<wo::gcbase::gctype::young>(2);
        target_val->set_gcunit<wo::value::valuetype::struct_type>(structptr);
    }

    structptr->m_values[0].set_integer(1);
    structptr->m_values[1].set_handle(result);


}
void wo_set_option_string(wo_value val, wo_vm vm, wo_string_t result)
{
    auto* wovm = WO_VM(vm);
    auto* target_val = WO_VAL(val);

    wo::struct_t* structptr;
    {
        _wo_enter_gc_guard g(vm);
        structptr = wo::struct_t::gc_new<wo::gcbase::gctype::young>(2);
        target_val->set_gcunit<wo::value::valuetype::struct_type>(structptr);
        structptr->m_values[1].set_string(result);
    }
    structptr->m_values[0].set_integer(1);



}
void wo_set_option_buffer(wo_value val, wo_vm vm, const void* result, size_t len)
{
    auto* wovm = WO_VM(vm);
    auto* target_val = WO_VAL(val);

    wo::struct_t* structptr;
    {
        _wo_enter_gc_guard g(vm);
        structptr = wo::struct_t::gc_new<wo::gcbase::gctype::young>(2);
        target_val->set_gcunit<wo::value::valuetype::struct_type>(structptr);

        structptr->m_values[1].set_buffer(result, len);
    }
    structptr->m_values[0].set_integer(1);
}
void wo_set_option_pointer(wo_value val, wo_vm vm, wo_ptr_t result)
{
    auto* wovm = WO_VM(vm);
    auto* target_val = WO_VAL(val);

    wo::struct_t* structptr;
    {
        _wo_enter_gc_guard g(vm);
        structptr = wo::struct_t::gc_new<wo::gcbase::gctype::young>(2);
        target_val->set_gcunit<wo::value::valuetype::struct_type>(structptr);
    }

    structptr->m_values[0].set_integer(1);
    structptr->m_values[1].set_handle((wo_handle_t)result);
    if (nullptr == result)
        wo_fail(WO_FAIL_ACCESS_NIL, "Cannot return nullptr");
}
void wo_set_option_ptr(wo_value val, wo_vm vm, wo_ptr_t result)
{
    auto* wovm = WO_VM(vm);
    auto* target_val = WO_VAL(val);

    wo::struct_t* structptr;
    {
        _wo_enter_gc_guard g(vm);
        structptr = wo::struct_t::gc_new<wo::gcbase::gctype::young>(2);
        target_val->set_gcunit<wo::value::valuetype::struct_type>(structptr);
    }

    if (result)
    {
        structptr->m_values[0].set_integer(1);
        structptr->m_values[1].set_handle((wo_handle_t)result);
    }
    else
        structptr->m_values[0].set_integer(2);
}
void wo_set_option_val(wo_value val, wo_vm vm, wo_value result)
{
    auto* wovm = WO_VM(vm);
    auto* target_val = WO_VAL(val);

    wo::struct_t* structptr;
    {
        _wo_enter_gc_guard g(vm);
        structptr = wo::struct_t::gc_new<wo::gcbase::gctype::young>(2);
        target_val->set_gcunit<wo::value::valuetype::struct_type>(structptr);
    }

    structptr->m_values[0].set_integer(1);
    structptr->m_values[1].set_val(WO_VAL(result));
}
void wo_set_option_gchandle(wo_value val, wo_vm vm, wo_ptr_t resource_ptr, wo_value holding_val, void(*destruct_func)(wo_ptr_t))
{
    auto* wovm = WO_VM(vm);
    auto* target_val = WO_VAL(val);

    wo::struct_t* structptr;
    {
        _wo_enter_gc_guard g(vm);
        structptr = wo::struct_t::gc_new<wo::gcbase::gctype::young>(2);
        target_val->set_gcunit<wo::value::valuetype::struct_type>(structptr);
    }

    structptr->m_values[0].set_integer(1);
    wo_set_gchandle(CS_VAL(&structptr->m_values[1]), vm, resource_ptr, holding_val, destruct_func);
}
void wo_set_option_none(wo_value val, wo_vm vm)
{
    auto* wovm = WO_VM(vm);
    auto* target_val = WO_VAL(val);

    wo::struct_t* structptr;
    {
        _wo_enter_gc_guard g(vm);
        structptr = wo::struct_t::gc_new<wo::gcbase::gctype::young>(2);
        target_val->set_gcunit<wo::value::valuetype::struct_type>(structptr);
    }
    structptr->m_values[0].set_integer(2);
}

void wo_set_err_void(wo_value val, wo_vm vm)
{
    auto* wovm = WO_VM(vm);
    auto* target_val = WO_VAL(val);

    wo::struct_t* structptr;
    {
        _wo_enter_gc_guard g(vm);
        structptr = wo::struct_t::gc_new<wo::gcbase::gctype::young>(2);
        target_val->set_gcunit<wo::value::valuetype::struct_type>(structptr);
    }

    structptr->m_values[0].set_integer(2);
    structptr->m_values[1].set_nil();


}
void wo_set_err_char(wo_value val, wo_vm vm, wo_char_t result)
{
    return wo_set_err_int(val, vm, (wo_integer_t)result);
}
void wo_set_err_bool(wo_value val, wo_vm vm, wo_bool_t result)
{
    auto* wovm = WO_VM(vm);
    auto* target_val = WO_VAL(val);

    wo::struct_t* structptr;
    {
        _wo_enter_gc_guard g(vm);
        structptr = wo::struct_t::gc_new<wo::gcbase::gctype::young>(2);
        target_val->set_gcunit<wo::value::valuetype::struct_type>(structptr);
    }

    structptr->m_values[0].set_integer(2);
    structptr->m_values[1].set_bool(result != WO_FALSE);
}
void wo_set_err_int(wo_value val, wo_vm vm, wo_integer_t result)
{
    auto* wovm = WO_VM(vm);
    auto* target_val = WO_VAL(val);

    wo::struct_t* structptr;
    {
        _wo_enter_gc_guard g(vm);
        structptr = wo::struct_t::gc_new<wo::gcbase::gctype::young>(2);
        target_val->set_gcunit<wo::value::valuetype::struct_type>(structptr);
    }

    structptr->m_values[0].set_integer(2);
    structptr->m_values[1].set_integer(result);
}
void wo_set_err_real(wo_value val, wo_vm vm, wo_real_t result)
{
    auto* wovm = WO_VM(vm);
    auto* target_val = WO_VAL(val);

    wo::struct_t* structptr;
    {
        _wo_enter_gc_guard g(vm);
        structptr = wo::struct_t::gc_new<wo::gcbase::gctype::young>(2);
        target_val->set_gcunit<wo::value::valuetype::struct_type>(structptr);
    }

    structptr->m_values[0].set_integer(2);
    structptr->m_values[1].set_real(result);


}
void wo_set_err_float(wo_value val, wo_vm vm, float result)
{
    auto* wovm = WO_VM(vm);
    auto* target_val = WO_VAL(val);

    wo::struct_t* structptr;
    {
        _wo_enter_gc_guard g(vm);
        structptr = wo::struct_t::gc_new<wo::gcbase::gctype::young>(2);
        target_val->set_gcunit<wo::value::valuetype::struct_type>(structptr);
    }

    structptr->m_values[0].set_integer(2);
    structptr->m_values[1].set_real((wo_real_t)result);
}
void wo_set_err_handle(wo_value val, wo_vm vm, wo_handle_t result)
{
    auto* wovm = WO_VM(vm);
    auto* target_val = WO_VAL(val);

    wo::struct_t* structptr;
    {
        _wo_enter_gc_guard g(vm);
        structptr = wo::struct_t::gc_new<wo::gcbase::gctype::young>(2);
        target_val->set_gcunit<wo::value::valuetype::struct_type>(structptr);
    }

    structptr->m_values[0].set_integer(2);
    structptr->m_values[1].set_handle(result);


}
void wo_set_err_string(wo_value val, wo_vm vm, wo_string_t result)
{
    auto* wovm = WO_VM(vm);
    auto* target_val = WO_VAL(val);

    wo::struct_t* structptr;
    {
        _wo_enter_gc_guard g(vm);
        structptr = wo::struct_t::gc_new<wo::gcbase::gctype::young>(2);
        target_val->set_gcunit<wo::value::valuetype::struct_type>(structptr);
        structptr->m_values[1].set_string(result);
    }

    structptr->m_values[0].set_integer(2);
}
void wo_set_err_buffer(wo_value val, wo_vm vm, const void* result, size_t len)
{
    auto* wovm = WO_VM(vm);
    auto* target_val = WO_VAL(val);

    wo::struct_t* structptr;
    {
        _wo_enter_gc_guard g(vm);
        structptr = wo::struct_t::gc_new<wo::gcbase::gctype::young>(2);
        target_val->set_gcunit<wo::value::valuetype::struct_type>(structptr);
        structptr->m_values[1].set_buffer(result, len);
    }

    structptr->m_values[0].set_integer(2);
}
void wo_set_err_pointer(wo_value val, wo_vm vm, wo_ptr_t result)
{
    auto* wovm = WO_VM(vm);
    auto* target_val = WO_VAL(val);

    wo::struct_t* structptr;
    {
        _wo_enter_gc_guard g(vm);
        structptr = wo::struct_t::gc_new<wo::gcbase::gctype::young>(2);
        target_val->set_gcunit<wo::value::valuetype::struct_type>(structptr);
    }

    structptr->m_values[0].set_integer(2);
    structptr->m_values[1].set_handle((wo_handle_t)result);

    if (nullptr == result)
        wo_fail(WO_FAIL_ACCESS_NIL, "Cannot return nullptr");
}
void wo_set_err_val(wo_value val, wo_vm vm, wo_value result)
{
    auto* wovm = WO_VM(vm);
    auto* target_val = WO_VAL(val);

    wo::struct_t* structptr;
    {
        _wo_enter_gc_guard g(vm);
        structptr = wo::struct_t::gc_new<wo::gcbase::gctype::young>(2);
        target_val->set_gcunit<wo::value::valuetype::struct_type>(structptr);
    }

    structptr->m_values[0].set_integer(2);
    structptr->m_values[1].set_val(WO_VAL(result));


}
void wo_set_err_gchandle(wo_value val, wo_vm vm, wo_ptr_t resource_ptr, wo_value holding_val, void(*destruct_func)(wo_ptr_t))
{
    auto* wovm = WO_VM(vm);
    auto* target_val = WO_VAL(val);

    wo::struct_t* structptr;
    {
        _wo_enter_gc_guard g(vm);
        structptr = wo::struct_t::gc_new<wo::gcbase::gctype::young>(2);
        target_val->set_gcunit<wo::value::valuetype::struct_type>(structptr);
    }

    structptr->m_values[0].set_integer(2);
    wo_set_gchandle(CS_VAL(&structptr->m_values[1]), vm, resource_ptr, holding_val, destruct_func);

}

wo_result_t wo_ret_option_void(wo_vm vm)
{
    auto* wovm = WO_VM(vm);
    wo_set_option_void(CS_VAL(wovm->cr), vm);
    return wo_result_t::WO_API_NORMAL;
}
wo_result_t  wo_ret_option_bool(wo_vm vm, wo_bool_t result)
{
    auto* wovm = WO_VM(vm);
    wo_set_option_bool(CS_VAL(wovm->cr), vm, result);
    return wo_result_t::WO_API_NORMAL;
}
wo_result_t wo_ret_option_char(wo_vm vm, wo_char_t result)
{
    auto* wovm = WO_VM(vm);
    wo_set_option_char(CS_VAL(wovm->cr), vm, result);
    return wo_result_t::WO_API_NORMAL;
}
wo_result_t wo_ret_option_int(wo_vm vm, wo_integer_t result)
{
    auto* wovm = WO_VM(vm);
    wo_set_option_int(CS_VAL(wovm->cr), vm, result);
    return wo_result_t::WO_API_NORMAL;
}
wo_result_t wo_ret_option_real(wo_vm vm, wo_real_t result)
{
    auto* wovm = WO_VM(vm);
    wo_set_option_real(CS_VAL(wovm->cr), vm, result);
    return wo_result_t::WO_API_NORMAL;
}
wo_result_t wo_ret_option_float(wo_vm vm, float result)
{
    auto* wovm = WO_VM(vm);
    wo_set_option_float(CS_VAL(wovm->cr), vm, result);
    return wo_result_t::WO_API_NORMAL;
}
wo_result_t  wo_ret_option_handle(wo_vm vm, wo_handle_t result)
{
    auto* wovm = WO_VM(vm);
    wo_set_option_handle(CS_VAL(wovm->cr), vm, result);
    return wo_result_t::WO_API_NORMAL;
}
wo_result_t  wo_ret_option_string(wo_vm vm, wo_string_t result)
{
    auto* wovm = WO_VM(vm);
    wo_set_option_string(CS_VAL(wovm->cr), vm, result);
    return wo_result_t::WO_API_NORMAL;
}
wo_result_t  wo_ret_option_buffer(wo_vm vm, const void* result, size_t len)
{
    auto* wovm = WO_VM(vm);
    wo_set_option_buffer(CS_VAL(wovm->cr), vm, result, len);
    return wo_result_t::WO_API_NORMAL;
}
wo_result_t wo_ret_option_pointer(wo_vm vm, wo_ptr_t result)
{
    auto* wovm = WO_VM(vm);
    wo_set_option_pointer(CS_VAL(wovm->cr), vm, result);
    return wo_result_t::WO_API_NORMAL;
}
wo_result_t wo_ret_option_ptr(wo_vm vm, wo_ptr_t result)
{
    auto* wovm = WO_VM(vm);
    wo_set_option_ptr(CS_VAL(wovm->cr), vm, result);
    return wo_result_t::WO_API_NORMAL;
}
wo_result_t wo_ret_option_val(wo_vm vm, wo_value result)
{
    auto* wovm = WO_VM(vm);
    wo_set_option_val(CS_VAL(wovm->cr), vm, result);
    return wo_result_t::WO_API_NORMAL;
}
wo_result_t wo_ret_option_gchandle(wo_vm vm, wo_ptr_t resource_ptr, wo_value holding_val, void(*destruct_func)(wo_ptr_t))
{
    auto* wovm = WO_VM(vm);
    wo_set_option_gchandle(CS_VAL(wovm->cr), vm, resource_ptr, holding_val, destruct_func);
    return wo_result_t::WO_API_NORMAL;
}
wo_result_t wo_ret_option_none(wo_vm vm)
{
    auto* wovm = WO_VM(vm);
    wo_set_option_none(CS_VAL(wovm->cr), vm);
    return wo_result_t::WO_API_NORMAL;
}

wo_result_t wo_ret_err_void(wo_vm vm)
{
    auto* wovm = WO_VM(vm);
    wo_set_err_void(CS_VAL(wovm->cr), vm);
    return wo_result_t::WO_API_NORMAL;
}
wo_result_t wo_ret_err_char(wo_vm vm, wo_char_t result)
{
    auto* wovm = WO_VM(vm);
    wo_set_err_char(CS_VAL(wovm->cr), vm, result);
    return wo_result_t::WO_API_NORMAL;
}
wo_result_t wo_ret_err_bool(wo_vm vm, wo_bool_t result)
{
    auto* wovm = WO_VM(vm);
    wo_set_err_bool(CS_VAL(wovm->cr), vm, result);
    return wo_result_t::WO_API_NORMAL;
}
wo_result_t wo_ret_err_int(wo_vm vm, wo_integer_t result)
{
    auto* wovm = WO_VM(vm);
    wo_set_err_int(CS_VAL(wovm->cr), vm, result);
    return wo_result_t::WO_API_NORMAL;
}
wo_result_t wo_ret_err_real(wo_vm vm, wo_real_t result)
{
    auto* wovm = WO_VM(vm);
    wo_set_err_real(CS_VAL(wovm->cr), vm, result);
    return wo_result_t::WO_API_NORMAL;
}
wo_result_t wo_ret_err_float(wo_vm vm, float result)
{
    auto* wovm = WO_VM(vm);
    wo_set_err_float(CS_VAL(wovm->cr), vm, result);
    return wo_result_t::WO_API_NORMAL;
}
wo_result_t wo_ret_err_handle(wo_vm vm, wo_handle_t result)
{
    auto* wovm = WO_VM(vm);
    wo_set_err_handle(CS_VAL(wovm->cr), vm, result);
    return wo_result_t::WO_API_NORMAL;
}
wo_result_t wo_ret_err_string(wo_vm vm, wo_string_t result)
{
    auto* wovm = WO_VM(vm);
    wo_set_err_string(CS_VAL(wovm->cr), vm, result);
    return wo_result_t::WO_API_NORMAL;
}
wo_result_t wo_ret_err_buffer(wo_vm vm, const void* result, size_t len)
{
    auto* wovm = WO_VM(vm);
    wo_set_err_buffer(CS_VAL(wovm->cr), vm, result, len);
    return wo_result_t::WO_API_NORMAL;
}
wo_result_t wo_ret_err_pointer(wo_vm vm, wo_ptr_t result)
{
    auto* wovm = WO_VM(vm);
    wo_set_err_pointer(CS_VAL(wovm->cr), vm, result);
    return wo_result_t::WO_API_NORMAL;
}
wo_result_t wo_ret_err_val(wo_vm vm, wo_value result)
{
    auto* wovm = WO_VM(vm);
    wo_set_err_val(CS_VAL(wovm->cr), vm, result);
    return wo_result_t::WO_API_NORMAL;
}
wo_result_t wo_ret_err_gchandle(wo_vm vm, wo_ptr_t resource_ptr, wo_value holding_val, void(*destruct_func)(wo_ptr_t))
{
    auto* wovm = WO_VM(vm);
    wo_set_err_gchandle(CS_VAL(wovm->cr), vm, resource_ptr, holding_val, destruct_func);
    return wo_result_t::WO_API_NORMAL;
}

void _wo_check_atexit()
{
    std::shared_lock g1(wo::vmbase::_alive_vm_list_mx);

    do
    {
    waitting_vm_leave:
        for (auto& vm : wo::vmbase::_alive_vm_list)
            if (!(vm->vm_interrupt & wo::vmbase::LEAVE_INTERRUPT))
                goto waitting_vm_leave;
    } while (0);

    // STOP GC
}

void wo_abort_all_vm_to_exit()
{
    // wo_stop used for stop all vm and exit..

    // 1. ABORT ALL VM
    std::shared_lock g1(wo::vmbase::_alive_vm_list_mx);

    for (auto& vm : wo::vmbase::_alive_vm_list)
        vm->interrupt(wo::vmbase::ABORT_INTERRUPT);

    std::atexit(_wo_check_atexit);
}

wo_integer_t wo_lengthof(wo_value value)
{
    auto _rsvalue = WO_VAL(value);
    if (_rsvalue->is_nil())
        return 0;
    if (_rsvalue->type == wo::value::valuetype::array_type)
    {
        wo::gcbase::gc_read_guard rg1(_rsvalue->array);
        return (wo_integer_t)_rsvalue->array->size();
    }
    else if (_rsvalue->type == wo::value::valuetype::dict_type)
    {
        wo::gcbase::gc_read_guard rg1(_rsvalue->dict);
        return (wo_integer_t)_rsvalue->dict->size();
    }
    else if (_rsvalue->type == wo::value::valuetype::string_type)
        return (wo_integer_t)wo::u8strlen(_rsvalue->string->c_str());
    else if (_rsvalue->type == wo::value::valuetype::struct_type)
    {
        // no need lock for struct's count
        return (wo_integer_t)_rsvalue->structs->m_count;
    }
    else
    {
        wo_fail(WO_FAIL_TYPE_FAIL, "Only 'string','array', 'struct' or 'map' can get length.");
        return 0;
    }
}

wo_int_t wo_str_bytelen(wo_value value)
{
    auto _rsvalue = WO_VAL(value);
    if (_rsvalue->type == wo::value::valuetype::string_type)
        return (wo_int_t)_rsvalue->string->size();

    wo_fail(WO_FAIL_TYPE_FAIL, "Only 'string' can get byte length.");
    return 0;
}

wchar_t wo_str_get_char(wo_string_t str, wo_int_t index)
{
    wchar_t ch = wo::u8stridx(str, (size_t)index);
    if (ch == 0 && wo::u8strlen(str) <= (size_t)index)
        wo_fail(WO_FAIL_INDEX_FAIL, "Index out of range.");
    return ch;
}

wo_wstring_t wo_str_to_wstr(wo_string_t str)
{
    static thread_local std::wstring wstr_buf;
    return (wstr_buf = wo::str_to_wstr(str)).c_str();
}

wo_string_t  wo_wstr_to_str(wo_wstring_t str)
{
    static thread_local std::string str_buf;
    return (str_buf = wo::wstr_to_str(str)).c_str();
}

void wo_enable_jit(wo_bool_t option)
{
    wo::config::ENABLE_JUST_IN_TIME = (option != WO_FALSE);
}

wo_bool_t wo_virtual_binary(wo_string_t filepath, const void* data, size_t len, wo_bool_t enable_modify)
{
    return WO_CBOOL(wo::create_virtual_binary((const char*)data, len, wo::str_to_wstr(filepath), enable_modify != WO_FALSE));
}

wo_bool_t wo_virtual_source(wo_string_t filepath, wo_string_t data, wo_bool_t enable_modify)
{
    return wo_virtual_binary(filepath, data, strlen(data), enable_modify);
}

wo_bool_t wo_remove_virtual_file(wo_string_t filepath)
{
    return WO_CBOOL(wo::remove_virtual_binary(wo::str_to_wstr(filepath)));
}

wo_vm wo_create_vm()
{
    return (wo_vm)new wo::vm;
}

wo_vm wo_sub_vm(wo_vm vm, size_t stacksz)
{
    return CS_VM(WO_VM(vm)->make_machine(stacksz));
}

wo_vm wo_gc_vm(wo_vm vm)
{
    return CS_VM(WO_VM(vm)->get_or_alloc_gcvm());
}

void wo_close_vm(wo_vm vm)
{
    delete (wo::vmbase*)vm;
}

wo_vm wo_borrow_vm(wo_vm vm)
{
    if (global_vm_pool != nullptr)
        return CS_VM(global_vm_pool->borrow_vm_from_exists_vm(WO_VM(vm)));
    return wo_sub_vm(vm, 1024);
}
void wo_release_vm(wo_vm vm)
{
    if (global_vm_pool != nullptr)
        global_vm_pool->release_vm(WO_VM(vm));
    else
        wo_close_vm(vm);
}

std::variant<
    wo::shared_pointer<wo::runtime_env>,
    wo::lexer*
> _wo_compile_to_nojit_env(wo_string_t virtual_src_path, const void* src, size_t len, size_t stacksz)
{
    if (stacksz == 0)
        stacksz = 1024;

    // 0. Try load binary
    const char* load_binary_failed_reason = nullptr;
    bool is_valid_binary = false;

    wo::shared_pointer<wo::runtime_env> env_result =
        wo::runtime_env::load_create_env_from_binary(
            virtual_src_path, src, len, stacksz,
            &load_binary_failed_reason,
            &is_valid_binary);

    if (env_result != nullptr)
    {
        // Load binary successfully!
        wo_assert(load_binary_failed_reason == nullptr);
        return env_result;
    }
    else if (is_valid_binary)
    {
        // Failed to load binary, maybe broken or version missing.
        wo_assert(load_binary_failed_reason != nullptr);
        // Has error, create a fake lexer to store error reason.

        wo::lexer* lex = new wo::lexer(L"", virtual_src_path);
        lex->lex_error(wo::lexer::errorlevel::error, wo_str_to_wstr(load_binary_failed_reason));

        return lex;
    }

    // 1. Prepare lexer..
    wo::lexer* lex = nullptr;
    if (src != nullptr)
        lex = new wo::lexer(wo::str_to_wstr(std::string((const char*)src, len).c_str()), virtual_src_path);
    else
    {
        bool is_virtual_file;
        std::wstring real_file_path;

        std::optional<std::unique_ptr<std::wistream>> content_stream =
            std::nullopt;

        uint64_t src_crc64_result = 0;

        if (wo::check_virtual_file_path(
            &is_virtual_file,
            &real_file_path,
            wo::str_to_wstr(virtual_src_path),
            std::nullopt))
        {
            content_stream = wo::open_virtual_file_stream<true>(
                real_file_path, is_virtual_file);

            if (content_stream)
                src_crc64_result = wo::crc_64(*content_stream.value(), 0);
        }

        lex = new wo::lexer(std::move(content_stream), virtual_src_path);
        wo_assure(!lex->has_been_imported(src_crc64_result));
    }

    lex->has_been_imported(lex->source_file);

    std::forward_list<wo::ast::ast_base*> m_last_context;
    bool need_exchange_back = wo::ast::ast_base::exchange_this_thread_ast(m_last_context);
    if (!lex->has_error())
    {
        // 2. Lexer will create ast_tree;
        auto result = wo::get_wo_grammar()->gen(*lex);
        if (result)
        {
            // 3. Create lang, most anything store here..
            wo::lang lang(*lex);

            lang.analyze_pass1(result);
            if (!lang.has_compile_error())
            {
                lang.analyze_pass2(result);
            }

            //result->display();
            if (!lang.has_compile_error())
            {
                wo::ir_compiler compiler;
                lang.analyze_finalize(result, &compiler);

                if (!lang.has_compile_error())
                {
                    compiler.end();
                    env_result = compiler.finalize(stacksz);
                }
            }
        }
    }

    wo::ast::ast_base::clean_this_thread_ast();

    if (need_exchange_back)
        wo::ast::ast_base::exchange_this_thread_ast(m_last_context);

    if (env_result)
    {
        delete lex;
        return env_result;
    }
    else
        return lex;
}

wo_bool_t _wo_load_source(wo_vm vm, wo_string_t virtual_src_path, const void* src, size_t len, size_t stacksz)
{
    wo::start_string_pool_guard sg;

    auto&& env_or_lex = _wo_compile_to_nojit_env(virtual_src_path, src, len, stacksz);
    if (auto* env_p = std::get_if<wo::shared_pointer<wo::runtime_env>>(&env_or_lex))
    {
        auto& env = *env_p;
        ((wo::vm*)vm)->set_runtime(env);
        return WO_TRUE;
    }
    else
    {
        auto* lex_p = std::get_if<wo::lexer*>(&env_or_lex);
        wo_assert(nullptr != lex_p);
        WO_VM(vm)->compile_info = *lex_p;
        return WO_FALSE;
    }
}

wo_bool_t wo_load_binary_with_stacksz(wo_vm vm, wo_string_t virtual_src_path, const void* buffer, size_t length, size_t stacksz)
{
    static std::atomic_size_t vcount = 0;
    std::string vpath;
    if (virtual_src_path == nullptr)
        vpath = "__runtime_script_" + std::to_string(++vcount) + "__";
    else
        vpath = virtual_src_path;

    if (!wo_virtual_binary(vpath.c_str(), buffer, length, WO_TRUE))
        return WO_FALSE;

    return _wo_load_source(vm, vpath.c_str(), buffer, length, stacksz);
}

wo_bool_t wo_load_binary(wo_vm vm, wo_string_t virtual_src_path, const void* buffer, size_t length)
{
    return wo_load_binary_with_stacksz(vm, virtual_src_path, buffer, length, 0);
}

void* wo_dump_binary(wo_vm vm, wo_bool_t saving_pdi, size_t* out_length)
{
    auto [bufptr, bufsz] = WO_VM(vm)->env->create_env_binary(saving_pdi != WO_FALSE);
    *out_length = bufsz;
    return bufptr;
}
void wo_free_binary(void* buffer)
{
    free(buffer);
}

wo_bool_t wo_has_compile_error(wo_vm vm)
{
    if (vm && WO_VM(vm)->compile_info && WO_VM(vm)->compile_info->has_error())
        return WO_TRUE;
    return WO_FALSE;
}

std::wstring _dump_src_info(const std::string& path, size_t beginaimrow, size_t beginpointplace, size_t aimrow, size_t pointplace, _wo_inform_style style)
{
    std::wstring src_full_path, result;
    bool is_virtual_file;

    if (wo::check_virtual_file_path(&is_virtual_file, &src_full_path, wo::str_to_wstr(path), std::nullopt))
    {
        auto content_stream = wo::open_virtual_file_stream<true>(src_full_path, is_virtual_file);
        if (content_stream)
        {
            auto& content_stream_ptr = content_stream.value();
            wo_assert(content_stream_ptr != nullptr);

            constexpr size_t UP_DOWN_SHOWN_LINE = 2;
            size_t current_row_no = 1;
            size_t current_col_no = 1;
            size_t from = beginaimrow > UP_DOWN_SHOWN_LINE ? beginaimrow - UP_DOWN_SHOWN_LINE : 0;
            size_t to = aimrow + UP_DOWN_SHOWN_LINE;

            bool first_line = true;

            auto print_src_file_print_lineno = [&current_row_no, &result, &first_line]() {
                wchar_t buf[20] = {};
                if (first_line)
                {
                    first_line = false;
                    swprintf(buf, 19, L"%-5zu | ", current_row_no);
                }
                else
                    swprintf(buf, 19, L"\n%-5zu | ", current_row_no);
                result += buf;
            };
            auto print_notify_line = [&result, &first_line, &current_row_no, beginpointplace, pointplace, style, beginaimrow, aimrow](size_t line_end_place) {
                wchar_t buf[20] = {};
                if (first_line)
                {
                    first_line = false;
                    swprintf(buf, 19, L"      | ");
                }
                else
                    swprintf(buf, 19, L"\n      | ");

                std::wstring append_result = buf;

                if (style == _wo_inform_style::WO_NEED_COLOR)
                    append_result += wo::str_to_wstr(ANSI_HIR);

                if (current_row_no == beginaimrow && current_row_no == aimrow)
                {
                    size_t i = 1;
                    for (; i < beginpointplace; i++)
                        append_result += L" ";
                    for (; i < pointplace; i++)
                        append_result += L"~";
                    append_result += L"~\\ HERE";
                }
                else if (current_row_no == beginaimrow)
                {
                    size_t i = 1;
                    for (; i < beginpointplace; i++)
                        append_result += L" ";
                    if (i < line_end_place)
                    {
                        for (; i < line_end_place; i++)
                            append_result += L"~";
                    }
                    else
                        return;
                }
                else if (current_row_no == aimrow)
                {
                    for (size_t i = 1; i < pointplace; i++)
                        append_result += L"~";
                    append_result += L"~\\ HERE";
                }
                else
                {
                    size_t i = 1;
                    if (i < line_end_place)
                    {
                        for (; i < line_end_place; i++)
                            append_result += L"~";
                    }
                    else
                        return;
                }
                if (style == _wo_inform_style::WO_NEED_COLOR)
                    append_result += wo::str_to_wstr(ANSI_RST);

                result += append_result;
            };

            if (from <= current_row_no && current_row_no <= to)
                print_src_file_print_lineno();

            for (;;)
            {
                wchar_t ch;
                content_stream_ptr->read(&ch, 1);

                if (content_stream_ptr->eof() || !*content_stream_ptr)
                    break;

                if (ch == L'\n')
                {
                    if (current_row_no >= beginaimrow && current_row_no <= aimrow)
                        print_notify_line(current_col_no);
                    current_col_no = 1;
                    current_row_no++;
                    if (from <= current_row_no && current_row_no <= to)
                        print_src_file_print_lineno();
                    continue;
                }
                else if (ch == L'\r')
                {
                    if (current_row_no >= beginaimrow && current_row_no <= aimrow)
                        print_notify_line(current_col_no);
                    current_col_no = 1;
                    current_row_no++;
                    if (from <= current_row_no && current_row_no <= to)
                        print_src_file_print_lineno();

                    auto index = content_stream_ptr->tellg();
                    content_stream_ptr->read(&ch, 1);
                    if (content_stream_ptr->eof() || !*content_stream_ptr || ch != L'\n')
                    {
                        content_stream_ptr->clear(content_stream_ptr->rdstate() & ~std::ios_base::failbit);
                        content_stream_ptr->seekg(index);
                    }
                    continue;
                }
                ++current_col_no;
                if (from <= current_row_no && current_row_no <= to && ch)
                    result += ch;

            }
            if (current_row_no >= beginaimrow && current_row_no <= aimrow)
                print_notify_line(current_col_no);
            result += L"\n";
        }
    }

    return result;
}

std::string _wo_dump_lexer_context_error(wo::lexer* lex, _wo_inform_style style)
{
    std::string src_file_path = "";
    size_t errcount = 0;

    std::string _vm_compile_errors;

    for (auto& err_info : lex->lex_error_list)
    {
        if (++errcount > 100)
        {
            _vm_compile_errors += wo::wstr_to_str(WO_TOO_MANY_ERROR(lex->lex_error_list.size()) + L"\n");
            break;
        }
        if (src_file_path != err_info.filename)
        {
            if (style == WO_NEED_COLOR)
                _vm_compile_errors += ANSI_HIR "In file: '" ANSI_RST + (src_file_path = err_info.filename) + ANSI_HIR "'" ANSI_RST "\n";
            else
                _vm_compile_errors += "In file: '" + (src_file_path = err_info.filename) + "'\n";
        }
        _vm_compile_errors += wo::wstr_to_str(err_info.to_wstring(style & WO_NEED_COLOR)) + "\n";

        // Print source informations..
        _vm_compile_errors += wo::wstr_to_str(
            _dump_src_info(src_file_path, err_info.begin_row, err_info.begin_col, err_info.end_row, err_info.end_col, style)) + "\n";
    }

    return _vm_compile_errors;
}

wo_string_t wo_get_compile_error(wo_vm vm, _wo_inform_style style)
{
    if (style == WO_DEFAULT)
        style = wo::config::ENABLE_OUTPUT_ANSI_COLOR_CTRL ? WO_NEED_COLOR : WO_NOTHING;

    thread_local std::string _vm_compile_errors;
    _vm_compile_errors = "";

    if (vm && WO_VM(vm)->compile_info)
    {
        auto& lex = *WO_VM(vm)->compile_info;
        _vm_compile_errors += _wo_dump_lexer_context_error(&lex, style);
    }
    return _vm_compile_errors.c_str();
}

wo_string_t wo_get_runtime_error(wo_vm vm)
{
    return wo_cast_string(CS_VAL(WO_VM(vm)->er));
}

wo_bool_t wo_abort_vm(wo_vm vm)
{
    std::shared_lock gs(wo::vmbase::_alive_vm_list_mx);

    if (wo::vmbase::_alive_vm_list.find(WO_VM(vm)) != wo::vmbase::_alive_vm_list.end())
    {
        return WO_CBOOL(WO_VM(vm)->interrupt(wo::vmbase::vm_interrupt_type::ABORT_INTERRUPT));
    }
    return WO_FALSE;
}

wo_value wo_push_int(wo_vm vm, wo_int_t val)
{
    return CS_VAL((WO_VM(vm)->sp--)->set_integer(val));
}
wo_value wo_push_real(wo_vm vm, wo_real_t val)
{
    return CS_VAL((WO_VM(vm)->sp--)->set_real(val));
}
wo_value wo_push_handle(wo_vm vm, wo_handle_t val)
{
    return CS_VAL((WO_VM(vm)->sp--)->set_handle(val));
}
wo_value wo_push_pointer(wo_vm vm, wo_ptr_t val)
{
    return CS_VAL((WO_VM(vm)->sp--)->set_handle((wo_handle_t)val));
}
wo_value wo_push_gchandle(wo_vm vm, wo_ptr_t resource_ptr, wo_value holding_val, void(*destruct_func)(wo_ptr_t))
{
    auto* csp = WO_VM(vm)->sp--;
    wo_set_gchandle(CS_VAL(csp), vm, resource_ptr, holding_val, destruct_func);
    return CS_VAL(csp);
}
wo_value wo_push_string(wo_vm vm, wo_string_t val)
{
    _wo_enter_gc_guard g(vm);
    return CS_VAL((WO_VM(vm)->sp--)->set_string(val));
}
wo_value wo_push_buffer(wo_vm vm, const void* val, size_t len)
{
    _wo_enter_gc_guard g(vm);
    return CS_VAL((WO_VM(vm)->sp--)->set_buffer(val, len));
}
wo_value wo_push_empty(wo_vm vm)
{
    return CS_VAL((WO_VM(vm)->sp--)->set_nil());
}
wo_value wo_push_val(wo_vm vm, wo_value val)
{
    return CS_VAL((WO_VM(vm)->sp--)->set_val(WO_VAL(val)));
}
wo_value wo_push_dup(wo_vm vm, wo_value val)
{
    _wo_enter_gc_guard g(vm);
    return CS_VAL((WO_VM(vm)->sp--)->set_dup(WO_VAL(val)));
}
wo_value wo_push_arr(wo_vm vm, wo_int_t count)
{
    auto* _rsvalue = WO_VM(vm)->sp--;

    _wo_enter_gc_guard g(vm);
    _rsvalue->set_gcunit<wo::value::valuetype::array_type>(
        wo::array_t::gc_new<wo::gcbase::gctype::young>((size_t)count));

    return CS_VAL(_rsvalue);

}
wo_value wo_push_struct(wo_vm vm, uint16_t count)
{
    auto* _rsvalue = WO_VM(vm)->sp--;

    _wo_enter_gc_guard g(vm);
    _rsvalue->set_gcunit<wo::value::valuetype::struct_type>(
        wo::struct_t::gc_new<wo::gcbase::gctype::young>(count));

    return CS_VAL(_rsvalue);
}
wo_value wo_push_map(wo_vm vm)
{
    auto* _rsvalue = WO_VM(vm)->sp--;
    _rsvalue->type = (wo::value::valuetype::dict_type);

    _wo_enter_gc_guard g(vm);
    _rsvalue->set_gcunit<wo::value::valuetype::dict_type>(
        wo::dict_t::gc_new<wo::gcbase::gctype::young>());

    return CS_VAL(_rsvalue);
}

wo_value wo_top_stack(wo_vm vm)
{
    return CS_VAL((WO_VM(vm)->sp - 1));
}
void wo_pop_stack(wo_vm vm)
{
    ++WO_VM(vm)->sp;
}

wo_value wo_invoke_rsfunc(wo_vm vm, wo_int_t vmfunc, wo_int_t argc)
{
    _wo_in_thread_vm_guard g(vm);
    _wo_enter_gc_guard g2(vm);
    wo_value result = CS_VAL(WO_VM(vm)->invoke(vmfunc, argc));
    return result;
}
wo_value wo_invoke_exfunc(wo_vm vm, wo_handle_t exfunc, wo_int_t argc)
{
    _wo_in_thread_vm_guard g(vm);
    _wo_enter_gc_guard g2(vm);
    wo_value result = CS_VAL(WO_VM(vm)->invoke(exfunc, argc));
    return result;
}
wo_value wo_invoke_value(wo_vm vm, wo_value vmfunc, wo_int_t argc)
{
    _wo_in_thread_vm_guard g(vm);
    _wo_enter_gc_guard g2(vm);
    wo::value* valfunc = WO_VAL(vmfunc);
    wo_value result = nullptr;
    if (!vmfunc)
        wo_fail(WO_FAIL_CALL_FAIL, "Cannot call a 'nil' function.");
    else if (valfunc->type == wo::value::valuetype::integer_type)
        result = CS_VAL(WO_VM(vm)->invoke(valfunc->integer, argc));
    else if (valfunc->type == wo::value::valuetype::handle_type)
        result = CS_VAL(WO_VM(vm)->invoke(valfunc->handle, argc));
    else if (valfunc->type == wo::value::valuetype::closure_type)
        result = CS_VAL(WO_VM(vm)->invoke(valfunc->closure, argc));
    else
        wo_fail(WO_FAIL_CALL_FAIL, "Not callable type.");
    return result;
}

wo_value wo_dispatch_rsfunc(wo_vm vm, wo_int_t vmfunc, wo_int_t argc)
{
    _wo_in_thread_vm_guard g(vm);
    _wo_enter_gc_guard g2(vm);
    auto* vmm = WO_VM(vm);
    vmm->set_br_yieldable(true);
    wo_value result = CS_VAL(vmm->co_pre_invoke(vmfunc, argc));
    return result;
}
wo_value wo_dispatch_exfunc(wo_vm vm, wo_handle_t exfunc, wo_int_t argc)
{
    _wo_in_thread_vm_guard g(vm);
    _wo_enter_gc_guard g2(vm);
    auto* vmm = WO_VM(vm);
    vmm->set_br_yieldable(true);
    wo_value result = CS_VAL(vmm->co_pre_invoke(exfunc, argc));
    return result;
}
wo_value wo_dispatch_value(wo_vm vm, wo_value vmfunc, wo_int_t argc)
{
    switch (WO_VAL(vmfunc)->type)
    {
    case wo::value::valuetype::closure_type:
    {
        _wo_in_thread_vm_guard g(vm);
        _wo_enter_gc_guard g2(vm);
        WO_VM(vm)->set_br_yieldable(true);
        return CS_VAL(WO_VM(vm)->co_pre_invoke(WO_VAL(vmfunc)->closure, argc));
        break;
    }
    case wo::value::valuetype::integer_type:
        return wo_dispatch_rsfunc(vm, WO_VAL(vmfunc)->integer, argc);
    case wo::value::valuetype::handle_type:
        return wo_dispatch_exfunc(vm, WO_VAL(vmfunc)->handle, argc);
    default:
        wo_fail(WO_FAIL_TYPE_FAIL, "Cannot dispatch non-function value by 'wo_dispatch_closure'.");
    }
    return nullptr;
}

wo_value wo_dispatch(wo_vm vm)
{
    _wo_in_thread_vm_guard g(vm);
    _wo_enter_gc_guard g2(vm);

    if (WO_VM(vm)->env)
    {
        wo_assert(WO_VM(vm)->tc->type == wo::value::valuetype::integer_type);
        auto arg_count = WO_VM(vm)->er->integer;

        auto dispatch_result = WO_VM(vm)->run();
        auto br_yield = WO_VM(vm)->get_and_clear_br_yield_flag();
        switch (dispatch_result)
        {
        case wo_result_t::WO_API_NORMAL:
            break;
        case wo_result_t::WO_API_RESYNC:
            if (!br_yield)
            {
                WO_VM(vm)->run();
                br_yield = WO_VM(vm)->get_and_clear_br_yield_flag();
            }
        }

        if (br_yield)
        {
            WO_VM(vm)->er->set_integer(arg_count);
            return WO_CONTINUE;
        }
        else
        {
            WO_VM(vm)->sp += arg_count;
            if (!WO_VM(vm)->is_aborted())
                return CS_VAL(WO_VM(vm)->cr);
        }
    }
    return nullptr;
}

wo_result_t wo_ret_yield(wo_vm vm)
{
    WO_VM(vm)->interrupt(wo::vmbase::BR_YIELD_INTERRUPT);
    return wo_result_t::WO_API_NORMAL;
}

wo_bool_t wo_load_source_with_stacksz(wo_vm vm, wo_string_t virtual_src_path, wo_string_t src, size_t stacksz)
{
    return wo_load_binary_with_stacksz(vm, virtual_src_path, src, strlen(src), stacksz);
}

wo_bool_t wo_load_file_with_stacksz(wo_vm vm, wo_string_t virtual_src_path, size_t stacksz)
{
    return _wo_load_source(vm, virtual_src_path, nullptr, 0, stacksz);
}

wo_bool_t wo_load_source(wo_vm vm, wo_string_t virtual_src_path, wo_string_t src)
{
    return wo_load_source_with_stacksz(vm, virtual_src_path, src, 0);
}

wo_bool_t wo_load_file(wo_vm vm, wo_string_t virtual_src_path)
{
    return wo_load_file_with_stacksz(vm, virtual_src_path, 0);
}

wo_bool_t wo_jit(wo_vm vm)
{
    _wo_enter_gc_guard g2(vm);

    if (wo::config::ENABLE_JUST_IN_TIME)
    {
        // NOTE: other operation for vm must happend after init(wo_run).
        wo_assert(WO_VM(vm)->env->_jit_functions.empty());

        analyze_jit(const_cast<wo::byte_t*>(WO_VM(vm)->env->rt_codes), WO_VM(vm)->env);
        return WO_TRUE;
    }
    return WO_FALSE;
}

wo_value wo_run(wo_vm vm)
{
    _wo_in_thread_vm_guard g(vm);
    _wo_enter_gc_guard g2(vm);
    if (WO_VM(vm)->env)
    {
        WO_VM(vm)->ip = WO_VM(vm)->env->rt_codes;
        WO_VM(vm)->run();

        if (!WO_VM(vm)->is_aborted())
            return CS_VAL(WO_VM(vm)->cr);
    }
    return nullptr;
}

// CONTAINER OPERATE
wo_bool_t wo_struct_try_get(wo_value out_val, wo_value value, uint16_t offset)
{
    auto _struct = WO_VAL(value);

    if (_struct->is_nil())
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is 'nil'.");
    else if (_struct->type == wo::value::valuetype::struct_type)
    {
        wo::struct_t* struct_impl = _struct->structs;
        wo::gcbase::gc_read_guard gwg1(struct_impl);
        if (offset < struct_impl->m_count)
        {
            WO_VAL(out_val)->set_val(&struct_impl->m_values[offset]);
            return WO_TRUE;
        }
    }
    else
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is not a struct.");
    return WO_FALSE;
}
wo_bool_t wo_struct_try_set(wo_value value, uint16_t offset, wo_value val)
{
    auto _struct = WO_VAL(value);

    if (_struct->is_nil())
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is 'nil'.");
    else if (_struct->type == wo::value::valuetype::struct_type)
    {
        wo::struct_t* struct_impl = _struct->structs;
        wo::gcbase::gc_read_guard gwg1(struct_impl);
        if (offset < struct_impl->m_count)
        {
            auto* result = &struct_impl->m_values[offset];
            if (wo::gc::gc_is_marking())
                wo::gcbase::write_barrier(result);

            result->set_val(WO_VAL(val));
            return WO_TRUE;
        }
    }
    else
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is not a struct.");
    return WO_FALSE;
}

void wo_struct_get(wo_value out_val, wo_value value, uint16_t offset)
{
    if (!wo_struct_try_get(out_val, value, offset))
        wo_fail(WO_FAIL_INDEX_FAIL, "Failed to index: out of range.");
}
void wo_struct_set(wo_value value, uint16_t offset, wo_value val)
{
    if (!wo_struct_try_set(value, offset, val))
        wo_fail(WO_FAIL_INDEX_FAIL, "Failed to index: out of range.");
}

wo_bool_t wo_result_get(wo_value out_val, wo_value resultval)
{
    auto* val = WO_VAL(resultval);
    if (val->type != wo::value::valuetype::struct_type
        || val->structs->m_count != 2
        || val->structs->m_values[0].type
        != wo::value::valuetype::integer_type)
        wo_fail(WO_FAIL_TYPE_FAIL, "Unexpected value type.");
    else
    {
        wo_set_val(out_val, CS_VAL(&val->structs->m_values[1]));
        return WO_CBOOL(val->structs->m_values[0].integer == 1);
    }
    return WO_FALSE;
}

void wo_arr_resize(wo_value arr, wo_int_t newsz, wo_value init_val)
{
    auto _arr = WO_VAL(arr);

    if (_arr->is_nil())
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is 'nil'.");
    else if (_arr->type == wo::value::valuetype::array_type)
    {
        wo::gcbase::gc_write_guard g1(_arr->array);
        size_t arrsz = _arr->array->size();
        if ((size_t)newsz < arrsz && wo::gc::gc_is_marking())
        {
            for (size_t i = (size_t)newsz; i < arrsz; ++i)
                wo::gcbase::write_barrier(&(*_arr->array)[i]);
        }

        if (init_val != nullptr)
            _arr->array->resize((size_t)newsz, *WO_VAL(init_val));
        else
            _arr->array->resize((size_t)newsz);
    }
    else
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is not an array.");
}
wo_bool_t wo_arr_insert(wo_value arr, wo_int_t place, wo_value val)
{
    auto _arr = WO_VAL(arr);

    if (_arr->is_nil())
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is 'nil'.");
    else if (_arr->type == wo::value::valuetype::array_type)
    {
        wo::gcbase::gc_write_guard g1(_arr->array);

        if ((size_t)place <= _arr->array->size())
        {
            auto index = _arr->array->insert(_arr->array->begin() + (size_t)place, wo::value());
            index->set_val(WO_VAL(val));

            return WO_TRUE;
        }
    }
    else
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is not an array.");

    return WO_FALSE;
}
wo_bool_t wo_arr_try_set(wo_value arr, wo_int_t index, wo_value val)
{
    auto _arr = WO_VAL(arr);
    if (_arr->is_nil())
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is 'nil'.");
    else if (_arr->type == wo::value::valuetype::array_type)
    {
        wo::gcbase::gc_write_guard g1(_arr->array);

        if ((size_t)index < _arr->array->size())
        {
            auto* store_val = &_arr->array->at((size_t)index);
            wo::gcbase::write_barrier(store_val);
            store_val->set_val(WO_VAL(val));
            return WO_TRUE;
        }
    }
    else
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is not an array.");

    return WO_FALSE;
}
void wo_arr_set(wo_value arr, wo_int_t index, wo_value val)
{
    if (!wo_arr_try_set(arr, index, val))
        wo_fail(WO_FAIL_INDEX_FAIL, "Failed to index: out of range.");
}
void wo_arr_add(wo_value arr, wo_value elem)
{
    auto _arr = WO_VAL(arr);

    if (_arr->is_nil())
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is 'nil'.");
    else if (_arr->type == wo::value::valuetype::array_type)
    {
        wo::gcbase::gc_write_guard g1(_arr->array);

        if (elem)
            _arr->array->push_back(*WO_VAL(elem));
        else
            _arr->array->emplace_back(wo::value());
    }
    else
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is not an array.");
}

wo_bool_t wo_arr_try_get(wo_value out_val, wo_value arr, wo_int_t index)
{
    auto _arr = WO_VAL(arr);
    if (_arr->is_nil())
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is 'nil'.");
    else if (_arr->type == wo::value::valuetype::array_type)
    {
        wo::gcbase::gc_read_guard g1(_arr->array);

        if ((size_t)index < _arr->array->size())
        {
            WO_VAL(out_val)->set_val(&(*_arr->array)[(size_t)index]);
            return WO_TRUE;
        }
    }
    else
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is not an array.");

    return WO_FALSE;
}
void wo_arr_get(wo_value out_val, wo_value arr, wo_int_t index)
{
    if (!wo_arr_try_get(out_val, arr, index))
        wo_fail(WO_FAIL_INDEX_FAIL, "Failed to index: out of range.");
}

void wo_map_get(wo_value out_val, wo_value map, wo_value index)
{
    if (!wo_map_try_get(out_val, map, index))
        wo_fail(WO_FAIL_INDEX_FAIL, "Failed to index: out of range.");
}

wo_bool_t wo_arr_pop_front(wo_value out_val, wo_value arr)
{
    auto _arr = WO_VAL(arr);
    if (_arr->is_nil())
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is 'nil'.");
    else if (_arr->type == wo::value::valuetype::array_type)
    {
        wo::gcbase::gc_write_guard g1(_arr->array);

        if (!_arr->array->empty())
        {
            WO_VAL(out_val)->set_val(&_arr->array->front());
            _arr->array->erase(_arr->array->begin());
            return WO_TRUE;
        }
    }
    else
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is not an array.");

    return WO_FALSE;
}
wo_bool_t wo_arr_pop_back(wo_value out_val, wo_value arr)
{
    auto _arr = WO_VAL(arr);
    if (_arr->is_nil())
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is 'nil'.");
    else if (_arr->type == wo::value::valuetype::array_type)
    {
        wo::gcbase::gc_write_guard g1(_arr->array);

        if (!_arr->array->empty())
        {
            WO_VAL(out_val)->set_val(&_arr->array->back());
            _arr->array->pop_back();
            return WO_TRUE;
        }
    }
    else
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is not an array.");

    return WO_FALSE;
}

void wo_arr_pop_front_val(wo_value out_val, wo_value arr)
{
    if (!wo_arr_pop_front(out_val, arr))
    {
        wo_fail(WO_FAIL_INDEX_FAIL, "Failed to pop front.");
    }
}

void wo_arr_pop_back_val(wo_value out_val, wo_value arr)
{
    if (!wo_arr_pop_back(out_val, arr))
    {
        wo_fail(WO_FAIL_INDEX_FAIL, "Failed to pop back.");
    }
}

wo_int_t wo_arr_find(wo_value arr, wo_value elem)
{
    auto _arr = WO_VAL(arr);
    auto _aim = WO_VAL(elem);
    if (_arr->is_nil())
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is 'nil'.");
    else if (_arr->type == wo::value::valuetype::array_type)
    {
        wo::gcbase::gc_read_guard g1(_arr->array);

        auto fnd = std::find_if(_arr->array->begin(), _arr->array->end(),
            [&](const wo::value& _elem)->bool
            {
                if (_elem.type == _aim->type)
                {
                    if (_elem.type == wo::value::valuetype::string_type)
                        return *_elem.string == *_aim->string;
                    return _elem.handle == _aim->handle;
                }
                return false;
            });
        if (fnd != _arr->array->end())
            return fnd - _arr->array->begin();
    }
    else
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is not an array.");

    return -1;
}
wo_bool_t wo_arr_remove(wo_value arr, wo_int_t index)
{
    auto _arr = WO_VAL(arr);
    if (_arr->is_nil())
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is 'nil'.");
    else if (_arr->type == wo::value::valuetype::array_type)
    {
        wo::gcbase::gc_write_guard g1(_arr->array);

        if (index >= 0)
        {
            if ((size_t)index < _arr->array->size())
            {
                if (wo::gc::gc_is_marking())
                    wo::gcbase::write_barrier(&(*_arr->array)[(size_t)index]);
                _arr->array->erase(_arr->array->begin() + (size_t)index);

                return WO_TRUE;
            }
        }
    }
    else
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is not an array.");

    return WO_FALSE;
}
void wo_arr_clear(wo_value arr)
{
    auto _arr = WO_VAL(arr);
    if (_arr->is_nil())
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is 'nil'.");
    else if (_arr->type == wo::value::valuetype::array_type)
    {
        wo::gcbase::gc_write_guard g1(_arr->array);
        if (wo::gc::gc_is_marking())
            for (auto& val : *_arr->array)
                wo::gcbase::write_barrier(&val);
        _arr->array->clear();
    }
    else
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is not an array.");
}

wo_bool_t wo_arr_is_empty(wo_value arr)
{
    auto _arr = WO_VAL(arr);
    if (_arr->is_nil())
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is 'nil'.");
    else if (_arr->type == wo::value::valuetype::array_type)
    {
        wo::gcbase::gc_read_guard g1(_arr->array);
        return WO_CBOOL(_arr->array->empty());
    }
    else
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is not an array.");
    return WO_TRUE;
}

wo_bool_t wo_map_find(wo_value map, wo_value index)
{
    auto _map = WO_VAL(map);
    if (_map->is_nil())
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is 'nil'.");
    else if (_map->type == wo::value::valuetype::dict_type)
    {
        wo::gcbase::gc_read_guard g1(_map->dict);
        if (index)
            return WO_CBOOL(_map->dict->find(*WO_VAL(index)) != _map->dict->end());
        return WO_CBOOL(_map->dict->find(wo::value()) != _map->dict->end());
    }
    else
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is not a map.");

    return WO_FALSE;
}

wo_bool_t wo_map_get_or_set_default(wo_value out_val, wo_value map, wo_value index, wo_value default_value)
{
    auto _map = WO_VAL(map);
    if (_map->is_nil())
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is 'nil'.");
    else if (_map->type == wo::value::valuetype::dict_type)
    {
        wo::value* store_val = nullptr;
        wo::gcbase::gc_write_guard g1(_map->dict);

        auto fnd = _map->dict->find(*WO_VAL(index));
        bool found = fnd != _map->dict->end();
        if (found)
            store_val = &fnd->second;

        if (!store_val)
        {
            store_val = &(*_map->dict)[*WO_VAL(index)];
            store_val->set_val(WO_VAL(default_value));
        }

        WO_VAL(out_val)->set_val(store_val);
        return WO_CBOOL(found);
    }
    else
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is not a map.");

    return WO_FALSE;
}

wo_bool_t wo_map_get_or_default(wo_value out_val, wo_value map, wo_value index, wo_value default_value)
{
    auto _map = WO_VAL(map);
    if (_map->is_nil())
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is 'nil'.");
    else if (_map->type == wo::value::valuetype::dict_type)
    {
        wo::value* result = nullptr;
        wo::gcbase::gc_read_guard g1(_map->dict);
        do
        {
            auto fnd = _map->dict->find(*WO_VAL(index));
            if (fnd != _map->dict->end())
                result = &fnd->second;
        } while (false);

        if (!result)
        {
            WO_VAL(out_val)->set_val(WO_VAL(default_value));
            return WO_FALSE;
        }
        WO_VAL(out_val)->set_val(result);
        return WO_TRUE;
    }
    else
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is not a map.");

    return WO_FALSE;
}

wo_bool_t wo_map_try_get(wo_value out_val, wo_value map, wo_value index)
{
    auto _map = WO_VAL(map);
    if (_map->is_nil())
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is 'nil'.");
    else if (_map->type == wo::value::valuetype::dict_type)
    {
        wo::gcbase::gc_read_guard g1(_map->dict);
        auto fnd = _map->dict->find(*WO_VAL(index));
        if (fnd != _map->dict->end())
        {
            WO_VAL(out_val)->set_val(&fnd->second);
            return WO_TRUE;
        }
    }
    else
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is not a map.");

    return WO_FALSE;
}

void wo_map_set(wo_value map, wo_value index, wo_value val)
{
    auto _map = WO_VAL(map);
    if (_map->is_nil())
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is 'nil'.");
    else if (_map->type == wo::value::valuetype::dict_type)
    {
        wo::gcbase::gc_write_guard g1(_map->dict);
        auto* store_val = &(*_map->dict)[*WO_VAL(index)];
        wo::gcbase::write_barrier(store_val);
        store_val->set_val(WO_VAL(val));
    }
    else
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is not a map.");
}

wo_bool_t wo_map_remove(wo_value map, wo_value index)
{
    auto _map = WO_VAL(map);
    if (_map->is_nil())
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is 'nil'.");
    else if (_map->type == wo::value::valuetype::dict_type)
    {
        wo::gcbase::gc_write_guard g1(_map->dict);
        if (wo::gc::gc_is_marking())
        {
            auto fnd = _map->dict->find(*WO_VAL(index));
            if (fnd != _map->dict->end())
            {
                wo::gcbase::write_barrier(&fnd->first);
                wo::gcbase::write_barrier(&fnd->second);
            }
        }
        return WO_CBOOL(0 != _map->dict->erase(*WO_VAL(index)));
    }
    else
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is not a map.");

    return WO_FALSE;
}
void wo_map_clear(wo_value map)
{
    auto _map = WO_VAL(map);
    if (_map->is_nil())
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is 'nil'.");
    else if (_map->type == wo::value::valuetype::dict_type)
    {
        wo::gcbase::gc_write_guard g1(_map->dict);
        if (wo::gc::gc_is_marking())
        {
            for (auto& kvpair : *_map->dict)
            {
                wo::gcbase::write_barrier(&kvpair.first);
                wo::gcbase::write_barrier(&kvpair.second);
            }
        }
        _map->dict->clear();
    }
    else
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is not a map.");
}

wo_bool_t wo_map_is_empty(wo_value map)
{
    auto _map = WO_VAL(map);
    if (_map->is_nil())
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is 'nil'.");
    else if (_map->type == wo::value::valuetype::dict_type)
    {
        wo::gcbase::gc_read_guard g1(_map->dict);
        return WO_CBOOL(_map->dict->empty());
    }
    else
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is not a map.");
    return WO_TRUE;
}

wo_bool_t wo_gchandle_close(wo_value gchandle)
{
    auto* gchandle_ptr = WO_VAL(gchandle)->gchandle;
    wo::gcbase::gc_write_guard g1(gchandle_ptr);
    return WO_CBOOL(gchandle_ptr->close());
}

// DEBUGGEE TOOLS
void wo_attach_default_debuggee()
{
    wo::default_debuggee* dgb = new wo::default_debuggee;
    if (auto* old_debuggee = wo::vmbase::attach_debuggee(dgb))
        delete old_debuggee;
}

wo_bool_t wo_has_attached_debuggee()
{
    if (wo::vmbase::current_debuggee() != nullptr)
        return WO_TRUE;
    return WO_FALSE;
}

void wo_detach_debuggee()
{
    if (auto* old_debuggee = wo::vmbase::attach_debuggee(nullptr))
        delete old_debuggee;
}

void wo_break_immediately()
{
    if (auto* debuggee = dynamic_cast<wo::default_debuggee*>(wo::vmbase::current_debuggee()))
        debuggee->breakdown_immediately();
    else
        wo_fail(WO_FAIL_DEBUGGEE_FAIL, "'wo_break_immediately' can only break the vm attached default debuggee.");
}

void wo_break_specify_immediately(wo_vm vmm)
{
    if (auto* debuggee = dynamic_cast<wo::default_debuggee*>(wo::vmbase::current_debuggee()))
        debuggee->breakdown_at_vm_immediately(WO_VM(vmm));
    else
        wo_fail(WO_FAIL_DEBUGGEE_FAIL, "'wo_break_immediately' can only break the vm attached default debuggee.");
}

wo_integer_t wo_extern_symb(wo_vm vm, wo_string_t fullname)
{
    const auto& extern_table = WO_VM(vm)->env->extern_script_functions;
    auto fnd = extern_table.find(fullname);
    if (fnd != extern_table.end())
        return fnd->second;
    return 0;
}

wo_string_t wo_debug_trace_callstack(wo_vm vm, size_t layer)
{
    std::stringstream sstream;
    WO_VM(vm)->dump_call_stack(layer, true, sstream);

    wo_set_string(CS_VAL(WO_VM(vm)->er), vm, sstream.str().c_str());
    wo_assert(WO_VM(vm)->er->type == wo::value::valuetype::string_type);

    return WO_VM(vm)->er->string->c_str();
}

void* wo_load_lib(const char* libname, const char* path, wo_bool_t panic_when_fail)
{
    void* loaded_lib_res_ptr = nullptr;
    std::lock_guard g1(loaded_named_libs_mx);
    if (path)
    {
        if (void* handle = wo::osapi::loadlib(path))
        {
            loaded_named_libs[libname].push_back(
                loaded_lib_info{ handle, 1 }
            );
            loaded_lib_res_ptr = handle;
        }
    }
    else
    {
        auto fnd = loaded_named_libs.find(libname);
        if (fnd != loaded_named_libs.end())
        {
            wo_assert(fnd->second.empty() == false);
            auto& libinfo = fnd->second.back();
            wo_assert(libinfo.m_use_count > 0);
            ++libinfo.m_use_count;
            loaded_lib_res_ptr = libinfo.m_lib_instance;
        }
    }

    if (loaded_lib_res_ptr == nullptr && panic_when_fail)
        wo_fail(WO_FAIL_BAD_LIB, "Failed to load specify library.");

    return loaded_lib_res_ptr;
}
void* wo_load_func(void* lib, const char* funcname)
{
    wo_assert(lib);
    return (void*)wo::osapi::loadfunc(lib, funcname);
}
void wo_unload_lib(void* lib)
{
    wo_assert(lib);

    std::lock_guard sg1(loaded_named_libs_mx);

    auto fnd = std::find_if(loaded_named_libs.begin(), loaded_named_libs.end(),
        [lib](const auto& idx)
        {
            return std::find_if(idx.second.begin(), idx.second.end(), [lib](const auto& info)
                {
                    return info.m_lib_instance == lib;
                }) != idx.second.end();
        });

    wo_assert(fnd != loaded_named_libs.end());
    auto infofnd = std::find_if(fnd->second.begin(), fnd->second.end(), [lib](const auto& info)
        {
            return info.m_lib_instance == lib;
        });
    wo_assert(infofnd != fnd->second.end());
    wo_assert(infofnd->m_lib_instance == lib);
    if (0 == --infofnd->m_use_count)
    {
        wo::osapi::freelib(infofnd->m_lib_instance);
        fnd->second.erase(infofnd);

        if (fnd->second.empty())
            loaded_named_libs.erase(fnd);
    }
}

wo_integer_t wo_crc64_u8(uint8_t byte, wo_integer_t crc)
{
    return (wo_integer_t)wo::crc_64(byte, (uint64_t)crc);
}
wo_integer_t wo_crc64_str(wo_string_t text)
{
    return (wo_integer_t)wo::crc_64(text, 0);
}
wo_integer_t wo_crc64_file(wo_string_t filepath)
{
    std::ifstream file(filepath, std::ios_base::in | std::ios_base::binary);
    if (!file.is_open())
        // Failed to open file, return 0; ?
        return 0;

    return (wo_integer_t)wo::crc_64(file, 0);
}

wo_vm wo_set_this_thread_vm(wo_vm vm_may_null)
{
    auto* old_one = wo::vmbase::_this_thread_vm;
    wo::vmbase::_this_thread_vm = WO_VM(vm_may_null);
    return CS_VM(old_one);
}

wo_bool_t wo_leave_gcguard(wo_vm vm)
{
    return WO_CBOOL(WO_VM(vm)->interrupt(wo::vmbase::vm_interrupt_type::LEAVE_INTERRUPT));
}
wo_bool_t wo_enter_gcguard(wo_vm vm)
{
    if (WO_VM(vm)->clear_interrupt(wo::vmbase::vm_interrupt_type::LEAVE_INTERRUPT))
    {
        // If in GC, hang up here to make sure safe.
        if ((WO_VM(vm)->vm_interrupt.load() & (
            wo::vmbase::vm_interrupt_type::GC_INTERRUPT
            | wo::vmbase::vm_interrupt_type::GC_HANGUP_INTERRUPT)) != 0)
        {
            if (!WO_VM(vm)->gc_checkpoint())
            {
                if (WO_VM(vm)->clear_interrupt(wo::vmbase::vm_interrupt_type::GC_HANGUP_INTERRUPT))
                    WO_VM(vm)->hangup();
            }
        }
        return WO_TRUE;
    }
    return WO_FALSE;
}

// LSP-API
size_t wo_lsp_get_compile_error_msg_count_from_vm(wo_vm vmm)
{
    return WO_VM(vmm)->compile_info->lex_error_list.size();
}

wo_lsp_error_msg* wo_lsp_get_compile_error_msg_detail_from_vm(wo_vm vmm, size_t index)
{
    auto& err_detail = WO_VM(vmm)->compile_info->lex_error_list[index];

    wo_lsp_error_msg* msg = new wo_lsp_error_msg;

    switch (err_detail.error_level)
    {
    case wo::lexer::errorlevel::error: msg->m_level = wo_lsp_error_level::WO_LSP_ERROR; break;
    case wo::lexer::errorlevel::infom: msg->m_level = wo_lsp_error_level::WO_LSP_INFORMATION; break;
    default:
        wo_error("Unknown error level.");
        break;
    }
    msg->m_begin_location[0] = err_detail.begin_row;
    msg->m_begin_location[1] = err_detail.begin_col;
    msg->m_end_location[0] = err_detail.end_row;
    msg->m_end_location[1] = err_detail.end_col;

    auto* filename = new char[err_detail.filename.size() + 1];
    strcpy(filename, err_detail.filename.data());
    msg->m_file_name = filename;
    msg->m_describe = wo::wstr_to_str_ptr(err_detail.describe);

    return msg;
}

void wo_lsp_free_compile_error_msg(wo_lsp_error_msg* msg)
{
    delete[] msg->m_describe;
    delete[] msg->m_file_name;
    delete msg;
}

wo_bool_t wo_execute(wo_string_t src, wo_execute_callback_ft callback, void* data)
{
    wo_vm _vm = wo_create_vm();

    static std::atomic_size_t vcount = 0;
    std::string vpath = "__execute_script_" + std::to_string(++vcount) + "__";

    wo_value result = nullptr;
    if (wo_load_source(_vm, vpath.c_str(), src))
    {
        wo_jit(_vm);
        result = wo_run(_vm);
        if (result == nullptr)
        {
            std::string err = "Failed to execute: ";
            err += wo_get_runtime_error(_vm);
            wo_execute_fail(_vm, WO_FAIL_EXECUTE_FAIL, err.c_str());
        }
    }
    else
    {
        std::string err = "Failed to compile: \n";
        err += wo_get_compile_error(_vm, _wo_inform_style::WO_NEED_COLOR);
        wo_execute_fail(_vm, WO_FAIL_EXECUTE_FAIL, err.c_str());
    }

    auto is_succ = WO_FALSE;

    if (result != nullptr)
    {
        callback(result, data);
        is_succ = WO_TRUE;
    }
    wo_close_vm(_vm);

    return is_succ;
}

wo_pin_value wo_create_pin_value(wo_value init_value)
{
    return wo::pin::create_pin_value(WO_VAL(init_value));
}
void wo_close_pin_value(wo_pin_value pin_value)
{
    wo::pin::close_pin_value(pin_value);
}
void wo_read_pin_value(wo_value out_value, wo_pin_value pin_value)
{
    wo::pin::read_pin_value(WO_VAL(out_value), pin_value);
}
