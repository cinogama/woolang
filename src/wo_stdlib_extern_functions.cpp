#define _CRT_SECURE_NO_WARNINGS
#include "wo_lang_extern_symbol_loader.hpp"
#include "wo_utf8.hpp"
#include "wo_vm.hpp"
#include "wo_io.hpp"

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
    return wo_ret_void(vm);
}
WO_API wo_api rslib_std_panic(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_panic(vm, wo_string(args + 0));
}
WO_API wo_api rslib_std_halt(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_halt(vm, wo_string(args + 0));
}

WO_API wo_api rslib_std_lengthof(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_int(vm, wo_lengthof(args));
}

WO_API wo_api rslib_std_str_bytelen(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_int(vm, wo_str_bytelen(args));
}

WO_API wo_api rslib_std_make_dup(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_dup(vm, args + 0);
}

WO_API wo_api rslib_std_string_toupper(wo_vm vm, wo_value args, size_t argc)
{
    std::wstring str = wo_str_to_wstr(wo_string(args + 0));
    for (auto& ch : str)
        ch = wo::lexer::lex_toupper(ch);
    return wo_ret_string(vm, wo_wstr_to_str(str.c_str()));
}

WO_API wo_api rslib_std_string_tolower(wo_vm vm, wo_value args, size_t argc)
{
    std::wstring str = wo_str_to_wstr(wo_string(args + 0));
    for (auto& ch : str)
        ch = wo::lexer::lex_tolower(ch);
    return wo_ret_string(vm, wo_wstr_to_str(str.c_str()));
}

WO_API wo_api rslib_std_string_isspace(wo_vm vm, wo_value args, size_t argc)
{
    std::wstring str = wo_str_to_wstr(wo_string(args + 0));
    if (!str.empty())
    {
        for (auto& ch : str)
            if (!wo::lexer::lex_isspace(ch))
                return wo_ret_bool(vm, false);
        return wo_ret_bool(vm, true);
    }
    return wo_ret_bool(vm, false);
}

WO_API wo_api rslib_std_string_isalpha(wo_vm vm, wo_value args, size_t argc)
{
    std::wstring str = wo_str_to_wstr(wo_string(args + 0));
    if (!str.empty())
    {
        for (auto& ch : str)
            if (!wo::lexer::lex_isalpha(ch))
                return wo_ret_bool(vm, false);
        return wo_ret_bool(vm, true);
    }
    return wo_ret_bool(vm, false);
}

WO_API wo_api rslib_std_string_isalnum(wo_vm vm, wo_value args, size_t argc)
{
    std::wstring str = wo_str_to_wstr(wo_string(args + 0));
    if (!str.empty())
    {
        for (auto& ch : str)
            if (!wo::lexer::lex_isalnum(ch))
                return wo_ret_bool(vm, false);
        return wo_ret_bool(vm, true);
    }
    return wo_ret_bool(vm, false);
}

WO_API wo_api rslib_std_string_isnumber(wo_vm vm, wo_value args, size_t argc)
{
    std::wstring str = wo_str_to_wstr(wo_string(args + 0));
    if (!str.empty())
    {
        for (auto& ch : str)
            if (!wo::lexer::lex_isdigit(ch))
                return wo_ret_bool(vm, false);
        return wo_ret_bool(vm, true);
    }
    return wo_ret_bool(vm, false);
}

WO_API wo_api rslib_std_string_ishex(wo_vm vm, wo_value args, size_t argc)
{
    std::wstring str = wo_str_to_wstr(wo_string(args + 0));
    if (!str.empty())
    {
        for (auto& ch : str)
            if (!wo::lexer::lex_isxdigit(ch))
                return wo_ret_bool(vm, false);
        return wo_ret_bool(vm, true);
    }
    return wo_ret_bool(vm, false);
}

WO_API wo_api rslib_std_string_isoct(wo_vm vm, wo_value args, size_t argc)
{
    std::wstring str = wo_str_to_wstr(wo_string(args + 0));
    if (!str.empty())
    {
        for (auto& ch : str)
            if (!wo::lexer::lex_isodigit(ch))
                return wo_ret_bool(vm, false);
        return wo_ret_bool(vm, true);
    }
    return wo_ret_bool(vm, false);
}

WO_API wo_api rslib_std_char_tostring(wo_vm vm, wo_value args, size_t argc)
{
    wo_char_t str[] = { wo_char(args + 0), 0 };
    return wo_ret_string(vm, wo_wstr_to_str(str));
}

WO_API wo_api rslib_std_char_toupper(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_char(vm, wo::lexer::lex_toupper(wo_char(args + 0)));
}

WO_API wo_api rslib_std_char_tolower(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_char(vm, wo::lexer::lex_tolower(wo_char(args + 0)));
}

WO_API wo_api rslib_std_char_isspace(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_bool(vm, wo::lexer::lex_isspace(wo_char(args + 0)));
}

WO_API wo_api rslib_std_char_isalpha(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_bool(vm, wo::lexer::lex_isalpha(wo_char(args + 0)));
}

WO_API wo_api rslib_std_char_isalnum(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_bool(vm, wo::lexer::lex_isalnum(wo_char(args + 0)));
}

WO_API wo_api rslib_std_char_isnumber(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_bool(vm, wo::lexer::lex_isdigit(wo_char(args + 0)));
}

WO_API wo_api rslib_std_char_ishex(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_bool(vm, wo::lexer::lex_isxdigit(wo_char(args + 0)));
}

WO_API wo_api rslib_std_char_isoct(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_bool(vm, wo::lexer::lex_isodigit(wo_char(args + 0)));
}

WO_API wo_api rslib_std_char_hexnum(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_bool(vm, wo::lexer::lex_hextonum(wo_char(args + 0)));
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
    if (fnd_place >= 0 && fnd_place < aim.size())
        return wo_ret_int(vm, (wo_integer_t)fnd_place);

    return wo_ret_int(vm, -1);
}

WO_API wo_api rslib_std_string_find_from(wo_vm vm, wo_value args, size_t argc)
{
    std::string aim = wo_string(args + 0);
    wo_string_t match = wo_string(args + 1);
    size_t from = (size_t)wo_int(args + 2);

    size_t fnd_place = aim.find(match, from);
    if (fnd_place >= from && fnd_place < aim.size())
        return wo_ret_int(vm, (wo_integer_t)fnd_place);

    return wo_ret_int(vm, -1);
}

WO_API wo_api rslib_std_string_rfind(wo_vm vm, wo_value args, size_t argc)
{
    std::string aim = wo_string(args + 0);
    wo_string_t match = wo_string(args + 1);

    size_t fnd_place = aim.rfind(match);
    if (fnd_place >= 0 && fnd_place < aim.size())
        return wo_ret_int(vm, (wo_integer_t)fnd_place);

    return wo_ret_int(vm, -1);
}

WO_API wo_api rslib_std_string_rfind_from(wo_vm vm, wo_value args, size_t argc)
{
    std::string aim = wo_string(args + 0);
    wo_string_t match = wo_string(args + 1);
    size_t from = (size_t)wo_int(args + 2);

    size_t fnd_place = aim.rfind(match, from);
    if (fnd_place >= from && fnd_place < aim.size())
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
    wo_value arr = wo_push_arr(vm, 0);

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
    return wo_ret_yield(vm);
}

WO_API wo_api rslib_std_array_resize(wo_vm vm, wo_value args, size_t argc)
{
    wo_arr_resize(args + 0, wo_int(args + 1), args + 2);
    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_array_descrease(wo_vm vm, wo_value args, size_t argc)
{
    auto new_size = wo_int(args + 1);

    if (new_size <= wo_lengthof(args + 0))
    {
        wo_arr_resize(args + 0, new_size, nullptr);
        return wo_ret_bool(vm, true);
    }
    return wo_ret_bool(vm, false);
}

WO_API wo_api rslib_std_array_shrink(wo_vm vm, wo_value args, size_t argc)
{
    auto newsz = wo_int(args + 1);
    if (newsz <= wo_lengthof(args + 0))
    {
        wo_arr_resize(args + 0, newsz, nullptr);
        return wo_ret_bool(vm, true);
    }
    return wo_ret_bool(vm, false);
}

WO_API wo_api rslib_std_array_insert(wo_vm vm, wo_value args, size_t argc)
{
    wo_arr_insert(args + 0, wo_int(args + 1), args + 2);
    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_array_swap(wo_vm vm, wo_value args, size_t argc)
{
    wo::value* arr1 = reinterpret_cast<wo::value*>(args + 0);
    wo::value* arr2 = reinterpret_cast<wo::value*>(args + 1);

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
    wo::value* arr1 = reinterpret_cast<wo::value*>(args + 0);
    wo::value* arr2 = reinterpret_cast<wo::value*>(args + 1);

    std::scoped_lock ssg1(arr1->array->gc_read_write_mx, arr2->array->gc_read_write_mx);

    if (wo::gc::gc_is_marking())
    {
        for (auto& elem : *arr1->array)
            arr1->array->add_memo(&elem);
    }

    *arr1->array->elem() = *arr2->array->elem();

    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_array_empty(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_bool(vm, wo_arr_is_empty(args + 0));
}

WO_API wo_api rslib_std_array_get(wo_vm vm, wo_value args, size_t argc)
{
    wo_value arr = args + 0;
    wo_integer_t idx = wo_int(args + 1);
    if (idx >= 0 && idx < wo_lengthof(arr))
        return wo_ret_option_val(vm, wo_arr_get(arr, idx));

    return wo_ret_option_none(vm);
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
    wo::value* arr1 = reinterpret_cast<wo::value*>(args + 0);
    wo::value* arr2 = reinterpret_cast<wo::value*>(args + 1);

    wo::gcbase::gc_write_guard wg1(arr_result->array);
    do
    {
        wo::gcbase::gc_read_guard rg2(arr1->array);
        *arr_result->array->elem() = *arr1->array->elem();
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
    wo::value* arr1 = reinterpret_cast<wo::value*>(args + 0);

    wo::gcbase::gc_write_guard wg1(arr_result->array);
    wo::gcbase::gc_read_guard rg2(arr1->array);

    auto begin = (size_t)wo_int(args + 1);
    if (begin > arr1->array->size())
        return wo_ret_panic(vm, "Index out of range when trying get sub array/vec.");

    if (argc == 2)
        arr_result->array->insert(arr_result->array->end(),
            arr1->array->begin() + begin, arr1->array->end());
    else
    {
        wo_assert(argc == 3);
        auto count = (size_t)wo_int(args + 2);

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
    wo::value* arr = reinterpret_cast<wo::value*>(args);
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
    wo::value* map1 = reinterpret_cast<wo::value*>(args + 0);
    wo::value* map2 = reinterpret_cast<wo::value*>(args + 1);

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
    wo::value* map1 = reinterpret_cast<wo::value*>(args + 0);
    wo::value* map2 = reinterpret_cast<wo::value*>(args + 1);

    std::scoped_lock ssg1(map1->dict->gc_read_write_mx, map2->dict->gc_read_write_mx);

    if (wo::gc::gc_is_marking())
    {
        for (auto& [key, elem] : *map1->dict)
        {
            map1->array->add_memo(&key);
            map1->array->add_memo(&elem);
        }
    }

    *map1->dict->elem() = *map2->dict->elem();

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
    wo::value* mapp = reinterpret_cast<wo::value*>(args);

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

WO_API wo_api rslib_std_take_token(wo_vm vm, wo_value args, size_t argc)
{
    wo_string_t input = wo_string(args + 0);
    wo_string_t format = wo_string(args + 1);

    std::string matching_format;

    while (*format)
    {
        matching_format += *format;
        if (*format == '%')
            matching_format += *format;
        ++format;
    }
    matching_format += "%zn";
    size_t token_length;

    if (sscanf(input, matching_format.c_str(), &token_length) >= 0)
        return wo_ret_option_string(vm, input + token_length);
    return wo_ret_option_none(vm);
}

WO_API wo_api rslib_std_take_string(wo_vm vm, wo_value args, size_t argc)
{
    wo_string_t input = wo_string(args + 0);
    size_t token_length;
    char string_buf[1024];

    if (sscanf(input, "%s%zn", string_buf, &token_length) == 1)
    {
        wo_value result = wo_push_struct(vm, 2);
        wo_set_string(wo_struct_get(result, 0), input + token_length);
        wo_set_string(wo_struct_get(result, 1), string_buf);
        return wo_ret_option_val(vm, result);
    }

    return wo_ret_option_none(vm);
}

WO_API wo_api rslib_std_take_int(wo_vm vm, wo_value args, size_t argc)
{
    wo_string_t input = wo_string(args + 0);
    size_t token_length;
    wo_integer_t integer;

    if (sscanf(input, "%lld%zn", &integer, &token_length) == 1)
    {
        wo_value result = wo_push_struct(vm, 2);
        wo_set_string(wo_struct_get(result, 0), input + token_length);
        wo_set_int(wo_struct_get(result, 1), integer);
        return wo_ret_option_val(vm, result);
    }

    return wo_ret_option_none(vm);
}

WO_API wo_api rslib_std_take_real(wo_vm vm, wo_value args, size_t argc)
{
    wo_string_t input = wo_string(args + 0);
    size_t token_length;
    wo_real_t real;

    if (sscanf(input, "%lf%zn", &real, &token_length) == 1)
    {
        wo_value result = wo_push_struct(vm, 2);
        wo_set_string(wo_struct_get(result, 0), input + token_length);
        wo_set_real(wo_struct_get(result, 1), real);
        return wo_ret_option_val(vm, result);
    }

    return wo_ret_option_none(vm);
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

WO_API wo_api rslib_std_create_wchars_from_str(wo_vm vm, wo_value args, size_t argc)
{
    std::wstring buf = wo_str_to_wstr(wo_string(args + 0));
    wo_value result_array = wo_push_arr(vm, buf.size());

    for (size_t i = 0; i < buf.size(); ++i)
        wo_set_int(wo_arr_get(result_array, (wo_int_t)i), (wo_int_t)(wo_handle_t)buf[i]);

    return wo_ret_val(vm, result_array);
}

WO_API wo_api rslib_std_create_chars_from_str(wo_vm vm, wo_value args, size_t argc)
{
    std::string buf = wo_string(args + 0);
    wo_value result_array = wo_push_arr(vm, buf.size());

    for (size_t i = 0; i < buf.size(); ++i)
        wo_set_int(wo_arr_get(result_array, (wo_int_t)i), (wo_int_t)(wo_handle_t)(unsigned char)buf[i]);

    return wo_ret_val(vm, result_array);
}

WO_API wo_api rslib_std_array_create(wo_vm vm, wo_value args, size_t argc)
{
    wo_integer_t arrsz = wo_int(args + 0);

    wo_value newarr = wo_push_arr(vm, arrsz);
    for (wo_integer_t i = 0; i < arrsz; ++i)
        wo_set_val(wo_arr_get(newarr, i), args + 1);

    return wo_ret_val(vm, newarr);
}

WO_API wo_api rslib_std_create_str_by_wchar(wo_vm vm, wo_value args, size_t argc)
{
    std::wstring buf;
    wo_int_t size = wo_lengthof(args + 0);

    for (wo_int_t i = 0; i < size; ++i)
        buf += (wchar_t)(wo_handle_t)wo_int(wo_arr_get(args + 0, i));

    return wo_ret_string(vm, wo::wstr_to_str(buf).c_str());
}

WO_API wo_api rslib_std_create_str_by_ascii(wo_vm vm, wo_value args, size_t argc)
{
    std::string buf;
    wo_int_t size = wo_lengthof(args + 0);

    for (wo_int_t i = 0; i < size; ++i)
        buf += (char)(unsigned char)(wo_handle_t)wo_int(wo_arr_get(args + 0, i));

    return wo_ret_string(vm, buf.c_str());
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

WO_API wo_api rslib_std_gchandle_close(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_bool(vm, wo_gchandle_close(args));
}

WO_API wo_api rslib_std_int_to_hex(wo_vm vm, wo_value args, size_t argc)
{
    char result[18];
    wo_integer_t val = wo_int(args + 0);
    if (val >= 0)
        sprintf(result, "%llX", (unsigned long long)val);
    else
        sprintf(result, "-%llX", (unsigned long long) - val);
    return wo_ret_string(vm, result);
}

WO_API wo_api rslib_std_int_to_oct(wo_vm vm, wo_value args, size_t argc)
{
    char result[24];
    wo_integer_t val = wo_int(args + 0);
    if (val >= 0)
        sprintf(result, "%llo", (unsigned long long)val);
    else
        sprintf(result, "-%llo", (unsigned long long) - val);
    return wo_ret_string(vm, result);
}

WO_API wo_api rslib_std_hex_to_int(wo_vm vm, wo_value args, size_t argc)
{
    wo_string_t str = wo_string(args + 0);
    while (*str && wo::lexer::lex_isspace(*str))
        ++str;
    unsigned long long result;
    if (*str != '-')
    {
        sscanf(str, "%llX", &result);
        return wo_ret_int(vm, (wo_integer_t)result);
    }
    else
    {
        sscanf(str, "-%llX", &result);
        return wo_ret_int(vm, -(wo_integer_t)result);
    }
}

WO_API wo_api rslib_std_oct_to_int(wo_vm vm, wo_value args, size_t argc)
{
    wo_string_t str = wo_string(args + 0);
    while (*str && wo::lexer::lex_isspace(*str))
        ++str;
    unsigned long long result;
    if (*str != '-')
    {
        sscanf(str, "%llo", &result);
        return wo_ret_int(vm, (wo_integer_t)result);
    }
    else
    {
        sscanf(str, "-%llo", &result);
        return wo_ret_int(vm, -(wo_integer_t)result);
    }
}

WO_API wo_api rslib_std_hex_to_handle(wo_vm vm, wo_value args, size_t argc)
{
    wo_string_t str = wo_string(args + 0);
    unsigned long long result;
    sscanf(str, "%llX", &result);
    return wo_ret_handle(vm, result);
}

WO_API wo_api rslib_std_oct_to_handle(wo_vm vm, wo_value args, size_t argc)
{
    wo_string_t str = wo_string(args + 0);
    unsigned long long result;
    sscanf(str, "%llo", &result);
    return wo_ret_handle(vm, result);
}

WO_API wo_api rslib_std_handle_to_hex(wo_vm vm, wo_value args, size_t argc)
{
    char result[18];
    wo_handle_t val = wo_handle(args + 0);

    sprintf(result, "%llX", (unsigned long long)val);

    return wo_ret_string(vm, result);
}

WO_API wo_api rslib_std_handle_to_oct(wo_vm vm, wo_value args, size_t argc)
{
    char result[24];
    wo_handle_t val = wo_handle(args + 0);

    sprintf(result, "%llo", (unsigned long long)val);

    return wo_ret_string(vm, result);
}


WO_API wo_api rslib_std_get_args(wo_vm vm, wo_value args, size_t argc)
{
    wo_integer_t argcarr = (wo_integer_t)wo::wo_args.size();
    wo_value argsarr = wo_push_arr(vm, argcarr);
    for (wo_integer_t i = 0; i < argcarr; ++i)
        wo_set_string(wo_arr_get(argsarr, i), wo::wo_args[(size_t)i].c_str());

    return wo_ret_val(vm, argsarr);
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

WO_API wo_api rslib_std_bit_or(wo_vm vm, wo_value args, size_t argc)
{
    auto result = wo_int(args + 0) | wo_int(args + 1);
    static_assert(std::is_same<decltype(result), wo_integer_t>::value);
    return wo_ret_int(vm, result);
}

WO_API wo_api rslib_std_bit_and(wo_vm vm, wo_value args, size_t argc)
{
    auto result = wo_int(args + 0) & wo_int(args + 1);
    static_assert(std::is_same<decltype(result), wo_integer_t>::value);
    return wo_ret_int(vm, result);
}

WO_API wo_api rslib_std_bit_xor(wo_vm vm, wo_value args, size_t argc)
{
    auto result = wo_int(args + 0) ^ wo_int(args + 1);
    static_assert(std::is_same<decltype(result), wo_integer_t>::value);
    return wo_ret_int(vm, result);
}

WO_API wo_api rslib_std_bit_not(wo_vm vm, wo_value args, size_t argc)
{
    auto result = ~wo_int(args + 0);
    static_assert(std::is_same<decltype(result), wo_integer_t>::value);
    return wo_ret_int(vm, result);
}

WO_API wo_api rslib_std_bit_shl(wo_vm vm, wo_value args, size_t argc)
{
    auto result = wo_int(args + 0) << wo_int(args + 1);
    static_assert(std::is_same<decltype(result), wo_integer_t>::value);
    return wo_ret_int(vm, result);
}
WO_API wo_api rslib_std_bit_ashr(wo_vm vm, wo_value args, size_t argc)
{
    auto result = wo_int(args + 0) >> wo_int(args + 1);
    static_assert(std::is_same<decltype(result), wo_integer_t>::value);
    return wo_ret_int(vm, result);
}
WO_API wo_api rslib_std_bit_shr(wo_vm vm, wo_value args, size_t argc)
{
    auto result = ((wo_handle_t)wo_int(args + 0) >> (wo_handle_t)wo_int(args + 1));
    static_assert(std::is_same<decltype(result), wo_handle_t>::value);
    return wo_ret_int(vm, (wo_integer_t)result);
}


const char* wo_stdlib_src_path = u8"woo/std.wo";
const char* wo_stdlib_src_data = {
u8R"(
namespace unsafe
{
    extern("rslib_std_return_itself") 
        public func cast<T, FromT>(val: FromT)=> pure T;
    
    extern("rslib_std_get_extern_symb")
        public func extsymbol<T>(fullname:string)=> pure option<T>;
}

namespace std
{
    extern("rslib_std_halt") public func halt(msg: string) => pure void;
    extern("rslib_std_panic") public func panic(msg: string)=> pure void;

    extern("rslib_std_declval") public func declval<T>()=> pure T;

    public alias origin_t<T> = typeof(\=std::declval:<T>();());

    extern("rslib_std_bit_or") public func bitor(a: int, b: int)=> pure int;
    extern("rslib_std_bit_and") public func bitand(a: int, b: int)=> pure int;
    extern("rslib_std_bit_xor") public func bitxor(a: int, b: int)=> pure int;
    extern("rslib_std_bit_not") public func bitnot(a: int)=> pure int;

    extern("rslib_std_bit_shl") public func bitshl(a: int, b: int)=> pure int;
    extern("rslib_std_bit_shr") public func bitshr(a: int, b: int)=> pure int;
    extern("rslib_std_bit_ashr") public func bitashr(a: int, b: int)=> pure int;
}

public using mutable<T> = struct {
    val : mut T
}
{
    public func create<T>(val: T)
    {
        return mutable:<T>{val = mut val};
    }
    public func set<T>(self: mutable<T>, val: T)
    {
        return self.val = val;
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
    public func bind<T, R>(self: option<T>, functor: (T)=>option<R>)
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
    public func map<T, R>(self: option<T>, functor: (T)=>R)
    {
        match(self)
        {
        value(x)?
            return option::value(functor(x));
        none?
            return option::none;
        }
    }
    public func or<T>(self: option<T>, orfunctor: ()=>T)=> T
    {
        match(self)
        {
        value(x)?
            return x;
        none?
            return orfunctor();
        }
    }
    public func orbind<T>(self: option<T>, orfunctor: ()=>option<T>)=> option<T>
    {
        match(self)
        {
        value(_)?
            return self;
        none?
            return orfunctor();
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
    public func flip<T, F>(self: result<T, F>)=> result<F, T>
    {
        match(self)
        {
        ok(v)? return err(v);
        err(e)? return ok(e);
        }
    }
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
        ok(v)? std::panic(F"Current result is ok({v}), is not failed.");
        err(e)? return err(e);
        }
    }
    public func map<T, F, U>(self: result<T, F>, functor: (T)=>U)=> result<pure U, F>
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
    public func or<T, F>(self: result<T, F>, functor: (F)=>T)=> T
    {
        match(self)
        {
        ok(v)? return v;
        err(e)? return functor(e);
        }
    }
    public func orbind<T, F>(self: result<T, F>, functor: (F)=>result<T, F>)=> result<T, F>
    {
        match(self)
        {
        ok(v)? return self;
        err(e)? return functor(e);
        }
    }
}
namespace std
{
    extern("rslib_std_print") public func print(...)=> impure void;
    extern("rslib_std_time_sec") public func time()=> impure real;

    public func println(...)
    {
        print((...)...);
        print("\n");
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
                public func _input_int()=>impure int;

                let result = _input_int();
                if (validator(result))
                    return result;
            }
            else if (std::declval:<T>() is real)
            {
                extern("rslib_std_input_readreal") 
                public func _input_real()=>impure real;

                let result = _input_real();
                if (validator(result))
                    return result;
            }
            else
            {
                extern("rslib_std_input_readstring") 
                public func _input_string()=>impure string;

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
            public func _input_line()=>impure string;

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
        public func randint(from: int, to: int)=>impure int;

    extern("rslib_std_randomreal") 
        public func randreal(from:real, to:real)=>impure real;

    extern("rslib_std_break_yield") 
        public func yield()=>pure void;

    extern("rslib_std_thread_sleep")
        public func sleep(tm:real)=>pure void;
   
    extern("rslib_std_get_args")
        public func args()=>pure array<string>;

    extern("rslib_std_get_exe_path")
        public func exepath()=>pure string;

    extern("rslib_std_equal_byte")
        public func issame<LT, RT>(a:LT, b:RT)=> pure bool;

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
    public func dup<T>(dupval: T)=> pure T;
}

public using cchar = char;

namespace char
{
    extern("rslib_std_char_tostring")
        public func tostring(val:char)=> pure string;

    extern("rslib_std_char_toupper")
        public func upper(val:char)=>pure char;

    extern("rslib_std_char_tolower")
        public func lower(val:char)=>pure char;

    extern("rslib_std_char_isspace")
        public func isspace(val:char)=>pure bool;

    extern("rslib_std_char_isalpha")
        public func isalpha(val:char)=> pure bool;

    extern("rslib_std_char_isalnum")
        public func isalnum(val:char)=> pure bool;

    extern("rslib_std_char_isnumber")
        public func isnumber(val:char)=> pure bool;

    extern("rslib_std_char_ishex")
        public func ishex(val:char)=> pure bool;

    extern("rslib_std_char_isoct")
        public func isoct(val:char)=> pure bool;

    extern("rslib_std_char_hexnum")
        public func hexnum(val:char)=> pure int;
}

namespace string
{
    extern("rslib_std_take_token") 
    public func take_token(datstr: string, expect_str: string)=> pure option<string>;

    extern("rslib_std_take_string") 
    public func take_string(datstr: string)=> pure option<(string, string)>;

    extern("rslib_std_take_int") 
    public func take_int(datstr: string)=> pure option<(string, int)>;

    extern("rslib_std_take_real") 
    public func take_real(datstr: string)=> pure option<(string, real)>;
    
    public func todict(val:string)=> option<dict<dynamic, dynamic>>
    {
        extern("rslib_std_parse_map_from_string") 
        func _tomap(val: string)=> pure option<dict<dynamic, dynamic>>;

        return _tomap(val);
    }
    public func toarray(val:string)=> pure option<array<dynamic>>
    {
        extern("rslib_std_parse_array_from_string") 
        func _toarray(val: string)=> pure option<array<dynamic>>;

        return _toarray(val);
    }

    extern("rslib_std_create_wchars_from_str")
    public func chars(buf: string)=> pure array<char>;

    extern("rslib_std_create_chars_from_str")
    public func cchars(buf: string)=> pure array<cchar>;

    extern("rslib_std_get_ascii_val_from_str") 
    public func getch(val:string, index: int)=> pure char;

    extern("rslib_std_lengthof") 
        public func len(val:string)=> pure int;

    extern("rslib_std_str_bytelen") 
        public func bytelen(val:string)=> pure int;

    extern("rslib_std_sub")
        public func sub(val:string, begin:int)=>pure string;

    extern("rslib_std_sub")
        public func subto(val:string, begin:int, length:int)=>pure string;
    
    extern("rslib_std_string_toupper")
        public func upper(val:string)=>pure string;

    extern("rslib_std_string_tolower")
        public func lower(val:string)=>pure string;

    extern("rslib_std_string_isspace")
        public func isspace(val:string)=>pure bool;

    extern("rslib_std_string_isalpha")
        public func isalpha(val:string)=> pure bool;

    extern("rslib_std_string_isalnum")
        public func isalnum(val:string)=> pure bool;

    extern("rslib_std_string_isnumber")
        public func isnumber(val:string)=> pure bool;

    extern("rslib_std_string_ishex")
        public func ishex(val:string)=> pure bool;

    extern("rslib_std_string_isoct")
        public func isoct(val:string)=> pure bool;

    extern("rslib_std_string_enstring")
        public func enstring(val:string)=>pure string;

    extern("rslib_std_string_destring")
        public func destring(val:string)=> pure string;

    extern("rslib_std_string_beginwith")
        public func beginwith(val:string, str:string)=> pure bool;

    extern("rslib_std_string_endwith")
        public func endwith(val:string, str:string)=> pure bool;

    extern("rslib_std_string_replace")
        public func replace(val:string, match_aim:string, str:string)=> pure string;

    extern("rslib_std_string_find")
        public func find(val:string, match_aim:string)=> pure int;

    extern("rslib_std_string_find_from")
        public func findfrom(val:string, match_aim:string, from: int)=> pure int;

    extern("rslib_std_string_rfind")
        public func rfind(val:string, match_aim:string)=> pure int;

    extern("rslib_std_string_rfind_from")
        public func rfindfrom(val:string, match_aim:string, from: int)=> pure int;

    extern("rslib_std_string_trim")
        public func trim(val:string)=>pure string;

    extern("rslib_std_string_split")
        public func split(val:string, spliter:string)=> pure array<string>;
}
)" R"(
namespace array
{
    extern("rslib_std_array_create") 
        public func create<T>(sz: int, init_val: T)=> pure array<T>;

    public func append<T>(self: array<T>, elem: T)
    {
        let newarr = self->tovec;
        do impure newarr->add(elem);

        return newarr->unsafe::cast:<array<T>>;
    }

    public func erase<T>(self: array<T>, index: int)
    {
        let newarr = self->tovec;
        do impure do newarr->remove(index);

        return newarr->unsafe::cast:<array<T>>;
    }
    public func inlay<T>(self: array<T>, index: int, insert_value: T)
    {
        let newarr = self->tovec;
        do impure newarr->insert(index, insert_value);

        return newarr->unsafe::cast:<array<T>>;
    }

    extern("rslib_std_create_str_by_wchar") 
        public func str(buf: array<char>)=> pure string;

    extern("rslib_std_create_str_by_ascii") 
        public func cstr(buf: array<cchar>)=> pure string;

    extern("rslib_std_lengthof") 
        public func len<T>(val: array<T>)=> pure int;

    extern("rslib_std_make_dup")
        public func dup<T>(val: array<T>)=> pure array<T>;

    extern("rslib_std_make_dup")
        public func tovec<T>(val: array<T>)=> pure vec<T>;

    extern("rslib_std_array_empty")
        public func empty<T>(val: array<T>)=> pure bool;

    extern("rslib_std_array_get")
        public func get<T>(a: array<T>, index: int)=> pure option<std::origin_t<T>>;

    extern("rslib_std_array_find")
        public func find<T>(val:array<T>, elem:T)=> pure int;

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
        for (let _, elem : val)
            if (functor(elem))
                do impure result->add(elem);
        return result->unsafe::cast:<array<T>>;
    }

    public func bind<T, R>(val: array<T>, functor: (T)=>array<R>)
    {
        let result = []mut: vec<R>;
        for (let _, elem : val)
            for (let _, insert : functor(elem))
                do impure result->add(insert);
        return result->unsafe::cast:<array<R>>;
    }

    public func map<T, R>(val: array<T>, functor: (T)=>R)
    {
        let result = []mut: vec<pure R>;
        for (let _, elem : val)
        {
            let r = functor(elem);
            do impure result->add(r);
        }
        return result->unsafe::cast:<array<pure R>>;
    }

    public func mapping<K, V>(val: array<(K, V)>)
    {
        let result = {}mut: map<pure K, pure V>;
        for (let _, (k, v) : val)
            do impure result->set(k, v);
        return result->unsafe::cast:<dict<pure K, pure V>>;
    }

    public func reduce<T>(self: array<T>, reducer: (T, T)=>T)
    {
        if (self->empty)
            return option::none;
        
        do impure
        {
            let mut result = self[0];
            for (let mut i = 1; i < self->len; i+=1)
                result = reducer(result, self[i]);
            
            return option::value(result);
        }
    }

    public func rreduce<T>(self: array<T>, reducer: (T, T)=>T)
    {
        if (self->empty)
            return option::none;
        
        do impure
        {
            let len = self->len;
            let mut result = self[len-1];
            for (let mut i = len-2; i >= 0; i-=1)
                result = reducer(self[i], result);

            return option::value(result);
        }
    }

    public using iterator<T> = gchandle
    {
        extern("rslib_std_array_iter_next")
            public func next<T>(iter:iterator<T>)=>pure option<(int, std::origin_t<T>)>;
    
        public func iter<T>(iter:iterator<T>) { return iter; }
    }

    extern("rslib_std_array_iter")
        public func iter<T>(val:array<T>)=>pure iterator<T>;

    extern("rslib_std_array_connect")
    public func connect<T>(self: array<T>, another: array<T>)=> pure array<T>;

    extern("rslib_std_array_sub")
    public func sub<T>(self: array<T>, begin: int)=> pure array<T>;
    
    extern("rslib_std_array_sub")
    public func subto<T>(self: array<T>, begin: int, count: int)=> pure array<T>;
}

namespace vec
{
    extern("rslib_std_array_create") 
        public func create<T>(sz: int, init_val: T)=> pure vec<T>;

    extern("rslib_std_create_str_by_wchar") 
        public func str(buf: vec<char>)=> pure string;

    extern("rslib_std_create_str_by_ascii") 
        public func cstr(buf: vec<cchar>)=> pure string;

    extern("rslib_std_lengthof") 
        public func len<T>(val: vec<T>)=> pure int;

    extern("rslib_std_make_dup")
        public func dup<T>(val: vec<T>)=> pure vec<T>;

    extern("rslib_std_make_dup")
        public func toarray<T>(val: vec<T>)=> pure array<T>;

    extern("rslib_std_array_empty")
        public func empty<T>(val: vec<T>)=> impure bool;

    extern("rslib_std_array_resize") 
        public func resize<T>(val: vec<T>, newsz: int, init_val: T)=> impure void;

    extern("rslib_std_array_descrease")
        public func decrease<T>(self: vec<T>, sz: int)=> impure bool;

    extern("rslib_std_array_shrink")
        public func shrink<T>(val: vec<T>, newsz: int)=> impure bool;

    extern("rslib_std_array_insert") 
        public func insert<T>(val: vec<T>, insert_place: int, insert_val: T)=> impure void;

    extern("rslib_std_array_swap") 
        public func swap<T>(val: vec<T>, another: vec<T>)=> impure void;

    extern("rslib_std_array_copy") 
        public func copy<T, C>(val: vec<T>, another: C<T>)=> impure void
            where std::declval:<C<T>>() is vec<T> || std::declval:<C<T>>() is array<T>;

    extern("rslib_std_array_get")
        public func get<T>(a: vec<T>, index: int)=> pure option<std::origin_t<T>>;

    extern("rslib_std_array_add") 
        public func add<T>(val: vec<T>, elem: T)=> impure void;

    extern("rslib_std_array_connect")
    public func connect<T>(self: vec<T>, another: vec<T>)=> impure vec<T>;

    extern("rslib_std_array_sub")
    public func sub<T>(self: vec<T>, begin: int)=> pure vec<T>;
    
    extern("rslib_std_array_sub")
    public func subto<T>(self: vec<T>, begin: int, count: int)=> pure vec<T>;

    extern("rslib_std_array_pop") 
        public func pop<T>(val: vec<T>)=> impure std::origin_t<T>;  

    extern("rslib_std_array_dequeue") 
        public func dequeue<T>(val: vec<T>)=> impure std::origin_t<T>;  

    extern("rslib_std_array_remove")
        public func remove<T>(val:vec<T>, index:int)=> impure bool;

    extern("rslib_std_array_find")
        public func find<T>(val:vec<T>, elem:T)=> pure int;

    public func findif<T>(val:vec<T>, judger:(T)=>bool)
    {
        for (let i, v : val)
            if (judger(v))
                return i;
        return -1;            
    }

    extern("rslib_std_array_clear")
        public func clear<T>(val:vec<T>)=> impure void;

    public func forall<T>(val: vec<T>, functor: (T)=>bool)
    {
        let result = []mut: vec<pure T>;
        for (let _, elem : val)
            if (functor(elem))
                result->add(elem);
        return result;
    }

    public func bind<T, R>(val: vec<T>, functor: (T)=>vec<R>)
    {
        let result = []mut: vec<pure R>;
        for (let _, elem : val)
            for (let _, insert : functor(elem))
                result->add(insert);
        return result;
    }

    public func map<T, R>(val: vec<T>, functor: (T)=>R)
    {
        let result = []mut: vec<pure R>;
        for (let _, elem : val)
            result->add(functor(elem));
        return result;
    }

    public func mapping<K, V>(val: vec<(K, V)>)
    {
        let result = {}mut: map<pure K, pure V>;
        for (let _, (k, v) : val)
            result->set(k, v);
        return result->unsafe::cast:<dict<pure K, pure V>>;
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
            public func next<T>(iter:iterator<T>)=> pure option<(int, std::origin_t<T>)>;
    
        public func iter<T>(iter:iterator<T>) { return iter; }
    }

    extern("rslib_std_array_iter")
        public func iter<T>(val:vec<T>)=> pure iterator<T>;
}

namespace dict
{
    public func bind<KT, VT, RK, RV>(val: dict<KT, VT>, functor: (KT, VT)=>dict<RK, RV>)
    {
        let result = {}mut: map<pure RK, pure RV>;
        for (let k, v : val)
            for (let rk, rv : functor(k, v))
                do impure result->set(rk, rv);
        return result->unsafe::cast:<dict<pure RK, pure RV>>;
    }

    public func apply<KT, VT>(self: dict<KT, VT>, key: KT, val: VT)
    {
        let newmap = self->tomap;
        do impure newmap->set(key, val);

        return newmap->unsafe::cast:<dict<KT, VT>>;
    }
)" R"(
    extern("rslib_std_lengthof") 
        public func len<KT, VT>(self: dict<KT, VT>)=> pure int;

    extern("rslib_std_make_dup")
        public func dup<KT, VT>(self: dict<KT, VT>)=> pure dict<KT, VT>;

    extern("rslib_std_make_dup")
        public func tomap<KT, VT>(self: dict<KT, VT>)=> pure map<KT, VT>;

    public func findif<KT, VT>(self: dict<KT, VT>, judger:(KT)=>bool)
    {
        for (let k, _ : self)
            if (judger(k))
                do impure return option::value(k);
        return option::none;            
    }

    extern("rslib_std_map_only_get") 
        public func get<KT, VT>(self: dict<KT, VT>, index: KT)=> pure option<std::origin_t<VT>>;

    extern("rslib_std_map_find") 
        public func contain<KT, VT>(self: dict<KT, VT>, index: KT)=>pure bool;

    extern("rslib_std_map_get_or_default") 
        public func getor<KT, VT>(self: dict<KT, VT>, index: KT, default_val: VT)=> pure std::origin_t<VT>;

    extern("rslib_std_map_empty")
        public func empty<KT, VT>(self: dict<KT, VT>)=> pure bool;

    public func erase<KT, VT>(self: dict<KT, VT>, index: KT)
    {
        let newmap = self->tomap;
        do impure do newmap->remove(index);

        return newmap->unsafe::cast:<dict<KT, VT>>;
    }

    public using iterator<KT, VT> = gchandle
    {
        extern("rslib_std_map_iter_next")
            public func next<KT, VT>(iter:iterator<KT, VT>)=>pure option<(KT, std::origin_t<VT>)>;

        public func iter<KT, VT>(iter:iterator<KT, VT>) { return iter; }
    }

    extern("rslib_std_map_iter")
        public func iter<KT, VT>(self:dict<KT, VT>)=>pure iterator<KT, VT>;

    public func keys<KT, VT>(self: dict<KT, VT>)=> array<KT>
    {
        let result = []mut: vec<pure KT>;
        for (let key, _ : self)
            do impure result->add(key);
        return result->unsafe::cast:<array<pure KT>>;
    }
    public func vals<KT, VT>(self: dict<KT, VT>)=> array<VT>
    {
        let result = []mut: vec<pure VT>;
        for (let _, val : self)
            do impure result->add(val);
        return result->unsafe::cast:<array<pure VT>>;
    }
    public func forall<KT, VT>(self: dict<KT, VT>, functor: (KT, VT)=>bool)
    {
        let result = {}mut: map<pure KT, pure VT>;
        for (let key, val : self)
            if (functor(key, val))
                do impure result->set(key, val);
        return result->unsafe::cast:<dict<pure KT, pure VT>>;
    }
    public func map<KT, VT, AT, BT>(self: dict<KT, VT>, functor: (KT, VT)=>(AT, BT))
    {
        let result = {}mut: map<pure AT, pure BT>;
        for (let key, val : self)
        {
            let (nk, nv) = functor(key, val);
            do impure result->set(nk, nv);
        }
        return result->unsafe::cast:<dict<pure AT, pure BT>>;
    }
    public func unmapping<KT, VT>(self: dict<KT, VT>)
    {
        let result = []mut: vec<(std::origin_t<KT>, std::origin_t<VT>)>;
        for (let key, val : self)
            do impure result->add((key, val));
        return result->unsafe::cast:<array<(KT, VT)>>;
    }
}

namespace map
{
    public func bind<KT, VT, RK, RV>(val: map<KT, VT>, functor: (KT, VT)=>map<RK, RV>)
    {
        let result = {}mut: map<pure RK, pure RV>;
        for (let k, v : val)
            for (let rk, rv : functor(k, v))
                result->set(rk, rv);
        return result;
    }

    extern("rslib_std_map_set") 
        public func set<KT, VT>(self: map<KT, VT>, key: KT, val: VT)=> impure void;

    extern("rslib_std_lengthof") 
        public func len<KT, VT>(self: map<KT, VT>)=>impure int;

    extern("rslib_std_make_dup")
        public func dup<KT, VT>(self: map<KT, VT>)=> impure map<KT, VT>;

    extern("rslib_std_make_dup")
        public func todict<KT, VT>(self: map<KT, VT>)=> impure dict<KT, VT>;

    public func findif<KT, VT>(self: map<KT, VT>, judger:(KT)=>bool)
    {
        for (let k, _ : self)
            if (judger(k))
                return option::value(k);
        return option::none;            
    }

    extern("rslib_std_map_find") 
        public func contain<KT, VT>(self: map<KT, VT>, index: KT)=>impure bool;

    extern("rslib_std_map_only_get") 
        public func get<KT, VT>(self: map<KT, VT>, index: KT)=> impure option<std::origin_t<VT>>;

    extern("rslib_std_map_get_or_default") 
        public func getor<KT, VT>(self: map<KT, VT>, index: KT, default_val: VT)=> impure std::origin_t<VT>;

    extern("rslib_std_map_get_or_set_default") 
        public func getorset<KT, VT>(self: map<KT, VT>, index: KT, default_val: VT)=>impure std::origin_t<VT>;

    extern("rslib_std_map_swap") 
        public func swap<KT, VT>(val: map<KT, VT>, another: map<KT, VT>)=> impure void;

    extern("rslib_std_map_copy") 
        public func copy<KT, VT>(val: map<KT, VT>, another: map<KT, VT>)=> impure void;

    extern("rslib_std_map_empty")
        public func empty<KT, VT>(self: map<KT, VT>)=> impure bool;

    extern("rslib_std_map_remove")
        public func remove<KT, VT>(self: map<KT, VT>, index: KT)=> impure bool;

    extern("rslib_std_map_clear")
        public func clear<KT, VT>(self: map<KT, VT>)=> impure void;

    public using iterator<KT, VT> = gchandle
    {
        extern("rslib_std_map_iter_next")
            public func next<KT, VT>(iter:iterator<KT, VT>)=>impure option<(KT, std::origin_t<VT>)>;

        public func iter<KT, VT>(iter:iterator<KT, VT>) { return iter; }
    }

    extern("rslib_std_map_iter")
        public func iter<KT, VT>(self:map<KT, VT>)=>impure iterator<KT, VT>;

    public func keys<KT, VT>(self: map<KT, VT>)=> impure array<KT>
    {
        let result = []mut: vec<KT>;
        for (let key, _ : self)
            result->add(key);
        return result->unsafe::cast:<array<KT>>;
    }
    public func vals<KT, VT>(self: map<KT, VT>)=> impure array<VT>
    {
        let result = []mut: vec<VT>;
        for (let _, val : self)
            result->add(val);
        return result->unsafe::cast:<array<VT>>;
    }
    public func forall<KT, VT>(self: map<KT, VT>, functor: (KT, VT)=>bool)=> impure map<KT, VT>
    {
        let result = {}mut: map<KT, VT>;
        for (let key, val : self)
            if (functor(key, val))
                result->set(key, val);
        return result;
    }
    public func map<KT, VT, AT, BT>(self: map<KT, VT>, functor: (KT, VT)=>(AT, BT))=> impure map<AT, BT>
    {
        let result = {}mut: map<AT, BT>;
        for (let key, val : self)
        {
            let (nk, nv) = functor(key, val);
            result->set(nk, nv);
        }
        return result->unsafe::cast:<map<AT, BT>>;
    }
    public func unmapping<KT, VT>(self: map<KT, VT>)=> impure array<(KT, VT)>
    {
        let result = []mut: vec<(std::origin_t<KT>, std::origin_t<VT>)>;
        for (let key, val : self)
            result->add((key, val));
        return result->unsafe::cast:<array<(KT, VT)>>;
    }
}

namespace int
{
    extern("rslib_std_int_to_hex")
        public func tohex(val: int)=> pure string;
    extern("rslib_std_int_to_oct")
        public func tooct(val: int)=> pure string;

    extern("rslib_std_hex_to_int")
        public func parsehex(val: string)=> pure int;
    extern("rslib_std_oct_to_int")
        public func parseoct(val: string)=> pure int;
}

namespace handle
{
    extern("rslib_std_handle_to_hex")
        public func tohex(val: handle)=> pure string;
    extern("rslib_std_handle_to_oct")
        public func tooct(val: handle)=> pure string;

    extern("rslib_std_hex_to_handle")
        public func parsehex(val: string)=> pure handle;
    extern("rslib_std_oct_to_handle")
        public func parseoct(val: string)=> pure handle;
}

namespace gchandle
{
    extern("rslib_std_gchandle_close")
        public func close(handle:gchandle)=> impure bool;
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
            public func breakpoint()=>impure void;

        extern("rslib_std_debug_attach_default_debuggee")
            public func attach_debuggee()=>impure void;
        extern("rslib_std_debug_disattach_default_debuggee")
            public func disattach_debuggee()=>impure void;

        public func breakdown()
        {
            attach_debuggee();
            breakpoint();
        }

        extern("rslib_std_debug_callstack_trace")
            public func callstack(layer:int) => impure string;

        public func run<FT>(foo: FT, ...)
        {
            attach_debuggee();
            let result = (foo:(...)=>dynamic)(......);
            disattach_debuggee();
    
            return result;
        }

        extern("rslib_std_debug_invoke")
        public func invoke<FT>(foo:FT, ...)=>impure typeof(foo(......));

        // Used for create a value with specify type, it's a dangergous function.
        extern("rslib_std_debug_empty_func")
        public func __empty_function<T>()=> pure T;
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

    lex->lex_error(wo::lexer::errorlevel::error, wo::str_to_wstr(wo_string(args + 1)).c_str());
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
        l_literal_string,       // "" "helloworld" @"println("hello");"@
        l_literal_char,         // 'x'
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

        l_value_assign,               // :=
        l_value_add_assign,           // +:=
        l_value_sub_assign,           // -:= 
        l_value_mul_assign,           // *:=
        l_value_div_assign,           // /:= 
        l_value_mod_assign,           // %:= 

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
        l_nil,
        l_true,
        l_false,
        l_while,
        l_if,
        l_else,
        l_namespace,
        l_for,
        l_extern,
        l_let,
        l_immut,
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
        l_static,
        l_break,
        l_continue,
        l_lambda,
        l_at,
        l_do,
        l_where,
        l_operator,
        l_union,
        l_match,
        l_struct,
        l_unpure,
        l_pure,
        l_macro
    }

    public using lexer = handle
    {
        extern("rslib_std_macro_lexer_lex")
            public func lex(lex:lexer, src:string)=>impure void;

        extern("rslib_std_macro_lexer_error")
            public func error(lex:lexer, msg:string)=>impure void;

        extern("rslib_std_macro_lexer_peek")
            public func peektoken(lex:lexer)=> impure (token_type, string);

        extern("rslib_std_macro_lexer_next")
            public func nexttoken(lex:lexer)=> impure (token_type, string);

        public func peek(lex: lexer)
        {
            let (type, str) = lex->peektoken;
            if (type == token_type::l_literal_string)
                return str->enstring;
            else if (type == token_type::l_literal_char)
            {
                let enstr = str->enstring;
                return F"'{enstr->subto(1, enstr->len-2)}'";
            }
            return str;
        }
        public func next(lex: lexer)
        {
            let (type, str) = lex->nexttoken;
            if (type == token_type::l_literal_string)
                return str->enstring;
            else if (type == token_type::l_literal_char)
            {
                let enstr = str->enstring;
                return F"'{enstr->subto(1, enstr->len-2)}'";
            }
            return str;
        }

        extern("rslib_std_macro_lexer_nextch")
            public func nextch(lex:lexer) => impure string;

        extern("rslib_std_macro_lexer_peekch")
            public func peekch(lex:lexer) => impure string;

        extern("rslib_std_macro_lexer_current_path")
            public func path(lex:lexer) => impure string;

        extern("rslib_std_macro_lexer_current_rowno")
            public func row(lex:lexer) => impure int;

        extern("rslib_std_macro_lexer_current_colno")
            public func col(lex:lexer) => impure int;

        public func trytoken(self: lexer, token: token_type)=> option<string>
        {
            let (tok, res) = self->peektoken();
            if (token == tok)
                return option::value(self->nexttoken()[1]);
            return option::none;
        }
        public func expecttoken(self: lexer, token: token_type)=> option<string>
        {
            let (tok, res) = self->nexttoken();
            if (tok == token)
                return option::value(res);
            self->error("Unexpected token here.");
            return option::none;
        }
        public func try(self: lexer, str: string)=> option<string>
        {
            let res = self->peek();
            if (res == str)
                return option::value(self->next());
            return option::none;
        }
        public func expect(self: lexer, str: string)=> option<string>
        {
            let res = self->next();
            if (res == str)
                return option::value(res);
            self->error("Unexpected token here.");
            return option::none;
        }
    }
}

)" };

WO_API wo_api rslib_std_call_shell(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_int(vm, system(wo_string(args + 0)));
}

WO_API wo_api rslib_std_get_env(wo_vm vm, wo_value args, size_t argc)
{
    const char* env = getenv(wo_string(args + 0));
    if (env)
        return wo_ret_option_string(vm, env);
    return wo_ret_option_none(vm);
}

const char* wo_stdlib_shell_src_path = u8"woo/shell.wo";
const char* wo_stdlib_shell_src_data = {
u8R"(
namespace std
{
    extern("rslib_std_call_shell")
        public func shell(cmd: string)=> impure int;

    extern("rslib_std_get_env")
        public func env(name: string)=> impure option<string>;
}
)" };
