#pragma once

#include "wo_assert.hpp"
#include "wo_utf8.hpp"

#include <string>
#include <list>
#include <queue>
#include <unordered_map>

#include <cwctype>
#include <cwchar>

#include "wo_lang_compiler_information.hpp"
#include "wo_source_file_manager.hpp"
#include "wo_const_string_pool.hpp"

#ifdef ANSI_WIDE_CHAR_SIGN
#   undef ANSI_WIDE_CHAR_SIGN
#   define ANSI_WIDE_CHAR_SIGN L
#endif

namespace wo
{
    namespace ast
    {
        class AstBase;
    }

    using lex_type_base_t = int8_t;
    enum class lex_type : lex_type_base_t
    {
        l_eof = -1,
        l_error = 0,
        l_empty,                // [empty]
        l_identifier,           // identifier.
        l_literal_integer,      // 1 233 0x123456 0b1101001 032
        l_literal_handle,       // 0L 256L 0xFFL
        l_literal_real,         // 0.2  0.
        l_literal_string,       // "helloworld"
        l_literal_raw_string,   // @ raw_string @
        l_literal_char,         // 'x'
        l_format_string_begin,  // F"..{
        l_format_string,        // }..{ 
        l_format_string_end,    // }.."
        l_semicolon,            // ;
        l_comma,                // ,
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
        l_value_assign,         // :=
        l_value_add_assign,     // +:=
        l_value_sub_assign,     // -:= 
        l_value_mul_assign,     // *:=
        l_value_div_assign,     // /:= 
        l_value_mod_assign,     // %:= 
        l_equal,                // ==
        l_not_equal,            // !=
        l_larg_or_equal,        // >=
        l_less_or_equal,        // <=
        l_less,                 // <
        l_larg,                 // >
        l_land,                 // &&
        l_lor,                  // ||
        l_or,                   // |
        l_lnot,                 // !
        l_scopeing,             // ::
        l_template_using_begin, // ::<
        l_typecast,             // :
        l_index_point,          // .
        l_double_index_point,   // ..  may be used? hey..
        l_variadic_sign,        // ...
        l_index_begin,          // '['
        l_index_end,            // ']'
        l_direct,               // '->'
        l_inv_direct,           // '<|'
        l_function_result,      // '=>'
        l_bind_monad,           // '>>'
        l_map_monad,            // '>>'
        l_left_brackets,        // (
        l_right_brackets,       // )
        l_left_curly_braces,    // {
        l_right_curly_braces,   // }
        l_question,             // ?
        l_import,
        l_export,
        l_nil,
        l_true,
        l_false,
        l_while,
        l_if,
        l_else,
        l_namespace,
        l_for,
        l_extern,
        l_let,
        l_mut,
        l_func,
        l_return,
        l_using,
        l_alias,
        l_enum,
        l_as,
        l_is,
        l_typeof,
        l_private,
        l_public,
        l_protected,
        l_static,
        l_break,
        l_continue,
        l_lambda,
        l_at,
        l_do,
        l_where,
        l_operator,
        l_union,
        l_match,
        l_struct,
        l_immut,
        l_typeid,
        l_macro,
        l_unknown_token,
    };

    // Make sure token enum defined in wo.h and here are same.
    static_assert((int)lex_type::l_eof == WO_LSPV2_TOKEN_EOF);
    static_assert((int)lex_type::l_unknown_token == WO_LSPV2_TOKEN_UNKNOWN_TOKEN);

    class lexer;

    class macro
    {
    public:
        std::string macro_name;
        wo_vm _macro_action_vm;

        size_t   begin_row;
        size_t   begin_col;
        size_t   end_row;
        size_t   end_col;
        wo_pstring_t filename;

        macro(lexer& lex);

        ~macro()
        {
            if (_macro_action_vm)
                wo_close_vm(_macro_action_vm);
        }
    };

    class lexer
    {
        friend class macro;

        lexer(const lexer&) = delete;
        lexer(lexer&&) = delete;

        lexer& operator = (const lexer&) = delete;
        lexer& operator = (lexer&&) = delete;
    public:
        enum class msglevel_t
        {
            error,
            infom,
        };

        struct compiler_message_t
        {
            msglevel_t  m_level;

            size_t      m_range_begin[2];
            size_t      m_range_end[2];
            std::string m_filename;

            std::string m_describe;

            // Auto assigned in `record_message`
            size_t      m_layer;

            std::string to_string(bool need_ansi_describe);
        };
        using compiler_message_list_t =
            std::list<compiler_message_t>;

        struct peeked_token_t
        {
            lex_type        m_lex_type;
            std::string     m_token_text;
            size_t          m_token_begin[4];
            size_t          m_token_end[2];
        };
    private:
        using declared_macro_map_t =
            std::unordered_map<std::string, std::unique_ptr<macro>>;
        using imported_source_path_set_t =
            std::unordered_set<wo_pstring_t>;
        using who_import_me_map_t =
            std::unordered_map<wo_pstring_t, std::unordered_set<wo_pstring_t>>;
        using export_import_map_t = who_import_me_map_t;

    private:
        const static std::unordered_map<std::string, lex_type> _lex_operator_list;
        const static std::unordered_map<std::string, lex_type> _key_word_list;

    private:
        static const char* lex_is_operate_type(lex_type tt);
        static const char* lex_is_keyword_type(lex_type tt);
        static lex_type lex_is_valid_operator(const std::string& op);
        static lex_type lex_is_keyword(const std::string& op);
        static bool lex_isoperatorch(int ch);
        static bool lex_isspace(int ch);
        static bool lex_isalpha(int ch);
        static bool lex_isidentbeg(int ch);
        static bool lex_isident(int ch);
        static bool lex_isalnum(int ch);
        static bool lex_isdigit(int ch);
        static bool lex_isxdigit(int ch);
        static bool lex_isodigit(int ch);

    public:
        static int lex_hextonum(int ch);
        static int lex_octtonum(int ch);
        static uint64_t read_from_unsigned_literal(const char* text);
        static int64_t read_from_literal(const char* text);

    private:
        struct SharedContext
        {
            std::list<compiler_message_list_t> m_error_frame;
            declared_macro_map_t m_declared_macro_list;

            // NOTE: Following wo_pstring_t only used in pass1.
            imported_source_path_set_t m_linked_script_path_set;
            who_import_me_map_t m_who_import_me_map_tree;
            export_import_map_t m_export_import_map;

            std::list<std::string> m_temp_virtual_file_path;

            SharedContext(const std::optional<wo_pstring_t>& source_path);
            ~SharedContext();
            SharedContext(const SharedContext&) = delete;
            SharedContext(SharedContext&&) = delete;
            SharedContext& operator = (const SharedContext&) = delete;
            SharedContext& operator = (SharedContext&&) = delete;

            const char* register_temp_virtual_file(wo_string_t context);
        };

        std::optional<lexer*> m_who_import_me;
        std::optional<wo_pstring_t> m_source_path;
        std::unique_ptr<std::istream> m_source_stream;
        std::shared_ptr<SharedContext> m_shared_context;

        std::list<ast::AstBase*> m_imported_ast_tree_list;

        std::queue<peeked_token_t> _m_peeked_tokens;
        size_t _m_row_counter;
        size_t _m_col_counter;

        // Only used in move next;
        size_t _m_this_token_pre_begin_row;
        size_t _m_this_token_pre_begin_col;
        size_t _m_this_token_begin_row;
        size_t _m_this_token_begin_col;
        bool _m_in_format_string;
        size_t _m_curry_count_in_format_string;

    public:
        lexer(
            std::optional<lexer*> who_import_me,
            const std::optional<wo_pstring_t>& source_path,
            std::optional<std::unique_ptr<std::istream>>&& source_stream);
    private:
        void    produce_token(lex_type type, std::string&& moved_token_text);
        void    token_pre_begin_here();
        void    token_begin_here();

    public:
        size_t get_error_frame_layer() const;
        compiler_message_list_t& get_current_error_frame();
        compiler_message_list_t& get_root_error_frame();

        [[nodiscard]]
        compiler_message_t& record_message(compiler_message_t&& moved_message);
        [[nodiscard]]
        compiler_message_t& append_message(const compiler_message_t& moved_message);

        template<typename ... FmtArgTs>
        void record_format(
            msglevel_t level,
            size_t range_begin_row,
            size_t range_begin_col,
            size_t range_end_row,
            size_t range_end_col,
            const std::string& source,
            const char* format,
            FmtArgTs&& ... format_args)
        {
            bool failed_flag = false;
            int count = snprintf(nullptr, 0, format, format_args...);
            
            if (count < 0)
                failed_flag = true;

            std::vector<char> describe(failed_flag ? 0 : count + 1);
            if (snprintf(describe.data(), describe.size(), format, format_args...) < 0)
                failed_flag = true;
           
            (void)record_message(
                compiler_message_t
                {
                    level,
                    { range_begin_row, range_begin_col },
                    { range_end_row, range_end_col },
                    source,
                    failed_flag ? format : describe.data(),
                });
        }

        template<typename ... FmtArgTs>
        void produce_lexer_error(
            msglevel_t level,
            const char* format,
            FmtArgTs&& ... format_args)
        {
            if (m_source_path.has_value())
                record_format(
                    level,
                    _m_row_counter,
                    _m_col_counter,
                    _m_row_counter,
                    _m_col_counter,
                    *m_source_path.value(),
                    format,
                    format_args...);

            produce_token(lex_type::l_error, "");
        }

        template<typename ... FmtArgTs>
        [[nodiscard]]
        lex_type record_parser_error(
            msglevel_t level,
            const char* format,
            FmtArgTs&& ... format_args)
        {
            record_format(
                level,
                _m_this_token_begin_row,
                _m_this_token_begin_col,
                _m_row_counter,
                _m_col_counter,
                *m_source_path.value(),
                format,
                format_args...);

            return lex_type::l_error;
        }

        template<typename AstT, typename ... FmtArgTs>
        lex_type record_lang_error(
            msglevel_t level,
            AstT* ast_node,
            const char* format,
            FmtArgTs&& ... format_args)
        {
            record_format(
                level,
                ast_node->source_location.begin_at.row,
                ast_node->source_location.begin_at.column,
                ast_node->source_location.end_at.row,
                ast_node->source_location.end_at.column,
                *ast_node->source_location.source_file,
                format,
                format_args...);

            return lex_type::l_error;
        }

        [[nodiscard]]
        bool check_source_path_has_been_linked_in(wo_pstring_t full_path);
        [[nodiscard]]
        bool check_source_has_been_imported_by_specify_source(
            wo_pstring_t checking_path, wo_pstring_t current_path) const;
        void record_import_relationship(wo_pstring_t imported_path, bool export_imports);
        void import_ast_tree(ast::AstBase* astbase);
        ast::AstBase* merge_imported_ast_trees(ast::AstBase* node);

        void skip_this_line();

        [[nodiscard]]
        int peek_char();
        [[nodiscard]]
        int read_char();
        [[nodiscard]]
        peeked_token_t* peek();
        void move_forward();
        void consume_forward();
        [[nodiscard]]
        bool try_handle_macro(const std::string& macro_name);
        [[nodiscard]]
        bool has_error() const;
        [[nodiscard]]
        wo_pstring_t get_source_path() const;
        [[nodiscard]]
        size_t get_error_frame_count_for_debug() const;
        [[nodiscard]]
        const declared_macro_map_t& get_declared_macro_list_for_debug() const;

        void begin_trying_block();
        void end_trying_block();

        [[nodiscard]]
        const std::optional<lexer*>& get_who_import_me() const;

        void get_now_location(size_t* out_row, size_t* out_col) const;
    };
}

#ifdef ANSI_WIDE_CHAR_SIGN
#   undef ANSI_WIDE_CHAR_SIGN
#   define ANSI_WIDE_CHAR_SIGN /* NOTHING */
#endif