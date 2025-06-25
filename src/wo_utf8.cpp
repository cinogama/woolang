#include "wo_afx.hpp"

#include <cwchar>

namespace wo
{
    static_assert(MB_LEN_MAX <= UINT8_MAX);

    uint8_t u8chsize(const char* chidx, size_t len)
    {
        std::mbstate_t mb = {};

        if (std::mbrlen(chidx, 1, &mb) == (size_t)-2)
        {
            auto strlength = strlen(chidx);
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
    size_t u8strnlen(wo_string_t u8str, size_t len)
    {
        size_t strlength = 0;
        while (len)
        {
            strlength++;
            size_t wchlen = u8chsize(u8str, len);

            len -= wchlen;
            u8str += wchlen;
        }
        return strlength;
    }
    wo_string_t u8strnidxstr(wo_string_t u8str, size_t len, size_t chidx)
    {
        while (chidx && len)
        {
            --chidx;
            size_t wchlen = u8chsize(u8str, len);
            
            len -= wchlen;
            u8str += wchlen;
        }
        return u8str;
    }
    wo_char_t u8strnidx(wo_string_t u8str, size_t len, size_t chidx)
    {
        std::mbstate_t mb = {};
        wchar_t wc;
        wo_string_t target_place = u8strnidxstr(u8str, len, chidx);
        size_t parse_result = std::mbrtowc(&wc, target_place, strlen(target_place), &mb);
        if (parse_result > 0 && parse_result != size_t(-1) && parse_result != size_t(-2))
            return wc;
        return (wo_char_t)(unsigned char)*target_place;
    }
    wo_string_t u8substrr(wo_string_t u8str, size_t len, size_t from, size_t til, size_t* out_sub_len)
    {
        auto substr = u8strnidxstr(u8str, len, from);
        *out_sub_len = til > from ? u8strnidxstr(u8str, len, til) - substr : 0;
        return substr;
    }
    wo_string_t u8substrn(wo_string_t u8str, size_t len, size_t from, size_t length, size_t* out_sub_len)
    {
        auto substr = u8strnidxstr(u8str, len, from);
        *out_sub_len = u8strnidxstr(u8str, len, from + length) - substr;
        return substr;
    }
    
    size_t clen2u8blen(wo_string_t u8str, size_t len)
    {
        size_t wclen = 0;
        for (;;)
        {
            size_t chlen = u8chsize(u8str, len);
            if (chlen > len)
                break;

            ++wclen;
            len -= chlen;
            u8str += chlen;
        }
        return wclen;
    }
    size_t u8blen2clen(wo_string_t u8str, size_t len, size_t wclen)
    {
        return (size_t)(u8strnidxstr(u8str, len, wclen) - u8str);
    }

    uint8_t u8mbc2wc(const char* ch, size_t len, wo_char_t* out_wch) 
    {
        std::mbstate_t mb = {};

        size_t mblen = std::mbrlen(ch, len, &mb);
        if (mblen == (size_t)-2 || mblen == (size_t)-1 || mblen == 0)
        {
            // Bad chs or zero term, give origin char
            *out_wch = (wo_char_t)(unsigned char)*ch;
            return 1;
        }

        std::mbrtowc(out_wch, ch, mblen, &mb);
        return (uint8_t)mblen;
    }
    uint8_t u8wc2mbc(wo_char_t ch, char* out_mbchs)
    {
        std::mbstate_t mb = {};

        size_t len = std::wcrtomb(out_mbchs, ch, &mb);
        if (len == (size_t)-1)
        {
            // Bad chs, give efbfbd
            out_mbchs[0] = '\xEF';
            out_mbchs[1] = '\xBF';
            out_mbchs[2] = '\xBD';
            return 3;
        }

        return (uint8_t)len;
    }
    
    wo_wstring_t u8mbstowcs_zero_term(wo_string_t u8str)
    {
        std::mbstate_t mb = {};
        size_t wstr_length = mbsrtowcs(nullptr, &u8str, 0, &mb);

        // Failed to parse, meet invalid string.
        if (wstr_length == (size_t)-1)
            wstr_length = 0;

        wchar_t* wstr_buffer = new wchar_t[wstr_length + 1];
        mbsrtowcs(wstr_buffer, &u8str, wstr_length, &mb);
        wstr_buffer[wstr_length] = 0;

        return wstr_buffer;
    }
    wo_string_t u8wcstombs_zero_term(wo_wstring_t wstr)
    {
        std::mbstate_t mb = {};
        size_t mstr_byte_length = wcsrtombs(nullptr, &wstr, 0, &mb);

        // Failed to parse, meet invalid string.
        if (mstr_byte_length == (size_t)-1)
            mstr_byte_length = 0;

        char* mstr_buffer = new char[mstr_byte_length + 1];
        wcsrtombs(mstr_buffer, &wstr, mstr_byte_length, &mb);
        mstr_buffer[mstr_byte_length] = 0;

        return mstr_buffer;
    }

    wo_wstring_t u8mbstowcs(wo_string_t u8str, size_t len, size_t* out_len)
    {
        std::wstring result;
        while (len)
        {
            wo_char_t ch;
            size_t chlen = u8mbc2wc(u8str, len, &ch);

            result.append(1, ch);
            u8str += chlen;
            len -= chlen;
        }
        
        auto* chs = new wo_char_t[result.size() + 1];
        std::copy(result.begin(), result.end(), chs);
        chs[result.size()] = 0;

        *out_len = result.size();

        return chs;
    }
    wo_string_t u8wcstombs(wo_wstring_t wstr, size_t len, size_t* out_len)
    {
        std::string result;
        while (len)
        {
            char ch[MB_LEN_MAX];
            size_t chlen = u8wc2mbc(*wstr, ch);

            result.append(ch, chlen);
            ++wstr;
            len--;
        }

        auto* chs = new char[result.size() + 1];
        std::copy(result.begin(), result.end(), chs);
        chs[result.size()] = 0;

        *out_len = result.size();

        return chs;
    }

    std::string wstr_to_str(wo_wstring_t wstr)
    {
        auto buf = u8wcstombs_zero_term(wstr);
        std::string result = buf;

        delete[]buf;

        return result;
    }
    std::string wstr_to_str(const std::wstring& wstr)
    {
        return wstr_to_str(wstr.c_str());
    }

    std::wstring str_to_wstr(wo_string_t str)
    {
        auto buf = u8mbstowcs_zero_term(str);
        std::wstring result = buf;

        delete[]buf;

        return result;
    }
    std::wstring str_to_wstr(const std::string& str)
    {
        return str_to_wstr(str.c_str());
    }

    std::string wstrn_to_str(wo_wstring_t wstr, size_t len)
    {
        size_t buflen = 0;
        auto buf = u8wcstombs(wstr, len, &buflen);
        std::string result(buf, buflen);

        delete[]buf;

        return result;
    }
    std::string wstrn_to_str(const std::wstring& wstr)
    {
        return wstrn_to_str(wstr.c_str(), wstr.size());
    }

    std::wstring strn_to_wstr(wo_string_t str, size_t len)
    {
        size_t buflen = 0;
        auto buf = u8mbstowcs(str, len, &buflen);
        std::wstring result(buf, buflen);

        delete[]buf;

        return result;
    }
    std::wstring strn_to_wstr(const std::string& str)
    {
        return strn_to_wstr(str.c_str(), str.size());
    }
}