#pragma once
// This file using to store compiler/lexer`s error informations.

#include <string>
#include <vector>
#include <memory>
#include <cstdio>
#include <utility>
#include <type_traits>

#define WO_LANG_EN 0
#define WO_LANG_ZH_CN 1

#define WO_MAX_ERROR_COUNT 100

#ifndef WO_USED_LANGUAGE
#   define WO_USED_LANGUAGE WO_LANG_ZH_CN
#endif

#define WO_MSG_HERE u8"此处"
#define WO_MSG_SEE_HERE u8"参见"
#define WO_MSG_SEE_ALSO u8"另见"
#define WO_MSG_STACK_OVERFLOW u8"栈溢出"

#define WO_MSG_TOO_MANY_ERROR(count) \
    u8"错误数量过多(已达上限), 仅显示前 " + std::to_string(count) +  u8" 条错误, 编译终止"

namespace wo
{
    struct LangContext;

    namespace diagnose
    {
        // Deferred diagnostic payload. Only the data needed to build the final
        // message (AST source location is copied eagerly by the record entry,
        // here we hold things like interned names, counters and pointers to
        // lang instances, all alive until the end of the compilation) is
        // captured at the record site. The message text is rendered once, by
        // `render`, when a compile has definitively failed - suppressed
        // diagnostics and successful compiles never pay for it.
        struct lang_diagnose_t
        {
            virtual ~lang_diagnose_t() = default;

            // `lang` is the (still alive) LangContext of the failed compile,
            // or nullptr when the failure happened before any LangContext
            // existed (lexer/parser stage). Diagnoses holding lang instances
            // use it to resolve their names. Non-const: name lookup goes
            // through the context's caches.
            virtual std::string render(LangContext* lang) const = 0;
            virtual bool same_as(const lang_diagnose_t& another) const = 0;
        };

        // Polymorphic wrapper around a plain-aggregate diagnose payload
        // (see the struct definitions below): the payload keeps value
        // semantics - aggregate initialization, field-wise equality - and
        // only this adapter carries the virtual interface used by the
        // message frames.
        template<typename DataT>
        struct diagnose_model_t final
            : lang_diagnose_t
        {
            DataT m_data;

            explicit diagnose_model_t(DataT&& moved_data)
                : m_data(std::move(moved_data))
            {
            }

            std::string render(LangContext* lang) const
            {
                return m_data.render(lang);
            }
            bool same_as(const lang_diagnose_t& another) const override
            {
                if (auto* typed = dynamic_cast<const diagnose_model_t*>(&another))
                    return m_data == typed->m_data;
                return false;
            }
        };

        // printf-style formatting helper shared by render() implementations,
        // keeps the historical two-pass snprintf behavior (falls back to the
        // raw format string if snprintf fails).
        template<typename ... FmtArgTs>
        inline std::string format(const char* fmt, FmtArgTs&& ... format_args)
        {
            const int count = snprintf(nullptr, 0, fmt, format_args...);
            if (count < 0)
                return fmt;

            std::vector<char> buffer(count + 1);
            if (snprintf(buffer.data(), buffer.size(), fmt, format_args...) < 0)
                return fmt;

            return std::string(buffer.data());
        }

        // Detects diagnose payload types: any plain aggregate exposing
        // `std::string render(LangContext*) const`.
        template<typename T, typename = void>
        struct is_diagnose_t : std::false_type
        {
        };
        template<typename T>
        struct is_diagnose_t<T, std::void_t<
            decltype(std::declval<const T&>().render(
                static_cast<LangContext*>(nullptr)))>>
            : std::true_type
        {
        };

        // Escape hatch for runtime-produced dynamic texts (binary restore
        // failure descriptions, macro runtime errors, ...) that have no
        // structured fields.
        struct err_raw_message final
        {
            std::string m_text;

            std::string render(LangContext* lang) const
            {
                return m_text;
            }
            bool operator==(const err_raw_message& another) const
            {
                return m_text == another.m_text;
            }
        };
    }
}

// PASS LEXER
namespace wo::diagnose
{
    struct err_unexpected_ch_after_ch final
    {
        int m_ch;
        std::string render(LangContext* lang) const
        {
            return format(u8"数字字面量中出现了非法字符 `%c`", m_ch);
        }
        bool operator==(const err_unexpected_ch_after_ch& a) const { return m_ch == a.m_ch; }
    };

    struct err_unexpected_eof final
    {
        std::string render(LangContext* lang) const
        {
            return u8"意外的文件结束";
        }
        bool operator==(const err_unexpected_eof&) const { return true; }
    };

    struct err_unexpected_token final
    {
        std::string m_token;
        std::string render(LangContext* lang) const
        {
            return format(u8"意外的符号: `%s`", m_token.c_str());
        }
        bool operator==(const err_unexpected_token& a) const { return m_token == a.m_token; }
    };

    struct err_illegal_literal final
    {
        int m_ch;
        std::string render(LangContext* lang) const
        {
            return format(u8"数字字面量后出现非法字符 `%c`", m_ch);
        }
        bool operator==(const err_illegal_literal& a) const { return m_ch == a.m_ch; }
    };

    struct err_unknown_operator_str final
    {
        std::string m_operator;
        std::string render(LangContext* lang) const
        {
            return format(u8"未知的运算符: `%s`", m_operator.c_str());
        }
        bool operator==(const err_unknown_operator_str& a) const { return m_operator == a.m_operator; }
    };

    struct err_unexpected_eol_in_char final
    {
        std::string render(LangContext* lang) const
        {
            return u8"字符常量中不允许换行符";
        }
        bool operator==(const err_unexpected_eol_in_char&) const { return true; }
    };

    struct err_no_char_in_char final
    {
        std::string render(LangContext* lang) const
        {
            return u8"字符常量至少要有一个字符";
        }
        bool operator==(const err_no_char_in_char&) const { return true; }
    };

    struct err_too_many_char_in_char final
    {
        std::string render(LangContext* lang) const
        {
            return u8"字符常量只能包含一个字符";
        }
        bool operator==(const err_too_many_char_in_char&) const { return true; }
    };

    struct err_unexpected_eol_in_string final
    {
        std::string render(LangContext* lang) const
        {
            return u8"字符串常量中不允许换行符";
        }
        bool operator==(const err_unexpected_eol_in_string&) const { return true; }
    };

    struct err_macro_name_should_be_identifier final
    {
        std::string render(LangContext* lang) const
        {
            return u8"宏名称必须是有效的标识符";
        }
        bool operator==(const err_macro_name_should_be_identifier&) const { return true; }
    };

    struct err_unknown_repeat_macro_define final
    {
        std::string m_name;
        std::string render(LangContext* lang) const
        {
            return format(u8"重复定义的宏: `%s`", m_name.c_str());
        }
        bool operator==(const err_unknown_repeat_macro_define& a) const { return m_name == a.m_name; }
    };

    struct err_unknown_pragma_command final
    {
        std::string m_command;
        std::string render(LangContext* lang) const
        {
            return format(u8"未知的预处理指令: `%s`", m_command.c_str());
        }
        bool operator==(const err_unknown_pragma_command& a) const { return m_command == a.m_command; }
    };

    struct err_line_need_string_as_path final
    {
        std::string render(LangContext* lang) const
        {
            return u8"此处需要提供字符串字面量作为文件路径";
        }
        bool operator==(const err_line_need_string_as_path&) const { return true; }
    };

    struct err_line_need_integer_as_row final
    {
        std::string render(LangContext* lang) const
        {
            return u8"此处需要提供整数字面量作为行号";
        }
        bool operator==(const err_line_need_integer_as_row&) const { return true; }
    };

    struct err_line_need_integer_as_col final
    {
        std::string render(LangContext* lang) const
        {
            return u8"此处需要提供整数字面量作为列号";
        }
        bool operator==(const err_line_need_integer_as_col&) const { return true; }
    };

    struct err_failed_to_compile_macro_controlor final
    {
        std::string render(LangContext* lang) const
        {
            return u8"宏编译失败";
        }
        bool operator==(const err_failed_to_compile_macro_controlor&) const { return true; }
    };

    struct err_failed_to_run_macro_controlor final
    {
        std::string m_macro_name;
        std::string m_reason;
        std::string render(LangContext* lang) const
        {
            return format(u8"执行宏 `%s` 时发生错误: %s",
                m_macro_name.c_str(), m_reason.c_str());
        }
        bool operator==(const err_failed_to_run_macro_controlor& a) const
        {
            return m_macro_name == a.m_macro_name && m_reason == a.m_reason;
        }
    };

    struct err_unknown_escseq_begin_with_ch final
    {
        int m_ch;
        std::string render(LangContext* lang) const
        {
            return format(u8"未知的转义序列: `\\%c`", m_ch);
        }
        bool operator==(const err_unknown_escseq_begin_with_ch& a) const { return m_ch == a.m_ch; }
    };

    struct err_invalid_token_macro_controlor final
    {
        std::string m_macro_name;
        std::string render(LangContext* lang) const
        {
            return format(u8"宏 `%s` 生成了非法的词法标记", m_macro_name.c_str());
        }
        bool operator==(const err_invalid_token_macro_controlor& a) const { return m_macro_name == a.m_macro_name; }
    };

    struct err_recursive_format_string_is_invalid final
    {
        std::string render(LangContext* lang) const
        {
            return u8"不允许嵌套的格式化字符串";
        }
        bool operator==(const err_recursive_format_string_is_invalid&) const { return true; }
    };

    struct err_here_should_have final
    {
        std::string m_what;
        std::string render(LangContext* lang) const
        {
            return format(u8"此处缺少 `%s`", m_what.c_str());
        }
        bool operator==(const err_here_should_have& a) const { return m_what == a.m_what; }
    };

    struct err_lexer_err_unknown_num_base final
    {
        std::string render(LangContext* lang) const
        {
            return u8"词法错误: 未知的数字进制";
        }
        bool operator==(const err_lexer_err_unknown_num_base&) const { return true; }
    };

    struct err_source_cannot_be_empty final
    {
        std::string render(LangContext* lang) const
        {
            return u8"不能编译空的源代码";
        }
        bool operator==(const err_source_cannot_be_empty&) const { return true; }
    };
}

// PASS AST BUILDER
namespace wo::diagnose
{
    struct err_cannot_open_file final
    {
        std::string m_path;
        std::string render(LangContext* lang) const
        {
            return format(u8"无法打开文件 `%s`", m_path.c_str());
        }
        bool operator==(const err_cannot_open_file& a) const { return m_path == a.m_path; }
    };

    struct err_arg_define_after_variadic final
    {
        std::string render(LangContext* lang) const
        {
            return u8"可变参数 `...` 必须作为最后一个参数, 其后不能有其他参数定义";
        }
        bool operator==(const err_arg_define_after_variadic&) const { return true; }
    };

    struct err_arg_define_after_expand_vecarr final
    {
        std::string render(LangContext* lang) const
        {
            return u8"数组展开后不能再定义其他参数";
        }
        bool operator==(const err_arg_define_after_expand_vecarr&) const { return true; }
    };

    struct err_unknown_type final
    {
        std::string render(LangContext* lang) const
        {
            return u8"未知类型或类型定义不完整";
        }
        bool operator==(const err_unknown_type&) const { return true; }
    };

    struct err_redefined final
    {
        std::string m_name;
        std::string render(LangContext* lang) const
        {
            return format(u8"名称 `%s` 在当前作用域中已被定义", m_name.c_str());
        }
        bool operator==(const err_redefined& a) const { return m_name == a.m_name; }
    };

    struct err_repeat_attribute final
    {
        std::string render(LangContext* lang) const
        {
            return u8"特性修饰符重复";
        }
        bool operator==(const err_repeat_attribute&) const { return true; }
    };

    struct err_failed_to_create_tuple_with_vaarg final
    {
        std::string render(LangContext* lang) const
        {
            return u8"元组类型中不能使用可变参数 `...`";
        }
        bool operator==(const err_failed_to_create_tuple_with_vaarg&) const { return true; }
    };

    struct err_invalid_key_expr final
    {
        std::string render(LangContext* lang) const
        {
            return u8"映射的键必须是单个值表达式";
        }
        bool operator==(const err_invalid_key_expr&) const { return true; }
    };

    struct err_unknown_macro_named final
    {
        std::string m_name;
        std::string render(LangContext* lang) const
        {
            return format(u8"未定义或未完成的宏 `%s`", m_name.c_str());
        }
        bool operator==(const err_unknown_macro_named& a) const { return m_name == a.m_name; }
    };

    struct err_compiler_disabled final
    {
        std::string render(LangContext* lang) const
        {
            return u8"当前编译器功能未启用";
        }
        bool operator==(const err_compiler_disabled&) const { return true; }
    };

    struct err_cannot_start_namespace final
    {
        std::string render(LangContext* lang) const
        {
            return u8"当前位置不允许定义命名空间";
        }
        bool operator==(const err_cannot_start_namespace&) const { return true; }
    };

    struct err_cannot_using_unsafe final
    {
        std::string render(LangContext* lang) const
        {
            return u8"出于安全考虑, 不能直接使用 `unsafe` 命名空间";
        }
        bool operator==(const err_cannot_using_unsafe&) const { return true; }
    };
}

// LANG-STAGE DIAGNOSES WITHOUT LANG-INSTANCE FIELDS
// (Diagnoses whose fields reference wo_lang.hpp types live in
// wo_lang_diagnose.hpp, included at the end of wo_lang.hpp.)
namespace wo::diagnose
{
    struct err_repl_only final
    {
        std::string render(LangContext* lang) const
        {
            return u8"表达式末尾缺少分号";
        }
        bool operator==(const err_repl_only&) const { return true; }
    };

    struct err_unfound_type_named final
    {
        std::string m_name;
        std::string render(LangContext* lang) const
        {
            return format(u8"未找到类型 `%s`", m_name.c_str());
        }
        bool operator==(const err_unfound_type_named& a) const { return m_name == a.m_name; }
    };

    struct err_unfound_variable_named final
    {
        std::string m_name;
        std::string render(LangContext* lang) const
        {
            return format(u8"未找到变量、常量或函数 `%s`", m_name.c_str());
        }
        bool operator==(const err_unfound_variable_named& a) const { return m_name == a.m_name; }
    };

    struct err_unexpected_template_count final
    {
        size_t m_expected;
        size_t m_given;
        std::string render(LangContext* lang) const
        {
            return format(u8"需要 %zu 个泛型参数, 但提供了 %zu 个", m_expected, m_given);
        }
        bool operator==(const err_unexpected_template_count& a) const
        {
            return m_expected == a.m_expected && m_given == a.m_given;
        }
    };

    struct err_unexpected_match_count_for_tuple final
    {
        size_t m_expected;
        size_t m_given;
        std::string render(LangContext* lang) const
        {
            return format(u8"需要 %zu 个元素的元组, 但提供了 %zu 个元素", m_expected, m_given);
        }
        bool operator==(const err_unexpected_match_count_for_tuple& a) const
        {
            return m_expected == a.m_expected && m_given == a.m_given;
        }
    };

    struct err_value_type_determined_failed final
    {
        std::string render(LangContext* lang) const
        {
            return u8"无法确定表达式类型";
        }
        bool operator==(const err_value_type_determined_failed&) const { return true; }
    };

    struct err_type_determined_failed final
    {
        std::string render(LangContext* lang) const
        {
            return u8"无法确定类型";
        }
        bool operator==(const err_type_determined_failed&) const { return true; }
    };

    struct err_recursive_template_instance final
    {
        std::string m_instance_name;
        std::string render(LangContext* lang) const
        {
            return format(u8"类型推断循环依赖：泛型实例 `%s` 递归地依赖自身",
                m_instance_name.c_str());
        }
        bool operator==(const err_recursive_template_instance& a) const
        {
            return m_instance_name == a.m_instance_name;
        }
    };

    struct err_recursive_eval_pass1 final
    {
        std::string render(LangContext* lang) const
        {
            return u8"递归依赖：表达式依赖自身";
        }
        bool operator==(const err_recursive_eval_pass1&) const { return true; }
    };

    struct err_constraint_should_be_const final
    {
        std::string render(LangContext* lang) const
        {
            return u8"约束条件必须是常量表达式";
        }
        bool operator==(const err_constraint_should_be_const&) const { return true; }
    };

    struct err_constraint_should_be_bool final
    {
        std::string render(LangContext* lang) const
        {
            return u8"约束条件表达式必须是布尔类型";
        }
        bool operator==(const err_constraint_should_be_bool&) const { return true; }
    };

    struct err_extern_lib_should_be_constant final
    {
        std::string render(LangContext* lang) const
        {
            return u8"指定的外部库名应当是一个常量";
        }
        bool operator==(const err_extern_lib_should_be_constant&) const { return true; }
    };

    struct err_extern_name_should_be_constant final
    {
        std::string render(LangContext* lang) const
        {
            return u8"指定的外部函数符号名应当是一个常量";
        }
        bool operator==(const err_extern_name_should_be_constant&) const { return true; }
    };

    struct err_unable_to_find_extern_function final
    {
        std::string m_lib_name;
        std::string m_symbol_name;
        std::string render(LangContext* lang) const
        {
            return format(u8"在库 `%s` 中找不到外部函数 `%s`",
                m_lib_name.c_str(), m_symbol_name.c_str());
        }
        bool operator==(const err_unable_to_find_extern_function& a) const
        {
            return m_lib_name == a.m_lib_name && m_symbol_name == a.m_symbol_name;
        }
    };

    struct err_constraint_failed final
    {
        std::string render(LangContext* lang) const
        {
            return u8"约束条件不满足";
        }
        bool operator==(const err_constraint_failed&) const { return true; }
    };

    struct err_not_in_reification_template_func final
    {
        std::string render(LangContext* lang) const
        {
            return u8"无法在缺少泛型上下文的情况下实例化匿名泛型函数";
        }
        bool operator==(const err_not_in_reification_template_func&) const { return true; }
    };

    struct err_cannot_index_struct_with_non_const final
    {
        std::string render(LangContext* lang) const
        {
            return u8"结构体索引必须是常量";
        }
        bool operator==(const err_cannot_index_struct_with_non_const&) const { return true; }
    };

    struct err_cannot_index_tuple_with_non_const final
    {
        std::string render(LangContext* lang) const
        {
            return u8"元组索引必须是常量";
        }
        bool operator==(const err_cannot_index_tuple_with_non_const&) const { return true; }
    };

    struct err_repeated_field_named final
    {
        std::string m_field;
        std::string render(LangContext* lang) const
        {
            return format(u8"结构体字段重复: 字段名 `%s` 已被使用", m_field.c_str());
        }
        bool operator==(const err_repeated_field_named& a) const { return m_field == a.m_field; }
    };

    struct err_not_all_field_initialized final
    {
        std::string m_missing_fields;
        std::string render(LangContext* lang) const
        {
            return format(u8"结构体初始化不完整，缺少以下字段: %s", m_missing_fields.c_str());
        }
        bool operator==(const err_not_all_field_initialized& a) const
        {
            return m_missing_fields == a.m_missing_fields;
        }
    };

    struct err_too_much_field_initialized final
    {
        std::string render(LangContext* lang) const
        {
            return u8"结构体初始化错误: 提供的初始值过多";
        }
        bool operator==(const err_too_much_field_initialized&) const { return true; }
    };

    struct err_different_type_in_binary final
    {
        std::string m_operator;
        std::string render(LangContext* lang) const
        {
            return format(u8"二元运算符 `%s` 两侧的操作数类型不匹配", m_operator.c_str());
        }
        bool operator==(const err_different_type_in_binary& a) const { return m_operator == a.m_operator; }
    };

    struct err_string_index_out_of_range final
    {
        int64_t m_index;
        size_t m_length;
        std::string render(LangContext* lang) const
        {
            return format(u8"字符串索引 %lld 越界 (字符串长度为 %zu 个字符)",
                (long long)m_index, m_length);
        }
        bool operator==(const err_string_index_out_of_range& a) const
        {
            return m_index == a.m_index && m_length == a.m_length;
        }
    };

    struct err_takeplace_pattern_matched final
    {
        std::string render(LangContext* lang) const
        {
            return u8"匹配顺序错误: 占位模式(`_`)必须放在最后";
        }
        bool operator==(const err_takeplace_pattern_matched&) const { return true; }
    };

    struct err_all_cases_should_be_matched final
    {
        std::string m_missing_cases;
        std::string render(LangContext* lang) const
        {
            return format(u8"匹配不完整: 必须覆盖所有可能的分支，缺少以下分支: %s",
                m_missing_cases.c_str());
        }
        bool operator==(const err_all_cases_should_be_matched& a) const
        {
            return m_missing_cases == a.m_missing_cases;
        }
    };

    struct err_exists_case_named_in_match final
    {
        std::string m_tag;
        std::string render(LangContext* lang) const
        {
            return format(u8"匹配项重复: `%s` 已经存在于匹配语句中", m_tag.c_str());
        }
        bool operator==(const err_exists_case_named_in_match& a) const { return m_tag == a.m_tag; }
    };

    struct err_bad_div_zero final
    {
        std::string render(LangContext* lang) const
        {
            return u8"数学错误: 除数为零";
        }
        bool operator==(const err_bad_div_zero&) const { return true; }
    };

    struct err_bad_div_overflow final
    {
        std::string render(LangContext* lang) const
        {
            return u8"数学错误: 除法运算溢出";
        }
        bool operator==(const err_bad_div_overflow&) const { return true; }
    };

    struct err_value_should_be_const_for_template_arg final
    {
        std::string render(LangContext* lang) const
        {
            return u8"泛型参数错误: 必须使用常量值作为参数";
        }
        bool operator==(const err_value_should_be_const_for_template_arg&) const { return true; }
    };

    struct err_this_template_arg_should_be_type final
    {
        std::string render(LangContext* lang) const
        {
            return u8"泛型参数错误: 此处需要一个类型参数";
        }
        bool operator==(const err_this_template_arg_should_be_type&) const { return true; }
    };

    struct err_this_template_arg_should_be_const final
    {
        std::string render(LangContext* lang) const
        {
            return u8"泛型参数错误: 此处需要一个常量参数";
        }
        bool operator==(const err_this_template_arg_should_be_const&) const { return true; }
    };

    struct err_this_template_arg_should_not_be_nothing final
    {
        std::string render(LangContext* lang) const
        {
            return u8"泛型参数错误: 不能使用 `nothing` 类型的常量作为值参数";
        }
        bool operator==(const err_this_template_arg_should_not_be_nothing&) const { return true; }
    };

    struct err_defer_cannot_be_here final
    {
        std::string render(LangContext* lang) const
        {
            return u8"`defer` 语句必须位于可执行的作用域（函数体或代码块）内";
        }
        bool operator==(const err_defer_cannot_be_here&) const { return true; }
    };

    struct err_bad_break final
    {
        std::string render(LangContext* lang) const
        {
            return u8"`break` 语句只能在循环中使用";
        }
        bool operator==(const err_bad_break&) const { return true; }
    };

    struct err_bad_continue final
    {
        std::string render(LangContext* lang) const
        {
            return u8"`continue` 语句只能在循环中使用";
        }
        bool operator==(const err_bad_continue&) const { return true; }
    };

    struct err_bad_label_named final
    {
        std::string m_label;
        std::string render(LangContext* lang) const
        {
            return format(u8"找不到标签为 `%s` 的循环", m_label.c_str());
        }
        bool operator==(const err_bad_label_named& a) const { return m_label == a.m_label; }
    };

    struct err_bad_flow_ctrl_in_defer final
    {
        std::string m_what;
        std::string render(LangContext* lang) const
        {
            return format(u8"不能在 `defer` 语句中执行 `%s`", m_what.c_str());
        }
        bool operator==(const err_bad_flow_ctrl_in_defer& a) const { return m_what == a.m_what; }
    };

    struct err_cannot_define_static_var_in_defer final
    {
        std::string render(LangContext* lang) const
        {
            return u8"不能在 `defer` 语句中定义静态变量";
        }
        bool operator==(const err_cannot_define_static_var_in_defer&) const { return true; }
    };

    struct err_function_may_no_return_value final
    {
        std::string render(LangContext* lang) const
        {
            return u8"函数存在未返回的分支";
        }
        bool operator==(const err_function_may_no_return_value&) const { return true; }
    };

    struct err_unable_capture_in_recursive_func final
    {
        std::string render(LangContext* lang) const
        {
            return u8"递归函数中不允许捕获外部变量";
        }
        bool operator==(const err_unable_capture_in_recursive_func&) const { return true; }
    };

    struct err_unexpected_packedargs final
    {
        std::string render(LangContext* lang) const
        {
            return u8"变长参数使用错误: 只能在变长参数函数内部使用";
        }
        bool operator==(const err_unexpected_packedargs&) const { return true; }
    };

    struct err_cannot_unpack_here final
    {
        std::string render(LangContext* lang) const
        {
            return u8"参数包展开只能在函数参数列表或元组元素列表中使用";
        }
        bool operator==(const err_cannot_unpack_here&) const { return true; }
    };

    struct err_struct_field_is_private final
    {
        std::string m_field;
        std::string m_where;
        std::string render(LangContext* lang) const
        {
            return format(u8"结构体字段 `%s` 是私有的，只能在 `%s` 内访问",
                m_field.c_str(), m_where.c_str());
        }
        bool operator==(const err_struct_field_is_private& a) const
        {
            return m_field == a.m_field && m_where == a.m_where;
        }
    };

    struct err_struct_field_is_protected final
    {
        std::string m_field;
        std::string m_where;
        std::string render(LangContext* lang) const
        {
            return format(u8"结构体字段 `%s` 是受保护的，只能在命名空间 `%s` 内访问",
                m_field.c_str(), m_where.c_str());
        }
        bool operator==(const err_struct_field_is_protected& a) const
        {
            return m_field == a.m_field && m_where == a.m_where;
        }
    };

    struct info_symbol_named_defined_here final
    {
        std::string m_name;
        std::string render(LangContext* lang) const
        {
            return format(u8"符号 `%s` 在此定义", m_name.c_str());
        }
        bool operator==(const info_symbol_named_defined_here& a) const { return m_name == a.m_name; }
    };

    struct info_template_deduct_no_deduction_site final
    {
        std::string m_param;
        std::string render(LangContext* lang) const
        {
            return format(u8"泛型参数 <%s> 未出现在任何参数类型中,无法通过实参推导",
                m_param.c_str());
        }
        bool operator==(const info_template_deduct_no_deduction_site& a) const
        {
            return m_param == a.m_param;
        }
    };

    struct info_template_deduct_mismatch_at_position final
    {
        std::string m_position;
        std::string m_expected;
        std::string m_actual;
        std::string m_param;
        std::string render(LangContext* lang) const
        {
            return format(u8"%s不匹配: 期望 `%s`,实际为 `%s`,无法由此推导泛型参数 <%s>",
                m_position.c_str(), m_expected.c_str(),
                m_actual.c_str(), m_param.c_str());
        }
        bool operator==(const info_template_deduct_mismatch_at_position& a) const
        {
            return m_position == a.m_position && m_expected == a.m_expected
                && m_actual == a.m_actual && m_param == a.m_param;
        }
    };

    struct info_dependency_chain_template_instance final
    {
        std::string m_instance_name;
        std::string render(LangContext* lang) const
        {
            return format(u8"依赖链：此处开始求值泛型实例 `%s` 的定义",
                m_instance_name.c_str());
        }
        bool operator==(const info_dependency_chain_template_instance& a) const
        {
            return m_instance_name == a.m_instance_name;
        }
    };

    struct info_dependency_chain_other_template_instance final
    {
        std::string render(LangContext* lang) const
        {
            return u8"依赖链：此处求值了另一个泛型实例";
        }
        bool operator==(const info_dependency_chain_other_template_instance&) const { return true; }
    };

    struct info_dependency_chain_anonymous_function final
    {
        std::string render(LangContext* lang) const
        {
            return u8"依赖链：此处检查一个函数定义";
        }
        bool operator==(const info_dependency_chain_anonymous_function&) const { return true; }
    };

    struct info_dependency_chain_where_constraints final
    {
        std::string render(LangContext* lang) const
        {
            return u8"依赖链：此处求值函数的 where 约束";
        }
        bool operator==(const info_dependency_chain_where_constraints&) const { return true; }
    };

    struct err_out_of_memory final
    {
        std::string render(LangContext* lang) const
        {
            return u8"内存不足，无法创建代码";
        }
        bool operator==(const err_out_of_memory&) const { return true; }
    };

    struct err_failed_to_deduce_template_type final
    {
        std::string render(LangContext* lang) const
        {
            return u8"泛型参数自动推导失败: 无法确定合适的类型参数";
        }
        bool operator==(const err_failed_to_deduce_template_type&) const { return true; }
    };

    struct err_pattern_index_should_be_mutable_type final
    {
        std::string render(LangContext* lang) const
        {
            return u8"索引结果不可变, 不能对此赋值";
        }
        bool operator==(const err_pattern_index_should_be_mutable_type&) const { return true; }
    };

    // Emitted instead of re-replaying a failed template instance's stashed
    // reason when the same diagnose has already made it into the final
    // report once (see LangContext::_collect_failed_template_instance).
    struct info_failure_reason_already_reported final
    {
        std::string render(LangContext* lang) const
        {
            return u8"该实例的失败原因已在之前的错误报告中说明";
        }
        bool operator==(const info_failure_reason_already_reported&) const { return true; }
    };
}

// Message-fragment macros (WO_MSG_*) used when rendering diagnostics; these
// are NOT diagnoses themselves, only building blocks for labels and dump-time
// texts (see lexer::compiler_message_t::to_string and the template deduction
// site labels).

// PASS0_1 TYPE CHECK PARSE(MSG) for template deduction diagnosis:
//   human readable positions(& sites) inside a declared type where a
//   deduction-blocking mismatch was located, e.g.
//   info_template_deduct_mismatch_at_position's m_position field.
#define WO_MSG_TEMPLATE_DEDUCT_POSITION_TYPE u8"类型"
#define WO_MSG_TEMPLATE_DEDUCT_RETURN_TYPE u8"返回类型"
#define WO_MSG_TEMPLATE_DEDUCT_RETURN_TYPE_NESTED u8"的返回类型"
#define WO_MSG_TEMPLATE_DEDUCT_PARAMETER_NO(no) u8"第 " + std::to_string(no) + u8" 个参数"
#define WO_MSG_TEMPLATE_DEDUCT_PARAMETER_NO_NESTED(no) u8"的第 " + std::to_string(no) + u8" 个参数"
#define WO_MSG_TEMPLATE_DEDUCT_TEMPLATE_ARGUMENT_NO(no) u8"第 " + std::to_string(no) + u8" 个泛型实参"
#define WO_MSG_TEMPLATE_DEDUCT_TEMPLATE_ARGUMENT_NO_NESTED(no) u8"的第 " + std::to_string(no) + u8" 个泛型实参"
#define WO_MSG_TEMPLATE_DEDUCT_ELEMENT_NO(no) u8"第 " + std::to_string(no) + u8" 个元素"
#define WO_MSG_TEMPLATE_DEDUCT_ELEMENT_NO_NESTED(no) u8"的第 " + std::to_string(no) + u8" 个元素"
#define WO_MSG_TEMPLATE_DEDUCT_FIELD_NAME(name) u8"字段 `" + name + u8"`"
#define WO_MSG_TEMPLATE_DEDUCT_FIELD_NAME_NESTED(name) u8"的字段 `" + name + u8"`"
#define WO_MSG_TEMPLATE_DEDUCT_PARAMETER_NAME(no, name) u8"参数 " + std::to_string(no) + u8" (`" + name + u8"`)"
