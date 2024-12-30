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

            auto determined_type_may_nullopt = init_value_type.value()->get_determined_type();
            if (!determined_type_may_nullopt.has_value())
            {
                lex.lang_error(lexer::errorlevel::error, pattern,
                    WO_ERR_TYPE_DETERMINED_FAILED);

                return false;
            }
            auto* determined_type = determined_type_may_nullopt.value();

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
            // Do nothing.
            return true;
        }
        return false;
    }

    void LangContext::init_pass1()
    {
        WO_LANG_REGISTER_PROCESSER(AstList, AstBase::AST_LIST, pass1);
        WO_LANG_REGISTER_PROCESSER(AstIdentifier, AstBase::AST_IDENTIFIER, pass1);
        WO_LANG_REGISTER_PROCESSER(AstStructFieldDefine, AstBase::AST_STRUCT_FIELD_DEFINE, pass1);
        WO_LANG_REGISTER_PROCESSER(AstTypeHolder, AstBase::AST_TYPE_HOLDER, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueMarkAsMutable, AstBase::AST_VALUE_MARK_AS_MUTABLE, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueMarkAsImmutable, AstBase::AST_VALUE_MARK_AS_IMMUTABLE, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueLiteral, AstBase::AST_VALUE_LITERAL, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueTypeid, AstBase::AST_VALUE_TYPEID, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueTypeCast, AstBase::AST_VALUE_TYPE_CAST, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueTypeCheckIs, AstBase::AST_VALUE_TYPE_CHECK_IS, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueTypeCheckAs, AstBase::AST_VALUE_TYPE_CHECK_AS, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueVariable, AstBase::AST_VALUE_VARIABLE, pass1);
        WO_LANG_REGISTER_PROCESSER(AstWhereConstraints, AstBase::AST_WHERE_CONSTRAINTS, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueFunctionCall_FakeAstArgumentDeductionContextA,
            AstBase::AST_VALUE_FUNCTION_CALL_FAKE_AST_ARGUMENT_DEDUCTION_CONTEXT_A, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueFunctionCall_FakeAstArgumentDeductionContextB,
            AstBase::AST_VALUE_FUNCTION_CALL_FAKE_AST_ARGUMENT_DEDUCTION_CONTEXT_B, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueFunctionCall, AstBase::AST_VALUE_FUNCTION_CALL, pass1);
        // WO_LANG_REGISTER_PROCESSER(AstValueBinaryOperator, AstBase::AST_VALUE_BINARY_OPERATOR, pass1);
        // WO_LANG_REGISTER_PROCESSER(AstValueUnaryOperator, AstBase::AST_VALUE_UNARY_OPERATOR, pass1);
        // WO_LANG_REGISTER_PROCESSER(AstValueTribleOperator, AstBase::AST_VALUE_TRIPLE_OPERATOR, pass1);
        WO_LANG_REGISTER_PROCESSER(AstFakeValueUnpack, AstBase::AST_FAKE_VALUE_UNPACK, pass1);
        // WO_LANG_REGISTER_PROCESSER(AstValueVariadicArgumentsPack, AstBase::AST_VALUE_VARIADIC_ARGUMENTS_PACK, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueIndex, AstBase::AST_VALUE_INDEX, pass1);
        WO_LANG_REGISTER_PROCESSER(AstPatternVariable, AstBase::AST_PATTERN_VARIABLE, pass1);
        WO_LANG_REGISTER_PROCESSER(AstPatternIndex, AstBase::AST_PATTERN_INDEX, pass1);
        WO_LANG_REGISTER_PROCESSER(AstVariableDefineItem, AstBase::AST_VARIABLE_DEFINE_ITEM, pass1);
        WO_LANG_REGISTER_PROCESSER(AstVariableDefines, AstBase::AST_VARIABLE_DEFINES, pass1);
        WO_LANG_REGISTER_PROCESSER(AstFunctionParameterDeclare, AstBase::AST_FUNCTION_PARAMETER_DECLARE, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueFunction, AstBase::AST_VALUE_FUNCTION, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueArrayOrVec, AstBase::AST_VALUE_ARRAY_OR_VEC, pass1);
        WO_LANG_REGISTER_PROCESSER(AstKeyValuePair, AstBase::AST_KEY_VALUE_PAIR, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueDictOrMap, AstBase::AST_VALUE_DICT_OR_MAP, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueTuple, AstBase::AST_VALUE_TUPLE, pass1);
        WO_LANG_REGISTER_PROCESSER(AstStructFieldValuePair, AstBase::AST_STRUCT_FIELD_VALUE_PAIR, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueStruct, AstBase::AST_VALUE_STRUCT, pass1);
        // WO_LANG_REGISTER_PROCESSER(AstValueAssign, AstBase::AST_VALUE_ASSIGN, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValuePackedArgs, AstBase::AST_VALUE_PACKED_ARGS, pass1);
        WO_LANG_REGISTER_PROCESSER(AstNamespace, AstBase::AST_NAMESPACE, pass1);
        WO_LANG_REGISTER_PROCESSER(AstScope, AstBase::AST_SCOPE, pass1);
        // WO_LANG_REGISTER_PROCESSER(AstMatchCase, AstBase::AST_MATCH_CASE, pass1);
        // WO_LANG_REGISTER_PROCESSER(AstMatch, AstBase::AST_MATCH, pass1);
        // WO_LANG_REGISTER_PROCESSER(AstIf, AstBase::AST_IF, pass1);
        // WO_LANG_REGISTER_PROCESSER(AstWhile, AstBase::AST_WHILE, pass1);
        // WO_LANG_REGISTER_PROCESSER(AstFor, AstBase::AST_FOR, pass1);
        // WO_LANG_REGISTER_PROCESSER(AstForeach, AstBase::AST_FOREACH, pass1);
        // WO_LANG_REGISTER_PROCESSER(AstBreak, AstBase::AST_BREAK, pass1);
        // WO_LANG_REGISTER_PROCESSER(AstContinue, AstBase::AST_CONTINUE, pass1);
        WO_LANG_REGISTER_PROCESSER(AstReturn, AstBase::AST_RETURN, pass1);
        WO_LANG_REGISTER_PROCESSER(AstLabeled, AstBase::AST_LABELED, pass1);
        WO_LANG_REGISTER_PROCESSER(AstUsingTypeDeclare, AstBase::AST_USING_TYPE_DECLARE, pass1);
        WO_LANG_REGISTER_PROCESSER(AstAliasTypeDeclare, AstBase::AST_ALIAS_TYPE_DECLARE, pass1);
        WO_LANG_REGISTER_PROCESSER(AstEnumDeclare, AstBase::AST_ENUM_DECLARE, pass1);
        WO_LANG_REGISTER_PROCESSER(AstUnionDeclare, AstBase::AST_UNION_DECLARE, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueMakeUnion, AstBase::AST_VALUE_MAKE_UNION, pass1);
        // WO_LANG_REGISTER_PROCESSER(AstUsingNamespace, AstBase::AST_USING_NAMESPACE, pass1);
        // WO_LANG_REGISTER_PROCESSER(AstExternInformation, AstBase::AST_EXTERN_INFORMATION, pass1);  
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

                bool has_template_arguments = node->m_template_arguments.has_value()
                    || node->m_LANG_determined_and_appended_template_arguments.has_value();

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
    WO_PASS_PROCESSER(AstStructFieldDefine)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_type);
            return HOLD;
        }
        return WO_EXCEPT_ERROR(state, OKAY);

    }
    WO_PASS_PROCESSER(AstTypeHolder)
    {
        if (node->m_LANG_trying_advancing_type_judgement)
            end_last_scope(); // Leave temporary advance the processing of declaration nodes.

        if (state == UNPROCESSED)
        {
            switch (node->m_formal)
            {
            case AstTypeHolder::IDENTIFIER:
                WO_CONTINUE_PROCESS(node->m_typeform.m_identifier);
                break;
            case AstTypeHolder::TYPEOF:
                WO_CONTINUE_PROCESS(node->m_typeform.m_typefrom);
                break;
            case AstTypeHolder::FUNCTION:
                WO_CONTINUE_PROCESS_LIST(node->m_typeform.m_function.m_parameters);
                WO_CONTINUE_PROCESS(node->m_typeform.m_function.m_return_type);
                break;
            case AstTypeHolder::STRUCTURE:
                WO_CONTINUE_PROCESS_LIST(node->m_typeform.m_structure.m_fields);
                break;
            case AstTypeHolder::TUPLE:
                WO_CONTINUE_PROCESS_LIST(node->m_typeform.m_tuple.m_fields);
                break;
            case AstTypeHolder::UNION:
                for (auto& field : node->m_typeform.m_union.m_fields)
                {
                    if (field.m_item)
                        WO_CONTINUE_PROCESS(field.m_item.value());
                }
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
                lang_Symbol* type_symbol = node->m_typeform.m_identifier->m_LANG_determined_symbol.value();

                if (type_symbol->m_symbol_kind == lang_Symbol::VARIABLE)
                {
                    lex.lang_error(lexer::errorlevel::error, node,
                        WO_ERR_UNEXPECTED_VAR_SYMBOL,
                        node->m_typeform.m_identifier->m_name->c_str());

                    return FAILED;
                }
                else
                {
                    if (!type_symbol->m_is_builtin)
                    {
                        union
                        {
                            lang_TypeInstance* type_instance;
                            lang_AliasInstance* alias_instance;
                        };

                        if (!node->m_LANG_template_evalating_state)
                        {
                            if (type_symbol->m_is_template)
                            {
                                lang_Symbol::TemplateArgumentListT template_args;
                                for (auto* typeholder : node->m_typeform.m_identifier->m_template_arguments.value())
                                {
                                    wo_assert(typeholder->m_LANG_determined_type);
                                    template_args.push_back(typeholder->m_LANG_determined_type.value());
                                }

                                auto template_eval_state_instance_may_nullopt = begin_eval_template_ast(
                                    lex, node, type_symbol, template_args, out_stack);

                                if (!template_eval_state_instance_may_nullopt)
                                    return FAILED;

                                auto* template_eval_state_instance =
                                    template_eval_state_instance_may_nullopt.value();

                                switch (template_eval_state_instance->m_state)
                                {
                                case lang_TemplateAstEvalStateValue::EVALUATING:
                                    node->m_LANG_template_evalating_state = template_eval_state_instance;
                                    return HOLD;
                                case lang_TemplateAstEvalStateValue::EVALUATED:
                                {
                                    if (type_symbol->m_symbol_kind == lang_Symbol::ALIAS)
                                        alias_instance = static_cast<lang_TemplateAstEvalStateAlias*>(
                                            template_eval_state_instance)->m_alias_instance.get();
                                    else
                                        type_instance = static_cast<lang_TemplateAstEvalStateType*>(
                                            template_eval_state_instance)->m_type_instance.get();

                                    break;
                                }
                                default:
                                    wo_error("Unexpected template eval state");
                                    break;
                                }
                            }
                            else
                            {
                                if (type_symbol->m_symbol_kind == lang_Symbol::ALIAS)
                                    alias_instance = type_symbol->m_alias_instance;
                                else
                                    type_instance = type_symbol->m_type_instance;
                            }

                            if (type_symbol->m_symbol_kind == lang_Symbol::ALIAS)
                            {
                                if (!alias_instance->m_determined_type.has_value())
                                {
                                    if (!node->m_LANG_trying_advancing_type_judgement && type_symbol->m_symbol_declare_ast)
                                    {
                                        node->m_LANG_trying_advancing_type_judgement = true;

                                        // Immediately advance the processing of declaration nodes.
                                        entry_spcify_scope(type_symbol->m_belongs_to_scope);
                                        WO_CONTINUE_PROCESS(type_symbol->m_symbol_declare_ast.value());
                                        return HOLD;
                                    }

                                    lex.lang_error(lexer::errorlevel::error, node,
                                        WO_ERR_TYPE_DETERMINED_FAILED);
                                    return FAILED;
                                }
                            }
                        }
                        else
                        {
                            auto* state = static_cast<lang_TemplateAstEvalStateBase*>(
                                node->m_LANG_template_evalating_state.value());

                            finish_eval_template_ast(state);

                            if (type_symbol->m_symbol_kind == lang_Symbol::ALIAS)
                                alias_instance = static_cast<lang_TemplateAstEvalStateAlias*>(
                                    state)->m_alias_instance.get();
                            else
                                type_instance = static_cast<lang_TemplateAstEvalStateType*>(
                                    state)->m_type_instance.get();
                        }

                        if (type_symbol->m_symbol_kind == lang_Symbol::ALIAS)
                        {
                            // Eval alias type.
                            type_instance = alias_instance->m_determined_type.value();
                        }

                        wo_assert(type_instance != nullptr);
                        node->m_LANG_determined_type = type_instance;
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
                wo_assert(node->m_typeform.m_typefrom->m_LANG_determined_type);
                node->m_LANG_determined_type = node->m_typeform.m_typefrom->m_LANG_determined_type.value();
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
        else
        {
            if (node->m_LANG_template_evalating_state)
            {
                auto* state = node->m_LANG_template_evalating_state.value();

                failed_eval_template_ast(state);
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
        if (node->m_LANG_trying_advancing_type_judgement)
            end_last_scope(); // Leave temporary advance the processing of declaration nodes.

        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_identifier);
            return HOLD;
        }
        else if (state == HOLD)
        {
            if (!node->m_LANG_trying_advancing_type_judgement)
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
                            lang_Symbol::TemplateArgumentListT template_args;
                            if (node->m_identifier->m_template_arguments.has_value())
                                for (auto* typeholder : node->m_identifier->m_template_arguments.value())
                                {
                                    wo_assert(typeholder->m_LANG_determined_type);
                                    template_args.push_back(typeholder->m_LANG_determined_type.value());
                                }
                            if (node->m_identifier->m_LANG_determined_and_appended_template_arguments.has_value())
                            {
                                const auto& determined_tyope_list =
                                    node->m_identifier->m_LANG_determined_and_appended_template_arguments.value();
                                template_args.insert(template_args.end(),
                                    determined_tyope_list.begin(),
                                    determined_tyope_list.end());
                            }

                            auto template_eval_state_instance_may_nullopt = begin_eval_template_ast(
                                lex, node, var_symbol, template_args, out_stack);

                            if (!template_eval_state_instance_may_nullopt)
                                return FAILED;

                            auto* template_eval_state_instance =
                                static_cast<lang_TemplateAstEvalStateValue*>(
                                    template_eval_state_instance_may_nullopt.value());

                            switch (template_eval_state_instance->m_state)
                            {
                            case lang_TemplateAstEvalStateValue::EVALUATING:
                            {
                                if (!template_eval_state_instance->m_value_instance->m_determined_type)
                                {
                                    node->m_LANG_template_evalating_state = template_eval_state_instance;
                                    return HOLD;
                                }
                                else
                                {
                                    // NOTE: If and only if a generic function instance can be 
                                    //  determined in the eval.
                                    //
                                    /* FALL THROUGH */
                                }
                            }
                            [[fallthrough]];
                            case lang_TemplateAstEvalStateValue::EVALUATED:
                                value_instance = template_eval_state_instance->m_value_instance.get();
                                break;
                            default:
                                wo_error("Unexpected template eval state");
                                break;
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
                    auto* state = static_cast<lang_TemplateAstEvalStateValue*>(
                        node->m_LANG_template_evalating_state.value());

                    finish_eval_template_ast(state);

                    value_instance = state->m_value_instance.get();
                }

                wo_assert(value_instance != nullptr);

                node->m_LANG_variable_instance = value_instance;
            }

            lang_ValueInstance* determined_value_instance =
                node->m_LANG_variable_instance.value();

            // Type has been determined?
            if (determined_value_instance->m_determined_type)
                node->m_LANG_determined_type = determined_value_instance->m_determined_type.value();
            else
            {
                if (!node->m_LANG_trying_advancing_type_judgement && determined_value_instance->m_symbol->m_symbol_declare_ast)
                {
                    node->m_LANG_trying_advancing_type_judgement = true;

                    // Type not determined, we need to determine it?
                    // NOTE: Immediately advance the processing of declaration nodes.
                    entry_spcify_scope(determined_value_instance->m_symbol->m_belongs_to_scope);
                    WO_CONTINUE_PROCESS(determined_value_instance->m_symbol->m_symbol_declare_ast.value());
                    return HOLD;
                }

                // Type determined failed in AstVariableDefines, treat as failed.
                lex.lang_error(lexer::errorlevel::error, node,
                    WO_ERR_VALUE_TYPE_DETERMINED_FAILED);
                return FAILED;
            }

            if (determined_value_instance->m_determined_constant)
                // Constant has been determined.
                node->decide_final_constant_value(
                    determined_value_instance->m_determined_constant.value());
        }
        else
        {
            if (node->m_LANG_template_evalating_state)
            {
                auto* state = node->m_LANG_template_evalating_state.value();

                failed_eval_template_ast(state);
            }
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstVariableDefines)
    {
        if (state == UNPROCESSED)
        {
            for (auto& defines : node->m_definitions)
                defines->m_LANG_declare_attribute = node->m_attribute;

            WO_CONTINUE_PROCESS_LIST(node->m_definitions);
            return HOLD;
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstVariableDefineItem)
    {
        if (state == UNPROCESSED)
        {
            if (!declare_pattern_symbol_pass0_1(
                lex,
                node->m_LANG_declare_attribute,
                node,
                node->m_pattern,
                node->m_init_value))
                return FAILED;

            if (node->m_pattern->node_type == AstBase::AST_PATTERN_SINGLE)
            {
                AstPatternSingle* single_pattern = static_cast<AstPatternSingle*>(node->m_pattern);
                if (single_pattern->m_template_parameters)
                    // Template variable, skip process init value.
                    return HOLD;
                else if (node->m_init_value->node_type == AstBase::AST_VALUE_FUNCTION)
                {
                    AstValueFunction* value_function = static_cast<AstValueFunction*>(node->m_init_value);
                    value_function->m_LANG_value_instance_to_update =
                        single_pattern->m_LANG_declared_symbol.value()->m_value_instance;
                }
            }

            WO_CONTINUE_PROCESS(node->m_init_value);
            return HOLD;
        }
        else if (state == HOLD)
        {
            if (!declare_pattern_symbol_pass0_1(
                lex,
                node->m_LANG_declare_attribute,
                node,
                node->m_pattern,
                node->m_init_value))
                return FAILED;

            if (!update_pattern_symbol_variable_type_pass1(
                lex,
                node->m_pattern,
                node->m_init_value,
                node->m_init_value->m_LANG_determined_type))
                return FAILED;
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
                        node,
                        node->source_location.source_file,
                        get_current_scope(),
                        node->m_type,
                        node->m_template_parameters.value(),
                        true);
                else
                    node->m_LANG_declared_symbol = define_symbol_in_current_scope(
                        node->m_typename,
                        node->m_attribute,
                        node,
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
    WO_PASS_PROCESSER(AstUsingTypeDeclare)
    {
        if (state == UNPROCESSED)
        {
            if (!node->m_LANG_declared_symbol)
            {
                if (node->m_template_parameters)
                    node->m_LANG_declared_symbol = define_symbol_in_current_scope(
                        node->m_typename,
                        node->m_attribute,
                        node,
                        node->source_location.source_file,
                        get_current_scope(),
                        node->m_type,
                        node->m_template_parameters.value(),
                        false);
                else
                    node->m_LANG_declared_symbol = define_symbol_in_current_scope(
                        node->m_typename,
                        node->m_attribute,
                        node,
                        node->source_location.source_file,
                        get_current_scope(),
                        lang_Symbol::kind::TYPE,
                        false);

                if (!node->m_LANG_declared_symbol)
                {
                    lex.lang_error(lexer::errorlevel::error, node,
                        WO_ERR_REDEFINED,
                        node->m_typename->c_str());
                    return FAILED;
                }
            }

            node->m_LANG_hold_state = AstUsingTypeDeclare::LANG_hold_state::HOLD_FOR_BASE_TYPE_EVAL;
            if (!node->m_template_parameters)
            {
                // Update type instance.
                WO_CONTINUE_PROCESS(node->m_type);
            }
            return HOLD;
        }
        else if (state == HOLD)
        {
            if (node->m_LANG_hold_state == AstUsingTypeDeclare::LANG_hold_state::HOLD_FOR_BASE_TYPE_EVAL)
            {
                if (!node->m_template_parameters)
                {
                    // TYPE HAS BEEN DETERMINED, UPDATE THE SYMBOL;
                    lang_Symbol* symbol = node->m_LANG_declared_symbol.value();
                    symbol->m_type_instance->determine_base_type_by_another_type(
                        node->m_type->m_LANG_determined_type.value());
                }

                node->m_LANG_hold_state = AstUsingTypeDeclare::LANG_hold_state::HOLD_FOR_NAMESPACE_BODY;
                if (node->m_in_type_namespace)
                {
                    WO_CONTINUE_PROCESS(node->m_in_type_namespace.value());
                }
                return HOLD;
            }
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstWhereConstraints)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS_LIST(node->m_constraints);
            return HOLD;
        }
        else if (state == HOLD)
        {
            bool failed = false;
            for (auto& constraint : node->m_constraints)
            {
                if (!constraint->m_evaled_const_value)
                {
                    failed = true;
                    lex.lang_error(lexer::errorlevel::error, constraint,
                        WO_ERR_CONSTRAINT_SHOULD_BE_CONST);
                    continue;
                }

                auto* constraint_type = constraint->m_LANG_determined_type.value();
                if (constraint_type != m_origin_types.m_bool.m_type_instance)
                {
                    failed = true;
                    lex.lang_error(lexer::errorlevel::error, constraint,
                        WO_ERR_CONSTRAINT_SHOULD_BE_BOOL);
                    continue;
                }

                if (!constraint->m_evaled_const_value.value().integer)
                {
                    failed = true;
                    lex.lang_error(lexer::errorlevel::error, constraint,
                        WO_ERR_CONSTRAINT_FAILED);
                    continue;
                }
            }

            if (failed)
                return FAILED;
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstFunctionParameterDeclare)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_type.value());
            return HOLD;
        }
        else if (state == HOLD)
        {
            if (!declare_pattern_symbol_pass0_1(
                lex,
                std::nullopt,
                std::nullopt,
                node->m_pattern,
                std::nullopt))
            {
                // Failed.
                return FAILED;
            }

            // Update pattern symbol type.
            if (!update_pattern_symbol_variable_type_pass1(
                lex,
                node->m_pattern,
                std::nullopt,
                node->m_type.value()->m_LANG_determined_type))
            {
                // Failed.
                return FAILED;
            }
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueFunction)
    {
        auto judge_function_return_type =
            [&](lang_TypeInstance* ret_type)
            {
                std::list<lang_TypeInstance*> parameters;
                for (auto& param : node->m_parameters)
                    parameters.push_back(param->m_type.value()->m_LANG_determined_type.value());

                node->m_LANG_determined_type = m_origin_types.create_function_type(
                    node->m_is_variadic, parameters, ret_type);

                wo_assert(node->m_LANG_determined_type.has_value());

                if (node->m_LANG_value_instance_to_update)
                {
                    node->m_LANG_value_instance_to_update.value()->m_determined_type =
                        node->m_LANG_determined_type;
                }
            };

        // Huston, we have a problem.
        if (state == UNPROCESSED)
        {
            if (node->m_pending_param_type_mark_template.has_value()
                && !node->m_LANG_in_template_reification_context)
            {
                lex.lang_error(lexer::errorlevel::error, node,
                    WO_ERR_NOT_IN_REIFICATION_TEMPLATE_FUNC);
                return FAILED;
            }

            if (node->m_LANG_determined_template_arguments.has_value())
            {
                begin_new_scope();

                wo_assert(node->m_LANG_determined_template_arguments.value().size()
                    == node->m_pending_param_type_mark_template.value().size());

                fast_create_template_type_alias_in_current_scope(
                    node->source_location.source_file,
                    node->m_pending_param_type_mark_template.value(),
                    node->m_LANG_determined_template_arguments.value());
            }

            node->m_LANG_hold_state = AstValueFunction::HOLD_FOR_PARAMETER_EVAL;

            // Begin new function.
            begin_new_function(node);

            WO_CONTINUE_PROCESS_LIST(node->m_parameters);
            return HOLD;
        }
        else if (state == HOLD)
        {
            switch (node->m_LANG_hold_state)
            {
            case AstValueFunction::HOLD_FOR_PARAMETER_EVAL:
            {
                if (node->m_marked_return_type)
                {
                    node->m_LANG_hold_state = AstValueFunction::HOLD_FOR_RETURN_TYPE_EVAL;
                    WO_CONTINUE_PROCESS(node->m_marked_return_type.value());
                    return HOLD;
                }
                /* FALL THROUGH */
            }
            [[fallthrough]];
            case AstValueFunction::HOLD_FOR_RETURN_TYPE_EVAL:
            {
                if (node->m_where_constraints)
                {
                    node->m_LANG_hold_state = AstValueFunction::HOLD_FOR_EVAL_WHERE_CONSTRAINTS;
                    WO_CONTINUE_PROCESS(node->m_where_constraints.value());
                    return HOLD;
                }
                /* FALL THROUGH */
            }
            [[fallthrough]];
            case AstValueFunction::HOLD_FOR_EVAL_WHERE_CONSTRAINTS:
            {
                // Eval function type for outside.
                if (node->m_marked_return_type)
                {
                    auto* return_type_instance = node->m_marked_return_type.value()->m_LANG_determined_type.value();
                    judge_function_return_type(return_type_instance);
                }

                node->m_LANG_hold_state = AstValueFunction::HOLD_FOR_BODY_EVAL;
                WO_CONTINUE_PROCESS(node->m_body);
                return HOLD;
            }
            case AstValueFunction::HOLD_FOR_BODY_EVAL:
            {
                end_last_function();
                if (node->m_LANG_determined_template_arguments.has_value())
                    end_last_scope();

                if (!node->m_LANG_determined_return_type)
                    node->m_LANG_determined_return_type = m_origin_types.m_void.m_type_instance;

                if (node->m_marked_return_type)
                {
                    // Function type has been determined.

                    auto* marked_return_type = node->m_marked_return_type.value();

                    auto* return_type_instance = marked_return_type->m_LANG_determined_type.value();
                    auto* determined_return_type = node->m_LANG_determined_return_type.value();

                    if (!is_type_accepted(
                        lex,
                        marked_return_type,
                        return_type_instance,
                        determined_return_type))
                    {
                        lex.lang_error(lexer::errorlevel::error, node,
                            WO_ERR_UNMATCHED_RETURN_TYPE_NAMED,
                            get_type_name_w(determined_return_type),
                            get_type_name_w(return_type_instance));
                        return FAILED;
                    }
                }
                else
                {
                    judge_function_return_type(
                        node->m_LANG_determined_return_type.value());
                }

                break;
            }
            default:
                wo_error("unknown hold state");
                break;
            }
        }
        else
        {
            end_last_function(); // Failed, leave the function.
            if (node->m_LANG_determined_template_arguments.has_value())
                end_last_scope(); // Failed, leave the template type alias.
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstReturn)
    {
        if (state == UNPROCESSED)
        {
            if (node->m_value)
            {
                WO_CONTINUE_PROCESS(node->m_value.value());
                return HOLD;
            }
            return OKAY;
        }
        else if (state == HOLD)
        {
            auto current_function = get_current_function();
            if (current_function)
            {
                auto* function_instance = current_function.value();

                lang_TypeInstance* return_value_type;
                if (node->m_value)
                    return_value_type = node->m_value.value()->m_LANG_determined_type.value();
                else
                    return_value_type = m_origin_types.m_void.m_type_instance;

                if (!function_instance->m_LANG_determined_return_type.has_value()
                    && function_instance->m_marked_return_type.has_value())
                {
                    function_instance->m_LANG_determined_return_type =
                        function_instance->m_marked_return_type.value()->m_LANG_determined_type.value();
                }

                if (function_instance->m_LANG_determined_return_type)
                {
                    auto* return_type_instance =
                        function_instance->m_LANG_determined_return_type.value();
                    auto* determined_return_type =
                        node->m_value.value()->m_LANG_determined_type.value();

                    if (!is_type_accepted(
                        lex,
                        node,
                        return_type_instance,
                        determined_return_type))
                    {
                        lex.lang_error(lexer::errorlevel::error, node,
                            WO_ERR_UNMATCHED_RETURN_TYPE_NAMED,
                            get_type_name_w(determined_return_type),
                            get_type_name_w(return_type_instance));
                        return FAILED;
                    }
                }
                else
                {
                    function_instance->m_LANG_determined_return_type =
                        node->m_value.value()->m_LANG_determined_type;
                }
            }
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueArrayOrVec)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS_LIST(node->m_elements);
            return HOLD;
        }
        else if (state == HOLD)
        {
            lang_TypeInstance* array_elemnet_type = m_origin_types.m_nothing.m_type_instance;
            if (!node->m_elements.empty())
            {
                auto element_iter = node->m_elements.begin();
                auto element_end = node->m_elements.end();

                array_elemnet_type = (*(element_iter++))->m_LANG_determined_type.value();
                for (; element_iter != element_end; ++element_iter)
                {
                    AstValueBase* element = *element_iter;
                    lang_TypeInstance* element_type = element->m_LANG_determined_type.value();
                    if (!is_type_accepted(lex, element, array_elemnet_type, element_type))
                    {
                        lex.lang_error(lexer::errorlevel::error,
                            element,
                            WO_ERR_UNMATCHED_ARRAY_ELEMENT_TYPE_NAMED,
                            get_type_name_w(element_type),
                            get_type_name_w(array_elemnet_type));
                        return FAILED;
                    }
                }
            }
            if (node->m_making_vec)
            {
                node->m_LANG_determined_type =
                    m_origin_types.create_vector_type(array_elemnet_type);
            }
            else
            {
                node->m_LANG_determined_type =
                    m_origin_types.create_array_type(array_elemnet_type);
            }
        }

        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueMarkAsMutable)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_marked_value);
            return HOLD;
        }
        else if (state == HOLD)
        {
            node->m_LANG_determined_type = mutable_type(
                node->m_marked_value->m_LANG_determined_type.value());
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueMarkAsImmutable)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_marked_value);
            return HOLD;
        }
        else if (state == HOLD)
        {
            node->m_LANG_determined_type = immutable_type(
                node->m_marked_value->m_LANG_determined_type.value());
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstKeyValuePair)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_key);
            WO_CONTINUE_PROCESS(node->m_value);
            return HOLD;
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueDictOrMap)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS_LIST(node->m_elements);
            return HOLD;
        }
        else if (state == HOLD)
        {
            lang_TypeInstance* key_type = m_origin_types.m_nothing.m_type_instance;
            lang_TypeInstance* value_type = m_origin_types.m_nothing.m_type_instance;

            if (!node->m_elements.empty())
            {
                auto element_iter = node->m_elements.begin();
                auto element_end = node->m_elements.end();

                key_type = (*element_iter)->m_key->m_LANG_determined_type.value();
                value_type = (*element_iter)->m_value->m_LANG_determined_type.value();

                for (++element_iter; element_iter != element_end; ++element_iter)
                {
                    AstKeyValuePair* element = *element_iter;
                    lang_TypeInstance* element_key_type = element->m_key->m_LANG_determined_type.value();
                    lang_TypeInstance* element_value_type = element->m_value->m_LANG_determined_type.value();

                    if (!is_type_accepted(lex, element, key_type, element_key_type))
                    {
                        lex.lang_error(lexer::errorlevel::error, element,
                            WO_ERR_UNMATCHED_DICT_KEY_TYPE_NAMED,
                            get_type_name_w(element_key_type),
                            get_type_name_w(key_type));
                        return FAILED;
                    }

                    if (!is_type_accepted(lex, element, value_type, element_value_type))
                    {
                        lex.lang_error(lexer::errorlevel::error, element,
                            WO_ERR_UNMATCHED_DICT_VALUE_TYPE_NAMED,
                            get_type_name_w(element_value_type),
                            get_type_name_w(value_type));
                        return FAILED;
                    }
                }
            }
            if (node->m_making_map)
            {
                node->m_LANG_determined_type =
                    m_origin_types.create_mapping_type(key_type, value_type);
            }
            else
            {
                node->m_LANG_determined_type =
                    m_origin_types.create_dictionary_type(key_type, value_type);
            }
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueTuple)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS_LIST(node->m_elements);
            return HOLD;
        }
        else if (state == HOLD)
        {
            std::list<lang_TypeInstance*> element_types;
            for (auto& element : node->m_elements)
            {
                auto determined_type = element->m_LANG_determined_type.value();
                if (element->node_type == AstBase::AST_FAKE_VALUE_UNPACK)
                {
                    auto* determined_base_type_instance = determined_type->get_determined_type().value();
                    // Unpacks base has been check in AstFakeValueUnpack.

                    if (determined_base_type_instance->m_base_type != lang_TypeInstance::DeterminedType::TUPLE)
                    {
                        lex.lang_error(lexer::errorlevel::error, element,
                            WO_ERR_ONLY_EXPAND_TUPLE,
                            get_type_name_w(determined_type));
                        return FAILED;
                    }

                    for (auto& element_type : determined_base_type_instance->m_external_type_description.m_tuple->m_element_types)
                        element_types.push_back(element_type);
                }
                else
                    element_types.push_back(determined_type);
            }

            node->m_LANG_determined_type = m_origin_types.create_tuple_type(element_types);
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueIndex)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_container);
            WO_CONTINUE_PROCESS(node->m_index);
            return HOLD;
        }
        else if (state == HOLD)
        {
            // Check is index able?
            auto* container_type_instance = node->m_container->m_LANG_determined_type.value();
            auto container_determined_base_type =
                container_type_instance->get_determined_type();
            auto* indexer_type_instance = node->m_index->m_LANG_determined_type.value();
            auto indexer_determined_base_type =
                indexer_type_instance->get_determined_type();

            if (!container_determined_base_type)
            {
                lex.lang_error(lexer::errorlevel::error, node->m_container,
                    WO_ERR_TYPE_NAMED_DETERMINED_FAILED,
                    get_type_name_w(container_type_instance));

                return FAILED;
            }
            if (!indexer_determined_base_type)
            {
                lex.lang_error(lexer::errorlevel::error, node->m_index,
                    WO_ERR_TYPE_NAMED_DETERMINED_FAILED,
                    get_type_name_w(indexer_type_instance));

                return FAILED;
            }

            auto* container_determined_base_type_instance =
                container_determined_base_type.value();
            auto* indexer_determined_base_type_instance =
                indexer_determined_base_type.value();

            switch (container_determined_base_type_instance->m_base_type)
            {
            case lang_TypeInstance::DeterminedType::ARRAY:
            case lang_TypeInstance::DeterminedType::VECTOR:
            {
                if (indexer_determined_base_type_instance->m_base_type
                    != lang_TypeInstance::DeterminedType::INTEGER)
                {
                    lex.lang_error(lexer::errorlevel::error, node->m_index,
                        WO_ERR_CANNOT_INDEX_TYPE_WITH_TYPE,
                        get_type_name_w(container_type_instance),
                        get_type_name_w(indexer_type_instance));
                    return FAILED;
                }
                node->m_LANG_determined_type =
                    container_determined_base_type_instance
                    ->m_external_type_description.m_array_or_vector
                    ->m_element_type;
                break;
            }
            case lang_TypeInstance::DeterminedType::DICTIONARY:
            case lang_TypeInstance::DeterminedType::MAPPING:
            {
                if (immutable_type(container_determined_base_type_instance
                    ->m_external_type_description.m_dictionary_or_mapping
                    ->m_key_type) != immutable_type(indexer_type_instance))
                {
                    lex.lang_error(lexer::errorlevel::error, node->m_index,
                        WO_ERR_CANNOT_INDEX_TYPE_WITH_TYPE,
                        get_type_name_w(container_type_instance),
                        get_type_name_w(indexer_type_instance));
                    return FAILED;
                }
                node->m_LANG_determined_type =
                    container_determined_base_type_instance
                    ->m_external_type_description.m_dictionary_or_mapping
                    ->m_value_type;
                break;
            }
            case lang_TypeInstance::DeterminedType::STRUCT:
            {
                if (m_origin_types.m_string.m_type_instance
                    != immutable_type(indexer_type_instance))
                {
                    lex.lang_error(lexer::errorlevel::error, node->m_index,
                        WO_ERR_CANNOT_INDEX_TYPE_WITH_TYPE,
                        get_type_name_w(container_type_instance),
                        get_type_name_w(indexer_type_instance));
                    return FAILED;
                }
                if (!node->m_index->m_evaled_const_value.has_value())
                {
                    lex.lang_error(lexer::errorlevel::error, node->m_index,
                        WO_ERR_CANNOT_INDEX_STRUCT_WITH_NON_CONST);
                    return FAILED;
                }

                wo_assert(node->m_index->m_evaled_const_value.value().type
                    == value::valuetype::string_type);

                wo_pstring_t member_name = wo::wstring_pool::get_pstr(
                    str_to_wstr(*node->m_index->m_evaled_const_value.value().string));

                auto* struct_type = container_determined_base_type_instance
                    ->m_external_type_description.m_struct;

                auto fnd = struct_type->m_member_types.find(member_name);
                if (fnd == struct_type->m_member_types.end())
                {
                    lex.lang_error(lexer::errorlevel::error, node->m_index,
                        WO_ERR_STRUCT_DONOT_HAVE_MAMBER_NAMED,
                        get_type_name_w(container_type_instance),
                        member_name);
                    return FAILED;
                }

                // TODO: check member's access permission.
                node->m_LANG_determined_type = fnd->second.m_member_type;
                node->m_LANG_fast_index_for_struct = fnd->second.m_offset;

                break;
            }
            case lang_TypeInstance::DeterminedType::TUPLE:
            {
                if (m_origin_types.m_int.m_type_instance
                    != immutable_type(indexer_type_instance))
                {
                    lex.lang_error(lexer::errorlevel::error, node->m_index,
                        WO_ERR_CANNOT_INDEX_TYPE_WITH_TYPE,
                        get_type_name_w(container_type_instance),
                        get_type_name_w(indexer_type_instance));
                    return FAILED;
                }
                if (!node->m_index->m_evaled_const_value.has_value())
                {
                    lex.lang_error(lexer::errorlevel::error, node->m_index,
                        WO_ERR_CANNOT_INDEX_TUPLE_WITH_NON_CONST);
                    return FAILED;
                }

                wo_assert(node->m_index->m_evaled_const_value.value().type
                    == value::valuetype::integer_type);

                auto index = node->m_index->m_evaled_const_value.value().integer;
                auto* tuple_type = container_determined_base_type_instance
                    ->m_external_type_description.m_tuple;

                if (index < 0 || (size_t)index >= tuple_type->m_element_types.size())
                {
                    lex.lang_error(lexer::errorlevel::error, node->m_index,
                        WO_ERR_TUPLE_INDEX_OUT_OF_RANGE,
                        get_type_name_w(container_type_instance),
                        tuple_type->m_element_types.size(),
                        (size_t)index);
                    return FAILED;
                }

                node->m_LANG_determined_type = tuple_type->m_element_types[index];
                node->m_LANG_fast_index_for_struct = index;

                break;
            }
            default:
                lex.lang_error(lexer::errorlevel::error, node->m_container,
                    WO_ERR_UNINDEXABLE_TYPE_NAMED,
                    get_type_name_w(container_type_instance));

                return FAILED;
            }
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstFakeValueUnpack)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_unpack_value);
            return HOLD;
        }
        else if (state == HOLD)
        {
            auto* unpack_value_type_instance =
                node->m_unpack_value->m_LANG_determined_type.value();
            auto unpack_value_type_determined_base_type =
                unpack_value_type_instance->get_determined_type();

            if (!unpack_value_type_determined_base_type)
            {
                lex.lang_error(lexer::errorlevel::error, node->m_unpack_value,
                    WO_ERR_TYPE_NAMED_DETERMINED_FAILED,
                    get_type_name_w(unpack_value_type_instance));
                return FAILED;
            }

            auto* unpack_value_determined_base_type_instance =
                unpack_value_type_determined_base_type.value();

            switch (unpack_value_determined_base_type_instance->m_base_type)
            {
            case lang_TypeInstance::DeterminedType::ARRAY:
            case lang_TypeInstance::DeterminedType::VECTOR:
            case lang_TypeInstance::DeterminedType::TUPLE:
                break;
            default:
                lex.lang_error(lexer::errorlevel::error, node->m_unpack_value,
                    WO_ERR_ONLY_EXPAND_ARRAY_VEC_AND_TUPLE,
                    get_type_name_w(unpack_value_type_instance));
                return FAILED;
            }

            node->m_LANG_determined_type = unpack_value_type_instance;
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueMakeUnion)
    {
        if (state == UNPROCESSED)
        {
            if (node->m_packed_value)
                WO_CONTINUE_PROCESS(node->m_packed_value.value());

            return HOLD;
        }
        else if (state == HOLD)
        {
            // Trick to make compiler happy~.
            node->m_LANG_determined_type =
                m_origin_types.m_nothing.m_type_instance;
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstEnumDeclare)
    {
        if (state == UNPROCESSED)
        {
            node->m_LANG_hold_state = AstEnumDeclare::HOLD_FOR_ENUM_TYPE_DECL;
            WO_CONTINUE_PROCESS(node->m_enum_type_declare);

            return HOLD;
        }
        else if (state == HOLD)
        {
            switch (node->m_LANG_hold_state)
            {
            case AstEnumDeclare::HOLD_FOR_ENUM_TYPE_DECL:
            {
                node->m_LANG_hold_state = AstEnumDeclare::HOLD_FOR_ENUM_ITEMS_DECL;
                WO_CONTINUE_PROCESS(node->m_enum_body);

                return HOLD;
            }
            case AstEnumDeclare::HOLD_FOR_ENUM_ITEMS_DECL:
                break;
            default:
                wo_error("unknown hold state");
                break;
            }
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstPatternVariable)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_variable);
            return HOLD;
        }
        else if (state == HOLD)
        {
            lang_ValueInstance* variable_instance = node->m_variable->m_LANG_variable_instance.value();
            if (!variable_instance->m_mutable)
            {
                lex.lang_error(lexer::errorlevel::error, node,
                    WO_ERR_PATTERN_VARIABLE_SHOULD_BE_MUTABLE);
                return FAILED;
            }
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstPatternIndex)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_index);
            return HOLD;
        }
        else if (state == HOLD)
        {
            lang_TypeInstance* index_result_type = node->m_index->m_LANG_determined_type.value();
            if (index_result_type->is_immutable())
            {
                lex.lang_error(lexer::errorlevel::error, node->m_index,
                    WO_ERR_PATTERN_INDEX_SHOULD_BE_MUTABLE_TYPE);
                return FAILED;
            }
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueTypeCast)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_cast_type);
            switch (node->m_cast_value->node_type)
            {
            case AstBase::AST_VALUE_FUNCTION:
            {
                AstValueFunction* casting_from_func = static_cast<AstValueFunction*>(node->m_cast_value);
                if (casting_from_func->m_pending_param_type_mark_template)
                {
                    // Trying to cast a template, defer type checking and do 
                    // template argument deduction later

                    node->m_LANG_hold_state = AstValueTypeCast::HOLD_FOR_TEMPLATE_ARGUMENT_DEDUCTION;
                    return HOLD;
                }
                break;
            }
            case AstBase::AST_VALUE_VARIABLE:
            {
                AstValueVariable* casting_from_var = static_cast<AstValueVariable*>(node->m_cast_value);
                if (find_symbol_in_current_scope(lex, casting_from_var->m_identifier))
                {
                    lang_Symbol* symbol = casting_from_var->m_identifier->m_LANG_determined_symbol.value();
                    if (symbol->m_symbol_kind == lang_Symbol::kind::VARIABLE
                        && symbol->m_is_template
                        && !symbol->m_template_value_instances->m_mutable
                        && symbol->m_template_value_instances->m_origin_value_ast->node_type == AstBase::AST_VALUE_FUNCTION
                        && (!casting_from_var->m_identifier->m_template_arguments.has_value()
                            || casting_from_var->m_identifier->m_template_arguments.value().size()
                            != symbol->m_template_value_instances->m_template_params.size()))
                    {
                        node->m_LANG_hold_state = AstValueTypeCast::HOLD_FOR_TEMPLATE_ARGUMENT_DEDUCTION;
                        return HOLD;
                    }
                }
                break;
            }
            }

            WO_CONTINUE_PROCESS(node->m_cast_value);

            node->m_LANG_hold_state = AstValueTypeCast::HOLD_FOR_NORMAL_EVAL;
            return HOLD;
        }
        else if (state == HOLD)
        {
            switch (node->m_LANG_hold_state)
            {
            case AstValueTypeCast::HOLD_FOR_TEMPLATE_ARGUMENT_DEDUCTION:
            {
                std::unordered_map<wo_pstring_t, lang_TypeInstance*> deduction_results;

                std::list<std::optional<lang_TypeInstance*>> argument_types;
                std::optional<lang_TypeInstance*> return_type;

                bool able_to_deduce = false;

                lang_TypeInstance* cast_target_type = node->m_cast_type->m_LANG_determined_type.value();
                auto cast_target_type_determined_base_type = cast_target_type->get_determined_type();
                if (cast_target_type_determined_base_type.has_value())
                {
                    auto* cast_target_type_determined_base_type_instance =
                        cast_target_type_determined_base_type.value();

                    if (cast_target_type_determined_base_type_instance->m_base_type ==
                        lang_TypeInstance::DeterminedType::FUNCTION)
                    {
                        auto* cast_target_determined_function_base_type =
                            cast_target_type_determined_base_type_instance->m_external_type_description.m_function;

                        return_type = cast_target_determined_function_base_type->m_return_type;
                        for (auto& param_type : cast_target_determined_function_base_type->m_param_types)
                            argument_types.push_back(param_type);

                        able_to_deduce = true;
                    }
                }

                if (!able_to_deduce)
                {
                    // Target type is not a function, unable to deduce.
                    lex.lang_error(lexer::errorlevel::error, node->m_cast_value,
                        WO_ERR_FAILED_TO_DEDUCE_TEMPLATE_TYPE);

                    return FAILED;
                }

                // Make function instance here.
                switch (node->m_cast_value->node_type)
                {
                case AstBase::AST_VALUE_VARIABLE:
                {
                    AstValueVariable* casting_from_var = static_cast<AstValueVariable*>(node->m_cast_value);
                    lang_Symbol* symbol = casting_from_var->m_identifier->m_LANG_determined_symbol.value();

                    auto* template_instance_prefab = symbol->m_template_value_instances;

                    std::list<wo_pstring_t> pending_template_params;
                    auto it_template_param = symbol->m_template_value_instances->m_template_params.begin();
                    auto it_template_param_end = symbol->m_template_value_instances->m_template_params.end();
                    if (casting_from_var->m_identifier->m_template_arguments.has_value())
                    {
                        for (auto& _useless : casting_from_var->m_identifier->m_template_arguments.value())
                        {
                            (void)_useless;
                            ++it_template_param;
                        }
                    }
                    for (; it_template_param != it_template_param_end; ++it_template_param)
                        pending_template_params.push_back(*it_template_param);

                    entry_spcify_scope(symbol->m_belongs_to_scope);

                    template_function_deduction_extraction_with_complete_type(
                        lex,
                        static_cast<AstValueFunction*>(symbol->m_template_value_instances->m_origin_value_ast),
                        argument_types,
                        return_type,
                        pending_template_params,
                        &deduction_results);

                    end_last_scope();

                    if (deduction_results.size() != pending_template_params.size())
                    {
                        lex.lang_error(lexer::errorlevel::error, casting_from_var,
                            WO_ERR_FAILED_TO_DEDUCE_TEMPLATE_TYPE);

                        return FAILED;
                    }

                    ////////////////////////////
                    std::list<lang_TypeInstance*> template_arguments;
                    for (wo_pstring_t param : pending_template_params)
                    {
                        template_arguments.push_back(deduction_results.at(param));
                    }
                    casting_from_var->m_identifier->m_LANG_determined_and_appended_template_arguments
                        = template_arguments;

                    WO_CONTINUE_PROCESS(casting_from_var);
                    break;
                }
                case AstBase::AST_VALUE_FUNCTION:
                {
                    AstValueFunction* casting_from_func = static_cast<AstValueFunction*>(node->m_cast_value);

                    const auto function_template_params =
                        casting_from_func->m_pending_param_type_mark_template.value();

                    template_function_deduction_extraction_with_complete_type(
                        lex,
                        casting_from_func,
                        argument_types,
                        return_type,
                        casting_from_func->m_pending_param_type_mark_template.value(),
                        &deduction_results);

                    if (deduction_results.size() != function_template_params.size())
                    {
                        lex.lang_error(lexer::errorlevel::error, casting_from_func,
                            WO_ERR_FAILED_TO_DEDUCE_TEMPLATE_TYPE);

                        return FAILED;
                    }

                    ////////////////////////////

                    std::list<lang_TypeInstance*> template_arguments;
                    for (wo_pstring_t param : function_template_params)
                    {
                        template_arguments.push_back(deduction_results.at(param));
                    }

                    casting_from_func->m_LANG_determined_template_arguments = template_arguments;
                    casting_from_func->m_LANG_in_template_reification_context = true;
                    WO_CONTINUE_PROCESS(casting_from_func);

                    break;
                }
                default:
                    wo_error("Unknown template target.");
                }

                node->m_LANG_hold_state = AstValueTypeCast::HOLD_FOR_NORMAL_EVAL;
                return HOLD;
            }
            case AstValueTypeCast::HOLD_FOR_NORMAL_EVAL:
            {
                auto* target_type = node->m_cast_type->m_LANG_determined_type.value();
                auto* casting_value_type = node->m_cast_value->m_LANG_determined_type.value();

                if (!check_cast_able(lex, node, target_type, casting_value_type))
                {
                    lex.lang_error(lexer::errorlevel::error, node,
                        WO_ERR_CANNOT_CAST_TYPE_TO_TYPE,
                        get_type_name_w(casting_value_type),
                        get_type_name_w(target_type));

                    return FAILED;
                }

                node->m_LANG_determined_type = target_type;
                break;
            }
            default:
                wo_error("unknown hold state");
                break;
            }
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueFunctionCall_FakeAstArgumentDeductionContextA)
    {
        if (state == UNPROCESSED)
        {
            entry_spcify_scope(node->m_apply_template_argument_scope);

            for (auto& [param_name, arg_type_inst] : node->m_deduction_results)
            {
                fast_create_one_template_type_alias_in_current_scope(
                    node->source_location.source_file,
                    param_name,
                    arg_type_inst);

                auto fnd = std::find(
                    node->m_undetermined_template_params.begin(),
                    node->m_undetermined_template_params.end(),
                    param_name);

                if (fnd != node->m_undetermined_template_params.end())
                    node->m_undetermined_template_params.erase(fnd);
            }

            node->m_LANG_hold_state =
                AstValueFunctionCall_FakeAstArgumentDeductionContextA::HOLD_FOR_PREPARE;

            return HOLD;
        }
        else if (state == HOLD)
        {
            switch (node->m_LANG_hold_state)
            {
            case AstValueFunctionCall_FakeAstArgumentDeductionContextA::HOLD_FOR_PREPARE:
            {
                // Remove determined template param
                for (auto it = node->m_undetermined_template_params.begin();
                    it != node->m_undetermined_template_params.end();)
                {
                    auto cur_it = it++;

                    auto fnd = node->m_deduction_results.find(*cur_it);
                    if (fnd != node->m_deduction_results.end())
                    {
                        fast_create_one_template_type_alias_in_current_scope(
                            node->source_location.source_file,
                            fnd->first,
                            fnd->second);

                        node->m_undetermined_template_params.erase(cur_it);
                    }
                }

                if (node->m_arguments_tobe_deduct.empty())
                {
                    // All arguments have been deduced.
                    end_last_scope();
                    break;
                }

                node->m_current_argument = node->m_arguments_tobe_deduct.begin();
                node->m_LANG_hold_state = AstValueFunctionCall_FakeAstArgumentDeductionContextA::HOLD_FOR_EVAL_PARAM_TYPE;

                return HOLD;
            }
            case AstValueFunctionCall_FakeAstArgumentDeductionContextA::HOLD_FOR_EVAL_PARAM_TYPE:
            {
                AstValueFunctionCall_FakeAstArgumentDeductionContextA::ArgumentMatch& current_argument =
                    *node->m_current_argument;

                if (!check_type_may_dependence_template_parameters(
                    current_argument.m_duped_param_type, node->m_undetermined_template_params))
                {
                    // Greate! we can update the type now.
                    WO_CONTINUE_PROCESS(current_argument.m_duped_param_type);
                }
                else
                {
                    if (current_argument.m_duped_param_type->m_formal != AstTypeHolder::FUNCTION)
                    {
                        node->m_LANG_hold_state =
                            AstValueFunctionCall_FakeAstArgumentDeductionContextA::HOLD_TO_NEXT_ARGUMENT;
                        return HOLD;
                    }

                    std::list<AstTypeHolder*> type_to_eval;

                    auto& function_formal = current_argument.m_duped_param_type->m_typeform.m_function;
                    for (auto* param : function_formal.m_parameters)
                    {
                        if (!check_type_may_dependence_template_parameters(param, node->m_undetermined_template_params))
                            type_to_eval.push_back(param);
                    }
                    if (!check_type_may_dependence_template_parameters(
                        function_formal.m_return_type, node->m_undetermined_template_params))
                        type_to_eval.push_back(function_formal.m_return_type);

                    WO_CONTINUE_PROCESS_LIST(type_to_eval);
                }
                node->m_LANG_hold_state =
                    AstValueFunctionCall_FakeAstArgumentDeductionContextA::HOLD_FOR_DEDUCT_ARGUMENT;
                return HOLD;
            }
            case AstValueFunctionCall_FakeAstArgumentDeductionContextA::HOLD_FOR_DEDUCT_ARGUMENT:
            {
                AstValueFunctionCall_FakeAstArgumentDeductionContextA::ArgumentMatch& current_argument =
                    *node->m_current_argument;

                std::list<std::optional<lang_TypeInstance*>> param_argument_types;
                std::optional<lang_TypeInstance*> param_return_type = std::nullopt;

                if (current_argument.m_duped_param_type->m_formal == AstTypeHolder::FUNCTION)
                {
                    // That's good!
                    for (auto* param_type_holder :
                        current_argument.m_duped_param_type->m_typeform.m_function.m_parameters)
                    {
                        param_argument_types.push_back(param_type_holder->m_LANG_determined_type);
                    }
                    param_return_type =
                        current_argument.m_duped_param_type->m_typeform.m_function.m_return_type->m_LANG_determined_type;
                }
                else
                {
                    if (current_argument.m_duped_param_type->m_LANG_determined_type.has_value())
                    {
                        lang_TypeInstance* determined_type =
                            current_argument.m_duped_param_type->m_LANG_determined_type.value();
                        auto determined_base_type = determined_type->get_determined_type();
                        if (determined_base_type.has_value())
                        {
                            auto* determined_base_type_instance = determined_base_type.value();
                            if (determined_base_type_instance->m_base_type == lang_TypeInstance::DeterminedType::FUNCTION)
                            {
                                for (auto* param_type :
                                    determined_base_type_instance->m_external_type_description.m_function->m_param_types)
                                {
                                    param_argument_types.push_back(param_type);
                                }
                                param_return_type =
                                    determined_base_type_instance->m_external_type_description.m_function->m_return_type;
                            }
                        }
                    }
                }

                if (param_argument_types.empty() && !param_return_type.has_value())
                    node->m_LANG_hold_state =
                    AstValueFunctionCall_FakeAstArgumentDeductionContextA::HOLD_TO_NEXT_ARGUMENT;
                else
                {
                    // Begin our main work!
                    switch (current_argument.m_argument->node_type)
                    {
                    case AstBase::AST_VALUE_VARIABLE:
                    {
                        AstValueVariable* argument_variable = static_cast<AstValueVariable*>(current_argument.m_argument);
                        lang_Symbol* symbol = argument_variable->m_identifier->m_LANG_determined_symbol.value();

                        wo_assert(symbol->m_is_template
                            && symbol->m_symbol_kind == lang_Symbol::kind::VARIABLE
                            && !symbol->m_template_value_instances->m_mutable);

                        auto& template_instance_prefab = symbol->m_template_value_instances;

                        std::unordered_map<wo_pstring_t, lang_TypeInstance*> deduction_results;
                        std::list<wo_pstring_t> pending_template_params;
                        auto it_template_param = symbol->m_template_value_instances->m_template_params.begin();
                        auto it_template_param_end = symbol->m_template_value_instances->m_template_params.end();
                        if (argument_variable->m_identifier->m_template_arguments.has_value())
                        {
                            for (auto& _useless : argument_variable->m_identifier->m_template_arguments.value())
                            {
                                (void)_useless;
                                ++it_template_param;
                            }
                        }
                        for (; it_template_param != it_template_param_end; ++it_template_param)
                            pending_template_params.push_back(*it_template_param);

                        entry_spcify_scope(symbol->m_belongs_to_scope);

                        template_function_deduction_extraction_with_complete_type(
                            lex,
                            static_cast<AstValueFunction*>(symbol->m_template_value_instances->m_origin_value_ast),
                            param_argument_types,
                            param_return_type,
                            pending_template_params,
                            &deduction_results);

                        end_last_scope();

                        if (deduction_results.size() == pending_template_params.size())
                        {
                            // We can decided this argument now.
                            std::list<lang_TypeInstance*> template_arguments;
                            for (wo_pstring_t param : pending_template_params)
                            {
                                template_arguments.push_back(deduction_results.at(param));
                            }

                            argument_variable->m_identifier->m_LANG_determined_and_appended_template_arguments
                                = template_arguments;

                            WO_CONTINUE_PROCESS(argument_variable);

                            node->m_LANG_hold_state =
                                AstValueFunctionCall_FakeAstArgumentDeductionContextA::HOLD_FOR_REVERSE_DEDUCT;
                        }
                        else
                            node->m_LANG_hold_state =
                            AstValueFunctionCall_FakeAstArgumentDeductionContextA::HOLD_TO_NEXT_ARGUMENT;

                        break;
                    }
                    case AstBase::AST_VALUE_FUNCTION:
                    {
                        AstValueFunction* argument_function = static_cast<AstValueFunction*>(current_argument.m_argument);
                        auto& pending_template_arguments = argument_function->m_pending_param_type_mark_template.value();

                        std::unordered_map<wo_pstring_t, lang_TypeInstance*> deduction_results;
                        template_function_deduction_extraction_with_complete_type(
                            lex,
                            argument_function,
                            param_argument_types,
                            param_return_type,
                            pending_template_arguments,
                            &deduction_results);

                        if (deduction_results.size() == pending_template_arguments.size())
                        {
                            // We can decided this argument now.
                            std::list<lang_TypeInstance*> template_arguments;
                            for (wo_pstring_t param : pending_template_arguments)
                            {
                                template_arguments.push_back(deduction_results.at(param));
                            }

                            argument_function->m_LANG_determined_template_arguments = template_arguments;
                            argument_function->m_LANG_in_template_reification_context = true;
                            WO_CONTINUE_PROCESS(argument_function);

                            node->m_LANG_hold_state =
                                AstValueFunctionCall_FakeAstArgumentDeductionContextA::HOLD_FOR_REVERSE_DEDUCT;
                        }
                        else
                            node->m_LANG_hold_state =
                            AstValueFunctionCall_FakeAstArgumentDeductionContextA::HOLD_TO_NEXT_ARGUMENT;

                        break;
                    }
                    default:
                        wo_error("Unknown argument type.");
                    }
                }

                return HOLD;
            }
            case AstValueFunctionCall_FakeAstArgumentDeductionContextA::HOLD_FOR_REVERSE_DEDUCT:
            {
                AstValueFunctionCall_FakeAstArgumentDeductionContextA::ArgumentMatch& current_argument =
                    *node->m_current_argument;

                lang_TypeInstance* argument_final_type =
                    current_argument.m_argument->m_LANG_determined_type.value();

                template_type_deduction_extraction_with_complete_type(
                    lex,
                    current_argument.m_duped_param_type,
                    argument_final_type,
                    node->m_undetermined_template_params,
                    &node->m_deduction_results);

                // Now we make progress, we can do more things.
                node->m_arguments_tobe_deduct.erase(node->m_current_argument);
                node->m_LANG_hold_state =
                    AstValueFunctionCall_FakeAstArgumentDeductionContextA::HOLD_FOR_PREPARE;

                return HOLD;
            }
            case AstValueFunctionCall_FakeAstArgumentDeductionContextA::HOLD_TO_NEXT_ARGUMENT:
            {
                if (++node->m_current_argument == node->m_arguments_tobe_deduct.end())
                {
                    // Failed...
                    lex.lang_error(lexer::errorlevel::error, node->m_arguments_tobe_deduct.front().m_argument,
                        WO_ERR_FAILED_TO_DEDUCE_TEMPLATE_TYPE);

                    end_last_scope(); // End the scope.

                    return FAILED;
                }
                node->m_LANG_hold_state =
                    AstValueFunctionCall_FakeAstArgumentDeductionContextA::HOLD_FOR_EVAL_PARAM_TYPE;

                return HOLD;
            }
            }
        }
        else
        {
            end_last_scope(); // Child failed, and end the scope.
        }

        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueFunctionCall_FakeAstArgumentDeductionContextB)
    {
        if (state == UNPROCESSED)
        {
            node->m_LANG_hold_state =
                AstValueFunctionCall_FakeAstArgumentDeductionContextB::HOLD_FOR_DEDUCE;
            return HOLD;
        }
        else if (state == HOLD)
        {
            switch (node->m_LANG_hold_state)
            {
            case AstValueFunctionCall_FakeAstArgumentDeductionContextB::HOLD_FOR_DEDUCE:
            {
                if (node->m_arguments_tobe_deduct.empty())
                {
                    // All arguments have been deduced.
                    break;
                }
                auto& param_and_argument_pair = node->m_arguments_tobe_deduct.front();

                lang_TypeInstance* param_type = param_and_argument_pair.m_param_type;
                AstValueBase* argument = param_and_argument_pair.m_argument;

                std::list<std::optional<lang_TypeInstance*>> param_argument_types;
                std::optional<lang_TypeInstance*> param_return_type = std::nullopt;

                auto param_type_determined_base_type = param_type->get_determined_type();
                if (!param_type_determined_base_type.has_value())
                {
                    lex.lang_error(lexer::errorlevel::error, argument,
                        WO_ERR_TYPE_NAMED_DETERMINED_FAILED,
                        get_type_name_w(param_type));
                    return FAILED;
                }

                auto* param_type_determined_base_type_instance = param_type_determined_base_type.value();
                if (param_type_determined_base_type_instance->m_base_type != lang_TypeInstance::DeterminedType::FUNCTION)
                {
                    lex.lang_error(lexer::errorlevel::error, argument,
                        WO_ERR_FAILED_TO_DEDUCE_TEMPLATE_TYPE);
                    return FAILED;
                }

                auto* param_type_determined_function_base_type =
                    param_type_determined_base_type_instance->m_external_type_description.m_function;

                for (auto& param_type : param_type_determined_function_base_type->m_param_types)
                    param_argument_types.push_back(param_type);
                param_return_type = param_type_determined_function_base_type->m_return_type;

                switch (argument->node_type)
                {
                case AstBase::AST_VALUE_FUNCTION:
                {
                    AstValueFunction* argument_function = static_cast<AstValueFunction*>(argument);
                    auto& pending_template_arguments = argument_function->m_pending_param_type_mark_template.value();

                    std::unordered_map<wo_pstring_t, lang_TypeInstance*> deduction_results;
                    template_function_deduction_extraction_with_complete_type(
                        lex,
                        argument_function,
                        param_argument_types,
                        param_return_type,
                        pending_template_arguments,
                        &deduction_results);

                    if (deduction_results.size() == pending_template_arguments.size())
                    {
                        // We can decided this argument now.
                        std::list<lang_TypeInstance*> template_arguments;
                        for (wo_pstring_t param : pending_template_arguments)
                        {
                            template_arguments.push_back(deduction_results.at(param));
                        }

                        argument_function->m_LANG_determined_template_arguments = template_arguments;
                        argument_function->m_LANG_in_template_reification_context = true;
                        WO_CONTINUE_PROCESS(argument_function);
                    }
                    else
                    {
                        lex.lang_error(lexer::errorlevel::error, argument,
                            WO_ERR_FAILED_TO_DEDUCE_TEMPLATE_TYPE);
                        return FAILED;
                    }
                    break;
                }
                case AstBase::AST_VALUE_VARIABLE:
                {
                    AstValueVariable* argument_variable = static_cast<AstValueVariable*>(argument);
                    lang_Symbol* symbol = argument_variable->m_identifier->m_LANG_determined_symbol.value();

                    wo_assert(symbol->m_is_template
                        && symbol->m_symbol_kind == lang_Symbol::kind::VARIABLE
                        && !symbol->m_template_value_instances->m_mutable);

                    auto& template_instance_prefab = symbol->m_template_value_instances;

                    std::unordered_map<wo_pstring_t, lang_TypeInstance*> deduction_results;
                    std::list<wo_pstring_t> pending_template_params;
                    auto it_template_param = symbol->m_template_value_instances->m_template_params.begin();
                    auto it_template_param_end = symbol->m_template_value_instances->m_template_params.end();
                    if (argument_variable->m_identifier->m_template_arguments.has_value())
                    {
                        for (auto& _useless : argument_variable->m_identifier->m_template_arguments.value())
                        {
                            (void)_useless;
                            ++it_template_param;
                        }
                    }
                    for (; it_template_param != it_template_param_end; ++it_template_param)
                        pending_template_params.push_back(*it_template_param);

                    entry_spcify_scope(symbol->m_belongs_to_scope);

                    template_function_deduction_extraction_with_complete_type(
                        lex,
                        static_cast<AstValueFunction*>(symbol->m_template_value_instances->m_origin_value_ast),
                        param_argument_types,
                        param_return_type,
                        pending_template_params,
                        &deduction_results);

                    end_last_scope();

                    if (deduction_results.size() == pending_template_params.size())
                    {
                        // We can decided this argument now.
                        std::list<lang_TypeInstance*> template_arguments;
                        for (wo_pstring_t param : pending_template_params)
                        {
                            template_arguments.push_back(deduction_results.at(param));
                        }

                        argument_variable->m_identifier->m_LANG_determined_and_appended_template_arguments
                            = template_arguments;

                        WO_CONTINUE_PROCESS(argument_variable);
                    }
                    else
                    {
                        lex.lang_error(lexer::errorlevel::error, argument,
                            WO_ERR_FAILED_TO_DEDUCE_TEMPLATE_TYPE);
                        return FAILED;
                    }

                    break;
                }
                default:
                    wo_error("Unknown argument type.");
                }

                node->m_arguments_tobe_deduct.pop_front();
                return HOLD;
            }
            default:
                wo_error("unknown hold state");
            }
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueFunctionCall)
    {
        // Huston, we have a problem, again.

        if (state == UNPROCESSED)
        {
            if (node->m_is_direct_call)
                // direct call(-> |> <|). first argument must be eval first;
                WO_CONTINUE_PROCESS(node->m_arguments.front());

            node->m_LANG_hold_state = AstValueFunctionCall::HOLD_FOR_FIRST_ARGUMENT_EVAL;
            return HOLD;
        }
        else if (state == HOLD)
        {
            switch (node->m_LANG_hold_state)
            {
            case AstValueFunctionCall::HOLD_FOR_FIRST_ARGUMENT_EVAL:
            {
                if (node->m_is_direct_call)
                {
                    lang_TypeInstance* first_argument_type_instance =
                        node->m_arguments.front()->m_LANG_determined_type.value();
                    lang_Symbol* first_argument_type_symbol =
                        first_argument_type_instance->m_symbol;

                    if (node->m_function->node_type == AstBase::AST_VALUE_VARIABLE)
                    {
                        AstValueVariable* function_variable = static_cast<AstValueVariable*>(node->m_function);
                        AstIdentifier* function_variable_identifier = function_variable->m_identifier;

                        if (first_argument_type_symbol->m_belongs_to_scope->is_namespace_scope())
                        {
                            if (function_variable_identifier->m_formal == AstIdentifier::FROM_CURRENT)
                            {
                                // Trying to find the function in type instance.
                                auto variable_scope_backup = function_variable_identifier->m_scope;

                                std::optional<lang_Namespace*> symbol_located_namespace =
                                    first_argument_type_symbol->m_belongs_to_scope->m_belongs_to_namespace;
                                function_variable_identifier->m_scope.push_front(first_argument_type_symbol->m_name);
                                for (;;)
                                {
                                    lang_Namespace* current_namespace = symbol_located_namespace.value();

                                    if (current_namespace->m_parent_namespace.has_value())
                                        symbol_located_namespace = current_namespace->m_parent_namespace;
                                    else
                                        break;
                                    function_variable_identifier->m_scope.push_front(current_namespace->m_name);
                                }

                                if (!find_symbol_in_current_scope(lex, function_variable_identifier))
                                    // Failed, restore the scope.
                                    function_variable_identifier->m_scope = variable_scope_backup;
                            }
                        }
                    }
                }
                switch (node->m_function->node_type)
                {
                case AstBase::AST_VALUE_VARIABLE:
                {
                    AstValueVariable* function_variable = static_cast<AstValueVariable*>(node->m_function);
                    AstIdentifier* function_variable_identifier = function_variable->m_identifier;

                    if (find_symbol_in_current_scope(lex, function_variable_identifier))
                    {
                        lang_Symbol* function_symbol = function_variable_identifier->m_LANG_determined_symbol.value();
                        if (function_symbol->m_symbol_kind == lang_Symbol::kind::VARIABLE
                            && function_symbol->m_is_template
                            && !function_symbol->m_template_value_instances->m_mutable
                            && function_symbol->m_template_value_instances->m_origin_value_ast->node_type == AstBase::AST_VALUE_FUNCTION
                            && (!function_variable_identifier->m_template_arguments.has_value()
                                || function_symbol->m_template_value_instances->m_template_params.size()
                                != function_variable_identifier->m_template_arguments.value().size()))
                        {
                            // Invoking function is a not complete template, defer type checking and do it later.
                            node->m_LANG_target_function_need_deduct_template_arguments = true;
                        }
                    }
                    break;
                }
                case AstBase::AST_VALUE_FUNCTION:
                {
                    AstValueFunction* function = static_cast<AstValueFunction*>(node->m_function);
                    if (function->m_pending_param_type_mark_template.has_value())
                        node->m_LANG_target_function_need_deduct_template_arguments = true;

                    break;
                }
                default:
                    // Do nothing.
                    break;
                }
                if (!node->m_LANG_target_function_need_deduct_template_arguments)
                {
                    // TARGET FUNCTION HAS NO TEMPLATE PARAM TO BE DEDUCT, EVAL IT NOW.
                    WO_CONTINUE_PROCESS(node->m_function);
                }
                else
                {
                    if (node->m_function->node_type == AstBase::AST_VALUE_VARIABLE)
                    {
                        // EVAL IT'S TEMPLATE ARGUMENTS.
                        AstValueVariable* function_variable = static_cast<AstValueVariable*>(node->m_function);
                        if (function_variable->m_identifier->m_template_arguments.has_value())
                            WO_CONTINUE_PROCESS_LIST(function_variable->m_identifier->m_template_arguments.value());
                    }
                }
                node->m_LANG_hold_state = AstValueFunctionCall::HOLD_FOR_FUNCTION_EVAL;
                return HOLD;
            }
            case AstValueFunctionCall::HOLD_FOR_FUNCTION_EVAL:
            {
                // Eval all arguments beside AstValueFunction or AstValueVariable which refer to
               // uncomplete template function.
                std::list<AstValueBase*> arguments;
                for (auto* argument_value : node->m_arguments)
                {
                    switch (argument_value->node_type)
                    {
                    case AstBase::AST_VALUE_FUNCTION:
                    {
                        AstValueFunction* argument_function = static_cast<AstValueFunction*>(argument_value);
                        if (argument_function->m_pending_param_type_mark_template.has_value())
                            // Skip template function.
                            continue;
                        break;
                    }
                    case AstBase::AST_VALUE_VARIABLE:
                    {
                        AstValueVariable* argument_variable = static_cast<AstValueVariable*>(argument_value);
                        if (find_symbol_in_current_scope(lex, argument_variable->m_identifier))
                        {
                            lang_Symbol* argument_symbol = argument_variable->m_identifier->m_LANG_determined_symbol.value();
                            if (argument_symbol->m_symbol_kind == lang_Symbol::kind::VARIABLE
                                && argument_symbol->m_is_template
                                && !argument_symbol->m_template_value_instances->m_mutable
                                && argument_symbol->m_template_value_instances->m_origin_value_ast->node_type == AstBase::AST_VALUE_FUNCTION
                                && (!argument_variable->m_identifier->m_template_arguments.has_value()
                                    || argument_symbol->m_template_value_instances->m_template_params.size()
                                    != argument_variable->m_identifier->m_template_arguments.value().size()))
                            {
                                // Skip uncomplete template variable.
                                continue;
                            }
                        }
                        break;
                    }
                    default:
                        break;
                    }
                    arguments.push_back(argument_value);
                }

                WO_CONTINUE_PROCESS_LIST(arguments);

                node->m_LANG_hold_state = AstValueFunctionCall::HOLD_FOR_ARGUMENTS_EVAL;
                return HOLD;
            }
            case AstValueFunctionCall::HOLD_FOR_ARGUMENTS_EVAL:
            {
                // If target function is a template function, we need to deduce template arguments.
                // HERE!
                if (node->m_LANG_target_function_need_deduct_template_arguments)
                {
                    std::unordered_map<wo_pstring_t, lang_TypeInstance*> deduction_results;

                    std::list<wo_pstring_t> pending_template_params;
                    std::list<AstTypeHolder*> target_param_holders;

                    bool entry_function_located_scope = false;

                    lang_Scope* target_function_scope;

                    switch (node->m_function->node_type)
                    {
                    case AstBase::AST_VALUE_VARIABLE:
                    {
                        AstValueVariable* function_variable = static_cast<AstValueVariable*>(node->m_function);
                        lang_Symbol* function_symbol = function_variable->m_identifier->m_LANG_determined_symbol.value();

                        target_function_scope = function_symbol->m_belongs_to_scope;

                        wo_assert(function_symbol->m_is_template
                            && function_symbol->m_symbol_kind == lang_Symbol::kind::VARIABLE);

                        auto it_template_param = function_symbol->m_template_value_instances->m_template_params.begin();
                        auto it_template_param_end = function_symbol->m_template_value_instances->m_template_params.end();
                        if (function_variable->m_identifier->m_template_arguments.has_value())
                        {
                            for (AstTypeHolder* template_arg : function_variable->m_identifier->m_template_arguments.value())
                            {
                                wo_pstring_t param_name = *(it_template_param++);

                                deduction_results.insert(
                                    std::make_pair(
                                        param_name, template_arg->m_LANG_determined_type.value()));
                            }
                        }
                        for (; it_template_param != it_template_param_end; ++it_template_param)
                            pending_template_params.push_back(*it_template_param);

                        // NOTE: node type has been checked in HOLD_FOR_FIRST_ARGUMENT_EVAL->HOLD_FOR_FUNCTION_EVAL
                        wo_assert(function_symbol->m_template_value_instances->m_origin_value_ast->node_type
                            == AstBase::AST_VALUE_FUNCTION);
                        AstValueFunction* origin_ast_value_function = static_cast<AstValueFunction*>(
                            function_symbol->m_template_value_instances->m_origin_value_ast);

                        for (auto* template_param_declare : origin_ast_value_function->m_parameters)
                            target_param_holders.push_back(template_param_declare->m_type.value());

                        // Entry function located scope, template_type_deduction_extraction_with_complete_type require todo so.
                        entry_function_located_scope = true;
                        entry_spcify_scope(function_symbol->m_belongs_to_scope);
                        break;
                    }
                    case AstBase::AST_VALUE_FUNCTION:
                    {
                        AstValueFunction* function = static_cast<AstValueFunction*>(node->m_function);

                        target_function_scope = get_current_scope();
                        pending_template_params = function->m_pending_param_type_mark_template.value();

                        for (auto* template_param_declare : function->m_parameters)
                            target_param_holders.push_back(template_param_declare->m_type.value());

                        break;
                    }
                    default:
                        wo_error("Unexpected template node type.");
                    }

                    auto it_target_param = target_param_holders.begin();
                    auto it_target_param_end = target_param_holders.end();
                    auto it_argument = node->m_arguments.begin();
                    auto it_argument_end = node->m_arguments.end();

                    for (; it_target_param != it_target_param_end
                        && it_argument != it_argument_end;
                        ++it_target_param, ++it_argument)
                    {
                        AstTypeHolder* param_type_holder = *it_target_param;
                        AstValueBase* argument_value = *it_argument;

                        if (!argument_value->m_LANG_determined_type.has_value())
                            // tobe determined, skip;
                            continue;

                        lang_TypeInstance* argument_type_instance =
                            argument_value->m_LANG_determined_type.value();

                        template_type_deduction_extraction_with_complete_type(
                            lex,
                            param_type_holder,
                            argument_type_instance,
                            pending_template_params,
                            &deduction_results);
                    }

                    if (entry_function_located_scope)
                        end_last_scope();

                    // NOTE: Some template arguments may not be deduced, it's okay.
                    //   for example:
                    //
                    //  func map<T, R>(a: array<T>, f: (T)=> R)
                    //
                    //   in this case, R will not able to be determined in first step.

                    entry_spcify_scope(target_function_scope);
                    begin_new_scope();

                    AstValueFunctionCall_FakeAstArgumentDeductionContextA* branch_a_context =
                        new AstValueFunctionCall_FakeAstArgumentDeductionContextA(get_current_scope());

                    node->m_LANG_branch_argument_deduction_context = branch_a_context;

                    branch_a_context->m_apply_template_argument_scope = get_current_scope();
                    branch_a_context->m_deduction_results = std::move(deduction_results);
                    branch_a_context->m_undetermined_template_params = std::move(pending_template_params);

                    end_last_scope();
                    end_last_scope();

                    it_target_param = target_param_holders.begin();
                    it_argument = node->m_arguments.begin();

                    for (; it_target_param != it_target_param_end
                        && it_argument != it_argument_end;
                        ++it_target_param, ++it_argument)
                    {
                        AstValueBase* argument_value = *it_argument;

                        if (argument_value->m_LANG_determined_type.has_value())
                            // has been determined, skip;
                            continue;

                        AstTypeHolder* param_type_holder =
                            static_cast<AstTypeHolder*>((*it_target_param)->clone());

                        branch_a_context->m_arguments_tobe_deduct.push_back(
                            AstValueFunctionCall_FakeAstArgumentDeductionContextA::ArgumentMatch{
                                argument_value, param_type_holder });
                    }

                    WO_CONTINUE_PROCESS(branch_a_context);
                    node->m_LANG_hold_state = AstValueFunctionCall::HOLD_BRANCH_A_TEMPLATE_ARGUMENT_DEDUCTION;
                }
                else
                {
                    AstValueFunctionCall_FakeAstArgumentDeductionContextB* branch_b_context =
                        new AstValueFunctionCall_FakeAstArgumentDeductionContextB();

                    lang_TypeInstance* target_function_type_instance =
                        node->m_function->m_LANG_determined_type.value();
                    auto target_function_type_instance_determined_base_type =
                        target_function_type_instance->get_determined_type();

                    if (!target_function_type_instance_determined_base_type.has_value())
                    {
                        lex.lang_error(lexer::errorlevel::error, node->m_function,
                            WO_ERR_FAILED_TO_DEDUCE_TEMPLATE_TYPE);
                        return FAILED;
                    }

                    auto* target_function_type_instance_determined_base_type_instance =
                        target_function_type_instance_determined_base_type.value();

                    if (target_function_type_instance_determined_base_type_instance->m_base_type
                        != lang_TypeInstance::DeterminedType::FUNCTION)
                    {
                        // TODO: More detail.
                        lex.lang_error(lexer::errorlevel::error, node->m_function,
                            WO_ERR_TARGET_TYPE_IS_NOT_A_FUNCTION,
                            get_type_name_w(target_function_type_instance));
                        return FAILED;
                    }

                    auto it_param_type = target_function_type_instance_determined_base_type_instance
                        ->m_external_type_description.m_function->m_param_types.begin();
                    auto it_param_type_end = target_function_type_instance_determined_base_type_instance
                        ->m_external_type_description.m_function->m_param_types.end();
                    auto it_argument = node->m_arguments.begin();
                    auto it_argument_end = node->m_arguments.end();

                    for (; it_param_type != it_param_type_end
                        && it_argument != it_argument_end;
                        ++it_param_type, ++it_argument)
                    {
                        lang_TypeInstance* param_type_instance = *it_param_type;
                        AstValueBase* argument_value = *it_argument;

                        if (argument_value->m_LANG_determined_type.has_value())
                            // has been determined, skip;
                            continue;

                        branch_b_context->m_arguments_tobe_deduct.push_back(
                            AstValueFunctionCall_FakeAstArgumentDeductionContextB::ArgumentMatch{
                                argument_value, param_type_instance });
                    }

                    node->m_LANG_branch_argument_deduction_context = branch_b_context;

                    // Function's parameters type has been determined, here will be more easier.
                    WO_CONTINUE_PROCESS(branch_b_context);
                    node->m_LANG_hold_state = AstValueFunctionCall::HOLD_BRANCH_B_TEMPLATE_ARGUMENT_DEDUCTION;
                }
                return HOLD;
            }
            case AstValueFunctionCall::HOLD_BRANCH_A_TEMPLATE_ARGUMENT_DEDUCTION:
            {
                // Template & arguments has been determined.
                // Make invoking function instance!.
                ///////////////////////////////////////////////////////////////////////////////
                // ATTENTION:
                //   Invoking function's type has not been checked until here, it might not a func
                ///////////////////////////////////////////////////////////////////////////////

                // Make instance!
                auto* branch_a_context = std::get<AstValueFunctionCall_FakeAstArgumentDeductionContextA*>(
                    node->m_LANG_branch_argument_deduction_context.value());

                switch (node->m_function->node_type)
                {
                case AstBase::AST_VALUE_FUNCTION:
                {
                    AstValueFunction* function = static_cast<AstValueFunction*>(node->m_function);
                    auto& pending_template_arguments = function->m_pending_param_type_mark_template.value();

                    std::list<lang_TypeInstance*> template_arguments;

                    for (wo_pstring_t param : pending_template_arguments)
                    {
                        auto fnd = branch_a_context->m_deduction_results.find(param);
                        if (fnd == branch_a_context->m_deduction_results.end())
                        {
                            lex.lang_error(lexer::errorlevel::error, function,
                                WO_ERR_NOT_ALL_TEMPLATE_ARGUMENT_DETERMINED);
                            return FAILED;
                        }
                        template_arguments.push_back(fnd->second);
                    }

                    function->m_LANG_determined_template_arguments = template_arguments;
                    function->m_LANG_in_template_reification_context = true;

                    WO_CONTINUE_PROCESS(function);
                    break;
                }
                case AstBase::AST_VALUE_VARIABLE:
                {
                    AstValueVariable* function_variable = static_cast<AstValueVariable*>(node->m_function);
                    lang_Symbol* symbol = function_variable->m_identifier->m_LANG_determined_symbol.value();

                    wo_assert(symbol->m_is_template
                        && symbol->m_symbol_kind == lang_Symbol::kind::VARIABLE
                        && !symbol->m_template_value_instances->m_mutable);

                    auto& template_instance_prefab = symbol->m_template_value_instances;

                    std::unordered_map<wo_pstring_t, lang_TypeInstance*> deduction_results;
                    auto it_template_param = symbol->m_template_value_instances->m_template_params.begin();
                    auto it_template_param_end = symbol->m_template_value_instances->m_template_params.end();
                    if (function_variable->m_identifier->m_template_arguments.has_value())
                    {
                        for (auto& _useless : function_variable->m_identifier->m_template_arguments.value())
                        {
                            (void)_useless;
                            ++it_template_param;
                        }
                    }
                    std::list<lang_TypeInstance*> template_argument_list;
                    for (; it_template_param != it_template_param_end; ++it_template_param)
                    {
                        wo_pstring_t param_name = *it_template_param;
                        auto fnd = branch_a_context->m_deduction_results.find(param_name);
                        if (fnd == branch_a_context->m_deduction_results.end())
                        {
                            lex.lang_error(lexer::errorlevel::error, function_variable,
                                WO_ERR_NOT_ALL_TEMPLATE_ARGUMENT_DETERMINED);
                            return FAILED;
                        }
                        template_argument_list.push_back(fnd->second);
                    }

                    function_variable->m_identifier->m_LANG_determined_and_appended_template_arguments
                        = template_argument_list;

                    WO_CONTINUE_PROCESS(function_variable);
                    break;
                }
                default:
                    wo_error("Unknown function type.");
                }

                node->m_LANG_hold_state = AstValueFunctionCall::HOLD_BRANCH_B_TEMPLATE_ARGUMENT_DEDUCTION;
                return HOLD;
            }
            case AstValueFunctionCall::HOLD_BRANCH_B_TEMPLATE_ARGUMENT_DEDUCTION:
            {
                // All argument & function has been determined.
                // Now we can do type checking.

                lang_TypeInstance* target_function_type_instance =
                    node->m_function->m_LANG_determined_type.value();

                auto target_function_type_instance_determined_base_type =
                    target_function_type_instance->get_determined_type();

                if (!target_function_type_instance_determined_base_type.has_value())
                {
                    lex.lang_error(lexer::errorlevel::error, node->m_function,
                        WO_ERR_TYPE_NAMED_DETERMINED_FAILED,
                        get_type_name_w(target_function_type_instance));
                    return FAILED;
                }

                auto* target_function_type_instance_determined_base_type_instance =
                    target_function_type_instance_determined_base_type.value();

                if (target_function_type_instance_determined_base_type_instance->m_base_type
                    != lang_TypeInstance::DeterminedType::FUNCTION)
                {
                    lex.lang_error(lexer::errorlevel::error, node->m_function,
                        WO_ERR_TARGET_TYPE_IS_NOT_A_FUNCTION,
                        get_type_name_w(target_function_type_instance));
                    return FAILED;
                }

                auto* target_function_type_instance_determined_base_type_function =
                    target_function_type_instance_determined_base_type_instance->m_external_type_description.m_function;
                auto& target_function_param_types =
                    target_function_type_instance_determined_base_type_function->m_param_types;

                std::list<std::pair<lang_TypeInstance*, AstValueBase*>> argument_types;
                bool expaned_array_or_vec = false;

                for (auto* argument_value : node->m_arguments)
                {
                    if (argument_value->node_type == AstBase::AST_FAKE_VALUE_UNPACK)
                    {
                        AstFakeValueUnpack* unpack = static_cast<AstFakeValueUnpack*>(argument_value);
                        auto* unpacked_value_determined_value =
                            unpack->m_unpack_value->m_LANG_determined_type.value()->get_determined_type().value();

                        switch (unpacked_value_determined_value->m_base_type)
                        {
                        case lang_TypeInstance::DeterminedType::ARRAY:
                        case lang_TypeInstance::DeterminedType::VECTOR:
                        {
                            expaned_array_or_vec = true;
                            const size_t elem_count_to_be_expand =
                                target_function_param_types.size() - argument_types.size();

                            unpack->m_LANG_need_to_be_unpack_count_FOR_RUNTIME_CHECK = elem_count_to_be_expand;
                            break;
                        }
                        case lang_TypeInstance::DeterminedType::TUPLE:
                        {
                            auto* tuple_determined_type =
                                unpacked_value_determined_value->m_external_type_description.m_tuple;

                            for (auto* type : tuple_determined_type->m_element_types)
                            {
                                argument_types.push_back(
                                    std::make_pair(type, unpack->m_unpack_value));
                            }

                            break;
                        }
                        default:
                            wo_error("Unexpected unpacked value type.");
                        }
                    }
                    else
                    {
                        lang_TypeInstance* argument_type_instance =
                            argument_value->m_LANG_determined_type.value();

                        argument_types.push_back(
                            std::make_pair(argument_type_instance, argument_value));
                    }
                }

                if (argument_types.size() < target_function_param_types.size()
                    && !expaned_array_or_vec)
                {
                    lex.lang_error(lexer::errorlevel::error, node,
                        WO_ERR_ARGUMENT_TOO_LESS);

                    return FAILED;
                }
                else if (argument_types.size() > target_function_param_types.size()
                    && !target_function_type_instance_determined_base_type_function->m_is_variadic)
                {
                    lex.lang_error(lexer::errorlevel::error, node,
                        WO_ERR_ARGUMENT_TOO_MUCH);

                    return FAILED;
                }

                auto it_param_type = target_function_param_types.begin();
                auto it_param_type_end = target_function_param_types.end();

                auto it_argument_type = argument_types.begin();
                auto it_argument_type_end = argument_types.end();

                for (; it_param_type != it_param_type_end
                    && it_argument_type != it_argument_type_end;
                    ++it_param_type, ++it_argument_type)
                {
                    auto& [type, arg_node] = *it_argument_type;
                    lang_TypeInstance* param_type = *it_param_type;

                    if (!is_type_accepted(lex, arg_node, param_type, type))
                    {
                        lex.lang_error(lexer::errorlevel::error, arg_node,
                            WO_ERR_CANNOT_ACCEPTABLE_TYPE_NAMED,
                            get_type_name_w(type),
                            get_type_name_w(param_type));

                        return FAILED;
                    }
                }

                // Finished, all job done!.
                node->m_LANG_determined_type =
                    target_function_type_instance_determined_base_type_function->m_return_type;

                break;
            }
            default:
                wo_error("Unknown hold state.");
            }
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstUnionDeclare)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_union_type_declare);
            return HOLD;
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueTypeid)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_id_type);
            return HOLD;
        }
        else if (state == HOLD)
        {
            lang_TypeInstance* id_type_instance = node->m_id_type->m_LANG_determined_type.value();
            size_t hash = (size_t)id_type_instance;

            wo::value cval;
            cval.set_integer(hash);
            node->decide_final_constant_value(cval);
            node->m_LANG_determined_type = m_origin_types.m_int.m_type_instance;
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueTypeCheckIs)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_check_type);
            WO_CONTINUE_PROCESS(node->m_check_value);

            return HOLD;
        }
        else if (state == HOLD)
        {
            lang_TypeInstance* value_type =
                node->m_check_value->m_LANG_determined_type.value();

            if (value_type == m_origin_types.m_dynamic.m_type_instance)
                ; // Dynamic type, do check in runtime.
            else
            {
                lang_TypeInstance* target_type =
                    node->m_check_type->m_LANG_determined_type.value();

                wo::value cval;
                cval.set_bool(is_type_accepted(
                    lex,
                    node,
                    target_type,
                    value_type));
                node->decide_final_constant_value(cval);
            }
            node->m_LANG_determined_type =
                m_origin_types.m_bool.m_type_instance;
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueTypeCheckAs)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_check_type);
            WO_CONTINUE_PROCESS(node->m_check_value);

            return HOLD;
        }
        else if (state == HOLD)
        {
            lang_TypeInstance* target_type =
                node->m_check_type->m_LANG_determined_type.value();

            lang_TypeInstance* value_type =
                node->m_check_value->m_LANG_determined_type.value();

            if (value_type == m_origin_types.m_dynamic.m_type_instance)
            {
                if (!check_cast_able(lex, node, target_type, value_type))
                {
                    lex.lang_error(lexer::errorlevel::error, node->m_check_type,
                        WO_ERR_CANNOT_CAST_TYPE_NAMED_FROM_DYNMAIC,
                        get_type_name_w(target_type));

                    return FAILED;
                }
            }
            else
            {
                if (!is_type_accepted(
                    lex,
                    node,
                    target_type,
                    value_type))
                {
                    lex.lang_error(lexer::errorlevel::error, node->m_check_value,
                        WO_ERR_CANNOT_ACCEPTABLE_TYPE_NAMED,
                        get_type_name_w(value_type),
                        get_type_name_w(target_type));
                    return FAILED;
                }

                if (node->m_check_value->m_evaled_const_value.has_value())
                    node->decide_final_constant_value(
                        node->m_check_value->m_evaled_const_value.value());
            }
            node->m_LANG_determined_type = target_type;
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstLabeled)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_body);
            return HOLD;
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValuePackedArgs)
    {
        wo_assert(state == UNPROCESSED);

        node->m_LANG_determined_type = m_origin_types.m_array_dynamic;

        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstStructFieldValuePair)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_value);
            return HOLD;
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueStruct)
    {
        if (state == UNPROCESSED)
        {
            if (node->m_marked_struct_type.has_value())
            {
                bool need_template_deduct = false;

                AstTypeHolder* struct_type = node->m_marked_struct_type.value();
                if (struct_type->m_formal == AstTypeHolder::IDENTIFIER)
                {
                    // Get it's symbol.
                    AstIdentifier* struct_type_identifier = struct_type->m_typeform.m_identifier;
                    auto finded_symbol = find_symbol_in_current_scope(lex, struct_type_identifier);
                    if (finded_symbol.has_value())
                    {
                        lang_Symbol* symbol = finded_symbol.value();
                        if (symbol->m_symbol_kind == lang_Symbol::kind::TYPE
                            && symbol->m_is_template
                            && symbol->m_template_type_instances->m_origin_value_ast->node_type == AstBase::AST_TYPE_HOLDER
                            && static_cast<AstTypeHolder*>(symbol->m_template_type_instances->m_origin_value_ast)->m_formal == AstTypeHolder::STRUCTURE
                            && (!struct_type_identifier->m_template_arguments.has_value()
                                || struct_type_identifier->m_template_arguments.value().size()
                                != symbol->m_template_type_instances->m_template_params.size()))
                        {
                            // This struct need template deduction.
                            need_template_deduct = true;
                        }
                    }
                }

                if (!need_template_deduct)
                {
                    WO_CONTINUE_PROCESS(struct_type);
                    node->m_LANG_hold_state = AstValueStruct::HOLD_FOR_STRUCT_TYPE_EVAL;
                }
                else
                {
                    // Walk through all members, but skip template member.
                    std::list<AstValueBase*> member_init_values;

                    for (auto* member_pair : node->m_fields)
                    {
                        switch (member_pair->m_value->node_type)
                        {
                        case AstBase::AST_VALUE_VARIABLE:
                        {
                            AstValueVariable* member_variable = static_cast<AstValueVariable*>(member_pair->m_value);
                            if (find_symbol_in_current_scope(lex, member_variable->m_identifier))
                            {
                                lang_Symbol* member_symbol = member_variable->m_identifier->m_LANG_determined_symbol.value();
                                if (member_symbol->m_symbol_kind == lang_Symbol::kind::VARIABLE
                                    && member_symbol->m_is_template
                                    && !member_symbol->m_template_value_instances->m_mutable
                                    && member_symbol->m_template_value_instances->m_origin_value_ast->node_type == AstBase::AST_VALUE_FUNCTION
                                    && (!member_variable->m_identifier->m_template_arguments.has_value()
                                        || member_symbol->m_template_value_instances->m_template_params.size()
                                        != member_variable->m_identifier->m_template_arguments.value().size()))
                                {
                                    // Skip uncomplete template variable.
                                    continue;
                                }
                            }
                            break;
                        }
                        case AstBase::AST_VALUE_FUNCTION:
                        {
                            AstValueFunction* member_function = static_cast<AstValueFunction*>(member_pair->m_value);
                            if (member_function->m_pending_param_type_mark_template.has_value())
                                // Skip template function.
                                continue;
                            break;
                        }
                        default:
                            break;
                        }

                        member_init_values.push_back(member_pair->m_value);
                    }
                    WO_CONTINUE_PROCESS_LIST(member_init_values);
                    node->m_LANG_hold_state = AstValueStruct::HOLD_FOR_EVAL_MEMBER_VALUE_BESIDE_TEMPLATE;
                }
            }
            else
            {
                // Make raw struct.
                WO_CONTINUE_PROCESS_LIST(node->m_fields);
                node->m_LANG_hold_state = AstValueStruct::HOLD_FOR_FIELD_EVAL;
            }
            return HOLD;
        }
        else if (state == HOLD)
        {
            switch (node->m_LANG_hold_state)
            {
            case AstValueStruct::HOLD_FOR_EVAL_MEMBER_VALUE_BESIDE_TEMPLATE:
            {
                // First deduct
                std::unordered_map<wo_pstring_t, lang_TypeInstance*> deduction_results;

                std::list<wo_pstring_t> pending_template_params;
                std::list<AstTypeHolder*> target_param_holders;

                wo_assert(node->m_marked_struct_type.value()->m_formal == AstTypeHolder::IDENTIFIER);
                AstTypeHolder* struct_type = node->m_marked_struct_type.value();
                AstIdentifier* struct_type_identifier = struct_type->m_typeform.m_identifier;

                lang_Symbol* symbol = struct_type_identifier->m_LANG_determined_symbol.value();
                auto& struct_type_def_prefab = symbol->m_template_type_instances;

                AstTypeHolder* struct_def_type_holder = struct_type_def_prefab->m_origin_value_ast;
                std::unordered_map<wo_pstring_t, lang_TypeInstance*> struct_template_deduction_results;

                TODO;;;
            }
            default:
                wo_error("Unknown hold state.");
            }
        }
    }

#undef WO_PASS_PROCESSER

#endif
}