#include "wo_utf8.hpp"
#include <cwchar>
#include <cstring>

namespace wo
{
    static_assert(MB_LEN_MAX <= 8);

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
    size_t u8strnlen(wo_string_t u8str, size_t len)
    {
        size_t strlength = 0;
        while (len)
        {
            strlength++;
            size_t wchlen = u8chsize(u8str);

            len -= wchlen;
            u8str += wchlen;
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
    wo_string_t u8strnidxstr(wo_string_t u8str, size_t chidx, size_t len)
    {
        while (chidx && len)
        {
            --chidx;
            size_t wchlen = u8chsize(u8str);
            
            len -= wchlen;
            u8str += wchlen;
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
    wchar_t u8strnidx(wo_string_t u8str, size_t chidx, size_t len)
    {
        std::mbstate_t mb = {};
        wchar_t wc;
        wo_string_t target_place = u8strnidxstr(u8str, chidx, len);
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
    wo_string_t u8substrn(wo_string_t u8str, size_t len, size_t from, size_t length, size_t* out_sub_len)
    {
        auto substr = u8strnidxstr(u8str, from, len);
        auto end_place = u8strnidxstr(u8str, from + length, len);
        *out_sub_len = end_place - substr;
        return substr;
    }
    size_t clen2u8blen(wo_string_t u8str, size_t len)
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

    uint8_t u8wchsize(wchar_t ch)
    {
        std::mbstate_t mb = {};

        size_t len = std::wcrtomb(nullptr, ch, &mb);
        if (len == (size_t)-1)
            return 1;

        return (uint8_t)len;
    }

    size_t u8blen2clen(wo_string_t u8str, size_t len)
    {
        return (size_t)(u8stridxstr(u8str, len) - u8str);
    }

    wchar_t* u8str2wstr(const char* str, size_t len, size_t* outlen)
    {

    }

    char* u8wstr2str(const wchar_t* str, size_t len, size_t* outlen)
    {

    }
}