#include "wo_lang.hpp"

namespace wo
{
#ifndef WO_DISABLE_COMPILER

    void LangContext::_collect_failed_template_instance(
        lexer& lex, ast::AstBase* node, lang_TemplateAstEvalStateBase* inst)
    {
        std::list<ast::AstTemplateParam*>* template_params = nullptr;
        switch (inst->m_symbol->m_symbol_kind)
        {
        case lang_Symbol::kind::VARIABLE:
            template_params = &inst->m_symbol->m_template_value_instances->m_template_params;
            break;
        case lang_Symbol::kind::ALIAS:
            template_params = &inst->m_symbol->m_template_alias_instances->m_template_params;
            break;
        case lang_Symbol::kind::TYPE:
            template_params = &inst->m_symbol->m_template_type_instances->m_template_params;
            break;

        default:
            wo_error("Unexpected symbol kind");
            break;
        }

        std::wstring failed_template_arg_list;

        auto it_template_arg = inst->m_template_arguments.begin();
        const auto it_template_arg_end = inst->m_template_arguments.end();
        auto it_template_param = template_params->begin();
        const auto it_template_param_end = template_params->end();

        for (; it_template_arg != it_template_arg_end
            && it_template_param != it_template_param_end;
            ++it_template_param, ++it_template_arg)
        {
            if (!failed_template_arg_list.empty())
                failed_template_arg_list += L", ";

            failed_template_arg_list += *(*it_template_param)->m_param_name;
            failed_template_arg_list += L" = ";

            if (it_template_arg->m_constant.has_value())
                failed_template_arg_list += L"{"
                + get_constant_str_w(it_template_arg->m_constant.value())
                + L": "
                + get_type_name_w(it_template_arg->m_type)
                + L"}";
            else
                failed_template_arg_list += get_type_name_w(it_template_arg->m_type);
        }

        lex.record_lang_error(lexer::msglevel_t::error, node,
            WO_ERR_FAILED_REIFICATION_CAUSED_BY,
            failed_template_arg_list.c_str(),
            get_symbol_name_w(inst->m_symbol));

        for (const auto& error_message : inst->m_failed_error_for_this_instance.value())
            // TODO: Describe the error support.
            lex.append_message(error_message).m_layer += error_message.m_layer + 1;
    }

    bool check_template_argument_count_and_type(
        lexer& lex,
        ast::AstBase* node,
        lang_Symbol::TemplateArgumentListT& template_arguments,
        const std::list<ast::AstTemplateParam*>& template_params)
    {
        if (template_arguments.size() == template_params.size())
            return true;

        if (template_arguments.size() < template_params.size())
        {
            // WO_ERR_NOT_ALL_TEMPLATE_ARGUMENT_DETERMINED has been raised.
            wo_assert(!lex.get_current_error_frame().empty());
        }
        else
        {
            lex.record_lang_error(lexer::msglevel_t::error, node,
                WO_ERR_UNEXPECTED_TEMPLATE_COUNT,
                template_params.size());
        }
        return false;
    }

    void entry_symbol_scope(LangContext* ctx, lang_Symbol* symbol)
    {
        bool has_enter_specify_scope = false;
        if (symbol->m_symbol_kind == lang_Symbol::kind::TYPE)
        {
            if (symbol->m_belongs_to_scope->is_namespace_scope())
            {
                lang_Namespace* type_namespace = symbol->m_belongs_to_scope->m_belongs_to_namespace;
                auto fnd = type_namespace->m_sub_namespaces.find(symbol->m_name);
                if (fnd != type_namespace->m_sub_namespaces.end())
                {
                    has_enter_specify_scope = true;
                    ctx->entry_spcify_scope(fnd->second->m_this_scope.get());
                }
            }
        }

        if (!has_enter_specify_scope)
            ctx->entry_spcify_scope(symbol->m_belongs_to_scope);
    }

    void check_and_do_final_deduce_for_constant_template_argument(
        LangContext* ctx,
        lexer& lex,
        ast::AstBase* node,
        lang_Symbol* symbol,
        lang_Symbol::TemplateArgumentListT& inout_template_arguments,
        const std::list<ast::AstTemplateParam*>& template_params)
    {
        if (inout_template_arguments.size() < template_params.size())
        {
            entry_symbol_scope(ctx, symbol);
            ctx->begin_new_scope(std::nullopt);

            std::unordered_map<wo_pstring_t, ast::AstIdentifier::TemplateArgumentInstance> deduce_result;

            // Define filled template arguments.
            auto inout_template_arguments_iter = inout_template_arguments.begin();
            const auto inout_template_arguments_iter_end = inout_template_arguments.end();
            auto template_params_iter = template_params.begin();

            for (; inout_template_arguments_iter != inout_template_arguments_iter_end;
                ++inout_template_arguments_iter, ++template_params_iter)
            {
                auto& argument = *inout_template_arguments_iter;
                auto* params = *template_params_iter;

                if (argument.m_constant.has_value() != params->m_marked_type.has_value())
                {
                    if (params->m_marked_type.has_value())
                        lex.record_lang_error(lexer::msglevel_t::error, params,
                            WO_ERR_THIS_TEMPLATE_ARG_SHOULD_BE_CONST);
                    else
                        lex.record_lang_error(lexer::msglevel_t::error, params,
                            WO_ERR_THIS_TEMPLATE_ARG_SHOULD_BE_TYPE);
                }
                
                (void)ctx->fast_create_one_template_type_alias_and_constant_in_current_scope(
                    params->m_param_name, argument);

                deduce_result.insert(std::make_pair(params->m_param_name, argument));
            }

            auto template_params_iter_end = template_params.end();

            std::list<ast::AstTemplateParam*> pending_template_params;

            for (; template_params_iter != template_params_iter_end; ++template_params_iter)
                pending_template_params.push_back(*template_params_iter);

            if (ctx->template_argument_deduction_from_constant(
                lex, template_params, pending_template_params, &deduce_result))
            {
                inout_template_arguments.clear();

                for (auto* param : template_params)
                {
                    auto fnd = deduce_result.find(param->m_param_name);
                    if (fnd != deduce_result.end())
                        inout_template_arguments.emplace_back(fnd->second);
                }
            }
            
            if (inout_template_arguments.size() != template_params.size())
            {
                std::wstring pending_type_list;
                bool first_param = true;

                for (auto* param : template_params)
                {
                    auto fnd = deduce_result.find(param->m_param_name);
                    if (fnd == deduce_result.end())
                    {
                        if (!first_param)
                            pending_type_list += L", ";
                        else
                            first_param = false;

                        pending_type_list += *param->m_param_name;
                    }
                }

                lex.record_lang_error(lexer::msglevel_t::error, node,
                    WO_ERR_NOT_ALL_TEMPLATE_ARGUMENT_DETERMINED,
                    pending_type_list.c_str());
            }
            

            ctx->end_last_scope();
            ctx->end_last_scope();
        }
    }

    std::optional<lang_TemplateAstEvalStateBase*> LangContext::begin_eval_template_ast(
        lexer& lex,
        ast::AstBase* node,
        lang_Symbol* templating_symbol,
        lang_Symbol::TemplateArgumentListT&& template_arguments,
        PassProcessStackT& out_stack,
        bool* out_continue_process_flag)
    {
        wo_assert(templating_symbol->m_is_template);

        lang_TemplateAstEvalStateBase* result;
        const std::list<ast::AstTemplateParam*>* template_params;

        lex.begin_trying_block();

        switch (templating_symbol->m_symbol_kind)
        {
        case lang_Symbol::kind::VARIABLE:
        {
            auto& template_variable_prefab = templating_symbol->m_template_value_instances;
            check_and_do_final_deduce_for_constant_template_argument(
                this,
                lex,
                node,
                templating_symbol,
                template_arguments,
                template_variable_prefab->m_template_params);

            auto* template_eval_state_instance =
                template_variable_prefab->find_or_create_template_instance(
                    template_arguments);

            if (template_eval_state_instance->m_ast->node_type == ast::AstBase::AST_VALUE_FUNCTION)
            {
                ast::AstValueFunction* function = static_cast<ast::AstValueFunction*>(
                    template_eval_state_instance->m_ast);

                function->m_LANG_value_instance_to_update =
                    template_eval_state_instance->m_value_instance.get();
                function->m_LANG_in_template_reification_context = true;
            }

            result = template_eval_state_instance;
            template_params = &template_variable_prefab->m_template_params;
            break;
        }
        case lang_Symbol::kind::ALIAS:
        {
            auto& template_alias_prefab = templating_symbol->m_template_alias_instances;
            check_and_do_final_deduce_for_constant_template_argument(
                this,
                lex,
                node,
                templating_symbol,
                template_arguments,
                template_alias_prefab->m_template_params);

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
            check_and_do_final_deduce_for_constant_template_argument(
                this,
                lex,
                node,
                templating_symbol,
                template_arguments,
                template_type_prefab->m_template_params);

            auto* template_eval_state_instance =
                template_type_prefab->find_or_create_template_instance(
                    template_arguments);

            result = template_eval_state_instance;
            template_params = &template_type_prefab->m_template_params;
            break;
        }
        default:
            wo_error("Unexpected symbol kind");
            lex.end_trying_block();
            return std::nullopt;
        }

        result->m_template_arguments = template_arguments;
        *out_continue_process_flag = false;

        switch (result->m_state)
        {
        case lang_TemplateAstEvalStateValue::state::EVALUATED:
            lex.end_trying_block();
            break;
        case lang_TemplateAstEvalStateValue::state::FAILED:
        {
            lex.end_trying_block();

            _collect_failed_template_instance(lex, node, result);
            return std::nullopt;
        }
        case lang_TemplateAstEvalStateValue::state::EVALUATING:
        {
            lex.end_trying_block();

            if (templating_symbol->m_symbol_kind == lang_Symbol::kind::VARIABLE)
            {
                // For function, recursive template instance is allowed.
                auto* template_eval_state_instance = static_cast<lang_TemplateAstEvalStateValue*>(result);
                if (template_eval_state_instance->m_value_instance->m_determined_type.has_value())
                    // Type has been determined, no need to evaluate again.
                    return result;
            }
            else if (templating_symbol->m_symbol_kind == lang_Symbol::kind::TYPE)
            {
                // For type, it's type instance has been determined, no need to evaluate again.
                auto* template_eval_state_instance = static_cast<lang_TemplateAstEvalStateType*>(result);
                return result;
            }
            // NOTE: Donot modify eval state here.
            //  Some case like `is pending` may meet this error but it's not a real error.
            lex.record_lang_error(lexer::msglevel_t::error, node,
                WO_ERR_RECURSIVE_TEMPLATE_INSTANCE);
            return std::nullopt;
        }
        case lang_TemplateAstEvalStateValue::state::UNPROCESSED:
            result->m_state = lang_TemplateAstEvalStateValue::state::EVALUATING;
            *out_continue_process_flag = true;

            // Entry the scope where template variable defined.
            entry_symbol_scope(this, templating_symbol);
            begin_new_scope(std::nullopt); // Begin new scope for defining template type alias.

            // ATTENTION: LIMIT TEMPLATE INSTANCE SYMBOL VISIBILITY!
            auto* current_scope = get_current_scope();
            current_scope->m_visibility_from_edge_for_template_check =
                templating_symbol->m_symbol_edge;

            ast::AstTemplateConstantTypeCheckInPass1* checker =
                new ast::AstTemplateConstantTypeCheckInPass1(result->m_ast);

            if (!check_template_argument_count_and_type(lex, node, template_arguments, *template_params)
                || !fast_check_and_create_template_type_alias_and_constant_in_current_scope(
                    lex, *template_params, template_arguments, checker))
            {
                failed_eval_template_ast(lex, node, result);
                return std::nullopt;
            }
            else
                WO_CONTINUE_PROCESS(checker);

            break;
        }
        return result;
    }

    void LangContext::finish_eval_template_ast(
        lexer& lex, lang_TemplateAstEvalStateBase* template_eval_instance)
    {
        wo_assert(template_eval_instance->m_state == lang_TemplateAstEvalStateBase::state::EVALUATING);
        auto* templating_symbol = template_eval_instance->m_symbol;;

        // Continue template evaluating.
        end_last_scope(); // Leave temporary scope for template type alias.
        end_last_scope(); // Leave scope where template variable defined.

        wo_assert(lex.get_current_error_frame().empty());
        lex.end_trying_block();

        switch (templating_symbol->m_symbol_kind)
        {
        case lang_Symbol::kind::VARIABLE:
        {
            lang_TemplateAstEvalStateValue* template_eval_instance_value =
                static_cast<lang_TemplateAstEvalStateValue*>(template_eval_instance);

            ast::AstValueBase* ast_value = static_cast<ast::AstValueBase*>(
                template_eval_instance_value->m_ast);

            auto* new_template_variable_instance =
                template_eval_instance_value->m_value_instance.get();

            new_template_variable_instance->m_determined_type =
                ast_value->m_LANG_determined_type.value();

            if (!new_template_variable_instance->m_mutable)
                new_template_variable_instance->try_determine_const_value(ast_value);

            template_eval_instance_value->m_state =
                lang_TemplateAstEvalStateValue::state::EVALUATED;

            break;
        }
        case lang_Symbol::kind::ALIAS:
        {
            lang_TemplateAstEvalStateAlias* template_eval_instance_alias =
                static_cast<lang_TemplateAstEvalStateAlias*>(template_eval_instance);

            ast::AstTypeHolder* ast_type = static_cast<ast::AstTypeHolder*>(
                template_eval_instance_alias->m_ast);

            auto* new_template_alias_instance =
                template_eval_instance_alias->m_alias_instance.get();

            new_template_alias_instance->m_determined_type = ast_type->m_LANG_determined_type;
            wo_assert(new_template_alias_instance->m_determined_type.has_value());

            template_eval_instance_alias->m_state =
                lang_TemplateAstEvalStateAlias::state::EVALUATED;

            break;
        }
        case lang_Symbol::kind::TYPE:
        {
            lang_TemplateAstEvalStateType* template_eval_instance_type =
                static_cast<lang_TemplateAstEvalStateType*>(template_eval_instance);

            ast::AstTypeHolder* ast_type = static_cast<ast::AstTypeHolder*>(
                template_eval_instance_type->m_ast);

            auto* new_template_type_instance =
                template_eval_instance_type->m_type_instance.get();

            new_template_type_instance->determine_base_type_by_another_type(
                ast_type->m_LANG_determined_type.value());

            template_eval_instance_type->m_state =
                lang_TemplateAstEvalStateType::state::EVALUATED;

            break;
        }
        default:
            wo_error("Unexpected symbol kind");
            break;
        }
    }

    void LangContext::failed_eval_template_ast(
        lexer& lex, ast::AstBase* node, lang_TemplateAstEvalStateBase* template_eval_instance)
    {
        // Child failed, we need pop the scope anyway.
        end_last_scope(); // Leave temporary scope for template type alias.
        end_last_scope(); // Leave scope where template variable defined.

        template_eval_instance->m_state = lang_TemplateAstEvalStateBase::state::FAILED;

        wo_assert(!lex.get_current_error_frame().empty());

        template_eval_instance->m_failed_error_for_this_instance.emplace(
            std::move(lex.get_current_error_frame()));

        lex.end_trying_block();

        const size_t current_error_frame_layer = lex.get_error_frame_layer();
        for (auto& error_message : template_eval_instance->m_failed_error_for_this_instance.value())
            error_message.m_layer -= current_error_frame_layer;

        _collect_failed_template_instance(lex, node, template_eval_instance);
    }

    bool LangContext::template_arguments_deduction_extraction_with_constant(
        lexer& lex,
        ast::AstValueBase* accept_constant_formal,
        lang_TypeInstance* applying_type_instance,
        const value& constant_instance,
        const std::list<ast::AstTemplateParam*>& pending_template_params,
        std::unordered_map<wo_pstring_t, ast::AstIdentifier::TemplateArgumentInstance>* out_determined_template_arg_pair)
    {
        switch (accept_constant_formal->node_type)
        {
        case ast::AstBase::AST_VALUE_VARIABLE:
        {
            ast::AstValueVariable* variable = static_cast<ast::AstValueVariable*>(accept_constant_formal);
            if (variable->m_identifier->m_formal == ast::AstIdentifier::identifier_formal::FROM_CURRENT
                && variable->m_identifier->m_scope.empty())
            {
                auto fnd = std::find_if(
                    pending_template_params.begin(),
                    pending_template_params.end(),
                    [variable](ast::AstTemplateParam* p)
                    {
                        return p->m_param_name == variable->m_identifier->m_name
                            && p->m_marked_type.has_value() /* Is not constant */;
                    });

                if (fnd != pending_template_params.end())
                {
                    // Got it!
                    ast::AstTemplateParam* template_param = *fnd;

                    out_determined_template_arg_pair->insert(
                        std::make_pair(
                            template_param->m_param_name,
                            ast::AstIdentifier::TemplateArgumentInstance(
                                applying_type_instance,
                                constant_instance)));
                }
            }
            break;
        }
        default:
            // Not support other formal now.
            break;
        }
        return true;
    }

    bool LangContext::template_argument_deduction_from_constant(
        lexer& lex,
        const std::list<ast::AstTemplateParam*>& template_params,
        const std::list<ast::AstTemplateParam*>& pending_template_params,
        std::unordered_map<wo_pstring_t, ast::AstIdentifier::TemplateArgumentInstance>* inout_determined_template_arg_pair)
    {
        for (auto* param : template_params)
        {
            auto fnd = inout_determined_template_arg_pair->find(param->m_param_name);
            if (fnd != inout_determined_template_arg_pair->end())
            {
                auto& argument = fnd->second;

                if (argument.m_constant.has_value() && param->m_marked_type.has_value())
                {
                    if (!template_arguments_deduction_extraction_with_type(
                        lex,
                        param->m_marked_type.value(),
                        argument.m_type,
                        pending_template_params,
                        inout_determined_template_arg_pair))
                        return false;
                }
            }
        }
        return true;
    }
    bool LangContext::template_arguments_deduction_extraction_with_type(
        lexer& lex,
        const ast::AstTypeHolder* accept_type_formal,
        lang_TypeInstance* applying_type_instance,
        const std::list<ast::AstTemplateParam*>& pending_template_params,
        std::unordered_map<wo_pstring_t, ast::AstIdentifier::TemplateArgumentInstance>* out_determined_template_arg_pair)
    {
        switch (accept_type_formal->m_formal)
        {
        case ast::AstTypeHolder::IDENTIFIER:
        {
            auto* identifier = accept_type_formal->m_typeform.m_identifier;
            bool try_eval_symbol_of_identifier = true;

            switch (identifier->m_formal)
            {
            case ast::AstIdentifier::identifier_formal::FROM_TYPE:
                // Not support.
                return true;
            case ast::AstIdentifier::identifier_formal::FROM_CURRENT:
                // Current identifier might be template need to be pick.
                if (identifier->m_scope.empty())
                {
                    // TODO: Support HKT?
                    // PERFECT! We got the type we need.

                    auto fnd = std::find_if(
                        pending_template_params.begin(),
                        pending_template_params.end(),
                        [identifier](ast::AstTemplateParam* p)
                        {
                            return p->m_param_name == identifier->m_name
                                && !p->m_marked_type.has_value() /* Is not constant */;
                        });

                    if (fnd != pending_template_params.end())
                    {
                        try_eval_symbol_of_identifier = false;

                        // Got it!
                        ast::AstTemplateParam* template_param = *fnd;

                        switch (accept_type_formal->m_mutable_mark)
                        {
                        case ast::AstTypeHolder::mutable_mark::MARK_AS_MUTABLE:
                            if (applying_type_instance->is_mutable())
                            {
                                // mut T <= mut Tinstance: T = Tinstance
                                out_determined_template_arg_pair->insert(
                                    std::make_pair(template_param->m_param_name, immutable_type(applying_type_instance)));
                            }
                            // Bad, continue;
                            break;
                        case ast::AstTypeHolder::mutable_mark::MARK_AS_IMMUTABLE:
                            if (applying_type_instance->is_mutable())
                            {
                                // immut T <X= mut Tinstance: Bad
                                lex.record_lang_error(lexer::msglevel_t::error, accept_type_formal,
                                    WO_ERR_UNACCEPTABLE_MUTABLE,
                                    get_type_name_w(applying_type_instance));

                                return false;
                            }
                            /* Fall through */
                            [[fallthrough]];
                        case ast::AstTypeHolder::mutable_mark::NONE:
                            out_determined_template_arg_pair->insert(
                                std::make_pair(template_param->m_param_name, applying_type_instance));
                            break;
                        default:
                            wo_error("Unexpected mutable mark");
                        }
                        goto _label_fake_hkt_trying_template;
                    }
                }
                /* FALL THROUGH */
                [[fallthrough]];
            case ast::AstIdentifier::identifier_formal::FROM_GLOBAL:
                do
                {
                    // Try determin symbol:
                    bool ambiguous = false;
                    if (!find_symbol_in_current_scope(lex, identifier, &ambiguous))
                    {
                        lex.record_lang_error(lexer::msglevel_t::error, accept_type_formal,
                            WO_ERR_UNFOUND_TYPE_NAMED,
                            identifier->m_name->c_str());

                        // Not found or ambiguous.
                        return false;
                    }
                    else if (ambiguous)
                        // Ambiguous.
                        return false;

                    auto* determined_type_symbol = identifier->m_LANG_determined_symbol.value();
                    switch (determined_type_symbol->m_symbol_kind)
                    {
                    case lang_Symbol::kind::TYPE:
                        break;
                    case lang_Symbol::kind::ALIAS:
                    {
                        if (determined_type_symbol->m_is_template)
                            // Not support;
                            return true;

                        if (!determined_type_symbol->m_alias_instance->m_determined_type.has_value())
                            // Not determined yet.
                            return true;

                        determined_type_symbol = determined_type_symbol
                            ->m_alias_instance->m_determined_type.value()->m_symbol;

                        break;
                    }
                    default:
                        wo_error("Unexpected symbol kind");
                    }

                    if (determined_type_symbol != applying_type_instance->m_symbol)
                        // Not match!
                        return true;

                } while (0);

            _label_fake_hkt_trying_template:
                // Walk through template arguments.
                if (identifier->m_template_arguments.has_value())
                {
                    // Has template, check if applying_type_instance match.
                    wo_assert(applying_type_instance->m_instance_template_arguments.has_value());

                    auto& identifier_template_arguments =
                        identifier->m_template_arguments.value();
                    auto& instance_template_arguments =
                        applying_type_instance->m_instance_template_arguments.value();

                    wo_assert(identifier_template_arguments.size() == instance_template_arguments.size());

                    auto it_identifier = identifier_template_arguments.begin();
                    auto it_instance = instance_template_arguments.begin();
                    auto it_end = identifier_template_arguments.end();

                    bool all_ok = true;
                    for (; it_identifier != it_end; ++it_identifier, ++it_instance)
                    {
                        all_ok = template_arguments_deduction_extraction_with_formal(
                            lex,
                            *it_identifier,
                            *it_instance,
                            pending_template_params,
                            out_determined_template_arg_pair) && all_ok;
                    }

                    if (!all_ok)
                        return false;
                }
            }

            return true;
        }
        case ast::AstTypeHolder::TYPEOF:
            // Junk! I cannot do any thing!
            return true;
        case ast::AstTypeHolder::FUNCTION:
        {
            if (applying_type_instance->m_symbol != m_origin_types.m_function)
                // User define type;
                return true;

            auto& function = accept_type_formal->m_typeform.m_function;
            auto instance_type_determined_base_may_null = applying_type_instance->get_determined_type();
            if (!instance_type_determined_base_may_null.has_value())
                // Not determined yet.
                return true;

            auto* instance_type_determined_base = instance_type_determined_base_may_null.value();
            if (instance_type_determined_base->m_base_type != lang_TypeInstance::DeterminedType::FUNCTION)
                // Not function.
                return true;

            auto* instance_type_determined_base_fn =
                instance_type_determined_base->m_external_type_description.m_function;

            if (function.m_is_variadic != instance_type_determined_base_fn->m_is_variadic
                || function.m_parameters.size() != instance_type_determined_base_fn->m_param_types.size())
                // Not match.
                return true;

            auto it_fn_param = function.m_parameters.begin();
            auto it_instance_param = instance_type_determined_base_fn->m_param_types.begin();
            auto it_fn_param_end = function.m_parameters.end();

            bool all_ok = true;
            for (; it_fn_param != it_fn_param_end; ++it_fn_param, ++it_instance_param)
            {
                all_ok = template_arguments_deduction_extraction_with_type(
                    lex,
                    *it_fn_param,
                    *it_instance_param,
                    pending_template_params,
                    out_determined_template_arg_pair) && all_ok;
            }

            all_ok = template_arguments_deduction_extraction_with_type(
                lex,
                function.m_return_type,
                instance_type_determined_base_fn->m_return_type,
                pending_template_params,
                out_determined_template_arg_pair) && all_ok;

            return all_ok;
        }
        case ast::AstTypeHolder::STRUCTURE:
        {
            if (applying_type_instance->m_symbol != m_origin_types.m_struct)
                // User define type;
                return true;

            auto& structure = accept_type_formal->m_typeform.m_structure;
            auto instance_type_determined_base_may_null = applying_type_instance->get_determined_type();
            if (!instance_type_determined_base_may_null.has_value())
                // Not determined yet.
                return true;

            auto* instance_type_determined_base = instance_type_determined_base_may_null.value();
            if (instance_type_determined_base->m_base_type != lang_TypeInstance::DeterminedType::STRUCT)
                // Not structure.
                return true;

            auto* instance_type_determined_base_struct =
                instance_type_determined_base->m_external_type_description.m_struct;

            if (structure.m_fields.size() != instance_type_determined_base_struct->m_member_types.size())
                // Not match.
                return true;

            std::list<std::pair<ast::AstTypeHolder*, lang_TypeInstance*>>
                member_type_instance_pairs;

            wo_integer_t member_index = 0;
            for (auto& field : structure.m_fields)
            {
                auto fnd = instance_type_determined_base_struct->m_member_types.find(field->m_name);
                if (fnd == instance_type_determined_base_struct->m_member_types.end())
                    // Not match.
                    return true;

                if (member_index != fnd->second.m_offset)
                    // Not match.
                    return true;

                member_type_instance_pairs.push_back(
                    std::make_pair(field->m_type, fnd->second.m_member_type));

                ++member_index;
            }

            bool all_ok = true;
            for (auto& member_type_instance_pair : member_type_instance_pairs)
            {
                all_ok = template_arguments_deduction_extraction_with_type(
                    lex,
                    member_type_instance_pair.first,
                    member_type_instance_pair.second,
                    pending_template_params,
                    out_determined_template_arg_pair) && all_ok;
            }

            return all_ok;
        }
        case ast::AstTypeHolder::TUPLE:
        {
            if (applying_type_instance->m_symbol != m_origin_types.m_tuple)
                // User define type;
                return true;

            auto& tuple = accept_type_formal->m_typeform.m_tuple;
            auto instance_type_determined_base_may_null = applying_type_instance->get_determined_type();
            if (!instance_type_determined_base_may_null.has_value())
                // Not determined yet.
                return true;

            auto* instance_type_determined_base = instance_type_determined_base_may_null.value();
            if (instance_type_determined_base->m_base_type != lang_TypeInstance::DeterminedType::TUPLE)
                // Not tuple.
                return true;

            auto* instance_type_determined_base_tuple =
                instance_type_determined_base->m_external_type_description.m_tuple;

            if (tuple.m_fields.size() != instance_type_determined_base_tuple->m_element_types.size())
                // Not match.
                return true;

            auto it_tuple_field = tuple.m_fields.begin();
            auto it_instance_field = instance_type_determined_base_tuple->m_element_types.begin();
            auto it_tuple_field_end = tuple.m_fields.end();

            bool all_ok = true;
            for (; it_tuple_field != it_tuple_field_end; ++it_tuple_field, ++it_instance_field)
            {
                all_ok = template_arguments_deduction_extraction_with_type(
                    lex,
                    *it_tuple_field,
                    *it_instance_field,
                    pending_template_params,
                    out_determined_template_arg_pair) && all_ok;
            }

            return all_ok;
        }
        case ast::AstTypeHolder::UNION:
            // That's will never happend.
            /* FALL THROUGH! */
            [[fallthrough]];
        default:
            wo_error("unknown typeholder formal.");
        }
    }

    bool LangContext::template_arguments_deduction_extraction_with_formal(
        lexer& lex,
        const ast::AstTemplateArgument* accept_template_param_formal,
        const ast::AstIdentifier::TemplateArgumentInstance& applying_template_argument_instance,
        const std::list<ast::AstTemplateParam*>& pending_template_params,
        std::unordered_map<wo_pstring_t, ast::AstIdentifier::TemplateArgumentInstance>* out_determined_template_arg_pair)
    {
        if (accept_template_param_formal->is_type()
            != !applying_template_argument_instance.m_constant.has_value())
        {
            // No need to report error.
            return true;
        }
        else
        {
            if (accept_template_param_formal->is_type())
            {
                return template_arguments_deduction_extraction_with_type(
                    lex,
                    accept_template_param_formal->get_type(),
                    applying_template_argument_instance.m_type,
                    pending_template_params,
                    out_determined_template_arg_pair);
            }
            else
            {
                return template_arguments_deduction_extraction_with_constant(
                    lex,
                    accept_template_param_formal->get_constant(),
                    applying_template_argument_instance.m_type,
                    applying_template_argument_instance.m_constant.value(),
                    pending_template_params,
                    out_determined_template_arg_pair);
            }
        }
    }

    bool LangContext::check_type_may_dependence_template_parameters(
        const ast::AstTypeHolder* accept_template_argument_formal,
        const std::list<ast::AstTemplateParam*>& pending_template_params)
    {
        std::vector<bool> template_mask(pending_template_params.size(), false);

        accept_template_argument_formal->_check_if_template_exist_in(
            pending_template_params, template_mask);

        for (bool b : template_mask)
        {
            if (b)
                return true;
        }
        return false;
    }
    bool LangContext::check_constant_may_dependence_template_parameters(
        const ast::AstValueBase* accept_template_argument_formal,
        const std::list<ast::AstTemplateParam*>& pending_template_params)
    {
        std::vector<bool> template_mask(pending_template_params.size(), false);

        accept_template_argument_formal->_check_if_template_exist_in(
            pending_template_params, template_mask);

        for (bool b : template_mask)
        {
            if (b)
                return true;
        }
        return false;
    }

    bool LangContext::check_formal_may_dependence_template_parameters(
        const ast::AstTemplateArgument* accept_template_argument_formal,
        const std::list<ast::AstTemplateParam*>& pending_template_params)
    {

        if (accept_template_argument_formal->is_type())
            return check_type_may_dependence_template_parameters(
                accept_template_argument_formal->get_type(), pending_template_params);
        else
            return check_constant_may_dependence_template_parameters(
                accept_template_argument_formal->get_constant(), pending_template_params);
    }

    void LangContext::template_function_deduction_extraction_with_complete_type(
        lexer& lex,
        ast::AstValueFunction* function_define,
        const std::list<std::optional<lang_TypeInstance*>>& argument_types,
        const std::optional<lang_TypeInstance*>& return_type,
        const std::list<ast::AstTemplateParam*>& pending_template_params,
        std::unordered_map<wo_pstring_t, ast::AstIdentifier::TemplateArgumentInstance>* out_determined_template_arg_pair
    )
    {
        if (function_define->m_marked_return_type.has_value()
            && return_type.has_value())
        {
            template_arguments_deduction_extraction_with_type(
                lex,
                function_define->m_marked_return_type.value(),
                return_type.value(),
                pending_template_params,
                out_determined_template_arg_pair);
        }

        auto it_arg_type = argument_types.begin();
        auto it_arg_type_end = argument_types.end();
        auto it_fn_param = function_define->m_parameters.begin();
        auto it_fn_param_end = function_define->m_parameters.end();

        for (; it_arg_type != it_arg_type_end && it_fn_param != it_fn_param_end;
            ++it_arg_type, ++it_fn_param)
        {
            if (!it_arg_type->has_value())
                continue;

            template_arguments_deduction_extraction_with_type(
                lex,
                (*it_fn_param)->m_type.value(),
                (*it_arg_type).value(),
                pending_template_params,
                out_determined_template_arg_pair);
        }
    }
    ast::AstValueBase* LangContext::get_marked_origin_value_node(ast::AstValueBase* node)
    {
        for (;;)
        {
            switch (node->node_type)
            {
            case ast::AstBase::AST_VALUE_MARK_AS_IMMUTABLE:
            {
                node = static_cast<ast::AstValueMarkAsImmutable*>(node)->m_marked_value;
                continue;
            }
            case ast::AstBase::AST_VALUE_MARK_AS_MUTABLE:
            {
                node = static_cast<ast::AstValueMarkAsMutable*>(node)->m_marked_value;
                continue;
            }
            default:
                break;
            }
            break;
        }
        return node;
    }
    bool LangContext::check_need_template_deduct_function(
        lexer& lex, ast::AstValueBase* target, PassProcessStackT& out_stack)
    {
        target = get_marked_origin_value_node(target);
        switch (target->node_type)
        {
        case ast::AstBase::AST_VALUE_FUNCTION:
        {
            ast::AstValueFunction* function = static_cast<ast::AstValueFunction*>(target);
            if (function->m_pending_param_type_mark_template.has_value())
                return true;

            break;
        }
        case ast::AstBase::AST_VALUE_VARIABLE:
        {
            ast::AstValueVariable* function_variable = static_cast<ast::AstValueVariable*>(target);
            ast::AstIdentifier* function_variable_identifier = function_variable->m_identifier;

            if (find_symbol_in_current_scope(lex, function_variable_identifier, std::nullopt))
            {
                lang_Symbol* function_symbol = function_variable_identifier->m_LANG_determined_symbol.value();
                if (function_symbol->m_symbol_kind == lang_Symbol::kind::VARIABLE
                    && function_symbol->m_is_template
                    && !function_symbol->m_template_value_instances->m_mutable
                    && function_symbol->m_template_value_instances->m_origin_value_ast->node_type == ast::AstBase::AST_VALUE_FUNCTION
                    && (!function_variable_identifier->m_template_arguments.has_value()
                        || function_symbol->m_template_value_instances->m_template_params.size()
                        != function_variable_identifier->m_template_arguments.value().size()))
                {
                    if (function_variable->m_identifier->m_template_arguments.has_value())
                        WO_CONTINUE_PROCESS_LIST(function_variable->m_identifier->m_template_arguments.value());

                    return true;
                }
            }
            break;
        }
        default:
            break;
        }
        return false;
    }
    bool LangContext::check_need_template_deduct_struct_type(
        lexer& lex, ast::AstTypeHolder* target, PassProcessStackT& out_stack)
    {
        if (target->m_formal == ast::AstTypeHolder::IDENTIFIER)
        {
            // Get it's symbol.
            ast::AstIdentifier* struct_type_identifier = target->m_typeform.m_identifier;
            auto finded_symbol = find_symbol_in_current_scope(lex, struct_type_identifier, std::nullopt);
            if (finded_symbol.has_value())
            {
                lang_Symbol* symbol = finded_symbol.value();
                if (symbol->m_symbol_kind == lang_Symbol::kind::TYPE
                    && symbol->m_is_template
                    && symbol->m_template_type_instances->m_origin_value_ast->node_type == ast::AstBase::AST_TYPE_HOLDER
                    && static_cast<ast::AstTypeHolder*>(symbol->m_template_type_instances->m_origin_value_ast)->m_formal == ast::AstTypeHolder::STRUCTURE
                    && (!struct_type_identifier->m_template_arguments.has_value()
                        || struct_type_identifier->m_template_arguments.value().size()
                        != symbol->m_template_type_instances->m_template_params.size()))
                {
                    if (struct_type_identifier->m_template_arguments.has_value())
                        WO_CONTINUE_PROCESS_LIST(struct_type_identifier->m_template_arguments.value());

                    return true;
                }
            }
        }
        return false;
    }

#endif
}