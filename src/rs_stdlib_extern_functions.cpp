#define _CRT_SECURE_NO_WARNINGS
#include "rs_lang_extern_symbol_loader.hpp"
#include "rs_utf8.hpp"
#include "rs_vm.hpp"
#include "rs_roroutine_simulate_mgr.hpp"
#include "rs_roroutine_thread_mgr.hpp"
#include "rs_io.hpp"
#include "rs_exceptions.hpp"

#include <chrono>
#include <random>
#include <thread>

RS_API rs_api rslib_std_print(rs_vm vm, rs_value args, size_t argc)
{
    for (size_t i = 0; i < argc; i++)
    {
        rs::rs_stdout << rs_cast_string(args + i);

        if (i + 1 < argc)
            rs::rs_stdout << " ";
    }
    return rs_ret_int(vm, argc);
}
RS_API rs_api rslib_std_panic(rs_vm vm, rs_value args, size_t argc)
{
    rs_fail(RS_FAIL_DEADLY, rs_string(args + 0));
    return rs_ret_nil(vm);
}
RS_API rs_api rslib_std_halt(rs_vm vm, rs_value args, size_t argc)
{
    throw rs::rsruntime_exception(RS_FAIL_HEAVY, rs_string(args + 0));
    return rs_ret_nil(vm);
}
RS_API rs_api rslib_std_throw(rs_vm vm, rs_value args, size_t argc)
{
    throw rs::rsruntime_exception(RS_FAIL_MEDIUM, rs_string(args + 0));
    return rs_ret_nil(vm);
}
RS_API rs_api rslib_std_fail(rs_vm vm, rs_value args, size_t argc)
{
    rs_fail(RS_FAIL_MEDIUM, rs_string(args + 0));
    return rs_ret_nil(vm);
}
RS_API rs_api rslib_std_lengthof(rs_vm vm, rs_value args, size_t argc)
{
    return rs_ret_int(vm, rs_lengthof(args));
}

RS_API rs_api rslib_std_string_toupper(rs_vm vm, rs_value args, size_t argc)
{
    std::string str = rs_string(args + 0);
    for (auto& ch : str)
        ch = (char)toupper((int)(unsigned char)ch);
    return rs_ret_string(vm, str.c_str());
}

RS_API rs_api rslib_std_string_tolower(rs_vm vm, rs_value args, size_t argc)
{
    std::string str = rs_string(args + 0);
    for (auto& ch : str)
        ch = (char)tolower((int)(unsigned char)ch);
    return rs_ret_string(vm, str.c_str());
}

RS_API rs_api rslib_std_string_isspace(rs_vm vm, rs_value args, size_t argc)
{
    rs_string_t str = rs_string(args + 0);

    if (*str)
    {
        auto&& wstr = rs::str_to_wstr(str);
        for (auto& wch : wstr)
            if (!rs::lexer::lex_isspace(wch))
                return rs_ret_bool(vm, false);
        return rs_ret_bool(vm, true);
    }
    return rs_ret_bool(vm, false);
}

RS_API rs_api rslib_std_string_isalpha(rs_vm vm, rs_value args, size_t argc)
{
    rs_string_t str = rs_string(args + 0);

    if (*str)
    {
        auto&& wstr = rs::str_to_wstr(str);
        for (auto& wch : wstr)
            if (!rs::lexer::lex_isalpha(wch))
                return rs_ret_bool(vm, false);
        return rs_ret_bool(vm, true);
    }
    return rs_ret_bool(vm, false);
}

RS_API rs_api rslib_std_string_isalnum(rs_vm vm, rs_value args, size_t argc)
{
    rs_string_t str = rs_string(args + 0);

    if (*str)
    {
        auto&& wstr = rs::str_to_wstr(str);
        for (auto& wch : wstr)
            if (!rs::lexer::lex_isalnum(wch))
                return rs_ret_bool(vm, false);
        return rs_ret_bool(vm, true);
    }
    return rs_ret_bool(vm, false);
}

RS_API rs_api rslib_std_string_isnumber(rs_vm vm, rs_value args, size_t argc)
{
    rs_string_t str = rs_string(args + 0);

    if (*str)
    {
        auto&& wstr = rs::str_to_wstr(str);
        for (auto& wch : wstr)
            if (!rs::lexer::lex_isdigit(wch))
                return rs_ret_bool(vm, false);
        return rs_ret_bool(vm, true);
    }
    return rs_ret_bool(vm, false);
}

RS_API rs_api rslib_std_string_ishex(rs_vm vm, rs_value args, size_t argc)
{
    rs_string_t str = rs_string(args + 0);

    if (*str)
    {
        auto&& wstr = rs::str_to_wstr(str);
        for (auto& wch : wstr)
            if (!rs::lexer::lex_isxdigit(wch))
                return rs_ret_bool(vm, false);
        return rs_ret_bool(vm, true);
    }
    return rs_ret_bool(vm, false);
}

RS_API rs_api rslib_std_string_isoct(rs_vm vm, rs_value args, size_t argc)
{
    rs_string_t str = rs_string(args + 0);

    if (*str)
    {
        auto&& wstr = rs::str_to_wstr(str);
        for (auto& wch : wstr)
            if (!rs::lexer::lex_isodigit(wch))
                return rs_ret_bool(vm, false);
        return rs_ret_bool(vm, true);
    }
    return rs_ret_bool(vm, false);
}

RS_API rs_api rslib_std_string_enstring(rs_vm vm, rs_value args, size_t argc)
{
    rs_string_t str = rs_string(args + 0);
    std::string result;
    while (*str)
    {
        unsigned char uch = *str;
        if (iscntrl(uch))
        {
            char encode[5] = {};
            sprintf(encode, "\\x%02x", (unsigned int)uch);

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
    result = "\"" + result + "\"";
    return rs_ret_string(vm, result.c_str());
}

RS_API rs_api rslib_std_string_destring(rs_vm vm, rs_value args, size_t argc)
{
    rs_string_t str = rs_string(args + 0);
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
                result += '\t'; break;
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
    return rs_ret_string(vm, result.c_str());
}

RS_API rs_api rslib_std_string_beginwith(rs_vm vm, rs_value args, size_t argc)
{
    rs_string_t aim = rs_string(args + 0);
    rs_string_t begin = rs_string(args + 1);

    while ((*aim) && (*begin))
    {
        if (*aim != *begin)
            return rs_ret_bool(vm, false);
        ++aim;
        ++begin;
    }

    return rs_ret_bool(vm, true);
}

RS_API rs_api rslib_std_string_endwith(rs_vm vm, rs_value args, size_t argc)
{
    rs_string_t aim = rs_string(args + 0);
    rs_string_t end = rs_string(args + 1);

    size_t aimlen = strlen(aim);
    size_t endlen = strlen(end);

    if (endlen > aimlen)
        return rs_ret_bool(vm, false);

    aim += (aimlen - endlen);
    while ((*aim) && (*end))
    {
        if (*aim != *end)
            return rs_ret_bool(vm, false);
        ++aim;
        ++end;
    }
    return rs_ret_bool(vm, true);
}

RS_API rs_api rslib_std_string_replace(rs_vm vm, rs_value args, size_t argc)
{
    std::string aim = rs_string(args + 0);
    rs_string_t match = rs_string(args + 1);
    rs_string_t replace = rs_string(args + 2);

    size_t matchlen = strlen(match);
    size_t replacelen = strlen(replace);
    size_t replace_begin = 0;
    do
    {
        size_t fnd_place = aim.find(match, replace_begin);
        if (fnd_place< replace_begin || fnd_place>aim.size())
            break;

        aim.replace(fnd_place, matchlen, replace);
        replace_begin += replacelen;

    } while (true);

    return rs_ret_string(vm, aim.c_str());
}

RS_API rs_api rslib_std_string_trim(rs_vm vm, rs_value args, size_t argc)
{
    std::string aim = rs_string(args + 0);

    size_t ibeg = 0;
    size_t iend = aim.size();
    for (; ibeg != iend; ibeg++)
    {
        auto uch = (unsigned char)aim[ibeg];
        if (isspace(uch) || iscntrl(uch))
            continue;
        break;
    }

    for (; iend != ibeg; iend--)
    {
        auto uch = (unsigned char)aim[iend - 1];
        if (isspace(uch) || iscntrl(uch))
            continue;
        break;
    }

    return rs_ret_string(vm, aim.substr(ibeg, iend - ibeg).c_str());
}

RS_API rs_api rslib_std_string_split(rs_vm vm, rs_value args, size_t argc)
{
    std::string aim = rs_string(args + 0);
    rs_string_t match = rs_string(args + 1);
    rs_value arr = args + 2;

    size_t matchlen = strlen(match);
    size_t split_begin = 0;

    while (true)
    {
        size_t fnd_place = aim.find(match, split_begin);
        if (fnd_place< split_begin || fnd_place>aim.size())
        {
            rs_value v = rs_arr_add(arr, nullptr);
            rs_set_string(v, aim.substr(split_begin).c_str());
            break;
        }
        rs_value v = rs_arr_add(arr, nullptr);
        rs_set_string(v, aim.substr(split_begin, fnd_place - split_begin).c_str());

        split_begin = fnd_place + matchlen;
    }
    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_time_sec(rs_vm vm, rs_value args, size_t argc)
{
    static std::chrono::system_clock _sys_clock;
    static auto _first_invoke_time = _sys_clock.now();

    auto _time_ms = rs_real_t((_sys_clock.now() - _first_invoke_time).count() * std::chrono::system_clock::period::num)
        / std::chrono::system_clock::period::den;
    return rs_ret_real(vm, _time_ms);
}

RS_API rs_api rslib_std_atomic_cas(rs_vm vm, rs_value args, size_t argc)
{
    rs::value* aim = reinterpret_cast<rs::value*>(args + 0)->get();
    rs::value* excepted = reinterpret_cast<rs::value*>(args + 1)->get();
    rs::value* swapval = reinterpret_cast<rs::value*>(args + 2)->get();

    rs_assert(aim->type == excepted->type && excepted->type == swapval->type);

    return rs_ret_bool(vm, ((std::atomic<rs_handle_t>*) & aim->handle)->compare_exchange_weak(excepted->handle, swapval->handle));
}

RS_API rs_api rslib_std_randomint(rs_vm vm, rs_value args, size_t argc)
{
    static std::random_device rd;
    static std::mt19937_64 mt64(rd());

    rs_int_t from = rs_int(args + 0);
    rs_int_t to = rs_int(args + 1);

    if (to < from)
        std::swap(from, to);

    std::uniform_int_distribution<rs_int_t> dis(from, to);
    return rs_ret_int(vm, dis(mt64));
}

RS_API rs_api rslib_std_randomreal(rs_vm vm, rs_value args)
{
    static std::random_device rd;
    static std::mt19937_64 mt64(rd());

    rs_real_t from = rs_real(args + 0);
    rs_real_t to = rs_real(args + 1);

    if (to < from)
        std::swap(from, to);

    std::uniform_real_distribution<rs_real_t> dis(from, to);
    return rs_ret_real(vm, dis(mt64));
}

RS_API rs_api rslib_std_break_yield(rs_vm vm, rs_value args, size_t argc)
{
    rs_break_yield(vm);
    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_array_resize(rs_vm vm, rs_value args, size_t argc)
{
    rs_arr_resize(args + 0, rs_int(args + 1), args + 2);
    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_array_add(rs_vm vm, rs_value args, size_t argc)
{
    return rs_ret_ref(vm, rs_arr_add(args + 0, args + 1));
}

RS_API rs_api rslib_std_array_remove(rs_vm vm, rs_value args, size_t argc)
{
    rs_arr_remove(args + 0, rs_int(args + 1));

    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_array_find(rs_vm vm, rs_value args, size_t argc)
{
    return rs_ret_int(vm, rs_arr_find(args + 0, args + 1));
}

RS_API rs_api rslib_std_array_clear(rs_vm vm, rs_value args, size_t argc)
{
    rs_arr_clear(args);

    return rs_ret_nil(vm);
}

struct array_iter
{
    using array_iter_t = decltype(std::declval<rs::array_t>().begin());

    array_iter_t iter;
    array_iter_t end_place;
    rs_int_t     index_count;
};

RS_API rs_api rslib_std_array_iter(rs_vm vm, rs_value args, size_t argc)
{
    rs::value* arr = reinterpret_cast<rs::value*>(args)->get();

    if (arr->is_nil())
    {
        rs_fail(RS_FAIL_TYPE_FAIL, "Value is 'nil'.");
        return rs_ret_nil(vm);
    }
    else
    {
        return rs_ret_gchandle(vm,
            new array_iter{ arr->array->begin(), arr->array->end(), 0 },
            args + 0,
            [](void* array_iter_t_ptr)
            {
                delete (array_iter*)array_iter_t_ptr;
            }
        );
    }
}

RS_API rs_api rslib_std_array_iter_next(rs_vm vm, rs_value args, size_t argc)
{
    array_iter& iter = *(array_iter*)rs_pointer(args);

    if (iter.iter == iter.end_place)
        return rs_ret_bool(vm, false);

    rs_set_int(args + 1, iter.index_count++); // key
    rs_set_val(args + 2, reinterpret_cast<rs_value>(&*(iter.iter++))); // val

    return rs_ret_bool(vm, true);
}

RS_API rs_api rslib_std_map_find(rs_vm vm, rs_value args, size_t argc)
{
    return rs_ret_bool(vm, rs_map_find(args + 0, args + 1));
}

RS_API rs_api rslib_std_map_only_get(rs_vm vm, rs_value args, size_t argc)
{
    rs_value result = rs_map_get(args + 0, args + 1);

    if (result)
        return rs_ret_ref(vm, result);

    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_map_contain(rs_vm vm, rs_value args, size_t argc)
{
    bool _map_has_indexed_val = rs_map_find(args + 0, args + 1);

    return rs_ret_int(vm, _map_has_indexed_val ? 1 : 0);
}

RS_API rs_api rslib_std_map_get_by_default(rs_vm vm, rs_value args, size_t argc)
{
    return rs_ret_ref(vm, rs_map_get_by_default(args + 0, args + 1, args + 2));
}

RS_API rs_api rslib_std_map_remove(rs_vm vm, rs_value args, size_t argc)
{
    return rs_ret_bool(vm, rs_map_remove(args + 0, args + 1));
}

RS_API rs_api rslib_std_map_clear(rs_vm vm, rs_value args, size_t argc)
{
    rs_map_clear(args + 0);
    return rs_ret_nil(vm);
}

struct map_iter
{
    using mapping_iter_t = decltype(std::declval<rs::mapping_t>().begin());

    mapping_iter_t iter;
    mapping_iter_t end_place;
};

RS_API rs_api rslib_std_map_iter(rs_vm vm, rs_value args, size_t argc)
{
    rs::value* mapp = reinterpret_cast<rs::value*>(args)->get();

    if (mapp->is_nil())
    {
        rs_fail(RS_FAIL_TYPE_FAIL, "Value is 'nil'.");
        return rs_ret_nil(vm);
    }
    else
    {
        return rs_ret_gchandle(vm,
            new map_iter{ mapp->mapping->begin(), mapp->mapping->end() },
            args + 0,
            [](void* array_iter_t_ptr)
            {
                delete (map_iter*)array_iter_t_ptr;
            }
        );
    }
}

RS_API rs_api rslib_std_map_iter_next(rs_vm vm, rs_value args, size_t argc)
{
    map_iter& iter = *(map_iter*)rs_pointer(args);

    if (iter.iter == iter.end_place)
        return rs_ret_bool(vm, false);

    rs_set_val(args + 1, reinterpret_cast<rs_value>(const_cast<rs::value*>(&iter.iter->first))); // key
    rs_set_val(args + 2, reinterpret_cast<rs_value>(&iter.iter->second)); // val
    iter.iter++;

    return rs_ret_bool(vm, true);
}

RS_API rs_api rslib_std_sub(rs_vm vm, rs_value args, size_t argc)
{
    if (rs_valuetype(args + 0) == RS_STRING_TYPE)
    {
        // return substr
        size_t sub_str_len = 0;
        if (argc == 2)
        {
            auto* substring = rs::u8substr(rs_string(args + 0), rs_int(args + 1), rs::u8str_npos, &sub_str_len);
            return rs_ret_string(vm, std::string(substring, sub_str_len).c_str());
        }
        auto* substring = rs::u8substr(rs_string(args + 0), rs_int(args + 1), rs_int(args + 2), &sub_str_len);
        return rs_ret_string(vm, std::string(substring, sub_str_len).c_str());
    }

    //return rs_ret_ref(vm, mapping_indexed);
    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_thread_sleep(rs_vm vm, rs_value args, size_t argc)
{
    using namespace std;

    std::this_thread::sleep_for(rs_real(args) * 1s);
    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_vm_create(rs_vm vm, rs_value args, size_t argc)
{
    return rs_ret_gchandle(vm,
        rs_create_vm(),
        nullptr,
        [](void* vm_ptr) {
            rs_close_vm((rs_vm)vm_ptr);
        });
}

RS_API rs_api rslib_std_vm_load_src(rs_vm vm, rs_value args, size_t argc)
{
    rs_vm vmm = (rs_vm)rs_pointer(args);

    bool compile_result;
    if (argc < 3)
        compile_result = rs_load_source(vmm, "_temp_source.rsn", rs_string(args + 1));
    else
        compile_result = rs_load_source(vmm, rs_string(args + 1), rs_string(args + 2));

    return rs_ret_bool(vm, compile_result);
}

RS_API rs_api rslib_std_vm_load_file(rs_vm vm, rs_value args, size_t argc)
{
    rs_vm vmm = (rs_vm)rs_pointer(args);
    bool compile_result = rs_load_file(vmm, rs_string(args + 1));
    return rs_ret_bool(vm, compile_result);
}

RS_API rs_api rslib_std_vm_run(rs_vm vm, rs_value args, size_t argc)
{
    rs_vm vmm = (rs_vm)rs_pointer(args);
    rs_value ret = rs_run(vmm);

    return rs_ret_val(vm, ret);
}

RS_API rs_api rslib_std_vm_has_compile_error(rs_vm vm, rs_value args, size_t argc)
{
    rs_vm vmm = (rs_vm)rs_pointer(args);
    return rs_ret_bool(vm, rs_has_compile_error(vmm));
}

RS_API rs_api rslib_std_vm_has_compile_warning(rs_vm vm, rs_value args, size_t argc)
{
    rs_vm vmm = (rs_vm)rs_pointer(args);
    return rs_ret_bool(vm, rs_has_compile_warning(vmm));
}

RS_API rs_api rslib_std_vm_get_compile_error(rs_vm vm, rs_value args, size_t argc)
{
    rs_vm vmm = (rs_vm)rs_pointer(args);
    _rs_inform_style style = argc > 1 ? (_rs_inform_style)rs_int(args + 1) : RS_DEFAULT;

    return rs_ret_string(vm, rs_get_compile_error(vmm, style));
}

RS_API rs_api rslib_std_vm_get_compile_warning(rs_vm vm, rs_value args, size_t argc)
{
    rs_vm vmm = (rs_vm)rs_pointer(args);
    _rs_inform_style style = argc > 1 ? (_rs_inform_style)rs_int(args + 1) : RS_DEFAULT;

    return rs_ret_string(vm, rs_get_compile_warning(vmm, style));
}

RS_API rs_api rslib_std_vm_virtual_source(rs_vm vm, rs_value args, size_t argc)
{
    return rs_ret_bool(vm, rs_virtual_source(
        rs_string(args + 0),
        rs_string(args + 1),
        rs_int(args + 2)
    ));
}

RS_API rs_api rslib_std_gchandle_close(rs_vm vm, rs_value args, size_t argc)
{
    return rs_gchandle_close(args);
}

RS_API rs_api rslib_std_thread_yield(rs_vm vm, rs_value args, size_t argc)
{
    rs_co_yield();
    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_get_exe_path(rs_vm vm, rs_value args, size_t argc)
{
    return rs_ret_string(vm, rs::exe_path());
}

RS_API rs_api rslib_std_get_extern_symb(rs_vm vm, rs_value args, size_t argc)
{
    return rs_ret_int(vm, rs_extern_symb(vm, rs_string(args + 0)));
}

const char* rs_stdlib_src_path = u8"rscene/std.rsn";
const char* rs_stdlib_src_data = {
u8R"(
const var true = 1 : bool;
const var false = 0 : bool;
namespace std
{
    extern("rslib_std_throw") func throw(var msg:string):void;
    extern("rslib_std_fail") func fail(var msg:string):void;
    extern("rslib_std_halt") func halt(var msg:string):void;
    extern("rslib_std_panic") func panic(var msg:string):void;

    extern("rslib_std_print") func print(...):int;
    extern("rslib_std_time_sec") func time():real;

    extern("rslib_std_atomic_cas") 
        func atomic_cas<T>(ref val:T, ref excepted:T, var swapval:T):T;

    func println(...)
    {
        var c = print((...)...);
        print("\n");
        return c;
    }

    extern("rslib_std_randomint") 
        func rand(var from:int, var to:int):int;

    extern("rslib_std_randomreal") 
        func rand(var from:real, var to:real):real;

    extern("rslib_std_break_yield") 
        func break_yield():void;

    extern("rslib_std_thread_sleep")
        func sleep(var tm:real):void;
   
    extern("rslib_std_get_exe_path")
        func exepath():string;

    extern("rslib_std_get_extern_symb")
        func extern_symbol<T>(var fullname:string):T;

    func max<T>(var a:T, var b:T) : bool
    {
        if (a >= b)
            return a;
        return b;
    }

    func min<T>(var a:T, var b:T) : bool
    {
        if (a <= b)
            return a;
        return b;
    }
}

namespace string
{
    extern("rslib_std_lengthof") 
        func len(var val:string):int;

    extern("rslib_std_sub")
        func sub(var val:string, var begin:int):string;

    extern("rslib_std_sub")
        func sub(var val:string, var begin:int, var length:int):string;
    
    extern("rslib_std_string_toupper")
        func upper(var val:string):string;

    extern("rslib_std_string_tolower")
        func lower(var val:string):string;

    extern("rslib_std_string_isspace")
        func isspace(var val:string):bool;

    extern("rslib_std_string_isalpha")
        func isalpha(var val:string):bool;

    extern("rslib_std_string_isalnum")
        func isalnum(var val:string):bool;

    extern("rslib_std_string_isnumber")
        func isnumber(var val:string):bool;

    extern("rslib_std_string_ishex")
        func ishex(var val:string):bool;

    extern("rslib_std_string_isoct")
        func isoct(var val:string):bool;

    extern("rslib_std_string_enstring")
        func enstring(var val:string):string;

    extern("rslib_std_string_destring")
        func destring(var val:string):string;

    extern("rslib_std_string_beginwith")
        func beginwith(var val:string, var str:string):bool;

    extern("rslib_std_string_endwith")
        func endwith(var val:string, var str:string):bool;

    extern("rslib_std_string_replace")
        func replace(var val:string, var match:string, var str:string):string;

    extern("rslib_std_string_trim")
        func trim(var val:string):string;

    extern("rslib_std_string_split")
        private func _split(var val:string, var spliter:string, var out_result:array<string>):void;

    func split(var val:string, var spliter:string)
    {
        var arr = []:array<string>;
        _split(val, spliter, arr);
        return arr;
    }
}

namespace array
{
    extern("rslib_std_lengthof") 
        func len<T>(var val:array<T>):int;

    extern("rslib_std_array_resize") 
        func resize<T>(var val:array<T>, var newsz:int, var init_val:T):void;

    func get<T>(var a:array<T>, var index:int)
    {
        return a[index];
    }

    extern("rslib_std_array_add") 
        func add<T>(var val:array<T>, var elem:T):T;
  
    func dup<T>(var val:array<T>)
    {
        const var _dupval = val;
        return _dupval;
    }

    extern("rslib_std_array_remove")
        func remove<T>(var val:array<T>, var index:int):void;

    extern("rslib_std_array_find")
        func find<T>(var val:array<T>, var elem:T):int;

    extern("rslib_std_array_clear")
        func clear<T>(var val:array<T>):void;

    using iterator<T> = gchandle;
    namespace iterator 
    {
        extern("rslib_std_array_iter_next")
            func next<T>(var iter:iterator<T>, ref out_key:int, ref out_val:T):bool;
    }

    extern("rslib_std_array_iter")
        func iter<T>(var val:array<T>):iterator<T>;
}

namespace map
{
    extern("rslib_std_lengthof") 
        func len<KT, VT>(var val:map<KT, VT>):int;
    extern("rslib_std_map_find") 
        func find<KT, VT>(var val:map<KT, VT>, var index:KT):bool;
    extern("rslib_std_map_only_get") 
        func get<KT, VT>(var m:map<KT, VT>, var index:KT):VT;
    extern("rslib_std_map_contain") 
        func contain<KT, VT>(var m:map<KT, VT>, var index:KT):bool;
    extern("rslib_std_map_get_by_default") 
        func get<KT, VT>(var m:map<KT, VT>, var index:KT, var default_val:VT):VT;
    func dup<KT, VT>(var val:map<KT, VT>)
    {
        const var _dupval = val;
        return _dupval;
    }

    extern("rslib_std_map_remove")
        func remove<KT, VT>(var val:map<KT, VT>, var index:int):void;
    extern("rslib_std_map_clear")
        func clear<KT, VT>(var val:map<KT, VT>):void;

    using iterator<KT, VT> = gchandle;
    namespace iterator 
    {
        extern("rslib_std_map_iter_next")
            func next<KT, VT>(var iter:iterator<KT, VT>, ref out_key:KT, ref out_val:VT):bool;
    }

    extern("rslib_std_map_iter")
        func iter<KT, VT>(var val:map<KT, VT>):iterator<KT, VT>;
}

namespace gchandle
{
    extern("rslib_std_gchandle_close")
        func close(var handle:gchandle):void;
}

)" };

RS_API rs_api rslib_std_debug_attach_default_debuggee(rs_vm vm, rs_value args, size_t argc)
{
    rs_attach_default_debuggee(vm);
    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_debug_disattach_default_debuggee(rs_vm vm, rs_value args, size_t argc)
{
    rs_disattach_and_free_debuggee(vm);
    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_debug_callstack_trace(rs_vm vm, rs_value args, size_t argc)
{
    return rs_ret_string(vm, rs_debug_trace_callstack(vm, (size_t)rs_int(args + 0)));
}

RS_API rs_api rslib_std_debug_breakpoint(rs_vm vm, rs_value args, size_t argc)
{
    rs_break_immediately(vm);
    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_debug_invoke(rs_vm vm, rs_value args, size_t argc)
{
    for (size_t index = argc - 1; index > 0; index--)
        rs_push_ref(vm, args + index);

    return rs_ret_val(vm, rs_invoke_value(vm, args, argc - 1));
}

const char* rs_stdlib_debug_src_path = u8"rscene/debug.rsn";
const char* rs_stdlib_debug_src_data = {
u8R"(
namespace std
{
    namespace debug
    {
        extern("rslib_std_debug_breakpoint")
            func breakpoint():void;
        extern("rslib_std_debug_attach_default_debuggee")
            func attach_debuggee():void;
        extern("rslib_std_debug_disattach_default_debuggee")
            func disattach_debuggee():void;

        extern("rslib_std_debug_callstack_trace")
            func callstack(var layer:int) : string;

        func run(var foo, ...)
        {
            attach_debuggee();
            var result = (foo:dynamic(...))(......);
            disattach_debuggee();
    
            return result;
        }

        extern("rslib_std_debug_invoke")
        func invoke<FT>(var foo:FT, ...):typeof(foo(......));
    }
}
)" };

const char* rs_stdlib_vm_src_path = u8"rscene/vm.rsn";
const char* rs_stdlib_vm_src_data = {
u8R"(
namespace std
{
    using vm = gchandle;
    namespace vm
    {
        enum info_style
        {
            RS_DEFAULT = 0,

            RS_NOTHING = 1,
            RS_NEED_COLOR = 2,
        }

        extern("rslib_std_vm_create")
        func create():vm;

        extern("rslib_std_vm_load_src")
        func load_source(var vmhandle:vm, var src:string):bool;
        extern("rslib_std_vm_load_src")
        func load_source(var vmhandle:vm, var vfilepath:string, var src:string):bool;

        extern("rslib_std_vm_load_file")
        func load_file(var vmhandle:vm, var vfilepath:string):bool;

        extern("rslib_std_vm_run")
        func run(var vmhandle:vm):dynamic;
        
        extern("rslib_std_vm_has_compile_error")
        func has_error(var vmhandle:vm):bool;

        extern("rslib_std_vm_has_compile_warning")
        func has_warning(var vmhandle:vm):bool;

        extern("rslib_std_vm_get_compile_error")
        func error_msg(var vmhandle:vm):string;

        extern("rslib_std_vm_get_compile_warning")
        func warning_msg(var vmhandle:vm):string;

        extern("rslib_std_vm_get_compile_error")
        func error_msg(var vmhandle:vm, var style:info_style):string;

        extern("rslib_std_vm_get_compile_warning")
        func warning_msg(var vmhandle:vm, var style:info_style):string;
        
        extern("rslib_std_vm_virtual_source")
        func virtual_source(var vfilepath:string, var src:string, var enable_overwrite:bool):bool;
    }
}
)" };

struct rs_thread_pack
{
    std::thread* _thread;
    rs_vm _vm;
};

RS_API rs_api rslib_std_thread_create(rs_vm vm, rs_value args, size_t argc)
{
    rs_vm new_thread_vm = rs_sub_vm(vm, reinterpret_cast<rs::vmbase*>(vm)->stack_size);

    rs_int_t funcaddr_vm = 0;
    rs_handle_t funcaddr_native = 0;

    if (rs_valuetype(args) == RS_INTEGER_TYPE)
        funcaddr_vm = rs_int(args);
    else if (rs_valuetype(args) == RS_HANDLE_TYPE)
        funcaddr_native = rs_handle(args);
    else
        rs_fail(RS_FAIL_TYPE_FAIL, "Cannot invoke this type of value.");

    for (size_t argidx = argc - 1; argidx > 0; argidx--)
    {
        rs_push_valref(new_thread_vm, args + argidx);
    }

    auto* _vmthread = new std::thread([=]() {
        try
        {
            if (funcaddr_vm)
                rs_invoke_rsfunc((rs_vm)new_thread_vm, funcaddr_vm, argc - 1);
            else
                rs_invoke_exfunc((rs_vm)new_thread_vm, funcaddr_native, argc - 1);
        }
        catch (...)
        {
            // ?
        }

        rs_close_vm(new_thread_vm);
        });

    return rs_ret_gchandle(vm,
        new rs_thread_pack{ _vmthread , new_thread_vm },
        nullptr,
        [](void* rs_thread_pack_ptr)
        {
            if (((rs_thread_pack*)rs_thread_pack_ptr)->_thread->joinable())
                ((rs_thread_pack*)rs_thread_pack_ptr)->_thread->detach();
            delete ((rs_thread_pack*)rs_thread_pack_ptr)->_thread;
        });
}

RS_API rs_api rslib_std_thread_wait(rs_vm vm, rs_value args, size_t argc)
{
    rs_thread_pack* rtp = (rs_thread_pack*)rs_pointer(args);

    if (rtp->_thread->joinable())
        rtp->_thread->join();

    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_thread_abort(rs_vm vm, rs_value args, size_t argc)
{
    rs_thread_pack* rtp = (rs_thread_pack*)rs_pointer(args);
    return rs_ret_int(vm, rs_abort_vm(rtp->_vm));
}


RS_API rs_api rslib_std_thread_mutex_create(rs_vm vm, rs_value args, size_t argc)
{
    return rs_ret_gchandle(vm,
        new std::shared_mutex,
        nullptr,
        [](void* mtx_ptr)
        {
            delete (std::shared_mutex*)mtx_ptr;
        });
}

RS_API rs_api rslib_std_thread_mutex_read(rs_vm vm, rs_value args, size_t argc)
{
    std::shared_mutex* smtx = (std::shared_mutex*)rs_pointer(args);
    smtx->lock_shared();

    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_thread_mutex_write(rs_vm vm, rs_value args, size_t argc)
{
    std::shared_mutex* smtx = (std::shared_mutex*)rs_pointer(args);
    smtx->lock();

    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_thread_mutex_read_end(rs_vm vm, rs_value args, size_t argc)
{
    std::shared_mutex* smtx = (std::shared_mutex*)rs_pointer(args);
    smtx->unlock_shared();

    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_thread_mutex_write_end(rs_vm vm, rs_value args, size_t argc)
{
    std::shared_mutex* smtx = (std::shared_mutex*)rs_pointer(args);
    smtx->unlock();

    return rs_ret_nil(vm);
}

////////////////////////////////////////////////////////////////////////

RS_API rs_api rslib_std_thread_spin_create(rs_vm vm, rs_value args, size_t argc)
{
    return rs_ret_gchandle(vm,
        new rs::gcbase::rw_lock,
        nullptr,
        [](void* mtx_ptr)
        {
            delete (rs::gcbase::rw_lock*)mtx_ptr;
        });
}

RS_API rs_api rslib_std_thread_spin_read(rs_vm vm, rs_value args, size_t argc)
{
    rs::gcbase::rw_lock* smtx = (rs::gcbase::rw_lock*)rs_pointer(args);
    smtx->lock_shared();

    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_thread_spin_write(rs_vm vm, rs_value args, size_t argc)
{
    rs::gcbase::rw_lock* smtx = (rs::gcbase::rw_lock*)rs_pointer(args);
    smtx->lock();

    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_thread_spin_read_end(rs_vm vm, rs_value args, size_t argc)
{
    rs::gcbase::rw_lock* smtx = (rs::gcbase::rw_lock*)rs_pointer(args);
    smtx->unlock_shared();

    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_thread_spin_write_end(rs_vm vm, rs_value args, size_t argc)
{
    rs::gcbase::rw_lock* smtx = (rs::gcbase::rw_lock*)rs_pointer(args);
    smtx->unlock();

    return rs_ret_nil(vm);
}



const char* rs_stdlib_thread_src_path = u8"rscene/thread.rsn";
const char* rs_stdlib_thread_src_data = {
u8R"(
namespace std
{
    using thread = gchandle;
    namespace thread
    {
        extern("rslib_std_thread_create")
            func create<FuncT>(var thread_work:FuncT, ...):thread;

        extern("rslib_std_thread_wait")
            func wait(var threadhandle : thread):void;

        extern("rslib_std_thread_abort")
            func abort(var threadhandle : thread):bool;
    }

    using mutex = gchandle;
    namespace mutex
    {
        extern("rslib_std_thread_mutex_create")
            func create():mutex;

        extern("rslib_std_thread_mutex_read")
            func read(var mtx : mutex):void;

        extern("rslib_std_thread_mutex_write")
            func lock(var mtx : mutex):void;

        extern("rslib_std_thread_mutex_read_end")
            func unread(var mtx : mutex):void;

        extern("rslib_std_thread_mutex_write_end")
            func unlock(var mtx : mutex):void;
    }

    using spin = gchandle;
    namespace spin
    {
        extern("rslib_std_thread_spin_create")
            func create():spin;

        extern("rslib_std_thread_spin_read")
            func read(var mtx : spin):void;

        extern("rslib_std_thread_spin_write")
            func lock(var mtx : spin):void;

        extern("rslib_std_thread_spin_read_end")
            func unread(var mtx : spin):void;

        extern("rslib_std_thread_spin_write_end")
            func unlock(var mtx : spin):void;
    }
}

)" };


///////////////////////////////////////////////////////////////////////////////////////
// roroutine APIs
///////////////////////////////////////////////////////////////////////////////////////

struct rs_co_waitable_base
{
    virtual ~rs_co_waitable_base() = default;
    virtual void wait_at_current_fthread() = 0;
};

template<typename T>
struct rs_co_waitable : rs_co_waitable_base
{
    rs::shared_pointer<T> sp_waitable;

    rs_co_waitable(const rs::shared_pointer<T>& sp)
        :sp_waitable(sp)
    {
        static_assert(std::is_base_of<rs::fwaitable, T>::value);
    }

    virtual void wait_at_current_fthread() override
    {
        rs::fthread::wait(sp_waitable);
    }
};

RS_API rs_api rslib_std_roroutine_launch(rs_vm vm, rs_value args, size_t argc)
{
    // rslib_std_roroutine_launch(...)   
    auto* _nvm = RSCO_WorkerPool::get_usable_vm(reinterpret_cast<rs::vmbase*>(vm));
    for (size_t i = argc - 1; i > 0; i--)
    {
        rs_push_valref(reinterpret_cast<rs_vm>(_nvm), args + i);
    }

    rs::shared_pointer<rs::RSCO_Waitter> gchandle_roroutine;

    if (RS_INTEGER_TYPE == rs_valuetype(args + 0))
        gchandle_roroutine = rs::fvmscheduler::new_work(_nvm, rs_int(args + 0), argc - 1);
    else
        gchandle_roroutine = rs::fvmscheduler::new_work(_nvm, rs_handle(args + 0), argc - 1);



    return rs_ret_gchandle(vm,
        new rs_co_waitable<rs::RSCO_Waitter>(gchandle_roroutine),
        nullptr,
        [](void* gchandle_roroutine_ptr)
        {
            delete (rs_co_waitable_base*)gchandle_roroutine_ptr;
        });
}

RS_API rs_api rslib_std_roroutine_abort(rs_vm vm, rs_value args, size_t argc)
{
    auto* gchandle_roroutine = (rs_co_waitable<rs::RSCO_Waitter>*) rs_pointer(args);
    if (gchandle_roroutine->sp_waitable)
        gchandle_roroutine->sp_waitable->abort();

    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_roroutine_completed(rs_vm vm, rs_value args, size_t argc)
{
    auto* gchandle_roroutine = (rs_co_waitable<rs::RSCO_Waitter>*) rs_pointer(args);
    if (gchandle_roroutine->sp_waitable)
        return rs_ret_bool(vm, gchandle_roroutine->sp_waitable->complete_flag);
    else
        return rs_ret_bool(vm, true);
}

RS_API rs_api rslib_std_roroutine_pause_all(rs_vm vm, rs_value args, size_t argc)
{
    rs_coroutine_pauseall();
    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_roroutine_resume_all(rs_vm vm, rs_value args, size_t argc)
{
    rs_coroutine_resumeall();
    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_roroutine_stop_all(rs_vm vm, rs_value args, size_t argc)
{
    rs_coroutine_stopall();
    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_roroutine_sleep(rs_vm vm, rs_value args, size_t argc)
{
    using namespace std;
    rs::fthread::wait(rs::fvmscheduler::wait(rs_real(args)));

    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_roroutine_yield(rs_vm vm, rs_value args, size_t argc)
{
    rs::fthread::yield();
    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_roroutine_wait(rs_vm vm, rs_value args, size_t argc)
{
    rs_co_waitable_base* waitable = (rs_co_waitable_base*)rs_pointer(args);

    waitable->wait_at_current_fthread();
    return rs_ret_nil(vm);
}

const char* rs_stdlib_roroutine_src_path = u8"rscene/co.rsn";
const char* rs_stdlib_roroutine_src_data = {
u8R"(
namespace std
{
    using co = gchandle;

    using waitable = gchandle;

    namespace co
    {
        extern("rslib_std_roroutine_launch")
            func create<FT>(var f:FT, ...):co;
        
        extern("rslib_std_roroutine_abort")
            func abort(var co:co):void;

        extern("rslib_std_roroutine_completed")
            func completed(var co:co):bool;

        // Static functions:

        extern("rslib_std_roroutine_pause_all")
            func pause_all():void;

        extern("rslib_std_roroutine_resume_all")
            func resume_all():void;

        extern("rslib_std_roroutine_stop_all")
            func stop_all():void;

        extern("rslib_std_roroutine_sleep")
            func sleep(var time:real):void;

        extern("rslib_std_thread_yield")
                func yield():bool;

        extern("rslib_std_roroutine_wait")
                func wait(var condi:waitable):void;

        extern("rslib_std_roroutine_wait")
                func wait(var condi:co):void;
    }
}

)" };

RS_API rs_api rslib_std_macro_lexer_lex(rs_vm vm, rs_value args, size_t argc)
{
    rs::lexer* lex = (rs::lexer*)rs_pointer(args + 0);

    rs::lexer tmp_lex(rs::str_to_wstr(
        rs_string(args + 1)
    ), "macro" + lex->source_file + "_impl.rsn");

    std::vector<std::pair<rs::lex_type, std::wstring>> lex_tokens;

    for (;;)
    {
        std::wstring result;
        auto token = tmp_lex.next(&result);

        if (token == +rs::lex_type::l_eof)
            break;

        lex_tokens.push_back({ token , result });
    }

    for (auto ri = lex_tokens.rbegin(); ri != lex_tokens.rend(); ri++)
        lex->push_temp_for_error_recover(ri->first, ri->second);

    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_macro_lexer_warning(rs_vm vm, rs_value args, size_t argc)
{
    rs::lexer* lex = (rs::lexer*)rs_pointer(args + 0);

    lex->lex_warning(0x0000, rs::str_to_wstr(rs_string(args + 1)).c_str());
    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_macro_lexer_error(rs_vm vm, rs_value args, size_t argc)
{
    rs::lexer* lex = (rs::lexer*)rs_pointer(args + 0);

    lex->lex_error(0x0000, rs::str_to_wstr(rs_string(args + 1)).c_str());
    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_macro_lexer_peek(rs_vm vm, rs_value args, size_t argc)
{
    rs::lexer* lex = (rs::lexer*)rs_pointer(args + 0);

    std::wstring out_result;
    auto token_type = lex->peek(&out_result);

    rs_set_string(args + 1, rs::wstr_to_str(out_result).c_str());

    return rs_ret_int(vm, (rs_integer_t)token_type);
}

RS_API rs_api rslib_std_macro_lexer_next(rs_vm vm, rs_value args, size_t argc)
{
    rs::lexer* lex = (rs::lexer*)rs_pointer(args + 0);

    std::wstring out_result;
    auto token_type = lex->next(&out_result);

    rs_set_string(args + 1, rs::wstr_to_str(out_result).c_str());

    return rs_ret_int(vm, (rs_integer_t)token_type);
}

RS_API rs_api rslib_std_macro_lexer_nextch(rs_vm vm, rs_value args, size_t argc)
{
    rs::lexer* lex = (rs::lexer*)rs_pointer(args + 0);

    wchar_t ch[2] = {};

    int readch = lex->next_one();

    if (readch == EOF)
        return rs_ret_string(vm, "");

    ch[0] = (wchar_t)readch;
    return rs_ret_string(vm, rs::wstr_to_str(ch).c_str());
}

RS_API rs_api rslib_std_macro_lexer_peekch(rs_vm vm, rs_value args, size_t argc)
{
    rs::lexer* lex = (rs::lexer*)rs_pointer(args + 0);

    wchar_t ch[2] = {};

    int readch = lex->peek_one();

    if (readch == EOF)
        return rs_ret_string(vm, "");

    ch[0] = (wchar_t)readch;
    return rs_ret_string(vm, rs::wstr_to_str(ch).c_str());
}

RS_API rs_api rslib_std_macro_lexer_current_path(rs_vm vm, rs_value args, size_t argc)
{
    rs::lexer* lex = (rs::lexer*)rs_pointer(args + 0);
    return rs_ret_string(vm, lex->source_file.c_str());
}

const char* rs_stdlib_macro_src_path = u8"rscene/macro.rsn";
const char* rs_stdlib_macro_src_data = {
u8R"(
import rscene.std;

namespace std
{
    enum token_type
    {
        l_eof = -1,
        l_error = 0,

        l_empty,          // [empty]

        l_identifier,           // identifier.
        l_literal_integer,      // 1 233 0x123456 0b1101001 032
        l_literal_handle,       // 0L 256L 0xFFL
        l_literal_real,         // 0.2  0.  .235
        l_literal_string,       // "STR"
        l_semicolon,            // ;

        l_comma,                // ,
        l_add,                  // +
        l_sub,                  // - 
        l_mul,                  // * 
        l_div,                  // / 
        l_mod,                  // % 
        l_assign,               // =
        l_add_assign,           // +=
        l_sub_assign,           // -= 
        l_mul_assign,           // *=
        l_div_assign,           // /= 
        l_mod_assign,           // %= 
        l_equal,                // ==
        l_not_equal,            // !=
        l_larg_or_equal,        // >=
        l_less_or_equal,        // <=
        l_less,                 // <
        l_larg,                 // >
        l_land,                 // &&
        l_lor,                  // ||
        l_lnot,                  // !
        l_scopeing,             // ::
        l_template_using_begin,             // ::<
        l_typecast,              // :
        l_index_point,          // .
        l_double_index_point,          // ..  may be used? hey..
        l_variadic_sign,          // ...
        l_index_begin,          // '['
        l_index_end,            // ']'
        l_direct,               // '->'

        l_left_brackets,        // (
        l_right_brackets,       // )
        l_left_curly_braces,    // {
        l_right_curly_braces,   // }

        l_import,               // import

        l_inf,
        l_nil,
        l_while,
        l_if,
        l_else,
        l_namespace,
        l_for,
        l_extern,

        l_var,
        l_ref,
        l_func,
        l_return,
        l_using,
        l_enum,
        l_as,
        l_is,
        l_typeof,

        l_private,
        l_public,
        l_protected,
        l_const,
        l_static,

        l_break,
        l_continue,
        l_goto,
        l_at,
        l_naming
    }

    using lexer = handle;
    namespace lexer
    {
        extern("rslib_std_macro_lexer_lex")
            func lex(var lex:lexer, var src:string):void;

        extern("rslib_std_macro_lexer_warning")
            func warning(var lex:lexer, var msg:string):void;

        extern("rslib_std_macro_lexer_error")
            func error(var lex:lexer, var msg:string):void;

        extern("rslib_std_macro_lexer_peek")
            func peek(var lex:lexer, ref out_token:string):token_type;

        extern("rslib_std_macro_lexer_next")
            func next(var lex:lexer, ref out_token:string):token_type;

        extern("rslib_std_macro_lexer_nextch")
            func nextch(var lex:lexer) : string;

        extern("rslib_std_macro_lexer_peekch")
            func peekch(var lex:lexer) : string;

        extern("rslib_std_macro_lexer_current_path")
            func path(var lex:lexer) : string;
    }
}

)" };