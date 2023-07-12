#include "wo_compiler_ir.hpp"

namespace wo
{
    lexer::lexer(const std::wstring& wstr, const std::string _source_file)
        : reading_buffer(wstr)
        , next_reading_index(0)
        , now_file_rowno(1)
        , now_file_colno(0)
        , next_file_rowno(1)
        , next_file_colno(1)
        , format_string_count(0)
        , curly_count(0)
        , used_macro_list(nullptr)
    {
        // read_stream.peek
        if (wstring_pool::_m_this_thread_pool)
            source_file = wstring_pool::get_pstr(str_to_wstr(_source_file));
        else
            source_file = nullptr;
    }
    lexer::lexer(const std::string _source_file)
        : next_reading_index(0)
        , now_file_rowno(1)
        , now_file_colno(0)
        , next_file_rowno(1)
        , next_file_colno(1)
        , format_string_count(0)
        , curly_count(0)
        , source_file(wstring_pool::get_pstr(str_to_wstr(_source_file)))
        , used_macro_list(nullptr)
    {
        // read_stream.peek
        std::wstring readed_real_path;
        std::wstring input_path = *source_file;
        if (!wo::read_virtual_source(&reading_buffer, &readed_real_path, input_path, nullptr))
        {
            lex_error(lexer::errorlevel::error, WO_ERR_CANNOT_OPEN_FILE, input_path.c_str());
        }
        else
        {
            source_file = wstring_pool::get_pstr(readed_real_path);
        }
    }

    lex_type lexer::peek(std::wstring* out_literal)
    {
        // Will store next_reading_index / file_rowno/file_colno
        // And disable error write:

        if (peeked_flag)
        {
            return try_handle_macro(out_literal, peek_result_type, peek_result_str, true);
        }

        if (!temp_token_buff_stack.empty())
        {
            just_have_err = false;
            return try_handle_macro(out_literal, temp_token_buff_stack.top().first, temp_token_buff_stack.top().second, true);
        }

        auto old_now_file_rowno = now_file_rowno;
        auto old_now_file_colno = now_file_colno;
        auto old_next_file_rowno = next_file_rowno;
        auto old_next_file_colno = next_file_colno;

        peek_result_type = next(&peek_result_str);
        if (out_literal)
            *out_literal = peek_result_str;

        peeked_flag = true;

        after_pick_now_file_rowno = now_file_rowno;
        after_pick_now_file_colno = now_file_colno;
        after_pick_next_file_rowno = next_file_rowno;
        after_pick_next_file_colno = next_file_colno;

        now_file_rowno = old_now_file_rowno;
        now_file_colno = old_now_file_colno;
        next_file_rowno = old_next_file_rowno;
        next_file_colno = old_next_file_colno;

        return peek_result_type;
    }
    int lexer::peek_ch()
    {
        if (next_reading_index >= reading_buffer.size())
            return EOF;

        return reading_buffer[next_reading_index];
    }
    int lexer::next_ch()
    {
        if (next_reading_index >= reading_buffer.size())
            return EOF;

        now_file_rowno = next_file_rowno;
        now_file_colno = next_file_colno;
        next_file_colno++;

        return reading_buffer[next_reading_index++];
    }
    void lexer::new_line()
    {
        now_file_rowno = next_file_rowno;
        now_file_colno = next_file_colno;

        next_file_colno = 1;
        next_file_rowno++;
    }
    void lexer::skip_error_line()
    {
        // reading until '\n'
        int result = EOF;
        do
        {
            result = next_one();

        } while (result != L'\n' && result != EOF);
    }
    int lexer::next_one()
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
    int lexer::peek_one()
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

    lex_type lexer::try_handle_macro(std::wstring* out_literal, lex_type result_type, const std::wstring& result_str, bool workinpeek)
    {
        // ATTENTION: out_literal may point to result_str, please make sure donot read result_str after modify *out_literal.
        if (result_type == +lex_type::l_macro)
        {
            if (used_macro_list)
            {
                auto fnd = used_macro_list->find(result_str);
                if (fnd != used_macro_list->end() && fnd->second->_macro_action_vm)
                {
                    auto symb = wo_extern_symb(fnd->second->_macro_action_vm,
                        wstr_to_str(L"macro_" + fnd->second->macro_name).c_str());
                    wo_assert(symb);

                    if (workinpeek)
                    {
                        wo_assert(peeked_flag == true || !temp_token_buff_stack.empty());
                        if (!temp_token_buff_stack.empty())
                            temp_token_buff_stack.pop();

                        peeked_flag = false;
                    }

                    wo_push_pointer(fnd->second->_macro_action_vm, this);
                    wo_invoke_rsfunc(fnd->second->_macro_action_vm, symb, 1);

                    if (workinpeek)
                    {
                        wo_assert(peeked_flag == false);
                        peek_result_type = next(&peek_result_str);
                        if (out_literal)
                            *out_literal = peek_result_str;
                        peeked_flag = true;
                        return peek_result_type;
                    }

                    return next(out_literal);
                }
            }
        }

        if (out_literal)
            *out_literal = result_str;

        return result_type;

    }
    lex_type lexer::next(std::wstring* out_literal)
    {
        just_have_err = false;

        if (peeked_flag)
        {
            peeked_flag = false;

            now_file_rowno = after_pick_now_file_rowno;
            now_file_colno = after_pick_now_file_colno;
            next_file_rowno = after_pick_next_file_rowno;
            next_file_colno = after_pick_next_file_colno;

            return try_handle_macro(out_literal, peek_result_type, peek_result_str, false);
        }

        if (!temp_token_buff_stack.empty())
        {
            auto result_str = temp_token_buff_stack.top().second;
            auto result_type = temp_token_buff_stack.top().first;

            temp_token_buff_stack.pop();
            return try_handle_macro(out_literal, result_type, result_str, false);
        }

        std::wstring tmp_result;
        auto write_result = [&](int ch) {if (out_literal)(*out_literal) += (wchar_t)ch; else tmp_result += (wchar_t)ch; };
        auto read_result = [&]() -> std::wstring& {if (out_literal)return *out_literal; return  tmp_result; };

        if (out_literal)
            (*out_literal) = L"";

    re_try_read_next_one:

        int readed_ch = next_one();

        this_time_peek_from_rowno = now_file_rowno;
        this_time_peek_from_colno = now_file_colno;

        if (lex_isspace(readed_ch))
            goto re_try_read_next_one;

        // //////////////////////////////////////////////////////////////////////////////////
        if (readed_ch == L'/')
        {
            int nextch = peek_one();
            if (nextch == L'/')
            {
                // skip this line..
                skip_error_line();
                goto re_try_read_next_one;
            }
            else if (nextch == L'*')
            {
                next_one();
                auto fcol_begin = now_file_colno;
                auto frow_begin = now_file_rowno;

                do
                {
                    int readed_ch = next_one();
                    if (readed_ch == L'*')
                    {
                        if (next_one() == L'/')
                        {
                            goto re_try_read_next_one;
                        }
                    }
                    if (readed_ch == EOF)
                    {
                        now_file_colno = fcol_begin;
                        now_file_rowno = frow_begin;
                        lex_error(lexer::errorlevel::error, WO_ERR_MISMATCH_ANNO_SYM);
                        fcol_begin = now_file_colno;
                        frow_begin = now_file_rowno;

                        goto re_try_read_next_one;
                    }

                } while (true);
            }
        }

        if (/*readed_ch == L'f' || */readed_ch == L'F')
        {
            if (peek_ch() == L'"')
            {
                // Is f"..."
                if (format_string_count)
                    return lex_error(lexer::errorlevel::error, WO_ERR_RECURSIVE_FORMAT_STRING_IS_INVALID);

                next_one();

                int following_ch;
                while (true)
                {
                    following_ch = next_one();
                    if (following_ch == L'"')
                        return lex_type::l_literal_string;
                    if (following_ch == L'{')
                    {
                        curly_count = 0;
                        format_string_count++;
                        return lex_type::l_format_string;
                    }
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
                            case L'{':
                            case L'}':
                                write_result(escape_ch); break;
                            case L'a':
                                write_result(L'\a'); break;
                            case L'b':
                                write_result(L'\b'); break;
                            case L'f':
                                write_result(L'\f'); break;
                            case L'n':
                                write_result(L'\n'); break;
                            case L'r':
                                write_result(L'\r'); break;
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
                                        goto str_escape_sequences_fail_in_format_begin;
                                    else
                                        break;
                                }
                                write_result(hex_ascii);
                                break;
                            }
                            case L'U':
                            case L'u':
                            {
                                // hex 1byte 
                                int hex_ascii = 0;
                                for (int i = 0; i < 4; i++)
                                {
                                    if (lex_isxdigit(peek_one()))
                                    {
                                        hex_ascii *= 16;
                                        hex_ascii += lex_hextonum(next_one());
                                    }
                                    else if (i == 0)
                                        goto str_escape_sequences_fail_in_format_begin;
                                    else
                                        break;
                                }
                                write_result(hex_ascii);
                                break;
                            }
                            default:
                            str_escape_sequences_fail_in_format_begin:
                                lex_error(lexer::errorlevel::error, WO_ERR_UNKNOW_ESCSEQ_BEGIN_WITH_CH, escape_ch);
                                write_result(escape_ch);
                                break;
                            }
                        }
                        else
                            write_result(following_ch);
                    }
                    else
                        return lex_error(lexer::errorlevel::error, WO_ERR_UNEXCEPTED_EOL_IN_STRING);
                }
            }
            //else if (peek_ch() == L'@')
            //{
            //    // Is f@"..."@
            //}
        }
        if (format_string_count && readed_ch == L'}' && curly_count == 0)
        {
            int following_ch;
            while (true)
            {
                following_ch = next_one();
                if (following_ch == L'"')
                {
                    --format_string_count;
                    return lex_type::l_format_string_end;
                }
                if (following_ch == L'{')
                {
                    curly_count = 0;
                    return lex_type::l_format_string;
                }
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
                        case L'{':
                        case L'}':
                            write_result(escape_ch); break;
                        case L'a':
                            write_result(L'\a'); break;
                        case L'b':
                            write_result(L'\b'); break;
                        case L'f':
                            write_result(L'\f'); break;
                        case L'n':
                            write_result(L'\n'); break;
                        case L'r':
                            write_result(L'\r'); break;
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
                                    goto str_escape_sequences_fail_in_format_string;
                                else
                                    break;
                            }
                            write_result(hex_ascii);
                            break;
                        }
                        case L'U':
                        case L'u':
                        {
                            // hex 1byte 
                            int hex_ascii = 0;
                            for (int i = 0; i < 4; i++)
                            {
                                if (lex_isxdigit(peek_one()))
                                {
                                    hex_ascii *= 16;
                                    hex_ascii += lex_hextonum(next_one());
                                }
                                else if (i == 0)
                                    goto str_escape_sequences_fail_in_format_string;
                                else
                                    break;
                            }
                            write_result(hex_ascii);
                            break;
                        }
                        default:
                        str_escape_sequences_fail_in_format_string:
                            lex_error(lexer::errorlevel::error, WO_ERR_UNKNOW_ESCSEQ_BEGIN_WITH_CH, escape_ch);
                            write_result(escape_ch);
                            break;
                        }
                    }
                    else
                        write_result(following_ch);
                }
                else
                    return lex_error(lexer::errorlevel::error, WO_ERR_UNEXCEPTED_EOL_IN_STRING);
            }

        }

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
                    return lex_error(lexer::errorlevel::error, WO_ERR_UNEXCEPT_CH_AFTER_CH, sec_ch, readed_ch);
            }

            int following_chs;
            do
            {
                following_chs = peek_one();

                if (following_chs == '_' || following_chs == '\'')
                {
                    // this behavior is learn from rust and c++
                    // You can freely use the _/' mark in the number literal, except for the leading mark (0 0x 0b) 
                    next_one();
                    continue;
                }

                if (base == 10)
                {
                    if (lex_isdigit(following_chs))
                        write_result(next_one());
                    else if (following_chs == L'.')
                    {
                        write_result(next_one());
                        if (is_real)
                            return lex_error(lexer::errorlevel::error, WO_ERR_UNEXCEPT_CH_AFTER_CH, following_chs, readed_ch);
                        is_real = true;
                    }
                    else if (lex_toupper(following_chs) == L'H')
                    {
                        if (is_real)
                            return lex_error(lexer::errorlevel::error, WO_ERR_UNEXCEPT_CH_AFTER_CH, following_chs, readed_ch);
                        next_one();
                        is_handle = true;
                        break;
                    }
                    else if (lex_isalnum(following_chs))
                        return lex_error(lexer::errorlevel::error, WO_ERR_ILLEGAL_LITERAL);
                    else
                        break;                  // end read
                }
                else if (base == 16)
                {
                    if (lex_isxdigit(following_chs) || lex_toupper(following_chs) == L'X')
                        write_result(next_one());
                    else if (following_chs == L'.')
                        return lex_error(lexer::errorlevel::error, WO_ERR_UNEXCEPT_CH_AFTER_CH, following_chs, readed_ch);
                    else if (lex_toupper(following_chs) == L'H')
                    {
                        next_one();
                        is_handle = true;
                        break;
                    }
                    else if (lex_isalnum(following_chs))
                        return lex_error(lexer::errorlevel::error, WO_ERR_ILLEGAL_LITERAL);
                    else
                        break;                  // end read
                }
                else if (base == 8)
                {
                    if (lex_isodigit(following_chs))
                        write_result(next_one());
                    else if (following_chs == L'.')
                        return lex_error(lexer::errorlevel::error, WO_ERR_UNEXCEPT_CH_AFTER_CH, following_chs, readed_ch);
                    else if (lex_toupper(following_chs) == L'H')
                    {
                        next_one();
                        is_handle = true;
                        break;
                    }
                    else if (lex_isalnum(following_chs))
                        return lex_error(lexer::errorlevel::error, WO_ERR_ILLEGAL_LITERAL);
                    else
                        break;                  // end read
                }
                else if (base == 2)
                {
                    if (following_chs == L'1' || following_chs == L'0' || lex_toupper(following_chs) == L'B')
                        write_result(next_one());
                    else if (following_chs == L'.')
                        return lex_error(lexer::errorlevel::error, WO_ERR_UNEXCEPT_CH_AFTER_CH, following_chs, readed_ch);
                    else if (lex_toupper(following_chs) == L'H')
                    {
                        next_one();
                        is_handle = true;
                    }
                    else if (lex_isalnum(following_chs))
                        return lex_error(lexer::errorlevel::error, WO_ERR_ILLEGAL_LITERAL);
                    else
                        break;                  // end read
                }
                else
                    return lex_error(lexer::errorlevel::error, WO_ERR_LEXER_ERR_UNKNOW_NUM_BASE);

            } while (true);

            // end reading, decide which type to return;
            wo_assert(!(is_real && is_handle));

            if (is_real)
                return lex_type::l_literal_real;
            if (is_handle)
                return lex_type::l_literal_handle;
            return lex_type::l_literal_integer;


        } // l_literal_integer/l_literal_handle/l_literal_real end
        else if (readed_ch == L';')
        {
            write_result(readed_ch);
            return lex_type::l_semicolon;
        }
        else if (readed_ch == L'@')
        {
            // @"(Example string "without" '\' it will be very happy!)"

            if (int tmp_ch = peek_one(); tmp_ch == L'"')
            {
                next_one();

                int following_ch;
                while (true)
                {
                    following_ch = next_one();
                    if (following_ch == L'"' && peek_one() == L'@')
                    {
                        next_one();
                        return lex_type::l_literal_string;
                    }

                    if (following_ch != EOF)
                        write_result(following_ch);
                    else
                        return lex_error(lexer::errorlevel::error, WO_ERR_UNEXCEPT_EOF);
                }
            }
            else
                goto checking_valid_operator;
            /*return lex_error(lexer::errorlevel::error, WO_ERR_UNEXCEPT_CH_AFTER_CH_EXCEPT_CH, tmp_ch, readed_ch, L'\"');*/
        }
        else if (readed_ch == L'"')
        {
            int following_ch;
            while (true)
            {
                following_ch = next_one();
                if (following_ch == L'"')
                    return lex_type::l_literal_string;
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
                        case L'r':
                            write_result(L'\r'); break;
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
                        case L'U':
                        case L'u':
                        {
                            // hex 1byte 
                            int hex_ascii = 0;
                            for (int i = 0; i < 4; i++)
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
                            lex_error(lexer::errorlevel::error, WO_ERR_UNKNOW_ESCSEQ_BEGIN_WITH_CH, escape_ch);
                            write_result(escape_ch);
                            break;
                        }
                    }
                    else
                        write_result(following_ch);
                }
                else
                    return lex_error(lexer::errorlevel::error, WO_ERR_UNEXCEPTED_EOL_IN_STRING);
            }
        }
        else if (readed_ch == L'\'')
        {
            int following_ch;

            following_ch = next_one();
            if (following_ch == L'\'')
                return lex_error(lexer::errorlevel::error, WO_ERR_UNEXCEPT_CH_AFTER_CH, L'\'', L'\'');
            else if (following_ch != EOF && following_ch != '\n')
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
                    case L'r':
                        write_result(L'\r'); break;
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
                                goto char_escape_sequences_fail;
                            else
                                break;
                        }
                        write_result(hex_ascii);
                        break;
                    }
                    case L'U':
                    case L'u':
                    {
                        // hex 1byte 
                        int hex_ascii = 0;
                        for (int i = 0; i < 4; i++)
                        {
                            if (lex_isxdigit(peek_one()))
                            {
                                hex_ascii *= 16;
                                hex_ascii += lex_hextonum(next_one());
                            }
                            else if (i == 0)
                                goto char_escape_sequences_fail;
                            else
                                break;
                        }
                        write_result(hex_ascii);
                        break;
                    }
                    default:
                    char_escape_sequences_fail:
                        lex_error(lexer::errorlevel::error, WO_ERR_UNKNOW_ESCSEQ_BEGIN_WITH_CH, escape_ch);
                        write_result(escape_ch);
                        break;
                    }
                }
                else
                    write_result(following_ch);
            }
            else
                return lex_error(lexer::errorlevel::error, WO_ERR_UNEXCEPTED_EOL_IN_CHAR);

            following_ch = next_one();
            if (following_ch == L'\'')
                return lex_type::l_literal_char;
            else
                return lex_error(lexer::errorlevel::error, WO_ERR_LEXER_ERR_UNKNOW_BEGIN_CH L" " WO_TERM_EXCEPTED L" '\''", following_ch);
        }
        else if (lex_isoperatorch(readed_ch))
        {
        checking_valid_operator:
            write_result(readed_ch);
            lex_type operator_type = lex_is_valid_operator(read_result());

            int following_ch;
            do
            {
                following_ch = peek_one();

                if (!lex_isoperatorch(following_ch))
                    break;

                lex_type tmp_op_type = lex_is_valid_operator(read_result() + (wchar_t)following_ch);
                if (tmp_op_type != +lex_type::l_error)
                {
                    // maxim eat!
                    operator_type = tmp_op_type;
                    write_result(next_one());
                }
                else if (operator_type == +lex_type::l_error)
                {
                    // not valid yet, continue...
                }
                else // is already a operator ready, return it.
                    break;

            } while (true);

            if (operator_type == +lex_type::l_error)
                return lex_error(lexer::errorlevel::error, WO_ERR_UNKNOW_OPERATOR_STR, read_result().c_str());

            return operator_type;
        }
        else if (readed_ch == L'(')
        {
            write_result(readed_ch);

            return lex_type::l_left_brackets;
        }
        else if (readed_ch == L')')
        {
            write_result(readed_ch);

            return lex_type::l_right_brackets;
        }
        else if (readed_ch == L'{')
        {
            write_result(readed_ch);
            ++curly_count;
            return lex_type::l_left_curly_braces;
        }
        else if (readed_ch == L'}')
        {
            write_result(readed_ch);
            --curly_count;
            return lex_type::l_right_curly_braces;
        }
        else if (readed_ch == L'#')
        {
            // Read sharp
            // #macro

            std::wstring pragma_name;

            if (lex_isidentbeg(peek_one()))
            {
                pragma_name += next_one();
                while (lex_isident(peek_one()))
                    pragma_name += next_one();
            }

            if (pragma_name == L"macro")
            {
                // OK FINISH PRAGMA, CONTINUE

                std::shared_ptr<macro> p = std::make_shared<macro>(*this);
                if (this->used_macro_list == nullptr)
                    this->used_macro_list = std::make_shared<std::unordered_map<std::wstring, std::shared_ptr<macro>>>();

                if (this->used_macro_list->find(p->macro_name) != this->used_macro_list->end())
                    lex_error(lexer::errorlevel::error, WO_ERR_UNKNOWN_REPEAT_MACRO_DEFINE, p->macro_name.c_str());
                else
                    (*this->used_macro_list)[p->macro_name] = p;

                goto re_try_read_next_one;
            }
            else
            {
                lex_error(lexer::errorlevel::error, WO_ERR_UNKNOWN_PRAGMA_COMMAND, pragma_name.c_str());
            }

            goto re_try_read_next_one;
        }
        else if (readed_ch == EOF)
        {
            return lex_type::l_eof;
        }
        else if (lex_isidentbeg(readed_ch))
        {
            // l_identifier or key world..
            write_result(readed_ch);

            int following_ch;
            while (true)
            {
                following_ch = peek_one();
                if (lex_isident(following_ch))
                    write_result(next_one());
                else
                    break;
            }

            if (lex_type keyword_type = lex_is_keyword(read_result()); +lex_type::l_error == keyword_type)
            {
                bool is_macro = false;
                while (true)
                {
                    following_ch = peek_one();
                    if (lex_isspace(following_ch))
                        next_one();
                    else if (following_ch == L'!')
                    {
                        // Peek next character, make sure not "!=".
                        // TODO: Too bad.
                        if (next_reading_index + 1 >= reading_buffer.size()
                            || reading_buffer[next_reading_index + 1] != L'=')
                        {
                            next_one();
                            is_macro = true;
                        }
                        break;
                    }
                    else
                        break;
                }

                if (is_macro)
                    return try_handle_macro(out_literal, lex_type::l_macro, read_result(), false);
                else
                    return lex_type::l_identifier;
            }
            else
                return keyword_type;
        }
        ///////////////////////////////////////////////////////////////////////////////////////
        return lex_error(lexer::errorlevel::error, WO_ERR_LEXER_ERR_UNKNOW_BEGIN_CH, readed_ch);
    }
    void lexer::push_temp_for_error_recover(lex_type type, const std::wstring& out_literal)
    {
        temp_token_buff_stack.push({ type ,out_literal });
    }

    macro::macro(lexer& lex)
        : _macro_action_vm(nullptr)
    {
        /*
        #macro PRINT_HELLOWORLD
        {
            lexer->lex(@"std::println("Helloworld")"@);
        }

        PRINT_HELLOWORLD;

        */
        lex.next(&macro_name);

        size_t scope_count = 1;
        if (lex.next(nullptr) == +lex_type::l_left_curly_braces)
        {
            std::wstring macro_anylzing_src =
                L"import woo::macro; extern func macro_" +
                macro_name + L"(lexer:std::lexer) {";
            ;
            size_t index = lex.next_reading_index;
            do
            {
                auto type = lex.next(nullptr);
                if (type == +lex_type::l_right_curly_braces)
                    scope_count--;
                else if (type == +lex_type::l_left_curly_braces)
                    scope_count++;
                else if (type == +lex_type::l_eof)
                    break;

            } while (scope_count);

            size_t end_index = lex.next_reading_index;

            _macro_action_vm = wo_create_vm();
            if (!wo_load_source(_macro_action_vm,
                ("macro_" + wstr_to_str(macro_name) + ".wo").c_str(),
                wstr_to_str(macro_anylzing_src + lex.reading_buffer.substr(index, end_index - index)).c_str()))
            {
                lex.lex_error(lexer::errorlevel::error, WO_ERR_FAILED_TO_COMPILE_MACRO_CONTROLOR,
                    str_to_wstr(wo_get_compile_error(_macro_action_vm, WO_NOTHING)).c_str());

                wo_close_vm(_macro_action_vm);
                _macro_action_vm = nullptr;
            }
            else
                wo_run(_macro_action_vm);
        }
        else
            lex.lex_error(lexer::errorlevel::error, WO_ERR_HERE_SHOULD_HAVE, L"{");

    }
}