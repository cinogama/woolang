#include "wo_lang_ast.hpp"

namespace wo
{
#ifndef WO_DISABLE_COMPILER
    namespace ast
    {
        void init_builder();

        struct astnode_builder
        {
            using ast_basic = wo::ast::AstBase;
            using inputs_t = std::vector<grammar::produce>;
            using builder_func_t = std::function<grammar::produce(lexer&, const inputs_t&)>;

            virtual ~astnode_builder() = default;
            static grammar::produce build(lexer& lex, const inputs_t& input)
            {
                wo_assert(false, "");
                return nullptr;
            }
        };

        inline std::unordered_map<size_t, size_t> _registed_builder_function_id_list;
        inline std::vector<astnode_builder::builder_func_t> _registed_builder_function;

        template <typename T>
        size_t _register_builder()
        {
            static_assert(std::is_base_of<astnode_builder, T>::value);
            _registed_builder_function.push_back(T::build);
            return _registed_builder_function.size();
        }

        template <typename T>
        size_t index()
        {
            size_t idx = _registed_builder_function_id_list[meta::type_hash<T>];
            wo_assert(idx != 0);
            return idx;
        }

        inline astnode_builder::builder_func_t get_builder(size_t idx)
        {
            wo_assert(idx != 0);
            return _registed_builder_function[idx - 1];
        }
    }

#define WO_NEED_TOKEN(ID)           input[(ID)].read_token()
#define WO_NEED_AST(ID)             input[(ID)].read_ast()
#define WO_NEED_AST_TYPE(ID, TYPE)  input[(ID)].read_ast(TYPE)
#define WO_NEED_AST_VALUE(ID)       static_cast<AstValueBase*>(input[(ID)].read_ast_value())
#define WO_NEED_AST_PATTERN(ID)     static_cast<AstPatternBase*>(input[(ID)].read_ast_pattern())
#define WO_IS_TOKEN(ID)             input[(ID)].is_token()
#define WO_IS_AST(ID)               input[(ID)].is_ast()
#define WO_IS_EMPTY(ID)             AstEmpty::is_empty(input[(ID)])

    namespace ast
    {
        template <size_t pass_idx>
        struct pass_direct : public astnode_builder
        {
            static grammar::produce build(lexer& lex, const inputs_t& input)
            {
                auto& token_or_ast = input[pass_idx];
                if (input.size() > 1 && token_or_ast.is_ast())
                {
                    AstBase* ast = token_or_ast.read_ast();
                    ast->source_location.source_file = nullptr;
                }
                return token_or_ast;
            }
        };

        template <size_t pass_idx>
        struct pass_direct_keep_source_location : public astnode_builder
        {
            static grammar::produce build(lexer& lex, const inputs_t& input)
            {
                return input[pass_idx];
            }
        };

        template <size_t first_node>
        struct pass_create_list : public astnode_builder
        {
            static grammar::produce build(lexer& lex, const inputs_t& input)
            {
                AstList* result = new AstList();
                if (!WO_IS_EMPTY(first_node))
                {
                    result->m_list.push_back(WO_NEED_AST(first_node));
                }

                return result;
            }
        };

        template <size_t from, size_t to_list>
        struct pass_append_list : public astnode_builder
        {
            static_assert(from != to_list, "from and to_list should not be the same");

            static grammar::produce build(lexer& lex, const inputs_t& input)
            {
                AstList* list = static_cast<AstList*>(WO_NEED_AST_TYPE(to_list, AstBase::AST_LIST));
                if (!WO_IS_EMPTY(from))
                {
                    if (from < to_list)
                        list->m_list.push_front(WO_NEED_AST(from));
                    else
                        list->m_list.push_back(WO_NEED_AST(from));
                }

                // Update source location.
                list->source_location.source_file = nullptr;
                return list;
            }
        };

        template <size_t idx>
        struct pass_sentence_block : public astnode_builder
        {
            static grammar::produce build(lexer& lex, const inputs_t& input)
            {
                auto* node = WO_NEED_AST(idx);
                if (node->node_type == AstBase::AST_SCOPE)
                    return node;

                return new AstScope(node);
            }
        };

#define WO_AST_BUILDER(NAME)                            \
struct NAME : public astnode_builder                    \
{                                                       \
    static grammar::produce build(                      \
        lexer& lex,                                     \
        const ast::astnode_builder::inputs_t& input);   \
}

        WO_AST_BUILDER(pass_mark_label);
        WO_AST_BUILDER(pass_import_files);
        WO_AST_BUILDER(pass_using_namespace);
        WO_AST_BUILDER(pass_empty);
        WO_AST_BUILDER(pass_token);
        WO_AST_BUILDER(pass_enum_item_create);
        WO_AST_BUILDER(pass_enum_finalize);
        WO_AST_BUILDER(pass_namespace);
        WO_AST_BUILDER(pass_using_type_as);
        WO_AST_BUILDER(pass_using_typename_space);
        WO_AST_BUILDER(pass_alias_type_as);
        WO_AST_BUILDER(pass_build_where_constraint);
        WO_AST_BUILDER(pass_func_def_named);
        WO_AST_BUILDER(pass_func_def_oper);
        WO_AST_BUILDER(pass_func_lambda_ml);
        WO_AST_BUILDER(pass_func_lambda);
        WO_AST_BUILDER(pass_break);
        WO_AST_BUILDER(pass_break_label);
        WO_AST_BUILDER(pass_continue);
        WO_AST_BUILDER(pass_continue_label);
        WO_AST_BUILDER(pass_return);
        WO_AST_BUILDER(pass_return_value);
        WO_AST_BUILDER(pass_return_lambda);
        WO_AST_BUILDER(pass_if);
        WO_AST_BUILDER(pass_while);
        WO_AST_BUILDER(pass_for_defined);
        WO_AST_BUILDER(pass_for_expr);
        WO_AST_BUILDER(pass_foreach);
        WO_AST_BUILDER(pass_mark_mut);
        WO_AST_BUILDER(pass_mark_immut);
        WO_AST_BUILDER(pass_typeof);
        WO_AST_BUILDER(pass_build_identifier_typeof);
        WO_AST_BUILDER(pass_build_identifier_normal);
        WO_AST_BUILDER(pass_build_identifier_global);
        WO_AST_BUILDER(pass_type_nil);
        WO_AST_BUILDER(pass_type_func);
        WO_AST_BUILDER(pass_type_struct_field);
        WO_AST_BUILDER(pass_type_struct);
        WO_AST_BUILDER(pass_type_from_identifier);
        WO_AST_BUILDER(pass_type_tuple);
        WO_AST_BUILDER(pass_type_mutable);
        WO_AST_BUILDER(pass_type_immutable);
        WO_AST_BUILDER(pass_attribute);
        WO_AST_BUILDER(pass_attribute_append);
        WO_AST_BUILDER(pass_pattern_for_assign);
        WO_AST_BUILDER(pass_reverse_vardef);
        WO_AST_BUILDER(pass_func_argument);
        WO_AST_BUILDER(pass_do_void_cast);
        WO_AST_BUILDER(pass_assign_operation);
        WO_AST_BUILDER(pass_binary_operation);
        WO_AST_BUILDER(pass_literal);
        WO_AST_BUILDER(pass_literal_char);
        WO_AST_BUILDER(pass_typeid);
        WO_AST_BUILDER(pass_unary_operation);
        WO_AST_BUILDER(pass_variable);
        WO_AST_BUILDER(pass_cast_type);
        WO_AST_BUILDER(pass_format_finish);
        WO_AST_BUILDER(pass_format_cast_string);
        WO_AST_BUILDER(pass_format_connect);
        WO_AST_BUILDER(pass_build_bind_monad);
        WO_AST_BUILDER(pass_build_map_monad);
        WO_AST_BUILDER(pass_normal_function_call);
        WO_AST_BUILDER(pass_directly_function_call);
        WO_AST_BUILDER(pass_directly_function_call_append_arguments);
        WO_AST_BUILDER(pass_inverse_function_call);
        WO_AST_BUILDER(pass_union_item);
        WO_AST_BUILDER(pass_union_item_constructor);
        WO_AST_BUILDER(pass_union_declare);
        WO_AST_BUILDER(pass_union_pattern_identifier_or_takeplace);
        WO_AST_BUILDER(pass_union_pattern_contain_element);
        WO_AST_BUILDER(pass_match_union_case);
        WO_AST_BUILDER(pass_match);
        WO_AST_BUILDER(pass_pattern_identifier_or_takepace);
        WO_AST_BUILDER(pass_pattern_mut_identifier_or_takepace);
        WO_AST_BUILDER(pass_pattern_identifier_or_takepace_with_template);
        WO_AST_BUILDER(pass_pattern_mut_identifier_or_takepace_with_template);
        WO_AST_BUILDER(pass_pattern_tuple);
        WO_AST_BUILDER(pass_variable_define_item);
        WO_AST_BUILDER(pass_variable_defines);
        WO_AST_BUILDER(pass_conditional_expression);
        WO_AST_BUILDER(pass_check_type_as);
        WO_AST_BUILDER(pass_check_type_is);
        WO_AST_BUILDER(pass_struct_member_init_pair);
        WO_AST_BUILDER(pass_struct_instance);
        WO_AST_BUILDER(pass_array_instance);
        WO_AST_BUILDER(pass_vec_instance);
        WO_AST_BUILDER(pass_dict_instance);
        WO_AST_BUILDER(pass_map_instance);
        WO_AST_BUILDER(pass_tuple_instance);
        WO_AST_BUILDER(pass_dict_field_init_pair);
        WO_AST_BUILDER(pass_index_operation_regular);
        WO_AST_BUILDER(pass_index_operation_member);
        WO_AST_BUILDER(pass_expand_arguments);
        WO_AST_BUILDER(pass_variadic_arguments_pack);
        WO_AST_BUILDER(pass_extern);
        WO_AST_BUILDER(pass_func_def_extn);
        WO_AST_BUILDER(pass_func_def_extn_oper);

#undef WO_AST_BUILDER
    }
#endif
}