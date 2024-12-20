#include "wo_lang_ast.hpp"

namespace wo
{
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

#define WO_AST_BUILDER(NAME)                            \
struct NAME : public ast::astnode_builder               \
{                                                       \
    static grammar::produce build(                      \
        lexer& lex,                                     \
        const ast::astnode_builder::inputs_t& input);   \
}

    namespace ast
    {
        template <size_t pass_idx>
        struct pass_direct : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                return input[pass_idx];
            }
        };

        template <size_t first_node>
        struct pass_create_list : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                AstList* result = new AstList();
                if (! WO_IS_EMPTY(first_node))
                {
                    result->m_list.push_back(WO_NEED_TOKEN(first_node));
                }

                return result;
            }
        };

        template <size_t from, size_t to_list>
        struct pass_append_list : public astnode_builder
        {
            static_assert(from != to_list, "from and to_list should not be the same");

            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                AstList* list = static_cast<AstList*>(WO_NEED_AST_TYPE(to_list, AstBase::AST_LIST));
                if (! WO_IS_EMPTY(from))
                {
                    if (from < to_list)
                        list->m_list.push_front(WO_NEED_TOKEN(from));
                    else
                        list->m_list.push_back(WO_NEED_TOKEN(from));                    
                }
            }
        };

        template <size_t idx>
        struct pass_sentence_block : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                auto* node = WO_NEED_AST(idx);
                if (node->node_type() == AstBase::AST_SCOPE)
                    return node;

                return new AstScope(node);
            }
        };

        WO_AST_BUILDER(pass_mark_label);
        WO_AST_BUILDER(pass_import_files);
        WO_AST_BUILDER(pass_using_namespace);
        WO_AST_BUILDER(pass_empty);
        WO_AST_BUILDER(pass_token);
        WO_AST_BUILDER(pass_enum_item_create);
        WO_AST_BUILDER(pass_enum_finalize);
        WO_AST_BUILDER(pass_namespace);
        WO_AST_BUILDER(pass_using_type_as);
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
        WO_AST_BUILDER(pass_type_tuple);
        WO_AST_BUILDER(pass_type_mutable);
        WO_AST_BUILDER(pass_type_immutable);
        WO_AST_BUILDER(pass_attribute);
        WO_AST_BUILDER(pass_attribute_append);
        WO_AST_BUILDER(pass_pattern_for_assign);
        WO_AST_BUILDER(pass_reverse_vardef);
        WO_AST_BUILDER(pass_func_argument);
    }
}