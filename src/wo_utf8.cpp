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

    uint8_t u8mbc2wc(const char* ch, size_t len_not_zero, wo_char_t* out_wch)
    {
        std::mbstate_t mb = {};

        wo_assert(len_not_zero != 0);

        size_t mblen = std::mbrlen(ch, len_not_zero, &mb);
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

    bool is_high_surrogate(char16_t ch)
    {
        return (ch >= 0xD800 && ch <= 0xDBFF);
    }
    bool is_low_surrogate(char16_t ch)
    {
        return (ch >= 0xDC00 && ch <= 0xDFFF);
    }
    bool surrogate_utf32_to_2_utf16(char32_t codepoint, char16_t* out_hight, char16_t* out_low)
    {
        if (codepoint >= static_cast<char32_t>(0x10000)
            && codepoint <= static_cast<char32_t>(0x10FFFF))
        {
            codepoint -= 0x10000;
            *out_hight = static_cast<char16_t>(
                (codepoint >> static_cast<char32_t>(10)) + static_cast<char32_t>(0xD800));
            *out_low = static_cast<char16_t>(
                (codepoint & static_cast<char32_t>(0x3FF)) + static_cast<char32_t>(0xDC00));
            return true;
        }
        return false;
    }
    char32_t merge_high_surrogate_to_utf32(char16_t high, char16_t low)
    {
        wo_assert(is_high_surrogate(high) && is_low_surrogate(low));
        return static_cast<char32_t>((high - static_cast<char16_t>(0xD800)) << static_cast<char16_t>(10))
            + static_cast<char32_t>(low - static_cast<char16_t>(0xDC00))
            + static_cast<char32_t>(0x10000);
    }

    std::wstring enwstring(
        wo_wstring_t str,
        size_t len,
        bool force_unicode)
    {
        std::wstring result;
        for (size_t i = 0; i < len; ++i)
        {
            const wchar_t ch = str[i];
            if (iswprint(ch))
            {
                switch (ch)
                {
                case '"':
                    result += L"\\\""; break;
                case '\\':
                    result += L"\\\\"; break;
                default:
                    result += ch; break;
                }
            }
            else
            {
                const char32_t uch = static_cast<char32_t>(ch);

                wchar_t enstring_sequence[13];
                char16_t high, low;

                switch (sizeof(wchar_t))
                {
                case 4:
                    // Consider using \uXXXX\uXXXX
                    if (surrogate_utf32_to_2_utf16(uch, &high, &low))
                    {
                        int result = swprintf(
                            enstring_sequence,
                            13,
                            L"\\u%04X\\u%04X",
                            static_cast<uint32_t>(high),
                            static_cast<uint32_t>(low));

                        (void)result;
                        wo_assert(result == 12);

                        break;
                    }
                    [[fallthrough]];
                case 2:
                    // Consider using \uXXXX
                    if (0 != (uch & static_cast<char32_t>(0xFF00)) || force_unicode)
                    {
                        int result = swprintf(
                            enstring_sequence,
                            7,
                            L"\\u%04X",
                            static_cast<uint32_t>(static_cast<char16_t>(uch)));

                        (void)result;
                        wo_assert(result == 6);

                        break;
                    }
                    // Consider using \xXX
                    do
                    {
                        int result = swprintf(
                            enstring_sequence,
                            3,
                            L"\\x%02X",
                            static_cast<uint32_t>(static_cast<uint8_t>(uch)));

                        (void)result;
                        wo_assert(result == 6);

                        break;
                    } while (0);
                    break;
                default:
                    wo_error("Unknown wchar_t size.");
                }

                result += enstring_sequence;
            }
        }
        return L"\"" + result + L"\"";
    }
    std::wstring dewstring(wo_wstring_t str)
    {
        std::wstring result;

        if (*str == L'\"')
            ++str;

        while (wchar_t uch = *str)
        {
            if (uch == L'\\')
            {
                // Escape character 
                wchar_t escape_ch = *++str;
                switch (escape_ch)
                {
                case L'a':
                    result += '\a'; break;
                case L'b':
                    result += '\b'; break;
                case L'f':
                    result += '\f'; break;
                case L'n':
                    result += '\n'; break;
                case L'r':
                    result += '\r'; break;
                case L't':
                    result += L'\t'; break;
                case L'v':
                    result += '\v'; break;
                case L'0': case L'1': case L'2': case L'3': case L'4':
                case L'5': case L'6': case L'7': case L'8': case L'9':
                {
                    // oct 1byte 
                    unsigned char oct_ascii = escape_ch - L'0';
                    for (int i = 0; i < 2; i++)
                    {
                        unsigned char nextch = (unsigned char)*++str;
                        if (wo::lexer::lex_isodigit(nextch))
                        {
                            oct_ascii *= 8;
                            oct_ascii += wo::lexer::lex_hextonum(nextch);
                        }
                        else
                            break;
                    }
                    result += oct_ascii;
                    break;
                }
                case L'X':
                case L'x':
                {
                    // hex 1byte 
                    unsigned char hex_ascii = 0;
                    for (int i = 0; i < 2; i++)
                    {
                        wchar_t nextch = *++str;
                        if (wo::lexer::lex_isxdigit(nextch))
                        {
                            hex_ascii *= 16;
                            hex_ascii += wo::lexer::lex_hextonum(nextch);
                        }
                        else
                            break;
                    }
                    result += static_cast<wchar_t>(hex_ascii);
                    break;
                }
                case L'u':
                {
                    // unicode 2byte
                    char16_t hex_ascii = 0;
                    for (int i = 0; i < 4; i++)
                    {
                        wchar_t nextch = *++str;
                        if (wo::lexer::lex_isxdigit(nextch))
                        {
                            hex_ascii *= 16;
                            hex_ascii += wo::lexer::lex_hextonum(nextch);
                        }
                        else
                            break;
                    }

                    if (is_high_surrogate(hex_ascii)
                        && *str == L'\\' 
                        && *(str + 1) == L'u')
                    {
                        str += 2;

                        if constexpr(sizeof(wchar_t) == sizeof(char16_t))
                            result += static_cast<wchar_t>(hex_ascii);

                        char16_t low_hex_ascii = 0;
                        for (int i = 0; i < 4; i++)
                        {
                            wchar_t nextch = *++str;
                            if (wo::lexer::lex_isxdigit(nextch))
                            {
                                low_hex_ascii *= 16;
                                low_hex_ascii += wo::lexer::lex_hextonum(nextch);
                            }
                            else
                                break;
                        }

                        if constexpr (sizeof(wchar_t) == sizeof(char16_t))
                            result += static_cast<wchar_t>(low_hex_ascii);
                        else
                        {
                            if (is_low_surrogate(low_hex_ascii))
                                result += static_cast<wchar_t>(
                                    merge_high_surrogate_to_utf32(hex_ascii, low_hex_ascii));
                            else
                                result += static_cast<wchar_t>(low_hex_ascii);
                        }
                    }
                    else
                        result += static_cast<wchar_t>(hex_ascii);
                    
                    break;
                }
                case L'\'':
                case L'"':
                case L'?':
                case L'\\':
                default:
                    result += escape_ch;
                    break;
                }
            }
            else if (uch == L'"')
                break;
            else
                result += uch;
        }

        return result;
    }
    std::string enstring(
        wo_string_t str,
        size_t len,
        bool force_unicode)
    {
        const auto wstr = strn_to_wstr(str, len);
        return wo::wstr_to_str(enwstring(wstr.data(), wstr.size(), force_unicode));
    }
    std::string destring(wo_string_t str)
    {
        const auto wstr = dewstring(str_to_wstr(str).c_str());
        return wo::wstrn_to_str(wstr.data(), wstr.size());
    }
}