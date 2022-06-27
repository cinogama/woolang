#define _CRT_SECURE_NO_WARNINGS
#include "wo_lang_extern_symbol_loader.hpp"
#include "wo_utf8.hpp"
#include "wo_vm.hpp"
#include "wo_roroutine_simulate_mgr.hpp"
#include "wo_roroutine_thread_mgr.hpp"
#include "wo_io.hpp"
#include "wo_exceptions.hpp"

#include <chrono>
#include <random>
#include <thread>

WO_API wo_api rslib_std_print(wo_vm vm, wo_value args, size_t argc)
{
    for (size_t i = 0; i < argc; i++)
    {
        wo::wo_stdout << wo_cast_string(args + i);

        if (i + 1 < argc)
            wo::wo_stdout << " ";
    }
    return wo_ret_int(vm, argc);
}
WO_API wo_api rslib_std_panic(wo_vm vm, wo_value args, size_t argc)
{
    wo_fail(WO_FAIL_DEADLY, wo_string(args + 0));
    return wo_ret_nil(vm);
}
WO_API wo_api rslib_std_halt(wo_vm vm, wo_value args, size_t argc)
{
    throw wo::rsruntime_exception(WO_FAIL_HEAVY, wo_string(args + 0));
    return wo_ret_nil(vm);
}
WO_API wo_api rslib_std_throw(wo_vm vm, wo_value args, size_t argc)
{
    throw wo::rsruntime_exception(WO_FAIL_MEDIUM, wo_string(args + 0));
    return wo_ret_nil(vm);
}
WO_API wo_api rslib_std_fail(wo_vm vm, wo_value args, size_t argc)
{
    wo_fail(WO_FAIL_MEDIUM, wo_string(args + 0));
    return wo_ret_nil(vm);
}
WO_API wo_api rslib_std_lengthof(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_int(vm, wo_lengthof(args));
}

WO_API wo_api rslib_std_string_toupper(wo_vm vm, wo_value args, size_t argc)
{
    std::string str = wo_string(args + 0);
    for (auto& ch : str)
        ch = (char)toupper((int)(unsigned char)ch);
    return wo_ret_string(vm, str.c_str());
}

WO_API wo_api rslib_std_string_tolower(wo_vm vm, wo_value args, size_t argc)
{
    std::string str = wo_string(args + 0);
    for (auto& ch : str)
        ch = (char)tolower((int)(unsigned char)ch);
    return wo_ret_string(vm, str.c_str());
}

WO_API wo_api rslib_std_string_isspace(wo_vm vm, wo_value args, size_t argc)
{
    wo_string_t str = wo_string(args + 0);

    if (*str)
    {
        auto&& wstr = wo::str_to_wstr(str);
        for (auto& wch : wstr)
            if (!wo::lexer::lex_isspace(wch))
                return wo_ret_bool(vm, false);
        return wo_ret_bool(vm, true);
    }
    return wo_ret_bool(vm, false);
}

WO_API wo_api rslib_std_string_isalpha(wo_vm vm, wo_value args, size_t argc)
{
    wo_string_t str = wo_string(args + 0);

    if (*str)
    {
        auto&& wstr = wo::str_to_wstr(str);
        for (auto& wch : wstr)
            if (!wo::lexer::lex_isalpha(wch))
                return wo_ret_bool(vm, false);
        return wo_ret_bool(vm, true);
    }
    return wo_ret_bool(vm, false);
}

WO_API wo_api rslib_std_string_isalnum(wo_vm vm, wo_value args, size_t argc)
{
    wo_string_t str = wo_string(args + 0);

    if (*str)
    {
        auto&& wstr = wo::str_to_wstr(str);
        for (auto& wch : wstr)
            if (!wo::lexer::lex_isalnum(wch))
                return wo_ret_bool(vm, false);
        return wo_ret_bool(vm, true);
    }
    return wo_ret_bool(vm, false);
}

WO_API wo_api rslib_std_string_isnumber(wo_vm vm, wo_value args, size_t argc)
{
    wo_string_t str = wo_string(args + 0);

    if (*str)
    {
        auto&& wstr = wo::str_to_wstr(str);
        for (auto& wch : wstr)
            if (!wo::lexer::lex_isdigit(wch))
                return wo_ret_bool(vm, false);
        return wo_ret_bool(vm, true);
    }
    return wo_ret_bool(vm, false);
}

WO_API wo_api rslib_std_string_ishex(wo_vm vm, wo_value args, size_t argc)
{
    wo_string_t str = wo_string(args + 0);

    if (*str)
    {
        auto&& wstr = wo::str_to_wstr(str);
        for (auto& wch : wstr)
            if (!wo::lexer::lex_isxdigit(wch))
                return wo_ret_bool(vm, false);
        return wo_ret_bool(vm, true);
    }
    return wo_ret_bool(vm, false);
}

WO_API wo_api rslib_std_string_isoct(wo_vm vm, wo_value args, size_t argc)
{
    wo_string_t str = wo_string(args + 0);

    if (*str)
    {
        auto&& wstr = wo::str_to_wstr(str);
        for (auto& wch : wstr)
            if (!wo::lexer::lex_isodigit(wch))
                return wo_ret_bool(vm, false);
        return wo_ret_bool(vm, true);
    }
    return wo_ret_bool(vm, false);
}

WO_API wo_api rslib_std_string_enstring(wo_vm vm, wo_value args, size_t argc)
{
    wo_string_t str = wo_string(args + 0);
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
    return wo_ret_string(vm, result.c_str());
}

WO_API wo_api rslib_std_string_destring(wo_vm vm, wo_value args, size_t argc)
{
    wo_string_t str = wo_string(args + 0);
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
    return wo_ret_string(vm, result.c_str());
}

WO_API wo_api rslib_std_string_beginwith(wo_vm vm, wo_value args, size_t argc)
{
    wo_string_t aim = wo_string(args + 0);
    wo_string_t begin = wo_string(args + 1);

    while ((*aim) && (*begin))
    {
        if (*aim != *begin)
            return wo_ret_bool(vm, false);
        ++aim;
        ++begin;
    }

    return wo_ret_bool(vm, true);
}

WO_API wo_api rslib_std_string_endwith(wo_vm vm, wo_value args, size_t argc)
{
    wo_string_t aim = wo_string(args + 0);
    wo_string_t end = wo_string(args + 1);

    size_t aimlen = strlen(aim);
    size_t endlen = strlen(end);

    if (endlen > aimlen)
        return wo_ret_bool(vm, false);

    aim += (aimlen - endlen);
    while ((*aim) && (*end))
    {
        if (*aim != *end)
            return wo_ret_bool(vm, false);
        ++aim;
        ++end;
    }
    return wo_ret_bool(vm, true);
}

WO_API wo_api rslib_std_string_replace(wo_vm vm, wo_value args, size_t argc)
{
    std::string aim = wo_string(args + 0);
    wo_string_t match = wo_string(args + 1);
    wo_string_t replace = wo_string(args + 2);

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

    return wo_ret_string(vm, aim.c_str());
}

WO_API wo_api rslib_std_string_find(wo_vm vm, wo_value args, size_t argc)
{
    std::string aim = wo_string(args + 0);
    wo_string_t match = wo_string(args + 1);

    size_t fnd_place = aim.find(match, 0);
    if (fnd_place > 0 && fnd_place < aim.size())
        return wo_ret_int(vm, (wo_integer_t)fnd_place);

    return wo_ret_int(vm, -1);
}

WO_API wo_api rslib_std_string_find_from(wo_vm vm, wo_value args, size_t argc)
{
    std::string aim = wo_string(args + 0);
    wo_string_t match = wo_string(args + 1);
    size_t from = (size_t)wo_string(args + 2);

    size_t fnd_place = aim.find(match, from);
    if (fnd_place > from && fnd_place < aim.size())
        return wo_ret_int(vm, (wo_integer_t)fnd_place);

    return wo_ret_int(vm, -1);
}


WO_API wo_api rslib_std_string_trim(wo_vm vm, wo_value args, size_t argc)
{
    std::string aim = wo_string(args + 0);

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

    return wo_ret_string(vm, aim.substr(ibeg, iend - ibeg).c_str());
}

WO_API wo_api rslib_std_string_split(wo_vm vm, wo_value args, size_t argc)
{
    std::string aim = wo_string(args + 0);
    wo_string_t match = wo_string(args + 1);
    wo_value arr = args + 2;

    size_t matchlen = strlen(match);
    size_t split_begin = 0;

    while (true)
    {
        size_t fnd_place = aim.find(match, split_begin);
        if (fnd_place< split_begin || fnd_place>aim.size())
        {
            wo_value v = wo_arr_add(arr, nullptr);
            wo_set_string(v, aim.substr(split_begin).c_str());
            break;
        }
        wo_value v = wo_arr_add(arr, nullptr);
        wo_set_string(v, aim.substr(split_begin, fnd_place - split_begin).c_str());

        split_begin = fnd_place + matchlen;
    }
    return wo_ret_nil(vm);
}

WO_API wo_api rslib_std_time_sec(wo_vm vm, wo_value args, size_t argc)
{
    static std::chrono::system_clock _sys_clock;
    static auto _first_invoke_time = _sys_clock.now();

    auto _time_ms = wo_real_t((_sys_clock.now() - _first_invoke_time).count() * std::chrono::system_clock::period::num)
        / std::chrono::system_clock::period::den;
    return wo_ret_real(vm, _time_ms);
}

WO_API wo_api rslib_std_atomic_cas(wo_vm vm, wo_value args, size_t argc)
{
    wo::value* aim = reinterpret_cast<wo::value*>(args + 0)->get();
    wo::value* excepted = reinterpret_cast<wo::value*>(args + 1)->get();
    wo::value* swapval = reinterpret_cast<wo::value*>(args + 2)->get();

    wo_assert(aim->type == excepted->type && excepted->type == swapval->type);

    return wo_ret_bool(vm, ((std::atomic<wo_handle_t>*) & aim->handle)->compare_exchange_weak(excepted->handle, swapval->handle));
}

WO_API wo_api rslib_std_randomint(wo_vm vm, wo_value args, size_t argc)
{
    static std::random_device rd;
    static std::mt19937_64 mt64(rd());

    wo_int_t from = wo_int(args + 0);
    wo_int_t to = wo_int(args + 1);

    if (to < from)
        std::swap(from, to);

    std::uniform_int_distribution<wo_int_t> dis(from, to);
    return wo_ret_int(vm, dis(mt64));
}

WO_API wo_api rslib_std_randomreal(wo_vm vm, wo_value args)
{
    static std::random_device rd;
    static std::mt19937_64 mt64(rd());

    wo_real_t from = wo_real(args + 0);
    wo_real_t to = wo_real(args + 1);

    if (to < from)
        std::swap(from, to);

    std::uniform_real_distribution<wo_real_t> dis(from, to);
    return wo_ret_real(vm, dis(mt64));
}

WO_API wo_api rslib_std_break_yield(wo_vm vm, wo_value args, size_t argc)
{
    wo_break_yield(vm);
    return wo_ret_nil(vm);
}

WO_API wo_api rslib_std_array_resize(wo_vm vm, wo_value args, size_t argc)
{
    wo_arr_resize(args + 0, wo_int(args + 1), args + 2);
    return wo_ret_nil(vm);
}

WO_API wo_api rslib_std_array_add(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_ref(vm, wo_arr_add(args + 0, args + 1));
}

WO_API wo_api rslib_std_array_remove(wo_vm vm, wo_value args, size_t argc)
{
    wo_arr_remove(args + 0, wo_int(args + 1));

    return wo_ret_nil(vm);
}

WO_API wo_api rslib_std_array_find(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_int(vm, wo_arr_find(args + 0, args + 1));
}

WO_API wo_api rslib_std_array_clear(wo_vm vm, wo_value args, size_t argc)
{
    wo_arr_clear(args);

    return wo_ret_nil(vm);
}

struct array_iter
{
    using array_iter_t = decltype(std::declval<wo::array_t>().begin());

    array_iter_t iter;
    array_iter_t end_place;
    wo_int_t     index_count;
};

WO_API wo_api rslib_std_array_iter(wo_vm vm, wo_value args, size_t argc)
{
    wo::value* arr = reinterpret_cast<wo::value*>(args)->get();

    if (arr->is_nil())
    {
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is 'nil'.");
        return wo_ret_nil(vm);
    }
    else
    {
        return wo_ret_gchandle(vm,
            new array_iter{ arr->array->begin(), arr->array->end(), 0 },
            args + 0,
            [](void* array_iter_t_ptr)
            {
                delete (array_iter*)array_iter_t_ptr;
            }
        );
    }
}

WO_API wo_api rslib_std_array_iter_next(wo_vm vm, wo_value args, size_t argc)
{
    array_iter& iter = *(array_iter*)wo_pointer(args);

    if (iter.iter == iter.end_place)
        return wo_ret_bool(vm, false);

    wo_set_int(args + 1, iter.index_count++); // key
    wo_set_val(args + 2, reinterpret_cast<wo_value>(&*(iter.iter++))); // val

    return wo_ret_bool(vm, true);
}

WO_API wo_api rslib_std_map_find(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_bool(vm, wo_map_find(args + 0, args + 1));
}

WO_API wo_api rslib_std_map_only_get(wo_vm vm, wo_value args, size_t argc)
{
    wo_value result = wo_map_get(args + 0, args + 1);

    if (result)
        return wo_ret_ref(vm, result);

    return wo_ret_nil(vm);
}

WO_API wo_api rslib_std_map_contain(wo_vm vm, wo_value args, size_t argc)
{
    bool _map_has_indexed_val = wo_map_find(args + 0, args + 1);

    return wo_ret_int(vm, _map_has_indexed_val ? 1 : 0);
}

WO_API wo_api rslib_std_map_get_by_default(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_ref(vm, wo_map_get_by_default(args + 0, args + 1, args + 2));
}

WO_API wo_api rslib_std_map_remove(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_bool(vm, wo_map_remove(args + 0, args + 1));
}

WO_API wo_api rslib_std_map_clear(wo_vm vm, wo_value args, size_t argc)
{
    wo_map_clear(args + 0);
    return wo_ret_nil(vm);
}

struct map_iter
{
    using mapping_iter_t = decltype(std::declval<wo::mapping_t>().begin());

    mapping_iter_t iter;
    mapping_iter_t end_place;
};

WO_API wo_api rslib_std_map_iter(wo_vm vm, wo_value args, size_t argc)
{
    wo::value* mapp = reinterpret_cast<wo::value*>(args)->get();

    if (mapp->is_nil())
    {
        wo_fail(WO_FAIL_TYPE_FAIL, "Value is 'nil'.");
        return wo_ret_nil(vm);
    }
    else
    {
        return wo_ret_gchandle(vm,
            new map_iter{ mapp->mapping->begin(), mapp->mapping->end() },
            args + 0,
            [](void* array_iter_t_ptr)
            {
                delete (map_iter*)array_iter_t_ptr;
            }
        );
    }
}

WO_API wo_api rslib_std_map_iter_next(wo_vm vm, wo_value args, size_t argc)
{
    map_iter& iter = *(map_iter*)wo_pointer(args);

    if (iter.iter == iter.end_place)
        return wo_ret_bool(vm, false);

    wo_set_val(args + 1, reinterpret_cast<wo_value>(const_cast<wo::value*>(&iter.iter->first))); // key
    wo_set_val(args + 2, reinterpret_cast<wo_value>(&iter.iter->second)); // val
    iter.iter++;

    return wo_ret_bool(vm, true);
}

WO_API wo_api rslib_std_sub(wo_vm vm, wo_value args, size_t argc)
{
    if (wo_valuetype(args + 0) == WO_STRING_TYPE)
    {
        // return substr
        size_t sub_str_len = 0;
        if (argc == 2)
        {
            auto* substring = wo::u8substr(wo_string(args + 0), wo_int(args + 1), wo::u8str_npos, &sub_str_len);
            return wo_ret_string(vm, std::string(substring, sub_str_len).c_str());
        }
        auto* substring = wo::u8substr(wo_string(args + 0), wo_int(args + 1), wo_int(args + 2), &sub_str_len);
        return wo_ret_string(vm, std::string(substring, sub_str_len).c_str());
    }

    //return wo_ret_ref(vm, mapping_indexed);
    return wo_ret_nil(vm);
}

WO_API wo_api rslib_std_thread_sleep(wo_vm vm, wo_value args, size_t argc)
{
    using namespace std;

    std::this_thread::sleep_for(wo_real(args) * 1s);
    return wo_ret_nil(vm);
}

WO_API wo_api rslib_std_vm_create(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_gchandle(vm,
        wo_create_vm(),
        nullptr,
        [](void* vm_ptr) {
            wo_close_vm((wo_vm)vm_ptr);
        });
}

WO_API wo_api rslib_std_vm_load_src(wo_vm vm, wo_value args, size_t argc)
{
    wo_vm vmm = (wo_vm)wo_pointer(args);

    bool compile_result;
    if (argc < 3)
        compile_result = wo_load_source(vmm, "_temp_source.wo", wo_string(args + 1));
    else
        compile_result = wo_load_source(vmm, wo_string(args + 1), wo_string(args + 2));

    return wo_ret_bool(vm, compile_result);
}

WO_API wo_api rslib_std_vm_load_file(wo_vm vm, wo_value args, size_t argc)
{
    wo_vm vmm = (wo_vm)wo_pointer(args);
    bool compile_result = wo_load_file(vmm, wo_string(args + 1));
    return wo_ret_bool(vm, compile_result);
}

WO_API wo_api rslib_std_vm_run(wo_vm vm, wo_value args, size_t argc)
{
    wo_vm vmm = (wo_vm)wo_pointer(args);
    wo_value ret = wo_run(vmm);

    return wo_ret_val(vm, ret);
}

WO_API wo_api rslib_std_vm_has_compile_error(wo_vm vm, wo_value args, size_t argc)
{
    wo_vm vmm = (wo_vm)wo_pointer(args);
    return wo_ret_bool(vm, wo_has_compile_error(vmm));
}

WO_API wo_api rslib_std_vm_has_compile_warning(wo_vm vm, wo_value args, size_t argc)
{
    wo_vm vmm = (wo_vm)wo_pointer(args);
    return wo_ret_bool(vm, wo_has_compile_warning(vmm));
}

WO_API wo_api rslib_std_vm_get_compile_error(wo_vm vm, wo_value args, size_t argc)
{
    wo_vm vmm = (wo_vm)wo_pointer(args);
    _wo_inform_style style = argc > 1 ? (_wo_inform_style)wo_int(args + 1) : WO_DEFAULT;

    return wo_ret_string(vm, wo_get_compile_error(vmm, style));
}

WO_API wo_api rslib_std_vm_get_compile_warning(wo_vm vm, wo_value args, size_t argc)
{
    wo_vm vmm = (wo_vm)wo_pointer(args);
    _wo_inform_style style = argc > 1 ? (_wo_inform_style)wo_int(args + 1) : WO_DEFAULT;

    return wo_ret_string(vm, wo_get_compile_warning(vmm, style));
}

WO_API wo_api rslib_std_vm_virtual_source(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_bool(vm, wo_virtual_source(
        wo_string(args + 0),
        wo_string(args + 1),
        wo_int(args + 2)
    ));
}

WO_API wo_api rslib_std_gchandle_close(wo_vm vm, wo_value args, size_t argc)
{
    return wo_gchandle_close(args);
}

WO_API wo_api rslib_std_thread_yield(wo_vm vm, wo_value args, size_t argc)
{
    wo_co_yield();
    return wo_ret_nil(vm);
}

WO_API wo_api rslib_std_get_exe_path(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_string(vm, wo::exe_path());
}

WO_API wo_api rslib_std_get_extern_symb(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_int(vm, wo_extern_symb(vm, wo_string(args + 0)));
}

const char* wo_stdlib_src_path = u8"woo/std.wo";
const char* wo_stdlib_src_data = {
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
        func isalpha(var val:string): bool;

    extern("rslib_std_string_isalnum")
        func isalnum(var val:string): bool;

    extern("rslib_std_string_isnumber")
        func isnumber(var val:string): bool;

    extern("rslib_std_string_ishex")
        func ishex(var val:string): bool;

    extern("rslib_std_string_isoct")
        func isoct(var val:string): bool;

    extern("rslib_std_string_enstring")
        func enstring(var val:string):string;

    extern("rslib_std_string_destring")
        func destring(var val:string): string;

    extern("rslib_std_string_beginwith")
        func beginwith(var val:string, var str:string): bool;

    extern("rslib_std_string_endwith")
        func endwith(var val:string, var str:string): bool;

    extern("rslib_std_string_replace")
        func replace(var val:string, var match_aim:string, var str:string): string;

    extern("rslib_std_string_find")
        func find(var val:string, var match_aim:string): int;

    extern("rslib_std_string_find_from")
        func find(var val:string, var match_aim:string, var from: int): int;

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
    
        func iter<T>(var iter:iterator<T>) { return iter; }
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

        func iter<KT, VT>(var iter:iterator<KT, VT>) { return iter; }
    }

    extern("rslib_std_map_iter")
        func iter<KT, VT>(var val:map<KT, VT>):iterator<KT, VT>;
}

namespace gchandle
{
    extern("rslib_std_gchandle_close")
        func close(var handle:gchandle):void;
}

func assert(var val: bool)
{
    if (!val)
        std::panic("Assert failed.");
}
func assert(var val: bool, var msg: string)
{
    if (!val)
        std::panic(F"Assert failed: {msg}");
}


optional option<T>
{
    value(T),
    none
}
namespace option
{
    func map<T, R>(var self: option<T>, var functor: R(T))
    {
        match(self)
        {
            option::value(x)?
                return option::value(functor(x));
            option::none?
                return option::none:<R>;
        }
    }

    func val<T>(var self: option<T>)
    {
        match(self)
        {
            option::value(x)?
                return x;
            option::none?
                std::panic("Except 'value' here, but get 'none'.");
        }
    }
    func has_value<T>(var self: option<T>)
    {
        match(self)
        {
            option::value(x)?
                return true;
            option::none?
                return false;
        }
    }
}

)" };

WO_API wo_api rslib_std_debug_attach_default_debuggee(wo_vm vm, wo_value args, size_t argc)
{
    wo_attach_default_debuggee(vm);
    return wo_ret_nil(vm);
}

WO_API wo_api rslib_std_debug_disattach_default_debuggee(wo_vm vm, wo_value args, size_t argc)
{
    wo_disattach_and_free_debuggee(vm);
    return wo_ret_nil(vm);
}

WO_API wo_api rslib_std_debug_callstack_trace(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_string(vm, wo_debug_trace_callstack(vm, (size_t)wo_int(args + 0)));
}

WO_API wo_api rslib_std_debug_breakpoint(wo_vm vm, wo_value args, size_t argc)
{
    wo_break_immediately(vm);
    return wo_ret_nil(vm);
}

WO_API wo_api rslib_std_debug_invoke(wo_vm vm, wo_value args, size_t argc)
{
    for (size_t index = argc - 1; index > 0; index--)
        wo_push_ref(vm, args + index);

    return wo_ret_val(vm, wo_invoke_value(vm, args, argc - 1));
}

WO_API wo_api rslib_std_debug_empty_func(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_nil(vm);
}


const char* wo_stdlib_debug_src_path = u8"woo/debug.wo";
const char* wo_stdlib_debug_src_data = {
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

        // Used for create a value with specify type, it's a dangergous function.
        extern("rslib_std_debug_empty_func")
        func __empty_function<T>():T;
    }
}
)" };

const char* wo_stdlib_vm_src_path = u8"woo/vm.wo";
const char* wo_stdlib_vm_src_data = {
u8R"(
namespace std
{
    using vm = gchandle;
    namespace vm
    {
        enum info_style
        {
            WO_DEFAULT = 0,

            WO_NOTHING = 1,
            WO_NEED_COLOR = 2,
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

struct wo_thread_pack
{
    std::thread* _thread;
    wo_vm _vm;
};

WO_API wo_api rslib_std_thread_create(wo_vm vm, wo_value args, size_t argc)
{
    wo_vm new_thread_vm = wo_sub_vm(vm, reinterpret_cast<wo::vmbase*>(vm)->stack_size);

    wo_value wo_calling_function = wo_push_valref(new_thread_vm, args);

    for (size_t argidx = argc - 1; argidx > 0; argidx--)
        wo_push_valref(new_thread_vm, args + argidx);


    auto* _vmthread = new std::thread([=]() {
        try
        {
            wo_invoke_value((wo_vm)new_thread_vm, wo_calling_function, argc - 1);
            wo_pop_stack((wo_vm)new_thread_vm);
        }
        catch (...)
        {
            // ?
        }

        wo_close_vm(new_thread_vm);
        });

    return wo_ret_gchandle(vm,
        new wo_thread_pack{ _vmthread , new_thread_vm },
        nullptr,
        [](void* wo_thread_pack_ptr)
        {
            if (((wo_thread_pack*)wo_thread_pack_ptr)->_thread->joinable())
                ((wo_thread_pack*)wo_thread_pack_ptr)->_thread->detach();
            delete ((wo_thread_pack*)wo_thread_pack_ptr)->_thread;
        });
}

WO_API wo_api rslib_std_thread_wait(wo_vm vm, wo_value args, size_t argc)
{
    wo_thread_pack* rtp = (wo_thread_pack*)wo_pointer(args);

    if (rtp->_thread->joinable())
        rtp->_thread->join();

    return wo_ret_nil(vm);
}

WO_API wo_api rslib_std_thread_abort(wo_vm vm, wo_value args, size_t argc)
{
    wo_thread_pack* rtp = (wo_thread_pack*)wo_pointer(args);
    return wo_ret_int(vm, wo_abort_vm(rtp->_vm));
}


WO_API wo_api rslib_std_thread_mutex_create(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_gchandle(vm,
        new std::shared_mutex,
        nullptr,
        [](void* mtx_ptr)
        {
            delete (std::shared_mutex*)mtx_ptr;
        });
}

WO_API wo_api rslib_std_thread_mutex_read(wo_vm vm, wo_value args, size_t argc)
{
    std::shared_mutex* smtx = (std::shared_mutex*)wo_pointer(args);
    smtx->lock_shared();

    return wo_ret_nil(vm);
}

WO_API wo_api rslib_std_thread_mutex_write(wo_vm vm, wo_value args, size_t argc)
{
    std::shared_mutex* smtx = (std::shared_mutex*)wo_pointer(args);
    smtx->lock();

    return wo_ret_nil(vm);
}

WO_API wo_api rslib_std_thread_mutex_read_end(wo_vm vm, wo_value args, size_t argc)
{
    std::shared_mutex* smtx = (std::shared_mutex*)wo_pointer(args);
    smtx->unlock_shared();

    return wo_ret_nil(vm);
}

WO_API wo_api rslib_std_thread_mutex_write_end(wo_vm vm, wo_value args, size_t argc)
{
    std::shared_mutex* smtx = (std::shared_mutex*)wo_pointer(args);
    smtx->unlock();

    return wo_ret_nil(vm);
}

////////////////////////////////////////////////////////////////////////

WO_API wo_api rslib_std_thread_spin_create(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_gchandle(vm,
        new wo::gcbase::rw_lock,
        nullptr,
        [](void* mtx_ptr)
        {
            delete (wo::gcbase::rw_lock*)mtx_ptr;
        });
}

WO_API wo_api rslib_std_thread_spin_read(wo_vm vm, wo_value args, size_t argc)
{
    wo::gcbase::rw_lock* smtx = (wo::gcbase::rw_lock*)wo_pointer(args);
    smtx->lock_shared();

    return wo_ret_nil(vm);
}

WO_API wo_api rslib_std_thread_spin_write(wo_vm vm, wo_value args, size_t argc)
{
    wo::gcbase::rw_lock* smtx = (wo::gcbase::rw_lock*)wo_pointer(args);
    smtx->lock();

    return wo_ret_nil(vm);
}

WO_API wo_api rslib_std_thread_spin_read_end(wo_vm vm, wo_value args, size_t argc)
{
    wo::gcbase::rw_lock* smtx = (wo::gcbase::rw_lock*)wo_pointer(args);
    smtx->unlock_shared();

    return wo_ret_nil(vm);
}

WO_API wo_api rslib_std_thread_spin_write_end(wo_vm vm, wo_value args, size_t argc)
{
    wo::gcbase::rw_lock* smtx = (wo::gcbase::rw_lock*)wo_pointer(args);
    smtx->unlock();

    return wo_ret_nil(vm);
}



const char* wo_stdlib_thread_src_path = u8"woo/thread.wo";
const char* wo_stdlib_thread_src_data = {
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

struct wo_co_waitable_base
{
    virtual ~wo_co_waitable_base() = default;
    virtual void wait_at_current_fthread() = 0;
};

template<typename T>
struct wo_co_waitable : wo_co_waitable_base
{
    wo::shared_pointer<T> sp_waitable;

    wo_co_waitable(const wo::shared_pointer<T>& sp)
        :sp_waitable(sp)
    {
        static_assert(std::is_base_of<wo::fwaitable, T>::value);
    }

    virtual void wait_at_current_fthread() override
    {
        wo::fthread::wait(sp_waitable);
    }
};

WO_API wo_api rslib_std_roroutine_launch(wo_vm vm, wo_value args, size_t argc)
{
    // rslib_std_roroutine_launch(...)   
    auto* _nvm = RSCO_WorkerPool::get_usable_vm(reinterpret_cast<wo::vmbase*>(vm));
    for (size_t i = argc - 1; i > 0; i--)
        wo_push_valref(reinterpret_cast<wo_vm>(_nvm), args + i);

    wo::shared_pointer<wo::RSCO_Waitter> gchandle_roroutine;

    auto functype = wo_valuetype(args + 0);
    if (WO_INTEGER_TYPE == functype)
        gchandle_roroutine = wo::fvmscheduler::new_work(_nvm, wo_int(args + 0), argc - 1);
    else if (WO_HANDLE_TYPE == functype)
        gchandle_roroutine = wo::fvmscheduler::new_work(_nvm, wo_handle(args + 0), argc - 1);
    else if (WO_CLOSURE_TYPE == functype)
        gchandle_roroutine = wo::fvmscheduler::new_work(_nvm, reinterpret_cast<wo::value*>(args + 0)->get()->closure, argc - 1);
    else
    {
        wo_fail(WO_FAIL_CALL_FAIL, "Unknown type to call.");
        return wo_ret_nil(vm);
    }


    return wo_ret_gchandle(vm,
        new wo_co_waitable<wo::RSCO_Waitter>(gchandle_roroutine),
        nullptr,
        [](void* gchandle_roroutine_ptr)
        {
            delete (wo_co_waitable_base*)gchandle_roroutine_ptr;
        });
}

WO_API wo_api rslib_std_roroutine_abort(wo_vm vm, wo_value args, size_t argc)
{
    auto* gchandle_roroutine = (wo_co_waitable<wo::RSCO_Waitter>*) wo_pointer(args);
    if (gchandle_roroutine->sp_waitable)
        gchandle_roroutine->sp_waitable->abort();

    return wo_ret_nil(vm);
}

WO_API wo_api rslib_std_roroutine_completed(wo_vm vm, wo_value args, size_t argc)
{
    auto* gchandle_roroutine = (wo_co_waitable<wo::RSCO_Waitter>*) wo_pointer(args);
    if (gchandle_roroutine->sp_waitable)
        return wo_ret_bool(vm, gchandle_roroutine->sp_waitable->complete_flag);
    else
        return wo_ret_bool(vm, true);
}

WO_API wo_api rslib_std_roroutine_pause_all(wo_vm vm, wo_value args, size_t argc)
{
    wo_coroutine_pauseall();
    return wo_ret_nil(vm);
}

WO_API wo_api rslib_std_roroutine_resume_all(wo_vm vm, wo_value args, size_t argc)
{
    wo_coroutine_resumeall();
    return wo_ret_nil(vm);
}

WO_API wo_api rslib_std_roroutine_stop_all(wo_vm vm, wo_value args, size_t argc)
{
    wo_coroutine_stopall();
    return wo_ret_nil(vm);
}

WO_API wo_api rslib_std_roroutine_sleep(wo_vm vm, wo_value args, size_t argc)
{
    using namespace std;
    wo::fthread::wait(wo::fvmscheduler::wait(wo_real(args)));

    return wo_ret_nil(vm);
}

WO_API wo_api rslib_std_roroutine_yield(wo_vm vm, wo_value args, size_t argc)
{
    wo::fthread::yield();
    return wo_ret_nil(vm);
}

WO_API wo_api rslib_std_roroutine_wait(wo_vm vm, wo_value args, size_t argc)
{
    wo_co_waitable_base* waitable = (wo_co_waitable_base*)wo_pointer(args);

    waitable->wait_at_current_fthread();
    return wo_ret_nil(vm);
}

const char* wo_stdlib_roroutine_src_path = u8"woo/co.wo";
const char* wo_stdlib_roroutine_src_data = {
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

WO_API wo_api rslib_std_macro_lexer_lex(wo_vm vm, wo_value args, size_t argc)
{
    wo::lexer* lex = (wo::lexer*)wo_pointer(args + 0);

    wo::lexer tmp_lex(wo::str_to_wstr(
        wo_string(args + 1)
    ), "macro" + lex->source_file + "_impl.wo");

    std::vector<std::pair<wo::lex_type, std::wstring>> lex_tokens;

    for (;;)
    {
        std::wstring result;
        auto token = tmp_lex.next(&result);

        if (token == +wo::lex_type::l_eof)
            break;

        lex_tokens.push_back({ token , result });
    }

    for (auto ri = lex_tokens.rbegin(); ri != lex_tokens.rend(); ri++)
        lex->push_temp_for_error_recover(ri->first, ri->second);

    return wo_ret_nil(vm);
}

WO_API wo_api rslib_std_macro_lexer_warning(wo_vm vm, wo_value args, size_t argc)
{
    wo::lexer* lex = (wo::lexer*)wo_pointer(args + 0);

    lex->lex_warning(0x0000, wo::str_to_wstr(wo_string(args + 1)).c_str());
    return wo_ret_nil(vm);
}

WO_API wo_api rslib_std_macro_lexer_error(wo_vm vm, wo_value args, size_t argc)
{
    wo::lexer* lex = (wo::lexer*)wo_pointer(args + 0);

    lex->lex_error(0x0000, wo::str_to_wstr(wo_string(args + 1)).c_str());
    return wo_ret_nil(vm);
}

WO_API wo_api rslib_std_macro_lexer_peek(wo_vm vm, wo_value args, size_t argc)
{
    wo::lexer* lex = (wo::lexer*)wo_pointer(args + 0);

    std::wstring out_result;
    auto token_type = lex->peek(&out_result);

    wo_set_string(args + 1, wo::wstr_to_str(out_result).c_str());

    return wo_ret_int(vm, (wo_integer_t)token_type);
}

WO_API wo_api rslib_std_macro_lexer_next(wo_vm vm, wo_value args, size_t argc)
{
    wo::lexer* lex = (wo::lexer*)wo_pointer(args + 0);

    std::wstring out_result;
    auto token_type = lex->next(&out_result);

    wo_set_string(args + 1, wo::wstr_to_str(out_result).c_str());

    return wo_ret_int(vm, (wo_integer_t)token_type);
}

WO_API wo_api rslib_std_macro_lexer_nextch(wo_vm vm, wo_value args, size_t argc)
{
    wo::lexer* lex = (wo::lexer*)wo_pointer(args + 0);

    wchar_t ch[2] = {};

    int readch = lex->next_one();

    if (readch == EOF)
        return wo_ret_string(vm, "");

    ch[0] = (wchar_t)readch;
    return wo_ret_string(vm, wo::wstr_to_str(ch).c_str());
}

WO_API wo_api rslib_std_macro_lexer_peekch(wo_vm vm, wo_value args, size_t argc)
{
    wo::lexer* lex = (wo::lexer*)wo_pointer(args + 0);

    wchar_t ch[2] = {};

    int readch = lex->peek_one();

    if (readch == EOF)
        return wo_ret_string(vm, "");

    ch[0] = (wchar_t)readch;
    return wo_ret_string(vm, wo::wstr_to_str(ch).c_str());
}

WO_API wo_api rslib_std_macro_lexer_current_path(wo_vm vm, wo_value args, size_t argc)
{
    wo::lexer* lex = (wo::lexer*)wo_pointer(args + 0);
    return wo_ret_string(vm, lex->source_file.c_str());
}

WO_API wo_api rslib_std_macro_lexer_current_rowno(wo_vm vm, wo_value args, size_t argc)
{
    wo::lexer* lex = (wo::lexer*)wo_pointer(args + 0);
    return wo_ret_int(vm, (wo_integer_t)lex->now_file_rowno);
}

WO_API wo_api rslib_std_macro_lexer_current_colno(wo_vm vm, wo_value args, size_t argc)
{
    wo::lexer* lex = (wo::lexer*)wo_pointer(args + 0);
    return wo_ret_int(vm, (wo_integer_t)lex->now_file_colno);
}


const char* wo_stdlib_macro_src_path = u8"woo/macro.wo";
const char* wo_stdlib_macro_src_data = {
u8R"(
import woo.std;

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

        extern("rslib_std_macro_lexer_current_rowno")
            func row(var lex:lexer) : int;

        extern("rslib_std_macro_lexer_current_colno")
            func col(var lex:lexer) : int;
    }
}

)" };