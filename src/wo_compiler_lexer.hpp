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

#include "enum.h"

#include "wo_lang_compiler_information.hpp"
#include "wo_source_file_manager.hpp"
#include "wo_const_string_pool.hpp"

#ifdef ANSI_WIDE_CHAR_SIGN
#undef ANSI_WIDE_CHAR_SIGN
#define ANSI_WIDE_CHAR_SIGN L
#endif

namespace wo
{
    BETTER_ENUM(lex_type, int,
        l_eof = -1,
        l_error = 0,
        l_empty,          // [empty]
        l_identifier,           // identifier.
        l_literal_integer,      // 1 233 0x123456 0b1101001 032
        l_literal_handle,       // 0L 256L 0xFFL
        l_literal_real,         // 0.2  0.  .235
        l_literal_string,       // "" "helloworld" @"println("hello");"@
        l_literal_char,         // 'x'
        l_format_string,        // f"..{  /  }..{ 
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
        l_function_result,      // '=>'
        l_bind_monad,           // '>>'
        l_map_monad,            // '>>'
        l_left_brackets,        // (
        l_right_brackets,       // )
        l_left_curly_braces,    // {
        l_right_curly_braces,   // }
        l_question,   // ?
        l_import,               // import
        l_inf,
        l_nil,
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
        l_where,
        l_operator,
        l_union,
        l_match,
        l_struct
    );

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
        std::wstring  reading_buffer;  // use wide char-code.
        size_t        next_reading_index;

        size_t        now_file_rowno;
        size_t        now_file_colno;

        size_t        next_file_rowno;
        size_t        next_file_colno;

        int         format_string_count;
        int         curly_count;

        wo_pstring_t   source_file;

        std::unordered_set<wo_pstring_t> imported_file_list;
        std::unordered_set<size_t> imported_file_crc64_list;

        std::shared_ptr<std::unordered_map<std::wstring, std::shared_ptr<macro>>> used_macro_list;

        bool has_been_imported(wo_pstring_t full_path)
        {
            if (imported_file_list.find(full_path) == imported_file_list.end())
                imported_file_list.insert(full_path);
            else
                return true;
            return false;
        }
        bool has_been_imported(size_t crc64)
        {
            if (imported_file_crc64_list.find(crc64) == imported_file_crc64_list.end())
                imported_file_crc64_list.insert(crc64);
            else
                return true;
            return false;
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
            {L"..",      {lex_type::l_double_index_point}},
            {L"...",      {lex_type::l_variadic_sign}},
            {L"[",      {lex_type::l_index_begin}},
            {L"]",      {lex_type::l_index_end}},
            {L"->",      {lex_type::l_direct }},
            {L"=>",      {lex_type::l_function_result }},
            {L"=>>",      {lex_type::l_bind_monad }},
            {L"->>",      {lex_type::l_map_monad }},
            {L"@",      {lex_type::l_at }},
            {L"?",      {lex_type::l_question }},
            {L"\\",      {lex_type::l_lambda}},
            {L"λ",      {lex_type::l_lambda}},
        };

        inline const static std::map<std::wstring, lex_keyword_info> key_word_list =
        {
            {L"import", {lex_type::l_import}},
            {L"inf", {lex_type::l_inf}},
            {L"nil", {lex_type::l_nil}},
            {L"while", {lex_type::l_while}},
            {L"for", {lex_type::l_for}},
            {L"if", {lex_type::l_if}},
            {L"else", {lex_type::l_else}},
            {L"let", {lex_type::l_let }},
            {L"mut", {lex_type::l_mut}},
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
            {L"struct", {lex_type::l_struct}}
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

            return ch == 0 || iswspace(ch);
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
        static bool lex_isidentbeg(int ch)
        {
            // if ch can used as begin of identifier, return true
            if (ch == EOF)
                return false;

            // according to ISO-30112, Chinese character is belonging alpha-set,
            // so we can use 'iswalpha' directly.
            return iswalpha(ch) || ch == L'_';
        }
        static bool lex_isident(int ch)
        {
            // if ch can used as begin of identifier, return true
            if (ch == EOF)
                return false;

            // according to ISO-30112, Chinese character is belonging alpha-set,
            // so we can use 'iswalpha' directly.
            return iswalnum(ch) || ch == L'_';
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
            wo_assert(lex_isxdigit(ch));

            if (isdigit(ch))
            {
                return ch - L'0';
            }
            return toupper(ch) - L'A' + 10;
        }

    public:
        lexer(const std::wstring& wstr, const std::string _source_file)
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
        lexer(const std::string _source_file)
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
            if (!wo::read_virtual_source(&reading_buffer, &readed_real_path, input_path))
            {
                lex_error(0x0000, WO_ERR_CANNOT_OPEN_FILE, input_path.c_str());
            }
            else
            {
                source_file = wstring_pool::get_pstr(readed_real_path);
            }
        }
    public:
        struct lex_error_msg
        {
            uint32_t errorno;
            size_t   begin_row;
            size_t   end_row;
            size_t   begin_col;
            size_t   end_col;
            std::wstring describe;
            std::string filename;

            std::wstring to_wstring(bool need_ansi_describe = true)
            {
                using namespace std;

                wchar_t error_num_str[10] = {};
                swprintf(error_num_str, 10, L"%04X", errorno);
                if (need_ansi_describe)
                    return (ANSI_HIR L"error" ANSI_RST)
                    + (L" (" + std::to_wstring(end_row) + L"," + std::to_wstring(end_col))
                    + (L") " + describe);
                else
                    return (L"error")
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
        lex_error_msg make_error(uint32_t errorno, AstT* tree_node, const wchar_t* fmt, TS&& ... args)
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
                0x1000 + errorno,
                begin_row_no,
                end_row_no,
                begin_col_no,
                end_col_no,
                describe,
                tree_node->source_file ? wstr_to_str(*tree_node->source_file) : "?"
            };
        }

        template<typename ... TS>
        lex_type lex_error(uint32_t errorno, const wchar_t* fmt, TS&& ... args)
        {
            wchar_t describe[256] = {};
            swprintf(describe, 255, fmt, args...);

            lex_error_msg msg = lex_error_msg
            {
                0x1000 + errorno,
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
        lex_type parser_error(uint32_t errorno, const wchar_t* fmt, TS&& ... args)
        {
            wchar_t describe[256] = {};
            swprintf(describe, 255, fmt, args...);

            lex_error_msg msg = lex_error_msg
            {
                0x1000 + errorno,
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
        lex_type lang_error(uint32_t errorno, AstT* tree_node, const wchar_t* fmt, TS&& ... args)
        {
            if (tree_node->source_file == nullptr)
                return parser_error(errorno, fmt, args...);

            size_t begin_row_no = tree_node->row_begin_no ? tree_node->row_begin_no : now_file_rowno;
            size_t begin_col_no = tree_node->col_begin_no ? tree_node->col_begin_no : now_file_colno;
            size_t end_row_no = tree_node->row_end_no ? tree_node->row_end_no : next_file_rowno;
            size_t end_col_no = tree_node->col_end_no ? tree_node->col_end_no : next_file_colno;

            wchar_t describe[256] = {};
            swprintf(describe, 255, fmt, args...);

            lex_error_msg msg = lex_error_msg
            {
                0x1000 + errorno,
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

        void reset()
        {
            next_reading_index = (0);
            now_file_rowno = (1);
            now_file_colno = (0);
            next_file_rowno = (1);
            next_file_colno = (1);
            format_string_count = (0);
            curly_count = (0);

            lex_error_list.clear();

            decltype(error_frame) tmp;
            error_frame.swap(tmp);
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

        lex_type peek(std::wstring* out_literal)
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

        int peek_ch()
        {
            if (next_reading_index >= reading_buffer.size())
                return EOF;

            return reading_buffer[next_reading_index];
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

        //lex_type next(std::wstring* out_literal)
        //{
        //    std::wstring fff;
        //    auto fffff = _next(&fff);
        //    if (lex_enable_error_warn)
        //        wo_wstdout << ANSI_HIG << "GET! " << fffff._to_string() << " " << fff << ANSI_RST << std::endl;
        //    if (out_literal)*out_literal = fff;
        //    return fffff;
        //}
        lex_type try_handle_macro(std::wstring* out_literal, lex_type result_type, const std::wstring& result_str, bool workinpeek)
        {
            // ATTENTION: out_literal may point to result_str, please make sure donot read result_str after modify *out_literal.
            if (result_type == +lex_type::l_identifier)
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
                            peeked_flag = true;
                            peek_result_type = next(&peek_result_str);
                            if (out_literal)
                                *out_literal = peek_result_str;
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
        lex_type next(std::wstring* out_literal)
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
                            lex_error(0x0005, WO_ERR_MISMATCH_ANNO_SYM);
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
                        return lex_error(0x0006, WO_ERR_RECURSIVE_FORMAT_STRING_IS_INVALID);

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
                                    lex_error(0x0001, WO_ERR_UNKNOW_ESCSEQ_BEGIN_WITH_CH, escape_ch);
                                    write_result(escape_ch);
                                    break;
                                }
                            }
                            else
                                write_result(following_ch);
                        }
                        else
                            return lex_error(0x0002, WO_ERR_UNEXCEPTED_EOL_IN_STRING);
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
                                lex_error(0x0001, WO_ERR_UNKNOW_ESCSEQ_BEGIN_WITH_CH, escape_ch);
                                write_result(escape_ch);
                                break;
                            }
                        }
                        else
                            write_result(following_ch);
                    }
                    else
                        return lex_error(0x0002, WO_ERR_UNEXCEPTED_EOL_IN_STRING);
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
                        return lex_error(0x0001, WO_ERR_UNEXCEPT_CH_AFTER_CH, sec_ch, readed_ch);
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
                                return lex_error(0x0001, WO_ERR_UNEXCEPT_CH_AFTER_CH, following_chs, readed_ch);
                            is_real = true;
                        }
                        else if (lex_toupper(following_chs) == L'H')
                        {
                            if (is_real)
                                return lex_error(0x0001, WO_ERR_UNEXCEPT_CH_AFTER_CH, following_chs, readed_ch);
                            next_one();
                            is_handle = true;
                            break;
                        }
                        else if (lex_isalnum(following_chs))
                            return lex_error(0x0004, WO_ERR_ILLEGAL_LITERAL);
                        else
                            break;                  // end read
                    }
                    else if (base == 16)
                    {
                        if (lex_isxdigit(following_chs) || lex_toupper(following_chs) == L'X')
                            write_result(next_one());
                        else if (following_chs == L'.')
                            return lex_error(0x0001, WO_ERR_UNEXCEPT_CH_AFTER_CH, following_chs, readed_ch);
                        else if (lex_toupper(following_chs) == L'H')
                        {
                            next_one();
                            is_handle = true;
                            break;
                        }
                        else if (lex_isalnum(following_chs))
                            return lex_error(0x0004, WO_ERR_ILLEGAL_LITERAL);
                        else
                            break;                  // end read
                    }
                    else if (base == 8)
                    {
                        if (lex_isodigit(following_chs))
                            write_result(next_one());
                        else if (following_chs == L'.')
                            return lex_error(0x0001, WO_ERR_UNEXCEPT_CH_AFTER_CH, following_chs, readed_ch);
                        else if (lex_toupper(following_chs) == L'H')
                        {
                            next_one();
                            is_handle = true;
                            break;
                        }
                        else if (lex_isalnum(following_chs))
                            return lex_error(0x0004, WO_ERR_ILLEGAL_LITERAL);
                        else
                            break;                  // end read
                    }
                    else if (base == 2)
                    {
                        if (following_chs == L'1' || following_chs == L'0' || lex_toupper(following_chs) == L'B')
                            write_result(next_one());
                        else if (following_chs == L'.')
                            return lex_error(0x0001, WO_ERR_UNEXCEPT_CH_AFTER_CH, following_chs, readed_ch);
                        else if (lex_toupper(following_chs) == L'H')
                        {
                            next_one();
                            is_handle = true;
                        }
                        else if (lex_isalnum(following_chs))
                            return lex_error(0x0004, WO_ERR_ILLEGAL_LITERAL);
                        else
                            break;                  // end read
                    }
                    else
                        return lex_error(0x0000, WO_ERR_LEXER_ERR_UNKNOW_NUM_BASE);

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
                            return lex_error(0x0002, WO_ERR_UNEXCEPT_EOF);
                    }
                }
                else
                    goto checking_valid_operator;
                /*return lex_error(0x0001, WO_ERR_UNEXCEPT_CH_AFTER_CH_EXCEPT_CH, tmp_ch, readed_ch, L'\"');*/
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
                                lex_error(0x0001, WO_ERR_UNKNOW_ESCSEQ_BEGIN_WITH_CH, escape_ch);
                                write_result(escape_ch);
                                break;
                            }
                        }
                        else
                            write_result(following_ch);
                    }
                    else
                        return lex_error(0x0002, WO_ERR_UNEXCEPTED_EOL_IN_STRING);
                }
            }
            else if (readed_ch == L'\'')
            {
                int following_ch;

                following_ch = next_one();
                if (following_ch == L'\'')
                    return lex_error(0x0001, WO_ERR_UNEXCEPT_CH_AFTER_CH, L'\'', L'\'');
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
                            lex_error(0x0001, WO_ERR_UNKNOW_ESCSEQ_BEGIN_WITH_CH, escape_ch);
                            write_result(escape_ch);
                            break;
                        }
                    }
                    else
                        write_result(following_ch);
                }
                else
                    return lex_error(0x0002, WO_ERR_UNEXCEPTED_EOL_IN_CHAR);

                following_ch = next_one();
                if (following_ch == L'\'')
                    return lex_type::l_literal_char;
                else
                    return lex_error(0x0001, WO_ERR_LEXER_ERR_UNKNOW_BEGIN_CH L" " WO_TERM_EXCEPTED L" '\''", following_ch);
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
                    return lex_error(0x0003, WO_ERR_UNKNOW_OPERATOR_STR, read_result().c_str());

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
                        lex_error(0x0000, WO_ERR_UNKNOWN_REPEAT_MACRO_DEFINE, p->macro_name.c_str());
                    else
                        (*this->used_macro_list)[p->macro_name] = p;

                    goto re_try_read_next_one;
                }
                else
                {
                    lex_error(0x0000, WO_ERR_UNKNOWN_PRAGMA_COMMAND, pragma_name.c_str());
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
                    return try_handle_macro(out_literal, lex_type::l_identifier, read_result(), false);
                else
                    return keyword_type;
            }
            ///////////////////////////////////////////////////////////////////////////////////////
            return lex_error(0x000, WO_ERR_LEXER_ERR_UNKNOW_BEGIN_CH, readed_ch);
        }

        void push_temp_for_error_recover(lex_type type, const std::wstring& out_literal)
        {
            temp_token_buff_stack.push({ type ,out_literal });
        }
    };

    inline macro::macro(lexer& lex)
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
                L"import woo.macro; extern func macro_" +
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
                lex.lex_error(0x0000, WO_ERR_FAILED_TO_COMPILE_MACRO_CONTROLOR,
                    str_to_wstr(wo_get_compile_error(_macro_action_vm, WO_NOTHING)).c_str());

                wo_close_vm(_macro_action_vm);
                _macro_action_vm = nullptr;
            }
            else
                wo_run(_macro_action_vm);
        }
        else
            lex.lex_error(0x0000, WO_ERR_HERE_SHOULD_HAVE, L"{");

    }
}

#ifdef ANSI_WIDE_CHAR_SIGN
#undef ANSI_WIDE_CHAR_SIGN
#define ANSI_WIDE_CHAR_SIGN /* NOTHING */
#endif