#include "wo_afx.hpp"

wo_bool_t _wo_load_source(
    wo_vm vm,
    wo_string_t virtual_src_path,
    const void* src,
    size_t len,
    const std::optional<wo::lexer*>& parent_lexer);

namespace wo
{
    const std::unordered_map<std::string, lex_type> lexer::_lex_operator_list =
    {
        {"+",      {lex_type::l_add}},
        {"-",      {lex_type::l_sub}},
        {"*",      {lex_type::l_mul}},
        {"/",      {lex_type::l_div}},
        {"%",      {lex_type::l_mod}},
        {"=",      {lex_type::l_assign}},
        {"+=",     {lex_type::l_add_assign}},
        {"-=",     {lex_type::l_sub_assign}},
        {"*=",     {lex_type::l_mul_assign}},
        {"/=",     {lex_type::l_div_assign}},
        {"%=",     {lex_type::l_mod_assign}},
        {":=",     {lex_type::l_value_assign}},
        {"+:=",    {lex_type::l_value_add_assign}},
        {"-:=",    {lex_type::l_value_sub_assign}},
        {"*:=",    {lex_type::l_value_mul_assign}},
        {"/:=",    {lex_type::l_value_div_assign}},
        {"%:=",    {lex_type::l_value_mod_assign}},
        {"+:",     {lex_type::l_value_add_assign}},
        {"-:",     {lex_type::l_value_sub_assign}},
        {"*:",     {lex_type::l_value_mul_assign}},
        {"/:",     {lex_type::l_value_div_assign}},
        {"%:",     {lex_type::l_value_mod_assign}},
        {"==",     {lex_type::l_equal}},                // ==
        {"!=",     {lex_type::l_not_equal}},            // !=
        {">=",     {lex_type::l_larg_or_equal}},        // >=
        {"<=",     {lex_type::l_less_or_equal}},        // <=
        {"<",      {lex_type::l_less}},                 // <
        {">",      {lex_type::l_larg}},                 // >
        {"&&",     {lex_type::l_land}},                 // &&
        {"||",     {lex_type::l_lor}},                  // ||
        {"|",      {lex_type::l_or}},                  // ||
        {"!",      {lex_type::l_lnot}},                  // !=
        {"::",     {lex_type::l_scopeing}},
        {":<",     {lex_type::l_template_using_begin}},
        {",",      {lex_type::l_comma}},
        {":",      {lex_type::l_typecast}},
        {".",      {lex_type::l_index_point}},
        {"..",     {lex_type::l_double_index_point}},
        {"...",    {lex_type::l_variadic_sign}},
        {"[",      {lex_type::l_index_begin}},
        {"]",      {lex_type::l_index_end}},
        {"->",     {lex_type::l_direct }},
        {"|>",     {lex_type::l_direct }},
        {"<|",     {lex_type::l_inv_direct}},
        {"=>",     {lex_type::l_function_result }},
        {"=>>",    {lex_type::l_bind_monad }},
        {"->>",    {lex_type::l_map_monad }},
        {"@",      {lex_type::l_at }},
        {"?",      {lex_type::l_question }},
        {"\\",     {lex_type::l_lambda}},
    };
    const std::unordered_map<std::string, lex_type> lexer::_key_word_list =
    {
        {"alias", {lex_type::l_alias} },
        {"as", {lex_type::l_as}},
        {"break", {lex_type::l_break}},
        {"continue", {lex_type::l_continue}},
        {"do", {lex_type::l_do}},
        {"else", {lex_type::l_else}},
        {"enum", {lex_type::l_enum}},
        {"export", {lex_type::l_export}},
        {"extern", {lex_type::l_extern}},
        {"false", {lex_type::l_false}},
        {"for", {lex_type::l_for}},
        {"func", {lex_type::l_func}},
        {"if", {lex_type::l_if}},
        {"immut", {lex_type::l_immut}},
        {"import", {lex_type::l_import}},
        {"is", {lex_type::l_is}},
        {"let", {lex_type::l_let }},
        {"match", {lex_type::l_match}},
        {"mut", {lex_type::l_mut}},
        {"namespace", {lex_type::l_namespace}},
        {"nil", {lex_type::l_nil}},
        {"operator", {lex_type::l_operator}},
        {"private", {lex_type::l_private}},
        {"protected", {lex_type::l_protected}},
        {"public", {lex_type::l_public}},
        {"return", {lex_type::l_return}},
        {"static", {lex_type::l_static}},
        {"struct", {lex_type::l_struct}},
        {"true", {lex_type::l_true}},
        {"typeid", {lex_type::l_typeid}},
        {"typeof", {lex_type::l_typeof}},
        {"union", {lex_type::l_union}},
        {"using", {lex_type::l_using} },
        {"where", {lex_type::l_where}},
        {"while", {lex_type::l_while}},
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
        auto* macro_name_info = lex.peek();
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

            lex.consume_forward();

            auto source_path = *lex.m_source_path.value();
            std::string line_mark = "#line "
                + wo::u8enstring(source_path.data(), source_path.size(), false)
                + " "
                + std::to_string(lex._m_row_counter + 1)
                + " "
                + std::to_string(lex._m_col_counter - 1);

            std::string macro_anylzing_src = line_mark + R"(
import woo::std;
import woo::macro;
extern func macro_entry(lexer: std::lexer)=> string
{
    do lexer;
)" + line_mark + "{";

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

                std::vector<char> macro_content;

                while (lex.m_source_stream->eof() == false
                    && lex.m_source_stream
                    && lex.m_source_stream->tellg() < macro_end_place)
                {
                    char ch;
                    lex.m_source_stream->read(&ch, 1);
                    macro_content.push_back(ch);
                }
                macro_content.push_back('\0');

                _macro_action_vm = wo_create_vm();
                auto macro_virtual_file_src = lex.m_shared_context->register_temp_virtual_file(
                    (macro_anylzing_src + macro_content.data() + "\nreturn \"\";}").c_str());

                lexer::imported_source_path_set_t origin_linked_sources;
                lexer::who_import_me_map_t origin_import_relationships;
                lexer::export_import_map_t origin_export_imports;

                origin_linked_sources.swap(lex.m_shared_context->m_linked_script_path_set);
                origin_import_relationships.swap(lex.m_shared_context->m_who_import_me_map_tree);
                origin_export_imports.swap(lex.m_shared_context->m_export_import_map);

                lex.begin_trying_block();
                if (!_wo_load_source(_macro_action_vm, macro_virtual_file_src, nullptr, 0, &lex))
                {
                    auto macro_error_frame = std::move(lex.get_current_error_frame());
                    lex.end_trying_block();

                    lex.produce_lexer_error(lexer::msglevel_t::error, WO_ERR_FAILED_TO_COMPILE_MACRO_CONTROLOR);
                    for (auto& error_message : macro_error_frame)
                    {
                        auto layer = error_message.m_layer;
                        lex.record_message(std::move(error_message)).m_layer = layer;
                    }

                    wo_close_vm(_macro_action_vm);
                    _macro_action_vm = nullptr;
                }
                else
                {
                    lex.end_trying_block();

                    // Donot jit to make debug friendly.
                    if (nullptr == wo_run(_macro_action_vm))
                    {
                        lex.produce_lexer_error(lexer::msglevel_t::error, WO_ERR_FAILED_TO_RUN_MACRO_CONTROLOR,
                            macro_name.c_str(),
                            wo_get_runtime_error(_macro_action_vm));
                    }
                }

                // Restore states.
                origin_linked_sources.swap(lex.m_shared_context->m_linked_script_path_set);
                origin_import_relationships.swap(lex.m_shared_context->m_who_import_me_map_tree);
                origin_export_imports.swap(lex.m_shared_context->m_export_import_map);
            }
        }
        else
            lex.produce_lexer_error(lexer::msglevel_t::error, WO_ERR_HERE_SHOULD_HAVE, "{");
    }

    std::string lexer::compiler_message_t::to_string(bool need_ansi_describe)
    {
        using namespace std;

        if (need_ansi_describe)
            return (
                m_level == msglevel_t::error
                ? (ANSI_HIR "error" ANSI_RST)
                : (ANSI_HIC "infom" ANSI_RST)
                )
            + (" (" + std::to_string(m_range_end[0] + 1) + "," + std::to_string(m_range_end[1]))
            + (") " + m_describe);
        else
            return (
                m_level == msglevel_t::error
                ? ("error")
                : ("infom")
                )
            + (" (" + std::to_string(m_range_end[0] + 1) + "," + std::to_string(m_range_end[1]))
            + (") " + m_describe);
    }

    ///////////////////////////////////////////////////

    const char* lexer::lex_is_operate_type(lex_type tt)
    {
        for (auto& [op_str, op_type] : _lex_operator_list)
        {
            if (op_type == tt)
                return op_str.c_str();
        }
        return nullptr;
    }
    const char* lexer::lex_is_keyword_type(lex_type tt)
    {
        for (auto& [op_str, op_type] : _key_word_list)
        {
            if (op_type == tt)
                return op_str.c_str();
        }
        return nullptr;
    }
    lex_type lexer::lex_is_valid_operator(const std::string& op)
    {
        if (auto fnd = _lex_operator_list.find(op); fnd != _lex_operator_list.end())
            return fnd->second;

        return lex_type::l_error;
    }
    lex_type lexer::lex_is_keyword(const std::string& op)
    {
        if (auto fnd = _key_word_list.find(op); fnd != _key_word_list.end())
            return fnd->second;

        return lex_type::l_error;
    }
    bool lexer::lex_isoperatorch(int ch)
    {
        const static std::unordered_set<char> operator_char_set = []() {
            std::unordered_set<char> _result;
            for (auto& [_operator, operator_info] : _lex_operator_list)
                for (char wch : _operator)
                    _result.insert(wch);
            return _result;
            }();
        return operator_char_set.find(ch) != operator_char_set.end();
    }
    bool lexer::lex_isspace(int ch)
    {
        if (ch == EOF)
            return false;

        if (ch > 0x7F)
            return false;

        return ch == 0 || isspace(ch);
    }
    bool lexer::lex_isalpha(int ch)
    {
        // if ch can used as begin of identifier, return true
        if (ch == EOF)
            return false;

        if (ch > 0x7F)
            // Treate all non ascii characters as alpha
            return true;

        return isalpha(ch);
    }
    bool lexer::lex_isidentbeg(int ch)
    {
        // if ch can used as begin of identifier, return true
        if (ch == EOF)
            return false;

        if (ch > 0x7F)
            // Treate all non ascii characters as alpha
            return true;

        return isalpha(ch) || ch == '_';
    }
    bool lexer::lex_isident(int ch)
    {
        // if ch can used as begin of identifier, return true
        if (ch == EOF)
            return false;

        if (ch > 0x7F)
            // Treate all non ascii characters as alpha
            return true;

        return isalnum(ch) || ch == '_';
    }
    bool lexer::lex_isalnum(int ch)
    {
        // if ch can used in identifier (not first character), return true
        if (ch == EOF)
            return false;

        if (ch > 0x7F)
            // Treate all non ascii characters as alpha
            return true;

        return isalnum(ch);
    }
    bool lexer::lex_isdigit(int ch)
    {
        if (ch == EOF)
            return false;

        if (ch > 0x7F)
            // Treate all non ascii characters as alpha
            return false;

        return isdigit((wchar_t)ch);
    }
    bool lexer::lex_isxdigit(int ch)
    {
        if (ch == EOF)
            return false;

        if (ch > 0x7F)
            // Treate all non ascii characters as alpha
            return false;

        return isxdigit((wchar_t)ch);
    }
    bool lexer::lex_isodigit(int ch)
    {
        if (ch == EOF)
            return false;

        return ch >= '0' && ch <= '7';
    }
    int lexer::lex_hextonum(int ch)
    {
        wo_assert(lex_isxdigit(ch));

        if (ch >= '0' && ch <= '9')
        {
            return ch - '0';
        }
        else if (ch >= 'A' && ch <= 'F')
        {
            return ch - 'A' + 10;
        }
        else if (ch >= 'a' && ch <= 'f')
        {
            return ch - 'a' + 10;
        }

        return -1;
    }
    int lexer::lex_octtonum(int ch)
    {
        wo_assert(lex_isodigit(ch));

        return ch - '0';
    }
    uint64_t lexer::read_from_unsigned_literal(const char* text)
    {
        uint64_t base = 10;
        uint64_t result = 0;

        if (text[0] == '0')
        {
            if (text[1] == 0)
                return 0;

            switch (text[1])
            {
            case 'X':
            case 'x':
                base = 16;
                text = text + 2;
                break;
            case 'B':
            case 'b':
                base = 2;
                text = text + 2;
                break;
            default:
                base = 8;
                ++text;
                break;
            }
        }
        while (*text)
        {
            if (*text == 'H' || *text == 'h')
                break;
            result = base * result + lexer::lex_hextonum(*text);
            ++text;
        }
        return result;
    }
    int64_t lexer::read_from_literal(const char* text)
    {
        if (text[0] == '-')
            return -(int64_t)read_from_unsigned_literal(text + 1);
        return (int64_t)read_from_unsigned_literal(text);
    }
    lexer::SharedContext::SharedContext(const std::optional<wo_pstring_t>& source_path)
    {
        // Make sure error frame has one frame.
        (void)m_error_frame.emplace_back();

        if (source_path.has_value())
            m_linked_script_path_set.insert(source_path.value());
    }
    lexer::SharedContext::~SharedContext()
    {
        for (auto vpath : m_temp_virtual_file_path)
            wo_assure(WO_TRUE == wo_remove_virtual_file(vpath.c_str()));
    }
    const char* lexer::SharedContext::register_temp_virtual_file(wo_string_t context)
    {
        // String pool is available during compiling.
        // We can use it directly.
        char temp_path[64];
        (void)snprintf(temp_path, 64, "woo/tmp/%p-%zu", this, m_temp_virtual_file_path.size());

        wo_assure(WO_TRUE == wo_virtual_source(
            temp_path, context, WO_TRUE));

        return m_temp_virtual_file_path.emplace_back(temp_path).c_str();
    }
    lexer::lexer(
        std::optional<lexer*> who_import_me,
        const std::optional<wo_pstring_t>& source_path,
        std::optional<std::unique_ptr<std::istream>>&& source_stream)
        : m_who_import_me(who_import_me)
        , m_source_path(source_path)
        , m_source_stream{}
        //
        , m_shared_context(
            who_import_me.has_value()
            // Share from root script.
            ? who_import_me.value()->m_shared_context
            // Create for root script.
            : std::make_shared<SharedContext>(source_path))
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
        if (source_stream)
        {
            m_source_stream = std::move(source_stream.value());
        }
        else
        {
            wo_assert(source_path.has_value());
            (void)record_parser_error(
                lexer::msglevel_t::error,
                WO_ERR_CANNOT_OPEN_FILE,
                source_path.value()->c_str());
        }
    }
    size_t lexer::get_error_frame_layer() const
    {
        return m_shared_context->m_error_frame.size();
    }
    lexer::compiler_message_list_t& lexer::get_current_error_frame()
    {
        return m_shared_context->m_error_frame.back();
    }
    lexer::compiler_message_list_t& lexer::get_root_error_frame()
    {
        return m_shared_context->m_error_frame.front();
    }
    bool lexer::check_source_path_has_been_linked_in(wo_pstring_t full_path)
    {
        return !m_shared_context->m_linked_script_path_set.insert(full_path).second;
    }
    bool lexer::check_source_has_been_imported_by_specify_source(
        wo_pstring_t checking_path, wo_pstring_t current_path) const
    {
        if (checking_path == current_path)
            return true;

        auto fnd = m_shared_context->m_who_import_me_map_tree.find(checking_path);
        if (fnd != m_shared_context->m_who_import_me_map_tree.end())
        {
            return fnd->second.find(current_path) != fnd->second.end();
        }
        return false;
    }
    void lexer::record_import_relationship(wo_pstring_t imported_path, bool export_imports)
    {
        auto* my_source_path = m_source_path.value();
        auto& who_import_me_map_tree = m_shared_context->m_who_import_me_map_tree;
        auto& export_import_map = m_shared_context->m_export_import_map;

        // Record forward and backward import relationship.
        who_import_me_map_tree[imported_path].insert(
            my_source_path);

        for (auto* export_import : export_import_map[imported_path])
            who_import_me_map_tree[export_import].insert(
                my_source_path);

        if (export_imports)
        {
            // Update the import relationship for who-import-me;
            for (auto& who_import_me : who_import_me_map_tree[my_source_path])
                who_import_me_map_tree[imported_path].insert(who_import_me);

            // Update export map.
            export_import_map[my_source_path].insert(
                imported_path);
        }
    }
    void lexer::import_ast_tree(ast::AstBase* astbase)
    {
        m_imported_ast_tree_list.push_back(astbase);
    }
    int lexer::peek_char()
    {
        uint8_t ch = static_cast<uint8_t>(m_source_stream->peek());

        if (m_source_stream->eof() || !*m_source_stream)
        {
            m_source_stream->clear(
                m_source_stream->rdstate() &
                ~(std::ios_base::failbit | std::ios_base::eofbit));
            return EOF;
        }

        if (ch == '\r')
            return '\n';

        return static_cast<int>(ch);
    }
    int lexer::read_char()
    {
        uint8_t ch = static_cast<uint8_t>(m_source_stream->get());
        if (m_source_stream->eof() || !*m_source_stream)
        {
            m_source_stream->clear(
                m_source_stream->rdstate() &
                ~(std::ios_base::failbit | std::ios_base::eofbit));
            return EOF;
        }

        if (ch == '\r')
        {
            if (peek_char() == '\n')
                // Eat \r\n as \n
                (void)m_source_stream->get();

            ch = '\n';
        }

        if (ch == '\n')
        {
            _m_col_counter = 0;
            ++_m_row_counter;
        }
        else
            ++_m_col_counter;

        return static_cast<int>(ch);
    }

    lexer::compiler_message_t& lexer::record_message(compiler_message_t&& message)
    {
        auto& emplaced_message =
            m_shared_context->m_error_frame.back().emplace_back(std::move(message));

        emplaced_message.m_layer
            = m_shared_context->m_error_frame.size() - 1 + (
                emplaced_message.m_level == msglevel_t::error ? 0 : 1);
        return emplaced_message;
    }
    lexer::compiler_message_t& lexer::append_message(const compiler_message_t& message)
    {
        auto& emplaced_message = m_shared_context->m_error_frame.back().emplace_back(message);

        emplaced_message.m_layer
            = m_shared_context->m_error_frame.size() - 1 + (
                emplaced_message.m_level == msglevel_t::error ? 0 : 1);
        return emplaced_message;
    }

    void lexer::produce_token(lex_type type, std::string&& moved_token_text)
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
    void lexer::skip_this_line()
    {
        // Skip this line.
        int skiped_char;
        do
        {
            skiped_char = read_char();
        } while (skiped_char != '\n' && skiped_char != EOF);
    }
    lexer::peeked_token_t* lexer::peek()
    {
        for (;;)
        {
            while (_m_peeked_tokens.empty())
                move_forward();

            wo_assert(!_m_peeked_tokens.empty());
            lexer::peeked_token_t* peeked_token = &_m_peeked_tokens.front();
            if (peeked_token->m_lex_type == lex_type::l_macro)
            {
                // We need to expand this macro here.
                std::string macro_name = peeked_token->m_token_text;
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
        return !m_shared_context->m_error_frame.back().empty();
    }
    wo_pstring_t lexer::get_source_path() const
    {
        return m_source_path.value();
    }
    size_t lexer::get_error_frame_count_for_debug() const
    {
        return m_shared_context->m_error_frame.size();
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
        return m_shared_context->m_declared_macro_list;
    }
    void lexer::begin_trying_block()
    {
        m_shared_context->m_error_frame.emplace_back();
    }
    void lexer::end_trying_block()
    {
        m_shared_context->m_error_frame.pop_back();
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

        std::string token_literal_result;
        auto append_result_char =
            [&token_literal_result](char ch) {token_literal_result.push_back(ch); };
        auto append_result_char_serial =
            [&token_literal_result](const char* serial, size_t len) {token_literal_result.append(serial, len); };

        // Format string.
        bool is_format_string_begin = false, is_format_string_middle = false;
        switch (readed_char)
        {
        case '/':
        {
            int peeked_char = peek_char();
            if (peeked_char == '/')
            {
                // Skip '/'
                (void)read_char();

                skip_this_line();
                return;
            }
            else if (peeked_char == '*')
            {
                // Skip '/'
                (void)read_char();

                // Skip to next '*/'
                do
                {
                    peeked_char = read_char();
                    if (peeked_char == EOF)
                        break;
                    else if (peeked_char == '*')
                    {
                        peeked_char = read_char();
                        if (peeked_char == EOF || peeked_char == '/')
                            break;
                    }
                } while (true);
                return;
            }
            else
                goto checking_valid_operator;
        }
        case
        ';':
        {
            append_result_char(readed_char);
            return produce_token(lex_type::l_semicolon, std::move(token_literal_result));
        }
        case '@':
        {
            // @"(Example string "without" '\' it will be very happy!)"
            if (int tmp_ch = peek_char(); tmp_ch == '"')
            {
                (void)read_char();

                int following_ch;
                while (true)
                {
                    following_ch = read_char();
                    if (following_ch == '"' && peek_char() == '@')
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
        case '\'':
        case '"':
        {
            int following_ch;
            while (true)
            {
                following_ch = read_char();
                if (following_ch == '"' && readed_char == '"')
                    return produce_token(lex_type::l_literal_string, std::move(token_literal_result));
                else if (following_ch == '\'' && readed_char == '\'')
                {
                    switch (wo::u8strnlen(token_literal_result.data(), token_literal_result.size()))
                    {
                    case 0:
                        return produce_lexer_error(msglevel_t::error, WO_ERR_NO_CHAR_IN_CHAR);
                    case 1:
                        return produce_token(lex_type::l_literal_char, std::move(token_literal_result));
                    default:
                        return produce_lexer_error(msglevel_t::error, WO_ERR_TOO_MANY_CHAR_IN_CHAR);
                    }
                    wo_error("Cannot be here.");
                }

                if (following_ch != EOF && following_ch != '\n')
                {
                    if (following_ch == '\\')
                    {
                        // Escape character 
                        int escape_ch = read_char();
                        switch (escape_ch)
                        {
                        case '\'':
                        case '"':
                        case '?':
                        case '\\':
                            append_result_char(escape_ch); break;
                        case 'a':
                            append_result_char('\a'); break;
                        case 'b':
                            append_result_char('\b'); break;
                        case 'f':
                            append_result_char('\f'); break;
                        case 'n':
                            append_result_char('\n'); break;
                        case 'r':
                            append_result_char('\r'); break;
                        case 't':
                            append_result_char('\t'); break;
                        case 'v':
                            append_result_char('\v'); break;
                        case '0': case '1': case '2': case '3':
                        case '4': case '5': case '6': case '7':
                        {
                            // oct 1byte 
                            int oct_ascii = escape_ch - '0';
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
                        case 'x':
                        {
                            // hex 1byte 
                            uint8_t hex_ascii = 0;
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
                            append_result_char(static_cast<char>(hex_ascii));
                            break;
                        }
                        case 'u':
                        {
                            // hex 1byte 
                            char16_t hex_ascii[UTF16MAXLEN] = {};
                            for (int i = 0; i < 4; i++)
                            {
                                if (lexer::lex_isxdigit(peek_char()))
                                {
                                    hex_ascii[0] *= 16;
                                    hex_ascii[0] += lexer::lex_hextonum(read_char());
                                }
                                else if (i == 0)
                                    goto str_escape_sequences_fail;
                                else
                                    break;
                            }

                            size_t u16_count = 1;
                            if (wo::u16hisurrogate(hex_ascii[0]))
                            {
                                if (read_char() != '\\'
                                    || read_char() != 'u')
                                    // Should be a surrogate pair.
                                    goto str_escape_sequences_fail;

                                for (int i = 0; i < 4; i++)
                                {
                                    if (lexer::lex_isxdigit(peek_char()))
                                    {
                                        hex_ascii[1] *= 16;
                                        hex_ascii[1] += lexer::lex_hextonum(read_char());
                                    }
                                    else if (i == 0)
                                        goto str_escape_sequences_fail;
                                    else
                                        break;
                                }
                                u16_count = 2;
                            }

                            char u8buf[UTF8MAXLEN] = {};
                            size_t u8len;

                            if (u16_count != wo::u16exractu8(hex_ascii, u16_count, u8buf, &u8len))
                                goto str_escape_sequences_fail;

                            append_result_char_serial(u8buf, u8len);
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
        case '(':
        {
            append_result_char(readed_char);
            return produce_token(lex_type::l_left_brackets, std::move(token_literal_result));
        }
        case ')':
        {
            append_result_char(readed_char);

            return produce_token(lex_type::l_right_brackets, std::move(token_literal_result));
        }
        case '{':
        {
            append_result_char(readed_char);
            ++_m_curry_count_in_format_string;
            return produce_token(lex_type::l_left_curly_braces, std::move(token_literal_result));
        }
        case '}':
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
        case '#':
        {
            if (peek_char() == '!')
            {
                // Is shebang, skip this line.
                // Skip '!'
                (void)read_char();

                skip_this_line();
                return;
            }

            // ATTENTION, SECURE:
            //  Disable pragma if source_file == nullptr, it's in deserialize.
            //  Processing pragma here may lead to arbitrary code execution.
            if (!m_source_path.has_value())
            {
                append_result_char(readed_char);
                produce_token(lex_type::l_unknown_token, std::move(token_literal_result));
                return;
            }

            // Read sharp
            // #macro
            std::string pragma_name;
            if (lexer::lex_isidentbeg(peek_char()))
            {
                pragma_name += read_char();
                while (lexer::lex_isident(peek_char()))
                    pragma_name += read_char();
            }

            if (pragma_name == "macro")
            {
                // OK FINISH PRAGMA, CONTINUE
                auto macro_instance = std::make_unique<macro>(*this);

                auto macro_name = macro_instance->macro_name;
                if (auto fnd = m_shared_context->m_declared_macro_list.insert(
                    std::make_pair(macro_name, std::move(macro_instance)));
                    !fnd.second)
                {
                    if (fnd.first->second->filename == this->m_source_path)
                        // NOTE: This script has been imported in another macro, and
                        //  the macro define has been inherted, just ignore and skip.
                        return;

                    produce_lexer_error(
                        msglevel_t::error, WO_ERR_UNKNOWN_REPEAT_MACRO_DEFINE, macro_name.c_str());

                    char describe[256] = {};
                    snprintf(describe, 256, WO_INFO_SYMBOL_NAMED_DEFINED_HERE, macro_name.c_str());
                    (void)record_message(
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
            else if (pragma_name == "line")
            {
                auto* file_name = peek();
                if (file_name->m_lex_type != lex_type::l_literal_string)
                {
                    return produce_lexer_error(
                        msglevel_t::error, WO_ERR_LINE_NEED_STRING_AS_PATH);
                }
                auto new_shown_file_path = wo::wstring_pool::get_pstr(file_name->m_token_text);
                move_forward();

                auto* row_no = peek();
                if (row_no->m_lex_type != lex_type::l_literal_integer)
                {
                    return produce_lexer_error(
                        msglevel_t::error, WO_ERR_LINE_NEED_INTEGER_AS_ROW);
                }
                auto new_row_counter = read_from_unsigned_literal(row_no->m_token_text.c_str());
                move_forward();

                auto* col_no = peek();
                if (col_no->m_lex_type != lex_type::l_literal_integer)
                {
                    return produce_lexer_error(
                        msglevel_t::error, WO_ERR_LINE_NEED_INTEGER_AS_COL);
                }
                auto new_col_counter = read_from_unsigned_literal(col_no->m_token_text.c_str());
                consume_forward();

                m_source_path = new_shown_file_path;
                _m_row_counter = new_row_counter - 1;
                _m_col_counter = new_col_counter;
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
                if (peek_char() == '!' && m_source_path.has_value())
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
                if (readed_char == '0')
                {
                    // it may be OCT DEC HEX
                    switch (char sec_ch = peek_char())
                    {
                    case 'x':
                    case 'X':
                        base = 16;                      // is hex
                        break;
                    case 'b':
                    case 'B':
                        base = 2;                      // is bin
                        break;
                    default:
                        if (lexer::lex_isodigit(sec_ch))
                            base = 8;                   // is oct
                        else
                            base = 10;                  // is dec
                        break;
                    }
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
                        else if (following_chs == '.')
                        {
                            append_result_char(read_char());
                            if (is_real)
                                return produce_lexer_error(msglevel_t::error, WO_ERR_UNEXCEPT_CH_AFTER_CH, readed_char, following_chs);
                            is_real = true;
                        }
                        else if (following_chs == 'H' || following_chs == 'h')
                        {
                            if (is_real)
                                return produce_lexer_error(msglevel_t::error, WO_ERR_UNEXCEPT_CH_AFTER_CH, readed_char, following_chs);
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
                        if (lexer::lex_isxdigit(following_chs) || following_chs == 'X' || following_chs == 'x')
                            append_result_char(read_char());
                        else if (following_chs == '.')
                            return produce_lexer_error(msglevel_t::error, WO_ERR_UNEXCEPT_CH_AFTER_CH, readed_char, following_chs);
                        else if (following_chs == 'H' || following_chs == 'h')
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
                        else if (following_chs == '.')
                            return produce_lexer_error(msglevel_t::error, WO_ERR_UNEXCEPT_CH_AFTER_CH, readed_char, following_chs);
                        else if (following_chs == 'H' || following_chs == 'h')
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
                        if (following_chs == '1' || following_chs == '0' || following_chs == 'B' || following_chs == 'b')
                            append_result_char(read_char());
                        else if (following_chs == '.')
                            return produce_lexer_error(msglevel_t::error, WO_ERR_UNEXCEPT_CH_AFTER_CH, readed_char, following_chs);
                        else if (following_chs == 'H' || following_chs == 'b')
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

                    lex_type tmp_op_type = lexer::lex_is_valid_operator(
                        token_literal_result + (char)following_ch);

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
            if (following_ch == '"')
            {
                if (is_format_string_begin)
                    return produce_token(lex_type::l_literal_string, std::move(token_literal_result));
                else
                {
                    _m_in_format_string = false;
                    return produce_token(lex_type::l_format_string_end, std::move(token_literal_result));
                }
            }
            if (following_ch == '{')
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
                if (following_ch == '\\')
                {
                    // Escape character 
                    int escape_ch = read_char();
                    switch (escape_ch)
                    {
                    case '\'':
                    case '"':
                    case '?':
                    case '\\':
                    case '{':
                    case '}':
                        append_result_char(escape_ch); break;
                    case 'a':
                        append_result_char('\a'); break;
                    case 'b':
                        append_result_char('\b'); break;
                    case 'f':
                        append_result_char('\f'); break;
                    case 'n':
                        append_result_char('\n'); break;
                    case 'r':
                        append_result_char('\r'); break;
                    case 't':
                        append_result_char('\t'); break;
                    case 'v':
                        append_result_char('\v'); break;
                    case '0': case '1': case '2': case '3': case '4':
                    case '5': case '6': case '7': case '8': case '9':
                    {
                        // oct 1byte 
                        int oct_ascii = escape_ch - '0';
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
                    case 'X':
                    case 'x':
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
                    case 'U':
                    case 'u':
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
                        return produce_lexer_error(
                            msglevel_t::error, WO_ERR_UNKNOW_ESCSEQ_BEGIN_WITH_CH, escape_ch);
                    }
                }
                else
                    append_result_char(following_ch);
            }
            else
                return produce_lexer_error(
                    msglevel_t::error, WO_ERR_UNEXCEPTED_EOL_IN_STRING);
        }

        // Cannot be here.
        wo_error("Cannot be here.");
    }
    bool lexer::try_handle_macro(const std::string& macro_name)
    {
        size_t macro_pre_begin_row = _m_this_token_pre_begin_row;
        size_t macro_pre_begin_col = _m_this_token_pre_begin_col;
        const size_t macro_begin_row = _m_this_token_begin_row;
        const size_t macro_begin_col = _m_this_token_begin_col;

        auto fnd = m_shared_context->m_declared_macro_list.find(macro_name);
        if (fnd != m_shared_context->m_declared_macro_list.end() && fnd->second->_macro_action_vm)
        {
            wo_integer_t script_func;
            [[maybe_unused]] wo_handle_t jit_func;

#if WO_ENABLE_RUNTIME_CHECK
            auto found =
#endif
                wo_extern_symb(
                    fnd->second->_macro_action_vm,
                    "macro_entry",
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
                    wo_get_runtime_error(fnd->second->_macro_action_vm));

                return false;
            }
            else
            {
                // String pool is available during compiling.
                // We can use it directly.
                wo_pstring_t result_content_vfile =
                    wstring_pool::get_pstr(
                        std::string(VIRTUAL_FILE_SCHEME_M) +
                        m_shared_context->register_temp_virtual_file(
                            wo_string(result)));

                wo::lexer tmp_lex(
                    this, // We can generate macro define by macro result.
                    result_content_vfile,
                    std::make_unique<std::istringstream>(
                        wo_string(result)));

                tmp_lex.begin_trying_block();

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

                    // Update the `pre location` for correct token location in macro results.
                    macro_pre_begin_row = macro_end_row;
                    macro_pre_begin_col = macro_end_col;

                    tmp_lex.move_forward();
                }

                while (!origin_peeked_queue.empty())
                {
                    _m_peeked_tokens.emplace(std::move(origin_peeked_queue.front()));
                    origin_peeked_queue.pop();
                }

                auto& current_error_frame = tmp_lex.get_current_error_frame();
                if (!current_error_frame.empty())
                {
                    auto lexer_error_frame = std::move(current_error_frame);
                    tmp_lex.end_trying_block();

                    produce_lexer_error(msglevel_t::error, WO_ERR_INVALID_TOKEN_MACRO_CONTROLOR,
                        fnd->second->macro_name.c_str());

                    for (auto& error_message : lexer_error_frame)
                    {
                        auto layer = error_message.m_layer;
                        record_message(std::move(error_message)).m_layer = layer;
                    }

                    // ATTENTION: Virtual file not been removed here, memory may leak.
                    return false;
                }
                else
                    tmp_lex.end_trying_block();
            }

            return true;
        }
        else
        {
            // Unfound macro.
            produce_token(lex_type::l_macro, std::string(macro_name));
            return false;
        }
    }
}