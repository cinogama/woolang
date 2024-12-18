#include "wo_lang_ast_builder.hpp"

namespace wo
{
    namespace ast
    {
        auto pass_mark_label::build(lexer& lex, ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            token label = WO_NEED_TOKEN(0);
            auto* node = WO_NEED_AST(2);

            return new AstLabeled(wstring_pool::get_pstr(label.identifier), node);
        }
        auto pass_import_files::build(lexer& lex, ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            wo_error("not implemented");
            return token{ lex_type::l_error };
        }
        auto pass_using_namespace::build(lexer& lex, ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            auto* used_namespace = static_cast<AstValueVariable*>(WO_NEED_AST_TYPE(1, AstBase::AST_VALUE_VARIABLE));
            if (used_namespace->m_identifier.m_formal != Identifier::identifier_formal::FROM_CURRENT)
                return token{ lex.lang_error(lexer::errorlevel::error, used_namespace, L"TMPERROR：不合法的命名空间说明") };

            std::list<wo_pstring_t> used_namespaces = used_namespace->m_identifier.m_scope;
            used_namespaces.push_back(used_namespace->m_identifier.m_name);

            return new AstUsingNamespace(used_namespaces);
        }
        auto pass_empty::build(lexer& lex, ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            return new AstEmpty();
        }
        auto pass_token::build(lexer& lex, ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            return new AstToken(WO_NEED_TOKEN(0));
        }
        auto pass_enum_item_create::build(lexer& lex, ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            token name = WO_NEED_TOKEN(0);

            if (input.size() == 3)
            {
                // ITEM = $INTVAL
                auto* initval = WO_NEED_AST_VALUE(2);
                return new AstEnumItem(wstring_pool::get_pstr(name.identifier), initval);
            }
            return new AstEnumItem(wstring_pool::get_pstr(name.identifier), std::nullopt);
        }
        auto pass_enum_finalize::build(lexer& lex, ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            token enum_name = WO_NEED_TOKEN(2);
            AstList* enum_items = static_cast<AstList*>(WO_NEED_AST_TYPE(4, AstBase::AST_LIST));

            std::list<AstEnumItem*> items;
            for (auto& item : enum_items->m_list)
            {
                wo_assert(item->node_type == AstBase::AST_ENUM_ITEM);
                items.push_back(static_cast<AstEnumItem*>(item));
            }

            return new AstEnumDeclare(wstring_pool::get_pstr(enum_name.identifier), items);
        }
        auto pass_namespace::build(lexer& lex, ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            AstList* space_names = static_cast<AstList*>(WO_NEED_AST_TYPE(1, AstBase::AST_LIST));
            auto* body_of_space = WO_NEED_AST(2);

            AstBase* content = body_of_space;

            auto names_rend = space_names->m_list.rend();
            for (auto ridx = space_names->m_list.rbegin(); ridx != names_rend; ++ridx)
            {
                if (content != body_of_space)
                    content->source_location = body_of_space->source_location;

                AstToken* name_token = static_cast<AstToken*>(*ridx);
                wo_assert(name_token->node_type == AstBase::AST_TOKEN);

                wo_pstring_t space_name = wstring_pool::get_pstr(name_token->m_token.identifier);
                AstNamespace* new_space = new AstNamespace(space_name, content);

                content = new_space;
            }

            return content;
        }
        auto pass_using_type_as::build(lexer& lex, ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            token new_type_name = WO_NEED_TOKEN(2);
            std::optional<AstList*> template_params = std::nullopt;
            AstTypeHolder* base_type = static_cast<AstTypeHolder*>(WO_NEED_AST_TYPE(5, AstBase::AST_TYPE_HOLDER));
            std::optional<AstBase*> in_block_sentence = std::nullopt;

            wo_pstring_t type_name = wstring_pool::get_pstr(new_type_name.identifier);

            if (!WO_IS_EMPTY(3))
                template_params = static_cast<AstList*>(WO_NEED_AST_TYPE(3, AstBase::AST_LIST));
            if (!WO_IS_EMPTY(6))
                in_block_sentence = WO_NEED_AST(6);

            std::optional<std::list<wo_pstring_t>> in_type_template_params = std::nullopt;
            if (template_params)
            {
                AstList* template_param_list = template_params.value();
                std::list<wo_pstring_t> params;

                for (auto& param : template_param_list->m_list)
                {
                    wo_assert(param->node_type == AstBase::AST_TOKEN);
                    AstToken* param_token = static_cast<AstToken*>(param);
                    params.push_back(wstring_pool::get_pstr(param_token->m_token.identifier));
                }

                in_type_template_params = std::move(params);
            }

            std::optional<AstNamespace*> in_type_namespace = std::nullopt;
            if (in_block_sentence)
            {
                AstNamespace* typed_namespace = new AstNamespace(type_name, in_block_sentence.value());
                in_type_namespace = typed_namespace;

                typed_namespace->source_location = in_block_sentence.value()->source_location;
            }

            return new AstUsingTypeDeclare(type_name, in_type_template_params, base_type, in_type_namespace);
        }
        auto pass_alias_type_as::build(lexer& lex, ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            token new_type_name = WO_NEED_TOKEN(2);
            std::optional<AstList*> template_params = std::nullopt;
            AstTypeHolder* base_type = static_cast<AstTypeHolder*>(WO_NEED_AST_TYPE(5, AstBase::AST_TYPE_HOLDER));

            wo_pstring_t type_name = wstring_pool::get_pstr(new_type_name.identifier);

            if (!WO_IS_EMPTY(3))
                template_params = static_cast<AstList*>(WO_NEED_AST_TYPE(3, AstBase::AST_LIST));

            std::optional<std::list<wo_pstring_t>> in_type_template_params = std::nullopt;
            if (template_params)
            {
                AstList* template_param_list = template_params.value();
                std::list<wo_pstring_t> params;

                for (auto& param : template_param_list->m_list)
                {
                    wo_assert(param->node_type == AstBase::AST_TOKEN);
                    AstToken* param_token = static_cast<AstToken*>(param);
                    params.push_back(wstring_pool::get_pstr(param_token->m_token.identifier));
                }

                in_type_template_params = std::move(params);
            }

            return new AstAliasTypeDeclare(type_name, in_type_template_params, base_type);
        }
        auto pass_build_where_constraint::build(lexer& lex, ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            AstList* expr_list = static_cast<AstList*>(WO_NEED_AST_TYPE(1, AstBase::AST_LIST));

            std::list<AstValueBase*> constraints;
            for (auto* cons : expr_list->m_list)
            {
                wo_assert(cons->node_type >= AstBase::AST_VALUE_begin && cons->node_type < AstBase::AST_VALUE_end);
                constraints.push_back(static_cast<AstValueBase*>(cons));
            }

            return new AstWhereConstraints(constraints);
        }
        auto pass_func_def_named::build(lexer& lex, ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            AstToken* function_name = static_cast<AstToken*>(WO_NEED_AST_TYPE(2, AstBase::AST_TOKEN));
            std::optional<AstList*> template_params = std::nullopt;
            AstList* paraments = static_cast<AstList*>(WO_NEED_AST_TYPE(5, AstBase::AST_LIST));
            std::optional<AstTypeHolder*> marked_return_type = std::nullopt;
            std::optional<AstWhereConstraints*> where_constraints = std::nullopt;
            AstBase* body = WO_NEED_AST(9);

            wo_pstring_t func_name = wstring_pool::get_pstr(function_name->m_token.identifier);

            if (!WO_IS_EMPTY(3))
                template_params = static_cast<AstList*>(WO_NEED_AST_TYPE(3, AstBase::AST_LIST));

            if (!WO_IS_EMPTY(7))
                marked_return_type = static_cast<AstTypeHolder*>(WO_NEED_AST_TYPE(7, AstBase::AST_TYPE_HOLDER));

            if (!WO_IS_EMPTY(8))
                where_constraints = static_cast<AstWhereConstraints*>(WO_NEED_AST_TYPE(8, AstBase::AST_WHERE_CONSTRAINTS));

            std::optional<std::list<wo_pstring_t>> in_template_params = std::nullopt;
            if (template_params)
            {
                std::list<wo_pstring_t> template_param_list;
                for (auto& param : template_params.value()->m_list)
                {
                    wo_assert(param->node_type == AstBase::AST_TOKEN);
                    AstToken* param_token = static_cast<AstToken*>(param);
                    template_param_list.push_back(wstring_pool::get_pstr(param_token->m_token.identifier));
                }
                in_template_params = std::move(template_param_list);
            }

            bool is_variadic_function = false;
            std::list<AstFunctionParameterDeclare*> in_params;
            for (auto& param : paraments->m_list)
            {
                if (param->node_type == AstBase::AST_FUNCTION_PARAMETER_DECLARE)
                    in_params.push_back(static_cast<AstFunctionParameterDeclare*>(param));
                else
                {
                    wo_assert(param->node_type == AstBase::AST_TOKEN);
                    wo_assert(static_cast<AstToken*>(param)->m_token.type == lex_type::l_variadic_sign);
                    is_variadic_function = true;
                }
            }

            auto* function_define = new AstVariableDefines();
            auto* function_value = new AstValueFunction(
                in_params, is_variadic_function, marked_return_type, where_constraints, body);
            auto* function_define_pattern = new AstPatternSingle(false, func_name, in_template_params);
            auto* function_define_item = new AstVariableDefineItem(function_define_pattern, function_value);

            function_define->m_definitions.push_back(function_define_item);

            // Update source location
            function_value->source_location = function_name->source_location;
            function_define_pattern->source_location = function_name->source_location;
            function_define_item->source_location = function_name->source_location;

            return function_define;
        }
        auto pass_func_def_oper::build(lexer& lex, ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            AstToken* overloading_operator = static_cast<AstToken*>(WO_NEED_AST_TYPE(3, AstBase::AST_TOKEN));
            std::optional<AstList*> template_params = std::nullopt;
            AstList* paraments = static_cast<AstList*>(WO_NEED_AST_TYPE(6, AstBase::AST_LIST));
            std::optional<AstTypeHolder*> marked_return_type = std::nullopt;
            std::optional<AstWhereConstraints*> where_constraints = std::nullopt;
            AstBase* body = WO_NEED_AST(10);

            wo_pstring_t func_name = wstring_pool::get_pstr(
                L"operator " + overloading_operator->m_token.identifier);

            if (!WO_IS_EMPTY(4))
                template_params = static_cast<AstList*>(WO_NEED_AST_TYPE(4, AstBase::AST_LIST));

            if (!WO_IS_EMPTY(8))
                marked_return_type = static_cast<AstTypeHolder*>(WO_NEED_AST_TYPE(8, AstBase::AST_TYPE_HOLDER));

            if (!WO_IS_EMPTY(9))
                where_constraints = static_cast<AstWhereConstraints*>(WO_NEED_AST_TYPE(9, AstBase::AST_WHERE_CONSTRAINTS));

            std::optional<std::list<wo_pstring_t>> in_template_params = std::nullopt;
            if (template_params)
            {
                std::list<wo_pstring_t> template_param_list;
                for (auto& param : template_params.value()->m_list)
                {
                    wo_assert(param->node_type == AstBase::AST_TOKEN);
                    AstToken* param_token = static_cast<AstToken*>(param);
                    template_param_list.push_back(wstring_pool::get_pstr(param_token->m_token.identifier));
                }
                in_template_params = std::move(template_param_list);
            }

            bool is_variadic_function = false;
            std::list<AstFunctionParameterDeclare*> in_params;
            for (auto& param : paraments->m_list)
            {
                if (param->node_type == AstBase::AST_FUNCTION_PARAMETER_DECLARE)
                    in_params.push_back(static_cast<AstFunctionParameterDeclare*>(param));
                else
                {
                    wo_assert(param->node_type == AstBase::AST_TOKEN);
                    wo_assert(static_cast<AstToken*>(param)->m_token.type == lex_type::l_variadic_sign);
                    is_variadic_function = true;
                }
            }

            auto* function_define = new AstVariableDefines();
            auto* function_value = new AstValueFunction(
                in_params, is_variadic_function, marked_return_type, where_constraints, body);
            auto* function_define_pattern = new AstPatternSingle(false, func_name, in_template_params);
            auto* function_define_item = new AstVariableDefineItem(function_define_pattern, function_value);

            function_define->m_definitions.push_back(function_define_item);

            // Update source location
            function_value->source_location = overloading_operator->source_location;
            function_define_pattern->source_location = overloading_operator->source_location;
            function_define_item->source_location = overloading_operator->source_location;

            return function_define;
        }
        auto pass_break::build(lexer& lex, ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            return new AstBreak(std::nullopt);
        }
        auto pass_break_label::build(lexer& lex, ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            token label = WO_NEED_TOKEN(1);
            return new AstBreak(wstring_pool::get_pstr(label.identifier));
        }
        auto pass_continue::build(lexer& lex, ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            return new AstContinue(std::nullopt);
        }
        auto pass_continue_label::build(lexer& lex, ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            token label = WO_NEED_TOKEN(1);
            return new AstContinue(wstring_pool::get_pstr(label.identifier));
        }
        auto pass_return::build(lexer& lex, ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            return new AstReturn(std::nullopt);
        }
        auto pass_return_value::build(lexer& lex, ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            return new AstReturn(WO_NEED_AST_VALUE(1));
        }
        auto pass_return_lambda::build(lexer& lex, ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            return new AstReturn(WO_NEED_AST_VALUE(0));
        }
        auto pass_func_lambda_ml::build(lexer& lex, ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            std::optional<AstList*> template_params = std::nullopt;
            AstList* paraments = static_cast<AstList*>(WO_NEED_AST_TYPE(4, AstBase::AST_LIST));
            std::optional<AstTypeHolder*> marked_return_type = std::nullopt;
            std::optional<AstWhereConstraints*> where_constraints = std::nullopt;
            AstBase* body = WO_NEED_AST(8);

            if (!WO_IS_EMPTY(2))
                template_params = static_cast<AstList*>(WO_NEED_AST_TYPE(2, AstBase::AST_LIST));

            if (!WO_IS_EMPTY(6))
                marked_return_type = static_cast<AstTypeHolder*>(WO_NEED_AST_TYPE(6, AstBase::AST_TYPE_HOLDER));

            if (!WO_IS_EMPTY(7))
                where_constraints = static_cast<AstWhereConstraints*>(WO_NEED_AST_TYPE(7, AstBase::AST_WHERE_CONSTRAINTS));

            std::optional<std::list<wo_pstring_t>> in_template_params = std::nullopt;
            if (template_params)
            {
                std::list<wo_pstring_t> template_param_list;
                for (auto& param : template_params.value()->m_list)
                {
                    wo_assert(param->node_type == AstBase::AST_TOKEN);
                    AstToken* param_token = static_cast<AstToken*>(param);
                    template_param_list.push_back(wstring_pool::get_pstr(param_token->m_token.identifier));
                }
                in_template_params = std::move(template_param_list);
            }

            bool is_variadic_function = false;
            std::list<AstFunctionParameterDeclare*> in_params;
            for (auto& param : paraments->m_list)
            {
                if (param->node_type == AstBase::AST_FUNCTION_PARAMETER_DECLARE)
                    in_params.push_back(static_cast<AstFunctionParameterDeclare*>(param));
                else
                {
                    wo_assert(param->node_type == AstBase::AST_TOKEN);
                    wo_assert(static_cast<AstToken*>(param)->m_token.type == lex_type::l_variadic_sign);
                    is_variadic_function = true;
                }
            }

            return new AstValueFunction(in_params, is_variadic_function, marked_return_type, where_constraints, body);
        }
        auto pass_func_lambda::build(lexer& lex, ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            std::optional<AstList*> template_params = std::nullopt;
            AstList* paraments = static_cast<AstList*>(WO_NEED_AST_TYPE(2, AstBase::AST_LIST));
            AstReturn* body_0 = static_cast<AstReturn*>(WO_NEED_AST_TYPE(4, AstBase::AST_RETURN));
            std::optional<AstVariableDefines*> body_1 = std::nullopt;

            if (!WO_IS_EMPTY(1))
                template_params = static_cast<AstList*>(WO_NEED_AST_TYPE(1, AstBase::AST_LIST));

            if (!WO_IS_EMPTY(5))
                body_1 = static_cast<AstVariableDefines*>(WO_NEED_AST_TYPE(5, AstBase::AST_VARIABLE_DEFINES));

            std::optional<std::list<wo_pstring_t>> in_template_params = std::nullopt;
            if (template_params)
            {
                std::list<wo_pstring_t> template_param_list;
                for (auto& param : template_params.value()->m_list)
                {
                    wo_assert(param->node_type == AstBase::AST_TOKEN);
                    AstToken* param_token = static_cast<AstToken*>(param);
                    template_param_list.push_back(wstring_pool::get_pstr(param_token->m_token.identifier));
                }
                in_template_params = std::move(template_param_list);
            }

            bool is_variadic_function = false;
            std::list<AstFunctionParameterDeclare*> in_params;
            for (auto& param : paraments->m_list)
            {
                if (param->node_type == AstBase::AST_FUNCTION_PARAMETER_DECLARE)
                    in_params.push_back(static_cast<AstFunctionParameterDeclare*>(param));
                else
                {
                    wo_assert(param->node_type == AstBase::AST_TOKEN);
                    wo_assert(static_cast<AstToken*>(param)->m_token.type == lex_type::l_variadic_sign);
                    is_variadic_function = true;
                }
            }

            auto* function_body = new AstList();
  
            if (body_1)
                // Append body_1 before body_0.
                function_body->m_list.push_back(body_1.value());
            function_body->m_list.push_back(body_0);

            auto* function_scope = new AstScope(function_body);

            // Update source location
            function_scope->source_location = body_0->source_location;
            function_scope->source_location = body_0->source_location;

            return new AstValueFunction(
                in_params, is_variadic_function, std::nullopt, std::nullopt, function_scope);
        }
    }

    namespace ast
    {
        void init_builder()
        {

        }
    }

    grammar::rule operator>>(grammar::rule ost, size_t builder_index)
    {
        ost.first.builder_index = builder_index;
        return ost;
    }
}