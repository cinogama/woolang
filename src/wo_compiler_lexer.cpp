#include "wo_compiler_ir.hpp"

// Function from other source.
std::string _wo_dump_lexer_context_error(wo::lexer* lex, wo_inform_style_t style);

namespace wo
{
    const std::unordered_map<std::wstring, lex_type> lexer::_lex_operator_list =
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
    const std::unordered_map<std::wstring, lex_type> lexer::_key_word_list =
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

    macro::macro(lexer& lex)
        : _macro_action_vm(nullptr)
        , filename(lex.m_source_path.value())
    {
        /*
        #macro PRINT_HELLOWORLD
        {
            return @"std::println("Helloworld")"@;
        }

        PRINT_HELLOWORLD!;
        */

        auto macro_name_info = lex.peek();
        macro_name = macro_name_info->m_token_text;
        begin_row = macro_name_info->m_token_begin[0];
        begin_col = macro_name_info->m_token_begin[1];
        end_col = macro_name_info->m_token_end[1];
        end_row = macro_name_info->m_token_end[0];

        lex.move_forward();

        size_t scope_count = 1;
        if (lex.peek()->m_lex_type == lex_type::l_left_curly_braces)
        {
            std::streampos macro_begin_place = lex.m_source_stream->tellg();

            lex.move_forward();

            std::wstring macro_anylzing_src =
                L"import woo::macro; extern func macro_" +
                macro_name + L"(lexer:std::lexer)=> string { do lexer;\n{";
            ;

            bool meet_eof = false;
            do
            {
                auto* readed_token = lex.peek();

                if (readed_token->m_lex_type == lex_type::l_right_curly_braces)
                    scope_count--;
                else if (readed_token->m_lex_type == lex_type::l_left_curly_braces)
                    scope_count++;
                else if (readed_token->m_lex_type == lex_type::l_eof)
                {
                    scope_count = 0;
                    meet_eof = true;
                }
                lex.consume_forward();

            } while (scope_count);

            if (meet_eof)
                lex.produce_lexer_error(lexer::msglevel_t::error, WO_ERR_UNEXCEPT_EOF);
            else
            {
                std::streampos macro_end_place = lex.m_source_stream->tellg();
                lex.m_source_stream->seekg(macro_begin_place);

                std::vector<wchar_t> macro_content;

                while (lex.m_source_stream->eof() == false
                    && lex.m_source_stream
                    && lex.m_source_stream->tellg() < macro_end_place)
                {
                    wchar_t ch;
                    lex.m_source_stream->read(&ch, 1);
                    macro_content.push_back(ch);
                }
                macro_content.push_back(L'\0');

                _macro_action_vm = wo_create_vm();
                if (!wo_load_source(_macro_action_vm,
                    (wstr_to_str(lex.m_source_path.value()->c_str()) + " : macro_" + wstr_to_str(macro_name) + ".wo").c_str(),
                    wstr_to_str(macro_anylzing_src + macro_content.data() + L"\nreturn \"\";}").c_str()))
                {
                    lex.produce_lexer_error(lexer::msglevel_t::error, WO_ERR_FAILED_TO_COMPILE_MACRO_CONTROLOR);
                    lex.get_current_error_frame().back().m_describe +=
                        str_to_wstr(wo_get_compile_error(_macro_action_vm, WO_NOTHING)) + WO_MACRO_CODE_END_HERE;

                    wo_close_vm(_macro_action_vm);
                    _macro_action_vm = nullptr;
                }
                else
                {
                    // Donot jit to make debug friendly.
                    if (nullptr == wo_run(_macro_action_vm))
                    {
                        lex.produce_lexer_error(lexer::msglevel_t::error, WO_ERR_FAILED_TO_RUN_MACRO_CONTROLOR,
                            macro_name.c_str(),
                            wo::str_to_wstr(wo_get_runtime_error(_macro_action_vm)).c_str());
                    }
                }
            }
        }
        else
            lex.produce_lexer_error(lexer::msglevel_t::error, WO_ERR_HERE_SHOULD_HAVE, L"{");
    }

    std::wstring lexer::compiler_message_t::to_wstring(bool need_ansi_describe)
    {
        using namespace std;

        if (need_ansi_describe)
            return (
                m_level == msglevel_t::error
                ? (ANSI_HIR L"error" ANSI_RST)
                : (ANSI_HIC L"infom" ANSI_RST)
                )
            + (L" (" + std::to_wstring(m_range_end[0] + 1) + L"," + std::to_wstring(m_range_end[1]))
            + (L") " + m_describe);
        else
            return (
                m_level == msglevel_t::error
                ? (L"error")
                : (L"info")
                )
            + (L" (" + std::to_wstring(m_range_end[0] + 1) + L"," + std::to_wstring(m_range_end[1]))
            + (L") " + m_describe);
    }

    ///////////////////////////////////////////////////

    const wchar_t* lexer::lex_is_operate_type(lex_type tt)
    {
        for (auto& [op_str, op_type] : _lex_operator_list)
        {
            if (op_type == tt)
                return op_str.c_str();
        }
        return nullptr;
    }
    const wchar_t* lexer::lex_is_keyword_type(lex_type tt)
    {
        for (auto& [op_str, op_type] : _key_word_list)
        {
            if (op_type == tt)
                return op_str.c_str();
        }
        return nullptr;
    }
    lex_type lexer::lex_is_valid_operator(const std::wstring& op)
    {
        if (auto fnd = _lex_operator_list.find(op); fnd != _lex_operator_list.end())
            return fnd->second;

        return lex_type::l_error;
    }
    lex_type lexer::lex_is_keyword(const std::wstring& op)
    {
        if (auto fnd = _key_word_list.find(op); fnd != _key_word_list.end())
            return fnd->second;

        return lex_type::l_error;
    }
    bool lexer::lex_isoperatorch(int ch)
    {
        const static std::unordered_set<wchar_t> operator_char_set = []() {
            std::unordered_set<wchar_t> _result;
            for (auto& [_operator, operator_info] : _lex_operator_list)
                for (wchar_t wch : _operator)
                    _result.insert(wch);
            return _result;
        }();
        return operator_char_set.find((wchar_t)ch) != operator_char_set.end();
    }
    bool lexer::lex_isspace(int ch)
    {
        if (ch == EOF)
            return false;

        return ch == 0 || iswspace((wchar_t)ch);
    }
    bool lexer::lex_isalpha(int ch)
    {
        // if ch can used as begin of identifier, return true
        if (ch == EOF)
            return false;

        // according to ISO-30112, Chinese character is belonging alpha-set,
        // so we can use 'iswalpha' directly.
        return iswalpha((wchar_t)ch);
    }
    bool lexer::lex_isidentbeg(int ch)
    {
        // if ch can used as begin of identifier, return true
        if (ch == EOF)
            return false;

        // according to ISO-30112, Chinese character is belonging alpha-set,
        // so we can use 'iswalpha' directly.
        return iswalpha((wchar_t)ch) || (wchar_t)ch == L'_';
    }
    bool lexer::lex_isident(int ch)
    {
        // if ch can used as begin of identifier, return true
        if (ch == EOF)
            return false;

        // according to ISO-30112, Chinese character is belonging alpha-set,
        // so we can use 'iswalpha' directly.
        return iswalnum((wchar_t)ch) || (wchar_t)ch == L'_';
    }
    bool lexer::lex_isalnum(int ch)
    {
        // if ch can used in identifier (not first character), return true
        if (ch == EOF)
            return false;

        // according to ISO-30112, Chinese character is belonging alpha-set,
        // so we can use 'iswalpha' directly.
        return iswalnum((wchar_t)ch);
    }
    bool lexer::lex_isdigit(int ch)
    {
        if (ch == EOF)
            return false;

        return iswdigit((wchar_t)ch);
    }
    bool lexer::lex_isxdigit(int ch)
    {
        if (ch == EOF)
            return false;

        return iswxdigit((wchar_t)ch);
    }
    bool lexer::lex_isodigit(int ch)
    {
        if (ch == EOF)
            return false;

        return (wchar_t)ch >= L'0' && (wchar_t)ch <= L'7';
    }
    int lexer::lex_toupper(int ch)
    {
        if (ch == EOF)
            return EOF;

        return towupper((wchar_t)ch);
    }
    int lexer::lex_tolower(int ch)
    {
        if (ch == EOF)
            return EOF;

        return towlower((wchar_t)ch);
    }
    int lexer::lex_hextonum(int ch)
    {
        wo_assert(lex_isxdigit(ch));

        if (iswdigit((wchar_t)ch))
        {
            return (wchar_t)ch - L'0';
        }
        return towupper((wchar_t)ch) - L'A' + 10;
    }
    int lexer::lex_octtonum(int ch)
    {
        wo_assert(lex_isodigit(ch));

        return (wchar_t)ch - L'0';
    }

    lexer::lexer(
        std::optional<lexer*> who_import_me,
        const std::optional<wo_pstring_t>& source_path,
        std::optional<std::unique_ptr<std::wistream>>&& source_stream)
        : m_who_import_me(who_import_me)
        , m_source_path(source_path)
        , m_source_stream{}
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
            : std::make_shared<imported_source_path_set_t>(
                source_path.has_value()
                ? imported_source_path_set_t{ source_path.value() }
                : imported_source_path_set_t{}))
        , m_imported_ast_tree_list{}
        //
        , _m_peeked_tokens{}
        , _m_row_counter(0)
        , _m_col_counter(0)
        , _m_this_token_pre_begin_row(0)
        , _m_this_token_pre_begin_col(0)
        , _m_this_token_begin_row(0)
        , _m_this_token_begin_col(0)
        , _m_in_format_string(false)
        , _m_curry_count_in_format_string(0)
    {
        // Make sure error frame has one frame.
        (void)m_error_frame.emplace_back();

        if (source_stream)
            m_source_stream = std::move(source_stream.value());
        else
        {
            wo_assert(source_path.has_value());
            (void)record_parser_error(
                lexer::msglevel_t::error,
                WO_ERR_CANNOT_OPEN_FILE,
                source_path.value()->c_str());
        }
    }
    lexer::compiler_message_list_t& lexer::get_current_error_frame()
    {
        return m_error_frame.back();
    }
    lexer::compiler_message_list_t& lexer::get_root_error_frame()
    {
        return m_error_frame.front();
    }
    bool lexer::check_source_path_has_been_imported(wo_pstring_t full_path)
    {
        return !m_imported_source_path_set->insert(full_path).second;
    }
    void lexer::import_ast_tree(ast::AstBase* astbase)
    {
        m_imported_ast_tree_list.push_back(astbase);
    }
    void lexer::merge_lexer_or_parser_error_from_import(lexer& abnother_lexer)
    {
        wo_assert(m_error_frame.size() == 1 && abnother_lexer.m_error_frame.size() == 1);
        m_error_frame.back().insert(
            m_error_frame.back().end(),
            abnother_lexer.m_error_frame.back().begin(),
            abnother_lexer.m_error_frame.back().end());
    }
    int lexer::peek_char()
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
    int lexer::read_char()
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

    void lexer::record_message(compiler_message_t&& message)
    {
        auto& emplaced_message = m_error_frame.back().emplace_back(std::move(message));

        emplaced_message.m_layer
            = m_error_frame.size() - 1 + (
                emplaced_message.m_level == msglevel_t::error ? 0 : 1);
    }
    void lexer::append_message(const compiler_message_t& message)
    {
        auto& emplaced_message = m_error_frame.back().emplace_back(message);

        emplaced_message.m_layer
            = m_error_frame.size() - 1 + (
                emplaced_message.m_level == msglevel_t::error ? 0 : 1);
    }

    void lexer::produce_token(lex_type type, std::wstring&& moved_token_text)
    {
        _m_peeked_tokens.emplace(
            peeked_token_t
            {
                type,
                std::move(moved_token_text),
                {
                    _m_this_token_begin_row,
                    _m_this_token_begin_col,
                    _m_this_token_pre_begin_row,
                    _m_this_token_pre_begin_col
                },
                { _m_row_counter, _m_col_counter },
            });
    }
    void lexer::token_pre_begin_here()
    {
        _m_this_token_pre_begin_row = _m_row_counter;
        _m_this_token_pre_begin_col = _m_col_counter;
    }
    void lexer::token_begin_here()
    {
        _m_this_token_begin_row = _m_row_counter;
        _m_this_token_begin_col = _m_col_counter;
    }

    const lexer::peeked_token_t* lexer::peek()
    {
        for (;;)
        {
            while (_m_peeked_tokens.empty())
                move_forward();

            wo_assert(!_m_peeked_tokens.empty());
            const lexer::peeked_token_t* peeked_token = &_m_peeked_tokens.front();
            if (peeked_token->m_lex_type == lex_type::l_macro)
            {
                // We need to expand this macro here.
                std::wstring macro_name = peeked_token->m_token_text;
                _m_peeked_tokens.pop();

                if (!try_handle_macro(macro_name))
                {
                    wo_assert(!_m_peeked_tokens.empty());
                    return &_m_peeked_tokens.front();
                }
                continue;
            }
            return peeked_token;
        }
    }
    bool lexer::has_error() const
    {
        wo_assert(m_error_frame.size() == 1);
        return !m_error_frame.back().empty();
    }
    wo_pstring_t lexer::get_source_path() const
    {
        return m_source_path.value();
    }
    size_t lexer::get_error_frame_count_for_debug() const
    {
        return m_error_frame.size();
    }
    const std::optional<lexer*>& lexer::get_who_import_me() const
    {
        return m_who_import_me;
    }
    void lexer::get_now_location(size_t* out_row, size_t* out_col) const
    {
        *out_row = _m_row_counter;
        *out_col = _m_col_counter;
    }
    const lexer::declared_macro_map_t& lexer::get_declared_macro_list_for_debug() const
    {
        return *m_declared_macro_list;
    }
    void lexer::begin_trying_block()
    {
        m_error_frame.emplace_back();
    }
    void lexer::end_trying_block()
    {
        m_error_frame.pop_back();
    }
    void lexer::consume_forward()
    {
        if (!_m_peeked_tokens.empty())
            _m_peeked_tokens.pop();
    }
    void lexer::move_forward()
    {
        if (size_t count = _m_peeked_tokens.size(); count > 0)
        {
            _m_peeked_tokens.pop();
            if (count > 1)
                return;
        }

        int readed_char;
        token_pre_begin_here();

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

        std::wstring token_literal_result;
        auto append_result_char =
            [&token_literal_result](auto ch) {token_literal_result.push_back(static_cast<wchar_t>(ch)); };

        // Format string.
        bool is_format_string_begin = false, is_format_string_middle = false;
        switch (readed_char)
        {
        case L'/':
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
            else
                goto checking_valid_operator;
        }
        case L';':
        {
            append_result_char(readed_char);
            return produce_token(lex_type::l_semicolon, std::move(token_literal_result));
        }
        case L'@':
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
        case L'"':
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
        case L'\'':
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
        case L'(':
        {
            append_result_char(readed_char);
            return produce_token(lex_type::l_left_brackets, std::move(token_literal_result));
        }
        case L')':
        {
            append_result_char(readed_char);

            return produce_token(lex_type::l_right_brackets, std::move(token_literal_result));
        }
        case L'{':
        {
            append_result_char(readed_char);
            ++_m_curry_count_in_format_string;
            return produce_token(lex_type::l_left_curly_braces, std::move(token_literal_result));
        }
        case L'}':
        {
            if (_m_in_format_string && _m_curry_count_in_format_string == 0)
            {
                is_format_string_middle = true;
                break;
            }
            append_result_char(readed_char);
            --_m_curry_count_in_format_string;
            return produce_token(lex_type::l_right_curly_braces, std::move(token_literal_result));
        }
        case L'#':
        {
            // ATTENTION, SECURE:
            //  Disable pragma if source_file == nullptr, it's in deserialize.
            //  Processing pragma here may lead to arbitrary code execution.
            if (!m_source_path.has_value())
            {
                append_result_char(readed_char);
                return produce_token(lex_type::l_unknown_token, std::move(token_literal_result));
            }

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
                auto macro_instance = std::make_unique<macro>(*this);
                auto macro_name = macro_instance->macro_name;
                if (auto fnd = m_declared_macro_list->insert(
                    std::make_pair(macro_name, std::move(macro_instance)));
                    !fnd.second)
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
                return;
            }
            else
            {
                return produce_lexer_error(
                    msglevel_t::error, WO_ERR_UNKNOWN_PRAGMA_COMMAND, pragma_name.c_str());
            }
            return;
        }
        case EOF:
        {
            return produce_token(lex_type::l_eof, std::move(token_literal_result));
        }
        case 'F':
        case 'f':
            if (peek_char() == '"')
            {
                (void)read_char();

                if (_m_in_format_string)
                    return produce_lexer_error(msglevel_t::error, WO_ERR_RECURSIVE_FORMAT_STRING_IS_INVALID);

                is_format_string_begin = true;
                break;
            }
            [[fallthrough]];
        default:
            if (lexer::lex_isidentbeg(readed_char))
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
                    return produce_token(lex_type::l_macro, std::move(token_literal_result));
                }

                if (lex_type keyword_type = lexer::lex_is_keyword(token_literal_result);
                    lex_type::l_error != keyword_type)
                    return produce_token(keyword_type, std::move(token_literal_result));

                return produce_token(lex_type::l_identifier, std::move(token_literal_result));
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
                    else if (lexer::lex_isodigit(sec_ch))
                        base = 8;                       // is oct
                    else
                        base = 10;                      // is dec
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
            else
            {
                append_result_char(readed_char);
                return produce_token(lex_type::l_unknown_token, std::move(token_literal_result));
            }
        }

        wo_assert(is_format_string_begin || is_format_string_middle);
        // Is f"..."
        int following_ch;
        while (true)
        {
            following_ch = read_char();
            if (following_ch == L'"')
            {
                if (is_format_string_begin)
                    return produce_token(lex_type::l_literal_string, std::move(token_literal_result));
                else
                {
                    _m_in_format_string = false;
                    return produce_token(lex_type::l_format_string_end, std::move(token_literal_result));
                }
            }
            if (following_ch == L'{')
            {
                _m_curry_count_in_format_string = 0;
                _m_in_format_string = true;

                if (is_format_string_begin)
                    return produce_token(lex_type::l_format_string_begin, std::move(token_literal_result));
                else
                    return produce_token(lex_type::l_format_string, std::move(token_literal_result));
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
    bool lexer::try_handle_macro(const std::wstring& macro_name)
    {
        wo_assert(m_source_path.has_value());

        const size_t macro_pre_begin_row = _m_this_token_pre_begin_row;
        const size_t macro_pre_begin_col = _m_this_token_pre_begin_col;
        const size_t macro_begin_row = _m_this_token_begin_row;
        const size_t macro_begin_col = _m_this_token_begin_col;

        auto fnd = m_declared_macro_list->find(macro_name);
        if (fnd != m_declared_macro_list->end() && fnd->second->_macro_action_vm)
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
            wo_value s = wo_reserve_stack(fnd->second->_macro_action_vm, 1, nullptr);
            wo_set_pointer(s, this);
            wo_value result = wo_invoke_rsfunc(
                fnd->second->_macro_action_vm, script_func, 1, nullptr, &s);

            wo_pop_stack(fnd->second->_macro_action_vm, 1);

            if (result == nullptr)
            {
                produce_lexer_error(msglevel_t::error,
                    WO_ERR_FAILED_TO_RUN_MACRO_CONTROLOR,
                    fnd->second->macro_name.c_str(),
                    wo::str_to_wstr(wo_get_runtime_error(fnd->second->_macro_action_vm)).c_str());

                return false;
            }
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

                wo::lexer tmp_lex(
                    this, // We can generate macro define by macro result.
                    result_content_vfile,
                    std::make_unique<std::wistringstream>(macro_result_buffer));

                std::queue<peeked_token_t> origin_peeked_queue;
                origin_peeked_queue.swap(_m_peeked_tokens);

                size_t macro_end_row = _m_row_counter;
                size_t macro_end_col = _m_col_counter;
                if (!origin_peeked_queue.empty())
                {
                    const auto& first_token = origin_peeked_queue.front();
                    macro_end_row = first_token.m_token_begin[2];
                    macro_end_col = first_token.m_token_begin[3];
                }

                for (;;)
                {
                    std::wstring result;
                    auto* token_instance = tmp_lex.peek();

                    if (token_instance->m_lex_type == wo::lex_type::l_error
                        || token_instance->m_lex_type == wo::lex_type::l_eof)
                        break;

                    auto& token = _m_peeked_tokens.emplace(std::move(*token_instance));
                    token.m_token_begin[0] = macro_begin_row;
                    token.m_token_begin[1] = macro_begin_col;
                    token.m_token_begin[2] = macro_pre_begin_row;
                    token.m_token_begin[3] = macro_pre_begin_col;
                    token.m_token_end[0] = macro_end_row;
                    token.m_token_end[1] = macro_end_col;

                    tmp_lex.move_forward();
                }

                while (!origin_peeked_queue.empty())
                {
                    _m_peeked_tokens.emplace(std::move(origin_peeked_queue.front()));
                    origin_peeked_queue.pop();
                }

                if (tmp_lex.has_error())
                {
                    produce_lexer_error(msglevel_t::error, WO_ERR_INVALID_TOKEN_MACRO_CONTROLOR,
                        fnd->second->macro_name.c_str());

                    get_current_error_frame().back().m_describe +=
                        str_to_wstr(_wo_dump_lexer_context_error(&tmp_lex, WO_NOTHING)) + WO_MACRO_ANALYZE_END_HERE;

                    return false;
                }
                wo_assure(WO_TRUE == wo_remove_virtual_file(wo::wstr_to_str(*result_content_vfile).c_str()));
            }

            return true;
        }
        else
        {
            // Unfound macro.
            produce_token(lex_type::l_macro, std::wstring(macro_name));
            return false;
        }
    }
}