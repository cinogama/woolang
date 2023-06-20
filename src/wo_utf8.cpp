#include "wo_utf8.hpp"
#include <cwchar>
#include <cstring>

namespace wo
{
    uint8_t u8chsize(const char* chidx)
    {
        std::mbstate_t mb = {};

        auto strlength = strlen(chidx);

        if (std::mbrlen(chidx, 1, &mb) == (size_t)-2)
        {
            if (strlength)
            {
                uint8_t strsz = strlength > UINT8_MAX ? UINT8_MAX : (uint8_t)strlength;
                auto chsize = (uint8_t)(std::mbrlen(chidx + 1, strsz - 1, &mb) + 1);

                // Failed to decode. 0 means code error, -1 means string has been cutdown.
                if (chsize == 0 || chsize == (uint8_t)-1)
                    return 1;

                return chsize;
            }

        }
        return 1;
    }
    size_t u8strlen(wo_string_t u8str)
    {
        size_t strlength = 0;
        while (*u8str)
        {
            strlength++;
            u8str += u8chsize(u8str);

        }
        return strlength;
    }
    wo_string_t u8stridxstr(wo_string_t u8str, size_t chidx)
    {
        while (chidx && *u8str)
        {
            --chidx;
            u8str += u8chsize(u8str);
        }
        return u8str;
    }
    wchar_t u8stridx(wo_string_t u8str, size_t chidx)
    {
        std::mbstate_t mb = {};
        wchar_t wc;
        wo_string_t target_place = u8stridxstr(u8str, chidx);
        size_t parse_result = std::mbrtowc(&wc, target_place, strlen(target_place), &mb);
        if (parse_result > 0 && parse_result != size_t(-1) && parse_result != size_t(-2))
            return wc;
        return (wchar_t)(unsigned char)*target_place;
    }
    wo_string_t u8substr(wo_string_t u8str, size_t from, size_t length, size_t* out_sub_len)
    {
        auto substr = u8stridxstr(u8str, from);
        auto end_place = u8stridxstr(u8str, from + length);
        *out_sub_len = end_place - substr;
        return substr;
    }
    size_t u8blen2clen(wo_string_t u8str, size_t len)
    {
        size_t clen = 0;
        for (;;)
        {
            size_t chlen = u8chsize(u8str);
            if (chlen > len)
                break;

            ++clen;
            len -= chlen;
            u8str += chlen;
        }
        return clen;
    }
}