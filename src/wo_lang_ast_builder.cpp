#include "wo_lang_ast_builder.hpp"

#include <tuple>

namespace wo
{
    namespace ast
    {
        auto pass_mark_label::build(lexer& lex, const ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            token label = WO_NEED_TOKEN(0);
            auto* node = WO_NEED_AST(2);

            return new AstLabeled(wstring_pool::get_pstr(label.identifier), node);
        }
        auto pass_import_files::build(lexer& lex, const ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            wo_error("not implemented");
            return token{ lex_type::l_error };
        }
        auto pass_using_namespace::build(lexer& lex, const ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            AstList* using_namespace_list = static_cast<AstList*>(WO_NEED_AST_TYPE(2, AstBase::AST_LIST));

            std::list<wo_pstring_t> used_namespaces;
            for (auto& ns : using_namespace_list->m_list)
            {
                wo_assert(ns->node_type == AstBase::AST_TOKEN);
                AstToken* ns_token = static_cast<AstToken*>(ns);
                used_namespaces.push_back(wstring_pool::get_pstr(ns_token->m_token.identifier));
            }

            return new AstUsingNamespace(used_namespaces);
        }
        auto pass_empty::build(lexer& lex, const ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            return new AstEmpty();
        }
        auto pass_token::build(lexer& lex, const ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            return new AstToken(WO_NEED_TOKEN(0));
        }
        auto pass_enum_item_create::build(lexer& lex, const ast::astnode_builder::inputs_t& input)-> grammar::produce
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
        auto pass_enum_finalize::build(lexer& lex, const ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            std::optional<AstDeclareAttribue*> attrib = std::nullopt;
            token enum_name = WO_NEED_TOKEN(2);
            AstList* enum_items = static_cast<AstList*>(WO_NEED_AST_TYPE(4, AstBase::AST_LIST));

            if (!WO_IS_EMPTY(0))
                attrib = static_cast<AstDeclareAttribue*>(WO_NEED_AST_TYPE(0, AstBase::AST_DECLARE_ATTRIBUTE));

            std::list<AstEnumItem*> items;
            for (auto& item : enum_items->m_list)
            {
                wo_assert(item->node_type == AstBase::AST_ENUM_ITEM);
                items.push_back(static_cast<AstEnumItem*>(item));
            }

            return new AstEnumDeclare(attrib, wstring_pool::get_pstr(enum_name.identifier), items);
        }
        auto pass_namespace::build(lexer& lex, const ast::astnode_builder::inputs_t& input)-> grammar::produce
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
        auto pass_using_type_as::build(lexer& lex, const ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            std::optional<AstDeclareAttribue*> attrib = std::nullopt;
            token new_type_name = WO_NEED_TOKEN(2);
            std::optional<AstList*> template_params = std::nullopt;
            AstTypeHolder* base_type = static_cast<AstTypeHolder*>(WO_NEED_AST_TYPE(5, AstBase::AST_TYPE_HOLDER));
            std::optional<AstBase*> in_block_sentence = std::nullopt;

            wo_pstring_t type_name = wstring_pool::get_pstr(new_type_name.identifier);

            if (!WO_IS_EMPTY(0))
                attrib = static_cast<AstDeclareAttribue*>(WO_NEED_AST_TYPE(0, AstBase::AST_DECLARE_ATTRIBUTE));
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

            return new AstUsingTypeDeclare(attrib, type_name, in_type_template_params, base_type, in_type_namespace);
        }
        auto pass_alias_type_as::build(lexer& lex, const ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            std::optional<AstDeclareAttribue*> attrib = std::nullopt;
            token new_type_name = WO_NEED_TOKEN(2);
            std::optional<AstList*> template_params = std::nullopt;
            AstTypeHolder* base_type = static_cast<AstTypeHolder*>(WO_NEED_AST_TYPE(5, AstBase::AST_TYPE_HOLDER));

            wo_pstring_t type_name = wstring_pool::get_pstr(new_type_name.identifier);

            if (!WO_IS_EMPTY(0))
                attrib = static_cast<AstDeclareAttribue*>(WO_NEED_AST_TYPE(0, AstBase::AST_DECLARE_ATTRIBUTE));
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

            return new AstAliasTypeDeclare(attrib, type_name, in_type_template_params, base_type);
        }
        auto pass_build_where_constraint::build(lexer& lex, const ast::astnode_builder::inputs_t& input)-> grammar::produce
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

        static auto _process_template_params(const std::optional<AstList*>& template_params)
            -> std::optional<std::list<wo_pstring_t>>
        {
            if (template_params)
            {
                std::list<wo_pstring_t> template_param_list;
                for (auto& param : template_params.value()->m_list)
                {
                    wo_assert(param->node_type == AstBase::AST_TOKEN);
                    AstToken* param_token = static_cast<AstToken*>(param);
                    template_param_list.push_back(wstring_pool::get_pstr(param_token->m_token.identifier));
                }
                return template_param_list;
            }
            return std::nullopt;
        }
        static auto _process_function_params(lexer& lex, AstList* paraments)
            ->std::tuple<bool, std::list<AstFunctionParameterDeclare*>>
        {
            bool is_variadic_function = false;
            std::list<AstFunctionParameterDeclare*> in_params;
            for (auto& param : paraments->m_list)
            {
                if (is_variadic_function)
                    lex.lang_error(lexer::errorlevel::error, param, WO_ERR_ARG_DEFINE_AFTER_VARIADIC);

                if (param->node_type == AstBase::AST_FUNCTION_PARAMETER_DECLARE)
                    in_params.push_back(static_cast<AstFunctionParameterDeclare*>(param));
                else
                {
                    wo_assert(param->node_type == AstBase::AST_TOKEN);
                    wo_assert(static_cast<AstToken*>(param)->m_token.type == lex_type::l_variadic_sign);
                    is_variadic_function = true;
                }
            }
            return std::make_tuple(is_variadic_function, in_params);
        }

        auto pass_func_def_named::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            std::optional<AstDeclareAttribue*> attrib = std::nullopt;
            AstToken* function_name = static_cast<AstToken*>(WO_NEED_AST_TYPE(2, AstBase::AST_TOKEN));
            std::optional<AstList*> template_params = std::nullopt;
            AstList* paraments = static_cast<AstList*>(WO_NEED_AST_TYPE(5, AstBase::AST_LIST));
            std::optional<AstTypeHolder*> marked_return_type = std::nullopt;
            std::optional<AstWhereConstraints*> where_constraints = std::nullopt;
            AstBase* body = WO_NEED_AST(9);

            wo_pstring_t func_name = wstring_pool::get_pstr(function_name->m_token.identifier);

            if (!WO_IS_EMPTY(0))
                attrib = static_cast<AstDeclareAttribue*>(WO_NEED_AST_TYPE(0, AstBase::AST_DECLARE_ATTRIBUTE));
            if (!WO_IS_EMPTY(3))
                template_params = static_cast<AstList*>(WO_NEED_AST_TYPE(3, AstBase::AST_LIST));
            if (!WO_IS_EMPTY(7))
                marked_return_type = static_cast<AstTypeHolder*>(WO_NEED_AST_TYPE(7, AstBase::AST_TYPE_HOLDER));
            if (!WO_IS_EMPTY(8))
                where_constraints = static_cast<AstWhereConstraints*>(WO_NEED_AST_TYPE(8, AstBase::AST_WHERE_CONSTRAINTS));

            std::optional<std::list<wo_pstring_t>> in_template_params = 
                _process_template_params(template_params);

            auto[is_variadic_function, in_params] = _process_function_params(lex, paraments);
    
            auto* function_define = new AstVariableDefines(attrib);
            auto* function_value = new AstValueFunction(
                in_params, is_variadic_function, std::nullopt, marked_return_type, where_constraints, body);
            auto* function_define_pattern = new AstPatternSingle(false, func_name, in_template_params);
            auto* function_define_item = new AstVariableDefineItem(function_define_pattern, function_value);

            function_define->m_definitions.push_back(function_define_item);

            // Update source location
            function_value->source_location = function_name->source_location;
            function_define_pattern->source_location = function_name->source_location;
            function_define_item->source_location = function_name->source_location;

            return function_define;
        }
        auto pass_func_def_oper::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            std::optional<AstDeclareAttribue*> attrib = std::nullopt;
            AstToken* overloading_operator = static_cast<AstToken*>(WO_NEED_AST_TYPE(3, AstBase::AST_TOKEN));
            std::optional<AstList*> template_params = std::nullopt;
            AstList* paraments = static_cast<AstList*>(WO_NEED_AST_TYPE(6, AstBase::AST_LIST));
            std::optional<AstTypeHolder*> marked_return_type = std::nullopt;
            std::optional<AstWhereConstraints*> where_constraints = std::nullopt;
            AstBase* body = WO_NEED_AST(10);

            wo_pstring_t func_name = wstring_pool::get_pstr(
                L"operator " + overloading_operator->m_token.identifier);

            if (!WO_IS_EMPTY(0))
                attrib = static_cast<AstDeclareAttribue*>(WO_NEED_AST_TYPE(0, AstBase::AST_DECLARE_ATTRIBUTE));
            if (!WO_IS_EMPTY(4))
                template_params = static_cast<AstList*>(WO_NEED_AST_TYPE(4, AstBase::AST_LIST));
            if (!WO_IS_EMPTY(8))
                marked_return_type = static_cast<AstTypeHolder*>(WO_NEED_AST_TYPE(8, AstBase::AST_TYPE_HOLDER));
            if (!WO_IS_EMPTY(9))
                where_constraints = static_cast<AstWhereConstraints*>(WO_NEED_AST_TYPE(9, AstBase::AST_WHERE_CONSTRAINTS));

            std::optional<std::list<wo_pstring_t>> in_template_params = 
                _process_template_params(template_params);

            auto [is_variadic_function, in_params] = _process_function_params(lex, paraments);

            auto* function_define = new AstVariableDefines(attrib);
            auto* function_value = new AstValueFunction(
                in_params, is_variadic_function, std::nullopt, marked_return_type, where_constraints, body);
            auto* function_define_pattern = new AstPatternSingle(false, func_name, in_template_params);
            auto* function_define_item = new AstVariableDefineItem(function_define_pattern, function_value);

            function_define->m_definitions.push_back(function_define_item);

            // Update source location
            function_value->source_location = overloading_operator->source_location;
            function_define_pattern->source_location = overloading_operator->source_location;
            function_define_item->source_location = overloading_operator->source_location;

            return function_define;
        }
        auto pass_func_def_extn::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            AstExternInformation* extern_info = 
                static_cast<AstExternInformation*>(WO_NEED_AST_TYPE(0, AstBase::AST_EXTERN_INFORMATION));
            std::optional<AstDeclareAttribue*> attrib = std::nullopt;
            AstToken* function_name = static_cast<AstToken*>(WO_NEED_AST_TYPE(3, AstBase::AST_TOKEN));
            std::optional<AstList*> template_params = std::nullopt;
            AstList* paraments = static_cast<AstList*>(WO_NEED_AST_TYPE(6, AstBase::AST_LIST));
            AstTypeHolder* marked_return_type = static_cast<AstTypeHolder*>(WO_NEED_AST_TYPE(8, AstBase::AST_TYPE_HOLDER));
            std::optional<AstWhereConstraints*> where_constraints = std::nullopt;

            wo_pstring_t func_name = wstring_pool::get_pstr(function_name->m_token.identifier);

            if (!WO_IS_EMPTY(1))
                attrib = static_cast<AstDeclareAttribue*>(WO_NEED_AST_TYPE(1, AstBase::AST_DECLARE_ATTRIBUTE));

            if (!WO_IS_EMPTY(4))
                template_params = static_cast<AstList*>(WO_NEED_AST_TYPE(4, AstBase::AST_LIST));

            if (!WO_IS_EMPTY(9))
                where_constraints = static_cast<AstWhereConstraints*>(WO_NEED_AST_TYPE(9, AstBase::AST_WHERE_CONSTRAINTS));

            std::optional<std::list<wo_pstring_t>> in_template_params = 
                _process_template_params(template_params);

            auto [is_variadic_function, in_params] = _process_function_params(lex, paraments);

            auto* function_define = new AstVariableDefines(attrib);
            auto* function_value = new AstValueFunction(
                in_params, is_variadic_function, in_template_params, marked_return_type, where_constraints, extern_info);
            auto* function_define_pattern = new AstPatternSingle(false, func_name, in_template_params);
            auto* function_define_item = new AstVariableDefineItem(function_define_pattern, function_value);

            function_define->m_definitions.push_back(function_define_item);

            // Update source location
            function_value->source_location = function_name->source_location;
            function_define_pattern->source_location = function_name->source_location;
            function_define_item->source_location = function_name->source_location;

            return function_define;
        }
        auto pass_func_def_extn_oper::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            AstExternInformation* extern_info =
                static_cast<AstExternInformation*>(WO_NEED_AST_TYPE(0, AstBase::AST_EXTERN_INFORMATION));
            std::optional<AstDeclareAttribue*> attrib = std::nullopt;
            AstToken* overloading_operator = static_cast<AstToken*>(WO_NEED_AST_TYPE(4, AstBase::AST_TOKEN));
            std::optional<AstList*> template_params = std::nullopt;
            AstList* paraments = static_cast<AstList*>(WO_NEED_AST_TYPE(7, AstBase::AST_LIST));
            AstTypeHolder* marked_return_type = static_cast<AstTypeHolder*>(WO_NEED_AST_TYPE(9, AstBase::AST_TYPE_HOLDER));
            std::optional<AstWhereConstraints*> where_constraints = std::nullopt;

            wo_pstring_t func_name = wstring_pool::get_pstr(
                L"operator " + overloading_operator->m_token.identifier);

            if (!WO_IS_EMPTY(1))
                attrib = static_cast<AstDeclareAttribue*>(WO_NEED_AST_TYPE(1, AstBase::AST_DECLARE_ATTRIBUTE));

            if (!WO_IS_EMPTY(5))
                template_params = static_cast<AstList*>(WO_NEED_AST_TYPE(5, AstBase::AST_LIST));

            if (!WO_IS_EMPTY(10))
                where_constraints = static_cast<AstWhereConstraints*>(WO_NEED_AST_TYPE(10, AstBase::AST_WHERE_CONSTRAINTS));

            std::optional<std::list<wo_pstring_t>> in_template_params =
                _process_template_params(template_params);

            auto [is_variadic_function, in_params] = _process_function_params(lex, paraments);

            auto* function_define = new AstVariableDefines(attrib);
            auto* function_value = new AstValueFunction(
                in_params, is_variadic_function, in_template_params, marked_return_type, where_constraints, extern_info);
            auto* function_define_pattern = new AstPatternSingle(false, func_name, in_template_params);
            auto* function_define_item = new AstVariableDefineItem(function_define_pattern, function_value);

            function_define->m_definitions.push_back(function_define_item);

            // Update source location
            function_value->source_location = overloading_operator->source_location;
            function_define_pattern->source_location = overloading_operator->source_location;
            function_define_item->source_location = overloading_operator->source_location;

            return function_define;
        }
        auto pass_break::build(lexer& lex, const ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            return new AstBreak(std::nullopt);
        }
        auto pass_break_label::build(lexer& lex, const ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            token label = WO_NEED_TOKEN(1);
            return new AstBreak(wstring_pool::get_pstr(label.identifier));
        }
        auto pass_continue::build(lexer& lex, const ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            return new AstContinue(std::nullopt);
        }
        auto pass_continue_label::build(lexer& lex, const ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            token label = WO_NEED_TOKEN(1);
            return new AstContinue(wstring_pool::get_pstr(label.identifier));
        }
        auto pass_return::build(lexer& lex, const ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            return new AstReturn(std::nullopt);
        }
        auto pass_return_value::build(lexer& lex, const ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            return new AstReturn(WO_NEED_AST_VALUE(1));
        }
        auto pass_return_lambda::build(lexer& lex, const ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            return new AstReturn(WO_NEED_AST_VALUE(0));
        }
        auto pass_func_lambda_ml::build(lexer& lex, const ast::astnode_builder::inputs_t& input)-> grammar::produce
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

            std::optional<std::list<wo_pstring_t>> in_template_params = 
                _process_template_params(template_params);

            auto [is_variadic_function, in_params] = _process_function_params(lex, paraments);

            return new AstValueFunction(
                in_params, 
                is_variadic_function, 
                in_template_params, 
                marked_return_type, 
                where_constraints, 
                body);
        }
        auto pass_func_lambda::build(lexer& lex, const ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            std::optional<AstList*> template_params = std::nullopt;
            AstList* paraments = static_cast<AstList*>(WO_NEED_AST_TYPE(2, AstBase::AST_LIST));
            AstReturn* body_0 = static_cast<AstReturn*>(WO_NEED_AST_TYPE(4, AstBase::AST_RETURN));
            std::optional<AstVariableDefines*> body_1 = std::nullopt;

            if (!WO_IS_EMPTY(1))
                template_params = static_cast<AstList*>(WO_NEED_AST_TYPE(1, AstBase::AST_LIST));

            if (!WO_IS_EMPTY(5))
                body_1 = static_cast<AstVariableDefines*>(WO_NEED_AST_TYPE(5, AstBase::AST_VARIABLE_DEFINES));

            std::optional<std::list<wo_pstring_t>> in_template_params = 
                _process_template_params(template_params);

            auto [is_variadic_function, in_params] = _process_function_params(lex, paraments);

            auto* function_body = new AstList();

            if (body_1)
                // Append body_1 before body_0.
                function_body->m_list.push_back(body_1.value());
            function_body->m_list.push_back(body_0);

            auto* function_scope = new AstScope(function_body);

            // Update source location
            function_body->source_location = body_0->source_location;
            function_scope->source_location = body_0->source_location;

            return new AstValueFunction(
                in_params, 
                is_variadic_function, 
                in_template_params, 
                std::nullopt, 
                std::nullopt, 
                function_scope);
        }
        auto pass_if::build(lexer& lex, const ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            auto* condition = WO_NEED_AST_VALUE(2);
            auto* body = WO_NEED_AST(4);
            std::optional<AstBase*> else_body = std::nullopt;

            if (!WO_IS_EMPTY(5))
                else_body = WO_NEED_AST(5);

            return new AstIf(condition, body, else_body);
        }
        auto pass_while::build(lexer& lex, const ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            auto* condition = WO_NEED_AST_VALUE(2);
            auto* body = WO_NEED_AST(4);

            return new AstWhile(condition, body);
        }
        auto pass_for_defined::build(lexer& lex, const ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            std::optional<AstVariableDefines*> init = std::nullopt;
            std::optional<AstValueBase*> condition = std::nullopt;
            std::optional<AstValueBase*> step = std::nullopt;
            auto* body = WO_NEED_AST(7);

            if (!WO_IS_EMPTY(2))
                init = static_cast<AstVariableDefines*>(WO_NEED_AST_TYPE(2, AstBase::AST_VARIABLE_DEFINES));
            if (!WO_IS_EMPTY(3))
                condition = WO_NEED_AST_VALUE(3);
            if (!WO_IS_EMPTY(5))
                step = WO_NEED_AST_VALUE(4);

            return new AstFor(init, condition, step, body);
        }
        auto pass_for_expr::build(lexer& lex, const ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            std::optional<AstBase*> init = std::nullopt;
            std::optional<AstValueBase*> condition = std::nullopt;
            std::optional<AstValueBase*> step = std::nullopt;
            auto* body = WO_NEED_AST(8);

            if (!WO_IS_EMPTY(2))
                init = WO_NEED_AST_VALUE(2);
            if (!WO_IS_EMPTY(4))
                condition = WO_NEED_AST_VALUE(3);
            if (!WO_IS_EMPTY(6))
                step = WO_NEED_AST_VALUE(4);

            return new AstFor(init, condition, step, body);
        }
        auto pass_foreach::build(lexer& lex, const ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            AstPatternBase* pattern = static_cast<AstPatternBase*>(WO_NEED_AST_PATTERN(3));
            AstValueBase* iterating_value = WO_NEED_AST_VALUE(5);
            AstBase* body = WO_NEED_AST(7);
            return new AstForeach(pattern, iterating_value, body);
        }
        auto pass_mark_mut::build(lexer& lex, const ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            return new AstValueMarkAsMutable(WO_NEED_AST_VALUE(1));
        }
        auto pass_mark_immut::build(lexer& lex, const ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            return new AstValueMarkAsImmutable(WO_NEED_AST_VALUE(1));
        }
        auto pass_typeof::build(lexer& lex, const ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            AstValueBase* value = WO_NEED_AST_VALUE(2);
            return new AstTypeHolder(value);
        }
        auto pass_build_identifier_typeof::build(lexer& lex, const ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            AstList* scope_identifier = static_cast<AstList*>(WO_NEED_AST_TYPE(0, AstBase::AST_LIST));
            std::optional<AstList*> template_arguments = std::nullopt;

            wo_assert(scope_identifier->m_list.size() >= 2);

            if (!WO_IS_EMPTY(1))
                template_arguments = static_cast<AstList*>(WO_NEED_AST_TYPE(1, AstBase::AST_LIST));

            std::optional<std::list<AstTypeHolder*>> template_args = std::nullopt;
            if (template_arguments)
            {
                std::list<AstTypeHolder*> args;
                for (auto& arg : template_arguments.value()->m_list)
                {
                    wo_assert(arg->node_type == AstBase::AST_TYPE_HOLDER);
                    args.push_back(static_cast<AstTypeHolder*>(arg));
                }
                template_args = std::move(args);
            }

            std::list<wo_pstring_t> scope_identifiers_and_name;
            auto token_iter = scope_identifier->m_list.begin();
            auto token_end = scope_identifier->m_list.end();

            wo_assert((*token_iter)->node_type == AstBase::AST_TYPE_HOLDER);
            AstTypeHolder* typeof_holder = static_cast<AstTypeHolder*>(*token_iter);

            ++token_iter;
            for (; token_iter != token_end; ++token_iter)
            {
                auto* asttoken = *token_iter;
                wo_assert(asttoken->node_type == AstBase::AST_TOKEN);
                scope_identifiers_and_name.push_back(
                    wstring_pool::get_pstr(static_cast<AstToken*>(asttoken)->m_token.identifier));
            }

            wo_pstring_t identifier_name = scope_identifiers_and_name.back();
            scope_identifiers_and_name.pop_back();

            return new AstIdentifier(identifier_name, template_args, scope_identifiers_and_name, typeof_holder);
        }
        auto pass_build_identifier_normal::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            AstList* scope_identifier = static_cast<AstList*>(WO_NEED_AST_TYPE(0, AstBase::AST_LIST));
            std::optional<AstList*> template_arguments = std::nullopt;

            wo_assert(scope_identifier->m_list.size() >= 1);

            if (!WO_IS_EMPTY(1))
                template_arguments = static_cast<AstList*>(WO_NEED_AST_TYPE(1, AstBase::AST_LIST));

            std::optional<std::list<AstTypeHolder*>> template_args = std::nullopt;
            if (template_arguments)
            {
                std::list<AstTypeHolder*> args;
                for (auto& arg : template_arguments.value()->m_list)
                {
                    wo_assert(arg->node_type == AstBase::AST_TYPE_HOLDER);
                    args.push_back(static_cast<AstTypeHolder*>(arg));
                }
                template_args = std::move(args);
            }

            std::list<wo_pstring_t> scope_identifiers_and_name;
            for (auto* asttoken : scope_identifier->m_list)
            {
                wo_assert(asttoken->node_type == AstBase::AST_TOKEN);
                scope_identifiers_and_name.push_back(
                    wstring_pool::get_pstr(static_cast<AstToken*>(asttoken)->m_token.identifier));
            }
            wo_pstring_t identifier_name = scope_identifiers_and_name.back();
            scope_identifiers_and_name.pop_back();

            return new AstIdentifier(identifier_name, template_args, scope_identifiers_and_name, false);
        }
        auto pass_build_identifier_global::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            AstList* scope_identifier = static_cast<AstList*>(WO_NEED_AST_TYPE(0, AstBase::AST_LIST));
            std::optional<AstList*> template_arguments = std::nullopt;

            wo_assert(scope_identifier->m_list.size() >= 1);

            if (!WO_IS_EMPTY(1))
                template_arguments = static_cast<AstList*>(WO_NEED_AST_TYPE(1, AstBase::AST_LIST));

            std::optional<std::list<AstTypeHolder*>> template_args = std::nullopt;
            if (template_arguments)
            {
                std::list<AstTypeHolder*> args;
                for (auto& arg : template_arguments.value()->m_list)
                {
                    wo_assert(arg->node_type == AstBase::AST_TYPE_HOLDER);
                    args.push_back(static_cast<AstTypeHolder*>(arg));
                }
                template_args = std::move(args);
            }

            std::list<wo_pstring_t> scope_identifiers_and_name;
            for (auto* asttoken : scope_identifier->m_list)
            {
                wo_assert(asttoken->node_type == AstBase::AST_TOKEN);
                scope_identifiers_and_name.push_back(
                    wstring_pool::get_pstr(static_cast<AstToken*>(asttoken)->m_token.identifier));
            }
            wo_pstring_t identifier_name = scope_identifiers_and_name.back();
            scope_identifiers_and_name.pop_back();

            return new AstIdentifier(identifier_name, template_args, scope_identifiers_and_name, true);
        }
        auto pass_type_nil::build(lexer& lex, const ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            AstToken* nil_token = static_cast<AstToken*>(WO_NEED_AST_TYPE(0, AstBase::AST_TOKEN));
            wo_assert(nil_token->m_token.type == lex_type::l_nil);

            AstIdentifier* nil_type_identifier = new AstIdentifier(WO_PSTR(nil), std::nullopt, {}, true);

            // Update source location
            nil_type_identifier->source_location = nil_token->source_location;

            return new AstTypeHolder(nil_type_identifier);
        }
        auto pass_type_func::build(lexer& lex, const ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            AstList* parament_types = static_cast<AstList*>(WO_NEED_AST_TYPE(0, AstBase::AST_LIST));
            AstTypeHolder* return_type = static_cast<AstTypeHolder*>(WO_NEED_AST_TYPE(2, AstBase::AST_TYPE_HOLDER));

            bool is_variadic_function = false;
            std::list<AstTypeHolder*> paraments;
            for (auto& param : parament_types->m_list)
            {
                if (is_variadic_function)
                    return token{ lex.lang_error(
                        lexer::errorlevel::error, param, WO_ERR_ARG_DEFINE_AFTER_VARIADIC) };

                if (param->node_type == AstBase::AST_TYPE_HOLDER)
                    paraments.push_back(static_cast<AstTypeHolder*>(param));
                else
                {
                    wo_assert(param->node_type == AstBase::AST_TOKEN);
                    wo_assert(static_cast<AstToken*>(param)->m_token.type == lex_type::l_variadic_sign);
                    is_variadic_function = true;
                }
            }

            return new AstTypeHolder(AstTypeHolder::FunctionType{
                is_variadic_function, paraments, return_type });
        }
        auto pass_type_struct_field::build(lexer& lex, const ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            std::optional<AstDeclareAttribue::accessc_attrib> attrib = std::nullopt;
            token field_name = WO_NEED_TOKEN(1);
            AstTypeHolder* field_type = static_cast<AstTypeHolder*>(WO_NEED_AST_TYPE(2, AstBase::AST_TYPE_HOLDER));

            if (!WO_IS_EMPTY(0))
            {
                switch (WO_NEED_TOKEN(0).type)
                {
                case lex_type::l_public:
                    attrib = AstDeclareAttribue::accessc_attrib::PUBLIC; break;
                case lex_type::l_private:
                    attrib = AstDeclareAttribue::accessc_attrib::PRIVATE; break;
                case lex_type::l_protected:
                    attrib = AstDeclareAttribue::accessc_attrib::PROTECTED; break;
                default:
                    wo_error("Unknown attribute.");
                }
            }

            return new AstStructFieldDefine(
                attrib, wstring_pool::get_pstr(field_name.identifier), field_type);
        }
        auto pass_type_struct::build(lexer& lex, const ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            AstList* members = static_cast<AstList*>(WO_NEED_AST_TYPE(2, AstBase::AST_LIST));

            std::list<AstStructFieldDefine*> fields;
            for (auto& field : members->m_list)
            {
                wo_assert(field->node_type == AstBase::AST_STRUCT_FIELD_DEFINE);
                fields.push_back(static_cast<AstStructFieldDefine*>(field));
            }

            return new AstTypeHolder(AstTypeHolder::StructureType{ fields });
        }
        auto pass_type_tuple::build(lexer& lex, const ast::astnode_builder::inputs_t& input)-> grammar::produce
        {
            AstList* tuple_types = static_cast<AstList*>(WO_NEED_AST_TYPE(0, AstBase::AST_LIST));

            std::list<AstTypeHolder*> fields;
            for (auto& field : tuple_types->m_list)
            {
                if (field->node_type != AstBase::AST_TYPE_HOLDER)
                    return token{ lex.lang_error(lexer::errorlevel::error, field, WO_ERR_FAILED_TO_CREATE_TUPLE_WITH_VAARG) };

                fields.push_back(static_cast<AstTypeHolder*>(field));
            }

            return new AstTypeHolder(AstTypeHolder::TupleType{ fields });
        }
        auto pass_attribute::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            AstToken* attrib_token = static_cast<AstToken*>(WO_NEED_AST_TYPE(0, AstBase::AST_TOKEN));

            auto* attrib = new AstDeclareAttribue();
            if (!attrib->modify_attrib(lex, attrib_token))
                return token{ lex_type::l_error };

            return attrib;
        }
        auto pass_attribute_append::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            AstDeclareAttribue* attrib = static_cast<AstDeclareAttribue*>(WO_NEED_AST_TYPE(0, AstBase::AST_DECLARE_ATTRIBUTE));
            AstToken* attrib_token = static_cast<AstToken*>(WO_NEED_AST_TYPE(2, AstBase::AST_TOKEN));

            if (!attrib->modify_attrib(lex, attrib_token))
                return token{ lex_type::l_error };

            return attrib;
        }
        auto pass_pattern_for_assign::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            AstValueBase* been_assigned_lvalue = WO_NEED_AST_VALUE(0);

            switch (been_assigned_lvalue->node_type)
            {
            case AstBase::AST_VALUE_VARIABLE:
                return new AstPatternVariable(static_cast<AstValueVariable*>(been_assigned_lvalue));
            case AstBase::AST_VALUE_INDEX:
                return new AstPatternIndex(static_cast<AstValueIndex*>(been_assigned_lvalue));
            default:
                wo_error("Unknown pattern for assign.");
                return token{ lex_type::l_error };
            }
        }
        auto pass_reverse_vardef::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            AstVariableDefines* vardef = static_cast<AstVariableDefines*>(WO_NEED_AST_TYPE(1, AstBase::AST_VARIABLE_DEFINES));
            vardef->m_definitions.reverse();
            return vardef;
        }
        auto pass_type_mutable::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            AstTypeHolder* type = static_cast<AstTypeHolder*>(WO_NEED_AST_TYPE(1, AstBase::AST_TYPE_HOLDER));
            type->m_mutable_mark = AstTypeHolder::mutable_mark::MARK_AS_MUTABLE;
            return type;
        }
        auto pass_type_immutable::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            AstTypeHolder* type = static_cast<AstTypeHolder*>(WO_NEED_AST_TYPE(1, AstBase::AST_TYPE_HOLDER));
            type->m_mutable_mark = AstTypeHolder::mutable_mark::MARK_AS_IMMUTABLE;
            return type;
        }
        auto pass_func_argument::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            AstPatternBase* pattern = static_cast<AstPatternBase*>(WO_NEED_AST_PATTERN(0));
            std::optional<AstTypeHolder*> type = std::nullopt;

            if (!WO_IS_EMPTY(1))
                type = static_cast<AstTypeHolder*>(WO_NEED_AST_TYPE(1, AstBase::AST_TYPE_HOLDER));

            return new AstFunctionParameterDeclare(pattern, type);
        }
        auto pass_do_void_cast::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            AstValueBase* value = WO_NEED_AST_VALUE(1);

            AstIdentifier* void_identifier = new AstIdentifier(WO_PSTR(void), std::nullopt, {}, true);
            AstTypeHolder* void_type = new AstTypeHolder(void_identifier);
            
            AstValueTypeCast* cast = new AstValueTypeCast(void_type, value);

            // Update source location
            void_identifier->source_location = value->source_location;
            void_type->source_location = value->source_location;

            return cast;
        }
        auto pass_assign_operation::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            AstPatternBase* lpattern = WO_NEED_AST_PATTERN(0);
            token operation = WO_NEED_TOKEN(1);
            AstValueBase* rvalue = WO_NEED_AST_VALUE(2);

            switch (operation.type)
            {
            case lex_type::l_assign:
                return new AstValueAssign(false, AstValueAssign::ASSIGN, lpattern, rvalue);
            case lex_type::l_add_assign:
                return new AstValueAssign(false, AstValueAssign::ADD_ASSIGN, lpattern, rvalue);
            case lex_type::l_sub_assign:
                return new AstValueAssign(false, AstValueAssign::SUBSTRACT_ASSIGN, lpattern, rvalue);
            case lex_type::l_mul_assign:
                return new AstValueAssign(false, AstValueAssign::MULTIPLY_ASSIGN, lpattern, rvalue);
            case lex_type::l_div_assign:
                return new AstValueAssign(false, AstValueAssign::DIVIDE_ASSIGN, lpattern, rvalue);
            case lex_type::l_mod_assign:
                return new AstValueAssign(false, AstValueAssign::MODULO_ASSIGN, lpattern, rvalue);
            case lex_type::l_value_assign:
                return new AstValueAssign(true, AstValueAssign::ASSIGN, lpattern, rvalue);
            case lex_type::l_value_add_assign:
                return new AstValueAssign(true, AstValueAssign::ADD_ASSIGN, lpattern, rvalue);
            case lex_type::l_value_sub_assign:
                return new AstValueAssign(true, AstValueAssign::SUBSTRACT_ASSIGN, lpattern, rvalue);
            case lex_type::l_value_mul_assign:
                return new AstValueAssign(true, AstValueAssign::MULTIPLY_ASSIGN, lpattern, rvalue);
            case lex_type::l_value_div_assign:
                return new AstValueAssign(true, AstValueAssign::DIVIDE_ASSIGN, lpattern, rvalue);
            case lex_type::l_value_mod_assign:
                return new AstValueAssign(true, AstValueAssign::MODULO_ASSIGN, lpattern, rvalue);
            default:
                wo_error("Unknown assign operation.");
                return token{ lex_type::l_error };
            }
        }
        auto pass_binary_operation::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            AstValueBase* lvalue = WO_NEED_AST_VALUE(0);
            token operation = WO_NEED_TOKEN(1);
            AstValueBase* rvalue = WO_NEED_AST_VALUE(2);

            switch (operation.type)
            {
            case lex_type::l_add:
                return new AstValueBinaryOperator(AstValueBinaryOperator::ADD, lvalue, rvalue);
            case lex_type::l_sub:
                return new AstValueBinaryOperator(AstValueBinaryOperator::SUBSTRACT, lvalue, rvalue);
            case lex_type::l_mul:
                return new AstValueBinaryOperator(AstValueBinaryOperator::MULTIPLY, lvalue, rvalue);
            case lex_type::l_div:
                return new AstValueBinaryOperator(AstValueBinaryOperator::DIVIDE, lvalue, rvalue);
            case lex_type::l_mod:
                return new AstValueBinaryOperator(AstValueBinaryOperator::MODULO, lvalue, rvalue);
            case lex_type::l_land:
                return new AstValueBinaryOperator(AstValueBinaryOperator::LOGICAL_AND, lvalue, rvalue);
            case lex_type::l_lor:
                return new AstValueBinaryOperator(AstValueBinaryOperator::LOGICAL_OR, lvalue, rvalue);
            case lex_type::l_equal:
                return new AstValueBinaryOperator(AstValueBinaryOperator::EQUAL, lvalue, rvalue);
            case lex_type::l_not_equal:
                return new AstValueBinaryOperator(AstValueBinaryOperator::NOT_EQUAL, lvalue, rvalue);
            case lex_type::l_less:
                return new AstValueBinaryOperator(AstValueBinaryOperator::LESS, lvalue, rvalue);
            case lex_type::l_less_or_equal:
                return new AstValueBinaryOperator(AstValueBinaryOperator::LESS_EQUAL, lvalue, rvalue);
            case lex_type::l_larg:
                return new AstValueBinaryOperator(AstValueBinaryOperator::GREATER, lvalue, rvalue);
            case lex_type::l_larg_or_equal:
                return new AstValueBinaryOperator(AstValueBinaryOperator::GREATER_EQUAL, lvalue, rvalue);
            default:
                wo_error("Unknown binary operation.");
                return token{ lex_type::l_error };
            }
        }
        auto pass_literal::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            token literal = WO_NEED_TOKEN(0);
            AstValueLiteral* literal_instance = new AstValueLiteral();

            wo::value literal_value;
            switch (literal.type)
            {
            case lex_type::l_literal_integer:
                literal_value.set_integer((wo_integer_t)std::stoll(literal.identifier));
                break;
            case lex_type::l_literal_handle:
                literal_value.set_handle((wo_handle_t)std::stoull(literal.identifier));
                break;
            case lex_type::l_literal_real:
                literal_value.set_real((wo_real_t)std::stod(literal.identifier));
                break;
            case lex_type::l_literal_string:
                literal_value.set_string_nogc(wstrn_to_str(literal.identifier));
                break;
            case lex_type::l_nil:
                literal_value.set_nil();
                break;
            case lex_type::l_true:
                literal_value.set_bool(true);
                break;
            case lex_type::l_false:
                literal_value.set_bool(false);
                break;
            default:
                wo_error("Unknown literal type.");
                break;
            }
            literal_instance->decide_final_constant_value(literal_value);
            return literal_instance;
        }
        auto pass_literal_char::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            AstToken* literal = static_cast<AstToken*>(WO_NEED_AST_TYPE(0, AstBase::AST_TOKEN));

            wo_assert(literal->m_token.type == lex_type::l_literal_char);

            AstValueLiteral* literal_instance = new AstValueLiteral();
            wo::value literal_value;
            literal_value.set_integer((wo_integer_t)(wo_handle_t)literal->m_token.identifier[0]);
            literal_instance->decide_final_constant_value(literal_value);

            AstIdentifier* char_identifier = new AstIdentifier(WO_PSTR(char), std::nullopt, {}, true);
            AstTypeHolder* char_type = new AstTypeHolder(char_identifier);

            AstValueTypeCast* cast = new AstValueTypeCast(char_type, literal_instance);

            // Update source location
            literal_instance->source_location = literal_instance->source_location;
            char_identifier->source_location = literal_instance->source_location;
            char_type->source_location = literal_instance->source_location;

            return cast;
        }
        auto pass_typeid::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            AstTypeHolder* type = static_cast<AstTypeHolder*>(WO_NEED_AST_TYPE(2, AstBase::AST_TYPE_HOLDER));
            return new AstValueTypeid(type);
        }
        auto pass_unary_operation::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            token operation = WO_NEED_TOKEN(0);
            AstValueBase* value = WO_NEED_AST_VALUE(1);

            switch (operation.type)
            {
            case lex_type::l_sub:
                return new AstValueUnaryOperator(AstValueUnaryOperator::NEGATIVE, value);
            case lex_type::l_lnot:
                return new AstValueUnaryOperator(AstValueUnaryOperator::LOGICAL_NOT, value);
            default:
                wo_error("Unknown unary operation.");
                return token{ lex_type::l_error };
            }
        }
        auto pass_variable::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            AstIdentifier* identifier = static_cast<AstIdentifier*>(WO_NEED_AST_TYPE(0, AstBase::AST_IDENTIFIER));
            return new AstValueVariable(identifier);
        }
        auto pass_cast_type::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            AstValueBase* value = WO_NEED_AST_VALUE(0);
            AstTypeHolder* type = static_cast<AstTypeHolder*>(WO_NEED_AST_TYPE(1, AstBase::AST_TYPE_HOLDER));

            return new AstValueTypeCast(type, value);
        }
        auto pass_format_finish::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            AstToken* format_string_begin = static_cast<AstToken*>(WO_NEED_AST_TYPE(0, AstBase::AST_TOKEN));
            AstValueBase* middle_value = WO_NEED_AST_VALUE(1);
            AstToken* format_string_end = static_cast<AstToken*>(WO_NEED_AST_TYPE(2, AstBase::AST_TOKEN));

            wo_assert(format_string_begin->m_token.type == lex_type::l_format_string_begin);
            wo_assert(format_string_end->m_token.type == lex_type::l_format_string_end);

            AstValueLiteral* format_string_begin_literal = new AstValueLiteral();
            format_string_begin_literal->decide_final_constant_value(wstrn_to_str(format_string_begin->m_token.identifier));
            AstValueLiteral* format_string_end_literal = new AstValueLiteral();
            format_string_end_literal->decide_final_constant_value(wstrn_to_str(format_string_end->m_token.identifier));

            AstValueBinaryOperator* first_middle_add =
                new AstValueBinaryOperator(AstValueBinaryOperator::ADD, format_string_begin_literal, middle_value);
            AstValueBinaryOperator* first_middle_end_add =
                new AstValueBinaryOperator(AstValueBinaryOperator::ADD, first_middle_add, format_string_end_literal);

            // Update source location
            format_string_begin_literal->source_location = format_string_begin->source_location;
            format_string_end_literal->source_location = format_string_end->source_location;
            first_middle_add->source_location = format_string_begin->source_location;

            return first_middle_end_add;
        }
        auto pass_format_cast_string::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            AstValueBase* value = WO_NEED_AST_VALUE(0);

            AstIdentifier* string_identifier = new AstIdentifier(WO_PSTR(string), std::nullopt, {}, true);
            AstTypeHolder* string_type = new AstTypeHolder(string_identifier);
            
            AstValueTypeCast* cast = new AstValueTypeCast(string_type, value);

            // Update source location
            string_identifier->source_location = value->source_location;
            string_type->source_location = value->source_location;

            return cast;
        }
        auto pass_format_connect::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            AstValueBase* first_string = WO_NEED_AST_VALUE(0);
            AstToken* middle_string_literal = static_cast<AstToken*>(WO_NEED_AST_TYPE(1, AstBase::AST_TOKEN));
            AstValueBase* last_value = WO_NEED_AST_VALUE(2);

            AstIdentifier* string_identifier = new AstIdentifier(WO_PSTR(string), std::nullopt, {}, true);
            AstTypeHolder* string_type = new AstTypeHolder(string_identifier);
            AstValueTypeCast* cast = new AstValueTypeCast(string_type, first_string);
            AstValueLiteral* middle_string = new AstValueLiteral();
            middle_string->decide_final_constant_value(wstrn_to_str(middle_string_literal->m_token.identifier));

            AstValueBinaryOperator* first_middle_add =
                new AstValueBinaryOperator(AstValueBinaryOperator::ADD, first_string, middle_string);

            AstValueBinaryOperator* first_middle_last_add =
                new AstValueBinaryOperator(AstValueBinaryOperator::ADD, first_middle_add, last_value);

            // Update source location
            string_identifier->source_location = first_string->source_location;
            string_type->source_location = first_string->source_location;
            cast->source_location = first_string->source_location;
            middle_string->source_location = middle_string_literal->source_location;
            first_middle_add->source_location = first_string->source_location;

            return first_middle_last_add;
        }
        auto pass_build_bind_monad::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            AstValueBase* bind_value = WO_NEED_AST_VALUE(0);
            AstValueBase* bind_func = WO_NEED_AST_VALUE(2);

            AstIdentifier* bind_identifier = new AstIdentifier(WO_PSTR(bind));
            AstValueVariable* bind_variable = new AstValueVariable(bind_identifier);
            AstValueFunctionCall* bind_call = new AstValueFunctionCall(true, bind_variable, { bind_value, bind_func });

            // Update source location
            bind_identifier->source_location = bind_value->source_location;
            bind_variable->source_location = bind_value->source_location;

            return bind_call;
        }
        auto pass_build_map_monad::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            AstValueBase* map_value = WO_NEED_AST_VALUE(0);
            AstValueBase* map_func = WO_NEED_AST_VALUE(2);

            AstIdentifier* map_identifier = new AstIdentifier(WO_PSTR(map));
            AstValueVariable* map_variable = new AstValueVariable(map_identifier);
            AstValueFunctionCall* map_call = new AstValueFunctionCall(true, map_variable, { map_value, map_func });

            // Update source location
            map_identifier->source_location = map_value->source_location;
            map_variable->source_location = map_value->source_location;

            return map_call;
        }
        auto pass_normal_function_call::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            AstValueBase* call_func = WO_NEED_AST_VALUE(0);
            AstList* call_arguments = static_cast<AstList*>(WO_NEED_AST_TYPE(1, AstBase::AST_LIST));

            std::list<AstValueBase*> arguments;
            for (auto& argument : call_arguments->m_list)
            {
                wo_assert(argument->node_type >= AstBase::AST_VALUE_begin && argument->node_type < AstBase::AST_VALUE_end);
                arguments.push_back(static_cast<AstValueBase*>(argument));
            }

            return new AstValueFunctionCall(false, call_func, arguments);
        }
        auto pass_directly_function_call::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            AstValueBase* first_argument = WO_NEED_AST_VALUE(0);
            AstValueBase* call_func = WO_NEED_AST_VALUE(2);

            return new AstValueFunctionCall(true, call_func, { first_argument });
        }
        auto pass_directly_function_call_append_arguments::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            AstValueFunctionCall* direct_call =
                static_cast<AstValueFunctionCall*>(WO_NEED_AST_TYPE(0, AstBase::AST_VALUE_FUNCTION_CALL));
            std::optional<AstList*> arguments = std::nullopt;

            wo_assert(direct_call->m_is_direct_call);

            if (!WO_IS_EMPTY(1))
                arguments = static_cast<AstList*>(WO_NEED_AST_TYPE(1, AstBase::AST_LIST));

            if (arguments)
            {
                for (auto* argument : arguments.value()->m_list)
                {
                    wo_assert(argument->node_type >= AstBase::AST_VALUE_begin && argument->node_type < AstBase::AST_VALUE_end);
                    direct_call->m_arguments.push_back(static_cast<AstValueBase*>(argument));
                }
            }
            return direct_call;
        }
        auto pass_inverse_function_call::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            AstValueFunctionCall* function_call = 
                static_cast<AstValueFunctionCall*>(WO_NEED_AST_TYPE(0, AstBase::AST_VALUE_FUNCTION_CALL));
            AstValueBase* inverse_argument = WO_NEED_AST_VALUE(2);

            if (function_call->m_is_direct_call)
            {
                auto insert_place = function_call->m_arguments.cbegin();
                ++insert_place;
                function_call->m_arguments.insert(insert_place, inverse_argument);
            }
            else
            {
                function_call->m_is_direct_call = true;
                function_call->m_arguments.push_front(inverse_argument);
            }

            return function_call;
        }
        auto pass_union_item::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            token union_item = WO_NEED_TOKEN(0);

            return new AstUnionItem(wstring_pool::get_pstr(union_item.identifier), std::nullopt);
        }
        auto pass_union_item_constructor::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            token union_item = WO_NEED_TOKEN(0);
            AstTypeHolder* constructor_type = static_cast<AstTypeHolder*>(WO_NEED_AST_TYPE(2, AstBase::AST_TYPE_HOLDER));

            return new AstUnionItem(wstring_pool::get_pstr(union_item.identifier), constructor_type);
        }
        auto pass_union_declare::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            std::optional<AstDeclareAttribue*> attrib = std::nullopt;
            token union_name = WO_NEED_TOKEN(2);
            std::optional<AstList*> union_template_params = std::nullopt;
            AstList* union_items = static_cast<AstList*>(WO_NEED_AST_TYPE(5, AstBase::AST_LIST));

            if (!WO_IS_EMPTY(0))
                attrib = static_cast<AstDeclareAttribue*>(WO_NEED_AST_TYPE(0, AstBase::AST_DECLARE_ATTRIBUTE));
            if (!WO_IS_EMPTY(3))
                union_template_params = static_cast<AstList*>(WO_NEED_AST_TYPE(3, AstBase::AST_LIST));

            std::list<AstUnionItem*> items;
            for (auto& item : union_items->m_list)
            {
                wo_assert(item->node_type == AstBase::AST_UNION_ITEM);
                items.push_back(static_cast<AstUnionItem*>(item));
            }

            std::optional<std::list<wo_pstring_t>> template_params = std::nullopt;
            if (union_template_params)
            {
                std::list<wo_pstring_t> params;
                for (auto& param : union_template_params.value()->m_list)
                {
                    wo_assert(param->node_type == AstBase::AST_TOKEN);
                    params.push_back(wstring_pool::get_pstr(static_cast<AstToken*>(param)->m_token.identifier));
                }
                template_params = std::move(params);
            }

            return new AstUnionDeclare(attrib, wstring_pool::get_pstr(union_name.identifier), template_params, items);
        }
        auto pass_union_pattern_identifier_or_takeplace::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            token identifier = WO_NEED_TOKEN(0);

            wo_pstring_t identifier_name = wstring_pool::get_pstr(identifier.identifier);
            if (identifier_name == WO_PSTR(_))
                return new AstPatternTakeplace();
            else
                return new AstPatternUnion(identifier_name, std::nullopt);
        }
        auto pass_union_pattern_contain_element::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            token identifier = WO_NEED_TOKEN(0);
            AstPatternBase* element = static_cast<AstPatternBase*>(WO_NEED_AST_PATTERN(2));

            return new AstPatternUnion(wstring_pool::get_pstr(identifier.identifier), element);
        }
        auto pass_match_union_case::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            AstPatternBase* pattern = static_cast<AstPatternBase*>(WO_NEED_AST_PATTERN(0));
            AstBase* body = WO_NEED_AST(1);

            return new AstMatchCase(pattern, body);
        }
        auto pass_match::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            AstValueBase* match_value = WO_NEED_AST_VALUE(2);
            AstList* match_cases = static_cast<AstList*>(WO_NEED_AST_TYPE(5, AstBase::AST_LIST));

            std::list<AstMatchCase*> cases;
            for (auto& match_case : match_cases->m_list)
            {
                wo_assert(match_case->node_type == AstBase::AST_MATCH_CASE);
                cases.push_back(static_cast<AstMatchCase*>(match_case));
            }

            return new AstMatch(match_value, cases);
        }
        auto pass_pattern_identifier_or_takepace::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            token identifier = WO_NEED_TOKEN(0);

            wo_pstring_t identifier_name = wstring_pool::get_pstr(identifier.identifier);
            if (identifier_name == WO_PSTR(_))
                return new AstPatternTakeplace();
            else
                return new AstPatternSingle(false, identifier_name, std::nullopt);
        }
        auto pass_pattern_mut_identifier_or_takepace::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            token identifier = WO_NEED_TOKEN(1);

            wo_pstring_t identifier_name = wstring_pool::get_pstr(identifier.identifier);
            if (identifier_name == WO_PSTR(_))
                return new AstPatternTakeplace();
            else
                return new AstPatternSingle(true, identifier_name, std::nullopt);
        }
        auto pass_pattern_identifier_or_takepace_with_template::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            token identifier = WO_NEED_TOKEN(0);
            AstList* template_arguments = static_cast<AstList*>(WO_NEED_AST_TYPE(1, AstBase::AST_LIST));

            // identifier_name == WO_PSTR(_)
            // `_` here is useless and meaningless.
            // TODO: Give error message.

            wo_pstring_t identifier_name = wstring_pool::get_pstr(identifier.identifier);
            std::list<wo_pstring_t> args;
            for (auto& arg : template_arguments->m_list)
            {
                wo_assert(arg->node_type == AstBase::AST_TOKEN);
                args.push_back(wstring_pool::get_pstr(static_cast<AstToken*>(arg)->m_token.identifier));
            }
            return new AstPatternSingle(false, identifier_name, args);
        }
        auto pass_pattern_mut_identifier_or_takepace_with_template::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            token identifier = WO_NEED_TOKEN(1);
            AstList* template_arguments = static_cast<AstList*>(WO_NEED_AST_TYPE(2, AstBase::AST_LIST));

            // identifier_name == WO_PSTR(_)
            // `_` here is useless and meaningless.
            // TODO: Give error message.

            wo_pstring_t identifier_name = wstring_pool::get_pstr(identifier.identifier);
            std::list<wo_pstring_t> args;
            for (auto& arg : template_arguments->m_list)
            {
                wo_assert(arg->node_type == AstBase::AST_TOKEN);
                args.push_back(wstring_pool::get_pstr(static_cast<AstToken*>(arg)->m_token.identifier));
            }
            return new AstPatternSingle(true, identifier_name, args);
        }
        auto pass_pattern_tuple::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            std::optional<AstList*> pattern_list = std::nullopt;

            if (!WO_IS_EMPTY(1))
                pattern_list = static_cast<AstList*>(WO_NEED_AST_TYPE(1, AstBase::AST_LIST));

            std::list<AstPatternBase*> patterns;
            if (pattern_list)
            {
                for (auto& pattern : pattern_list.value()->m_list)
                {
                    wo_assert(pattern->node_type >= AstBase::AST_PATTERN_begin && pattern->node_type < AstBase::AST_PATTERN_end);
                    patterns.push_back(static_cast<AstPatternBase*>(pattern));
                }
            }
            return new AstPatternTuple(patterns);
        }
        auto pass_macro_failed::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            wo_assert(WO_NEED_TOKEN(0).type == lex_type::l_macro);
            return token{ lex.parser_error(lexer::errorlevel::error, WO_ERR_UNKNOWN_MACRO_NAMED, WO_NEED_TOKEN(0).identifier.c_str()) };
        }
        auto pass_variable_define_item::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            AstPatternBase* define_pattern = static_cast<AstPatternBase*>(WO_NEED_AST_PATTERN(0));
            AstValueBase* define_value = WO_NEED_AST_VALUE(2);

            return new AstVariableDefineItem(define_pattern, define_value);
        }
        auto pass_variable_defines::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            std::optional<AstDeclareAttribue*> attrib = std::nullopt;
            AstList* items = static_cast<AstList*>(WO_NEED_AST_TYPE(2, AstBase::AST_LIST));

            if (!WO_IS_EMPTY(0))
                attrib = static_cast<AstDeclareAttribue*>(WO_NEED_AST_TYPE(0, AstBase::AST_DECLARE_ATTRIBUTE));

            std::list<AstVariableDefineItem*> defines;
            for (auto& item : items->m_list)
            {
                wo_assert(item->node_type == AstBase::AST_VARIABLE_DEFINE_ITEM);
                defines.push_back(static_cast<AstVariableDefineItem*>(item));
            }

            AstVariableDefines* variable_defines = new AstVariableDefines(attrib);
            variable_defines->m_definitions = std::move(defines);

            return variable_defines;
        }
        auto pass_conditional_expression::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            AstValueBase* condition = WO_NEED_AST_VALUE(0);
            AstValueBase* true_branch = WO_NEED_AST_VALUE(2);
            AstValueBase* false_branch = WO_NEED_AST_VALUE(4);

            return new AstValueTribleOperator(condition, true_branch, false_branch);
        }
        auto pass_type_from_identifier::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            AstIdentifier* identifier = static_cast<AstIdentifier*>(WO_NEED_AST_TYPE(0, AstBase::AST_IDENTIFIER));
            return new AstTypeHolder(identifier);
        }
        auto pass_check_type_as::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            AstValueBase* value = WO_NEED_AST_VALUE(0);
            AstTypeHolder* type = static_cast<AstTypeHolder*>(WO_NEED_AST_TYPE(1, AstBase::AST_TYPE_HOLDER));

            return new AstValueTypeCheckAs(type, value);
        }
        auto pass_check_type_is::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            AstValueBase* value = WO_NEED_AST_VALUE(0);
            AstTypeHolder* type = static_cast<AstTypeHolder*>(WO_NEED_AST_TYPE(1, AstBase::AST_TYPE_HOLDER));

            return new AstValueTypeCheckIs(type, value);
        }
        auto pass_struct_member_init_pair::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            token member_name = WO_NEED_TOKEN(0);
            AstValueBase* member_value = WO_NEED_AST_VALUE(2);

            return new AstStructFieldValuePair(
                wstring_pool::get_pstr(member_name.identifier), member_value);
        }
        auto pass_struct_instance::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            std::optional<AstTypeHolder*> type = std::nullopt;
            AstList* members = static_cast<AstList*>(WO_NEED_AST_TYPE(2, AstBase::AST_LIST));

            if (!WO_IS_EMPTY(0))
                type = static_cast<AstTypeHolder*>(WO_NEED_AST_TYPE(0, AstBase::AST_TYPE_HOLDER));

            std::list<AstStructFieldValuePair*> fields;
            for (auto& field : members->m_list)
            {
                wo_assert(field->node_type == AstBase::AST_STRUCT_FIELD_VALUE_PAIR);
                fields.push_back(static_cast<AstStructFieldValuePair*>(field));
            }

            return new AstValueStruct(type, fields);
        }
        auto pass_array_instance::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            AstList* elements = static_cast<AstList*>(WO_NEED_AST_TYPE(1, AstBase::AST_LIST));
           
            std::list<AstValueBase*> values;
            for (auto& element : elements->m_list)
            {
                wo_assert(element->node_type >= AstBase::AST_VALUE_begin && element->node_type < AstBase::AST_VALUE_end);
                values.push_back(static_cast<AstValueBase*>(element));
            }

            return new AstValueArrayOrVec(values, false);
        }
        auto pass_vec_instance::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            AstList* elements = static_cast<AstList*>(WO_NEED_AST_TYPE(1, AstBase::AST_LIST));

            std::list<AstValueBase*> values;
            for (auto& element : elements->m_list)
            {
                wo_assert(element->node_type >= AstBase::AST_VALUE_begin && element->node_type < AstBase::AST_VALUE_end);
                values.push_back(static_cast<AstValueBase*>(element));
            }

            return new AstValueArrayOrVec(values, true);
        }
        auto pass_dict_field_init_pair::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            AstValueArrayOrVec* key = static_cast<AstValueArrayOrVec*>(WO_NEED_AST_TYPE(0, AstBase::AST_VALUE_ARRAY_OR_VEC));
            AstValueBase* value = WO_NEED_AST_VALUE(2);

            if (key->m_making_vec || key->m_elements.size() != 1)
                return token{ lex.lang_error(lexer::errorlevel::error, key, WO_ERR_INVALID_KEY_EXPR) };

            // NOTE: Abondon the key node.

            return new AstKeyValuePair(key->m_elements.front(), value);
        }
        auto pass_dict_instance::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            AstList* elements = static_cast<AstList*>(WO_NEED_AST_TYPE(1, AstBase::AST_LIST));

            std::list<AstKeyValuePair*> pairs;
            for (auto& element : elements->m_list)
            {
                wo_assert(element->node_type == AstBase::AST_KEY_VALUE_PAIR);
                pairs.push_back(static_cast<AstKeyValuePair*>(element));
            }

            return new AstValueDictOrMap(pairs, false);
        }
        auto pass_map_instance::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            AstList* elements = static_cast<AstList*>(WO_NEED_AST_TYPE(1, AstBase::AST_LIST));

            std::list<AstKeyValuePair*> pairs;
            for (auto& element : elements->m_list)
            {
                wo_assert(element->node_type == AstBase::AST_KEY_VALUE_PAIR);
                pairs.push_back(static_cast<AstKeyValuePair*>(element));
            }

            return new AstValueDictOrMap(pairs, true);
        }
        auto pass_tuple_instance::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            AstList* elements = static_cast<AstList*>(WO_NEED_AST_TYPE(0, AstBase::AST_LIST));

            std::list<AstValueBase*> values;
            for (auto& element : elements->m_list)
            {
                wo_assert(element->node_type >= AstBase::AST_VALUE_begin && element->node_type < AstBase::AST_VALUE_end);
                values.push_back(static_cast<AstValueBase*>(element));
            }

            return new AstValueTuple(values);
        }
        auto pass_index_operation_regular::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            AstValueBase* value = WO_NEED_AST_VALUE(0);
            AstValueBase* index = WO_NEED_AST_VALUE(2);

            return new AstValueIndex(value, index);
        }
        auto pass_index_operation_member::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            AstValueBase* value = WO_NEED_AST_VALUE(0);
            AstToken* index = static_cast<AstToken*>(WO_NEED_AST_TYPE(2, AstBase::AST_TOKEN));

            AstValueLiteral* index_literal;
            switch (index->m_token.type)
            {
            case lex_type::l_identifier:
            {
                index_literal = new AstValueLiteral();
                index_literal->decide_final_constant_value(wstrn_to_str(index->m_token.identifier));
                break;
            }
            case lex_type::l_literal_integer:
            {
                wo::value index_value;
                index_value.set_integer((wo_integer_t)std::stoll(index->m_token.identifier));
                index_literal = new AstValueLiteral();
                index_literal->decide_final_constant_value(index_value);
                break;
            }
            default:
                wo_error("Unknown index type.");
                return token{ lex_type::l_error };
            }

            return new AstValueIndex(value, index_literal);
        }
        auto pass_expand_arguments::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            AstValueBase* value = WO_NEED_AST_VALUE(0);
            return new AstFakeValueUnpack(value);
        }
        auto pass_variadic_arguments_pack::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            return new AstValueVariadicArgumentsPack();
        }
        auto pass_extern::build(lexer& lex, const ast::astnode_builder::inputs_t& input)->grammar::produce
        {
            token symbol;
            std::optional<token> library = std::nullopt;
            std::optional<AstList*> attributes = std::nullopt;

            if (input.size() == 7)
            {
                library = WO_NEED_TOKEN(2);
                symbol = WO_NEED_TOKEN(4);
                if (!WO_IS_EMPTY(3))
                    attributes = static_cast<AstList*>(WO_NEED_AST_TYPE(0, AstBase::AST_LIST));
            }
            else
            {
                wo_assert(input.size() == 5);
                symbol = WO_NEED_TOKEN(2);
                if (!WO_IS_EMPTY(3))
                    attributes = static_cast<AstList*>(WO_NEED_AST_TYPE(0, AstBase::AST_LIST));
            }

            uint32_t attribute_mask = 0;
            if (attributes)
            {
                for (auto& attribute : attributes.value()->m_list)
                {
                    wo_assert(attribute->node_type == AstBase::AST_TOKEN);
                    AstToken* attribute_token = static_cast<AstToken*>(attribute);

                    wo_assert(attribute_token->m_token.type == lex_type::l_identifier);

                    if (attribute_token->m_token.identifier == L"slow")
                        attribute_mask |= AstExternInformation::SLOW;
                    else if (attribute_token->m_token.identifier == L"repeat")
                        attribute_mask |= AstExternInformation::REPEATABLE;
                    else
                        return token{ lex.lang_error(lexer::errorlevel::error, attribute, WO_ERR_UNKNOWN_EXTERN_ATTRIB) };
                }
            }

            return new AstExternInformation(
                wo::wstring_pool::get_pstr(symbol.identifier),
                library 
                    ? std::optional(wo::wstring_pool::get_pstr(library->identifier)) 
                    : std::nullopt, 
                attribute_mask);
        }
    }

    namespace ast
    {
        void init_builder() 
        {
#define WO_AST_BUILDER(...) _registed_builder_function_id_list[meta::type_hash<__VA_ARGS__>] = _register_builder<__VA_ARGS__>(); 
            WO_AST_BUILDER(pass_direct<0>);
            WO_AST_BUILDER(pass_direct<1>);
            WO_AST_BUILDER(pass_create_list<0>);
            WO_AST_BUILDER(pass_create_list<1>);
            WO_AST_BUILDER(pass_append_list<1, 0>);
            WO_AST_BUILDER(pass_append_list<0, 1>);
            WO_AST_BUILDER(pass_append_list<2, 0>);
            WO_AST_BUILDER(pass_append_list<1, 2>);
            WO_AST_BUILDER(pass_append_list<1, 3>);
            WO_AST_BUILDER(pass_sentence_block<0>);
            WO_AST_BUILDER(pass_sentence_block<1>);
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
            WO_AST_BUILDER(pass_macro_failed);
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
    }

    grammar::rule operator>>(grammar::rule ost, size_t builder_index)
    {
        ost.first.builder_index = builder_index;
        return ost;
    }
}