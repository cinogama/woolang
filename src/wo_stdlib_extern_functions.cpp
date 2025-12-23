#include "wo_afx.hpp"

#include "wo_lang_extern_symbol_loader.hpp"

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
    size_t argc = (size_t)wo_argc(vm);

    auto leaved = wo_leave_gcguard(vm);
    {
        for (size_t i = 0; i < argc; i++)
        {
            wo::wo_stdout << wo_cast_string(args + i);

            if (i + 1 < argc)
                wo::wo_stdout << " ";
        }
    }
    if (leaved)
        wo_enter_gcguard(vm);

    return wo_ret_void(vm);
}
WO_API wo_api rslib_std_panic(wo_vm vm, wo_value args)
{
    return wo_ret_panic(vm, wo_string(args + 0));
}

WO_API wo_api rslib_std_str_char_len(wo_vm vm, wo_value args)
{
    return wo_ret_int(vm, (wo_integer_t)wo_str_char_len(args));
}
WO_API wo_api rslib_std_str_byte_len(wo_vm vm, wo_value args)
{
    return wo_ret_int(vm, (wo_integer_t)wo_str_byte_len(args));
}

WO_API wo_api rslib_std_make_dup(wo_vm vm, wo_value args)
{
    return wo_ret_dup(vm, args + 0);
}

WO_API wo_api rslib_std_string_toupper(wo_vm vm, wo_value args)
{
    size_t len = 0;
    wo_string_t rawstr = wo_raw_string(args + 0, &len);

    auto wstr = wo::u8strtou32(rawstr, len);

    for (auto& ch : wstr)
        ch = wo::u32isu16(ch)
        ? static_cast<wo_wchar_t>(towupper(static_cast<wchar_t>(ch)))
        : ch;

    auto result = wo::u32strtou8(wstr.c_str(), wstr.size());

    return wo_ret_raw_string(vm, result.c_str(), result.size());
}

WO_API wo_api rslib_std_string_tolower(wo_vm vm, wo_value args)
{
    size_t len = 0;
    wo_string_t rawstr = wo_raw_string(args + 0, &len);

    auto wstr = wo::u8strtou32(rawstr, len);

    for (auto& ch : wstr)
        ch = wo::u32isu16(ch)
        ? static_cast<wo_wchar_t>(towlower(static_cast<wchar_t>(ch)))
        : ch;

    auto result = wo::u32strtou8(wstr.c_str(), wstr.size());
    return wo_ret_raw_string(vm, result.c_str(), result.size());
}

WO_API wo_api rslib_std_string_isspace(wo_vm vm, wo_value args)
{
    size_t len = 0;
    wo_string_t rawstr = wo_raw_string(args + 0, &len);

    auto wstr = wo::u8strtou32(rawstr, len);

    if (!wstr.empty())
    {
        for (auto ch : wstr)
            if (!wo::u32isu16(ch) || !iswspace(static_cast<wchar_t>(ch)))
                return wo_ret_bool(vm, WO_FALSE);
        return wo_ret_bool(vm, WO_TRUE);
    }
    return wo_ret_bool(vm, WO_FALSE);
}

WO_API wo_api rslib_std_string_isalpha(wo_vm vm, wo_value args)
{
    size_t len = 0;
    wo_string_t rawstr = wo_raw_string(args + 0, &len);

    auto wstr = wo::u8strtou32(rawstr, len);

    if (!wstr.empty())
    {
        for (auto ch : wstr)
            if (wo::u32isu16(ch) && !iswalpha(static_cast<wchar_t>(ch)))
                return wo_ret_bool(vm, WO_FALSE);
        return wo_ret_bool(vm, WO_TRUE);
    }
    return wo_ret_bool(vm, WO_FALSE);
}

WO_API wo_api rslib_std_string_isalnum(wo_vm vm, wo_value args)
{
    size_t len = 0;
    wo_string_t rawstr = wo_raw_string(args + 0, &len);

    auto wstr = wo::u8strtou32(rawstr, len);

    if (!wstr.empty())
    {
        for (auto ch : wstr)
            if (wo::u32isu16(ch) && !iswalnum(static_cast<wchar_t>(ch)))
                return wo_ret_bool(vm, WO_FALSE);
        return wo_ret_bool(vm, WO_TRUE);
    }
    return wo_ret_bool(vm, WO_FALSE);
}

WO_API wo_api rslib_std_string_isnumber(wo_vm vm, wo_value args)
{
    size_t len = 0;
    wo_string_t rawstr = wo_raw_string(args + 0, &len);

    auto wstr = wo::u8strtou32(rawstr, len);

    if (!wstr.empty())
    {
        for (auto ch : wstr)
            if (!wo::u32isu16(ch) || !iswdigit(static_cast<wchar_t>(ch)))
                return wo_ret_bool(vm, WO_FALSE);
        return wo_ret_bool(vm, WO_TRUE);
    }
    return wo_ret_bool(vm, WO_FALSE);
}

WO_API wo_api rslib_std_string_ishex(wo_vm vm, wo_value args)
{
    size_t len = 0;
    wo_string_t rawstr = wo_raw_string(args + 0, &len);

    auto wstr = wo::u8strtou32(rawstr, len);

    if (!wstr.empty())
    {
        for (auto ch : wstr)
            if (!wo::u32isu16(ch) || !iswxdigit(static_cast<wchar_t>(ch)))
                return wo_ret_bool(vm, WO_FALSE);
        return wo_ret_bool(vm, WO_TRUE);
    }
    return wo_ret_bool(vm, WO_FALSE);
}

WO_API wo_api rslib_std_string_isoct(wo_vm vm, wo_value args)
{
    size_t len = 0;
    wo_string_t rawstr = wo_raw_string(args + 0, &len);

    auto wstr = wo::u8strtou32(rawstr, len);

    if (!wstr.empty())
    {
        for (auto& ch : wstr)
            if (ch < static_cast<wo_wchar_t>('0') || ch > static_cast<wo_wchar_t>('7'))
                return wo_ret_bool(vm, WO_FALSE);
        return wo_ret_bool(vm, WO_TRUE);
    }
    return wo_ret_bool(vm, WO_FALSE);
}

WO_API wo_api rslib_std_char_tostring(wo_vm vm, wo_value args)
{
    const wo_wchar_t wc = wo_char(args + 0);

    size_t len;
    char result[wo::UTF8MAXLEN];

    wo::u32exractu8(wc, result, &len);

    return wo_ret_raw_string(vm, result, len);
}

WO_API wo_api rslib_std_char_toupper(wo_vm vm, wo_value args)
{
    const wo_wchar_t wc = wo_char(args + 0);

    return wo_ret_char(
        vm, wo::u32isu16(wc) ? static_cast<wo_wchar_t>(towupper(static_cast<wchar_t>(wc))) : wc);
}

WO_API wo_api rslib_std_char_tolower(wo_vm vm, wo_value args)
{
    const wo_wchar_t wc = wo_char(args + 0);
    return wo_ret_char(vm, wo::u32isu16(wc) ? tolower(wc) : wc);
}

WO_API wo_api rslib_std_char_isspace(wo_vm vm, wo_value args)
{
    const wo_wchar_t wc = wo_char(args + 0);
    return wo_ret_bool(
        vm, WO_CBOOL(wo::u32isu16(wc) && iswspace(static_cast<wchar_t>(wc))));
}

WO_API wo_api rslib_std_char_isalpha(wo_vm vm, wo_value args)
{
    const wo_wchar_t wc = wo_char(args + 0);
    return wo_ret_bool(vm, WO_CBOOL(!wo::u32isu16(wc) || iswalpha(static_cast<wchar_t>(wc))));
}

WO_API wo_api rslib_std_char_isalnum(wo_vm vm, wo_value args)
{
    const wo_wchar_t wc = wo_char(args + 0);
    return wo_ret_bool(vm, WO_CBOOL(!wo::u32isu16(wc) || iswalnum(static_cast<wchar_t>(wc))));
}

WO_API wo_api rslib_std_char_isnumber(wo_vm vm, wo_value args)
{
    const wo_wchar_t wc = wo_char(args + 0);
    return wo_ret_bool(vm, WO_CBOOL(wo::u32isu16(wc) && iswdigit(static_cast<wchar_t>(wc))));
}

WO_API wo_api rslib_std_char_ishex(wo_vm vm, wo_value args)
{
    const wo_wchar_t wc = wo_char(args + 0);
    return wo_ret_bool(vm, WO_CBOOL(wo::u32isu16(wc) && iswxdigit(static_cast<wchar_t>(wc))));
}

WO_API wo_api rslib_std_char_isoct(wo_vm vm, wo_value args)
{
    const wo_wchar_t wc = wo_char(args + 0);
    return wo_ret_bool(vm, WO_CBOOL(wc >= static_cast<wo_wchar_t>('0') && wc <= static_cast<wo_wchar_t>('7')));
}

WO_API wo_api rslib_std_char_hexnum(wo_vm vm, wo_value args)
{
    const wo_wchar_t wc = wo_char(args + 0);
    if (!wo::u32isu16(wc) || !iswxdigit(static_cast<wchar_t>(wc)))
        return wo_ret_panic(vm, "Non-hexadecimal character.");

    return wo_ret_int(vm, wo::lexer::lex_hextonum(static_cast<wchar_t>(wc)));
}

WO_API wo_api rslib_std_char_octnum(wo_vm vm, wo_value args)
{
    auto ch = wo_char(args + 0);
    if (ch < static_cast<wo_wchar_t>('0') || ch > static_cast<wo_wchar_t>('7'))
        return wo_ret_panic(vm, "Non-octal character.");

    return wo_ret_int(vm, wo::lexer::lex_octtonum(ch));
}


WO_API wo_api rslib_std_string_enstring(wo_vm vm, wo_value args)
{
    size_t len;
    wo_string_t str = wo_raw_string(args + 0, &len);

    return wo_ret_string(
        vm,
        wo::u8enstring(str, len, false).c_str());
}

WO_API wo_api rslib_std_string_destring(wo_vm vm, wo_value args)
{
    std::string result = wo::u8destring(wo_string(args + 0));
    return wo_ret_raw_string(vm, result.data(), result.size());
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
    size_t match_len = 0;
    wo_string_t match_str = wo_raw_string(args + 1, &match_len);

    const auto wstr = wo::u8strtou32(str, len);
    const auto wmatch = wo::u8strtou32(match_str, match_len);

    size_t fnd_place = wstr.find(wmatch);
    if (fnd_place >= 0 && fnd_place < wstr.size())
        return wo_ret_option_int(vm, (wo_integer_t)fnd_place);

    return wo_ret_option_none(vm);
}
WO_API wo_api rslib_std_string_find_from(wo_vm vm, wo_value args)
{
    size_t len = 0;
    wo_string_t str = wo_raw_string(args + 0, &len);
    size_t match_len = 0;
    wo_string_t match_str = wo_raw_string(args + 1, &match_len);

    const auto wstr = wo::u8strtou32(str, len);
    const auto wmatch = wo::u8strtou32(match_str, match_len);

    const size_t from = (size_t)wo_int(args + 2);

    size_t fnd_place = wstr.find(wmatch, from);
    if (fnd_place >= from && fnd_place < wstr.size())
        return wo_ret_option_int(vm, (wo_integer_t)fnd_place);

    return wo_ret_option_none(vm);
}
WO_API wo_api rslib_std_string_rfind(wo_vm vm, wo_value args)
{
    size_t len = 0;
    wo_string_t str = wo_raw_string(args + 0, &len);
    size_t match_len = 0;
    wo_string_t match_str = wo_raw_string(args + 1, &match_len);

    const auto wstr = wo::u8strtou32(str, len);
    const auto wmatch = wo::u8strtou32(match_str, match_len);

    size_t fnd_place = wstr.rfind(wmatch);
    if (fnd_place >= 0 && fnd_place < wstr.size())
        return wo_ret_option_int(vm, (wo_integer_t)fnd_place);

    return wo_ret_option_none(vm);
}
WO_API wo_api rslib_std_string_rfind_from(wo_vm vm, wo_value args)
{
    size_t len = 0;
    wo_string_t str = wo_raw_string(args + 0, &len);
    size_t match_len = 0;
    wo_string_t match_str = wo_raw_string(args + 1, &match_len);

    const auto wstr = wo::u8strtou32(str, len);
    const auto wmatch = wo::u8strtou32(match_str, match_len);

    const size_t from = (size_t)wo_int(args + 2);

    size_t fnd_place = wstr.rfind(wmatch, from);
    if (fnd_place >= 0 && fnd_place < from)
        return wo_ret_option_int(vm, (wo_integer_t)fnd_place);

    return wo_ret_option_none(vm);
}
WO_API wo_api rslib_std_string_trim(wo_vm vm, wo_value args)
{
    size_t len = 0;
    wo_string_t str = wo_raw_string(args + 0, &len);

    const auto wstr = wo::u8strtou32(str, len);
    const wo_wchar_t* wstrp = wstr.data();
    const size_t wstrlen = wstr.length();

    size_t ibeg = 0;
    size_t iend = wstrlen;
    for (; ibeg != iend; ibeg++)
    {
        auto uch = wstrp[ibeg];
        if (wo::u32isu16(uch) && (iswspace(static_cast<wchar_t>(uch)) || iswcntrl(static_cast<wchar_t>(uch))))
            continue;
        break;
    }

    for (; iend != ibeg; iend--)
    {
        auto uch = wstrp[iend - 1];
        if (wo::u32isu16(uch) && (iswspace(static_cast<wchar_t>(uch)) || iswcntrl(static_cast<wchar_t>(uch))))
            continue;
        break;
    }
    auto view = wo::u32strtou8(wstrp + ibeg, iend - ibeg);
    return wo_ret_raw_string(vm, view.data(), view.size());
}

struct string_split_iter_t
{
    const std::string_view m_str;
    const std::string m_sep;
    size_t m_split_from;
};

WO_API wo_api rslib_std_string_split(wo_vm vm, wo_value args)
{
    size_t len = 0;
    wo_string_t str = wo_raw_string(args + 0, &len);

    return wo_ret_gchandle(
        vm,
        new string_split_iter_t
        {
            std::string_view(str, len),
            wo_string(args + 1),
            0,
        },
        args + 0,
        [](wo_ptr_t p)
        {
            delete reinterpret_cast<string_split_iter_t*>(p);
        });
}

WO_API wo_api rslib_std_string_split_iter(wo_vm vm, wo_value args)
{
    auto* iter = reinterpret_cast<string_split_iter_t*>(wo_pointer(args + 0));

    if (iter->m_split_from > iter->m_str.length())
        return wo_ret_option_none(vm);

    if (iter->m_sep.empty())
    {
        wo_wchar_t useless;
        const auto* split_str_p =
            iter->m_str.data() + iter->m_split_from;
        const size_t split_str_len =
            iter->m_str.length() - iter->m_split_from;

        if (split_str_len == 0)
        {
            // Make index invalid;
            ++iter->m_split_from;
            wo_assert(iter->m_split_from > iter->m_str.length());

            return wo_ret_option_none(vm);
        }

        const size_t chlen = wo::u8charnlen(
            split_str_p,
            split_str_len);

        (void)useless;

        iter->m_split_from += chlen;
        return wo_ret_option_raw_string(
            vm,
            split_str_p,
            chlen);
    }
    else
    {
        size_t fnd_place = iter->m_str.find(iter->m_sep, iter->m_split_from);
        if (fnd_place < iter->m_split_from || fnd_place > iter->m_str.length())
        {
            // No sep found.
            auto view = iter->m_str.substr(iter->m_split_from);

            // Make index invalid;
            iter->m_split_from = iter->m_str.length() + 1;

            return wo_ret_option_raw_string(vm, view.data(), view.length());
        }
        auto view = iter->m_str.substr(
            iter->m_split_from, fnd_place - iter->m_split_from);

        iter->m_split_from = fnd_place + iter->m_sep.size();
        return wo_ret_option_raw_string(vm, view.data(), view.length());
    }
}

WO_API wo_api rslib_std_string_append_char(wo_vm vm, wo_value args)
{
    std::string str(_wo_raw_str_view(args + 0));
    auto wc = static_cast<char32_t>(wo_int(args + 1));

    size_t u8len;
    char u8str[wo::UTF8MAXLEN];

    wo::u32exractu8(wc, u8str, &u8len);

    str.append(u8str, u8len);

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

    auto leaved = wo_leave_gcguard(vm);
    {
        while (!(std::cin >> result))
        {
            char _useless_for_clear = 0;
            std::cin.clear();
            while (std::cin.readsome(&_useless_for_clear, 1));
        }
    }
    if (leaved)
        wo_enter_gcguard(vm);

    return wo_ret_int(vm, result);
}

WO_API wo_api rslib_std_input_readreal(wo_vm vm, wo_value args)
{
    // Read real value from keyboard, always return valid input result;
    wo_real_t result;
    auto leaved = wo_leave_gcguard(vm);
    {
        while (!(std::cin >> result))
        {
            char _useless_for_clear = 0;
            std::cin.clear();
            while (std::cin.readsome(&_useless_for_clear, 1));
        }
    }
    if (leaved)
        wo_enter_gcguard(vm);
    return wo_ret_real(vm, result);
}

WO_API wo_api rslib_std_input_readstring(wo_vm vm, wo_value args)
{
    // Read real value from keyboard, always return valid input result;
    std::string result;
    auto leaved = wo_leave_gcguard(vm);
    {
        while (!(std::cin >> result))
        {
            char _useless_for_clear = 0;
            std::cin.clear();
            while (std::cin.readsome(&_useless_for_clear, 1));
        }
    }
    if (leaved)
        wo_enter_gcguard(vm);
    return wo_ret_string(vm, result.c_str());
}

WO_API wo_api rslib_std_input_readline(wo_vm vm, wo_value args)
{
    // Read real value from keyboard, always return valid input result;
    std::string result;
    auto leaved = wo_leave_gcguard(vm);
    {
        while (!(std::getline(std::cin, result)))
        {
            char _useless_for_clear = 0;
            std::cin.clear();
            while (std::cin.readsome(&_useless_for_clear, 1));
        }
    }
    if (leaved)
        wo_enter_gcguard(vm);
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
    wo_arr_resize(args + 0, (wo_size_t)wo_int(args + 1), args + 2);
    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_array_shrink(wo_vm vm, wo_value args)
{
    size_t newsz = (size_t)wo_int(args + 1);
    if (newsz <= wo_arr_len(args + 0))
    {
        wo_arr_resize(args + 0, newsz, nullptr);
        return wo_ret_bool(vm, WO_TRUE);
    }
    return wo_ret_bool(vm, WO_FALSE);
}

WO_API wo_api rslib_std_array_insert(wo_vm vm, wo_value args)
{
    wo_arr_insert(args + 0, (wo_size_t)wo_int(args + 1), args + 2);
    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_array_swap(wo_vm vm, wo_value args)
{
    wo::value* arr1 = std::launder(reinterpret_cast<wo::value*>(args + 0));
    wo::value* arr2 = std::launder(reinterpret_cast<wo::value*>(args + 1));

    std::scoped_lock ssg1(arr1->m_array->gc_read_write_mx, arr2->m_array->gc_read_write_mx);

    if (wo::gc::gc_is_marking())
    {
        for (auto& elem : *arr1->m_array)
            wo::value::write_barrier(&elem);
        for (auto& elem : *arr2->m_array)
            wo::value::write_barrier(&elem);
    }

    arr1->m_array->swap(*arr2->m_array);

    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_array_copy(wo_vm vm, wo_value args)
{
    wo::value* arr1 = std::launder(reinterpret_cast<wo::value*>(args + 0));
    wo::value* arr2 = std::launder(reinterpret_cast<wo::value*>(args + 1));

    std::scoped_lock ssg1(arr1->m_array->gc_read_write_mx, arr2->m_array->gc_read_write_mx);
    *arr1->m_array->elem() = *arr2->m_array->elem();

    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_array_empty(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, wo_arr_is_empty(args + 0));
}
WO_API wo_api rslib_std_array_len(wo_vm vm, wo_value args)
{
    return wo_ret_int(vm, (wo_integer_t)wo_arr_len(args + 0));
}
WO_API wo_api rslib_std_array_get(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    wo_value arr = args + 0;
    wo_integer_t idx = wo_int(args + 1);

    wo_value elem = s + 0;

    if (wo_arr_try_get(elem, arr, (wo_size_t)idx))
        return wo_ret_option_val(vm, elem);

    return wo_ret_option_none(vm);
}

WO_API wo_api rslib_std_array_get_or_default(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    wo_value arr = args + 0;
    wo_integer_t idx = wo_int(args + 1);

    wo_value elem = s + 0;

    if (wo_arr_try_get(elem, arr, (wo_size_t)idx))
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
    wo_value s = wo_reserve_stack(vm, 1, &args);

    wo_value result = s + 0;
    wo_set_arr(result, 0);

    wo::value* arr_result = std::launder(reinterpret_cast<wo::value*>(result));
    wo::value* arr1 = std::launder(reinterpret_cast<wo::value*>(args + 0));
    wo::value* arr2 = std::launder(reinterpret_cast<wo::value*>(args + 1));
    do
    {
        wo::gcbase::gc_read_guard rg2(arr1->m_array);
        *arr_result->m_array->elem() = *arr1->m_array->elem();
    } while (0);
    do
    {
        wo::gcbase::gc_read_guard rg3(arr2->m_array);
        arr_result->m_array->insert(arr_result->m_array->end(),
            arr2->m_array->begin(), arr2->m_array->end());
    } while (0);

    return wo_ret_val(vm, result);
}

WO_API wo_api rslib_std_array_sub(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);
    wo_value result = s + 0;
    wo_set_arr(result, 0);

    wo::value* arr_result = std::launder(reinterpret_cast<wo::value*>(result));
    wo::value* arr1 = std::launder(reinterpret_cast<wo::value*>(args + 0));

    wo::gcbase::gc_read_guard rg2(arr1->m_array);

    auto begin = (size_t)wo_int(args + 1);
    if (begin > arr1->m_array->size())
        return wo_ret_panic(vm, "Index out of range when trying get sub array/vec.");

    arr_result->m_array->insert(arr_result->m_array->end(),
        arr1->m_array->begin() + begin, arr1->m_array->end());

    return wo_ret_val(vm, result);
}

WO_API wo_api rslib_std_array_subto(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    wo_value result = s + 0;
    wo_set_arr(result, 0);

    wo::value* arr_result = std::launder(reinterpret_cast<wo::value*>(result));
    wo::value* arr1 = std::launder(reinterpret_cast<wo::value*>(args + 0));

    wo::gcbase::gc_read_guard rg2(arr1->m_array);

    auto begin = (size_t)wo_int(args + 1);
    if (begin > arr1->m_array->size())
        return wo_ret_panic(vm, "Index out of range when trying get sub array/vec.");

    auto count = (size_t)wo_int(args + 2);

    if (begin + count > arr1->m_array->size())
        return wo_ret_panic(vm, "Index out of range when trying get sub array/vec.");

    auto&& begin_iter = arr1->m_array->begin() + begin;
    auto&& end_iter = begin_iter + count;

    arr_result->m_array->insert(arr_result->m_array->end(),
        begin_iter, end_iter);

    return wo_ret_val(vm, result);
}

WO_API wo_api rslib_std_array_sub_range(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 1, &args);

    wo_value result = s + 0;
    wo_set_arr(result, 0);

    wo::value* arr_result = std::launder(reinterpret_cast<wo::value*>(result));
    wo::value* arr1 = std::launder(reinterpret_cast<wo::value*>(args + 0));

    wo::gcbase::gc_read_guard rg2(arr1->m_array);

    auto begin = (size_t)wo_int(args + 1);
    auto end = (size_t)wo_int(args + 2);

    if (begin > arr1->m_array->size() || end > arr1->m_array->size())
        return wo_ret_panic(vm, "Index out of range when trying get sub array/vec.");

    if (end > begin)
    {
        auto&& begin_iter = arr1->m_array->begin() + begin;
        auto&& end_iter = arr1->m_array->begin() + end;

        arr_result->m_array->insert(arr_result->m_array->end(),
            begin_iter, end_iter);
    }
    return wo_ret_val(vm, result);
}

WO_API wo_api rslib_std_array_pop(wo_vm vm, wo_value args)
{
    wo_value elem = wo_reserve_stack(vm, 1, &args);
    if (wo_arr_pop_back(elem, args + 0))
        return wo_ret_option_val(vm, elem);
    return wo_ret_option_none(vm);
}

WO_API wo_api rslib_std_array_dequeue(wo_vm vm, wo_value args)
{
    wo_value elem = wo_reserve_stack(vm, 1, &args);
    if (wo_arr_pop_front(elem, args + 0))
        return wo_ret_option_val(vm, elem);
    return wo_ret_option_none(vm);
}

WO_API wo_api rslib_std_array_pop_val(wo_vm vm, wo_value args)
{
    wo_value elem = wo_reserve_stack(vm, 1, &args);
    wo_arr_pop_back_val(elem, args + 0);
    return wo_ret_val(vm, elem);
}

WO_API wo_api rslib_std_array_dequeue_val(wo_vm vm, wo_value args)
{
    wo_value elem = wo_reserve_stack(vm, 1, &args);
    wo_arr_pop_front_val(elem, args + 0);
    return wo_ret_val(vm, elem);
}

WO_API wo_api rslib_std_array_remove(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm,
        wo_arr_remove(args + 0, (wo_size_t)wo_int(args + 1)));
}

WO_API wo_api rslib_std_array_find(wo_vm vm, wo_value args)
{
    wo_size_t index;
    if (WO_TRUE == wo_arr_find(args + 0, args + 1, &index))
    {
        return wo_ret_option_int(vm, (wo_int_t)index);
    }
    return wo_ret_option_none(vm);
}

WO_API wo_api rslib_std_array_front(wo_vm vm, wo_value args)
{
    wo_value elem = wo_reserve_stack(vm, 1, &args);
    if (wo_arr_front(elem, args + 0))
        return wo_ret_option_val(vm, elem);
    return wo_ret_option_none(vm);
}
WO_API wo_api rslib_std_array_back(wo_vm vm, wo_value args)
{
    wo_value elem = wo_reserve_stack(vm, 1, &args);
    if (wo_arr_back(elem, args + 0))
        return wo_ret_option_val(vm, elem);
    return wo_ret_option_none(vm);
}

WO_API wo_api rslib_std_array_front_val(wo_vm vm, wo_value args)
{
    wo_value elem = wo_reserve_stack(vm, 1, &args);
    wo_arr_front_val(elem, args + 0);
    return wo_ret_val(vm, elem);
}
WO_API wo_api rslib_std_array_back_val(wo_vm vm, wo_value args)
{
    wo_value elem = wo_reserve_stack(vm, 1, &args);
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
    wo_value result = wo_reserve_stack(vm, 1, &args);
    wo_map_keys(result, vm, args + 0);
    return wo_ret_val(vm, result);
}
WO_API wo_api rslib_std_map_vals(wo_vm vm, wo_value args)
{
    wo_value result = wo_reserve_stack(vm, 1, &args);
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
        new array_iter{ arr->m_array->begin(), arr->m_array->end(), 0 },
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

WO_API wo_api rslib_std_map_create(wo_vm vm, wo_value args)
{
    wo_value result = wo_reserve_stack(vm, 1, &args);
    wo_set_map(result, (wo_size_t)wo_int(args + 0));
    return wo_ret_val(vm, result);
}

WO_API wo_api rslib_std_map_reserve(wo_vm vm, wo_value args)
{
    wo_map_set(args + 0, args + 1, args + 2);
    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_map_set(wo_vm vm, wo_value args)
{
    wo_map_set(args + 0, args + 1, args + 2);
    return wo_ret_void(vm);
}
WO_API wo_api rslib_std_map_len(wo_vm vm, wo_value args)
{
    return wo_ret_int(vm, (wo_integer_t)wo_map_len(args + 0));
}
WO_API wo_api rslib_std_map_only_get(wo_vm vm, wo_value args)
{
    wo_value elem = wo_reserve_stack(vm, 1, &args);

    if (wo_map_try_get(elem, args + 0, args + 1))
        return wo_ret_option_val(vm, elem);

    return wo_ret_option_none(vm);
}

WO_API wo_api rslib_std_map_get_or_set_default(wo_vm vm, wo_value args)
{
    wo_value elem = wo_reserve_stack(vm, 1, &args);
    wo_map_get_or_set(elem, args + 0, args + 1, args + 2);
    return wo_ret_val(vm, elem);
}


WO_API wo_api rslib_std_map_get_or_set_default_do(wo_vm vm, wo_value args)
{
    return wo_ret_map_get_or_set_do(vm, args + 0, args + 1, args + 2, &args, nullptr);
}

WO_API wo_api rslib_std_map_get_or_default(wo_vm vm, wo_value args)
{
    wo_value elem = wo_reserve_stack(vm, 1, &args);
    wo_map_get_or_default(elem, args + 0, args + 1, args + 2);
    return wo_ret_val(vm, elem);
}

WO_API wo_api rslib_std_map_swap(wo_vm vm, wo_value args)
{
    wo::value* map1 = std::launder(reinterpret_cast<wo::value*>(args + 0));
    wo::value* map2 = std::launder(reinterpret_cast<wo::value*>(args + 1));

    std::scoped_lock ssg1(map1->m_dictionary->gc_read_write_mx, map2->m_dictionary->gc_read_write_mx);

    if (wo::gc::gc_is_marking())
    {
        for (auto& [key, elem] : *map1->m_dictionary)
        {
            wo::value::write_barrier(&key);
            wo::value::write_barrier(&elem);
        }
        for (auto& [key, elem] : *map2->m_dictionary)
        {
            wo::value::write_barrier(&key);
            wo::value::write_barrier(&elem);
        }
    }

    map1->m_dictionary->swap(*map2->m_dictionary);

    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_map_copy(wo_vm vm, wo_value args)
{
    wo::value* map1 = std::launder(reinterpret_cast<wo::value*>(args + 0));
    wo::value* map2 = std::launder(reinterpret_cast<wo::value*>(args + 1));

    std::scoped_lock ssg1(map1->m_dictionary->gc_read_write_mx, map2->m_dictionary->gc_read_write_mx);
    *map1->m_dictionary->elem() = *map2->m_dictionary->elem();

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
    using mapping_iter_t = decltype(std::declval<wo::dictionary_t>().begin());

    mapping_iter_t iter;
    mapping_iter_t end_place;
};

WO_API wo_api rslib_std_map_iter(wo_vm vm, wo_value args)
{
    wo::value* mapp = std::launder(reinterpret_cast<wo::value*>(args));

    return wo_ret_gchandle(vm,
        new map_iter{ mapp->m_dictionary->begin(), mapp->m_dictionary->end() },
        args + 0,
        [](void* array_iter_t_ptr)
        {
            delete (map_iter*)array_iter_t_ptr;
        }
    );
}

WO_API wo_api rslib_std_map_iter_next(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);

    map_iter& iter = *(map_iter*)wo_pointer(args);

    if (iter.iter == iter.end_place)
        return wo_ret_option_none(vm);

    wo_value result_tuple = s + 0;
    wo_value elem = s + 1;
    wo_set_struct(result_tuple, 2);

    wo_set_val(
        elem, 
        std::launder(
            reinterpret_cast<wo_value>(
                const_cast<wo::value*>(&iter.iter->first)))); // key
    wo_struct_set(result_tuple, 0, elem);
    wo_set_val(
        elem, 
        std::launder(
            reinterpret_cast<wo_value>(
                &iter.iter->second))); // val
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
    wo_value s = wo_reserve_stack(vm, 2, &args);

    wo_string_t input = wo_string(args + 0);
    size_t token_length;
    char string_buf[1024];

    if (sscanf(input, "%s%zn", string_buf, &token_length) == 1)
    {
        wo_value result = s + 0;
        wo_value elem = s + 1;

        wo_set_struct(result, 2);

        wo_set_string(elem, input + token_length);
        wo_struct_set(result, 0, elem);
        wo_set_string(elem, string_buf);
        wo_struct_set(result, 1, elem);
        return wo_ret_option_val(vm, result);
    }

    return wo_ret_option_none(vm);
}

WO_API wo_api rslib_std_take_int(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);

    wo_string_t input = wo_string(args + 0);
    size_t token_length;
    long long integer;

    if (sscanf(input, "%lld%zn", &integer, &token_length) == 1)
    {
        wo_value result = s + 0;
        wo_value elem = s + 1;

        wo_set_struct(result, 2);

        wo_set_string(elem, input + token_length);
        wo_struct_set(result, 0, elem);
        wo_set_int(elem, (wo_integer_t)integer);
        wo_struct_set(result, 1, elem);
        return wo_ret_option_val(vm, result);
    }

    return wo_ret_option_none(vm);
}

WO_API wo_api rslib_std_take_real(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);

    wo_string_t input = wo_string(args + 0);
    size_t token_length;
    wo_real_t real;

    if (sscanf(input, "%lf%zn", &real, &token_length) == 1)
    {
        wo_value result = s + 0;
        wo_value elem = s + 1;

        wo_set_struct(result, 2);

        wo_set_string(elem, input + token_length);
        wo_struct_set(result, 0, elem);
        wo_set_real(elem, real);
        wo_struct_set(result, 1, elem);
        return wo_ret_option_val(vm, result);
    }

    return wo_ret_option_none(vm);
}

WO_API wo_api rslib_std_parse_map_from_string(wo_vm vm, wo_value args)
{
    wo_value result_dict = wo_reserve_stack(vm, 1, &args);
    if (wo_deserialize(result_dict, wo_string(args + 0), WO_MAPPING_TYPE))
        return wo_ret_option_val(vm, result_dict);
    return wo_ret_option_none(vm);
}

WO_API wo_api rslib_std_parse_array_from_string(wo_vm vm, wo_value args)
{
    wo_value result_arr = wo_reserve_stack(vm, 1, &args);
    if (wo_deserialize(result_arr, wo_string(args + 0), WO_ARRAY_TYPE))
        return wo_ret_option_val(vm, result_arr);
    return wo_ret_option_none(vm);
}

WO_API wo_api rslib_std_create_wchars_from_str(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);

    size_t len = 0;
    wo_string_t str = wo_raw_string(args + 0, &len);

    const auto buf = wo::u8strtou32(str, len);

    wo_value result_array = s + 0;
    wo_value elem = s + 1;

    wo_set_arr(result_array, buf.size());

    for (size_t i = 0; i < buf.size(); ++i)
    {
        wo_set_int(elem, (wo_int_t)(wo_handle_t)buf[i]);
        wo_arr_set(result_array, (wo_int_t)i, elem);
    }

    return wo_ret_val(vm, result_array);
}

WO_API wo_api rslib_std_create_chars_from_str(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);

    size_t len = 0;
    wo_string_t str = wo_raw_string(args + 0, &len);

    wo_value result_array = s + 0;
    wo_value elem = s + 1;

    wo_set_arr(result_array, len);

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
    wo_value s = wo_reserve_stack(vm, 1, &args);
    wo_size_t arrsz = (wo_size_t)wo_int(args + 0);

    wo_value newarr = s + 0;
    wo_set_arr(newarr, arrsz);

    for (wo_size_t i = 0; i < arrsz; ++i)
        wo_arr_set(newarr, i, args + 1);

    return wo_ret_val(vm, newarr);
}

WO_API wo_api rslib_std_create_str_by_wchar(wo_vm vm, wo_value args)
{
    wo::value* arr = std::launder(reinterpret_cast<wo::value*>(args + 0));
    wo::gcbase::gc_read_guard rg1(arr->m_array);

    std::vector<wo_wchar_t> buf;
    buf.reserve(arr->m_array->size());

    for (auto& value : *arr->m_array)
        buf.push_back(wo_char(std::launder(reinterpret_cast<wo_value>(&value))));

    std::string result = wo::u32strtou8(buf.data(), buf.size());
    return wo_ret_raw_string(vm, result.c_str(), result.size());
}

WO_API wo_api rslib_std_create_str_by_ascii(wo_vm vm, wo_value args)
{
    wo::value* arr = std::launder(reinterpret_cast<wo::value*>(args + 0));
    wo::gcbase::gc_read_guard rg1(arr->m_array);

    std::vector<char> buf;
    buf.reserve(arr->m_array->size());

    for (auto& value : *arr->m_array)
        buf.push_back(static_cast<char>(wo_int(std::launder(reinterpret_cast<wo_value>(&value)))));

    return wo_ret_raw_string(vm, buf.data(), buf.size());
}


WO_API wo_api rslib_std_return_itself(wo_vm vm, wo_value args)
{
    return wo_ret_val(vm, args + 0);
}

WO_API wo_api rslib_std_get_ascii_val_from_str(wo_vm vm, wo_value args)
{
    wo_size_t len = 0;
    wo_string_t str = wo_raw_string(args + 0, &len);

    wo_size_t idx = (wo_size_t)wo_int(args + 1);
    if (idx >= len)
        return wo_ret_panic(vm, "Index out of range.");

    return wo_ret_int(vm, (wo_int_t)str[idx]);
}

WO_API wo_api rslib_std_string_sub(wo_vm vm, wo_value args)
{
    size_t len = 0;
    wo_string_t str = wo_raw_string(args + 0, &len);

    size_t sub_str_len = 0;
    auto* substring = wo::u8substr(str, len, (size_t)wo_int(args + 1), &sub_str_len);
    return wo_ret_raw_string(vm, substring, sub_str_len);
}

WO_API wo_api rslib_std_string_sub_len(wo_vm vm, wo_value args)
{
    size_t len = 0;
    wo_string_t str = wo_raw_string(args + 0, &len);

    size_t sub_str_len = 0;
    auto* substring = wo::u8substrn(str, len, (size_t)wo_int(args + 1), (size_t)wo_int(args + 2), &sub_str_len);
    return wo_ret_raw_string(vm, substring, sub_str_len);
}

WO_API wo_api rslib_std_string_sub_range(wo_vm vm, wo_value args)
{
    size_t len = 0;
    wo_string_t str = wo_raw_string(args + 0, &len);

    size_t sub_str_len = 0;
    auto* substring = wo::u8substrr(str, len, (size_t)wo_int(args + 1), (size_t)wo_int(args + 2), &sub_str_len);
    return wo_ret_raw_string(vm, substring, sub_str_len);
}

WO_API wo_api rslib_std_thread_sleep(wo_vm vm, wo_value args)
{
    using namespace std;

    auto leaved = wo_leave_gcguard(vm);
    {
        std::this_thread::sleep_for(wo_real(args) * 1s);
    }
    if (leaved)
        wo_enter_gcguard(vm);
    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_gchandle_close(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, wo_gchandle_close(args));
}

WO_API wo_api rslib_std_tuple_nthcdr(wo_vm vm, wo_value args)
{
    // func _nthcdr<T>(t: T, idx : int)=> nthcdr_t<T, {Idx}> ;
    wo_value s = wo_reserve_stack(vm, 2, &args);
    wo_size_t len = wo_struct_len(args + 0);
    wo_size_t idx = (wo_size_t)wo_int(args + 1);

    wo_assert(idx <= len);
    wo_value result = s + 0;
    wo_value elem = s + 1;

    wo_set_struct(result, (uint16_t)(len - idx));
    for (size_t i = idx; i < len; ++i)
    {
        wo_struct_get(elem, args + 0, (uint16_t)i);
        wo_struct_set(result, (uint16_t)(i - idx), elem);
    }
    return wo_ret_val(vm, result);
}

WO_API wo_api rslib_std_tuple_cdr(wo_vm vm, wo_value args)
{
    // func _cdr<T>(t: T)=> cdr_t<T>
    wo_value s = wo_reserve_stack(vm, 2, &args);
    wo_size_t len = wo_struct_len(args + 0);

    wo_assert(1 <= len);
    wo_value result = s + 0;
    wo_value elem = s + 1;

    wo_set_struct(result, (uint16_t)(len - 1));
    for (size_t i = 1; i < len; ++i)
    {
        wo_struct_get(elem, args + 0, (uint16_t)i);
        wo_struct_set(result, (uint16_t)(i - 1), elem);
    }
    return wo_ret_val(vm, result);
}

WO_API wo_api rslib_std_int_to_hex(wo_vm vm, wo_value args)
{
    char result[18];
    wo_integer_t val = wo_int(args + 0);

    int written;
    if (val >= 0)
        written = snprintf(result, sizeof(result), "%llX", (unsigned long long)val);
    else
        written = snprintf(result, sizeof(result), "-%llX", (unsigned long long) - val);

    (void)written;
    wo_assert(written > 0 && written < (int)sizeof(result));
    return wo_ret_string(vm, result);
}

WO_API wo_api rslib_std_int_to_oct(wo_vm vm, wo_value args)
{
    char result[24];
    wo_integer_t val = wo_int(args + 0);

    int written;
    if (val >= 0)
        written = snprintf(result, sizeof(result), "%llo", (unsigned long long)val);
    else
        written = snprintf(result, sizeof(result), "-%llo", (unsigned long long) - val);

    (void)written;
    wo_assert(written > 0 && written < (int)sizeof(result));
    return wo_ret_string(vm, result);
}

WO_API wo_api rslib_std_get_args(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 2, &args);

    const auto& wo_arg_list = wo::get_args();
    wo_size_t argcarr = wo_arg_list.size();

    wo_value argsarr = s + 0;
    wo_value elem = s + 1;

    wo_set_arr(argsarr, argcarr);

    for (wo_size_t i = 0; i < argcarr; ++i)
    {
        wo_set_string(elem, wo_arg_list[i].c_str());
        wo_arr_set(argsarr, i, elem);
    }

    return wo_ret_val(vm, argsarr);
}

WO_API wo_api rslib_std_get_exe_path(wo_vm vm, wo_value args)
{
    return wo_ret_string(vm, wo::exe_path().c_str());
}

WO_API wo_api rslib_std_get_extern_symb(wo_vm vm, wo_value args)
{
    wo_unref_value extern_func;

    if (WO_TRUE == wo_extern_symb(&extern_func, vm, wo_string(args + 0)))
        return wo_ret_option_val(vm, &extern_func);

    return wo_ret_option_none(vm);
}

WO_API wo_api rslib_std_replace_tp(wo_vm vm, wo_value args)
{
    wo_integer_t new_tp_val = wo_int(args + 0);
    wo_value tp = wo_register(vm, WO_REG_TP);

    wo_integer_t old_tp_val = 0;
    if (wo_valuetype(tp) == WO_INTEGER_TYPE)
        old_tp_val = wo_int(tp);

    wo_set_int(tp, new_tp_val);

    return wo_ret_int(vm, old_tp_val);
}

WO_API wo_api rslib_std_equal_byte(wo_vm vm, wo_value args)
{
    return wo_ret_bool(vm, wo_equal_byte(args + 0, args + 1));
}

WO_API wo_api rslib_std_bad_function(wo_vm vm, wo_value args)
{
    return wo_ret_panic(vm, "This function cannot be called at runtime.");
}

WO_API wo_api rslib_std_get_env(wo_vm vm, wo_value args)
{
    static std::mutex env_lock;
    std::lock_guard g(env_lock);

    const char* env = getenv(wo_string(args + 0));
    if (env)
        return wo_ret_option_string(vm, env);
    return wo_ret_option_none(vm);
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

WO_API wo_api rslib_std_weakref_create(wo_vm vm, wo_value args)
{
    return wo_ret_gchandle(vm, wo_create_weak_ref(args + 0), nullptr,
        [](void* p)
        {
            wo_close_weak_ref(reinterpret_cast<wo_weak_ref>(p));
        });
}

WO_API wo_api rslib_std_weakref_trylock(wo_vm vm, wo_value args)
{
    wo_value result = wo_reserve_stack(vm, 1, &args);

    auto wref = reinterpret_cast<wo_weak_ref>(wo_pointer(args + 0));
    if (WO_TRUE == wo_lock_weak_ref(result, wref))
        return wo_ret_option_val(vm, result);

    return wo_ret_option_none(vm);
}

WO_API wo_api rslib_std_env_pin_create(wo_vm vm, wo_value args)
{
    auto new_shared_env = new wo::shared_pointer<wo::runtime_env>(
        reinterpret_cast<wo::vmbase*>(vm)->env);

    return wo_ret_gcstruct(
        vm,
        new_shared_env,
        [](wo_gc_work_context_t ctx, wo_ptr_t p)
        {
            auto* env_p = reinterpret_cast<wo::shared_pointer<wo::runtime_env>*>(p)->get();
            auto* global_and_const_values = env_p->constant_and_global_storage;

            // Env's constant & global values might missing mark if other vm instance
            // has been closed.
            // So we need to mark them again here.
            for (size_t cgr_index = env_p->constant_value_count;
                cgr_index < env_p->constant_and_global_value_takeplace_count;
                cgr_index++)
            {
                auto* global_val = global_and_const_values + cgr_index;
                wo_gc_mark(ctx, std::launder(reinterpret_cast<wo_value>(global_val)));
            }
        },
        [](wo_ptr_t p)
        {
            auto* shared_env_ptr =
                reinterpret_cast<wo::shared_pointer<wo::runtime_env>*>(p);

            delete shared_env_ptr;
        });
}

WO_API wo_api rslib_std_dynamic_deserialize(wo_vm vm, wo_value args)
{
    wo_value result = wo_reserve_stack(vm, 1, &args);
    if (wo_deserialize(result, wo_string(args + 0), WO_INVALID_TYPE))
        return wo_ret_option_val(vm, result);
    return wo_ret_option_none(vm);
}

#if defined(__APPLE__) && defined(__MACH__)
#   include <TargetConditionals.h>
#endif

const char* wo_stdlib_src_path = u8"woo/std.wo";
const char* wo_stdlib_src_data = {
u8R"(// Woolang standard library.
namespace std
{
    namespace platform
    {
        public enum os_type
        {
            WIN32,
            ANDROID,
            LINUX,
            IOS,
            MACOS,
            UNKNOWN,
        }
        public enum arch_type
        {
            X86,
            AMD64,
            ARM32,
            ARM64,
            UNKNOWN32,
            UNKNOWN64,
        }
        public enum runtime_type
        {
            DEBUG,
            RELEASE,
        }
)"
//////////////////////////////////////////////////////////////////////
"        public let os = "
#if defined(_WIN32)
    "os_type::WIN32;\n"
#elif defined(__ANDROID__)
    "os_type::ANDROID;\n"
#elif defined(__linux__)
    "os_type::LINUX;\n"
#elif defined(__APPLE__) && defined(__MACH__)
#   if TARGET_IPHONE_SIMULATOR || TARGET_OS_IPHONE
    "os_type::IOS;\n"
#   elif TARGET_OS_MAC
    "os_type::MACOS;\n"
#   else
    "os_type::UNKNOWN;\n"
#   endif
#else
    "os_type::UNKNOWN;\n"
#endif
    //////////////////////////////////////////////////////////////////////
    "        public let arch = "
    #if defined(_X86_)||defined(__i386) || defined(_M_IX86)
        "arch_type::X86;\n"
    #elif defined(__x86_64) || defined(_M_AMD64)
        "arch_type::AMD64;\n"
    #elif defined(__arm) || defined(_M_ARM)
        "arch_type::ARM32;\n"
    #elif defined(__aarch64__) || defined(_M_ARM64)
        "arch_type::ARM64;\n"
    #else
    #   if defined(WO_PLATFORM_32)
        "arch_type::UNKNOWN32;\n"
    #   elif defined(WO_PLATFORM_64)
        "arch_type::UNKNOWN64;\n"
    #   else
    #       error "Unsupported cpu archtype."
    #   endif
    #endif
    //////////////////////////////////////////////////////////////////////
    "        public let bitwidth = "
    #if defined(WO_PLATFORM_32)
        "32;\n"
    #elif defined(WO_PLATFORM_64)
        "64;\n"
    #else
    #    error "Unsupported cpu archtype."
    #endif
    //////////////////////////////////////////////////////////////////////
    "        public let runtime = "
    #if defined(NDEBUG)
        "runtime_type::RELEASE;\n"
    #else
        "runtime_type::DEBUG;\n"
    #endif
    //////////////////////////////////////////////////////////////////////
    "        public let version = "
    #define WO_VERSION_WRAP(a, b, c, d) "(" #a ", " #b ", " #c ", " #d ")"
        WO_VERSION
    ";\n"
    //////////////////////////////////////////////////////////////////////
    u8R"(
        extern("rslib_std_get_env")
            public func env(name: string)=> option<string>;
    }
}
namespace unsafe
{
    extern("rslib_std_return_itself") 
        public func cast<T, FromT>(val: FromT)=> T;
    extern("rslib_std_get_extern_symb")
        public func extern_symbol<T>(fullname: string)=> option<T>;
    extern("rslib_std_replace_tp")
        public func swap_argc(argc: int)=> int;
}
namespace std
{
    extern("rslib_std_panic")
        public func panic(msg: string)=> nothing;
    public using bad_t<Msg: string> = nothing;
    public let bad<Msg: string> = panic(Msg): bad_t<{Msg}>;
    extern("rslib_std_bad_function")
        public func declval<T>()=> T;
    namespace type_traits
    {
        public alias invoke_result_t<F, ArgTs> = typeof(declval:<F>()(declval:<ArgTs>()...));
        public alias iterator_result_t<T> = option::item_t<typeof(declval:<T>()->next)>;
        public let is_array<AT> = typeid(declval:<array::item_t<AT>>()) != 0;
        public let is_vec<AT> = typeid(declval:<vec::item_t<AT>>()) != 0;
        public let is_dict<DT> = typeid(declval:<dict::item_t<DT>>()) != 0;
        public let is_map<DT> = typeid(declval:<map::item_t<DT>>()) != 0;
        public let is_tuple<T> = 
            typeid((declval:<T>()...,)) != 0 && is_same:<T, typeof((declval:<T>()...,))>;
        public let is_same<A, B> = typeid:<A> == typeid:<B> && typeid:<A> != 0;
        public let is_mutable<A> = is_same:<A, mut A>;
        public let is_invocable<F, ArgTs> = typeid(declval:<F>()(declval:<ArgTs>()...)) != 0;
        public let is_function<F> = is_invocable:<F, array<nothing>>;
        public let is_iterator<T> = typeid(declval:<iterator_result_t<T>>()) != 0;
        public let is_iterable<T> = 
            typeid(declval:<T>()->iter) != 0 && is_iterator:<typeof(declval:<T>()->iter)>;
        public let is_option<T> = typeid:<option::item_t<T>> != 0;
        public let is_result<T> = typeid:<result::ok_t<T>> != 0;
        public alias expected_t<T> = typeof(
            is_option:<T> 
                ? declval:<option::item_t<T>>() 
                | is_result:<T> 
                    ? declval:<result::ok_t<T>>()
                    | bad:<{"T should be a option or result."}>());
    }
    public func use<T, R>(val: T, f: (T)=> R)
        where typeid(val->close) != 0;
    {
        let v = f(val);
        do val->close;
        return v;
    }
    public func iterator<T>(v: T)
        where type_traits::is_iterator:<T> || type_traits::is_iterable:<T>;
    {
        if (type_traits::is_iterator:<T>)
            return v;
        else
            return v->iter;
    }
    public using weakref<T> = gchandle
    {
        extern("rslib_std_weakref_create")
            public func create<T>(val: T)=> weakref<T>;
        extern("rslib_std_weakref_trylock")
            public func get<T>(self: weakref<T>)=> option<T>;
        public func close<T>(self: weakref<T>)=> bool
        {
            return self: gchandle->close;
        }
    }
    using far<FT> = FT
    {
        using env_pin = gchandle
        {   
            extern("rslib_std_env_pin_create")
                func create()=> env_pin;
        }
        let decltuple<N: int> = 
            N <= 0 
                ? std::declval:<()>
                | std::declval:<typeof((std::declval:<nothing>(), decltuple:<{N - 1}>()...))>
                ;
        let declargc_counter<FT, N: int> = 
            typeid(std::declval:<FT>()(decltuple:<{N}>()...)) != 0
                ? N
                | declargc_counter:<FT, {N + 1}>
                ;
        let declargc<FT> = declargc_counter:<FT, {0}>;
        let declisvariadic<FT> = 
            typeid(std::declval:<FT>()(decltuple:<{declargc:<FT> + 1}>()...)) != 0;
        public func wrap<FT>(f: FT)=> far<FT>
            where type_traits::is_function:<FT>;
        {
            let p = env_pin::create();
            return 
                func(...)
                {
                    do p; // Capture the env pin.
                    if (!declisvariadic:<FT>)
                        do unsafe::swap_argc(declargc:<FT>);
                    return f(...->unsafe::cast:<array<nothing>>...);
                }->unsafe::cast:<FT>: far<FT>;
        }
    }
    public using mutable<T> = struct {
        val : mut T
    }
    {
        public func create<T>(val: T)=> mutable<T>
        {
            return mutable:<T>{val = mut val};
        }
        public func set<T>(self: mutable<T>, val: T)=> void
        {
            self.val = val;
        }
        public func get<T>(self: mutable<T>)=> T
        {
            return self.val;
        }
        public func apply<T>(self: mutable<T>, f: (T)=> T)
        {
            self.val = f(self.val);
        }
    }
}
public union option<T>
{
    value(T),
    none
}
namespace option
{
    public alias item_t<T> = 
        typeof(std::declval:<T>()->\<E>_: option<E> = std::declval:<E>(););
    public func ret<T>(elem: T)=> option<T>
    {
        return option::value(elem);
    }
    public func map<T, R>(self: option<T>, functor: (T)=> R)
        => option<R>
    {
        match(self)
        {
        value(x)?
            return option::value(functor(x));
        none?
            return option::none;
        }
    }
    public func map_or<T, R>(self: option<T>, functor: (T)=> R, default: R)
        => R
    {
        match(self)
        {
        value(x)?
            return functor(x);
        none?
            return default;
        }
    }
    public func map_or_do<T, R>(self: option<T>, functor: (T)=> R, default: ()=> R)
        => R
    {
        match(self)
        {
        value(x)?
            return functor(x);
        none?
            return default();
        }
    }
    public func bind<T, R>(self: option<T>, functor: (T)=> option<R>)
        => option<R>
    {
        match(self)
        {
        value(u)? return functor(u);
        none? return option::none;
        }
    }
    public func bind_or<T, R>(self: option<T>, functor: (T)=> option<R>, default: R)
    {
        match(self)
        {
        value(u)? return functor(u)->or(default);
        none? return default;
        }
    }
    public func bind_or_do<T, R>(self: option<T>, functor: (T)=> option<R>, default: ()=> R)
    {
        match(self)
        {
        value(u)? return functor(u)->or_do(default);
        none? return default();
        }
    }
    public func or<T>(self: option<T>, default: T)
        => T
    {
        match(self)
        {
        value(x)? return x;
        none? return default;
        }
    }
    public func or_do<T>(self: option<T>, default: ()=> T)
        => T
    {
        match(self)
        {
        value(x)? return x;
        none? return default();
        }
    }
    public func or_bind<T>(self: option<T>, functor: ()=> option<T>)
        => option<T>
    {
        match(self)
        {
        value(_)? return self;
        none? return functor();
        }
    }
    public func unwrap<T>(self: option<T>)=> T
    {
        match(self)
        {
        value(x)? return x;
        none? return std::panic("`unwrap` called on a `option::none`.");
        }
    }
    public func is_value<T>(self: option<T>)=> bool
    {
        match(self)
        {
        value(_)? return true;
        none? return false;
        }
    }
    public func is_none<T>(self: option<T>)=> bool
    {
        match(self)
        {
        value(_)? return false;
        none? return true;
        }
    }
    public func ok_or<T, F>(self: option<T>, err: F)
        => result<T, F>
    {
        match(self)
        {
        value(x)? return result::ok(x);
        none? return result::err(err);
        }
    }
    public func ok_or_do<T, F>(self: option<T>, err: ()=> F)
        => result<T, F>
    {
        match(self)
        {
        value(x)? return result::ok(x);
        none? return result::err(err());
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
    public alias item_t<T> = 
        typeof(std::declval:<T>()->\<O, E>_: result<O, E> = std::declval:<(O, E)>(););
    public alias ok_t<T> = 
        typeof(std::declval:<T>()->\<O, E>_: result<O, E> = std::declval:<O>(););
    public alias err_t<T> = 
        typeof(std::declval:<T>()->\<O, E>_: result<O, E> = std::declval:<E>(););
    public func ret<T>(elem: T)=> result<T, nothing>
    {
        return result::ok(elem);
    }
    public func map<T, F, R>(self: result<T, F>, functor: (T)=> R)
        => result<R, F>
    {
        match(self)
        {
        ok(v)? return result::ok(functor(v));
        err(e)? return result::err(e);
        }
    }
    public func map_or<T, F, R>(self: result<T, F>, functor: (T)=> R, default: R)
        => R
    {
        match(self)
        {
        ok(v)? return functor(v);
        err(_)? return default;
        }
    }
    public func map_or_do<T, F, R>(self: result<T, F>, functor: (T)=> R, default: ()=> R)
        => R
    {
        match(self)
        {
        ok(v)? return functor(v);
        err(_)? return default();
        }
    }
    public func bind<T, F, R>(self: result<T, F>, functor: (T)=> result<R, F>)
        => result<R, F>
    {
        match(self)
        {
        ok(v)? return functor(v);
        err(e)? return result::err(e);
        }
    }
    public func bind_or<T, F, R>(self: result<T, F>, functor: (T)=> result<R, F>, default: R)
        => result<R, F>
    {
        match(self)
        {
        ok(v)? return functor(v)->or(default);
        err(e)? return default;
        }
    }
    public func bind_or_do<T, F, R>(self: result<T, F>, functor: (T)=> result<R, F>, default: ()=> R)
        => result<R, F>
    {
        match(self)
        {
        ok(v)? return functor(v)->or_do(default);
        err(e)? return default();
        }
    }
    public func or_map<T, F, RF>(self: result<T, F>, functor: (F)=> RF)
        => result<T, RF>
    {
        match(self)
        {
        ok(v)? return result::ok(v);
        err(e)? return result::err(functor(e));
        }
    }
    public func or_bind<T, F, RF>(self: result<T, F>, functor: (F)=> result<T, RF>)
        => result<T, RF>
    {
        match(self)
        {
        ok(v)? return result::ok(v);
        err(e)? return functor(e);
        }
    }
    public func or<T, F>(self: result<T, F>, default: T)
        => T
    {
        match(self)
        {
        ok(v)? return v;
        err(_)? return default;
        }
    }
    public func or_do<T, F>(self: result<T, F>, default: (F)=> T)
        => T
    {
        match(self)
        {
        ok(v)? return v;
        err(e)? return default(e);
        }
    }
    public func unwrap<T, F>(self: result<T, F>)
        => T
    {
        match(self)
        {
        ok(v)? return v;
        err(e)? 
            return std::panic(
                typeid(e: string) != 0
                    ? F"`unwrap` called on an `result::err`: {e}"
                    | "`unwrap` called on an `result::err`.");
        }
    }
    public func unwrap_err<T, F>(self: result<T, F>)
        => F
    {
        match(self)
        {
        ok(v)? return std::panic(
            typeid(v: string) != 0
                ? F"`unwrap_err` called on an `result::ok`: {v}"
                | "`unwrap_err` called on an `result::ok`.");
        err(e)? return e;
        }
    }
    public func is_ok<T, F>(self: result<T, F>)
        => bool
    {
        match(self)
        {
        ok(_)? return true;
        err(_)? return false;
        }
    }
    public func is_err<T, F>(self: result<T, F>)
        => bool
    {
        match(self)
        {
        ok(_)? return false;
        err(_)? return true;
        }
    }
    public func okay<T, F>(self: result<T, F>)
        => option<T>
    {
        match(self)
        {
        ok(v)? return option::value(v);
        err(_)? return option::none;
        }
    }
    public func error<T, F>(self: result<T, F>)
        => option<F>
    {
        match(self)
        {
        ok(_)? return option::none;
        err(e)? return option::value(e);
        }
    }
}
namespace std
{
    extern("rslib_std_print") 
        public func print(...)=> void;
    extern("rslib_std_time_sec") 
        public func time()=> real;
    public func println(...)=> void
    {
        print((...)...);
        print("\n");
    }
    public func input<T>(validator: (T)=>bool)=> T
        where declval:<T>() is int
            || declval:<T>() is real
            || declval:<T>() is string;
    {
        while (true)
        {
            if (declval:<T>() is int)
            {
                extern("rslib_std_input_readint") 
                    func input_int()=> int;
                let result = input_int();
                if (validator(result))
                    return result;
            }
            else if (declval:<T>() is real)
            {
                extern("rslib_std_input_readreal") 
                    func input_real()=> real;
                let result = input_real();
                if (validator(result))
                    return result;
            }
            else
            {
                extern("rslib_std_input_readstring") 
                    func input_string()=> string;
                let result = input_string();
                if (validator(result))
                    return result;
            }
        }
    }
)" R"(
    public func input_line<T>(parser: (string)=>option<T>)=> T
    {
        while (true)
        {
            extern("rslib_std_input_readline") 
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
    public func rand<T>(from: T, to: T)=> T
        where from is int || from is real;
    {
        if (from is int)
        {
            extern("rslib_std_randomint") 
            func rand_int(from: int, to: int)=> int;
            return rand_int(from, to);
        }
        else
        {
            extern("rslib_std_randomreal") 
            func rand_real(from: real, to: real)=> real;
            return rand_real(from, to);
        }
    }
    extern("rslib_std_break_yield") 
        public func yield()=> void;
    extern("rslib_std_thread_sleep")
        public func sleep(tm: real)=> void;
    extern("rslib_std_get_args")
        public func args()=> array<string>;
    extern("rslib_std_get_exe_path")
        public func host_path()=> string;
    extern("rslib_std_equal_byte")
        public func is_same<LT, RT>(a: LT, b: RT)=> bool;
    extern("rslib_std_make_dup", repeat)
        public func dup<T>(dupval: T)=> T;
}
namespace dynamic
{
    extern("rslib_std_serialize", repeat)
        public func serialize(self: dynamic)=> option<string>;
    extern("rslib_std_dynamic_deserialize")
        public func deserialize(datstr: string)=> option<dynamic>;
}
public using cchar = char;
namespace char
{
    extern("rslib_std_char_tostring")
        public func to_string(val: char)=> string;
    extern("rslib_std_char_toupper")
        public func upper(val: char)=> char;
    extern("rslib_std_char_tolower")
        public func lower(val: char)=> char;
    extern("rslib_std_char_isspace")
        public func is_space(val: char)=> bool;
    extern("rslib_std_char_isalpha")
        public func is_alpha(val: char)=> bool;
    extern("rslib_std_char_isalnum")
        public func is_alnum(val: char)=> bool;
    extern("rslib_std_char_isnumber")
        public func is_number(val: char)=> bool;
    extern("rslib_std_char_ishex")
        public func is_hex(val: char)=> bool;
    extern("rslib_std_char_isoct")
        public func is_oct(val: char)=> bool;
    extern("rslib_std_char_hexnum")
        public func hex_int(val: char)=> int;
    extern("rslib_std_char_octnum")
        public func oct_int(val: char)=> int;
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
    public func get_cchar(val: string, index: int)=> cchar;
    extern("rslib_std_str_char_len") 
    public func len(val: string)=> int;
    extern("rslib_std_str_byte_len") 
    public func byte_len(val: string)=> int;
    extern("rslib_std_string_sub")
    public func sub(val: string, begin: int)=> string;
    extern("rslib_std_string_sub_len")
    public func sub_len(val: string, begin: int, length: int)=> string;
    extern("rslib_std_string_sub_range")
    public func sub_range(val: string, begin: int, end: int)=> string;
    extern("rslib_std_string_toupper")
        public func upper(val: string)=> string;
    extern("rslib_std_string_tolower")
        public func lower(val: string)=> string;
    extern("rslib_std_string_isspace")
        public func is_space(val: string)=> bool;
    extern("rslib_std_string_isalpha")
        public func is_alpha(val: string)=>  bool;
    extern("rslib_std_string_isalnum")
        public func is_alnum(val: string)=>  bool;
    extern("rslib_std_string_isnumber")
        public func is_number(val: string)=>  bool;
    extern("rslib_std_string_ishex")
        public func is_hex(val: string)=>  bool;
    extern("rslib_std_string_isoct")
        public func is_oct(val: string)=>  bool;
    extern("rslib_std_string_enstring")
        public func enstring(val: string)=> string;
    extern("rslib_std_string_destring")
        public func destring(val: string)=>  string;
    extern("rslib_std_string_beginwith")
        public func begin_with(val: string, str: string)=> bool;
    extern("rslib_std_string_endwith")
        public func end_with(val: string, str: string)=> bool;
    extern("rslib_std_string_replace")
        public func replace(val: string, match_aim: string, str: string)=> string;
    extern("rslib_std_string_find")
        public func find(val: string, match_aim: string)=> option<int>;
    extern("rslib_std_string_find_from")
        public func find_from(val: string, match_aim: string, from: int)=> option<int>;
    extern("rslib_std_string_rfind")
        public func rfind(val: string, match_aim: string)=> option<int>;
    extern("rslib_std_string_rfind_from")
        public func rfind_from(val: string, match_aim: string, from: int)=> option<int>;
    extern("rslib_std_string_trim")
        public func trim(val: string)=> string;
    extern("rslib_std_string_split")
        public func split(val: string, sep: string)=> spliter_t;
    using spliter_t = gchandle
    {
        extern("rslib_std_string_split_iter")
        public func next(self: spliter_t)=> option<string>;
    }
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
namespace array
{
    public alias item_t<AT> = 
        typeof(std::declval:<AT>()->\<T>_: array<T> = std::declval:<T>(););
    extern("rslib_std_array_create", repeat) 
        public func create<T>(sz: int, init_val: T)=> array<T>;
    extern("rslib_std_serialize", repeat) 
        public func serialize<T>(self: array<T>)=> option<string>;
    extern("rslib_std_parse_array_from_string", repeat)
        public func deserialize(val: string)=> option<array<dynamic>>;
    public func append<T>(self: array<T>, elem: T)=> array<T>
    {
        let newarr = self->to_vec;
        newarr->add(elem);
        return newarr->unsafe::cast:<array<T>>;
    }
    public func erase<T>(self: array<T>, index: int)=> array<T>
    {
        let newarr = self->to_vec;
        do newarr->remove(index);
        return newarr->unsafe::cast:<array<T>>;
    }
    public func inlay<T>(self: array<T>, index: int, insert_value: T)=> array<T>
    {
        let newarr = self->to_vec;
        newarr->insert(index, insert_value);
        return newarr->unsafe::cast:<array<T>>;
    }
    extern("rslib_std_create_str_by_wchar", repeat) 
        public func str(buf: array<char>)=> string;
    extern("rslib_std_create_str_by_ascii", repeat) 
        public func cstr(buf: array<cchar>)=> string;
    extern("rslib_std_array_len", repeat) 
        public func len<T>(val: array<T>)=> int;
    extern("rslib_std_make_dup", repeat)
        public func dup<T>(val: array<T>)=> array<T>;
    extern("rslib_std_make_dup", repeat)
        public func to_vec<T>(val: array<T>)=> vec<T>;
    extern("rslib_std_array_empty", repeat)
        public func empty<T>(val: array<T>)=> bool;
    public func resize<T>(val: array<T>, newsz: int, init_val: T)=> array<T>
    {
        let newarr = val->to_vec;
        newarr->resize(newsz, init_val);
        return newarr as vec<T>->unsafe::cast:<array<T>>;
    }
    public func shrink<T>(val: array<T>, newsz: int)=> array<T>
    {
        let newarr = val->to_vec;
        do newarr->shrink(newsz);
        return newarr as vec<T>->unsafe::cast:<array<T>>;
    }
    extern("rslib_std_array_get", repeat)
        public func get<T>(a: array<T>, index: int)=> option<T>;
    extern("rslib_std_array_get_or_default", repeat)
        public func get_or<T>(a: array<T>, index: int, val: T)=> T;
    public func get_or_do<T>(a: array<T>, index: int, f: ()=> T)=> T
    {
        match (a->get(index))
        {
        value(v)? return v;
        none? return f();
        }
    }
    extern("rslib_std_array_find", repeat)
        public func find<T>(val: array<T>, elem: T)=> option<int>;
    public func find_if<T>(val: array<T>, judger:(T)=> bool)=> option<int>
    {
        let mut count = 0;
        for (let v : val)
            if (judger(v))
                return option::value(count);
            else
                count += 1;
        return option::none;
    }
    public func forall<T>(val: array<T>, functor: (T)=> bool)=> array<T>
    {
        let result = []mut: vec<T>;
        for (let elem : val)
            if (functor(elem))
                result->add(elem);
        return result->unsafe::cast:<array<T>>;
    }
    public func bind<T, R>(val: array<T>, functor: (T)=> array<R>)=> array<R>
    {
        let result = []mut: vec<R>;
        for (let elem : val)
            for (let insert : functor(elem))
                result->add(insert);
        return result->unsafe::cast:<array<R>>;
    }
    public func map<T, R>(val: array<T>, functor: (T)=> R)=> array<R>
    {
        let result = []mut: vec<R>;
        for (let elem : val)
        {
            result->add(functor(elem));
        }
        return result->unsafe::cast:<array<R>>;
    }
    public func mapping<K, V>(val: array<(K, V)>)=> dict<K, V>
    {
        let result = {}mut: map<K, V>;
        for (let (k, v) : val)
            result->set(k, v);
        return result->unsafe::cast:<dict<K, V>>;
    }
    public func reduce<T>(self: array<T>, reducer: (T, T)=> T)=> option<T>
    {
        if (self->empty)
            return option::none;
        let mut result = self[0];
        for (let mut i = 1; i < self->len; i+=1)
            result = reducer(result, self[i]);
        return option::value(result);
    }
    public func rreduce<T>(self: array<T>, reducer: (T, T)=> T)=> option<T>
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
            public func next<T>(iter: iterator<T>)=> option<T>;
    }
    extern("rslib_std_array_iter", repeat)
        public func iter<T>(val: array<T>)=> iterator<T>;
    extern("rslib_std_array_connect", repeat)
        public func connect<T>(self: array<T>, another: array<T>)=> array<T>;
    extern("rslib_std_array_sub", repeat)
    public func sub<T>(self: array<T>, begin: int)=> array<T>;
    extern("rslib_std_array_subto", repeat)
    public func sub_len<T>(self: array<T>, begin: int, count: int)=> array<T>;
    extern("rslib_std_array_sub_range", repeat)
    public func sub_range<T>(self: array<T>, begin: int, end: int)=> array<T>;
    extern("rslib_std_array_front", repeat)
    public func front<T>(val: array<T>)=> option<T>;
    extern("rslib_std_array_back", repeat)
    public func back<T>(val: array<T>)=> option<T>;
    extern("rslib_std_array_front_val", repeat)
    public func unwrap_front<T>(val: array<T>)=> T;
    extern("rslib_std_array_back_val", repeat)
    public func unwrap_back<T>(val: array<T>)=> T;
}
)" R"(
namespace vec
{
    public alias item_t<AT> = 
        typeof(std::declval:<AT>()->\<T>_: vec<T> = std::declval:<T>(););
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
    extern("rslib_std_array_len", repeat) 
        public func len<T>(val: vec<T>)=> int;
    extern("rslib_std_make_dup", repeat)
        public func dup<T>(val: vec<T>)=> vec<T>;
    extern("rslib_std_make_dup", repeat)
        public func to_array<T>(val: vec<T>)=> array<T>;
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
        public func copy<T, CT>(val: vec<T>, another: CT)=> void
            where another is array<T> || another is vec<T>;
    extern("rslib_std_array_get", repeat)
        public func get<T>(a: vec<T>, index: int)=> option<T>;
    extern("rslib_std_array_get_or_default", repeat)
        public func get_or<T>(a: vec<T>, index: int, val: T)=> T;
    public func get_or_do<T>(a: vec<T>, index: int, f: ()=> T)=> T
    {
        match (a->get(index))
        {
        value(v)? return v;
        none? return f();
        }
    }
    extern("rslib_std_array_add") 
        public func add<T>(val: vec<T>, elem: T)=> void;
    extern("rslib_std_array_connect", repeat)
        public func connect<T>(self: vec<T>, another: vec<T>)=> vec<T>;
    extern("rslib_std_array_sub", repeat)
    public func sub<T>(self: vec<T>, begin: int)=> vec<T>;
    extern("rslib_std_array_subto", repeat)
    public func sub_len<T>(self: vec<T>, begin: int, count: int)=> vec<T>;
    extern("rslib_std_array_sub_range", repeat)
    public func sub_range<T>(self: vec<T>, begin: int, end: int)=> vec<T>;
    extern("rslib_std_array_pop") 
        public func pop<T>(val: vec<T>)=> option<T>;  
    extern("rslib_std_array_dequeue") 
        public func dequeue<T>(val: vec<T>)=> option<T>;  
    extern("rslib_std_array_pop_val") 
        public func unwrap_pop<T>(val: vec<T>)=> T;  
    extern("rslib_std_array_dequeue_val") 
        public func unwrap_dequeue<T>(val: vec<T>)=> T;  
    extern("rslib_std_array_remove")
        public func remove<T>(val: vec<T>, index: int)=> bool;
    extern("rslib_std_array_find", repeat)
        public func find<T>(val: vec<T>, elem: T)=> option<int>;
    public func find_if<T>(val: vec<T>, judger:(T)=> bool)=> option<int>
    {
        let mut count = 0;
        for (let v : val)
            if (judger(v))
                return option::value(count);
            else
                count += 1;
        return option::none;
    }
    extern("rslib_std_array_clear")
        public func clear<T>(val: vec<T>)=> void;
    public func forall<T>(val: vec<T>, functor: (T)=> bool)=> vec<T>
    {
        let result = []mut: vec<T>;
        for (let elem : val)
            if (functor(elem))
                result->add(elem);
        return result;
    }
    public func bind<T, R>(val: vec<T>, functor: (T)=>vec<R>)=> vec<R>
    {
        let result = []mut: vec<R>;
        for (let elem : val)
            for (let insert : functor(elem))
                result->add(insert);
        return result;
    }
    public func map<T, R>(val: vec<T>, functor: (T)=> R)=> vec<R>
    {
        let result = []mut: vec<R>;
        for (let elem : val)
            result->add(functor(elem));
        return result;
    }
    public func mapping<K, V>(val: vec<(K, V)>)=> dict<K, V>
    {
        let result = {}mut: map<K, V>;
        for (let (k, v) : val)
            result->set(k, v);
        return result->unsafe::cast:<dict<K, V>>;
    }
    public func reduce<T>(self: vec<T>, reducer: (T, T)=>T)=> option<T>
    {
        if (self->empty)
            return option::none;
        
        let mut result = self[0];
        for (let mut i = 1; i < self->len; i+=1)
            result = reducer(result, self[i]);
        return option::value(result);
    }
    public func rreduce<T>(self: vec<T>, reducer: (T, T)=>T)=> option<T>
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
            public func next<T>(iter: iterator<T>)=> option<T>;
    }
    extern("rslib_std_array_iter", repeat)
        public func iter<T>(val: vec<T>)=> iterator<T>;
    extern("rslib_std_array_front", repeat)
    public func front<T>(val: vec<T>)=> option<T>;
    extern("rslib_std_array_back", repeat)
    public func back<T>(val: vec<T>)=> option<T>;
    extern("rslib_std_array_front_val", repeat)
    public func unwrap_front<T>(val: vec<T>)=> T;
    extern("rslib_std_array_back_val", repeat)
    public func unwrap_back<T>(val: vec<T>)=> T;
}
namespace dict
{
    public alias item_t<DT> = 
        typeof(std::declval:<DT>()->\<KT, VT>_: dict<KT, VT> = std::declval:<(KT, VT)>(););
    public alias key_t<DT> = typeof(std::declval:<dict::item_t<DT>>().0);
    public alias val_t<DT> = typeof(std::declval:<dict::item_t<DT>>().1);
    extern("rslib_std_serialize", repeat) 
        public func serialize<KT, VT>(self: dict<KT, VT>)=> option<string>;
    extern("rslib_std_parse_map_from_string", repeat) 
        public func deserialize(val: string)=> option<dict<dynamic, dynamic>>;
    public func bind<KT, VT, RK, RV>(val: dict<KT, VT>, functor: (KT, VT)=> dict<RK, RV>)=> dict<RK, RV>
    {
        let result = {}mut: map<RK, RV>;
        for (let (k, v) : val)
            for (let (rk, rv) : functor(k, v))
                result->set(rk, rv);
        return result->unsafe::cast:<dict<RK, RV>>;
    }
    public func apply<KT, VT>(self: dict<KT, VT>, key: KT, val: VT)=> dict<KT, VT>
    {
        let newmap = self->to_map;
        newmap->set(key, val);
        return newmap->unsafe::cast:<dict<KT, VT>>;
    }
    extern("rslib_std_map_len", repeat) 
        public func len<KT, VT>(self: dict<KT, VT>)=> int;
    extern("rslib_std_make_dup", repeat)
        public func dup<KT, VT>(self: dict<KT, VT>)=> dict<KT, VT>;
    extern("rslib_std_make_dup", repeat)
        public func to_map<KT, VT>(self: dict<KT, VT>)=> map<KT, VT>;
    public func find_if<KT, VT>(self: dict<KT, VT>, judger:(KT, VT)=> bool)=> option<KT>
    {
        for (let (k, v) : self)
            if (judger(k, v))
                return option::value(k);
        return option::none;            
    }
    extern("rslib_std_map_only_get", repeat) 
        public func get<KT, VT>(self: dict<KT, VT>, index: KT)=> option<VT>;
    extern("rslib_std_map_find", repeat) 
        public func contains<KT, VT>(self: dict<KT, VT>, index: KT)=> bool;
    extern("rslib_std_map_get_or_default", repeat) 
        public func get_or<KT, VT>(self: dict<KT, VT>, index: KT, default_val: VT)=> VT;
    public func get_or_do<KT, VT>(self: dict<KT, VT>, index: KT, f: ()=> VT)=> VT
    {
        match (self->get(index))
        {
        value(v)? return v;
        none? return f();
        }
    }
    extern("rslib_std_map_keys", repeat)
        public func keys<KT, VT>(self: dict<KT, VT>)=> array<KT>;
    extern("rslib_std_map_vals", repeat)
        public func vals<KT, VT>(self: dict<KT, VT>)=> array<VT>;
    extern("rslib_std_map_empty", repeat)
        public func empty<KT, VT>(self: dict<KT, VT>)=> bool;
    public func erase<KT, VT>(self: dict<KT, VT>, index: KT)=> dict<KT, VT>
    {
        let newmap = self->to_map;
        do newmap->remove(index);
        return newmap->unsafe::cast:<dict<KT, VT>>;
    }
    public using iterator<KT, VT> = gchandle
    {
        extern("rslib_std_map_iter_next", repeat)
            public func next<KT, VT>(iter: iterator<KT, VT>)=> option<(KT, VT)>;
    }
    extern("rslib_std_map_iter", repeat)
        public func iter<KT, VT>(self: dict<KT, VT>)=> iterator<KT, VT>;
    public func forall<KT, VT>(self: dict<KT, VT>, functor: (KT, VT)=> bool)=> dict<KT, VT>
    {
        let result = {}mut: map<KT, VT>;
        for (let (key, val) : self)
            if (functor(key, val))
                result->set(key, val);
        return result->unsafe::cast:<dict<KT, VT>>;
    }
    public func map<KT, VT, AT, BT>(self: dict<KT, VT>, functor: (KT, VT)=> (AT, BT))=> dict<AT, BT>
    {
        let result = {}mut: map<AT, BT>;
        for (let (key, val) : self)
        {
            let (nk, nv) = functor(key, val);
            result->set(nk, nv);
        }
        return result->unsafe::cast:<dict<AT, BT>>;
    }
    public func unmapping<KT, VT>(self: dict<KT, VT>)=> array<(KT, VT)>
    {
        let result = []mut: vec<(KT, VT)>;
        for (let kvpair : self)
            result->add(kvpair);
        return result->unsafe::cast:<array<(KT, VT)>>;
    }
}
)" R"(
namespace map
{
    public alias item_t<DT> = 
        typeof(std::declval:<DT>()->\<KT, VT>_: map<KT, VT> = std::declval:<(KT, VT)>(););
    public alias key_t<DT> = typeof(std::declval:<map::item_t<DT>>().0);
    public alias val_t<DT> = typeof(std::declval:<map::item_t<DT>>().1);
    extern("rslib_std_serialize", repeat) 
        public func serialize<KT, VT>(self: map<KT, VT>)=> option<string>;
                    
    extern("rslib_std_parse_map_from_string", repeat) 
        public func deserialize(val: string)=> option<map<dynamic, dynamic>>;
    public func bind<KT, VT, RK, RV>(val: map<KT, VT>, functor: (KT, VT)=> map<RK, RV>)=> map<RK, RV>
    {
        let result = {}mut: map<RK, RV>;
        for (let (k, v) : val)
            for (let (rk, rv) : functor(k, v))
                result->set(rk, rv);
        return result;
    }
    extern("rslib_std_map_create")
        public func create<KT, VT>(sz: int)=> map<KT, VT>;
    extern("rslib_std_map_reserve")
        public func reserve<KT, VT>(self: map<KT, VT>, newsz: int)=> void;
    extern("rslib_std_map_set") 
        public func set<KT, VT>(self: map<KT, VT>, key: KT, val: VT)=> void;
    extern("rslib_std_map_len", repeat) 
        public func len<KT, VT>(self: map<KT, VT>)=> int;
    extern("rslib_std_make_dup", repeat)
        public func dup<KT, VT>(self: map<KT, VT>)=> map<KT, VT>;
    extern("rslib_std_make_dup", repeat)
        public func to_dict<KT, VT>(self: map<KT, VT>)=> dict<KT, VT>;
    public func find_if<KT, VT>(self: map<KT, VT>, judger:(KT, VT)=> bool)=> option<KT>
    {
        for (let (k, v) : self)
            if (judger(k, v))
                return option::value(k);
        return option::none;            
    }
    extern("rslib_std_map_find", repeat) 
        public func contains<KT, VT>(self: map<KT, VT>, index: KT)=> bool;
    extern("rslib_std_map_only_get", repeat) 
        public func get<KT, VT>(self: map<KT, VT>, index: KT)=> option<VT>;
    extern("rslib_std_map_get_or_default", repeat) 
        public func get_or<KT, VT>(self: map<KT, VT>, index: KT, default_val: VT)=> VT;
    extern("rslib_std_map_get_or_set_default") 
        public func get_or_set<KT, VT>(self: map<KT, VT>, index: KT, default_val: VT)=> VT;
    extern("rslib_std_map_get_or_set_default_do") 
        public func get_or_set_do<KT, VT>(self: map<KT, VT>, index: KT, defalt_do: ()=>VT)=> VT;
    public func get_or_do<KT, VT>(self: map<KT, VT>, index: KT, f: ()=> VT)=> VT
    {
        match (self->get(index))
        {
        value(v)? return v;
        none? return f();
        }
    }
    extern("rslib_std_map_swap") 
        public func swap<KT, VT>(val: map<KT, VT>, another: map<KT, VT>)=> void;
    extern("rslib_std_map_copy") 
        public func copy<KT, VT, CT>(val: map<KT, VT>, another: CT)=> void
            where another is dict<KT, VT> || another is map<KT, VT>;
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
            public func next<KT, VT>(iter: iterator<KT, VT>)=> option<(KT, VT)>;
    }
    extern("rslib_std_map_iter", repeat)
        public func iter<KT, VT>(self: map<KT, VT>)=> iterator<KT, VT>;
    public func forall<KT, VT>(self: map<KT, VT>, functor: (KT, VT)=>bool)=> map<KT, VT>
    {
        let result = {}mut: map<KT, VT>;
        for (let (key, val) : self)
            if (functor(key, val))
                result->set(key, val);
        return result;
    }
    public func map<KT, VT, AT, BT>(self: map<KT, VT>, functor: (KT, VT)=>(AT, BT))=> map<AT, BT>
    {
        let result = {}mut: map<AT, BT>;
        for (let (key, val) : self)
        {
            let (nk, nv) = functor(key, val);
            result->set(nk, nv);
        }
        return result->unsafe::cast:<map<AT, BT>>;
    }
    public func unmapping<KT, VT>(self: map<KT, VT>)=> array<(KT, VT)>
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
        public func to_hex(val: int)=> string;
    extern("rslib_std_int_to_oct")
        public func to_oct(val: int)=> string;
    extern("rslib_std_bit_or") 
        public func bor(a: int, b: int)=> int;
    extern("rslib_std_bit_and") 
        public func band(a: int, b: int)=> int;
    extern("rslib_std_bit_xor") 
        public func bxor(a: int, b: int)=> int;
    extern("rslib_std_bit_not") 
        public func bnot(a: int)=> int;
    extern("rslib_std_bit_shl") 
        public func bshl(a: int, b: int)=> int;
    extern("rslib_std_bit_shr") 
        public func bshr(a: int, b: int)=> int;
    extern("rslib_std_bit_ashr") 
        public func bashr(a: int, b: int)=> int;
}
namespace gchandle
{
    extern("rslib_std_gchandle_close")
        public func close(handle: gchandle)=> bool;
}
namespace tuple
{
    alias _nth_t<T, Idx: int> = typeof(std::declval:<T>()[Idx]);
    let _length_counter<T, Len: int> = 
        typeid:<_nth_t:<T, {Len}>> != 0 
            ? _length_counter:<T, {Len + 1}>
            | Len;
    public let length<T> =
        std::type_traits::is_tuple:<T>
            ? _length_counter:<T, {0}>
            | std::bad:<{"T is not a tuple"}>();
    public alias nth_t<T, N: int> = typeof(
        length:<T> > N && N >= 0
            ? std::declval:<_nth_t:<T, {N}>>()
            | std::bad:<{F"tuple index: {N} out of bounds: {length:<T>}"}>()); 
    public alias car_t<T> = nth_t<T, {0}>;
    public alias nthcdr_t<T, N: int> = typeof(
        length:<T> > N && N >= 0
            ? (std::declval:<nth_t:<T, {N}>>(), std::declval:<nthcdr_t<T, {N + 1}>>()...)
            | length:<T> == N
                ? ()
                | std::bad:<{F"tuple index: {N} out of bounds: {length:<T>}"}>());
    public alias cdr_t<T> = nthcdr_t:<T, {1}>;
    public let is_empty<T> = length:<T> == 0;
    public let is_lat<T> = is_empty:<T> 
        ? true
        | std::type_traits::is_tuple:<car_t<T>>
            ? false
            | is_lat:<cdr_t<T>>;
    public func car<T>(t: T)
        where length:<T> > 0;
    {
        return t[0];
    }
    public func nth<Idx: int, T>(t: T)
        where length:<T> > Idx && Idx >= 0;
    {
        return t[Idx];
    }
    public func nthcdr<Idx: int, T>(t: T)
        where length:<T> >= Idx && Idx >= 0;
    {
        extern("rslib_std_tuple_nthcdr")
            func _nthcdr<T>(t: T, idx: int)=> nthcdr_t<T, {Idx}>;
        return _nthcdr(t, Idx);
    }
    public func cdr<T>(t: T)
        where length:<T> > 0;
    {
        extern("rslib_std_tuple_cdr")
            func _cdr<T>(t: T)=> cdr_t<T>;
        return _cdr(t);
    }
}
public func assert(val: bool)
{
    if (!val)
        std::panic("Assert failed.");
}
public func assert_message(val: bool, msg: string)
{
    if (!val)
        std::panic(F"Assert failed: {msg}");
}
)" };

WO_API wo_api rslib_std_debug_attach_default_debuggee(wo_vm vm, wo_value args)
{
    auto leaved = wo_leave_gcguard(vm);
    {
        wo_attach_default_debuggee();
    }
    if (leaved != WO_FALSE)
        wo_enter_gcguard(vm);
    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_debug_disattach_default_debuggee(wo_vm vm, wo_value args)
{
    auto leaved = wo_leave_gcguard(vm);
    {
        wo_detach_debuggee();
    }
    if (leaved != WO_FALSE)
        wo_enter_gcguard(vm);
    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_debug_callstack_trace(wo_vm vm, wo_value args)
{
    wo_value s = wo_reserve_stack(vm, 3, &args);
    wo::vmbase* vmbase = reinterpret_cast<wo::vmbase*>(vm);

    bool finished;
    auto traces = vmbase->dump_call_stack_func_info((size_t)wo_int(args + 0), true, &finished);

    wo_set_arr(s + 0, traces.size());
    size_t index = 0;
    for (auto& trace : traces)
    {
        /*
        using callstack = struct{
            public function    : string,
            public file        : string,
            public row         : int,
            public column      : int,
            public callway     : callway,
        };
        */
        wo_set_struct(s + 1, 5);
        wo_set_string(s + 2, trace.m_func_name.c_str());
        wo_struct_set(s + 1, 0, s + 2);
        wo_set_string(s + 2, trace.m_file_path.c_str());
        wo_struct_set(s + 1, 1, s + 2);
        wo_set_int(s + 2, trace.m_row);
        wo_struct_set(s + 1, 2, s + 2);
        wo_set_int(s + 2, trace.m_col);
        wo_struct_set(s + 1, 3, s + 2);
        wo_set_int(s + 2, static_cast<wo_integer_t>(trace.m_call_way));
        wo_struct_set(s + 1, 4, s + 2);

        wo_arr_set(s + 0, index++, s + 1);
    }

    wo_set_struct(s + 1, 2);
    wo_set_bool(s + 2, finished ? WO_TRUE : WO_FALSE);
    wo_struct_set(s + 1, 0, s + 2);
    wo_struct_set(s + 1, 1, s + 0);

    return wo_ret_val(vm, s + 1);
}

WO_API wo_api rslib_std_debug_breakpoint(wo_vm vm, wo_value args)
{
    auto leaved = wo_leave_gcguard(vm);
    {
        wo_break_specify_immediately(vm);
    }
    if (leaved != WO_FALSE)
        wo_enter_gcguard(vm);

    (void)wo_ret_void(vm);
    return WO_API_RESYNC_JIT_STATE_TO_VM_STATE;
}

WO_API wo_api rslib_std_debug_invoke(wo_vm vm, wo_value args)
{
    size_t argc = (size_t)wo_argc(vm);
    wo_value s = wo_reserve_stack(vm, argc - 1, &args);

    for (size_t index = 1; index < argc; ++index)
        wo_set_val(s + (index - 1), args + index);

    return wo_ret_val(vm, wo_invoke_value(
        vm, args + 0, argc - 1, &args, &s));
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
        extern("rslib_std_debug_breakpoint")
            public func breakpoint()=> void;

        extern("rslib_std_debug_attach_default_debuggee")
            public func attach_debuggee()=> void;
        extern("rslib_std_debug_disattach_default_debuggee")
            public func disattach_debuggee()=> void;

        public func breakdown()
        {
            attach_debuggee();
            breakpoint();
        }
        public enum callway
        {
            BAD,
            NEAR,
            FAR,
            NATIVE,
        }
        public using callstack = struct{
            public function    : string,
            public file        : string,
            public row         : int,
            public column      : int,
            public callway     : callway,
        }
        {
            public func to_string(self: callstack)
            {
                return F"{self.function}\n\t\t-- at {self.file}"
                    + (self.callway == callway::FAR ? " <far>" | "")
                    + (self.callway == callway::NATIVE ? "" | F" ({self.row + 1}, {self.column + 1}\x29");
            }
        }
        extern("rslib_std_debug_callstack_trace")
            public func traceback(layer: int)=> (bool, array<callstack>);
        extern("rslib_std_debug_invoke")
            public func invoke<Ft>(f: Ft, ...) => dynamic
                where typeid(f(......)) != 0;
    }
}
)" };

WO_API wo_api rslib_std_macro_lexer_error(wo_vm vm, wo_value args)
{
    wo::lexer* lex = (wo::lexer*)wo_pointer(args + 0);

    (void)lex->record_parser_error(
        wo::lexer::msglevel_t::error, "%s", wo_string(args + 1));
    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_macro_lexer_peek(wo_vm vm, wo_value args)
{
    wo::lexer* lex = (wo::lexer*)wo_pointer(args + 0);

    // ATTENTION: args might invalid after peek (
    //      if stack extension happend in recursive macro handling), 
    //  we cannot use args after peek.
    auto leaved = wo_leave_gcguard(vm);
    auto* token_instance = lex->peek(true);
    if (leaved != WO_FALSE)
        wo_enter_gcguard(vm);

    wo_value result = wo_register(vm, WO_REG_T0);
    wo_value elem = wo_register(vm, WO_REG_T1);

    wo_set_struct(result, 2);
    wo_set_int(elem, (wo_integer_t)token_instance->m_lex_type);
    wo_struct_set(result, 0, elem);
    wo_set_string(elem, token_instance->m_token_text.c_str());
    wo_struct_set(result, 1, elem);

    return wo_ret_val(vm, result);
}

WO_API wo_api rslib_std_macro_lexer_next(wo_vm vm, wo_value args)
{
    wo::lexer* lex = (wo::lexer*)wo_pointer(args + 0);

    // ATTENTION: args might invalid after peek (
    //      if stack extension happend in recursive macro handling), 
    //  we cannot use args after peek.
    auto leaved = wo_leave_gcguard(vm);
    auto* token_instance = lex->peek(true);
    if (leaved != WO_FALSE)
        wo_enter_gcguard(vm);

    wo_value result = wo_register(vm, WO_REG_T0);
    wo_value elem = wo_register(vm, WO_REG_T1);

    wo_set_struct(result, 2);
    wo_set_int(elem, (wo_integer_t)token_instance->m_lex_type);
    wo_struct_set(result, 0, elem);
    wo_set_string(elem, token_instance->m_token_text.c_str());
    wo_struct_set(result, 1, elem);

    // Use consume_forward to avoid recursive macro handler invoke.
    lex->consume_forward();

    return wo_ret_val(vm, result);
}

WO_API wo_api rslib_std_macro_lexer_current_path(wo_vm vm, wo_value args)
{
    wo::lexer* lex = (wo::lexer*)wo_pointer(args + 0);
    return wo_ret_string(vm, lex->get_source_path()->c_str());
}

WO_API wo_api rslib_std_macro_lexer_current_location(wo_vm vm, wo_value args)
{
    wo::lexer* lex = (wo::lexer*)wo_pointer(args + 0);

    // ATTENTION: args might invalid after peek (
    //      if stack extension happend in recursive macro handling), 
    //  we cannot use args after peek.
    auto leaved = wo_leave_gcguard(vm);
    auto* peek_token = lex->peek(true);
    if (leaved)
        wo_enter_gcguard(vm);

    wo_value result = wo_register(vm, WO_REG_T0);
    wo_value elem = wo_register(vm, WO_REG_T1);

    wo_set_struct(result, 2);
    wo_set_int(elem, (wo_integer_t)peek_token->m_token_begin[2]);
    wo_struct_set(result, 0, elem);
    wo_set_int(elem, (wo_integer_t)peek_token->m_token_begin[3]);
    wo_struct_set(result, 1, elem);

    return wo_ret_val(vm, result);
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
        l_literal_real,         // 0.2  0.
        l_literal_string,       // "helloworld"
        l_literal_raw_string,   // @ raw_string @
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
        l_value_assign,         // :=
        l_value_add_assign,     // +:=
        l_value_sub_assign,     // -:= 
        l_value_mul_assign,     // *:=
        l_value_div_assign,     // /:= 
        l_value_mod_assign,     // %:= 
        l_equal,                // ==
        l_not_equal,            // !=
        l_larg_or_equal,        // >=
        l_less_or_equal,        // <=
        l_less,                 // <
        l_larg,                 // >
        l_land,                 // &&
        l_lor,                  // ||
        l_or,                   // |
        l_lnot,                 // !
        l_scopeing,             // ::
        l_template_using_begin, // ::<
        l_typecast,             // :
        l_index_point,          // .
        l_double_index_point,   // ..  may be used? hey..
        l_variadic_sign,        // ...
        l_index_begin,          // '['
        l_index_end,            // ']'
        l_direct,               // '->'
        l_inv_direct,           // '<|'
        l_function_result,      // '=>'
        l_bind_monad,           // '>>'
        l_map_monad,            // '>>'
        l_left_brackets,        // (
        l_right_brackets,       // )
        l_left_curly_braces,    // {
        l_right_curly_braces,   // }
        l_question,             // ?
        l_import,
        l_export,
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
        l_defer,
        l_macro,
        l_line_comment,
        l_block_comment,
        L_shebang_comment,
        l_unknown_token,

        lex_type_COUNT,
    }

    public using lexer = handle
    {
        extern("rslib_std_macro_lexer_error")
            public func error(lex: lexer, msg: string)=> void;

        extern("rslib_std_macro_lexer_peek")
            public func peek_token(lex: lexer)=> (token_type, string);

        extern("rslib_std_macro_lexer_next")
            public func next_token(lex: lexer)=> (token_type, string);
        
        private func wrap_token(type: token_type, str: string)
        {
            if (type == token_type::l_literal_string)
                return str->enstring;
            else if (type == token_type::l_literal_raw_string)
                return "@\"" + str + "\"@";
            else if (type == token_type::l_format_string_begin)
            {
                let enstr = str->enstring;
                return "F" 
                    + enstr->sub_len(0, enstr->len - 1)
                        ->replace("{", "\\{") 
                        ->replace("}", "\\}") 
                    + "{";
            }
            else if (type == token_type::l_format_string)
            {
                let enstr = str->enstring;
                return "}" 
                    + enstr->sub_len(1, enstr->len - 2) 
                        ->replace("{", "\\{") 
                        ->replace("}", "\\}") 
                    + "{";
            }
            else if (type == token_type::l_format_string_end)
            {
                let enstr = str->enstring;
                return "}" 
                    + enstr->sub_len(1, enstr->len - 1)
                        ->replace("{", "\\{") 
                        ->replace("}", "\\}") ;
            }
            else if (type == token_type::l_literal_char)
            {
                let enstr = str->enstring;
                return F"'{enstr->sub_len(1, enstr->len - 2)}'";
            }
            else if (type == token_type::l_macro)
                return str + "!";

            return str;
        }

        public func peek(lex: lexer)
        {
            return wrap_token(lex->peek_token...);
        }
        public func next(lex: lexer)
        {
            return wrap_token(lex->next_token...);
        }

        extern("rslib_std_macro_lexer_current_path")
            public func path(lex: lexer) => string;

        extern("rslib_std_macro_lexer_current_location")
            public func location(lex: lexer) => (int, int);

        public func try_token(self: lexer, token: token_type)=> option<string>
        {
            let (tok, _) = self->peek_token;
            if (token == tok)
                return option::value(self->next_token.1);
            return option::none;
        }
        public func expect_token(self: lexer, token: token_type)=> option<string>
        {
            let (tok, res) = self->next_token();
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

WO_API wo_api rslib_std_shell(wo_vm vm, wo_value args)
{
    if (wo::config::ENABLE_SHELL_PACKAGE)
    {
        auto leaved = wo_leave_gcguard(vm);
        int system_result = system(wo_string(args + 0));
        if (leaved != WO_FALSE)
            wo_enter_gcguard(vm);
        return wo_ret_int(vm, (wo_int_t)system_result);
    }
    else
        return wo_ret_panic(vm, "Function defined in 'std/shell.wo' has been forbidden, "
            "trying to restart without '--enable-shell 1'.");
}

const char* wo_stdlib_shell_src_path = u8"woo/shell.wo";
const char* wo_stdlib_shell_src_data = {
u8R"(
import woo::std;
namespace std
{
    extern("rslib_std_shell")
        public func shell(cmd: string)=> int;
}
)" };


namespace wo
{
    void rslib_extern_symbols::init_wo_lib()
    {
        wo_assert(_current_wo_lib_handle == nullptr);

        wo_extern_lib_func_t funcs[] = {
            {"rslib_std_array_add", (void*)&rslib_std_array_add},
            {"rslib_std_array_back", (void*)&rslib_std_array_back},
            {"rslib_std_array_back_val", (void*)&rslib_std_array_back_val},
            {"rslib_std_array_clear", (void*)&rslib_std_array_clear},
            {"rslib_std_array_connect", (void*)&rslib_std_array_connect},
            {"rslib_std_array_copy", (void*)&rslib_std_array_copy},
            {"rslib_std_array_create", (void*)&rslib_std_array_create},
            {"rslib_std_array_dequeue", (void*)&rslib_std_array_dequeue},
            {"rslib_std_array_dequeue_val", (void*)&rslib_std_array_dequeue_val},
            {"rslib_std_array_empty", (void*)&rslib_std_array_empty},
            {"rslib_std_array_find", (void*)&rslib_std_array_find},
            {"rslib_std_array_front", (void*)&rslib_std_array_front},
            {"rslib_std_array_front_val", (void*)&rslib_std_array_front_val},
            {"rslib_std_array_get", (void*)&rslib_std_array_get},
            {"rslib_std_array_get_or_default", (void*)&rslib_std_array_get_or_default},
            {"rslib_std_array_insert", (void*)&rslib_std_array_insert},
            {"rslib_std_array_iter", (void*)&rslib_std_array_iter},
            {"rslib_std_array_iter_next", (void*)&rslib_std_array_iter_next},
            {"rslib_std_array_len", (void*)&rslib_std_array_len},
            {"rslib_std_array_pop", (void*)&rslib_std_array_pop},
            {"rslib_std_array_pop_val", (void*)&rslib_std_array_pop_val},
            {"rslib_std_array_remove", (void*)&rslib_std_array_remove},
            {"rslib_std_array_resize", (void*)&rslib_std_array_resize},
            {"rslib_std_array_shrink", (void*)&rslib_std_array_shrink},
            {"rslib_std_array_sub", (void*)&rslib_std_array_sub},
            {"rslib_std_array_sub_range", (void*)&rslib_std_array_sub_range},
            {"rslib_std_array_subto", (void*)&rslib_std_array_subto},
            {"rslib_std_array_swap", (void*)&rslib_std_array_swap},
            {"rslib_std_bad_function", (void*)&rslib_std_bad_function},
            {"rslib_std_bit_and", (void*)&rslib_std_bit_and},
            {"rslib_std_bit_ashr", (void*)&rslib_std_bit_ashr},
            {"rslib_std_bit_not", (void*)&rslib_std_bit_not},
            {"rslib_std_bit_or", (void*)&rslib_std_bit_or},
            {"rslib_std_bit_shl", (void*)&rslib_std_bit_shl},
            {"rslib_std_bit_shr", (void*)&rslib_std_bit_shr},
            {"rslib_std_bit_xor", (void*)&rslib_std_bit_xor},
            {"rslib_std_break_yield", (void*)&rslib_std_break_yield},
            {"rslib_std_char_hexnum", (void*)&rslib_std_char_hexnum},
            {"rslib_std_char_isalnum", (void*)&rslib_std_char_isalnum},
            {"rslib_std_char_isalpha", (void*)&rslib_std_char_isalpha},
            {"rslib_std_char_ishex", (void*)&rslib_std_char_ishex},
            {"rslib_std_char_isnumber", (void*)&rslib_std_char_isnumber},
            {"rslib_std_char_isoct", (void*)&rslib_std_char_isoct},
            {"rslib_std_char_isspace", (void*)&rslib_std_char_isspace},
            {"rslib_std_char_octnum", (void*)&rslib_std_char_octnum},
            {"rslib_std_char_tolower", (void*)&rslib_std_char_tolower},
            {"rslib_std_char_tostring", (void*)&rslib_std_char_tostring},
            {"rslib_std_char_toupper", (void*)&rslib_std_char_toupper},
            {"rslib_std_create_chars_from_str", (void*)&rslib_std_create_chars_from_str},
            {"rslib_std_create_str_by_ascii", (void*)&rslib_std_create_str_by_ascii},
            {"rslib_std_create_str_by_wchar", (void*)&rslib_std_create_str_by_wchar},
            {"rslib_std_create_wchars_from_str", (void*)&rslib_std_create_wchars_from_str},
            {"rslib_std_debug_attach_default_debuggee", (void*)&rslib_std_debug_attach_default_debuggee},
            {"rslib_std_debug_breakpoint", (void*)&rslib_std_debug_breakpoint},
            {"rslib_std_debug_callstack_trace", (void*)&rslib_std_debug_callstack_trace},
            {"rslib_std_debug_disattach_default_debuggee", (void*)&rslib_std_debug_disattach_default_debuggee},
            {"rslib_std_debug_invoke", (void*)&rslib_std_debug_invoke},
            {"rslib_std_dynamic_deserialize", (void*)&rslib_std_dynamic_deserialize},
            {"rslib_std_env_pin_create", (void*)&rslib_std_env_pin_create},
            {"rslib_std_equal_byte", (void*)&rslib_std_equal_byte},
            {"rslib_std_gchandle_close", (void*)&rslib_std_gchandle_close},
            {"rslib_std_get_args", (void*)&rslib_std_get_args},
            {"rslib_std_get_ascii_val_from_str", (void*)&rslib_std_get_ascii_val_from_str},
            {"rslib_std_get_env", (void*)&rslib_std_get_env},
            {"rslib_std_get_exe_path", (void*)&rslib_std_get_exe_path},
            {"rslib_std_get_extern_symb", (void*)&rslib_std_get_extern_symb},
            {"rslib_std_input_readint", (void*)&rslib_std_input_readint},
            {"rslib_std_input_readline", (void*)&rslib_std_input_readline},
            {"rslib_std_input_readreal", (void*)&rslib_std_input_readreal},
            {"rslib_std_input_readstring", (void*)&rslib_std_input_readstring},
            {"rslib_std_int_to_hex", (void*)&rslib_std_int_to_hex},
            {"rslib_std_int_to_oct", (void*)&rslib_std_int_to_oct},
            {"rslib_std_macro_lexer_current_location", (void*)&rslib_std_macro_lexer_current_location},
            {"rslib_std_macro_lexer_current_path", (void*)&rslib_std_macro_lexer_current_path},
            {"rslib_std_macro_lexer_error", (void*)&rslib_std_macro_lexer_error},
            {"rslib_std_macro_lexer_next", (void*)&rslib_std_macro_lexer_next},
            {"rslib_std_macro_lexer_peek", (void*)&rslib_std_macro_lexer_peek},
            {"rslib_std_make_dup", (void*)&rslib_std_make_dup},
            {"rslib_std_map_clear", (void*)&rslib_std_map_clear},
            {"rslib_std_map_copy", (void*)&rslib_std_map_copy},
            {"rslib_std_map_create", (void*)&rslib_std_map_create},
            {"rslib_std_map_empty", (void*)&rslib_std_map_empty},
            {"rslib_std_map_find", (void*)&rslib_std_map_find},
            {"rslib_std_map_get_or_default", (void*)&rslib_std_map_get_or_default},
            {"rslib_std_map_get_or_set_default", (void*)&rslib_std_map_get_or_set_default},
            {"rslib_std_map_get_or_set_default_do", (void*)&rslib_std_map_get_or_set_default_do},
            {"rslib_std_map_iter", (void*)&rslib_std_map_iter},
            {"rslib_std_map_iter_next", (void*)&rslib_std_map_iter_next},
            {"rslib_std_map_keys", (void*)&rslib_std_map_keys},
            {"rslib_std_map_len", (void*)&rslib_std_map_len},
            {"rslib_std_map_only_get", (void*)&rslib_std_map_only_get},
            {"rslib_std_map_remove", (void*)&rslib_std_map_remove},
            {"rslib_std_map_reserve", (void*)&rslib_std_map_reserve},
            {"rslib_std_map_set", (void*)&rslib_std_map_set},
            {"rslib_std_map_swap", (void*)&rslib_std_map_swap},
            {"rslib_std_map_vals", (void*)&rslib_std_map_vals},
            {"rslib_std_panic", (void*)&rslib_std_panic},
            {"rslib_std_parse_array_from_string", (void*)&rslib_std_parse_array_from_string},
            {"rslib_std_parse_map_from_string", (void*)&rslib_std_parse_map_from_string},
            {"rslib_std_print", (void*)&rslib_std_print},
            {"rslib_std_randomint", (void*)&rslib_std_randomint},
            {"rslib_std_randomreal", (void*)&rslib_std_randomreal},
            {"rslib_std_replace_tp", (void*)rslib_std_replace_tp},
            {"rslib_std_return_itself", (void*)&rslib_std_return_itself},
            {"rslib_std_serialize", (void*)&rslib_std_serialize},
            {"rslib_std_shell", (void*)&rslib_std_shell},
            {"rslib_std_str_byte_len", (void*)&rslib_std_str_byte_len},
            {"rslib_std_str_char_len", (void*)&rslib_std_str_char_len},
            {"rslib_std_string_append_cchar", (void*)&rslib_std_string_append_cchar},
            {"rslib_std_string_append_char", (void*)&rslib_std_string_append_char},
            {"rslib_std_string_beginwith", (void*)&rslib_std_string_beginwith},
            {"rslib_std_string_destring", (void*)&rslib_std_string_destring},
            {"rslib_std_string_endwith", (void*)&rslib_std_string_endwith},
            {"rslib_std_string_enstring", (void*)&rslib_std_string_enstring},
            {"rslib_std_string_find", (void*)&rslib_std_string_find},
            {"rslib_std_string_find_from", (void*)&rslib_std_string_find_from},
            {"rslib_std_string_isalnum", (void*)&rslib_std_string_isalnum},
            {"rslib_std_string_isalpha", (void*)&rslib_std_string_isalpha},
            {"rslib_std_string_ishex", (void*)&rslib_std_string_ishex},
            {"rslib_std_string_isnumber", (void*)&rslib_std_string_isnumber},
            {"rslib_std_string_isoct", (void*)&rslib_std_string_isoct},
            {"rslib_std_string_isspace", (void*)&rslib_std_string_isspace},
            {"rslib_std_string_replace", (void*)&rslib_std_string_replace},
            {"rslib_std_string_rfind", (void*)&rslib_std_string_rfind},
            {"rslib_std_string_rfind_from", (void*)&rslib_std_string_rfind_from},
            {"rslib_std_string_split", (void*)&rslib_std_string_split},
            {"rslib_std_string_split_iter", (void*)&rslib_std_string_split_iter},
            {"rslib_std_string_sub", (void*)&rslib_std_string_sub},
            {"rslib_std_string_sub_len", (void*)&rslib_std_string_sub_len},
            {"rslib_std_string_sub_range", (void*)&rslib_std_string_sub_range},
            {"rslib_std_string_tolower", (void*)&rslib_std_string_tolower},
            {"rslib_std_string_toupper", (void*)&rslib_std_string_toupper},
            {"rslib_std_string_trim", (void*)&rslib_std_string_trim},
            {"rslib_std_take_int", (void*)&rslib_std_take_int},
            {"rslib_std_take_real", (void*)&rslib_std_take_real},
            {"rslib_std_take_string", (void*)&rslib_std_take_string},
            {"rslib_std_take_token", (void*)&rslib_std_take_token},
            {"rslib_std_thread_sleep", (void*)&rslib_std_thread_sleep},
            {"rslib_std_time_sec", (void*)&rslib_std_time_sec},
            {"rslib_std_tuple_nthcdr", (void*)&rslib_std_tuple_nthcdr},
            {"rslib_std_tuple_cdr", (void*)&rslib_std_tuple_cdr},
            {"rslib_std_weakref_create", (void*)&rslib_std_weakref_create},
            {"rslib_std_weakref_trylock", (void*)&rslib_std_weakref_trylock},
            WO_EXTERN_LIB_FUNC_END
        };
        _current_wo_lib_handle = wo_fake_lib("woolang", funcs, nullptr);
    }
}