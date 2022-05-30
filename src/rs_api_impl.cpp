// rs_api_impl.cpp
#define _CRT_SECURE_NO_WARNINGS
#include "rs_vm.hpp"
#include "rs_source_file_manager.hpp"
#include "rs_compiler_parser.hpp"
#include "rs_exceptions.hpp"
#include "rs_stdlib.hpp"
#include "rs_lang_grammar_loader.hpp"
#include "rs_lang.hpp"
#include "rs_utf8.hpp"
#include "rs_runtime_debuggee.hpp"
#include "rs_global_setting.hpp"
#include "rs_io.hpp"
#include "rs_roroutine_simulate_mgr.hpp"
#include "rs_roroutine_thread_mgr.hpp"

#include <csignal>
#include <sstream>

// TODO LIST
// 1. ALL GC_UNIT OPERATE SHOULD BE ATOMIC

#define RS_VERSION(DEV,MAIN,SUB,CORRECT) ((0x##DEV##ull)<<(3*16))|((0x##MAIN##ull)<<(2*16))|((0x##SUB##ull)<<(1*16))|((0x##CORRECT##ull)<<(0*16))
#define RS_VERSION_STR(DEV,MAIN,SUB,CORRECT) #DEV "." #MAIN "." #SUB "." #CORRECT "."

#ifdef _DEBUG
#define RS_DEBUG_SFX "debug"
#else
#define RS_DEBUG_SFX ""
#endif

constexpr rs_integer_t version = RS_VERSION(de, 0, 0, 1);
constexpr char         version_str[] = RS_VERSION_STR(de, 0, 0, 1) RS_DEBUG_SFX;

#undef RS_DEBUG_SFX
#undef RS_VERSION_STR
#undef RS_VERSION


#include <atomic>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <string>


void _default_fail_handler(rs_string_t src_file, uint32_t lineno, rs_string_t functionname, uint32_t rterrcode, rs_string_t reason)
{
    rs::rs_stderr << ANSI_HIR "RS Runtime happend a failure: "
        << ANSI_HIY << reason << " (E" << std::hex << rterrcode << std::dec << ")" << ANSI_RST << rs::rs_endl;
    rs::rs_stderr << "\tAt source: \t" << src_file << rs::rs_endl;
    rs::rs_stderr << "\tAt line: \t" << lineno << rs::rs_endl;
    rs::rs_stderr << "\tAt function: \t" << functionname << rs::rs_endl;
    rs::rs_stderr << rs::rs_endl;

    rs::rs_stderr << ANSI_HIR "callstack: " ANSI_RST << rs::rs_endl;

    if (rs::vmbase::_this_thread_vm)
        rs::vmbase::_this_thread_vm->dump_call_stack(32, true, std::cerr);

    rs::rs_stderr << rs::rs_endl;

    if ((rterrcode & RS_FAIL_TYPE_MASK) == RS_FAIL_MINOR)
    {
        rs::rs_stderr << ANSI_HIY "This is a minor failure, ignore it." ANSI_RST << rs::rs_endl;
        // Just ignore it..
    }
    else if ((rterrcode & RS_FAIL_TYPE_MASK) == RS_FAIL_MEDIUM)
    {
        // Just throw it..
        rs::rs_stderr << ANSI_HIY "This is a medium failure, it will be throw." ANSI_RST << rs::rs_endl;
        throw rs::rsruntime_exception(rterrcode, reason);
    }
    else if ((rterrcode & RS_FAIL_TYPE_MASK) == RS_FAIL_HEAVY)
    {
        // Just throw it..
        rs::rs_stderr << ANSI_HIY "This is a heavy failure, abort." ANSI_RST << rs::rs_endl;
        throw rs::rsruntime_exception(rterrcode, reason);
    }
    else
    {
        rs::rs_stderr << "This failure may cause a crash or nothing happens." << rs::rs_endl;
        rs::rs_stderr << "1) Abort program.(You can attatch debuggee.)" << rs::rs_endl;
        rs::rs_stderr << "2) Continue.(May cause unknown errors.)" << rs::rs_endl;
        rs::rs_stderr << "3) Roll back to last RS-EXCEPTION-RECOVERY.(Safe, but may cause memory leak and dead-lock.)" << rs::rs_endl;
        rs::rs_stderr << "4) Halt (Not exactly safe, this vm will be abort.)" << rs::rs_endl;
        rs::rs_stderr << "5) Throw exception.(Not exactly safe)" << rs::rs_endl;
        do
        {
            int choice;
            rs::rs_stderr << "Please input your choice: " ANSI_HIY;
            std::cin >> choice;
            rs::rs_stderr << ANSI_RST;
            switch (choice)
            {
            case 1:
                rs_error(reason);
            case 2:
                return;
            case 3:
                if (rs::vmbase::_this_thread_vm)
                {
                    rs::vmbase::_this_thread_vm->er->set_gcunit_with_barrier(rs::value::valuetype::string_type);
                    rs::string_t::gc_new<rs::gcbase::gctype::eden>(rs::vmbase::_this_thread_vm->er->gcunit, reason);
                    rs::exception_recovery::rollback(rs::vmbase::_this_thread_vm);
                }
                else
                    rs::rs_stderr << ANSI_HIR "No virtual machine running in this thread." ANSI_RST << rs::rs_endl;

                break;
            case 4:
                rs::rs_stderr << ANSI_HIR "Current virtual machine will abort." ANSI_RST << rs::rs_endl;
                throw rs::rsruntime_exception(rterrcode, reason);

                // in debug, if there is no catcher for rs_runtime_error, 
                // the program may continue working.
                // Abort here.
                rs_error(reason);
            case 5:
                throw rs::rsruntime_exception(RS_FAIL_MEDIUM, reason);

                // in debug, if there is no catcher for rs_runtime_error, 
                // the program may continue working.
                // Abort here.
                rs_error(reason);
            default:
                rs::rs_stderr << ANSI_HIR "Invalid choice" ANSI_RST << rs::rs_endl;
            }

            char _useless_for_clear = 0;
            std::cin.clear();
            while (std::cin.readsome(&_useless_for_clear, 1));

        } while (true);
    }
}
static std::atomic<rs_fail_handler> _rs_fail_handler_function = &_default_fail_handler;

rs_fail_handler rs_regist_fail_handler(rs_fail_handler new_handler)
{
    return _rs_fail_handler_function.exchange(new_handler);
}
void rs_cause_fail(rs_string_t src_file, uint32_t lineno, rs_string_t functionname, uint32_t rterrcode, rs_string_t reason)
{
    _rs_fail_handler_function.load()(src_file, lineno, functionname, rterrcode, reason);
}

void rs_throw(rs_string_t reason)
{
    throw rs::rsruntime_exception(RS_FAIL_MEDIUM, reason);
}

void rs_halt(rs_string_t reason)
{
    throw rs::rsruntime_exception(RS_FAIL_HEAVY, reason);
}

void _rs_ctrl_c_signal_handler(int sig)
{
    // CTRL + C, 
    rs::rs_stderr << ANSI_HIR "CTRL+C:" ANSI_RST " Pause all virtual-machine by default debuggee immediately." << rs::rs_endl;

    std::lock_guard g1(rs::vmbase::_alive_vm_list_mx);
    for (auto vm : rs::vmbase::_alive_vm_list)
    {
        if (!rs_has_attached_debuggee((rs_vm)vm))
            rs_attach_default_debuggee((rs_vm)vm);
        rs_break_immediately((rs_vm)vm);
    }

    rs_handle_ctrl_c(_rs_ctrl_c_signal_handler);

}

void rs_handle_ctrl_c(void(*handler)(int))
{
    signal(SIGINT, handler ? handler : _rs_ctrl_c_signal_handler);
}

#undef rs_init

void rs_finish()
{
    bool scheduler_need_shutdown = true;

    // Ready to shutdown all vm & coroutine.
    do
    {
        if (scheduler_need_shutdown)
            rs_coroutine_stopall();

        do
        {
            std::lock_guard g1(rs::vmbase::_alive_vm_list_mx);

            for (auto& alive_vms : rs::vmbase::_alive_vm_list)
                alive_vms->interrupt(rs::vmbase::ABORT_INTERRUPT);
        } while (false);

        if (scheduler_need_shutdown)
        {
            rs::fvmscheduler::shutdown();
            scheduler_need_shutdown = false;
        }
        rs_gc_immediately();

        std::this_thread::yield();

        std::lock_guard g1(rs::vmbase::_alive_vm_list_mx);
        if (rs::vmbase::_alive_vm_list.empty())
            break;

    } while (true);

    rs_gc_stop();
}

void rs_init(int argc, char** argv)
{
    const char* basic_env_local = "en_US.UTF-8";
    bool enable_std_package = true;
    bool enable_ctrl_c_to_debug = true;
    bool enable_gc = true;
    size_t coroutine_mgr_thread_count = 4;

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
            else if ("enable-ctrlc-debug" == current_arg)
                enable_ctrl_c_to_debug = atoi(argv[++command_idx]);
            else if ("enable-gc" == current_arg)
                enable_gc = atoi(argv[++command_idx]);
            else if ("enable-code-allign" == current_arg)
                rs::config::ENABLE_IR_CODE_ACTIVE_ALLIGN = atoi(argv[++command_idx]);
            else if ("enable-ansi-color" == current_arg)
                rs::config::ENABLE_OUTPUT_ANSI_COLOR_CTRL = atoi(argv[++command_idx]);
            else if ("coroutine-thread-count" == current_arg)
                coroutine_mgr_thread_count = atoi(argv[++command_idx]);
            else
                rs::rs_stderr << ANSI_HIR "RScene: " << ANSI_RST << "unknown setting --" << current_arg << rs::rs_endl;
        }
    }

    rs::rs_init_locale(basic_env_local);

    if (enable_gc)
        rs::gc::gc_start(); // I dont know who will disable gc..

    rs::fvmscheduler::init(coroutine_mgr_thread_count);

    if (enable_std_package)
    {
        rs_virtual_source(rs_stdlib_basic_src_path, rs_stdlib_basic_src_data, false);
        rs_virtual_source(rs_stdlib_src_path, rs_stdlib_src_data, false);
        rs_virtual_source(rs_stdlib_debug_src_path, rs_stdlib_debug_src_data, false);
        rs_virtual_source(rs_stdlib_vm_src_path, rs_stdlib_vm_src_data, false);
        rs_virtual_source(rs_stdlib_thread_src_path, rs_stdlib_thread_src_data, false);
        rs_virtual_source(rs_stdlib_roroutine_src_path, rs_stdlib_roroutine_src_data, false);
        rs_virtual_source(rs_stdlib_macro_src_path, rs_stdlib_macro_src_data, false);
    }

    if (enable_ctrl_c_to_debug)
        rs_handle_ctrl_c(nullptr);
}

rs_string_t  rs_compile_date(void)
{
    return __DATE__ " " __TIME__;
}
rs_string_t  rs_version(void)
{
    return version_str;
}
rs_integer_t rs_version_int(void)
{
    return version;
}

#define RS_ORIGIN_VAL(v) (reinterpret_cast<rs::value*>(v))
#define RS_VAL(v) (RS_ORIGIN_VAL(v)->get())
#define RS_VM(v) (reinterpret_cast<rs::vmbase*>(v))
#define CS_VAL(v) (reinterpret_cast<rs_value>(v))
#define CS_VM(v) (reinterpret_cast<rs_vm>(v))

rs_string_t rs_locale_name()
{
    return rs::rs_global_locale_name.c_str();
}

rs_string_t rs_exe_path()
{
    return rs::exe_path();
}

rs_ptr_t rs_safety_pointer_ignore_fail(rs::gchandle_t* gchandle)
{
    if (gchandle->has_been_closed)
    {
        return nullptr;
    }
    return gchandle->holding_handle;
}

rs_ptr_t rs_safety_pointer(rs::gchandle_t* gchandle)
{
    if (gchandle->has_been_closed)
    {
        rs_fail(RS_FAIL_ACCESS_NIL, "Reading a closed gchandle.");
        return nullptr;
    }
    return gchandle->holding_handle;
}

rs_type rs_valuetype(rs_value value)
{
    auto _rsvalue = RS_VAL(value);

    return (rs_type)_rsvalue->type;
}
rs_integer_t rs_int(rs_value value)
{
    auto _rsvalue = RS_VAL(value);
    if (_rsvalue->type != rs::value::valuetype::integer_type)
    {
        rs_fail(RS_FAIL_TYPE_FAIL, "This value is not an integer.");
        return rs_cast_int(value);
    }
    return _rsvalue->integer;
}
rs_real_t rs_real(rs_value value)
{
    auto _rsvalue = RS_VAL(value);
    if (_rsvalue->type != rs::value::valuetype::real_type)
    {
        rs_fail(RS_FAIL_TYPE_FAIL, "This value is not an real.");
        return rs_cast_real(value);
    }
    return _rsvalue->real;
}
float rs_float(rs_value value)
{
    auto _rsvalue = RS_VAL(value);
    if (_rsvalue->type != rs::value::valuetype::real_type)
    {
        rs_fail(RS_FAIL_TYPE_FAIL, "This value is not an real.");
        return rs_cast_float(value);
    }
    return (float)_rsvalue->real;
}
rs_handle_t rs_handle(rs_value value)
{
    auto _rsvalue = RS_VAL(value);
    if (_rsvalue->type != rs::value::valuetype::handle_type
        && _rsvalue->type != rs::value::valuetype::gchandle_type)
    {
        rs_fail(RS_FAIL_TYPE_FAIL, "This value is not a handle.");
        return rs_cast_handle(value);
    }
    return _rsvalue->type == rs::value::valuetype::handle_type ?
        (rs_handle_t)_rsvalue->handle
        :
        (rs_handle_t)rs_safety_pointer(_rsvalue->gchandle);
}
rs_ptr_t rs_pointer(rs_value value)
{
    auto _rsvalue = RS_VAL(value);
    if (_rsvalue->type != rs::value::valuetype::handle_type
        && _rsvalue->type != rs::value::valuetype::gchandle_type)
    {
        rs_fail(RS_FAIL_TYPE_FAIL, "This value is not a handle.");
        return rs_cast_pointer(value);
    }
    return _rsvalue->type == rs::value::valuetype::handle_type ?
        (rs_ptr_t)_rsvalue->handle
        :
        (rs_ptr_t)rs_safety_pointer(_rsvalue->gchandle);
}
rs_string_t rs_string(rs_value value)
{
    auto _rsvalue = RS_VAL(value);
    if (_rsvalue->type != rs::value::valuetype::string_type)
    {
        rs_fail(RS_FAIL_TYPE_FAIL, "This value is not a string.");
        return rs_cast_string(value);
    }
    rs::gcbase::gc_read_guard rg1(_rsvalue->string);
    return _rsvalue->string->c_str();
}
rs_bool_t rs_bool(const rs_value value)
{
    auto _rsvalue = RS_VAL(value);
    return _rsvalue->handle != 0;
}
rs_value rs_value_of_gchandle(rs_value value)
{
    auto _rsvalue = RS_VAL(value);
    if (_rsvalue->type != rs::value::valuetype::gchandle_type)
    {
        rs_fail(RS_FAIL_TYPE_FAIL, "This value is not a gchandle.");
        return nullptr;
    }
    return CS_VAL(&_rsvalue->gchandle->holding_value);
}
void rs_set_int(rs_value value, rs_integer_t val)
{
    auto _rsvalue = RS_VAL(value);
    _rsvalue->set_integer(val);
}
void rs_set_real(rs_value value, rs_real_t val)
{
    auto _rsvalue = RS_VAL(value);
    _rsvalue->set_real(val);
}
void rs_set_handle(rs_value value, rs_handle_t val)
{
    auto _rsvalue = RS_VAL(value);
    _rsvalue->set_handle(val);
}
void rs_set_pointer(rs_value value, rs_ptr_t val)
{
    auto _rsvalue = RS_VAL(value);
    _rsvalue->set_handle((rs_handle_t)val);
}
void rs_set_string(rs_value value, rs_string_t val)
{
    auto _rsvalue = RS_VAL(value);
    _rsvalue->set_string(val);
}
void rs_set_val(rs_value value, rs_value val)
{
    auto _rsvalue = RS_VAL(value);
    _rsvalue->set_val(RS_VAL(val));
}
void rs_set_ref(rs_value value, rs_value val)
{
    auto _rsvalue = RS_ORIGIN_VAL(value);
    auto _ref_val = RS_VAL(val);

    if (_rsvalue->is_ref())
        _rsvalue->set_ref(_rsvalue->ref->set_ref(_ref_val));
    else
        _rsvalue->set_ref(_ref_val);
}

rs_integer_t rs_cast_int(rs_value value)
{
    auto _rsvalue = RS_VAL(value);

    switch (_rsvalue->type)
    {
    case rs::value::valuetype::integer_type:
        return _rsvalue->integer;
    case rs::value::valuetype::handle_type:
        return (rs_integer_t)_rsvalue->handle;
    case rs::value::valuetype::real_type:
        return (rs_integer_t)_rsvalue->real;
    case rs::value::valuetype::string_type:
    {
        rs::gcbase::gc_read_guard rg1(_rsvalue->string);
        return (rs_integer_t)atoll(_rsvalue->string->c_str());
    }
    default:
        rs_fail(RS_FAIL_TYPE_FAIL, "This value can not cast to integer.");
        return 0;
        break;
    }
}
rs_real_t rs_cast_real(rs_value value)
{
    auto _rsvalue = RS_VAL(value);

    switch (reinterpret_cast<rs::value*>(value)->type)
    {
    case rs::value::valuetype::integer_type:
        return (rs_real_t)_rsvalue->integer;
    case rs::value::valuetype::handle_type:
        return (rs_real_t)_rsvalue->handle;
    case rs::value::valuetype::real_type:
        return _rsvalue->real;
    case rs::value::valuetype::string_type:
    {
        rs::gcbase::gc_read_guard rg1(_rsvalue->string);
        return atof(_rsvalue->string->c_str());
    }
    default:
        rs_fail(RS_FAIL_TYPE_FAIL, "This value can not cast to real.");
        return 0;
        break;
    }
}

float rs_cast_float(rs_value value)
{
    return (float)rs_cast_real(value);
}

rs_handle_t rs_cast_handle(rs_value value)
{
    auto _rsvalue = RS_VAL(value);

    switch (reinterpret_cast<rs::value*>(value)->type)
    {
    case rs::value::valuetype::integer_type:
        return (rs_handle_t)_rsvalue->integer;
    case rs::value::valuetype::handle_type:
        return _rsvalue->handle;
    case rs::value::valuetype::gchandle_type:
        return (rs_handle_t)rs_safety_pointer(_rsvalue->gchandle);
    case rs::value::valuetype::real_type:
        return (rs_handle_t)_rsvalue->real;
    case rs::value::valuetype::string_type:
    {
        rs::gcbase::gc_read_guard rg1(_rsvalue->string);
        return (rs_handle_t)atoll(_rsvalue->string->c_str());
    }
    default:
        rs_fail(RS_FAIL_TYPE_FAIL, "This value can not cast to handle.");
        return 0;
        break;
    }
}
rs_ptr_t rs_cast_pointer(rs_value value)
{
    return (rs_ptr_t)rs_cast_handle(value);
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
                    if (rs::lexer::lex_isodigit(nextch))
                    {
                        oct_ascii *= 8;
                        oct_ascii += rs::lexer::lex_hextonum(nextch);
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
                    if (rs::lexer::lex_isxdigit(nextch))
                    {
                        hex_ascii *= 16;
                        hex_ascii += rs::lexer::lex_hextonum(nextch);
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
                    if (rs::lexer::lex_isxdigit(nextch))
                    {
                        hex_ascii *= 16;
                        hex_ascii += rs::lexer::lex_hextonum(nextch);
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
void _rs_cast_value(rs::value* value, rs::lexer* lex, rs::value::valuetype except_type);
void _rs_cast_array(rs::value* value, rs::lexer* lex)
{
    rs::array_t* rsarr;
    rs::array_t::gc_new<rs::gcbase::gctype::eden>((rs::gcbase*&)rsarr);

    while (true)
    {
        auto lex_type = lex->peek(nullptr);
        if (lex_type == +rs::lex_type::l_index_end)
        {
            lex->next(nullptr);
            break;
        }

        _rs_cast_value(value, lex, rs::value::valuetype::invalid); // key!
        rsarr->push_back(*value);

        if (lex->peek(nullptr) == +rs::lex_type::l_comma)
            lex->next(nullptr);
    }

    value->set_gcunit_with_barrier(rs::value::valuetype::array_type, rsarr);
}
void _rs_cast_map(rs::value* value, rs::lexer* lex)
{
    rs::mapping_t* rsmap;
    rs::mapping_t::gc_new<rs::gcbase::gctype::eden>((rs::gcbase*&)rsmap);

    while (true)
    {
        auto lex_type = lex->peek(nullptr);
        if (lex_type == +rs::lex_type::l_right_curly_braces)
        {
            // end
            lex->next(nullptr);
            break;
        }

        _rs_cast_value(value, lex, rs::value::valuetype::invalid); // key!
        auto& val_place = (*rsmap)[*value];

        lex_type = lex->next(nullptr);
        if (lex_type != +rs::lex_type::l_typecast)
            rs_fail(RS_FAIL_TYPE_FAIL, "Unexcept token while parsing map, here should be ':'.");

        _rs_cast_value(&val_place, lex, rs::value::valuetype::invalid); // value!

        if (lex->peek(nullptr) == +rs::lex_type::l_comma)
            lex->next(nullptr);
    }

    value->set_gcunit_with_barrier(rs::value::valuetype::mapping_type, rsmap);
}
void _rs_cast_value(rs::value* value, rs::lexer* lex, rs::value::valuetype except_type)
{
    std::wstring wstr;
    auto lex_type = lex->next(&wstr);
    if (lex_type == +rs::lex_type::l_left_curly_braces) // is map
        _rs_cast_map(value, lex);
    else if (lex_type == +rs::lex_type::l_index_begin) // is array
        _rs_cast_array(value, lex);
    else if (lex_type == +rs::lex_type::l_literal_string) // is string
        value->set_string(rs::wstr_to_str(wstr).c_str());
    else if (lex_type == +rs::lex_type::l_literal_integer) // is integer
        value->set_integer(std::stoll(rs::wstr_to_str(wstr).c_str()));
    else if (lex_type == +rs::lex_type::l_literal_real) // is real
        value->set_real(std::stod(rs::wstr_to_str(wstr).c_str()));
    else if (lex_type == +rs::lex_type::l_nil) // is nil
        value->set_nil();
    else if (wstr == L"true")
        value->set_integer(1);// true
    else if (wstr == L"false")
        value->set_integer(0);// false
    else if (wstr == L"null")
        value->set_nil();// null
    else
        rs_fail(RS_FAIL_TYPE_FAIL, "Unknown token while parsing.");

    if (except_type != rs::value::valuetype::invalid && except_type != value->type)
        rs_fail(RS_FAIL_TYPE_FAIL, "Unexcept value type after parsing.");

}
void rs_cast_value_from_str(rs_value value, rs_string_t str, rs_type except_type)
{
    rs::lexer lex(rs::str_to_wstr(str), "json");

    _rs_cast_value(RS_VAL(value), &lex, (rs::value::valuetype)except_type);
}

void _rs_cast_string(rs::value* value, std::map<rs::gcbase*, int>* traveled_gcunit, bool _fit_layout, std::string* out_str, int depth, bool force_to_be_str)
{
    auto _rsvalue = value->get();

    //if (value->type == rs::value::valuetype::is_ref)
    //    *out_str += "<is_ref>";

    switch (_rsvalue->type)
    {
    case rs::value::valuetype::integer_type:
        *out_str += _enstring(std::to_string(_rsvalue->integer), force_to_be_str);
        return;
    case rs::value::valuetype::handle_type:
        *out_str += _enstring(std::to_string(_rsvalue->handle), force_to_be_str);
        return;
    case rs::value::valuetype::real_type:
        *out_str += _enstring(std::to_string(_rsvalue->real), force_to_be_str);
        return;
    case rs::value::valuetype::gchandle_type:
        *out_str += _enstring(std::to_string((rs_handle_t)rs_safety_pointer(_rsvalue->gchandle)), force_to_be_str);
        return;
    case rs::value::valuetype::string_type:
    {
        rs::gcbase::gc_read_guard rg1(_rsvalue->string);
        *out_str += _enstring(*_rsvalue->string, true);
        return;
    }
    case rs::value::valuetype::mapping_type:
    {
        if (rs::mapping_t* map = _rsvalue->mapping)
        {
            rs::gcbase::gc_read_guard rg1(_rsvalue->mapping);
            if ((*traveled_gcunit)[map] >= 1)
            {
                _fit_layout = true;
                if ((*traveled_gcunit)[map] >= 2)
                {
                    *out_str += "{ ... }";
                    return;
                }
            }
            (*traveled_gcunit)[map]++;

            *out_str += _fit_layout ? "{" : "{\n";
            bool first_kv_pair = true;
            for (auto& [v_key, v_val] : *map)
            {
                if (!first_kv_pair)
                    *out_str += _fit_layout ? ", " : ",\n";
                first_kv_pair = false;

                for (int i = 0; !_fit_layout && i <= depth; i++)
                    *out_str += "    ";
                _rs_cast_string(const_cast<rs::value*>(&v_key), traveled_gcunit, _fit_layout, out_str, depth + 1, true);
                *out_str += _fit_layout ? ":" : " : ";
                _rs_cast_string(&v_val, traveled_gcunit, _fit_layout, out_str, depth + 1, false);

            }
            if (!_fit_layout)
                *out_str += "\n";
            for (int i = 0; !_fit_layout && i < depth; i++)
                *out_str += "    ";
            *out_str += "}";

            (*traveled_gcunit)[map]--;
        }
        else
            *out_str += "nil";
        return;
    }
    case rs::value::valuetype::array_type:
    {
        if (rs::array_t* arr = _rsvalue->array)
        {
            rs::gcbase::gc_read_guard rg1(_rsvalue->array);
            if ((*traveled_gcunit)[arr] >= 1)
            {
                _fit_layout = true;
                if ((*traveled_gcunit)[arr] >= 2)
                {
                    *out_str += "[ ... ]";
                    return;
                }
            }
            (*traveled_gcunit)[arr]++;

            *out_str += _fit_layout ? "[" : "[\n";
            bool first_value = true;
            for (auto& v_val : *arr)
            {
                if (!first_value)
                    *out_str += _fit_layout ? "," : ",\n";
                first_value = false;

                for (int i = 0; !_fit_layout && i <= depth; i++)
                    *out_str += "    ";
                _rs_cast_string(&v_val, traveled_gcunit, _fit_layout, out_str, depth + 1, false);
            }
            if (!_fit_layout)
                *out_str += "\n";
            for (int i = 0; !_fit_layout && i < depth; i++)
                *out_str += "    ";
            *out_str += "]";

            (*traveled_gcunit)[arr]--;
        }
        else
            *out_str += "nil";
        return;
    }
    case rs::value::valuetype::invalid:
        *out_str += "nil";
        return;
    default:
        rs_fail(RS_FAIL_TYPE_FAIL, "This value can not cast to string.");
        *out_str += "";
        break;
    }
}
rs_string_t rs_cast_string(const rs_value value)
{
    thread_local std::string _buf;
    _buf = "";

    auto _rsvalue = RS_VAL(value);
    switch (_rsvalue->type)
    {
    case rs::value::valuetype::integer_type:
        _buf = std::to_string(_rsvalue->integer);
        return _buf.c_str();
    case rs::value::valuetype::handle_type:
        _buf = std::to_string(_rsvalue->handle);
        return _buf.c_str();
    case rs::value::valuetype::gchandle_type:
        _buf = std::to_string((rs_handle_t)rs_safety_pointer(_rsvalue->gchandle));
        return _buf.c_str();
    case rs::value::valuetype::real_type:
        _buf = std::to_string(_rsvalue->real);
        return _buf.c_str();
    case rs::value::valuetype::string_type:
    {
        rs::gcbase::gc_read_guard rg1(_rsvalue->string);
        return _rsvalue->string->c_str();
    }
    case rs::value::valuetype::invalid:
        return "nil";
    default:
        break;
    }

    std::map<rs::gcbase*, int> _tved_gcunit;
    _rs_cast_string(reinterpret_cast<rs::value*>(value), &_tved_gcunit, false, &_buf, 0, false);

    return _buf.c_str();
}

rs_string_t rs_type_name(const rs_value value)
{
    auto _rsvalue = RS_VAL(value);
    switch (_rsvalue->type)
    {
    case rs::value::valuetype::integer_type:
        return "int";
    case rs::value::valuetype::handle_type:
        return "handle";
    case rs::value::valuetype::real_type:
        return "real";
    case rs::value::valuetype::string_type:
        return "string";
    case rs::value::valuetype::array_type:
        return "array";
    case rs::value::valuetype::mapping_type:
        return "map";
    case rs::value::valuetype::gchandle_type:
        return "gchandle";
    case rs::value::valuetype::invalid:
        return "nil";
    default:
        return "unknown";
    }
}

rs_integer_t rs_argc(const rs_vm vm)
{
    return reinterpret_cast<const rs::vmbase*>(vm)->tc->integer;
}
rs_result_t rs_ret_bool(rs_vm vm, rs_bool_t result)
{
    return reinterpret_cast<rs_result_t>(RS_VM(vm)->cr->set_integer(result ? 1 : 0));
}
rs_result_t rs_ret_int(rs_vm vm, rs_integer_t result)
{
    return reinterpret_cast<rs_result_t>(RS_VM(vm)->cr->set_integer(result));
}
rs_result_t rs_ret_real(rs_vm vm, rs_real_t result)
{
    return reinterpret_cast<rs_result_t>(RS_VM(vm)->cr->set_real(result));
}
rs_result_t rs_ret_handle(rs_vm vm, rs_handle_t result)
{
    return reinterpret_cast<rs_result_t>(RS_VM(vm)->cr->set_handle(result));
}
rs_result_t rs_ret_pointer(rs_vm vm, rs_ptr_t result)
{
    return reinterpret_cast<rs_result_t>(RS_VM(vm)->cr->set_handle((rs_handle_t)result));
}
rs_result_t rs_ret_string(rs_vm vm, rs_string_t result)
{
    return reinterpret_cast<rs_result_t>(RS_VM(vm)->cr->set_string(result));
}
rs_result_t rs_ret_gchandle(rs_vm vm, rs_ptr_t resource_ptr, rs_value holding_val, void(*destruct_func)(rs_ptr_t))
{
    RS_VM(vm)->cr->set_gcunit_with_barrier(rs::value::valuetype::gchandle_type);
    auto handle_ptr = rs::gchandle_t::gc_new<rs::gcbase::gctype::eden>(RS_VM(vm)->cr->gcunit);
    handle_ptr->holding_handle = resource_ptr;
    if (holding_val)
    {
        handle_ptr->holding_value.set_val(RS_VAL(holding_val));
        if (handle_ptr->holding_value.is_gcunit())
            handle_ptr->holding_value.gcunit->gc_type = rs::gcbase::gctype::no_gc;
    }
    handle_ptr->destructor = destruct_func;

    return reinterpret_cast<rs_result_t>(RS_VM(vm)->cr);
}
rs_result_t rs_ret_nil(rs_vm vm)
{
    return reinterpret_cast<rs_result_t>(RS_VM(vm)->cr->set_nil());
}
rs_result_t  rs_ret_val(rs_vm vm, rs_value result)
{
    if (result)
        return reinterpret_cast<rs_result_t>(
            RS_VM(vm)->cr->set_val(
                reinterpret_cast<rs::value*>(result)->get()
            ));
    return rs_ret_nil(vm);
}
rs_result_t  rs_ret_ref(rs_vm vm, rs_value result)
{
    if (result)
        return reinterpret_cast<rs_result_t>(
            RS_VM(vm)->cr->set_ref(
                reinterpret_cast<rs::value*>(result)->get()
            ));
    return rs_ret_nil(vm);
}

void rs_coroutine_pauseall()
{
    rs::fvmscheduler::pause_all();
}
void rs_coroutine_resumeall()
{
    rs::fvmscheduler::resume_all();
}

void rs_coroutine_stopall()
{
    rs::fvmscheduler::stop_all();
}

void _rs_check_atexit()
{
    std::shared_lock g1(rs::vmbase::_alive_vm_list_mx);

    do
    {
    waitting_vm_leave:
        for (auto& vm : rs::vmbase::_alive_vm_list)
            if (!(vm->vm_interrupt & rs::vmbase::LEAVE_INTERRUPT))
                goto waitting_vm_leave;
    } while (0);

    // STOP GC
}

void rs_abort_all_vm_to_exit()
{
    // rs_stop used for stop all vm and exit..

    // 1. ABORT ALL VM
    std::shared_lock g1(rs::vmbase::_alive_vm_list_mx);

    for (auto& vm : rs::vmbase::_alive_vm_list)
        vm->interrupt(rs::vmbase::ABORT_INTERRUPT);

    std::atexit(_rs_check_atexit);
}

rs_integer_t rs_lengthof(rs_value value)
{
    auto _rsvalue = RS_VAL(value);
    if (_rsvalue->is_nil())
        return 0;
    if (_rsvalue->type == rs::value::valuetype::array_type)
    {
        rs::gcbase::gc_read_guard rg1(_rsvalue->array);
        return _rsvalue->array->size();
    }
    else if (_rsvalue->type == rs::value::valuetype::mapping_type)
    {
        rs::gcbase::gc_read_guard rg1(_rsvalue->mapping);
        return _rsvalue->mapping->size();
    }
    else if (_rsvalue->type == rs::value::valuetype::string_type)
    {
        rs::gcbase::gc_read_guard rg1(_rsvalue->string);
        return rs::u8strlen(_rsvalue->string->c_str());
    }
    else
    {
        rs_fail(RS_FAIL_TYPE_FAIL, "Only 'string','array' or 'map' can get length.");
        return 0;
    }
}

rs_bool_t rs_virtual_source(rs_string_t filepath, rs_string_t data, rs_bool_t enable_modify)
{
    return rs::create_virtual_source(rs::str_to_wstr(data), rs::str_to_wstr(filepath), enable_modify);
}

rs_vm rs_create_vm()
{
    return (rs_vm)new rs::vm;
}

rs_vm rs_sub_vm(rs_vm vm, size_t stacksz)
{
    return CS_VM(RS_VM(vm)->make_machine(stacksz));
}

rs_vm rs_gc_vm(rs_vm vm)
{
    return CS_VM(RS_VM(vm)->get_or_alloc_gcvm());
}

void rs_close_vm(rs_vm vm)
{
    delete (rs::vmbase*)vm;
}

void rs_co_yield()
{
    rs::fthread::yield();
}

void rs_co_sleep(double time)
{
    rs::fvmscheduler::wait(time);
}

struct rs_custom_waitter : public rs::fvmscheduler_fwaitable_base
{
    void* m_custom_data;

    bool be_pending()override
    {
        return true;
    }
};

rs_waitter_t rs_co_create_waitter()
{
    rs::shared_pointer<rs_custom_waitter>* cwaitter
        = new rs::shared_pointer<rs_custom_waitter>(new rs_custom_waitter);
    return cwaitter;
}

void rs_co_awake_waitter(rs_waitter_t waitter, void* val)
{
    (*(rs::shared_pointer<rs_custom_waitter>*)waitter)->m_custom_data = val;
    (*(rs::shared_pointer<rs_custom_waitter>*)waitter)->awake();
}

void* rs_co_wait_for(rs_waitter_t waitter)
{
    rs::fthread::wait(*(rs::shared_pointer<rs_custom_waitter>*)waitter);

    auto result = (*(rs::shared_pointer<rs_custom_waitter>*)waitter)->m_custom_data;
    delete (rs::shared_pointer<rs_custom_waitter>*)waitter;

    return result;
}


rs_bool_t _rs_load_source(rs_vm vm, rs_string_t virtual_src_path, rs_string_t src, size_t stacksz)
{
    // 1. Prepare lexer..
    rs::lexer* lex = nullptr;
    if (src)
        lex = new rs::lexer(rs::str_to_wstr(src), virtual_src_path);
    else
        lex = new rs::lexer(virtual_src_path);

    lex->has_been_imported(rs::str_to_wstr(lex->source_file));

    std::forward_list<rs::grammar::ast_base*> m_last_context;
    bool need_exchange_back = rs::grammar::ast_base::exchange_this_thread_ast(m_last_context);
    if (!lex->has_error())
    {
        // 2. Lexer will create ast_tree;
        auto result = rs::get_rs_grammar()->gen(*lex);
        if (result)
        {
            // 3. Create lang, most anything store here..
            rs::lang lang(*lex);

            lang.analyze_pass1(result);
            lang.analyze_pass_template();
            lang.analyze_pass2(result);

            //result->display();
            if (!lang.has_compile_error())
            {
                rs::ir_compiler compiler;
                lang.analyze_finalize(result, &compiler);

                if (!lang.has_compile_error())
                {
                    compiler.end();
                    ((rs::vm*)vm)->set_runtime(compiler);

                    // OK
                }
            }
        }
    }

    rs::grammar::ast_base::clean_this_thread_ast();

    if (need_exchange_back)
        rs::grammar::ast_base::exchange_this_thread_ast(m_last_context);

    bool compile_has_err = lex->has_error();
    if (compile_has_err || lex->has_warning())
        RS_VM(vm)->compile_info = lex;
    else
        delete lex;

    return !compile_has_err;
}

rs_bool_t rs_has_compile_error(rs_vm vm)
{
    if (vm && RS_VM(vm)->compile_info && RS_VM(vm)->compile_info->has_error())
        return true;
    return false;
}

rs_bool_t rs_has_compile_warning(rs_vm vm)
{
    if (vm && RS_VM(vm)->compile_info && RS_VM(vm)->compile_info->has_warning())
        return true;
    return false;
}

rs_string_t rs_get_compile_error(rs_vm vm, _rs_inform_style style)
{
    if (style == RS_DEFAULT)
        style = rs::config::ENABLE_OUTPUT_ANSI_COLOR_CTRL ? RS_NEED_COLOR : RS_NOTHING;

    thread_local std::string _vm_compile_errors;
    _vm_compile_errors = "";
    if (vm && RS_VM(vm)->compile_info)
    {
        auto& lex = *RS_VM(vm)->compile_info;


        std::string src_file_path = "";
        for (auto& err_info : lex.lex_error_list)
        {
            if (src_file_path != err_info.filename)
            {
                if (style == RS_NEED_COLOR)
                    _vm_compile_errors += ANSI_HIR "In file: '" ANSI_RST + (src_file_path = err_info.filename) + ANSI_HIR "'" ANSI_RST "\n";
                else
                    _vm_compile_errors += "In file: '" + (src_file_path = err_info.filename) + "'\n";
            }
            _vm_compile_errors += rs::wstr_to_str(err_info.to_wstring(style & RS_NEED_COLOR)) + "\n";
        }
        /*src_file_path = "";
        for (auto& war_info : lex.lex_warn_list)
        {
            if (src_file_path != war_info.filename)
                rs::rs_stderr << ANSI_HIY "In file: '" ANSI_RST << (src_file_path = war_info.filename) << ANSI_HIY "'" ANSI_RST << rs::rs_endl;
            rs_wstderr << war_info.to_wstring() << rs::rs_endl;
        }*/
    }
    return _vm_compile_errors.c_str();
}

rs_string_t rs_get_compile_warning(rs_vm vm, _rs_inform_style style)
{
    if (style == RS_DEFAULT)
        style = rs::config::ENABLE_OUTPUT_ANSI_COLOR_CTRL ? RS_NEED_COLOR : RS_NOTHING;

    thread_local std::string _vm_compile_errors;
    _vm_compile_errors = "";
    if (vm && RS_VM(vm)->compile_info)
    {
        auto& lex = *RS_VM(vm)->compile_info;

        std::string src_file_path = "";
        for (auto& war_info : lex.lex_warn_list)
        {
            if (src_file_path != war_info.filename)
            {
                if (style == RS_NEED_COLOR)
                    _vm_compile_errors += ANSI_HIY "In file: '" ANSI_RST + (src_file_path = war_info.filename) + ANSI_HIY "'" ANSI_RST "\n";
                else
                    _vm_compile_errors += "In file: '" + (src_file_path = war_info.filename) + "'\n";
            }
            _vm_compile_errors += rs::wstr_to_str(war_info.to_wstring(style & RS_NEED_COLOR)) + "\n";
        }
        /*src_file_path = "";
        for (auto& war_info : lex.lex_warn_list)
        {
            if (src_file_path != war_info.filename)
                rs::rs_stderr << ANSI_HIY "In file: '" ANSI_RST << (src_file_path = war_info.filename) << ANSI_HIY "'" ANSI_RST << rs::rs_endl;
            rs_wstderr << war_info.to_wstring() << rs::rs_endl;
        }*/
    }
    return _vm_compile_errors.c_str();
}

rs_string_t rs_get_runtime_error(rs_vm vm)
{
    return rs_cast_string(CS_VAL(RS_VM(vm)->er));
}

rs_bool_t rs_abort_vm(rs_vm vm)
{
    std::shared_lock gs(rs::vmbase::_alive_vm_list_mx);

    if (rs::vmbase::_alive_vm_list.find(RS_VM(vm)) != rs::vmbase::_alive_vm_list.end())
    {
        return RS_VM(vm)->interrupt(rs::vmbase::vm_interrupt_type::ABORT_INTERRUPT);
    }
    return false;
}

rs_value rs_push_int(rs_vm vm, rs_int_t val)
{
    return CS_VAL((RS_VM(vm)->sp--)->set_integer(val));
}
rs_value rs_push_real(rs_vm vm, rs_real_t val)
{
    return CS_VAL((RS_VM(vm)->sp--)->set_real(val));
}
rs_value rs_push_handle(rs_vm vm, rs_handle_t val)
{
    return CS_VAL((RS_VM(vm)->sp--)->set_handle(val));
}
rs_value rs_push_pointer(rs_vm vm, rs_ptr_t val)
{
    return CS_VAL((RS_VM(vm)->sp--)->set_handle((rs_handle_t)val));
}
rs_value rs_push_gchandle(rs_vm vm, rs_ptr_t resource_ptr, rs_value holding_val, void(*destruct_func)(rs_ptr_t))
{
    auto* csp = RS_VM(vm)->sp--;

    csp->set_gcunit_with_barrier(rs::value::valuetype::gchandle_type);
    auto handle_ptr = rs::gchandle_t::gc_new<rs::gcbase::gctype::eden>(csp->gcunit);
    handle_ptr->holding_handle = resource_ptr;
    if (holding_val)
    {
        handle_ptr->holding_value.set_val(RS_VAL(holding_val));
        if (handle_ptr->holding_value.is_gcunit())
            handle_ptr->holding_value.gcunit->gc_type = rs::gcbase::gctype::no_gc;
    }
    handle_ptr->destructor = destruct_func;

    return CS_VAL(csp);
}
rs_value rs_push_string(rs_vm vm, rs_string_t val)
{
    return CS_VAL((RS_VM(vm)->sp--)->set_string(val));
}
rs_value rs_push_nil(rs_vm vm)
{
    return CS_VAL((RS_VM(vm)->sp--)->set_nil());
}
rs_value rs_push_val(rs_vm vm, rs_value val)
{
    if (val)
        return CS_VAL((RS_VM(vm)->sp--)->set_val(RS_VAL(val)));
    return CS_VAL((RS_VM(vm)->sp--)->set_nil());
}
rs_value rs_push_ref(rs_vm vm, rs_value val)
{
    if (val)
        return CS_VAL((RS_VM(vm)->sp--)->set_ref(RS_VAL(val)));
    return CS_VAL((RS_VM(vm)->sp--)->set_nil());
}
rs_value rs_push_valref(rs_vm vm, rs_value val)
{
    if (val)
        return CS_VAL((RS_VM(vm)->sp--)->set_trans(RS_ORIGIN_VAL(val)));
    return CS_VAL((RS_VM(vm)->sp--)->set_nil());
}


rs_value rs_top_stack(rs_vm vm)
{
    return CS_VAL((RS_VM(vm)->sp - 1));
}
void rs_pop_stack(rs_vm vm)
{
    ++RS_VM(vm)->sp;
}
rs_value rs_invoke_rsfunc(rs_vm vm, rs_int_t vmfunc, rs_int_t argc)
{
    return CS_VAL(RS_VM(vm)->invoke(vmfunc, argc));
}
rs_value rs_invoke_exfunc(rs_vm vm, rs_handle_t exfunc, rs_int_t argc)
{
    return CS_VAL(RS_VM(vm)->invoke(exfunc, argc));
}
rs_value rs_invoke_value(rs_vm vm, rs_value vmfunc, rs_int_t argc)
{
    if (!vmfunc)
        rs_fail(RS_FAIL_CALL_FAIL, "Cannot call a 'nil' function.");
    else if (RS_VAL(vmfunc)->type == rs::value::valuetype::integer_type)
        return CS_VAL(RS_VM(vm)->invoke(RS_VAL(vmfunc)->integer, argc));
    else if (RS_VAL(vmfunc)->type == rs::value::valuetype::handle_type)
        return CS_VAL(RS_VM(vm)->invoke(RS_VAL(vmfunc)->handle, argc));
    else
        rs_fail(RS_FAIL_CALL_FAIL, "Not callable type.");
    return nullptr;
}

rs_value rs_dispatch_rsfunc(rs_vm vm, rs_int_t vmfunc, rs_int_t argc)
{
    auto* vmm = RS_VM(vm);
    vmm->set_br_yieldable(true);
    return CS_VAL(vmm->co_pre_invoke(vmfunc, argc));
}

rs_value rs_dispatch(rs_vm vm)
{
    if (RS_VM(vm)->env)
    {
        RS_VM(vm)->run();
        
        if (RS_VM(vm)->veh)
        {
            if (RS_VM(vm)->get_and_clear_br_yield_flag())
                return RS_CONTINUE;

            return reinterpret_cast<rs_value>(RS_VM(vm)->cr);
        }
        else
            return nullptr;
    }
    return nullptr;
}

void rs_break_yield(rs_vm vm)
{
    RS_VM(vm)->interrupt(rs::vmbase::BR_YIELD_INTERRUPT);
}

rs_bool_t rs_load_source_with_stacksz(rs_vm vm, rs_string_t virtual_src_path, rs_string_t src, size_t stacksz)
{
    if (!virtual_src_path)
        virtual_src_path = "__runtime_script__";

    rs_virtual_source(virtual_src_path, src, true);

    return _rs_load_source(vm, virtual_src_path, src, stacksz);
}

rs_bool_t rs_load_file_with_stacksz(rs_vm vm, rs_string_t virtual_src_path, size_t stacksz)
{
    return _rs_load_source(vm, virtual_src_path, nullptr, stacksz);
}

rs_bool_t rs_load_source(rs_vm vm, rs_string_t virtual_src_path, rs_string_t src)
{
    return rs_load_source_with_stacksz(vm, virtual_src_path, src, 0);
}

rs_bool_t rs_load_file(rs_vm vm, rs_string_t virtual_src_path)
{
    return rs_load_file_with_stacksz(vm, virtual_src_path, 0);
}

rs_value rs_run(rs_vm vm)
{
    if (RS_VM(vm)->env)
    {
        RS_VM(vm)->ip = RS_VM(vm)->env->rt_codes;
        RS_VM(vm)->run();
        if (RS_VM(vm)->veh)
            return reinterpret_cast<rs_value>(RS_VM(vm)->cr);
        else
            return nullptr;
    }
    return nullptr;
}

// CONTAINER OPERATE

void rs_arr_resize(rs_value arr, rs_int_t newsz, rs_value init_val)
{
    auto _arr = RS_VAL(arr);

    if (_arr->is_nil())
        rs_fail(RS_FAIL_TYPE_FAIL, "Value is 'nil'.");
    else if (_arr->type == rs::value::valuetype::array_type)
    {
        rs::gcbase::gc_write_guard g1(_arr->array);
        size_t arrsz = _arr->array->size();
        if ((size_t)newsz < arrsz && rs::gc::gc_is_marking())
        {
            for (size_t i = newsz; i < arrsz; ++i)
                _arr->array->add_memo(&(*_arr->array)[i]);
        }
        _arr->array->resize((size_t)newsz, *RS_VAL(init_val));
    }
    else
        rs_fail(RS_FAIL_TYPE_FAIL, "Value is not an array.");
}

rs_value rs_arr_add(rs_value arr, rs_value elem)
{
    auto _arr = RS_VAL(arr);

    if (_arr->is_nil())
        rs_fail(RS_FAIL_TYPE_FAIL, "Value is 'nil'.");
    else if (_arr->type == rs::value::valuetype::array_type)
    {
        rs::gcbase::gc_write_guard g1(_arr->array);

        if (elem)
            _arr->array->push_back(*RS_VAL(elem));
        else
            _arr->array->emplace_back(rs::value());

        return reinterpret_cast<rs_value>(&_arr->array->back());
    }
    else
        rs_fail(RS_FAIL_TYPE_FAIL, "Value is not an array.");

    return nullptr;
}

rs_value rs_arr_get(rs_value arr, rs_int_t index)
{
    auto _arr = RS_VAL(arr);
    if (_arr->is_nil())
        rs_fail(RS_FAIL_TYPE_FAIL, "Value is 'nil'.");
    else if (_arr->type == rs::value::valuetype::array_type)
    {
        rs::gcbase::gc_read_guard g1(_arr->array);

        if ((size_t)index <= _arr->array->size())
            return CS_VAL(&(*_arr->array)[index]);
        else
            rs_fail(RS_FAIL_INDEX_FAIL, "Index out of range.");

    }
    else
        rs_fail(RS_FAIL_TYPE_FAIL, "Value is not an array.");

    return nullptr;
}
rs_int_t rs_arr_find(rs_value arr, rs_value elem)
{
    auto _arr = RS_VAL(arr);
    auto _aim = RS_VAL(elem);
    if (_arr->is_nil())
        rs_fail(RS_FAIL_TYPE_FAIL, "Value is 'nil'.");
    else if (_arr->type == rs::value::valuetype::array_type)
    {
        rs::gcbase::gc_read_guard g1(_arr->array);

        auto fnd = std::find_if(_arr->array->begin(), _arr->array->end(),
            [&](const rs::value& _elem)->bool
            {
                return _elem.type == _aim->type
                    && _elem.handle == _aim->handle;
            });
        if (fnd != _arr->array->end())
            return fnd - _arr->array->begin();
        return -1;

    }
    else
        rs_fail(RS_FAIL_TYPE_FAIL, "Value is not an array.");

    return -1;
}
void rs_arr_remove(rs_value arr, rs_int_t index)
{
    auto _arr = RS_VAL(arr);
    if (_arr->is_nil())
        rs_fail(RS_FAIL_TYPE_FAIL, "Value is 'nil'.");
    else if (_arr->type == rs::value::valuetype::array_type)
    {
        rs::gcbase::gc_write_guard g1(_arr->array);

        if (index < 0)
            ;// do nothing..
        else if ((size_t)index <= _arr->array->size())
        {
            if (rs::gc::gc_is_marking())
                _arr->array->add_memo(&(*_arr->array)[index]);
            _arr->array->erase(_arr->array->begin() + index);
        }
        else
            rs_fail(RS_FAIL_INDEX_FAIL, "Index out of range.");
    }
    else
        rs_fail(RS_FAIL_TYPE_FAIL, "Value is not an array.");
}
void rs_arr_clear(rs_value arr)
{
    auto _arr = RS_VAL(arr);
    if (_arr->is_nil())
        rs_fail(RS_FAIL_TYPE_FAIL, "Value is 'nil'.");
    else if (_arr->type == rs::value::valuetype::array_type)
    {
        rs::gcbase::gc_write_guard g1(_arr->array);
        if (rs::gc::gc_is_marking())
            for (auto& val : *_arr->array)
                _arr->array->add_memo(&val);
        _arr->array->clear();
    }
    else
        rs_fail(RS_FAIL_TYPE_FAIL, "Value is not an array.");
}

rs_bool_t rs_map_find(rs_value map, rs_value index)
{
    auto _map = RS_VAL(map);
    if (_map->is_nil())
        rs_fail(RS_FAIL_TYPE_FAIL, "Value is 'nil'.");
    else if (_map->type == rs::value::valuetype::mapping_type)
    {
        rs::gcbase::gc_read_guard g1(_map->mapping);
        if (index)
            return _map->mapping->find(*RS_VAL(index)) != _map->mapping->end();
        return  _map->mapping->find(rs::value()) != _map->mapping->end();
    }
    else
        rs_fail(RS_FAIL_TYPE_FAIL, "Value is not a map.");

    return false;
}

rs_value rs_map_get_by_default(rs_value map, rs_value index, rs_value default_value)
{
    auto _map = RS_VAL(map);
    if (_map->is_nil())
        rs_fail(RS_FAIL_TYPE_FAIL, "Value is 'nil'.");
    else if (_map->type == rs::value::valuetype::mapping_type)
    {
        rs::value* result = nullptr;
        rs::gcbase::gc_write_guard g1(_map->mapping);
        do
        {
            auto fnd = _map->mapping->find(*RS_VAL(index));
            if (fnd != _map->mapping->end())
                result = &fnd->second;
        } while (false);
        if (!result)
        {
            if (default_value)
                result = &((*_map->mapping)[*RS_VAL(index)] = *RS_VAL(default_value));
            else
            {
                result = &((*_map->mapping)[*RS_VAL(index)]);
                result->set_nil();
            }
        }
        if (rs::gc::gc_is_marking())
            _map->mapping->add_memo(result);

        return CS_VAL(result);
    }
    else
        rs_fail(RS_FAIL_TYPE_FAIL, "Value is not a map.");

    return nullptr;
}

rs_value rs_map_get(rs_value map, rs_value index)
{
    auto _map = RS_VAL(map);
    if (_map->is_nil())
        rs_fail(RS_FAIL_TYPE_FAIL, "Value is 'nil'.");
    else if (_map->type == rs::value::valuetype::mapping_type)
    {
        rs::gcbase::gc_read_guard g1(_map->mapping);
        auto fnd = _map->mapping->find(*RS_VAL(index));
        if (fnd != _map->mapping->end())
        {
            if (rs::gc::gc_is_marking())
                _map->mapping->add_memo(&fnd->second);
            return CS_VAL(&fnd->second);
        }
        return nullptr;
    }
    else
        rs_fail(RS_FAIL_TYPE_FAIL, "Value is not a map.");

    return nullptr;
}

rs_bool_t rs_map_remove(rs_value map, rs_value index)
{
    auto _map = RS_VAL(map);
    if (_map->is_nil())
        rs_fail(RS_FAIL_TYPE_FAIL, "Value is 'nil'.");
    else if (_map->type == rs::value::valuetype::mapping_type)
    {
        rs::gcbase::gc_write_guard g1(_map->mapping);
        if (rs::gc::gc_is_marking())
        {
            auto fnd = _map->mapping->find(*RS_VAL(index));
            if (fnd != _map->mapping->end())
            {
                _map->mapping->add_memo(&fnd->first);
                _map->mapping->add_memo(&fnd->second);
            }
        }
        return 0 != _map->mapping->erase(*RS_VAL(index));
    }
    else
        rs_fail(RS_FAIL_TYPE_FAIL, "Value is not a map.");

    return false;
}
void rs_map_clear(rs_value map)
{
    auto _map = RS_VAL(map);
    if (_map->is_nil())
        rs_fail(RS_FAIL_TYPE_FAIL, "Value is 'nil'.");
    else if (_map->type == rs::value::valuetype::mapping_type)
    {
        rs::gcbase::gc_write_guard g1(_map->mapping);
        if (rs::gc::gc_is_marking())
        {
            for (auto& kvpair : *_map->mapping)
            {
                _map->mapping->add_memo(&kvpair.first);
                _map->mapping->add_memo(&kvpair.second);
            }
        }
        _map->mapping->clear();
    }
    else
        rs_fail(RS_FAIL_TYPE_FAIL, "Value is not a map.");
}


rs_bool_t rs_gchandle_close(rs_value gchandle)
{
    if (RS_VAL(gchandle)->gchandle)
        return RS_VAL(gchandle)->gchandle->close();
    return false;
}

// DEBUGGEE TOOLS
void rs_attach_default_debuggee(rs_vm vm)
{
    rs::default_debuggee* dgb = new rs::default_debuggee;
    if (auto* old_debuggee = RS_VM(vm)->attach_debuggee(dgb))
        delete old_debuggee;
}

rs_bool_t rs_has_attached_debuggee(rs_vm vm)
{
    if (RS_VM(vm)->current_debuggee())
        return true;
    return false;
}

void rs_disattach_debuggee(rs_vm vm)
{
    RS_VM(vm)->attach_debuggee(nullptr);
}

void rs_disattach_and_free_debuggee(rs_vm vm)
{
    if (auto* dbg = RS_VM(vm)->attach_debuggee(nullptr))
        delete dbg;
}

void rs_break_immediately(rs_vm vm)
{
    if (auto* debuggee = dynamic_cast<rs::default_debuggee*>(RS_VM(vm)->current_debuggee()))
        debuggee->breakdown_immediately();
    else
        rs_fail(RS_FAIL_DEBUGGEE_FAIL, "'rs_break_immediately' can only break the vm attached default debuggee.");

}

rs_integer_t rs_extern_symb(rs_vm vm, rs_string_t fullname)
{
    const auto& extern_table = RS_VM(vm)->env->program_debug_info->extern_function_map;
    auto fnd = extern_table.find(fullname);
    if (fnd != extern_table.end())
        return fnd->second;
    return 0;
}

rs_string_t rs_debug_trace_callstack(rs_vm vm, size_t layer)
{
    std::stringstream sstream;
    RS_VM(vm)->dump_call_stack(layer, false, sstream);

    rs_set_string(CS_VAL(RS_VM(vm)->er), "");
    rs_assert(RS_VM(vm)->er->type == rs::value::valuetype::string_type);

    *(RS_VM(vm)->er->string) = sstream.str();
    return RS_VM(vm)->er->string->c_str();
}