#include "wo_afx.hpp"

namespace wo
{
#ifndef WO_DISABLE_COMPILER
    using namespace ast;

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
        abort();
        return OKAY;
    }
    WO_PASS_PROCESSER(AstWhile)
    {
        abort();
        return OKAY;
    }
    WO_PASS_PROCESSER(AstFor)
    {
        abort();
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
        abort();
        return OKAY;
    }
    WO_PASS_PROCESSER(AstContinue)
    {
        abort();
        return OKAY;
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
        abort();
        return OKAY;
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

        abort();
        return OKAY;
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
        abort();
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
            anylize_pass(lex, val, &LangContext::pass_final_B_process_bytecode_generation);

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

            m_ircontext.eval_ignore();
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
                    abort();

                    //opnum::opnumbase* immediately_value =
                    //    m_ircontext.opnum_imm_value(constant_value);

                    //const auto& asigned_target = result.get_assign_target();
                    //if (asigned_target.has_value())
                    //    m_ircontext.c().mov(WO_OPNUM(asigned_target.value()), WO_OPNUM(immediately_value));
                    //else
                    //    result.set_result(m_ircontext, immediately_value);
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