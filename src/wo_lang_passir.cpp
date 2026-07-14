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

    static bool is_int_or_handle_type(lang_TypeInstance::DeterminedType::base_type bt)
    {
        return bt == lang_TypeInstance::DeterminedType::INTEGER
            || bt == lang_TypeInstance::DeterminedType::HANDLE;
    }

    static ast::AstValueBinaryOperator* try_get_optimizable_comparison_binop(ast::AstValueBase* cond)
    {
        if (cond->node_type != ast::AstBase::AST_VALUE_BINARY_OPERATOR)
            return nullptr;

        auto* binop = static_cast<ast::AstValueBinaryOperator*>(cond);

        if (binop->m_LANG_overload_call.has_value())
            return nullptr;

        switch (binop->m_operator)
        {
        case ast::AstValueBinaryOperator::LESS:
        case ast::AstValueBinaryOperator::LESS_EQUAL:
        case ast::AstValueBinaryOperator::GREATER:
        case ast::AstValueBinaryOperator::GREATER_EQUAL:
        case ast::AstValueBinaryOperator::EQUAL:
        case ast::AstValueBinaryOperator::NOT_EQUAL:
            break;
        default:
            return nullptr;
        }

        auto* left_type_instance = binop->m_left->m_LANG_determined_type.value();
        auto* right_type_instance = binop->m_right->m_LANG_determined_type.value();

        if (!is_int_or_handle_type(left_type_instance->get_determined_type().value()->m_base_type)
            || !is_int_or_handle_type(right_type_instance->get_determined_type().value()->m_base_type))
            return nullptr;

        return binop;
    }

    static void emit_optimized_jcc_negated(
        IRCompiler& c,
        ast::AstValueBinaryOperator* binop,
        const woort_IRValue* left,
        const woort_IRValue* right,
        woort_IRLabel* target)
    {
        switch (binop->m_operator)
        {
        case ast::AstValueBinaryOperator::LESS:          c.jcc_ge(left, right, target); break;
        case ast::AstValueBinaryOperator::LESS_EQUAL:     c.jcc_gt(left, right, target); break;
        case ast::AstValueBinaryOperator::GREATER:       c.jcc_le(left, right, target); break;
        case ast::AstValueBinaryOperator::GREATER_EQUAL:  c.jcc_lt(left, right, target); break;
        case ast::AstValueBinaryOperator::EQUAL:          c.jcc_ne(left, right, target); break;
        case ast::AstValueBinaryOperator::NOT_EQUAL:      c.jcc_eq(left, right, target); break;
        default: wo_error("Unknown comparison operator."); break;
        }
    }

    static void emit_optimized_jcc_direct(
        IRCompiler& c,
        ast::AstValueBinaryOperator* binop,
        const woort_IRValue* left,
        const woort_IRValue* right,
        woort_IRLabel* target)
    {
        switch (binop->m_operator)
        {
        case ast::AstValueBinaryOperator::LESS:          c.jcc_lt(left, right, target); break;
        case ast::AstValueBinaryOperator::LESS_EQUAL:     c.jcc_le(left, right, target); break;
        case ast::AstValueBinaryOperator::GREATER:       c.jcc_gt(left, right, target); break;
        case ast::AstValueBinaryOperator::GREATER_EQUAL:  c.jcc_ge(left, right, target); break;
        case ast::AstValueBinaryOperator::EQUAL:          c.jcc_eq(left, right, target); break;
        case ast::AstValueBinaryOperator::NOT_EQUAL:      c.jcc_ne(left, right, target); break;
        default: wo_error("Unknown comparison operator."); break;
        }
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

            const bool use_pvalue = m_repl_context.has_value()
                && m_repl_context.value()->m_pvalue_indirect_for_mutable_statics
                && instance->m_mutable;

            instance->m_IR_storage.emplace(
                lang_ValueInstance::Storage(static_storage, use_pvalue));

            m_ircontext.c().record_static_var(
                instance->m_symbol->m_name->c_str(), static_storage);
        }
        else
        {
            // Is stack variable.
            woort_IRValue* const stack_storage =
                m_ircontext.c().new_value();

            instance->m_IR_storage.emplace(
                lang_ValueInstance::Storage(stack_storage));

            m_ircontext.c().record_local_var(
                instance->m_symbol->m_name->c_str(), stack_storage);
        }

        auto& storage = instance->m_IR_storage.value();

        if (storage.m_type == lang_ValueInstance::Storage::StorageType::GLOBAL)
        {
            const woort_IRValue* store_src;
            if (tuple_member_offset.has_value())
            {
                const uint16_t index = tuple_member_offset.value();

                woort_IRValue* const v =
                    m_ircontext.c().new_value();

                m_ircontext.c().ldidstruct(v, opnumval, index);
                store_src = v;
            }
            else
                store_src = opnumval;

            if (storage.m_is_pvalue_indirect)
            {
                woort_IRValue* const box = m_ircontext.c().new_value();
                m_ircontext.c().mkpvalue(box, store_src);
                m_ircontext.c().store(storage.m_static_index, box);
            }
            else
                m_ircontext.c().store(storage.m_static_index, store_src);
        }
        else
        {
            wo_assert(storage.m_type == lang_ValueInstance::Storage::StorageType::STACKOFFSET);
            if (tuple_member_offset.has_value())
            {
                const uint16_t index = tuple_member_offset.value();
                m_ircontext.c().ldidstruct(storage.m_stack_slot, opnumval, index);
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

                m_ircontext.c().ldidstruct(tuple_source_for_write, opnumval, index);
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
        WO_LANG_REGISTER_PROCESSER(AstEchoForREPL, AstBase::AST_ECHO_FOR_REPL, passir_A);

        // WO_LANG_REGISTER_PROCESSER(AstValueNothing, AstBase::AST_VALUE_NOTHING, passir_B);
        WO_LANG_REGISTER_PROCESSER(AstValueMarkAsMutable, AstBase::AST_VALUE_MARK_AS_MUTABLE, passir_B);
        WO_LANG_REGISTER_PROCESSER(AstValueMarkAsImmutable, AstBase::AST_VALUE_MARK_AS_IMMUTABLE, passir_B);
        WO_LANG_REGISTER_PROCESSER(AstValueLiteral, AstBase::AST_VALUE_LITERAL, passir_B);
        WO_LANG_REGISTER_PROCESSER(AstValueTypeid, AstBase::AST_VALUE_TYPEID, passir_B);
        WO_LANG_REGISTER_PROCESSER(AstValueTypeCast, AstBase::AST_VALUE_TYPE_CAST, passir_B);
        WO_LANG_REGISTER_PROCESSER(AstValueDoAsVoid, AstBase::AST_VALUE_DO_AS_VOID, passir_B);
        WO_LANG_REGISTER_PROCESSER(AstValueTypeCheckIs, AstBase::AST_VALUE_TYPE_CHECK_IS, passir_B);
        WO_LANG_REGISTER_PROCESSER(AstValueTypeCheckAssert, AstBase::AST_VALUE_TYPE_CHECK_ASSERT, passir_B);
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
                auto* binop = try_get_optimizable_comparison_binop(node->m_condition);
                if (binop)
                {
                    m_ircontext.begin_eval_readonly();
                    if (!pass_final_value(lex, binop->m_left))
                        return FAILED;
                    auto* left_val = m_ircontext.get_eval_result();

                    m_ircontext.begin_eval_readonly();
                    if (!pass_final_value(lex, binop->m_right))
                        return FAILED;
                    auto* right_val = m_ircontext.get_eval_result();

                    emit_optimized_jcc_negated(m_ircontext.c(), binop, left_val, right_val,
                        m_ircontext.c().named_label(node, "#if_else"));
                }
                else
                {
                    m_ircontext.begin_eval_readonly();
                    if (!pass_final_value(lex, node->m_condition))
                        return FAILED;

                    m_ircontext.c().jccz(
                        m_ircontext.get_eval_result(),
                        m_ircontext.c().named_label(node, "#if_else"));
                }

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
                auto* binop = try_get_optimizable_comparison_binop(node->m_condition);
                if (binop)
                {
                    m_ircontext.begin_eval_readonly();
                    if (!pass_final_value(lex, binop->m_left))
                        return FAILED;
                    auto* left_val = m_ircontext.get_eval_result();

                    m_ircontext.begin_eval_readonly();
                    if (!pass_final_value(lex, binop->m_right))
                        return FAILED;
                    auto* right_val = m_ircontext.get_eval_result();

                    emit_optimized_jcc_negated(m_ircontext.c(), binop, left_val, right_val,
                        m_ircontext.c().named_label(node, "#while_end"));
                }
                else
                {
                    m_ircontext.begin_eval_readonly();
                    if (!pass_final_value(lex, node->m_condition))
                        return FAILED;

                    m_ircontext.c().jccz(
                        m_ircontext.get_eval_result(),
                        m_ircontext.c().named_label(node, "#while_end"));
                }
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

                    auto* binop = try_get_optimizable_comparison_binop(node->m_condition.value());
                    if (binop)
                    {
                        m_ircontext.begin_eval_readonly();
                        if (!pass_final_value(lex, binop->m_left))
                            return FAILED;
                        auto* left_val = m_ircontext.get_eval_result();

                        m_ircontext.begin_eval_readonly();
                        if (!pass_final_value(lex, binop->m_right))
                            return FAILED;
                        auto* right_val = m_ircontext.get_eval_result();

                        emit_optimized_jcc_direct(m_ircontext.c(), binop, left_val, right_val,
                            m_ircontext.c().named_label(node, "#for_begin"));
                    }
                    else
                    {
                        m_ircontext.begin_eval_readonly();
                        if (!pass_final_value(lex, node->m_condition.value()))
                            return FAILED;

                        m_ircontext.c().jcc(
                            m_ircontext.get_eval_result(),
                            m_ircontext.c().named_label(node, "#for_begin"));
                    }
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
            // Clear stale IR state from prior compilation (REPL re-compile).
            for (auto* match_case : node->m_cases)
            {
                match_case->m_IR_matching_index_opnum_MUST_BE_CLEAR_FOR_REPL.reset();
                match_case->m_IR_matching_struct_opnum_MUST_BE_CLEAR_FOR_REPL.reset();
            }

            m_ircontext.begin_eval_readonly();
            if (!pass_final_value(lex, node->m_matched_value))
                return FAILED;

            auto* matching_value = m_ircontext.get_eval_result();
            woort_IRValue* const matching_index = m_ircontext.c().new_value();
            m_ircontext.c().ldidstruct(
                matching_index,
                matching_value,
                0);

            for (auto& match_case : node->m_cases)
            {
                match_case->m_IR_matching_index_opnum_MUST_BE_CLEAR_FOR_REPL = matching_index;
                match_case->m_IR_matching_struct_opnum_MUST_BE_CLEAR_FOR_REPL = matching_value;
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
                    node->m_IR_matching_index_opnum_MUST_BE_CLEAR_FOR_REPL.value(),
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
                        node->m_IR_matching_struct_opnum_MUST_BE_CLEAR_FOR_REPL.value(),
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
            m_ircontext.c().bind(m_ircontext.c().named_label(node, "#match_case_end"));
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
            m_ircontext.eval_and_ignore();
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
                            m_ircontext.eval_and_ignore();
                            if (!pass_final_value(lex, function.value()))
                                // Failed 
                                return FAILED;
                        }
                    }
                    else
                    {
                        // 
                        wo_assert(!template_value_instance->m_IR_storage.has_value());
                        bool pvalue_boxing = false;
                        if (template_value_instance->m_symbol->m_is_global
                            || template_value_instance->m_symbol->is_declared_as_static())
                        {
                            // Is static variable,
                            const woort_IRStaticIndex static_storage =
                                m_ircontext.c().alloc_static();

                            const bool use_pvalue = m_repl_context.has_value()
                                && m_repl_context.value()->m_pvalue_indirect_for_mutable_statics
                                && template_value_instance->m_mutable;

                            template_value_instance->m_IR_storage.emplace(
                                lang_ValueInstance::Storage(static_storage, use_pvalue));

                            m_ircontext.c().record_static_var(
                                template_value_instance->m_symbol->m_name->c_str(), static_storage);

                            if (use_pvalue)
                            {
                                pvalue_boxing = true;
                                m_ircontext.begin_eval_readonly();
                            }
                            else
                                m_ircontext.eval_to_assign_static(static_storage, node);
                        }
                        else
                        {
                            // Is stack variable.
                            woort_IRValue* const stack_storage =
                                m_ircontext.c().new_value();

                            template_value_instance->m_IR_storage.emplace(
                                lang_ValueInstance::Storage(stack_storage));

                            m_ircontext.c().record_local_var(
                                template_value_instance->m_symbol->m_name->c_str(), stack_storage);

                            m_ircontext.eval_to_assign(stack_storage, node);
                        }

                        if (!pass_final_value(lex, static_cast<AstValueBase*>(template_instance->m_ast)))
                            // Failed 
                            return FAILED;

                        if (pvalue_boxing)
                        {
                            const woort_IRValue* const init_val = m_ircontext.get_eval_result();
                            woort_IRValue* const box = m_ircontext.c().new_value();
                            m_ircontext.c().mkpvalue(box, init_val);
                            m_ircontext.c().store(
                                template_value_instance->m_IR_storage.value().m_static_index, box);
                        }
                        else
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
                bool pvalue_boxing = false;
                if (pattern_symbol->m_value_instance->m_symbol->m_is_global
                    || pattern_symbol->m_value_instance->m_symbol->is_declared_as_static())
                {
                    // Is static variable,
                    const woort_IRStaticIndex static_storage =
                        m_ircontext.c().alloc_static();

                    const bool use_pvalue = m_repl_context.has_value()
                        && m_repl_context.value()->m_pvalue_indirect_for_mutable_statics
                        && pattern_symbol->m_value_instance->m_mutable;

                    pattern_symbol->m_value_instance->m_IR_storage.emplace(
                        lang_ValueInstance::Storage(static_storage, use_pvalue));

                    m_ircontext.c().record_static_var(
                        pattern_symbol->m_value_instance->m_symbol->m_name->c_str(), static_storage);

                    if (use_pvalue)
                    {
                        pvalue_boxing = true;
                        m_ircontext.begin_eval_readonly();
                    }
                    else
                        m_ircontext.eval_to_assign_static(static_storage, node);
                }
                else
                {
                    // Is stack variable.
                    woort_IRValue* const stack_storage =
                        m_ircontext.c().new_value();

                    pattern_symbol->m_value_instance->m_IR_storage.emplace(
                        lang_ValueInstance::Storage(stack_storage));

                    m_ircontext.c().record_local_var(
                        pattern_symbol->m_value_instance->m_symbol->m_name->c_str(), stack_storage);

                    m_ircontext.eval_to_assign(stack_storage, node);
                }

                if (!pass_final_value(lex, static_cast<AstValueBase*>(node->m_init_value)))
                    // Failed 
                    return FAILED;

                if (pvalue_boxing)
                {
                    const woort_IRValue* const init_val = m_ircontext.get_eval_result();
                    woort_IRValue* const box = m_ircontext.c().new_value();
                    m_ircontext.c().mkpvalue(box, init_val);
                    m_ircontext.c().store(
                        pattern_symbol->m_value_instance->m_IR_storage.value().m_static_index, box);
                }
                else
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
            // Clear stale IR state from prior compilation (REPL re-compile).
            node->m_IR_return_value_may_none_MUST_BE_CLEAR_FOR_REPL.reset();

            if (node->m_value.has_value())
            {
                if (node->m_LANG_defer_instances.empty())
                    m_ircontext.begin_eval_readonly();
                else
                {
                    woort_IRValue* v = m_ircontext.c().new_value();
                    m_ircontext.eval_to_assign(v, node);

                    node->m_IR_return_value_may_none_MUST_BE_CLEAR_FOR_REPL.emplace(v);
                }

                if (!pass_final_value(lex, node->m_value.value()))
                    return FAILED;

                if (node->m_IR_return_value_may_none_MUST_BE_CLEAR_FOR_REPL.has_value())
                    m_ircontext.pop_eval_result();
                else
                    node->m_IR_return_value_may_none_MUST_BE_CLEAR_FOR_REPL.emplace(
                        m_ircontext.get_eval_result());
            }

            WO_CONTINUE_PROCESS_LIST(node->m_LANG_defer_instances);
            return HOLD;
        }
        else if (state == HOLD)
        {
            bool is_ret_void = true;
            if (node->m_IR_return_value_may_none_MUST_BE_CLEAR_FOR_REPL.has_value())
            {
                lang_TypeInstance* const ret_type =
                    node->m_value.value()->m_LANG_determined_type.value();

                if (lang_TypeInstance::DeterminedType::base_type::VOID
                    != ret_type->get_determined_type().value()->m_base_type)
                {
                    is_ret_void = false;
                }
            }

            if (is_ret_void)
            {
                if (node->m_LANG_belong_function_may_null_if_outside.has_value())
                    m_ircontext.c().ret_void();
                else
                    m_ircontext.c().ret(m_ircontext.c().load_imm_int(0));
            }
            else
            {
                m_ircontext.c().ret(node->m_IR_return_value_may_none_MUST_BE_CLEAR_FOR_REPL.value());
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
    WO_PASS_PROCESSER(AstEchoForREPL)
    {
        wo_assert(state == UNPROCESSED);

        woort_IRConstantIndex echo_cidx =
            m_ircontext.c().alloc_direct_extern_function(
                rslib_extern_symbols::g_builtin_debug_print);

        m_ircontext.eval_to_push_box();
        if (!pass_final_value(lex, node->m_expression))
            return FAILED;

        m_ircontext.c().pushchk(m_ircontext.c().load_imm_int(1));

        m_ircontext.c().callnfp(echo_cidx, 2, nullptr);

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

        // Clear stale IR state from prior compilation (REPL re-compile).
        node->m_IR_function_MUST_BE_CLEAR_FOR_REPL.reset();

        m_ircontext.m_being_used_function_instance.insert(node);

        if (node->m_LANG_captured_context.m_captured_variables.empty())
        {
            // Simple normal function
            m_ircontext.apply_eval_result(
                [&](BytecodeGenerateContext::EvalResult& result)
                {
                    const auto& target_storage = result.get_assign_target(node->m_LANG_determined_type.value());
                    if (target_storage.has_value())
                    {
                        auto& [need_box, target] = target_storage.value();
                        woort_IRValue* const* const target_irvalue =
                            std::get_if<woort_IRValue*>(&target);

                        if (target_irvalue == nullptr)
                        {
                            if (need_box.has_value())
                            {
                                woort_IRValue* const v = m_ircontext.c().new_value();
                                m_ircontext.c().boxdyn(
                                    v, need_box.value(), m_ircontext.c().load_imm_closure(node));
                                m_ircontext.c().store(
                                    std::get<woort_IRStaticIndex>(target), v);
                            }
                            else
                            {
                                m_ircontext.c().store(
                                    std::get<woort_IRStaticIndex>(target),
                                    m_ircontext.c().load_imm_closure(node));
                            }
                        }
                        else
                        {
                            m_ircontext.c().mov(
                                *target_irvalue,
                                m_ircontext.c().load_imm_closure(node));

                            if (need_box.has_value())
                                m_ircontext.c().boxdyn(
                                    *target_irvalue,
                                    need_box.value(),
                                    *target_irvalue);
                        }
                    }
                    else
                        result.set_result_const_closure(
                            m_ircontext, node);
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

                        wo_assert(storage.m_type == lang_ValueInstance::Storage::StorageType::STACKOFFSET);
                        m_ircontext.c().pushchk(storage.m_stack_slot);
                    }

                    const auto& target_storage = result.get_assign_target(node->m_LANG_determined_type.value());
                    if (target_storage.has_value())
                    {
                        auto& [need_box, target] = target_storage.value();
                        woort_IRValue* const* const target_irvalue =
                            std::get_if<woort_IRValue*>(&target);

                        if (target_irvalue == nullptr)
                        {
                            woort_IRValue* const v = m_ircontext.c().new_value();
                            m_ircontext.c().mkclosure(
                                v,
                                (uint32_t)node->m_LANG_captured_context.m_captured_variables.size(),
                                m_ircontext.c().imm_closure(node));

                            if (need_box.has_value())
                                m_ircontext.c().boxdyn(v, need_box.value(), v);

                            m_ircontext.c().store(std::get<woort_IRStaticIndex>(target), v);
                        }
                        else
                        {
                            m_ircontext.c().mkclosure(
                                *target_irvalue,
                                (uint32_t)node->m_LANG_captured_context.m_captured_variables.size(),
                                m_ircontext.c().imm_closure(node));

                            if (need_box.has_value())
                                m_ircontext.c().boxdyn(
                                    *target_irvalue,
                                    need_box.value(),
                                    *target_irvalue);
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
                    m_ircontext.do_eval_if_not_ignore(
                        &BytecodeGenerateContext::eval_action_and_ignore);
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

                    const auto& asigned_target = result.get_assign_target(node->m_LANG_determined_type.value());

                    std::optional<woort_BoxValueType> need_box;

                    if (asigned_target.has_value())
                    {
                        const auto& [nb, target] = asigned_target.value();
                        need_box = nb;

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

                    if (need_box.has_value())
                        m_ircontext.c().boxdyn(make_result_target, need_box.value(), make_result_target);

                    if (need_write_back_to_static.has_value())
                    {
                        m_ircontext.c().store(need_write_back_to_static.value(), make_result_target);
                    }
                    else if (!asigned_target.has_value())
                    {
                        result.set_result_stack_temp(
                            m_ircontext,
                            make_result_target,
                            node->m_LANG_determined_type.value());
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

                    const auto& asigned_target =
                        result.get_assign_target(node->m_LANG_determined_type.value());

                    std::optional<woort_BoxValueType> need_box;

                    if (asigned_target.has_value())
                    {
                        const auto& [nb, target] = asigned_target.value();
                        need_box = nb;

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
                    }

                    const uint32_t elem_count = (uint32_t)node->m_elements.size();
                    m_ircontext.c().mkmap(make_result_target, elem_count);

                    if (need_box.has_value())
                        m_ircontext.c().boxdyn(make_result_target, need_box.value(), make_result_target);

                    if (need_write_back_to_static.has_value())
                    {
                        m_ircontext.c().store(need_write_back_to_static.value(), make_result_target);
                    }
                    else if (!asigned_target.has_value())
                    {
                        result.set_result_stack_temp(
                            m_ircontext,
                            make_result_target,
                            node->m_LANG_determined_type.value());
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

                    const auto& asigned_target = result.get_assign_target(node->m_LANG_determined_type.value());

                    std::optional<woort_BoxValueType> need_box;

                    if (asigned_target.has_value())
                    {
                        const auto& [nb, target] = asigned_target.value();
                        need_box = nb;

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
                    }

                    const uint32_t elem_count = (uint32_t)node->m_elements.size();
                    m_ircontext.c().mkvec(make_result_target, elem_count);

                    if (need_box.has_value())
                        m_ircontext.c().boxdyn(make_result_target, need_box.value(), make_result_target);

                    if (need_write_back_to_static.has_value())
                    {
                        m_ircontext.c().store(need_write_back_to_static.value(), make_result_target);
                    }
                    else if (!asigned_target.has_value())
                    {
                        result.set_result_stack_temp(
                            m_ircontext,
                            make_result_target,
                            node->m_LANG_determined_type.value());
                    }
                });
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueFunctionCall)
    {
        if (state == UNPROCESSED)
        {
            // Clear stale IR state from prior compilation (REPL re-compile).
            node->m_IR_unpack_counter_if_in_variadic_func_MUST_BE_CLEAR_FOR_REPL.reset();
            // NOTE: m_IR_unpack_all_counter_MUST_BE_CLEAR_FOR_REPL on AstFakeValueUnpack
            //       arguments is always overwritten by this processor (line below at
            //       emplace) before AstFakeValueUnpack reads it, so no reset needed here.

            if (node->m_function->node_type == AstBase::AST_VALUE_VARIABLE)
            {
                AstValueVariable* func_var = static_cast<AstValueVariable*>(node->m_function);
                if (func_var->m_LANG_variable_instance.value()->m_determined_constant_or_function.has_value())
                {
                    // If no captured variables, we can call near.
                    // But in REPL, skip near-call for functions emitted in a
                    // prior eval — those must go through CALL via a FAR-CALL
                    // closure that references the prior CodeEnv, rather than
                    // being re-emitted in this CodeEnv via CALLNWO.
                    auto func_opt = func_var->m_LANG_variable_instance.value()->
                        m_determined_constant_or_function.value().value_try_function();
                    if (func_opt.has_value()
                        && (!m_repl_context.has_value()
                            || m_repl_context.value()->m_prior_function_bytecode.find(func_opt.value())
                            == m_repl_context.value()->m_prior_function_bytecode.end()))
                    {
                        node->m_IR_invoking_function_near.emplace(func_opt.value());
                    }
                }
            }
            else if (node->m_function->node_type == AstBase::AST_VALUE_FUNCTION)
            {
                ast::AstValueFunction* invoking_target_function = static_cast<ast::AstValueFunction*>(node->m_function);
                if (invoking_target_function->m_LANG_captured_context.m_captured_variables.empty())
                {
                    // No captured variables, we can call near.
                    node->m_IR_invoking_function_near = invoking_target_function;

                    // What ever, we still need compiler known this function.
                    m_ircontext.eval_and_ignore();
                    WO_CONTINUE_PROCESS(node->m_function);
                }
            }
            bool has_invoking_function_near = node->m_IR_invoking_function_near.has_value();
            if (has_invoking_function_near)
            {
                auto* function = node->m_IR_invoking_function_near.value();
                m_ircontext.m_being_used_function_instance.insert(function);

                if (function->m_LANG_extern_information.has_value())
                {
                    auto* extern_information = function->m_LANG_extern_information.value();
                    if (extern_information->m_IR_externed_function_MUST_BE_CLEAR_FOR_REPL.has_value())
                    {
                        auto* externed_function = extern_information->m_IR_externed_function_MUST_BE_CLEAR_FOR_REPL.value();
                        if (config::ENABLE_SKIP_INVOKE_UNSAFE_CAST
                            && rslib_extern_symbols::g_builtin_return_it_self == externed_function)
                        {
                            // TODO: RISK IN BOXED TYPE, WE NEED DO EXTRA CAST?

                            // Optimized for rslib_std_return_itself.
                            m_ircontext.eval_for_upper_as_type(node->m_LANG_determined_type.value());
                            WO_CONTINUE_PROCESS(node->m_arguments.front());

                            node->m_LANG_hold_state = AstValueFunctionCall::IR_HOLD_FOR_FAST_ITSELF;
                            return HOLD;
                        }
                    }
                }
            }

            const size_t target_function_named_param_count =
                node->m_function->m_LANG_determined_type.value()->get_determined_type().value()->
                m_external_type_description.m_function->m_param_types.size();

            size_t arg_index = 0;
            for (auto* argument : node->m_arguments)
            {
                // Functions arguments will be pushed into stack inversely.
                // So here we eval them by origin order, stack pop will be in reverse order.

                if (argument->node_type == AstBase::AST_FAKE_VALUE_UNPACK)
                {
                    if (node->m_LANG_has_runtime_full_unpackargs)
                    {
                        AstFakeValueUnpack* const ast_fake_value_unpack =
                            static_cast<AstFakeValueUnpack*>(argument);

                        wo_assert(ast_fake_value_unpack->m_LANG_unpack_method ==
                            AstFakeValueUnpack::IR_unpack_method::UNPACK_FOR_FUNCTION_CALL);
                        wo_assert(ast_fake_value_unpack->m_LANG_need_to_be_unpack_count.value().m_unpack_all);

                        woort_IRValue* const v = m_ircontext.c().new_value();

                        node->m_IR_unpack_counter_if_in_variadic_func_MUST_BE_CLEAR_FOR_REPL.emplace(v);
                        ast_fake_value_unpack->m_IR_unpack_all_counter_MUST_BE_CLEAR_FOR_REPL.emplace(v);
                    }
                    m_ircontext.eval_action_and_ignore();
                }
                else if (arg_index < target_function_named_param_count)
                    m_ircontext.eval_to_push();
                else
                    m_ircontext.eval_to_push_box();

                ++arg_index;

                WO_CONTINUE_PROCESS(argument);
            }

            wo_assert(node->m_LANG_has_runtime_full_unpackargs ==
                node->m_IR_unpack_counter_if_in_variadic_func_MUST_BE_CLEAR_FOR_REPL.has_value());

            if (!has_invoking_function_near)
            {
                // We need eval it.
                m_ircontext.begin_eval_readonly();
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
                uint32_t fact_argument_to_pop = (uint32_t)node->m_LANG_certenly_function_argument_count;
                if (node->m_LANG_invoking_variadic_function)
                {
                    // Need push extra argument counter.
                    if (node->m_IR_unpack_counter_if_in_variadic_func_MUST_BE_CLEAR_FOR_REPL.has_value())
                    {
                        woort_IRValue* const v =
                            node->m_IR_unpack_counter_if_in_variadic_func_MUST_BE_CLEAR_FOR_REPL.value();

                        if (fact_argument_to_pop != 0)
                        {
                            m_ircontext.c().addi(
                                v,
                                v,
                                m_ircontext.c().load_imm_int((woort_Int)fact_argument_to_pop));
                        }
                        m_ircontext.c().pushchk(v);

                        fact_argument_to_pop = 0;
                    }
                    else
                    {
                        m_ircontext.c().pushchk(
                            m_ircontext.c().load_imm_int((woort_Int)fact_argument_to_pop));
                    }
                    ++fact_argument_to_pop;
                }

                auto emit_invoke_call = [&](woort_IRValue* dst) {
                    if (node->m_IR_invoking_function_near.has_value())
                    {
                        AstValueFunction* invoking_function = node->m_IR_invoking_function_near.value();
                        wo_assert(invoking_function->m_LANG_captured_context.m_captured_variables.empty());

                        if (invoking_function->m_LANG_extern_information.has_value())
                            m_ircontext.c().callnfp(
                                m_ircontext.c().imm_function(invoking_function),
                                fact_argument_to_pop,
                                dst);
                        else
                            m_ircontext.c().callnwo(
                                m_ircontext.c().imm_function(invoking_function),
                                fact_argument_to_pop,
                                dst);
                    }
                    else
                    {
                        auto* opnumbase = m_ircontext.get_eval_result();
                        m_ircontext.c().call(opnumbase, fact_argument_to_pop, dst);
                    }
                    };

                // Ok, invoke finished.
                if (m_ircontext.is_eval_result_just_ignored())
                {
                    emit_invoke_call(nullptr);

                    if (node->m_IR_unpack_counter_if_in_variadic_func_MUST_BE_CLEAR_FOR_REPL.has_value())
                        m_ircontext.c().poprs(node->m_IR_unpack_counter_if_in_variadic_func_MUST_BE_CLEAR_FOR_REPL.value());
                }

                m_ircontext.apply_eval_result(
                    [&](BytecodeGenerateContext::EvalResult& result)
                    {
                        lang_TypeInstance* const type_instance = node->m_LANG_determined_type.value();
                        const bool is_void = type_instance->is_based_on_void_in_IR();

                        const auto& target_storage = result.get_assign_target(type_instance);

                        if (is_void)
                        {
                            emit_invoke_call(nullptr);
                            if (node->m_IR_unpack_counter_if_in_variadic_func_MUST_BE_CLEAR_FOR_REPL.has_value())
                                m_ircontext.c().poprs(node->m_IR_unpack_counter_if_in_variadic_func_MUST_BE_CLEAR_FOR_REPL.value());

                            if (!target_storage.has_value())
                                result.set_result_junk(m_ircontext);
                        }
                        else
                        {
                            if (target_storage.has_value())
                            {
                                auto& [need_box, target] = target_storage.value();
                                woort_IRValue* const* const target_irvalue =
                                    std::get_if<woort_IRValue*>(&target);

                                if (target_irvalue == nullptr)
                                {
                                    woort_IRValue* const v = m_ircontext.c().new_value();

                                    emit_invoke_call(v);

                                    if (need_box.has_value())
                                        m_ircontext.c().boxdyn(v, need_box.value(), v);

                                    m_ircontext.c().store(std::get<woort_IRStaticIndex>(target), v);
                                }
                                else
                                {
                                    emit_invoke_call(*target_irvalue);

                                    if (need_box.has_value())
                                        m_ircontext.c().boxdyn(*target_irvalue, need_box.value(), *target_irvalue);
                                }
                                if (node->m_IR_unpack_counter_if_in_variadic_func_MUST_BE_CLEAR_FOR_REPL.has_value())
                                    m_ircontext.c().poprs(node->m_IR_unpack_counter_if_in_variadic_func_MUST_BE_CLEAR_FOR_REPL.value());
                            }
                            else
                            {
                                woort_IRValue* const v = m_ircontext.c().new_value();
                                emit_invoke_call(v);

                                if (node->m_IR_unpack_counter_if_in_variadic_func_MUST_BE_CLEAR_FOR_REPL.has_value())
                                    m_ircontext.c().poprs(node->m_IR_unpack_counter_if_in_variadic_func_MUST_BE_CLEAR_FOR_REPL.value());

                                result.set_result_stack_temp(m_ircontext, v, node->m_LANG_determined_type.value());
                            }
                        }
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
        wo_assert(value_instance->IR_need_storage());

        if (!value_instance->m_IR_storage.has_value())
        {
            lex.record_lang_error(lexer::msglevel_t::error, node,
                WO_ERR_VARIABLE_STORAGE_NOT_DETERMINED,
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
                lang_TypeInstance* const type_instance = node->m_LANG_determined_type.value();
                const bool is_void = type_instance->is_based_on_void_in_IR();

                const auto target_storage = result.get_assign_target(type_instance);
                if (target_storage.has_value())
                {
                    if (is_void)
                        ; // Nothing to do, void value is junk.
                    else
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

                                if (variable_storage.m_is_pvalue_indirect)
                                    m_ircontext.c().loadpvalue(v, v);

                                if (need_box.has_value())
                                    m_ircontext.c().boxdyn(v, need_box.value(), v);

                                m_ircontext.c().store(
                                    std::get<woort_IRStaticIndex>(target), v);
                            }
                            else
                            {
                                wo_assert(variable_storage.m_type == lang_ValueInstance::Storage::STACKOFFSET);

                                woort_IRValue* v = variable_storage.m_stack_slot;

                                if (need_box.has_value())
                                {
                                    v = m_ircontext.c().new_value();
                                    m_ircontext.c().boxdyn(v, need_box.value(), variable_storage.m_stack_slot);
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

                                if (variable_storage.m_is_pvalue_indirect)
                                    m_ircontext.c().loadpvalue(v, v);

                                m_ircontext.c().mov(*target_irvalue, v);
                            }
                            else
                            {
                                wo_assert(variable_storage.m_type == lang_ValueInstance::Storage::STACKOFFSET);
                                m_ircontext.c().mov(*target_irvalue, variable_storage.m_stack_slot);
                            }

                            if (need_box.has_value())
                                m_ircontext.c().boxdyn(*target_irvalue, need_box.value(), *target_irvalue);
                        }
                    }
                }
                else if (!is_void)
                {
                    if (variable_storage.m_type == lang_ValueInstance::Storage::GLOBAL)
                    {
                        if (variable_storage.m_is_pvalue_indirect)
                        {
                            woort_IRValue* const ptr = m_ircontext.c().new_value();
                            woort_IRValue* const v = m_ircontext.c().new_value();
                            m_ircontext.c().load(ptr, variable_storage.m_static_index);
                            m_ircontext.c().loadpvalue(v, ptr);
                            result.set_result_stack_temp(
                                m_ircontext, v, node->m_LANG_determined_type.value());
                        }
                        else
                            result.set_result_static(
                                m_ircontext,
                                variable_storage.m_static_index,
                                node->m_LANG_determined_type.value());
                    }
                    else
                    {
                        wo_assert(variable_storage.m_type == lang_ValueInstance::Storage::STACKOFFSET);
                        result.set_result_stack_var(
                            m_ircontext,
                            variable_storage.m_stack_slot,
                            node->m_LANG_determined_type.value());
                    }
                }
                else
                {
                    result.set_result_junk(m_ircontext);
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
    WO_PASS_PROCESSER(AstValueTypeCheckAssert)
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

                        const auto& target_storage = result.get_assign_target(node->m_LANG_determined_type.value());
                        if (target_storage.has_value())
                        {
                            auto& [need_box, target] = target_storage.value();
                            woort_IRValue* const* const target_irvalue =
                                std::get_if<woort_IRValue*>(&target);

                            if (target_irvalue == nullptr)
                            {
                                if (need_box.has_value())
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

                    const auto& target_storage = result.get_assign_target(node->m_LANG_determined_type.value());
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

                            if (need_box.has_value())
                                m_ircontext.c().boxdyn(v, need_box.value(), v);

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
                                    *target_irvalue, need_box.value(), *target_irvalue);
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
            if (node->m_LANG_overload_call.has_value())
            {
                m_ircontext.eval_for_upper();
                WO_CONTINUE_PROCESS(node->m_LANG_overload_call.value());

                node->m_LANG_hold_state = AstValueMayConsiderOperatorOverload::IR_HOLD_FOR_OVERLOAD_CALL_EVAL;
                return HOLD;
            }
            else
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
            } // close else block for non-overload path
        }
        else if (state == HOLD)
        {
            if (node->m_LANG_hold_state == AstValueMayConsiderOperatorOverload::IR_HOLD_FOR_OVERLOAD_CALL_EVAL)
            {
                wo_assert(node->m_LANG_overload_call.has_value());

                m_ircontext.cleanup_for_eval_upper();
            }
            else
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

                            const auto& target_storage = result.get_assign_target(node->m_LANG_determined_type.value());

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
                                return;
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

                                        if (need_box.has_value())
                                            m_ircontext.c().boxdyn(
                                                v, need_box.value(), v);

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
                                                *target_irvalue, need_box.value(), *target_irvalue);
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
                                                    m_ircontext.c().boxdyn(v, need_box.value(), v);

                                                m_ircontext.c().store(std::get<woort_IRStaticIndex>(target), v);
                                            }
                                            else
                                            {
                                                m_ircontext.c().rtoi(*target_irvalue, opnum_to_cast);
                                                if (need_box)
                                                    m_ircontext.c().boxdyn(
                                                        *target_irvalue,
                                                        need_box.value(),
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
                                                    m_ircontext.c().boxdyn(v, need_box.value(), v);

                                                m_ircontext.c().store(std::get<woort_IRStaticIndex>(target), v);
                                            }
                                            else
                                            {
                                                m_ircontext.c().caststo(
                                                    *target_irvalue, opnum_to_cast, WOORT_BOX_VALUE_TYPE_INT);

                                                if (need_box)
                                                    m_ircontext.c().boxdyn(
                                                        *target_irvalue, need_box.value(), *target_irvalue);
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
                                                    m_ircontext.c().boxdyn(v, need_box.value(), v);

                                                m_ircontext.c().store(std::get<woort_IRStaticIndex>(target), v);
                                            }
                                            else
                                            {
                                                m_ircontext.c().itor(*target_irvalue, opnum_to_cast);
                                                if (need_box)
                                                    m_ircontext.c().boxdyn(
                                                        *target_irvalue, need_box.value(), *target_irvalue);
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
                                                    m_ircontext.c().boxdyn(v, need_box.value(), v);

                                                m_ircontext.c().store(std::get<woort_IRStaticIndex>(target), v);
                                            }
                                            else
                                            {
                                                m_ircontext.c().caststo(
                                                    *target_irvalue, opnum_to_cast, WOORT_BOX_VALUE_TYPE_REAL);

                                                if (need_box)
                                                    m_ircontext.c().boxdyn(
                                                        *target_irvalue, need_box.value(), *target_irvalue);
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
                                                    m_ircontext.c().boxdyn(v, need_box.value(), v);

                                                m_ircontext.c().store(std::get<woort_IRStaticIndex>(target), v);
                                            }
                                            else
                                            {
                                                m_ircontext.c().nei(
                                                    *target_irvalue, opnum_to_cast, m_ircontext.c().load_imm_int(0));

                                                if (need_box)
                                                    m_ircontext.c().boxdyn(
                                                        *target_irvalue, need_box.value(), *target_irvalue);
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
                                                    m_ircontext.c().boxdyn(v, need_box.value(), v);

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
                                                    m_ircontext.c().boxdyn(v, need_box.value(), v);

                                                m_ircontext.c().store(std::get<woort_IRStaticIndex>(target), v);
                                            }
                                            else
                                            {
                                                m_ircontext.c().caststo(
                                                    *target_irvalue, opnum_to_cast, WOORT_BOX_VALUE_TYPE_BOOL);

                                                if (need_box)
                                                    m_ircontext.c().boxdyn(
                                                        *target_irvalue, need_box.value(), *target_irvalue);
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

                                            if (target_irvalue == nullptr)
                                            {
                                                woort_IRValue* const v = m_ircontext.c().new_value();
                                                m_ircontext.c().itos(v, opnum_to_cast);

                                                if (need_box.has_value())
                                                    m_ircontext.c().boxdyn(v, need_box.value(), v);

                                                m_ircontext.c().store(std::get<woort_IRStaticIndex>(target), v);
                                            }
                                            else
                                            {
                                                m_ircontext.c().itos(*target_irvalue, opnum_to_cast);

                                                if (need_box.has_value())
                                                    m_ircontext.c().boxdyn(
                                                        *target_irvalue, need_box.value(), *target_irvalue);
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

                                            if (target_irvalue == nullptr)
                                            {
                                                woort_IRValue* const v = m_ircontext.c().new_value();
                                                m_ircontext.c().rtos(v, opnum_to_cast);

                                                if (need_box.has_value())
                                                    m_ircontext.c().boxdyn(v, need_box.value(), v);

                                                m_ircontext.c().store(std::get<woort_IRStaticIndex>(target), v);
                                            }
                                            else
                                            {
                                                m_ircontext.c().rtos(*target_irvalue, opnum_to_cast);

                                                if (need_box.has_value())
                                                    m_ircontext.c().boxdyn(
                                                        *target_irvalue, need_box.value(), *target_irvalue);
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

                                            if (target_irvalue == nullptr)
                                            {
                                                woort_IRValue* const v = m_ircontext.c().new_value();
                                                m_ircontext.c().castsfrom(
                                                    v,
                                                    opnum_to_cast,
                                                    convert_lang_base_type_to_woort_type_exclude_compile_type(
                                                        src_determined_type_instance->m_base_type));

                                                if (need_box.has_value())
                                                    m_ircontext.c().boxdyn(v, need_box.value(), v);

                                                m_ircontext.c().store(std::get<woort_IRStaticIndex>(target), v);
                                            }
                                            else
                                            {
                                                m_ircontext.c().castsfrom(
                                                    *target_irvalue,
                                                    opnum_to_cast,
                                                    convert_lang_base_type_to_woort_type_exclude_compile_type(
                                                        src_determined_type_instance->m_base_type));

                                                if (need_box.has_value())
                                                    m_ircontext.c().boxdyn(
                                                        *target_irvalue, need_box.value(), *target_irvalue);
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
                    const auto& target_storage = result.get_assign_target(node->m_LANG_determined_type.value());

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
        const woort_IRValue* left_opnum,
        const woort_IRValue* right_opnum)
    {
        if (!config::ENABLE_RUNTIME_CHECKING_INTEGER_DIVISION)
            // Skip runtime check if disabled.
            return;

        if (right->m_evaled_const_value.has_value())
        {
            int64_t right_value = right->m_evaled_const_value.value().value_integer();
            wo_assert(right_value != 0);
            wo_assert(!left.has_value() || !left.value()->m_evaled_const_value.has_value());

            if (right_value == -1)
                // Need check l
                bgc.c().chkdivil(left_opnum);

            // Otherwise, no need to check.
        }
        else if (left.has_value() && left.value()->m_evaled_const_value.has_value())
        {
            int64_t left_value = left.value()->m_evaled_const_value.value().value_integer();
            if (left_value == INT64_MIN)
                // Need check r
                bgc.c().chkdivir(right_opnum);
            else
                // Need check rz
                bgc.c().chkdivir(right_opnum);
        }
        else
        {
            // Left & right both not constant.
            bgc.c().chkdivilr(left_opnum, right_opnum);
        }
    }

    WO_PASS_PROCESSER(AstFakeValueUnpack)
    {
        if (state == UNPROCESSED)
        {
            if (node->m_LANG_unpack_method == AstFakeValueUnpack::SHOULD_NOT_UNPACK)
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

                    auto* const unpacking_tuple_determined_type =
                        node->m_LANG_determined_type.value()->get_determined_type().value();

                    if (node->m_LANG_unpack_method == AstFakeValueUnpack::UNPACK_FOR_TUPLE)
                    {
                        wo_assert(unpacking_tuple_determined_type->m_base_type == lang_TypeInstance::DeterminedType::TUPLE);
                        auto* tuple_info = unpacking_tuple_determined_type->m_external_type_description.m_tuple;
                        const uint32_t tuple_elem_count = (uint32_t)tuple_info->m_element_types.size();

                        for (uint32_t i = 0; i < tuple_elem_count; ++i)
                            m_ircontext.c().pushidstruct(unpacking_opnum, i);
                    }
                    else
                    {
                        const auto& unpack_requirement = node->m_LANG_need_to_be_unpack_count.value();
                        if (unpacking_tuple_determined_type->m_base_type == lang_TypeInstance::DeterminedType::TUPLE)
                        {
                            auto* tuple_info = unpacking_tuple_determined_type->m_external_type_description.m_tuple;
                            const uint32_t tuple_elem_count = (uint32_t)tuple_info->m_element_types.size();

                            if (unpack_requirement.m_unpack_all)
                            {
                                for (uint32_t i = tuple_elem_count; i > 0; --i)
                                {
                                    woort_BoxValueType box_type;

                                    if (i - 1 < unpack_requirement.m_require_unpack_count
                                        || !tuple_info->m_element_types[i - 1]->is_need_to_box_in_IR(&box_type))
                                        m_ircontext.c().pushidstruct(unpacking_opnum, i - 1);
                                    else
                                    {
                                        switch (box_type)
                                        {
                                        case WOORT_BOX_VALUE_TYPE_INT:
                                            m_ircontext.c().pushidstboxi(unpacking_opnum, i - 1);
                                            break;
                                        case WOORT_BOX_VALUE_TYPE_REAL:
                                            m_ircontext.c().pushidstboxr(unpacking_opnum, i - 1);
                                            break;
                                        case WOORT_BOX_VALUE_TYPE_BOOL:
                                            m_ircontext.c().pushidstboxb(unpacking_opnum, i - 1);
                                            break;
                                        default:
                                            /* Unexpected */
                                            wo_error("Unknown box_type.");
                                        }
                                    }
                                }
                            }
                            else
                            {
                                // If require to unpack 0 argument, just skip & ignore.
                                if (unpack_requirement.m_require_unpack_count != 0)
                                {
                                    for (uint32_t i = (uint32_t)unpack_requirement.m_require_unpack_count; i > 0; --i)
                                        m_ircontext.c().pushidstruct(unpacking_opnum, i - 1);
                                }
                            }
                        }
                        else
                        {
                            wo_assert(unpack_requirement.m_require_unpack_count <= UINT8_MAX);

                            woort_BoxValueType _useless_type;
                            const bool elem_need_unbox = unpacking_tuple_determined_type
                                ->m_external_type_description.m_array_or_vector
                                ->m_element_type
                                ->is_need_to_box_in_IR(&_useless_type);

                            (void)_useless_type;

                            if (unpack_requirement.m_unpack_all)
                            {
                                woort_IRValue* const v = node->m_IR_unpack_all_counter_MUST_BE_CLEAR_FOR_REPL.value();
                                if (elem_need_unbox)
                                    m_ircontext.c().unpackvecall(
                                        v,
                                        (uint8_t)unpack_requirement.m_require_unpack_count,
                                        unpacking_opnum);
                                else
                                    m_ircontext.c().unpackvecxall(
                                        v,
                                        (uint8_t)unpack_requirement.m_require_unpack_count,
                                        unpacking_opnum);
                            }
                            else
                            {
                                // If require to unpack 0 argument, just skip & ignore.
                                if (unpack_requirement.m_require_unpack_count != 0)
                                {
                                    if (elem_need_unbox)
                                        m_ircontext.c().unpackvec(
                                            (uint8_t)unpack_requirement.m_require_unpack_count,
                                            unpacking_opnum);
                                    else
                                        m_ircontext.c().unpackvecx(
                                            (uint8_t)unpack_requirement.m_require_unpack_count,
                                            unpacking_opnum);
                                }
                            }
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
            // Clear stale IR state from prior compilation (REPL re-compile).
            node->m_IR_value_to_store_shortcut_result_MUST_BE_CLEAR_FOR_REPL.reset();

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
                node->m_IR_value_to_store_shortcut_result_MUST_BE_CLEAR_FOR_REPL.emplace(v);

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
                    node->m_IR_value_to_store_shortcut_result_MUST_BE_CLEAR_FOR_REPL.value(),
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
                            node->m_IR_value_to_store_shortcut_result_MUST_BE_CLEAR_FOR_REPL.value();

                        m_ircontext.pop_eval_result();

                        const auto& target_storage = result.get_assign_target(node->m_LANG_determined_type.value());
                        if (target_storage.has_value())
                        {
                            auto& [need_box, target] = target_storage.value();
                            woort_IRValue* const* const target_irvalue =
                                std::get_if<woort_IRValue*>(&target);

                            if (target_irvalue == nullptr)
                            {
                                if (need_box.has_value())
                                    m_ircontext.c().boxdyn(
                                        shortcut_evaled_value,
                                        need_box.value(),
                                        shortcut_evaled_value);

                                m_ircontext.c().store(
                                    std::get<woort_IRStaticIndex>(target),
                                    shortcut_evaled_value);
                            }
                            else
                            {
                                if (need_box.has_value())
                                    m_ircontext.c().boxdyn(
                                        *target_irvalue,
                                        need_box.value(),
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

                        if (binary_op == &IRCompiler::divi || binary_op == &IRCompiler::modi)
                        {
                            check_and_generate_check_ir_for_divi_and_modi(
                                m_ircontext, node->m_left, node->m_right, left_opnum, right_opnum);
                        }

                        const auto& target_storage = result.get_assign_target(node->m_LANG_determined_type.value());
                        if (target_storage.has_value())
                        {
                            auto& [need_box, target] = target_storage.value();
                            woort_IRValue* const* const target_irvalue =
                                std::get_if<woort_IRValue*>(&target);

                            if (target_irvalue == nullptr)
                            {
                                woort_IRValue* const v = m_ircontext.c().new_value();
                                (m_ircontext.c().*binary_op)(v, left_opnum, right_opnum);

                                if (need_box.has_value())
                                    m_ircontext.c().boxdyn(v, need_box.value(), v);

                                m_ircontext.c().store(std::get<woort_IRStaticIndex>(target), v);
                            }
                            else
                            {
                                (m_ircontext.c().*binary_op)(*target_irvalue, left_opnum, right_opnum);
                                if (need_box.has_value())
                                    m_ircontext.c().boxdyn(
                                        *target_irvalue, need_box.value(), *target_irvalue);
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
        if (state == UNPROCESSED)
        {
            m_ircontext.do_eval_if_not_ignore(
                &BytecodeGenerateContext::begin_eval_readonly);
            WO_CONTINUE_PROCESS(node->m_operand);
            return HOLD;
        }
        else if (state == HOLD)
        {
            m_ircontext.apply_eval_result(
                [&](BytecodeGenerateContext::EvalResult& result)
                {
                    const auto& target_storage = result.get_assign_target(node->m_LANG_determined_type.value());
                    auto* opnum_to_unary = m_ircontext.get_eval_result();

                    switch (node->m_operator)
                    {
                    case AstValueUnaryOperator::NEGATIVE:
                    {
                        lang_TypeInstance* operand_type_instance =
                            node->m_operand->m_LANG_determined_type.value();
                        auto operand_determined_type =
                            operand_type_instance->get_determined_type().value();

                        if (target_storage.has_value())
                        {
                            auto& [need_box, target] = target_storage.value();
                            woort_IRValue* const* const target_irvalue =
                                std::get_if<woort_IRValue*>(&target);

                            if (target_irvalue == nullptr)
                            {
                                woort_IRValue* const v = m_ircontext.c().new_value();

                                switch (operand_determined_type->m_base_type)
                                {
                                case lang_TypeInstance::DeterminedType::INTEGER:
                                {
                                    m_ircontext.c().negi(v, opnum_to_unary);
                                    break;
                                }
                                case lang_TypeInstance::DeterminedType::REAL:
                                {
                                    m_ircontext.c().negr(v, opnum_to_unary);
                                    break;
                                }
                                default:
                                    wo_error("Unknown type.");
                                    break;
                                }

                                if (need_box.has_value())
                                    m_ircontext.c().boxdyn(
                                        v, need_box.value(), v);

                                m_ircontext.c().store(std::get<woort_IRStaticIndex>(target), v);
                            }
                            else
                            {
                                switch (operand_determined_type->m_base_type)
                                {
                                case lang_TypeInstance::DeterminedType::INTEGER:
                                {
                                    m_ircontext.c().negi(*target_irvalue, opnum_to_unary);
                                    break;
                                }
                                case lang_TypeInstance::DeterminedType::REAL:
                                {
                                    m_ircontext.c().negr(*target_irvalue, opnum_to_unary);
                                    break;
                                }
                                default:
                                    wo_error("Unknown type.");
                                    break;
                                }

                                if (need_box.has_value())
                                    m_ircontext.c().boxdyn(
                                        *target_irvalue,
                                        need_box.value(),
                                        *target_irvalue);
                            }
                        }
                        else
                        {
                            woort_IRValue* const v = m_ircontext.c().new_value();

                            switch (operand_determined_type->m_base_type)
                            {
                            case lang_TypeInstance::DeterminedType::INTEGER:
                            {
                                m_ircontext.c().negi(v, opnum_to_unary);
                                break;
                            }
                            case lang_TypeInstance::DeterminedType::REAL:
                            {
                                m_ircontext.c().negr(v, opnum_to_unary);
                                break;
                            }
                            default:
                                wo_error("Unknown type.");
                                break;
                            }
                            result.set_result_stack_temp(
                                m_ircontext, v, node->m_LANG_determined_type.value());
                        }
                        break;
                    }
                    case AstValueUnaryOperator::LOGICAL_NOT:
                    {
                        if (target_storage.has_value())
                        {
                            auto& [need_box, target] = target_storage.value();
                            woort_IRValue* const* const target_irvalue =
                                std::get_if<woort_IRValue*>(&target);

                            if (target_irvalue == nullptr)
                            {
                                woort_IRValue* const v = m_ircontext.c().new_value();

                                m_ircontext.c().lnot(v, opnum_to_unary);
                                if (need_box.has_value())
                                    m_ircontext.c().boxdyn(
                                        v, need_box.value(), v);

                                m_ircontext.c().store(std::get<woort_IRStaticIndex>(target), v);
                            }
                            else
                            {
                                m_ircontext.c().lnot(*target_irvalue, opnum_to_unary);
                                if (need_box.has_value())
                                    m_ircontext.c().boxdyn(
                                        *target_irvalue,
                                        need_box.value(),
                                        *target_irvalue);
                            }
                        }
                        else
                        {
                            woort_IRValue* const v = m_ircontext.c().new_value();

                            m_ircontext.c().lnot(v, opnum_to_unary);

                            result.set_result_stack_temp(
                                m_ircontext, v, node->m_LANG_determined_type.value());
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
            // Clear stale IR state from prior compilation (REPL re-compile).
            node->m_IR_cond_eval_result_MUST_BE_CLEAR_FOR_REPL.reset();

            if (node->m_condition->m_evaled_const_value.has_value())
            {
                m_ircontext.eval_for_upper();

                if (node->m_condition->m_evaled_const_value.value().value_bool() != 0)
                    WO_CONTINUE_PROCESS(node->m_true_value);
                else
                    WO_CONTINUE_PROCESS(node->m_false_value);

                node->m_LANG_hold_state = AstValueTribleOperator::IR_HOLD_FOR_BRANCH_CONST_EVAL;
            }
            else
            {
                auto* binop = try_get_optimizable_comparison_binop(node->m_condition);
                if (binop)
                {
                    m_ircontext.begin_eval_readonly();
                    if (!pass_final_value(lex, binop->m_left))
                        return FAILED;
                    auto* left_val = m_ircontext.get_eval_result();

                    m_ircontext.begin_eval_readonly();
                    if (!pass_final_value(lex, binop->m_right))
                        return FAILED;
                    auto* right_val = m_ircontext.get_eval_result();

                    emit_optimized_jcc_negated(m_ircontext.c(), binop, left_val, right_val,
                        m_ircontext.c().named_label(node, "#cond_false"));

                    if (m_ircontext.is_eval_result_just_ignored())
                    {
                        m_ircontext.eval_and_ignore();
                    }
                    else if (m_ircontext.upper_need_get_result()
                        && !m_ircontext.upper_need_assign())
                    {
                        woort_IRValue* const v = m_ircontext.c().new_value();
                        node->m_IR_cond_eval_result_MUST_BE_CLEAR_FOR_REPL.emplace(v);

                        if (m_ircontext.upper_need_box())
                            m_ircontext.eval_to_assign_box(v, std::nullopt);
                        else
                            m_ircontext.eval_to_assign(v, std::nullopt);
                    }
                    else
                        m_ircontext.eval_for_upper();

                    WO_CONTINUE_PROCESS(node->m_true_value);

                    node->m_LANG_hold_state = AstValueTribleOperator::IR_HOLD_FOR_BRANCH_A_EVAL;
                }
                else
                {
                    m_ircontext.begin_eval_readonly();
                    WO_CONTINUE_PROCESS(node->m_condition);

                    node->m_LANG_hold_state = AstValueTribleOperator::IR_HOLD_FOR_COND_EVAL;
                }
            }
            return HOLD;
        }
        else if (state == HOLD)
        {
            switch (node->m_LANG_hold_state)
            {
            case AstValueTribleOperator::IR_HOLD_FOR_COND_EVAL:
            {
                const woort_IRValue* const cond = m_ircontext.get_eval_result();
                m_ircontext.c().jccz(cond, m_ircontext.c().named_label(node, "#cond_false"));

                if (m_ircontext.is_eval_result_just_ignored())
                {
                    m_ircontext.eval_and_ignore();
                }
                else if (m_ircontext.upper_need_get_result()
                    && !m_ircontext.upper_need_assign())
                {
                    woort_IRValue* const v = m_ircontext.c().new_value();
                    node->m_IR_cond_eval_result_MUST_BE_CLEAR_FOR_REPL.emplace(v);

                    // Eval for box if need box.
                    if (m_ircontext.upper_need_box())
                        m_ircontext.eval_to_assign_box(v, std::nullopt);
                    else
                        m_ircontext.eval_to_assign(v, std::nullopt);
                }
                else
                    m_ircontext.eval_for_upper();

                WO_CONTINUE_PROCESS(node->m_true_value);

                node->m_LANG_hold_state = AstValueTribleOperator::IR_HOLD_FOR_BRANCH_A_EVAL;
                return HOLD;
            }
            case AstValueTribleOperator::IR_HOLD_FOR_BRANCH_A_EVAL:
            {
                m_ircontext.c().jmp(m_ircontext.c().named_label(node, "#cond_end"));
                m_ircontext.c().bind(m_ircontext.c().named_label(node, "#cond_false"));

                if (node->m_IR_cond_eval_result_MUST_BE_CLEAR_FOR_REPL.has_value())
                {
                    m_ircontext.pop_eval_result();

                    woort_IRValue* const v = node->m_IR_cond_eval_result_MUST_BE_CLEAR_FOR_REPL.value();

                    // Eval for box if need box.
                    if (m_ircontext.upper_need_box())
                        m_ircontext.eval_to_assign_box(v, std::nullopt);
                    else
                        m_ircontext.eval_to_assign(v, std::nullopt);
                }
                else if (m_ircontext.is_eval_result_just_ignored())
                {
                    m_ircontext.eval_and_ignore();
                }
                else
                {
                    if (m_ircontext.upper_need_assign())
                        m_ircontext.pop_eval_result();

                    m_ircontext.eval_for_upper();
                }

                WO_CONTINUE_PROCESS(node->m_false_value);

                node->m_LANG_hold_state = AstValueTribleOperator::IR_HOLD_FOR_BRANCH_B_EVAL;
                return HOLD;
            }
            case AstValueTribleOperator::IR_HOLD_FOR_BRANCH_B_EVAL:
            {
                m_ircontext.c().bind(m_ircontext.c().named_label(node, "#cond_end"));

                if (node->m_IR_cond_eval_result_MUST_BE_CLEAR_FOR_REPL.has_value()
                    || m_ircontext.is_eval_result_just_ignored())
                {
                    m_ircontext.apply_eval_result(
                        [&](BytecodeGenerateContext::EvalResult& result)
                        {
                            wo_assert(!result.get_assign_target(node->m_LANG_determined_type.value()).has_value());
                            m_ircontext.pop_eval_result();

                            result.set_result_stack_temp(
                                m_ircontext, node->m_IR_cond_eval_result_MUST_BE_CLEAR_FOR_REPL.value(),
                                /* Assure no box, it has been boxed in branch eval. */
                                m_origin_types.m_dynamic.m_type_instance);
                        }
                    );
                }
                else
                {
                    m_ircontext.cleanup_for_eval_upper();
                }
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
            if (node->m_LANG_overload_call.has_value())
            {
                m_ircontext.eval_for_upper();
                WO_CONTINUE_PROCESS(node->m_LANG_overload_call.value());

                node->m_LANG_hold_state = AstValueIndex::IR_HOLD_FOR_OVERLOAD_CALL_EVAL;
            }
            else if (!node->m_LANG_fast_index_for_struct.has_value())
            {
                m_ircontext.do_eval_if_not_ignore(
                    &BytecodeGenerateContext::begin_eval_readonly);
                WO_CONTINUE_PROCESS(node->m_index);

                m_ircontext.do_eval_if_not_ignore(
                    &BytecodeGenerateContext::begin_eval_readonly);
                WO_CONTINUE_PROCESS(node->m_container);

                node->m_LANG_hold_state = AstValueIndex::IR_HOLD_FOR_NORMAL_LR_EVAL;
            }
            else
            {
                m_ircontext.do_eval_if_not_ignore(
                    &BytecodeGenerateContext::begin_eval_readonly);
                WO_CONTINUE_PROCESS(node->m_container);

                node->m_LANG_hold_state = AstValueIndex::IR_HOLD_FOR_NORMAL_LR_EVAL;
            }

            return HOLD;
        }
        else if (state == HOLD)
        {
            switch (node->m_LANG_hold_state)
            {
            case AstValueIndex::IR_HOLD_FOR_OVERLOAD_CALL_EVAL:
                wo_assert(node->m_LANG_overload_call.has_value());
                m_ircontext.cleanup_for_eval_upper();
                break;
            case AstValueIndex::IR_HOLD_FOR_NORMAL_LR_EVAL:
                wo_assert(!node->m_LANG_overload_call.has_value());

                m_ircontext.apply_eval_result(
                    [&](BytecodeGenerateContext::EvalResult& result)
                    {
                        lang_TypeInstance* const result_type_instance =
                            node->m_LANG_determined_type.value();

                        const bool is_void = result_type_instance->is_based_on_void_in_IR();

                        const auto& asigned_target =
                            result.get_assign_target(result_type_instance);

                        lang_TypeInstance* const container_type_instance =
                            node->m_container->m_LANG_determined_type.value();
                        auto* const determined_container_type =
                            container_type_instance->get_determined_type().value();

                        if (determined_container_type->m_base_type == lang_TypeInstance::DeterminedType::STRUCT
                            || determined_container_type->m_base_type == lang_TypeInstance::DeterminedType::TUPLE)
                        {
                            auto* container_opnum = m_ircontext.get_eval_result();

                            wo_assert(node->m_LANG_fast_index_for_struct.has_value());

                            const uint32_t fast_index = (uint32_t)node->m_LANG_fast_index_for_struct.value();

                            if (asigned_target.has_value())
                            {
                                if (is_void)
                                    ;
                                else
                                {
                                    const auto& [need_box, target] = asigned_target.value();
                                    woort_IRValue* const* const target_irvalue =
                                        std::get_if<woort_IRValue*>(&target);

                                    woort_IRValue* const v = target_irvalue
                                        ? *target_irvalue
                                        : m_ircontext.c().new_value();

                                    m_ircontext.c().ldidstruct(v, container_opnum, fast_index);
                                    if (need_box.has_value())
                                        m_ircontext.c().boxdyn(v, need_box.value(), v);

                                    if (target_irvalue == nullptr)
                                        m_ircontext.c().store(std::get<woort_IRStaticIndex>(target), v);
                                }
                            }
                            else if (!is_void)
                            {
                                result.set_result_struct_index(
                                    m_ircontext, container_opnum, fast_index, node->m_LANG_determined_type.value());
                            }
                            else
                            {
                                result.set_result_junk(m_ircontext);
                            }
                        }
                        else if (!is_void)
                        {
                            woort_IRValue* make_result_target = nullptr;

                            std::optional<woort_IRStaticIndex> need_write_back_to_static =
                                std::nullopt;

                            std::optional<woort_BoxValueType> need_box;

                            if (asigned_target.has_value())
                            {
                                const auto& [nb, target] = asigned_target.value();
                                need_box = nb;

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
                                make_result_target = m_ircontext.c().new_value();

                            auto* const determined_result_type =
                                result_type_instance->get_determined_type().value();

                            bool use_x_variant = determined_result_type->m_base_type ==
                                lang_TypeInstance::DeterminedType::base_type::DYNAMIC;

                            bool need_extra_box_for_unsafe_cast_optimize = false;
                            if (need_box.has_value())
                            {
                                woort_BoxValueType box_type;
                                if (result_type_instance->is_need_to_box_in_IR(&box_type))
                                {
                                    if (box_type != need_box.value())
                                        need_extra_box_for_unsafe_cast_optimize = true;
                                    else if (!use_x_variant)
                                        use_x_variant = true;
                                }
                            }

                            switch (determined_container_type->m_base_type)
                            {
                            case lang_TypeInstance::DeterminedType::ARRAY:
                            case lang_TypeInstance::DeterminedType::VECTOR:
                            {
                                auto* index_opnum = m_ircontext.get_eval_result();
                                auto* container_opnum = m_ircontext.get_eval_result();

                                if (use_x_variant)
                                    m_ircontext.c().ldidvecx(
                                        make_result_target, container_opnum, index_opnum);
                                else
                                    m_ircontext.c().ldidvec(
                                        make_result_target, container_opnum, index_opnum);

                                break;
                            }
                            case lang_TypeInstance::DeterminedType::DICTIONARY:
                            case lang_TypeInstance::DeterminedType::MAPPING:
                            {
                                auto* index_opnum = m_ircontext.get_eval_result();
                                auto* container_opnum = m_ircontext.get_eval_result();

                                auto* key_type_instance =
                                    determined_container_type->m_external_type_description.m_dictionary_or_mapping->m_key_type;
                                auto* key_determined_type =
                                    key_type_instance->get_determined_type().value();

                                if (use_x_variant)
                                {
                                    switch (key_determined_type->m_base_type)
                                    {
                                    case lang_TypeInstance::DeterminedType::INTEGER:
                                        m_ircontext.c().ldiddictix(make_result_target, container_opnum, index_opnum);
                                        break;
                                    case lang_TypeInstance::DeterminedType::REAL:
                                        m_ircontext.c().ldiddictrx(make_result_target, container_opnum, index_opnum);
                                        break;
                                    case lang_TypeInstance::DeterminedType::BOOLEAN:
                                        m_ircontext.c().ldiddictbx(make_result_target, container_opnum, index_opnum);
                                        break;
                                    default:
                                        m_ircontext.c().ldiddictxx(make_result_target, container_opnum, index_opnum);
                                        break;
                                    }
                                }
                                else
                                {
                                    switch (key_determined_type->m_base_type)
                                    {
                                    case lang_TypeInstance::DeterminedType::INTEGER:
                                        m_ircontext.c().ldiddicti(make_result_target, container_opnum, index_opnum);
                                        break;
                                    case lang_TypeInstance::DeterminedType::REAL:
                                        m_ircontext.c().ldiddictr(make_result_target, container_opnum, index_opnum);
                                        break;
                                    case lang_TypeInstance::DeterminedType::BOOLEAN:
                                        m_ircontext.c().ldiddictb(make_result_target, container_opnum, index_opnum);
                                        break;
                                    default:
                                        m_ircontext.c().ldiddictx(make_result_target, container_opnum, index_opnum);
                                        break;
                                    }
                                }

                                if (need_extra_box_for_unsafe_cast_optimize)
                                    m_ircontext.c().boxdyn(
                                        make_result_target, need_box.value(), make_result_target);

                                break;
                            }
                            case lang_TypeInstance::DeterminedType::STRING:
                            {
                                auto* index_opnum = m_ircontext.get_eval_result();
                                auto* container_opnum = m_ircontext.get_eval_result();

                                m_ircontext.c().ldidstring(make_result_target, container_opnum, index_opnum);
                                if (need_box.has_value())
                                    m_ircontext.c().boxdyn(make_result_target, need_box.value(), make_result_target);
                                break;
                            }
                            case lang_TypeInstance::DeterminedType::STRUCT:
                            case lang_TypeInstance::DeterminedType::TUPLE:
                            default:
                                wo_error("Unknown type.");
                            }

                            if (asigned_target.has_value())
                            {
                                if (need_extra_box_for_unsafe_cast_optimize)
                                    m_ircontext.c().boxdyn(
                                        make_result_target, need_box.value(), make_result_target);

                                if (need_write_back_to_static.has_value())
                                {
                                    m_ircontext.c().store(need_write_back_to_static.value(), make_result_target);
                                }
                            }
                            else
                            {
                                result.set_result_stack_temp(
                                    m_ircontext,
                                    make_result_target,
                                    node->m_LANG_determined_type.value());
                            }
                        }
                        else if (!asigned_target.has_value())
                        {
                            result.set_result_junk(m_ircontext);
                        }
                        /////////////////////////////////////////////////////////////////////////
                    }
                );
                break;
            default:
                wo_error("Unknown type.");
            }

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
                m_ircontext.do_eval_if_not_ignore(
                    &BytecodeGenerateContext::eval_to_push);
                WO_CONTINUE_PROCESS(field_value->m_value);
            }

            return HOLD;
        }
        else if (state == HOLD)
        {
            m_ircontext.apply_eval_result(
                [&](BytecodeGenerateContext::EvalResult& result)
                {
                    const auto& target_storage = result.get_assign_target(node->m_LANG_determined_type.value());
                    if (target_storage.has_value())
                    {
                        auto& [need_box, target] = target_storage.value();
                        woort_IRValue* const* const target_irvalue =
                            std::get_if<woort_IRValue*>(&target);

                        if (target_irvalue == nullptr)
                        {
                            woort_IRValue* const v = m_ircontext.c().new_value();
                            m_ircontext.c().mkstruct(
                                v,
                                (uint32_t)node->m_fields.size());

                            if (need_box.has_value())
                                m_ircontext.c().boxdyn(v, need_box.value(), v);

                            m_ircontext.c().store(std::get<woort_IRStaticIndex>(target), v);
                        }
                        else
                        {
                            m_ircontext.c().mkstruct(
                                *target_irvalue,
                                (uint32_t)node->m_fields.size());

                            if (need_box.has_value())
                                m_ircontext.c().boxdyn(
                                    *target_irvalue, need_box.value(), *target_irvalue);
                        }
                    }
                    else
                    {
                        woort_IRValue* const v = m_ircontext.c().new_value();
                        m_ircontext.c().mkstruct(
                            v,
                            (uint32_t)node->m_fields.size());
                        result.set_result_stack_temp(m_ircontext, v, node->m_LANG_determined_type.value());
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
                m_ircontext.do_eval_if_not_ignore(
                    &BytecodeGenerateContext::begin_eval_readonly);

                WO_CONTINUE_PROCESS(node->m_packed_value.value());
            }
            return HOLD;
        }
        else
        {
            m_ircontext.apply_eval_result(
                [&](BytecodeGenerateContext::EvalResult& result)
                {
                    auto* const packed_opnum =
                        node->m_packed_value.has_value()
                        ? m_ircontext.get_eval_result()
                        : m_ircontext.c().load_imm_nil();

                    const auto& target_storage = result.get_assign_target(node->m_LANG_determined_type.value());
                    if (target_storage.has_value())
                    {
                        auto& [need_box, target] = target_storage.value();
                        woort_IRValue* const* const target_irvalue =
                            std::get_if<woort_IRValue*>(&target);

                        if (target_irvalue == nullptr)
                        {
                            woort_IRValue* const v = m_ircontext.c().new_value();

                            m_ircontext.c().mkunion(
                                v,
                                packed_opnum,
                                (uint32_t)node->m_index);

                            if (need_box.has_value())
                                m_ircontext.c().boxdyn(v, need_box.value(), v);

                            m_ircontext.c().store(std::get<woort_IRStaticIndex>(target), v);
                        }
                        else
                        {
                            m_ircontext.c().mkunion(
                                *target_irvalue,
                                packed_opnum,
                                (uint32_t)node->m_index);

                            if (need_box.has_value())
                                m_ircontext.c().boxdyn(
                                    *target_irvalue, need_box.value(), *target_irvalue);
                        }
                    }
                    else
                    {
                        woort_IRValue* const v = m_ircontext.c().new_value();

                        m_ircontext.c().mkunion(
                            v,
                            packed_opnum,
                            (uint32_t)node->m_index);
                        result.set_result_stack_temp(
                            m_ircontext, v, node->m_LANG_determined_type.value());
                    }
                }
            );
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

                const auto& target_storage = result.get_assign_target(node->m_LANG_determined_type.value());
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

                        if (need_box.has_value())
                            m_ircontext.c().boxdyn(v, need_box.value(), v);

                        m_ircontext.c().store(std::get<woort_IRStaticIndex>(target), v);
                    }
                    else
                    {
                        m_ircontext.c().packarg(
                            *target_irvalue,
                            (uint16_t)current_func->m_parameters.size());

                        if (need_box.has_value())
                            m_ircontext.c().boxdyn(
                                *target_irvalue, need_box.value(), *target_irvalue);
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
        wo_assert(state == UNPROCESSED);
        m_ircontext.apply_eval_result(
            [&](BytecodeGenerateContext::EvalResult& result)
            {
                const auto& target_storage = result.get_assign_target(node->m_LANG_determined_type.value());
                if (target_storage.has_value())
                {
                    auto& [need_box, target] = target_storage.value();
                    woort_IRValue* const* const target_irvalue =
                        std::get_if<woort_IRValue*>(&target);

                    if (target_irvalue == nullptr)
                    {
                        woort_IRValue* const v = m_ircontext.c().new_value();

                        m_ircontext.c().mov(
                            v,
                            node->m_opnum);

                        if (need_box.has_value())
                            m_ircontext.c().boxdyn(v, need_box.value(), v);

                        m_ircontext.c().store(std::get<woort_IRStaticIndex>(target), v);
                    }
                    else
                    {
                        m_ircontext.c().mov(
                            *target_irvalue, node->m_opnum);

                        if (need_box.has_value())
                            m_ircontext.c().boxdyn(
                                *target_irvalue, need_box.value(), *target_irvalue);
                    }
                }
                else
                {
                    result.set_result_stack_temp(
                        m_ircontext, node->m_opnum, node->m_LANG_determined_type.value());
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
                        WO_ERR_VARIABLE_STORAGE_NOT_DETERMINED,
                        get_value_name(assign_value_instance));

                    if (assign_value_instance->m_symbol->m_symbol_declare_ast.has_value())
                        lex.record_lang_error(lexer::msglevel_t::infom,
                            assign_value_instance->m_symbol->m_symbol_declare_ast.value(),
                            WO_INFO_SYMBOL_NAMED_DEFINED_HERE,
                            get_value_name(assign_value_instance));

                    return FAILED;
                }

                auto& storage = assign_value_instance->m_IR_storage.value();

                switch (node->m_assign_type)
                {
                case AstValueAssign::ASSIGN:
                    if (storage.m_type == lang_ValueInstance::Storage::StorageType::STACKOFFSET)
                        m_ircontext.eval_to_assign(storage.m_stack_slot, node);
                    else if (storage.m_is_pvalue_indirect)
                    {
                        woort_IRValue* const pvalue_assign_temp = m_ircontext.c().new_value();
                        m_ircontext.eval_to_assign(pvalue_assign_temp, node);
                    }
                    else
                        m_ircontext.eval_to_assign_static(storage.m_static_index, node);

                    WO_CONTINUE_PROCESS(node->m_right);
                    break;
                default:
                    if (node->m_LANG_overload_call.has_value())
                    {
                        // WTF! WTF! WTF!
                        // IT'S FXXKING FUNCTION OVERLOAD CALL!

                        // We just eval it and assigned it to value.
                        if (storage.m_type == lang_ValueInstance::Storage::StorageType::STACKOFFSET)
                            m_ircontext.eval_to_assign(storage.m_stack_slot, node);
                        else if (storage.m_is_pvalue_indirect)
                        {
                            woort_IRValue* const pvalue_assign_temp = m_ircontext.c().new_value();
                            m_ircontext.eval_to_assign(pvalue_assign_temp, node);
                        }
                        else
                            m_ircontext.eval_to_assign_static(storage.m_static_index, node);

                        WO_CONTINUE_PROCESS(node->m_LANG_overload_call.value());
                    }
                    else
                    {
                        m_ircontext.begin_eval_readonly();
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
                    m_ircontext.begin_eval_readonly();
                    WO_CONTINUE_PROCESS(node->m_right);

                    node->m_LANG_hold_state = AstValueAssign::IR_HOLD_TO_APPLY_ASSIGN;

                    /* FALL THROUGH */
                    [[fallthrough]];
                default:
                    if (!pattern_index->m_index->m_LANG_fast_index_for_struct.has_value())
                    {
                        m_ircontext.begin_eval_readonly();
                        WO_CONTINUE_PROCESS(pattern_index->m_index->m_index);
                    }

                    m_ircontext.begin_eval_readonly();
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
                    AstValueIROpnum* const index_opnum = new AstValueIROpnum(m_ircontext.get_eval_result());
                    index_opnum->m_LANG_determined_type = pattern_index->m_index->m_index->m_LANG_determined_type;
                    index_opnum->source_location = pattern_index->m_index->m_index->source_location;
                    pattern_index->m_index->m_index = index_opnum;
                }

                AstValueIROpnum* const container_opnum = new AstValueIROpnum(m_ircontext.get_eval_result());
                container_opnum->m_LANG_determined_type = pattern_index->m_index->m_container->m_LANG_determined_type;
                container_opnum->source_location = pattern_index->m_index->m_container->source_location;
                pattern_index->m_index->m_container = container_opnum;

                if (node->m_LANG_overload_call.has_value())
                {
                    m_ircontext.begin_eval_readonly();
                    WO_CONTINUE_PROCESS(node->m_LANG_overload_call.value());
                }
                else
                {
                    m_ircontext.begin_eval_readonly();
                    WO_CONTINUE_PROCESS(node->m_right);

                    m_ircontext.begin_eval_readonly();
                    WO_CONTINUE_PROCESS(pattern_index->m_index);
                }

                node->m_LANG_hold_state = AstValueAssign::IR_HOLD_TO_APPLY_ASSIGN;
                return HOLD;
            }
            case AstValueAssign::IR_HOLD_TO_APPLY_ASSIGN:
            {
                const woort_IRValue* eval_result_for_upper = nullptr;

                if (node->m_assign_place->node_type == AstBase::AST_PATTERN_VARIABLE)
                {
                    AstPatternVariable* assign_var = static_cast<AstPatternVariable*>(node->m_assign_place);
                    lang_ValueInstance* assign_value_instance = assign_var->m_variable->m_LANG_variable_instance.value();

                    const auto& storage = assign_value_instance->m_IR_storage.value();

                    switch (node->m_assign_type)
                    {
                    case AstValueAssign::ASSIGN:
                        if (storage.m_type == lang_ValueInstance::Storage::StorageType::GLOBAL
                            && storage.m_is_pvalue_indirect)
                        {
                            const woort_IRValue* const rhs_result =
                                m_ircontext.get_eval_result();
                            if (!m_ircontext.is_eval_result_just_ignored())
                                eval_result_for_upper = rhs_result;

                            woort_IRValue* const ptr = m_ircontext.c().new_value();
                            m_ircontext.c().load(ptr, storage.m_static_index);
                            m_ircontext.c().storepvalue(ptr, rhs_result);
                        }
                        else
                        {
                            if (m_ircontext.is_eval_result_just_ignored())
                                m_ircontext.pop_eval_result();
                            else
                                eval_result_for_upper = m_ircontext.get_eval_result();
                        }
                        break;
                    default:
                        if (node->m_LANG_overload_call.has_value())
                        {
                            if (storage.m_type == lang_ValueInstance::Storage::StorageType::GLOBAL
                                && storage.m_is_pvalue_indirect)
                            {
                                const woort_IRValue* const rhs_result =
                                    m_ircontext.get_eval_result();
                                if (!m_ircontext.is_eval_result_just_ignored())
                                    eval_result_for_upper = rhs_result;

                                woort_IRValue* const ptr = m_ircontext.c().new_value();
                                m_ircontext.c().load(ptr, storage.m_static_index);
                                m_ircontext.c().storepvalue(ptr, rhs_result);
                            }
                            else
                            {
                                if (m_ircontext.is_eval_result_just_ignored())
                                    m_ircontext.pop_eval_result();
                                else
                                    eval_result_for_upper = m_ircontext.get_eval_result();
                            }
                        }
                        else
                        {
                            woort_IRValue* assign_expr_result_opnum;
                            auto* right_value_result = m_ircontext.get_eval_result();

                            // Do normal assign operate;
                            woort_IRValue* pvalue_ptr = nullptr;
                            if (storage.m_type == lang_ValueInstance::Storage::StorageType::GLOBAL)
                            {
                                if (storage.m_is_pvalue_indirect)
                                {
                                    pvalue_ptr = m_ircontext.c().new_value();
                                    m_ircontext.c().load(pvalue_ptr, storage.m_static_index);
                                    assign_expr_result_opnum = m_ircontext.c().new_value();
                                    m_ircontext.c().loadpvalue(assign_expr_result_opnum, pvalue_ptr);
                                }
                                else
                                {
                                    assign_expr_result_opnum = m_ircontext.c().new_value();
                                    m_ircontext.c().load(assign_expr_result_opnum, storage.m_static_index);
                                }
                            }
                            else
                            {
                                assign_expr_result_opnum = storage.m_stack_slot;
                            }

                            lang_TypeInstance* assign_type_instance =
                                assign_var->m_variable->m_LANG_determined_type.value();
                            auto* determined_assign_type =
                                assign_type_instance->get_determined_type().value();

                            using binary_op_t = void (IRCompiler::*)(
                                woort_IRValue*, const woort_IRValue*, const woort_IRValue*);

                            binary_op_t binary_op;

                            switch (node->m_assign_type)
                            {
                            case AstValueAssign::ADD_ASSIGN:
                                switch (determined_assign_type->m_base_type)
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
                            case AstValueAssign::SUBSTRACT_ASSIGN:
                                switch (determined_assign_type->m_base_type)
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
                            case AstValueAssign::MULTIPLY_ASSIGN:
                                switch (determined_assign_type->m_base_type)
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
                            case AstValueAssign::DIVIDE_ASSIGN:
                                switch (determined_assign_type->m_base_type)
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
                            case AstValueAssign::MODULO_ASSIGN:
                                switch (determined_assign_type->m_base_type)
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
                            default:
                                wo_error("Unknown operator.");
                                break;
                            }

                            if (binary_op == &IRCompiler::divi || binary_op == &IRCompiler::modi)
                            {
                                check_and_generate_check_ir_for_divi_and_modi(
                                    m_ircontext,
                                    std::nullopt,
                                    node->m_right,
                                    assign_expr_result_opnum,
                                    right_value_result);
                            }
                            (m_ircontext.c().*binary_op)(
                                assign_expr_result_opnum,
                                assign_expr_result_opnum,
                                right_value_result);

                            if (storage.m_type == lang_ValueInstance::Storage::StorageType::GLOBAL)
                            {
                                // Write back.
                                if (pvalue_ptr != nullptr)
                                    m_ircontext.c().storepvalue(pvalue_ptr, assign_expr_result_opnum);
                                else
                                    m_ircontext.c().store(storage.m_static_index, assign_expr_result_opnum);
                            }

                            eval_result_for_upper = assign_expr_result_opnum;
                        }
                        break;
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

                    const woort_IRValue* container_opnum;
                    const woort_IRValue* index_opnum;

                    switch (node->m_assign_type)
                    {
                    case AstValueAssign::ASSIGN:
                    {
                        eval_result_for_upper = m_ircontext.get_eval_result();

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
                            eval_result_for_upper = m_ircontext.get_eval_result();
                        else
                        {
                            auto* const right_value_result = m_ircontext.get_eval_result();
                            const woort_IRValue* const assign_expr_result_opnum = m_ircontext.get_eval_result();

                            using binary_op_t = void (IRCompiler::*)(
                                woort_IRValue*, const woort_IRValue*, const woort_IRValue*);

                            binary_op_t binary_op;

                            switch (node->m_assign_type)
                            {
                            case AstValueAssign::ADD_ASSIGN:
                                switch (determined_assign_type->m_base_type)
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
                            case AstValueAssign::SUBSTRACT_ASSIGN:
                                switch (determined_assign_type->m_base_type)
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
                            case AstValueAssign::MULTIPLY_ASSIGN:
                                switch (determined_assign_type->m_base_type)
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
                            case AstValueAssign::DIVIDE_ASSIGN:
                                switch (determined_assign_type->m_base_type)
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
                            case AstValueAssign::MODULO_ASSIGN:
                                switch (determined_assign_type->m_base_type)
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
                            default:
                                wo_error("Unknown operator.");
                                break;
                            }

                            woort_IRValue* const v = m_ircontext.c().new_value();

                            if (binary_op == &IRCompiler::divi || binary_op == &IRCompiler::modi)
                            {
                                check_and_generate_check_ir_for_divi_and_modi(
                                    m_ircontext,
                                    std::nullopt,
                                    node->m_right,
                                    assign_expr_result_opnum,
                                    right_value_result);
                            }
                            (m_ircontext.c().*binary_op)(
                                v,
                                assign_expr_result_opnum,
                                right_value_result);

                            eval_result_for_upper = v;
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
                        using stdx_operate_t = void (IRCompiler::*)(
                            const woort_IRValue* c, const woort_IRValue* idx, const woort_IRValue* val);

                        stdx_operate_t stdx_operate = nullptr;

                        // NOTE: index_opnum must be valid, check m_LANG_fast_index_for_struct to make sure it.
                        //  if m_LANG_fast_index_for_struct should be nullopt here.
                        wo_assert(!pattern_index->m_index->m_LANG_fast_index_for_struct.has_value());

                        if (determined_container_type->m_base_type == lang_TypeInstance::DeterminedType::DICTIONARY)
                        {
                            switch (
                                /* Result type. */
                                pattern_index->m_index->m_LANG_determined_type.value()->
                                get_determined_type().value()->m_base_type)
                            {
                            case lang_TypeInstance::DeterminedType::base_type::INTEGER:
                            case lang_TypeInstance::DeterminedType::base_type::HANDLE:
                                switch (
                                    /* Index key type. */
                                    pattern_index->m_index->m_index->m_LANG_determined_type.value()->
                                    get_determined_type().value()->m_base_type)
                                {
                                case lang_TypeInstance::DeterminedType::base_type::INTEGER:
                                case lang_TypeInstance::DeterminedType::base_type::HANDLE:
                                    stdx_operate = &IRCompiler::stiddictii;
                                    break;
                                case lang_TypeInstance::DeterminedType::base_type::REAL:
                                    stdx_operate = &IRCompiler::stiddictri;
                                    break;
                                case lang_TypeInstance::DeterminedType::base_type::BOOLEAN:
                                    stdx_operate = &IRCompiler::stiddictbi;
                                    break;
                                default:
                                    stdx_operate = &IRCompiler::stiddictxi;
                                    break;
                                }
                                break;
                            case lang_TypeInstance::DeterminedType::base_type::REAL:
                                switch (
                                    /* Index key type. */
                                    pattern_index->m_index->m_index->m_LANG_determined_type.value()->
                                    get_determined_type().value()->m_base_type)
                                {
                                case lang_TypeInstance::DeterminedType::base_type::INTEGER:
                                case lang_TypeInstance::DeterminedType::base_type::HANDLE:
                                    stdx_operate = &IRCompiler::stiddictir;
                                    break;
                                case lang_TypeInstance::DeterminedType::base_type::REAL:
                                    stdx_operate = &IRCompiler::stiddictrr;
                                    break;
                                case lang_TypeInstance::DeterminedType::base_type::BOOLEAN:
                                    stdx_operate = &IRCompiler::stiddictbr;
                                    break;
                                default:
                                    stdx_operate = &IRCompiler::stiddictxr;
                                    break;
                                }
                                break;
                            case lang_TypeInstance::DeterminedType::base_type::BOOLEAN:
                                switch (
                                    /* Index key type. */
                                    pattern_index->m_index->m_index->m_LANG_determined_type.value()->
                                    get_determined_type().value()->m_base_type)
                                {
                                case lang_TypeInstance::DeterminedType::base_type::INTEGER:
                                case lang_TypeInstance::DeterminedType::base_type::HANDLE:
                                    stdx_operate = &IRCompiler::stiddictib;
                                    break;
                                case lang_TypeInstance::DeterminedType::base_type::REAL:
                                    stdx_operate = &IRCompiler::stiddictrb;
                                    break;
                                case lang_TypeInstance::DeterminedType::base_type::BOOLEAN:
                                    stdx_operate = &IRCompiler::stiddictbb;
                                    break;
                                default:
                                    stdx_operate = &IRCompiler::stiddictxb;
                                    break;
                                }
                                break;
                            default:
                                switch (
                                    /* Index key type. */
                                    pattern_index->m_index->m_index->m_LANG_determined_type.value()->
                                    get_determined_type().value()->m_base_type)
                                {
                                case lang_TypeInstance::DeterminedType::base_type::INTEGER:
                                case lang_TypeInstance::DeterminedType::base_type::HANDLE:
                                    stdx_operate = &IRCompiler::stiddictix;
                                    break;
                                case lang_TypeInstance::DeterminedType::base_type::REAL:
                                    stdx_operate = &IRCompiler::stiddictrx;
                                    break;
                                case lang_TypeInstance::DeterminedType::base_type::BOOLEAN:
                                    stdx_operate = &IRCompiler::stiddictbx;
                                    break;
                                default:
                                    stdx_operate = &IRCompiler::stiddictxx;
                                    break;
                                }
                                break;
                            }
                        }
                        else if (determined_container_type->m_base_type == lang_TypeInstance::DeterminedType::MAPPING)
                        {
                            switch (
                                /* Result type. */
                                pattern_index->m_index->m_LANG_determined_type.value()->
                                get_determined_type().value()->m_base_type)
                            {
                            case lang_TypeInstance::DeterminedType::base_type::INTEGER:
                            case lang_TypeInstance::DeterminedType::base_type::HANDLE:
                                switch (
                                    /* Index key type. */
                                    pattern_index->m_index->m_index->m_LANG_determined_type.value()->
                                    get_determined_type().value()->m_base_type)
                                {
                                case lang_TypeInstance::DeterminedType::base_type::INTEGER:
                                case lang_TypeInstance::DeterminedType::base_type::HANDLE:
                                    stdx_operate = &IRCompiler::stidmapii;
                                    break;
                                case lang_TypeInstance::DeterminedType::base_type::REAL:
                                    stdx_operate = &IRCompiler::stidmapri;
                                    break;
                                case lang_TypeInstance::DeterminedType::base_type::BOOLEAN:
                                    stdx_operate = &IRCompiler::stidmapbi;
                                    break;
                                default:
                                    stdx_operate = &IRCompiler::stidmapxi;
                                    break;
                                }
                                break;
                            case lang_TypeInstance::DeterminedType::base_type::REAL:
                                switch (
                                    /* Index key type. */
                                    pattern_index->m_index->m_index->m_LANG_determined_type.value()->
                                    get_determined_type().value()->m_base_type)
                                {
                                case lang_TypeInstance::DeterminedType::base_type::INTEGER:
                                case lang_TypeInstance::DeterminedType::base_type::HANDLE:
                                    stdx_operate = &IRCompiler::stidmapir;
                                    break;
                                case lang_TypeInstance::DeterminedType::base_type::REAL:
                                    stdx_operate = &IRCompiler::stidmaprr;
                                    break;
                                case lang_TypeInstance::DeterminedType::base_type::BOOLEAN:
                                    stdx_operate = &IRCompiler::stidmapbr;
                                    break;
                                default:
                                    stdx_operate = &IRCompiler::stidmapxr;
                                    break;
                                }
                                break;
                            case lang_TypeInstance::DeterminedType::base_type::BOOLEAN:
                                switch (
                                    /* Index key type. */
                                    pattern_index->m_index->m_index->m_LANG_determined_type.value()->
                                    get_determined_type().value()->m_base_type)
                                {
                                case lang_TypeInstance::DeterminedType::base_type::INTEGER:
                                case lang_TypeInstance::DeterminedType::base_type::HANDLE:
                                    stdx_operate = &IRCompiler::stidmapib;
                                    break;
                                case lang_TypeInstance::DeterminedType::base_type::REAL:
                                    stdx_operate = &IRCompiler::stidmaprb;
                                    break;
                                case lang_TypeInstance::DeterminedType::base_type::BOOLEAN:
                                    stdx_operate = &IRCompiler::stidmapbb;
                                    break;
                                default:
                                    stdx_operate = &IRCompiler::stidmapxb;
                                    break;
                                }
                                break;
                            default:
                                switch (
                                    /* Index key type. */
                                    pattern_index->m_index->m_index->m_LANG_determined_type.value()->
                                    get_determined_type().value()->m_base_type)
                                {
                                case lang_TypeInstance::DeterminedType::base_type::INTEGER:
                                case lang_TypeInstance::DeterminedType::base_type::HANDLE:
                                    stdx_operate = &IRCompiler::stidmapix;
                                    break;
                                case lang_TypeInstance::DeterminedType::base_type::REAL:
                                    stdx_operate = &IRCompiler::stidmaprx;
                                    break;
                                case lang_TypeInstance::DeterminedType::base_type::BOOLEAN:
                                    stdx_operate = &IRCompiler::stidmapbx;
                                    break;
                                default:
                                    stdx_operate = &IRCompiler::stidmapxx;
                                    break;
                                }
                                break;
                            }
                        }
                        else
                        {
                            switch (
                                /* Result type. */
                                pattern_index->m_index->m_LANG_determined_type.value()->
                                get_determined_type().value()->m_base_type)
                            {
                            case lang_TypeInstance::DeterminedType::base_type::INTEGER:
                            case lang_TypeInstance::DeterminedType::base_type::HANDLE:
                                stdx_operate = &IRCompiler::stidveci;
                                break;
                            case lang_TypeInstance::DeterminedType::base_type::REAL:
                                stdx_operate = &IRCompiler::stidvecr;
                                break;
                            case lang_TypeInstance::DeterminedType::base_type::BOOLEAN:
                                stdx_operate = &IRCompiler::stidvecb;
                                break;
                            default:
                                stdx_operate = &IRCompiler::stidvecx;
                                break;
                            }
                        }

                        wo_assert(stdx_operate != nullptr);

                        (m_ircontext.c().*stdx_operate)(
                            container_opnum,
                            index_opnum,
                            eval_result_for_upper);

                        break;
                    }
                    case lang_TypeInstance::DeterminedType::STRUCT:
                    case lang_TypeInstance::DeterminedType::TUPLE:
                    {
                        m_ircontext.c().stidstruct(
                            container_opnum,
                            (uint32_t)pattern_index->m_index->m_LANG_fast_index_for_struct.value(),
                            eval_result_for_upper);
                        break;
                    }
                    default:
                        wo_error("Unknown type.");
                        break;
                    }
                }

                m_ircontext.apply_eval_result(
                    [&](BytecodeGenerateContext::EvalResult& result)
                    {
                        const auto& target_storage = result.get_assign_target(node->m_LANG_determined_type.value());
                        if (target_storage.has_value())
                        {
                            if (node->m_valued_assign)
                            {
                                auto& [need_box, target] = target_storage.value();
                                woort_IRValue* const* const target_irvalue =
                                    std::get_if<woort_IRValue*>(&target);

                                if (target_irvalue == nullptr)
                                {
                                    woort_IRValue* const v = m_ircontext.c().new_value();

                                    if (need_box.has_value())
                                        m_ircontext.c().boxdyn(v, need_box.value(), eval_result_for_upper);
                                    else
                                        m_ircontext.c().mov(v, eval_result_for_upper);

                                    m_ircontext.c().store(std::get<woort_IRStaticIndex>(target), v);
                                }
                                else
                                {
                                    if (need_box.has_value())
                                        m_ircontext.c().boxdyn(
                                            *target_irvalue, need_box.value(), eval_result_for_upper);
                                    else
                                        m_ircontext.c().mov(*target_irvalue, eval_result_for_upper);
                                }
                            }
                            else
                                // No valued return, junk it.
                                ;
                        }
                        else
                        {
                            if (node->m_valued_assign)
                            {
                                result.set_result_stack_temp(
                                    m_ircontext, eval_result_for_upper, node->m_LANG_determined_type.value());
                            }
                            else
                                // Give a junk value.
                                result.set_result_junk(m_ircontext);
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
                    const auto& asigned_target = result.get_assign_target(ast_value->m_LANG_determined_type.value());
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