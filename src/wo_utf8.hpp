#pragma once

#define WO_IMPL
#include "wo.h"

#include <cstdint>
#include <string>

namespace wo
{
    constexpr size_t UTF8MAXLEN = MB_LEN_MAX;
    constexpr size_t UTF16MAXLEN = 2;

    bool u8strnchar(wo_string_t u8str, size_t bytelen, size_t* out_charsz);
    size_t u8charnlen(const char* u8charp, size_t bytelen);
    size_t u8strnlen(wo_string_t u8str, size_t bytelen);
    
    wo_string_t u8substr(wo_string_t u8str, size_t bytelen, size_t from, size_t* out_len);
    wo_string_t u8substrr(wo_string_t u8str, size_t bytelen, size_t from, size_t tail, size_t* out_len);
    wo_string_t u8substrn(wo_string_t u8str, size_t bytelen, size_t from, size_t length, size_t* out_len);

    size_t u8combineu32(const char* u8charp, size_t bytelen, char32_t* out_c32);
    void u32exractu8(char32_t ch32, char out_c8[UTF8MAXLEN], size_t* out_u8len);
    size_t u8combineu16(const char* u8charp, size_t bytelen, char16_t out_c16[UTF16MAXLEN], size_t* out_u16len);
    size_t u16exractu8(const char16_t* u16charp, size_t charcount, char out_c8[UTF8MAXLEN], size_t* out_u8len);

    bool u16hisurrogate(char16_t ch);
    bool u16losurrogate(char16_t ch);

    std::string u8enstring(wo_string_t u8str, size_t bytelen, bool force_unicode);
    std::string u8destring(wo_string_t enu8str_zero_term);

    std::u32string u8strtou32(wo_string_t u8str, size_t bytelen);
    std::string u32strtou8(const char32_t* u32charp, size_t u32len);
    std::u16string u8strtou16(wo_string_t u8str, size_t bytelen);
    std::string u16strtou8(const char16_t* u16charp, size_t u16len);

    size_t u16strcount(const char16_t* u16str);
    size_t u32strcount(const char32_t* u32str);

    bool u32isu16(char32_t ch32);
}