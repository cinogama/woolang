#pragma once

#define WO_IMPL
#include "wo.h"

#include <cstdint>

namespace wo
{
    constexpr size_t u8str_npos = SIZE_MAX/2;

    size_t u8strlen(wo_string_t u8str);
    wo_string_t u8stridxstr(wo_string_t u8str, size_t chidx);
    wchar_t u8stridx(wo_string_t u8str, size_t chidx);
    wo_string_t u8substr(wo_string_t u8str, size_t from, size_t length, size_t* out_sub_len);
    size_t u8blen2clen(wo_string_t u8str, size_t len);
}