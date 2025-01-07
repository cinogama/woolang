#include "wo_lang.hpp"

WO_API wo_api rslib_std_bad_function(wo_vm vm, wo_value args);

namespace wo
{
#ifndef WO_DISABLE_COMPILER
    using namespace ast;

#define WO_OPNUM(opnumptr) (*static_cast<opnum::opnumbase*>(opnumptr))

    opnum::opnumbase* LangContext::IR_function_opnum(AstValueFunction* func)
    {
        if (func->m_IR_extern_information.has_value())
        {
            auto* extern_function_instance =
                func->m_IR_extern_information.value()->m_IR_externed_function.value();

            return m_ircontext.opnum_imm_handle(
                (wo_handle_t)(intptr_t)(void*)extern_function_instance);
        }

        char result[48];
        sprintf(result, "#func_%p_begin", func);

        return m_ircontext.opnum_imm_rsfunc(result);
    }

    bool LangContext::update_allocate_instance_storage_passir(
        lexer& lex,
        lang_ValueInstance* instance)
    {
        // Instance must not have storage.
        wo_assert(!instance->m_IR_storage.has_value());

        lang_Symbol* symbol = instance->m_symbol;
        if (symbol->m_is_global ||
            (symbol->m_declare_attribute.has_value()
                && symbol->m_declare_attribute.value()->m_lifecycle.has_value()
                && symbol->m_declare_attribute.value()->m_lifecycle.value() == AstDeclareAttribue::lifecycle_attrib::STATIC))
        {
            // Global or staitc, allocate global storage.
            instance->m_IR_storage = lang_ValueInstance::Storage{
                lang_ValueInstance::lang_ValueInstance::Storage::GLOBAL,
                m_ircontext.m_global_storage_allocating++
            };
        }
        else
        {
            wo_error("TODO;");
        }
        return true;
    }

    bool LangContext::update_instance_storage_and_code_gen_passir(
        lexer& lex,
        lang_ValueInstance* instance,
        opnum::opnumbase* opnumval,
        const std::optional<uint16_t>& tuple_member_offset)
    {
        if (!update_allocate_instance_storage_passir(lex, instance))
            return false;

        auto* target_storage =
            m_ircontext.get_storage_place(instance->m_IR_storage.value());

        if (tuple_member_offset.has_value())
        {
            uint16_t index = tuple_member_offset.value();
            m_ircontext.c().idstruct(WO_OPNUM(target_storage), WO_OPNUM(opnumval), index);
        }
        else
            m_ircontext.c().mov(WO_OPNUM(target_storage), WO_OPNUM(opnumval));
        return true;
    }
    bool LangContext::update_pattern_storage_and_code_gen_passir(
        lexer& lex,
        ast::AstPatternBase* pattern,
        opnum::opnumbase* opnumval,
        const std::optional<uint16_t>& tuple_member_offset)
    {
        switch (pattern->node_type)
        {
        case AstBase::AST_PATTERN_SINGLE:
        {
            AstPatternSingle* pattern_single = static_cast<AstPatternSingle*>(pattern);
            lang_Symbol* pattern_symbol = pattern_single->m_LANG_declared_symbol.value();

            wo_assert(!pattern_symbol->m_is_template
                && pattern_symbol->m_symbol_kind == lang_Symbol::kind::VARIABLE);

            lang_ValueInstance* pattern_value_instance = pattern_symbol->m_value_instance;

            return update_instance_storage_and_code_gen_passir(
                lex, pattern_value_instance, opnumval, tuple_member_offset);
        }
        case AstBase::AST_PATTERN_TUPLE:
        {
            AstPatternTuple* pattern_tuple = static_cast<AstPatternTuple*>(pattern);

            opnum::opnumbase* tuple_source;
            if (tuple_member_offset.has_value())
            {
                uint16_t index = tuple_member_offset.value();
                auto borrowed_reg = m_ircontext.borrow_opnum_temporary_register(lex, pattern_tuple);
                if (!borrowed_reg.has_value())
                    return false;

                tuple_source = borrowed_reg.value();
                m_ircontext.c().idstruct(WO_OPNUM(tuple_source), WO_OPNUM(opnumval), index);
            }
            else
                tuple_source = opnumval;

            bool match_pattern_result = true;
            uint16_t struct_offset = 0;
            for (auto* sub_pattern : pattern_tuple->m_fields)
            {
                if (!update_pattern_storage_and_code_gen_passir(
                    lex, sub_pattern, tuple_source, struct_offset))
                {
                    match_pattern_result = false;
                    break;
                }

                struct_offset++;
            }

            if (tuple_member_offset.has_value())
                m_ircontext.try_return_opnum_temporary_register(tuple_source);

            return match_pattern_result;
        }
        case AstBase::AST_PATTERN_TAKEPLACE:
        {
            // Nothing todo.
            return true;
        }
        default:
            wo_error("Unknown pattern type.");
            return false;
        }
    }

    void LangContext::init_passir()
    {
        WO_LANG_REGISTER_PROCESSER(AstList, AstBase::AST_LIST, passir_A);
        // WO_LANG_REGISTER_PROCESSER(AstIdentifier, AstBase::AST_IDENTIFIER, passir_A);
        // WO_LANG_REGISTER_PROCESSER(AstStructFieldDefine, AstBase::AST_STRUCT_FIELD_DEFINE, passir_A);
        // WO_LANG_REGISTER_PROCESSER(AstTypeHolder, AstBase::AST_TYPE_HOLDER, passir_A);
        // WO_LANG_REGISTER_PROCESSER(AstWhereConstraints, AstBase::AST_WHERE_CONSTRAINTS, passir_A);
        // WO_LANG_REGISTER_PROCESSER(AstValueFunctionCall_FakeAstArgumentDeductionContextA,
        //    AstBase::AST_VALUE_FUNCTION_CALL_FAKE_AST_ARGUMENT_DEDUCTION_CONTEXT_A, passir_A);
        // WO_LANG_REGISTER_PROCESSER(AstValueFunctionCall_FakeAstArgumentDeductionContextB,
        //    AstBase::AST_VALUE_FUNCTION_CALL_FAKE_AST_ARGUMENT_DEDUCTION_CONTEXT_B, passir_A);
        // WO_LANG_REGISTER_PROCESSER(AstPatternVariable, AstBase::AST_PATTERN_VARIABLE, passir_A);
        // WO_LANG_REGISTER_PROCESSER(AstPatternIndex, AstBase::AST_PATTERN_INDEX, passir_A);
        WO_LANG_REGISTER_PROCESSER(AstVariableDefineItem, AstBase::AST_VARIABLE_DEFINE_ITEM, passir_A);
        WO_LANG_REGISTER_PROCESSER(AstVariableDefines, AstBase::AST_VARIABLE_DEFINES, passir_A);
        // WO_LANG_REGISTER_PROCESSER(AstFunctionParameterDeclare, AstBase::AST_FUNCTION_PARAMETER_DECLARE, passir_A);
        // WO_LANG_REGISTER_PROCESSER(AstKeyValuePair, AstBase::AST_KEY_VALUE_PAIR, passir_A);
        // WO_LANG_REGISTER_PROCESSER(AstStructFieldValuePair, AstBase::AST_STRUCT_FIELD_VALUE_PAIR, passir_A);
        // WO_LANG_REGISTER_PROCESSER(AstNamespace, AstBase::AST_NAMESPACE, passir_A);
        // WO_LANG_REGISTER_PROCESSER(AstScope, AstBase::AST_SCOPE, passir_A);
        // WO_LANG_REGISTER_PROCESSER(AstMatchCase, AstBase::AST_MATCH_CASE, passir_A);
        // WO_LANG_REGISTER_PROCESSER(AstMatch, AstBase::AST_MATCH, passir_A);
        // WO_LANG_REGISTER_PROCESSER(AstIf, AstBase::AST_IF, passir_A);
        // WO_LANG_REGISTER_PROCESSER(AstWhile, AstBase::AST_WHILE, passir_A);
        // WO_LANG_REGISTER_PROCESSER(AstFor, AstBase::AST_FOR, passir_A);
        // WO_LANG_REGISTER_PROCESSER(AstForeach, AstBase::AST_FOREACH, passir_A);
        // WO_LANG_REGISTER_PROCESSER(AstBreak, AstBase::AST_BREAK, passir_A);
        // WO_LANG_REGISTER_PROCESSER(AstContinue, AstBase::AST_CONTINUE, passir_A);
        // WO_LANG_REGISTER_PROCESSER(AstReturn, AstBase::AST_RETURN, passir_A);
        // WO_LANG_REGISTER_PROCESSER(AstLabeled, AstBase::AST_LABELED, passir_A);
        // WO_LANG_REGISTER_PROCESSER(AstUsingTypeDeclare, AstBase::AST_USING_TYPE_DECLARE, passir_A);
        // WO_LANG_REGISTER_PROCESSER(AstAliasTypeDeclare, AstBase::AST_ALIAS_TYPE_DECLARE, passir_A);
        // WO_LANG_REGISTER_PROCESSER(AstEnumDeclare, AstBase::AST_ENUM_DECLARE, passir_A);
        // WO_LANG_REGISTER_PROCESSER(AstUnionDeclare, AstBase::AST_UNION_DECLARE, passir_A);
        // WO_LANG_REGISTER_PROCESSER(AstUsingNamespace, AstBase::AST_USING_NAMESPACE, passir_A);
        // WO_LANG_REGISTER_PROCESSER(AstExternInformation, AstBase::AST_EXTERN_INFORMATION, passir_A);
        WO_LANG_REGISTER_PROCESSER(AstNop, AstBase::AST_EXTERN_INFORMATION, passir_A);

        // WO_LANG_REGISTER_PROCESSER(AstValueMarkAsMutable, AstBase::AST_VALUE_MARK_AS_MUTABLE, passir_B);
        // WO_LANG_REGISTER_PROCESSER(AstValueMarkAsImmutable, AstBase::AST_VALUE_MARK_AS_IMMUTABLE, passir_B);
        WO_LANG_REGISTER_PROCESSER(AstValueLiteral, AstBase::AST_VALUE_LITERAL, passir_B);
        WO_LANG_REGISTER_PROCESSER(AstValueTypeid, AstBase::AST_VALUE_TYPEID, passir_B);
        WO_LANG_REGISTER_PROCESSER(AstValueTypeCast, AstBase::AST_VALUE_TYPE_CAST, passir_B);
        WO_LANG_REGISTER_PROCESSER(AstValueTypeCheckIs, AstBase::AST_VALUE_TYPE_CHECK_IS, passir_B);
        // WO_LANG_REGISTER_PROCESSER(AstValueTypeCheckAs, AstBase::AST_VALUE_TYPE_CHECK_AS, passir_B);
        WO_LANG_REGISTER_PROCESSER(AstValueVariable, AstBase::AST_VALUE_VARIABLE, passir_B);
        // WO_LANG_REGISTER_PROCESSER(AstValueFunctionCall, AstBase::AST_VALUE_FUNCTION_CALL, passir_B);
        // WO_LANG_REGISTER_PROCESSER(AstValueBinaryOperator, AstBase::AST_VALUE_BINARY_OPERATOR, passir_B);
        // WO_LANG_REGISTER_PROCESSER(AstValueUnaryOperator, AstBase::AST_VALUE_UNARY_OPERATOR, passir_B);
        // WO_LANG_REGISTER_PROCESSER(AstValueTribleOperator, AstBase::AST_VALUE_TRIBLE_OPERATOR, passir_B);
        // WO_LANG_REGISTER_PROCESSER(AstFakeValueUnpack, AstBase::AST_FAKE_VALUE_UNPACK, passir_B);
        // WO_LANG_REGISTER_PROCESSER(AstValueVariadicArgumentsPack, AstBase::AST_VALUE_VARIADIC_ARGUMENTS_PACK, passir_B);
        // WO_LANG_REGISTER_PROCESSER(AstValueIndex, AstBase::AST_VALUE_INDEX, passir_B);
        WO_LANG_REGISTER_PROCESSER(AstValueFunction, AstBase::AST_VALUE_FUNCTION, passir_B);
        // WO_LANG_REGISTER_PROCESSER(AstValueArrayOrVec, AstBase::AST_VALUE_ARRAY_OR_VEC, passir_B);
        // WO_LANG_REGISTER_PROCESSER(AstValueDictOrMap, AstBase::AST_VALUE_DICT_OR_MAP, passir_B);
        WO_LANG_REGISTER_PROCESSER(AstValueTuple, AstBase::AST_VALUE_TUPLE, passir_B);
        // WO_LANG_REGISTER_PROCESSER(AstValueStruct, AstBase::AST_VALUE_STRUCT, passir_B);
        // WO_LANG_REGISTER_PROCESSER(AstValueAssign, AstBase::AST_VALUE_ASSIGN, passir_B);
        // WO_LANG_REGISTER_PROCESSER(AstValuePackedArgs, AstBase::AST_VALUE_PACKED_ARGS, passir_B);
        // WO_LANG_REGISTER_PROCESSER(AstValueMakeUnion, AstBase::AST_VALUE_MAKE_UNION, passir_B);
    }

#define WO_PASS_PROCESSER(AST) WO_PASS_PROCESSER_IMPL(AST, passir_A)
    WO_PASS_PROCESSER(AstList)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS_LIST(node->m_list);
            return HOLD;
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstVariableDefines)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS_LIST(node->m_definitions);
            return HOLD;
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstVariableDefineItem)
    {
        wo_assert(state == UNPROCESSED);

        if (node->m_pattern->node_type == AstBase::AST_PATTERN_SINGLE)
        {
            // Might be constant, template.
            AstPatternSingle* pattern_single = static_cast<AstPatternSingle*>(node->m_pattern);
            lang_Symbol* pattern_symbol = pattern_single->m_LANG_declared_symbol.value();
            wo_assert(pattern_symbol->m_symbol_kind == lang_Symbol::kind::VARIABLE);

            if (pattern_symbol->m_is_template)
            {
                // Is template, walk through all the template instance;
                for (auto& [_useless, template_instance] :
                    pattern_symbol->m_template_value_instances->m_template_instances)
                {
                    (void)_useless;

                    lang_ValueInstance* template_value_instance = template_instance->m_value_instance.get();
                    if (!template_value_instance->IR_need_storage())
                    {
                        // No need storage.
                        auto** function = std::get_if<AstValueFunction*>(
                            &template_value_instance->m_determined_constant_or_function.value());

                        if (function != nullptr)
                        {
                            template_value_instance->m_IR_normal_function = *function;
                            (*function)->m_IR_marked_function_name = get_value_name_w(template_value_instance);

                            // We still eval the function to let compiler know the function.
                            m_ircontext.eval_ignore();
                            if (!pass_final_value(lex, *function))
                                // Failed 
                                return FAILED;
                        }
                    }

                    // Need storage and initialize.
                    m_ircontext.eval();
                    if (!pass_final_value(lex, static_cast<AstValueBase*>(template_instance->m_ast)))
                        // Failed 
                        return FAILED;

                    auto* result_opnum = m_ircontext.get_eval_result();

                    if (!update_instance_storage_and_code_gen_passir(
                        lex, template_value_instance, result_opnum, std::nullopt))
                        return FAILED;
                }
            }
            else if (!pattern_symbol->m_value_instance->IR_need_storage())
            {
                // No need storage.
                auto** function = std::get_if<AstValueFunction*>(
                    &pattern_symbol->m_value_instance->m_determined_constant_or_function.value());

                if (function != nullptr)
                {
                    pattern_symbol->m_value_instance->m_IR_normal_function = *function;
                    (*function)->m_IR_marked_function_name = get_value_name_w(pattern_symbol->m_value_instance);

                    m_ircontext.eval_ignore();
                    if (!pass_final_value(lex, *function))
                        // Failed 
                        return FAILED;
                }
            }
            else
            {
                // Not template, but need storage.

                // NOTE: 
                m_ircontext.eval();
                if (!pass_final_value(lex, node->m_init_value))
                    // Failed 
                    return FAILED;

                auto* result_opnum = m_ircontext.get_eval_result();

                if (!update_instance_storage_and_code_gen_passir(
                    lex, pattern_symbol->m_value_instance, result_opnum, std::nullopt))
                    return FAILED;
            }
            return OKAY;
        }
        else if (node->m_pattern->node_type == AstBase::AST_PATTERN_TAKEPLACE)
        {
            m_ircontext.eval_ignore();
            if (!pass_final_value(lex, node->m_init_value))
                // Failed 
                return FAILED;

            return OKAY;
        }

        // Other pattern type.
        m_ircontext.eval_keep();
        if (!pass_final_value(lex, node->m_init_value))
            // Failed 
            return FAILED;

        auto* result_opnum = m_ircontext.get_eval_result();

        bool update_result = update_pattern_storage_and_code_gen_passir(
            lex, node->m_pattern, result_opnum, std::nullopt);

        m_ircontext.try_return_opnum_temporary_register(result_opnum);

        if (!update_result)
            return FAILED;

        return OKAY;
    }
    WO_PASS_PROCESSER(AstNop)
    {
        wo_assert(state == UNPROCESSED);

        m_ircontext.c().nop();

        return OKAY;
    }
#undef WO_PASS_PROCESSER
#define WO_PASS_PROCESSER(AST) WO_PASS_PROCESSER_IMPL(AST, passir_B)
    WO_PASS_PROCESSER(AstValueLiteral)
    {
        wo_error("Should not be here.");
        return OKAY;
    }
    WO_PASS_PROCESSER(AstValueTypeid)
    {
        wo_error("Should not be here.");
        return OKAY;
    }
    WO_PASS_PROCESSER(AstValueFunction)
    {
        wo_assert(state == UNPROCESSED);
        if (node->m_body->node_type == AstBase::AST_EXTERN_INFORMATION)
        {
            AstExternInformation* extern_info = static_cast<AstExternInformation*>(node->m_body);
            wo_native_func_t extern_function;
            if (extern_info->m_extern_from_library.has_value())
            {
                extern_function =
                    rslib_extern_symbols::get_lib_symbol(
                        wstr_to_str(*node->source_location.source_file).c_str(),
                        wstr_to_str(*extern_info->m_extern_from_library.value()).c_str(),
                        wstr_to_str(*extern_info->m_extern_symbol).c_str(),
                        m_ircontext.m_extern_libs);
            }
            else
            {
                extern_function =
                    rslib_extern_symbols::get_global_symbol(
                        wstr_to_str(*extern_info->m_extern_symbol).c_str());
            }

            if (extern_function != nullptr)
                extern_info->m_IR_externed_function = extern_function;
            else
            {
                if (config::ENABLE_IGNORE_NOT_FOUND_EXTERN_SYMBOL)
                    extern_info->m_IR_externed_function = rslib_std_bad_function;
                else
                {
                    lex.lang_error(lexer::errorlevel::error, node,
                        WO_ERR_UNABLE_TO_FIND_EXTERN_FUNCTION,
                        extern_info->m_extern_from_library.value_or(WO_PSTR(woolang))->c_str(),
                        extern_info->m_extern_symbol->c_str());

                    return FAILED;
                }
            }

            node->m_IR_extern_information = extern_info;
        }
        else
        {
            // Record it!
            m_ircontext.m_being_used_function_instance.insert(node);
        }

        auto* function_opnum = IR_function_opnum(node);

        if (!m_ircontext.apply_eval_result(
            [&](BytecodeGenerateContext::EvalResult& result)
            {
                auto target_storage = result.get_assign_target();
                if (target_storage.has_value())
                {
                    m_ircontext.c().mov(
                        WO_OPNUM(target_storage.value()),
                        WO_OPNUM(function_opnum));
                }
                else
                    result.set_result(m_ircontext, lex, node, function_opnum);

                return true;
            }
        ))
        {
            // Eval failed.
            return FAILED;
        }

        return OKAY;
    }
    WO_PASS_PROCESSER(AstValueTuple)
    {
        if (state == UNPROCESSED)
        {
            auto rit_field_end = node->m_elements.rend();
            for (auto rit_field = node->m_elements.rbegin();
                rit_field != rit_field_end;
                ++rit_field)
            {
                m_ircontext.eval_sth_if_not_ignore(
                    &BytecodeGenerateContext::eval_push);

                WO_CONTINUE_PROCESS(*rit_field);
            }
            return HOLD;
        }
        else if (state == HOLD)
        {
            // Field has been pushed into stack.
            if (!m_ircontext.apply_eval_result(
                [&](BytecodeGenerateContext::EvalResult& result)
                {
                    opnum::opnumbase* make_result_target = nullptr;

                    auto& asigned_target = result.get_assign_target();
                    if (asigned_target.has_value())
                        make_result_target = asigned_target.value();
                    else
                    {
                        auto borrowed_reg = m_ircontext.borrow_opnum_temporary_register(lex, node);
                        if (borrowed_reg.has_value())
                            make_result_target = borrowed_reg.value();
                        else
                            // Unable to borrow register.
                            return false;

                        result.set_result(m_ircontext, lex, node, make_result_target);
                    }

                    m_ircontext.c().mkstruct(
                        WO_OPNUM(make_result_target), (uint16_t)node->m_elements.size());

                    return true;
                }))
            {
                // Eval failed.
                return FAILED;
            }
        }

        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueVariable)
    {
        wo_assert(state == UNPROCESSED);
        lang_ValueInstance* value_instance = node->m_LANG_variable_instance.value();

        if (value_instance->m_IR_normal_function.has_value())
        {
            AstValueFunction* func = value_instance->m_IR_normal_function.value();
            if (!m_ircontext.apply_eval_result(
                [&](BytecodeGenerateContext::EvalResult& result)
                {
                    auto target_storage = result.get_assign_target();
                    auto* function_opnum = IR_function_opnum(func);
                    if (target_storage.has_value())
                    {
                        m_ircontext.c().mov(
                            WO_OPNUM(target_storage.value()),
                            WO_OPNUM(function_opnum));
                    }
                    else
                        result.set_result(m_ircontext, lex, node, function_opnum);

                    return true;
                }))
            {
                // Eval failed.
                return FAILED;
            }
            return OKAY;
        }

        wo_assert(value_instance->IR_need_storage());

        if (!value_instance->m_IR_storage.has_value())
        {
            lex.lang_error(lexer::errorlevel::error, node,
                WO_ERR_VARIBALE_STORAGE_NOT_DETERMINED,
                get_value_name_w(value_instance));

            if (value_instance->m_symbol->m_symbol_declare_ast.has_value())
                lex.lang_error(lexer::errorlevel::infom,
                    value_instance->m_symbol->m_symbol_declare_ast.value(),
                    WO_INFO_SYMBOL_NAMED_DEFINED_HERE,
                    get_value_name_w(value_instance));

            return FAILED;
        }

        auto* storage_opnum =
            m_ircontext.get_storage_place(value_instance->m_IR_storage.value());

        if (!m_ircontext.apply_eval_result(
            [&](BytecodeGenerateContext::EvalResult& result)
            {
                auto target_storage = result.get_assign_target();
                if (target_storage.has_value())
                {
                    m_ircontext.c().mov(
                        WO_OPNUM(target_storage.value()),
                        WO_OPNUM(storage_opnum));
                }
                else
                    result.set_result(m_ircontext, lex, node, storage_opnum);

                return true;
            }))
        {
            // Eval failed.
            return FAILED;
        }

        return OKAY;
    }
    WO_PASS_PROCESSER(AstValueTypeCheckIs)
    {
        // Only dynamic check can be here.
        if (state == UNPROCESSED)
        {
            wo_assert(immutable_type(
                node->m_check_value->m_LANG_determined_type.value()) == m_origin_types.m_dynamic.m_type_instance);

            m_ircontext.eval_sth_if_not_ignore(
                &BytecodeGenerateContext::eval);

            WO_CONTINUE_PROCESS(node->m_check_value);
            return HOLD;
        }
        else if (state == HOLD)
        {
            if (!m_ircontext.apply_eval_result(
                [&](BytecodeGenerateContext::EvalResult& result)
                {
                    auto* opnum_to_check = m_ircontext.get_eval_result();

                    auto* target_type_instance =
                        node->m_check_type->m_LANG_determined_type.value();
                    auto* target_determined_type_instance =
                        target_type_instance->get_determined_type().value();

                    auto target_storage = result.get_assign_target();

                    value::valuetype check_type;
                    switch (target_determined_type_instance->m_base_type)
                    {
                    case lang_TypeInstance::DeterminedType::NIL:
                        check_type = value::valuetype::invalid;
                        break;
                    case lang_TypeInstance::DeterminedType::INTEGER:
                        check_type = value::valuetype::integer_type;
                        break;
                    case lang_TypeInstance::DeterminedType::REAL:
                        check_type = value::valuetype::real_type;
                        break;
                    case lang_TypeInstance::DeterminedType::HANDLE:
                        check_type = value::valuetype::handle_type;
                        break;
                    case lang_TypeInstance::DeterminedType::BOOLEAN:
                        check_type = value::valuetype::bool_type;
                        break;
                    case lang_TypeInstance::DeterminedType::STRING:
                        check_type = value::valuetype::string_type;
                        break;
                    case lang_TypeInstance::DeterminedType::GCHANDLE:
                        check_type = value::valuetype::gchandle_type;
                        break;
                    case lang_TypeInstance::DeterminedType::DICTIONARY:
                        check_type = value::valuetype::dict_type;
                        break;
                    case lang_TypeInstance::DeterminedType::ARRAY:
                        check_type = value::valuetype::array_type;
                    default:
                        wo_error("Unknown type.");
                        break;
                    }

                    m_ircontext.c().typeis(WO_OPNUM(opnum_to_check), check_type);

                    if (target_storage.has_value())
                    {
                        m_ircontext.c().mov(
                            WO_OPNUM(target_storage.value()),
                            WO_OPNUM(m_ircontext.opnum_spreg(opnum::reg::spreg::cr)));
                    }
                    else
                    {
                        result.set_result(
                            m_ircontext, lex, node, m_ircontext.opnum_spreg(opnum::reg::spreg::cr));
                    }
                    return true;
                }))
            {
                // Eval failed.
                return FAILED;
            }
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueTypeCast)
    {
        if (state == UNPROCESSED)
        {
            auto* target_type_instance =
                node->m_cast_type->m_LANG_determined_type.value();
            auto* target_determined_type_instance =
                target_type_instance->get_determined_type().value();

            if (target_determined_type_instance->m_base_type
                != lang_TypeInstance::DeterminedType::VOID)
            {
                m_ircontext.eval_sth_if_not_ignore(
                    &BytecodeGenerateContext::eval);
            }
            else
                m_ircontext.eval_ignore();

            WO_CONTINUE_PROCESS(node->m_cast_value);
            return HOLD;
        }
        else if (state == HOLD)
        {
            if (!m_ircontext.apply_eval_result(
                [&](BytecodeGenerateContext::EvalResult& result)
                {
                    auto* target_type_instance =
                        node->m_cast_type->m_LANG_determined_type.value();
                    auto* target_determined_type_instance =
                        target_type_instance->get_determined_type().value();

                    auto* src_type_instance =
                        node->m_cast_value->m_LANG_determined_type.value();
                    auto* src_determined_type_instance =
                        src_type_instance->get_determined_type().value();

                    auto target_storage = result.get_assign_target();

                    if (target_determined_type_instance->m_base_type
                        != src_determined_type_instance->m_base_type
                        && target_determined_type_instance->m_base_type
                        != lang_TypeInstance::DeterminedType::DYNAMIC)
                    {
                        // Need runtime cast.
                        value::valuetype cast_type;
                        switch (target_determined_type_instance->m_base_type)
                        {
                        case lang_TypeInstance::DeterminedType::NIL:
                            cast_type = value::valuetype::invalid;
                            break;
                        case lang_TypeInstance::DeterminedType::INTEGER:
                            cast_type = value::valuetype::integer_type;
                            break;
                        case lang_TypeInstance::DeterminedType::REAL:
                            cast_type = value::valuetype::real_type;
                            break;
                        case lang_TypeInstance::DeterminedType::HANDLE:
                            cast_type = value::valuetype::handle_type;
                            break;
                        case lang_TypeInstance::DeterminedType::BOOLEAN:
                            cast_type = value::valuetype::bool_type;
                            break;
                        case lang_TypeInstance::DeterminedType::STRING:
                            cast_type = value::valuetype::string_type;
                            break;
                        case lang_TypeInstance::DeterminedType::GCHANDLE:
                            cast_type = value::valuetype::gchandle_type;
                            break;
                        case lang_TypeInstance::DeterminedType::DICTIONARY:
                            cast_type = value::valuetype::dict_type;
                            break;
                        case lang_TypeInstance::DeterminedType::ARRAY:
                            cast_type = value::valuetype::array_type;
                            break;
                        case lang_TypeInstance::DeterminedType::VOID:
                        {
                            if (target_storage.has_value())
                            {
                                // NO NEED TO DO ANYTHING.
                                // VOID VALUE IS PURE JUNK VALUE.
                            }
                            else
                            {
                                // Return a junk value.
                                result.set_result(
                                    m_ircontext, lex, node, m_ircontext.opnum_spreg(opnum::reg::spreg::ni));
                            }
                            return true;
                        }
                        default:
                            wo_error("Unknown type.");
                            break;
                        }

                        // NOTE: If target type is void, we can't get result from context.
                        //  Expr has been evaled as non-result mode.
                        auto* opnum_to_cast = m_ircontext.get_eval_result();

                        if (target_storage.has_value())
                        {
                            m_ircontext.c().movcast(
                                WO_OPNUM(target_storage.value()),
                                WO_OPNUM(opnum_to_cast),
                                cast_type);
                        }
                        else
                        {
                            auto borrowed_reg = m_ircontext.borrow_opnum_temporary_register(lex, node);
                            if (borrowed_reg.has_value())
                            {
                                m_ircontext.c().movcast(
                                    WO_OPNUM(borrowed_reg.value()),
                                    WO_OPNUM(opnum_to_cast),
                                    cast_type);
                                result.set_result(m_ircontext, lex, node, borrowed_reg.value());
                            }
                            else
                                return false;
                        }
                    }
                    else
                    {

                        auto* opnum_to_cast = m_ircontext.get_eval_result();

                        // No need to cast.
                        if (target_storage.has_value())
                        {
                            m_ircontext.c().mov(
                                WO_OPNUM(target_storage.value()),
                                WO_OPNUM(opnum_to_cast));
                        }
                        else
                        {
                            m_ircontext.try_keep_opnum_temporary_register(opnum_to_cast);
                            result.set_result(m_ircontext, lex, node, opnum_to_cast);
                        }
                    }

                    return true;
                }))
            {
                // Eval failed.
                return FAILED;
            }
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
#undef WO_PASS_PROCESSER

    bool LangContext::pass_final_value(lexer& lex, ast::AstValueBase* val)
    {
        bool anylize_result =
            anylize_pass(lex, val, &LangContext::pass_final_B_process_bytecode_generation);

        return anylize_result;
    }
    LangContext::pass_behavior LangContext::pass_final_A_process_bytecode_generation(
        lexer& lex, const AstNodeWithState& node_state, PassProcessStackT& out_stack)
    {
        if (node_state.m_ast_node->node_type >= AstBase::AST_VALUE_begin && node_state.m_ast_node->node_type < AstBase::AST_VALUE_end)
        {
            // Is value, goto final pass B.
            wo_assert(node_state.m_state == UNPROCESSED);

            AstValueBase* eval_value = static_cast<AstValueBase*>(node_state.m_ast_node);
            lang_TypeInstance* type_instance = eval_value->m_LANG_determined_type.value();
            auto* immutable_type_instance = immutable_type(type_instance);

            if (immutable_type_instance != m_origin_types.m_void.m_type_instance
                && immutable_type_instance != m_origin_types.m_nothing.m_type_instance)
            {
                lex.lang_error(lexer::errorlevel::error, eval_value,
                    WO_ERR_NON_VOID_TYPE_EXPR_AS_STMT,
                    get_type_name_w(type_instance));

                return FAILED;
            }

            m_ircontext.eval_ignore();
            bool result = pass_final_value(lex, eval_value);

            if (result)
                return OKAY;
            else
                return FAILED;
        }
        wo_assert(m_passir_A_processers->check_has_processer(node_state.m_ast_node->node_type));
        return m_passir_A_processers->process_node(this, lex, node_state, out_stack);
    }
    LangContext::pass_behavior LangContext::pass_final_B_process_bytecode_generation(
        lexer& lex, const AstNodeWithState& node_state, PassProcessStackT& out_stack)
    {
        // PASS1 must process all nodes.
        wo_assert(m_passir_B_processers->check_has_processer(node_state.m_ast_node->node_type));
        wo_assert(node_state.m_ast_node->node_type >= AstBase::AST_VALUE_begin && node_state.m_ast_node->node_type < AstBase::AST_VALUE_end);

        AstValueBase* ast_value = static_cast<AstValueBase*>(node_state.m_ast_node);
        if (ast_value->m_evaled_const_value.has_value())
        {
            // This value has been evaluated as constant value.
            if (!m_ircontext.apply_eval_result(
                [&](BytecodeGenerateContext::EvalResult& result)
                {
                    opnum::opnumbase* immediately_value =
                        m_ircontext.opnum_imm_value(ast_value->m_evaled_const_value.value());

                    auto& asigned_target = result.get_assign_target();
                    if (asigned_target.has_value())
                        m_ircontext.c().mov(WO_OPNUM(asigned_target.value()), WO_OPNUM(immediately_value));
                    else
                        result.set_result(m_ircontext, lex, ast_value, immediately_value);

                    return true;
                }))
            {
                // Eval failed?
                return FAILED;
            }
            return OKAY;
        }

        auto process_result =
            m_passir_B_processers->process_node(this, lex, node_state, out_stack);

        if (process_result == FAILED)
            m_ircontext.failed_eval_result();

        return process_result;
    }

#endif
}