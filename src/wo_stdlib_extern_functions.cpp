#include "wo_lang_extern_symbol_loader.hpp"
#include "wo_utf8.hpp"
#include "wo_vm.hpp"
#include "wo_io.hpp"

#include <chrono>
#include <random>
#include <thread>

#if WO_BUILD_WITH_MINGW
#   include <mingw.thread.h>
#endif

std::string_view _wo_raw_str_view(wo_value val)
{
    size_t len = 0;
    wo_string_t str = wo_raw_string(val, &len);

    return std::string_view(str, len);
}

WO_API wo_api rslib_std_print(wo_vm vm, wo_value args)
{
    size_t argc = (size_t)wo_vaarg_count(vm);

    for (size_t i = 0; i < argc; i++)
    {
        wo::wo_stdout << wo_cast_string(args + i);

        if (i + 1 < argc)
            wo::wo_stdout << " ";
    }
    return wo_ret_void(vm);
}
WO_API wo_api rslib_std_panic(wo_vm vm, wo_value args)
{
    return wo_ret_panic(vm, wo_string(args + 0));
}
WO_API wo_api rslib_std_halt(wo_vm vm, wo_value args)
{
    return wo_ret_halt(vm, wo_string(args + 0));
}

WO_API wo_api rslib_std_lengthof(wo_vm vm, wo_value args)
{
    return wo_ret_int(vm, wo_lengthof(args));
}

WO_API wo_api rslib_std_str_bytelen(wo_vm vm, wo_value args)
{
    return wo_ret_int(vm, wo_str_bytelen(args));
}

WO_API wo_api rslib_std_make_dup(wo_vm vm, wo_value args)
{
    return wo_ret_dup(vm, args + 0);
}

WO_API wo_api rslib_std_string_toupper(wo_vm vm, wo_value args)
{
    size_t len = 0;
    wo_string_t rawstr = wo_raw_string(args + 0, &len);
    std::wstring str = wo::strn_to_wstr(rawstr, len);

    for (auto& ch : str)
        ch = wo::lexer::lex_toupper(ch);

    auto&& result = wo::wstrn_to_str(str.c_str(), str.size());
    return wo_ret_raw_string(vm, result.c_str(), result.size());
}

WO_API wo_api rslib_std_string_tolower(wo_vm vm, wo_value args)
{
    size_t len = 0;
    wo_string_t rawstr = wo_raw_string(args + 0, &len);
    std::wstring str = wo::strn_to_wstr(rawstr, len);

    for (auto& ch : str)
        ch = wo::lexer::lex_tolower(ch);

    auto&& result = wo::wstrn_to_str(str.c_str(), str.size());
    return wo_ret_raw_string(vm, result.c_str(), result.size());
}

WO_API wo_api rslib_std_string_isspace(wo_vm vm, wo_value args)
{
    size_t len = 0;
    wo_string_t rawstr = wo_raw_string(args + 0, &len);
    std::wstring str = wo::strn_to_wstr(rawstr, len);

    if (!str.empty())
    {
        for (auto& ch : str)
            if (!wo::lexer::lex_isspace(ch))
                return wo_ret_bool(vm, WO_FALSE);
        return wo_ret_bool(vm, WO_TRUE);
    }
    return wo_ret_bool(vm, WO_FALSE);
}

WO_API wo_api rslib_std_string_isalpha(wo_vm vm, wo_value args)
{
    size_t len = 0;
    wo_string_t rawstr = wo_raw_string(args + 0, &len);
    std::wstring str = wo::strn_to_wstr(rawstr, len);

    if (!str.empty())
    {
        for (auto& ch : str)
            if (!wo::lexer::lex_isalpha(ch))
                return wo_ret_bool(vm, WO_FALSE);
        return wo_ret_bool(vm, WO_TRUE);
    }
    return wo_ret_bool(vm, WO_FALSE);
}

WO_API wo_api rslib_std_string_isalnum(wo_vm vm, wo_value args)
{
    size_t len = 0;
    wo_string_t rawstr = wo_raw_string(args + 0, &len);
    std::wstring str = wo::strn_to_wstr(rawstr, len);

    if (!str.empty())
    {
        for (auto& ch : str)
            if (!wo::lexer::lex_isalnum(ch))
                return wo_ret_bool(vm, WO_FALSE);
        return wo_ret_bool(vm, WO_TRUE);
    }
    return wo_ret_bool(vm, WO_FALSE);
}

WO_API wo_api rslib_std_string_isnumber(wo_vm vm, wo_value args)
{
    size_t len = 0;
    wo_string_t rawstr = wo_raw_string(args + 0, &len);
    std::wstring str = wo::strn_to_wstr(rawstr, len);

    if (!str.empty())
    {
        for (auto& ch : str)
            if (!wo::lexer::lex_isdigit(ch))
                return wo_ret_bool(vm, WO_FALSE);
        return wo_ret_bool(vm, WO_TRUE);
    }
    return wo_ret_bool(vm, WO_FALSE);
}

WO_API wo_api rslib_std_string_ishex(wo_vm vm, wo_value args)
{
    size_t len = 0;
    wo_string_t rawstr = wo_raw_string(args + 0, &len);
    std::wstring str = wo::strn_to_wstr(rawstr, len);

    if (!str.empty())
    {
        for (auto& ch : str)
            if (!wo::lexer::lex_isxdigit(ch))
                return wo_ret_bool(vm, WO_FALSE);
        return wo_ret_bool(vm, WO_TRUE);
    }
    return wo_ret_bool(vm, WO_FALSE);
}

WO_API wo_api rslib_std_string_isoct(wo_vm vm, wo_value args)
{
    size_t len = 0;
    wo_string_t rawstr = wo_raw_string(args + 0, &len);
    std::wstring str = wo::strn_to_wstr(rawstr, len);

    if (!str.empty())
    {
        for (auto& ch : str)
            if (!wo::lexer::lex_isodigit(ch))
                return wo_ret_bool(vm, WO_FALSE);
        return wo_ret_bool(vm, WO_TRUE);
    }
    return wo_ret_bool(vm, WO_FALSE);
}

WO_API wo_api rslib_std_char_tostring(wo_vm vm, wo_value args)
{
    wo_char_t wc = wo_char(args + 0);

    std::string result = wo::wstrn_to_str(&wc, 1);

    return wo_ret_raw_string(vm, result.c_str(), result.size());
}

WO_API wo_api rslib_std_char_toupper(wo_vm vm, wo_value args)
{
    return wo_ret_char(vm, wo::lexer::lex_toupper(wo_char(args + 0)));
}

WO_API wo_api rslib_std_char_tolower(wo_vm vm, wo_value args)
{
    return wo_ret_char(vm, wo::lexer::lex_tolower(wo_char(args + 0)));
}

WO_API wo_api rslib_std_char_isspace(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, WO_CBOOL(wo::lexer::lex_isspace(wo_char(args + 0))));
}

WO_API wo_api rslib_std_char_isalpha(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, WO_CBOOL(wo::lexer::lex_isalpha(wo_char(args + 0))));
}

WO_API wo_api rslib_std_char_isalnum(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, WO_CBOOL(wo::lexer::lex_isalnum(wo_char(args + 0))));
}

WO_API wo_api rslib_std_char_isnumber(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, WO_CBOOL(wo::lexer::lex_isdigit(wo_char(args + 0))));
}

WO_API wo_api rslib_std_char_ishex(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, WO_CBOOL(wo::lexer::lex_isxdigit(wo_char(args + 0))));
}

WO_API wo_api rslib_std_char_isoct(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, WO_CBOOL(wo::lexer::lex_isodigit(wo_char(args + 0))));
}

WO_API wo_api rslib_std_char_hexnum(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, WO_CBOOL(wo::lexer::lex_hextonum(wo_char(args + 0))));
}

WO_API wo_api rslib_std_string_enstring(wo_vm vm, wo_value args)
{
    size_t len = 0;
    wo_string_t str = wo_raw_string(args + 0, &len);
    std::string result;
    while (len)
    {
        unsigned char uch = (unsigned char)*str;
        if (uch == 0 || iscntrl(uch))
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
        --len;
    }
    result = "\"" + result + "\"";
    return wo_ret_string(vm, result.c_str());
}

WO_API wo_api rslib_std_string_destring(wo_vm vm, wo_value args)
{
    size_t len = 0;
    wo_string_t str = wo_raw_string(args + 0, &len);

    std::string result;

    if (*str == '"')
        ++str;
    while (len)
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
        --len;
    }
    return wo_ret_raw_string(vm, result.c_str(), result.size());
}

WO_API wo_api rslib_std_string_beginwith(wo_vm vm, wo_value args)
{
    size_t aimlen = 0;
    size_t beginlen = 0;
    wo_string_t aim = wo_raw_string(args + 0, &aimlen);
    wo_string_t begin = wo_raw_string(args + 1, &beginlen);

    if (beginlen > aimlen)
        return wo_ret_bool(vm, WO_FALSE);

    while (aimlen && beginlen)
    {
        if (*aim != *begin)
            return wo_ret_bool(vm, WO_FALSE);
        ++aim;
        ++begin;

        --aimlen;
        --beginlen;
    }

    return wo_ret_bool(vm, WO_TRUE);
}

WO_API wo_api rslib_std_string_endwith(wo_vm vm, wo_value args)
{
    size_t aimlen = 0;
    size_t endlen = 0;
    wo_string_t aim = wo_raw_string(args + 0, &aimlen);
    wo_string_t end = wo_raw_string(args + 1, &endlen);

    if (endlen > aimlen)
        return wo_ret_bool(vm, WO_FALSE);

    aim += (aimlen - endlen);
    while (aimlen && endlen)
    {
        if (*aim != *end)
            return wo_ret_bool(vm, WO_FALSE);
        ++aim;
        ++end;

        --aimlen;
        --endlen;
    }
    return wo_ret_bool(vm, WO_TRUE);
}

WO_API wo_api rslib_std_string_replace(wo_vm vm, wo_value args)
{
    std::string aim(_wo_raw_str_view(args + 0));
    const auto match = _wo_raw_str_view(args + 1);
    const auto replace = _wo_raw_str_view(args + 2);

    size_t replace_begin = 0;
    do
    {
        size_t fnd_place = aim.find(match, replace_begin);
        if (fnd_place< replace_begin || fnd_place>aim.size())
            break;

        aim.replace(fnd_place, match.size(), replace);
        replace_begin += replace.size();

    } while (true);

    return wo_ret_raw_string(vm, aim.c_str(), aim.size());
}

WO_API wo_api rslib_std_string_find(wo_vm vm, wo_value args)
{
    size_t len = 0;
    wo_string_t str = wo_raw_string(args + 0, &len);

    const std::string_view aim(str, len);
    const auto match = _wo_raw_str_view(args + 1);

    size_t fnd_place = aim.find(match);
    if (fnd_place >= 0 && fnd_place < aim.size())
        return wo_ret_int(vm, (wo_integer_t)wo::clen2u8blen(str, fnd_place));

    return wo_ret_int(vm, -1);
}

WO_API wo_api rslib_std_string_find_from(wo_vm vm, wo_value args)
{
    size_t len = 0;
    wo_string_t str = wo_raw_string(args + 0, &len);

    const std::string_view aim(str, len);
    const auto match = _wo_raw_str_view(args + 1);
    size_t from = wo::u8blen2clen(str, len, (size_t)wo_int(args + 2));

    size_t fnd_place = aim.find(match, from);
    if (fnd_place >= from && fnd_place < aim.size())
        return wo_ret_int(vm, (wo_integer_t)wo::clen2u8blen(str, fnd_place));

    return wo_ret_int(vm, -1);
}

WO_API wo_api rslib_std_string_rfind(wo_vm vm, wo_value args)
{
    size_t len = 0;
    wo_string_t str = wo_raw_string(args + 0, &len);

    const std::string_view aim(str, len);
    const auto match = _wo_raw_str_view(args + 1);

    size_t fnd_place = aim.rfind(match);
    if (fnd_place >= 0 && fnd_place < aim.size())
        return wo_ret_int(vm, (wo_integer_t)wo::clen2u8blen(str, fnd_place));

    return wo_ret_int(vm, -1);
}

WO_API wo_api rslib_std_string_rfind_from(wo_vm vm, wo_value args)
{
    size_t len = 0;
    wo_string_t str = wo_raw_string(args + 0, &len);

    const std::string_view aim(str, len);
    const auto match = _wo_raw_str_view(args + 1);
    size_t from = wo::u8blen2clen(str, len, (size_t)wo_int(args + 2));

    size_t fnd_place = aim.rfind(match, from);
    if (fnd_place >= 0 && fnd_place < from)
        return wo_ret_int(vm, (wo_integer_t)wo::clen2u8blen(str, fnd_place));

    return wo_ret_int(vm, -1);
}

WO_API wo_api rslib_std_string_trim(wo_vm vm, wo_value args)
{
    const std::string_view aim = _wo_raw_str_view(args + 0);

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

    auto view = aim.substr(ibeg, iend - ibeg);
    return wo_ret_raw_string(vm, view.data(), view.size());
}

WO_API wo_api rslib_std_string_split(wo_vm vm, wo_value args)
{
    size_t len = 0;
    wo_string_t str = wo_raw_string(args + 0, &len);

    const std::string_view aim(str, len);
    const std::string_view match = _wo_raw_str_view(args + 1);

    wo_value arr = wo_push_arr(vm, 0);
    wo_value elem = wo_push_empty(vm);

    if (match.size() == 0)
    {
        std::wstring origin_str = wo::strn_to_wstr(str, len);

        for (wchar_t wch : origin_str)
        {
            std::string result = wo::wstrn_to_str(&wch, 1);

            wo_set_raw_string(elem, vm, result.c_str(), result.size());
            wo_arr_add(arr, elem);
        }
    }
    else
    {
        size_t split_begin = 0;

        while (true)
        {
            size_t fnd_place = aim.find(match, split_begin);
            if (fnd_place < split_begin || fnd_place > aim.size())
            {
                auto view = aim.substr(split_begin);

                wo_set_raw_string(elem, vm, view.data(), view.size());
                wo_arr_add(arr, elem);
                break;
            }
            auto view = aim.substr(split_begin, fnd_place - split_begin);

            wo_set_raw_string(elem, vm, view.data(), view.size());
            wo_arr_add(arr, elem);

            split_begin = fnd_place + match.size();
        }
    }
    return wo_ret_val(vm, arr);
}

WO_API wo_api rslib_std_string_append_char(wo_vm vm, wo_value args)
{
    std::string str(_wo_raw_str_view(args + 0));
    wchar_t wc = (wchar_t)(wo_handle_t)wo_int(args + 1);

    str += wo::wstrn_to_str(&wc, 1);

    return wo_ret_raw_string(vm, str.c_str(), str.size());
}
WO_API wo_api rslib_std_string_append_cchar(wo_vm vm, wo_value args)
{
    std::string str(_wo_raw_str_view(args + 0));
    str += (char)(wo_handle_t)wo_int(args + 1);

    return wo_ret_raw_string(vm, str.c_str(), str.size());
}

wo_real_t _wo_inside_time_sec()
{
    static auto _first_invoke_time = std::chrono::system_clock::now();

    return wo_real_t(
        (std::chrono::system_clock::now() - _first_invoke_time).count()
        * std::chrono::system_clock::period::num)
        / std::chrono::system_clock::period::den;
}

WO_API wo_api rslib_std_time_sec(wo_vm vm, wo_value args)
{
    return wo_ret_real(vm, _wo_inside_time_sec());
}

WO_API wo_api rslib_std_input_readint(wo_vm vm, wo_value args)
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

WO_API wo_api rslib_std_input_readreal(wo_vm vm, wo_value args)
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

WO_API wo_api rslib_std_input_readstring(wo_vm vm, wo_value args)
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

WO_API wo_api rslib_std_input_readline(wo_vm vm, wo_value args)
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

WO_API wo_api rslib_std_randomint(wo_vm vm, wo_value args)
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

WO_API wo_api rslib_std_break_yield(wo_vm vm, wo_value args)
{
    return wo_ret_yield(vm);
}

WO_API wo_api rslib_std_array_resize(wo_vm vm, wo_value args)
{
    wo_arr_resize(args + 0, wo_int(args + 1), args + 2);
    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_array_shrink(wo_vm vm, wo_value args)
{
    auto newsz = wo_int(args + 1);
    if (newsz <= wo_lengthof(args + 0))
    {
        wo_arr_resize(args + 0, newsz, nullptr);
        return wo_ret_bool(vm, WO_TRUE);
    }
    return wo_ret_bool(vm, WO_FALSE);
}

WO_API wo_api rslib_std_array_insert(wo_vm vm, wo_value args)
{
    wo_arr_insert(args + 0, wo_int(args + 1), args + 2);
    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_array_swap(wo_vm vm, wo_value args)
{
    wo::value* arr1 = std::launder(reinterpret_cast<wo::value*>(args + 0));
    wo::value* arr2 = std::launder(reinterpret_cast<wo::value*>(args + 1));

    std::scoped_lock ssg1(arr1->array->gc_read_write_mx, arr2->array->gc_read_write_mx);

    if (wo::gc::gc_is_marking())
    {
        for (auto& elem : *arr1->array)
            wo::gcbase::write_barrier(&elem);
        for (auto& elem : *arr2->array)
            wo::gcbase::write_barrier(&elem);
    }

    arr1->array->swap(*arr2->array);

    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_array_copy(wo_vm vm, wo_value args)
{
    wo::value* arr1 = std::launder(reinterpret_cast<wo::value*>(args + 0));
    wo::value* arr2 = std::launder(reinterpret_cast<wo::value*>(args + 1));

    std::scoped_lock ssg1(arr1->array->gc_read_write_mx, arr2->array->gc_read_write_mx);
    *arr1->array->elem() = *arr2->array->elem();

    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_array_empty(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, wo_arr_is_empty(args + 0));
}

WO_API wo_api rslib_std_array_get(wo_vm vm, wo_value args)
{
    wo_value arr = args + 0;
    wo_integer_t idx = wo_int(args + 1);

    wo_value elem = wo_push_empty(vm);

    if (wo_arr_try_get(elem, arr, idx))
        return wo_ret_option_val(vm, elem);

    return wo_ret_option_none(vm);
}

WO_API wo_api rslib_std_array_get_or_default(wo_vm vm, wo_value args)
{
    wo_value arr = args + 0;
    wo_integer_t idx = wo_int(args + 1);

    wo_value elem = wo_push_empty(vm);

    if (wo_arr_try_get(elem, arr, idx))
        return wo_ret_val(vm, elem);

    return wo_ret_val(vm, args + 2);
}

WO_API wo_api rslib_std_array_add(wo_vm vm, wo_value args)
{
    wo_arr_add(args + 0, args + 1);
    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_array_connect(wo_vm vm, wo_value args)
{
    wo_value result = wo_push_arr(vm, 0);

    wo::value* arr_result = std::launder(reinterpret_cast<wo::value*>(result));
    wo::value* arr1 = std::launder(reinterpret_cast<wo::value*>(args + 0));
    wo::value* arr2 = std::launder(reinterpret_cast<wo::value*>(args + 1));
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

WO_API wo_api rslib_std_array_sub(wo_vm vm, wo_value args)
{
    wo_value result = wo_push_arr(vm, 0);

    wo::value* arr_result = std::launder(reinterpret_cast<wo::value*>(result));
    wo::value* arr1 = std::launder(reinterpret_cast<wo::value*>(args + 0));

    wo::gcbase::gc_read_guard rg2(arr1->array);

    auto begin = (size_t)wo_int(args + 1);
    if (begin > arr1->array->size())
        return wo_ret_panic(vm, "Index out of range when trying get sub array/vec.");

    arr_result->array->insert(arr_result->array->end(),
        arr1->array->begin() + begin, arr1->array->end());

    return wo_ret_val(vm, result);
}

WO_API wo_api rslib_std_array_subto(wo_vm vm, wo_value args)
{
    wo_value result = wo_push_arr(vm, 0);

    wo::value* arr_result = std::launder(reinterpret_cast<wo::value*>(result));
    wo::value* arr1 = std::launder(reinterpret_cast<wo::value*>(args + 0));

    wo::gcbase::gc_read_guard rg2(arr1->array);

    auto begin = (size_t)wo_int(args + 1);
    if (begin > arr1->array->size())
        return wo_ret_panic(vm, "Index out of range when trying get sub array/vec.");

    auto count = (size_t)wo_int(args + 2);

    if (begin + count > arr1->array->size())
        return wo_ret_panic(vm, "Index out of range when trying get sub array/vec.");

    auto&& begin_iter = arr1->array->begin() + begin;
    auto&& end_iter = begin_iter + count;

    arr_result->array->insert(arr_result->array->end(),
        begin_iter, end_iter);

    return wo_ret_val(vm, result);
}

WO_API wo_api rslib_std_array_pop(wo_vm vm, wo_value args)
{
    wo_value elem = wo_push_empty(vm);
    if (wo_arr_pop_back(elem, args + 0))
        return wo_ret_option_val(vm, elem);
    return wo_ret_option_none(vm);
}

WO_API wo_api rslib_std_array_dequeue(wo_vm vm, wo_value args)
{
    wo_value elem = wo_push_empty(vm);
    if (wo_arr_pop_front(elem, args + 0))
        return wo_ret_option_val(vm, elem);
    return wo_ret_option_none(vm);
}

WO_API wo_api rslib_std_array_pop_val(wo_vm vm, wo_value args)
{
    wo_value elem = wo_push_empty(vm);
    wo_arr_pop_back_val(elem, args + 0);
    return wo_ret_val(vm, elem);
}

WO_API wo_api rslib_std_array_dequeue_val(wo_vm vm, wo_value args)
{
    wo_value elem = wo_push_empty(vm);
    wo_arr_pop_front_val(elem, args + 0);
    return wo_ret_val(vm, elem);
}

WO_API wo_api rslib_std_array_remove(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, wo_arr_remove(args + 0, wo_int(args + 1)));
}

WO_API wo_api rslib_std_array_find(wo_vm vm, wo_value args)
{
    return wo_ret_int(vm, wo_arr_find(args + 0, args + 1));
}

WO_API wo_api rslib_std_array_front(wo_vm vm, wo_value args)
{
    wo_value elem = wo_push_empty(vm);
    if (wo_arr_front(elem, args + 0))
        return wo_ret_option_val(vm, elem);
    return wo_ret_option_none(vm);
}
WO_API wo_api rslib_std_array_back(wo_vm vm, wo_value args)
{
    wo_value elem = wo_push_empty(vm);
    if (wo_arr_back(elem, args + 0))
        return wo_ret_option_val(vm, elem);
    return wo_ret_option_none(vm);
}

WO_API wo_api rslib_std_array_front_val(wo_vm vm, wo_value args)
{
    wo_value elem = wo_push_empty(vm);
    wo_arr_front_val(elem, args + 0);
    return wo_ret_val(vm, elem);
}
WO_API wo_api rslib_std_array_back_val(wo_vm vm, wo_value args)
{
    wo_value elem = wo_push_empty(vm);
    wo_arr_back_val(elem, args + 0);
    return wo_ret_val(vm, elem);
}

WO_API wo_api rslib_std_array_clear(wo_vm vm, wo_value args)
{
    wo_arr_clear(args);

    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_map_keys(wo_vm vm, wo_value args)
{
    wo_value result = wo_push_empty(vm);
    wo_map_keys(result, vm, args + 0);
    return wo_ret_val(vm, result);
}
WO_API wo_api rslib_std_map_vals(wo_vm vm, wo_value args)
{
    wo_value result = wo_push_empty(vm);
    wo_map_vals(result, vm, args + 0);
    return wo_ret_val(vm, result);
}

struct array_iter
{
    using array_iter_t = decltype(std::declval<wo::array_t>().begin());

    array_iter_t iter;
    array_iter_t end_place;
    wo_int_t     index_count;
};

WO_API wo_api rslib_std_array_iter(wo_vm vm, wo_value args)
{
    wo::value* arr = std::launder(reinterpret_cast<wo::value*>(args));
    return wo_ret_gchandle(vm,
        new array_iter{ arr->array->begin(), arr->array->end(), 0 },
        args + 0,
        [](void* array_iter_t_ptr)
        {
            delete (array_iter*)array_iter_t_ptr;
        }
    );
}

WO_API wo_api rslib_std_array_iter_next(wo_vm vm, wo_value args)
{
    array_iter& iter = *(array_iter*)wo_pointer(args);

    if (iter.iter == iter.end_place)
        return wo_ret_option_none(vm);

    return wo_ret_option_val(
        vm, std::launder(reinterpret_cast<wo_value>(&*(iter.iter++))));
}

WO_API wo_api rslib_std_map_find(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, wo_map_find(args + 0, args + 1));
}

WO_API wo_api rslib_std_map_set(wo_vm vm, wo_value args)
{
    wo_map_set(args + 0, args + 1, args + 2);
    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_map_only_get(wo_vm vm, wo_value args)
{
    wo_value elem = wo_push_empty(vm);

    if (wo_map_try_get(elem, args + 0, args + 1))
        return wo_ret_option_val(vm, elem);

    return wo_ret_option_none(vm);
}

WO_API wo_api rslib_std_map_get_or_set_default(wo_vm vm, wo_value args)
{
    wo_value elem = wo_push_empty(vm);
    wo_map_get_or_set_default(elem, args + 0, args + 1, args + 2);
    return wo_ret_val(vm, elem);
}

WO_API wo_api rslib_std_map_get_or_default(wo_vm vm, wo_value args)
{
    wo_value elem = wo_push_empty(vm);
    wo_map_get_or_default(elem, args + 0, args + 1, args + 2);
    return wo_ret_val(vm, elem);
}

WO_API wo_api rslib_std_map_swap(wo_vm vm, wo_value args)
{
    wo::value* map1 = std::launder(reinterpret_cast<wo::value*>(args + 0));
    wo::value* map2 = std::launder(reinterpret_cast<wo::value*>(args + 1));

    std::scoped_lock ssg1(map1->dict->gc_read_write_mx, map2->dict->gc_read_write_mx);

    if (wo::gc::gc_is_marking())
    {
        for (auto& [key, elem] : *map1->dict)
        {
            wo::gcbase::write_barrier(&key);
            wo::gcbase::write_barrier(&elem);
        }
        for (auto& [key, elem] : *map2->dict)
        {
            wo::gcbase::write_barrier(&key);
            wo::gcbase::write_barrier(&elem);
        }
    }

    map1->dict->swap(*map2->dict);

    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_map_copy(wo_vm vm, wo_value args)
{
    wo::value* map1 = std::launder(reinterpret_cast<wo::value*>(args + 0));
    wo::value* map2 = std::launder(reinterpret_cast<wo::value*>(args + 1));

    std::scoped_lock ssg1(map1->dict->gc_read_write_mx, map2->dict->gc_read_write_mx);
    *map1->dict->elem() = *map2->dict->elem();

    return wo_ret_void(vm);
}


WO_API wo_api rslib_std_map_remove(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, wo_map_remove(args + 0, args + 1));
}

WO_API wo_api rslib_std_map_empty(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, wo_map_is_empty(args + 0));
}

WO_API wo_api rslib_std_map_clear(wo_vm vm, wo_value args)
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

WO_API wo_api rslib_std_map_iter(wo_vm vm, wo_value args)
{
    wo::value* mapp = std::launder(reinterpret_cast<wo::value*>(args));

    return wo_ret_gchandle(vm,
        new map_iter{ mapp->dict->begin(), mapp->dict->end() },
        args + 0,
        [](void* array_iter_t_ptr)
        {
            delete (map_iter*)array_iter_t_ptr;
        }
    );
}

WO_API wo_api rslib_std_map_iter_next(wo_vm vm, wo_value args)
{
    map_iter& iter = *(map_iter*)wo_pointer(args);

    if (iter.iter == iter.end_place)
        return wo_ret_option_none(vm);

    wo_value result_tuple = wo_push_struct(vm, 2);
    wo_value elem = wo_push_empty(vm);

    wo_set_val(elem, std::launder(reinterpret_cast<wo_value>(const_cast<wo::value*>(&iter.iter->first)))); // key
    wo_struct_set(result_tuple, 0, elem);
    wo_set_val(elem, std::launder(reinterpret_cast<wo_value>(&iter.iter->second))); // val
    wo_struct_set(result_tuple, 1, elem);
    iter.iter++;

    return wo_ret_option_val(vm, result_tuple);
}

WO_API wo_api rslib_std_take_token(wo_vm vm, wo_value args)
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

WO_API wo_api rslib_std_take_string(wo_vm vm, wo_value args)
{
    wo_string_t input = wo_string(args + 0);
    size_t token_length;
    char string_buf[1024];

    if (sscanf(input, "%s%zn", string_buf, &token_length) == 1)
    {
        wo_value result = wo_push_struct(vm, 2);
        wo_value elem = wo_push_empty(vm);

        wo_set_string(elem, vm, input + token_length);
        wo_struct_set(result, 0, elem);
        wo_set_string(elem, vm, string_buf);
        wo_struct_set(result, 1, elem);
        return wo_ret_option_val(vm, result);
    }

    return wo_ret_option_none(vm);
}

WO_API wo_api rslib_std_take_int(wo_vm vm, wo_value args)
{
    wo_string_t input = wo_string(args + 0);
    size_t token_length;
    long long integer;

    if (sscanf(input, "%lld%zn", &integer, &token_length) == 1)
    {
        wo_value result = wo_push_struct(vm, 2);
        wo_value elem = wo_push_empty(vm);

        wo_set_string(elem, vm, input + token_length);
        wo_struct_set(result, 0, elem);
        wo_set_int(elem, (wo_integer_t)integer);
        wo_struct_set(result, 1, elem);
        return wo_ret_option_val(vm, result);
    }

    return wo_ret_option_none(vm);
}

WO_API wo_api rslib_std_take_real(wo_vm vm, wo_value args)
{
    wo_string_t input = wo_string(args + 0);
    size_t token_length;
    wo_real_t real;

    if (sscanf(input, "%lf%zn", &real, &token_length) == 1)
    {
        wo_value result = wo_push_struct(vm, 2);
        wo_value elem = wo_push_empty(vm);

        wo_set_string(elem, vm, input + token_length);
        wo_struct_set(result, 0, elem);
        wo_set_real(elem, real);
        wo_struct_set(result, 1, elem);
        return wo_ret_option_val(vm, result);
    }

    return wo_ret_option_none(vm);
}

WO_API wo_api rslib_std_parse_map_from_string(wo_vm vm, wo_value args)
{
    wo_value result_dict = wo_push_empty(vm);
    if (wo_deserialize(vm, result_dict, wo_string(args + 0), WO_MAPPING_TYPE))
        return wo_ret_option_val(vm, result_dict);
    return wo_ret_option_none(vm);
}

WO_API wo_api rslib_std_parse_array_from_string(wo_vm vm, wo_value args)
{
    wo_value result_arr = wo_push_empty(vm);
    if (wo_deserialize(vm, result_arr, wo_string(args + 0), WO_ARRAY_TYPE))
        return wo_ret_option_val(vm, result_arr);
    return wo_ret_option_none(vm);
}

WO_API wo_api rslib_std_create_wchars_from_str(wo_vm vm, wo_value args)
{
    size_t len = 0;
    wo_string_t str = wo_raw_string(args + 0, &len);

    std::wstring buf = wo::strn_to_wstr(str, len);
    wo_value result_array = wo_push_arr(vm, buf.size());

    wo_value elem = wo_push_empty(vm);

    for (size_t i = 0; i < buf.size(); ++i)
    {
        wo_set_int(elem, (wo_int_t)(wo_handle_t)buf[i]);
        wo_arr_set(result_array, (wo_int_t)i, elem);
    }

    return wo_ret_val(vm, result_array);
}

WO_API wo_api rslib_std_create_chars_from_str(wo_vm vm, wo_value args)
{
    size_t len = 0;
    wo_string_t str = wo_raw_string(args + 0, &len);

    wo_value result_array = wo_push_arr(vm, len);

    wo_value elem = wo_push_empty(vm);

    for (size_t i = 0; i < len; ++i)
    {
        wo_set_int(elem, (wo_int_t)(wo_handle_t)(unsigned char)str[i]);
        wo_arr_set(result_array, (wo_int_t)i, elem);
    }

    return wo_ret_val(vm, result_array);
}

WO_API wo_api rslib_std_serialize(wo_vm vm, wo_value args)
{
    wo_string_t result = nullptr;
    if (wo_serialize(args + 0, &result))
    {
        wo_assert(result != nullptr);
        return wo_ret_option_string(vm, result);
    }
    return wo_ret_option_none(vm);
}

WO_API wo_api rslib_std_array_create(wo_vm vm, wo_value args)
{
    wo_integer_t arrsz = wo_int(args + 0);

    wo_value newarr = wo_push_arr(vm, arrsz);
    for (wo_integer_t i = 0; i < arrsz; ++i)
        wo_arr_set(newarr, i, args + 1);

    return wo_ret_val(vm, newarr);
}

WO_API wo_api rslib_std_create_str_by_wchar(wo_vm vm, wo_value args)
{
    std::wstring buf;

    wo::value* arr = std::launder(reinterpret_cast<wo::value*>(args + 0));
    wo::gcbase::gc_read_guard rg1(arr->array);

    size_t size = arr->array->size();

    for (size_t i = 0; i < size; ++i)
        buf += (wchar_t)(wo_handle_t)wo_int(std::launder(reinterpret_cast<wo_value>(&arr->array->at(i))));

    std::string result = wo::wstrn_to_str(buf);
    return wo_ret_raw_string(vm, result.c_str(), result.size());
}

WO_API wo_api rslib_std_create_str_by_ascii(wo_vm vm, wo_value args)
{
    std::string buf;

    wo::value* arr = std::launder(reinterpret_cast<wo::value*>(args + 0));
    wo::gcbase::gc_read_guard rg1(arr->array);

    size_t size = arr->array->size();

    for (size_t i = 0; i < size; ++i)
        buf += (char)(unsigned char)(wo_handle_t)wo_int(std::launder(reinterpret_cast<wo_value>(&arr->array->at(i))));

    return wo_ret_raw_string(vm, buf.c_str(), buf.size());
}


WO_API wo_api rslib_std_return_itself(wo_vm vm, wo_value args)
{
    return wo_ret_val(vm, args + 0);
}

WO_API wo_api rslib_std_get_ascii_val_from_str(wo_vm vm, wo_value args)
{
    size_t len = 0;
    wo_string_t str = wo_raw_string(args + 0, &len);

    return wo_ret_int(vm, (wo_int_t)wo_strn_get_char(str, len, wo_int(args + 1)));
}

WO_API wo_api rslib_std_string_sub(wo_vm vm, wo_value args)
{
    size_t len = 0;
    wo_string_t str = wo_raw_string(args + 0, &len);

    size_t sub_str_len = 0;
    auto* substring = wo::u8substrn(str, len, (size_t)wo_int(args + 1), wo::u8str_npos, &sub_str_len);
    return wo_ret_raw_string(vm, substring, sub_str_len);
}

WO_API wo_api rslib_std_string_subto(wo_vm vm, wo_value args)
{
    size_t len = 0;
    wo_string_t str = wo_raw_string(args + 0, &len);

    size_t sub_str_len = 0;
    auto* substring = wo::u8substrn(str, len, (size_t)wo_int(args + 1), (size_t)wo_int(args + 2), &sub_str_len);
    return wo_ret_raw_string(vm, substring, sub_str_len);
}

WO_API wo_api rslib_std_thread_sleep(wo_vm vm, wo_value args)
{
    using namespace std;
    std::this_thread::sleep_for(wo_real(args) * 1s);
    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_gchandle_close(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, wo_gchandle_close(args));
}

WO_API wo_api rslib_std_int_to_hex(wo_vm vm, wo_value args)
{
    char result[18];
    wo_integer_t val = wo_int(args + 0);
    if (val >= 0)
        sprintf(result, "%llX", (unsigned long long)val);
    else
        sprintf(result, "-%llX", (unsigned long long) - val);
    return wo_ret_string(vm, result);
}

WO_API wo_api rslib_std_int_to_oct(wo_vm vm, wo_value args)
{
    char result[24];
    wo_integer_t val = wo_int(args + 0);
    if (val >= 0)
        sprintf(result, "%llo", (unsigned long long)val);
    else
        sprintf(result, "-%llo", (unsigned long long) - val);
    return wo_ret_string(vm, result);
}

WO_API wo_api rslib_std_hex_to_int(wo_vm vm, wo_value args)
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

WO_API wo_api rslib_std_oct_to_int(wo_vm vm, wo_value args)
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

WO_API wo_api rslib_std_hex_to_handle(wo_vm vm, wo_value args)
{
    wo_string_t str = wo_string(args + 0);
    unsigned long long result;
    sscanf(str, "%llX", &result);
    return wo_ret_handle(vm, result);
}

WO_API wo_api rslib_std_oct_to_handle(wo_vm vm, wo_value args)
{
    wo_string_t str = wo_string(args + 0);
    unsigned long long result;
    sscanf(str, "%llo", &result);
    return wo_ret_handle(vm, result);
}

WO_API wo_api rslib_std_handle_to_hex(wo_vm vm, wo_value args)
{
    char result[18];
    wo_handle_t val = wo_handle(args + 0);

    sprintf(result, "%llX", (unsigned long long)val);

    return wo_ret_string(vm, result);
}

WO_API wo_api rslib_std_handle_to_oct(wo_vm vm, wo_value args)
{
    char result[24];
    wo_handle_t val = wo_handle(args + 0);

    sprintf(result, "%llo", (unsigned long long)val);

    return wo_ret_string(vm, result);
}


WO_API wo_api rslib_std_get_args(wo_vm vm, wo_value args)
{
    wo_integer_t argcarr = (wo_integer_t)wo::wo_args.size();
    wo_value argsarr = wo_push_arr(vm, argcarr);

    wo_value elem = wo_push_empty(vm);
    for (wo_integer_t i = 0; i < argcarr; ++i)
    {
        wo_set_string(elem, vm, wo::wo_args[(size_t)i].c_str());
        wo_arr_set(argsarr, i, elem);
    }

    return wo_ret_val(vm, argsarr);
}

WO_API wo_api rslib_std_get_exe_path(wo_vm vm, wo_value args)
{
    return wo_ret_string(vm, wo::exe_path());
}

WO_API wo_api rslib_std_get_extern_symb(wo_vm vm, wo_value args)
{
    wo_integer_t script_func;
    wo_handle_t jit_func;
    if (WO_TRUE == wo_extern_symb(vm, wo_string(args + 0), &script_func, &jit_func))
    {
        if (jit_func != 0)
            return wo_ret_option_handle(vm, jit_func);
        return wo_ret_option_int(vm, script_func);
    }
    return wo_ret_option_none(vm);
}

WO_API wo_api rslib_std_equal_byte(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, wo_equal_byte(args + 0, args + 1));
}

WO_API wo_api rslib_std_bad_function(wo_vm vm, wo_value args)
{
    return wo_ret_panic(vm, "This function cannot be called at runtime.");
}

WO_API wo_api rslib_std_bit_or(wo_vm vm, wo_value args)
{
    auto result = wo_int(args + 0) | wo_int(args + 1);
    static_assert(std::is_same<decltype(result), wo_integer_t>::value);
    return wo_ret_int(vm, result);
}

WO_API wo_api rslib_std_bit_and(wo_vm vm, wo_value args)
{
    auto result = wo_int(args + 0) & wo_int(args + 1);
    static_assert(std::is_same<decltype(result), wo_integer_t>::value);
    return wo_ret_int(vm, result);
}

WO_API wo_api rslib_std_bit_xor(wo_vm vm, wo_value args)
{
    auto result = wo_int(args + 0) ^ wo_int(args + 1);
    static_assert(std::is_same<decltype(result), wo_integer_t>::value);
    return wo_ret_int(vm, result);
}

WO_API wo_api rslib_std_bit_not(wo_vm vm, wo_value args)
{
    auto result = ~wo_int(args + 0);
    static_assert(std::is_same<decltype(result), wo_integer_t>::value);
    return wo_ret_int(vm, result);
}

WO_API wo_api rslib_std_bit_shl(wo_vm vm, wo_value args)
{
    auto result = wo_int(args + 0) << wo_int(args + 1);
    static_assert(std::is_same<decltype(result), wo_integer_t>::value);
    return wo_ret_int(vm, result);
}
WO_API wo_api rslib_std_bit_ashr(wo_vm vm, wo_value args)
{
    auto result = wo_int(args + 0) >> wo_int(args + 1);
    static_assert(std::is_same<decltype(result), wo_integer_t>::value);
    return wo_ret_int(vm, result);
}
WO_API wo_api rslib_std_bit_shr(wo_vm vm, wo_value args)
{
    auto result = ((wo_handle_t)wo_int(args + 0) >> (wo_handle_t)wo_int(args + 1));
    static_assert(std::is_same<decltype(result), wo_handle_t>::value);
    return wo_ret_int(vm, (wo_integer_t)result);
}

#if defined(__APPLE__) && defined(__MACH__)
#include <TargetConditionals.h>
#endif

const char* wo_stdlib_src_path = u8"woo/std.wo";
const char* wo_stdlib_src_data = {
u8R"(
namespace std
{
    public enum os
    {
        WIN32,
        ANDROID,
        LINUX,
        IOS,
        MACOS,
        UNKNOWN,
    }
    public enum arch
    {
        X86,
        AMD64,
        ARM32,
        ARM64,
        UNKNOWN,
    }
)"
"   public let platform_os = "
#if defined(_WIN32)
    "os::WIN32;\n"
#elif defined(__ANDROID__)
    "os::ANDROID;\n"
#elif defined(__linux__)
    "os::LINUX;\n"
#elif defined(__APPLE__) && defined(__MACH__)
#if TARGET_IPHONE_SIMULATOR || TARGET_OS_IPHONE
    "os::IOS;\n"
#elif TARGET_OS_MAC
    "os::MACOS;\n"
#else
    "os::UNKNOWN;\n"
#endif
#else
    "os::UNKNOWN;\n"
#endif
"   public let platform_arch = "
#if defined(_X86_)||defined(__i386)||(defined(_WIN32)&&!defined(_WIN64))
    "arch::X86;\n"
#elif defined(__x86_64)||defined(_M_X64)
    "arch::AMD64;\n"
#elif defined(__arm)||defined(_M_ARM)
    "arch::ARM32;\n"
#elif defined(__aarch64__)||defined(_M_ARM64)
    "arch::ARM64;\n"
#else
    "arch::UNKNOWN;\n"
#endif
u8R"(
}
namespace unsafe
{
    extern("rslib_std_return_itself") 
        public func cast<T, FromT>(val: FromT)=> T;
    
    extern("rslib_std_get_extern_symb")
        public func extsymbol<T>(fullname:string)=> option<T>;
}
namespace std
{
    extern("rslib_std_halt") public func halt(msg: string) => nothing;
    extern("rslib_std_panic") public func panic(msg: string)=> nothing;

    extern("rslib_std_bad_function") public func declval<T>()=> T;

    public alias origin_t<T> = immut T;
    public alias function_result_t<FT> = typeof(std::declval:<FT>()([]...));

    public let is_same_type<A, B> = typeid:<A> == typeid:<B>;
    public let is_same_base_type<A, B> = std::is_same_type:<std::origin_t<A>, std::origin_t<B>>;
    public let is_accpetable_base_type<A, B> = std::declval:<std::origin_t<A>>() is std::origin_t<B>;
    public let is_mutable_type<A> = std::is_same_type:<A, mut A>;

    public alias array_elem_t<AT> = typeof(std::declval:<AT>()->\<T>_: array<T> = std::declval:<T>(););
    public alias vec_elem_t<AT> = typeof(std::declval:<AT>()->\<T>_: vec<T> = std::declval:<T>(););

    public alias dict_elem_t<DT> = typeof(std::declval:<DT>()->\<KT, VT>_: dict<KT, VT> = std::declval:<(KT, VT)>(););
    public alias map_elem_t<DT> = typeof(std::declval:<DT>()->\<KT, VT>_: map<KT, VT> = std::declval:<(KT, VT)>(););

    public let is_array<AT> = !(std::declval:<std::array_elem_t<AT>>() is pending);
    public let is_vec<AT> = !(std::declval:<std::vec_elem_t<AT>>() is pending);
    public let is_dict<DT> = !(std::declval:<std::dict_elem_t<DT>>() is pending);
    public let is_map<DT> = !(std::declval:<std::map_elem_t<DT>>() is pending);

    public let is_tuple<T> = 
        !is_array:<T> &&
        !is_vec:<T> && 
        !(std::declval:<T>()...->\...=do nil; is pending);
    
    extern("rslib_std_bit_or") public func bitor(a: int, b: int)=> int;
    extern("rslib_std_bit_and") public func bitand(a: int, b: int)=> int;
    extern("rslib_std_bit_xor") public func bitxor(a: int, b: int)=> int;
    extern("rslib_std_bit_not") public func bitnot(a: int)=> int;

    extern("rslib_std_bit_shl") public func bitshl(a: int, b: int)=> int;
    extern("rslib_std_bit_shr") public func bitshr(a: int, b: int)=> int;
    extern("rslib_std_bit_ashr") public func bitashr(a: int, b: int)=> int;

    public func use<T, R>(val: T, f: (T)=> R)
        where val->close is pending == false;
    {
        let v = f(val);
        do val->close;

        return v;
    }

    public alias iterator_result_t<T> = option::item<typeof(std::declval:<T>()->next)>;
    public let is_iterator<T> = std::declval:<iterator_result_t<T>>() is pending == false;
    public let is_iterable<T> = 
        std::declval:<T>()->iter is pending == false
        ? std::is_iterator:<typeof(std::declval:<T>()->iter)>
        | false;
        
    public func iterator<T>(v: T)
        where std::is_iterator:<T> || std::is_iterable:<T>;
    {
        if (is_iterable:<T>)
            return v->iter;
        else
            return v;
    }
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
    alias item<T> = typeof(std::declval:<T>()->\<E>_: option<E> = std::declval:<E>(););

    public func bind<T, R>(self: option<T>, functor: (T)=> option<R>)
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
    public func map<T, R>(self: option<T>, functor: (T)=> R)
    {
        match(self)
        {
        value(x)?
            return option::value(functor(x));
        none?
            return option::none;
        }
    }
    public func or<T>(self: option<T>, orfunctor: ()=> T)
    {
        match(self)
        {
        value(x)?
            return x;
        none?
            return orfunctor();
        }
    }
    public func orbind<T>(self: option<T>, orfunctor: ()=> option<T>)
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
        value(_)?
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
    alias okitem<T> = typeof(std::declval:<T>()->\<O, E>_: result<O, E> = std::declval:<O>(););
    alias erritem<T> = typeof(std::declval:<T>()->\<O, E>_: result<O, E> = std::declval:<E>(););

    public func flip<T, F>(self: result<T, F>)
    {
        match(self)
        {
        ok(v)? return err(v);
        err(e)? return ok(e);
        }
    }
    public func unwarp<T, F>(self: result<T, F>)
    {
        match(self)
        {
        ok(v)? return v;
        err(e)? 
            if (e is void)
                std::panic("An error was found in 'unwarp'.");
            else
                std::panic(F"An error was found in 'unwarp': {e}");
        }
    }
    public func unwarpor<T, F>(self: result<T, F>, default_val: T)
    {
        match(self)
        {
        ok(v)? return v;
        err(_)? return default_val;
        }
    }
    public func isok<T, F>(self: result<T, F>)
    {
        match(self)
        {
        ok(_)? return true;
        err(_)? return false;
        }
    }
    public func iserr<T, F>(self: result<T, F>)
    {
        match(self)
        {
        ok(_)? return false;
        err(_)? return true;
        }
    }
    public func okay<T, F>(self: result<T, F>)
    {
        match(self)
        {
        ok(v)? return option::value(v);
        err(_)? return option::none;
        }
    }
    public func error<T, F>(self: result<T, F>)
    {
        match(self)
        {
        ok(_)? return option::none;
        err(e)? return option::value(e);
        }
    }
    public func succ<T, F>(self: result<T, F>)
    {
        match(self)
        {
        ok(v)? return ok(v);
        err(e)?
            if (e is void)
                std::panic(F"An error was found in 'succ'.");
            else
                std::panic(F"An error was found in 'succ': {e}");
        }
    }
    public func fail<T, F>(self: result<T, F>)=> result<nothing, F>
    {
        match(self)
        {
        ok(v)?
            if (v is void)
                std::panic(F"Expected result::err in 'fail'.");
            else
                std::panic(F"Expected result::err in 'fail', but get result::ok: {v}.");
        err(e)? return err(e);
        }
    }
    public func map<T, F, U>(self: result<T, F>, functor: (T)=> U)
    {
        match(self)
        {
        ok(v)? return ok(functor(v));
        err(e)? return err(e);
        }
    }
    public func bind<T, F, U>(self: result<T, F>, functor: (T)=> result<U, F>)
    {
        match(self)
        {
        ok(v)? return functor(v);
        err(e)? return err(e);
        }
    }
    public func or<T, F>(self: result<T, F>, functor: (F)=> T)
    {
        match(self)
        {
        ok(v)? return v;
        err(e)? return functor(e);
        }
    }
    public func orbind<T, F>(self: result<T, F>, functor: (F)=> result<T, F>)
    {
        match(self)
        {
        ok(_)? return self;
        err(e)? return functor(e);
        }
    }
}
namespace std
{
    extern("rslib_std_print", slow) public func print(...)=> void;
    extern("rslib_std_time_sec") public func time()=> real;

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
                extern("rslib_std_input_readint", slow) 
                public func _input_int()=> int;

                let result = _input_int();
                if (validator(result))
                    return result;
            }
            else if (std::declval:<T>() is real)
            {
                extern("rslib_std_input_readreal", slow) 
                public func _input_real()=> real;

                let result = _input_real();
                if (validator(result))
                    return result;
            }
            else
            {
                extern("rslib_std_input_readstring", slow) 
                public func _input_string()=> string;

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
            extern("rslib_std_input_readline", slow) 
            public func _input_line()=> string;

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
        public func randint(from: int, to: int)=> int;

    extern("rslib_std_randomreal") 
        public func randreal(from:real, to:real)=> real;

    extern("rslib_std_break_yield") 
        public func yield()=> void;

    extern("rslib_std_thread_sleep", slow)
        public func sleep(tm:real)=> void;
   
    extern("rslib_std_get_args")
        public func args()=> array<string>;

    extern("rslib_std_get_exe_path")
        public func exepath()=> string;

    extern("rslib_std_equal_byte")
        public func issame<LT, RT>(a:LT, b:RT)=> bool;

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

    extern("rslib_std_make_dup", repeat)
    public func dup<T>(dupval: T)=> T;
}

public using cchar = char;

namespace char
{
    extern("rslib_std_char_tostring")
        public func tostring(val:char)=> string;

    extern("rslib_std_char_toupper")
        public func upper(val:char)=> char;

    extern("rslib_std_char_tolower")
        public func lower(val:char)=> char;

    extern("rslib_std_char_isspace")
        public func isspace(val:char)=> bool;

    extern("rslib_std_char_isalpha")
        public func isalpha(val:char)=> bool;

    extern("rslib_std_char_isalnum")
        public func isalnum(val:char)=> bool;

    extern("rslib_std_char_isnumber")
        public func isnumber(val:char)=> bool;

    extern("rslib_std_char_ishex")
        public func ishex(val:char)=> bool;

    extern("rslib_std_char_isoct")
        public func isoct(val:char)=> bool;

    extern("rslib_std_char_hexnum")
        public func hexnum(val:char)=> int;
}

namespace string
{
    extern("rslib_std_take_token") 
    public func take_token(datstr: string, expect_str: string)=> option<string>;

    extern("rslib_std_take_string") 
    public func take_string(datstr: string)=> option<(string, string)>;

    extern("rslib_std_take_int") 
    public func take_int(datstr: string)=> option<(string, int)>;

    extern("rslib_std_take_real") 
    public func take_real(datstr: string)=> option<(string, real)>;
    
    extern("rslib_std_create_wchars_from_str")
    public func chars(buf: string)=> array<char>;

    extern("rslib_std_create_chars_from_str")
    public func cchars(buf: string)=> array<cchar>;

    extern("rslib_std_get_ascii_val_from_str") 
    public func getch(val:string, index: int)=> char;

    extern("rslib_std_lengthof", repeat) 
        public func len(val:string)=> int;

    extern("rslib_std_str_bytelen") 
        public func bytelen(val:string)=> int;

    extern("rslib_std_string_sub")
        public func sub(val:string, begin:int)=> string;

    extern("rslib_std_string_subto")
        public func subto(val:string, begin:int, length:int)=> string;
    
    extern("rslib_std_string_toupper")
        public func upper(val:string)=> string;

    extern("rslib_std_string_tolower")
        public func lower(val:string)=> string;

    extern("rslib_std_string_isspace")
        public func isspace(val:string)=> bool;

    extern("rslib_std_string_isalpha")
        public func isalpha(val:string)=>  bool;

    extern("rslib_std_string_isalnum")
        public func isalnum(val:string)=>  bool;

    extern("rslib_std_string_isnumber")
        public func isnumber(val:string)=>  bool;

    extern("rslib_std_string_ishex")
        public func ishex(val:string)=>  bool;

    extern("rslib_std_string_isoct")
        public func isoct(val:string)=>  bool;

    extern("rslib_std_string_enstring")
        public func enstring(val:string)=> string;

    extern("rslib_std_string_destring")
        public func destring(val:string)=>  string;

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
        public func trim(val:string)=> string;

    extern("rslib_std_string_split")
        public func split(val:string, spliter:string)=> array<string>;

    public func append<CharOrCCharT>(val: string, ch: CharOrCCharT)=> string
        where ch is char || ch is cchar;
    {
        if (ch is char)
        {
            extern("rslib_std_string_append_char")func _append_char(val: string, ch: char)=> string;
            return _append_char(val, ch);
        }
        else
        {
            extern("rslib_std_string_append_cchar")func _append_cchar(val: string, ch: cchar)=> string;
            return _append_cchar(val, ch);
        }
    }
}
)" R"(
namespace array
{
    extern("rslib_std_array_create", repeat) 
        public func create<T>(sz: int, init_val: T)=> array<T>;

    extern("rslib_std_serialize", repeat) 
        public func serialize<T>(self: array<T>)=> option<string>;

    extern("rslib_std_parse_array_from_string", repeat)
        public func deserialize(val: string)=> option<array<dynamic>>;

    public func append<T>(self: array<T>, elem: T)
    {
        let newarr = self->tovec;
        newarr->add(elem);

        return newarr->unsafe::cast:<array<T>>;
    }

    public func erase<T>(self: array<T>, index: int)
    {
        let newarr = self->tovec;
        do newarr->remove(index);

        return newarr->unsafe::cast:<array<T>>;
    }
    public func inlay<T>(self: array<T>, index: int, insert_value: T)
    {
        let newarr = self->tovec;
        newarr->insert(index, insert_value);

        return newarr->unsafe::cast:<array<T>>;
    }

    extern("rslib_std_create_str_by_wchar", repeat) 
        public func str(buf: array<char>)=> string;

    extern("rslib_std_create_str_by_ascii", repeat) 
        public func cstr(buf: array<cchar>)=> string;

    extern("rslib_std_lengthof", repeat) 
        public func len<T>(val: array<T>)=> int;

    extern("rslib_std_make_dup", repeat)
        public func dup<T>(val: array<T>)=> array<T>;

    extern("rslib_std_make_dup", repeat)
        public func tovec<T>(val: array<T>)=> vec<T>;

    extern("rslib_std_array_empty", repeat)
        public func empty<T>(val: array<T>)=> bool;

    public func resize<T>(val: array<T>, newsz: int, init_val: T)
    {
        let newarr = val->tovec;
        newarr->resize(newsz, init_val);

        return newarr as vec<T>->unsafe::cast:<array<T>>;
    }
    public func shrink<T>(val: array<T>, newsz: int)
    {   
        let newarr = val->tovec;
        do newarr->shrink(newsz);

        return newarr as vec<T>->unsafe::cast:<array<T>>;
    }

    extern("rslib_std_array_get", repeat)
        public func get<T>(a: array<T>, index: int)=> option<T>;

    extern("rslib_std_array_get_or_default", repeat)
        public func getor<T>(a: array<T>, index: int, val: T)=> T;

    extern("rslib_std_array_find", repeat)
        public func find<T>(val:array<T>, elem:T)=> int;

    public func findif<T>(val:array<T>, judger:(T)=> bool)
    {
        let mut count = 0;
        for (let v : val)
            if (judger(v))
                return count;
            else
                count += 1;
        return -1;
    }

    public func forall<T>(val: array<T>, functor: (T)=> bool)
    {
        let result = []mut: vec<T>;
        for (let elem : val)
            if (functor(elem))
                result->add(elem);
        return result->unsafe::cast:<array<T>>;
    }

    public func bind<T, R>(val: array<T>, functor: (T)=> array<R>)
    {
        let result = []mut: vec<R>;
        for (let elem : val)
            for (let insert : functor(elem))
                result->add(insert);
        return result->unsafe::cast:<array<R>>;
    }

    public func map<T, R>(val: array<T>, functor: (T)=> R)
    {
        let result = []mut: vec<R>;
        for (let elem : val)
        {
            let r = functor(elem);
            result->add(r);
        }
        return result->unsafe::cast:<array<R>>;
    }

    public func mapping<K, V>(val: array<(K, V)>)
    {
        let result = {}mut: map<K, V>;
        for (let (k, v) : val)
            result->set(k, v);
        return result->unsafe::cast:<dict<K, V>>;
    }

    public func reduce<T>(self: array<T>, reducer: (T, T)=> T)
    {
        if (self->empty)
            return option::none;

        let mut result = self[0];
        for (let mut i = 1; i < self->len; i+=1)
            result = reducer(result, self[i]);
            
        return option::value(result);
    }

    public func rreduce<T>(self: array<T>, reducer: (T, T)=> T)
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
        extern("rslib_std_array_iter_next", repeat)
            public func next<T>(iter:iterator<T>)=> option<T>;
    }

    extern("rslib_std_array_iter", repeat)
        public func iter<T>(val:array<T>)=> iterator<T>;

    extern("rslib_std_array_connect", repeat)
        public func connect<T>(self: array<T>, another: array<T>)=> array<T>;

    extern("rslib_std_array_sub", repeat)
    public func sub<T>(self: array<T>, begin: int)=> array<T>;
    
    extern("rslib_std_array_subto", repeat)
    public func subto<T>(self: array<T>, begin: int, count: int)=> array<T>;

    extern("rslib_std_array_front", repeat)
    public func front<T>(val: array<T>)=> option<T>;

    extern("rslib_std_array_back", repeat)
    public func back<T>(val: array<T>)=> option<T>;

    extern("rslib_std_array_front_val", repeat)
    public func frontval<T>(val: array<T>)=> T;

    extern("rslib_std_array_back_val", repeat)
    public func backval<T>(val: array<T>)=> T;
}

namespace vec
{
    extern("rslib_std_array_create", repeat) 
        public func create<T>(sz: int, init_val: T)=> vec<T>;

    extern("rslib_std_serialize", repeat) 
        public func serialize<T>(self: vec<T>)=> option<string>;

    extern("rslib_std_parse_array_from_string", repeat) 
        public func deserialize(val: string)=> option<vec<dynamic>>;

    extern("rslib_std_create_str_by_wchar", repeat) 
        public func str(buf: vec<char>)=> string;

    extern("rslib_std_create_str_by_ascii", repeat) 
        public func cstr(buf: vec<cchar>)=> string;

    extern("rslib_std_lengthof", repeat) 
        public func len<T>(val: vec<T>)=> int;

    extern("rslib_std_make_dup", repeat)
        public func dup<T>(val: vec<T>)=> vec<T>;

    extern("rslib_std_make_dup", repeat)
        public func toarray<T>(val: vec<T>)=> array<T>;

    extern("rslib_std_array_empty", repeat)
        public func empty<T>(val: vec<T>)=> bool;

    extern("rslib_std_array_resize") 
        public func resize<T>(val: vec<T>, newsz: int, init_val: T)=> void;

    extern("rslib_std_array_shrink")
        public func shrink<T>(val: vec<T>, newsz: int)=> bool;

    extern("rslib_std_array_insert") 
        public func insert<T>(val: vec<T>, insert_place: int, insert_val: T)=> void;

    extern("rslib_std_array_swap")
        public func swap<T>(val: vec<T>, another: vec<T>)=> void;

    extern("rslib_std_array_copy") 
        public func copy<T, C>(val: vec<T>, another: C<T>)=> void
            where std::is_array:<C<T>> || std::is_vec:<C<T>>;

    extern("rslib_std_array_get", repeat)
        public func get<T>(a: vec<T>, index: int)=> option<T>;

    extern("rslib_std_array_add") 
        public func add<T>(val: vec<T>, elem: T)=> void;

    extern("rslib_std_array_connect", repeat)
        public func connect<T>(self: vec<T>, another: vec<T>)=> vec<T>;

    extern("rslib_std_array_sub", repeat)
    public func sub<T>(self: vec<T>, begin: int)=> vec<T>;
    
    extern("rslib_std_array_subto", repeat)
    public func subto<T>(self: vec<T>, begin: int, count: int)=> vec<T>;

    extern("rslib_std_array_pop") 
        public func pop<T>(val: vec<T>)=> option<T>;  

    extern("rslib_std_array_dequeue") 
        public func dequeue<T>(val: vec<T>)=> option<T>;  

    extern("rslib_std_array_pop_val") 
        public func popval<T>(val: vec<T>)=> T;  

    extern("rslib_std_array_dequeue_val") 
        public func dequeueval<T>(val: vec<T>)=> T;  

    extern("rslib_std_array_remove")
        public func remove<T>(val:vec<T>, index:int)=> bool;

    extern("rslib_std_array_find", repeat)
        public func find<T>(val:vec<T>, elem:T)=> int;

    public func findif<T>(val:vec<T>, judger:(T)=> bool)
    {
        let mut count = 0;
        for (let v : val)
            if (judger(v))
                return count;
            else
                count += 1;
        return -1;
    }

    extern("rslib_std_array_clear")
        public func clear<T>(val:vec<T>)=> void;

    public func forall<T>(val: vec<T>, functor: (T)=> bool)
    {
        let result = []mut: vec<T>;
        for (let elem : val)
            if (functor(elem))
                result->add(elem);
        return result;
    }

    public func bind<T, R>(val: vec<T>, functor: (T)=>vec<R>)
    {
        let result = []mut: vec<R>;
        for (let elem : val)
            for (let insert : functor(elem))
                result->add(insert);
        return result;
    }

    public func map<T, R>(val: vec<T>, functor: (T)=> R)
    {
        let result = []mut: vec<R>;
        for (let elem : val)
            result->add(functor(elem));
        return result;
    }

    public func mapping<K, V>(val: vec<(K, V)>)
    {
        let result = {}mut: map<K, V>;
        for (let (k, v) : val)
            result->set(k, v);
        return result->unsafe::cast:<dict<K, V>>;
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
        extern("rslib_std_array_iter_next", repeat)
            public func next<T>(iter:iterator<T>)=> option<T>;
    }

    extern("rslib_std_array_iter", repeat)
        public func iter<T>(val:vec<T>)=> iterator<T>;

    extern("rslib_std_array_front", repeat)
    public func front<T>(val: vec<T>)=> option<T>;

    extern("rslib_std_array_back", repeat)
    public func back<T>(val: vec<T>)=> option<T>;

    extern("rslib_std_array_front_val", repeat)
    public func frontval<T>(val: vec<T>)=> T;

    extern("rslib_std_array_back_val", repeat)
    public func backval<T>(val: vec<T>)=> T;
}

namespace dict
{
    extern("rslib_std_serialize", repeat) 
        public func serialize<KT, VT>(self: dict<KT, VT>)=> option<string>;

    extern("rslib_std_parse_map_from_string", repeat) 
        public func deserialize(val: string)=> option<dict<dynamic, dynamic>>;

    public func bind<KT, VT, RK, RV>(val: dict<KT, VT>, functor: (KT, VT)=> dict<RK, RV>)
    {
        let result = {}mut: map<RK, RV>;
        for (let (k, v) : val)
            for (let (rk, rv) : functor(k, v))
                result->set(rk, rv);
        return result->unsafe::cast:<dict<RK, RV>>;
    }

    public func apply<KT, VT>(self: dict<KT, VT>, key: KT, val: VT)
    {
        let newmap = self->tomap;
        newmap->set(key, val);

        return newmap->unsafe::cast:<dict<KT, VT>>;
    }
)" R"(
    extern("rslib_std_lengthof", repeat) 
        public func len<KT, VT>(self: dict<KT, VT>)=> int;

    extern("rslib_std_make_dup", repeat)
        public func dup<KT, VT>(self: dict<KT, VT>)=> dict<KT, VT>;

    extern("rslib_std_make_dup", repeat)
        public func tomap<KT, VT>(self: dict<KT, VT>)=> map<KT, VT>;

    public func findif<KT, VT>(self: dict<KT, VT>, judger:(KT)=> bool)
    {
        for (let (k, _) : self)
            if (judger(k))
                return option::value(k);
        return option::none;            
    }

    extern("rslib_std_map_only_get", repeat) 
        public func get<KT, VT>(self: dict<KT, VT>, index: KT)=> option<VT>;

    extern("rslib_std_map_find", repeat) 
        public func contain<KT, VT>(self: dict<KT, VT>, index: KT)=> bool;

    extern("rslib_std_map_get_or_default", repeat) 
        public func getor<KT, VT>(self: dict<KT, VT>, index: KT, default_val: VT)=> VT;

    extern("rslib_std_map_keys", repeat)
        public func keys<KT, VT>(self: dict<KT, VT>)=> array<KT>;

    extern("rslib_std_map_vals", repeat)
        public func vals<KT, VT>(self: dict<KT, VT>)=> array<VT>;

    extern("rslib_std_map_empty", repeat)
        public func empty<KT, VT>(self: dict<KT, VT>)=> bool;

    public func erase<KT, VT>(self: dict<KT, VT>, index: KT)
    {
        let newmap = self->tomap;
        do newmap->remove(index);

        return newmap->unsafe::cast:<dict<KT, VT>>;
    }

    public using iterator<KT, VT> = gchandle
    {
        extern("rslib_std_map_iter_next", repeat)
            public func next<KT, VT>(iter:iterator<KT, VT>)=> option<(KT, VT)>;

        public func iter<KT, VT>(iter:iterator<KT, VT>) { return iter; }
    }

    extern("rslib_std_map_iter", repeat)
        public func iter<KT, VT>(self:dict<KT, VT>)=> iterator<KT, VT>;

    public func forall<KT, VT>(self: dict<KT, VT>, functor: (KT, VT)=> bool)
    {
        let result = {}mut: map<KT, VT>;
        for (let (key, val) : self)
            if (functor(key, val))
                result->set(key, val);
        return result->unsafe::cast:<dict<KT, VT>>;
    }
    public func map<KT, VT, AT, BT>(self: dict<KT, VT>, functor: (KT, VT)=> (AT, BT))
    {
        let result = {}mut: map<AT, BT>;
        for (let (key, val) : self)
        {
            let (nk, nv) = functor(key, val);
            result->set(nk, nv);
        }
        return result->unsafe::cast:<dict<AT, BT>>;
    }
    public func unmapping<KT, VT>(self: dict<KT, VT>)
    {
        let result = []mut: vec<(KT, VT)>;
        for (let kvpair : self)
            result->add(kvpair);
        return result->unsafe::cast:<array<(KT, VT)>>;
    }
}

namespace map
{
    extern("rslib_std_serialize", repeat) 
        public func serialize<KT, VT>(self: map<KT, VT>)=> option<string>;
                    
    extern("rslib_std_parse_map_from_string", repeat) 
        public func deserialize(val: string)=> option<map<dynamic, dynamic>>;

    public func bind<KT, VT, RK, RV>(val: map<KT, VT>, functor: (KT, VT)=> map<RK, RV>)
    {
        let result = {}mut: map<RK, RV>;
        for (let (k, v) : val)
            for (let (rk, rv) : functor(k, v))
                result->set(rk, rv);
        return result;
    }

    extern("rslib_std_map_set") 
        public func set<KT, VT>(self: map<KT, VT>, key: KT, val: VT)=> void;

    extern("rslib_std_lengthof", repeat) 
        public func len<KT, VT>(self: map<KT, VT>)=> int;

    extern("rslib_std_make_dup", repeat)
        public func dup<KT, VT>(self: map<KT, VT>)=> map<KT, VT>;

    extern("rslib_std_make_dup", repeat)
        public func todict<KT, VT>(self: map<KT, VT>)=> dict<KT, VT>;

    public func findif<KT, VT>(self: map<KT, VT>, judger:(KT)=> bool)
    {
        for (let (k, _) : self)
            if (judger(k))
                return option::value(k);
        return option::none;            
    }

    extern("rslib_std_map_find", repeat) 
        public func contain<KT, VT>(self: map<KT, VT>, index: KT)=> bool;

    extern("rslib_std_map_only_get", repeat) 
        public func get<KT, VT>(self: map<KT, VT>, index: KT)=> option<VT>;

    extern("rslib_std_map_get_or_default", repeat) 
        public func getor<KT, VT>(self: map<KT, VT>, index: KT, default_val: VT)=> VT;

    extern("rslib_std_map_get_or_set_default") 
        public func getorset<KT, VT>(self: map<KT, VT>, index: KT, default_val: VT)=> VT;

    extern("rslib_std_map_swap") 
        public func swap<KT, VT>(val: map<KT, VT>, another: map<KT, VT>)=> void;

    extern("rslib_std_map_copy") 
        public func copy<KT, VT>(val: map<KT, VT>, another: map<KT, VT>)=> void;

    extern("rslib_std_map_keys", repeat)
        public func keys<KT, VT>(self: map<KT, VT>)=> array<KT>;

    extern("rslib_std_map_vals", repeat)
        public func vals<KT, VT>(self: map<KT, VT>)=> array<VT>;

    extern("rslib_std_map_empty", repeat)
        public func empty<KT, VT>(self: map<KT, VT>)=> bool;

    extern("rslib_std_map_remove")
        public func remove<KT, VT>(self: map<KT, VT>, index: KT)=> bool;

    extern("rslib_std_map_clear")
        public func clear<KT, VT>(self: map<KT, VT>)=> void;

    public using iterator<KT, VT> = gchandle
    {
        extern("rslib_std_map_iter_next", repeat)
            public func next<KT, VT>(iter:iterator<KT, VT>)=> option<(KT, VT)>;

        public func iter<KT, VT>(iter:iterator<KT, VT>) { return iter; }
    }

    extern("rslib_std_map_iter", repeat)
        public func iter<KT, VT>(self:map<KT, VT>)=> iterator<KT, VT>;

    public func forall<KT, VT>(self: map<KT, VT>, functor: (KT, VT)=>bool)
    {
        let result = {}mut: map<KT, VT>;
        for (let (key, val) : self)
            if (functor(key, val))
                result->set(key, val);
        return result;
    }
    public func map<KT, VT, AT, BT>(self: map<KT, VT>, functor: (KT, VT)=>(AT, BT))
    {
        let result = {}mut: map<AT, BT>;
        for (let (key, val) : self)
        {
            let (nk, nv) = functor(key, val);
            result->set(nk, nv);
        }
        return result->unsafe::cast:<map<AT, BT>>;
    }
    public func unmapping<KT, VT>(self: map<KT, VT>)
    {
        let result = []mut: vec<(KT, VT)>;
        for (let kvpair : self)
            result->add(kvpair);
        return result->unsafe::cast:<array<(KT, VT)>>;
    }
}

namespace int
{
    extern("rslib_std_int_to_hex")
        public func tohex(val: int)=> string;
    extern("rslib_std_int_to_oct")
        public func tooct(val: int)=> string;

    extern("rslib_std_hex_to_int")
        public func parsehex(val: string)=> int;
    extern("rslib_std_oct_to_int")
        public func parseoct(val: string)=> int;
}

namespace handle
{
    extern("rslib_std_handle_to_hex")
        public func tohex(val: handle)=> string;
    extern("rslib_std_handle_to_oct")
        public func tooct(val: handle)=> string;

    extern("rslib_std_hex_to_handle")
        public func parsehex(val: string)=> handle;
    extern("rslib_std_oct_to_handle")
        public func parseoct(val: string)=> handle;
}

namespace gchandle
{
    extern("rslib_std_gchandle_close", slow)
        public func close(handle:gchandle)=> bool;
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

WO_API wo_api rslib_std_debug_attach_default_debuggee(wo_vm vm, wo_value args)
{
    wo_attach_default_debuggee();
    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_debug_disattach_default_debuggee(wo_vm vm, wo_value args)
{
    wo_detach_debuggee();
    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_debug_callstack_trace(wo_vm vm, wo_value args)
{
    return wo_ret_string(vm, wo_debug_trace_callstack(vm, (size_t)wo_int(args + 0)));
}

WO_API wo_api rslib_std_debug_breakpoint(wo_vm vm, wo_value args)
{
    wo_break_specify_immediately(vm);
    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_debug_invoke(wo_vm vm, wo_value args)
{
    size_t argc = (size_t)wo_vaarg_count(vm);

    for (size_t index = argc - 1; index > 0; index--)
        wo_push_val(vm, args + index);

    return wo_ret_val(vm, wo_invoke_value(vm, args, argc - 1));
}

WO_API wo_api rslib_std_debug_empty_func(wo_vm vm, wo_value args)
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
        extern("rslib_std_debug_breakpoint", slow)
            public func breakpoint()=> void;

        extern("rslib_std_debug_attach_default_debuggee", slow)
            public func attach_debuggee()=> void;
        extern("rslib_std_debug_disattach_default_debuggee", slow)
            public func disattach_debuggee()=> void;

        public func breakdown()
        {
            attach_debuggee();
            breakpoint();
        }

        extern("rslib_std_debug_callstack_trace")
            public func callstack(layer:int) =>  string;

        public func run<FT>(foo: FT, ...)
        {
            attach_debuggee();
            let result = (foo:(...)=>dynamic)(......);
            disattach_debuggee();
    
            return result;
        }

        extern("rslib_std_debug_invoke", slow)
        public func invoke<FT>(foo:FT, ...)=> typeof(foo(......));

        // Used for create a value with specified type, it's a dangergous function.
        extern("rslib_std_debug_empty_func")
        public func __empty_function<T>()=> T;
    }
}
)" };

WO_API wo_api rslib_std_macro_lexer_error(wo_vm vm, wo_value args)
{
    wo::lexer* lex = (wo::lexer*)wo_pointer(args + 0);

    lex->lex_error(wo::lexer::errorlevel::error, wo::str_to_wstr(wo_string(args + 1)).c_str());
    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_macro_lexer_peek(wo_vm vm, wo_value args)
{
    wo::lexer* lex = (wo::lexer*)wo_pointer(args + 0);

    std::wstring out_result;
    auto token_type = lex->peek(&out_result);

    wo_value result = wo_push_empty(vm);
    wo_value elem = wo_push_empty(vm);

    wo_set_struct(result, vm, 2);
    wo_set_int(elem, (wo_integer_t)token_type);
    wo_struct_set(result, 0, elem);
    wo_set_string(elem, vm, wo::wstr_to_str(out_result).c_str());
    wo_struct_set(result, 1, elem);

    return wo_ret_val(vm, result);
}

WO_API wo_api rslib_std_macro_lexer_next(wo_vm vm, wo_value args)
{
    wo::lexer* lex = (wo::lexer*)wo_pointer(args + 0);

    std::wstring out_result;
    auto token_type = lex->next(&out_result);

    wo_value result = wo_push_empty(vm);
    wo_value elem = wo_push_empty(vm);

    wo_set_struct(result, vm, 2);
    wo_set_int(elem, (wo_integer_t)token_type);
    wo_struct_set(result, 0, elem);
    wo_set_string(elem, vm, wo::wstr_to_str(out_result).c_str());
    wo_struct_set(result, 1, elem);

    return wo_ret_val(vm, result);
}

WO_API wo_api rslib_std_macro_lexer_nextch(wo_vm vm, wo_value args)
{
    wo::lexer* lex = (wo::lexer*)wo_pointer(args + 0);

    wchar_t ch[2] = {};

    int readch = lex->next_one();

    if (readch == EOF)
        return wo_ret_string(vm, "");

    ch[0] = (wchar_t)readch;
    return wo_ret_string(vm, wo::wstr_to_str(ch).c_str());
}

WO_API wo_api rslib_std_macro_lexer_peekch(wo_vm vm, wo_value args)
{
    wo::lexer* lex = (wo::lexer*)wo_pointer(args + 0);

    wchar_t ch[2] = {};

    int readch = lex->peek_one();

    if (readch == EOF)
        return wo_ret_string(vm, "");

    ch[0] = (wchar_t)readch;
    return wo_ret_string(vm, wo::wstr_to_str(ch).c_str());
}

WO_API wo_api rslib_std_macro_lexer_current_path(wo_vm vm, wo_value args)
{
    wo::lexer* lex = (wo::lexer*)wo_pointer(args + 0);
    return wo_ret_string(vm, wo::wstr_to_str(*lex->source_file).c_str());
}

WO_API wo_api rslib_std_macro_lexer_current_rowno(wo_vm vm, wo_value args)
{
    wo::lexer* lex = (wo::lexer*)wo_pointer(args + 0);
    return wo_ret_int(vm, (wo_integer_t)lex->now_file_rowno);
}

WO_API wo_api rslib_std_macro_lexer_current_colno(wo_vm vm, wo_value args)
{
    wo::lexer* lex = (wo::lexer*)wo_pointer(args + 0);
    return wo_ret_int(vm, (wo_integer_t)lex->now_file_colno);
}

const char* wo_stdlib_macro_src_path = u8"woo/macro.wo";
const char* wo_stdlib_macro_src_data = {
u8R"(
import woo::std;

namespace std
{
    public enum token_type
    {
        l_eof = -1,
        l_error = 0,
        l_empty,                // [empty]
        l_identifier,           // identifier.
        l_literal_integer,      // 1 233 0x123456 0b1101001 032
        l_literal_handle,       // 0L 256L 0xFFL
        l_literal_real,         // 0.2  0.  .235
        l_literal_string,       // "" "helloworld" @"println("hello");"@
        l_literal_char,         // 'x'
        l_format_string_begin,  // F"..{
        l_format_string,        // }..{ 
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
        l_inv_direct,
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
        l_immut,
        l_typeid,
        l_macro,
        l_unknown_token,
    }

    public using lexer = handle
    {
        extern("rslib_std_macro_lexer_error")
            public func error(lex:lexer, msg:string)=> void;

        extern("rslib_std_macro_lexer_peek")
            public func peektoken(lex:lexer)=> (token_type, string);

        extern("rslib_std_macro_lexer_next")
            public func nexttoken(lex:lexer)=> (token_type, string);
        
        private func wrap_token(type: token_type, str: string)
        {
            if (type == token_type::l_literal_string)
                return str->enstring;
            else if (type == token_type::l_format_string_begin)
            {
                let enstr = str->enstring;
                return "F" + enstr->subto(0, enstr->len-1) + "{";
            }
            else if (type == token_type::l_format_string)
            {
                let enstr = str->enstring;
                return "}" + enstr->subto(1, enstr->len-2) + "{";
            }
            else if (type == token_type::l_format_string_end)
            {
                let enstr = str->enstring;
                return "}" + enstr->subto(1, enstr->len-1);
            }
            else if (type == token_type::l_literal_char)
            {
                let enstr = str->enstring;
                return F"'{enstr->subto(1, enstr->len-2)}'";
            }
            return str;
        }

        public func peek(lex: lexer)
        {
            return wrap_token(lex->peektoken...);
        }
        public func next(lex: lexer)
        {
            return wrap_token(lex->nexttoken...);
        }

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

        public func trytoken(self: lexer, token: token_type)=> option<string>
        {
            let (tok, _) = self->peektoken();
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

WO_API wo_api rslib_std_call_shell(wo_vm vm, wo_value args)
{
    if (wo::config::ENABLE_SHELL_PACKAGE)
        return wo_ret_int(vm, system(wo_string(args + 0)));
    else
        return wo_ret_panic(vm, "Function defined in 'std/shell.wo' has been forbidden, "
            "trying to restart without '--enable-shell 0'.");
}

WO_API wo_api rslib_std_get_env(wo_vm vm, wo_value args)
{
    if (wo::config::ENABLE_SHELL_PACKAGE)
    {
        const char* env = getenv(wo_string(args + 0));
        if (env)
            return wo_ret_option_string(vm, env);
        return wo_ret_option_none(vm);
    }
    else
        return wo_ret_panic(vm, "Function defined in 'std/shell.wo' has been forbidden, "
            "trying to restart without '--enable-shell 0'.");
}

const char* wo_stdlib_shell_src_path = u8"woo/shell.wo";
const char* wo_stdlib_shell_src_data = {
u8R"(
namespace std
{
    extern("rslib_std_call_shell", slow)
        public func shell(cmd: string)=> int;

    extern("rslib_std_get_env")
        public func env(name: string)=> option<string>;
}
)" };
