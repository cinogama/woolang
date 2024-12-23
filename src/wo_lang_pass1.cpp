#include "wo_lang.hpp"

namespace wo
{
#ifndef WO_DISABLE_COMPILER
    using namespace ast;

    bool LangContext::update_pattern_symbol_variable_type_pass1(
        lexer& lex,
        ast::AstPatternBase* pattern,
        const std::optional<AstValueBase*>& init_value,
        lang_TypeInstance* init_value_type)
    {
        auto* determined_type = init_value_type->get_determined_type();

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

                lang_symbol->m_value_instance->m_determined_type = init_value_type;

                // NOTE: Donot decide constant value for mutable variable.
                if (init_value && !lang_symbol->m_value_instance->m_mutable)
                {
                    AstValueBase* value = init_value.value();
                    if (value->m_evaled_const_value)
                    {
                        lang_symbol->m_value_instance->m_determined_constant = wo::value();
                        lang_symbol->m_value_instance->m_determined_constant->set_val_compile_time(
                            &value->m_evaled_const_value.value());
                    }
                }
            }
            return true;
        }
        case AstBase::AST_PATTERN_TUPLE:
        {
            AstPatternTuple* tuple_pattern = static_cast<AstPatternTuple*>(pattern);

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
            begin_new_scope();

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
            if (!begin_new_namespace(node->m_name))
            {
                lex.lang_error(lexer::errorlevel::error, node, WO_ERR_CANNOT_START_NAMESPACE);
                return FAILED;
            }

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
                                node->m_LANG_determined_type = type_symbol->m_alias_instance->m_determined_type.value();
                            else
                            {
                                wo_assert(type_symbol->m_symbol_kind == lang_Symbol::TYPE);
                                node->m_LANG_determined_type = type_symbol->m_type_instance;
                            }
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
                    // TODO: template variable
                    wo_error("template variable not implemented");
                else
                {
                    lang_ValueInstance* value_instance = var_symbol->m_value_instance;

                    // Type has been determined.
                    if (value_instance->m_determined_type)
                        node->m_LANG_determined_type = value_instance->m_determined_type.value();
                    else
                        // Type determined failed in AstVariableDefines, treat as failed.
                        return FAILED;

                    if (value_instance->m_determined_constant)
                        // Constant has been determined.
                        node->decide_final_constant_value(
                            value_instance->m_determined_constant.value());
                }
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
                        defines->m_init_value->m_LANG_determined_type.value());
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

#undef WO_PASS_PROCESSER

#endif
}