#include "wo_afx.hpp"

WO_API wo_api rslib_std_return_itself(wo_vm vm, wo_value args);

namespace wo
{
#ifndef WO_DISABLE_COMPILER
    using namespace ast;

#define WO_OPNUM(opnumptr) (*static_cast<opnum::opnumbase*>(opnumptr))
    std::string _generate_label(const std::string& prefix, const void* p)
    {
        char result[128];
        auto r = snprintf(result, 128, "%s_%p", prefix.c_str(), p);
        if (r < 0 || r >= 128)
            wo_error("Failed to generate label.");

        return result;
    }
    bool _is_storage_can_addressing(lang_ValueInstance::Storage& storage)
    {
        return storage.m_type == lang_ValueInstance::Storage::GLOBAL
            || (storage.m_index >= -64 && storage.m_index <= 63);
    }

    std::string LangContext::IR_function_label(ast::AstValueFunction* func)
    {
        char result[48];
        auto n = snprintf(result, 48, "#func_%p", func);

        if (n < 0 || n >= 48)
            wo_error("Failed to generate label.");

        return result;
    }
    opnum::opnumbase* LangContext::IR_function_opnum(AstValueFunction* func)
    {
        if (func->m_IR_extern_information.has_value())
        {
            auto* extern_function_instance =
                func->m_IR_extern_information.value()->m_IR_externed_function.value();

            return m_ircontext.opnum_imm_handle(
                (wo_handle_t)(intptr_t)(void*)extern_function_instance);
        }
        return m_ircontext.opnum_imm_rsfunc(func);
    }

    void BytecodeGenerateContext::begin_loop_while(ast::AstWhile* ast)
    {
        m_loop_content_stack.push_back(LoopContent{
            ast->m_IR_binded_label.has_value()
                ? std::optional(ast->m_IR_binded_label.value()->m_label)
                : std::nullopt,
            _generate_label("#while_end_", ast),
            _generate_label("#while_begin_", ast)
            });
    }
    void BytecodeGenerateContext::begin_loop_for(ast::AstFor* ast)
    {
        m_loop_content_stack.push_back(LoopContent{
            ast->m_IR_binded_label.has_value()
                ? std::optional(ast->m_IR_binded_label.value()->m_label)
                : std::nullopt,
            _generate_label("#for_end_", ast),
            _generate_label("#for_next_", ast)
            });
    }

    void BytecodeGenerateContext::end_loop()
    {
        wo_assert(!m_loop_content_stack.empty());
        m_loop_content_stack.pop_back();
    }

    std::optional<BytecodeGenerateContext::LoopContent*>
        BytecodeGenerateContext::find_nearest_loop_content_label(
            const std::optional<wo_pstring_t>& label)
    {
        auto loop_rend = m_loop_content_stack.rend();
        for (auto it = m_loop_content_stack.rbegin();
            it != loop_rend;
            ++it)
        {
            if (!label.has_value()
                || (it->m_label.has_value() && it->m_label.value() == label.value()))
                return &(*it);
        }
        return std::nullopt;
    }

    void LangContext::update_allocate_global_instance_storage_passir(
        lang_ValueInstance* instance)
    {
        // Instance must not have storage.
        if (instance->m_IR_storage.has_value())
            return;

        lang_Symbol* symbol = instance->m_symbol;
        wo_assert(
            !get_scope_located_function(symbol->m_belongs_to_scope).has_value()
            || symbol->m_is_global
            || (symbol->m_declare_attribute.has_value()
                && symbol->m_declare_attribute.value()->m_lifecycle.has_value()
                && symbol->m_declare_attribute.value()->m_lifecycle.value() == AstDeclareAttribue::lifecycle_attrib::STATIC));

        // Global or staitc, allocate global storage.
        instance->m_IR_storage = lang_ValueInstance::Storage{
            lang_ValueInstance::lang_ValueInstance::Storage::GLOBAL,
            m_ircontext.m_global_storage_allocating++
        };
    }

    bool LangContext::update_instance_storage_and_code_gen_passir(
        lexer& lex,
        lang_ValueInstance* instance,
        opnum::opnumbase* opnumval,
        const std::optional<uint16_t>& tuple_member_offset)
    {
        update_allocate_global_instance_storage_passir(instance);

        auto& storage = instance->m_IR_storage.value();

        if (_is_storage_can_addressing(storage))
        {
            auto* target_storage =
                m_ircontext.get_storage_place(storage);

            if (tuple_member_offset.has_value())
            {
                uint16_t index = tuple_member_offset.value();
                m_ircontext.c().idstruct(WO_OPNUM(target_storage), WO_OPNUM(opnumval), index);
            }
            else
                m_ircontext.c().mov(WO_OPNUM(target_storage), WO_OPNUM(opnumval));
        }
        else
        {
            if (tuple_member_offset.has_value())
            {
                auto* tmp = m_ircontext.borrow_opnum_temporary_register(
                    WO_BORROW_TEMPORARY_FROM(nullptr));

                uint16_t index = tuple_member_offset.value();
                m_ircontext.c().idstruct(WO_OPNUM(tmp), WO_OPNUM(opnumval), index);
                m_ircontext.c().sts(
                    WO_OPNUM(tmp),
                    WO_OPNUM(m_ircontext.opnum_imm_int(storage.m_index)));

                m_ircontext.return_opnum_temporary_register(tmp);
            }
            else
                m_ircontext.c().sts(
                    WO_OPNUM(opnumval),
                    WO_OPNUM(m_ircontext.opnum_imm_int(storage.m_index)));
        }

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
                tuple_source = m_ircontext.borrow_opnum_temporary_register(WO_BORROW_TEMPORARY_FROM(nullptr));
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
        // No need to impl:
        // WO_LANG_REGISTER_PROCESSER(AstIdentifier, AstBase::AST_IDENTIFIER, passir_A);
        // WO_LANG_REGISTER_PROCESSER(AstTemplateArgument, AstBase::AST_TEMPLATE_ARGUMENT, passir_A);
        // WO_LANG_REGISTER_PROCESSER(AstTemplateParam, AstBase::AST_TEMPLATE_PARAM, passir_A);
        // WO_LANG_REGISTER_PROCESSER(AstStructFieldDefine, AstBase::AST_STRUCT_FIELD_DEFINE, passir_A);
        // WO_LANG_REGISTER_PROCESSER(AstTypeHolder, AstBase::AST_TYPE_HOLDER, passir_A);
        // WO_LANG_REGISTER_PROCESSER(AstWhereConstraints, AstBase::AST_WHERE_CONSTRAINTS, passir_A);
        // WO_LANG_REGISTER_PROCESSER(AstValueFunctionCall_FakeAstArgumentDeductionContextA,
        //    AstBase::AST_VALUE_FUNCTION_CALL_FAKE_AST_ARGUMENT_DEDUCTION_CONTEXT_A, passir_A);
        // WO_LANG_REGISTER_PROCESSER(AstValueFunctionCall_FakeAstArgumentDeductionContextB,
        //    AstBase::AST_VALUE_FUNCTION_CALL_FAKE_AST_ARGUMENT_DEDUCTION_CONTEXT_B, passir_A);
        // WO_LANG_REGISTER_PROCESSER(AstPatternVariable, AstBase::AST_PATTERN_VARIABLE, passir_A);
        // WO_LANG_REGISTER_PROCESSER(AstPatternIndex, AstBase::AST_PATTERN_INDEX, passir_A);
        // WO_LANG_REGISTER_PROCESSER(AstFunctionParameterDeclare, AstBase::AST_FUNCTION_PARAMETER_DECLARE, passir_A);
        // WO_LANG_REGISTER_PROCESSER(AstKeyValuePair, AstBase::AST_KEY_VALUE_PAIR, passir_A);
        // WO_LANG_REGISTER_PROCESSER(AstStructFieldValuePair, AstBase::AST_STRUCT_FIELD_VALUE_PAIR, passir_A);
        // WO_LANG_REGISTER_PROCESSER(AstExternInformation, AstBase::AST_EXTERN_INFORMATION, passir_A);

        WO_LANG_REGISTER_PROCESSER(AstList, AstBase::AST_LIST, passir_A);
        WO_LANG_REGISTER_PROCESSER(AstVariableDefineItem, AstBase::AST_VARIABLE_DEFINE_ITEM, passir_A);
        WO_LANG_REGISTER_PROCESSER(AstVariableDefines, AstBase::AST_VARIABLE_DEFINES, passir_A);
        WO_LANG_REGISTER_PROCESSER(AstNamespace, AstBase::AST_NAMESPACE, passir_A);
        WO_LANG_REGISTER_PROCESSER(AstScope, AstBase::AST_SCOPE, passir_A);
        WO_LANG_REGISTER_PROCESSER(AstMatchCase, AstBase::AST_MATCH_CASE, passir_A);
        WO_LANG_REGISTER_PROCESSER(AstMatch, AstBase::AST_MATCH, passir_A);
        WO_LANG_REGISTER_PROCESSER(AstIf, AstBase::AST_IF, passir_A);
        WO_LANG_REGISTER_PROCESSER(AstWhile, AstBase::AST_WHILE, passir_A);
        WO_LANG_REGISTER_PROCESSER(AstFor, AstBase::AST_FOR, passir_A);
        WO_LANG_REGISTER_PROCESSER(AstForeach, AstBase::AST_FOREACH, passir_A);
        WO_LANG_REGISTER_PROCESSER(AstBreak, AstBase::AST_BREAK, passir_A);
        WO_LANG_REGISTER_PROCESSER(AstContinue, AstBase::AST_CONTINUE, passir_A);
        WO_LANG_REGISTER_PROCESSER(AstReturn, AstBase::AST_RETURN, passir_A);
        WO_LANG_REGISTER_PROCESSER(AstLabeled, AstBase::AST_LABELED, passir_A);
        WO_LANG_REGISTER_PROCESSER(AstUsingTypeDeclare, AstBase::AST_USING_TYPE_DECLARE, passir_A);
        WO_LANG_REGISTER_PROCESSER(AstAliasTypeDeclare, AstBase::AST_ALIAS_TYPE_DECLARE, passir_A);
        WO_LANG_REGISTER_PROCESSER(AstUsingNamespace, AstBase::AST_USING_NAMESPACE, passir_A);
        WO_LANG_REGISTER_PROCESSER(AstEnumDeclare, AstBase::AST_ENUM_DECLARE, passir_A);
        WO_LANG_REGISTER_PROCESSER(AstUnionDeclare, AstBase::AST_UNION_DECLARE, passir_A);
        WO_LANG_REGISTER_PROCESSER(AstNop, AstBase::AST_NOP, passir_A);

        // WO_LANG_REGISTER_PROCESSER(AstValueNothing, AstBase::AST_VALUE_NOTHING, passir_B);
        WO_LANG_REGISTER_PROCESSER(AstValueMarkAsMutable, AstBase::AST_VALUE_MARK_AS_MUTABLE, passir_B);
        WO_LANG_REGISTER_PROCESSER(AstValueMarkAsImmutable, AstBase::AST_VALUE_MARK_AS_IMMUTABLE, passir_B);
        WO_LANG_REGISTER_PROCESSER(AstValueLiteral, AstBase::AST_VALUE_LITERAL, passir_B);
        WO_LANG_REGISTER_PROCESSER(AstValueTypeid, AstBase::AST_VALUE_TYPEID, passir_B);
        WO_LANG_REGISTER_PROCESSER(AstValueTypeCast, AstBase::AST_VALUE_TYPE_CAST, passir_B);
        WO_LANG_REGISTER_PROCESSER(AstValueDoAsVoid, AstBase::AST_VALUE_DO_AS_VOID, passir_B);
        WO_LANG_REGISTER_PROCESSER(AstValueTypeCheckIs, AstBase::AST_VALUE_TYPE_CHECK_IS, passir_B);
        WO_LANG_REGISTER_PROCESSER(AstValueTypeCheckAs, AstBase::AST_VALUE_TYPE_CHECK_AS, passir_B);
        WO_LANG_REGISTER_PROCESSER(AstValueVariable, AstBase::AST_VALUE_VARIABLE, passir_B);
        WO_LANG_REGISTER_PROCESSER(AstValueFunctionCall, AstBase::AST_VALUE_FUNCTION_CALL, passir_B);
        WO_LANG_REGISTER_PROCESSER(AstValueBinaryOperator, AstBase::AST_VALUE_BINARY_OPERATOR, passir_B);
        WO_LANG_REGISTER_PROCESSER(AstValueUnaryOperator, AstBase::AST_VALUE_UNARY_OPERATOR, passir_B);
        WO_LANG_REGISTER_PROCESSER(AstValueTribleOperator, AstBase::AST_VALUE_TRIBLE_OPERATOR, passir_B);
        WO_LANG_REGISTER_PROCESSER(AstFakeValueUnpack, AstBase::AST_FAKE_VALUE_UNPACK, passir_B);
        WO_LANG_REGISTER_PROCESSER(AstValueVariadicArgumentsPack, AstBase::AST_VALUE_VARIADIC_ARGUMENTS_PACK, passir_B);
        WO_LANG_REGISTER_PROCESSER(AstValueIndex, AstBase::AST_VALUE_INDEX, passir_B);
        WO_LANG_REGISTER_PROCESSER(AstValueFunction, AstBase::AST_VALUE_FUNCTION, passir_B);
        WO_LANG_REGISTER_PROCESSER(AstValueArrayOrVec, AstBase::AST_VALUE_ARRAY_OR_VEC, passir_B);
        WO_LANG_REGISTER_PROCESSER(AstValueDictOrMap, AstBase::AST_VALUE_DICT_OR_MAP, passir_B);
        WO_LANG_REGISTER_PROCESSER(AstValueTuple, AstBase::AST_VALUE_TUPLE, passir_B);
        WO_LANG_REGISTER_PROCESSER(AstValueStruct, AstBase::AST_VALUE_STRUCT, passir_B);
        WO_LANG_REGISTER_PROCESSER(AstValueAssign, AstBase::AST_VALUE_ASSIGN, passir_B);
        WO_LANG_REGISTER_PROCESSER(AstValueMakeUnion, AstBase::AST_VALUE_MAKE_UNION, passir_B);
        WO_LANG_REGISTER_PROCESSER(AstValueIROpnum, AstBase::AST_VALUE_IR_OPNUM, passir_B);
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
    WO_PASS_PROCESSER(AstScope)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_body);
            return HOLD;
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstNamespace)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_body);
            return HOLD;
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstUsingTypeDeclare)
    {
        wo_assert(state == UNPROCESSED);

        return OKAY;
    }
    WO_PASS_PROCESSER(AstAliasTypeDeclare)
    {
        wo_assert(state == UNPROCESSED);
        return OKAY;
    }
    WO_PASS_PROCESSER(AstUsingNamespace)
    {
        wo_assert(state == UNPROCESSED);
        return OKAY;
    }
    WO_PASS_PROCESSER(AstEnumDeclare)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_enum_body);
            WO_CONTINUE_PROCESS(node->m_enum_type_declare);
            return HOLD;
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstUnionDeclare)
    {
        if (state == UNPROCESSED)
        {
            if (node->m_union_namespace.has_value())
                WO_CONTINUE_PROCESS(node->m_union_namespace.value());

            WO_CONTINUE_PROCESS(node->m_union_type_declare);
            return HOLD;
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstIf)
    {
        if (state == UNPROCESSED)
        {
            if (node->m_condition->m_evaled_const_value.has_value())
            {
                if (node->m_condition->m_evaled_const_value.value().value_bool() != 0)
                    WO_CONTINUE_PROCESS(node->m_true_body);
                else if (node->m_false_body.has_value())
                    WO_CONTINUE_PROCESS(node->m_false_body.value());

                node->m_LANG_hold_state = AstIf::IR_HOLD_FOR_FALSE_BODY;
            }
            else
            {
                m_ircontext.eval_to(m_ircontext.opnum_spreg(opnum::reg::cr));
                if (!pass_final_value(lex, node->m_condition))
                    return FAILED;

                (void)m_ircontext.get_eval_result();

                m_ircontext.c().jf(opnum::tag(_generate_label("#if_else_", node)));
                WO_CONTINUE_PROCESS(node->m_true_body);

                node->m_LANG_hold_state = AstIf::IR_HOLD_FOR_TRUE_BODY;
            }
            return HOLD;
        }
        else if (state == HOLD)
        {
            switch (node->m_LANG_hold_state)
            {
            case AstIf::IR_HOLD_FOR_TRUE_BODY:
                if (node->m_false_body.has_value())
                {
                    m_ircontext.c().jmp(opnum::tag(_generate_label("#if_end_", node)));
                    WO_CONTINUE_PROCESS(node->m_false_body.value());
                }
                m_ircontext.c().tag(_generate_label("#if_else_", node));
                node->m_LANG_hold_state = AstIf::IR_HOLD_FOR_FALSE_BODY;

                return HOLD;
            case AstIf::IR_HOLD_FOR_FALSE_BODY:
                if (!node->m_condition->m_evaled_const_value.has_value()
                    && node->m_false_body.has_value())
                {
                    m_ircontext.c().tag(_generate_label("#if_end_", node));
                }

                break;
            default:
                wo_error("Unknown hold state.");
                break;
            }
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstWhile)
    {
        if (state == UNPROCESSED)
        {
            bool dead_loop = false;
            if (node->m_condition->m_evaled_const_value.has_value())
            {
                if (node->m_condition->m_evaled_const_value.value().value_bool() == 0)
                    return OKAY; // Skip body.

                dead_loop = true;
            }
            m_ircontext.c().tag(_generate_label("#while_begin_", node));

            if (!dead_loop)
            {
                m_ircontext.eval_to(m_ircontext.opnum_spreg(opnum::reg::cr));
                if (!pass_final_value(lex, node->m_condition))
                    return FAILED;

                (void)m_ircontext.get_eval_result();

                m_ircontext.c().jf(opnum::tag(_generate_label("#while_end_", node)));
            }

            // Loop begin
            m_ircontext.begin_loop_while(node);

            WO_CONTINUE_PROCESS(node->m_body);

            return HOLD;
        }
        else if (state == HOLD)
        {
            m_ircontext.c().jmp(opnum::tag(_generate_label("#while_begin_", node)));
            m_ircontext.c().tag(_generate_label("#while_end_", node));

            m_ircontext.end_loop();
        }
        else
        {
            // Failed, we still need to end the loop.
            m_ircontext.end_loop();
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstFor)
    {
        /*
        for (INITEXPR; COND; NEXT)
            BODY
        ===========================>
            INITEXPR
            jmp for_cond
        for_begin:
            BODY
        for_next:
            NEXT
        for_cond:
            COND
            jt for_begin
        for_end:
        */

        if (state == UNPROCESSED)
        {
            if (node->m_initial.has_value())
                WO_CONTINUE_PROCESS(node->m_initial.value());

            node->m_LANG_hold_state = AstFor::IR_HOLD_FOR_INIT_EVAL;
            return HOLD;
        }
        else if (state == HOLD)
        {
            switch (node->m_LANG_hold_state)
            {
            case AstFor::IR_HOLD_FOR_INIT_EVAL:
                if (node->m_condition.has_value()
                    && !node->m_condition.value()->m_evaled_const_value.has_value())
                    // Need runtime cond.
                    m_ircontext.c().jmp(opnum::tag(_generate_label("#for_cond_", node)));
                else
                {
                    if (node->m_condition.has_value()
                        && node->m_condition.value()->m_evaled_const_value.has_value()
                        && !node->m_condition.value()->m_evaled_const_value.value().value_bool())
                    {
                        // Skip body next and cond.
                        return OKAY;
                    }
                }

                m_ircontext.c().tag(_generate_label("#for_begin_", node));

                // Loop begin
                m_ircontext.begin_loop_for(node);

                WO_CONTINUE_PROCESS(node->m_body);
                node->m_LANG_hold_state = AstFor::IR_HOLD_FOR_BODY_EVAL;
                return HOLD;

            case AstFor::IR_HOLD_FOR_BODY_EVAL:
                m_ircontext.end_loop();

                m_ircontext.c().tag(_generate_label("#for_next_", node));

                if (node->m_step.has_value())
                {
                    m_ircontext.eval_ignore();
                    if (!pass_final_value(lex, node->m_step.value()))
                        return FAILED;
                }

                if (node->m_condition.has_value()
                    && !node->m_condition.value()->m_evaled_const_value.has_value())
                {
                    m_ircontext.c().tag(_generate_label("#for_cond_", node));

                    m_ircontext.eval_to(m_ircontext.opnum_spreg(opnum::reg::cr));
                    if (!pass_final_value(lex, node->m_condition.value()))
                        return FAILED;

                    (void)m_ircontext.get_eval_result();

                    m_ircontext.c().jt(opnum::tag(_generate_label("#for_begin_", node)));
                }
                else
                {
                    // Must be dead loop here.
                    wo_assert(!node->m_condition.has_value()
                        || node->m_condition.value()->m_evaled_const_value.value().value_bool());

                    m_ircontext.c().jmp(opnum::tag(_generate_label("#for_begin_", node)));
                }
                m_ircontext.c().tag(_generate_label("#for_end_", node));

                break;
            }
        }
        else
        {
            // Failed, we still need to end the loop.
            if (node->m_LANG_hold_state == AstFor::IR_HOLD_FOR_BODY_EVAL)
                m_ircontext.end_loop();
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstForeach)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_forloop_body);
            return HOLD;
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstBreak)
    {
        wo_assert(state == UNPROCESSED);

        auto loop = m_ircontext.find_nearest_loop_content_label(node->m_label);
        if (!loop.has_value())
        {
            lex.record_lang_error(lexer::msglevel_t::error, node,
                WO_ERR_BAD_BREAK);

            if (node->m_label.has_value())
                lex.record_lang_error(lexer::msglevel_t::infom, node,
                    WO_INFO_BAD_LABEL_NAMED,
                    node->m_label.value());

            return FAILED;
        }

        m_ircontext.c().jmp(loop.value()->m_break_label);
        return OKAY;
    }
    WO_PASS_PROCESSER(AstContinue)
    {
        wo_assert(state == UNPROCESSED);

        auto loop = m_ircontext.find_nearest_loop_content_label(node->m_label);
        if (!loop.has_value())
        {
            lex.record_lang_error(lexer::msglevel_t::error, node,
                WO_ERR_BAD_CONTINUE);

            if (node->m_label.has_value())
                lex.record_lang_error(lexer::msglevel_t::infom, node,
                    WO_INFO_BAD_LABEL_NAMED,
                    node->m_label.value());

            return FAILED;
        }

        m_ircontext.c().jmp(loop.value()->m_continue_label);
        return OKAY;
    }
    WO_PASS_PROCESSER(AstMatch)
    {
        if (state == UNPROCESSED)
        {
            m_ircontext.eval_keep();
            if (!pass_final_value(lex, node->m_matched_value))
                return FAILED;

            auto* matching_value = m_ircontext.get_eval_result();
            m_ircontext.c().idstruct(
                WO_OPNUM(m_ircontext.opnum_spreg(opnum::reg::spreg::cr)),
                WO_OPNUM(matching_value),
                0);

            node->m_IR_matching_struct_opnum = matching_value;
            for (auto& match_case : node->m_cases)
            {
                match_case->m_IR_matching_struct_opnum = matching_value;
                match_case->m_IR_match = node;
            }

            WO_CONTINUE_PROCESS_LIST(node->m_cases);

            return HOLD;
        }
        else if (state == HOLD)
        {
            m_ircontext.try_return_opnum_temporary_register(node->m_IR_matching_struct_opnum.value());
            m_ircontext.c().ext_panic(WO_OPNUM(m_ircontext.opnum_imm_string(
                wstring_pool::get_pstr(
                    L"Bad label for union: '"
                    + std::wstring(get_type_name_w(node->m_matched_value->m_LANG_determined_type.value()))
                    + L"', may be bad value returned by the external function."))));

            m_ircontext.c().tag(_generate_label("#match_end_", node));
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstMatchCase)
    {
        if (state == UNPROCESSED)
        {
            std::string match_case_end_label = _generate_label("#match_case_end_", node);

            if (node->m_LANG_case_label_or_takeplace.has_value())
                m_ircontext.c().jnequb(
                    WO_OPNUM(m_ircontext.opnum_imm_int(node->m_LANG_case_label_or_takeplace.value())),
                    opnum::tag(match_case_end_label));

            if (node->m_pattern->node_type == AstBase::AST_PATTERN_UNION)
            {
                AstPatternUnion* pattern_union = static_cast<AstPatternUnion*>(node->m_pattern);
                if (pattern_union->m_field.has_value())
                {
                    AstPatternBase* pattern_base = pattern_union->m_field.value();

                    update_pattern_storage_and_code_gen_passir(
                        lex,
                        pattern_base,
                        node->m_IR_matching_struct_opnum.value(),
                        (uint16_t)1);
                }
            }
            else
            {
                wo_assert(node->m_pattern->node_type == AstBase::AST_PATTERN_TAKEPLACE);
            }

            WO_CONTINUE_PROCESS(node->m_body);
            return HOLD;
        }
        else if (state == HOLD)
        {
            m_ircontext.c().jmp(_generate_label("#match_end_", node->m_IR_match.value()));
            m_ircontext.c().tag(_generate_label("#match_case_end_", node));
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstLabeled)
    {
        if (state == UNPROCESSED)
        {
            switch (node->m_body->node_type)
            {
            case AstBase::AST_WHILE:
            {
                AstWhile* while_node = static_cast<AstWhile*>(node->m_body);
                while_node->m_IR_binded_label = node;
                break;
            }
            case AstBase::AST_FOR:
            {
                AstFor* for_node = static_cast<AstFor*>(node->m_body);
                for_node->m_IR_binded_label = node;
                break;
            }
            case AstBase::AST_FOREACH:
            {
                AstForeach* foreach_node = static_cast<AstForeach*>(node->m_body);
                foreach_node->m_forloop_body->m_IR_binded_label = node;
                break;
            }
            default:
                // Not a loop, skip
                break;
            }

            WO_CONTINUE_PROCESS(node->m_body);
            return HOLD;
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstVariableDefines)
    {
        if (state == UNPROCESSED)
        {
            if (node->m_attribute.has_value())
            {
                auto& attribute = node->m_attribute.value();
                if (attribute->m_lifecycle.has_value()
                    && attribute->m_lifecycle.value() == AstDeclareAttribue::lifecycle_attrib::STATIC)
                {
                    node->m_IR_static_init_flag_global_offset = m_ircontext.m_global_storage_allocating++;
                }
            }

            if (node->m_IR_static_init_flag_global_offset.has_value())
            {
                m_ircontext.c().equb(
                    WO_OPNUM(m_ircontext.opnum_global(node->m_IR_static_init_flag_global_offset.value())),
                    WO_OPNUM(m_ircontext.opnum_spreg(opnum::reg::ni)));
                m_ircontext.c().jf(opnum::tag(_generate_label("#static_end_", node)));
                m_ircontext.c().mov(
                    WO_OPNUM(m_ircontext.opnum_global(node->m_IR_static_init_flag_global_offset.value())),
                    WO_OPNUM(m_ircontext.opnum_imm_bool(true)));
            }

            WO_CONTINUE_PROCESS_LIST(node->m_definitions);
            return HOLD;
        }
        if (state == HOLD)
        {
            if (node->m_IR_static_init_flag_global_offset.has_value())
                m_ircontext.c().tag(_generate_label("#static_end_", node));
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstVariableDefineItem)
    {
        wo_assert(state == UNPROCESSED);

        if (node->m_pattern->node_type == AstBase::AST_PATTERN_TAKEPLACE)
        {
            m_ircontext.eval_ignore();
            if (!pass_final_value(lex, node->m_init_value))
                // Failed 
                return FAILED;

            return OKAY;
        }
        else if (node->m_pattern->node_type == AstBase::AST_PATTERN_SINGLE)
        {
            // Might be constant, template.
            AstPatternSingle* pattern_single = static_cast<AstPatternSingle*>(node->m_pattern);
            lang_Symbol* pattern_symbol = pattern_single->m_LANG_declared_symbol.value();
            wo_assert(pattern_symbol->m_symbol_kind == lang_Symbol::kind::VARIABLE);

            if (pattern_symbol->m_is_template)
            {
                // Is template, walk through all the template instance;
                for (auto* template_instance :
                    pattern_symbol->m_template_value_instances->m_finished_instance_list)
                {
                    wo_assert(template_instance->m_state == lang_TemplateAstEvalStateValue::state::EVALUATED);

                    lang_ValueInstance* template_value_instance = template_instance->m_value_instance.get();
                    if (!template_value_instance->IR_need_storage())
                    {
                        // No need storage.
                        auto function = template_value_instance->m_determined_constant_or_function
                            .value().value_try_function();

                        if (function.has_value())
                        {
                            wo_assert(function.value()->m_LANG_value_instance_to_update.value()
                                == template_value_instance);

                            // We still eval the function to let compiler know the function.
                            m_ircontext.eval_ignore();
                            if (!pass_final_value(lex, function.value()))
                                // Failed 
                                return FAILED;
                        }
                    }
                    else
                    {
                        // Need storage and initialize.
                        bool fast_eval = template_value_instance->m_IR_storage.has_value()
                            && (_is_storage_can_addressing(template_value_instance->m_IR_storage.value()));
                        if (fast_eval)
                            m_ircontext.eval_to(
                                m_ircontext.get_storage_place(
                                    template_value_instance->m_IR_storage.value()));
                        else
                            m_ircontext.eval();
                        if (!pass_final_value(lex, static_cast<AstValueBase*>(template_instance->m_ast)))
                            // Failed 
                            return FAILED;

                        auto* result_opnum = m_ircontext.get_eval_result();

                        if (fast_eval)
                            (void)result_opnum;
                        else if (!update_instance_storage_and_code_gen_passir(
                            lex, template_value_instance, result_opnum, std::nullopt))
                            return FAILED;
                    }
                }
            }
            else if (!pattern_symbol->m_value_instance->IR_need_storage())
            {
                // No need storage.
                auto function = pattern_symbol->m_value_instance->m_determined_constant_or_function
                    .value().value_try_function();

                if (function.has_value())
                {
                    m_ircontext.eval_ignore();
                    if (!pass_final_value(lex, function.value()))
                        // Failed 
                        return FAILED;
                }
            }
            else
            {
                // Not template, but need storage.

                bool fast_eval = pattern_symbol->m_value_instance->m_IR_storage.has_value()
                    && _is_storage_can_addressing(pattern_symbol->m_value_instance->m_IR_storage.value());

                if (fast_eval)
                    m_ircontext.eval_to(
                        m_ircontext.get_storage_place(
                            pattern_symbol->m_value_instance->m_IR_storage.value()));
                else
                    m_ircontext.eval();

                if (!pass_final_value(lex, node->m_init_value))
                    // Failed 
                    return FAILED;

                auto* result_opnum = m_ircontext.get_eval_result();

                if (fast_eval)
                    (void)result_opnum;
                else if (!update_instance_storage_and_code_gen_passir(
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
    WO_PASS_PROCESSER(AstReturn)
    {
        if (node->m_value.has_value())
        {
            m_ircontext.eval_to(m_ircontext.opnum_spreg(opnum::reg::cr));
            if (!pass_final_value(lex, node->m_value.value()))
                return FAILED;

            (void)m_ircontext.get_eval_result();
        }

        if (node->m_LANG_belong_function_may_null_if_outside.has_value())
        {
            AstValueFunction* returned_func = node->m_LANG_belong_function_may_null_if_outside.value();
            if (returned_func->m_is_variadic)
            {
                m_ircontext.c().jmp(opnum::tag(IR_function_label(returned_func) + "_ret"));
            }
            else
            {
                if (!returned_func->m_LANG_captured_context.m_captured_variables.empty())
                    m_ircontext.c().ret(
                        (uint16_t)returned_func->m_LANG_captured_context.m_captured_variables.size());
                else
                    m_ircontext.c().ret();
            }
        }
        else
        {
            if (!node->m_value.has_value())
                // If return void outside function, treat as return 0;
                m_ircontext.c().mov(
                    WO_OPNUM(m_ircontext.opnum_spreg(opnum::reg::cr)),
                    WO_OPNUM(m_ircontext.opnum_imm_int(0)));

            m_ircontext.c().jmp(opnum::tag("#woolang_program_end"));
        }

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
        if (node->m_IR_extern_information.has_value())
        {
            wo_assert(node->m_LANG_captured_context.m_captured_variables.empty());

            AstExternInformation* extern_info = node->m_IR_extern_information.value();
            wo_assert(extern_info == static_cast<AstExternInformation*>(node->m_body));

            m_ircontext.c().record_extern_native_function(
                (intptr_t)(void*)extern_info->m_IR_externed_function.value(),
                *node->source_location.source_file,
                extern_info->m_extern_from_library.has_value()
                ? std::optional(*extern_info->m_extern_from_library.value())
                : std::nullopt,
                *extern_info->m_extern_symbol);
        }
        else
        {
            // Record it!
            m_ircontext.m_being_used_function_instance.insert(node);
        }

        if (node->m_LANG_captured_context.m_captured_variables.empty())
        {
            // Simple normal function
            auto* function_opnum = IR_function_opnum(node);

            m_ircontext.apply_eval_result(
                [&](BytecodeGenerateContext::EvalResult& result)
                {
                    const auto& target_storage = result.get_assign_target();
                    if (target_storage.has_value())
                    {
                        m_ircontext.c().mov(
                            WO_OPNUM(target_storage.value()),
                            WO_OPNUM(function_opnum));
                    }
                    else
                        result.set_result(m_ircontext, function_opnum);
                }
            );
        }
        else
        {
            // Need capture.
            m_ircontext.apply_eval_result(
                [&](BytecodeGenerateContext::EvalResult& result)
                {
                    auto* borrowed_opnum = m_ircontext.borrow_opnum_temporary_register(WO_BORROW_TEMPORARY_FROM(node));
                    for (auto& [capture_from_value, _useless] : node->m_LANG_captured_context.m_captured_variables)
                    {
                        (void)_useless;

                        auto& storage = capture_from_value->m_IR_storage.value();
                        if (_is_storage_can_addressing(storage))
                            m_ircontext.c().psh(
                                WO_OPNUM(m_ircontext.get_storage_place(storage)));
                        else
                        {
                            m_ircontext.c().lds(
                                WO_OPNUM(borrowed_opnum),
                                WO_OPNUM(m_ircontext.opnum_imm_int(storage.m_index)));
                            m_ircontext.c().psh(
                                WO_OPNUM(borrowed_opnum));
                        }
                    }

                    const auto& target_storage = result.get_assign_target();
                    m_ircontext.c().mkclos(
                        (uint16_t)node->m_LANG_captured_context.m_captured_variables.size(),
                        opnum::tag(IR_function_label(node)));

                    if (target_storage.has_value())
                    {
                        m_ircontext.return_opnum_temporary_register(borrowed_opnum);
                        if (opnum::reg* target_reg = dynamic_cast<opnum::reg*>(target_storage.value());
                            target_reg == nullptr || target_reg->id != opnum::reg::cr)
                        {
                            m_ircontext.c().mov(
                                WO_OPNUM(target_storage.value()),
                                WO_OPNUM(m_ircontext.opnum_spreg(opnum::reg::cr)));
                            // Or do nothing, target is same.
                        }
                    }
                    else
                    {
                        m_ircontext.c().mov(
                            WO_OPNUM(borrowed_opnum),
                            WO_OPNUM(m_ircontext.opnum_spreg(opnum::reg::cr)));

                        result.set_result(m_ircontext, borrowed_opnum);
                    }
                }
            );
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
                auto* field_value = *rit_field;

                if (field_value->node_type == AstBase::AST_FAKE_VALUE_UNPACK)
                {
                    m_ircontext.eval_sth_if_not_ignore(
                        &BytecodeGenerateContext::eval_action);
                }
                else
                {
                    m_ircontext.eval_sth_if_not_ignore(
                        &BytecodeGenerateContext::eval_push);
                }
                WO_CONTINUE_PROCESS(field_value);
            }
            return HOLD;
        }
        else if (state == HOLD)
        {
            // Field has been pushed into stack.
            m_ircontext.apply_eval_result(
                [&](BytecodeGenerateContext::EvalResult& result)
                {
                    opnum::opnumbase* make_result_target = nullptr;

                    const auto& asigned_target = result.get_assign_target();
                    if (asigned_target.has_value())
                        make_result_target = asigned_target.value();
                    else
                    {
                        make_result_target = m_ircontext.borrow_opnum_temporary_register(WO_BORROW_TEMPORARY_FROM(node));
                        result.set_result(m_ircontext, make_result_target);
                    }

                    uint16_t elem_count = (uint16_t)node->m_elements.size();
                    for (auto* elem_field : node->m_elements)
                    {
                        if (elem_field->node_type == AstBase::AST_FAKE_VALUE_UNPACK)
                        {
                            auto* unpacking_tuple_determined_type =
                                elem_field->m_LANG_determined_type.value()->get_determined_type().value();

                            wo_assert(unpacking_tuple_determined_type->m_base_type == lang_TypeInstance::DeterminedType::TUPLE);
                            auto* tuple_info = unpacking_tuple_determined_type->m_external_type_description.m_tuple;
                            uint16_t tuple_elem_count = (uint16_t)tuple_info->m_element_types.size();

                            --elem_count;
                            elem_count += tuple_elem_count;
                        }
                    }

                    m_ircontext.c().mkstruct(
                        WO_OPNUM(make_result_target), elem_count);
                });
        }

        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueDictOrMap)
    {
        if (state == UNPROCESSED)
        {
            auto rit_field_end = node->m_elements.rend();
            for (auto rit_field = node->m_elements.rbegin();
                rit_field != rit_field_end;
                ++rit_field)
            {
                auto* field_value = *rit_field;

                m_ircontext.eval_sth_if_not_ignore(
                    &BytecodeGenerateContext::eval_push);

                WO_CONTINUE_PROCESS(field_value->m_value);

                m_ircontext.eval_sth_if_not_ignore(
                    &BytecodeGenerateContext::eval_push);

                WO_CONTINUE_PROCESS(field_value->m_key);
            }
            return HOLD;
        }
        else if (state == HOLD)
        {
            // Field has been pushed into stack.
            m_ircontext.apply_eval_result(
                [&](BytecodeGenerateContext::EvalResult& result)
                {
                    opnum::opnumbase* make_result_target = nullptr;

                    const auto& asigned_target = result.get_assign_target();
                    if (asigned_target.has_value())
                        make_result_target = asigned_target.value();
                    else
                    {
                        make_result_target = m_ircontext.borrow_opnum_temporary_register(WO_BORROW_TEMPORARY_FROM(node));
                        result.set_result(m_ircontext, make_result_target);
                    }

                    uint16_t elem_count = (uint16_t)node->m_elements.size();
                    m_ircontext.c().mkmap(WO_OPNUM(make_result_target), elem_count);
                });
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueArrayOrVec)
    {
        if (state == UNPROCESSED)
        {
            auto rit_field_end = node->m_elements.rend();
            for (auto rit_field = node->m_elements.rbegin();
                rit_field != rit_field_end;
                ++rit_field)
            {
                auto* field_value = *rit_field;

                m_ircontext.eval_sth_if_not_ignore(
                    &BytecodeGenerateContext::eval_push);

                WO_CONTINUE_PROCESS(field_value);
            }
            return HOLD;
        }
        else if (state == HOLD)
        {
            // Field has been pushed into stack.
            m_ircontext.apply_eval_result(
                [&](BytecodeGenerateContext::EvalResult& result)
                {
                    opnum::opnumbase* make_result_target = nullptr;

                    const auto& asigned_target = result.get_assign_target();
                    if (asigned_target.has_value())
                        make_result_target = asigned_target.value();
                    else
                    {
                        make_result_target = m_ircontext.borrow_opnum_temporary_register(WO_BORROW_TEMPORARY_FROM(node));
                        result.set_result(m_ircontext, make_result_target);
                    }

                    uint16_t elem_count = (uint16_t)node->m_elements.size();
                    m_ircontext.c().mkarr(WO_OPNUM(make_result_target), elem_count);
                });
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueFunctionCall)
    {
        if (state == UNPROCESSED)
        {
            if (node->m_function->node_type == AstBase::AST_VALUE_VARIABLE)
            {
                AstValueVariable* func_var = static_cast<AstValueVariable*>(node->m_function);
                auto ir_normal_function_instance = func_var->m_LANG_variable_instance.value()->m_IR_normal_function;

                // If no captured variables, we can call near.
                node->m_IR_invoking_function_near = ir_normal_function_instance;
            }
            else if (node->m_function->node_type == AstBase::AST_VALUE_FUNCTION)
            {
                ast::AstValueFunction* invoking_target_function = static_cast<ast::AstValueFunction*>(node->m_function);
                if (invoking_target_function->m_LANG_captured_context.m_captured_variables.empty())
                {
                    // No captured variables, we can call near.
                    node->m_IR_invoking_function_near = invoking_target_function;

                    // What ever, we still need compiler known this function.
                    m_ircontext.eval_ignore();
                    WO_CONTINUE_PROCESS(node->m_function);
                }
            }
            bool has_invoking_function_near = node->m_IR_invoking_function_near.has_value();
            if (has_invoking_function_near)
            {
                auto* function = node->m_IR_invoking_function_near.value();
                if (function->m_IR_extern_information.has_value())
                {
                    auto* extern_information = function->m_IR_extern_information.value();
                    if (extern_information->m_IR_externed_function.has_value())
                    {
                        auto* externed_function = extern_information->m_IR_externed_function.value();
                        if (config::ENABLE_SKIP_INVOKE_UNSAFE_CAST
                            && (void*)&rslib_std_return_itself == (void*)externed_function)
                        {
                            // Optimized for rslib_std_return_itself.
                            m_ircontext.eval_for_upper();
                            WO_CONTINUE_PROCESS(node->m_arguments.front());

                            node->m_LANG_hold_state = AstValueFunctionCall::IR_HOLD_FOR_FAST_ITSELF;
                            return HOLD;
                        }
                    }
                }
            }

            bool argument_count_is_sured = true;
            for (auto* arguments : node->m_arguments)
            {
                // Functions arguments will be pushed into stack inversely.
                // So here we eval them by origin order, stack pop will be in reverse order.

                if (arguments->node_type == AstBase::AST_FAKE_VALUE_UNPACK)
                    m_ircontext.eval_action();
                else
                    m_ircontext.eval_push();

                WO_CONTINUE_PROCESS(arguments);
            }

            if (node->m_LANG_invoking_variadic_function)
            {
                m_ircontext.c().psh(WO_OPNUM(m_ircontext.opnum_spreg(opnum::reg::tc)));
                m_ircontext.c().mov(
                    WO_OPNUM(m_ircontext.opnum_spreg(opnum::reg::tc)),
                    WO_OPNUM(m_ircontext.opnum_imm_int(node->m_LANG_certenly_function_argument_count)));
            }

            if (!has_invoking_function_near)
            {
                // We need eval it.
                m_ircontext.eval_keep();
                WO_CONTINUE_PROCESS(node->m_function);
            }

            node->m_LANG_hold_state = AstValueFunctionCall::IR_HOLD_FOR_NORMAL_CALL;
            return HOLD;
        }
        else if (state == HOLD)
        {
            switch (node->m_LANG_hold_state)
            {
            case AstValueFunctionCall::IR_HOLD_FOR_FAST_ITSELF:
                m_ircontext.cleanup_for_eval_upper();
                break;
            case AstValueFunctionCall::IR_HOLD_FOR_NORMAL_CALL:
            {
                if (node->m_IR_invoking_function_near.has_value())
                {
                    AstValueFunction* invoking_function = node->m_IR_invoking_function_near.value();
                    wo_assert(invoking_function->m_LANG_captured_context.m_captured_variables.empty());

                    if (invoking_function->m_IR_extern_information.has_value()
                        && 0 == (invoking_function->m_IR_extern_information.value()->m_attribute_flags & AstExternInformation::SLOW))
                    {
                        m_ircontext.c().callfast(
                            (void*)invoking_function->m_IR_extern_information.value()->m_IR_externed_function.value());
                    }
                    else
                        m_ircontext.c().call(WO_OPNUM(IR_function_opnum(invoking_function)));
                }
                else
                {
                    auto* opnumbase = m_ircontext.get_eval_result();
                    m_ircontext.c().call(WO_OPNUM(opnumbase));

                    m_ircontext.try_return_opnum_temporary_register(opnumbase);
                }

                if (!node->m_LANG_has_runtime_full_unpackargs)
                    m_ircontext.c().pop(
                        node->m_LANG_certenly_function_argument_count);
                else
                    m_ircontext.c().ext_popn(
                        WO_OPNUM(m_ircontext.opnum_spreg(opnum::reg::tc)));

                if (node->m_LANG_invoking_variadic_function)
                    m_ircontext.c().pop(
                        WO_OPNUM(m_ircontext.opnum_spreg(opnum::reg::tc)));

                // Ok, invoke finished.
                m_ircontext.apply_eval_result(
                    [&](BytecodeGenerateContext::EvalResult& result)
                    {
                        const auto& target_storage = result.get_assign_target();
                        if (target_storage.has_value())
                        {
                            if (opnum::reg* target_reg = dynamic_cast<opnum::reg*>(target_storage.value());
                                target_reg == nullptr || target_reg->id != opnum::reg::cr)
                            {
                                m_ircontext.c().mov(
                                    WO_OPNUM(target_storage.value()),
                                    WO_OPNUM(m_ircontext.opnum_spreg(opnum::reg::cr)));
                            }
                            // Or do nothing, target is same.
                        }
                        else
                            result.set_result(
                                m_ircontext, m_ircontext.opnum_spreg(opnum::reg::cr));
                    });

                break;
            }
            default:
                break;
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
            m_ircontext.apply_eval_result(
                [&](BytecodeGenerateContext::EvalResult& result)
                {
                    const auto& target_storage = result.get_assign_target();
                    auto* function_opnum = IR_function_opnum(func);
                    if (target_storage.has_value())
                    {
                        m_ircontext.c().mov(
                            WO_OPNUM(target_storage.value()),
                            WO_OPNUM(function_opnum));
                    }
                    else
                        result.set_result(m_ircontext, function_opnum);
                });
            return OKAY;
        }

        wo_assert(value_instance->IR_need_storage());

        if (!value_instance->m_IR_storage.has_value())
        {
            lex.record_lang_error(lexer::msglevel_t::error, node,
                WO_ERR_VARIBALE_STORAGE_NOT_DETERMINED,
                get_value_name_w(value_instance));

            if (value_instance->m_symbol->m_symbol_declare_ast.has_value())
                lex.record_lang_error(lexer::msglevel_t::infom,
                    value_instance->m_symbol->m_symbol_declare_ast.value(),
                    WO_INFO_SYMBOL_NAMED_DEFINED_HERE,
                    get_value_name_w(value_instance));

            return FAILED;
        }

        auto& variable_storage = value_instance->m_IR_storage.value();

        if (_is_storage_can_addressing(variable_storage))
        {
            // In global or stack range, we can use direct access.
            auto* storage_opnum =
                m_ircontext.get_storage_place(variable_storage);

            m_ircontext.apply_eval_result(
                [&](BytecodeGenerateContext::EvalResult& result)
                {
                    const auto& target_storage = result.get_assign_target();
                    if (target_storage.has_value())
                    {
                        m_ircontext.c().mov(
                            WO_OPNUM(target_storage.value()),
                            WO_OPNUM(storage_opnum));
                    }
                    else
                        result.set_result(m_ircontext, storage_opnum);
                });
        }
        else
        {
            m_ircontext.apply_eval_result(
                [&](BytecodeGenerateContext::EvalResult& result)
                {
                    const auto& target_storage = result.get_assign_target();
                    if (target_storage.has_value())
                    {
                        m_ircontext.c().lds(
                            WO_OPNUM(target_storage.value()),
                            WO_OPNUM(m_ircontext.opnum_imm_int(variable_storage.m_index)));
                    }
                    else
                    {
                        auto* storage_opnum = m_ircontext.borrow_opnum_temporary_register(WO_BORROW_TEMPORARY_FROM(node));
                        m_ircontext.c().lds(
                            WO_OPNUM(storage_opnum),
                            WO_OPNUM(m_ircontext.opnum_imm_int(variable_storage.m_index)));
                        result.set_result(m_ircontext, storage_opnum);
                    }
                });
        }

        return OKAY;
    }
    WO_PASS_PROCESSER(AstValueMarkAsMutable)
    {
        if (state == UNPROCESSED)
        {
            m_ircontext.eval_for_upper();
            WO_CONTINUE_PROCESS(node->m_marked_value);
            return HOLD;
        }
        else if (state == HOLD)
        {
            m_ircontext.cleanup_for_eval_upper();
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueMarkAsImmutable)
    {
        if (state == UNPROCESSED)
        {
            m_ircontext.eval_for_upper();
            WO_CONTINUE_PROCESS(node->m_marked_value);
            return HOLD;
        }
        else if (state == HOLD)
        {
            m_ircontext.cleanup_for_eval_upper();
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueTypeCheckAs)
    {
        if (state == UNPROCESSED)
        {
            if (node->m_IR_dynamic_need_runtime_check)
                m_ircontext.eval_sth_if_not_ignore(&BytecodeGenerateContext::eval);
            else
                m_ircontext.eval_for_upper();

            WO_CONTINUE_PROCESS(node->m_check_value);
            return HOLD;
        }
        else if (state == HOLD)
        {
            if (node->m_IR_dynamic_need_runtime_check)
            {
                m_ircontext.apply_eval_result(
                    [&](BytecodeGenerateContext::EvalResult& result)
                    {
                        auto* opnum_to_check = m_ircontext.get_eval_result();
                        // Runtime check here.

                        auto* target_type_instance =
                            node->m_check_type->m_LANG_determined_type.value();
                        auto* target_determined_type_instance =
                            target_type_instance->get_determined_type().value();

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
                            break;
                        default:
                            wo_error("Unknown type.");
                            break;
                        }
                        m_ircontext.c().typeas(WO_OPNUM(opnum_to_check), check_type);

                        const auto& target_storage = result.get_assign_target();
                        if (target_storage.has_value())
                        {
                            m_ircontext.c().mov(
                                WO_OPNUM(target_storage.value()),
                                WO_OPNUM(opnum_to_check));
                        }
                        else
                        {
                            m_ircontext.try_keep_opnum_temporary_register(opnum_to_check WO_BORROW_TEMPORARY_FROM_SP(node));
                            result.set_result(m_ircontext, opnum_to_check);
                        }
                    });
            }
            else
                m_ircontext.cleanup_for_eval_upper();
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueTypeCheckIs)
    {
        // Only dynamic check can be here.
        if (state == UNPROCESSED)
        {
            wo_assert(
                m_origin_types.m_dynamic.m_type_instance ==
                immutable_type(node->m_check_value->m_LANG_determined_type.value()));

            m_ircontext.eval_sth_if_not_ignore(
                &BytecodeGenerateContext::eval);

            WO_CONTINUE_PROCESS(node->m_check_value);
            return HOLD;
        }
        else if (state == HOLD)
        {
            m_ircontext.apply_eval_result(
                [&](BytecodeGenerateContext::EvalResult& result)
                {
                    auto* opnum_to_check = m_ircontext.get_eval_result();

                    auto* target_type_instance =
                        node->m_check_type->m_LANG_determined_type.value();
                    auto* target_determined_type_instance =
                        target_type_instance->get_determined_type().value();

                    const auto& target_storage = result.get_assign_target();

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
                        break;
                    default:
                        wo_error("Unknown type.");
                        break;
                    }

                    m_ircontext.c().typeis(WO_OPNUM(opnum_to_check), check_type);

                    if (target_storage.has_value())
                    {
                        if (opnum::reg* target_reg = dynamic_cast<opnum::reg*>(target_storage.value());
                            target_reg == nullptr || target_reg->id != opnum::reg::cr)
                        {
                            m_ircontext.c().mov(
                                WO_OPNUM(target_storage.value()),
                                WO_OPNUM(m_ircontext.opnum_spreg(opnum::reg::cr)));
                        }
                        // Or do nothing, target is same.
                    }
                    else
                    {
                        result.set_result(
                            m_ircontext, m_ircontext.opnum_spreg(opnum::reg::spreg::cr));
                    }
                });
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

            auto* src_type_instance =
                node->m_cast_value->m_LANG_determined_type.value();
            auto* src_determined_type_instance =
                src_type_instance->get_determined_type().value();

            if (target_determined_type_instance->m_base_type
                == lang_TypeInstance::DeterminedType::VOID)
            {
                // Here we need mark this sign to apply `eval_ignore`.
                node->m_IR_need_eval = true;
                m_ircontext.eval_ignore();
            }
            else if (target_determined_type_instance->m_base_type
                != src_determined_type_instance->m_base_type
                && target_determined_type_instance->m_base_type
                != lang_TypeInstance::DeterminedType::DYNAMIC
                && src_determined_type_instance->m_base_type
                != lang_TypeInstance::DeterminedType::NOTHING)
            {
                // Need runtime check.
                node->m_IR_need_eval = true;

                m_ircontext.eval_sth_if_not_ignore(
                    &BytecodeGenerateContext::eval);
            }
            else
            {
                // No cast
                node->m_IR_need_eval = false;

                m_ircontext.eval_for_upper();
            }

            // Eval.
            WO_CONTINUE_PROCESS(node->m_cast_value);

            return HOLD;
        }
        else if (state == HOLD)
        {
            if (node->m_IR_need_eval)
            {
                m_ircontext.apply_eval_result(
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

                        const auto& target_storage = result.get_assign_target();

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
                                    m_ircontext, m_ircontext.opnum_spreg(opnum::reg::spreg::ni));
                            }
                            return;
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
                            auto* borrowed_reg = m_ircontext.borrow_opnum_temporary_register(
                                WO_BORROW_TEMPORARY_FROM(node));

                            m_ircontext.c().movcast(
                                WO_OPNUM(borrowed_reg),
                                WO_OPNUM(opnum_to_cast),
                                cast_type);
                            result.set_result(m_ircontext, borrowed_reg);
                        }
                    });
            }
            else
                m_ircontext.cleanup_for_eval_upper();
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueDoAsVoid)
    {
        if (state == UNPROCESSED)
        {
            // Cast to void, just ignore it.
            m_ircontext.eval_ignore();
            WO_CONTINUE_PROCESS(node->m_do_value);

            return HOLD;
        }
        else if (state == HOLD)
        {
            m_ircontext.apply_eval_result(
                [&](BytecodeGenerateContext::EvalResult& result)
                {
                    const auto& target_storage = result.get_assign_target();

                    if (target_storage.has_value())
                    {
                        // NO NEED TO DO ANYTHING.
                        // VOID VALUE IS PURE JUNK VALUE.
                    }
                    else
                    {
                        // Return a junk value.
                        result.set_result(
                            m_ircontext, m_ircontext.opnum_spreg(opnum::reg::spreg::ni));
                    }
                });
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }

    void check_and_generate_check_ir_for_divi_and_modi(
        BytecodeGenerateContext& bgc,
        const std::optional<ast::AstValueBase*>& left,
        ast::AstValueBase* right,
        opnum::opnumbase* left_opnum,
        opnum::opnumbase* right_opnum)
    {
        if (!config::ENABLE_RUNTIME_CHECKING_INTEGER_DIVISION)
            // Skip runtime check if disabled.
            return;

        if (right->m_evaled_const_value.has_value())
        {
            wo_integer_t right_value = right->m_evaled_const_value.value().value_integer();
            wo_assert(right_value != 0);
            wo_assert(!left.has_value() || !left.value()->m_evaled_const_value.has_value());

            if (right_value == -1)
                // Need check l
                bgc.c().ext_cdivil(*left_opnum);

            // Otherwise, no need to check.
        }
        else if (left.has_value() && left.value()->m_evaled_const_value.has_value())
        {
            wo_integer_t left_value = left.value()->m_evaled_const_value.value().value_integer();
            if (left_value == INT64_MIN)
                // Need check r
                bgc.c().ext_cdivir(*right_opnum);
            else
                // Need check rz
                bgc.c().ext_cdivirz(*right_opnum);
        }
        else
        {
            // Left & right both not constant.
            bgc.c().ext_cdivilr(*left_opnum, *right_opnum);
        }
    }

    WO_PASS_PROCESSER(AstFakeValueUnpack)
    {
        if (state == UNPROCESSED)
        {
            if (node->m_IR_unpack_method == AstFakeValueUnpack::SHOULD_NOT_UNPACK)
            {
                lex.record_lang_error(lexer::msglevel_t::error, node,
                    WO_ERR_CANNOT_UNPACK_HERE);

                return FAILED;
            }

            m_ircontext.eval_sth_if_not_ignore(
                &BytecodeGenerateContext::eval);

            WO_CONTINUE_PROCESS(node->m_unpack_value);
            return HOLD;
        }
        else if (state == HOLD)
        {
            m_ircontext.apply_eval_result(
                [&](BytecodeGenerateContext::EvalResult& result)
                {
                    auto* unpacking_opnum = m_ircontext.get_eval_result();

                    if (node->m_IR_unpack_method == AstFakeValueUnpack::UNPACK_FOR_TUPLE)
                    {
                        m_ircontext.try_keep_opnum_temporary_register(
                            unpacking_opnum
                            WO_BORROW_TEMPORARY_FROM_SP(node));

                        auto* unpacking_tuple_determined_type =
                            node->m_LANG_determined_type.value()->get_determined_type().value();

                        wo_assert(unpacking_tuple_determined_type->m_base_type == lang_TypeInstance::DeterminedType::TUPLE);
                        auto* tuple_info = unpacking_tuple_determined_type->m_external_type_description.m_tuple;
                        uint16_t tuple_elem_count = (uint16_t)tuple_info->m_element_types.size();

                        for (uint16_t i = 0; i < tuple_elem_count; ++i)
                        {
                            auto* tmp = m_ircontext.borrow_opnum_temporary_register(
                                WO_BORROW_TEMPORARY_FROM(node));
                            m_ircontext.c().idstruct(
                                WO_OPNUM(tmp),
                                WO_OPNUM(unpacking_opnum),
                                i);
                            m_ircontext.c().psh(WO_OPNUM(tmp));

                            m_ircontext.return_opnum_temporary_register(tmp);

                        }
                        m_ircontext.try_return_opnum_temporary_register(unpacking_opnum);
                    }
                    else
                    {
                        const auto& unpack_requirement = node->m_IR_need_to_be_unpack_count.value();

                        if (unpack_requirement.m_unpack_all)
                        {
                            m_ircontext.c().unpackargs(
                                WO_OPNUM(unpacking_opnum),
                                -(int32_t)unpack_requirement.m_require_unpack_count);
                        }
                        else
                        {
                            // If require to unpack 0 argument, just skip & ignore.
                            if (unpack_requirement.m_require_unpack_count != 0)
                                m_ircontext.c().unpackargs(
                                    WO_OPNUM(unpacking_opnum),
                                    (int32_t)unpack_requirement.m_require_unpack_count);
                        }
                    }
                });
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueBinaryOperator)
    {
        if (state == UNPROCESSED)
        {
            node->m_LANG_hold_state = AstValueBinaryOperator::IR_HOLD_FOR_NORMAL_LR_OR_OVERLOAD_EVAL;
            if (node->m_LANG_overload_call.has_value())
            {
                m_ircontext.eval_for_upper();
                WO_CONTINUE_PROCESS(node->m_LANG_overload_call.value());
            }
            else if (node->m_operator == AstValueBinaryOperator::LOGICAL_AND
                || node->m_operator == AstValueBinaryOperator::LOGICAL_OR)
            {
                // Need short cut, 
                m_ircontext.eval_to(m_ircontext.opnum_spreg(opnum::reg::cr));
                WO_CONTINUE_PROCESS(node->m_left);

                node->m_LANG_hold_state = AstValueBinaryOperator::IR_HOLD_FOR_LAND_LOR_LEFT_SHORT_CUT;
            }
            else
            {
                m_ircontext.eval_sth_if_not_ignore(
                    &BytecodeGenerateContext::eval);
                WO_CONTINUE_PROCESS(node->m_right);

                m_ircontext.eval_sth_if_not_ignore(
                    &BytecodeGenerateContext::eval_keep);
                WO_CONTINUE_PROCESS(node->m_left);
            }
            return HOLD;
        }
        else if (state == HOLD)
        {
            switch (node->m_LANG_hold_state)
            {
            case AstValueBinaryOperator::IR_HOLD_FOR_LAND_LOR_LEFT_SHORT_CUT:
            {
                (void)m_ircontext.get_eval_result();

                switch (node->m_operator)
                {
                case AstValueBinaryOperator::LOGICAL_AND:
                    m_ircontext.c().jf(opnum::tag(_generate_label("#lshortcut_", node)));
                    break;
                case AstValueBinaryOperator::LOGICAL_OR:
                    m_ircontext.c().jt(opnum::tag(_generate_label("#lshortcut_", node)));
                    break;
                default:
                    wo_error("Unknown operator.");
                    break;
                }

                if (!m_ircontext.eval_result_ignored())
                    m_ircontext.eval_to(m_ircontext.opnum_spreg(opnum::reg::cr));
                else
                    m_ircontext.eval_ignore();

                WO_CONTINUE_PROCESS(node->m_right);

                node->m_LANG_hold_state = AstValueBinaryOperator::IR_HOLD_FOR_LAND_LOR_RIGHT;
                return HOLD;
            }
            case  AstValueBinaryOperator::IR_HOLD_FOR_LAND_LOR_RIGHT:
            {
                m_ircontext.c().tag(_generate_label("#lshortcut_", node));

                m_ircontext.apply_eval_result(
                    [&](BytecodeGenerateContext::EvalResult& result)
                    {
                        (void)m_ircontext.get_eval_result();

                        const auto& target_storage = result.get_assign_target();
                        if (target_storage.has_value())
                        {
                            if (opnum::reg* target_reg = dynamic_cast<opnum::reg*>(target_storage.value());
                                target_reg == nullptr || target_reg->id != opnum::reg::cr)
                            {
                                m_ircontext.c().mov(
                                    WO_OPNUM(target_storage.value()),
                                    WO_OPNUM(m_ircontext.opnum_spreg(opnum::reg::cr)));
                            }
                            // Or do nothing, target is same.
                        }
                        else
                        {
                            result.set_result(
                                m_ircontext, m_ircontext.opnum_spreg(opnum::reg::spreg::cr));
                        }
                    }
                );
                break;
            }
            case AstValueBinaryOperator::IR_HOLD_FOR_NORMAL_LR_OR_OVERLOAD_EVAL:
                if (!node->m_LANG_overload_call.has_value())
                {
                    m_ircontext.apply_eval_result(
                        [&](BytecodeGenerateContext::EvalResult& result)
                        {
                            auto* right_opnum = m_ircontext.get_eval_result();
                            auto* left_opnum = m_ircontext.get_eval_result();

                            // We need to make sure following `borrow_opnum_temporary_register`
                            // will not borrow the right value.
                            m_ircontext.try_keep_opnum_temporary_register(
                                right_opnum
                                WO_BORROW_TEMPORARY_FROM_SP(node));

                            if (node->m_operator < AstValueBinaryOperator::LOGICAL_AND
                                && nullptr == dynamic_cast<opnum::temporary*>(left_opnum))
                            {
                                // Is not temporary register, we need keep it.
                                // NOTE: If left not temporary, we dont need to return it.
                                //  at the same time.
                                auto* borrow_reg = m_ircontext.borrow_opnum_temporary_register(
                                    WO_BORROW_TEMPORARY_FROM(node));
                                m_ircontext.c().mov(
                                    WO_OPNUM(borrow_reg),
                                    WO_OPNUM(left_opnum));

                                left_opnum = borrow_reg;
                            }

                            // After all borrow_opnum_temporary_register, we can return right.
                            m_ircontext.try_return_opnum_temporary_register(right_opnum);

                            lang_TypeInstance* left_type_instance = node->m_left->m_LANG_determined_type.value();
                            auto* left_determined_type = left_type_instance->get_determined_type().value();

                            switch (node->m_operator)
                            {
                            case AstValueBinaryOperator::ADD:
                                switch (left_determined_type->m_base_type)
                                {
                                case lang_TypeInstance::DeterminedType::INTEGER:
                                    m_ircontext.c().addi(WO_OPNUM(left_opnum), WO_OPNUM(right_opnum));
                                    break;
                                case lang_TypeInstance::DeterminedType::REAL:
                                    m_ircontext.c().addr(WO_OPNUM(left_opnum), WO_OPNUM(right_opnum));
                                    break;
                                case lang_TypeInstance::DeterminedType::HANDLE:
                                    m_ircontext.c().addh(WO_OPNUM(left_opnum), WO_OPNUM(right_opnum));
                                    break;
                                case lang_TypeInstance::DeterminedType::STRING:
                                    m_ircontext.c().adds(WO_OPNUM(left_opnum), WO_OPNUM(right_opnum));
                                    break;
                                default:
                                    wo_error("Unknown type.");
                                    break;
                                }
                                break;
                            case AstValueBinaryOperator::SUBSTRACT:
                                switch (left_determined_type->m_base_type)
                                {
                                case lang_TypeInstance::DeterminedType::INTEGER:
                                    m_ircontext.c().subi(WO_OPNUM(left_opnum), WO_OPNUM(right_opnum));
                                    break;
                                case lang_TypeInstance::DeterminedType::REAL:
                                    m_ircontext.c().subr(WO_OPNUM(left_opnum), WO_OPNUM(right_opnum));
                                    break;
                                case lang_TypeInstance::DeterminedType::HANDLE:
                                    m_ircontext.c().subh(WO_OPNUM(left_opnum), WO_OPNUM(right_opnum));
                                    break;
                                default:
                                    wo_error("Unknown type.");
                                    break;
                                }
                                break;
                            case AstValueBinaryOperator::MULTIPLY:
                                switch (left_determined_type->m_base_type)
                                {
                                case lang_TypeInstance::DeterminedType::INTEGER:
                                    m_ircontext.c().muli(WO_OPNUM(left_opnum), WO_OPNUM(right_opnum));
                                    break;
                                case lang_TypeInstance::DeterminedType::REAL:
                                    m_ircontext.c().mulr(WO_OPNUM(left_opnum), WO_OPNUM(right_opnum));
                                    break;
                                default:
                                    wo_error("Unknown type.");
                                    break;
                                }
                                break;
                            case AstValueBinaryOperator::DIVIDE:
                                switch (left_determined_type->m_base_type)
                                {
                                case lang_TypeInstance::DeterminedType::INTEGER:
                                    check_and_generate_check_ir_for_divi_and_modi(
                                        m_ircontext, node->m_left, node->m_right, left_opnum, right_opnum);
                                    m_ircontext.c().divi(WO_OPNUM(left_opnum), WO_OPNUM(right_opnum));
                                    break;
                                case lang_TypeInstance::DeterminedType::REAL:
                                    m_ircontext.c().divr(WO_OPNUM(left_opnum), WO_OPNUM(right_opnum));
                                    break;
                                default:
                                    wo_error("Unknown type.");
                                    break;
                                }
                                break;
                            case AstValueBinaryOperator::MODULO:
                                switch (left_determined_type->m_base_type)
                                {
                                case lang_TypeInstance::DeterminedType::INTEGER:
                                    check_and_generate_check_ir_for_divi_and_modi(
                                        m_ircontext, node->m_left, node->m_right, left_opnum, right_opnum);
                                    m_ircontext.c().modi(WO_OPNUM(left_opnum), WO_OPNUM(right_opnum));
                                    break;
                                case lang_TypeInstance::DeterminedType::REAL:
                                    m_ircontext.c().modr(WO_OPNUM(left_opnum), WO_OPNUM(right_opnum));
                                    break;
                                default:
                                    wo_error("Unknown type.");
                                    break;
                                }
                                break;
                            case AstValueBinaryOperator::LOGICAL_AND:
                                m_ircontext.c().land(WO_OPNUM(left_opnum), WO_OPNUM(right_opnum));
                                break;
                            case AstValueBinaryOperator::LOGICAL_OR:
                                m_ircontext.c().lor(WO_OPNUM(left_opnum), WO_OPNUM(right_opnum));
                                break;
                            case AstValueBinaryOperator::GREATER:
                                switch (left_determined_type->m_base_type)
                                {
                                case lang_TypeInstance::DeterminedType::INTEGER:
                                    m_ircontext.c().gti(WO_OPNUM(left_opnum), WO_OPNUM(right_opnum));
                                    break;
                                case lang_TypeInstance::DeterminedType::REAL:
                                    m_ircontext.c().gtr(WO_OPNUM(left_opnum), WO_OPNUM(right_opnum));
                                    break;
                                case lang_TypeInstance::DeterminedType::HANDLE:
                                case lang_TypeInstance::DeterminedType::STRING:
                                    m_ircontext.c().gtx(WO_OPNUM(left_opnum), WO_OPNUM(right_opnum));
                                    break;
                                default:
                                    wo_error("Unknown type.");
                                    break;
                                }
                                break;
                            case AstValueBinaryOperator::GREATER_EQUAL:
                                switch (left_determined_type->m_base_type)
                                {
                                case lang_TypeInstance::DeterminedType::INTEGER:
                                    m_ircontext.c().egti(WO_OPNUM(left_opnum), WO_OPNUM(right_opnum));
                                    break;
                                case lang_TypeInstance::DeterminedType::REAL:
                                    m_ircontext.c().egtr(WO_OPNUM(left_opnum), WO_OPNUM(right_opnum));
                                    break;
                                case lang_TypeInstance::DeterminedType::HANDLE:
                                case lang_TypeInstance::DeterminedType::STRING:
                                    m_ircontext.c().egtx(WO_OPNUM(left_opnum), WO_OPNUM(right_opnum));
                                    break;
                                default:
                                    wo_error("Unknown type.");
                                    break;
                                }
                                break;
                            case AstValueBinaryOperator::LESS:
                                switch (left_determined_type->m_base_type)
                                {
                                case lang_TypeInstance::DeterminedType::INTEGER:
                                    m_ircontext.c().lti(WO_OPNUM(left_opnum), WO_OPNUM(right_opnum));
                                    break;
                                case lang_TypeInstance::DeterminedType::REAL:
                                    m_ircontext.c().ltr(WO_OPNUM(left_opnum), WO_OPNUM(right_opnum));
                                    break;
                                case lang_TypeInstance::DeterminedType::HANDLE:
                                case lang_TypeInstance::DeterminedType::STRING:
                                    m_ircontext.c().ltx(WO_OPNUM(left_opnum), WO_OPNUM(right_opnum));
                                    break;
                                default:
                                    wo_error("Unknown type.");
                                    break;
                                }
                                break;
                            case AstValueBinaryOperator::LESS_EQUAL:
                                switch (left_determined_type->m_base_type)
                                {
                                case lang_TypeInstance::DeterminedType::INTEGER:
                                    m_ircontext.c().elti(WO_OPNUM(left_opnum), WO_OPNUM(right_opnum));
                                    break;
                                case lang_TypeInstance::DeterminedType::REAL:
                                    m_ircontext.c().eltr(WO_OPNUM(left_opnum), WO_OPNUM(right_opnum));
                                    break;
                                case lang_TypeInstance::DeterminedType::HANDLE:
                                case lang_TypeInstance::DeterminedType::STRING:
                                    m_ircontext.c().eltx(WO_OPNUM(left_opnum), WO_OPNUM(right_opnum));
                                    break;
                                default:
                                    wo_error("Unknown type.");
                                    break;
                                }
                                break;
                            case AstValueBinaryOperator::EQUAL:
                                switch (left_determined_type->m_base_type)
                                {
                                case lang_TypeInstance::DeterminedType::HANDLE:
                                case lang_TypeInstance::DeterminedType::INTEGER:
                                case lang_TypeInstance::DeterminedType::BOOLEAN:
                                    m_ircontext.c().equb(WO_OPNUM(left_opnum), WO_OPNUM(right_opnum));
                                    break;
                                case lang_TypeInstance::DeterminedType::REAL:
                                    m_ircontext.c().equr(WO_OPNUM(left_opnum), WO_OPNUM(right_opnum));
                                    break;
                                case lang_TypeInstance::DeterminedType::STRING:
                                    m_ircontext.c().equs(WO_OPNUM(left_opnum), WO_OPNUM(right_opnum));
                                    break;
                                default:
                                    wo_error("Unknown type.");
                                    break;
                                }
                                break;
                            case AstValueBinaryOperator::NOT_EQUAL:
                                switch (left_determined_type->m_base_type)
                                {
                                case lang_TypeInstance::DeterminedType::HANDLE:
                                case lang_TypeInstance::DeterminedType::INTEGER:
                                case lang_TypeInstance::DeterminedType::BOOLEAN:
                                    m_ircontext.c().nequb(WO_OPNUM(left_opnum), WO_OPNUM(right_opnum));
                                    break;
                                case lang_TypeInstance::DeterminedType::REAL:
                                    m_ircontext.c().nequr(WO_OPNUM(left_opnum), WO_OPNUM(right_opnum));
                                    break;
                                case lang_TypeInstance::DeterminedType::STRING:
                                    m_ircontext.c().nequs(WO_OPNUM(left_opnum), WO_OPNUM(right_opnum));
                                    break;
                                default:
                                    wo_error("Unknown type.");
                                    break;
                                }
                                break;
                            default:
                                wo_error("Unknown operator.");
                                break;
                            }

                            const auto& target_storage = result.get_assign_target();
                            if (node->m_operator < AstValueBinaryOperator::LOGICAL_AND)
                            {
                                // Calculate result stored at left_opnum.
                                if (target_storage.has_value())
                                {
                                    m_ircontext.try_return_opnum_temporary_register(left_opnum);
                                    m_ircontext.c().mov(
                                        WO_OPNUM(target_storage.value()),
                                        WO_OPNUM(left_opnum));
                                }
                                else
                                {
                                    result.set_result(m_ircontext, left_opnum);
                                }
                            }
                            else
                            {
                                // Calculate result stored at cr.
                                m_ircontext.try_return_opnum_temporary_register(left_opnum);
                                if (target_storage.has_value())
                                {
                                    if (opnum::reg* target_reg = dynamic_cast<opnum::reg*>(target_storage.value());
                                        target_reg == nullptr || target_reg->id != opnum::reg::cr)
                                    {
                                        m_ircontext.c().mov(
                                            WO_OPNUM(target_storage.value()),
                                            WO_OPNUM(m_ircontext.opnum_spreg(opnum::reg::cr)));
                                    }
                                    // Or do nothing, target is same.
                                }
                                else
                                {
                                    result.set_result(
                                        m_ircontext, m_ircontext.opnum_spreg(opnum::reg::cr));
                                }
                            }
                        });
                }
                else
                    m_ircontext.cleanup_for_eval_upper();

                break;
            }
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueUnaryOperator)
    {
        if (state == UNPROCESSED)
        {
            m_ircontext.eval_sth_if_not_ignore(
                &BytecodeGenerateContext::eval);
            WO_CONTINUE_PROCESS(node->m_operand);
            return HOLD;
        }
        else if (state == HOLD)
        {
            m_ircontext.apply_eval_result(
                [&](BytecodeGenerateContext::EvalResult& result)
                {
                    const auto& target_storage = result.get_assign_target();
                    auto* opnum_to_unary = m_ircontext.get_eval_result();

                    switch (node->m_operator)
                    {
                    case AstValueUnaryOperator::NEGATIVE:
                    {
                        opnum::opnumbase* target_result_opnum = nullptr;
                        if (target_storage.has_value())
                            target_result_opnum = target_storage.value();
                        else
                        {
                            m_ircontext.try_keep_opnum_temporary_register(opnum_to_unary
                                WO_BORROW_TEMPORARY_FROM_SP(node));

                            target_result_opnum = m_ircontext.borrow_opnum_temporary_register(
                                WO_BORROW_TEMPORARY_FROM(node));
                            result.set_result(m_ircontext, target_result_opnum);

                            m_ircontext.try_return_opnum_temporary_register(opnum_to_unary);
                        }

                        lang_TypeInstance* operand_type_instance =
                            node->m_operand->m_LANG_determined_type.value();
                        auto operand_determined_type =
                            operand_type_instance->get_determined_type().value();

                        switch (operand_determined_type->m_base_type)
                        {
                        case lang_TypeInstance::DeterminedType::INTEGER:
                        {
                            m_ircontext.c().mov(WO_OPNUM(target_result_opnum), WO_OPNUM(m_ircontext.opnum_imm_int(0)));
                            m_ircontext.c().subi(WO_OPNUM(target_result_opnum), WO_OPNUM(opnum_to_unary));
                            break;
                        }
                        case lang_TypeInstance::DeterminedType::REAL:
                        {
                            m_ircontext.c().mov(WO_OPNUM(target_result_opnum), WO_OPNUM(m_ircontext.opnum_imm_real(0.)));
                            m_ircontext.c().subr(WO_OPNUM(target_result_opnum), WO_OPNUM(opnum_to_unary));
                            break;
                        }
                        default:
                            wo_error("Unknown type.");
                            break;
                        }
                        break;
                    }
                    case AstValueUnaryOperator::LOGICAL_NOT:
                    {
                        m_ircontext.c().equb(
                            WO_OPNUM(opnum_to_unary),
                            WO_OPNUM(m_ircontext.opnum_spreg(opnum::reg::ni)));

                        if (target_storage.has_value())
                        {
                            if (opnum::reg* target_reg = dynamic_cast<opnum::reg*>(target_storage.value());
                                target_reg == nullptr || target_reg->id != opnum::reg::cr)
                            {
                                m_ircontext.c().mov(
                                    WO_OPNUM(target_storage.value()),
                                    WO_OPNUM(m_ircontext.opnum_spreg(opnum::reg::cr)));
                            }
                            // Or do nothing, target is same.
                        }
                        else
                        {
                            result.set_result(
                                m_ircontext, m_ircontext.opnum_spreg(opnum::reg::cr));
                        }
                        break;
                    }
                    default:
                        wo_error("Unknown operator.");
                        break;
                    }
                }
            );
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueTribleOperator)
    {
        if (state == UNPROCESSED)
        {
            if (node->m_condition->m_evaled_const_value.has_value())
            {
                m_ircontext.eval_for_upper();

                if (node->m_condition->m_evaled_const_value.value().value_integer() != 0)
                    WO_CONTINUE_PROCESS(node->m_true_value);
                else
                    WO_CONTINUE_PROCESS(node->m_false_value);

                node->m_LANG_hold_state = AstValueTribleOperator::IR_HOLD_FOR_BRANCH_CONST_EVAL;
            }
            else
            {
                m_ircontext.eval_to(m_ircontext.opnum_spreg(opnum::reg::spreg::cr));
                WO_CONTINUE_PROCESS(node->m_condition);

                node->m_LANG_hold_state = AstValueTribleOperator::IR_HOLD_FOR_COND_EVAL;
            }
            return HOLD;
        }
        else if (state == HOLD)
        {
            switch (node->m_LANG_hold_state)
            {
            case AstValueTribleOperator::IR_HOLD_FOR_COND_EVAL:
            {
                m_ircontext.c().jf(opnum::tag(_generate_label("#cond_false_", node)));

                m_ircontext.eval_to_if_not_ignore(
                    m_ircontext.opnum_spreg(opnum::reg::spreg::cr));

                WO_CONTINUE_PROCESS(node->m_true_value);

                node->m_LANG_hold_state = AstValueTribleOperator::IR_HOLD_FOR_BRANCH_A_EVAL;
                return HOLD;
            }
            case AstValueTribleOperator::IR_HOLD_FOR_BRANCH_A_EVAL:
            {
                m_ircontext.c().jmp(opnum::tag(_generate_label("#cond_end_", node)));
                m_ircontext.c().tag(_generate_label("#cond_false_", node));

                m_ircontext.eval_to_if_not_ignore(
                    m_ircontext.opnum_spreg(opnum::reg::spreg::cr));

                WO_CONTINUE_PROCESS(node->m_false_value);

                node->m_LANG_hold_state = AstValueTribleOperator::IR_HOLD_FOR_BRANCH_B_EVAL;
                return HOLD;
            }
            case AstValueTribleOperator::IR_HOLD_FOR_BRANCH_B_EVAL:
            {
                m_ircontext.c().tag(_generate_label("#cond_end_", node));

                m_ircontext.apply_eval_result(
                    [&](BytecodeGenerateContext::EvalResult& result)
                    {
                        // Ignore results.
                        (void)m_ircontext.get_eval_result(); // False branch result.
                        (void)m_ircontext.get_eval_result(); // True branch result.
                        (void)m_ircontext.get_eval_result(); // Condition result.

                        const auto& target_storage = result.get_assign_target();
                        if (target_storage.has_value())
                        {
                            if (opnum::reg* target_reg = dynamic_cast<opnum::reg*>(target_storage.value());
                                target_reg == nullptr || target_reg->id != opnum::reg::cr)
                            {
                                m_ircontext.c().mov(
                                    WO_OPNUM(target_storage.value()),
                                    WO_OPNUM(m_ircontext.opnum_spreg(opnum::reg::cr)));
                            }
                            // Or do nothing, target is same.
                        }
                        else
                        {
                            result.set_result(
                                m_ircontext, m_ircontext.opnum_spreg(opnum::reg::spreg::cr));
                        }
                    }
                );
                break;
            }
            case AstValueTribleOperator::IR_HOLD_FOR_BRANCH_CONST_EVAL:
                m_ircontext.cleanup_for_eval_upper();
                break;
            default:
                wo_error("Unknown hold state.");
                break;
            }
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueIndex)
    {
        if (state == UNPROCESSED)
        {
            if (!node->m_LANG_fast_index_for_struct.has_value())
            {
                m_ircontext.eval_sth_if_not_ignore(
                    &BytecodeGenerateContext::eval);
                WO_CONTINUE_PROCESS(node->m_index);

                m_ircontext.eval_sth_if_not_ignore(
                    &BytecodeGenerateContext::eval_keep);
                WO_CONTINUE_PROCESS(node->m_container);
            }
            else
            {
                m_ircontext.eval_sth_if_not_ignore(
                    &BytecodeGenerateContext::eval);
                WO_CONTINUE_PROCESS(node->m_container);
            }

            return HOLD;
        }
        else if (state == HOLD)
        {
            m_ircontext.apply_eval_result(
                [&](BytecodeGenerateContext::EvalResult& result)
                {
                    const auto& target_storage = result.get_assign_target();

                    lang_TypeInstance* container_type_instance =
                        node->m_container->m_LANG_determined_type.value();
                    auto* determined_container_type =
                        container_type_instance->get_determined_type().value();

                    switch (determined_container_type->m_base_type)
                    {
                    case lang_TypeInstance::DeterminedType::ARRAY:
                    case lang_TypeInstance::DeterminedType::VECTOR:
                    case lang_TypeInstance::DeterminedType::DICTIONARY:
                    case lang_TypeInstance::DeterminedType::MAPPING:
                    case lang_TypeInstance::DeterminedType::STRING:
                    {
                        auto* index_opnum = m_ircontext.get_eval_result();
                        auto* container_opnum = m_ircontext.get_eval_result();

                        wo_assert(!node->m_LANG_fast_index_for_struct.has_value());
                        m_ircontext.try_return_opnum_temporary_register(container_opnum);

                        // NOTE: Keep index & container if is IR OPNUM node.
                        if (node->m_index->node_type == AstBase::AST_VALUE_IR_OPNUM)
                            m_ircontext.try_keep_opnum_temporary_register(
                                index_opnum
                                WO_BORROW_TEMPORARY_FROM_SP(node));
                        if (node->m_container->node_type == AstBase::AST_VALUE_IR_OPNUM)
                            m_ircontext.try_keep_opnum_temporary_register(
                                index_opnum
                                WO_BORROW_TEMPORARY_FROM_SP(node));

                        if (determined_container_type->m_base_type == lang_TypeInstance::DeterminedType::ARRAY
                            || determined_container_type->m_base_type == lang_TypeInstance::DeterminedType::VECTOR)
                            m_ircontext.c().idarr(
                                WO_OPNUM(container_opnum),
                                WO_OPNUM(index_opnum));
                        else if (determined_container_type->m_base_type == lang_TypeInstance::DeterminedType::STRING)
                            m_ircontext.c().idstr(
                                WO_OPNUM(container_opnum),
                                WO_OPNUM(index_opnum));
                        else
                            m_ircontext.c().iddict(
                                WO_OPNUM(container_opnum),
                                WO_OPNUM(index_opnum));

                        if (target_storage.has_value())
                        {
                            if (opnum::reg* target_reg = dynamic_cast<opnum::reg*>(target_storage.value());
                                target_reg == nullptr || target_reg->id != opnum::reg::cr)
                            {
                                m_ircontext.c().mov(
                                    WO_OPNUM(target_storage.value()),
                                    WO_OPNUM(m_ircontext.opnum_spreg(opnum::reg::cr)));
                            }
                        }
                        else
                        {
                            result.set_result(
                                m_ircontext, m_ircontext.opnum_spreg(opnum::reg::spreg::cr));
                        }
                        break;
                    }
                    case lang_TypeInstance::DeterminedType::STRUCT:
                    case lang_TypeInstance::DeterminedType::TUPLE:
                    {
                        auto* container_opnum = m_ircontext.get_eval_result();

                        wo_assert(node->m_LANG_fast_index_for_struct.has_value());

                        // NOTE: Keep index & container if is IR OPNUM node.
                        if (node->m_container->node_type == AstBase::AST_VALUE_IR_OPNUM)
                            m_ircontext.try_keep_opnum_temporary_register(
                                container_opnum
                                WO_BORROW_TEMPORARY_FROM_SP(node));

                        uint16_t fast_index = (uint16_t)node->m_LANG_fast_index_for_struct.value();
                        if (target_storage.has_value())
                        {
                            m_ircontext.c().idstruct(
                                WO_OPNUM(target_storage.value()),
                                WO_OPNUM(container_opnum),
                                fast_index);
                        }
                        else
                        {
                            auto* borrowed_reg = m_ircontext.borrow_opnum_temporary_register(
                                WO_BORROW_TEMPORARY_FROM(node));
                            m_ircontext.c().idstruct(
                                WO_OPNUM(borrowed_reg),
                                WO_OPNUM(container_opnum),
                                fast_index);
                            result.set_result(m_ircontext, borrowed_reg);
                        }
                        break;
                    }
                    default:
                        wo_error("Unknown type.");
                        break;
                    }
                }
            );
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueStruct)
    {
        if (state == UNPROCESSED)
        {
            std::vector<AstStructFieldValuePair*> struct_value_tobe_make(
                node->m_fields.rbegin(), node->m_fields.rend());

            if (node->m_marked_struct_type.has_value())
            {
                // Making specify struct.
                auto* struct_type_instance = node->m_marked_struct_type.value()->m_LANG_determined_type.value();
                auto* struct_determined_type = struct_type_instance->get_determined_type().value();
                auto* struct_detail_info = struct_determined_type->m_external_type_description.m_struct;

                std::sort(struct_value_tobe_make.begin(), struct_value_tobe_make.end(),
                    [&struct_detail_info](AstStructFieldValuePair* a, AstStructFieldValuePair* b)
                    {
                        const auto offset_a = struct_detail_info->m_member_types.at(a->m_name).m_offset;
                        const auto offset_b = struct_detail_info->m_member_types.at(b->m_name).m_offset;
                        return offset_a > offset_b;
                    });
            }

            for (auto* field_value : struct_value_tobe_make)
            {
                m_ircontext.eval_sth_if_not_ignore(
                    &BytecodeGenerateContext::eval_push);
                WO_CONTINUE_PROCESS(field_value->m_value);
            }

            return HOLD;
        }
        else if (state == HOLD)
        {
            m_ircontext.apply_eval_result(
                [&](BytecodeGenerateContext::EvalResult& result)
                {
                    const auto& target_storage = result.get_assign_target();
                    if (target_storage.has_value())
                    {
                        m_ircontext.c().mkstruct(
                            WO_OPNUM(target_storage.value()),
                            (uint16_t)node->m_fields.size());
                    }
                    else
                    {
                        auto* borrowed_reg = m_ircontext.borrow_opnum_temporary_register(
                            WO_BORROW_TEMPORARY_FROM(node));
                        m_ircontext.c().mkstruct(
                            WO_OPNUM(borrowed_reg),
                            (uint16_t)node->m_fields.size());
                        result.set_result(m_ircontext, borrowed_reg);
                    }
                }
            );
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueMakeUnion)
    {
        if (state == UNPROCESSED)
        {
            if (node->m_packed_value.has_value())
            {
                m_ircontext.eval_sth_if_not_ignore(
                    &BytecodeGenerateContext::eval);

                WO_CONTINUE_PROCESS(node->m_packed_value.value());
            }
            return HOLD;
        }
        else
        {
            if (node->m_packed_value.has_value())
            {
                m_ircontext.apply_eval_result(
                    [&](BytecodeGenerateContext::EvalResult& result)
                    {
                        auto* packed_opnum = m_ircontext.get_eval_result();

                        const auto& target_storage = result.get_assign_target();
                        if (target_storage.has_value())
                        {
                            m_ircontext.c().mkunion(
                                WO_OPNUM(target_storage.value()),
                                WO_OPNUM(packed_opnum),
                                (uint16_t)node->m_index);
                        }
                        else
                        {
                            auto* borrowed_reg = m_ircontext.borrow_opnum_temporary_register(
                                WO_BORROW_TEMPORARY_FROM(node));
                            m_ircontext.c().mkunion(
                                WO_OPNUM(borrowed_reg),
                                WO_OPNUM(packed_opnum),
                                (uint16_t)node->m_index);
                            result.set_result(m_ircontext, borrowed_reg);
                        }
                    }
                );
            }
            else
            {
                m_ircontext.apply_eval_result(
                    [&](BytecodeGenerateContext::EvalResult& result)
                    {
                        const auto& target_storage = result.get_assign_target();
                        if (target_storage.has_value())
                        {
                            m_ircontext.c().mkunion(
                                WO_OPNUM(target_storage.value()),
                                WO_OPNUM(m_ircontext.opnum_spreg(opnum::reg::spreg::ni)),
                                (uint16_t)node->m_index);
                        }
                        else
                        {
                            auto* borrowed_reg = m_ircontext.borrow_opnum_temporary_register(
                                WO_BORROW_TEMPORARY_FROM(node));
                            m_ircontext.c().mkunion(
                                WO_OPNUM(borrowed_reg),
                                WO_OPNUM(m_ircontext.opnum_spreg(opnum::reg::spreg::ni)),
                                (uint16_t)node->m_index);
                            result.set_result(m_ircontext, borrowed_reg);
                        }
                    }
                );
            }
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueVariadicArgumentsPack)
    {
        wo_assert(state == UNPROCESSED);
        m_ircontext.apply_eval_result(
            [&](BytecodeGenerateContext::EvalResult& result)
            {
                AstValueFunction* current_func = node->m_LANG_function_instance.value();

                const auto& target_storage = result.get_assign_target();
                if (target_storage.has_value())
                {
                    m_ircontext.c().ext_packargs(
                        WO_OPNUM(target_storage.value()),
                        (uint16_t)current_func->m_parameters.size(),
                        (uint16_t)current_func->m_LANG_captured_context.m_captured_variables.size());
                }
                else
                {
                    auto* borrowed_opnum = m_ircontext.borrow_opnum_temporary_register(
                        WO_BORROW_TEMPORARY_FROM(node));
                    m_ircontext.c().ext_packargs(
                        WO_OPNUM(borrowed_opnum),
                        (uint16_t)current_func->m_parameters.size(),
                        (uint16_t)current_func->m_LANG_captured_context.m_captured_variables.size());
                    result.set_result(m_ircontext, borrowed_opnum);
                }
            }
        );
        return OKAY;
    }
    WO_PASS_PROCESSER(AstValueIROpnum)
    {
        wo_assert(state == UNPROCESSED);
        m_ircontext.apply_eval_result(
            [&](BytecodeGenerateContext::EvalResult& result)
            {
                const auto& target_storage = result.get_assign_target();
                if (target_storage.has_value())
                {
                    m_ircontext.c().mov(
                        WO_OPNUM(target_storage.value()),
                        WO_OPNUM(node->m_opnum));
                }
                else
                {
                    result.set_result(m_ircontext, node->m_opnum);
                }
            }
        );
        return OKAY;
    }
    WO_PASS_PROCESSER(AstValueAssign)
    {
        // Variable:
        //      V = E               V/E
        //      V += E              V/tmp
        //      V = op+ V E         cr
        // Index:
        //      C[I] = E            E
        //      C[I] += E           tmp
        //      C[I] = op+ C[I] E   cr

        if (state == UNPROCESSED)
        {
            if (node->m_assign_place->node_type == AstBase::AST_PATTERN_VARIABLE)
            {
                AstPatternVariable* assign_var = static_cast<AstPatternVariable*>(node->m_assign_place);
                lang_ValueInstance* assign_value_instance = assign_var->m_variable->m_LANG_variable_instance.value();

                if (!assign_value_instance->m_IR_storage.has_value())
                {
                    lex.record_lang_error(lexer::msglevel_t::error, node,
                        WO_ERR_VARIBALE_STORAGE_NOT_DETERMINED,
                        get_value_name_w(assign_value_instance));

                    if (assign_value_instance->m_symbol->m_symbol_declare_ast.has_value())
                        lex.record_lang_error(lexer::msglevel_t::infom,
                            assign_value_instance->m_symbol->m_symbol_declare_ast.value(),
                            WO_INFO_SYMBOL_NAMED_DEFINED_HERE,
                            get_value_name_w(assign_value_instance));

                    return FAILED;
                }

                auto& storage = assign_value_instance->m_IR_storage.value();

                switch (node->m_assign_type)
                {
                case AstValueAssign::ASSIGN:
                    if (_is_storage_can_addressing(storage))
                        m_ircontext.eval_to(m_ircontext.get_storage_place(storage));
                    else
                        m_ircontext.eval();

                    WO_CONTINUE_PROCESS(node->m_right);
                    break;
                default:
                    if (node->m_LANG_overload_call.has_value())
                    {
                        // WTF! WTF! WTF!
                        // IT'S FXXKING FUNCTION OVERLOAD CALL!

                        // We just eval it and assigned it to value.
                        if (_is_storage_can_addressing(storage))
                            m_ircontext.eval_to(m_ircontext.get_storage_place(storage));
                        else
                            m_ircontext.eval();

                        WO_CONTINUE_PROCESS(node->m_LANG_overload_call.value());
                    }
                    else
                    {
                        m_ircontext.eval();
                        WO_CONTINUE_PROCESS(node->m_right);
                    }
                    break;
                }

                node->m_LANG_hold_state = AstValueAssign::IR_HOLD_TO_APPLY_ASSIGN;
            }
            else
            {
                wo_assert(node->m_assign_place->node_type == AstBase::AST_PATTERN_INDEX);
                AstPatternIndex* pattern_index = static_cast<AstPatternIndex*>(node->m_assign_place);
                // What ever, we need eval container and index for following operation;

                node->m_LANG_hold_state = AstValueAssign::IR_HOLD_FOR_INDEX_PATTERN_EVAL;
                switch (node->m_assign_type)
                {
                case AstValueAssign::ASSIGN:
                    m_ircontext.eval();
                    WO_CONTINUE_PROCESS(node->m_right);

                    node->m_LANG_hold_state = AstValueAssign::IR_HOLD_TO_APPLY_ASSIGN;

                    /* FALL THROUGH */
                    [[fallthrough]];
                default:
                    if (!pattern_index->m_index->m_LANG_fast_index_for_struct.has_value())
                    {
                        m_ircontext.eval_keep();
                        WO_CONTINUE_PROCESS(pattern_index->m_index->m_index);
                    }

                    m_ircontext.eval_keep();
                    WO_CONTINUE_PROCESS(pattern_index->m_index->m_container);
                    break;
                }
            }
            return HOLD;
        }
        else if (state == HOLD)
        {
            switch (node->m_LANG_hold_state)
            {
            case AstValueAssign::IR_HOLD_FOR_INDEX_PATTERN_EVAL:
            {
                AstPatternIndex* pattern_index = static_cast<AstPatternIndex*>(node->m_assign_place);

                wo_assert(node->m_assign_type != AstValueAssign::ASSIGN
                    && node->m_assign_place->node_type == AstBase::AST_PATTERN_INDEX);

                wo_assert(!node->m_LANG_overload_call.has_value()
                    || node->m_LANG_overload_call.value()->m_arguments.front() == pattern_index->m_index);

                // ATTENTION: We will do bad things to update index op's container & index by AstValueIROpnum
                if (!pattern_index->m_index->m_LANG_fast_index_for_struct.has_value())
                {
                    AstValueIROpnum* index_opnum = new AstValueIROpnum(m_ircontext.get_eval_result());
                    index_opnum->m_LANG_determined_type = pattern_index->m_index->m_index->m_LANG_determined_type;
                    index_opnum->source_location = pattern_index->m_index->m_index->source_location;
                    pattern_index->m_index->m_index = index_opnum;
                }

                AstValueIROpnum* container_opnum = new AstValueIROpnum(m_ircontext.get_eval_result());
                container_opnum->m_LANG_determined_type = pattern_index->m_index->m_container->m_LANG_determined_type;
                container_opnum->source_location = pattern_index->m_index->m_container->source_location;
                pattern_index->m_index->m_container = container_opnum;

                if (node->m_LANG_overload_call.has_value())
                {
                    m_ircontext.eval();
                    WO_CONTINUE_PROCESS(node->m_LANG_overload_call.value());
                }
                else
                {
                    m_ircontext.eval();
                    WO_CONTINUE_PROCESS(node->m_right);

                    m_ircontext.eval_keep();
                    WO_CONTINUE_PROCESS(pattern_index->m_index);
                }

                node->m_LANG_hold_state = AstValueAssign::IR_HOLD_TO_APPLY_ASSIGN;
                return HOLD;
            }
            case AstValueAssign::IR_HOLD_TO_APPLY_ASSIGN:
            {
                opnum::opnumbase* assign_expr_result_opnum;

                if (node->m_assign_place->node_type == AstBase::AST_PATTERN_VARIABLE)
                {
                    AstPatternVariable* assign_var = static_cast<AstPatternVariable*>(node->m_assign_place);
                    lang_ValueInstance* assign_value_instance = assign_var->m_variable->m_LANG_variable_instance.value();

                    auto& storage = assign_value_instance->m_IR_storage.value();

                    switch (node->m_assign_type)
                    {
                    case AstValueAssign::ASSIGN:
                        assign_expr_result_opnum = m_ircontext.get_eval_result();
                        break;
                    default:
                        if (node->m_LANG_overload_call.has_value())
                            assign_expr_result_opnum = m_ircontext.get_eval_result();
                        else
                        {
                            auto* right_value_result = m_ircontext.get_eval_result();

                            // Do normal assign operate;
                            if (_is_storage_can_addressing(storage))
                                assign_expr_result_opnum = m_ircontext.get_storage_place(storage);
                            else
                            {
                                assign_expr_result_opnum = m_ircontext.borrow_opnum_temporary_register(
                                    WO_BORROW_TEMPORARY_FROM(node));
                                m_ircontext.c().lds(
                                    WO_OPNUM(assign_expr_result_opnum),
                                    WO_OPNUM(m_ircontext.opnum_imm_int(storage.m_index)));
                            }

                            lang_TypeInstance* assign_type_instance = assign_var->m_variable->m_LANG_determined_type.value();
                            auto* determined_assign_type = assign_type_instance->get_determined_type().value();

                            switch (node->m_assign_type)
                            {
                            case AstValueAssign::ADD_ASSIGN:
                                switch (determined_assign_type->m_base_type)
                                {
                                case lang_TypeInstance::DeterminedType::INTEGER:
                                    m_ircontext.c().addi(
                                        WO_OPNUM(assign_expr_result_opnum),
                                        WO_OPNUM(right_value_result));
                                    break;
                                case lang_TypeInstance::DeterminedType::REAL:
                                    m_ircontext.c().addr(
                                        WO_OPNUM(assign_expr_result_opnum),
                                        WO_OPNUM(right_value_result));
                                    break;
                                case lang_TypeInstance::DeterminedType::HANDLE:
                                    m_ircontext.c().addh(
                                        WO_OPNUM(assign_expr_result_opnum),
                                        WO_OPNUM(right_value_result));
                                    break;
                                case lang_TypeInstance::DeterminedType::STRING:
                                    m_ircontext.c().adds(
                                        WO_OPNUM(assign_expr_result_opnum),
                                        WO_OPNUM(right_value_result));
                                    break;
                                default:
                                    wo_error("Unknown type.");
                                    break;
                                }
                                break;
                            case AstValueAssign::SUBSTRACT_ASSIGN:
                                switch (determined_assign_type->m_base_type)
                                {
                                case lang_TypeInstance::DeterminedType::INTEGER:
                                    m_ircontext.c().subi(
                                        WO_OPNUM(assign_expr_result_opnum),
                                        WO_OPNUM(right_value_result));
                                    break;
                                case lang_TypeInstance::DeterminedType::HANDLE:
                                    m_ircontext.c().subh(
                                        WO_OPNUM(assign_expr_result_opnum),
                                        WO_OPNUM(right_value_result));
                                    break;
                                case lang_TypeInstance::DeterminedType::REAL:
                                    m_ircontext.c().subr(
                                        WO_OPNUM(assign_expr_result_opnum),
                                        WO_OPNUM(right_value_result));
                                    break;
                                default:
                                    wo_error("Unknown type.");
                                    break;
                                }
                                break;
                            case AstValueAssign::MULTIPLY_ASSIGN:
                                switch (determined_assign_type->m_base_type)
                                {
                                case lang_TypeInstance::DeterminedType::INTEGER:
                                    m_ircontext.c().muli(
                                        WO_OPNUM(assign_expr_result_opnum),
                                        WO_OPNUM(right_value_result));
                                    break;
                                case lang_TypeInstance::DeterminedType::REAL:
                                    m_ircontext.c().mulr(
                                        WO_OPNUM(assign_expr_result_opnum),
                                        WO_OPNUM(right_value_result));
                                    break;
                                default:
                                    wo_error("Unknown type.");
                                    break;
                                }
                                break;
                            case AstValueAssign::DIVIDE_ASSIGN:
                                switch (determined_assign_type->m_base_type)
                                {
                                case lang_TypeInstance::DeterminedType::INTEGER:
                                    check_and_generate_check_ir_for_divi_and_modi(
                                        m_ircontext, std::nullopt, node->m_right,
                                        assign_expr_result_opnum, right_value_result);
                                    m_ircontext.c().divi(
                                        WO_OPNUM(assign_expr_result_opnum),
                                        WO_OPNUM(right_value_result));
                                    break;
                                case lang_TypeInstance::DeterminedType::REAL:
                                    m_ircontext.c().divr(
                                        WO_OPNUM(assign_expr_result_opnum),
                                        WO_OPNUM(right_value_result));
                                    break;
                                default:
                                    wo_error("Unknown type.");
                                    break;
                                }
                                break;
                            case AstValueAssign::MODULO_ASSIGN:
                                switch (determined_assign_type->m_base_type)
                                {
                                case lang_TypeInstance::DeterminedType::INTEGER:
                                    check_and_generate_check_ir_for_divi_and_modi(
                                        m_ircontext, std::nullopt, node->m_right,
                                        assign_expr_result_opnum, right_value_result);
                                    m_ircontext.c().modi(
                                        WO_OPNUM(assign_expr_result_opnum),
                                        WO_OPNUM(right_value_result));
                                    break;
                                case lang_TypeInstance::DeterminedType::REAL:
                                    m_ircontext.c().modr(
                                        WO_OPNUM(assign_expr_result_opnum),
                                        WO_OPNUM(right_value_result));
                                    break;
                                default:
                                    wo_error("Unknown type.");
                                    break;
                                }
                                break;
                            default:
                                wo_error("Unknown operator.");
                                break;
                            }

                            if (_is_storage_can_addressing(storage))
                                // Assign completed.
                                ;
                            else
                                m_ircontext.try_return_opnum_temporary_register(
                                    assign_expr_result_opnum);
                        }
                        break;
                    }

                    /////////////////////// EVAL FINISHED ///////////////////////

                    if (_is_storage_can_addressing(storage))
                        // Nothing todo, assign has been complete.
                        ;
                    else
                    {
                        m_ircontext.c().sts(
                            WO_OPNUM(assign_expr_result_opnum),
                            WO_OPNUM(m_ircontext.opnum_imm_int(storage.m_index)));
                    }
                }
                else
                {
                    // ATTENTION: Here still 2(or 1) temporary reg stores in AstValueIROpnum need to be free manually.
                    wo_assert(node->m_assign_place->node_type == AstBase::AST_PATTERN_INDEX);
                    AstPatternIndex* pattern_index = static_cast<AstPatternIndex*>(node->m_assign_place);

                    lang_TypeInstance* assign_type_instance = pattern_index->m_index->m_LANG_determined_type.value();
                    auto* determined_assign_type = assign_type_instance->get_determined_type().value();

                    lang_TypeInstance* container_type_instance =
                        pattern_index->m_index->m_container->m_LANG_determined_type.value();

                    auto* determined_container_type = container_type_instance->get_determined_type().value();

                    opnum::opnumbase* container_opnum;
                    opnum::opnumbase* index_opnum;

                    switch (node->m_assign_type)
                    {
                    case AstValueAssign::ASSIGN:
                    {
                        assign_expr_result_opnum = m_ircontext.get_eval_result();

                        if (!pattern_index->m_index->m_LANG_fast_index_for_struct.has_value())
                            index_opnum = m_ircontext.get_eval_result();

                        container_opnum = m_ircontext.get_eval_result();
                        break;
                    }
                    default:
                    {
                        container_opnum = static_cast<AstValueIROpnum*>(
                            pattern_index->m_index->m_container)->m_opnum;
                        if (!pattern_index->m_index->m_LANG_fast_index_for_struct.has_value())
                            index_opnum = static_cast<AstValueIROpnum*>(
                                pattern_index->m_index->m_index)->m_opnum;

                        if (node->m_LANG_overload_call.has_value())
                            assign_expr_result_opnum = m_ircontext.get_eval_result();
                        else
                        {
                            auto* right_value_result = m_ircontext.get_eval_result();
                            assign_expr_result_opnum = m_ircontext.get_eval_result();

                            m_ircontext.try_return_opnum_temporary_register(assign_expr_result_opnum);

                            switch (node->m_assign_type)
                            {
                            case AstValueAssign::ADD_ASSIGN:
                                switch (determined_assign_type->m_base_type)
                                {
                                case lang_TypeInstance::DeterminedType::INTEGER:
                                    m_ircontext.c().addi(
                                        WO_OPNUM(assign_expr_result_opnum),
                                        WO_OPNUM(right_value_result));
                                    break;
                                case lang_TypeInstance::DeterminedType::REAL:
                                    m_ircontext.c().addr(
                                        WO_OPNUM(assign_expr_result_opnum),
                                        WO_OPNUM(right_value_result));
                                    break;
                                case lang_TypeInstance::DeterminedType::HANDLE:
                                    m_ircontext.c().addh(
                                        WO_OPNUM(assign_expr_result_opnum),
                                        WO_OPNUM(right_value_result));
                                    break;
                                case lang_TypeInstance::DeterminedType::STRING:
                                    m_ircontext.c().adds(
                                        WO_OPNUM(assign_expr_result_opnum),
                                        WO_OPNUM(right_value_result));
                                    break;
                                default:
                                    wo_error("Unknown type.");
                                    break;
                                }
                                break;
                            case AstValueAssign::SUBSTRACT_ASSIGN:
                                switch (determined_assign_type->m_base_type)
                                {
                                case lang_TypeInstance::DeterminedType::INTEGER:
                                    m_ircontext.c().subi(
                                        WO_OPNUM(assign_expr_result_opnum),
                                        WO_OPNUM(right_value_result));
                                    break;
                                case lang_TypeInstance::DeterminedType::HANDLE:
                                    m_ircontext.c().subh(
                                        WO_OPNUM(assign_expr_result_opnum),
                                        WO_OPNUM(right_value_result));
                                    break;
                                case lang_TypeInstance::DeterminedType::REAL:
                                    m_ircontext.c().subr(
                                        WO_OPNUM(assign_expr_result_opnum),
                                        WO_OPNUM(right_value_result));
                                    break;
                                default:
                                    wo_error("Unknown type.");
                                    break;
                                }
                                break;
                            case AstValueAssign::MULTIPLY_ASSIGN:
                                switch (determined_assign_type->m_base_type)
                                {
                                case lang_TypeInstance::DeterminedType::INTEGER:
                                    m_ircontext.c().muli(
                                        WO_OPNUM(assign_expr_result_opnum),
                                        WO_OPNUM(right_value_result));
                                    break;
                                case lang_TypeInstance::DeterminedType::REAL:
                                    m_ircontext.c().mulr(
                                        WO_OPNUM(assign_expr_result_opnum),
                                        WO_OPNUM(right_value_result));
                                    break;
                                default:
                                    wo_error("Unknown type.");
                                    break;
                                }
                                break;
                            case AstValueAssign::DIVIDE_ASSIGN:
                                switch (determined_assign_type->m_base_type)
                                {
                                case lang_TypeInstance::DeterminedType::INTEGER:
                                    check_and_generate_check_ir_for_divi_and_modi(
                                        m_ircontext, std::nullopt, node->m_right,
                                        assign_expr_result_opnum, right_value_result);
                                    m_ircontext.c().divi(
                                        WO_OPNUM(assign_expr_result_opnum),
                                        WO_OPNUM(right_value_result));
                                    break;
                                case lang_TypeInstance::DeterminedType::REAL:
                                    m_ircontext.c().divr(
                                        WO_OPNUM(assign_expr_result_opnum),
                                        WO_OPNUM(right_value_result));
                                    break;
                                default:
                                    wo_error("Unknown type.");
                                    break;
                                }
                                break;
                            case AstValueAssign::MODULO_ASSIGN:
                                switch (determined_assign_type->m_base_type)
                                {
                                case lang_TypeInstance::DeterminedType::INTEGER:
                                    check_and_generate_check_ir_for_divi_and_modi(
                                        m_ircontext, std::nullopt, node->m_right,
                                        assign_expr_result_opnum, right_value_result);
                                    m_ircontext.c().modi(
                                        WO_OPNUM(assign_expr_result_opnum),
                                        WO_OPNUM(right_value_result));
                                    break;
                                case lang_TypeInstance::DeterminedType::REAL:
                                    m_ircontext.c().modr(
                                        WO_OPNUM(assign_expr_result_opnum),
                                        WO_OPNUM(right_value_result));
                                    break;
                                default:
                                    wo_error("Unknown type.");
                                    break;
                                }
                                break;
                            default:
                                wo_error("Unknown operator.");
                                break;
                            }
                        }
                        break;
                    }
                    }

                    /////////////////////// EVAL FINISHED ///////////////////////

                    switch (determined_container_type->m_base_type)
                    {
                    case lang_TypeInstance::DeterminedType::ARRAY:
                    case lang_TypeInstance::DeterminedType::VECTOR:
                    case lang_TypeInstance::DeterminedType::DICTIONARY:
                    case lang_TypeInstance::DeterminedType::MAPPING:
                    {
                        auto* assigned_right_value = assign_expr_result_opnum;

                        bool is_register = dynamic_cast<opnum::reg*>(assigned_right_value) != nullptr;

                        if (!is_register
                            && dynamic_cast<opnum::temporary*>(assigned_right_value) == nullptr)
                        {
                            is_register = false;
                            auto* borrowed_reg = m_ircontext.borrow_opnum_temporary_register(
                                WO_BORROW_TEMPORARY_FROM(node));
                            m_ircontext.c().mov(
                                WO_OPNUM(borrowed_reg),
                                WO_OPNUM(assigned_right_value));
                            m_ircontext.return_opnum_temporary_register(borrowed_reg);
                            assigned_right_value = borrowed_reg;
                        }

                        if (is_register)
                        {
                            if (determined_container_type->m_base_type == lang_TypeInstance::DeterminedType::DICTIONARY)
                                m_ircontext.c().siddict(
                                    WO_OPNUM(container_opnum),
                                    WO_OPNUM(index_opnum),
                                    *dynamic_cast<opnum::reg*>(assigned_right_value));
                            else if (determined_container_type->m_base_type == lang_TypeInstance::DeterminedType::MAPPING)
                                m_ircontext.c().sidmap(
                                    WO_OPNUM(container_opnum),
                                    WO_OPNUM(index_opnum),
                                    *dynamic_cast<opnum::reg*>(assigned_right_value));
                            else
                                m_ircontext.c().sidarr(
                                    WO_OPNUM(container_opnum),
                                    WO_OPNUM(index_opnum),
                                    *dynamic_cast<opnum::reg*>(assigned_right_value));
                        }
                        else
                        {
                            if (determined_container_type->m_base_type == lang_TypeInstance::DeterminedType::DICTIONARY)
                                m_ircontext.c().siddict(
                                    WO_OPNUM(container_opnum),
                                    WO_OPNUM(index_opnum),
                                    *dynamic_cast<opnum::temporary*>(assigned_right_value));
                            else if (determined_container_type->m_base_type == lang_TypeInstance::DeterminedType::MAPPING)
                                m_ircontext.c().sidmap(
                                    WO_OPNUM(container_opnum),
                                    WO_OPNUM(index_opnum),
                                    *dynamic_cast<opnum::temporary*>(assigned_right_value));
                            else
                                m_ircontext.c().sidarr(
                                    WO_OPNUM(container_opnum),
                                    WO_OPNUM(index_opnum),
                                    *dynamic_cast<opnum::temporary*>(assigned_right_value));
                        }
                        break;
                    }
                    case lang_TypeInstance::DeterminedType::STRUCT:
                    case lang_TypeInstance::DeterminedType::TUPLE:
                    {
                        m_ircontext.c().sidstruct(
                            WO_OPNUM(container_opnum),
                            WO_OPNUM(assign_expr_result_opnum),
                            (int16_t)pattern_index->m_index->m_LANG_fast_index_for_struct.value());
                        break;
                    }
                    default:
                        wo_error("Unknown type.");
                        break;
                    }

                    if (!pattern_index->m_index->m_LANG_fast_index_for_struct.has_value())
                        m_ircontext.try_return_opnum_temporary_register(index_opnum);
                    m_ircontext.try_return_opnum_temporary_register(container_opnum);
                }

                m_ircontext.apply_eval_result(
                    [&](BytecodeGenerateContext::EvalResult& result)
                    {
                        const auto& target_storage = result.get_assign_target();
                        if (target_storage.has_value())
                        {
                            if (node->m_valued_assign)
                                m_ircontext.c().mov(
                                    WO_OPNUM(target_storage.value()),
                                    WO_OPNUM(assign_expr_result_opnum));
                            else
                                // No valued return, junk it.
                                ;
                        }
                        else
                        {
                            if (node->m_valued_assign)
                            {
                                m_ircontext.try_keep_opnum_temporary_register(
                                    assign_expr_result_opnum
                                    WO_BORROW_TEMPORARY_FROM_SP(node));
                                result.set_result(m_ircontext, assign_expr_result_opnum);
                            }
                            else
                                // Give a junk value.
                                result.set_result(
                                    m_ircontext, m_ircontext.opnum_spreg(opnum::reg::spreg::ni));
                        }
                    }
                );
                break;
            }
            default:
                wo_error("Unknown hold state.");
                break;
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

            if (type_instance->m_symbol != m_origin_types.m_void.m_symbol
                && type_instance->m_symbol != m_origin_types.m_nothing.m_symbol)
            {
                lex.record_lang_error(lexer::msglevel_t::error, eval_value,
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
        wo_assert(node_state.m_ast_node->node_type == AstBase::AST_EMPTY
            || m_passir_A_processers->check_has_processer(node_state.m_ast_node->node_type));

        m_ircontext.c().pdb_info->generate_debug_info_at_astnode(node_state.m_ast_node, &m_ircontext.c());
        return m_passir_A_processers->process_node(this, lex, node_state, out_stack);
    }
    LangContext::pass_behavior LangContext::pass_final_B_process_bytecode_generation(
        lexer& lex, const AstNodeWithState& node_state, PassProcessStackT& out_stack)
    {
        // PASS1 must process all nodes.
        wo_assert(node_state.m_ast_node->node_type == AstBase::AST_EMPTY
            || m_passir_B_processers->check_has_processer(node_state.m_ast_node->node_type));
        wo_assert(node_state.m_ast_node->node_type >= AstBase::AST_VALUE_begin
            && node_state.m_ast_node->node_type < AstBase::AST_VALUE_end);

        AstValueBase* ast_value = static_cast<AstValueBase*>(node_state.m_ast_node);
        if (ast_value->m_evaled_const_value.has_value())
        {
            // This value has been evaluated as constant value.
            m_ircontext.apply_eval_result(
                [&](BytecodeGenerateContext::EvalResult& result)
                {
                    opnum::opnumbase* immediately_value =
                        m_ircontext.opnum_imm_value(ast_value->m_evaled_const_value.value());

                    const auto& asigned_target = result.get_assign_target();
                    if (asigned_target.has_value())
                        m_ircontext.c().mov(WO_OPNUM(asigned_target.value()), WO_OPNUM(immediately_value));
                    else
                        result.set_result(m_ircontext, immediately_value);
                });
            return OKAY;
        }

        m_ircontext.c().pdb_info->generate_debug_info_at_astnode(node_state.m_ast_node, &m_ircontext.c());
        auto compile_result =
            m_passir_B_processers->process_node(this, lex, node_state, out_stack);

        if (compile_result == FAILED)
            m_ircontext.failed_eval_result();

        return compile_result;
    }

#endif
}