#pragma once

#define RS_IMPL
#include "rs.h"

#include <cstdint>

namespace rs
{
    constexpr size_t u8str_npos = SIZE_MAX/2;

    size_t u8strlen(rs_string_t u8str);
    rs_string_t u8stridxstr(rs_string_t u8str, size_t chidx);
    size_t u8stridx(rs_string_t u8str, size_t chidx);
    rs_string_t u8substr(rs_string_t u8str, size_t from, size_t length, size_t* out_sub_len);
}