#pragma once

#define WO_IMPL
#include "wo.h"

#include <cstdint>

namespace wo
{
    constexpr size_t u8str_npos = SIZE_MAX/2;

    size_t u8strlen(wo_string_t u8str);
    size_t u8strnlen(wo_string_t u8str, size_t len);
    wo_string_t u8stridxstr(wo_string_t u8str, size_t chidx);
    wo_string_t u8strnidxstr(wo_string_t u8str, size_t chidx, size_t len);
    wchar_t u8stridx(wo_string_t u8str, size_t chidx);
    wchar_t u8strnidx(wo_string_t u8str, size_t chidx, size_t len);
    wo_string_t u8substr(wo_string_t u8str, size_t from, size_t length, size_t* out_sub_len);
    wo_string_t u8substrn(wo_string_t u8str, size_t len, size_t from, size_t length, size_t* out_sub_len);
    size_t u8blen2clen(wo_string_t u8str, size_t len);
    size_t clen2u8blen(wo_string_t u8str, size_t len);
}