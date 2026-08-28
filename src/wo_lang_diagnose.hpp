#pragma once
// LANG-STAGE TYPED DIAGNOSES
//
// Diagnoses whose payloads hold pointers to lang instances (type / value /
// symbol), all alive until the end of the compilation: their display names
// are resolved by render() at the end of a failed compile, so a suppressed
// diagnose never pays for name lookup and the reported name always reflects
// the final state of the instance.
//
// This header must be included after wo_lang.hpp's types are complete (it is
// included at the end of wo_lang.hpp).

namespace wo
{
    namespace diagnose
    {
        struct err_expected_template_argument final
        {
            lang_Symbol* m_symbol;
            std::string render(LangContext* lang) const
            {
                return format(u8"`%s` 需要指定泛型参数", lang->get_symbol_name(m_symbol));
            }
            bool operator==(const err_expected_template_argument& a) const
            {
                return m_symbol == a.m_symbol;
            }
        };

        struct err_unexpected_template_argument final
        {
            lang_Symbol* m_symbol;
            std::string render(LangContext* lang) const
            {
                return format(u8"`%s` 不是泛型目标, 不需要泛型参数",
                    lang->get_symbol_name(m_symbol));
            }
            bool operator==(const err_unexpected_template_argument& a) const
            {
                return m_symbol == a.m_symbol;
            }
        };

        struct err_cannot_use_builtin_typename_here final
        {
            lang_Symbol* m_symbol;
            std::string render(LangContext* lang) const
            {
                return format(u8"内置类型名 `%s` 不能在此处作为类型使用",
                    lang->get_symbol_name(m_symbol));
            }
            bool operator==(const err_cannot_use_builtin_typename_here& a) const
            {
                return m_symbol == a.m_symbol;
            }
        };

        struct err_unused_variable final
        {
            lang_Symbol* m_symbol;
            std::string render(LangContext* lang) const
            {
                return format(u8"局部变量 `%s` 已声明但未使用",
                    lang->get_symbol_name(m_symbol));
            }
            bool operator==(const err_unused_variable& a) const
            {
                return m_symbol == a.m_symbol;
            }
        };

        struct err_ambiguous_target_named final
        {
            std::string m_name;
            lang_Symbol* m_selected;
            std::string render(LangContext* lang) const
            {
                return format(u8"引用不明确: `%s` 可能指代多个目标 (已选择 `%s`)",
                    m_name.c_str(), lang->get_symbol_name(m_selected));
            }
            bool operator==(const err_ambiguous_target_named& a) const
            {
                return m_name == a.m_name && m_selected == a.m_selected;
            }
        };

        struct err_unexpected_match_type_for_tuple final
        {
            lang_TypeInstance* m_type;
            std::string render(LangContext* lang) const
            {
                return format(u8"此处期待一个元组类型，但是提供的是 `%s`",
                    lang->get_type_name(m_type));
            }
            bool operator==(const err_unexpected_match_type_for_tuple& a) const
            {
                return m_type == a.m_type;
            }
        };

        struct err_type_named_determined_failed final
        {
            lang_TypeInstance* m_type;
            std::string render(LangContext* lang) const
            {
                return format(u8"类型 `%s` 尚未完全确定, 无法进行类型检查",
                    lang->get_type_name(m_type));
            }
            bool operator==(const err_type_named_determined_failed& a) const
            {
                return m_type == a.m_type;
            }
        };

        struct err_unmatched_return_type_named final
        {
            lang_TypeInstance* m_actual;
            lang_TypeInstance* m_expected;
            std::string render(LangContext* lang) const
            {
                return format(u8"返回类型不匹配：实际返回 `%s`, 但声明或推导为 `%s`",
                    lang->get_type_name(m_actual), lang->get_type_name(m_expected));
            }
            bool operator==(const err_unmatched_return_type_named& a) const
            {
                return m_actual == a.m_actual && m_expected == a.m_expected;
            }
        };

        struct err_unmatched_array_element_type_named final
        {
            lang_TypeInstance* m_actual;
            lang_TypeInstance* m_expected;
            std::string render(LangContext* lang) const
            {
                return format(u8"数组元素类型不匹配：当前为 `%s`, 但预期为 `%s`",
                    lang->get_type_name(m_actual), lang->get_type_name(m_expected));
            }
            bool operator==(const err_unmatched_array_element_type_named& a) const
            {
                return m_actual == a.m_actual && m_expected == a.m_expected;
            }
        };

        struct err_unmatched_dict_key_type_named final
        {
            lang_TypeInstance* m_actual;
            lang_TypeInstance* m_expected;
            std::string render(LangContext* lang) const
            {
                return format(u8"字典键类型不匹配：当前为 `%s`, 但预期为 `%s`",
                    lang->get_type_name(m_actual), lang->get_type_name(m_expected));
            }
            bool operator==(const err_unmatched_dict_key_type_named& a) const
            {
                return m_actual == a.m_actual && m_expected == a.m_expected;
            }
        };

        struct err_unmatched_dict_value_type_named final
        {
            lang_TypeInstance* m_actual;
            lang_TypeInstance* m_expected;
            std::string render(LangContext* lang) const
            {
                return format(u8"字典值类型不匹配：当前为 `%s`, 但预期为 `%s`",
                    lang->get_type_name(m_actual), lang->get_type_name(m_expected));
            }
            bool operator==(const err_unmatched_dict_value_type_named& a) const
            {
                return m_actual == a.m_actual && m_expected == a.m_expected;
            }
        };

        struct err_unindexable_type_named final
        {
            lang_TypeInstance* m_type;
            std::string render(LangContext* lang) const
            {
                return format(u8"类型 `%s` 不支持索引操作", lang->get_type_name(m_type));
            }
            bool operator==(const err_unindexable_type_named& a) const
            {
                return m_type == a.m_type;
            }
        };

        struct err_cannot_index_type_with_type final
        {
            lang_TypeInstance* m_container;
            lang_TypeInstance* m_indexer;
            std::string render(LangContext* lang) const
            {
                return format(u8"类型 `%s` 不支持使用 `%s` 类型作为索引",
                    lang->get_type_name(m_container), lang->get_type_name(m_indexer));
            }
            bool operator==(const err_cannot_index_type_with_type& a) const
            {
                return m_container == a.m_container && m_indexer == a.m_indexer;
            }
        };

        struct err_only_expand_array_vec_and_tuple final
        {
            lang_TypeInstance* m_type;
            std::string render(LangContext* lang) const
            {
                return format(u8"只能展开数组、向量或元组, 当前类型为 `%s`",
                    lang->get_type_name(m_type));
            }
            bool operator==(const err_only_expand_array_vec_and_tuple& a) const
            {
                return m_type == a.m_type;
            }
        };

        struct err_only_expand_tuple final
        {
            lang_TypeInstance* m_type;
            std::string render(LangContext* lang) const
            {
                return format(u8"只能展开元组, 当前尝试展开 `%s`",
                    lang->get_type_name(m_type));
            }
            bool operator==(const err_only_expand_tuple& a) const
            {
                return m_type == a.m_type;
            }
        };

        struct err_struct_do_not_have_member_named final
        {
            lang_TypeInstance* m_struct;
            std::string m_member;
            std::string render(LangContext* lang) const
            {
                return format(u8"结构体 `%s` 没有成员 `%s`",
                    lang->get_type_name(m_struct), m_member.c_str());
            }
            bool operator==(const err_struct_do_not_have_member_named& a) const
            {
                return m_struct == a.m_struct && m_member == a.m_member;
            }
        };

        struct err_tuple_index_out_of_range final
        {
            lang_TypeInstance* m_tuple;
            size_t m_count;
            int64_t m_index;
            std::string render(LangContext* lang) const
            {
                return format(u8"元组 `%s` 只有 %zu 个元素, 索引 %lld 越界",
                    lang->get_type_name(m_tuple), m_count, (long long)m_index);
            }
            bool operator==(const err_tuple_index_out_of_range& a) const
            {
                return m_tuple == a.m_tuple && m_count == a.m_count && m_index == a.m_index;
            }
        };

        struct err_pattern_variable_should_be_mutable final
        {
            lang_ValueInstance* m_variable;
            std::string render(LangContext* lang) const
            {
                return format(u8"不能对不可变变量 `%s` 赋值",
                    lang->get_value_name(m_variable));
            }
            bool operator==(const err_pattern_variable_should_be_mutable& a) const
            {
                return m_variable == a.m_variable;
            }
        };

        struct err_cannot_cast_type_to_type final
        {
            lang_TypeInstance* m_from;
            lang_TypeInstance* m_to;
            std::string render(LangContext* lang) const
            {
                return format(u8"类型转换失败: 无法从类型 `%s` 转换为 `%s`",
                    lang->get_type_name(m_from), lang->get_type_name(m_to));
            }
            bool operator==(const err_cannot_cast_type_to_type& a) const
            {
                return m_from == a.m_from && m_to == a.m_to;
            }
        };

        struct err_failed_to_deduce_not_func_param_type final
        {
            lang_TypeInstance* m_type;
            std::string render(LangContext* lang) const
            {
                return format(u8"泛型函数类型匹配失败: 期望函数类型但实际为 `%s`",
                    lang->get_type_name(m_type));
            }
            bool operator==(const err_failed_to_deduce_not_func_param_type& a) const
            {
                return m_type == a.m_type;
            }
        };

        struct err_not_all_template_argument_determined final
        {
            std::string m_pending_list;
            std::string render(LangContext* lang) const
            {
                return format(u8"泛型参数不完整: 参数 <%s> 无法自动推导",
                    m_pending_list.c_str());
            }
            bool operator==(const err_not_all_template_argument_determined& a) const
            {
                return m_pending_list == a.m_pending_list;
            }
        };

        struct err_target_type_is_not_a_function final
        {
            lang_TypeInstance* m_type;
            std::string render(LangContext* lang) const
            {
                return format(u8"调用目标类型错误: `%s` 不是函数类型",
                    lang->get_type_name(m_type));
            }
            bool operator==(const err_target_type_is_not_a_function& a) const
            {
                return m_type == a.m_type;
            }
        };

        struct err_argument_too_much final
        {
            size_t m_given;
            size_t m_accepted;
            std::string render(LangContext* lang) const
            {
                return format(u8"函数调用参数过多: 提供了 %zu 个参数, 但只接受 %zu 个",
                    m_given, m_accepted);
            }
            bool operator==(const err_argument_too_much& a) const
            {
                return m_given == a.m_given && m_accepted == a.m_accepted;
            }
        };

        struct err_argument_too_less final
        {
            size_t m_given;
            size_t m_needed;
            std::string render(LangContext* lang) const
            {
                return format(u8"函数调用参数不足: 提供了 %zu 个参数, 但需要 %zu 个",
                    m_given, m_needed);
            }
            bool operator==(const err_argument_too_less& a) const
            {
                return m_given == a.m_given && m_needed == a.m_needed;
            }
        };

        struct err_type_not_accepted_named final
        {
            lang_TypeInstance* m_actual;
            lang_TypeInstance* m_expected;
            std::string render(LangContext* lang) const
            {
                return format(u8"类型不匹配: 无法将类型 `%s` 用于此处 (期望 `%s`)",
                    lang->get_type_name(m_actual), lang->get_type_name(m_expected));
            }
            bool operator==(const err_type_not_accepted_named& a) const
            {
                return m_actual == a.m_actual && m_expected == a.m_expected;
            }
        };

        struct err_operator_as_result_type_not_accepted_named final
        {
            lang_TypeInstance* m_result;
            lang_TypeInstance* m_target;
            std::string render(LangContext* lang) const
            {
                return format(u8"类型不匹配: operator as 的返回类型 `%s` 无法转换到目标类型 `%s`",
                    lang->get_type_name(m_result), lang->get_type_name(m_target));
            }
            bool operator==(const err_operator_as_result_type_not_accepted_named& a) const
            {
                return m_result == a.m_result && m_target == a.m_target;
            }
        };

        struct err_cannot_cast_type_named_from_dynamic final
        {
            lang_TypeInstance* m_target;
            std::string render(LangContext* lang) const
            {
                return format(u8"无法在运行时将 dynamic 值转换为类型 `%s`",
                    lang->get_type_name(m_target));
            }
            bool operator==(const err_cannot_cast_type_named_from_dynamic& a) const
            {
                return m_target == a.m_target;
            }
        };

        struct err_type_named_is_not_struct final
        {
            lang_TypeInstance* m_type;
            std::string render(LangContext* lang) const
            {
                return format(u8"类型错误: `%s` 不是结构体类型", lang->get_type_name(m_type));
            }
            bool operator==(const err_type_named_is_not_struct& a) const
            {
                return m_type == a.m_type;
            }
        };

        struct err_unacceptable_type_in_operate final
        {
            lang_TypeInstance* m_operand;
            std::string m_operator;
            std::string render(LangContext* lang) const
            {
                return format(u8"类型 `%s` 不支持 `%s` 运算",
                    lang->get_type_name(m_operand), m_operator.c_str());
            }
            bool operator==(const err_unacceptable_type_in_operate& a) const
            {
                return m_operand == a.m_operand && m_operator == a.m_operator;
            }
        };

        struct err_unacceptable_type_in_cond final
        {
            lang_TypeInstance* m_type;
            std::string render(LangContext* lang) const
            {
                return format(u8"条件表达式错误: 必须是布尔类型 (当前为 `%s`)",
                    lang->get_type_name(m_type));
            }
            bool operator==(const err_unacceptable_type_in_cond& a) const
            {
                return m_type == a.m_type;
            }
        };

        struct err_unexpected_matching_type final
        {
            lang_TypeInstance* m_type;
            std::string render(LangContext* lang) const
            {
                return format(u8"匹配类型错误: match 语句只支持 union 类型 (当前为 `%s`)",
                    lang->get_type_name(m_type));
            }
            bool operator==(const err_unexpected_matching_type& a) const
            {
                return m_type == a.m_type;
            }
        };

        struct err_unexists_case_named_in_match final
        {
            std::string m_tag;
            lang_TypeInstance* m_union;
            std::string render(LangContext* lang) const
            {
                return format(u8"匹配项不存在: `%s` 不是 union 类型 `%s` 的成员",
                    m_tag.c_str(), lang->get_type_name(m_union));
            }
            bool operator==(const err_unexists_case_named_in_match& a) const
            {
                return m_tag == a.m_tag && m_union == a.m_union;
            }
        };

        struct err_have_not_value_case_in_match final
        {
            lang_TypeInstance* m_union;
            std::string m_tag;
            std::string render(LangContext* lang) const
            {
                return format(u8"匹配类型 `%s` 时，匹配项 `%s` 必须包含值参数",
                    lang->get_type_name(m_union), m_tag.c_str());
            }
            bool operator==(const err_have_not_value_case_in_match& a) const
            {
                return m_union == a.m_union && m_tag == a.m_tag;
            }
        };

        struct err_have_value_case_in_match final
        {
            lang_TypeInstance* m_union;
            std::string m_tag;
            std::string render(LangContext* lang) const
            {
                return format(u8"匹配类型 `%s` 时，匹配项 `%s` 不应包含值参数",
                    lang->get_type_name(m_union), m_tag.c_str());
            }
            bool operator==(const err_have_value_case_in_match& a) const
            {
                return m_union == a.m_union && m_tag == a.m_tag;
            }
        };

        struct err_failed_reification_caused_by final
        {
            std::string m_argument_list;
            lang_Symbol* m_symbol;
            std::string render(LangContext* lang) const
            {
                return format(u8"泛型实例化失败: 无法使用参数 <%s> 实例化 `%s`",
                    m_argument_list.c_str(), lang->get_symbol_name(m_symbol));
            }
            bool operator==(const err_failed_reification_caused_by& a) const
            {
                return m_argument_list == a.m_argument_list && m_symbol == a.m_symbol;
            }
        };

        struct err_symbol_is_private final
        {
            lang_Symbol* m_symbol;
            std::string m_where;
            std::string render(LangContext* lang) const
            {
                return format(u8"符号 `%s` 是私有的，只能在 `%s` 内访问",
                    lang->get_symbol_name(m_symbol), m_where.c_str());
            }
            bool operator==(const err_symbol_is_private& a) const
            {
                return m_symbol == a.m_symbol && m_where == a.m_where;
            }
        };

        struct err_symbol_is_protected final
        {
            lang_Symbol* m_symbol;
            std::string m_where;
            std::string render(LangContext* lang) const
            {
                return format(u8"符号 `%s` 是受保护的，只能在命名空间 `%s` 内访问",
                    lang->get_symbol_name(m_symbol), m_where.c_str());
            }
            bool operator==(const err_symbol_is_protected& a) const
            {
                return m_symbol == a.m_symbol && m_where == a.m_where;
            }
        };

        struct err_source_must_be_imported final
        {
            lang_Symbol* m_symbol;
            std::string m_source_file;
            std::string render(LangContext* lang) const
            {
                return format(u8"无法访问 `%s`，需要先导入脚本 `%s`",
                    lang->get_symbol_name(m_symbol), m_source_file.c_str());
            }
            bool operator==(const err_source_must_be_imported& a) const
            {
                return m_symbol == a.m_symbol && m_source_file == a.m_source_file;
            }
        };

        struct err_unable_to_mix_types final
        {
            lang_TypeInstance* m_left;
            lang_TypeInstance* m_right;
            std::string render(LangContext* lang) const
            {
                return format(u8"类型冲突: 无法同时兼容 `%s` 和 `%s` 类型",
                    lang->get_type_name(m_left), lang->get_type_name(m_right));
            }
            bool operator==(const err_unable_to_mix_types& a) const
            {
                return m_left == a.m_left && m_right == a.m_right;
            }
        };

        struct err_unacceptable_mutable final
        {
            lang_TypeInstance* m_type;
            std::string render(LangContext* lang) const
            {
                return format(u8"类型不匹配: 需要不可变类型，但实际为 `%s`",
                    lang->get_type_name(m_type));
            }
            bool operator==(const err_unacceptable_mutable& a) const
            {
                return m_type == a.m_type;
            }
        };

        struct err_variable_storage_not_determined final
        {
            lang_ValueInstance* m_value;
            std::string render(LangContext* lang) const
            {
                return format(u8"引用了未完成初始化的变量 `%s`",
                    lang->get_value_name(m_value));
            }
            bool operator==(const err_variable_storage_not_determined& a) const
            {
                return m_value == a.m_value;
            }
        };

        struct err_non_void_type_expr_as_stmt final
        {
            lang_TypeInstance* m_type;
            std::string render(LangContext* lang) const
            {
                return format(u8"非 void 类型的表达式(当前为 `%s`)不能单独作为语句，可能导致结果被忽略",
                    lang->get_type_name(m_type));
            }
            bool operator==(const err_non_void_type_expr_as_stmt& a) const
            {
                return m_type == a.m_type;
            }
        };

        struct info_maybe_named_defined_here final
        {
            lang_Symbol* m_symbol;
            std::string render(LangContext* lang) const
            {
                return format(u8"可能是此处定义的 `%s`", lang->get_symbol_name(m_symbol));
            }
            bool operator==(const info_maybe_named_defined_here& a) const
            {
                return m_symbol == a.m_symbol;
            }
        };

        struct info_maybe_named_defined_in_compiler final
        {
            lang_Symbol* m_symbol;
            std::string render(LangContext* lang) const
            {
                return format(u8"可能是编译器内置的 `%s`", lang->get_symbol_name(m_symbol));
            }
            bool operator==(const info_maybe_named_defined_in_compiler& a) const
            {
                return m_symbol == a.m_symbol;
            }
        };

        struct info_type_named_before_direct_sign final
        {
            lang_TypeInstance* m_type;
            std::string render(LangContext* lang) const
            {
                return format(u8"第一个参数为 `%s` 类型", lang->get_type_name(m_type));
            }
            bool operator==(const info_type_named_before_direct_sign& a) const
            {
                return m_type == a.m_type;
            }
        };

        struct info_this_value_is_type_named final
        {
            lang_TypeInstance* m_type;
            std::string render(LangContext* lang) const
            {
                return format(u8"当前表达式类型为 `%s`", lang->get_type_name(m_type));
            }
            bool operator==(const info_this_value_is_type_named& a) const
            {
                return m_type == a.m_type;
            }
        };

        struct info_captured_variable_used_here final
        {
            lang_ValueInstance* m_value;
            std::string render(LangContext* lang) const
            {
                return format(u8"引用了外部变量 `%s`", lang->get_value_name(m_value));
            }
            bool operator==(const info_captured_variable_used_here& a) const
            {
                return m_value == a.m_value;
            }
        };

        struct info_trying_refill_template_argument final
        {
            lang_TypeInstance* m_alias_type;
            lang_Symbol* m_alias_symbol;
            std::string render(LangContext* lang) const
            {
                return format(u8"正在为类型 `%s`(通过别名 `%s` 引用) 重新指定泛型参数",
                    lang->get_type_name(m_alias_type), lang->get_symbol_name(m_alias_symbol));
            }
            bool operator==(const info_trying_refill_template_argument& a) const
            {
                return m_alias_type == a.m_alias_type && m_alias_symbol == a.m_alias_symbol;
            }
        };

        struct info_old_function_return_type_is final
        {
            lang_TypeInstance* m_type;
            std::string render(LangContext* lang) const
            {
                return format(u8"函数先前返回类型被推断为 `%s`",
                    lang->get_type_name(m_type));
            }
            bool operator==(const info_old_function_return_type_is& a) const
            {
                return m_type == a.m_type;
            }
        };

        struct info_dependency_chain_function final
        {
            lang_ValueInstance* m_function;
            std::string render(LangContext* lang) const
            {
                return format(u8"依赖链：此处检查函数 `%s` 的定义",
                    lang->get_value_name(m_function));
            }
            bool operator==(const info_dependency_chain_function& a) const
            {
                return m_function == a.m_function;
            }
        };

        struct info_template_deduct_mismatch_between_param_and_arg final
        {
            std::string m_site_label;
            std::string m_formal_type_display;
            lang_TypeInstance* m_actual_type;
            std::string render(LangContext* lang) const
            {
                return format(u8"%s 的声明类型为 `%s`,但传入实参的类型为 `%s`",
                    m_site_label.c_str(), m_formal_type_display.c_str(),
                    lang->get_type_name(m_actual_type));
            }
            bool operator==(const info_template_deduct_mismatch_between_param_and_arg& a) const
            {
                return m_site_label == a.m_site_label
                    && m_formal_type_display == a.m_formal_type_display
                    && m_actual_type == a.m_actual_type;
            }
        };
    }
}
