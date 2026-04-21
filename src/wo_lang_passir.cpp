#include "wo_afx.hpp"

namespace wo
{
#ifndef WO_DISABLE_COMPILER
    using namespace ast;

    void BytecodeGenerateContext::begin_loop_while(ast::AstWhile* ast)
    {
        m_loop_content_stack.push_back(
            LoopContent{
                ast->m_LANG_binded_label.has_value()
                    ? std::optional(ast->m_LANG_binded_label.value()->m_label)
                    : std::nullopt,
                c().named_label(ast, "#while_end"),
                c().named_label(ast, "#while_begin")
            });
    }
    void BytecodeGenerateContext::begin_loop_for(ast::AstFor* ast)
    {
        m_loop_content_stack.push_back(
            LoopContent{
                ast->m_LANG_binded_label.has_value()
                    ? std::optional(ast->m_LANG_binded_label.value()->m_label)
                    : std::nullopt,
                c().named_label(ast, "#for_end"),
                c().named_label(ast, "#for_next"),
            });
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

    void BytecodeGenerateContext::end_loop()
    {
        wo_assert(!m_loop_content_stack.empty());
        m_loop_content_stack.pop_back();
    }

    bool LangContext::update_instance_storage_and_code_gen_passir(
        lang_ValueInstance* instance,
        const woort_IRValue* opnumval,
        const std::optional<uint16_t>& tuple_member_offset)
    {
        if (instance->m_symbol->m_is_global
            || instance->m_symbol->is_declared_as_static())
        {
            // Is static variable,
            const woort_IRStaticIndex static_storage =
                m_ircontext.c().alloc_static();

            instance->m_IR_storage.emplace(
                lang_ValueInstance::Storage(static_storage));
        }
        else
        {
            // Is stack variable.
            woort_IRValue* const stack_storage =
                m_ircontext.c().new_value();

            instance->m_IR_storage.emplace(
                lang_ValueInstance::Storage(stack_storage));
        }

        auto& storage = instance->m_IR_storage.value();

        if (storage.m_type == lang_ValueInstance::Storage::StorageType::GLOBAL)
        {
            if (tuple_member_offset.has_value())
            {
                const uint16_t index = tuple_member_offset.value();

                woort_IRValue* const v =
                    m_ircontext.c().new_value();

                m_ircontext.c().ldidxstruct(v, opnumval, index);
                m_ircontext.c().store(storage.m_static_index, v);
            }
            else
                m_ircontext.c().store(storage.m_static_index, opnumval);
        }
        else
        {
            wo_assert(storage.m_type == lang_ValueInstance::Storage::StorageType::STACKOFFSET);
            if (tuple_member_offset.has_value())
            {
                const uint16_t index = tuple_member_offset.value();
                m_ircontext.c().ldidxstruct(storage.m_stack_slot, opnumval, index);
            }
            else
                m_ircontext.c().mov(storage.m_stack_slot, opnumval);
        }
        return true;
    }

    bool LangContext::update_pattern_storage_and_code_gen_passir(
        lexer& lex,
        ast::AstPatternBase* pattern,
        const woort_IRValue* opnumval,
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
            if (!pattern_value_instance->IR_need_storage())
                // Skip, constant.
                return true;

            return update_instance_storage_and_code_gen_passir(
                pattern_value_instance, opnumval, tuple_member_offset);
        }
        case AstBase::AST_PATTERN_TUPLE:
        {
            AstPatternTuple* pattern_tuple = static_cast<AstPatternTuple*>(pattern);

            const woort_IRValue* tuple_source;
            if (tuple_member_offset.has_value())
            {
                uint16_t index = tuple_member_offset.value();
                woort_IRValue* tuple_source_for_write = m_ircontext.c().new_value();

                m_ircontext.c().ldidxstruct(tuple_source_for_write, opnumval, index);
                tuple_source = tuple_source_for_write;
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
        WO_LANG_REGISTER_PROCESSER(AstDefer, AstBase::AST_DEFER, passir_A);
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
    WO_PASS_PROCESSER(AstDefer)
    {
        // NOTE: Donot generate code for AstDefer, 
        // code will be generated in AstReturn & AstScope.

        return OKAY;
    }
    WO_PASS_PROCESSER(AstScope)
    {
        if (state == UNPROCESSED)
        {
            node->m_LANG_hold_state = AstScope::IR_HOLD_FOR_BODY_EVAL;
            WO_CONTINUE_PROCESS(node->m_body);
            return HOLD;
        }
        else if (state == HOLD)
        {
            switch (node->m_LANG_hold_state)
            {
            case AstScope::IR_HOLD_FOR_BODY_EVAL:
                if (!node->m_LANG_defer_instances.empty())
                {
                    node->m_LANG_hold_state = AstScope::IR_HOLD_FOR_DEFER_EVAL;
                    WO_CONTINUE_PROCESS_LIST(node->m_LANG_defer_instances);
                    return HOLD;
                }
                /* FALL-THROUGH */
                [[fallthrough]];
            case AstScope::IR_HOLD_FOR_DEFER_EVAL:
                break;
            default:
                break;
            }
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
                // TODO: More optimzied code generate required.

                m_ircontext.begin_eval_readonly();
                if (!pass_final_value(lex, node->m_condition))
                    return FAILED;

                m_ircontext.c().jccz(
                    m_ircontext.get_eval_result(),
                    m_ircontext.c().named_label(node, "#if_else"));

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
                    m_ircontext.c().jmp(m_ircontext.c().named_label(node, "#if_end"));
                    WO_CONTINUE_PROCESS(node->m_false_body.value());
                }
                m_ircontext.c().bind(m_ircontext.c().named_label(node, "#if_else"));
                node->m_LANG_hold_state = AstIf::IR_HOLD_FOR_FALSE_BODY;

                return HOLD;
            case AstIf::IR_HOLD_FOR_FALSE_BODY:
                if (!node->m_condition->m_evaled_const_value.has_value()
                    && node->m_false_body.has_value())
                {
                    m_ircontext.c().bind(m_ircontext.c().named_label(node, "#if_end"));
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
            m_ircontext.c().bind(m_ircontext.c().named_label(node, "#while_begin"));

            if (!dead_loop)
            {
                // TODO: More optimzied code generate required.

                m_ircontext.begin_eval_readonly();
                if (!pass_final_value(lex, node->m_condition))
                    return FAILED;

                m_ircontext.c().jccz(
                    m_ircontext.get_eval_result(),
                    m_ircontext.c().named_label(node, "#while_end"));
            }

            // Loop begin
            m_ircontext.begin_loop_while(node);

            WO_CONTINUE_PROCESS(node->m_body);

            return HOLD;
        }
        else if (state == HOLD)
        {
            m_ircontext.c().jmp(m_ircontext.c().named_label(node, "#while_begin"));
            m_ircontext.c().bind(m_ircontext.c().named_label(node, "#while_end"));

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
            ..InitExpr..
            jfwd _for_cond
        _for_begin:
            ..Body..
        _for_next:
            NEXT
        _for_cond:
            jCOND _for_begin
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
                {
                    // Need runtime cond.
                    m_ircontext.c().jmp(m_ircontext.c().named_label(node, "#for_cond"));
                }
                else
                {
                    if (node->m_condition.has_value()
                        && node->m_condition.value()->m_evaled_const_value.has_value()
                        && !node->m_condition.value()->m_evaled_const_value.value().value_bool())
                    {
                        // Always false, skip body next and cond.
                        return OKAY;
                    }
                }
                m_ircontext.c().bind(
                    m_ircontext.c().named_label(node, "#for_begin"));

                // Loop begin
                m_ircontext.begin_loop_for(node);

                WO_CONTINUE_PROCESS(node->m_body);
                node->m_LANG_hold_state = AstFor::IR_HOLD_FOR_BODY_EVAL;
                return HOLD;

            case AstFor::IR_HOLD_FOR_BODY_EVAL:
                m_ircontext.end_loop();

                m_ircontext.c().bind(
                    m_ircontext.c().named_label(node, "#for_next"));

                if (node->m_step.has_value())
                {
                    m_ircontext.eval_and_ignore();
                    if (!pass_final_value(lex, node->m_step.value()))
                        return FAILED;
                }

                if (node->m_condition.has_value()
                    && !node->m_condition.value()->m_evaled_const_value.has_value())
                {
                    m_ircontext.c().bind(
                        m_ircontext.c().named_label(node, "#for_cond"));

                    // TODO: More optimzied code generate required.

                    m_ircontext.begin_eval_readonly();
                    if (!pass_final_value(lex, node->m_condition.value()))
                        return FAILED;

                    m_ircontext.c().jcc(
                        m_ircontext.get_eval_result(),
                        m_ircontext.c().named_label(node, "#for_begin"));

                }
                else
                {
                    // Must be dead loop here.
                    m_ircontext.c().jmp(
                        m_ircontext.c().named_label(node, "#for_begin"));
                }
                m_ircontext.c().bind(
                    m_ircontext.c().named_label(node, "#for_end"));

                break;
            default:
                wo_error("Unknown hold state.");
                break;
            }
        }
        else
        {
            // Failed, we still need to end the loop.
            if (node->m_LANG_hold_state == AstFor::IR_HOLD_FOR_BODY_EVAL)
                m_ircontext.end_loop();
        }
        return OKAY;
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
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS_LIST(node->m_LANG_defer_instances);
            return HOLD;
        }
        else if (state == HOLD)
        {
            auto loop = m_ircontext.find_nearest_loop_content_label(node->m_label);
            // Loop has been checked in pass1.

            // WO_GENERATE_PDI_FOR(node);
            m_ircontext.c().jmp(loop.value()->m_break_label);
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstContinue)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS_LIST(node->m_LANG_defer_instances);
            return HOLD;
        }
        else if (state == HOLD)
        {
            auto loop = m_ircontext.find_nearest_loop_content_label(node->m_label);
            // Loop has been checked in pass1.

            m_ircontext.c().jmp(loop.value()->m_continue_label);
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstMatch)
    {
        if (state == UNPROCESSED)
        {
            m_ircontext.begin_eval_readonly();
            if (!pass_final_value(lex, node->m_matched_value))
                return FAILED;

            auto* matching_value = m_ircontext.get_eval_result();
            woort_IRValue* const matching_index = m_ircontext.c().new_value();
            m_ircontext.c().ldidxstruct(
                matching_index,
                matching_value,
                0);

            for (auto& match_case : node->m_cases)
            {
                match_case->m_IR_matching_index_opnum = matching_index;
                match_case->m_IR_matching_struct_opnum = matching_value;
                match_case->m_IR_match = node;
            }

            WO_CONTINUE_PROCESS_LIST(node->m_cases);

            return HOLD;
        }
        else if (state == HOLD)
        {
            m_ircontext.c().panic(m_ircontext.c().load_imm_string(
                wstring_pool::get_pstr(
                    "Bad label for union: '"
                    + std::string(get_type_name(node->m_matched_value->m_LANG_determined_type.value()))
                    + "', may be bad value returned by the external function.")));

            m_ircontext.c().bind(m_ircontext.c().named_label(node, "#match_end"));
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstMatchCase)
    {
        if (state == UNPROCESSED)
        {
            if (node->m_LANG_case_label_or_takeplace.has_value())
                m_ircontext.c().jcc_ne(
                    node->m_IR_matching_index_opnum.value(),
                    m_ircontext.c().load_imm_int(node->m_LANG_case_label_or_takeplace.value()),
                    m_ircontext.c().named_label(node, "#match_case_end"));

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
            m_ircontext.c().jmp(m_ircontext.c().named_label(node->m_IR_match.value(), "#match_end"));
            m_ircontext.c().bind(m_ircontext.c().named_label(node, "#match_case_end_"));
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
                    node->m_IR_static_init_flag_global_offset = m_ircontext.c().alloc_static();
                }
            }

            if (node->m_IR_static_init_flag_global_offset.has_value())
            {
                /*
                    jifinited _static_end if ALOAD flag != 0

                    ..init..

                    astore flag = 2

                _static_end:
                */

                m_ircontext.c().jifinited(
                    node->m_IR_static_init_flag_global_offset.value(),
                    m_ircontext.c().named_label(node, "#static_end"));
            }

            WO_CONTINUE_PROCESS_LIST(node->m_definitions);
            return HOLD;
        }
        if (state == HOLD)
        {
            if (node->m_IR_static_init_flag_global_offset.has_value())
            {
                m_ircontext.c().astore(
                    node->m_IR_static_init_flag_global_offset.value(),
                    m_ircontext.c().load_imm_int(2));
                m_ircontext.c().bind(
                    m_ircontext.c().named_label(node, "#static_end"));
            }
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }

    bool _check_pattern_all_no_need_storage(AstPatternTuple* ptuple)
    {
        for (auto* subtuple : ptuple->m_fields)
        {
            switch (subtuple->node_type)
            {
            case AstBase::AST_PATTERN_TAKEPLACE:
                break;
            case AstBase::AST_PATTERN_SINGLE:
            {
                AstPatternSingle* pattern_single =
                    static_cast<AstPatternSingle*>(subtuple);
                lang_Symbol* pattern_symbol = pattern_single->m_LANG_declared_symbol.value();

                wo_assert(pattern_symbol->m_symbol_kind == lang_Symbol::kind::VARIABLE
                    && !pattern_symbol->m_is_template);

                if (pattern_symbol->m_value_instance->IR_need_storage())
                    return false; // Need storage.
                break;
            }
            case AstBase::AST_PATTERN_TUPLE:
            {
                AstPatternTuple* subtuple_pattern =
                    static_cast<AstPatternTuple*>(subtuple);
                if (!_check_pattern_all_no_need_storage(subtuple_pattern))
                    return false; // Need storage.
                break;
            }
            default:
                wo_error("Unknown pattern type.");
                return false; // Unknown pattern type.
            }
        }
        return true; // No need storage.
    }

    WO_PASS_PROCESSER(AstVariableDefineItem)
    {
        wo_assert(state == UNPROCESSED);

        switch (node->m_pattern->node_type)
        {
        case AstBase::AST_PATTERN_TAKEPLACE:
            m_ircontext.eval_result_just_ignored();
            if (!pass_final_value(lex, node->m_init_value))
                // Failed 
                return FAILED;

            return OKAY;
        case AstBase::AST_PATTERN_SINGLE:
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
                    if (template_instance->m_constant_template_argument_have_unfinished_function)
                    {
                        bool bad_template_instance = false;

                        // Template argument may have non-constant-function, recheck here.
                        for (auto& argument : template_value_instance->m_instance_template_arguments.value())
                        {
                            if (!argument.m_constant.has_value())
                                continue;

                            auto constant = argument.m_constant.value().value_try_function();
                            if (!constant.has_value())
                                continue;

                            auto& captured_context = constant.value()->m_LANG_captured_context;
                            if (!captured_context.m_finished
                                || !captured_context.m_captured_variables.empty())
                            {
                                bad_template_instance = true;
                                break;
                            }
                        }

                        if (bad_template_instance)
                            // Skip bad instance.
                            continue;
                    }
                    if (!template_value_instance->IR_need_storage())
                    {
                        // No need storage.
                        auto function = template_value_instance->m_determined_constant_or_function
                            .value().value_try_function();

                        if (function.has_value())
                        {
                            // We still eval the function to let compiler know the function.
                            m_ircontext.eval_result_just_ignored();
                            if (!pass_final_value(lex, function.value()))
                                // Failed 
                                return FAILED;
                        }
                    }
                    else
                    {
                        // 
                        wo_assert(!template_value_instance->m_IR_storage.has_value());
                        if (template_value_instance->m_symbol->m_is_global
                            || template_value_instance->m_symbol->is_declared_as_static())
                        {
                            // Is static variable,
                            const woort_IRStaticIndex static_storage =
                                m_ircontext.c().alloc_static();

                            template_value_instance->m_IR_storage.emplace(
                                lang_ValueInstance::Storage(static_storage));

                            m_ircontext.eval_to_assign_static(static_storage, node);
                        }
                        else
                        {
                            // Is stack variable.
                            woort_IRValue* const stack_storage =
                                m_ircontext.c().new_value();

                            template_value_instance->m_IR_storage.emplace(
                                lang_ValueInstance::Storage(stack_storage));

                            m_ircontext.eval_to_assign(stack_storage, node);
                        }

                        if (!pass_final_value(lex, static_cast<AstValueBase*>(template_instance->m_ast)))
                            // Failed 
                            return FAILED;

                        m_ircontext.pop_eval_result();
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
                    m_ircontext.eval_and_ignore();
                    if (!pass_final_value(lex, function.value()))
                        // Failed 
                        return FAILED;
                }
            }
            else
            {
                // Not template, but need storage.
                wo_assert(!pattern_symbol->m_value_instance->m_IR_storage.has_value());
                if (pattern_symbol->m_value_instance->m_symbol->m_is_global
                    || pattern_symbol->m_value_instance->m_symbol->is_declared_as_static())
                {
                    // Is static variable,
                    const woort_IRStaticIndex static_storage =
                        m_ircontext.c().alloc_static();

                    pattern_symbol->m_value_instance->m_IR_storage.emplace(
                        lang_ValueInstance::Storage(static_storage));

                    m_ircontext.eval_to_assign_static(static_storage, node);
                }
                else
                {
                    // Is stack variable.
                    woort_IRValue* const stack_storage =
                        m_ircontext.c().new_value();

                    pattern_symbol->m_value_instance->m_IR_storage.emplace(
                        lang_ValueInstance::Storage(stack_storage));

                    m_ircontext.eval_to_assign(stack_storage, node);
                }

                if (!pass_final_value(lex, static_cast<AstValueBase*>(node->m_init_value)))
                    // Failed 
                    return FAILED;

                m_ircontext.pop_eval_result();
            }
            return OKAY;
        }
        case AstBase::AST_PATTERN_TUPLE:
        {
            AstPatternTuple* pattern_tuple = static_cast<AstPatternTuple*>(node->m_pattern);
            if (_check_pattern_all_no_need_storage(pattern_tuple))
            {
                m_ircontext.eval_and_ignore();
                if (!pass_final_value(lex, node->m_init_value))
                    // Failed 
                    return FAILED;
            }
            else
            {
                m_ircontext.begin_eval_readonly();
                if (!pass_final_value(lex, node->m_init_value))
                    // Failed 
                    return FAILED;

                auto* result_opnum = m_ircontext.get_eval_result();

                bool update_result = update_pattern_storage_and_code_gen_passir(
                    lex, node->m_pattern, result_opnum, std::nullopt);

                if (!update_result)
                    return FAILED;
            }
            return OKAY;
        }
        default:
            wo_error("Unknown pattern type.");
            return FAILED; // Unknown pattern type.
        }
    }
    WO_PASS_PROCESSER(AstReturn)
    {
        if (state == UNPROCESSED)
        {
            if (node->m_value.has_value())
            {
                m_ircontext.begin_eval_readonly();

                if (!pass_final_value(lex, node->m_value.value()))
                    return FAILED;

                node->m_IR_return_value_may_none.emplace(
                    m_ircontext.get_eval_result());
            }

            WO_CONTINUE_PROCESS_LIST(node->m_LANG_defer_instances);
            return HOLD;
        }
        else if (state == HOLD)
        {
            if (node->m_IR_return_value_may_none.has_value())
            {
                m_ircontext.c().ret(node->m_IR_return_value_may_none.value());
            }
            else
            {
                if (node->m_LANG_belong_function_may_null_if_outside.has_value())
                    m_ircontext.c().ret_void();
                else
                    m_ircontext.c().ret(m_ircontext.c().load_imm_int(0));
            }
        }
        return WO_EXCEPT_ERROR(state, OKAY);
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

        m_ircontext.m_being_used_function_instance.insert(node);

        if (node->m_LANG_captured_context.m_captured_variables.empty())
        {
            // Simple normal function
            m_ircontext.apply_eval_result(
                [&](BytecodeGenerateContext::EvalResult& result)
                {
                    const auto& target_storage = result.get_assign_target();
                    if (target_storage.has_value())
                    {
                        auto& [need_box, target] = target_storage.value();
                        woort_IRValue* const* const target_irvalue =
                            std::get_if<woort_IRValue*>(&target);

                        /* No need to box. */
                        (void)need_box;

                        if (target_irvalue == nullptr)
                        {
                            m_ircontext.c().store(
                                std::get<woort_IRStaticIndex>(target),
                                m_ircontext.c().load_imm_closure(node));
                        }
                        else
                        {
                            m_ircontext.c().mov(
                                *target_irvalue,
                                m_ircontext.c().load_imm_closure(node));
                        }
                    }
                    else
                        result.set_result_const_idx_no_need_box(
                            m_ircontext, m_ircontext.c().imm_closure(node));
                }
            );
        }
        else
        {
            // Need capture.
            m_ircontext.apply_eval_result(
                [&](BytecodeGenerateContext::EvalResult& result)
                {
                    for (auto& [capture_from_value, _useless] : node->m_LANG_captured_context.m_captured_variables)
                    {
                        (void)_useless;

                        auto& storage = capture_from_value->m_IR_storage.value();

                        if (storage.m_type == lang_ValueInstance::Storage::StorageType::STACKOFFSET)
                            m_ircontext.c().pushchk(storage.m_stack_slot);
                        else
                            m_ircontext.c().pushstaticchk(storage.m_static_index);
                    }

                    const auto& target_storage = result.get_assign_target();
                    if (target_storage.has_value())
                    {
                        auto& [need_box, target] = target_storage.value();
                        woort_IRValue* const* const target_irvalue =
                            std::get_if<woort_IRValue*>(&target);

                        /* No need to box. */
                        (void)need_box;

                        if (target_irvalue == nullptr)
                        {
                            woort_IRValue* const v = m_ircontext.c().new_value();
                            m_ircontext.c().mkclosure(
                                v,
                                (uint32_t)node->m_LANG_captured_context.m_captured_variables.size(),
                                m_ircontext.c().imm_closure(node));
                            m_ircontext.c().store(std::get<woort_IRStaticIndex>(target), v);
                        }
                        else
                        {
                            m_ircontext.c().mkclosure(
                                *target_irvalue,
                                (uint32_t)node->m_LANG_captured_context.m_captured_variables.size(),
                                m_ircontext.c().imm_closure(node));
                        }
                    }
                    else
                    {
                        woort_IRValue* const v = m_ircontext.c().new_value();

                        m_ircontext.c().mkclosure(
                            v,
                            (uint32_t)node->m_LANG_captured_context.m_captured_variables.size(),
                            m_ircontext.c().imm_closure(node));

                        result.set_result_stack_temp(
                            m_ircontext, v, node->m_LANG_determined_type.value());
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
                    abort();
                    /* m_ircontext.do_eval_if_not_ignore(
                         &BytecodeGenerateContext::eval_action);*/
                }
                else
                {
                    m_ircontext.do_eval_if_not_ignore(
                        &BytecodeGenerateContext::eval_to_push);
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
                    woort_IRValue* make_result_target = nullptr;

                    std::optional<woort_IRStaticIndex> need_write_back_to_static =
                        std::nullopt;

                    const auto& asigned_target = result.get_assign_target();
                    if (asigned_target.has_value())
                    {
                        const auto& [need_box, target] = asigned_target.value();

                        /* No need to box. */
                        (void)need_box;

                        woort_IRValue* const* const target_irvalue =
                            std::get_if<woort_IRValue*>(&target);

                        if (target_irvalue == nullptr)
                        {
                            need_write_back_to_static.emplace(
                                std::get<woort_IRStaticIndex>(target));

                            make_result_target = m_ircontext.c().new_value();
                        }
                        else
                        {
                            make_result_target = *target_irvalue;
                        }
                    }
                    else
                    {
                        make_result_target = m_ircontext.c().new_value();
                        result.set_result_stack_temp(
                            m_ircontext,
                            make_result_target,
                            node->m_LANG_determined_type.value());
                    }

                    uint32_t elem_count = (uint32_t)node->m_elements.size();
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

                    m_ircontext.c().mkstruct(make_result_target, elem_count);

                    if (need_write_back_to_static.has_value())
                    {
                        m_ircontext.c().store(need_write_back_to_static.value(), make_result_target);
                    }
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

                m_ircontext.do_eval_if_not_ignore(
                    &BytecodeGenerateContext::eval_to_push_box);

                WO_CONTINUE_PROCESS(field_value->m_value);

                m_ircontext.do_eval_if_not_ignore(
                    &BytecodeGenerateContext::eval_to_push_box);

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
                    woort_IRValue* make_result_target = nullptr;

                    std::optional<woort_IRStaticIndex> need_write_back_to_static =
                        std::nullopt;

                    const auto& asigned_target = result.get_assign_target();
                    if (asigned_target.has_value())
                    {
                        const auto& [need_box, target] = asigned_target.value();

                        /* No need to box. */
                        (void)need_box;

                        woort_IRValue* const* const target_irvalue =
                            std::get_if<woort_IRValue*>(&target);

                        if (target_irvalue == nullptr)
                        {
                            need_write_back_to_static.emplace(
                                std::get<woort_IRStaticIndex>(target));

                            make_result_target = m_ircontext.c().new_value();
                        }
                        else
                        {
                            make_result_target = *target_irvalue;
                        }
                    }
                    else
                    {
                        make_result_target = m_ircontext.c().new_value();
                        result.set_result_stack_temp(
                            m_ircontext,
                            make_result_target,
                            node->m_LANG_determined_type.value());
                    }

                    const uint32_t elem_count = (uint32_t)node->m_elements.size();
                    m_ircontext.c().mkmap(make_result_target, elem_count);

                    if (need_write_back_to_static.has_value())
                    {
                        m_ircontext.c().store(need_write_back_to_static.value(), make_result_target);
                    }
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

                m_ircontext.do_eval_if_not_ignore(
                    &BytecodeGenerateContext::eval_to_push_box);

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
                    woort_IRValue* make_result_target = nullptr;

                    std::optional<woort_IRStaticIndex> need_write_back_to_static =
                        std::nullopt;

                    const auto& asigned_target = result.get_assign_target();
                    if (asigned_target.has_value())
                    {
                        const auto& [need_box, target] = asigned_target.value();

                        /* No need to box. */
                        (void)need_box;

                        woort_IRValue* const* const target_irvalue =
                            std::get_if<woort_IRValue*>(&target);

                        if (target_irvalue == nullptr)
                        {
                            need_write_back_to_static.emplace(
                                std::get<woort_IRStaticIndex>(target));

                            make_result_target = m_ircontext.c().new_value();
                        }
                        else
                        {
                            make_result_target = *target_irvalue;
                        }
                    }
                    else
                    {
                        make_result_target = m_ircontext.c().new_value();
                        result.set_result_stack_temp(
                            m_ircontext,
                            make_result_target,
                            node->m_LANG_determined_type.value());
                    }

                    const uint32_t elem_count = (uint32_t)node->m_elements.size();
                    m_ircontext.c().mkvec(make_result_target, elem_count);

                    if (need_write_back_to_static.has_value())
                    {
                        m_ircontext.c().store(need_write_back_to_static.value(), make_result_target);
                    }
                });
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueFunctionCall)
    {
        abort();
        return OKAY;
    }
    WO_PASS_PROCESSER(AstValueVariable)
    {
        wo_assert(state == UNPROCESSED);

        lang_ValueInstance* value_instance = node->m_LANG_variable_instance.value();
        wo_assert(value_instance->IR_need_storage());

        if (!value_instance->m_IR_storage.has_value())
        {
            lex.record_lang_error(lexer::msglevel_t::error, node,
                WO_ERR_VARIBALE_STORAGE_NOT_DETERMINED,
                get_value_name(value_instance));

            if (value_instance->m_symbol->m_symbol_declare_ast.has_value())
                lex.record_lang_error(lexer::msglevel_t::infom,
                    value_instance->m_symbol->m_symbol_declare_ast.value(),
                    WO_INFO_SYMBOL_NAMED_DEFINED_HERE,
                    get_value_name(value_instance));

            return FAILED;
        }

        auto& variable_storage = value_instance->m_IR_storage.value();

        m_ircontext.apply_eval_result(
            [&](BytecodeGenerateContext::EvalResult& result)
            {
                const auto target_storage = result.get_assign_target();
                if (target_storage.has_value())
                {
                    const auto& [need_box, target] = target_storage.value();

                    woort_IRValue* const* const target_irvalue =
                        std::get_if<woort_IRValue*>(&target);

                    woort_BoxValueType box_type;

                    const bool type_need_box = need_box
                        && node->m_LANG_determined_type.value()->is_need_to_box_in_IR(&box_type);

                    if (target_irvalue == nullptr)
                    {
                        // Assigned to static.
                        if (variable_storage.m_type == lang_ValueInstance::Storage::GLOBAL)
                        {
                            woort_IRValue* const v = m_ircontext.c().new_value();
                            m_ircontext.c().load(v, variable_storage.m_static_index);

                            if (type_need_box)
                                m_ircontext.c().boxdyn(v, box_type, v);

                            m_ircontext.c().store(
                                std::get<woort_IRStaticIndex>(target), v);
                        }
                        else
                        {
                            wo_assert(variable_storage.m_type == lang_ValueInstance::Storage::STACKOFFSET);

                            woort_IRValue* v = variable_storage.m_stack_slot;

                            if (type_need_box)
                            {
                                v = m_ircontext.c().new_value();
                                m_ircontext.c().boxdyn(v, box_type, variable_storage.m_stack_slot);
                            }

                            m_ircontext.c().store(
                                std::get<woort_IRStaticIndex>(target), v);
                        }
                    }
                    else
                    {
                        if (variable_storage.m_type == lang_ValueInstance::Storage::GLOBAL)
                        {
                            woort_IRValue* const v = m_ircontext.c().new_value();
                            m_ircontext.c().load(v, variable_storage.m_static_index);
                            m_ircontext.c().mov(*target_irvalue, v);
                        }
                        else
                        {
                            wo_assert(variable_storage.m_type == lang_ValueInstance::Storage::STACKOFFSET);
                            m_ircontext.c().mov(*target_irvalue, variable_storage.m_stack_slot);
                        }

                        if (type_need_box)
                            m_ircontext.c().boxdyn(*target_irvalue, box_type, *target_irvalue);
                    }
                }
                else
                {
                    if (variable_storage.m_type == lang_ValueInstance::Storage::GLOBAL)
                        result.set_result_static(
                            m_ircontext,
                            variable_storage.m_static_index,
                            node->m_LANG_determined_type.value());
                    else
                    {
                        wo_assert(variable_storage.m_type == lang_ValueInstance::Storage::STACKOFFSET);
                        result.set_result_stack_var(
                            m_ircontext,
                            variable_storage.m_stack_slot,
                            node->m_LANG_determined_type.value());
                    }
                }
            });

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
                m_ircontext.do_eval_if_not_ignore(
                    &BytecodeGenerateContext::begin_eval_readonly);
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

                        const auto& target_storage = result.get_assign_target();
                        if (target_storage.has_value())
                        {
                            auto& [need_box, target] = target_storage.value();
                            woort_IRValue* const* const target_irvalue =
                                std::get_if<woort_IRValue*>(&target);

                            if (target_irvalue == nullptr)
                            {
                                if (need_box)
                                {
                                    m_ircontext.c().assertdyn(
                                        opnum_to_check,
                                        convert_lang_base_type_to_woort_type_exclude_compile_type(
                                            target_determined_type_instance->m_base_type));
                                    m_ircontext.c().store(std::get<woort_IRStaticIndex>(target), opnum_to_check);
                                }
                                else
                                {
                                    woort_IRValue* const v = m_ircontext.c().new_value();
                                    m_ircontext.c().unboxdyn(
                                        v,
                                        convert_lang_base_type_to_woort_type_exclude_compile_type(
                                            target_determined_type_instance->m_base_type),
                                        opnum_to_check);
                                    m_ircontext.c().store(std::get<woort_IRStaticIndex>(target), v);
                                }
                            }
                            else
                            {
                                if (need_box)
                                {
                                    m_ircontext.c().assertdyn(
                                        opnum_to_check,
                                        convert_lang_base_type_to_woort_type_exclude_compile_type(
                                            target_determined_type_instance->m_base_type));
                                    m_ircontext.c().mov(*target_irvalue, opnum_to_check);
                                }
                                else
                                {
                                    m_ircontext.c().unboxdyn(
                                        *target_irvalue,
                                        convert_lang_base_type_to_woort_type_exclude_compile_type(
                                            target_determined_type_instance->m_base_type),
                                        opnum_to_check);
                                }
                            }
                        }
                        else
                        {
                            woort_IRValue* const v = m_ircontext.c().new_value();
                            m_ircontext.c().unboxdyn(
                                v,
                                convert_lang_base_type_to_woort_type_exclude_compile_type(
                                    target_determined_type_instance->m_base_type),
                                opnum_to_check);

                            result.set_result_stack_temp(m_ircontext, v, node->m_LANG_determined_type.value());
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

            m_ircontext.do_eval_if_not_ignore(
                &BytecodeGenerateContext::begin_eval_readonly);

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
                    if (target_storage.has_value())
                    {
                        auto& [need_box, target] = target_storage.value();
                        woort_IRValue* const* const target_irvalue =
                            std::get_if<woort_IRValue*>(&target);

                        if (target_irvalue == nullptr)
                        {
                            woort_IRValue* const v = m_ircontext.c().new_value();
                            m_ircontext.c().checkdyn(
                                v,
                                convert_lang_base_type_to_woort_type_exclude_compile_type(
                                    target_determined_type_instance->m_base_type),
                                opnum_to_check);

                            if (need_box)
                                m_ircontext.c().boxdyn(v, WOORT_BOX_VALUE_TYPE_BOOL, v);

                            m_ircontext.c().store(std::get<woort_IRStaticIndex>(target), v);
                        }
                        else
                        {
                            woort_IRValue* const v = m_ircontext.c().new_value();
                            m_ircontext.c().checkdyn(
                                v,
                                convert_lang_base_type_to_woort_type_exclude_compile_type(
                                    target_determined_type_instance->m_base_type),
                                opnum_to_check);

                            if (need_box)
                                m_ircontext.c().boxdyn(
                                    *target_irvalue, WOORT_BOX_VALUE_TYPE_BOOL, *target_irvalue);
                        }
                    }
                    else
                    {
                        woort_IRValue* const v = m_ircontext.c().new_value();
                        m_ircontext.c().checkdyn(
                            v,
                            convert_lang_base_type_to_woort_type_exclude_compile_type(
                                target_determined_type_instance->m_base_type),
                            opnum_to_check);

                        result.set_result_stack_temp(m_ircontext, v, node->m_LANG_determined_type.value());
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
                m_ircontext.eval_and_ignore();
            }
            else if (
                (target_determined_type_instance->m_base_type
                    != src_determined_type_instance->m_base_type
                    && (
                        /* INT & HANDLE IS SAME TYPE IN WOORT. */
                        (target_determined_type_instance->m_base_type != lang_TypeInstance::DeterminedType::INTEGER
                            && target_determined_type_instance->m_base_type != lang_TypeInstance::DeterminedType::HANDLE)
                        || (src_determined_type_instance->m_base_type != lang_TypeInstance::DeterminedType::INTEGER
                            && src_determined_type_instance->m_base_type != lang_TypeInstance::DeterminedType::HANDLE))
                    && src_determined_type_instance->m_base_type
                    != lang_TypeInstance::DeterminedType::NOTHING))
            {
                if (target_determined_type_instance->m_base_type
                    != lang_TypeInstance::DeterminedType::DYNAMIC)
                {
                    // Woolang 1.15: This is special handling for type exemptions; although these types are 
                    // different at the language level, they are identical at the WooRT underlying layer, 
                    // so no conversion is needed.
                    if (
                        // INT <- HANDLE/BOOL
                        (target_determined_type_instance->m_base_type == lang_TypeInstance::DeterminedType::INTEGER
                            && (src_determined_type_instance->m_base_type == lang_TypeInstance::DeterminedType::HANDLE
                                || src_determined_type_instance->m_base_type == lang_TypeInstance::DeterminedType::BOOLEAN))
                        // HANDLE <- INT/BOOL
                        || (target_determined_type_instance->m_base_type == lang_TypeInstance::DeterminedType::HANDLE
                            && (src_determined_type_instance->m_base_type == lang_TypeInstance::DeterminedType::INTEGER
                                || src_determined_type_instance->m_base_type == lang_TypeInstance::DeterminedType::BOOLEAN)))
                    {
                        // No need runtime operation
                        goto _label_no_cast;
                    }
                    else
                    {
                        // Need runtime check.
                        node->m_IR_need_eval = true;

                        m_ircontext.do_eval_if_not_ignore(
                            &BytecodeGenerateContext::begin_eval_readonly);
                    }
                }
                else
                {
                    // Need box.
                    node->m_IR_need_eval = false;

                    m_ircontext.eval_for_upper_box();
                }
            }
            else
            {
                // No cast
            _label_no_cast:
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

                        // The case where the target type is dynamic has already been handled by eval_for_upper_box; 
                        // it is impossible for this situation to occur here.
                        wo_assert(target_determined_type_instance->m_base_type != lang_TypeInstance::DeterminedType::DYNAMIC);

                        // VOID expr needed, skip and give junk value.
                        if (target_determined_type_instance->m_base_type == lang_TypeInstance::DeterminedType::VOID)
                        {
                            if (target_storage.has_value())
                            {
                                // NO NEED TO DO ANYTHING.
                                // VOID VALUE IS PURE JUNK VALUE.
                            }
                            else
                            {
                                // Return a junk value.
                                result.set_result_junk(m_ircontext);
                            }
                        }

                        // Need runtime cast.
                        auto* opnum_to_cast = m_ircontext.get_eval_result();

                        if (src_determined_type_instance->m_base_type == lang_TypeInstance::DeterminedType::DYNAMIC)
                        {
                            const auto target_woort_type =
                                convert_lang_base_type_to_woort_type_exclude_compile_type(
                                    target_determined_type_instance->m_base_type);

                            // Here, converting from the dynamic type to a specified type uses `castdyn`.
                            if (target_storage.has_value())
                            {
                                auto& [need_box, target] = target_storage.value();
                                woort_IRValue* const* const target_irvalue =
                                    std::get_if<woort_IRValue*>(&target);

                                if (target_irvalue == nullptr)
                                {
                                    woort_IRValue* const v = m_ircontext.c().new_value();
                                    m_ircontext.c().castdyn(
                                        v,
                                        opnum_to_cast,
                                        target_woort_type);

                                    if (need_box)
                                        m_ircontext.c().boxdyn(
                                            v, target_woort_type, v);

                                    m_ircontext.c().store(std::get<woort_IRStaticIndex>(target), v);
                                }
                                else
                                {
                                    m_ircontext.c().castdyn(
                                        *target_irvalue,
                                        opnum_to_cast,
                                        target_woort_type);

                                    if (need_box)
                                        m_ircontext.c().boxdyn(
                                            *target_irvalue, target_woort_type, *target_irvalue);
                                }
                            }
                            else
                            {
                                woort_IRValue* const v = m_ircontext.c().new_value();
                                m_ircontext.c().castdyn(
                                    v,
                                    opnum_to_cast,
                                    target_woort_type);
                                result.set_result_stack_temp(m_ircontext, v, target_type_instance);
                            }
                        }
                        else
                        {
                            switch (target_determined_type_instance->m_base_type)
                            {
                            case lang_TypeInstance::DeterminedType::INTEGER:
                            case lang_TypeInstance::DeterminedType::HANDLE:
                                /////////////////////////////////////////////////////////////////////////
                                switch (src_determined_type_instance->m_base_type)
                                {
                                case lang_TypeInstance::DeterminedType::REAL:
                                    if (target_storage.has_value())
                                    {
                                        auto& [need_box, target] = target_storage.value();
                                        woort_IRValue* const* const target_irvalue =
                                            std::get_if<woort_IRValue*>(&target);

                                        if (target_irvalue == nullptr)
                                        {
                                            woort_IRValue* const v = m_ircontext.c().new_value();
                                            m_ircontext.c().rtoi(v, opnum_to_cast);

                                            if (need_box)
                                                m_ircontext.c().boxdyn(v, WOORT_BOX_VALUE_TYPE_INT, v);

                                            m_ircontext.c().store(std::get<woort_IRStaticIndex>(target), v);
                                        }
                                        else
                                        {
                                            m_ircontext.c().rtoi(*target_irvalue, opnum_to_cast);
                                            if (need_box)
                                                m_ircontext.c().boxdyn(
                                                    *target_irvalue,
                                                    WOORT_BOX_VALUE_TYPE_INT,
                                                    *target_irvalue);
                                        }
                                    }
                                    else
                                    {
                                        woort_IRValue* const v = m_ircontext.c().new_value();
                                        m_ircontext.c().rtoi(
                                            v,
                                            opnum_to_cast);
                                        result.set_result_stack_temp(m_ircontext, v, target_type_instance);
                                    }
                                    break;
                                case lang_TypeInstance::DeterminedType::STRING:
                                    if (target_storage.has_value())
                                    {
                                        auto& [need_box, target] = target_storage.value();
                                        woort_IRValue* const* const target_irvalue =
                                            std::get_if<woort_IRValue*>(&target);

                                        if (target_irvalue == nullptr)
                                        {
                                            woort_IRValue* const v = m_ircontext.c().new_value();
                                            m_ircontext.c().caststo(v, opnum_to_cast, WOORT_BOX_VALUE_TYPE_INT);

                                            if (need_box)
                                                m_ircontext.c().boxdyn(v, WOORT_BOX_VALUE_TYPE_INT, v);

                                            m_ircontext.c().store(std::get<woort_IRStaticIndex>(target), v);
                                        }
                                        else
                                        {
                                            m_ircontext.c().caststo(
                                                *target_irvalue, opnum_to_cast, WOORT_BOX_VALUE_TYPE_INT);

                                            if (need_box)
                                                m_ircontext.c().boxdyn(
                                                    *target_irvalue, WOORT_BOX_VALUE_TYPE_INT, *target_irvalue);
                                        }
                                    }
                                    else
                                    {
                                        woort_IRValue* const v = m_ircontext.c().new_value();
                                        m_ircontext.c().caststo(
                                            v,
                                            opnum_to_cast,
                                            WOORT_BOX_VALUE_TYPE_INT);
                                        result.set_result_stack_temp(m_ircontext, v, target_type_instance);
                                    }
                                    break;
                                default:
                                    wo_error("Unexpected type.");
                                }
                                break;
                            case lang_TypeInstance::DeterminedType::REAL:
                                /////////////////////////////////////////////////////////////////////////
                                switch (src_determined_type_instance->m_base_type)
                                {
                                case lang_TypeInstance::DeterminedType::INTEGER:
                                case lang_TypeInstance::DeterminedType::BOOLEAN:
                                    if (target_storage.has_value())
                                    {
                                        auto& [need_box, target] = target_storage.value();
                                        woort_IRValue* const* const target_irvalue =
                                            std::get_if<woort_IRValue*>(&target);

                                        if (target_irvalue == nullptr)
                                        {
                                            woort_IRValue* const v = m_ircontext.c().new_value();
                                            m_ircontext.c().itor(v, opnum_to_cast);

                                            if (need_box)
                                                m_ircontext.c().boxdyn(v, WOORT_BOX_VALUE_TYPE_REAL, v);

                                            m_ircontext.c().store(std::get<woort_IRStaticIndex>(target), v);
                                        }
                                        else
                                        {
                                            m_ircontext.c().itor(*target_irvalue, opnum_to_cast);
                                            if (need_box)
                                                m_ircontext.c().boxdyn(
                                                    *target_irvalue, WOORT_BOX_VALUE_TYPE_REAL, *target_irvalue);
                                        }
                                    }
                                    else
                                    {
                                        woort_IRValue* const v = m_ircontext.c().new_value();
                                        m_ircontext.c().itor(
                                            v,
                                            opnum_to_cast);
                                        result.set_result_stack_temp(m_ircontext, v, target_type_instance);
                                    }
                                    break;
                                case lang_TypeInstance::DeterminedType::STRING:
                                    if (target_storage.has_value())
                                    {
                                        auto& [need_box, target] = target_storage.value();
                                        woort_IRValue* const* const target_irvalue =
                                            std::get_if<woort_IRValue*>(&target);

                                        if (target_irvalue == nullptr)
                                        {
                                            woort_IRValue* const v = m_ircontext.c().new_value();
                                            m_ircontext.c().caststo(v, opnum_to_cast, WOORT_BOX_VALUE_TYPE_REAL);

                                            if (need_box)
                                                m_ircontext.c().boxdyn(v, WOORT_BOX_VALUE_TYPE_REAL, v);

                                            m_ircontext.c().store(std::get<woort_IRStaticIndex>(target), v);
                                        }
                                        else
                                        {
                                            m_ircontext.c().caststo(
                                                *target_irvalue, opnum_to_cast, WOORT_BOX_VALUE_TYPE_REAL);

                                            if (need_box)
                                                m_ircontext.c().boxdyn(
                                                    *target_irvalue, WOORT_BOX_VALUE_TYPE_REAL, *target_irvalue);
                                        }
                                    }
                                    else
                                    {
                                        woort_IRValue* const v = m_ircontext.c().new_value();
                                        m_ircontext.c().caststo(
                                            v,
                                            opnum_to_cast,
                                            WOORT_BOX_VALUE_TYPE_REAL);
                                        result.set_result_stack_temp(m_ircontext, v, target_type_instance);
                                    }
                                    break;
                                default:
                                    wo_error("Unexpected type.");
                                }
                                break;
                            case lang_TypeInstance::DeterminedType::BOOLEAN:
                                /////////////////////////////////////////////////////////////////////////
                                switch (src_determined_type_instance->m_base_type)
                                {
                                case lang_TypeInstance::DeterminedType::INTEGER:
                                    if (target_storage.has_value())
                                    {
                                        auto& [need_box, target] = target_storage.value();
                                        woort_IRValue* const* const target_irvalue =
                                            std::get_if<woort_IRValue*>(&target);

                                        if (target_irvalue == nullptr)
                                        {
                                            woort_IRValue* const v = m_ircontext.c().new_value();
                                            m_ircontext.c().nei(v, opnum_to_cast, m_ircontext.c().load_imm_int(0));

                                            if (need_box)
                                                m_ircontext.c().boxdyn(v, WOORT_BOX_VALUE_TYPE_BOOL, v);

                                            m_ircontext.c().store(std::get<woort_IRStaticIndex>(target), v);
                                        }
                                        else
                                        {
                                            m_ircontext.c().nei(
                                                *target_irvalue, opnum_to_cast, m_ircontext.c().load_imm_int(0));

                                            if (need_box)
                                                m_ircontext.c().boxdyn(
                                                    *target_irvalue, WOORT_BOX_VALUE_TYPE_BOOL, *target_irvalue);
                                        }
                                    }
                                    else
                                    {
                                        woort_IRValue* const v = m_ircontext.c().new_value();
                                        m_ircontext.c().nei(
                                            v,
                                            opnum_to_cast,
                                            m_ircontext.c().load_imm_int(0));
                                        result.set_result_stack_temp(m_ircontext, v, target_type_instance);
                                    }
                                    break;
                                case lang_TypeInstance::DeterminedType::REAL:
                                    if (target_storage.has_value())
                                    {
                                        auto& [need_box, target] = target_storage.value();
                                        woort_IRValue* const* const target_irvalue =
                                            std::get_if<woort_IRValue*>(&target);

                                        if (target_irvalue == nullptr)
                                        {
                                            woort_IRValue* const v = m_ircontext.c().new_value();
                                            m_ircontext.c().nei(v, opnum_to_cast, m_ircontext.c().load_imm_real(0.));

                                            if (need_box)
                                                m_ircontext.c().boxdyn(v, WOORT_BOX_VALUE_TYPE_BOOL, v);

                                            m_ircontext.c().store(std::get<woort_IRStaticIndex>(target), v);
                                        }
                                        else
                                        {
                                            m_ircontext.c().ner(
                                                *target_irvalue, opnum_to_cast, m_ircontext.c().load_imm_real(0.));

                                            if (need_box)
                                                m_ircontext.c().boxdyn(
                                                    *target_irvalue, WOORT_BOX_VALUE_TYPE_BOOL, *target_irvalue);
                                        }
                                    }
                                    else
                                    {
                                        woort_IRValue* const v = m_ircontext.c().new_value();
                                        m_ircontext.c().nei(
                                            v,
                                            opnum_to_cast,
                                            m_ircontext.c().load_imm_real(0.));
                                        result.set_result_stack_temp(m_ircontext, v, target_type_instance);
                                    }
                                    break;
                                case lang_TypeInstance::DeterminedType::STRING:
                                    if (target_storage.has_value())
                                    {
                                        auto& [need_box, target] = target_storage.value();
                                        woort_IRValue* const* const target_irvalue =
                                            std::get_if<woort_IRValue*>(&target);

                                        if (target_irvalue == nullptr)
                                        {
                                            woort_IRValue* const v = m_ircontext.c().new_value();
                                            m_ircontext.c().caststo(v, opnum_to_cast, WOORT_BOX_VALUE_TYPE_BOOL);

                                            if (need_box)
                                                m_ircontext.c().boxdyn(v, WOORT_BOX_VALUE_TYPE_BOOL, v);

                                            m_ircontext.c().store(std::get<woort_IRStaticIndex>(target), v);
                                        }
                                        else
                                        {
                                            m_ircontext.c().caststo(
                                                *target_irvalue, opnum_to_cast, WOORT_BOX_VALUE_TYPE_BOOL);

                                            if (need_box)
                                                m_ircontext.c().boxdyn(
                                                    *target_irvalue, WOORT_BOX_VALUE_TYPE_BOOL, *target_irvalue);
                                        }
                                    }
                                    else
                                    {
                                        woort_IRValue* const v = m_ircontext.c().new_value();
                                        m_ircontext.c().caststo(
                                            v,
                                            opnum_to_cast,
                                            WOORT_BOX_VALUE_TYPE_BOOL);
                                        result.set_result_stack_temp(m_ircontext, v, target_type_instance);
                                    }
                                    break;
                                default:
                                    wo_error("Unexpected type.");
                                }
                                break;
                            case lang_TypeInstance::DeterminedType::STRING:
                                switch (src_determined_type_instance->m_base_type)
                                {
                                case lang_TypeInstance::DeterminedType::HANDLE:
                                case lang_TypeInstance::DeterminedType::INTEGER:
                                    if (target_storage.has_value())
                                    {
                                        auto& [need_box, target] = target_storage.value();
                                        woort_IRValue* const* const target_irvalue =
                                            std::get_if<woort_IRValue*>(&target);

                                        /* No need to box. */
                                        (void)need_box;

                                        if (target_irvalue == nullptr)
                                        {
                                            woort_IRValue* const v = m_ircontext.c().new_value();
                                            m_ircontext.c().itos(v, opnum_to_cast);
                                            m_ircontext.c().store(std::get<woort_IRStaticIndex>(target), v);
                                        }
                                        else
                                        {
                                            m_ircontext.c().itos(*target_irvalue, opnum_to_cast);
                                        }
                                    }
                                    else
                                    {
                                        woort_IRValue* const v = m_ircontext.c().new_value();
                                        m_ircontext.c().itos(
                                            v,
                                            opnum_to_cast);
                                        result.set_result_stack_temp(m_ircontext, v, target_type_instance);
                                    }
                                    break;
                                case lang_TypeInstance::DeterminedType::REAL:
                                    if (target_storage.has_value())
                                    {
                                        auto& [need_box, target] = target_storage.value();
                                        woort_IRValue* const* const target_irvalue =
                                            std::get_if<woort_IRValue*>(&target);

                                        /* No need to box. */
                                        (void)need_box;

                                        if (target_irvalue == nullptr)
                                        {
                                            woort_IRValue* const v = m_ircontext.c().new_value();
                                            m_ircontext.c().rtos(v, opnum_to_cast);
                                            m_ircontext.c().store(std::get<woort_IRStaticIndex>(target), v);
                                        }
                                        else
                                        {
                                            m_ircontext.c().rtos(*target_irvalue, opnum_to_cast);
                                        }
                                    }
                                    else
                                    {
                                        woort_IRValue* const v = m_ircontext.c().new_value();
                                        m_ircontext.c().rtos(
                                            v,
                                            opnum_to_cast);
                                        result.set_result_stack_temp(m_ircontext, v, target_type_instance);
                                    }
                                    break;
                                default:
                                    if (target_storage.has_value())
                                    {
                                        auto& [need_box, target] = target_storage.value();
                                        woort_IRValue* const* const target_irvalue =
                                            std::get_if<woort_IRValue*>(&target);

                                        /* No need to box. */
                                        (void)need_box;

                                        if (target_irvalue == nullptr)
                                        {
                                            woort_IRValue* const v = m_ircontext.c().new_value();
                                            m_ircontext.c().castsfrom(
                                                v,
                                                opnum_to_cast,
                                                convert_lang_base_type_to_woort_type_exclude_compile_type(
                                                    src_determined_type_instance->m_base_type));
                                            m_ircontext.c().store(std::get<woort_IRStaticIndex>(target), v);
                                        }
                                        else
                                        {
                                            m_ircontext.c().castsfrom(
                                                *target_irvalue,
                                                opnum_to_cast,
                                                convert_lang_base_type_to_woort_type_exclude_compile_type(
                                                    src_determined_type_instance->m_base_type));
                                        }
                                    }
                                    else
                                    {
                                        woort_IRValue* const v = m_ircontext.c().new_value();
                                        m_ircontext.c().castsfrom(
                                            v,
                                            opnum_to_cast,
                                            convert_lang_base_type_to_woort_type_exclude_compile_type(
                                                src_determined_type_instance->m_base_type));
                                        result.set_result_stack_temp(m_ircontext, v, target_type_instance);
                                    }
                                }
                                break;
                            default:
                                wo_error("Unexpected type.");
                            }
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
            m_ircontext.eval_and_ignore();
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
                        result.set_result_junk(m_ircontext);
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
        //if (!config::ENABLE_RUNTIME_CHECKING_INTEGER_DIVISION)
        //    // Skip runtime check if disabled.
        //    return;

        //if (right->m_evaled_const_value.has_value())
        //{
        //    int64_t right_value = right->m_evaled_const_value.value().value_integer();
        //    wo_assert(right_value != 0);
        //    wo_assert(!left.has_value() || !left.value()->m_evaled_const_value.has_value());

        //    if (right_value == -1)
        //        // Need check l
        //        bgc.c().ext_cdivil(*left_opnum);

        //    // Otherwise, no need to check.
        //}
        //else if (left.has_value() && left.value()->m_evaled_const_value.has_value())
        //{
        //    int64_t left_value = left.value()->m_evaled_const_value.value().value_integer();
        //    if (left_value == INT64_MIN)
        //        // Need check r
        //        bgc.c().ext_cdivir(*right_opnum);
        //    else
        //        // Need check rz
        //        bgc.c().ext_cdivirz(*right_opnum);
        //}
        //else
        //{
        //    // Left & right both not constant.
        //    bgc.c().ext_cdivilr(*left_opnum, *right_opnum);
        //}
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

            m_ircontext.do_eval_if_not_ignore(
                &BytecodeGenerateContext::begin_eval_readonly);

            WO_CONTINUE_PROCESS(node->m_unpack_value);
            return HOLD;
        }
        else if (state == HOLD)
        {
            m_ircontext.apply_eval_result(
                [&](BytecodeGenerateContext::EvalResult&)
                {
                    auto* unpacking_opnum = m_ircontext.get_eval_result();

                    if (node->m_IR_unpack_method == AstFakeValueUnpack::UNPACK_FOR_TUPLE)
                    {
                        auto* unpacking_tuple_determined_type =
                            node->m_LANG_determined_type.value()->get_determined_type().value();

                        wo_assert(unpacking_tuple_determined_type->m_base_type == lang_TypeInstance::DeterminedType::TUPLE);
                        auto* tuple_info = unpacking_tuple_determined_type->m_external_type_description.m_tuple;
                        const uint32_t tuple_elem_count = (uint32_t)tuple_info->m_element_types.size();

                        for (uint32_t i = 0; i < tuple_elem_count; ++i)
                            m_ircontext.c().pushidxstruct(unpacking_opnum, i);
                    }
                    else
                    {
                        const auto& unpack_requirement = node->m_IR_need_to_be_unpack_count.value();

                        if (unpack_requirement.m_unpack_all)
                        {
                            m_ircontext.c().unpack(
                                WO_OPNUM(unpacking_opnum),
                                -(int32_t)unpack_requirement.m_require_unpack_count);
                        }
                        else
                        {
                            // If require to unpack 0 argument, just skip & ignore.
                            if (unpack_requirement.m_require_unpack_count != 0)
                                m_ircontext.c().unpack(
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
            if (node->m_LANG_overload_call.has_value())
            {
                m_ircontext.eval_for_upper();
                WO_CONTINUE_PROCESS(node->m_LANG_overload_call.value());

                node->m_LANG_hold_state = AstValueBinaryOperator::IR_HOLD_FOR_OVERLOAD_CALL_EVAL;
            }
            else if (node->m_operator == AstValueBinaryOperator::LOGICAL_AND
                || node->m_operator == AstValueBinaryOperator::LOGICAL_OR)
            {
                // Need short cut, 
                woort_IRValue* const v = m_ircontext.c().new_value();
                node->m_IR_value_to_store_shortcut_result.emplace(v);

                m_ircontext.eval_to_assign(v, std::nullopt);

                WO_CONTINUE_PROCESS(node->m_left);

                node->m_LANG_hold_state = AstValueBinaryOperator::IR_HOLD_FOR_LAND_LOR_LEFT_SHORT_CUT;
            }
            else
            {
                m_ircontext.do_eval_if_not_ignore(
                    &BytecodeGenerateContext::begin_eval_readonly);
                WO_CONTINUE_PROCESS(node->m_right);

                m_ircontext.do_eval_if_not_ignore(
                    &BytecodeGenerateContext::begin_eval_readonly);
                WO_CONTINUE_PROCESS(node->m_left);

                node->m_LANG_hold_state = AstValueBinaryOperator::IR_HOLD_FOR_NORMAL_LR_EVAL;
            }
            return HOLD;
        }
        else if (state == HOLD)
        {
            switch (node->m_LANG_hold_state)
            {
            case AstValueBinaryOperator::IR_HOLD_FOR_LAND_LOR_LEFT_SHORT_CUT:
            {
                const woort_IRValue* const left_result = m_ircontext.get_eval_result();
                switch (node->m_operator)
                {
                case AstValueBinaryOperator::LOGICAL_AND:
                    m_ircontext.c().jccz(left_result, m_ircontext.c().named_label(node, "#lshortcut"));
                    break;
                case AstValueBinaryOperator::LOGICAL_OR:
                    m_ircontext.c().jcc(left_result, m_ircontext.c().named_label(node, "#lshortcut"));
                    break;
                default:
                    wo_error("Unknown operator.");
                    break;
                }

                m_ircontext.eval_to_assign_if_not_ignore(
                    node->m_IR_value_to_store_shortcut_result.value(),
                    std::nullopt);

                WO_CONTINUE_PROCESS(node->m_right);

                node->m_LANG_hold_state = AstValueBinaryOperator::IR_HOLD_FOR_LAND_LOR_RIGHT;
                return HOLD;
            }
            case  AstValueBinaryOperator::IR_HOLD_FOR_LAND_LOR_RIGHT:
            {
                m_ircontext.c().bind(m_ircontext.c().named_label(node, "#lshortcut"));

                m_ircontext.apply_eval_result(
                    [&](BytecodeGenerateContext::EvalResult& result)
                    {
                        auto* const shortcut_evaled_value =
                            node->m_IR_value_to_store_shortcut_result.value();

                        m_ircontext.pop_eval_result();

                        const auto& target_storage = result.get_assign_target();
                        if (target_storage.has_value())
                        {
                            auto& [need_box, target] = target_storage.value();
                            woort_IRValue* const* const target_irvalue =
                                std::get_if<woort_IRValue*>(&target);

                            if (target_irvalue == nullptr)
                            {
                                if (need_box)
                                    m_ircontext.c().boxdyn(
                                        shortcut_evaled_value,
                                        WOORT_BOX_VALUE_TYPE_BOOL,
                                        shortcut_evaled_value);

                                m_ircontext.c().store(
                                    std::get<woort_IRStaticIndex>(target),
                                    shortcut_evaled_value);
                            }
                            else
                            {
                                if (need_box)
                                    m_ircontext.c().boxdyn(
                                        *target_irvalue,
                                        WOORT_BOX_VALUE_TYPE_BOOL,
                                        shortcut_evaled_value);
                                else
                                    m_ircontext.c().mov(
                                        *target_irvalue,
                                        shortcut_evaled_value);
                            }

                        }
                        else
                        {
                            result.set_result_stack_temp(
                                m_ircontext,
                                shortcut_evaled_value,
                                node->m_LANG_determined_type.value());
                        }
                    }
                );
                break;
            }
            case AstValueBinaryOperator::IR_HOLD_FOR_NORMAL_LR_EVAL:
                wo_assert(!node->m_LANG_overload_call.has_value());

                m_ircontext.apply_eval_result(
                    [&](BytecodeGenerateContext::EvalResult& result)
                    {
                        auto* const right_opnum = m_ircontext.get_eval_result();
                        auto* const left_opnum = m_ircontext.get_eval_result();

                        using binary_op_t = void (IRCompiler::*)(
                            woort_IRValue*, const woort_IRValue*, const woort_IRValue*);

                        binary_op_t binary_op;

                        lang_TypeInstance* left_type_instance = node->m_left->m_LANG_determined_type.value();
                        auto* left_determined_type = left_type_instance->get_determined_type().value();

                        switch (node->m_operator)
                        {
                        case AstValueBinaryOperator::ADD:
                            switch (left_determined_type->m_base_type)
                            {
                            case lang_TypeInstance::DeterminedType::INTEGER:
                            case lang_TypeInstance::DeterminedType::HANDLE:
                                binary_op = &IRCompiler::addi;
                                break;
                            case lang_TypeInstance::DeterminedType::REAL:
                                binary_op = &IRCompiler::addr;
                                break;
                            case lang_TypeInstance::DeterminedType::STRING:
                                binary_op = &IRCompiler::adds;
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
                            case lang_TypeInstance::DeterminedType::HANDLE:
                                binary_op = &IRCompiler::subi;
                                break;
                            case lang_TypeInstance::DeterminedType::REAL:
                                binary_op = &IRCompiler::subr;
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
                                binary_op = &IRCompiler::muli;
                                break;
                            case lang_TypeInstance::DeterminedType::REAL:
                                binary_op = &IRCompiler::mulr;
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
                                binary_op = &IRCompiler::divi;
                                break;
                            case lang_TypeInstance::DeterminedType::REAL:
                                binary_op = &IRCompiler::divr;
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
                                binary_op = &IRCompiler::modi;
                                break;
                            case lang_TypeInstance::DeterminedType::REAL:
                                binary_op = &IRCompiler::modr;
                                break;
                            default:
                                wo_error("Unknown type.");
                                break;
                            }
                            break;
                        case AstValueBinaryOperator::LOGICAL_AND:
                            binary_op = &IRCompiler::land;
                            break;
                        case AstValueBinaryOperator::LOGICAL_OR:
                            binary_op = &IRCompiler::lor;
                            break;
                        case AstValueBinaryOperator::GREATER:
                            switch (left_determined_type->m_base_type)
                            {
                            case lang_TypeInstance::DeterminedType::INTEGER:
                            case lang_TypeInstance::DeterminedType::HANDLE:
                                binary_op = &IRCompiler::gti;
                                break;
                            case lang_TypeInstance::DeterminedType::REAL:
                                binary_op = &IRCompiler::gtr;
                                break;
                            case lang_TypeInstance::DeterminedType::STRING:
                                binary_op = &IRCompiler::gts;
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
                            case lang_TypeInstance::DeterminedType::HANDLE:
                                binary_op = &IRCompiler::gei;
                                break;
                            case lang_TypeInstance::DeterminedType::REAL:
                                binary_op = &IRCompiler::ger;
                                break;
                            case lang_TypeInstance::DeterminedType::STRING:
                                binary_op = &IRCompiler::ges;
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
                            case lang_TypeInstance::DeterminedType::HANDLE:
                                binary_op = &IRCompiler::lti;
                                break;
                            case lang_TypeInstance::DeterminedType::REAL:
                                binary_op = &IRCompiler::ltr;
                                break;
                            case lang_TypeInstance::DeterminedType::STRING:
                                binary_op = &IRCompiler::lts;
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
                            case lang_TypeInstance::DeterminedType::HANDLE:
                                binary_op = &IRCompiler::lei;
                                break;
                            case lang_TypeInstance::DeterminedType::REAL:
                                binary_op = &IRCompiler::ler;
                                break;
                            case lang_TypeInstance::DeterminedType::STRING:
                                binary_op = &IRCompiler::les;
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
                                binary_op = &IRCompiler::eqi;
                                break;
                            case lang_TypeInstance::DeterminedType::REAL:
                                binary_op = &IRCompiler::eqr;
                                break;
                            case lang_TypeInstance::DeterminedType::STRING:
                                binary_op = &IRCompiler::eqs;
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
                                binary_op = &IRCompiler::nei;
                                break;
                            case lang_TypeInstance::DeterminedType::REAL:
                                binary_op = &IRCompiler::ner;
                                break;
                            case lang_TypeInstance::DeterminedType::STRING:
                                binary_op = &IRCompiler::nes;
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
                        if (target_storage.has_value())
                        {
                            auto& [need_box, target] = target_storage.value();
                            woort_IRValue* const* const target_irvalue =
                                std::get_if<woort_IRValue*>(&target);

                            woort_BoxValueType box_type;
                            const bool type_need_box = need_box
                                && node->m_LANG_determined_type.value()->is_need_to_box_in_IR(&box_type);

                            if (target_irvalue == nullptr)
                            {
                                woort_IRValue* const v = m_ircontext.c().new_value();

                                (m_ircontext.c().*binary_op)(v, left_opnum, right_opnum);

                                if (type_need_box)
                                    m_ircontext.c().boxdyn(v, box_type, v);

                                m_ircontext.c().store(std::get<woort_IRStaticIndex>(target), v);
                            }
                            else
                            {
                                (m_ircontext.c().*binary_op)(*target_irvalue, left_opnum, right_opnum);
                                if (type_need_box)
                                    m_ircontext.c().boxdyn(
                                        *target_irvalue, box_type, *target_irvalue);
                            }
                        }
                        else
                        {
                            woort_IRValue* const v = m_ircontext.c().new_value();

                            (m_ircontext.c().*binary_op)(v, left_opnum, right_opnum);

                            result.set_result_stack_temp(
                                m_ircontext, v, node->m_LANG_determined_type.value());
                        }
                    });

                break;
            case AstValueBinaryOperator::IR_HOLD_FOR_OVERLOAD_CALL_EVAL:
                wo_assert(node->m_LANG_overload_call.has_value());

                m_ircontext.cleanup_for_eval_upper();
                break;
            default:
                wo_error("Unknown hold state.");
                break;
            }
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueUnaryOperator)
    {
        abort();
        return OKAY;
    }
    WO_PASS_PROCESSER(AstValueTribleOperator)
    {
        abort();
        return OKAY;
    }
    WO_PASS_PROCESSER(AstValueIndex)
    {
        abort();
        return OKAY;
    }
    WO_PASS_PROCESSER(AstValueStruct)
    {
        abort();
        return OKAY;
    }
    WO_PASS_PROCESSER(AstValueMakeUnion)
    {
        abort();
        return OKAY;
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
                    auto& [need_box, target] = target_storage.value();
                    woort_IRValue* const* const target_irvalue =
                        std::get_if<woort_IRValue*>(&target);

                    if (target_irvalue == nullptr)
                    {
                        woort_IRValue* const v = m_ircontext.c().new_value();
                        m_ircontext.c().packarg(
                            v,
                            (uint16_t)current_func->m_parameters.size());

                        m_ircontext.c().store(std::get<woort_IRStaticIndex>(target), v);
                    }
                    else
                    {
                        m_ircontext.c().packarg(
                            *target_irvalue,
                            (uint16_t)current_func->m_parameters.size());
                    }
                }
                else
                {
                    woort_IRValue* const v = m_ircontext.c().new_value();

                    m_ircontext.c().packarg(
                        v,
                        (uint16_t)current_func->m_parameters.size());

                    result.set_result_stack_temp(m_ircontext, v, node->m_LANG_determined_type.value());
                }
            }
        );
        return OKAY;
    }
    WO_PASS_PROCESSER(AstValueIROpnum)
    {
        abort();
        return OKAY;
    }
    WO_PASS_PROCESSER(AstValueAssign)
    {
        abort();
        return OKAY;
    }
#undef WO_PASS_PROCESSER

    bool LangContext::pass_final_value(lexer& lex, ast::AstValueBase* val)
    {
        bool anylize_result =
            anylize_pass(lex, val, &LangContext::pass_final_B_process_bytecode_generation, true);

        return anylize_result;
    }
    LangContext::pass_behavior LangContext::pass_final_A_process_bytecode_generation(
        lexer& lex, const AstNodeWithState& node_state, PassProcessStackT& out_stack)
    {
        if (node_state.m_ast_node->node_type >= AstBase::AST_VALUE_begin
            && node_state.m_ast_node->node_type < AstBase::AST_VALUE_end)
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
                    get_type_name(type_instance));

                return FAILED;
            }

            m_ircontext.eval_and_ignore();
            bool result = pass_final_value(lex, eval_value);

            if (result)
                return OKAY;
            else
                return FAILED;
        }
        wo_assert(node_state.m_ast_node->node_type == AstBase::AST_EMPTY
            || m_passir_A_processers->check_has_processer(node_state.m_ast_node->node_type));

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
            auto& constant_value = ast_value->m_evaled_const_value.value();

            // NOTE: We need to let compiler know this constant function.
            walk_through_constant_to_record_function_ast(constant_value);

            // This value has been evaluated as constant value.
            m_ircontext.apply_eval_result(
                [&](BytecodeGenerateContext::EvalResult& result)
                {
                    const auto& asigned_target = result.get_assign_target();
                    if (asigned_target.has_value())
                    {
                        const auto& [need_box, target] = asigned_target.value();

                        woort_IRValue* const* const target_irvalue =
                            std::get_if<woort_IRValue*>(&target);

                        if (target_irvalue == nullptr)
                        {
                            // Assigned to static.
                            m_ircontext.c().store(
                                std::get<woort_IRStaticIndex>(target),
                                need_box
                                ? m_ircontext.c().load_imm_box_const(constant_value)
                                : m_ircontext.c().load_imm_const(constant_value));
                        }
                        else
                        {
                            m_ircontext.c().mov(
                                *target_irvalue,
                                need_box
                                ? m_ircontext.c().load_imm_box_const(constant_value)
                                : m_ircontext.c().load_imm_const(constant_value));
                        }
                    }
                    else
                    {
                        result.set_result_const(m_ircontext, constant_value);
                    }
                });
            return OKAY;
        }

        auto compile_result =
            m_passir_B_processers->process_node(this, lex, node_state, out_stack);

        if (compile_result == FAILED)
            m_ircontext.failed_eval_result();

        return compile_result;
    }
    void LangContext::walk_through_constant_to_record_function_ast(
        const ast::ConstantValue& const_value)
    {
        switch (const_value.m_type)
        {
        case ast::ConstantValue::Type::FUNCTION:
            m_ircontext.m_being_used_function_instance.insert(const_value.value_function());
            break;
        case ast::ConstantValue::Type::STRUCT:
        {
            auto& struct_constant = const_value.value_struct();
            for (size_t idx = 0; idx < struct_constant.m_count; ++idx)
                walk_through_constant_to_record_function_ast(struct_constant.m_elements[idx]);
            break;
        }
        default:
            break;
        }
    }
#endif
}