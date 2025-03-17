#include "wo_compiler_ir.hpp"

// Function from other source.
std::string _wo_dump_lexer_context_error(wo::lexer* lex, wo_inform_style_t style);

namespace wo
{
    const std::map<std::wstring, lex_type> tobe_lexer::_lex_operator_list =
    {
        {L"+",      {lex_type::l_add}},
        {L"-",      {lex_type::l_sub}},
        {L"*",      {lex_type::l_mul}},
        {L"/",      {lex_type::l_div}},
        {L"%",      {lex_type::l_mod}},
        {L"=",      {lex_type::l_assign}},
        {L"+=",     {lex_type::l_add_assign}},
        {L"-=",     {lex_type::l_sub_assign}},
        {L"*=",     {lex_type::l_mul_assign}},
        {L"/=",     {lex_type::l_div_assign}},
        {L"%=",     {lex_type::l_mod_assign}},
        {L":=",     {lex_type::l_value_assign}},
        {L"+:=",    {lex_type::l_value_add_assign}},
        {L"-:=",    {lex_type::l_value_sub_assign}},
        {L"*:=",    {lex_type::l_value_mul_assign}},
        {L"/:=",    {lex_type::l_value_div_assign}},
        {L"%:=",    {lex_type::l_value_mod_assign}},
        {L"+:",     {lex_type::l_value_add_assign}},
        {L"-:",     {lex_type::l_value_sub_assign}},
        {L"*:",     {lex_type::l_value_mul_assign}},
        {L"/:",     {lex_type::l_value_div_assign}},
        {L"%:",     {lex_type::l_value_mod_assign}},
        {L"==",     {lex_type::l_equal}},                // ==
        {L"!=",     {lex_type::l_not_equal}},            // !=
        {L">=",     {lex_type::l_larg_or_equal}},        // >=
        {L"<=",     {lex_type::l_less_or_equal}},        // <=
        {L"<",      {lex_type::l_less}},                 // <
        {L">",      {lex_type::l_larg}},                 // >
        {L"&&",     {lex_type::l_land}},                 // &&
        {L"||",     {lex_type::l_lor}},                  // ||
        {L"|",      {lex_type::l_or}},                  // ||
        {L"!",      {lex_type::l_lnot}},                  // !=
        {L"::",     {lex_type::l_scopeing}},
        {L":<",     {lex_type::l_template_using_begin}},
        {L",",      {lex_type::l_comma}},
        {L":",      {lex_type::l_typecast}},
        {L".",      {lex_type::l_index_point}},
        {L"..",     {lex_type::l_double_index_point}},
        {L"...",    {lex_type::l_variadic_sign}},
        {L"[",      {lex_type::l_index_begin}},
        {L"]",      {lex_type::l_index_end}},
        {L"->",     {lex_type::l_direct }},
        {L"|>",     {lex_type::l_direct }},
        {L"<|",     {lex_type::l_inv_direct}},
        {L"=>",     {lex_type::l_function_result }},
        {L"=>>",    {lex_type::l_bind_monad }},
        {L"->>",    {lex_type::l_map_monad }},
        {L"@",      {lex_type::l_at }},
        {L"?",      {lex_type::l_question }},
        {L"\\",     {lex_type::l_lambda}},
        {L"λ",      {lex_type::l_lambda}},
    };
    const std::map<std::wstring, lex_type> tobe_lexer::_key_word_list =
    {
        {L"import", {lex_type::l_import}},
        {L"nil", {lex_type::l_nil}},
        {L"true", {lex_type::l_true}},
        {L"false", {lex_type::l_false}},
        {L"while", {lex_type::l_while}},
        {L"for", {lex_type::l_for}},
        {L"if", {lex_type::l_if}},
        {L"else", {lex_type::l_else}},
        {L"let", {lex_type::l_let }},
        {L"mut", {lex_type::l_mut}},
        {L"immut", {lex_type::l_immut}},
        {L"func", {lex_type::l_func}},
        {L"return", {lex_type::l_return}},
        {L"using", {lex_type::l_using} },
        {L"alias", {lex_type::l_alias} },
        {L"namespace", {lex_type::l_namespace}},
        {L"extern", {lex_type::l_extern}},
        {L"public", {lex_type::l_public}},
        {L"private", {lex_type::l_private}},
        {L"protected", {lex_type::l_protected}},
        {L"static", {lex_type::l_static}},
        {L"enum", {lex_type::l_enum}},
        {L"as", {lex_type::l_as}},
        {L"is", {lex_type::l_is}},
        {L"typeof", {lex_type::l_typeof}},
        {L"break", {lex_type::l_break}},
        {L"continue", {lex_type::l_continue}},
        {L"where", {lex_type::l_where}},
        {L"operator", {lex_type::l_operator}},
        {L"union", {lex_type::l_union}},
        {L"match", {lex_type::l_match}},
        {L"struct", {lex_type::l_struct}},
        {L"typeid", {lex_type::l_typeid}},
        {L"do", {lex_type::l_do}},
    };

    lexer::lexer(
        std::optional<std::unique_ptr<std::wistream>>&& stream,
        wo_pstring_t _source_file_may_null,
        lexer* importer)
        : format_string_count(0)
        , curly_count(0)
        , peeked_flag(false)
        , used_macro_list(nullptr)
        , now_file_rowno(1)
        , now_file_colno(0)
        , next_file_rowno(1)
        , next_file_colno(1)
        , just_have_err(false)
        , source_file(_source_file_may_null)
        , last_lexer(importer)
    {
        if (stream)
            reading_buffer = std::move(stream.value());
        else
        {
            wo_assert(source_file != nullptr);
            parser_error(lexer::errorlevel::error, WO_ERR_CANNOT_OPEN_FILE, source_file->c_str());
        }
    }

    lex_type lexer::peek(std::wstring* out_literal)
    {
        // Will store next_reading_index / file_rowno/file_colno
        // And disable error write:

        if (peeked_flag)
        {
            return try_handle_macro(
                out_literal,
                peek_result_type,
                peek_result_str,
                true);
        }

        if (!temp_token_buff_stack.empty())
        {
            just_have_err = false;
            return try_handle_macro(
                out_literal,
                temp_token_buff_stack.top().first,
                temp_token_buff_stack.top().second,
                true);
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
        wchar_t ch = reading_buffer->peek();

        if (reading_buffer->eof() || !*reading_buffer)
        {
            reading_buffer->clear(reading_buffer->rdstate() & ~std::ios_base::failbit);
            return EOF;
        }

        return static_cast<int>(ch);
    }

    int lexer::next_ch()
    {
        wchar_t ch;
        if (!reading_buffer->get(ch))
        {
            reading_buffer->clear(reading_buffer->rdstate() & ~std::ios_base::failbit);
            return EOF;
        }

        now_file_rowno = next_file_rowno;
        now_file_colno = next_file_colno;
        next_file_colno++;

        return static_cast<int>(ch);
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

        if (readed_ch == L'\n')          // manage linux's LF
        {
            new_line();
            return L'\n';
        }
        if (readed_ch == L'\r')          // manage mac's CR
        {
            if (peek_ch() == L'\n')
                next_ch();             // windows CRLF, eat LF

            new_line();
            return L'\n';
        }

        return readed_ch;
    }

    int lexer::peek_one()
    {
        int readed_ch = peek_ch();

        if (readed_ch == L'\r')
            return L'\n';

        return readed_ch;
    }

    lex_type lexer::try_handle_macro(std::wstring* out_literal, lex_type result_type, const std::wstring& result_str, bool workinpeek)
    {
        // ATTENTION: out_literal may point to result_str, 
        //  please make sure donot read result_str after modify *out_literal.
        if (result_type == lex_type::l_macro && source_file != nullptr)
        {
            if (used_macro_list)
            {
                auto fnd = used_macro_list->find(result_str);
                if (fnd != used_macro_list->end() && fnd->second->_macro_action_vm)
                {
                    wo_integer_t script_func;
                    [[maybe_unused]] wo_handle_t jit_func;

#if WO_ENABLE_RUNTIME_CHECK
                    auto found =
#endif
                        wo_extern_symb(
                            fnd->second->_macro_action_vm,
                            wstr_to_str(L"macro_" + fnd->second->macro_name).c_str(),
                            &script_func,
                            &jit_func);


#if WO_ENABLE_RUNTIME_CHECK
                    wo_assert(found == WO_TRUE);
#endif

                    if (workinpeek)
                    {
                        wo_assert(peeked_flag == true || !temp_token_buff_stack.empty());
                        if (!temp_token_buff_stack.empty())
                            temp_token_buff_stack.pop();

                        peeked_flag = false;

                        now_file_rowno = after_pick_now_file_rowno;
                        now_file_colno = after_pick_now_file_colno;
                        next_file_rowno = after_pick_next_file_rowno;
                        next_file_colno = after_pick_next_file_colno;
                    }

                    wo_value s = wo_reserve_stack(fnd->second->_macro_action_vm, 1, nullptr);
                    wo_set_pointer(s, this);
                    wo_value result = wo_invoke_rsfunc(
                        fnd->second->_macro_action_vm, script_func, 1, nullptr, &s);

                    wo_pop_stack(fnd->second->_macro_action_vm, 1);

                    if (result == nullptr)
                        lex_error(wo::lexer::errorlevel::error, WO_ERR_FAILED_TO_RUN_MACRO_CONTROLOR,
                            fnd->second->macro_name.c_str(),
                            wo::str_to_wstr(wo_get_runtime_error(fnd->second->_macro_action_vm)).c_str());
                    else
                    {
                        // String pool is available during compiling.
                        // We can use it directly.
                        wo_pstring_t result_content_vfile =
                            wstring_pool::get_pstr(
                                L"woo/macro_"
                                + fnd->second->macro_name
                                + L"_result_"
                                + std::to_wstring((intptr_t)this)
                                + L".wo");

                        wo_assure(WO_TRUE == wo_virtual_source(
                            wo::wstr_to_str(*result_content_vfile).c_str(), wo_string(result), WO_TRUE));

                        std::wstring macro_result_buffer = wo::str_to_wstr(wo_string(result));

                        wo::lexer tmp_lex(std::make_unique<std::wistringstream>(macro_result_buffer), result_content_vfile, nullptr);

                        std::vector<std::pair<wo::lex_type, std::wstring>> lex_tokens;
                        for (;;)
                        {
                            std::wstring result;
                            auto token = tmp_lex.next(&result);

                            if (token == wo::lex_type::l_error || token == wo::lex_type::l_eof)
                                break;

                            lex_tokens.push_back({ token , result });
                        }
                        if (peeked_flag)
                        {
                            lex_tokens.push_back({ peek_result_type, peek_result_str });
                            peeked_flag = false;
                        }
                        if (tmp_lex.has_error())
                        {
                            lex_error(wo::lexer::errorlevel::error, WO_ERR_INVALID_TOKEN_MACRO_CONTROLOR,
                                fnd->second->macro_name.c_str());

                            get_cur_error_frame().back().describe +=
                                str_to_wstr(_wo_dump_lexer_context_error(&tmp_lex, WO_NOTHING)) + WO_MACRO_ANALYZE_END_HERE;
                        }
                        wo_assure(WO_TRUE == wo_remove_virtual_file(wo::wstr_to_str(*result_content_vfile).c_str()));

                        for (auto ri = lex_tokens.rbegin(); ri != lex_tokens.rend(); ri++)
                            push_temp_for_error_recover(ri->first, ri->second);
                    }
                    if (workinpeek)
                    {
                        after_pick_now_file_rowno = now_file_rowno;
                        after_pick_now_file_colno = now_file_colno;
                        after_pick_next_file_rowno = next_file_rowno;
                        after_pick_next_file_colno = next_file_colno;

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
        auto read_result = [&]() -> std::wstring& {if (out_literal)return *out_literal; return tmp_result; };

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
                    readed_ch = next_one();
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
                        return lex_type::l_format_string_begin;
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
                if (tmp_op_type != lex_type::l_error)
                {
                    // maxim eat!
                    operator_type = tmp_op_type;
                    write_result(next_one());
                }
                else if (operator_type == lex_type::l_error)
                {
                    // not valid yet, continue...
                }
                else // is already a operator ready, return it.
                    break;

            } while (true);

            if (operator_type == lex_type::l_error)
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
                pragma_name += (wchar_t)next_one();
                while (lex_isident(peek_one()))
                    pragma_name += (wchar_t)next_one();
            }

            // ATTENTION, SECURE:
            //  Disable macro handler if source_file == nullptr, it's in deserialize.
            //  Processing macros here may lead to arbitrary code execution.
            if (pragma_name == L"macro" && source_file != nullptr)
            {
                // OK FINISH PRAGMA, CONTINUE

                std::shared_ptr<macro> p = std::make_shared<macro>(*this);
                if (this->used_macro_list == nullptr)
                    this->used_macro_list = std::make_shared<std::unordered_map<std::wstring, std::shared_ptr<macro>>>();

                if (auto fnd = this->used_macro_list->find(p->macro_name);
                    fnd != this->used_macro_list->end())
                {
                    lex_error(lexer::errorlevel::error, WO_ERR_UNKNOWN_REPEAT_MACRO_DEFINE, p->macro_name.c_str());

                    wchar_t describe[256] = {};
                    swprintf(describe, 255, WO_INFO_SYMBOL_NAMED_DEFINED_HERE, p->macro_name.c_str());
                    error_impl(
                        lex_error_msg{
                            lexer::errorlevel::infom,
                            fnd->second->begin_row,
                            fnd->second->end_row,
                            fnd->second->begin_col,
                            fnd->second->end_col,
                            describe,
                            *fnd->second->filename,
                            (size_t)1
                        });

                }
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

            if (lex_type keyword_type = lex_is_keyword(read_result()); lex_type::l_error == keyword_type)
            {
                // ATTENTION, SECURE:
                //  Disable macro handler if source_file == nullptr, it's in deserialize.
                //  Processing macros here may lead to arbitrary code execution.
                if (peek_one() == L'!' && source_file != nullptr)
                {
                    next_one();
                    return try_handle_macro(out_literal, lex_type::l_macro, read_result(), false);
                }
                else
                    return lex_type::l_identifier;
            }
            else
                return keyword_type;
        }
        ///////////////////////////////////////////////////////////////////////////////////////
        else
        {
            write_result(readed_ch);
            return lex_type::l_unknown_token;
        }

    }
    void lexer::push_temp_for_error_recover(lex_type type, const std::wstring& out_literal)
    {
        temp_token_buff_stack.push({ type ,out_literal });
    }

    macro::macro(lexer& lex)
        : _macro_action_vm(nullptr)
        , begin_row(lex.now_file_rowno)
        , begin_col(lex.now_file_colno)
        , end_row(lex.next_file_rowno)
        , end_col(lex.next_file_colno)
        , filename(lex.source_file)
    {
        /*
        #macro PRINT_HELLOWORLD
        {
            return @"std::println("Helloworld")"@;
        }

        PRINT_HELLOWORLD!;
        */
        lex.next(&macro_name);

        size_t scope_count = 1;
        if (lex.next(nullptr) == lex_type::l_left_curly_braces)
        {
            std::wstring macro_anylzing_src =
                L"import woo::macro; extern func macro_" +
                macro_name + L"(lexer:std::lexer)=> string { do lexer;\n{";
            ;
            auto begin_place = lex.reading_buffer->tellg();
            bool meet_eof = false;
            do
            {
                auto type = lex.next(nullptr);

                if (type == lex_type::l_right_curly_braces)
                    scope_count--;
                else if (type == lex_type::l_left_curly_braces)
                    scope_count++;
                else if (type == lex_type::l_eof)
                {
                    meet_eof = true;
                    break;
                }

            } while (scope_count);

            if (meet_eof)
                lex.lex_error(lexer::errorlevel::error, WO_ERR_UNEXCEPT_EOF);
            else
            {
                auto macro_content_end_place = lex.reading_buffer->tellg();
                lex.reading_buffer->seekg(begin_place);

                std::vector<wchar_t> macro_content;

                while (lex.reading_buffer->eof() == false
                    && lex.reading_buffer
                    && lex.reading_buffer->tellg() < macro_content_end_place)
                {
                    wchar_t ch;
                    lex.reading_buffer->read(&ch, 1);
                    macro_content.push_back(ch);
                }
                macro_content.push_back(L'\0');

                _macro_action_vm = wo_create_vm();
                if (!wo_load_source(_macro_action_vm,
                    (wstr_to_str(lex.source_file->c_str()) + " : macro_" + wstr_to_str(macro_name) + ".wo").c_str(),
                    wstr_to_str(macro_anylzing_src + macro_content.data() + L"\nreturn \"\";}").c_str()))
                {
                    lex.lex_error(lexer::errorlevel::error, WO_ERR_FAILED_TO_COMPILE_MACRO_CONTROLOR);
                    lex.get_cur_error_frame().back().describe +=
                        str_to_wstr(wo_get_compile_error(_macro_action_vm, WO_NOTHING)) + WO_MACRO_CODE_END_HERE;

                    wo_close_vm(_macro_action_vm);
                    _macro_action_vm = nullptr;
                }
                else
                {
                    // Donot jit to make debug friendly.
                    if (nullptr == wo_run(_macro_action_vm))
                    {
                        lex.lex_error(lexer::errorlevel::error, WO_ERR_FAILED_TO_RUN_MACRO_CONTROLOR,
                            macro_name.c_str(),
                            wo::str_to_wstr(wo_get_runtime_error(_macro_action_vm)).c_str());
                    }
                }
            }
        }
        else
            lex.lex_error(lexer::errorlevel::error, WO_ERR_HERE_SHOULD_HAVE, L"{");
    }

    ///////////////////////////////////////////////////

    const wchar_t* tobe_lexer::lex_is_operate_type(lex_type tt)
    {
        for (auto& [op_str, op_type] : _lex_operator_list)
        {
            if (op_type == tt)
                return op_str.c_str();
        }
        return nullptr;
    }
    const wchar_t* tobe_lexer::lex_is_keyword_type(lex_type tt)
    {
        for (auto& [op_str, op_type] : _key_word_list)
        {
            if (op_type == tt)
                return op_str.c_str();
        }
        return nullptr;
    }
    lex_type tobe_lexer::lex_is_valid_operator(const std::wstring& op)
    {
        if (auto fnd = _lex_operator_list.find(op); fnd != _lex_operator_list.end())
            return fnd->second;

        return lex_type::l_error;
    }
    lex_type tobe_lexer::lex_is_keyword(const std::wstring& op)
    {
        if (auto fnd = _key_word_list.find(op); fnd != _key_word_list.end())
            return fnd->second;

        return lex_type::l_error;
    }
    bool tobe_lexer::lex_isoperatorch(int ch)
    {
        const static std::set<wchar_t> operator_char_set = []() {
            std::set<wchar_t> _result;
            for (auto& [_operator, operator_info] : _lex_operator_list)
                for (wchar_t wch : _operator)
                    _result.insert(wch);
            return _result;
            }();
        return operator_char_set.find((wchar_t)ch) != operator_char_set.end();
    }
    bool tobe_lexer::lex_isspace(int ch)
    {
        if (ch == EOF)
            return false;

        return ch == 0 || iswspace((wchar_t)ch);
    }
    bool tobe_lexer::lex_isalpha(int ch)
    {
        // if ch can used as begin of identifier, return true
        if (ch == EOF)
            return false;

        // according to ISO-30112, Chinese character is belonging alpha-set,
        // so we can use 'iswalpha' directly.
        return iswalpha((wchar_t)ch);
    }
    bool tobe_lexer::lex_isidentbeg(int ch)
    {
        // if ch can used as begin of identifier, return true
        if (ch == EOF)
            return false;

        // according to ISO-30112, Chinese character is belonging alpha-set,
        // so we can use 'iswalpha' directly.
        return iswalpha((wchar_t)ch) || (wchar_t)ch == L'_';
    }
    bool tobe_lexer::lex_isident(int ch)
    {
        // if ch can used as begin of identifier, return true
        if (ch == EOF)
            return false;

        // according to ISO-30112, Chinese character is belonging alpha-set,
        // so we can use 'iswalpha' directly.
        return iswalnum((wchar_t)ch) || (wchar_t)ch == L'_';
    }
    bool tobe_lexer::lex_isalnum(int ch)
    {
        // if ch can used in identifier (not first character), return true
        if (ch == EOF)
            return false;

        // according to ISO-30112, Chinese character is belonging alpha-set,
        // so we can use 'iswalpha' directly.
        return iswalnum((wchar_t)ch);
    }
    bool tobe_lexer::lex_isdigit(int ch)
    {
        if (ch == EOF)
            return false;

        return iswdigit((wchar_t)ch);
    }
    bool tobe_lexer::lex_isxdigit(int ch)
    {
        if (ch == EOF)
            return false;

        return iswxdigit((wchar_t)ch);
    }
    bool tobe_lexer::lex_isodigit(int ch)
    {
        if (ch == EOF)
            return false;

        return (wchar_t)ch >= L'0' && (wchar_t)ch <= L'7';
    }
    int tobe_lexer::lex_toupper(int ch)
    {
        if (ch == EOF)
            return EOF;

        return towupper((wchar_t)ch);
    }
    int tobe_lexer::lex_tolower(int ch)
    {
        if (ch == EOF)
            return EOF;

        return towlower((wchar_t)ch);
    }
    int tobe_lexer::lex_hextonum(int ch)
    {
        wo_assert(lex_isxdigit(ch));

        if (iswdigit((wchar_t)ch))
        {
            return (wchar_t)ch - L'0';
        }
        return towupper((wchar_t)ch) - L'A' + 10;
    }
    int tobe_lexer::lex_octtonum(int ch)
    {
        wo_assert(lex_isodigit(ch));

        return (wchar_t)ch - L'0';
    }

    tobe_lexer::tobe_lexer(
        std::optional<tobe_lexer*> who_import_me,
        const std::optional<wo_pstring_t>& source_path,
        std::unique_ptr<std::wistream>&& source_stream)
        : m_who_import_me(who_import_me)
        , m_source_path(source_path)
        , m_source_stream(std::move(source_stream))
        //
        , m_error_frame{}
        , m_declared_macro_list(
            who_import_me.has_value()
            // Share from root script.
            ? who_import_me.value()->m_declared_macro_list
            // Create for root script.
            : std::make_shared<declared_macro_map_t>())
        , m_imported_source_path_set(
            who_import_me.has_value()
            // Share from root script.
            ? who_import_me.value()->m_imported_source_path_set
            // Create for root script.
            : std::make_shared<imported_source_path_set_t>())
        , m_imported_ast_tree_list{}
        //
        , _m_peeked_tokens{}
        , _m_row_counter(0)
        , _m_col_counter(0)
        , _m_this_token_begin_row(0)
        , _m_this_token_begin_col(0)
        , _m_in_format_string(false)
        , _m_curry_count_in_format_string(0)
    {
        // Make sure error frame has one frame.
        (void)m_error_frame.emplace();
        wo_assert((bool)m_source_stream);
    }
    bool tobe_lexer::check_source_path_has_been_imported(wo_pstring_t full_path)
    {
        return !m_imported_source_path_set->insert(full_path).second;
    }
    void tobe_lexer::import_ast_tree(ast::AstBase* astbase)
    {
        m_imported_ast_tree_list.push_back(astbase);
    }
    int tobe_lexer::peek_char()
    {
        wchar_t ch = m_source_stream->peek();

        if (m_source_stream->eof() || !*m_source_stream)
        {
            m_source_stream->clear(
                m_source_stream->rdstate() &
                ~(std::ios_base::failbit | std::ios_base::eofbit));
            return EOF;
        }

        if (ch == L'\r')
            return L'\n';

        return static_cast<int>(ch);
    }
    int tobe_lexer::read_char()
    {
        wchar_t ch = m_source_stream->get();
        if (m_source_stream->eof() || !*m_source_stream)
        {
            m_source_stream->clear(
                m_source_stream->rdstate() &
                ~(std::ios_base::failbit | std::ios_base::eofbit));
            return EOF;
        }

        if (ch == L'\r')
        {
            if (peek_char() == L'\n')
                // Eat \r\n as \n
                (void)m_source_stream->get();

            ch = L'\n';
        }

        if (ch == L'\n')
        {
            _m_col_counter = 0;
            ++_m_row_counter;
        }
        else
            ++_m_col_counter;

        return static_cast<int>(ch);
    }

    void tobe_lexer::record_message(compiler_message_t&& message)
    {
        auto& emplaced_message = m_error_frame.top().emplace_back(std::move(message));

        emplaced_message.m_layer
            = m_error_frame.size() - 1 + (
                emplaced_message.m_level == msglevel_t::error ? 0 : 1);
    }

    void tobe_lexer::produce_token(lex_type type, std::wstring&& moved_token_text)
    {
        _m_peeked_tokens.emplace(std::move(
            peeked_token_t
            {
                type,
                std::move(moved_token_text),
                { _m_this_token_begin_row, _m_this_token_begin_col },
                { _m_row_counter, _m_col_counter },
            }));
    }
    void tobe_lexer::token_begin_here()
    {
        _m_this_token_begin_row = _m_row_counter;
        _m_this_token_begin_col = _m_col_counter;
    }

    const tobe_lexer::peeked_token_t* tobe_lexer::peek()
    {
        while (_m_peeked_tokens.empty())
            move_forward();

        wo_assert(!_m_peeked_tokens.empty());
        return &_m_peeked_tokens.front();
    }
    void tobe_lexer::move_forward()
    {
        if (size_t count = _m_peeked_tokens.size(); count > 0)
        {
            _m_peeked_tokens.pop();
            if (count > 1)
                return;
        }

        int readed_char;

        do
        {
            readed_char = read_char();

            if (!lexer::lex_isspace(readed_char))
                break;

        } while (true);

        // Mark token begin place.
        token_begin_here();
        if (_m_this_token_begin_col != 0)
            // Shift 1 col.
            --_m_this_token_begin_col;

        // Check if comment?
        if (readed_char == L'/')
        {
            int peeked_char = peek_char();
            if (peeked_char == L'/')
            {
                // Skip this line.
                do
                {
                    peeked_char = read_char();

                } while (peeked_char != L'\n' && peeked_char != EOF);
                return;
            }
            else if (peeked_char == L'*')
            {
                // Skip to next '*/'
                do
                {
                    peeked_char = read_char();
                    if (peeked_char == EOF)
                        break;
                    else if (peeked_char == L'*')
                    {
                        peeked_char = read_char();
                        if (peeked_char == EOF || peeked_char == L'/')
                            break;
                    }
                } while (true);
                return;
            }
        }

        std::wstring token_literal_result;
        auto append_result_char =
            [&token_literal_result](auto ch) {token_literal_result.push_back(static_cast<wchar_t>(ch)); };

        // Format string.
        bool is_format_string = false;
        if ((readed_char == L'f' || readed_char == L'F') && peek_char() == L'"')
        {
            (void)read_char();
            is_format_string = true;
        }
        else if (_m_in_format_string && readed_char == L'}' && _m_curry_count_in_format_string == 0)
            is_format_string = true;

        if (is_format_string)
        {
            // Is f"..."
            if (_m_in_format_string)
                return produce_lexer_error(msglevel_t::error, WO_ERR_RECURSIVE_FORMAT_STRING_IS_INVALID);

            int following_ch;
            while (true)
            {
                following_ch = read_char();
                if (following_ch == L'"')
                    return produce_token(lex_type::l_literal_string, std::move(token_literal_result));
                if (following_ch == L'{')
                {
                    _m_curry_count_in_format_string = 0;
                    _m_in_format_string = true;
                    return produce_token(lex_type::l_format_string_begin, std::move(token_literal_result));
                }
                if (following_ch != EOF && following_ch != '\n')
                {
                    if (following_ch == L'\\')
                    {
                        // Escape character 
                        int escape_ch = read_char();
                        switch (escape_ch)
                        {
                        case L'\'':
                        case L'"':
                        case L'?':
                        case L'\\':
                        case L'{':
                        case L'}':
                            append_result_char(escape_ch); break;
                        case L'a':
                            append_result_char(L'\a'); break;
                        case L'b':
                            append_result_char(L'\b'); break;
                        case L'f':
                            append_result_char(L'\f'); break;
                        case L'n':
                            append_result_char(L'\n'); break;
                        case L'r':
                            append_result_char(L'\r'); break;
                        case L't':
                            append_result_char(L'\t'); break;
                        case L'v':
                            append_result_char(L'\v'); break;
                        case L'0': case L'1': case L'2': case L'3': case L'4':
                        case L'5': case L'6': case L'7': case L'8': case L'9':
                        {
                            // oct 1byte 
                            int oct_ascii = escape_ch - L'0';
                            for (int i = 0; i < 2; i++)
                            {
                                if (lexer::lex_isodigit(peek_char()))
                                {
                                    oct_ascii *= 8;
                                    oct_ascii += lexer::lex_hextonum(read_char());
                                }
                                else
                                    break;
                            }
                            append_result_char(oct_ascii);
                            break;
                        }
                        case L'X':
                        case L'x':
                        {
                            // hex 1byte 
                            int hex_ascii = 0;
                            for (int i = 0; i < 2; i++)
                            {
                                if (lexer::lex_isxdigit(peek_char()))
                                {
                                    hex_ascii *= 16;
                                    hex_ascii += lexer::lex_hextonum(read_char());
                                }
                                else if (i == 0)
                                    goto str_escape_sequences_fail_in_format_begin;
                                else
                                    break;
                            }
                            append_result_char(hex_ascii);
                            break;
                        }
                        case L'U':
                        case L'u':
                        {
                            // hex 1byte 
                            int hex_ascii = 0;
                            for (int i = 0; i < 4; i++)
                            {
                                if (lexer::lex_isxdigit(peek_char()))
                                {
                                    hex_ascii *= 16;
                                    hex_ascii += lexer::lex_hextonum(read_char());
                                }
                                else if (i == 0)
                                    goto str_escape_sequences_fail_in_format_begin;
                                else
                                    break;
                            }
                            append_result_char(hex_ascii);
                            break;
                        }
                        default:
                        str_escape_sequences_fail_in_format_begin:
                            return produce_lexer_error(msglevel_t::error, WO_ERR_UNKNOW_ESCSEQ_BEGIN_WITH_CH, escape_ch);
                        }
                    }
                    else
                        append_result_char(following_ch);
                }
                else
                    return produce_lexer_error(msglevel_t::error, WO_ERR_UNEXCEPTED_EOL_IN_STRING);
            }

            // Cannot be here.
            wo_error("Cannot be here.");
        }
        else if (lexer::lex_isdigit(readed_char))
        {
            append_result_char(readed_char);

            int base = 10;
            bool is_real = false;
            bool is_handle = false;

            // is digit, return l_literal_integer/l_literal_handle/l_literal_real
            if (readed_char == L'0')
            {
                // it may be OCT DEC HEX
                int sec_ch = peek_char();
                if (lexer::lex_toupper(sec_ch) == 'X')
                    base = 16;                      // is hex
                else if (lexer::lex_toupper(sec_ch) == 'B')
                    base = 2;                      // is bin
                else
                    base = 8;                       // is oct or 0
            }

            int following_chs;
            do
            {
                following_chs = peek_char();

                if (following_chs == '_' || following_chs == '\'')
                {
                    // this behavior is learn from rust and c++
                    // You can freely use the _/' mark in the number literal, except for the leading mark (0 0x 0b) 
                    (void)read_char();
                    continue;
                }

                if (base == 10)
                {
                    if (lexer::lex_isdigit(following_chs))
                        append_result_char(read_char());
                    else if (following_chs == L'.')
                    {
                        append_result_char(read_char());
                        if (is_real)
                            return produce_lexer_error(msglevel_t::error, WO_ERR_UNEXCEPT_CH_AFTER_CH, following_chs, readed_char);
                        is_real = true;
                    }
                    else if (lexer::lex_toupper(following_chs) == L'H')
                    {
                        if (is_real)
                            return produce_lexer_error(msglevel_t::error, WO_ERR_UNEXCEPT_CH_AFTER_CH, following_chs, readed_char);
                        (void)read_char();
                        is_handle = true;
                        break;
                    }
                    else if (lexer::lex_isalnum(following_chs))
                        return produce_lexer_error(msglevel_t::error, WO_ERR_ILLEGAL_LITERAL);
                    else
                        break;                  // end read
                }
                else if (base == 16)
                {
                    if (lexer::lex_isxdigit(following_chs) || lexer::lex_toupper(following_chs) == L'X')
                        append_result_char(read_char());
                    else if (following_chs == L'.')
                        return produce_lexer_error(msglevel_t::error, WO_ERR_UNEXCEPT_CH_AFTER_CH, following_chs, readed_char);
                    else if (lexer::lex_toupper(following_chs) == L'H')
                    {
                        (void)read_char();
                        is_handle = true;
                        break;
                    }
                    else if (lexer::lex_isalnum(following_chs))
                        return produce_lexer_error(msglevel_t::error, WO_ERR_ILLEGAL_LITERAL);
                    else
                        break;                  // end read
                }
                else if (base == 8)
                {
                    if (lexer::lex_isodigit(following_chs))
                        append_result_char(read_char());
                    else if (following_chs == L'.')
                        return produce_lexer_error(msglevel_t::error, WO_ERR_UNEXCEPT_CH_AFTER_CH, following_chs, readed_char);
                    else if (lexer::lex_toupper(following_chs) == L'H')
                    {
                        (void)read_char();
                        is_handle = true;
                        break;
                    }
                    else if (lexer::lex_isalnum(following_chs))
                        return produce_lexer_error(msglevel_t::error, WO_ERR_ILLEGAL_LITERAL);
                    else
                        break;                  // end read
                }
                else if (base == 2)
                {
                    if (following_chs == L'1' || following_chs == L'0' || lexer::lex_toupper(following_chs) == L'B')
                        append_result_char(read_char());
                    else if (following_chs == L'.')
                        return produce_lexer_error(msglevel_t::error, WO_ERR_UNEXCEPT_CH_AFTER_CH, following_chs, readed_char);
                    else if (lexer::lex_toupper(following_chs) == L'H')
                    {
                        (void)read_char();
                        is_handle = true;
                    }
                    else if (lexer::lex_isalnum(following_chs))
                        return produce_lexer_error(msglevel_t::error, WO_ERR_ILLEGAL_LITERAL);
                    else
                        break;                  // end read
                }
                else
                    return produce_lexer_error(msglevel_t::error, WO_ERR_LEXER_ERR_UNKNOW_NUM_BASE);

            } while (true);

            // end reading, decide which type to return;
            wo_assert(!(is_real && is_handle));

            if (is_real)
                return produce_token(lex_type::l_literal_real, std::move(token_literal_result));
            if (is_handle)
                return produce_token(lex_type::l_literal_handle, std::move(token_literal_result));
            return produce_token(lex_type::l_literal_integer, std::move(token_literal_result));


        } // l_literal_integer/l_literal_handle/l_literal_real end
        else if (readed_char == L';')
        {
            append_result_char(readed_char);
            return produce_token(lex_type::l_semicolon, std::move(token_literal_result));
        }
        else if (readed_char == L'@')
        {
            // @"(Example string "without" '\' it will be very happy!)"

            if (int tmp_ch = peek_char(); tmp_ch == L'"')
            {
                (void)read_char();

                int following_ch;
                while (true)
                {
                    following_ch = read_char();
                    if (following_ch == L'"' && peek_char() == L'@')
                    {
                        (void)read_char();
                        return produce_token(lex_type::l_literal_string, std::move(token_literal_result));
                    }

                    if (following_ch != EOF)
                        append_result_char(following_ch);
                    else
                        return produce_lexer_error(msglevel_t::error, WO_ERR_UNEXCEPT_EOF);
                }
            }
            else
                goto checking_valid_operator;
        }
        else if (readed_char == L'"')
        {
            int following_ch;
            while (true)
            {
                following_ch = read_char();
                if (following_ch == L'"')
                    return produce_token(lex_type::l_literal_string, std::move(token_literal_result));
                if (following_ch != EOF && following_ch != '\n')
                {
                    if (following_ch == L'\\')
                    {
                        // Escape character 
                        int escape_ch = read_char();
                        switch (escape_ch)
                        {
                        case L'\'':
                        case L'"':
                        case L'?':
                        case L'\\':
                            append_result_char(escape_ch); break;
                        case L'a':
                            append_result_char(L'\a'); break;
                        case L'b':
                            append_result_char(L'\b'); break;
                        case L'f':
                            append_result_char(L'\f'); break;
                        case L'n':
                            append_result_char(L'\n'); break;
                        case L'r':
                            append_result_char(L'\r'); break;
                        case L't':
                            append_result_char(L'\t'); break;
                        case L'v':
                            append_result_char(L'\v'); break;
                        case L'0': case L'1': case L'2': case L'3': case L'4':
                        case L'5': case L'6': case L'7': case L'8': case L'9':
                        {
                            // oct 1byte 
                            int oct_ascii = escape_ch - L'0';
                            for (int i = 0; i < 2; i++)
                            {
                                if (lexer::lex_isodigit(peek_char()))
                                {
                                    oct_ascii *= 8;
                                    oct_ascii += lexer::lex_hextonum(read_char());
                                }
                                else
                                    break;
                            }
                            append_result_char(oct_ascii);
                            break;
                        }
                        case L'X':
                        case L'x':
                        {
                            // hex 1byte 
                            int hex_ascii = 0;
                            for (int i = 0; i < 2; i++)
                            {
                                if (lexer::lex_isxdigit(peek_char()))
                                {
                                    hex_ascii *= 16;
                                    hex_ascii += lexer::lex_hextonum(read_char());
                                }
                                else if (i == 0)
                                    goto str_escape_sequences_fail;
                                else
                                    break;
                            }
                            append_result_char(hex_ascii);
                            break;
                        }
                        case L'U':
                        case L'u':
                        {
                            // hex 1byte 
                            int hex_ascii = 0;
                            for (int i = 0; i < 4; i++)
                            {
                                if (lexer::lex_isxdigit(peek_char()))
                                {
                                    hex_ascii *= 16;
                                    hex_ascii += lexer::lex_hextonum(read_char());
                                }
                                else if (i == 0)
                                    goto str_escape_sequences_fail;
                                else
                                    break;
                            }
                            append_result_char(hex_ascii);
                            break;
                        }
                        default:
                        str_escape_sequences_fail:
                            return produce_lexer_error(msglevel_t::error, WO_ERR_UNKNOW_ESCSEQ_BEGIN_WITH_CH, escape_ch);
                        }
                    }
                    else
                        append_result_char(following_ch);
                }
                else
                    return produce_lexer_error(msglevel_t::error, WO_ERR_UNEXCEPTED_EOL_IN_STRING);
            }
        }
        else if (readed_char == L'\'')
        {
            int following_ch;

            following_ch = read_char();
            if (following_ch == L'\'')
                return produce_lexer_error(msglevel_t::error, WO_ERR_UNEXCEPT_CH_AFTER_CH, L'\'', L'\'');
            else if (following_ch != EOF && following_ch != '\n')
            {
                if (following_ch == L'\\')
                {
                    // Escape character 
                    int escape_ch = read_char();
                    switch (escape_ch)
                    {
                    case L'\'':
                    case L'"':
                    case L'?':
                    case L'\\':
                        append_result_char(escape_ch); break;
                    case L'a':
                        append_result_char(L'\a'); break;
                    case L'b':
                        append_result_char(L'\b'); break;
                    case L'f':
                        append_result_char(L'\f'); break;
                    case L'n':
                        append_result_char(L'\n'); break;
                    case L'r':
                        append_result_char(L'\r'); break;
                    case L't':
                        append_result_char(L'\t'); break;
                    case L'v':
                        append_result_char(L'\v'); break;
                    case L'0': case L'1': case L'2': case L'3': case L'4':
                    case L'5': case L'6': case L'7': case L'8': case L'9':
                    {
                        // oct 1byte 
                        int oct_ascii = escape_ch - L'0';
                        for (int i = 0; i < 2; i++)
                        {
                            if (lexer::lex_isodigit(peek_char()))
                            {
                                oct_ascii *= 8;
                                oct_ascii += lexer::lex_hextonum(read_char());
                            }
                            else
                                break;
                        }
                        append_result_char(oct_ascii);
                        break;
                    }
                    case L'X':
                    case L'x':
                    {
                        // hex 1byte 
                        int hex_ascii = 0;
                        for (int i = 0; i < 2; i++)
                        {
                            if (lexer::lex_isxdigit(peek_char()))
                            {
                                hex_ascii *= 16;
                                hex_ascii += lexer::lex_hextonum(read_char());
                            }
                            else if (i == 0)
                                goto char_escape_sequences_fail;
                            else
                                break;
                        }
                        append_result_char(hex_ascii);
                        break;
                    }
                    case L'U':
                    case L'u':
                    {
                        // hex 1byte 
                        int hex_ascii = 0;
                        for (int i = 0; i < 4; i++)
                        {
                            if (lexer::lex_isxdigit(peek_char()))
                            {
                                hex_ascii *= 16;
                                hex_ascii += lexer::lex_hextonum(read_char());
                            }
                            else if (i == 0)
                                goto char_escape_sequences_fail;
                            else
                                break;
                        }
                        append_result_char(hex_ascii);
                        break;
                    }
                    default:
                    char_escape_sequences_fail:
                        return produce_lexer_error(msglevel_t::error, WO_ERR_UNKNOW_ESCSEQ_BEGIN_WITH_CH, escape_ch);
                    }
                }
                else
                    append_result_char(following_ch);
            }
            else
                return produce_lexer_error(msglevel_t::error, WO_ERR_UNEXCEPTED_EOL_IN_CHAR);

            following_ch = read_char();
            if (following_ch == L'\'')
                return produce_token(lex_type::l_literal_char, std::move(token_literal_result));
            else
                return produce_lexer_error(msglevel_t::error, WO_ERR_LEXER_ERR_UNKNOW_BEGIN_CH L" " WO_TERM_EXCEPTED L" '\''", following_ch);
        }
        else if (lexer::lex_isoperatorch(readed_char))
        {
        checking_valid_operator:
            append_result_char(readed_char);
            lex_type operator_type = lexer::lex_is_valid_operator(token_literal_result);

            int following_ch;
            do
            {
                following_ch = peek_char();

                if (!lexer::lex_isoperatorch(following_ch))
                    break;

                lex_type tmp_op_type = lexer::lex_is_valid_operator(token_literal_result + (wchar_t)following_ch);
                if (tmp_op_type != lex_type::l_error)
                {
                    // maxim eat!
                    operator_type = tmp_op_type;
                    append_result_char(read_char());
                }
                else if (operator_type == lex_type::l_error)
                {
                    // not valid yet, continue...
                }
                else // is already a operator ready, return it.
                    break;

            } while (true);

            if (operator_type == lex_type::l_error)
                return produce_lexer_error(msglevel_t::error, WO_ERR_UNKNOW_OPERATOR_STR, token_literal_result.c_str());

            return produce_token(operator_type, std::move(token_literal_result));
        }
        else if (readed_char == L'(')
        {
            append_result_char(readed_char);

            return produce_token(lex_type::l_left_brackets, std::move(token_literal_result));
        }
        else if (readed_char == L')')
        {
            append_result_char(readed_char);

            return produce_token(lex_type::l_right_brackets, std::move(token_literal_result));
        }
        else if (readed_char == L'{')
        {
            append_result_char(readed_char);
            ++_m_curry_count_in_format_string;
            return produce_token(lex_type::l_left_curly_braces, std::move(token_literal_result));
        }
        else if (readed_char == L'}')
        {
            append_result_char(readed_char);
            --_m_curry_count_in_format_string;
            return produce_token(lex_type::l_right_curly_braces, std::move(token_literal_result));
        }
        // ATTENTION, SECURE:
        //  Disable pragma if source_file == nullptr, it's in deserialize.
        //  Processing pragma here may lead to arbitrary code execution.
        else if (readed_char == L'#' && m_source_path.has_value())
        {
            // Read sharp
            // #macro
            std::wstring pragma_name;

            if (lexer::lex_isidentbeg(peek_char()))
            {
                pragma_name += (wchar_t)read_char();
                while (lexer::lex_isident(peek_char()))
                    pragma_name += (wchar_t)read_char();
            }

            if (pragma_name == L"macro")
            {
                // OK FINISH PRAGMA, CONTINUE
                /*auto macro_instance = std::make_unique<macro>(*this);
                auto macro_name = macro_instance->macro_name;
                if (auto fnd = m_declared_macro_list->insert(
                    std::make_pair(macro_name, std::move(macro_instance)));
                    fnd.second)
                {
                    produce_lexer_error(
                        msglevel_t::error, WO_ERR_UNKNOWN_REPEAT_MACRO_DEFINE, macro_name.c_str());

                    wchar_t describe[256] = {};
                    swprintf(describe, 255, WO_INFO_SYMBOL_NAMED_DEFINED_HERE, macro_name.c_str());
                    record_message(
                        compiler_message_t{
                            msglevel_t::infom,
                            { fnd.first->second->begin_row, fnd.first->second->begin_col },
                            { fnd.first->second->end_row, fnd.first->second->end_col },
                            *fnd.first->second->filename,
                            describe,
                        });
                }
                return;*/
            }
            else
            {
                return produce_lexer_error(
                    msglevel_t::error, WO_ERR_UNKNOWN_PRAGMA_COMMAND, pragma_name.c_str());
            }
            return;
        }
        else if (readed_char == EOF)
        {
            return produce_token(lex_type::l_eof, std::move(token_literal_result));
        }
        else if (lexer::lex_isidentbeg(readed_char))
        {
            // l_identifier or key world..
            append_result_char(readed_char);

            int following_ch;
            while (true)
            {
                following_ch = peek_char();
                if (lexer::lex_isident(following_ch))
                    append_result_char(read_char());
                else
                    break;
            }

            // ATTENTION, SECURE:
            //  Disable macro handler if source_file == nullptr, it's in deserialize.
            //  Processing macros here may lead to arbitrary code execution.
            if (peek_char() == L'!' && m_source_path != nullptr)
            {
                (void)read_char(); // Eat `!`
                //try_handle_macro(
                //    out_literal, 
                //    lex_type::l_macro, 
                //    token_literal_result, 
                //    false), 
                //    std::move(token_literal_result);
                return;
            }

            if (lex_type keyword_type = lexer::lex_is_keyword(token_literal_result);
                lex_type::l_error != keyword_type)
                return produce_token(keyword_type, std::move(token_literal_result));

            return produce_token(lex_type::l_identifier, std::move(token_literal_result));
        }
        ///////////////////////////////////////////////////////////////////////////////////////
        else
        {
            append_result_char(readed_char);
            return produce_token(lex_type::l_unknown_token, std::move(token_literal_result));
        }
    }
}