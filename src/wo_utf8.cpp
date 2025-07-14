#include "wo_afx.hpp"

#include <cwchar>

namespace wo
{
    size_t u8charnlen(const char* u8charp, size_t bytelen)
    {
        if (bytelen != 0)
        {
            const uint8_t u8ch = static_cast<uint8_t>(*u8charp);
            // Count the lead char with __lzcnt.

            int8_t mask = static_cast<int8_t>(0b10000000);
            for (size_t i = 1; i < UTF8MAXLEN; ++i)
            {
                // ARSH.
                mask >>= 1;

                if (static_cast<uint8_t>(mask) != (static_cast<uint8_t>(mask) & u8ch)
                    || i >= bytelen)
                    // Matched, Invalid (0b10xxxxxx) or ASCII/ASCII-Extended (0b0xxxxxxx) or length not engough.
                    return i;
                else
                {
                    if (static_cast<uint8_t>(0b10000000u)
                        != (static_cast<uint8_t>(u8charp[i]) & static_cast<uint8_t>(0b11000000u)))
                        // Invalid, not 0b10xxxxxx.
                        return 1;
                }
            }
            // Too long, invalid.
            return 1;
        }
        return 0;
    }
    size_t u8strnlen(wo_string_t u8str, size_t bytelen)
    {
        size_t result = 0;
        while (bytelen)
        {
            bytelen -= u8charnlen(u8str, bytelen);
            ++result;
        }
        return result;
    }
    bool u8strnchar(wo_string_t u8str, size_t bytelen, size_t* out_charsz)
    {
        if (1 == (*out_charsz = u8charnlen(u8str, bytelen)))
        {
            if (static_cast<uint8_t>(*u8str) & static_cast<uint8_t>(0b10000000))
                return false;
        }
        return true;
    }
    wo_string_t u8substr(wo_string_t u8str, size_t bytelen, size_t from, size_t* out_len)
    {
        const char* p = u8str;
        for (; from != 0 && bytelen != 0; --from)
        {
            const size_t charlen = u8charnlen(p, bytelen);
            p += charlen;
            bytelen -= charlen;
        }
        *out_len = bytelen;
        return p;
    }
    wo_string_t u8substrn(wo_string_t u8str, size_t bytelen, size_t from, size_t length, size_t* out_len)
    {
        size_t step_a_len;
        const char* p = u8substr(u8str, bytelen, from, &step_a_len);
        const char* p_end = u8substr(p, step_a_len, length, &/* useless */step_a_len);

        *out_len = p_end - p;
        return p;
    }
    wo_string_t u8substrr(wo_string_t u8str, size_t bytelen, size_t from, size_t tail, size_t* out_len)
    {
        return u8substrn(u8str, bytelen, from, tail >= from ? (tail - from) + 1 : 0, out_len);
    }

    size_t u8combineu16(const char* u8charp, size_t bytelen, char16_t out_c16[UTF16MAXLEN], size_t* out_u16len)
    {
        const uint8_t* u8ptr = reinterpret_cast<const uint8_t*>(u8charp);
        const size_t charlen = u8charnlen(u8charp, bytelen);
        uint32_t codepoint = 0;

        // Decode UTF-8 character
        switch (charlen)
        {
        case 0:
            // No character, return 0
            out_c16[0] = 0;
            *out_u16len = 0;
            return 0;
        case 1:
            // Single-byte character (ASCII)
            codepoint = *u8ptr;
            break;
        case 2:
            // Two-byte character
            codepoint = ((u8ptr[0] & 0x1F) << 6) | (u8ptr[1] & 0x3F);
            break;
        case 3:
            // Three-byte character
            codepoint = ((u8ptr[0] & 0x0F) << 12) | ((u8ptr[1] & 0x3F) << 6) | (u8ptr[2] & 0x3F);
            break;
        case 4:
            // Four-byte character
            codepoint = ((u8ptr[0] & 0x07) << 18) | ((u8ptr[1] & 0x3F) << 12) |
                ((u8ptr[2] & 0x3F) << 6) | (u8ptr[3] & 0x3F);
            break;
            //case 5:
            //    // Five-byte character
            //    codepoint = ((u8ptr[0] & 0x03) << 24) | ((u8ptr[1] & 0x3F) << 18) |
            //        ((u8ptr[2] & 0x3F) << 12) | ((u8ptr[3] & 0x3F) << 6) | (u8ptr[4] & 0x3F);
            //    break;
            //case 6:
            //    // Six-byte character
            //    codepoint = ((u8ptr[0] & 0x01) << 30) | ((u8ptr[1] & 0x3F) << 24) |
            //        ((u8ptr[2] & 0x3F) << 18) | ((u8ptr[3] & 0x3F) << 12) |
            //        ((u8ptr[4] & 0x3F) << 6) | (u8ptr[5] & 0x3F);
            //    break;
        default:
            wo_error("Invalid UTF-8 character length.");
        }

        // Encode as UTF-16
        if (codepoint <= 0xFFFF)
        {
            // BMP character
            out_c16[0] = static_cast<char16_t>(codepoint);
            *out_u16len = 1;
        }
        else if (codepoint <= 0x10FFFF)
        {
            // Supplementary character, use surrogate pair
            codepoint -= 0x10000;
            out_c16[0] = static_cast<char16_t>(0xD800 + (codepoint >> 10));    // High surrogate
            out_c16[1] = static_cast<char16_t>(0xDC00 + (codepoint & 0x3FF));  // Low surrogate
            *out_u16len = 2;
        }
        else
        {
            // Invalid codepoint (shouldn't happen with proper UTF-8 input)
            out_c16[0] = u'\uFFFD'; // Unicode replacement character
            *out_u16len = 1;
        }
        return charlen;
    }
    size_t u16exractu8(const char16_t* u16charp, size_t charcount, char out_c8[UTF8MAXLEN], size_t* out_u8len)
    {
        if (charcount == 0)
        {
            out_c8[0] = '\0'; // Null-terminate the output
            *out_u8len = 0;
            return 0;
        }

        char32_t codepoint;
        if (charcount >= 2
            && u16charp[0] >= static_cast<char16_t>(0xD800u)
            && u16charp[0] <= static_cast<char16_t>(0xDBFFu)
            && u16charp[1] >= static_cast<char16_t>(0xDC00u)
            && u16charp[1] <= static_cast<char16_t>(0xDFFFu))
        {
            // Is surrogate
            codepoint =
                static_cast<char32_t>(
                    ((u16charp[0] - static_cast<char16_t>(0xD800)) << static_cast<char16_t>(10))
                    | (u16charp[1] - static_cast<char16_t>(0xDC00)))
                + static_cast<char32_t>(0x10000);
        }
        else
        {
            codepoint = static_cast<char32_t>(u16charp[0]);
        }

        // Encode as UTF-8
        if (codepoint <= 0x7F)
        {
            // Single-byte character (ASCII)
            out_c8[0] = static_cast<char>(codepoint);
            return 1;
        }
        else if (codepoint <= 0x7FF)
        {
            // Two-byte character
            out_c8[0] = static_cast<char>(0xC0 | (codepoint >> 6));
            out_c8[1] = static_cast<char>(0x80 | (codepoint & 0x3F));
            *out_u8len = 2;
        }
        else if (codepoint <= 0xFFFF)
        {
            // Three-byte character
            out_c8[0] = static_cast<char>(0xE0 | (codepoint >> 12));
            out_c8[1] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
            out_c8[2] = static_cast<char>(0x80 | (codepoint & 0x3F));
            *out_u8len = 3;
        }
        else if (codepoint <= 0x10FFFF)
        {
            // Four-byte character
            out_c8[0] = static_cast<char>(0xF0 | (codepoint >> 18));
            out_c8[1] = static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
            out_c8[2] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
            out_c8[3] = static_cast<char>(0x80 | (codepoint & 0x3F));
            *out_u8len = 4;
            return 2;
        }
        else
        {
            // Invalid codepoint, use replacement character
            out_c8[0] = 0xEF;
            out_c8[1] = 0xBF;
            out_c8[2] = 0xBD; // Unicode replacement character
            *out_u8len = 3;
        }
        return 1;
    }

    std::string u8enstring(wo_string_t u8str, size_t bytelen, bool force_unicode)
    {
        std::string result;

        char escape_serial[13];

        const char* p = u8str;
        const char* p_end = u8str + bytelen;
        while (p != p_end)
        {
            char16_t u16buf[UTF16MAXLEN];
            size_t u16len = 0;

            const size_t this_char_u8_length = u8combineu16(p, bytelen, u16buf, &u16len);
            switch (u16len)
            {
            case 1:
                if (iswprint(static_cast<wchar_t>(u16buf[0])))
                    for (size_t i = 0; i < this_char_u8_length; ++i)
                        result.push_back(static_cast<char>(p[i]));
                else
                {
                    // Encode.
                    if (u16buf[0] > static_cast<char16_t>(0x00FFu) || force_unicode)
                    {
                        int r = snprintf(
                            escape_serial, 7, "\\u%04X", static_cast<uint16_t>(u16buf[0]));

                        wo_assert(r == 6);
                        (void)r;
                    }
                    else
                    {
                        int r = snprintf(
                            escape_serial, 5, "\\x%02X", static_cast<uint16_t>(u16buf[0]));

                        wo_assert(r == 4);
                        (void)r;
                    }
                    result += escape_serial;
                }
                break;
            case 2:
            {
                int r = snprintf(
                    escape_serial,
                    13,
                    "\\u%04X\\u%04X",
                    static_cast<uint16_t>(u16buf[0]),
                    static_cast<uint16_t>(u16buf[1]));

                wo_assert(r == 12);
                (void)r;

                result += escape_serial;
                break;
            }
            default:
                wo_error("Should not been here.");
            }

            p += this_char_u8_length;
        }
        wo_assert(p == p_end);

        return '\"' + result + '\"';
    }
    std::string u8destring(wo_string_t enu8str_zero_term)
    {
        std::string result;

        const char* p = enu8str_zero_term;
        if (*p == '"')
            ++p;

        while (const char pch = *p)
        {
            if (pch == '\\')
            {
                switch (const char pescch = *(++p))
                {
                case 'a':
                    result += '\a'; break;
                case 'b':
                    result += '\b'; break;
                case 'f':
                    result += '\f'; break;
                case 'n':
                    result += '\n'; break;
                case 'r':
                    result += '\r'; break;
                case 't':
                    result += '\t'; break;
                case 'v':
                    result += '\v'; break;
                case '0': case '1': case '2': case '3':
                case '4': case '5': case '6': case '7':
                {
                    // oct 1byte 
                    uint8_t oct_ascii = pescch - '0';
                    for (int i = 0; i < 2; i++)
                    {
                        const char nextch = *++p;
                        if (nextch >= '0' && nextch <= '8')
                        {
                            oct_ascii *= 8;
                            oct_ascii += static_cast<uint8_t>(nextch - '0');
                        }
                        else
                            break;
                    }
                    result += static_cast<char>(oct_ascii);
                    break;
                }
                case 'x':
                {
                    // hex 1byte.
                    uint8_t hex_ascii = 0;
                    for (int i = 0; i < 2; i++)
                    {
                        const char nextch = *++p;
                        if (nextch >= '0' && nextch <= '9')
                        {
                            hex_ascii *= 16;
                            hex_ascii += static_cast<uint8_t>(nextch - '0');
                        }
                        else if (nextch >= 'A' && nextch <= 'F')
                        {
                            hex_ascii *= 16;
                            hex_ascii += static_cast<uint8_t>(nextch - 'A') + static_cast<uint8_t>(10);
                        }
                        else if (nextch >= 'a' && nextch <= 'f')
                        {
                            hex_ascii *= 16;
                            hex_ascii += static_cast<uint8_t>(nextch - 'a') + static_cast<uint8_t>(10);
                        }
                        else
                        {
                            --p;
                            break;
                        }
                    }

                    result += static_cast<char>(hex_ascii);
                    break;
                }
                case 'u':
                {
                    // hex u16 2byte or 4byte.
                    char16_t hex_u16[UTF16MAXLEN] = {};
                    size_t hex_u16_count = 1;

                    for (int i = 0; i < 4; i++)
                    {
                        const char nextch = *++p;
                        if (nextch >= '0' && nextch <= '9')
                        {
                            hex_u16[0] *= 16;
                            hex_u16[0] += static_cast<uint8_t>(nextch - '0');
                        }
                        else if (nextch >= 'A' && nextch <= 'F')
                        {
                            hex_u16[0] *= 16;
                            hex_u16[0] += static_cast<uint8_t>(nextch - 'A') + static_cast<uint8_t>(10);
                        }
                        else if (nextch >= 'a' && nextch <= 'f')
                        {
                            hex_u16[0] *= 16;
                            hex_u16[0] += static_cast<uint8_t>(nextch - 'a') + static_cast<uint8_t>(10);
                        }
                        else
                        {
                            --p;
                            break;
                        }
                    }

                    const char* second_p = p + 1;

                    if (hex_u16[0] >= static_cast<char16_t>(0xD800u)
                        && hex_u16[0] <= static_cast<char16_t>(0xDBFFu)
                        && *second_p == '\\'
                        && *(++second_p) == 'u')
                    {
                        for (int i = 0; i < 4; i++)
                        {
                            const char nextch = *++second_p;
                            if (nextch >= '0' && nextch <= '9')
                            {
                                hex_u16[1] *= 16;
                                hex_u16[1] += static_cast<uint8_t>(nextch - '0');
                            }
                            else if (nextch >= 'A' && nextch <= 'F')
                            {
                                hex_u16[1] *= 16;
                                hex_u16[1] += static_cast<uint8_t>(nextch - 'A') + static_cast<uint8_t>(10);
                            }
                            else if (nextch >= 'a' && nextch <= 'f')
                            {
                                hex_u16[1] *= 16;
                                hex_u16[1] += static_cast<uint8_t>(nextch - 'a') + static_cast<uint8_t>(10);
                            }
                            else
                            {
                                --second_p;
                                break;
                            }
                        }
                        hex_u16_count = 2;
                    }

                    char u8buf[UTF8MAXLEN] = {};
                    size_t u8len = 0;

                    if (1 < u16exractu8(hex_u16, hex_u16_count, u8buf, &u8len))
                        p = second_p;

                    result.append(u8buf, u8len);
                    break;
                }
                default:
                    // Just add the backslash and the next character.
                    result += pescch;
                }
            }
            else if (pch == '"')
                break;
            else
                result += pch;

            ++p;
        }

        return result;
    }
}