#include "wo_afx.hpp"

#include <cwchar>

namespace wo
{
    static_assert(MB_LEN_MAX <= UINT8_MAX);

    size_t u8charnlen(const char* u8charp, size_t bytelen)
    {
        if (bytelen != 0)
        {
            const uint8_t u8ch = static_cast<uint8_t>(*u8charp);
            // Count the lead char with __lzcnt.

            const size_t lead_len = __lzcnt(~u8ch) - 1;

        }
        return 0;
    }
    size_t u8strnlen(wo_string_t u8str, size_t bytelen);
    bool u8strnchar(wo_string_t u8str, size_t bytelen, size_t* out_charsz);
    wo_string_t u8substr(wo_string_t u8str, size_t bytelen, size_t from, size_t* out_len);
    wo_string_t u8substrr(wo_string_t u8str, size_t bytelen, size_t from, size_t tail, size_t* out_len);
    wo_string_t u8substrn(wo_string_t u8str, size_t bytelen, size_t from, size_t length, size_t* out_len);

    size_t u8combineu32(const char* u8charp, size_t bytelen, char32_t out_c32[U8MAXLEN]);
    size_t u8combineu16(const char* u8charp, size_t bytelen, char16_t out_c16[U8MAXLEN]);
    size_t u16exractu8(const char16_t* u16charp, size_t charcount, char out_c8[U8MAXLEN]);
    size_t u32exractu8(const char32_t* u32charp, size_t charcount, char out_c8[U8MAXLEN]);

    std::string u8enstring(wo_string_t u8str, size_t bytelen);
    std::string u8destring(wo_string_t enu8str_zero_term);
}