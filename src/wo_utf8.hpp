#pragma once

#define WO_IMPL
#include "wo.h"

#include <cstdint>
#include <string>

namespace wo
{
    constexpr size_t u8str_npos = SIZE_MAX/2;

    size_t u8strnlen(wo_string_t u8str, size_t len);
    wo_string_t u8strnidxstr(wo_string_t u8str, size_t len, size_t chidx);
    wo_char_t u8strnidx(wo_string_t u8str, size_t len, size_t chidx);
    wo_string_t u8substrr(wo_string_t u8str, size_t len, size_t from, size_t til, size_t* out_sub_len);
    wo_string_t u8substrn(wo_string_t u8str, size_t len, size_t from, size_t length, size_t* out_sub_len);

    size_t clen2u8blen(wo_string_t u8str, size_t len);
    size_t u8blen2clen(wo_string_t u8str, size_t len, size_t wclen);

    uint8_t u8mbc2wc(const char* ch, size_t len_not_zero, wo_char_t* out_wch);
    uint8_t u8wc2mbc(wo_char_t ch, char* out_mbchs);

    wo_wstring_t u8mbstowcs_zero_term(wo_string_t u8str);
    wo_string_t u8wcstombs_zero_term(wo_wstring_t wstr);

    wo_wstring_t u8mbstowcs(wo_string_t u8str, size_t len, size_t* out_len);
    wo_string_t u8wcstombs(wo_wstring_t wstr, size_t len, size_t* out_len);

    std::string wstr_to_str(wo_wstring_t wstr);
    std::string wstr_to_str(const std::wstring& wstr);

    std::wstring str_to_wstr(wo_string_t str);
    std::wstring str_to_wstr(const std::string& str);

    std::string wstrn_to_str(wo_wstring_t wstr, size_t len);
    std::string wstrn_to_str(const std::wstring& wstr);

    std::wstring strn_to_wstr(wo_string_t str, size_t len);
    std::wstring strn_to_wstr(const std::string& str);
}