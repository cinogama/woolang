#include "wo_lang.hpp"

namespace wo
{
#ifndef WO_DISABLE_COMPILER

    void LangContext::_collect_failed_template_instance(
        lexer& lex, ast::AstBase* node, lang_TemplateAstEvalStateBase* inst)
    {
        std::list<wo_pstring_t>* template_params = nullptr;
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

        wo_assert(inst->m_template_arguments.size() == template_params->size());
        std::wstring failed_template_arg_list;

        auto it_template_arg = inst->m_template_arguments.begin();
        auto it_template_param = template_params->begin();
        auto it_template_param_end = template_params->end();

        for (; it_template_param != it_template_param_end;
            ++it_template_param, ++it_template_arg)
        {
            if (!failed_template_arg_list.empty())
                failed_template_arg_list += L", ";

            failed_template_arg_list += **it_template_param;
            failed_template_arg_list += L" = ";
            failed_template_arg_list += get_type_name_w(*it_template_arg);
        }

        lex.lang_error(lexer::errorlevel::error, node,
            WO_ERR_FAILED_REIFICATION_CAUSED_BY,
            failed_template_arg_list.c_str(),
            get_symbol_name_w(inst->m_symbol));

        for (const auto& errmsg : inst->m_failed_error_for_this_instance.value())
            // TODO: Describe the error support.
            lex.error_impl(errmsg);
    }

    std::optional<lang_TemplateAstEvalStateBase*> LangContext::begin_eval_template_ast(
        lexer& lex,
        ast::AstBase* node,
        lang_Symbol* templating_symbol,
        const lang_Symbol::TemplateArgumentListT& template_arguments,
        PassProcessStackT& out_stack,
        bool* out_continue_process_flag)
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

        result->m_template_arguments = template_arguments;
        *out_continue_process_flag = false;

        switch (result->m_state)
        {
        case lang_TemplateAstEvalStateValue::EVALUATED:
            break;
        case lang_TemplateAstEvalStateValue::FAILED:
        {
            _collect_failed_template_instance(lex, node, result);
            return std::nullopt;
        }
        case lang_TemplateAstEvalStateValue::EVALUATING:
        {
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
            lex.lang_error(lexer::errorlevel::error, node,
                WO_ERR_RECURSIVE_TEMPLATE_INSTANCE);
            return std::nullopt;
        }
        case lang_TemplateAstEvalStateValue::UNPROCESSED:
            result->m_state = lang_TemplateAstEvalStateValue::EVALUATING;
            *out_continue_process_flag = true;

            // Entry the scope where template variable defined.

            bool has_enter_specify_scope = false;
            if (templating_symbol->m_symbol_kind == lang_Symbol::kind::TYPE)
            {
                if (templating_symbol->m_belongs_to_scope->is_namespace_scope())
                {
                    lang_Namespace* type_namespace = templating_symbol->m_belongs_to_scope->m_belongs_to_namespace;
                    auto fnd = type_namespace->m_sub_namespaces.find(templating_symbol->m_name);
                    if (fnd != type_namespace->m_sub_namespaces.end())
                    {
                        has_enter_specify_scope = true;
                        entry_spcify_scope(fnd->second->m_this_scope.get());
                    }
                }
            }

            if (!has_enter_specify_scope)
                entry_spcify_scope(templating_symbol->m_belongs_to_scope);

            begin_new_scope(); // Begin new scope for defining template type alias.

            auto* current_scope = get_current_scope();

            fast_create_template_type_alias_in_current_scope(
                templating_symbol->m_defined_source, *template_params, template_arguments);

            // ATTENTION: LIMIT TEMPLATE INSTANCE SYMBOL VISIBILITY!
            current_scope->m_visibility_from_edge_for_template_check =
                templating_symbol->m_symbol_edge;

            lex.begin_trying_block();
            WO_CONTINUE_PROCESS(result->m_ast);
            break;
        }
        return result;
    }

    void LangContext::finish_eval_template_ast(
        lexer& lex, lang_TemplateAstEvalStateBase* template_eval_instance)
    {
        wo_assert(lex.get_cur_error_frame().empty());
        lex.end_trying_block();

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

            auto* new_template_variable_instance =
                template_eval_instance_value->m_value_instance.get();

            new_template_variable_instance->m_determined_type =
                ast_value->m_LANG_determined_type.value();

            if (!new_template_variable_instance->m_mutable)
                new_template_variable_instance->try_determine_const_value(ast_value);

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

            auto* new_template_alias_instance =
                template_eval_instance_alias->m_alias_instance.get();

            new_template_alias_instance->m_determined_type = ast_type->m_LANG_determined_type;
            wo_assert(new_template_alias_instance->m_determined_type.has_value());

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

            auto* new_template_type_instance =
                template_eval_instance_type->m_type_instance.get();

            new_template_type_instance->determine_base_type_by_another_type(
                ast_type->m_LANG_determined_type.value());

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
        lexer& lex, ast::AstBase* node, lang_TemplateAstEvalStateBase* template_eval_instance)
    {
        wo_assert(!lex.get_cur_error_frame().empty());
        template_eval_instance->m_failed_error_for_this_instance.emplace(
            std::move(lex.get_cur_error_frame()));

        lex.end_trying_block();

        // Child failed, we need pop the scope anyway.
        end_last_scope(); // Leave temporary scope for template type alias.
        end_last_scope(); // Leave scope where template variable defined.

        template_eval_instance->m_state = lang_TemplateAstEvalStateBase::FAILED;

        _collect_failed_template_instance(lex, node, template_eval_instance);
    }

    bool LangContext::template_type_deduction_extraction_with_complete_type(
        lexer& lex,
        ast::AstTypeHolder* accept_type_formal,
        lang_TypeInstance* applying_type_instance,
        const std::list<wo_pstring_t>& pending_template_params,
        std::unordered_map<wo_pstring_t, lang_TypeInstance*>* out_determined_template_arg_pair)
    {
        switch (accept_type_formal->m_formal)
        {
        case ast::AstTypeHolder::IDENTIFIER:
        {
            auto* identifier = accept_type_formal->m_typeform.m_identifier;
            bool try_eval_symbol_of_identifier = true;

            switch (identifier->m_formal)
            {
            case ast::AstIdentifier::FROM_TYPE:
                // Not support.
                return true;
            case ast::AstIdentifier::FROM_CURRENT:
                // Current identifier might be template need to be pick.
                if (identifier->m_scope.empty())
                {
                    // TODO: Support HKT?
                    // PERFECT! We got the type we need.

                    auto fnd = std::find(
                        pending_template_params.begin(),
                        pending_template_params.end(),
                        identifier->m_name);

                    if (fnd != pending_template_params.end())
                    {
                        try_eval_symbol_of_identifier = false;

                        // Got it!
                        wo_pstring_t template_param_name = *fnd;

                        switch (accept_type_formal->m_mutable_mark)
                        {
                        case ast::AstTypeHolder::mutable_mark::MARK_AS_MUTABLE:
                            if (applying_type_instance->is_mutable())
                            {
                                // mut T <= mut Tinstance: T = Tinstance
                                out_determined_template_arg_pair->insert(
                                    std::make_pair(template_param_name, immutable_type(applying_type_instance)));
                            }
                            // Bad, continue;
                            break;
                        case ast::AstTypeHolder::mutable_mark::MARK_AS_IMMUTABLE:
                            if (applying_type_instance->is_mutable())
                            {
                                // immut T <X= mut Tinstance: Bad
                                lex.lang_error(lexer::errorlevel::error, accept_type_formal,
                                    WO_ERR_UNACCEPTABLE_MUTABLE,
                                    get_type_name_w(applying_type_instance));
                                
                                return false;
                            }
                            /* Fall through */
                            [[fallthrough]];
                        case ast::AstTypeHolder::mutable_mark::NONE:
                            out_determined_template_arg_pair->insert(
                                std::make_pair(template_param_name, applying_type_instance));
                            break;
                        default:
                            wo_error("Unexpected mutable mark");
                        }
                        goto _label_fake_hkt_trying_template;
                    }
                }
                /* FALL THROUGH */
                [[fallthrough]];
            case ast::AstIdentifier::FROM_GLOBAL:
                do
                {
                    // Try determin symbol:
                    bool ambiguous = false;
                    if (!find_symbol_in_current_scope(lex, identifier, &ambiguous))
                    {
                        lex.lang_error(lexer::errorlevel::error, accept_type_formal,
                            WO_ERR_UNKNOWN_IDENTIFIER,
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
                    case lang_Symbol::kind::VARIABLE:
                    {
                        // WTF..
                        lex.lang_error(
                            lexer::errorlevel::error, 
                            accept_type_formal,
                            WO_ERR_UNEXPECTED_VAR_SYMBOL,
                            get_symbol_name_w(determined_type_symbol));

                        if (determined_type_symbol->m_symbol_declare_ast.has_value())
                        {
                            lex.lang_error(
                                lexer::errorlevel::infom,
                                determined_type_symbol->m_symbol_declare_ast.value(),
                                WO_INFO_SYMBOL_NAMED_DEFINED_HERE,
                                get_symbol_name_w(determined_type_symbol));
                        }
                        return false;
                    }
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
                        all_ok = template_type_deduction_extraction_with_complete_type(
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
                all_ok = template_type_deduction_extraction_with_complete_type(
                    lex,
                    *it_fn_param,
                    *it_instance_param,
                    pending_template_params,
                    out_determined_template_arg_pair) && all_ok;
            }

            all_ok = template_type_deduction_extraction_with_complete_type(
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
                all_ok = template_type_deduction_extraction_with_complete_type(
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
                all_ok = template_type_deduction_extraction_with_complete_type(
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

        return true;
    }

    bool LangContext::check_type_may_dependence_template_parameters(
        ast::AstTypeHolder* accept_type_formal,
        const std::list<wo_pstring_t>& pending_template_params)
    {
        switch (accept_type_formal->m_formal)
        {
        case ast::AstTypeHolder::IDENTIFIER:
        {
            auto* identifier = accept_type_formal->m_typeform.m_identifier;
            switch (identifier->m_formal)
            {
            case ast::AstIdentifier::FROM_CURRENT:
                // Current identifier might be template need to be pick.
                if (!identifier->m_template_arguments.has_value()
                    && identifier->m_scope.empty())
                {
                    if (std::find(
                        pending_template_params.begin(),
                        pending_template_params.end(),
                        identifier->m_name) != pending_template_params.end())
                        // Found!
                        return true;
                }
                /* FALL THROUGH */
                [[fallthrough]];
            case ast::AstIdentifier::FROM_TYPE:
                if (identifier->m_from_type.has_value())
                {
                    ast::AstTypeHolder** from_type =
                        std::get_if<ast::AstTypeHolder*>(&identifier->m_from_type.value());

                    if (from_type != nullptr
                        && check_type_may_dependence_template_parameters(
                            *from_type, pending_template_params))
                        return true;
                }
                /* FALL THROUGH */
                [[fallthrough]];
            case ast::AstIdentifier::FROM_GLOBAL:
                if (identifier->m_template_arguments.has_value())
                {
                    auto& template_arguments = identifier->m_template_arguments.value();
                    for (auto& template_argument : template_arguments)
                    {
                        if (check_type_may_dependence_template_parameters(
                            template_argument,
                            pending_template_params))
                            return true;
                    }
                }
                return false;
            }
        }
        case ast::AstTypeHolder::TYPEOF:
            // Cannot check, treat it as depdencs
            return true;
        case ast::AstTypeHolder::FUNCTION:
        {
            auto& function = accept_type_formal->m_typeform.m_function;
            if (check_type_may_dependence_template_parameters(
                function.m_return_type, pending_template_params))
                return true;

            for (auto& param : function.m_parameters)
            {
                if (check_type_may_dependence_template_parameters(
                    param, pending_template_params))
                    return true;
            }

            return false;
        }
        case ast::AstTypeHolder::STRUCTURE:
        {
            auto& structure = accept_type_formal->m_typeform.m_structure;
            for (auto& field : structure.m_fields)
            {
                if (check_type_may_dependence_template_parameters(
                    field->m_type, pending_template_params))
                    return true;
            }

            return false;
        }
        case ast::AstTypeHolder::TUPLE:
        {
            auto& tuple = accept_type_formal->m_typeform.m_tuple;
            for (auto& field : tuple.m_fields)
            {
                if (check_type_may_dependence_template_parameters(
                    field, pending_template_params))
                    return true;
            }

            return false;
        }
        case ast::AstTypeHolder::UNION:
            // That's will never happend.
            /* FALL THROUGH! */
            [[fallthrough]];
        default:
            wo_error("unknown typeholder formal.");
        }

        return false;
    }

    void LangContext::template_function_deduction_extraction_with_complete_type(
        lexer& lex,
        ast::AstValueFunction* function_define,
        const std::list<std::optional<lang_TypeInstance*>>& argument_types,
        const std::optional<lang_TypeInstance*>& return_type,
        const std::list<wo_pstring_t>& pending_template_params,
        std::unordered_map<wo_pstring_t, lang_TypeInstance*>* out_determined_template_arg_pair
    )
    {
        if (function_define->m_marked_return_type.has_value()
            && return_type.has_value())
        {
            template_type_deduction_extraction_with_complete_type(
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

            template_type_deduction_extraction_with_complete_type(
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