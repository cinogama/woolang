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
        WO_LANG_REGISTER_PROCESSER(AstIdentifier, AstBase::AST_IDENTIFIER, pass1);
        // WO_LANG_REGISTER_PROCESSER(AstStructFieldDefine, AstBase::AST_PATTERN_SINGLE, pass1);
        WO_LANG_REGISTER_PROCESSER(AstTypeHolder, AstBase::AST_TYPE_HOLDER, pass1);
        // WO_LANG_REGISTER_PROCESSER(AstValueMarkAsMutable, AstBase::AST_VALUE_MARK_AS_MUTABLE, pass1);
        // WO_LANG_REGISTER_PROCESSER(AstValueMarkAsImmutable, AstBase::AST_VALUE_MARK_AS_IMMUTABLE, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueLiteral, AstBase::AST_VALUE_LITERAL, pass1);
        // WO_LANG_REGISTER_PROCESSER(AstValueTypeid, AstBase::AST_VALUE_TYPEID, pass1);
        // WO_LANG_REGISTER_PROCESSER(AstValueTypeCast, AstBase::AST_VALUE_TYPE_CAST, pass1);
        // WO_LANG_REGISTER_PROCESSER(AstValueTypeCheckIs, AstBase::AST_VALUE_TYPE_CHECK_IS, pass1);
        // WO_LANG_REGISTER_PROCESSER(AstValueTypeCheckAs, AstBase::AST_VALUE_TYPE_CHECK_AS, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueVariable, AstBase::AST_VALUE_VARIABLE, pass1);
        WO_LANG_REGISTER_PROCESSER(AstWhereConstraints, AstBase::AST_WHERE_CONSTRAINTS, pass1);
        // WO_LANG_REGISTER_PROCESSER(AstValueFunctionCall, AstBase::AST_VALUE_FUNCTION_CALL, pass1);
        // WO_LANG_REGISTER_PROCESSER(AstValueBinaryOperator, AstBase::AST_VALUE_BINARY_OPERATOR, pass1);
        // WO_LANG_REGISTER_PROCESSER(AstValueUnaryOperator, AstBase::AST_VALUE_UNARY_OPERATOR, pass1);
        // WO_LANG_REGISTER_PROCESSER(AstValueTribleOperator, AstBase::AST_VALUE_TRIPLE_OPERATOR, pass1);
        // WO_LANG_REGISTER_PROCESSER(AstFakeValueUnpack, AstBase::AST_FAKE_VALUE_UNPACK, pass1);
        // WO_LANG_REGISTER_PROCESSER(AstValueVariadicArgumentsPack, AstBase::AST_VALUE_VARIADIC_ARGUMENTS_PACK, pass1);
        // WO_LANG_REGISTER_PROCESSER(AstValueIndex, AstBase::AST_VALUE_INDEX, pass1);
        // WO_LANG_REGISTER_PROCESSER(AstPatternVariable, AstBase::AST_PATTERN_VARIABLE, pass1);
        // WO_LANG_REGISTER_PROCESSER(AstPatternIndex, AstBase::AST_PATTERN_INDEX, pass1);
        WO_LANG_REGISTER_PROCESSER(AstVariableDefineItem, AstBase::AST_VARIABLE_DEFINE_ITEM, pass1);
        WO_LANG_REGISTER_PROCESSER(AstVariableDefines, AstBase::AST_VARIABLE_DEFINES, pass1);
        WO_LANG_REGISTER_PROCESSER(AstFunctionParameterDeclare, AstBase::AST_FUNCTION_PARAMETER_DECLARE, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueFunction, AstBase::AST_VALUE_FUNCTION, pass1);
        // WO_LANG_REGISTER_PROCESSER(AstValueArrayOrVec, AstBase::AST_VALUE_ARRAY_OR_VEC, pass1);
        // WO_LANG_REGISTER_PROCESSER(AstKeyValuePair, AstBase::AST_KEY_VALUE_PAIR, pass1);
        // WO_LANG_REGISTER_PROCESSER(AstValueDictOrMap, AstBase::AST_VALUE_DICT_OR_MAP, pass1);
        // WO_LANG_REGISTER_PROCESSER(AstValueTuple, AstBase::AST_VALUE_TUPLE, pass1);
        // WO_LANG_REGISTER_PROCESSER(AstStructFieldValuePair, AstBase::AST_STRUCT_FIELD_VALUE_PAIR, pass1);
        // WO_LANG_REGISTER_PROCESSER(AstValueStruct, AstBase::AST_VALUE_STRUCT, pass1);
        // WO_LANG_REGISTER_PROCESSER(AstValueAssign, AstBase::AST_VALUE_ASSIGN, pass1);
        // WO_LANG_REGISTER_PROCESSER(AstValuePackedArgs, AstBase::AST_VALUE_PACKED_ARGS, pass1);
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
        // WO_LANG_REGISTER_PROCESSER(AstReturn, AstBase::AST_RETURN, pass1);
        // WO_LANG_REGISTER_PROCESSER(AstLabeled, AstBase::AST_LABELED, pass1);
        WO_LANG_REGISTER_PROCESSER(AstUsingTypeDeclare, AstBase::AST_USING_TYPE_DECLARE, pass1);
        WO_LANG_REGISTER_PROCESSER(AstAliasTypeDeclare, AstBase::AST_ALIAS_TYPE_DECLARE, pass1);
        // WO_LANG_REGISTER_PROCESSER(AstEnumDeclare, AstBase::AST_ENUM_DECLARE, pass1);
        // WO_LANG_REGISTER_PROCESSER(AstUnionDeclare, AstBase::AST_UNION_DECLARE, pass1);
        // WO_LANG_REGISTER_PROCESSER(AstValueMakeUnion, AstBase::AST_VALUE_MAKE_UNION, pass1);
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
        if (node->m_LANG_trying_advancing_type_judgement)
            end_last_scope(); // Leave temporary advance the processing of declaration nodes.

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
                                for (auto* typeholder : node->m_identifier->m_template_arguments.value())
                                {
                                    wo_assert(typeholder->m_LANG_determined_type);
                                    template_args.push_back(typeholder->m_LANG_determined_type.value());
                                }

                                auto template_eval_state_instance_may_nullopt = begin_eval_template_ast(
                                    lex, node, type_symbol, template_args, out_stack);

                                if (!template_eval_state_instance_may_nullopt)
                                    return FAILED;

                                auto* template_eval_state_instance = template_eval_state_instance_may_nullopt.value();

                                switch (template_eval_state_instance->m_state)
                                {
                                case lang_TemplateAstEvalStateValue::EVALUATING:
                                    node->m_LANG_template_evalating_state = template_eval_state_instance;
                                    return HOLD;
                                case lang_TemplateAstEvalStateValue::EVALUATED:
                                {
                                    if (type_symbol->m_symbol_kind == lang_Symbol::ALIAS)
                                        alias_instance = static_cast<lang_TemplateAstEvalStateAlias*>(
                                            template_eval_state_instance)->m_alias_instance.value().get();
                                    else
                                        type_instance = static_cast<lang_TemplateAstEvalStateType*>(
                                            template_eval_state_instance)->m_type_instance.value().get();

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
                                {
                                    alias_instance = type_symbol->m_alias_instance;

                                    //if (type_symbol->m_alias_instance->m_determined_type)
                                    //    /*&& type_symbol->m_alias_instance->m_determined_type.value()->m_determined_type*/
                                    //    node->m_LANG_determined_type = type_symbol->m_alias_instance->m_determined_type.value();
                                    //else
                                    //    goto _label_depend_type_unjudged;
                                }
                                else
                                {
                                    type_instance = type_symbol->m_type_instance;

                                    //wo_assert(type_symbol->m_symbol_kind == lang_Symbol::TYPE);
                                    //if (type_symbol->m_type_instance->m_determined_type)
                                    //    node->m_LANG_determined_type = type_symbol->m_type_instance;
                                    //else
                                    //    goto _label_depend_type_unjudged;
                                }
                            }

                            bool type_determined = false;
                            if (type_symbol->m_symbol_kind == lang_Symbol::ALIAS)
                            {
                                type_determined = 
                                    alias_instance->m_determined_type 
                                    && alias_instance->m_determined_type.value()->m_determined_type.has_value();
                            }
                            else
                            {
                                type_determined = 
                                    type_instance->m_determined_type.has_value();
                            }

                            if (!type_determined)
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
                        else
                        {
                            auto* state = static_cast<lang_TemplateAstEvalStateBase*>(
                                node->m_LANG_template_evalating_state.value());

                            finish_eval_template_ast(state);

                            if (type_symbol->m_symbol_kind == lang_Symbol::ALIAS)
                                alias_instance = static_cast<lang_TemplateAstEvalStateAlias*>(
                                    state)->m_alias_instance.value().get();
                            else
                                type_instance = static_cast<lang_TemplateAstEvalStateType*>(
                                    state)->m_type_instance.value().get();
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
                            for (auto* typeholder : node->m_identifier->m_template_arguments.value())
                            {
                                wo_assert(typeholder->m_LANG_determined_type);
                                template_args.push_back(typeholder->m_LANG_determined_type.value());
                            }

                            auto template_eval_state_instance_may_nullopt = begin_eval_template_ast(
                                lex, node, var_symbol, template_args, out_stack);

                            if (!template_eval_state_instance_may_nullopt)
                                return FAILED;

                            auto* template_eval_state_instance = template_eval_state_instance_may_nullopt.value();

                            switch (template_eval_state_instance->m_state)
                            {
                            case lang_TemplateAstEvalStateValue::EVALUATING:
                                node->m_LANG_template_evalating_state = template_eval_state_instance;
                                return HOLD;
                            case lang_TemplateAstEvalStateValue::EVALUATED:
                                value_instance = static_cast<lang_TemplateAstEvalStateValue*>(
                                    template_eval_state_instance)->m_value_instance.value().get();
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

                    value_instance = state->m_value_instance.value().get();
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
                    node,
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
                    symbol->m_type_instance->determine_base_type(
                        node->m_type->m_LANG_determined_type.value()->get_determined_type());
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

                if (! constraint->m_evaled_const_value.value().integer)
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
        // Huston, we have a problem.
        if (state == UNPROCESSED)
        {
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
                    node->m_LANG_hold_state= AstValueFunction::HOLD_FOR_RETURN_TYPE_EVAL;
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
               TODO: // Consider about how to eval recursive funciton call.

                node->m_LANG_hold_state = AstValueFunction::HOLD_FOR_BODY_EVAL;
                WO_CONTINUE_PROCESS(node->m_body);
                return HOLD;
            }
            case AstValueFunction::HOLD_FOR_BODY_EVAL:
            {
                end_last_function();
                break;
            }
            default:
                wo_error("unknown hold state");
                break;
            }
        }
        else
        {
            end_last_function();
        }

    }

#undef WO_PASS_PROCESSER

#endif
}