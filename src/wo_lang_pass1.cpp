#include "wo_lang.hpp"

namespace wo
{
#ifndef WO_DISABLE_COMPILER
    using namespace ast;

    bool LangContext::update_pattern_symbol_variable_type_pass1(
        lexer& lex,
        ast::AstPatternBase* pattern,
        const std::optional<AstValueBase*>& init_value,
        const std::optional<lang_TypeInstance*>& init_value_type)
    {
        switch (pattern->node_type)
        {
        case AstBase::AST_PATTERN_SINGLE:
        {
            AstPatternSingle* single_pattern = static_cast<AstPatternSingle*>(pattern);

            if (!single_pattern->m_template_parameters)
            {
                wo_assert(single_pattern->m_LANG_declared_symbol);

                auto* lang_symbol = single_pattern->m_LANG_declared_symbol.value();
                wo_assert(lang_symbol->m_symbol_kind == lang_Symbol::kind::VARIABLE);

                std::optional<wo::value> constant_value = std::nullopt;

                // NOTE: Donot decide constant value for mutable variable.
                if (init_value && !lang_symbol->m_value_instance->m_mutable)
                {
                    AstValueBase* value = init_value.value();
                    constant_value = value->m_evaled_const_value;
                }
                lang_symbol->m_value_instance->determined_value_instance(
                    init_value_type.value(), constant_value);
            }
            return true;
        }
        case AstBase::AST_PATTERN_TUPLE:
        {
            AstPatternTuple* tuple_pattern = static_cast<AstPatternTuple*>(pattern);

            auto* determined_type = init_value_type.value()->get_determined_type();

            if (determined_type->m_base_type == lang_TypeInstance::DeterminedType::TUPLE)
            {
                if (determined_type->m_external_type_description.m_tuple->m_element_types.size()
                    == tuple_pattern->m_fields.size())
                {
                    auto type_iter = determined_type->m_external_type_description.m_tuple->m_element_types.begin();
                    auto pattern_iter = tuple_pattern->m_fields.begin();
                    auto pattern_end = tuple_pattern->m_fields.end();

                    bool success = true;

                    for (; pattern_iter != pattern_end; ++pattern_iter, ++type_iter)
                        success = success && update_pattern_symbol_variable_type_pass1(
                            lex, *pattern_iter, std::nullopt, *type_iter);

                    return success;
                }
                else
                {
                    lex.lang_error(lexer::errorlevel::error, pattern,
                        WO_ERR_UNEXPECTED_MATCH_COUNT_FOR_TUPLE,
                        determined_type->m_external_type_description.m_tuple->m_element_types.size(),
                        tuple_pattern->m_fields.size());
                }
            }
            else
            {
                // TODO: Give typename.
                lex.lang_error(lexer::errorlevel::error, pattern,
                    WO_ERR_UNEXPECTED_MATCH_TYPE_FOR_TUPLE);
            }
            return false;
        }
        case AstBase::AST_PATTERN_UNION:
        {
            wo_error("TODO");
            break;
        }
        case AstBase::AST_PATTERN_TAKEPLACE:
            break;
        }
        return false;
    }

    void LangContext::init_pass1()
    {
        WO_LANG_REGISTER_PROCESSER(AstList, AstBase::AST_LIST, pass1);
        WO_LANG_REGISTER_PROCESSER(AstScope, AstBase::AST_SCOPE, pass1);
        WO_LANG_REGISTER_PROCESSER(AstNamespace, AstBase::AST_NAMESPACE, pass1);
        WO_LANG_REGISTER_PROCESSER(AstIdentifier, AstBase::AST_IDENTIFIER, pass1);
        WO_LANG_REGISTER_PROCESSER(AstTypeHolder, AstBase::AST_TYPE_HOLDER, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueLiteral, AstBase::AST_VALUE_LITERAL, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueVariable, AstBase::AST_VALUE_VARIABLE, pass1);
        WO_LANG_REGISTER_PROCESSER(AstVariableDefines, AstBase::AST_VARIABLE_DEFINES, pass1);
        WO_LANG_REGISTER_PROCESSER(AstVariableDefineItem, AstBase::AST_VARIABLE_DEFINE_ITEM, pass1);
        WO_LANG_REGISTER_PROCESSER(AstAliasTypeDeclare, AstBase::AST_ALIAS_TYPE_DECLARE, pass1);
        //WO_LANG_REGISTER_PROCESSER(AstUsingTypeDeclare, AstBase::AST_USING_TYPE_DECLARE, pass1);
    }

#define WO_PASS_PROCESSER(AST) WO_PASS_PROCESSER_IMPL(AST, pass1)

    WO_PASS_PROCESSER(AstList)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS_LIST(node->m_list);

            return HOLD;
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstScope)
    {
        if (state == UNPROCESSED)
        {
            if (node->m_LANG_determined_scope)
                entry_spcify_scope(node->m_LANG_determined_scope.value());
            else
            {
                begin_new_scope();
                node->m_LANG_determined_scope = get_current_scope();
            }

            WO_CONTINUE_PROCESS(node->m_body);

            return HOLD;
        }
        end_last_scope();
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstNamespace)
    {
        if (state == UNPROCESSED)
        {
            entry_spcify_namespace(node->m_LANG_determined_namespace.value());
          
            WO_CONTINUE_PROCESS(node->m_body);

            return HOLD;
        }
        end_last_scope();
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstIdentifier)
    {
        if (state == UNPROCESSED)
        {
            switch (node->m_formal)
            {
            case AstIdentifier::FROM_GLOBAL:
            case AstIdentifier::FROM_CURRENT:
                break;
            case AstIdentifier::FROM_TYPE:
                wo_assert(node->m_from_type);
                WO_CONTINUE_PROCESS(node->m_from_type.value());
                break;
            }
            if (node->m_template_arguments)
                WO_CONTINUE_PROCESS_LIST(node->m_template_arguments.value());
            return HOLD;
        }
        else if (state == HOLD)
        {
            if (!find_symbol_in_current_scope(lex, node))
            {
                lex.lang_error(lexer::errorlevel::error, node,
                    WO_ERR_UNKNOWN_IDENTIFIER,
                    node->m_name->c_str());

                return FAILED;
            }
            else
            {
                lang_Symbol* symbol = node->m_LANG_determined_symbol.value();

                bool has_template_arguments = node->m_template_arguments.has_value();
                bool accept_template_arguments = symbol->m_is_template;

                if ((has_template_arguments != accept_template_arguments)
                    && !symbol->m_is_builtin)
                {
                    if (accept_template_arguments)
                        lex.lang_error(lexer::errorlevel::error, node,
                            WO_ERR_EXPECTED_TEMPLATE_ARGUMENT);
                    else
                        lex.lang_error(lexer::errorlevel::error, node,
                            WO_ERR_UNEXPECTED_TEMPLATE_ARGUMENT);

                    return FAILED;
                }
            }
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstTypeHolder)
    {
        if (state == UNPROCESSED)
        {
            switch (node->m_formal)
            {
            case AstTypeHolder::IDENTIFIER:
                WO_CONTINUE_PROCESS(node->m_identifier);
                break;
            case AstTypeHolder::TYPEOF:
                WO_CONTINUE_PROCESS(node->m_typefrom);
                break;
            case AstTypeHolder::FUNCTION:
                WO_CONTINUE_PROCESS_LIST(node->m_function.m_parameters);
                WO_CONTINUE_PROCESS(node->m_function.m_return_type);
                break;
            case AstTypeHolder::STRUCTURE:
                WO_CONTINUE_PROCESS_LIST(node->m_structure.m_fields);
                break;
            case AstTypeHolder::TUPLE:
                WO_CONTINUE_PROCESS_LIST(node->m_tuple.m_fields);
                break;
            case AstTypeHolder::UNION:
                for (auto& field : node->m_union.m_fields)
                    break;
            default:
                wo_error("unknown type holder formal");
                break;
            }
            return HOLD;
        }
        else if (state == HOLD)
        {
            switch (node->m_formal)
            {
            case AstTypeHolder::IDENTIFIER:
            {
                lang_Symbol* type_symbol = node->m_identifier->m_LANG_determined_symbol.value();

                if (type_symbol->m_symbol_kind == lang_Symbol::VARIABLE)
                {
                    lex.lang_error(lexer::errorlevel::error, node,
                        WO_ERR_UNEXPECTED_VAR_SYMBOL,
                        node->m_identifier->m_name->c_str());

                    return FAILED;
                }
                else
                {
                    if (!type_symbol->m_is_builtin)
                    {
                        if (type_symbol->m_is_template)
                            // TODO: template type.
                            wo_error("template type not implemented");
                        else
                        {
                            if (type_symbol->m_symbol_kind == lang_Symbol::ALIAS)
                            {
                                if (type_symbol->m_alias_instance->m_determined_type)
                                    node->m_LANG_determined_type =
                                    type_symbol->m_alias_instance->m_determined_type.value();
                                else
                                {
                                    if (m_finishing_pending_nodes)
                                    {
                                        lex.lang_error(lexer::errorlevel::error, node,
                                            WO_ERR_TYPE_DETERMINED_FAILED);
                                        return FAILED;
                                    }
                                    else
                                        return PENDING;
                                }
                            }
                            else
                            {
                                wo_assert(type_symbol->m_symbol_kind == lang_Symbol::TYPE);
                                if (type_symbol->m_type_instance->m_determined_type)
                                    node->m_LANG_determined_type = type_symbol->m_type_instance;
                                else
                                {
                                    if (m_finishing_pending_nodes)
                                    {
                                        lex.lang_error(lexer::errorlevel::error, node,
                                            WO_ERR_TYPE_DETERMINED_FAILED);
                                        return FAILED;
                                    }
                                    else
                                        return PENDING;
                                }
                            }
                        }

                        wo_assert(node->m_LANG_determined_type);
                        switch (node->m_mutable_mark)
                        {
                        case AstTypeHolder::MARK_AS_IMMUTABLE:
                            node->m_LANG_determined_type = immutable_type(node->m_LANG_determined_type.value());
                            break;
                        case AstTypeHolder::MARK_AS_MUTABLE:
                            node->m_LANG_determined_type = mutable_type(node->m_LANG_determined_type.value());
                            break;
                        default:
                            // Do nothing.
                            break;
                        }

                        break;
                    }
                    else
                    {
                        /* FALL-THROUGH */
                    }
                }
                /* FALL-THROUGH */
            }
            [[fallthrough]];
            case AstTypeHolder::FUNCTION:
            case AstTypeHolder::TUPLE:
            case AstTypeHolder::UNION:
            case AstTypeHolder::STRUCTURE:
                node->m_LANG_determined_type = m_origin_types.create_or_find_origin_type(lex, node);
                break;
            case AstTypeHolder::TYPEOF:
                wo_assert(node->m_typefrom->m_LANG_determined_type);
                node->m_LANG_determined_type = node->m_typefrom->m_LANG_determined_type.value();
                break;
            default:
                wo_error("unknown type holder formal");
                break;
            }

            switch (node->m_mutable_mark)
            {
            case AstTypeHolder::MARK_AS_IMMUTABLE:
                node->m_LANG_determined_type = immutable_type(node->m_LANG_determined_type.value());
                break;
            case AstTypeHolder::MARK_AS_MUTABLE:
                node->m_LANG_determined_type = mutable_type(node->m_LANG_determined_type.value());
                break;
            default:
                // Do nothing.
                break;
            }
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueLiteral)
    {
        wo_assert(state == UNPROCESSED);
        wo_assert(node->m_evaled_const_value);

        switch (node->m_evaled_const_value.value().type)
        {
        case value::valuetype::invalid:
            node->m_LANG_determined_type = m_origin_types.m_nil.m_type_instance;
            break;
        case value::valuetype::integer_type:
            node->m_LANG_determined_type = m_origin_types.m_int.m_type_instance;
            break;
        case value::valuetype::real_type:
            node->m_LANG_determined_type = m_origin_types.m_real.m_type_instance;
            break;
        case value::valuetype::handle_type:
            node->m_LANG_determined_type = m_origin_types.m_handle.m_type_instance;
            break;
        case value::valuetype::bool_type:
            node->m_LANG_determined_type = m_origin_types.m_bool.m_type_instance;
            break;
        case value::valuetype::string_type:
            node->m_LANG_determined_type = m_origin_types.m_string.m_type_instance;
            break;
        default:
            wo_error("unknown literal type");
            break;
        }

        return OKAY;
    }
    WO_PASS_PROCESSER(AstValueVariable)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_identifier);
            return HOLD;
        }
        else if (state == HOLD)
        {
            lang_ValueInstance* value_instance = nullptr;

            if (!node->m_LANG_template_evalating_state)
            {
                wo_assert(node->m_identifier->m_LANG_determined_symbol);
                lang_Symbol* var_symbol = node->m_identifier->m_LANG_determined_symbol.value();

                if (var_symbol->m_symbol_kind != lang_Symbol::VARIABLE)
                {
                    lex.lang_error(lexer::errorlevel::error, node,
                        WO_ERR_UNEXPECTED_TYPE_SYMBOL,
                        node->m_identifier->m_name->c_str());

                    return FAILED;
                }
                else
                {
                    if (var_symbol->m_is_template)
                    {
                        // TEMPLATE!!!
                        // NOTE: In function call, template arguments will be 
                        //  derived and filled completely.
                        auto& this_variable_template_arguments =
                            node->m_identifier->m_template_arguments.value();
                        auto& template_variable_prefab =
                            var_symbol->m_template_value_instances;

                        if (this_variable_template_arguments.size()
                            != template_variable_prefab->m_template_params.size())
                        {
                            lex.lang_error(lexer::errorlevel::error, node,
                                WO_ERR_UNEXPECTED_TEMPLATE_COUNT,
                                template_variable_prefab->m_template_params.size());

                            return FAILED;
                        }
                        // Template matched.
                        // Create a new value instance.
                        lang_Symbol::TemplateArgumentListT template_args;
                        for (auto* typeholder : this_variable_template_arguments)
                        {
                            wo_assert(typeholder->m_LANG_determined_type);
                            template_args.push_back(typeholder->m_LANG_determined_type.value());
                        }
                        auto* template_eval_state_instance =
                            template_variable_prefab->find_or_create_template_instance(template_args);

                        switch (template_eval_state_instance->m_state)
                        {
                        case lang_TemplateAstEvalStateValue::EVALUATED:
                            value_instance = template_eval_state_instance->m_value_instance.value().get();
                            break;
                        case lang_TemplateAstEvalStateValue::FAILED:
                            // TODO: Collect error message in template eval.
                            lex.lang_error(lexer::errorlevel::error, node,
                                WO_ERR_VALUE_TYPE_DETERMINED_FAILED);
                            return FAILED;
                        case lang_TemplateAstEvalStateValue::EVALUATING:
                            // NOTE: Donot modify eval state here.
                            //  Some case like `is pending` may meet this error but it's not a real error.
                            lex.lang_error(lexer::errorlevel::error, node,
                                WO_ERR_RECURSIVE_TEMPLATE_INSTANCE);
                            return FAILED;
                        case lang_TemplateAstEvalStateValue::UNPROCESSED:
                            template_eval_state_instance->m_state = lang_TemplateAstEvalStateValue::EVALUATING;
                            node->m_LANG_template_evalating_state = template_eval_state_instance;
                            
                            entry_spcify_scope(var_symbol->m_belongs_to_scope); // Entry the scope where template variable defined.
                            begin_new_scope(); // Begin new scope for defining template type alias.
                            fast_create_template_type_alias_in_current_scope(
                                lex, var_symbol, template_variable_prefab->m_template_params, template_args);

                            WO_CONTINUE_PROCESS(template_eval_state_instance->m_ast);
                            return HOLD;
                        }

                    }
                    else
                    {
                        value_instance = var_symbol->m_value_instance;
                    }
                }
            }
            else
            {
                auto* state = node->m_LANG_template_evalating_state.value();
                wo_assert(state->m_state == lang_TemplateAstEvalStateValue::EVALUATING);

                // Continue template evaluating.
                end_last_scope(); // Leave temporary scope for template type alias.
                end_last_scope(); // Leave scope where template variable defined.

                AstValueBase* ast_value = static_cast<AstValueBase*>(state->m_ast);

                // Make new value instance.
                auto* symbol = state->m_symbol;
                auto new_template_variable_instance = std::make_unique<lang_ValueInstance>(
                    symbol->m_template_value_instances->m_mutable, state->m_symbol);

                std::optional<wo::value> constant_value = std::nullopt;
                if (!new_template_variable_instance->m_mutable && ast_value->m_evaled_const_value)
                {
                    constant_value = ast_value->m_evaled_const_value;
                }
                new_template_variable_instance->determined_value_instance(
                    ast_value->m_LANG_determined_type.value(), constant_value);

                value_instance = new_template_variable_instance.get();
                
                state->m_value_instance = std::move(new_template_variable_instance);
                state->m_state = lang_TemplateAstEvalStateValue::EVALUATED;
            }

            wo_assert(value_instance != nullptr);

            // Type has been determined.
            if (value_instance->m_determined_type)
            {
                node->m_LANG_determined_type = value_instance->m_determined_type.value();
                node->m_LANG_variable_instance = value_instance;
            }
            else
            {
                if (m_finishing_pending_nodes)
                {
                    // Type determined failed in AstVariableDefines, treat as failed.
                    lex.lang_error(lexer::errorlevel::error, node,
                        WO_ERR_VALUE_TYPE_DETERMINED_FAILED);
                    return FAILED;
                }
                else
                    return PENDING;
            }

            if (value_instance->m_determined_constant)
                // Constant has been determined.
                node->decide_final_constant_value(
                    value_instance->m_determined_constant.value());
        }
        else
        {
            if (node->m_LANG_template_evalating_state)
            {
                auto* state = node->m_LANG_template_evalating_state.value();
                state->m_state = lang_TemplateAstEvalStateValue::FAILED;

                // Child failed, we need pop the scope anyway.
                end_last_scope(); // Leave temporary scope for template type alias.
                end_last_scope(); // Leave scope where template variable defined.
            }
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstVariableDefines)
    {
        if (state == UNPROCESSED)
        {
            if (node->m_attribute)
                WO_CONTINUE_PROCESS(node->m_attribute.value());

            WO_CONTINUE_PROCESS_LIST(node->m_definitions);
            return HOLD;
        }
        else if (state == HOLD)
        {
            bool success = true;
            for (auto& defines : node->m_definitions)
                success = success && declare_pattern_symbol_pass0_1(
                    lex,
                    node->m_attribute,
                    defines->m_pattern,
                    defines->m_init_value);

            // UPDATE PATTERN SYMBOL TYPE;
            if (success)
            {
                for (auto& defines : node->m_definitions)
                    success = success && update_pattern_symbol_variable_type_pass1(
                        lex,
                        defines->m_pattern,
                        defines->m_init_value,
                        defines->m_init_value->m_LANG_determined_type);
            }

            if (!success)
                return FAILED;

            // TODO update_pattern_symbol_pass1;
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstVariableDefineItem)
    {
        if (state == UNPROCESSED)
        {
            if (node->m_pattern->node_type == AstBase::AST_PATTERN_SINGLE)
            {
                AstPatternSingle* single = static_cast<AstPatternSingle*>(node->m_pattern);
                if (single->m_template_parameters)
                    // Template variable, skip process init value.
                    return OKAY;
            }

            WO_CONTINUE_PROCESS(node->m_init_value);
            return HOLD;
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstAliasTypeDeclare)
    {
        if (state == UNPROCESSED)
        {
            if (!node->m_LANG_declared_symbol)
            {
                if (node->m_template_parameters)
                    node->m_LANG_declared_symbol = define_symbol_in_current_scope(
                        node->m_typename,
                        node->m_attribute,
                        node->source_location.source_file,
                        get_current_scope(),
                        node->m_type,
                        node->m_template_parameters.value(),
                        true);
                else
                    node->m_LANG_declared_symbol = define_symbol_in_current_scope(
                        node->m_typename,
                        node->m_attribute,
                        node->source_location.source_file,
                        get_current_scope(),
                        lang_Symbol::kind::ALIAS,
                        false);


                if (!node->m_LANG_declared_symbol)
                {
                    lex.lang_error(lexer::errorlevel::error, node, 
                        WO_ERR_REDEFINED,
                        node->m_typename->c_str());
                    return FAILED;
                }

            }
            if (!node->m_template_parameters)
            {
                // Update type instance.
                WO_CONTINUE_PROCESS(node->m_type);
                return HOLD;
            }
            return OKAY;
        }
        else if (state == HOLD)
        {
            wo_assert(!node->m_template_parameters);
            lang_Symbol* symbol = node->m_LANG_declared_symbol.value();

            symbol->m_alias_instance->m_determined_type = 
                node->m_type->m_LANG_determined_type.value();
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }

#undef WO_PASS_PROCESSER

#endif
}