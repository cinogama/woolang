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
        abort();
        return OKAY;
    }
    WO_PASS_PROCESSER(AstMatchCase)
    {
        abort();
        return OKAY;
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
        abort();
        return OKAY;
    }
    WO_PASS_PROCESSER(AstNop)
    {
        abort();
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
        abort();
        return OKAY;
    }
    WO_PASS_PROCESSER(AstValueTuple)
    {
        abort();
        return OKAY;
    }
    WO_PASS_PROCESSER(AstValueDictOrMap)
    {
        abort();
        return OKAY;
    }
    WO_PASS_PROCESSER(AstValueArrayOrVec)
    {
        abort();
        return OKAY;
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

        if (value_instance->m_IR_normal_function.has_value())
        {
            abort();
            //AstValueFunction* func = value_instance->m_IR_normal_function.value();
            //m_ircontext.apply_eval_result(
            //    [&](BytecodeGenerateContext::EvalResult& result)
            //    {
            //        const auto& target_storage = result.get_assign_target();
            //        auto* function_opnum = m_ircontext.opnum_func(func);
            //        if (target_storage.has_value())
            //        {
            //            m_ircontext.c().mov(
            //                WO_OPNUM(target_storage.value()),
            //                WO_OPNUM(function_opnum));
            //        }
            //        else
            //            result.set_result(m_ircontext, function_opnum);
            //    });
            //return OKAY;
        }

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

                    if (target_irvalue == nullptr)
                    {
                        // Assigned to static.
                        if (variable_storage.m_type == lang_ValueInstance::Storage::GLOBAL)
                        {
                            woort_IRValue* const v = m_ircontext.c().new_value();
                            m_ircontext.c().load(v, variable_storage.m_static_index);
                            m_ircontext.c().store(
                                std::get<woort_IRStaticIndex>(target), v);
                        }
                        else
                        {
                            wo_assert(variable_storage.m_type == lang_ValueInstance::Storage::STACKOFFSET);
                            m_ircontext.c().store(
                                std::get<woort_IRStaticIndex>(target),
                                variable_storage.m_stack_slot);
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
        abort();
        return OKAY;
    }
    WO_PASS_PROCESSER(AstValueTypeCheckIs)
    {
        abort();
        return OKAY;
    }
    WO_PASS_PROCESSER(AstValueTypeCast)
    {
        abort();
        return OKAY;
    }
    WO_PASS_PROCESSER(AstValueDoAsVoid)
    {
        abort();
        return OKAY;
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
        abort();
        return OKAY;
    }
    WO_PASS_PROCESSER(AstValueBinaryOperator)
    {
        abort();
        return OKAY;
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
        abort();
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