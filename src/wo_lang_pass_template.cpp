#include "wo_lang.hpp"

namespace wo
{
#ifndef WO_DISABLE_COMPILER

    std::optional<lang_TemplateAstEvalStateBase*> LangContext::begin_eval_template_ast(
        lexer& lex,
        ast::AstBase* node,
        lang_Symbol* templating_symbol, 
        const lang_Symbol::TemplateArgumentListT& template_arguments,
        PassProcessStackT& out_stack)
    {
        wo_assert(templating_symbol->m_is_template);

        lang_TemplateAstEvalStateBase* result;
        const std::list<wo_pstring_t>* template_params;

        switch (templating_symbol->m_symbol_kind)
        {
        case lang_Symbol::kind::VARIABLE:
        {
            auto& template_variable_prefab = templating_symbol->m_template_value_instances;
            if (template_arguments.size() != template_variable_prefab->m_template_params.size())
            {
                lex.lang_error(lexer::errorlevel::error, node,
                    WO_ERR_UNEXPECTED_TEMPLATE_COUNT,
                    template_variable_prefab->m_template_params.size());

                return std::nullopt;
            }

            auto* template_eval_state_instance = 
                template_variable_prefab->find_or_create_template_instance(
                    template_arguments);

            result = template_eval_state_instance;
            template_params = &template_variable_prefab->m_template_params;
            break;
        }
        case lang_Symbol::kind::ALIAS:
        {
            auto& template_alias_prefab = templating_symbol->m_template_alias_instances;
            if (template_arguments.size() != template_alias_prefab->m_template_params.size())
            {
                lex.lang_error(lexer::errorlevel::error, node,
                    WO_ERR_UNEXPECTED_TEMPLATE_COUNT,
                    template_alias_prefab->m_template_params.size());

                return std::nullopt;
            }

            auto* template_eval_state_instance =
                template_alias_prefab->find_or_create_template_instance(
                    template_arguments);

            result = template_eval_state_instance;
            template_params = &template_alias_prefab->m_template_params;
            break;
        }
        case lang_Symbol::kind::TYPE:
        {
            auto& template_type_prefab = templating_symbol->m_template_type_instances;
            if (template_arguments.size() != template_type_prefab->m_template_params.size())
            {
                lex.lang_error(lexer::errorlevel::error, node,
                    WO_ERR_UNEXPECTED_TEMPLATE_COUNT,
                    template_type_prefab->m_template_params.size());

                return std::nullopt;
            }

            auto* template_eval_state_instance =
                template_type_prefab->find_or_create_template_instance(
                    template_arguments);

            result = template_eval_state_instance;
            template_params = &template_type_prefab->m_template_params;
            break;
        }
        default:
            wo_error("Unexpected symbol kind");
            return std::nullopt;
        }

        switch (result->m_state)
        {
        case lang_TemplateAstEvalStateValue::EVALUATED:
            break;
        case lang_TemplateAstEvalStateValue::FAILED:
            // TODO: Collect error message in template eval.
            lex.lang_error(lexer::errorlevel::error, node,
                WO_ERR_VALUE_TYPE_DETERMINED_FAILED);
            return std::nullopt;
        case lang_TemplateAstEvalStateValue::EVALUATING:
            // NOTE: Donot modify eval state here.
            //  Some case like `is pending` may meet this error but it's not a real error.
            lex.lang_error(lexer::errorlevel::error, node,
                WO_ERR_RECURSIVE_TEMPLATE_INSTANCE);
            return std::nullopt;
        case lang_TemplateAstEvalStateValue::UNPROCESSED:
            result->m_state = lang_TemplateAstEvalStateValue::EVALUATING;

            entry_spcify_scope(templating_symbol->m_belongs_to_scope); // Entry the scope where template variable defined.
            begin_new_scope(); // Begin new scope for defining template type alias.
            fast_create_template_type_alias_in_current_scope(
                lex, templating_symbol, *template_params, template_arguments);

            WO_CONTINUE_PROCESS(result->m_ast);
            break;
        }
        return result;
    }

    void LangContext::finish_eval_template_ast(
        lang_TemplateAstEvalStateBase* template_eval_instance)
    {
        wo_assert(template_eval_instance->m_state == lang_TemplateAstEvalStateBase::EVALUATING);
        auto* templating_symbol = template_eval_instance->m_symbol;;

        // Continue template evaluating.
        end_last_scope(); // Leave temporary scope for template type alias.
        end_last_scope(); // Leave scope where template variable defined.

        switch (templating_symbol->m_symbol_kind)
        {
        case lang_Symbol::kind::VARIABLE:
        {
            lang_TemplateAstEvalStateValue* template_eval_instance_value =
                static_cast<lang_TemplateAstEvalStateValue*>(template_eval_instance);

            ast::AstValueBase* ast_value = static_cast<ast::AstValueBase*>(
                template_eval_instance_value->m_ast);

            auto new_template_variable_instance = std::make_unique<lang_ValueInstance>(
                templating_symbol->m_template_value_instances->m_mutable, templating_symbol);

            std::optional<wo::value> constant_value = std::nullopt;
            if (!new_template_variable_instance->m_mutable && template_eval_instance_value->m_ast)
            {
                constant_value = ast_value->m_evaled_const_value;
            }
            new_template_variable_instance->determined_value_instance(
                ast_value->m_LANG_determined_type.value(), constant_value);

            template_eval_instance_value->m_value_instance = 
                std::move(new_template_variable_instance);

            template_eval_instance_value->m_state = 
                lang_TemplateAstEvalStateValue::EVALUATED;

            break;
        }
        case lang_Symbol::kind::ALIAS:
        {
            lang_TemplateAstEvalStateAlias* template_eval_instance_alias =
                static_cast<lang_TemplateAstEvalStateAlias*>(template_eval_instance);

            ast::AstTypeHolder* ast_type = static_cast<ast::AstTypeHolder*>(
                template_eval_instance_alias->m_ast);

            auto new_template_alias_instance = std::make_unique<lang_AliasInstance>(
                templating_symbol);

            new_template_alias_instance->m_determined_type = ast_type->m_LANG_determined_type;
            wo_assert(new_template_alias_instance->m_determined_type.has_value());

            template_eval_instance_alias->m_alias_instance =
                std::move(new_template_alias_instance);

            template_eval_instance_alias->m_state =
                lang_TemplateAstEvalStateAlias::EVALUATED;

            break;
        }
        case lang_Symbol::kind::TYPE:
        {
            lang_TemplateAstEvalStateType* template_eval_instance_type =
                static_cast<lang_TemplateAstEvalStateType*>(template_eval_instance);

            ast::AstTypeHolder* ast_type = static_cast<ast::AstTypeHolder*>(
                template_eval_instance_type->m_ast);

            auto new_template_type_instance = std::make_unique<lang_TypeInstance>(
                templating_symbol);

            new_template_type_instance->determine_base_type(
                ast_type->m_LANG_determined_type.value()->get_determined_type());

            template_eval_instance_type->m_type_instance =
                std::move(new_template_type_instance);

            template_eval_instance_type->m_state =
                lang_TemplateAstEvalStateType::EVALUATED;

            break;
        }
        default:
            wo_error("Unexpected symbol kind");
            break;
        }
    }

    void LangContext::failed_eval_template_ast(
        lang_TemplateAstEvalStateBase* template_eval_instance)
    {
        // Child failed, we need pop the scope anyway.
        end_last_scope(); // Leave temporary scope for template type alias.
        end_last_scope(); // Leave scope where template variable defined.

        template_eval_instance->m_state = lang_TemplateAstEvalStateBase::FAILED;
    }

#endif
}