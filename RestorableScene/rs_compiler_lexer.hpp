#pragma once

#include "rs_assert.hpp"

#include <string>
#include <cwctype>
#include <vector>
#include <cwchar>

#ifdef ANSI_WIDE_CHAR_SIGN
#undef ANSI_WIDE_CHAR_SIGN
#define ANSI_WIDE_CHAR_SIGN L
#endif

namespace rs
{
    class lexer
    {
    public:
        enum lex_type
        {
            l_eof = -1,
            l_error = 0,
            l_identifier,           // identifier.
            l_literal_integer,      // 1 233 0x123456 0b1101001 032
            l_literal_handle,       // 0L 256L 0xFFL
            l_literal_real,         // 0.2  0.  .235
            l_literal_string,       // "" "helloworld" @"(println("hello");)"
            l_semicolon,            // ;

            l_add,                  // +
            l_sub,                  // - 
            l_mul,                  // * 
            l_div,                  // / 
            l_mod,                  // % 
            l_assign,               // =
            l_add_assign,           // +=
            l_sub_assign,           // -= 
            l_mul_assign,           // *=
            l_div_assign,           // /= 
            l_mod_assign,           // %= 
            l_equal,                // ==
            l_not_equal,            // !=
            l_larg_or_equal,        // >=
            l_less_or_equal,        // <=
            l_less,                 // <
            l_larg,                 // >
            l_land,                 // &&
            l_lor,                  // ||
            l_not,                  // !
            l_scopeing,             // ::

            l_left_brackets,        // (
            l_right_brackets,       // )
            l_left_curly_braces,    // {
            l_right_curly_braces,   // }

            l_import,               // import

            l_inf,
            l_nil,
        };

        struct lex_operator_info
        {
            lex_type in_lexer_type;
        };

        struct lex_keyword_info
        {
            lex_type in_lexer_type;
        };

    private:
        std::wstring  reading_buffer;  // use wide char-code.
        size_t        next_reading_index;

        size_t        now_file_rowno;
        size_t        now_file_colno;

        size_t        next_file_rowno;
        size_t        next_file_colno;

    private:

        inline const static std::map<std::wstring, lex_operator_info> lex_operator_list =
        {
            {L"+",      {l_add}},
            {L"-",      {l_sub}},
            {L"*",      {l_mul}},
            {L"/",      {l_div}},
            {L"%",      {l_mod}},
            {L"=",      {l_assign}},
            {L"+=",     {l_add_assign}},
            {L"-=",     {l_sub_assign}},
            {L"*=",     {l_mul_assign}},
            {L"/=",     {l_div_assign}},
            {L"%=",     {l_mod_assign}},
            {L"==",     {l_equal}},                // ==
            {L"!=",     {l_not_equal}},            // !=
            {L">=",     {l_larg_or_equal}},        // >=
            {L"<=",     {l_less_or_equal}},        // <=
            {L"<",      {l_less}},                 // <
            {L">",      {l_larg}},                 // >
            {L"&&",     {l_land}},                 // &&
            {L"||",     {l_lor}},                  // ||
            {L"!",      {l_not}},            // !=
            {L"::",     {l_scopeing}}
        };

        inline const static std::map<std::wstring, lex_keyword_info> key_word_list =
        {
            {L"import", {l_import}},
            {L"inf", {l_inf}},
            {L"nil", {l_nil}},
        };


    public:
        static lex_type lex_is_valid_operator(const std::wstring& op)
        {
            if (lex_operator_list.find(op) != lex_operator_list.end())
            {
                return lex_operator_list.at(op).in_lexer_type;
            }
            return l_error;
        }
        static lex_type lex_is_keyword(const std::wstring& op)
        {
            if (key_word_list.find(op) != key_word_list.end())
            {
                return key_word_list.at(op).in_lexer_type;
            }
            return l_error;
        }
        static bool lex_isoperatorch(int ch)
        {
            const static std::set<wchar_t> operator_char_set = []() {
                std::set<wchar_t> _result;
                for (auto& [_operator, operator_info] : lex_operator_list)
                    for (wchar_t ch : _operator)
                        _result.insert(ch);
                return _result;
            }();
            return operator_char_set.find(ch) != operator_char_set.end();
        }
        static bool lex_isspace(int ch)
        {
            if (ch == EOF)
                return false;

            return iswspace(ch);
        }
        static bool lex_isalpha(int ch)
        {
            // if ch can used as begin of identifier, return true
            if (ch == EOF)
                return false;

            // according to ISO-30112, Chinese character is belonging alpha-set,
            // so we can use 'iswalpha' directly.
            return iswalpha(ch);
        }
        static bool lex_isalnum(int ch)
        {
            // if ch can used in identifier (not first character), return true
            if (ch == EOF)
                return false;

            // according to ISO-30112, Chinese character is belonging alpha-set,
            // so we can use 'iswalpha' directly.
            return iswalnum(ch);
        }
        static bool lex_isdigit(int ch)
        {
            if (ch == EOF)
                return false;

            return isdigit(ch);
        }
        static bool lex_isxdigit(int ch)
        {
            if (ch == EOF)
                return false;

            return isxdigit(ch);
        }
        static bool lex_isodigit(int ch)
        {
            if (ch == EOF)
                return false;

            return ch >= '0' && ch <= '7';
        }
        static int lex_toupper(int ch)
        {
            if (ch == EOF)
                return EOF;

            return toupper(ch);
        }
        static int lex_hextonum(int ch)
        {
            rs_assert(lex_isxdigit(ch));

            if (isdigit(ch))
            {
                return ch - L'0';
            }
            return toupper(ch) - L'A' + 10;
        }

    public:
        lexer(const std::wstring& wstr)
            : reading_buffer(wstr)
            , next_reading_index(0)
            , now_file_rowno(1)
            , now_file_colno(0)
            , next_file_rowno(1)
            , next_file_colno(1)
        {
            // read_stream.peek
        }

    public:
        struct lex_error_msg
        {
            bool     is_warning;
            uint32_t errorno;
            size_t   row;
            size_t   col;
            std::wstring describe;

            std::wstring to_wstring(bool need_ansi_describe = true)
            {
                using namespace std;

                wchar_t error_num_str[10] = {};
                swprintf(error_num_str, 10, L"%04X", errorno);

                return (is_warning ?
                    (ANSI_HIY L"warning" ANSI_RST) : (ANSI_HIR L"error" ANSI_RST))
                    + ((is_warning ? L" W"s : L" E"s) + error_num_str)
                    + (L" (" + std::to_wstring(row) + L"," + std::to_wstring(col))
                    + (L") " + describe);
            }
        };

        bool lex_enable_error_warn = true;
        std::vector<lex_error_msg> lex_error_list;
        std::vector<lex_error_msg> lex_warn_list;

        template<typename ... TS>
        lex_type lex_error(uint32_t errorno, const wchar_t* fmt, TS&& ... args)
        {
            if (!lex_enable_error_warn)
                return l_error;

            size_t needed_sz = swprintf(nullptr, 0, fmt, args...);
            std::vector<wchar_t> describe;
            describe.resize(needed_sz + 1);
            swprintf(describe.data(), needed_sz + 1, fmt, args...);
            lex_error_msg& msg = lex_error_list.emplace_back(
                lex_error_msg
                {
                    false,
                    0x1000 + errorno,
                    now_file_rowno,
                    now_file_colno,
                    describe.data()
                }
            );
            skip_error_line();

            return l_error;
        }
        template<typename ... TS>
        void lex_warning(uint32_t errorno, const wchar_t* fmt, TS&& ... args)
        {
            if (!lex_enable_error_warn)
                return;

            size_t needed_sz = swprintf(nullptr, 0, fmt, args...);
            std::vector<wchar_t> describe;
            describe.resize(needed_sz + 1);
            swprintf(describe.data(), needed_sz + 1, fmt, args...);
            lex_error_msg& msg = lex_warn_list.emplace_back(
                lex_error_msg
                {
                    true,
                    errorno,
                    now_file_rowno,
                    now_file_colno,
                    describe.data()
                }
            );
        }

    public:

        lex_type peek(std::wstring* out_literal)
        {
            // Will store next_reading_index / file_rowno/file_colno
            // And disable error write:

            lex_enable_error_warn = false;

            auto old_index = next_reading_index;
            auto old_now_file_rowno = now_file_rowno;
            auto old_now_file_colno = now_file_colno;
            auto old_next_file_rowno = next_file_rowno;
            auto old_next_file_colno = next_file_colno;

            auto result = next(out_literal);

            next_reading_index = old_index;
            now_file_rowno = old_now_file_rowno;
            now_file_colno = old_now_file_colno;
            next_file_rowno = old_next_file_rowno;
            next_file_colno = old_next_file_colno;

            lex_enable_error_warn = true;

            return result;
        }

        int peek_ch()
        {
            if (next_reading_index >= reading_buffer.size())
                return EOF;

            return reading_buffer[next_reading_index + 1];
        }
        int next_ch()
        {
            if (next_reading_index >= reading_buffer.size())
                return EOF;

            now_file_rowno = next_file_rowno;
            now_file_colno = next_file_colno;
            next_file_colno++;

            return reading_buffer[next_reading_index++];
        }

        void new_line()
        {
            now_file_rowno = next_file_rowno;
            now_file_colno = next_file_colno;

            next_file_colno = 1;
            next_file_rowno++;
        }

        void skip_error_line()
        {
            // reading until '\n'
            int result = EOF;
            do
            {
                result = next_one();

            } while (result != L'\n' && result != EOF);
        }


        int next_one()
        {
            int readed_ch = next_ch();
            if (readed_ch == EOF)
                return readed_ch;

            if (readed_ch == '\n')          // manage linux's LF
            {
                new_line();
                return '\n';
            }
            if (readed_ch == '\r')          // manage mac's CR
            {
                if (peek_ch() == '\n')
                    next_ch();             // windows CRLF, eat LF

                new_line();
                return '\n';
            }

            return readed_ch;
        }
        int peek_one()
        {
            // Will store next_reading_index / file_rowno/file_colno

            auto old_index = next_reading_index;
            auto old_now_file_rowno = now_file_rowno;
            auto old_now_file_colno = now_file_colno;
            auto old_next_file_rowno = next_file_rowno;
            auto old_next_file_colno = next_file_colno;

            int result = next_one();

            next_reading_index = old_index;
            now_file_rowno = old_now_file_rowno;
            now_file_colno = old_now_file_colno;
            next_file_rowno = old_next_file_rowno;
            next_file_colno = old_next_file_colno;

            return result;
        }

        lex_type next(std::wstring* out_literal)
        {
            auto write_result = [&](int ch) {if (out_literal)(*out_literal) += (wchar_t)ch; };

            if (out_literal)
                (*out_literal) = L"";

        re_try_read_next_one:

            int readed_ch = next_one();

            if (lex_isspace(readed_ch))
                goto re_try_read_next_one;

            // //////////////////////////////////////////////////////////////////////////////////

            if (lex_isdigit(readed_ch))
            {
                write_result(readed_ch);

                int base = 10;
                bool is_real = false;
                bool is_handle = false;

                // is digit, return l_literal_integer/l_literal_handle/l_literal_real
                if (readed_ch == L'0')
                {
                    // it may be OCT DEC HEX
                    int sec_ch = peek_one();
                    if (lex_toupper(sec_ch) == 'X')
                        base = 16;                      // is hex
                    else if (lex_toupper(sec_ch) == 'B')
                        base = 2;                      // is bin
                    else if (lex_isodigit(sec_ch))
                        base = 8;                       // is oct
                    else if (!lex_isdigit(sec_ch))
                        base = 10;                      // is dec"0"
                    else
                        return lex_error(0x0001, L"Unexcepted character '%c' after '%c'.", sec_ch, readed_ch);
                }

                int following_chs;
                do
                {
                    following_chs = peek_one();
                    if (base == 10)
                    {
                        if (lex_isdigit(following_chs))
                            write_result(next_one());
                        else if (following_chs == L'.')
                        {
                            write_result(next_one());
                            if (is_real)
                                return lex_error(0x0001, L"Unexcepted character '%c' after '%c'.", following_chs, readed_ch);
                            is_real = true;
                        }
                        else if (lex_toupper(following_chs) == L'H')
                        {
                            write_result(next_one());
                            if (is_real)
                                return lex_error(0x0001, L"Unexcepted character '%c' after '%c'.", following_chs, readed_ch);
                            is_handle = true;
                        }
                        else if (lex_isalnum(following_chs))
                            return lex_error(0x0004, L"Illegal decimal literal.", following_chs, readed_ch);
                        else
                            break;                  // end read
                    }
                    else if (base == 16)
                    {
                        if (lex_isxdigit(following_chs) || lex_toupper(following_chs) == L'X')
                            write_result(next_one());
                        else if (following_chs == L'.')
                            return lex_error(0x0001, L"Unexcepted character '%c' after '%c'.", following_chs, readed_ch);
                        else if (lex_toupper(following_chs) == L'H')
                        {
                            write_result(next_one());
                            is_handle = true;
                        }
                        else if (lex_isalnum(following_chs))
                            return lex_error(0x0004, L"Illegal hexadecimal literal.", following_chs, readed_ch);
                        else
                            break;                  // end read
                    }
                    else if (base == 8)
                    {
                        if (lex_isodigit(following_chs))
                            write_result(next_one());
                        else if (following_chs == L'.')
                            return lex_error(0x0001, L"Unexcepted character '%c' after '%c'.", following_chs, readed_ch);
                        else if (lex_toupper(following_chs) == L'H')
                        {
                            write_result(next_one());
                            is_handle = true;
                        }
                        else if (lex_isalnum(following_chs))
                            return lex_error(0x0004, L"Illegal octal literal.", following_chs, readed_ch);
                        else
                            break;                  // end read
                    }
                    else if (base == 2)
                    {
                        if (following_chs == L'1' || following_chs == L'0' || lex_toupper(following_chs) == L'B')
                            write_result(next_one());
                        else if (following_chs == L'.')
                            return lex_error(0x0001, L"Unexcepted character '%c' after '%c'.", following_chs, readed_ch);
                        else if (lex_toupper(following_chs) == L'H')
                        {
                            write_result(next_one());
                            is_handle = true;
                        }
                        else if (lex_isalnum(following_chs))
                            return lex_error(0x0004, L"Illegal binary literal.", following_chs, readed_ch);
                        else
                            break;                  // end read
                    }
                    else
                        return lex_error(0x000, L"Lexer error, unknown number base.");

                } while (true);

                // end reading, decide which type to return;
                rs_assert(!(is_real && is_handle));

                if (is_real)
                    return l_literal_real;
                if (is_handle)
                    return l_literal_handle;
                return l_literal_integer;


            } // l_literal_integer/l_literal_handle/l_literal_real end
            else if (readed_ch == L';')
            {
                write_result(readed_ch);
                return l_semicolon;
            }
            else if (readed_ch == L'@')
            {
                // @"(Example string "without" '\' it will be very happy!)"

                if (int tmp_ch = next_one(); tmp_ch == L'"')
                {
                    int following_ch;
                    while (true)
                    {
                        following_ch = next_one();
                        if (following_ch == L'"' && peek_one() == L'@')
                        {
                            next_one();
                            return l_literal_string;
                        }

                        if (following_ch != EOF)
                            write_result(following_ch);
                        else
                            return lex_error(0x0002, L"Unexcepted EOF when parsing string.");
                    }
                }
                else
                    return lex_error(0x0001, L"Unexcepted character '%c' after '%c', except '\"'.", tmp_ch, readed_ch);
            }
            else if (readed_ch == L'"')
            {
                int following_ch;
                while (true)
                {
                    following_ch = next_one();
                    if (following_ch == L'"')
                        return l_literal_string;
                    if (following_ch != EOF && following_ch != '\n')
                    {
                        if (following_ch == L'\\')
                        {
                            // Escape character 
                            int escape_ch = next_one();
                            switch (escape_ch)
                            {
                            case L'\'':
                            case L'"':
                            case L'?':
                            case L'\\':
                                write_result(escape_ch); break;
                            case L'a':
                                write_result(L'\a'); break;
                            case L'b':
                                write_result(L'\b'); break;
                            case L'f':
                                write_result(L'\f'); break;
                            case L'n':
                                write_result(L'\n'); break;
                            case L't':
                                write_result(L'\t'); break;
                            case L'v':
                                write_result(L'\v'); break;
                            case L'0': case L'1': case L'2': case L'3': case L'4':
                            case L'5': case L'6': case L'7': case L'8': case L'9':
                            {
                                // oct 1byte 
                                int oct_ascii = escape_ch - L'0';
                                for (int i = 0; i < 2; i++)
                                {
                                    if (lex_isodigit(peek_one()))
                                    {
                                        oct_ascii *= 8;
                                        oct_ascii += lex_hextonum(next_one());
                                    }
                                    else
                                        break;
                                }
                                write_result(oct_ascii);
                                break;
                            }
                            case L'X':
                            case L'x':
                            {
                                // hex 1byte 
                                int hex_ascii = 0;
                                for (int i = 0; i < 2; i++)
                                {
                                    if (lex_isxdigit(peek_one()))
                                    {
                                        hex_ascii *= 16;
                                        hex_ascii += lex_hextonum(next_one());
                                    }
                                    else if (i == 0)
                                        goto str_escape_sequences_fail;
                                    else
                                        break;
                                }
                                write_result(hex_ascii);
                                break;
                            }
                            default:
                            str_escape_sequences_fail:
                                lex_warning(0x0001, L"Unknown escape sequences begin with '%c'.", escape_ch);
                                write_result(escape_ch);
                                break;
                            }
                        }
                        else
                            write_result(following_ch);
                    }
                    else
                        return lex_error(0x0002, L"Unexcepted end of line when parsing string.");
                }
            }
            else if (lex_isalpha(readed_ch))
            {
                // l_identifier or key world..
                write_result(readed_ch);

                int following_ch;
                while (true)
                {
                    following_ch = peek_one();
                    if (lex_isalnum(following_ch))
                        write_result(next_one());
                    else
                        break;
                }
                // TODO: Check it, does this str is a keyword?

                if (lex_type keyword_type = lex_is_keyword(*out_literal); l_error == keyword_type)
                    return l_identifier;
                else
                    return keyword_type;
            }
            else if (lex_isoperatorch(readed_ch))
            {
                write_result(readed_ch);
                lex_type operator_type = lex_is_valid_operator(*out_literal);

                int following_ch;
                do
                {
                    following_ch = peek_one();

                    if (!lex_isoperatorch(following_ch))
                        break;

                    lex_type tmp_op_type = lex_is_valid_operator(*out_literal + (wchar_t)following_ch);
                    if (tmp_op_type != l_error)
                    {
                        // maxim eat!
                        operator_type = tmp_op_type;
                        write_result(next_one());
                    }
                    else if (operator_type == l_error)
                    {
                        // not valid yet, continue...
                    }
                    else // is already a operator ready, return it.
                        break;

                } while (true);

                if (operator_type == l_error)
                    return lex_error(0x0003, L"Unknown operator: '%s'.", out_literal->c_str());

                return operator_type;
            }
            else if (readed_ch == L'(')
            {
                write_result(readed_ch);

                return l_left_brackets;
            }
            else if (readed_ch == L')')
            {
                write_result(readed_ch);

                return l_right_brackets;
            }
            else if (readed_ch == L'{')
            {
                write_result(readed_ch);

                return l_left_curly_braces;
            }
            else if (readed_ch == L'}')
            {
                write_result(readed_ch);

                return l_right_curly_braces;
            }
            else if (readed_ch == EOF)
            {
                return l_eof;
            }
            ///////////////////////////////////////////////////////////////////////////////////////
            return lex_error(0x000, L"Lexer error, unknown begin character: '%c'.", readed_ch);
        }
    };
}

#ifdef ANSI_WIDE_CHAR_SIGN
#undef ANSI_WIDE_CHAR_SIGN
#define ANSI_WIDE_CHAR_SIGN /* NOTHING */
#endif