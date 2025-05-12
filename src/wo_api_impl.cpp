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

#include <atomic>
#include <iostream>
#include <sstream>
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

struct dylib_table_instance
{
    using fake_table_t = std::unordered_map<std::string, void*>;

    void* m_native_dylib_handle;
    fake_table_t* m_fake_dylib_table;
    dylib_table_instance* m_dependenced_dylib;
    size_t          m_use_count;

    explicit dylib_table_instance(
        const std::string& name,
        const wo_extern_lib_func_t* funcs,
        dylib_table_instance* dependenced_lib_may_null)
        : m_native_dylib_handle(nullptr)
        , m_fake_dylib_table(new fake_table_t())
        , m_dependenced_dylib(dependenced_lib_may_null)
        , m_use_count(1)
    {
        const auto* func_pair = funcs;
        while (func_pair->m_name != nullptr)
        {
            m_fake_dylib_table->insert(
                std::make_pair(func_pair->m_name, func_pair->m_func_addr));

            ++func_pair;
        }
        if (m_dependenced_dylib != nullptr)
        {
            wo_assert(m_dependenced_dylib->m_use_count > 0);
            ++m_dependenced_dylib->m_use_count;
        }
    }

    explicit dylib_table_instance(void* handle_may_null)
        : m_native_dylib_handle(handle_may_null)
        , m_fake_dylib_table(nullptr)
        , m_dependenced_dylib(nullptr)
        , m_use_count(1)
    {
    }

    ~dylib_table_instance()
    {
        if (m_native_dylib_handle != nullptr)
            wo::osapi::freelib(m_native_dylib_handle);

        if (m_fake_dylib_table != nullptr)
            delete m_fake_dylib_table;

        if (m_dependenced_dylib != nullptr)
            wo_unload_lib(m_dependenced_dylib, WO_DYLIB_UNREF);
    }
    dylib_table_instance(const dylib_table_instance&) = delete;
    dylib_table_instance(dylib_table_instance&&) = delete;
    dylib_table_instance& operator = (const dylib_table_instance&) = delete;
    dylib_table_instance& operator = (dylib_table_instance&&) = delete;

    void* load_func(const char* name)
    {
        if (m_native_dylib_handle != nullptr)
            return wo::osapi::loadfunc(m_native_dylib_handle, name);
        else
        {
            wo_assert(m_fake_dylib_table != nullptr);

            auto fnd = m_fake_dylib_table->find(name);
            if (fnd != m_fake_dylib_table->end())
                return fnd->second;

            return nullptr;
        }
    }
};
struct loaded_lib_info
{
    using named_libs_map_t = std::unordered_map<
        std::string, std::unique_ptr<loaded_lib_info>>;

    inline static std::recursive_mutex loaded_named_libs_mx;
    inline static named_libs_map_t loaded_named_libs;

    dylib_table_instance* m_lib_instance;

    static dylib_table_instance* load_lib(
        const char* libname,
        const wchar_t* path,
        const wchar_t* script_path,
        wo_bool_t panic_when_fail)
    {
        std::lock_guard g1(loaded_named_libs_mx);
        dylib_table_instance* loaded_lib_res_ptr = nullptr;

        if (auto fnd = loaded_named_libs.find(libname);
            fnd != loaded_named_libs.end())
        {
            auto* libinfo = fnd->second.get();

            loaded_lib_res_ptr = libinfo->m_lib_instance;
            wo_assert(loaded_lib_res_ptr != nullptr);
            wo_assert(loaded_lib_res_ptr->m_use_count > 0);

            ++loaded_lib_res_ptr->m_use_count;
        }
        else
        {
            if (path != nullptr)
            {
                if (void* handle = wo::osapi::loadlib(path, script_path))
                    loaded_lib_res_ptr = new dylib_table_instance(handle);
            }

            if (loaded_lib_res_ptr != nullptr)
            {
                auto instance_info = std::make_unique<loaded_lib_info>();
                instance_info->m_lib_instance = loaded_lib_res_ptr;
                wo_assert(loaded_lib_res_ptr->m_use_count == 1);

                loaded_named_libs[libname] = std::move(instance_info);
            }
        }

        if (loaded_lib_res_ptr == nullptr && panic_when_fail)
            wo_fail(WO_FAIL_BAD_LIB, "Failed to load specify library.");

        return loaded_lib_res_ptr;
    }

    static dylib_table_instance* create_fake_lib(
        const char* libname,
        const wo_extern_lib_func_t* funcs,
        dylib_table_instance* dependenced_lib_may_null)
    {
        std::lock_guard g1(loaded_named_libs_mx);

        if (loaded_named_libs.find(libname) != loaded_named_libs.end())
            return nullptr;

        auto* instance = new dylib_table_instance(
            libname, funcs, dependenced_lib_may_null);

        auto instance_info = std::make_unique<loaded_lib_info>();
        instance_info->m_lib_instance = instance;
        wo_assert(instance->m_use_count == 1);

        loaded_named_libs[libname] = std::move(instance_info);

        return instance;
    }
    static void unload_lib(dylib_table_instance* lib, wo_dylib_unload_method_t method)
    {
        wo_assert(lib != nullptr);

        std::lock_guard sg1(loaded_named_libs_mx);

        bool bury_dylib = (method & WO_DYLIB_BURY) != 0;

        if ((method & WO_DYLIB_UNREF) != 0
            && 0 == --lib->m_use_count)
        {
            bury_dylib = true;
            delete lib;
        }

        if (bury_dylib)
        {
            // Out of ref, remove it from list;
            for (auto fnd = loaded_named_libs.begin(); fnd != loaded_named_libs.end(); ++fnd)
            {
                auto* instance_info = fnd->second.get();
                if (instance_info->m_lib_instance == lib)
                {
                    loaded_named_libs.erase(fnd);
                    break;
                }
            }
        }
    }
};

wo::vmpool* global_vm_pool = nullptr;

void _default_fail_handler(wo_vm vm, wo_string_t src_file, uint32_t lineno, wo_string_t functionname, uint32_t rterrcode, wo_string_t reason)
{
    auto* cur_thread_vm = std::launder(reinterpret_cast<wo::vmbase*>(vm));
    bool leaved_flag = cur_thread_vm != nullptr
        ? wo_leave_gcguard(reinterpret_cast<wo_vm>(cur_thread_vm))
        : false;

    wo::wo_stderr << ANSI_HIR "Woolang runtime happend a failure: "
        << ANSI_HIY << reason << " (Code: " << std::hex << rterrcode << std::dec << ")" << ANSI_RST << wo::wo_endl;
    wo::wo_stderr << "\tAt source: \t" << src_file << wo::wo_endl;
    wo::wo_stderr << "\tAt line: \t" << lineno << wo::wo_endl;
    wo::wo_stderr << "\tAt function: \t" << functionname << wo::wo_endl;
    wo::wo_stderr << wo::wo_endl;

    wo::wo_stderr << ANSI_HIR "callstack: " ANSI_RST << wo::wo_endl;

    if (cur_thread_vm != nullptr)
        cur_thread_vm->dump_call_stack(32, true, std::cerr);
    else
        wo::wo_stderr << ANSI_HIM "No woolang vm found on this thread." ANSI_RST << wo::wo_endl;

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
        wo::wo_stderr << ANSI_HIY "This failure may cause a crash." ANSI_RST << wo::wo_endl;
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
                        wo::wo_stderr << ANSI_HIR "Current virtual machine will be aborted." ANSI_RST << wo::wo_endl;
                        wo_ret_halt(reinterpret_cast<wo_vm>(cur_thread_vm), reason);
                        breakout = true;
                    }
                    else
                        wo::wo_stderr << ANSI_HIR "No virtual machine running on this thread." ANSI_RST << wo::wo_endl;
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
                        wo::wo_stderr << ANSI_HIR "No virtual machine running on this thread." ANSI_RST << wo::wo_endl;
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
static std::atomic<wo_fail_handler_t> _wo_fail_handler_function = &_default_fail_handler;

wo_fail_handler_t wo_register_fail_handler(wo_fail_handler_t new_handler)
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

wo_real_t _wo_last_ctrl_c_time = -1.f;
wo_size_t _wo_ctrl_c_hit_count = 0;

void _wo_ctrl_c_signal_handler(int)
{
    // CTRL + C
    wo::wo_stderr << ANSI_HIR "CTRL+C" ANSI_RST
        ": Trying to breakdown all virtual-machine by default debuggee immediately." << wo::wo_endl;

    if (!wo_has_attached_debuggee())
        wo_attach_default_debuggee();

    auto _ctrl_c_time = _wo_inside_time_sec();
    if (_ctrl_c_time - _wo_last_ctrl_c_time < 1.0f)
    {
        if (_wo_ctrl_c_hit_count >= 4)
            wo_error("Panic termination.");
        else
        {
            wo::wo_stderr << ANSI_HIY "CTRL+C" ANSI_RST
                ": Continue pressing Ctrl+C `" ANSI_HIG << 4 - _wo_ctrl_c_hit_count << ANSI_RST "` time(s) to trigger a panic termination" << wo::wo_endl;
        }
    }
    else
        _wo_ctrl_c_hit_count = 0;

    _wo_last_ctrl_c_time = _ctrl_c_time;
    ++_wo_ctrl_c_hit_count;

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

            std::stringstream not_closed_vm_call_stacks;

            size_t not_close_vm_count = 0;
            for (auto& alive_vms : wo::vmbase::_alive_vm_list)
            {
                if (alive_vms->virtual_machine_type == wo::vmbase::vm_type::NORMAL)
                {
                    if (0 == not_close_vm_count++)
                        not_closed_vm_call_stacks << "Unclosed VM list:";

                    not_closed_vm_call_stacks << std::endl << "<unclosed " << (void*)alive_vms << ">" << std::endl;
                    alive_vms->dump_call_stack(32, true, not_closed_vm_call_stacks);
                }
                alive_vms->interrupt(wo::vmbase::ABORT_INTERRUPT);
            }

            auto current_time = time(nullptr);
            if (non_close_vm_last_warning_time == 0 || current_time != non_close_vm_last_warning_time)
            {
                non_close_vm_last_warning_time = current_time;
                if (not_close_vm_count != 0 && not_close_vm_count != non_close_vm_last_warning_vm_count)
                {
                    non_close_vm_last_warning_vm_count = not_close_vm_count;
                    wo_warning((not_closed_vm_call_stacks.str()
                        + "\n"
                        + std::to_string(not_close_vm_count)
                        + " vm(s) have not been closed, please check.").c_str());
                }
            }

        } while (false);

        using namespace std;

        wo_gc_immediately(WO_TRUE);
        std::this_thread::sleep_for(10ms);

        if (wo::vmbase::_alive_vm_count_for_gc_vm_destruct == 0)
            break;

    } while (true);

    wo::gc::gc_stop();

    if (do_after_shutdown != nullptr)
        do_after_shutdown(custom_data);

    womem_shutdown();
    wo::vm_debuggee_bridge_base::_free_abandons_in_shutdown();

    wo::rslib_extern_symbols::free_wo_lib();

#ifndef WO_DISABLE_COMPILER
    wo::LangContext::shutdown_lang_processers();
#endif

    do
    {
        std::lock_guard sg1(loaded_lib_info::loaded_named_libs_mx);
        if (loaded_lib_info::loaded_named_libs.empty() == false)
        {
            std::string not_unload_lib_warn = "Some of library(s) loaded by 'wo_load_lib' is not been unload after shutdown:";
            for (auto& [path, _] : loaded_lib_info::loaded_named_libs)
                not_unload_lib_warn += "\n\t\t" + path;
            wo_warning(not_unload_lib_warn.c_str());
        }
    } while (0);
}

void wo_init(int argc, char** argv)
{
    const char* basic_env_local = wo::DEFAULT_LOCALE_NAME;
    bool enable_std_package = true;
    bool enable_ctrl_c_to_debug = true;
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
            else if ("enable-gc-thread-count" == current_arg)
                wo::config::GC_WORKER_THREAD_COUNT = (size_t)atoi(argv[++command_idx]);
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
            else if ("enable-runtime-checking-integer-division" == current_arg)
                wo::config::ENABLE_RUNTIME_CHECKING_INTEGER_DIVISION = (bool)atoi(argv[++command_idx]);
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

    if (wo::config::GC_WORKER_THREAD_COUNT == 0)
        wo::config::GC_WORKER_THREAD_COUNT = 1; // 1 GC-thread at least.

    wo::gc::gc_start();

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

#ifndef WO_DISABLE_COMPILER
    wo_assure(wo::get_wo_grammar()); // Create grammar when init.
    wo::LangContext::init_lang_processers();
#endif
}

#define WO_VAL(v) (std::launder(reinterpret_cast<wo::value*>(v)))
#define WO_VM(v) (std::launder(reinterpret_cast<wo::vmbase*>(v)))
#define CS_VAL(v) (reinterpret_cast<wo_value>(v))
#define CS_VM(v) (reinterpret_cast<wo_vm>(v))
#define WO_API_STATE_OF_VM(v) (\
    wo_assert(((v->bp + 1)->type & (~1)) == wo::value::valuetype::callstack \
        || ((v->bp + 1)->type & (~1)) == wo::value::valuetype::nativecallstack), \
    ((v->bp + 1)->type & wo::value::valuetype::stack_externed_flag) == 0 ? WO_API_NORMAL : WO_API_RESYNC)

struct _wo_reserved_stack_args_update_guard
{
    wo::vmbase* m_vm;
    wo_value* m_rs;
    wo_value* m_args;

    wo::value* m_origin_stack_begin;
    size_t m_rs_offset;
    size_t m_args_offset;

    _wo_reserved_stack_args_update_guard(const _wo_in_thread_vm_guard&) = delete;
    _wo_reserved_stack_args_update_guard(_wo_in_thread_vm_guard&&) = delete;
    _wo_reserved_stack_args_update_guard& operator = (const _wo_in_thread_vm_guard&) = delete;
    _wo_reserved_stack_args_update_guard& operator = (_wo_in_thread_vm_guard&&) = delete;

    _wo_reserved_stack_args_update_guard(
        wo_vm _vm,
        wo_value* _rs,
        wo_value* _args)
        : m_vm(WO_VM(_vm))
        , m_rs(_rs)
        , m_args(_args)
    {
        m_origin_stack_begin = m_vm->stack_mem_begin;
        if (m_rs != nullptr)
        {
            wo_assert(WO_VAL(*m_rs) > m_vm->_self_stack_mem_buf
                && WO_VAL(*m_rs) <= m_vm->stack_mem_begin);

            m_rs_offset = m_origin_stack_begin - WO_VAL(m_rs);
        }

        if (m_args != nullptr)
        {
            wo_assert(WO_VAL(*m_args) > m_vm->_self_stack_mem_buf
                && WO_VAL(*m_args) <= m_vm->stack_mem_begin);

            m_args_offset = m_origin_stack_begin - WO_VAL(m_args);
        }
    }
    ~_wo_reserved_stack_args_update_guard()
    {
        if (m_origin_stack_begin != m_vm->stack_mem_begin)
        {
            // Need update.
            if (m_rs != nullptr)
                *m_rs = CS_VAL(m_vm->stack_mem_begin - m_rs_offset);

            if (m_args != nullptr)
                *m_args = CS_VAL(m_vm->stack_mem_begin - m_args_offset);

            if (m_vm->bp != m_vm->stack_mem_begin)
            {
                auto* current_call_base = m_vm->bp + 1;

                // NOTE: If bp + 1 is not callstack:
                //  1) It has been marked `stack_externed_flag`
                //  2) The VM has already returned from last function call.
                if (current_call_base->type == wo::value::valuetype::callstack
                    || current_call_base->type == wo::value::valuetype::nativecallstack)
                {
                    current_call_base->type = (wo::value::valuetype)(
                        current_call_base->type | wo::value::valuetype::stack_externed_flag);
                }
            }
        }
    }
};

wo_string_t wo_locale_name(void)
{
    thread_local static std::string buf;
    buf = wo::get_locale().name();
    return buf.c_str();
}
wo_string_t wo_exe_path()
{
    thread_local static std::string buf;
    buf = wo::wstr_to_str(wo::exe_path());
    return buf.c_str();
}
void wo_set_exe_path(wo_string_t path)
{
    wo::set_exe_path(wo::str_to_wstr(path));
}
wo_string_t wo_work_path()
{
    thread_local static std::string buf;
    buf = wo::wstr_to_str(wo::work_path());
    return buf.c_str();
}

wo_bool_t wo_set_work_path(wo_string_t path)
{
    return WO_CBOOL(wo::set_work_path(wo::str_to_wstr(path)));
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

wo_type_t wo_valuetype(wo_value value)
{
    auto* _rsvalue = WO_VAL(value);

    return (wo_type_t)_rsvalue->type;
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
    return _rsvalue->type == wo::value::valuetype::handle_type
        ? (wo_ptr_t)_rsvalue->handle
        : (wo_ptr_t)wo_safety_pointer(_rsvalue->gchandle);
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

const void* wo_buffer(wo_value value, wo_size_t* bytelen)
{
    auto* _rsvalue = WO_VAL(value);
    if (_rsvalue->type != wo::value::valuetype::string_type)
    {
        wo_fail(WO_FAIL_TYPE_FAIL, "This value is not a string.");
        *bytelen = 0;
        return "<not string value>";
    }
    *bytelen = _rsvalue->string->size();
    return _rsvalue->string->c_str();
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
void _wo_set_string(wo_value value, wo_vm vm, wo_string_t val)
{
    auto* _rsvalue = WO_VAL(value);
    _rsvalue->set_string(val);
}
void wo_set_string(wo_value value, wo_vm vm, wo_string_t val)
{
    _wo_enter_gc_guard g(vm);
    _wo_set_string(value, vm, val);
}
void _wo_set_string_vfmt(wo_value value, wo_vm vm, wo_string_t fmt, va_list v1)
{
    va_list v2;
    va_copy(v2, v1);
    std::vector<char> buf(1 + vsnprintf(nullptr, 0, fmt, v1));
    va_end(v1);
    std::vsnprintf(buf.data(), buf.size(), fmt, v2);
    _wo_set_string(value, vm, buf.data());
}
void wo_set_string_fmt(wo_value value, wo_vm vm, wo_string_t fmt, ...)
{
    va_list v1;
    va_start(v1, fmt);
    _wo_set_string_vfmt(value, vm, fmt, v1);
    va_end(v1);
}
void wo_set_buffer(wo_value value, wo_vm vm, const void* val, wo_size_t len)
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
void wo_set_gchandle(
    wo_value value,
    wo_vm vm,
    wo_ptr_t resource_ptr,
    wo_value holding_val,
    wo_gchandle_close_func_t destruct_func)
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

    wo::gcbase* holding_gcbase = nullptr;
    if (holding_val)
    {
        wo::value* holding_value = WO_VAL(holding_val);

        if (holding_value->type >= wo::value::valuetype::need_gc_flag)
            holding_gcbase = holding_value->gcunit;
    }
    handle_ptr->set_custom_mark_unit(holding_gcbase);
}

void wo_set_gcstruct(
    wo_value value,
    wo_vm vm,
    wo_ptr_t resource_ptr,
    wo_gcstruct_mark_func_t mark_func,
    wo_gchandle_close_func_t destruct_func)
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
    handle_ptr->set_custom_mark_callback(mark_func);
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

void wo_set_map(wo_value value, wo_vm vm, wo_size_t reserved)
{
    auto _rsvalue = WO_VAL(value);

    _wo_enter_gc_guard g(vm);
    _rsvalue->set_gcunit<wo::value::valuetype::dict_type>(
        wo::dict_t::gc_new<wo::gcbase::gctype::young>(reserved));
}

void wo_set_union(wo_value value, wo_vm vm, wo_integer_t id, wo_value value_may_null)
{
    auto* target_val = WO_VAL(value);

    wo::struct_t* structptr;
    {
        _wo_enter_gc_guard g(vm);
        structptr = wo::struct_t::gc_new<wo::gcbase::gctype::young>(2);
        target_val->set_gcunit<wo::value::valuetype::struct_type>(structptr);
    }

    structptr->m_values[0].set_integer(id);

    wo_assert(
        structptr->m_values[1].type == wo::value::valuetype::invalid &&
        structptr->m_values[1].handle == 0);

    if (value_may_null != nullptr)
        structptr->m_values[1].set_val(WO_VAL(value_may_null));
    else
        structptr->m_values[1].set_nil();
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
    // NOTE: _wo_cast_array is in GC-GUARD.
    wo::array_t* rsarr = wo::array_t::gc_new<wo::gcbase::gctype::young>(0);;
    wo::gcbase::gc_modify_write_guard g1(rsarr);

    // Store array into instance to make sure it can be marked if GC launched by `out of memory`.
    value->set_gcunit<wo::value::valuetype::array_type>(rsarr);

    while (true)
    {
        if (lex->peek()->m_lex_type == wo::lex_type::l_index_end)
        {
            lex->move_forward();
            break;
        }

        if (!_wo_cast_value(vm, &rsarr->emplace_back(), lex, wo::value::valuetype::invalid)) // val!
            return WO_FALSE;

        if (lex->peek()->m_lex_type == wo::lex_type::l_comma)
            lex->move_forward();
    }
    return WO_TRUE;
}
wo_bool_t _wo_cast_map(wo_vm vm, wo::value* value, wo::lexer* lex)
{
    // NOTE: _wo_cast_map is in GC-GUARD.
    wo::dict_t* rsmap = wo::dict_t::gc_new<wo::gcbase::gctype::young>(0);
    wo::gcbase::gc_modify_write_guard g1(rsmap);

    // Store array into instance to make sure it can be marked if GC launched by `out of memory`.
    value->set_gcunit<wo::value::valuetype::dict_type>(rsmap);

    wo::value* tempory_key_value_storage = nullptr;

    while (true)
    {
        if (lex->peek()->m_lex_type == wo::lex_type::l_right_curly_braces)
        {
            // end
            lex->move_forward();
            break;
        }

        // We need to make sure the map instance can be marked if gc work
        // launched by `out of memory`.
        if (tempory_key_value_storage == nullptr)
            tempory_key_value_storage = &(*rsmap)[wo::value::TAKEPLACE];

        if (!_wo_cast_value(vm, tempory_key_value_storage, lex, wo::value::valuetype::invalid)) // key!
            return WO_FALSE;

        auto& val_place = (*rsmap)[*tempory_key_value_storage];

        auto lex_type = lex->peek()->m_lex_type;
        lex->move_forward();

        if (lex_type != wo::lex_type::l_typecast)
            return WO_FALSE;

        if (!_wo_cast_value(vm, &val_place, lex, wo::value::valuetype::invalid)) // value!
            return WO_FALSE;

        if (lex->peek()->m_lex_type == wo::lex_type::l_comma)
            lex->move_forward();
    }

    if (tempory_key_value_storage != nullptr)
        rsmap->erase(wo::value::TAKEPLACE);

    return WO_TRUE;
}
wo_bool_t _wo_cast_value(wo_vm vm, wo::value* value, wo::lexer* lex, wo::value::valuetype except_type)
{
    auto* token = lex->peek();

    if (token->m_lex_type == wo::lex_type::l_left_curly_braces) // is map
    {
        lex->move_forward();
        if (!_wo_cast_map(vm, value, lex))
            return WO_FALSE;
    }
    else if (token->m_lex_type == wo::lex_type::l_index_begin) // is array
    {
        lex->move_forward();
        if (!_wo_cast_array(vm, value, lex))
            return WO_FALSE;
    }
    else if (token->m_lex_type == wo::lex_type::l_literal_string) // is string   
    {
        value->set_string(wo::wstr_to_str(token->m_token_text));
        lex->move_forward();
    }
    else if (token->m_lex_type == wo::lex_type::l_add
        || token->m_lex_type == wo::lex_type::l_sub
        || token->m_lex_type == wo::lex_type::l_literal_integer
        || token->m_lex_type == wo::lex_type::l_literal_real) // is integer
    {
        bool positive = true;
        if (token->m_lex_type == wo::lex_type::l_sub
            || token->m_lex_type == wo::lex_type::l_add)
        {
            if (token->m_lex_type == wo::lex_type::l_sub)
                positive = false;

            lex->move_forward();

            token = lex->peek();
            if (token->m_lex_type != wo::lex_type::l_literal_integer
                && token->m_lex_type != wo::lex_type::l_literal_real)
                // wo_fail(WO_FAIL_TYPE_FAIL, "Unknown token while parsing.");
                return WO_FALSE;
        }

        if (token->m_lex_type == wo::lex_type::l_literal_integer) // is real
            value->set_integer(positive
                ? std::stoll(wo::wstr_to_str(token->m_token_text).c_str())
                : -std::stoll(wo::wstr_to_str(token->m_token_text).c_str()));
        else if (token->m_lex_type == wo::lex_type::l_literal_real) // is real
            value->set_real(positive
                ? std::stod(wo::wstr_to_str(token->m_token_text).c_str())
                : -std::stod(wo::wstr_to_str(token->m_token_text).c_str()));

        lex->move_forward();
    }
    else if (token->m_lex_type == wo::lex_type::l_nil) // is nil
    {
        value->set_nil();
        lex->move_forward();
    }
    else if (token->m_token_text == L"true")
    {
        value->set_bool(true);// true
        lex->move_forward();
    }
    else if (token->m_token_text == L"false")
    {
        value->set_bool(false);// false
        lex->move_forward();
    }
    else if (token->m_token_text == L"null")
    {
        value->set_nil();// null
        lex->move_forward();
    }
    else
        //wo_fail(WO_FAIL_TYPE_FAIL, "Unknown token while parsing.");
        return WO_FALSE;

    if (except_type != wo::value::valuetype::invalid && except_type != value->type)
        // wo_fail(WO_FAIL_TYPE_FAIL, "Unexcept value type after parsing.");
        return WO_FALSE;
    return WO_TRUE;

}
wo_bool_t wo_deserialize(wo_vm vm, wo_value value, wo_string_t str, wo_type_t except_type)
{
    _wo_enter_gc_guard g(vm);

    std::wstring strbuffer = wo::str_to_wstr(str);

    // NOTE: File name must be nullptr here to make sure macro not work.
    wo::lexer lex(std::nullopt, std::nullopt, std::make_unique<std::wistringstream>(strbuffer));
    return _wo_cast_value(vm, WO_VAL(value), &lex, (wo::value::valuetype)except_type);
}

enum cast_string_mode
{
    FORMAT,
    FIT,
    SERIALIZE,
};

bool _wo_cast_string(
    const wo::value* value,
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

        std::set<const wo::value*, wo::value_ptr_compare>
            _sorted_keys;

        for (auto& [key, _] : *map)
            wo_assure(_sorted_keys.insert(&key).second);

        for (auto* sorted_key : _sorted_keys)
        {
            if (!first_kv_pair)
                *out_str += _fit_layout ? "," : ",\n";
            first_kv_pair = false;

            for (int i = 0; !_fit_layout && i <= depth; i++)
                *out_str += "    ";
            if (!_wo_cast_string(sorted_key, out_str, mode, traveled_gcunit, depth + 1))
                return false;
            *out_str += _fit_layout ? ":" : " : ";
            if (!_wo_cast_string(&map->at(*sorted_key), out_str, mode, traveled_gcunit, depth + 1))
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

wo_string_t wo_type_name(wo_type_t type)
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
wo_result_t wo_ret_void(wo_vm vm)
{
    wo::vmbase* vmbase = WO_VM(vm);
    return WO_API_STATE_OF_VM(vmbase);
}
wo_result_t wo_ret_bool(wo_vm vm, wo_bool_t result)
{
    wo::vmbase* vmbase = WO_VM(vm);
    WO_VM(vm)->cr->set_bool(result != WO_FALSE);
    return WO_API_STATE_OF_VM(vmbase);
}
wo_result_t wo_ret_int(wo_vm vm, wo_integer_t result)
{
    wo::vmbase* vmbase = WO_VM(vm);
    WO_VM(vm)->cr->set_integer(result);
    return WO_API_STATE_OF_VM(vmbase);
}
wo_result_t wo_ret_char(wo_vm vm, wo_char_t result)
{
    return wo_ret_int(vm, (wo_integer_t)result);
}
wo_result_t wo_ret_real(wo_vm vm, wo_real_t result)
{
    wo::vmbase* vmbase = WO_VM(vm);
    WO_VM(vm)->cr->set_real(result);
    return WO_API_STATE_OF_VM(vmbase);
}
wo_result_t wo_ret_float(wo_vm vm, float result)
{
    wo::vmbase* vmbase = WO_VM(vm);
    WO_VM(vm)->cr->set_real((wo_real_t)result);
    return WO_API_STATE_OF_VM(vmbase);
}
wo_result_t wo_ret_handle(wo_vm vm, wo_handle_t result)
{
    wo::vmbase* vmbase = WO_VM(vm);
    WO_VM(vm)->cr->set_handle(result);
    return WO_API_STATE_OF_VM(vmbase);
}
wo_result_t wo_ret_pointer(wo_vm vm, wo_ptr_t result)
{
    wo::vmbase* vmbase = WO_VM(vm);

    if (result)
    {
        WO_VM(vm)->cr->set_handle((wo_handle_t)result);
        return WO_API_STATE_OF_VM(vmbase);
    }
    return wo_ret_panic(vm, "Cannot return a nullptr");
}
wo_result_t wo_ret_string(wo_vm vm, wo_string_t result)
{
    _wo_enter_gc_guard g(vm);
    wo::vmbase* vmbase = WO_VM(vm);

    WO_VM(vm)->cr->set_string(result);
    return WO_API_STATE_OF_VM(vmbase);
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

wo_result_t wo_ret_buffer(wo_vm vm, const void* result, wo_size_t len)
{
    _wo_enter_gc_guard g(vm);
    wo::vmbase* vmbase = WO_VM(vm);

    WO_VM(vm)->cr->set_buffer(result, len);
    return WO_API_STATE_OF_VM(vmbase);
}
wo_result_t wo_ret_gchandle(wo_vm vm, wo_ptr_t resource_ptr, wo_value holding_val, wo_gchandle_close_func_t destruct_func)
{
    wo::vmbase* vmbase = WO_VM(vm);
    wo_set_gchandle(CS_VAL(WO_VM(vm)->cr), vm, resource_ptr, holding_val, destruct_func);
    return WO_API_STATE_OF_VM(vmbase);
}
wo_result_t wo_ret_gcstruct(wo_vm vm, wo_ptr_t resource_ptr, wo_gcstruct_mark_func_t mark_func, wo_gchandle_close_func_t destruct_func)
{
    wo::vmbase* vmbase = WO_VM(vm);
    wo_set_gcstruct(CS_VAL(WO_VM(vm)->cr), vm, resource_ptr, mark_func, destruct_func);
    return WO_API_STATE_OF_VM(vmbase);
}
wo_result_t wo_ret_val(wo_vm vm, wo_value result)
{
    wo_assert(result);
    wo::vmbase* vmbase = WO_VM(vm);

    WO_VM(vm)->cr->set_val(WO_VAL(result));
    return WO_API_STATE_OF_VM(vmbase);
}

wo_result_t wo_ret_dup(wo_vm vm, wo_value result)
{
    wo::vmbase* vmbase = WO_VM(vm);

    auto* val = WO_VAL(result);

    _wo_enter_gc_guard g(vm);
    WO_VM(vm)->cr->set_dup(val);

    return WO_API_STATE_OF_VM(vmbase);
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

    wo::vmbase* vmbase = WO_VM(vm);
    {
        _wo_enter_gc_guard g(vm);
        vmbase->er->set_string(buf.data());
    }
    vmbase->interrupt(wo::vmbase::vm_interrupt_type::ABORT_INTERRUPT);
    wo::wo_stderr << ANSI_HIR "Halt happend: " ANSI_RST << wo_cast_string((wo_value)vmbase->er) << wo::wo_endl;
    vmbase->dump_call_stack(32, true, std::cerr);
    return wo_result_t::WO_API_RESYNC;
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

    wo::vmbase* vmbase = WO_VM(vm);
    {
        _wo_enter_gc_guard g(vm);
        vmbase->er->set_string(buf.data());
    }
    wo_fail(WO_FAIL_USER_PANIC, vmbase->er->string->c_str());
    return wo_result_t::WO_API_RESYNC;
}

void wo_set_option_void(wo_value val, wo_vm vm)
{
    auto* target_val = WO_VAL(val);

    wo::struct_t* structptr;
    {
        _wo_enter_gc_guard g(vm);
        structptr = wo::struct_t::gc_new<wo::gcbase::gctype::young>(2);
        target_val->set_gcunit<wo::value::valuetype::struct_type>(structptr);
    }

    structptr->m_values[0].set_integer(0);
    structptr->m_values[1].set_nil();
}
void wo_set_option_bool(wo_value val, wo_vm vm, wo_bool_t result)
{
    auto* target_val = WO_VAL(val);

    wo::struct_t* structptr;
    {
        _wo_enter_gc_guard g(vm);
        structptr = wo::struct_t::gc_new<wo::gcbase::gctype::young>(2);
        target_val->set_gcunit<wo::value::valuetype::struct_type>(structptr);
    }

    structptr->m_values[0].set_integer(0);
    structptr->m_values[1].set_bool(result != WO_FALSE);

}
void wo_set_option_char(wo_value val, wo_vm vm, wo_char_t result)
{
    return wo_set_option_int(val, vm, (wo_integer_t)result);
}
void wo_set_option_int(wo_value val, wo_vm vm, wo_integer_t result)
{
    auto* target_val = WO_VAL(val);

    wo::struct_t* structptr;
    {
        _wo_enter_gc_guard g(vm);
        structptr = wo::struct_t::gc_new<wo::gcbase::gctype::young>(2);
        target_val->set_gcunit<wo::value::valuetype::struct_type>(structptr);
    }

    structptr->m_values[0].set_integer(0);
    structptr->m_values[1].set_integer(result);


}
void wo_set_option_real(wo_value val, wo_vm vm, wo_real_t result)
{
    auto* target_val = WO_VAL(val);

    wo::struct_t* structptr;
    {
        _wo_enter_gc_guard g(vm);
        structptr = wo::struct_t::gc_new<wo::gcbase::gctype::young>(2);
        target_val->set_gcunit<wo::value::valuetype::struct_type>(structptr);
    }

    structptr->m_values[0].set_integer(0);
    structptr->m_values[1].set_real(result);


}
void wo_set_option_float(wo_value val, wo_vm vm, float result)
{
    auto* target_val = WO_VAL(val);

    wo::struct_t* structptr;
    {
        _wo_enter_gc_guard g(vm);
        structptr = wo::struct_t::gc_new<wo::gcbase::gctype::young>(2);
        target_val->set_gcunit<wo::value::valuetype::struct_type>(structptr);
    }

    structptr->m_values[0].set_integer(0);
    structptr->m_values[1].set_real((wo_real_t)result);
}
void wo_set_option_handle(wo_value val, wo_vm vm, wo_handle_t result)
{
    auto* target_val = WO_VAL(val);

    wo::struct_t* structptr;
    {
        _wo_enter_gc_guard g(vm);
        structptr = wo::struct_t::gc_new<wo::gcbase::gctype::young>(2);
        target_val->set_gcunit<wo::value::valuetype::struct_type>(structptr);
    }

    structptr->m_values[0].set_integer(0);
    structptr->m_values[1].set_handle(result);


}
void wo_set_option_string(wo_value val, wo_vm vm, wo_string_t result)
{
    auto* target_val = WO_VAL(val);

    wo::struct_t* structptr;
    {
        _wo_enter_gc_guard g(vm);
        structptr = wo::struct_t::gc_new<wo::gcbase::gctype::young>(2);
        target_val->set_gcunit<wo::value::valuetype::struct_type>(structptr);
        structptr->m_values[1].set_string(result);
    }
    structptr->m_values[0].set_integer(0);
}
void _wo_set_option_string_vfmt(wo_value val, wo_vm vm, wo_string_t fmt, va_list v1)
{
    auto* target_val = WO_VAL(val);

    wo::struct_t* structptr;
    {
        _wo_enter_gc_guard g(vm);
        structptr = wo::struct_t::gc_new<wo::gcbase::gctype::young>(2);
        target_val->set_gcunit<wo::value::valuetype::struct_type>(structptr);

        _wo_set_string_vfmt(CS_VAL(&structptr->m_values[1]), vm, fmt, v1);
    }
    structptr->m_values[0].set_integer(0);
}
void wo_set_option_string_fmt(wo_value val, wo_vm vm, wo_string_t fmt, ...)
{
    va_list v1;
    va_start(v1, fmt);
    _wo_set_option_string_vfmt(val, vm, fmt, v1);
    va_end(v1);
}
void wo_set_option_buffer(wo_value val, wo_vm vm, const void* result, wo_size_t len)
{
    auto* target_val = WO_VAL(val);

    wo::struct_t* structptr;
    {
        _wo_enter_gc_guard g(vm);
        structptr = wo::struct_t::gc_new<wo::gcbase::gctype::young>(2);
        target_val->set_gcunit<wo::value::valuetype::struct_type>(structptr);

        structptr->m_values[1].set_buffer(result, len);
    }
    structptr->m_values[0].set_integer(0);
}
void wo_set_option_pointer(wo_value val, wo_vm vm, wo_ptr_t result)
{
    auto* target_val = WO_VAL(val);

    wo::struct_t* structptr;
    {
        _wo_enter_gc_guard g(vm);
        structptr = wo::struct_t::gc_new<wo::gcbase::gctype::young>(2);
        target_val->set_gcunit<wo::value::valuetype::struct_type>(structptr);
    }

    structptr->m_values[0].set_integer(0);
    structptr->m_values[1].set_handle((wo_handle_t)result);
    if (nullptr == result)
        wo_fail(WO_FAIL_ACCESS_NIL, "Cannot return nullptr");
}
void wo_set_option_ptr_may_null(wo_value val, wo_vm vm, wo_ptr_t result)
{
    auto* target_val = WO_VAL(val);

    wo::struct_t* structptr;
    {
        _wo_enter_gc_guard g(vm);
        structptr = wo::struct_t::gc_new<wo::gcbase::gctype::young>(2);
        target_val->set_gcunit<wo::value::valuetype::struct_type>(structptr);
    }

    if (result)
    {
        structptr->m_values[0].set_integer(0);
        structptr->m_values[1].set_handle((wo_handle_t)result);
    }
    else
        structptr->m_values[0].set_integer(1);
}
void wo_set_option_val(wo_value val, wo_vm vm, wo_value result)
{
    auto* target_val = WO_VAL(val);

    wo::struct_t* structptr;
    {
        _wo_enter_gc_guard g(vm);
        structptr = wo::struct_t::gc_new<wo::gcbase::gctype::young>(2);
        target_val->set_gcunit<wo::value::valuetype::struct_type>(structptr);
    }

    structptr->m_values[0].set_integer(0);
    structptr->m_values[1].set_val(WO_VAL(result));
}
void wo_set_option_gchandle(wo_value val, wo_vm vm, wo_ptr_t resource_ptr, wo_value holding_val, wo_gchandle_close_func_t destruct_func)
{
    auto* target_val = WO_VAL(val);

    wo::struct_t* structptr;
    {
        _wo_enter_gc_guard g(vm);
        structptr = wo::struct_t::gc_new<wo::gcbase::gctype::young>(2);
        target_val->set_gcunit<wo::value::valuetype::struct_type>(structptr);
    }

    structptr->m_values[0].set_integer(0);
    wo_set_gchandle(CS_VAL(&structptr->m_values[1]), vm, resource_ptr, holding_val, destruct_func);
}
void wo_set_option_gcstruct(wo_value val, wo_vm vm, wo_ptr_t resource_ptr, wo_gcstruct_mark_func_t mark_func, wo_gchandle_close_func_t destruct_func)
{
    auto* target_val = WO_VAL(val);

    wo::struct_t* structptr;
    {
        _wo_enter_gc_guard g(vm);
        structptr = wo::struct_t::gc_new<wo::gcbase::gctype::young>(2);
        target_val->set_gcunit<wo::value::valuetype::struct_type>(structptr);
    }

    structptr->m_values[0].set_integer(0);
    wo_set_gcstruct(CS_VAL(&structptr->m_values[1]), vm, resource_ptr, mark_func, destruct_func);
}
void wo_set_option_none(wo_value val, wo_vm vm)
{
    auto* target_val = WO_VAL(val);

    wo::struct_t* structptr;
    {
        _wo_enter_gc_guard g(vm);
        structptr = wo::struct_t::gc_new<wo::gcbase::gctype::young>(2);
        target_val->set_gcunit<wo::value::valuetype::struct_type>(structptr);
    }
    structptr->m_values[0].set_integer(1);
}

void wo_set_err_void(wo_value val, wo_vm vm)
{
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
void wo_set_err_char(wo_value val, wo_vm vm, wo_char_t result)
{
    return wo_set_err_int(val, vm, (wo_integer_t)result);
}
void wo_set_err_bool(wo_value val, wo_vm vm, wo_bool_t result)
{
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
void wo_set_err_int(wo_value val, wo_vm vm, wo_integer_t result)
{
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
void wo_set_err_real(wo_value val, wo_vm vm, wo_real_t result)
{
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
void wo_set_err_float(wo_value val, wo_vm vm, float result)
{
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
void wo_set_err_handle(wo_value val, wo_vm vm, wo_handle_t result)
{
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
void wo_set_err_string(wo_value val, wo_vm vm, wo_string_t result)
{
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
void _wo_set_err_string_vfmt(wo_value val, wo_vm vm, wo_string_t fmt, va_list v1)
{
    auto* target_val = WO_VAL(val);

    wo::struct_t* structptr;
    {
        _wo_enter_gc_guard g(vm);
        structptr = wo::struct_t::gc_new<wo::gcbase::gctype::young>(2);
        target_val->set_gcunit<wo::value::valuetype::struct_type>(structptr);

        _wo_set_string_vfmt(CS_VAL(&structptr->m_values[1]), vm, fmt, v1);
    }

    structptr->m_values[0].set_integer(1);
}
void wo_set_err_string_fmt(wo_value val, wo_vm vm, wo_string_t fmt, ...)
{
    va_list v1;
    va_start(v1, fmt);
    _wo_set_err_string_vfmt(val, vm, fmt, v1);
    va_end(v1);
}
void wo_set_err_buffer(wo_value val, wo_vm vm, const void* result, wo_size_t len)
{
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
void wo_set_err_pointer(wo_value val, wo_vm vm, wo_ptr_t result)
{
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
void wo_set_err_val(wo_value val, wo_vm vm, wo_value result)
{
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
void wo_set_err_gchandle(wo_value val, wo_vm vm, wo_ptr_t resource_ptr, wo_value holding_val, wo_gchandle_close_func_t destruct_func)
{
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
void wo_set_err_gcstruct(wo_value val, wo_vm vm, wo_ptr_t resource_ptr, wo_gcstruct_mark_func_t mark_func, wo_gchandle_close_func_t destruct_func)
{
    auto* target_val = WO_VAL(val);

    wo::struct_t* structptr;
    {
        _wo_enter_gc_guard g(vm);
        structptr = wo::struct_t::gc_new<wo::gcbase::gctype::young>(2);
        target_val->set_gcunit<wo::value::valuetype::struct_type>(structptr);
    }

    structptr->m_values[0].set_integer(1);
    wo_set_gcstruct(CS_VAL(&structptr->m_values[1]), vm, resource_ptr, mark_func, destruct_func);
}
wo_result_t wo_ret_union(wo_vm vm, wo_integer_t id, wo_value value_may_null)
{
    wo::vmbase* vmbase = WO_VM(vm);
    wo_set_union(CS_VAL(vmbase->cr), vm, id, value_may_null);
    return WO_API_STATE_OF_VM(vmbase);
}
wo_result_t wo_ret_option_void(wo_vm vm)
{
    wo::vmbase* vmbase = WO_VM(vm);
    wo_set_option_void(CS_VAL(vmbase->cr), vm);
    return WO_API_STATE_OF_VM(vmbase);
}
wo_result_t  wo_ret_option_bool(wo_vm vm, wo_bool_t result)
{
    wo::vmbase* vmbase = WO_VM(vm);
    wo_set_option_bool(CS_VAL(vmbase->cr), vm, result);
    return WO_API_STATE_OF_VM(vmbase);
}
wo_result_t wo_ret_option_char(wo_vm vm, wo_char_t result)
{
    wo::vmbase* vmbase = WO_VM(vm);
    wo_set_option_char(CS_VAL(vmbase->cr), vm, result);
    return WO_API_STATE_OF_VM(vmbase);
}
wo_result_t wo_ret_option_int(wo_vm vm, wo_integer_t result)
{
    wo::vmbase* vmbase = WO_VM(vm);
    wo_set_option_int(CS_VAL(vmbase->cr), vm, result);
    return WO_API_STATE_OF_VM(vmbase);
}
wo_result_t wo_ret_option_real(wo_vm vm, wo_real_t result)
{
    wo::vmbase* vmbase = WO_VM(vm);
    wo_set_option_real(CS_VAL(vmbase->cr), vm, result);
    return WO_API_STATE_OF_VM(vmbase);
}
wo_result_t wo_ret_option_float(wo_vm vm, float result)
{
    wo::vmbase* vmbase = WO_VM(vm);
    wo_set_option_float(CS_VAL(vmbase->cr), vm, result);
    return WO_API_STATE_OF_VM(vmbase);
}
wo_result_t  wo_ret_option_handle(wo_vm vm, wo_handle_t result)
{
    wo::vmbase* vmbase = WO_VM(vm);
    wo_set_option_handle(CS_VAL(vmbase->cr), vm, result);
    return WO_API_STATE_OF_VM(vmbase);
}
wo_result_t  wo_ret_option_string(wo_vm vm, wo_string_t result)
{
    wo::vmbase* vmbase = WO_VM(vm);
    wo_set_option_string(CS_VAL(vmbase->cr), vm, result);
    return WO_API_STATE_OF_VM(vmbase);
}
wo_result_t  wo_ret_option_string_fmt(wo_vm vm, wo_string_t fmt, ...)
{
    wo::vmbase* vmbase = WO_VM(vm);
    va_list v1;
    va_start(v1, fmt);
    _wo_set_option_string_vfmt(CS_VAL(vmbase->cr), vm, fmt, v1);
    va_end(v1);
    return WO_API_STATE_OF_VM(vmbase);
}
wo_result_t  wo_ret_option_buffer(wo_vm vm, const void* result, wo_size_t len)
{
    wo::vmbase* vmbase = WO_VM(vm);
    wo_set_option_buffer(CS_VAL(vmbase->cr), vm, result, len);
    return WO_API_STATE_OF_VM(vmbase);
}
wo_result_t wo_ret_option_pointer(wo_vm vm, wo_ptr_t result)
{
    wo::vmbase* vmbase = WO_VM(vm);
    wo_set_option_pointer(CS_VAL(vmbase->cr), vm, result);
    return WO_API_STATE_OF_VM(vmbase);
}
wo_result_t wo_ret_option_ptr_may_null(wo_vm vm, wo_ptr_t result)
{
    wo::vmbase* vmbase = WO_VM(vm);
    wo_set_option_ptr_may_null(CS_VAL(vmbase->cr), vm, result);
    return WO_API_STATE_OF_VM(vmbase);
}
wo_result_t wo_ret_option_val(wo_vm vm, wo_value result)
{
    wo::vmbase* vmbase = WO_VM(vm);
    wo_set_option_val(CS_VAL(vmbase->cr), vm, result);
    return WO_API_STATE_OF_VM(vmbase);
}
wo_result_t wo_ret_option_gchandle(wo_vm vm, wo_ptr_t resource_ptr, wo_value holding_val, wo_gchandle_close_func_t destruct_func)
{
    wo::vmbase* vmbase = WO_VM(vm);
    wo_set_option_gchandle(CS_VAL(vmbase->cr), vm, resource_ptr, holding_val, destruct_func);
    return WO_API_STATE_OF_VM(vmbase);
}
wo_result_t wo_ret_option_gcstruct(wo_vm vm, wo_ptr_t resource_ptr, wo_gcstruct_mark_func_t mark_func, wo_gchandle_close_func_t destruct_func)
{
    wo::vmbase* vmbase = WO_VM(vm);
    wo_set_option_gcstruct(CS_VAL(vmbase->cr), vm, resource_ptr, mark_func, destruct_func);
    return WO_API_STATE_OF_VM(vmbase);
}
wo_result_t wo_ret_option_none(wo_vm vm)
{
    wo::vmbase* vmbase = WO_VM(vm);
    wo_set_option_none(CS_VAL(vmbase->cr), vm);
    return WO_API_STATE_OF_VM(vmbase);
}

wo_result_t wo_ret_err_void(wo_vm vm)
{
    wo::vmbase* vmbase = WO_VM(vm);
    wo_set_err_void(CS_VAL(vmbase->cr), vm);
    return WO_API_STATE_OF_VM(vmbase);
}
wo_result_t wo_ret_err_char(wo_vm vm, wo_char_t result)
{
    wo::vmbase* vmbase = WO_VM(vm);
    wo_set_err_char(CS_VAL(vmbase->cr), vm, result);
    return WO_API_STATE_OF_VM(vmbase);
}
wo_result_t wo_ret_err_bool(wo_vm vm, wo_bool_t result)
{
    wo::vmbase* vmbase = WO_VM(vm);
    wo_set_err_bool(CS_VAL(vmbase->cr), vm, result);
    return WO_API_STATE_OF_VM(vmbase);
}
wo_result_t wo_ret_err_int(wo_vm vm, wo_integer_t result)
{
    wo::vmbase* vmbase = WO_VM(vm);
    wo_set_err_int(CS_VAL(vmbase->cr), vm, result);
    return WO_API_STATE_OF_VM(vmbase);
}
wo_result_t wo_ret_err_real(wo_vm vm, wo_real_t result)
{
    wo::vmbase* vmbase = WO_VM(vm);
    wo_set_err_real(CS_VAL(vmbase->cr), vm, result);
    return WO_API_STATE_OF_VM(vmbase);
}
wo_result_t wo_ret_err_float(wo_vm vm, float result)
{
    wo::vmbase* vmbase = WO_VM(vm);
    wo_set_err_float(CS_VAL(vmbase->cr), vm, result);
    return WO_API_STATE_OF_VM(vmbase);
}
wo_result_t wo_ret_err_handle(wo_vm vm, wo_handle_t result)
{
    wo::vmbase* vmbase = WO_VM(vm);
    wo_set_err_handle(CS_VAL(vmbase->cr), vm, result);
    return WO_API_STATE_OF_VM(vmbase);
}
wo_result_t wo_ret_err_string(wo_vm vm, wo_string_t result)
{
    wo::vmbase* vmbase = WO_VM(vm);
    wo_set_err_string(CS_VAL(vmbase->cr), vm, result);
    return WO_API_STATE_OF_VM(vmbase);
}
wo_result_t wo_ret_err_string_fmt(wo_vm vm, wo_string_t fmt, ...)
{
    wo::vmbase* vmbase = WO_VM(vm);
    va_list v1;
    va_start(v1, fmt);
    _wo_set_err_string_vfmt(CS_VAL(vmbase->cr), vm, fmt, v1);
    va_end(v1);
    return WO_API_STATE_OF_VM(vmbase);
}

wo_result_t wo_ret_err_buffer(wo_vm vm, const void* result, wo_size_t len)
{
    wo::vmbase* vmbase = WO_VM(vm);
    wo_set_err_buffer(CS_VAL(vmbase->cr), vm, result, len);
    return WO_API_STATE_OF_VM(vmbase);
}
wo_result_t wo_ret_err_pointer(wo_vm vm, wo_ptr_t result)
{
    wo::vmbase* vmbase = WO_VM(vm);
    wo_set_err_pointer(CS_VAL(vmbase->cr), vm, result);
    return WO_API_STATE_OF_VM(vmbase);
}
wo_result_t wo_ret_err_val(wo_vm vm, wo_value result)
{
    wo::vmbase* vmbase = WO_VM(vm);
    wo_set_err_val(CS_VAL(vmbase->cr), vm, result);
    return WO_API_STATE_OF_VM(vmbase);
}
wo_result_t wo_ret_err_gchandle(wo_vm vm, wo_ptr_t resource_ptr, wo_value holding_val, wo_gchandle_close_func_t destruct_func)
{
    wo::vmbase* vmbase = WO_VM(vm);
    wo_set_err_gchandle(CS_VAL(vmbase->cr), vm, resource_ptr, holding_val, destruct_func);
    return WO_API_STATE_OF_VM(vmbase);
}
wo_result_t wo_ret_err_gcstruct(wo_vm vm, wo_ptr_t resource_ptr, wo_gcstruct_mark_func_t mark_func, wo_gchandle_close_func_t destruct_func)
{
    wo::vmbase* vmbase = WO_VM(vm);
    wo_set_err_gcstruct(CS_VAL(vmbase->cr), vm, resource_ptr, mark_func, destruct_func);
    return WO_API_STATE_OF_VM(vmbase);
}
wo_result_t wo_ret_yield(wo_vm vm)
{
    WO_VM(vm)->interrupt(wo::vmbase::BR_YIELD_INTERRUPT);
    return wo_result_t::WO_API_RESYNC;
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

wo_size_t wo_str_char_len(wo_value value)
{
    auto _rsvalue = WO_VAL(value);

    if (_rsvalue->type == wo::value::valuetype::string_type)
        return (wo_integer_t)wo::u8strnlen(
            _rsvalue->string->c_str(), _rsvalue->string->size());

    wo_fail(WO_FAIL_TYPE_FAIL, "Value is not a string.");
    return 0;
}

wo_size_t wo_lengthof(wo_value value)
{
    auto _rsvalue = WO_VAL(value);

    switch (_rsvalue->type)
    {
    case wo::value::valuetype::array_type:
        return wo_arr_len(value);
    case wo::value::valuetype::dict_type:
        return wo_map_len(value);
    case wo::value::valuetype::string_type:
        return wo_str_char_len(value);
    case wo::value::valuetype::struct_type:
        return wo_struct_len(value);
    default:
        wo_fail(WO_FAIL_TYPE_FAIL, "Only 'string','array', 'struct' or 'dict' can get length.");
        return 0;
    }
}

wo_size_t wo_str_byte_len(wo_value value)
{
    auto _rsvalue = WO_VAL(value);
    if (_rsvalue->type == wo::value::valuetype::string_type)
        return (wo_int_t)_rsvalue->string->size();

    wo_fail(WO_FAIL_TYPE_FAIL, "Value is not a string.");
    return 0;
}

wo_char_t wo_str_get_char(wo_string_t str, wo_size_t index)
{
    size_t len = strlen(str);

    wo_char_t ch = wo::u8strnidx(str, len, (size_t)index);
    if (ch == 0 && wo::u8strnlen(str, len) <= (size_t)index)
        wo_fail(WO_FAIL_INDEX_FAIL, "Index out of range.");
    return ch;
}

wo_wstring_t wo_str_to_wstr(wo_string_t str)
{
    static thread_local std::wstring wstr_buf;
    wstr_buf = wo::str_to_wstr(str);

    return wstr_buf.c_str();
}

wo_string_t  wo_wstr_to_str(wo_wstring_t str)
{
    static thread_local std::string str_buf;
    str_buf = wo::wstr_to_str(str);

    return str_buf.c_str();
}

wo_char_t wo_strn_get_char(wo_string_t str, wo_size_t size, wo_size_t index)
{
    wo_char_t ch = wo::u8strnidx(str, size, (size_t)index);
    if (ch == 0 && wo::u8strnlen(str, size) <= (size_t)index)
        wo_fail(WO_FAIL_INDEX_FAIL, "Index out of range.");
    return ch;
}
wo_wstring_t wo_strn_to_wstr(wo_string_t str, wo_size_t size)
{
    static thread_local std::wstring wstr_buf;
    wstr_buf = wo::strn_to_wstr(str, size);

    return wstr_buf.c_str();
}
wo_string_t  wo_wstrn_to_str(wo_wstring_t str, wo_size_t size)
{
    static thread_local std::string str_buf;
    str_buf = wo::wstrn_to_str(str, size);

    return str_buf.c_str();
}

void wo_enable_jit(wo_bool_t option)
{
    wo::config::ENABLE_JUST_IN_TIME = (option != WO_FALSE);
}

wo_bool_t wo_virtual_binary(wo_string_t filepath, const void* data, wo_size_t len, wo_bool_t enable_modify)
{
    return WO_CBOOL(wo::create_virtual_binary(wo::str_to_wstr(filepath), data, len, enable_modify != WO_FALSE));
}
wo_bool_t wo_virtual_source(wo_string_t filepath, wo_string_t data, wo_bool_t enable_modify)
{
    return wo_virtual_binary(filepath, data, strlen(data), enable_modify);
}

struct _wo_virtual_file
{
    std::string m_path;
    wo::stream_types<false>::buffer m_buffer;

    _wo_virtual_file(const _wo_virtual_file&) = delete;
    _wo_virtual_file(_wo_virtual_file&&) = delete;
    _wo_virtual_file& operator = (const _wo_virtual_file&) = delete;
    _wo_virtual_file& operator = (_wo_virtual_file&&) = delete;

    ~_wo_virtual_file() = default;
    _wo_virtual_file() = default;
};

wo_virtual_file_t wo_open_virtual_file(wo_string_t filepath)
{
    _wo_virtual_file* vfinstance = new _wo_virtual_file;

    std::wstring real_path;

    if (wo::check_and_read_virtual_source<false>(
        &vfinstance->m_buffer,
        &real_path,
        wo::str_to_wstr(filepath),
        std::nullopt))
    {
        vfinstance->m_path = wo::wstr_to_str(real_path);
        return vfinstance;
    }

    delete vfinstance;
    return nullptr;
}
wo_string_t wo_virtual_file_path(wo_virtual_file_t file)
{
    return file->m_path.c_str();
}
const void* wo_virtual_file_data(wo_virtual_file_t file, size_t* len)
{
    *len = file->m_buffer.size();
    return file->m_buffer.c_str();
}
void wo_close_virtual_file(wo_virtual_file_t file)
{
    delete file;
}

wo_bool_t wo_remove_virtual_file(wo_string_t filepath)
{
    return WO_CBOOL(wo::remove_virtual_binary(wo::str_to_wstr(filepath)));
}

struct _wo_virtual_file_iter
{
    std::vector<std::string> m_paths;
    size_t m_index;
};
wo_virtual_file_iter_t wo_open_virtual_file_iter(void)
{
    auto* result = new _wo_virtual_file_iter();
    result->m_index = 0;

    auto paths = wo::get_all_virtual_file_path();
    for (auto& p : paths)
    {
        result->m_paths.push_back(wo::wstrn_to_str(p));
    }
    return result;
}
wo_string_t wo_next_virtual_file_iter(wo_virtual_file_iter_t iter)
{
    if (iter->m_index >= iter->m_paths.size())
    {
        wo_assert(iter->m_index == iter->m_paths.size());
        return nullptr;
    }
    return iter->m_paths[iter->m_index++].c_str();
}
void wo_close_virtual_file_iter(wo_virtual_file_iter_t iter)
{
    delete iter;
}

wo_vm wo_sub_vm(wo_vm vm)
{
    return CS_VM(WO_VM(vm)->make_machine(wo::vmbase::vm_type::NORMAL));
}

wo_vm wo_gc_vm(wo_vm vm)
{
    return CS_VM(WO_VM(vm)->get_or_alloc_gcvm());
}

void wo_close_vm(wo_vm vm)
{
    delete WO_VM(vm);
}

wo_vm wo_borrow_vm(wo_vm vm)
{
    if (global_vm_pool != nullptr)
        return CS_VM(global_vm_pool->borrow_vm_from_exists_vm(WO_VM(vm)));
    return wo_sub_vm(vm);
}
void wo_release_vm(wo_vm vm)
{
    if (global_vm_pool != nullptr)
        global_vm_pool->release_vm(WO_VM(vm));
    else
        wo_close_vm(vm);
}

wo::compile_result _wo_compile_impl(
    wo_string_t virtual_src_path,
    const void* src,
    size_t      len,
    const std::optional<wo::lexer*>& parent_lexer,
    std::optional<wo::shared_pointer<wo::runtime_env>>* out_env_if_success,
    std::optional<std::unique_ptr<wo::lexer>>* out_lexer_if_failed
#ifndef WO_DISABLE_COMPILER
    , std::optional<std::unique_ptr<wo::LangContext>>* out_langcontext_if_pass_grammar
#endif
)
{
    // 0. Try load binary
    const char* load_binary_failed_reason = nullptr;
    bool is_valid_binary = false;

    wo::compile_result compile_result = wo::compile_result::PROCESS_FAILED;

    std::optional<wo::shared_pointer<wo::runtime_env>> compile_env_result =
        wo::runtime_env::load_create_env_from_binary(
            virtual_src_path, src, len, wo::vmbase::VM_DEFAULT_STACK_SIZE,
            &load_binary_failed_reason,
            &is_valid_binary);
    std::unique_ptr<wo::lexer> compile_lexer;

    if (!compile_env_result.has_value())
    {
        std::wstring wvspath = wo::str_to_wstr(virtual_src_path);
        if (is_valid_binary)
        {
            // Is Woolang format binary, but failed to load.
            // Failed to load binary, maybe broken or version missing.
            wo_assert(load_binary_failed_reason != nullptr);

            compile_lexer = std::make_unique<wo::lexer>(
                parent_lexer,
                wo::wstring_pool::get_pstr(wvspath),
                std::make_unique<std::wistringstream>(std::wstring()));

            (void)compile_lexer->record_parser_error(
                wo::lexer::msglevel_t::error,
                wo::str_to_wstr(load_binary_failed_reason).c_str());
        }
        else
        {
            // 1. Prepare lexer..
            if (src != nullptr)
            {
                // Load from virtual source.
                wo::normalize_path(&wvspath);

                std::wstring strbuffer = wo::str_to_wstr(std::string((const char*)src, len).c_str());
                compile_lexer = std::make_unique<wo::lexer>(
                    parent_lexer,
                    wo::wstring_pool::get_pstr(wvspath),
                    std::make_unique<std::wistringstream>(strbuffer));
            }
            else
            {
                // Load from real file.
                std::wstring real_file_path;

                std::optional<std::unique_ptr<std::wistream>> content_stream =
                    std::nullopt;

                if (wo::check_virtual_file_path(
                    &real_file_path,
                    wvspath,
                    std::nullopt))
                {
                    content_stream =
                        wo::open_virtual_file_stream<true>(real_file_path);
                }

                compile_lexer = std::make_unique<wo::lexer>(
                    parent_lexer,
                    wo::wstring_pool::get_pstr(real_file_path),
                    std::move(content_stream));
            }

#ifndef WO_DISABLE_COMPILER
            if (!compile_lexer->has_error())
            {
                // 2. Lexer will create ast_tree;
                auto* result = wo::get_wo_grammar()->gen(*compile_lexer);
                if (result != nullptr)
                {
                    compile_result =
                        wo::compile_result::PROCESS_FAILED_BUT_GRAMMAR_OK;

                    std::unique_ptr<wo::LangContext> lang_context =
                        std::make_unique<wo::LangContext>();

                    compile_result = lang_context->process(*compile_lexer, result);
                    if (wo::compile_result::PROCESS_OK == compile_result)
                    {
                        // Finish!, finalize the compiler.
                        compile_env_result = std::move(
                            lang_context->m_ircontext.c().finalize(
                                wo::vmbase::VM_DEFAULT_STACK_SIZE));
                    }

                    if (out_langcontext_if_pass_grammar != nullptr)
                        *out_langcontext_if_pass_grammar = std::move(lang_context);
                }
            }
#else
            (void)compile_lexer->record_parser_error(
                wo::lexer::msglevel_t::error, WO_ERR_COMPILER_DISABLED);
#endif
        }
    }
    else
        // Load binary success. 
        compile_result = wo::compile_result::PROCESS_OK;

    // Compile finished.
    if (compile_env_result.has_value())
    {
        // Success
        wo_assert(compile_result == wo::compile_result::PROCESS_OK);

        if (out_env_if_success != nullptr)
            *out_env_if_success = std::move(compile_env_result.value());
    }
    else
    {
        // Failed
        wo_assert((bool)compile_lexer);
        wo_assert(compile_result != wo::compile_result::PROCESS_OK);

        if (out_lexer_if_failed != nullptr)
            *out_lexer_if_failed = std::move(compile_lexer);
    }
    return compile_result;
}

wo_bool_t _wo_load_source(
    wo_vm vm,
    wo_string_t virtual_src_path,
    const void* src,
    size_t len,
    const std::optional<wo::lexer*>& parent_lexer)
{
    wo::start_string_pool_guard sg;

    std::optional<wo::shared_pointer<wo::runtime_env>> _env_if_success;
    std::optional<std::unique_ptr<wo::lexer>> _lexer_if_failed;

#ifndef WO_DISABLE_COMPILER
    std::forward_list<wo::ast::AstBase*> m_last_context;
    bool need_exchange_back =
        wo::ast::AstBase::exchange_this_thread_ast(m_last_context);
#endif

    auto compile_result =
        _wo_compile_impl(
            virtual_src_path,
            src,
            len,
            parent_lexer,
            &_env_if_success,
            &_lexer_if_failed
#ifndef WO_DISABLE_COMPILER
            , nullptr
#endif
        );

#ifndef WO_DISABLE_COMPILER
    wo::ast::AstBase::clean_this_thread_ast();

    if (need_exchange_back)
        wo::ast::AstBase::exchange_this_thread_ast(
            m_last_context);
#endif

    if (compile_result == wo::compile_result::PROCESS_OK)
    {
        WO_VM(vm)->set_runtime(_env_if_success.value());
        return WO_TRUE;
    }
    else
    {
        wo_assert(_lexer_if_failed.has_value() && _lexer_if_failed.value()->has_error());

        WO_VM(vm)->compile_info = std::move(_lexer_if_failed);
        return WO_FALSE;
    }
}

wo_bool_t wo_load_binary(wo_vm vm, wo_string_t virtual_src_path, const void* buffer, wo_size_t length)
{
    static std::atomic_size_t vcount = 0;
    std::string vpath;
    if (virtual_src_path == nullptr)
        vpath = "/woolang/__runtime_script_" + std::to_string(++vcount) + "__";
    else
    {
        std::wstring wvpath = wo::str_to_wstr(virtual_src_path);
        wo::normalize_path(&wvpath);
        vpath = wo::wstr_to_str(wvpath);
    }

    if (!wo_virtual_binary(vpath.c_str(), buffer, length, WO_TRUE))
        return WO_FALSE;

    return _wo_load_source(vm, vpath.c_str(), buffer, length, std::nullopt);
}

void* wo_dump_binary(wo_vm vm, wo_bool_t saving_pdi, wo_size_t* out_length)
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
    auto* vmm = WO_VM(vm);

    if (vm && vmm->compile_info.has_value())
        return WO_TRUE;

    return WO_FALSE;
}

std::wstring _dump_src_info(
    const std::wstring& path,
    const wo::lexer::compiler_message_t& errmsg,
    size_t depth,
    size_t beginaimrow,
    size_t beginpointplace,
    size_t aimrow,
    size_t pointplace,
    wo_inform_style_t style)
{
    std::wstring src_full_path, result;

    if (wo::check_virtual_file_path(&src_full_path, path, std::nullopt))
    {
        auto content_stream = wo::open_virtual_file_stream<true>(src_full_path);
        if (content_stream)
        {
            auto& content_stream_ptr = content_stream.value();
            wo_assert(content_stream_ptr != nullptr);

            constexpr size_t UP_DOWN_SHOWN_LINE = 2;
            size_t current_row_no = 0;
            size_t current_col_no = 0;
            size_t from = beginaimrow > UP_DOWN_SHOWN_LINE ? beginaimrow - UP_DOWN_SHOWN_LINE : 0;
            size_t to = aimrow + UP_DOWN_SHOWN_LINE;

            bool first_line = true;

            auto print_src_file_print_lineno =
                [&current_row_no, &result, &first_line, depth]()
            {
                wchar_t buf[20] = {};
                if (first_line)
                    first_line = false;
                else
                    result += L"\n";

                swprintf(buf, 19, L"%-5zu | ", current_row_no + 1);
                result += std::wstring(depth == 0 ? 0 : depth + 1, L' ') + buf;
            };
            auto print_notify_line =
                [&result, &first_line, &current_row_no, &errmsg, beginpointplace, pointplace, style, beginaimrow, aimrow, depth](
                    size_t line_end_place)
            {
                wchar_t buf[20] = {};
                if (first_line)
                    first_line = false;
                else
                    result += L"\n";

                swprintf(buf, 19, L"      | ");
                std::wstring append_result = buf;

                if (style == WO_NEED_COLOR)
                    append_result += errmsg.m_level == wo::lexer::msglevel_t::error
                    ? wo::str_to_wstr(ANSI_HIR)
                    : wo::str_to_wstr(ANSI_HIC);

                if (current_row_no == aimrow)
                {
                    if (current_row_no == beginaimrow)
                    {
                        size_t i = 1;
                        for (; i <= beginpointplace; i++)
                            append_result += L" ";
                        for (; i < pointplace; i++)
                            append_result += L"~";
                    }
                    else
                        for (size_t i = 1; i < pointplace; i++)
                            append_result += L"~";

                    append_result += L"~\\"
                        + wo::str_to_wstr(ANSI_UNDERLNE)
                        + L" " WO_HERE
                        + wo::str_to_wstr(ANSI_NUNDERLNE);

                    if (depth != 0)
                        append_result += L": " + errmsg.m_describe;
                }
                else
                {
                    if (current_row_no == beginaimrow)
                    {
                        size_t i = 1;
                        for (; i <= beginpointplace; i++)
                            append_result += L" ";
                        if (i < line_end_place)
                            for (; i < line_end_place; i++)
                                append_result += L"~";
                        else
                            return;
                    }
                    else
                    {
                        size_t i = 1;
                        if (i < line_end_place)
                            for (; i < line_end_place; i++)
                                append_result += L"~";
                        else
                            return;
                    }
                }

                if (style == WO_NEED_COLOR)
                    append_result += wo::str_to_wstr(ANSI_RST);

                result += std::wstring(depth == 0 ? 0 : depth + 1, L' ') + append_result;
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
                    current_col_no = 0;
                    current_row_no++;
                    if (from <= current_row_no && current_row_no <= to)
                        print_src_file_print_lineno();
                    continue;
                }
                else if (ch == L'\r')
                {
                    if (current_row_no >= beginaimrow && current_row_no <= aimrow)
                        print_notify_line(current_col_no);
                    current_col_no = 0;
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

std::string _wo_dump_lexer_context_error(wo::lexer* lex, wo_inform_style_t style)
{
    std::wstring src_file_path;
    std::string _vm_compile_errors;

    size_t last_depth = 0;

    for (auto& err_info : lex->get_current_error_frame())
    {
        if (err_info.m_layer != 0)
        {
            auto see_also = wo::wstr_to_str(last_depth >= err_info.m_layer ? WO_SEE_ALSO : WO_SEE_HERE);
            if (style == WO_NEED_COLOR)
                _vm_compile_errors
                += std::string(err_info.m_layer, ' ')
                + ANSI_HIY + see_also + ANSI_RST
                + ":\n";
            else
                _vm_compile_errors
                += std::string(err_info.m_layer, ' ')
                + see_also
                + ":\n";
        }

        last_depth = err_info.m_layer;

        if (src_file_path != err_info.m_filename)
        {
            if (style == WO_NEED_COLOR)
                _vm_compile_errors +=
                ANSI_HIR "In file: '" ANSI_RST
                + wo::wstrn_to_str(src_file_path = err_info.m_filename)
                + ANSI_HIR "'" ANSI_RST "\n";
            else
                _vm_compile_errors +=
                "In file: '"
                + wo::wstrn_to_str(src_file_path = err_info.m_filename)
                + "'\n";
        }

        if (err_info.m_layer == 0)
            _vm_compile_errors += wo::wstr_to_str(err_info.to_wstring(style & WO_NEED_COLOR)) + "\n";

        // Print source informations..
        _vm_compile_errors += wo::wstr_to_str(
            _dump_src_info(
                src_file_path,
                err_info,
                err_info.m_layer,
                err_info.m_range_begin[0],
                err_info.m_range_begin[1],
                err_info.m_range_end[0],
                err_info.m_range_end[1],
                style));
    }

    if (lex->get_current_error_frame().size() >= WO_MAX_ERROR_COUNT)
        _vm_compile_errors += wo::wstr_to_str(WO_TOO_MANY_ERROR(WO_MAX_ERROR_COUNT) + L"\n");

    return _vm_compile_errors;
}

wo_string_t wo_get_compile_error(wo_vm vm, wo_inform_style_t style)
{
    auto* vmm = WO_VM(vm);

    if (style == WO_DEFAULT)
        style = wo::config::ENABLE_OUTPUT_ANSI_COLOR_CTRL ? WO_NEED_COLOR : WO_NOTHING;

    thread_local std::string _vm_compile_errors;
    _vm_compile_errors = "";

    if (vm && vmm->compile_info.has_value())
    {
        _vm_compile_errors += _wo_dump_lexer_context_error(
            vmm->compile_info.value().get(), style);
    }
    return _vm_compile_errors.c_str();
}

wo_string_t wo_get_runtime_error(wo_vm vm)
{
    return wo_cast_string(CS_VAL(WO_VM(vm)->er));
}

wo_bool_t wo_abort_vm(wo_vm vm)
{
    auto* vmm = WO_VM(vm);
    std::shared_lock gs(wo::vmbase::_alive_vm_list_mx);

    if (wo::vmbase::_alive_vm_list.find(vmm) != wo::vmbase::_alive_vm_list.end())
    {
        return WO_CBOOL(vmm->interrupt(wo::vmbase::vm_interrupt_type::ABORT_INTERRUPT));
    }
    return WO_FALSE;
}

wo_value wo_register(wo_vm vm, wo_reg regid)
{
    return CS_VAL(WO_VM(vm)->register_mem_begin + regid);
}

wo_value wo_reserve_stack(wo_vm vm, wo_size_t stack_sz, wo_value* inout_args_maynull)
{
    // Check stack size.
    wo::vmbase* vmbase = WO_VM(vm);

    wo_assert(inout_args_maynull == nullptr
        || (WO_VAL(*inout_args_maynull) > vmbase->_self_stack_mem_buf
            && WO_VAL(*inout_args_maynull) <= vmbase->stack_mem_begin));

    const size_t args_offset =
        inout_args_maynull ? vmbase->stack_mem_begin - WO_VAL(*inout_args_maynull) : 0;

    if (vmbase->assure_stack_size(stack_sz) && inout_args_maynull)
    {
        *inout_args_maynull = CS_VAL(vmbase->stack_mem_begin - args_offset);
        if (vmbase->bp != vmbase->stack_mem_begin)
        {
            auto* current_call_base = vmbase->bp + 1;

            // NOTE: If bp + 1 is not callstack:
            //  1) It has been marked `stack_externed_flag`
            //  2) The VM has already returned from last function call.
            if (current_call_base->type == wo::value::valuetype::callstack
                || current_call_base->type == wo::value::valuetype::nativecallstack)
            {
                current_call_base->type = (wo::value::valuetype)(
                    current_call_base->type | wo::value::valuetype::stack_externed_flag);
            }
        }
    }

    vmbase->sp -= stack_sz;
    auto result = vmbase->sp + 1;

    // Clean reserved space.
    memset(result, 0, sizeof(wo::value) * stack_sz);

    return CS_VAL(result);
}

void wo_pop_stack(wo_vm vm, wo_size_t stack_sz)
{
    WO_VM(vm)->sp += stack_sz;
}

wo_stack_value wo_cast_stack_value(wo_vm vm, wo_value value_in_stack)
{
    auto* vmbase = WO_VM(vm);

    auto* stack_begin = vmbase->stack_mem_begin;
    auto* stack_value = WO_VAL(value_in_stack);

    if (stack_value > vmbase->sp && stack_value <= stack_begin)
        return stack_begin - stack_value;

    wo_fail(WO_FAIL_INDEX_FAIL, "Invalid stack value.");
    return SIZE_MAX;
}

void wo_stack_value_set(wo_stack_value sv, wo_vm vm, wo_value val)
{
    auto* vmbase = WO_VM(vm);

    wo_assert(sv < (size_t)(vmbase->stack_mem_begin - vmbase->sp));
    wo_set_val(CS_VAL(vmbase->stack_mem_begin - sv), val);
}
void wo_stack_value_get(wo_value outval, wo_stack_value sv, wo_vm vm)
{
    auto* vmbase = WO_VM(vm);

    wo_assert(sv < (size_t)(vmbase->stack_mem_begin - vmbase->sp));
    wo_set_val(outval, CS_VAL(vmbase->stack_mem_begin - sv));
}

wo_value wo_invoke_rsfunc(
    wo_vm vm, wo_int_t vmfunc, wo_int_t argc, wo_value* inout_args_maynull, wo_value* inout_s_maynull)
{
    _wo_in_thread_vm_guard g(vm);
    _wo_enter_gc_guard g2(vm);
    _wo_reserved_stack_args_update_guard g3(vm, inout_args_maynull, inout_s_maynull);
    wo_value result = CS_VAL(WO_VM(vm)->invoke(vmfunc, argc));
    return result;
}
wo_value wo_invoke_exfunc(
    wo_vm vm, wo_handle_t exfunc, wo_int_t argc, wo_value* inout_args_maynull, wo_value* inout_s_maynull)
{
    _wo_in_thread_vm_guard g(vm);
    _wo_enter_gc_guard g2(vm);
    _wo_reserved_stack_args_update_guard g3(vm, inout_args_maynull, inout_s_maynull);
    wo_value result = CS_VAL(WO_VM(vm)->invoke(exfunc, argc));
    return result;
}
wo_value wo_invoke_value(
    wo_vm vm, wo_value vmfunc, wo_int_t argc, wo_value* inout_args_maynull, wo_value* inout_s_maynull)
{
    _wo_in_thread_vm_guard g(vm);
    _wo_enter_gc_guard g2(vm);
    _wo_reserved_stack_args_update_guard g3(vm, inout_args_maynull, inout_s_maynull);

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

void wo_dispatch_rsfunc(wo_vm vm, wo_int_t vmfunc, wo_int_t argc, wo_value* inout_args_maynull, wo_value* inout_s_maynull)
{
    _wo_in_thread_vm_guard g(vm);
    _wo_enter_gc_guard g2(vm);
    _wo_reserved_stack_args_update_guard g3(vm, inout_args_maynull, inout_s_maynull);

    auto* vmm = WO_VM(vm);

    vmm->set_br_yieldable(true);
    vmm->co_pre_invoke(vmfunc, argc);
}
void wo_dispatch_exfunc(
    wo_vm vm, wo_handle_t exfunc, wo_int_t argc, wo_value* inout_args_maynull, wo_value* inout_s_maynull)
{
    _wo_in_thread_vm_guard g(vm);
    _wo_enter_gc_guard g2(vm);
    _wo_reserved_stack_args_update_guard g3(vm, inout_args_maynull, inout_s_maynull);

    auto* vmm = WO_VM(vm);

    vmm->set_br_yieldable(true);
    vmm->co_pre_invoke(exfunc, argc);
}
void wo_dispatch_value(
    wo_vm vm, wo_value vmfunc, wo_int_t argc, wo_value* inout_args_maynull, wo_value* inout_s_maynull)
{
    switch (WO_VAL(vmfunc)->type)
    {
    case wo::value::valuetype::closure_type:
    {
        _wo_in_thread_vm_guard g(vm);
        _wo_enter_gc_guard g2(vm);
        _wo_reserved_stack_args_update_guard g3(vm, inout_args_maynull, inout_s_maynull);

        auto* vmm = WO_VM(vm);

        vmm->set_br_yieldable(true);
        vmm->co_pre_invoke(WO_VAL(vmfunc)->closure, argc);
        break;
    }
    case wo::value::valuetype::integer_type:
        wo_dispatch_rsfunc(
            vm, WO_VAL(vmfunc)->integer, argc, inout_args_maynull, inout_s_maynull);
        break;
    case wo::value::valuetype::handle_type:
        wo_dispatch_exfunc(
            vm, WO_VAL(vmfunc)->handle, argc, inout_args_maynull, inout_s_maynull);
        break;
    default:
        wo_fail(WO_FAIL_TYPE_FAIL, "Cannot dispatch non-function value by 'wo_dispatch_closure'.");
    }
}

wo_value wo_dispatch(
    wo_vm vm, wo_value* inout_args_maynull, wo_value* inout_s_maynull)
{
    _wo_in_thread_vm_guard g(vm);
    _wo_enter_gc_guard g2(vm);
    _wo_reserved_stack_args_update_guard g3(vm, inout_args_maynull, inout_s_maynull);

    auto* vmm = WO_VM(vm);
    wo_assert(vmm->get_br_yieldable());

    if (vmm->env)
    {
        wo_assert(vmm->tc->type == wo::value::valuetype::integer_type);

        auto origin_tc = (++(vmm->sp))->integer;
        auto origin_spbp = (++(vmm->sp))->vmcallstack;

        auto dispatch_result = vmm->run();

        switch (dispatch_result)
        {
        case wo_result_t::WO_API_RESYNC:
            // NOTE: WO_API_RESYNC returned by `wo_func_addr`(and it's a extern function)
            //  Only following cases happend:
            //  1) Stack reallocated.
            //  2) Aborted
            //  3) Yield
            //  For case 1) & 2), return immediately; for 3) we should clean it and mark 
            //  the VM's yield flag.
            if (vmm->clear_interrupt(wo::vmbase::vm_interrupt_type::BR_YIELD_INTERRUPT))
                vmm->mark_br_yield();
            break;
        case wo_result_t::WO_API_NORMAL:
            break;
        case wo_result_t::WO_API_SYNC:
            vmm->run();
            break;
        }

        if (vmm->get_and_clear_br_yield_flag())
        {
            (vmm->sp--)->set_callstack(origin_spbp.ret_ip, origin_spbp.bp);
            (vmm->sp--)->set_integer(origin_tc);

            return WO_CONTINUE;
        }
        else
        {
            vmm->sp = vmm->stack_mem_begin - origin_spbp.ret_ip;
            vmm->bp = vmm->stack_mem_begin - origin_spbp.bp;
            vmm->tc->set_integer(origin_tc);

            vmm->set_br_yieldable(false);

            if (!vmm->is_aborted())
                return CS_VAL(vmm->cr);
        }
    }
    return nullptr;
}

wo_bool_t wo_load_source(wo_vm vm, wo_string_t virtual_src_path, wo_string_t src)
{
    return wo_load_binary(vm, virtual_src_path, src, strlen(src));
}

wo_bool_t wo_load_file(wo_vm vm, wo_string_t virtual_src_path)
{
    return _wo_load_source(vm, virtual_src_path, nullptr, 0, std::nullopt);
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

    auto* vmm = WO_VM(vm);

    if (vmm->env)
    {
        vmm->ip = vmm->env->rt_codes;
        vmm->run();

        if (!vmm->is_aborted())
            return CS_VAL(vmm->cr);
    }
    return nullptr;
}

wo_size_t wo_struct_len(wo_value value)
{
    auto _struct = WO_VAL(value);

    if (_struct->type == wo::value::valuetype::struct_type)
    {
        // no need lock for struct's count
        return (wo_size_t)_struct->structs->m_count;
    }

    wo_fail(WO_FAIL_TYPE_FAIL, "Value is not a struct.");
    return 0;
}

wo_bool_t wo_struct_try_get(wo_value out_val, wo_value value, uint16_t offset)
{
    auto _struct = WO_VAL(value);

    if (_struct->type == wo::value::valuetype::struct_type)
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

    if (_struct->type == wo::value::valuetype::struct_type)
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

wo_integer_t wo_union_get(wo_value out_val, wo_value unionval)
{
    auto* val = WO_VAL(unionval);
    if (val->type != wo::value::valuetype::struct_type
        || val->structs->m_count != 2
        || val->structs->m_values[0].type
        != wo::value::valuetype::integer_type)
        wo_fail(WO_FAIL_TYPE_FAIL, "Unexpected value type.");
    else
    {
        auto r = val->structs->m_values[0].integer;
        wo_set_val(out_val, CS_VAL(&val->structs->m_values[1]));
        return r;
    }
    return -1;
}

wo_bool_t wo_result_get(wo_value out_val, wo_value resultval)
{
    return WO_CBOOL(0 == wo_union_get(out_val, resultval));
}

void wo_arr_resize(wo_value arr, wo_size_t newsz, wo_value init_val)
{
    auto _arr = WO_VAL(arr);

    if (_arr->type == wo::value::valuetype::array_type)
    {
        wo::gcbase::gc_modify_write_guard g1(_arr->array);
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
wo_bool_t wo_arr_insert(wo_value arr, wo_size_t place, wo_value val)
{
    auto _arr = WO_VAL(arr);

    if (_arr->type == wo::value::valuetype::array_type)
    {
        wo::gcbase::gc_modify_write_guard g1(_arr->array);

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
wo_bool_t wo_arr_try_set(wo_value arr, wo_size_t index, wo_value val)
{
    auto _arr = WO_VAL(arr);
    if (_arr->type == wo::value::valuetype::array_type)
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
void wo_arr_set(wo_value arr, wo_size_t index, wo_value val)
{
    if (!wo_arr_try_set(arr, index, val))
        wo_fail(WO_FAIL_INDEX_FAIL, "Failed to index: out of range.");
}
void wo_arr_add(wo_value arr, wo_value elem)
{
    auto _arr = WO_VAL(arr);

    if (_arr->type == wo::value::valuetype::array_type)
    {
        wo::gcbase::gc_modify_write_guard g1(_arr->array);

        if (elem)
            _arr->array->push_back(*WO_VAL(elem));
        else
            (void)_arr->array->emplace_back();
    }
    else
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is not an array.");
}

wo_size_t wo_arr_len(wo_value arr)
{
    auto _arr = WO_VAL(arr);
    if (_arr->type == wo::value::valuetype::array_type)
    {
        wo::gcbase::gc_read_guard g1(_arr->array);
        return (wo_size_t)_arr->array->size();
    }

    wo_fail(WO_FAIL_TYPE_FAIL, "Value is not an array.");
    return 0;
}

wo_bool_t wo_arr_try_get(wo_value out_val, wo_value arr, wo_size_t index)
{
    auto _arr = WO_VAL(arr);
    if (_arr->type == wo::value::valuetype::array_type)
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
void wo_arr_get(wo_value out_val, wo_value arr, wo_size_t index)
{
    if (!wo_arr_try_get(out_val, arr, index))
        wo_fail(WO_FAIL_INDEX_FAIL, "Failed to index: out of range.");
}

void wo_map_get(wo_value out_val, wo_value map, wo_value index)
{
    if (!wo_map_try_get(out_val, map, index))
        wo_fail(WO_FAIL_INDEX_FAIL, "Failed to index: out of range.");
}

wo_bool_t wo_arr_front(wo_value out_val, wo_value arr)
{
    auto _arr = WO_VAL(arr);
    if (_arr->type == wo::value::valuetype::array_type)
    {
        wo::gcbase::gc_read_guard g1(_arr->array);
        if (!_arr->array->empty())
        {
            WO_VAL(out_val)->set_val(&_arr->array->front());
            return WO_TRUE;
        }
    }
    else
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is not an array.");

    return WO_FALSE;
}
wo_bool_t wo_arr_back(wo_value out_val, wo_value arr)
{
    auto _arr = WO_VAL(arr);
    if (_arr->type == wo::value::valuetype::array_type)
    {
        wo::gcbase::gc_read_guard g1(_arr->array);
        if (!_arr->array->empty())
        {
            WO_VAL(out_val)->set_val(&_arr->array->back());
            return WO_TRUE;
        }
    }
    else
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is not an array.");

    return WO_FALSE;
}
void wo_arr_front_val(wo_value out_val, wo_value arr)
{
    if (!wo_arr_front(out_val, arr))
    {
        wo_fail(WO_FAIL_INDEX_FAIL, "Failed to get front.");
    }
}
void wo_arr_back_val(wo_value out_val, wo_value arr)
{
    if (!wo_arr_back(out_val, arr))
    {
        wo_fail(WO_FAIL_INDEX_FAIL, "Failed to get back.");
    }
}

wo_bool_t wo_arr_pop_front(wo_value out_val, wo_value arr)
{
    auto _arr = WO_VAL(arr);
    if (_arr->type == wo::value::valuetype::array_type)
    {
        wo::gcbase::gc_modify_write_guard g1(_arr->array);

        if (!_arr->array->empty())
        {
            auto* value_to_pop = &_arr->array->front();

            if (wo::gc::gc_is_marking())
                wo::gcbase::write_barrier(value_to_pop);

            WO_VAL(out_val)->set_val(value_to_pop);
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
    if (_arr->type == wo::value::valuetype::array_type)
    {
        wo::gcbase::gc_modify_write_guard g1(_arr->array);

        if (!_arr->array->empty())
        {
            auto* value_to_pop = &_arr->array->back();

            if (wo::gc::gc_is_marking())
                wo::gcbase::write_barrier(value_to_pop);

            WO_VAL(out_val)->set_val(value_to_pop);
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

wo_bool_t wo_arr_find(wo_value arr, wo_value elem, wo_size_t* out_index)
{
    auto _arr = WO_VAL(arr);
    auto _aim = WO_VAL(elem);
    if (_arr->type == wo::value::valuetype::array_type)
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
        {
            *out_index = fnd - _arr->array->begin();
            return WO_TRUE;
        }
    }
    else
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is not an array.");

    return WO_FALSE;
}
wo_bool_t wo_arr_remove(wo_value arr, wo_size_t index)
{
    auto _arr = WO_VAL(arr);
    if (_arr->type == wo::value::valuetype::array_type)
    {
        wo::gcbase::gc_modify_write_guard g1(_arr->array);

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
    if (_arr->type == wo::value::valuetype::array_type)
    {
        wo::gcbase::gc_modify_write_guard g1(_arr->array);
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
    if (_arr->type == wo::value::valuetype::array_type)
    {
        wo::gcbase::gc_read_guard g1(_arr->array);
        return WO_CBOOL(_arr->array->empty());
    }
    else
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is not an array.");
    return WO_TRUE;
}

wo_size_t wo_map_len(wo_value map)
{
    auto _map = WO_VAL(map);
    if (_map->type == wo::value::valuetype::dict_type)
    {
        wo::gcbase::gc_read_guard g1(_map->dict);
        return (wo_size_t)_map->dict->size();
    }
    wo_fail(WO_FAIL_TYPE_FAIL, "Value is not a map.");
    return 0;
}

wo_bool_t wo_map_find(wo_value map, wo_value index)
{
    auto _map = WO_VAL(map);
    if (_map->type == wo::value::valuetype::dict_type)
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

wo_bool_t wo_map_get_or_set(wo_value out_val, wo_value map, wo_value index, wo_value default_value)
{
    auto _map = WO_VAL(map);
    if (_map->type == wo::value::valuetype::dict_type)
    {
        wo::value* store_val = nullptr;
        wo::gcbase::gc_modify_write_guard g1(_map->dict);

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

wo_result_t wo_ret_map_get_or_set_do(
    wo_vm vm,
    wo_value map,
    wo_value index,
    wo_value value_function,
    wo_value* inout_args_maynull,
    wo_value* inout_s_maynull)
{
    wo::vmbase* vmbase = WO_VM(vm);

    // function call might modify stack, we should pay attention to it.
    auto _map = WO_VAL(map);
    if (_map->type == wo::value::valuetype::dict_type)
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
            wo::dict_t* dict = _map->dict;
            wo::value raw_index;

            raw_index.set_val(WO_VAL(index));

            wo_value result = wo_invoke_value(
                vm, value_function, 0, inout_args_maynull, inout_s_maynull);

            ////////////////////////////////////////////////////////////////////
           // DONOT USE map, index, value_function after this line
           ////////////////////////////////////////////////////////////////////

            if (nullptr == result)
                // Aborted.
                return WO_API_RESYNC;

            (*dict)[raw_index] = *WO_VAL(result);
            return WO_API_STATE_OF_VM(vmbase);
        }
        else
            return wo_ret_val(vm, CS_VAL(result));
    }
    else
    {
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is not a map.");
        return WO_API_RESYNC;
    }
}

wo_bool_t wo_map_get_or_default(wo_value out_val, wo_value map, wo_value index, wo_value default_value)
{
    auto _map = WO_VAL(map);
    if (_map->type == wo::value::valuetype::dict_type)
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
    if (_map->type == wo::value::valuetype::dict_type)
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

void wo_map_reserve(wo_value map, wo_size_t sz)
{
    auto _map = WO_VAL(map);
    if (_map->type == wo::value::valuetype::dict_type)
    {
        wo::gcbase::gc_modify_write_guard g1(_map->dict);
        _map->dict->reserve(sz);
    }
    else
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is not a map.");
}

void wo_map_set(wo_value map, wo_value index, wo_value val)
{
    auto _map = WO_VAL(map);
    if (_map->type == wo::value::valuetype::dict_type)
    {
        wo::gcbase::gc_modify_write_guard g1(_map->dict);
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
    if (_map->type == wo::value::valuetype::dict_type)
    {
        wo::gcbase::gc_modify_write_guard g1(_map->dict);
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
    if (_map->type == wo::value::valuetype::dict_type)
    {
        wo::gcbase::gc_modify_write_guard g1(_map->dict);
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
    if (_map->type == wo::value::valuetype::dict_type)
    {
        wo::gcbase::gc_read_guard g1(_map->dict);
        return WO_CBOOL(_map->dict->empty());
    }
    else
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is not a map.");
    return WO_TRUE;
}

void wo_map_keys(wo_value out_val, wo_vm vm, wo_value map)
{
    _wo_enter_gc_guard g(vm);

    auto _map = WO_VAL(map);
    if (_map->type == wo::value::valuetype::dict_type)
    {
        wo::gcbase::gc_read_guard g1(_map->dict);
        auto* keys = wo::array_t::gc_new<wo::gcbase::gctype::young>(_map->dict->size());
        wo::gcbase::gc_modify_write_guard g2(keys);
        size_t i = 0;
        for (auto& kvpair : *_map->dict)
        {
            keys->at(i++).set_val(&kvpair.first);
        }
        WO_VAL(out_val)->set_gcunit<wo::value::valuetype::array_type>(keys);
    }
    else
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is not a map.");
}
void wo_map_vals(wo_value out_val, wo_vm vm, wo_value map)
{
    _wo_enter_gc_guard g(vm);

    auto _map = WO_VAL(map);
    if (_map->type == wo::value::valuetype::dict_type)
    {
        wo::gcbase::gc_read_guard g1(_map->dict);
        auto* vals = wo::array_t::gc_new<wo::gcbase::gctype::young>(_map->dict->size());
        wo::gcbase::gc_modify_write_guard g2(vals);
        size_t i = 0;
        for (auto& kvpair : *_map->dict)
        {
            vals->at(i++).set_val(&kvpair.second);
        }
        WO_VAL(out_val)->set_gcunit<wo::value::valuetype::array_type>(vals);
    }
    else
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is not a map.");
}

wo_bool_t wo_gchandle_close(wo_value gchandle)
{
    auto* gchandle_ptr = WO_VAL(gchandle)->gchandle;
    wo::gcbase::gc_write_guard g1(gchandle_ptr);
    return WO_CBOOL(gchandle_ptr->do_close());
}

void wo_gcunit_lock(wo_value gc_reference_object)
{
    auto* value = WO_VAL(gc_reference_object);
    if (value->is_gcunit())
    {
        auto* gcunit = WO_VAL(gc_reference_object)->gcunit;
        gcunit->write();
    }
    else
    {
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is not lockable.");
    }
}
void wo_gcunit_unlock(wo_value gc_reference_object)
{
    auto* value = WO_VAL(gc_reference_object);
    if (value->is_gcunit())
    {
        auto* gcunit = WO_VAL(gc_reference_object)->gcunit;
        gcunit->write_end();
    }
    else
    {
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is not lockable.");
    }
}
void wo_gcunit_lock_shared_force(wo_value gc_reference_object)
{
    auto* value = WO_VAL(gc_reference_object);
    if (value->is_gcunit())
    {
        auto* gcunit = WO_VAL(gc_reference_object)->gcunit;
        gcunit->read();
    }
    else
    {
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is not lockable.");
    }
}
void wo_gcunit_unlock_shared_force(wo_value gc_reference_object)
{
    auto* value = WO_VAL(gc_reference_object);
    if (value->is_gcunit())
    {
        auto* gcunit = WO_VAL(gc_reference_object)->gcunit;
        gcunit->read_end();
}
    else
    {
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is not lockable.");
    }
    }
void wo_gcunit_lock_shared(wo_value gc_reference_object)
{
#if WO_FORCE_GC_OBJ_THREAD_SAFETY
    wo_gcunit_lock_shared_force(gc_reference_object);
#endif
    (void)gc_reference_object;
}
void wo_gcunit_unlock_shared(wo_value gc_reference_object)
{
#if WO_FORCE_GC_OBJ_THREAD_SAFETY
    wo_gcunit_unlock_shared_force(gc_reference_object);
#endif
    (void)gc_reference_object;
}

// DEBUGGEE TOOLS
void wo_attach_default_debuggee()
{
    wo::default_cli_debuggee_bridge* dbg = new wo::default_cli_debuggee_bridge();
    if (auto* old_debuggee = wo::vmbase::attach_debuggee(dbg))
        old_debuggee->_abandon();
}

void wo_attach_user_debuggee(wo_debuggee_callback_func_t callback, void* userdata)
{
    wo::c_debuggee_bridge* dbg = new wo::c_debuggee_bridge(callback, userdata);
    if (auto* old_debuggee = wo::vmbase::attach_debuggee(dbg))
        old_debuggee->_abandon();
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
        old_debuggee->_abandon();
}

void wo_break_immediately()
{
    if (auto* debuggee = dynamic_cast<wo::default_cli_debuggee_bridge*>(wo::vmbase::current_debuggee()))
        debuggee->breakdown_immediately();
    else
        wo_fail(WO_FAIL_DEBUGGEE_FAIL, "'wo_break_immediately' can only break the vm attached default debuggee.");
}

void wo_break_specify_immediately(wo_vm vmm)
{
    if (auto* debuggee = dynamic_cast<wo::default_cli_debuggee_bridge*>(wo::vmbase::current_debuggee()))
        debuggee->breakdown_at_vm_immediately(WO_VM(vmm));
    else
        wo_fail(WO_FAIL_DEBUGGEE_FAIL, "'wo_break_specify_immediately' can only break the vm attached default debuggee.");
}

wo_bool_t wo_extern_symb(wo_vm vm, wo_string_t fullname, wo_integer_t* out_wo_func, wo_handle_t* out_jit_func)
{
    auto env = WO_VM(vm)->env;
    if (env->try_find_script_func(fullname, out_wo_func))
    {
        if (!env->try_find_jit_func(*out_wo_func, out_jit_func))
            *out_jit_func = 0;

        return WO_TRUE;
    }
    return WO_FALSE;
}

wo_string_t wo_debug_trace_callstack(wo_vm vm, wo_size_t layer)
{
    auto* vmm = WO_VM(vm);

    std::stringstream sstream;
    vmm->dump_call_stack(layer, true, sstream);

    wo_set_string(CS_VAL(vmm->er), vm, sstream.str().c_str());
    wo_assert(vmm->er->type == wo::value::valuetype::string_type);

    return vmm->er->string->c_str();
}

wo_dylib_handle_t wo_fake_lib(
    const char* libname, const wo_extern_lib_func_t* funcs, wo_dylib_handle_t dependence_dylib_may_null)
{
    return (void*)loaded_lib_info::create_fake_lib(
        libname, funcs, std::launder(reinterpret_cast<dylib_table_instance*>(dependence_dylib_may_null)));
}

wo_dylib_handle_t wo_load_lib(
    const char* libname, const char* path, const char* script_path, wo_bool_t panic_when_fail)
{
    return loaded_lib_info::load_lib(
        libname,
        wo::str_to_wstr(path).c_str(),
        script_path != nullptr ? wo::str_to_wstr(script_path).c_str() : nullptr,
        panic_when_fail);
}
wo_dylib_handle_t wo_load_func(void* lib, const char* funcname)
{
    auto* dylib = std::launder(reinterpret_cast<dylib_table_instance*>(lib));
    return dylib->load_func(funcname);
}
void wo_unload_lib(wo_dylib_handle_t lib, wo_dylib_unload_method_t method)
{
    auto* dylib = std::launder(reinterpret_cast<dylib_table_instance*>(lib));
    loaded_lib_info::unload_lib(dylib, method);
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
    auto* vm = WO_VM(vm_may_null);

    auto* old_one = wo::vmbase::_this_thread_vm;
    wo::vmbase::_this_thread_vm = vm;

#if WO_ENABLE_RUNTIME_CHECK
    if (vm != nullptr)
        vm->attaching_thread_id = std::this_thread::get_id();
#endif

    return CS_VM(old_one);
}

void wo_gc_checkpoint(wo_vm vm)
{
    auto* vmm = WO_VM(vm);

    // If in GC, hang up here to make sure safe.
    if ((vmm->vm_interrupt.load() & (
        wo::vmbase::vm_interrupt_type::GC_INTERRUPT
        | wo::vmbase::vm_interrupt_type::GC_HANGUP_INTERRUPT)) != 0)
    {
        if (!vmm->gc_checkpoint())
        {
            if (vmm->clear_interrupt(wo::vmbase::vm_interrupt_type::GC_HANGUP_INTERRUPT))
                vmm->hangup();
        }
    }
}

wo_bool_t wo_leave_gcguard(wo_vm vm)
{
    auto* vmm = WO_VM(vm);

    if (!vmm->check_interrupt(wo::vmbase::vm_interrupt_type::LEAVE_INTERRUPT))
    {
        wo_gc_checkpoint(vm);

        wo_assure(vmm->interrupt(wo::vmbase::vm_interrupt_type::LEAVE_INTERRUPT));
        return WO_TRUE;
    }
    return WO_FALSE;
}
wo_bool_t wo_enter_gcguard(wo_vm vm)
{
    auto* vmm = WO_VM(vm);

    if (vmm->check_interrupt(wo::vmbase::vm_interrupt_type::LEAVE_INTERRUPT))
    {
        wo_gc_checkpoint(vm);

        wo_assure(vmm->clear_interrupt(wo::vmbase::vm_interrupt_type::LEAVE_INTERRUPT));
        return WO_TRUE;
    }
    return WO_FALSE;
}

wo_weak_ref wo_create_weak_ref(wo_value val)
{
    return wo::weakref::create_weak_ref(WO_VAL(val));
}
void wo_close_weak_ref(wo_weak_ref ref)
{
    wo::weakref::close_weak_ref(ref);
}
wo_bool_t wo_lock_weak_ref(wo_value out_val, wo_weak_ref ref)
{
    return WO_CBOOL(wo::weakref::lock_weak_ref(WO_VAL(out_val), ref));
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
        err += wo_get_compile_error(_vm, WO_NEED_COLOR);
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

wo_pin_value wo_create_pin_value(void)
{
    wo_pin_value v = wo::pin::create_pin_value();
    return v;
}
void wo_pin_value_set(wo_pin_value pin_value, wo_value val)
{
    wo::pin::set_pin_value(pin_value, WO_VAL(val));
}
void wo_pin_value_set_dup(wo_pin_value pin_value, wo_value val)
{
    wo::pin::set_dup_pin_value(pin_value, WO_VAL(val));
}
void wo_pin_value_get(wo_value out_value, wo_pin_value pin_value)
{
    wo::pin::read_pin_value(WO_VAL(out_value), pin_value);
}
void wo_close_pin_value(wo_pin_value pin_value)
{
    wo::pin::close_pin_value(pin_value);
}
void wo_gc_record_memory(wo_value val)
{
    if (wo::gc::gc_is_marking())
        wo::gcbase::write_barrier(WO_VAL(val));
}

wo_ir_compiler wo_create_ir_compiler(void)
{
    return reinterpret_cast<wo_ir_compiler>(new wo::ir_compiler);
}
void wo_close_ir_compiler(wo_ir_compiler ircompiler)
{
    delete std::launder(reinterpret_cast<wo::ir_compiler*>(ircompiler));
}

void wo_ir_opcode(wo_ir_compiler compiler, uint8_t opcode, uint8_t drh, uint8_t drl)
{
    auto* c = std::launder(reinterpret_cast<wo::ir_compiler*>(compiler));

    wo::instruct::opcode code = (wo::instruct::opcode)(opcode << (uint8_t)2);
    uint8_t dr = (uint8_t)(drh << (uint8_t)1) | drl;
    c->ir_opcode(code, dr);
}

void wo_ir_bind_tag(wo_ir_compiler compiler, wo_string_t name)
{
    auto* c = std::launder(reinterpret_cast<wo::ir_compiler*>(compiler));
    c->tag(name);
}

void wo_ir_int(wo_ir_compiler compiler, wo_integer_t val)
{
    auto* c = std::launder(reinterpret_cast<wo::ir_compiler*>(compiler));
    c->ir_opnum(wo::opnum::imm(val));
}
void wo_ir_real(wo_ir_compiler compiler, wo_real_t val)
{
    auto* c = std::launder(reinterpret_cast<wo::ir_compiler*>(compiler));
    c->ir_opnum(wo::opnum::imm(val));
}
void wo_ir_handle(wo_ir_compiler compiler, wo_handle_t val)
{
    auto* c = std::launder(reinterpret_cast<wo::ir_compiler*>(compiler));
    c->ir_opnum(wo::opnum::imm_hdl(val));
}
void wo_ir_string(wo_ir_compiler compiler, wo_string_t val)
{
    auto* c = std::launder(reinterpret_cast<wo::ir_compiler*>(compiler));
    c->ir_opnum(wo::opnum::imm_str(val));
}
void wo_ir_bool(wo_ir_compiler compiler, wo_bool_t val)
{
    auto* c = std::launder(reinterpret_cast<wo::ir_compiler*>(compiler));
    c->ir_opnum(wo::opnum::imm((bool)val));
}
void wo_ir_glb(wo_ir_compiler compiler, int32_t offset)
{
    auto* c = std::launder(reinterpret_cast<wo::ir_compiler*>(compiler));
    c->ir_opnum(wo::opnum::global(offset));
}
void wo_ir_reg(wo_ir_compiler compiler, uint8_t regid)
{
    auto* c = std::launder(reinterpret_cast<wo::ir_compiler*>(compiler));
    c->ir_opnum(wo::opnum::reg(regid));
}
void wo_ir_bp(wo_ir_compiler compiler, int8_t offset)
{
    auto* c = std::launder(reinterpret_cast<wo::ir_compiler*>(compiler));
    c->ir_opnum(wo::opnum::reg(wo::opnum::reg::bp_offset(offset)));
}
void wo_ir_tag(wo_ir_compiler compiler, wo_string_t name)
{
    auto* c = std::launder(reinterpret_cast<wo::ir_compiler*>(compiler));
    c->ir_opnum(wo::opnum::tag(name));
}

void wo_ir_immtag(wo_ir_compiler compiler, wo_string_t name)
{
    auto* c = std::launder(reinterpret_cast<wo::ir_compiler*>(compiler));
    c->ir_imm_tag(wo::opnum::tag(name));
}
void wo_ir_immu8(wo_ir_compiler compiler, uint8_t val)
{
    auto* c = std::launder(reinterpret_cast<wo::ir_compiler*>(compiler));
    c->ir_imm_u8(val);
}
void wo_ir_immu16(wo_ir_compiler compiler, uint16_t val)
{
    auto* c = std::launder(reinterpret_cast<wo::ir_compiler*>(compiler));
    c->ir_imm_u16(val);
}
void wo_ir_immu32(wo_ir_compiler compiler, uint32_t val)
{
    auto* c = std::launder(reinterpret_cast<wo::ir_compiler*>(compiler));
    c->ir_imm_u32(val);
}
void wo_ir_immu64(wo_ir_compiler compiler, uint64_t val)
{
    auto* c = std::launder(reinterpret_cast<wo::ir_compiler*>(compiler));
    c->ir_imm_u64(val);
}

void wo_ir_register_extern_function(
    wo_ir_compiler compiler, wo_native_func_t extern_func,
    wo_string_t script_path,
    wo_string_t library_name_may_null,
    wo_string_t function_name)
{
    auto* c = std::launder(reinterpret_cast<wo::ir_compiler*>(compiler));
    c->record_extern_native_function((intptr_t)extern_func,
        wo::str_to_wstr(script_path),
        library_name_may_null == nullptr ? std::nullopt : std::optional(wo::str_to_wstr(library_name_may_null)),
        wo::str_to_wstr(function_name));
}

void wo_load_ir_compiler(wo_vm vm, wo_ir_compiler compiler)
{
    auto* c = std::launder(reinterpret_cast<wo::ir_compiler*>(compiler));
    c->end();

    WO_VM(vm)->set_runtime(c->finalize(wo::vmbase::VM_DEFAULT_STACK_SIZE));
}
