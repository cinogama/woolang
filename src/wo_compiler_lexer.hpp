#pragma once

#include "wo_assert.hpp"

#include <string>
#include <vector>
#include <map>
#include <set>
#include <stack>
#include <unordered_map>

#include <cwctype>
#include <cwchar>

#include "wo_lang_compiler_information.hpp"
#include "wo_source_file_manager.hpp"
#include "wo_const_string_pool.hpp"

#ifdef ANSI_WIDE_CHAR_SIGN
#undef ANSI_WIDE_CHAR_SIGN
#define ANSI_WIDE_CHAR_SIGN L
#endif

namespace wo
{
    namespace ast
    {
        class ast_base;
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
        l_literal_real,         // 0.2  0.  .235
        l_literal_string,       // "" "helloworld" @"println("hello");"@
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
        l_value_assign,               // :=
        l_value_add_assign,           // +:=
        l_value_sub_assign,           // -:= 
        l_value_mul_assign,           // *:=
        l_value_div_assign,           // /:= 
        l_value_mod_assign,           // %:= 
        l_equal,                // ==
        l_not_equal,            // !=
        l_larg_or_equal,        // >=
        l_less_or_equal,        // <=
        l_less,                 // <
        l_larg,                 // >
        l_land,                 // &&
        l_lor,                  // ||
        l_or,                   // |
        l_lnot,                  // !
        l_scopeing,             // ::
        l_template_using_begin,             // ::<
        l_typecast,              // :
        l_index_point,          // .
        l_double_index_point,          // ..  may be used? hey..
        l_variadic_sign,          // ...
        l_index_begin,          // '['
        l_index_end,            // ']'
        l_direct,               // '->'
        l_inv_direct,
        l_function_result,      // '=>'
        l_bind_monad,           // '>>'
        l_map_monad,            // '>>'
        l_left_brackets,        // (
        l_right_brackets,       // )
        l_left_curly_braces,    // {
        l_right_curly_braces,   // }
        l_question,   // ?
        l_import,               // import
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

    class lexer;

    class macro
    {
    public:
        std::wstring macro_name;
        wo_vm _macro_action_vm;

        macro(lexer& lex);

        ~macro()
        {
            if (_macro_action_vm)
                wo_close_vm(_macro_action_vm);
        }
    };

    class lexer
    {
    public:
        struct lex_operator_info
        {
            lex_type in_lexer_type;
        };
        struct lex_keyword_info
        {
            lex_type in_lexer_type;
        };

    public:
        std::unique_ptr<std::wistream> reading_buffer;

        size_t        now_file_rowno;
        size_t        now_file_colno;

        size_t        next_file_rowno;
        size_t        next_file_colno;

        int         format_string_count;
        int         curly_count;

        wo_pstring_t   source_file;

        std::unordered_set<wo_pstring_t> imported_file_list;
        std::unordered_set<uint64_t> imported_file_crc64_list;
        std::vector<ast::ast_base*> imported_ast;

        std::shared_ptr<std::unordered_map<std::wstring, std::shared_ptr<macro>>> used_macro_list;

        bool has_been_imported(wo_pstring_t full_path)
        {
            if (imported_file_list.find(full_path) == imported_file_list.end())
                imported_file_list.insert(full_path);
            else
                return true;
            return false;
        }
        bool has_been_imported(uint64_t crc64)
        {
            if (imported_file_crc64_list.find(crc64) == imported_file_crc64_list.end())
                imported_file_crc64_list.insert(crc64);
            else
                return true;
            return false;
        }
        void append_import_file_ast(ast::ast_base* astbase)
        {
            imported_ast.push_back(astbase);
        }
    private:
        inline const static std::map<std::wstring, lex_operator_info> lex_operator_list =
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
            {L"<|",     {lex_type::l_inv_direct}},
            {L"=>",     {lex_type::l_function_result }},
            {L"=>>",    {lex_type::l_bind_monad }},
            {L"->>",    {lex_type::l_map_monad }},
            {L"@",      {lex_type::l_at }},
            {L"?",      {lex_type::l_question }},
            {L"\\",     {lex_type::l_lambda}},
            {L"λ",      {lex_type::l_lambda}},
        };
        inline const static std::map<std::wstring, lex_keyword_info> key_word_list =
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
    public:
        static const wchar_t* lex_is_operate_type(lex_type tt)
        {
            for (auto& [op_str, op_type] : lex_operator_list)
            {
                if (op_type.in_lexer_type == tt)
                    return op_str.c_str();
            }
            return nullptr;
        }
        static const wchar_t* lex_is_keyword_type(lex_type tt)
        {
            for (auto& [op_str, op_type] : key_word_list)
            {
                if (op_type.in_lexer_type == tt)
                    return op_str.c_str();
            }
            return nullptr;
        }
        static lex_type lex_is_valid_operator(const std::wstring& op)
        {
            if (lex_operator_list.find(op) != lex_operator_list.end())
            {
                return lex_operator_list.at(op).in_lexer_type;
            }
            return lex_type::l_error;
        }
        static lex_type lex_is_keyword(const std::wstring& op)
        {
            if (key_word_list.find(op) != key_word_list.end())
            {
                return key_word_list.at(op).in_lexer_type;
            }
            return lex_type::l_error;
        }
        static bool lex_isoperatorch(int ch)
        {
            const static std::set<wchar_t> operator_char_set = []() {
                std::set<wchar_t> _result;
                for (auto& [_operator, operator_info] : lex_operator_list)
                    for (wchar_t wch : _operator)
                        _result.insert(wch);
                return _result;
            }();
            return operator_char_set.find((wchar_t)ch) != operator_char_set.end();
        }
        static bool lex_isspace(int ch)
        {
            if (ch == EOF)
                return false;

            return ch == 0 || iswspace((wchar_t)ch);
        }
        static bool lex_isalpha(int ch)
        {
            // if ch can used as begin of identifier, return true
            if (ch == EOF)
                return false;

            // according to ISO-30112, Chinese character is belonging alpha-set,
            // so we can use 'iswalpha' directly.
            return iswalpha((wchar_t)ch);
        }
        static bool lex_isidentbeg(int ch)
        {
            // if ch can used as begin of identifier, return true
            if (ch == EOF)
                return false;

            // according to ISO-30112, Chinese character is belonging alpha-set,
            // so we can use 'iswalpha' directly.
            return iswalpha((wchar_t)ch) || (wchar_t)ch == L'_';
        }
        static bool lex_isident(int ch)
        {
            // if ch can used as begin of identifier, return true
            if (ch == EOF)
                return false;

            // according to ISO-30112, Chinese character is belonging alpha-set,
            // so we can use 'iswalpha' directly.
            return iswalnum((wchar_t)ch) || (wchar_t)ch == L'_';
        }
        static bool lex_isalnum(int ch)
        {
            // if ch can used in identifier (not first character), return true
            if (ch == EOF)
                return false;

            // according to ISO-30112, Chinese character is belonging alpha-set,
            // so we can use 'iswalpha' directly.
            return iswalnum((wchar_t)ch);
        }
        static bool lex_isdigit(int ch)
        {
            if (ch == EOF)
                return false;

            return iswdigit((wchar_t)ch);
        }
        static bool lex_isxdigit(int ch)
        {
            if (ch == EOF)
                return false;

            return iswxdigit((wchar_t)ch);
        }
        static bool lex_isodigit(int ch)
        {
            if (ch == EOF)
                return false;

            return (wchar_t)ch >= L'0' && (wchar_t)ch <= L'7';
        }
        static int lex_toupper(int ch)
        {
            if (ch == EOF)
                return EOF;

            return towupper((wchar_t)ch);
        }
        static int lex_tolower(int ch)
        {
            if (ch == EOF)
                return EOF;

            return towlower((wchar_t)ch);
        }
        static int lex_hextonum(int ch)
        {
            wo_assert(lex_isxdigit(ch));

            if (iswdigit((wchar_t)ch))
            {
                return (wchar_t)ch - L'0';
            }
            return towupper((wchar_t)ch) - L'A' + 10;
        }

    public:
        lexer(const lexer&) = delete;
        lexer(lexer&&) = delete;

        lexer& operator = (const lexer&) = delete;
        lexer& operator = (lexer&&) = delete;

        lexer(const std::wstring& content, const std::string _source_file);
        lexer(std::optional<std::unique_ptr<std::wistream>>&& stream, const std::string _source_file);
    public:
        enum class errorlevel
        {
            error,
            infom,
        };

        struct lex_error_msg
        {
            errorlevel error_level;
            size_t   begin_row;
            size_t   end_row;
            size_t   begin_col;
            size_t   end_col;
            std::wstring describe;
            std::string filename;

            std::wstring to_wstring(bool need_ansi_describe = true)
            {
                using namespace std;

                if (need_ansi_describe)
                    return (
                        error_level == errorlevel::error
                        ? (ANSI_HIR L"error" ANSI_RST)
                        : (ANSI_HIC L"infom" ANSI_RST)
                        )
                    + (L" (" + std::to_wstring(end_row) + L"," + std::to_wstring(end_col))
                    + (L") " + describe);
                else
                    return (
                        error_level == errorlevel::error
                        ? (L"error")
                        : (L"info")
                        )
                    + (L" (" + std::to_wstring(end_row) + L"," + std::to_wstring(end_col))
                    + (L") " + describe);
            }
        };

        std::vector<lex_error_msg> lex_error_list;
        std::vector<std::vector<lex_error_msg>> error_frame;
        size_t error_frame_offset = 0;

        void begin_trying_block()
        {
            error_frame.push_back({});
        }
        void end_trying_block()
        {
            error_frame.pop_back();
        }
        void set_eror_at(size_t id)
        {
            error_frame_offset = id;
        }

        std::vector<lex_error_msg>& get_cur_error_frame()
        {
            wo_assert(error_frame_offset <= error_frame.size());
            if (error_frame.empty() || error_frame_offset == error_frame.size())
                return lex_error_list;
            return error_frame[error_frame.size() - error_frame_offset - 1];
        }

        bool just_have_err = false; // it will be clear at next()

        template<typename AstT, typename ... TS>
        lex_error_msg make_error(lexer::errorlevel errorlevel, AstT* tree_node, const wchar_t* fmt, TS&& ... args)
        {
            size_t begin_row_no = tree_node->row_begin_no ? tree_node->row_begin_no : now_file_rowno;
            size_t begin_col_no = tree_node->col_begin_no ? tree_node->col_begin_no : now_file_colno;
            size_t end_row_no = tree_node->row_end_no ? tree_node->row_end_no : next_file_rowno;
            size_t end_col_no = tree_node->col_end_no ? tree_node->col_end_no : next_file_colno;

            wchar_t describe[256] = {};
            swprintf(describe, 255, fmt, args...);

            return
                lex_error_msg
            {
                errorlevel,
                begin_row_no,
                end_row_no,
                begin_col_no,
                end_col_no,
                describe,
                tree_node->source_file ? wstr_to_str(*tree_node->source_file) : "?"
            };
        }

        template<typename ... TS>
        lex_type lex_error(lexer::errorlevel errorlevel, const wchar_t* fmt, TS&& ... args)
        {
            wchar_t describe[256] = {};
            swprintf(describe, 255, fmt, args...);

            lex_error_msg msg = lex_error_msg
            {
                errorlevel,
                now_file_rowno,
                next_file_rowno,
                now_file_colno,
                next_file_colno,
                describe,
                source_file ? wstr_to_str(*source_file) : "json"
            };
            just_have_err = true;
            get_cur_error_frame().emplace_back(msg);
            skip_error_line();
            return lex_type::l_error;
        }

        template<typename ... TS>
        lex_type parser_error(lexer::errorlevel errorlevel, const wchar_t* fmt, TS&& ... args)
        {
            wchar_t describe[256] = {};
            swprintf(describe, 255, fmt, args...);

            lex_error_msg msg = lex_error_msg
            {
                errorlevel,
                now_file_rowno,
                next_file_rowno,
                now_file_colno,
                next_file_colno,
                describe,
                wstr_to_str(*source_file)
            };

            just_have_err = true;
            get_cur_error_frame().emplace_back(msg);
            return lex_type::l_error;
        }

        template<typename AstT, typename ... TS>
        lex_type lang_error(lexer::errorlevel errorlevel, AstT* tree_node, const wchar_t* fmt, TS&& ... args)
        {
            if (tree_node->source_file == nullptr)
                return parser_error(errorlevel, fmt, args...);

            size_t begin_row_no = tree_node->row_begin_no ? tree_node->row_begin_no : now_file_rowno;
            size_t begin_col_no = tree_node->col_begin_no ? tree_node->col_begin_no : now_file_colno;
            size_t end_row_no = tree_node->row_end_no ? tree_node->row_end_no : next_file_rowno;
            size_t end_col_no = tree_node->col_end_no ? tree_node->col_end_no : next_file_colno;

            wchar_t describe[256] = {};
            swprintf(describe, 255, fmt, args...);

            lex_error_msg msg = lex_error_msg
            {
                errorlevel,
                begin_row_no,
                end_row_no,
                begin_col_no,
                end_col_no,
                describe,
                wstr_to_str(*tree_node->source_file)
            };

            just_have_err = true;
            get_cur_error_frame().emplace_back(msg);

            return lex_type::l_error;
        }
        bool has_error() const
        {
            return !lex_error_list.empty();
        }

    public:
        std::stack<std::pair<lex_type, std::wstring>> temp_token_buff_stack;
        lex_type peek_result_type = lex_type::l_error;
        std::wstring peek_result_str;
        bool peeked_flag = false;
        size_t after_pick_now_file_rowno;
        size_t after_pick_now_file_colno;
        size_t after_pick_next_file_rowno;
        size_t after_pick_next_file_colno;

        size_t this_time_peek_from_rowno;
        size_t this_time_peek_from_colno;

        lex_type peek(std::wstring* out_literal);
        int peek_ch();
        int next_ch();
        void new_line();
        void skip_error_line();
        int next_one();
        int peek_one();

        lex_type try_handle_macro(std::wstring* out_literal, lex_type result_type, const std::wstring& result_str, bool workinpeek);
        lex_type next(std::wstring* out_literal);
        void push_temp_for_error_recover(lex_type type, const std::wstring& out_literal);
    };
}

#ifdef ANSI_WIDE_CHAR_SIGN
#undef ANSI_WIDE_CHAR_SIGN
#define ANSI_WIDE_CHAR_SIGN /* NOTHING */
#endif