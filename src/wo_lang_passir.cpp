#include "wo_lang.hpp"

namespace wo
{
#ifndef WO_DISABLE_COMPILER
    using namespace ast;
    
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
        // WO_LANG_REGISTER_PROCESSER(AstValueTypeid, AstBase::AST_VALUE_TYPEID, passir_B);
        // WO_LANG_REGISTER_PROCESSER(AstValueTypeCast, AstBase::AST_VALUE_TYPE_CAST, passir_B);
        // WO_LANG_REGISTER_PROCESSER(AstValueTypeCheckIs, AstBase::AST_VALUE_TYPE_CHECK_IS, passir_B);
        // WO_LANG_REGISTER_PROCESSER(AstValueTypeCheckAs, AstBase::AST_VALUE_TYPE_CHECK_AS, passir_B);
        // WO_LANG_REGISTER_PROCESSER(AstValueVariable, AstBase::AST_VALUE_VARIABLE, passir_B);
        // WO_LANG_REGISTER_PROCESSER(AstValueFunctionCall, AstBase::AST_VALUE_FUNCTION_CALL, passir_B);
        // WO_LANG_REGISTER_PROCESSER(AstValueBinaryOperator, AstBase::AST_VALUE_BINARY_OPERATOR, passir_B);
        // WO_LANG_REGISTER_PROCESSER(AstValueUnaryOperator, AstBase::AST_VALUE_UNARY_OPERATOR, passir_B);
        // WO_LANG_REGISTER_PROCESSER(AstValueTribleOperator, AstBase::AST_VALUE_TRIBLE_OPERATOR, passir_B);
        // WO_LANG_REGISTER_PROCESSER(AstFakeValueUnpack, AstBase::AST_FAKE_VALUE_UNPACK, passir_B);
        // WO_LANG_REGISTER_PROCESSER(AstValueVariadicArgumentsPack, AstBase::AST_VALUE_VARIADIC_ARGUMENTS_PACK, passir_B);
        // WO_LANG_REGISTER_PROCESSER(AstValueIndex, AstBase::AST_VALUE_INDEX, passir_B);
        // WO_LANG_REGISTER_PROCESSER(AstValueFunction, AstBase::AST_VALUE_FUNCTION, passir_B);
        // WO_LANG_REGISTER_PROCESSER(AstValueArrayOrVec, AstBase::AST_VALUE_ARRAY_OR_VEC, passir_B);
        // WO_LANG_REGISTER_PROCESSER(AstValueDictOrMap, AstBase::AST_VALUE_DICT_OR_MAP, passir_B);
        // WO_LANG_REGISTER_PROCESSER(AstValueTuple, AstBase::AST_VALUE_TUPLE, passir_B);
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
        if (state == UNPROCESSED)
        {
            if (node->m_pattern->node_type == AstBase::AST_PATTERN_SINGLE)
            {
                // Might be constant, template.
                AstPatternSingle* pattern_single = static_cast<AstPatternSingle*>(node->m_pattern);
                if (pattern_single->m_template_parameters.has_value())
                {
                    // Is template, walk through all the template instance;
                    lang_Symbol* symbol_instance = pattern_single->m_LANG_declared_symbol.value();
                    wo_assert(symbol_instance->m_is_template
                        && symbol_instance->m_symbol_kind == lang_Symbol::kind::VARIABLE);

                    for (auto& [_useless, template_instance] : 
                        symbol_instance->m_template_value_instances->m_template_instances)
                    {
                        (void)_useless;

                        lang_ValueInstance* template_value_instance = template_instance->m_value_instance.get();
                        if (template_value_instance->m_determined_constant_or_function.has_value())
                        {
                            wo_assert(!template_value_instance->m_mutable);

                            auto& determined_constant_or_function = 
                                template_value_instance->m_determined_constant_or_function.value();

                            ast::AstValueFunction** function_instance =
                                std::get_if<ast::AstValueFunction*>(&determined_constant_or_function);

                            if (function_instance != nullptr)
                            {
                                if ((*function_instance)->m_LANG_captured_context.m_captured_variables.empty())
                                    // Function without captured variables, no need storage.
                                    continue;
                            }
                            else
                                // Is constant, no need storage.
                                continue;
                        }

                        if (!update_allocate_instance_storage_passir(lex, template_value_instance))
                        {
                            return FAILED;
                        }

                        TODO; BAD IMPL;
                        auto* varibale_storage =
                            m_ircontext.get_storage_place(template_value_instance->m_IR_storage.value());

                        m_ircontext.begin_eval(varibale_storage);
                        if (!pass_final_value(lex, static_cast<AstValueBase*>(template_instance->m_ast)))
                        {
                            return FAILED;
                        }
                        (void)m_ircontext.get_eval_result();
                    }
                    
                }
                else
                {
                }
            }

            return HOLD;
        }



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

            m_ircontext.skip_eval_result();
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
            m_ircontext.apply_eval_result(
                [&](BytecodeGenerateContext::EvalResult& result)
                {
                    opnum::opnumbase* immediately_value =
                        m_ircontext.opnum_imm_value(ast_value->m_evaled_const_value.value());

                    auto& asigned_target = result.get_assign_target();
                    if (asigned_target.has_value())
                        m_ircontext.c().mov(*asigned_target.value(), *immediately_value);
                    else
                        result.set_result(immediately_value);
                });
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