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
    return wo_ret_panic(vm, wo_string(args + 0));
}
WO_API wo_api rslib_std_halt(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_halt(vm, wo_string(args + 0));
}
WO_API wo_api rslib_std_throw(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_throw(vm, wo_string(args + 0));
}
WO_API wo_api rslib_std_fail(wo_vm vm, wo_value args, size_t argc)
{
    wo_fail(WO_FAIL_MEDIUM, wo_string(args + 0));
    return 0;
}

WO_API wo_api rslib_std_lengthof(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_int(vm, wo_lengthof(args));
}
WO_API wo_api rslib_std_make_dup(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_dup(vm, args + 0);
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

    size_t fnd_place = aim.find(match);
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

WO_API wo_api rslib_std_string_rfind(wo_vm vm, wo_value args, size_t argc)
{
    std::string aim = wo_string(args + 0);
    wo_string_t match = wo_string(args + 1);

    size_t fnd_place = aim.rfind(match);
    if (fnd_place > 0 && fnd_place < aim.size())
        return wo_ret_int(vm, (wo_integer_t)fnd_place);

    return wo_ret_int(vm, -1);
}

WO_API wo_api rslib_std_string_rfind_from(wo_vm vm, wo_value args, size_t argc)
{
    std::string aim = wo_string(args + 0);
    wo_string_t match = wo_string(args + 1);
    size_t from = (size_t)wo_string(args + 2);

    size_t fnd_place = aim.rfind(match, from);
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
    wo_value arr = wo_push_empty(vm);

    wo_set_arr(arr, 0);

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
    return wo_ret_val(vm, arr);
}

WO_API wo_api rslib_std_time_sec(wo_vm vm, wo_value args, size_t argc)
{
    static std::chrono::system_clock _sys_clock;
    static auto _first_invoke_time = _sys_clock.now();

    auto _time_ms = wo_real_t((_sys_clock.now() - _first_invoke_time).count() * std::chrono::system_clock::period::num)
        / std::chrono::system_clock::period::den;
    return wo_ret_real(vm, _time_ms);
}

WO_API wo_api rslib_std_input_readint(wo_vm vm, wo_value args, size_t argc)
{
    // Read int value from keyboard, always return valid input result;
    wo_int_t result;

    while (!(std::cin >> result))
    {
        char _useless_for_clear = 0;
        std::cin.clear();
        while (std::cin.readsome(&_useless_for_clear, 1));
    }
    return wo_ret_int(vm, result);
}

WO_API wo_api rslib_std_input_readreal(wo_vm vm, wo_value args, size_t argc)
{
    // Read real value from keyboard, always return valid input result;
    wo_real_t result;

    while (!(std::cin >> result))
    {
        char _useless_for_clear = 0;
        std::cin.clear();
        while (std::cin.readsome(&_useless_for_clear, 1));
    }
    return wo_ret_real(vm, result);
}

WO_API wo_api rslib_std_input_readstring(wo_vm vm, wo_value args, size_t argc)
{
    // Read real value from keyboard, always return valid input result;
    std::string result;

    while (!(std::cin >> result))
    {
        char _useless_for_clear = 0;
        std::cin.clear();
        while (std::cin.readsome(&_useless_for_clear, 1));
    }
    return wo_ret_string(vm, result.c_str());
}

WO_API wo_api rslib_std_input_readline(wo_vm vm, wo_value args, size_t argc)
{
    // Read real value from keyboard, always return valid input result;
    std::string result;

    while (!(std::getline(std::cin, result)))
    {
        char _useless_for_clear = 0;
        std::cin.clear();
        while (std::cin.readsome(&_useless_for_clear, 1));
    }
    return wo_ret_string(vm, result.c_str());
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
    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_array_resize(wo_vm vm, wo_value args, size_t argc)
{
    wo_arr_resize(args + 0, wo_int(args + 1), args + 2);
    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_array_insert(wo_vm vm, wo_value args, size_t argc)
{
    wo_arr_insert(args + 0, wo_int(args + 1), args + 2);
    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_array_swap(wo_vm vm, wo_value args, size_t argc)
{
    wo::value* arr1 = reinterpret_cast<wo::value*>(args + 0)->get();
    wo::value* arr2 = reinterpret_cast<wo::value*>(args + 1)->get();

    std::scoped_lock ssg1(arr1->array->gc_read_write_mx, arr2->array->gc_read_write_mx);

    if (wo::gc::gc_is_marking())
    {
        for (auto& elem : *arr1->array)
            arr1->array->add_memo(&elem);
        for (auto& elem : *arr2->array)
            arr2->array->add_memo(&elem);
    }

    arr1->array->swap(*arr2->array);

    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_array_copy(wo_vm vm, wo_value args, size_t argc)
{
    wo::value* arr1 = reinterpret_cast<wo::value*>(args + 0)->get();
    wo::value* arr2 = reinterpret_cast<wo::value*>(args + 1)->get();

    std::scoped_lock ssg1(arr1->array->gc_read_write_mx, arr2->array->gc_read_write_mx);

    if (wo::gc::gc_is_marking())
    {
        for (auto& elem : *arr1->array)
            arr1->array->add_memo(&elem);
    }

    *arr1->array = *arr2->array;

    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_array_empty(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_bool(vm, wo_arr_is_empty(args + 0));
}

WO_API wo_api rslib_std_array_add(wo_vm vm, wo_value args, size_t argc)
{
    wo_arr_add(args + 0, args + 1);
    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_array_connect(wo_vm vm, wo_value args, size_t argc)
{
    wo_value result = wo_push_empty(vm);
    wo_set_arr(result, 0);

    wo::value* arr_result = reinterpret_cast<wo::value*>(result);
    wo::value* arr1 = reinterpret_cast<wo::value*>(args + 0)->get();
    wo::value* arr2 = reinterpret_cast<wo::value*>(args + 1)->get();

    wo::gcbase::gc_write_guard wg1(arr_result->array);
    do
    {
        wo::gcbase::gc_read_guard rg2(arr1->array);
        *arr_result->array = *arr1->array;
    } while (0);
    do
    {
        wo::gcbase::gc_read_guard rg3(arr2->array);
        arr_result->array->insert(arr_result->array->end(),
            arr2->array->begin(), arr2->array->end());
    } while (0);

    return wo_ret_val(vm, result);
}

WO_API wo_api rslib_std_array_sub(wo_vm vm, wo_value args, size_t argc)
{
    wo_value result = wo_push_empty(vm);
    wo_set_arr(result, 0);

    wo::value* arr_result = reinterpret_cast<wo::value*>(result);
    wo::value* arr1 = reinterpret_cast<wo::value*>(args + 0)->get();

    wo::gcbase::gc_write_guard wg1(arr_result->array);
    wo::gcbase::gc_read_guard rg2(arr1->array);

    auto begin = wo_int(args + 1);
    if (begin > arr1->array->size())
        return wo_ret_panic(vm, "Index out of range when trying get sub array/vec.");

    if (argc == 2)
        arr_result->array->insert(arr_result->array->end(),
            arr1->array->begin() + begin, arr1->array->end());
    else
    {
        wo_assert(argc == 3);
        auto count = wo_int(args + 2);

        if (begin + count > arr1->array->size())
            return wo_ret_panic(vm, "Index out of range when trying get sub array/vec.");

        auto&& begin_iter = arr1->array->begin() + begin;
        auto&& end_iter = begin_iter + count;

        arr_result->array->insert(arr_result->array->end(),
            begin_iter, end_iter);
    }


    return wo_ret_val(vm, result);
}

WO_API wo_api rslib_std_array_pop(wo_vm vm, wo_value args, size_t argc)
{
    auto arrsz = wo_lengthof(args + 0);
    auto ret = wo_ret_val(vm, wo_arr_get(args + 0, arrsz - 1));
    if (arrsz)
        wo_arr_remove(args + 0, arrsz - 1);

    return ret;
}

WO_API wo_api rslib_std_array_dequeue(wo_vm vm, wo_value args, size_t argc)
{
    auto arrsz = wo_lengthof(args + 0);
    auto ret = wo_ret_val(vm, wo_arr_get(args + 0, 0));
    if (arrsz)
        wo_arr_remove(args + 0, 0);

    return ret;
}

WO_API wo_api rslib_std_array_remove(wo_vm vm, wo_value args, size_t argc)
{
    wo_arr_remove(args + 0, wo_int(args + 1));

    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_array_find(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_int(vm, wo_arr_find(args + 0, args + 1));
}

WO_API wo_api rslib_std_array_clear(wo_vm vm, wo_value args, size_t argc)
{
    wo_arr_clear(args);

    return wo_ret_void(vm);
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
    if (arr->type != wo::value::valuetype::array_type)
        return wo_ret_panic(vm, "DEBUG!");
    return wo_ret_gchandle(vm,
        new array_iter{ arr->array->begin(), arr->array->end(), 0 },
        args + 0,
        [](void* array_iter_t_ptr)
        {
            delete (array_iter*)array_iter_t_ptr;
        }
    );
}

WO_API wo_api rslib_std_array_iter_next(wo_vm vm, wo_value args, size_t argc)
{
    array_iter& iter = *(array_iter*)wo_pointer(args);

    if (iter.iter == iter.end_place)
        return wo_ret_option_none(vm);

    wo_value result_tuple = wo_push_struct(vm, 2);

    wo_set_int(wo_struct_get(result_tuple, 0), iter.index_count++); // key
    wo_set_val(wo_struct_get(result_tuple, 1), reinterpret_cast<wo_value>(&*(iter.iter++))); // val

    return wo_ret_option_val(vm, result_tuple);
}

WO_API wo_api rslib_std_map_find(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_bool(vm, wo_map_find(args + 0, args + 1));
}

WO_API wo_api rslib_std_map_set(wo_vm vm, wo_value args, size_t argc)
{
    wo_map_set(args + 0, args + 1, args + 2);
    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_map_only_get(wo_vm vm, wo_value args, size_t argc)
{
    wo_value result = wo_map_get(args + 0, args + 1);

    if (result)
        return wo_ret_option_val(vm, result);

    return wo_ret_option_none(vm);
}

WO_API wo_api rslib_std_map_contain(wo_vm vm, wo_value args, size_t argc)
{
    bool _map_has_indexed_val = wo_map_find(args + 0, args + 1);

    return wo_ret_int(vm, _map_has_indexed_val ? 1 : 0);
}

WO_API wo_api rslib_std_map_get_or_set_default(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_val(vm, wo_map_get_or_set_default(args + 0, args + 1, args + 2));
}

WO_API wo_api rslib_std_map_get_or_default(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_val(vm, wo_map_get_or_default(args + 0, args + 1, args + 2));
}

WO_API wo_api rslib_std_map_swap(wo_vm vm, wo_value args, size_t argc)
{
    wo::value* map1 = reinterpret_cast<wo::value*>(args + 0)->get();
    wo::value* map2 = reinterpret_cast<wo::value*>(args + 1)->get();

    std::scoped_lock ssg1(map1->dict->gc_read_write_mx, map2->dict->gc_read_write_mx);

    if (wo::gc::gc_is_marking())
    {
        for (auto& [key, elem] : *map1->dict)
        {
            map1->array->add_memo(&key);
            map1->array->add_memo(&elem);
        }
        for (auto& [key, elem] : *map2->dict)
        {
            map2->array->add_memo(&key);
            map2->array->add_memo(&elem);
        }
    }

    map1->dict->swap(*map2->dict);

    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_map_copy(wo_vm vm, wo_value args, size_t argc)
{
    wo::value* map1 = reinterpret_cast<wo::value*>(args + 0)->get();
    wo::value* map2 = reinterpret_cast<wo::value*>(args + 1)->get();

    std::scoped_lock ssg1(map1->dict->gc_read_write_mx, map2->dict->gc_read_write_mx);

    if (wo::gc::gc_is_marking())
    {
        for (auto& [key, elem] : *map1->dict)
        {
            map1->array->add_memo(&key);
            map1->array->add_memo(&elem);
        }
    }

    *map1->dict = *map2->dict;

    return wo_ret_void(vm);
}


WO_API wo_api rslib_std_map_remove(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_bool(vm, wo_map_remove(args + 0, args + 1));
}

WO_API wo_api rslib_std_map_empty(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_bool(vm, wo_map_is_empty(args + 0));
}

WO_API wo_api rslib_std_map_clear(wo_vm vm, wo_value args, size_t argc)
{
    wo_map_clear(args + 0);
    return wo_ret_void(vm);
}

struct map_iter
{
    using mapping_iter_t = decltype(std::declval<wo::dict_t>().begin());

    mapping_iter_t iter;
    mapping_iter_t end_place;
};

WO_API wo_api rslib_std_map_iter(wo_vm vm, wo_value args, size_t argc)
{
    wo::value* mapp = reinterpret_cast<wo::value*>(args)->get();

    return wo_ret_gchandle(vm,
        new map_iter{ mapp->dict->begin(), mapp->dict->end() },
        args + 0,
        [](void* array_iter_t_ptr)
        {
            delete (map_iter*)array_iter_t_ptr;
        }
    );
}

WO_API wo_api rslib_std_map_iter_next(wo_vm vm, wo_value args, size_t argc)
{
    map_iter& iter = *(map_iter*)wo_pointer(args);

    if (iter.iter == iter.end_place)
        return wo_ret_option_none(vm);

    wo_value result_tuple = wo_push_struct(vm, 2);

    wo_set_val(wo_struct_get(result_tuple, 0), reinterpret_cast<wo_value>(const_cast<wo::value*>(&iter.iter->first))); // key
    wo_set_val(wo_struct_get(result_tuple, 1), reinterpret_cast<wo_value>(&iter.iter->second)); // val
    iter.iter++;

    return wo_ret_option_val(vm, result_tuple);
}

WO_API wo_api rslib_std_parse_map_from_string(wo_vm vm, wo_value args, size_t argc)
{
    // TODO: wo_cast_value_from_str will create dict/array, to make sure gc-safe, wo should let gc pending when call this function.
    wo_value result_dict = wo_push_empty(vm);
    if (wo_cast_value_from_str(result_dict, wo_string(args + 0), WO_MAPPING_TYPE))
        return wo_ret_option_val(vm, result_dict);
    return wo_ret_option_none(vm);
}

WO_API wo_api rslib_std_parse_array_from_string(wo_vm vm, wo_value args, size_t argc)
{
    // TODO: wo_cast_value_from_str will create dict/array, to make sure gc-safe, wo should let gc pending when call this function.
    wo_value result_arr = wo_push_empty(vm);
    if (wo_cast_value_from_str(result_arr, wo_string(args + 0), WO_ARRAY_TYPE))
        return wo_ret_option_val(vm, result_arr);
    return wo_ret_option_none(vm);
}

WO_API wo_api rslib_std_create_chars_from_str(wo_vm vm, wo_value args, size_t argc)
{
    std::wstring buf = wo_str_to_wstr(wo_string(args + 0));
    wo_value result_array = wo_push_empty(vm);
    wo_set_arr(result_array, buf.size());

    for (size_t i = 0; i < buf.size(); ++i)
        wo_set_int(wo_arr_get(result_array, (wo_int_t)i), (wo_int_t)buf[i]);

    return wo_ret_val(vm, result_array);
}

WO_API wo_api rslib_std_create_str_by_asciis(wo_vm vm, wo_value args, size_t argc)
{
    std::wstring buf;
    wo_int_t size = wo_lengthof(args + 0);

    for (wo_int_t i = 0; i < size; ++i)
        buf += (wchar_t)wo_int(wo_arr_get(args + 0, i));

    return wo_ret_string(vm, wo::wstr_to_str(buf).c_str());
}

WO_API wo_api rslib_std_return_itself(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_val(vm, args + 0);
}

WO_API wo_api rslib_std_get_ascii_val_from_str(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_int(vm, (wo_int_t)wo_str_get_char(wo_string(args + 0), wo_int(args + 1)));
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
    return wo_ret_halt(vm, "Other type cannot be oped by 'sub'.");
}

WO_API wo_api rslib_std_thread_sleep(wo_vm vm, wo_value args, size_t argc)
{
    using namespace std;

    std::this_thread::sleep_for(wo_real(args) * 1s);
    return wo_ret_void(vm);
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

    if (ret)
        return wo_ret_option_val(vm, ret);
    return wo_ret_option_none(vm);
}

WO_API wo_api rslib_std_vm_has_compile_error(wo_vm vm, wo_value args, size_t argc)
{
    wo_vm vmm = (wo_vm)wo_pointer(args);
    return wo_ret_bool(vm, wo_has_compile_error(vmm));
}

WO_API wo_api rslib_std_vm_get_compile_error(wo_vm vm, wo_value args, size_t argc)
{
    wo_vm vmm = (wo_vm)wo_pointer(args);
    return wo_ret_string(vm, wo_get_compile_error(vmm, WO_DEFAULT));
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
    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_get_exe_path(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_string(vm, wo::exe_path());
}

WO_API wo_api rslib_std_get_extern_symb(wo_vm vm, wo_value args, size_t argc)
{
    wo_integer_t ext_symb = wo_extern_symb(vm, wo_string(args + 0));
    if (ext_symb)
        return wo_ret_option_int(vm, ext_symb);
    else
        return wo_ret_option_none(vm);
}

WO_API wo_api rslib_std_equal_byte(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_bool(vm, wo_equal_byte(args + 0, args + 1));
}

WO_API wo_api rslib_std_declval(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_panic(vm, "This function cannot be invoke");
}

const char* wo_stdlib_src_path = u8"woo/std.wo";
const char* wo_stdlib_src_data = {
u8R"(
public let const true = 1: bool, const false = 0: bool;

namespace std
{
    extern("rslib_std_throw") public func throw(msg: string) => void;
    extern("rslib_std_fail") public func fail(msg: string) => void;
    extern("rslib_std_halt") public func halt(msg: string) => void;
    extern("rslib_std_panic") public func panic(msg: string)=> void;

    extern("rslib_std_declval") public func declval<T>()=> T;
}

public using mutable<T> = struct {
    mut val : T
}
{
    public func create<T>(val: T)
    {
        return mutable:<T>{val = val};
    }
    public func set<T>(self: mutable<T>, val: T)
    {
        self.val = val;
    }
    public func get<T>(self: mutable<T>)
    {
        return self.val;
    }
}

public union option<T>
{
    value(T),
    none
}
namespace option
{
    public func reduce<T>(self: option<T>)
    {
        if (self->val() is option<anything>)
            match(self)
            {
            value(u)? return u->reduce();
            none? return none;
            }
        else
            return self;
    }
    public func bind<T, R>(self: option<T>, functor: (T)=>option<R>)=> option<R>
    {
        match(self)
        {
        value(u)? return functor(u);
        none? return none;
        }
    }
    public func ret<T>(value: T)
    {
        return option::value(value);
    }
    public func map<T, R>(self: option<T>, functor: (T)=>R) => option<R>
    {
        match(self)
        {
        value(x)?
            return option::value(functor(x));
        none?
            return option::none;
        }
    }
    public func or<T>(self: option<T>, orfunctor: ()=>T)
    {
        match(self)
        {
        value(x)?
            return self;
        none?
            return value(orfunctor());
        }
    }
    public func valor<T>(self: option<T>, default_val: T)
    {
        match(self)
        {
        value(x)?
            return x;
        none?
            return default_val;
        }
    }
    public func val<T>(self: option<T>)
    {
        match(self)
        {
        value(x)?
            return x;
        none?
            std::panic("Expect 'value' here, but get 'none'.");
        }
    }
    public func has<T>(self: option<T>)
    {
        match(self)
        {
        value(x)?
            return true;
        none?
            return false;
        }
    }
}
public union result<T, F>
{
    ok(T),
    err(F),
}
namespace result
{
    public func unwarp<T, F>(self: result<T, F>)=> T
    {
        match(self)
        {
        ok(v)? return v;
        err(e)? std::panic(F"An error was found when 'unwarp': {e}");
        }
    }
    public func isok<T, F>(self: result<T, F>)=> bool
    {
        match(self)
        {
        ok(_)? return true;
        err(_)? return false;
        }
    }
    public func iserr<T, F>(self: result<T, F>)=> bool
    {
        match(self)
        {
        ok(_)? return false;
        err(_)? return true;
        }
    }
    public func okay<T, F>(self: result<T, F>)=> option<T>
    {
        match(self)
        {
        ok(v)? return option::value(v);
        err(_)? return option::none;
        }
    }
    public func error<T, F>(self: result<T, F>)=> option<F>
    {
        match(self)
        {
        ok(_)? return option::none;
        err(e)? return option::value(e);
        }
    }
    public func succ<T, F>(self: result<T, F>)=> result<T, nothing>
    {
        match(self)
        {
        ok(v)? return ok(v);
        err(e)? std::panic(F"An error was found in 'succ': {e}");
        }
    }
    public func fail<T, F>(self: result<T, F>)=> result<nothing, F>
    {
        match(self)
        {
        ok(v)? std::panic(F"Current result is not failed.");
        err(e)? return err(e);
        }
    }
    public func map<T, F, U>(self: result<T, F>, functor: (T)=>U)=> result<U, F>
    {
        match(self)
        {
        ok(v)? return ok(functor(v));
        err(e)? return err(e);
        }
    }
    public func bind<T, F, U>(self: result<T, F>, functor: (T)=>result<U, F>)=> result<U, F>
    {
        match(self)
        {
        ok(v)? return functor(v);
        err(e)? return err(e);
        }
    }
    public func or<T, F, U>(self: result<T, F>, functor: (F)=>U)=> result<T, U>
    {
        match(self)
        {
        ok(v)? return ok(v);
        err(e)? return err(functor(e));
        }
    }
}

namespace std
{
    extern("rslib_std_print") public func print(...)=>int;
    extern("rslib_std_time_sec") public func time()=>real;

    public func println(...)
    {
        let c = print((...)...);
        print("\n");
        return c;
    }

    public func input<T>(validator: (T)=>bool)
        where std::declval:<T>() is int
            || std::declval:<T>() is real
            || std::declval:<T>() is string;
    {
        while (true)
        {
            if (std::declval:<T>() is int)
            {
                extern("rslib_std_input_readint") 
                public func _input_int()=>int;

                let result = _input_int();
                if (validator(result))
                    return result;
            }
            else if (std::declval:<T>() is real)
            {
                extern("rslib_std_input_readreal") 
                public func _input_real()=>real;

                let result = _input_real();
                if (validator(result))
                    return result;
            }
            else
            {
                extern("rslib_std_input_readstring") 
                public func _input_string()=>string;

                let result = _input_string();
                if (validator(result))
                    return result;
            }
        }
    }

    public func inputline<T>(parser: (string)=>option<T>)
    {
        while (true)
        {
            extern("rslib_std_input_readline") 
            public func _input_line()=>string;

            match (parser(_input_line()))
            {
            value(result)?
                return result;
            none?
                ; // do nothing
            }
        }
    }

    extern("rslib_std_randomint") 
        public func randint(from: int, to: int)=>int;

    extern("rslib_std_randomreal") 
        public func randreal(from:real, to:real)=>real;

    extern("rslib_std_break_yield") 
        public func yield()=>void;

    extern("rslib_std_thread_sleep")
        public func sleep(tm:real)=>void;
   
    extern("rslib_std_get_exe_path")
        public func exepath()=>string;

    extern("rslib_std_get_extern_symb")
        public func extern_symbol<T>(fullname:string)=> option<T>;

    extern("rslib_std_equal_byte")
    public func equalbyte<LT, RT>(a:LT, b:RT)=> bool;

    public func max<T>(a:T, b:T)
        where (a<b) is bool;
    {
        if (a < b)
            return b;
        return a;
    }

    public func min<T>(a:T, b:T)
        where (a<b) is bool;
    {
        if (a < b)
            return a;
        return b;
    }

    extern("rslib_std_make_dup")
    public func dup<T>(dupval: T)=> T;
}
public using char = int;
namespace string
{
    public func todict(val:string)=> option<dict<dynamic, dynamic>>
    {
        extern("rslib_std_parse_map_from_string") 
        func _tomap(val: string)=> option<dict<dynamic, dynamic>>;

        return _tomap(val);
    }
    public func toarray(val:string)=> option<array<dynamic>>
    {
        extern("rslib_std_parse_array_from_string") 
        func _toarray(val: string)=> option<array<dynamic>>;

        return _toarray(val);
    }

    public func chars(buf: string)=> array<char>
    {
        extern("rslib_std_create_chars_from_str") 
        func _chars(buf: string)=> array<char>;

        return _chars(buf);
    }

    extern("rslib_std_get_ascii_val_from_str") 
    public func getch(val:string, index: int)=> char;

    extern("rslib_std_lengthof") 
        public func len(val:string)=> int;

    extern("rslib_std_sub")
        public func sub(val:string, begin:int)=>string;

    extern("rslib_std_sub")
        public func subto(val:string, begin:int, length:int)=>string;
    
    extern("rslib_std_string_toupper")
        public func upper(val:string)=>string;

    extern("rslib_std_string_tolower")
        public func lower(val:string)=>string;

    extern("rslib_std_string_isspace")
        public func isspace(val:string)=>bool;

    extern("rslib_std_string_isalpha")
        public func isalpha(val:string)=> bool;

    extern("rslib_std_string_isalnum")
        public func isalnum(val:string)=> bool;

    extern("rslib_std_string_isnumber")
        public func isnumber(val:string)=> bool;

    extern("rslib_std_string_ishex")
        public func ishex(val:string)=> bool;

    extern("rslib_std_string_isoct")
        public func isoct(val:string)=> bool;

    extern("rslib_std_string_enstring")
        public func enstring(val:string)=>string;

    extern("rslib_std_string_destring")
        public func destring(val:string)=> string;

    extern("rslib_std_string_beginwith")
        public func beginwith(val:string, str:string)=> bool;

    extern("rslib_std_string_endwith")
        public func endwith(val:string, str:string)=> bool;

    extern("rslib_std_string_replace")
        public func replace(val:string, match_aim:string, str:string)=> string;

    extern("rslib_std_string_find")
        public func find(val:string, match_aim:string)=> int;

    extern("rslib_std_string_find_from")
        public func findfrom(val:string, match_aim:string, from: int)=> int;

    extern("rslib_std_string_rfind")
        public func rfind(val:string, match_aim:string)=> int;

    extern("rslib_std_string_rfind_from")
        public func rfindfrom(val:string, match_aim:string, from: int)=> int;

    extern("rslib_std_string_trim")
        public func trim(val:string)=>string;

    public func split(val:string, spliter:string)
    {
        extern("rslib_std_string_split")
            private func _split(val:string, spliter:string)=> array<string>;

        return _split(val, spliter);
    }
}
)" R"(
namespace array
{
    namespace unsafe
    {
        extern("rslib_std_return_itself") 
            private func astype<AimT, T>(val: array<T>)=> AimT;
    }

    public func append<T>(self: array<T>, elem: T)
    {
        let newarr = self->tovec;
        newarr->add(elem);

        return newarr->unsafe::astype:<array<T>>;
    }

    public func erase<T>(self: array<T>, index: int)
    {
        let newarr = self->tovec;
        newarr->remove(index);

        return newarr->unsafe::astype:<array<T>>;
    }
    public func inlay<T>(self: array<T>, index: int, insert_value: T)
    {
        let newarr = self->tovec;
        newarr->insert(index, insert_value);

        return newarr->unsafe::astype:<array<T>>;
    }

    extern("rslib_std_create_str_by_asciis") 
        public func str(buf: array<char>)=> string;

    extern("rslib_std_lengthof") 
        public func len<T>(val: array<T>)=> int;

    extern("rslib_std_make_dup")
        public func dup<T>(val: array<T>)=> array<T>;

    extern("rslib_std_make_dup")
        public func tovec<T>(val: array<T>)=> vec<T>;

    extern("rslib_std_array_empty")
        public func empty<T>(val: array<T>)=> bool;

    public func get<T>(a: array<T>, index: int)
    {
        return a[index];
    }

    extern("rslib_std_array_find")
        public func find<T>(val:array<T>, elem:T)=>int;

    public func findif<T>(val:array<T>, judger:(T)=>bool)
    {
        for (let i, v : val)
            if (judger(v))
                return i;
        return -1;            
    }

    public func forall<T>(val: array<T>, functor: (T)=>bool)
    {
        let result = []mut: vec<T>;
        for (let elem : val)
            if (functor(elem))
                result->add(elem);
        return result->unsafe::astype:<array<T>>;
    }

    public func bind<T, R>(val: array<T>, functor: (T)=>array<R>)
    {
        let result = []mut: vec<R>;
        for (let _, elem : val)
            for (let _, insert : functor(elem))
                result->add(insert);
        return result->unsafe::astype:<array<R>>;
    }

    public func map<T, R>(val: array<T>, functor: (T)=>R)
    {
        let result = []mut: vec<R>;
        for (let _, elem : val)
            result->add(functor(elem));
        return result->unsafe::astype:<array<R>>;
    }

    public func mapping<K, V>(val: array<(K, V)>)
    {
        let result = {}mut: map<K, V>;
        for (let _, (k, v) : val)
            result->set(k, v);
        return result->unsafe::astype:<dict<K, V>>;
    }

    public func reduce<T>(self: array<T>, reducer: (T, T)=>T)
    {
        if (self->empty)
            return option::none;
        
        let mut result = self[0];
        for (let mut i = 1; i < self->len; i+=1)
            result = reducer(result, self[i]);

        return option::value(result);
    }

    public func rreduce<T>(self: array<T>, reducer: (T, T)=>T)
    {
        if (self->empty)
            return option::none;
        
        let len = self->len;
        let mut result = self[len-1];
        for (let mut i = len-2; i >= 0; i-=1)
            result = reducer(self[i], result);

        return option::value(result);
    }

    public using iterator<T> = gchandle
    {
        extern("rslib_std_array_iter_next")
            public func next<T>(iter:iterator<T>)=>option<(int, T)>;
    
        public func iter<T>(iter:iterator<T>) { return iter; }
    }

    extern("rslib_std_array_iter")
        public func iter<T>(val:array<T>)=>iterator<T>;

    extern("rslib_std_array_connect")
    public func connect<T>(self: array<T>, another: array<T>)=> array<T>;

    extern("rslib_std_array_sub")
    public func sub<T>(self: array<T>, begin: int)=> array<T>;
    
    extern("rslib_std_array_sub")
    public func subto<T>(self: array<T>, begin: int, count: int)=> array<T>;
}

namespace vec
{
    namespace unsafe
    {   
        extern("rslib_std_return_itself") 
            private func astype<AimT, T>(val: vec<T>)=> AimT;
    }

    extern("rslib_std_create_str_by_asciis") 
        public func str(buf: vec<char>)=> string;

    extern("rslib_std_lengthof") 
        public func len<T>(val: vec<T>)=> int;

    extern("rslib_std_make_dup")
        public func dup<T>(val: vec<T>)=> vec<T>;

    extern("rslib_std_make_dup")
        public func toarray<T>(val: vec<T>)=> array<T>;

    extern("rslib_std_array_empty")
        public func empty<T>(val: vec<T>)=> bool;

    extern("rslib_std_array_resize") 
        public func resize<T>(val: vec<T>, newsz: int, init_val: T)=> void;

    extern("rslib_std_array_insert") 
        public func insert<T>(val: vec<T>, insert_place: int, insert_val: T)=> void;

    extern("rslib_std_array_swap") 
        public func swap<T>(val: vec<T>, another: vec<T>)=> void;

    extern("rslib_std_array_copy") 
        public func copy<T, C>(val: vec<T>, another: C<T>)=> void
            where std::declval:<C<T>>() is vec<T> || std::declval:<C<T>>() is array<T>;

    public func get<T>(a: vec<T>, index: int)
    {
        return a[index];
    }

    extern("rslib_std_array_add") 
        public func add<T>(val: vec<T>, elem: T)=>void;

    extern("rslib_std_array_connect")
    public func connect<T>(self: vec<T>, another: vec<T>)=> vec<T>;

    extern("rslib_std_array_sub")
    public func sub<T>(self: vec<T>, begin: int)=> vec<T>;
    
    extern("rslib_std_array_sub")
    public func subto<T>(self: vec<T>, begin: int, count: int)=> vec<T>;

    extern("rslib_std_array_pop") 
        public func pop<T>(val: vec<T>)=> T;  

    extern("rslib_std_array_dequeue") 
        public func dequeue<T>(val: vec<T>)=> T;  

    extern("rslib_std_array_remove")
        public func remove<T>(val:vec<T>, index:int)=>void;

    extern("rslib_std_array_find")
        public func find<T>(val:vec<T>, elem:T)=>int;

    public func findif<T>(val:vec<T>, judger:(T)=>bool)
    {
        for (let i, v : val)
            if (judger(v))
                return i;
        return -1;            
    }

    extern("rslib_std_array_clear")
        public func clear<T>(val:vec<T>)=>void;

    public func forall<T>(val: vec<T>, functor: (T)=>bool)
    {
        let result = []mut: vec<T>;
        for (let _, elem : val)
            if (functor(elem))
                result->add(elem);
        return result;
    }

    public func bind<T, R>(val: vec<T>, functor: (T)=>vec<R>)
    {
        let result = []mut: vec<R>;
        for (let _, elem : val)
            for (let _, insert : functor(elem))
                result->add(insert);
        return result;
    }

    public func map<T, R>(val: vec<T>, functor: (T)=>R)
    {
        let result = []mut: vec<R>;
        for (let elem : val)
            result->add(functor(elem));
        return result;
    }

    public func mapping<K, V>(val: vec<(K, V)>)
    {
        let result = {}mut: map<K, V>;
        for (let _, (k, v) : val)
            result->set(k, v);
        return result->unsafe::astype:<dict<K, V>>;
    }

    public func reduce<T>(self: vec<T>, reducer: (T, T)=>T)
    {
        if (self->empty)
            return option::none;
        
        let mut result = self[0];
        for (let mut i = 1; i < self->len; i+=1)
            result = reducer(result, self[i]);

        return option::value(result);
    }

    public func rreduce<T>(self: vec<T>, reducer: (T, T)=>T)
    {
        if (self->empty)
            return option::none;
        
        let len = self->len;
        let mut result = self[len-1];
        for (let mut i = len-2; i >= 0; i-=1)
            result = reducer(self[i], result);

        return option::value(result);
    }

    public using iterator<T> = gchandle
    {
        extern("rslib_std_array_iter_next")
            public func next<T>(iter:iterator<T>)=>option<(int, T)>;
    
        public func iter<T>(iter:iterator<T>) { return iter; }
    }

    extern("rslib_std_array_iter")
        public func iter<T>(val:vec<T>)=>iterator<T>;
}

namespace dict
{
    namespace unsafe
    {   
        extern("rslib_std_return_itself") 
            private func astype<AimT, KT, VT>(val: dict<KT, VT>)=> AimT;
    }

    public func bind<KT, VT, RK, RV>(val: dict<KT, VT>, functor: (KT, VT)=>dict<RK, RV>)
    {
        let result = {}mut: map<RK, RV>;
        for (let k, v : val)
            for (let rk, rv : functor(k, v))
                result->set(rk, rv);
        return result->unsafe::astype:<dict<RK, RV>>;
    }

    public func apply<KT, VT>(self: dict<KT, VT>, key: KT, val: VT)
    {
        let newmap = self->tomap;
        newmap->set(key, val);

        return newmap->unsafe::astype:<dict<K, V>>;
    }

    extern("rslib_std_lengthof") 
        public func len<KT, VT>(self: dict<KT, VT>)=>int;

    extern("rslib_std_make_dup")
        public func dup<KT, VT>(self: dict<KT, VT>)=> dict<KT, VT>;

    extern("rslib_std_make_dup")
        public func tomap<KT, VT>(self: dict<KT, VT>)=> map<KT, VT>;

    extern("rslib_std_map_find") 
        public func find<KT, VT>(self: dict<KT, VT>, index: KT)=> bool;

    public func findif<KT, VT>(self: dict<KT, VT>, judger:(KT)=>bool)
    {
        for (let k, _ : self)
            if (judger(k))
                return option::value(k);
        return option::none;            
    }

    extern("rslib_std_map_only_get") 
        public func get<KT, VT>(self: dict<KT, VT>, index: KT)=> option<VT>;

    extern("rslib_std_map_contain") 
        public func contain<KT, VT>(self: dict<KT, VT>, index: KT)=>bool;

    extern("rslib_std_map_get_or_default") 
        public func getor<KT, VT>(self: dict<KT, VT>, index: KT, default_val: VT)=> VT;

    extern("rslib_std_map_empty")
        public func empty<KT, VT>(self: dict<KT, VT>)=> bool;

    public using iterator<KT, VT> = gchandle
    {
        extern("rslib_std_map_iter_next")
            public func next<KT, VT>(iter:iterator<KT, VT>)=>option<(KT, VT)>;

        public func iter<KT, VT>(iter:iterator<KT, VT>) { return iter; }
    }

    extern("rslib_std_map_iter")
        public func iter<KT, VT>(self:dict<KT, VT>)=>iterator<KT, VT>;

    public func keys<KT, VT>(self: dict<KT, VT>)=> array<KT>
    {
        let result = []mut: vec<KT>;
        for (let key, val : self)
            result->add(key);
        return result->unsafe::astype:<array<KT>>;
    }
    public func vals<KT, VT>(self: dict<KT, VT>)=> array<VT>
    {
        let result = []mut: vec<VT>;
        for (let key, val : self)
            result->add(val);
        return result->unsafe::astype:<array<VT>>;
    }
    public func forall<KT, VT>(self: dict<KT, VT>, functor: (KT, VT)=>bool)=> dict<KT, VT>
    {
        let result = {}mut: map<KT, VT>;
        for (let key, val : self)
            if (functor(key, val))
                result->set(key, val);
        return result->unsafe::astype:<dict<KT, VT>>;
    }
    public func map<KT, VT, AT, BT>(self: dict<KT, VT>, functor: (KT, VT)=>(AT, BT))=> dict<AT, BT>
    {
        let result = {}mut: map<AT, BT>;
        for (let key, val : self)
        {
            let (nk, nv) = functor(key, val);
            result->set(nk, nv);
        }
        return result->unsafe::astype:<dict<AT, BT>>;
    }
    public func unmapping<KT, VT>(self: dict<KT, VT>)=> array<(KT, VT)>
    {
        let result = []mut: vec<(KT, VT)>;
        for (let key, val : self)
            result->add((key, val));
        return result->unsafe::astype:<array<(KT, VT)>>;
    }
}

namespace map
{
    namespace unsafe
    {   
        extern("rslib_std_return_itself") 
            private func astype<AimT, KT, VT>(val: map<KT, VT>)=> AimT;
    }

    public func bind<KT, VT, RK, RV>(val: map<KT, VT>, functor: (KT, VT)=>map<RK, RV>)
    {
        let result = {}mut: map<RK, RV>;
        for (let k, v : val)
            for (let rk, rv : functor(k, v))
                result->set(rk, rv);
        return result;
    }

    extern("rslib_std_map_set") 
        public func set<KT, VT>(self: map<KT, VT>, key: KT, val: VT)=> void;

    extern("rslib_std_lengthof") 
        public func len<KT, VT>(self: map<KT, VT>)=>int;

    extern("rslib_std_make_dup")
        public func dup<KT, VT>(self: map<KT, VT>)=> map<KT, VT>;

    extern("rslib_std_make_dup")
        public func todict<KT, VT>(self: map<KT, VT>)=> dict<KT, VT>;

    extern("rslib_std_map_find") 
        public func find<KT, VT>(self: map<KT, VT>, index: KT)=> bool;

    public func findif<KT, VT>(self: map<KT, VT>, judger:(KT)=>bool)
    {
        for (let k, _ : self)
            if (judger(k))
                return option::value(k);
        return option::none;            
    }

    extern("rslib_std_map_contain") 
        public func contain<KT, VT>(self: map<KT, VT>, index: KT)=>bool;

    extern("rslib_std_map_only_get") 
        public func get<KT, VT>(self: map<KT, VT>, index: KT)=> option<VT>;

    extern("rslib_std_map_get_or_default") 
        public func getor<KT, VT>(self: map<KT, VT>, index: KT, default_val: VT)=> VT;

    extern("rslib_std_map_get_or_set_default") 
        public func getorset<KT, VT>(self: map<KT, VT>, index: KT, default_val: VT)=>VT;

    extern("rslib_std_map_swap") 
        public func swap<KT, VT>(val: map<KT, VT>, another: map<KT, VT>)=> void;

    extern("rslib_std_map_copy") 
        public func copy<KT, VT>(val: map<KT, VT>, another: map<KT, VT>)=> void;

    extern("rslib_std_map_empty")
        public func empty<KT, VT>(self: map<KT, VT>)=> bool;

    extern("rslib_std_map_remove")
        public func remove<KT, VT>(self: map<KT, VT>, index: int)=> void;

    extern("rslib_std_map_clear")
        public func clear<KT, VT>(self: map<KT, VT>)=> void;

    public using iterator<KT, VT> = gchandle
    {
        extern("rslib_std_map_iter_next")
            public func next<KT, VT>(iter:iterator<KT, VT>)=>option<(KT, VT)>;

        public func iter<KT, VT>(iter:iterator<KT, VT>) { return iter; }
    }

    extern("rslib_std_map_iter")
        public func iter<KT, VT>(self:map<KT, VT>)=>iterator<KT, VT>;

    public func keys<KT, VT>(self: map<KT, VT>)=> array<KT>
    {
        let result = []mut: vec<KT>;
        for (let key, val : self)
            result->add(key);
        return result->unsafe::astype:<array<KT>>;
    }
    public func vals<KT, VT>(self: map<KT, VT>)=> array<VT>
    {
        let result = []mut: vec<VT>;
        for (let key, val : self)
            result->add(val);
        return result->unsafe::astype:<array<VT>>;
    }
    public func forall<KT, VT>(self: map<KT, VT>, functor: (KT, VT)=>bool)=> map<KT, VT>
    {
        let result = {}mut: map<KT, VT>;
        for (let key, val : self)
            if (functor(key, val))
                result->set(key, val);
        return result;
    }
    public func map<KT, VT, AT, BT>(self: map<KT, VT>, functor: (KT, VT)=>(AT, BT))=> map<AT, BT>
    {
        let result = {}mut: map<AT, BT>;
        for (let key, val : self)
        {
            let (nk, nv) = functor(key, val);
            result->set(nk, nv);
        }
        return result->unsafe::astype:<map<AT, BT>>;
    }
    public func unmapping<KT, VT>(self: map<KT, VT>)=> array<(KT, VT)>
    {
        let result = []mut: vec<(KT, VT)>;
        for (let key, val : self)
            result->add((key, val));
        return result->unsafe::astype:<array<(KT, VT)>>;
    }
}

namespace gchandle
{
    extern("rslib_std_gchandle_close")
        public func close(handle:gchandle)=>void;
}

public func assert(val: bool)
{
    if (!val)
        std::panic("Assert failed.");
}
public func assertmsg(val: bool, msg: string)
{
    if (!val)
        std::panic(F"Assert failed: {msg}");
}
)" };

WO_API wo_api rslib_std_debug_attach_default_debuggee(wo_vm vm, wo_value args, size_t argc)
{
    wo_attach_default_debuggee(vm);
    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_debug_disattach_default_debuggee(wo_vm vm, wo_value args, size_t argc)
{
    wo_disattach_and_free_debuggee(vm);
    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_debug_callstack_trace(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_string(vm, wo_debug_trace_callstack(vm, (size_t)wo_int(args + 0)));
}

WO_API wo_api rslib_std_debug_breakpoint(wo_vm vm, wo_value args, size_t argc)
{
    wo_break_immediately(vm);
    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_debug_invoke(wo_vm vm, wo_value args, size_t argc)
{
    for (size_t index = argc - 1; index > 0; index--)
        wo_push_val(vm, args + index);

    return wo_ret_val(vm, wo_invoke_value(vm, args, argc - 1));
}

WO_API wo_api rslib_std_debug_empty_func(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_void(vm);
}


const char* wo_stdlib_debug_src_path = u8"woo/debug.wo";
const char* wo_stdlib_debug_src_data = {
u8R"(
namespace std
{
    namespace debug
    {
        extern("rslib_std_debug_breakpoint")
            public func breakpoint()=>void;

        extern("rslib_std_debug_attach_default_debuggee")
            public func attach_debuggee()=>void;
        extern("rslib_std_debug_disattach_default_debuggee")
            public func disattach_debuggee()=>void;

        public func breakdown()
        {
            attach_debuggee();
            breakpoint();
        }

        extern("rslib_std_debug_callstack_trace")
            public func callstack(layer:int) => string;

        public func run<FT>(foo: FT, ...)
        {
            attach_debuggee();
            let result = (foo:(...)=>dynamic)(......);
            disattach_debuggee();
    
            return result;
        }

        extern("rslib_std_debug_invoke")
        public func invoke<FT>(foo:FT, ...)=>typeof(foo(......));

        // Used for create a value with specify type, it's a dangergous function.
        extern("rslib_std_debug_empty_func")
        public func __empty_function<T>()=>T;
    }
}
)" };

const char* wo_stdlib_vm_src_path = u8"woo/vm.wo";
const char* wo_stdlib_vm_src_data = {
u8R"(
import woo.std;
namespace std
{
    public using vm = gchandle
    {
        public enum info_style
        {
            WO_DEFAULT = 0,

            WO_NOTHING = 1,
            WO_NEED_COLOR = 2,
        }

        extern("rslib_std_vm_create")
        public func create()=>vm;

        extern("rslib_std_vm_load_src")
        public func loadsrc(vmhandle:vm, vfilepath:string, src:string)=>bool;

        extern("rslib_std_vm_load_file")
        public func loadfile(vmhandle:vm, vfilepath:string)=>bool;

        extern("rslib_std_vm_run")
        public func run(vmhandle:vm)=> option<dynamic>;
        
        extern("rslib_std_vm_has_compile_error")
        public func haserr(vmhandle:vm)=>bool;

        extern("rslib_std_vm_get_compile_error")
        public func errmsg(vmhandle:vm)=>string;

        extern("rslib_std_vm_virtual_source")
        public func virtualsrc(vfilepath:string, src:string, enable_overwrite:bool)=>bool;

        public func close(self: vm)
        {
            self:gchandle->close();
        }
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

    wo_value wo_calling_function = wo_push_val(new_thread_vm, args + 0);
    wo_int_t arg_count = 0;

    wo_value arg_pack = args + 1;
    arg_count = wo_lengthof(arg_pack);

    for (size_t i = arg_count; i > 0; i--)
        wo_push_val(new_thread_vm, wo_struct_get(arg_pack, (uint16_t)i - 1));

    auto* _vmthread = new std::thread([=]() {
        wo_invoke_value((wo_vm)new_thread_vm, wo_calling_function, arg_count);
        wo_pop_stack((wo_vm)new_thread_vm);
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
            delete (wo_thread_pack*)wo_thread_pack_ptr;
        });
}

WO_API wo_api rslib_std_thread_wait(wo_vm vm, wo_value args, size_t argc)
{
    wo_thread_pack* rtp = (wo_thread_pack*)wo_pointer(args);

    if (rtp->_thread->joinable())
        rtp->_thread->join();

    return wo_ret_void(vm);
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

    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_thread_mutex_write(wo_vm vm, wo_value args, size_t argc)
{
    std::shared_mutex* smtx = (std::shared_mutex*)wo_pointer(args);
    smtx->lock();

    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_thread_mutex_read_end(wo_vm vm, wo_value args, size_t argc)
{
    std::shared_mutex* smtx = (std::shared_mutex*)wo_pointer(args);
    smtx->unlock_shared();

    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_thread_mutex_write_end(wo_vm vm, wo_value args, size_t argc)
{
    std::shared_mutex* smtx = (std::shared_mutex*)wo_pointer(args);
    smtx->unlock();

    return wo_ret_void(vm);
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

    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_thread_spin_write(wo_vm vm, wo_value args, size_t argc)
{
    wo::gcbase::rw_lock* smtx = (wo::gcbase::rw_lock*)wo_pointer(args);
    smtx->lock();

    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_thread_spin_read_end(wo_vm vm, wo_value args, size_t argc)
{
    wo::gcbase::rw_lock* smtx = (wo::gcbase::rw_lock*)wo_pointer(args);
    smtx->unlock_shared();

    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_thread_spin_write_end(wo_vm vm, wo_value args, size_t argc)
{
    wo::gcbase::rw_lock* smtx = (wo::gcbase::rw_lock*)wo_pointer(args);
    smtx->unlock();

    return wo_ret_void(vm);
}

const char* wo_stdlib_thread_src_path = u8"woo/thread.wo";
const char* wo_stdlib_thread_src_data = {
u8R"(
namespace std
{
    public using thread = gchandle
    {
        extern("rslib_std_thread_create")
        public func create<FuncT, ArgTs>(thread_work: FuncT, args: ArgTs)=>thread
            where thread_work(args...) is anything;

        extern("rslib_std_thread_wait")
            public func wait(threadhandle : thread)=>void;

        extern("rslib_std_thread_abort")
            public func abort(threadhandle : thread)=>bool;
    }

    public using mutex = gchandle
    {
        extern("rslib_std_thread_mutex_create")
            public func create()=>mutex;

        extern("rslib_std_thread_mutex_read")
            public  func read(mtx : mutex)=>void;

        extern("rslib_std_thread_mutex_write")
            public func lock(mtx : mutex)=>void;

        extern("rslib_std_thread_mutex_read_end")
            public func unread(mtx : mutex)=>void;

        extern("rslib_std_thread_mutex_write_end")
            public func unlock(mtx : mutex)=>void;
    }

    public using spin = gchandle
    {
        extern("rslib_std_thread_spin_create")
            public func create()=>spin;

        extern("rslib_std_thread_spin_read")
            public func read(mtx : spin)=>void;

        extern("rslib_std_thread_spin_write")
            public func lock(mtx : spin)=>void;

        extern("rslib_std_thread_spin_read_end")
            public func unread(mtx : spin)=>void;

        extern("rslib_std_thread_spin_write_end")
            public func unlock(mtx : spin)=>void;
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
    wo_int_t arg_count = 0;

    wo_value wo_calling_function = wo_push_val((wo_vm)_nvm, args + 0);

    wo_value arg_pack = args + 1;
    arg_count = wo_lengthof(arg_pack);

    for (size_t i = arg_count; i > 0; i--)
        wo_push_val(reinterpret_cast<wo_vm>(_nvm), wo_struct_get(arg_pack, (uint16_t)i - 1));

    wo::shared_pointer<wo::RSCO_Waitter> gchandle_roroutine;

    auto functype = wo_valuetype(args + 0);
    if (WO_INTEGER_TYPE == functype)
        gchandle_roroutine = wo::fvmscheduler::new_work(_nvm, wo_int(args + 0), arg_count);
    else if (WO_HANDLE_TYPE == functype)
        gchandle_roroutine = wo::fvmscheduler::new_work(_nvm, wo_handle(args + 0), arg_count);
    else if (WO_CLOSURE_TYPE == functype)
        gchandle_roroutine = wo::fvmscheduler::new_work(_nvm, reinterpret_cast<wo::value*>(args + 0)->get()->closure, arg_count);
    else
        return wo_ret_halt(vm, "Unknown type to call.");

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

    return wo_ret_void(vm);
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
    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_roroutine_resume_all(wo_vm vm, wo_value args, size_t argc)
{
    wo_coroutine_resumeall();
    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_roroutine_stop_all(wo_vm vm, wo_value args, size_t argc)
{
    wo_coroutine_stopall();
    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_roroutine_sleep(wo_vm vm, wo_value args, size_t argc)
{
    using namespace std;
    wo::fthread::wait(wo::fvmscheduler::wait(wo_real(args)));

    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_roroutine_yield(wo_vm vm, wo_value args, size_t argc)
{
    wo::fthread::yield();
    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_roroutine_wait(wo_vm vm, wo_value args, size_t argc)
{
    wo_co_waitable_base* waitable = (wo_co_waitable_base*)wo_pointer(args);

    waitable->wait_at_current_fthread();
    return wo_ret_void(vm);
}

const char* wo_stdlib_roroutine_src_path = u8"woo/co.wo";
const char* wo_stdlib_roroutine_src_data = {
u8R"(
namespace std
{
    public using co = gchandle
    {
        extern("rslib_std_roroutine_launch")
            public func create<FT, ArgTs>(f: FT, args: ArgTs)=> co 
            where f(args...) is anything;
        
        extern("rslib_std_roroutine_abort")
            public func abort(co:co)=>void;

        extern("rslib_std_roroutine_completed")
            public func completed(co:co)=>bool;

        // Static functions:

        extern("rslib_std_roroutine_pause_all")
            public func pauseall()=>void;

        extern("rslib_std_roroutine_resume_all")
            public func resumeall()=>void;

        extern("rslib_std_roroutine_stop_all")
            public func stopall()=>void;

        extern("rslib_std_roroutine_sleep")
            public func sleep(time:real)=>void;

        extern("rslib_std_thread_yield")
            public func yield()=>bool;

        extern("rslib_std_roroutine_wait")
            public func wait(condi:co)=>void;
    }
}

)" };

WO_API wo_api rslib_std_macro_lexer_lex(wo_vm vm, wo_value args, size_t argc)
{
    wo::lexer* lex = (wo::lexer*)wo_pointer(args + 0);

    wo::lexer tmp_lex(wo::str_to_wstr(wo_string(args + 1)), "macro" + wo::wstr_to_str(*lex->source_file) + "_impl.wo");

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

    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_macro_lexer_error(wo_vm vm, wo_value args, size_t argc)
{
    wo::lexer* lex = (wo::lexer*)wo_pointer(args + 0);

    lex->lex_error(0x0000, wo::str_to_wstr(wo_string(args + 1)).c_str());
    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_macro_lexer_peek(wo_vm vm, wo_value args, size_t argc)
{
    wo::lexer* lex = (wo::lexer*)wo_pointer(args + 0);

    std::wstring out_result;
    auto token_type = lex->peek(&out_result);

    wo_value result = wo_push_empty(vm);
    wo_set_struct(result, 2);
    wo_set_int(wo_struct_get(result, 0), (wo_integer_t)token_type);
    wo_set_string(wo_struct_get(result, 1), wo::wstr_to_str(out_result).c_str());

    return wo_ret_val(vm, result);
}

WO_API wo_api rslib_std_macro_lexer_next(wo_vm vm, wo_value args, size_t argc)
{
    wo::lexer* lex = (wo::lexer*)wo_pointer(args + 0);

    std::wstring out_result;
    auto token_type = lex->next(&out_result);

    wo_value result = wo_push_empty(vm);
    wo_set_struct(result, 2);
    wo_set_int(wo_struct_get(result, 0), (wo_integer_t)token_type);
    wo_set_string(wo_struct_get(result, 1), wo::wstr_to_str(out_result).c_str());

    return wo_ret_val(vm, result);
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
    return wo_ret_string(vm, wo::wstr_to_str(*lex->source_file).c_str());
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
    public enum token_type
    {
        l_eof = -1,
        l_error = 0,

        l_empty,          // [empty]

        l_identifier,           // identifier.
        l_literal_integer,      // 1 233 0x123456 0b1101001 032
        l_literal_handle,       // 0L 256L 0xFFL
        l_literal_real,         // 0.2  0.  .235
        l_literal_string,       // "" "helloworld" @ "(println("hello");) " @

        l_format_string,        // f"..{  /  }..{ 
        l_format_string_end,    // }.."

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
        l_or,                   // |
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
        l_function_result,      // '=>'
        l_bind_monad,           // '>>'
        l_map_monad,            // '>>'

        l_left_brackets,        // (
        l_right_brackets,       // )
        l_left_curly_braces,    // {
        l_right_curly_braces,   // }

        l_question,   // ?

        l_import,               // import

        l_inf,
        l_nil,
        l_while,
        l_if,
        l_else,
        l_namespace,
        l_for,
        l_extern,

        l_let,
        l_mut,
        l_func,
        l_return,
        l_using,
        l_alias,
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
        l_lambda,
        l_at,
        l_where,
        l_operator,

        l_expect,
        l_union,
        l_match,
        l_struct
    }

    public using lexer = handle
    {
        extern("rslib_std_macro_lexer_lex")
            public func lex(lex:lexer, src:string)=>void;

        extern("rslib_std_macro_lexer_error")
            public func error(lex:lexer, msg:string)=>void;

        extern("rslib_std_macro_lexer_peek")
            public func peek(lex:lexer)=> (token_type, string);

        extern("rslib_std_macro_lexer_next")
            public func next(lex:lexer)=> (token_type, string);

        extern("rslib_std_macro_lexer_nextch")
            public func nextch(lex:lexer) => string;

        extern("rslib_std_macro_lexer_peekch")
            public func peekch(lex:lexer) => string;

        extern("rslib_std_macro_lexer_current_path")
            public func path(lex:lexer) => string;

        extern("rslib_std_macro_lexer_current_rowno")
            public func row(lex:lexer) => int;

        extern("rslib_std_macro_lexer_current_colno")
            public func col(lex:lexer) => int;

        public func try(self: lexer, token: token_type)=> option<string>
        {
            let (tok, res) = self->peek();
            if (token == tok)
            {
                self->next();
                return option::value(res);
            }
            return option::none;
        }
        public func expected(self: lexer, token: token_type)=> option<string>
        {
            let result = self->try(token);
            match(result)
            {
            none?       self->error("Unexpected token here.");
            value(_)?   ; /* do nothing */
            }
            return result
        }
    }
}

)" };