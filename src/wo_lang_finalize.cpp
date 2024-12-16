#include "wo_lang.hpp"

WO_API wo_api rslib_std_return_itself(wo_vm vm, wo_value args);

namespace wo
{
    using namespace ast;
    using namespace opnum;

#define WO_AST() astnode; wo_assert(astnode != nullptr)
#define WO_AST_PASS(NODETYPE) void lang::finalize_##NODETYPE (ast::NODETYPE* astnode, ir_compiler* compiler)
#define WO_VALUE_PASS(NODETYPE) opnum::opnumbase& lang::finalize_value_##NODETYPE (ast::NODETYPE* astnode, ir_compiler* compiler, bool get_pure_value)

    WO_AST_PASS(ast_varref_defines)
    {
        auto* a_varref_defines = WO_AST();
        for (auto& varref_define : a_varref_defines->var_refs)
        {
            bool need_init_check =
                a_varref_defines->located_function != nullptr && a_varref_defines->declear_attribute->is_static_attr();

            std::string init_static_flag_check_tag;
            if (need_init_check)
            {
                init_static_flag_check_tag = compiler->get_unique_tag_based_command_ip();

                auto& static_inited_flag = get_new_global_variable();
                compiler->equb(static_inited_flag, reg(reg::ni));
                compiler->jf(tag(init_static_flag_check_tag));
                compiler->mov(static_inited_flag, imm(true));
            }

            analyze_pattern_in_finalize(varref_define.pattern, varref_define.init_val, false, compiler);

            if (need_init_check)
                compiler->tag(init_static_flag_check_tag);
        }
    }
    WO_AST_PASS(ast_list)
    {
        auto* a_list = WO_AST();

        auto* child = a_list->children;
        while (child)
        {
            real_analyze_finalize(child, compiler);
            child = child->sibling;
        }
    }
    WO_AST_PASS(ast_if)
    {
        auto* a_if = WO_AST();

        if (a_if->judgement_value->is_constant)
        {
            auto_analyze_value(a_if->judgement_value, compiler, false);

            if (a_if->judgement_value->get_constant_value().integer)
                real_analyze_finalize(a_if->execute_if_true, compiler);
            else if (a_if->execute_else)
                real_analyze_finalize(a_if->execute_else, compiler);
        }
        else
        {
            auto generate_state = compiler->get_now_ip();

            auto& if_judge_val = auto_analyze_value(a_if->judgement_value, compiler);

            if (auto* immval = dynamic_cast<opnum::immbase*>(&if_judge_val))
            {
                compiler->revert_code_to(generate_state);

                if (immval->is_true())
                    real_analyze_finalize(a_if->execute_if_true, compiler);
                else if (a_if->execute_else)
                    real_analyze_finalize(a_if->execute_else, compiler);
            }
            else
            {
                mov_value_to_cr(if_judge_val, compiler);

                auto ifelse_tag = "if_else_" + compiler->get_unique_tag_based_command_ip();
                auto ifend_tag = "if_end_" + compiler->get_unique_tag_based_command_ip();

                compiler->jf(tag(ifelse_tag));

                real_analyze_finalize(a_if->execute_if_true, compiler);
                if (a_if->execute_else)
                    compiler->jmp(tag(ifend_tag));
                compiler->tag(ifelse_tag);
                if (a_if->execute_else)
                {
                    real_analyze_finalize(a_if->execute_else, compiler);
                    compiler->tag(ifend_tag);
                }
            }
        }
    }
    WO_AST_PASS(ast_while)
    {
        auto* a_while = WO_AST();

        auto while_begin_tag = "while_begin_" + compiler->get_unique_tag_based_command_ip();
        auto while_end_tag = "while_end_" + compiler->get_unique_tag_based_command_ip();

        loop_stack_for_break_and_continue.push_back({
           a_while->marking_label,

             while_end_tag,
           while_begin_tag,

            });

        compiler->tag(while_begin_tag);                                                         // while_begin_tag:
        if (a_while->judgement_value != nullptr)
        {
            mov_value_to_cr(auto_analyze_value(a_while->judgement_value, compiler), compiler);  //    * do expr
            compiler->jf(tag(while_end_tag));                                                   //    jf    while_end_tag;
        }

        real_analyze_finalize(a_while->execute_sentence, compiler);                             //              ...

        compiler->jmp(tag(while_begin_tag));                                                    //    jmp   while_begin_tag;
        compiler->tag(while_end_tag);                                                           // while_end_tag:

        loop_stack_for_break_and_continue.pop_back();
    }
    WO_AST_PASS(ast_forloop)
    {
        auto* a_forloop = WO_AST();

        auto forloop_begin_tag = "forloop_begin_" + compiler->get_unique_tag_based_command_ip();
        auto forloop_continue_tag = "forloop_continue_" + compiler->get_unique_tag_based_command_ip();
        auto forloop_end_tag = "forloop_end_" + compiler->get_unique_tag_based_command_ip();

        loop_stack_for_break_and_continue.push_back({
           a_forloop->marking_label,

           forloop_end_tag,
           a_forloop->after_execute ? forloop_continue_tag : forloop_begin_tag,
            });

        if (a_forloop->pre_execute)
            real_analyze_finalize(a_forloop->pre_execute, compiler);

        compiler->tag(forloop_begin_tag);

        if (a_forloop->judgement_expr)
        {
            mov_value_to_cr(auto_analyze_value(a_forloop->judgement_expr, compiler), compiler);
            compiler->jf(tag(forloop_end_tag));
        }

        real_analyze_finalize(a_forloop->execute_sentences, compiler);

        compiler->tag(forloop_continue_tag);

        if (a_forloop->after_execute)
            real_analyze_finalize(a_forloop->after_execute, compiler);

        compiler->jmp(tag(forloop_begin_tag));
        compiler->tag(forloop_end_tag);

        loop_stack_for_break_and_continue.pop_back();
    }
    WO_AST_PASS(ast_sentence_block)
    {
        auto* a_sentence_block = WO_AST();
        real_analyze_finalize(a_sentence_block->sentence_list, compiler);
    }
    WO_AST_PASS(ast_return)
    {
        auto* a_return = WO_AST();
        if (a_return->return_value)
            mov_value_to_cr(auto_analyze_value(a_return->return_value, compiler), compiler);

        if (a_return->located_function == nullptr)
        {
            if (a_return->return_value == nullptr)
                compiler->mov(opnum::reg(opnum::reg::spreg::cr), opnum::imm(0));

            compiler->jmp(opnum::tag("__rsir_rtcode_seg_function_define_end"));
        }
        else if (a_return->return_value)
        {
            if (a_return->located_function->is_closure_function())
                compiler->ret((uint16_t)a_return->located_function->capture_variables.size());
            else
                compiler->ret();
        }
        else
            compiler->jmp(tag(a_return->located_function->get_ir_func_signature_tag() + "_do_ret"));
    }
    WO_AST_PASS(ast_namespace)
    {
        auto* a_namespace = WO_AST();
        if (a_namespace->in_scope_sentence != nullptr)
            real_analyze_finalize(a_namespace->in_scope_sentence, compiler);
    }
    WO_AST_PASS(ast_using_namespace)
    {
        // do nothing
    }
    WO_AST_PASS(ast_using_type_as)
    {
        auto* a_using_type_as = WO_AST();
        if (a_using_type_as->namespace_decl != nullptr)
            real_analyze_finalize(a_using_type_as->namespace_decl, compiler);
    }
    WO_AST_PASS(ast_nop)
    {
        compiler->nop();
    }
    WO_AST_PASS(ast_foreach)
    {
        auto* a_foreach = WO_AST();
        real_analyze_finalize(a_foreach->used_iter_define, compiler);
        real_analyze_finalize(a_foreach->loop_sentences, compiler);
    }
    WO_AST_PASS(ast_break)
    {
        auto* a_break = WO_AST();
        if (a_break->label == nullptr)
        {
            if (loop_stack_for_break_and_continue.empty())
                lang_anylizer->lang_error(lexer::errorlevel::error, a_break, WO_ERR_INVALID_OPERATE, L"break");
            else
                compiler->jmp(tag(loop_stack_for_break_and_continue.back().current_loop_break_aim_tag));
        }
        else
        {
            for (auto ridx = loop_stack_for_break_and_continue.rbegin();
                ridx != loop_stack_for_break_and_continue.rend();
                ridx++)
            {
                if (ridx->current_loop_label == a_break->label)
                {
                    compiler->jmp(tag(ridx->current_loop_break_aim_tag));
                    goto break_label_successful;
                }
            }

            lang_anylizer->lang_error(lexer::errorlevel::error, a_break, WO_ERR_INVALID_OPERATE, L"break");

        break_label_successful:
            ;
        }
    }
    WO_AST_PASS(ast_continue)
    {
        auto* a_continue = WO_AST();
        if (a_continue->label == nullptr)
        {
            if (loop_stack_for_break_and_continue.empty())
                lang_anylizer->lang_error(lexer::errorlevel::error, a_continue, WO_ERR_INVALID_OPERATE, L"continue");
            else
                compiler->jmp(tag(loop_stack_for_break_and_continue.back().current_loop_continue_aim_tag));
        }
        else
        {
            for (auto ridx = loop_stack_for_break_and_continue.rbegin();
                ridx != loop_stack_for_break_and_continue.rend();
                ridx++)
            {
                if (ridx->current_loop_label == a_continue->label)
                {
                    compiler->jmp(tag(ridx->current_loop_continue_aim_tag));
                    goto continue_label_successful;
                }
            }

            lang_anylizer->lang_error(lexer::errorlevel::error, a_continue, WO_ERR_INVALID_OPERATE, L"continue");

        continue_label_successful:
            ;
        }
    }
    WO_AST_PASS(ast_union_make_option_ob_to_cr_and_ret)
    {
        auto* a_union_make_option_ob_to_cr_and_ret = WO_AST();
        if (a_union_make_option_ob_to_cr_and_ret->argument_may_nil)
            compiler->mkunion(reg(reg::cr), auto_analyze_value(a_union_make_option_ob_to_cr_and_ret->argument_may_nil, compiler),
                a_union_make_option_ob_to_cr_and_ret->id);
        else
            compiler->mkunion(reg(reg::cr), reg(reg::ni), a_union_make_option_ob_to_cr_and_ret->id);

        // TODO: ast_union_make_option_ob_to_cr_and_ret not exist in closure function, so we just ret here.
        //       need check!
        compiler->ret();
    }
    WO_AST_PASS(ast_match)
    {
        auto* a_match = WO_AST();
        a_match->match_end_tag_in_final_pass = compiler->get_unique_tag_based_command_ip() + "match_end";

        compiler->mov(reg(reg::pm), auto_analyze_value(a_match->match_value, compiler));
        // 1. Get id in cr.
        compiler->idstruct(reg(reg::cr), reg(reg::pm), 0);

        real_analyze_finalize(a_match->cases, compiler);
        compiler->ext_panic(opnum::imm_str("All cases failed to match, may be wrong type value returned by the external function."));

        compiler->tag(a_match->match_end_tag_in_final_pass);
    }
    WO_AST_PASS(ast_match_union_case)
    {
        auto* a_match_union_case = WO_AST();
        auto current_case_end = compiler->get_unique_tag_based_command_ip() + "case_end";

        // 0.Check id?
        if (ast_pattern_union_value* a_pattern_union_value = dynamic_cast<ast_pattern_union_value*>(a_match_union_case->union_pattern))
        {
            if (a_match_union_case->in_match->match_value->value_type->is_pending())
                lang_anylizer->lang_error(lexer::errorlevel::error, a_match_union_case, WO_ERR_UNKNOWN_MATCHING_VAL_TYPE);

            if (ast_value_variable* case_item = a_pattern_union_value->union_expr)
            {
                auto fnd = a_match_union_case->in_match->match_value->value_type->struct_member_index.find(case_item->var_name);
                if (fnd == a_match_union_case->in_match->match_value->value_type->struct_member_index.end())
                    lang_anylizer->lang_error(lexer::errorlevel::error, a_match_union_case, WO_ERR_UNKNOWN_CASE_TYPE);
                else
                {
                    compiler->jnequb(imm((wo_integer_t)fnd->second.offset), tag(current_case_end));
                }
            }
            else
                ; // Meet default pattern, just goon, no need to check tag.

            if (a_pattern_union_value->pattern_arg_in_union_may_nil)
            {
                auto& valreg = get_useable_register_for_pure_value();
                compiler->idstruct(valreg, reg(reg::pm), 1);

                wo_assert(a_match_union_case->take_place_value_may_nil);
                a_match_union_case->take_place_value_may_nil->used_reg = &valreg;

                analyze_pattern_in_finalize(
                    a_pattern_union_value->pattern_arg_in_union_may_nil, 
                    a_match_union_case->take_place_value_may_nil, 
                    true, 
                    compiler);
            }
            else
            {
                if (a_pattern_union_value->union_expr != nullptr
                    && a_pattern_union_value->union_expr->value_type->argument_types.size() != 0)
                    lang_anylizer->lang_error(lexer::errorlevel::error, a_match_union_case, WO_ERR_INVALID_CASE_TYPE_NO_ARG_RECV);
            }
        }
        else
            lang_anylizer->lang_error(lexer::errorlevel::error, a_match_union_case, WO_ERR_UNEXPECT_PATTERN_CASE);

        real_analyze_finalize(a_match_union_case->in_case_sentence, compiler);

        compiler->jmp(tag(a_match_union_case->in_match->match_end_tag_in_final_pass));
        compiler->tag(current_case_end);
    }

#define WO_NEW_OPNUM(...) (*generated_opnum_list_for_clean.emplace_back(new __VA_ARGS__))
    WO_VALUE_PASS(ast_value_function_define)
    {
        auto* a_value_function_define = WO_AST();
        // function defination
        if (nullptr == a_value_function_define->externed_func_info)
        {
            if (a_value_function_define->ir_func_has_been_generated == false)
            {
                in_used_functions.push_back(a_value_function_define);
                a_value_function_define->ir_func_has_been_generated = true;
            }

            if (!a_value_function_define->is_closure_function())
                return WO_NEW_OPNUM(opnum::tagimm_rsfunc(a_value_function_define->get_ir_func_signature_tag()));
            else
            {
                // make closure
                for (auto idx = a_value_function_define->capture_variables.rbegin();
                    idx != a_value_function_define->capture_variables.rend();
                    ++idx)
                {
                    auto opnum_or_stackoffset = get_opnum_by_symbol(a_value_function_define, *idx, compiler);
                    if (auto* opnum = std::get_if<opnumbase*>(&opnum_or_stackoffset))
                        compiler->psh(**opnum);
                    else
                    {
                        int16_t stack_offset = std::get<int16_t>(opnum_or_stackoffset);
                        wo_assert(get_pure_value == false);

                        set_stack_offset_stored(stack_offset);

                        auto& usable_stack = get_useable_register_for_pure_value();
                        compiler->lds(usable_stack, imm(stack_offset));
                        compiler->psh(usable_stack);

                        complete_using_register(usable_stack);
                    }

                }

                compiler->mkclos((uint16_t)a_value_function_define->capture_variables.size(),
                    opnum::tagimm_rsfunc(a_value_function_define->get_ir_func_signature_tag()));
                if (get_pure_value)
                {
                    auto& treg = get_useable_register_for_pure_value();
                    compiler->mov(treg, reg(reg::cr));
                    return treg;
                }
                else
                    return WO_NEW_OPNUM(reg(reg::cr));  // return cr~
            }
        }
        else
        {
            // Extern template function in define, skip it.
            return WO_NEW_OPNUM(reg(reg::ni));
        }
    }
    WO_VALUE_PASS(ast_value_variable)
    {
        auto* a_value_variable = WO_AST();
        // ATTENTION: HERE JUST VALUE , NOT JUDGE FUNCTION
        auto symb = a_value_variable->symbol;

        if (symb == nullptr)
        {
            lang_anylizer->lang_error(lexer::errorlevel::error, a_value_variable,
                WO_ERR_UNKNOWN_IDENTIFIER,
                a_value_variable->get_full_variable_name().c_str());

            return WO_NEW_OPNUM(reg(reg::ni));
        }

        if (symb->is_template_symbol)
        {
            // In fact, all variable symbol cannot be templated, it must because of non-impl-template-function-name.
            // func foo<T>(x: T){...}
            // let f = foo;  // << here
            lang_anylizer->lang_error(lexer::errorlevel::error, a_value_variable, WO_ERR_NO_MATCHED_FUNC_TEMPLATE);
        }

        auto opnum_or_stackoffset = get_opnum_by_symbol(a_value_variable, symb, compiler, get_pure_value);
        if (auto* opnum = std::get_if<opnumbase*>(&opnum_or_stackoffset))
            return **opnum;
        else
        {
            int16_t stack_offset = std::get<int16_t>(opnum_or_stackoffset);
            wo_assert(get_pure_value == false);

            set_stack_offset_stored(stack_offset);

            auto& usable_stack = get_useable_register_for_pure_value();
            compiler->lds(usable_stack, imm(stack_offset));

            return usable_stack;
        }
    }
    WO_VALUE_PASS(ast_value_binary)
    {
        auto* a_value_binary = WO_AST();
        if (a_value_binary->overrided_operation_call)
            return analyze_value(a_value_binary->overrided_operation_call, compiler, get_pure_value);

        // if mixed type, do opx
        value::valuetype optype = value::valuetype::invalid;
        if (a_value_binary->left->value_type->is_same(a_value_binary->right->value_type, true))
            optype = a_value_binary->left->value_type->value_type;

        auto& beoped_left_opnum = analyze_value(a_value_binary->left, compiler, true);
        auto& op_right_opnum = analyze_value(a_value_binary->right, compiler);

        switch (a_value_binary->operate)
        {
        case lex_type::l_add:
            switch (optype)
            {
            case wo::value::valuetype::integer_type:
                compiler->addi(beoped_left_opnum, op_right_opnum); break;
            case wo::value::valuetype::real_type:
                compiler->addr(beoped_left_opnum, op_right_opnum); break;
            case wo::value::valuetype::handle_type:
                compiler->addh(beoped_left_opnum, op_right_opnum); break;
            case wo::value::valuetype::string_type:
                compiler->adds(beoped_left_opnum, op_right_opnum); break;
            default:
                lang_anylizer->lang_error(
                    lexer::errorlevel::error,
                    a_value_binary, 
                    WO_ERR_CANNOT_CALC_WITH_L_AND_R,
                    a_value_binary->left->value_type->get_type_name(false).c_str(),
                    a_value_binary->right->value_type->get_type_name(false).c_str());
                break;
            }
            break;
        case lex_type::l_sub:
            switch (optype)
            {
            case wo::value::valuetype::integer_type:
                compiler->subi(beoped_left_opnum, op_right_opnum); break;
            case wo::value::valuetype::real_type:
                compiler->subr(beoped_left_opnum, op_right_opnum); break;
            case wo::value::valuetype::handle_type:
                compiler->subh(beoped_left_opnum, op_right_opnum); break;
            default:
                lang_anylizer->lang_error(lexer::errorlevel::error,
                    a_value_binary, 
                    WO_ERR_CANNOT_CALC_WITH_L_AND_R,
                    a_value_binary->left->value_type->get_type_name(false).c_str(),
                    a_value_binary->right->value_type->get_type_name(false).c_str());
                break;
            }

            break;
        case lex_type::l_mul:
            switch (optype)
            {
            case wo::value::valuetype::integer_type:
                compiler->muli(beoped_left_opnum, op_right_opnum); break;
            case wo::value::valuetype::real_type:
                compiler->mulr(beoped_left_opnum, op_right_opnum); break;
            default:
                lang_anylizer->lang_error(lexer::errorlevel::error, 
                    a_value_binary,
                    WO_ERR_CANNOT_CALC_WITH_L_AND_R,
                    a_value_binary->left->value_type->get_type_name(false).c_str(),
                    a_value_binary->right->value_type->get_type_name(false).c_str());
                break;
            }

            break;
        case lex_type::l_div:
            switch (optype)
            {
            case wo::value::valuetype::integer_type:
            {
                check_division(a_value_binary, a_value_binary->left, a_value_binary->right, beoped_left_opnum, op_right_opnum, compiler);
                compiler->divi(beoped_left_opnum, op_right_opnum);
                break;
            }
            case wo::value::valuetype::real_type:
                compiler->divr(beoped_left_opnum, op_right_opnum); break;
            default:
                lang_anylizer->lang_error(lexer::errorlevel::error, a_value_binary, WO_ERR_CANNOT_CALC_WITH_L_AND_R,
                    a_value_binary->left->value_type->get_type_name(false).c_str(),
                    a_value_binary->right->value_type->get_type_name(false).c_str());
                break;
            }

            break;
        case lex_type::l_mod:
            switch (optype)
            {
            case wo::value::valuetype::integer_type:
            {
                check_division(a_value_binary, a_value_binary->left, a_value_binary->right, beoped_left_opnum, op_right_opnum, compiler);
                compiler->modi(beoped_left_opnum, op_right_opnum);
                break;
            }
            case wo::value::valuetype::real_type:
                compiler->modr(beoped_left_opnum, op_right_opnum); break;
            default:
                lang_anylizer->lang_error(lexer::errorlevel::error, a_value_binary, WO_ERR_CANNOT_CALC_WITH_L_AND_R,
                    a_value_binary->left->value_type->get_type_name(false).c_str(),
                    a_value_binary->right->value_type->get_type_name(false).c_str());
                break;
            }

            break;
        default:
            wo_error("Do not support this operator..");
            break;
        }
        complete_using_register(op_right_opnum);

        if (get_pure_value && is_cr_reg(beoped_left_opnum))
        {
            auto& treg = get_useable_register_for_pure_value();
            compiler->mov(treg, beoped_left_opnum);
            return treg;
        }
        else
            return beoped_left_opnum;
    }
    WO_VALUE_PASS(ast_value_assign)
    {
        auto* a_value_assign = WO_AST();

        ast_value_index* a_value_index = dynamic_cast<ast_value_index*>(a_value_assign->left);

        opnumbase* beassign_index_from_opnum = nullptr;
        opnumbase* beassign_index_key_opnum = nullptr;
        opnumbase* beassign_w_left_opnum = nullptr;
        opnumbase* beassign_r_left_opnum = nullptr;
        opnumbase* beoped_right_opnum = nullptr;

        if (a_value_index == nullptr)
        {
            beoped_right_opnum = &analyze_value(a_value_assign->right, compiler);

#if WO_ENABLE_RUNTIME_CHECK
            bool eval_right_cr_modified = false;
            beassign_r_left_opnum = &analyze_value_and_get_cr_modified(a_value_assign->left, compiler, false, &eval_right_cr_modified);

            wo_assert(eval_right_cr_modified == false);
#else
            beassign_r_left_opnum = &analyze_value(a_value_assign->left, compiler);
#endif

            if (!_last_value_from_stack_offset_may_null)
                beassign_w_left_opnum = beassign_r_left_opnum;
        }
        else
        {
            beoped_right_opnum = &analyze_value(a_value_assign->right, compiler, true);
            if (a_value_assign->operate != lex_type::l_assign && a_value_assign->operate != lex_type::l_value_assign)
                beassign_r_left_opnum = &do_index_value_impl(a_value_index, compiler, true, true, &beassign_index_from_opnum, &beassign_index_key_opnum);
            else
            {
                beassign_index_from_opnum = &analyze_value(a_value_index->from, compiler, true);
                if (!a_value_index->from->value_type->is_struct() && !a_value_index->from->value_type->is_tuple())
                {
                    beassign_index_key_opnum = &analyze_value(a_value_index->index, compiler, true);
                }
            }
        }
        wo_assert(beoped_right_opnum != nullptr);

        if (a_value_assign->overrided_operation_call != nullptr)
        {
            compiler->psh(*beoped_right_opnum);
            compiler->psh(*beassign_r_left_opnum);

            auto* called_function = a_value_assign->overrided_operation_call->called_func;
            auto* called_function_define = dynamic_cast<ast::ast_value_function_define*>(called_function);

            if (called_function->value_type->is_variadic_function_type)
                compiler->mov(reg(reg::tc), imm(2));

            if (called_function_define != nullptr
                && called_function_define->externed_func_info != nullptr)
            {
                if (called_function_define->externed_func_info->is_slow_leaving_call == false)
                    compiler->callfast((void*)called_function_define->externed_func_info->externed_func);
                else
                    compiler->call((void*)called_function_define->externed_func_info->externed_func);
            }
            else
            {
                auto* called_func_aim = &analyze_value(a_value_assign->overrided_operation_call->called_func, compiler);
                compiler->call(complete_using_register(*called_func_aim));
            }
            compiler->pop(2);
            complete_using_register(*beassign_r_left_opnum);
            beassign_r_left_opnum = &WO_NEW_OPNUM(reg(reg::cr));

            set_cr_modified();
        }
        else
        {
            const value::valuetype optype = a_value_assign->left->value_type->value_type;

            switch (a_value_assign->operate)
            {
            case lex_type::l_assign:
            case lex_type::l_value_assign:
            {
                if (beassign_r_left_opnum != nullptr)
                    complete_using_register(*beassign_r_left_opnum);

                beassign_r_left_opnum = beoped_right_opnum;

                break;
            }
            case lex_type::l_add_assign:
            case lex_type::l_value_add_assign:
                switch (optype)
                {
                case wo::value::valuetype::integer_type:
                    compiler->addi(*beassign_r_left_opnum, *beoped_right_opnum); break;
                case wo::value::valuetype::real_type:
                    compiler->addr(*beassign_r_left_opnum, *beoped_right_opnum); break;
                case wo::value::valuetype::handle_type:
                    compiler->addh(*beassign_r_left_opnum, *beoped_right_opnum); break;
                case wo::value::valuetype::string_type:
                    compiler->adds(*beassign_r_left_opnum, *beoped_right_opnum); break;
                default:
                    lang_anylizer->lang_error(lexer::errorlevel::error, a_value_assign, WO_ERR_CANNOT_CALC_WITH_L_AND_R,
                        a_value_assign->left->value_type->get_type_name(false).c_str(),
                        a_value_assign->right->value_type->get_type_name(false).c_str());
                    break;
                }
                break;
            case lex_type::l_sub_assign:
            case lex_type::l_value_sub_assign:
                switch (optype)
                {
                case wo::value::valuetype::integer_type:
                    compiler->subi(*beassign_r_left_opnum, *beoped_right_opnum); break;
                case wo::value::valuetype::real_type:
                    compiler->subr(*beassign_r_left_opnum, *beoped_right_opnum); break;
                case wo::value::valuetype::handle_type:
                    compiler->subh(*beassign_r_left_opnum, *beoped_right_opnum); break;
                default:
                    lang_anylizer->lang_error(lexer::errorlevel::error, a_value_assign, WO_ERR_CANNOT_CALC_WITH_L_AND_R,
                        a_value_assign->left->value_type->get_type_name(false).c_str(),
                        a_value_assign->right->value_type->get_type_name(false).c_str());
                    break;
                }

                break;
            case lex_type::l_mul_assign:
            case lex_type::l_value_mul_assign:
                switch (optype)
                {
                case wo::value::valuetype::integer_type:
                    compiler->muli(*beassign_r_left_opnum, *beoped_right_opnum); break;
                case wo::value::valuetype::real_type:
                    compiler->mulr(*beassign_r_left_opnum, *beoped_right_opnum); break;
                default:
                    lang_anylizer->lang_error(lexer::errorlevel::error, a_value_assign, WO_ERR_CANNOT_CALC_WITH_L_AND_R,
                        a_value_assign->left->value_type->get_type_name(false).c_str(),
                        a_value_assign->right->value_type->get_type_name(false).c_str());
                    break;
                }

                break;
            case lex_type::l_div_assign:
            case lex_type::l_value_div_assign:
                switch (optype)
                {
                case wo::value::valuetype::integer_type:
                {
                    check_division(a_value_assign, a_value_assign->left, a_value_assign->right, *beassign_r_left_opnum, *beoped_right_opnum, compiler);
                    compiler->divi(*beassign_r_left_opnum, *beoped_right_opnum);
                    break;
                }
                case wo::value::valuetype::real_type:
                    compiler->divr(*beassign_r_left_opnum, *beoped_right_opnum); break;
                default:
                    lang_anylizer->lang_error(lexer::errorlevel::error, a_value_assign, WO_ERR_CANNOT_CALC_WITH_L_AND_R,
                        a_value_assign->left->value_type->get_type_name(false).c_str(),
                        a_value_assign->right->value_type->get_type_name(false).c_str());
                    break;
                }
                break;
            case lex_type::l_mod_assign:
            case lex_type::l_value_mod_assign:
                switch (optype)
                {
                case wo::value::valuetype::integer_type:
                {
                    check_division(a_value_assign, a_value_assign->left, a_value_assign->right, *beassign_r_left_opnum, *beoped_right_opnum, compiler);
                    compiler->modi(*beassign_r_left_opnum, *beoped_right_opnum);
                    break;
                }
                case wo::value::valuetype::real_type:
                    compiler->modr(*beassign_r_left_opnum, *beoped_right_opnum); break;
                default:
                    lang_anylizer->lang_error(lexer::errorlevel::error, a_value_assign, WO_ERR_CANNOT_CALC_WITH_L_AND_R,
                        a_value_assign->left->value_type->get_type_name(false).c_str(),
                        a_value_assign->right->value_type->get_type_name(false).c_str());
                    break;
                }

                break;
            default:
                wo_error("Do not support this operator..");
                break;
            }
        }

        wo_assert(beassign_r_left_opnum != nullptr);

        if (beassign_w_left_opnum != nullptr)
        {
            if (beassign_w_left_opnum != beassign_r_left_opnum)
                compiler->mov(*beassign_w_left_opnum, *beassign_r_left_opnum);
            else
                ;// Assign has been finished at above.
        }
        else if (beassign_index_from_opnum != nullptr)
        {
            if (a_value_index->from->value_type->is_struct() || a_value_index->from->value_type->is_tuple())
            {
                wo_assert(beassign_index_key_opnum == nullptr);
                compiler->sidstruct(*beassign_index_from_opnum, *beassign_r_left_opnum, a_value_index->struct_offset);
            }
            else
            {
                if (!is_reg(*beassign_r_left_opnum))
                {
                    auto* tmp = &WO_NEW_OPNUM(reg(reg::tp));;
                    compiler->mov(*tmp, *beassign_r_left_opnum);

                    beassign_r_left_opnum = tmp;
                }

                if (a_value_index->from->value_type->is_array() || a_value_index->from->value_type->is_vec())
                    compiler->sidarr(*beassign_index_from_opnum, *beassign_index_key_opnum, *dynamic_cast<reg*>(beassign_r_left_opnum));
                else if (a_value_index->from->value_type->is_dict())
                    compiler->siddict(*beassign_index_from_opnum, *beassign_index_key_opnum, *dynamic_cast<reg*>(beassign_r_left_opnum));
                else if (a_value_index->from->value_type->is_map())
                    compiler->sidmap(*beassign_index_from_opnum, *beassign_index_key_opnum, *dynamic_cast<reg*>(beassign_r_left_opnum));
                else
                    wo_error("Bad index type.");
            }

            complete_using_register(*beassign_index_from_opnum);

            if (beassign_index_key_opnum != nullptr)
                complete_using_register(*beassign_index_key_opnum);
        }

        complete_using_register(*beassign_r_left_opnum);
        complete_using_register(*beoped_right_opnum);

        if (get_pure_value)
        {
            auto& treg = get_useable_register_for_pure_value();
            compiler->mov(treg, *beassign_r_left_opnum);
            return treg;
        }
        else
            return *beassign_r_left_opnum;
    }
    WO_VALUE_PASS(ast_value_mutable)
    {
        auto* a_value_mutable_or_pure = WO_AST();
        return analyze_value(a_value_mutable_or_pure->val, compiler, get_pure_value);
    }
    WO_VALUE_PASS(ast_value_init)
    {
        auto* a_value_init = WO_AST();
        return analyze_value(a_value_init->init_value, compiler, get_pure_value);
    }
    WO_VALUE_PASS(ast_value_type_cast)
    {
        auto* a_value_type_cast = WO_AST();
        if (a_value_type_cast->value_type->is_dynamic()
            || a_value_type_cast->value_type->is_void()
            || a_value_type_cast->value_type->accept_type(a_value_type_cast->_be_cast_value_node->value_type, true, true)
            || a_value_type_cast->value_type->is_function())
            // no cast, just as origin value
            return analyze_value(a_value_type_cast->_be_cast_value_node, compiler, get_pure_value);

        auto& treg = get_useable_register_for_pure_value();
        compiler->movcast(treg,
            complete_using_register(analyze_value(a_value_type_cast->_be_cast_value_node, compiler)),
            a_value_type_cast->value_type->value_type);
        return treg;
    }
    WO_VALUE_PASS(ast_value_type_judge)
    {
        auto* a_value_type_judge = WO_AST();
        auto& result = analyze_value(a_value_type_judge->_be_cast_value_node, compiler, get_pure_value);

        if (a_value_type_judge->_be_cast_value_node->value_type->is_dynamic())
        {
            wo_assert(
                a_value_type_judge->value_type->is_pure_base_type()
                && !a_value_type_judge->value_type->is_void()
                && !a_value_type_judge->value_type->is_nothing());

            if (!a_value_type_judge->value_type->is_dynamic())
            {
                wo_test(!a_value_type_judge->value_type->is_pending());
                compiler->typeas(result, a_value_type_judge->value_type->value_type);
            }
        }
        else
        {
            wo_assert(a_value_type_judge->value_type->accept_type(
                a_value_type_judge->_be_cast_value_node->value_type, false, true));
        }

        return result;
    }

    WO_VALUE_PASS(ast_value_type_check)
    {
        auto* a_value_type_check = WO_AST();
        if (a_value_type_check->aim_type->accept_type(a_value_type_check->_be_check_value_node->value_type, false, true))
            return WO_NEW_OPNUM(imm(true));

        if (a_value_type_check->_be_check_value_node->value_type->is_dynamic())
        {
            wo_assert(a_value_type_check->aim_type->is_pure_base_type()
                && !a_value_type_check->aim_type->is_void()
                && !a_value_type_check->aim_type->is_nothing());

            if (!a_value_type_check->aim_type->is_dynamic())
            {
                // is dynamic do check..
                auto& result = analyze_value(a_value_type_check->_be_check_value_node, compiler);

                wo_assert(!a_value_type_check->aim_type->is_pending());
                compiler->typeis(complete_using_register(result), a_value_type_check->aim_type->value_type);

                set_last_value_also_store_to_cr();

                if (get_pure_value)
                {
                    auto& treg = get_useable_register_for_pure_value();
                    compiler->mov(treg, reg(reg::cr));
                    return treg;
                }
                else
                    return WO_NEW_OPNUM(reg(reg::cr));
            }
        }

        return WO_NEW_OPNUM(imm(false));
    }

    WO_VALUE_PASS(ast_value_funccall)
    {
        auto* a_value_funccall = WO_AST();

        auto arg = a_value_funccall->arguments->children;
        ast_value_variable* funcvariable = dynamic_cast<ast_value_variable*>(a_value_funccall->called_func);
        ast_value_function_define* funcdef = dynamic_cast<ast_value_function_define*>(a_value_funccall->called_func);

        if (funcvariable != nullptr)
        {
            if (funcvariable->symbol != nullptr && funcvariable->symbol->type == lang_symbol::symbol_type::function)
                funcdef = funcvariable->symbol->get_funcdef();
        }

        if (funcdef != nullptr
            && funcdef->externed_func_info != nullptr)
        {
            if (config::ENABLE_SKIP_INVOKE_UNSAFE_CAST
                && funcdef->externed_func_info->externed_func == &rslib_std_return_itself)
            {
                // Invoking unsafe::cast, skip and return it self.
                ast_value* arg_val = dynamic_cast<ast_value*>(arg);

                if (arg_val->sibling == nullptr && nullptr == dynamic_cast<ast_fakevalue_unpacked_args*>(arg_val))
                    return analyze_value(arg_val, compiler, get_pure_value);
            }
        }

        if (now_function_in_final_anylize && now_function_in_final_anylize->value_type->is_variadic_function_type)
            compiler->psh(reg(reg::tc));

        std::vector<ast_value* >arg_list;

        bool full_unpack_arguments = false;
        wo_integer_t extern_unpack_arg_count = 0;

        while (arg)
        {
            ast_value* arg_val = dynamic_cast<ast_value*>(arg);
            wo_assert(arg_val);

            if (full_unpack_arguments)
            {
                lang_anylizer->lang_error(lexer::errorlevel::error, arg_val, WO_ERR_ARG_DEFINE_AFTER_VARIADIC);
                break;
            }

            if (auto* a_fakevalue_unpacked_args = dynamic_cast<ast_fakevalue_unpacked_args*>(arg_val))
            {
                if (a_fakevalue_unpacked_args->expand_count == ast_fakevalue_unpacked_args::UNPACK_ALL_ARGUMENT)
                {
                    if (a_fakevalue_unpacked_args->unpacked_pack->value_type->is_tuple())
                    {
                        auto* unpacking_tuple_type = a_fakevalue_unpacked_args->unpacked_pack->value_type;
                        if (unpacking_tuple_type->using_type_name)
                            unpacking_tuple_type = unpacking_tuple_type->using_type_name;
                        a_fakevalue_unpacked_args->expand_count =
                            (int32_t)unpacking_tuple_type->template_arguments.size();
                    }
                    else
                    {
                        a_fakevalue_unpacked_args->expand_count = 0;
                    }
                }

                if (a_fakevalue_unpacked_args->expand_count <= 0)
                {
                    if (!(a_fakevalue_unpacked_args->unpacked_pack->value_type->is_tuple()
                        && a_fakevalue_unpacked_args->expand_count == 0))
                        full_unpack_arguments = true;
                }
            }

            arg_list.insert(arg_list.begin(), arg_val);
            arg = arg->sibling;
        }

        for (auto* argv : arg_list)
        {
            if (auto* a_fakevalue_unpacked_args = dynamic_cast<ast_fakevalue_unpacked_args*>(argv))
            {
                auto& packing = analyze_value(a_fakevalue_unpacked_args->unpacked_pack, compiler,
                    a_fakevalue_unpacked_args->expand_count <= 0);

                if (a_fakevalue_unpacked_args->unpacked_pack->value_type->is_tuple())
                {
                    extern_unpack_arg_count += a_fakevalue_unpacked_args->expand_count - 1;
                    compiler->unpackargs(packing, a_fakevalue_unpacked_args->expand_count);
                }
                else
                {
                    if (a_fakevalue_unpacked_args->expand_count <= 0)
                        compiler->mov(reg(reg::tc), imm(arg_list.size() + extern_unpack_arg_count - 1));
                    else
                        extern_unpack_arg_count += a_fakevalue_unpacked_args->expand_count - 1;

                    compiler->unpackargs(packing, a_fakevalue_unpacked_args->expand_count);
                }
                complete_using_register(packing);
            }
            else
            {
                compiler->psh(complete_using_register(analyze_value(argv, compiler)));
            }
        }

        auto* called_func_aim = &analyze_value(a_value_funccall->called_func, compiler);

        if (!full_unpack_arguments &&
            a_value_funccall->called_func->value_type->is_variadic_function_type)
            compiler->mov(reg(reg::tc), imm(arg_list.size() + extern_unpack_arg_count));

        opnumbase* reg_for_current_funccall_argc = nullptr;
        if (full_unpack_arguments)
        {
            reg_for_current_funccall_argc = &get_useable_register_for_pure_value();
            compiler->mov(*reg_for_current_funccall_argc, reg(reg::tc));
        }

        if (funcdef != nullptr
            && funcdef->externed_func_info != nullptr)
        {
            if (funcdef->externed_func_info->is_slow_leaving_call == false)
                compiler->callfast((void*)funcdef->externed_func_info->externed_func);
            else
                compiler->call((void*)funcdef->externed_func_info->externed_func);
        }
        else
            compiler->call(complete_using_register(*called_func_aim));

        opnum::opnumbase* result_storage_place = nullptr;

        if (full_unpack_arguments)
        {
            set_cr_modified();

            result_storage_place = &get_useable_register_for_pure_value();
            compiler->mov(*result_storage_place, reg(reg::cr));

            wo_assert(reg_for_current_funccall_argc);
            auto pop_end = compiler->get_unique_tag_based_command_ip() + "_pop_end";
            auto pop_head = compiler->get_unique_tag_based_command_ip() + "_pop_head";

            // TODO: using duff's device to optmise
            if (arg_list.size() + extern_unpack_arg_count - 1)
            {
                compiler->pop(arg_list.size() + extern_unpack_arg_count - 1);
                compiler->subi(*reg_for_current_funccall_argc, imm(arg_list.size() + extern_unpack_arg_count - 1));
            }
            compiler->tag(pop_head);
            compiler->gti(*reg_for_current_funccall_argc, imm(0));
            compiler->jf(tag(pop_end));
            compiler->pop(1);
            compiler->subi(*reg_for_current_funccall_argc, imm(1));
            compiler->jmp(tag(pop_head));
            compiler->tag(pop_end);
        }
        else
        {
            set_last_value_also_store_to_cr();

            result_storage_place = &WO_NEW_OPNUM(reg(reg::cr));
            compiler->pop(arg_list.size() + extern_unpack_arg_count);
        }

        if (now_function_in_final_anylize && now_function_in_final_anylize->value_type->is_variadic_function_type)
            compiler->pop(reg(reg::tc));

        if (!get_pure_value)
        {
            return *result_storage_place;
        }
        else
        {
            if (full_unpack_arguments)
                return *result_storage_place;

            auto& funcresult = get_useable_register_for_pure_value();
            compiler->mov(funcresult, *result_storage_place);
            return funcresult;
        }
    }

    WO_VALUE_PASS(ast_value_logical_binary)
    {
        auto* a_value_logical_binary = WO_AST();

        if (a_value_logical_binary->overrided_operation_call)
            return analyze_value(a_value_logical_binary->overrided_operation_call, compiler, get_pure_value);

        value::valuetype optype = value::valuetype::invalid;
        if (a_value_logical_binary->left->value_type->is_same(a_value_logical_binary->right->value_type, true))
            optype = a_value_logical_binary->left->value_type->value_type;

        if (!a_value_logical_binary->left->value_type->is_same(a_value_logical_binary->right->value_type, true))
        {
            if (!((a_value_logical_binary->left->value_type->is_integer() ||
                a_value_logical_binary->left->value_type->is_real()) &&
                (a_value_logical_binary->right->value_type->is_integer() ||
                    a_value_logical_binary->right->value_type->is_real())))
                lang_anylizer->lang_error(lexer::errorlevel::error, a_value_logical_binary, WO_ERR_CANNOT_CALC_WITH_L_AND_R,
                    a_value_logical_binary->left->value_type->get_type_name(false).c_str(),
                    a_value_logical_binary->right->value_type->get_type_name(false).c_str());
        }
        if (a_value_logical_binary->operate == lex_type::l_equal || a_value_logical_binary->operate == lex_type::l_not_equal)
        {
            if (a_value_logical_binary->left->value_type->is_union())
            {
                lang_anylizer->lang_error(lexer::errorlevel::error, a_value_logical_binary->left, WO_ERR_RELATION_CANNOT_COMPARE,
                    lexer::lex_is_operate_type(a_value_logical_binary->operate), L"union");
            }
            if (a_value_logical_binary->right->value_type->is_union())
            {
                lang_anylizer->lang_error(lexer::errorlevel::error, a_value_logical_binary->right, WO_ERR_RELATION_CANNOT_COMPARE,
                    lexer::lex_is_operate_type(a_value_logical_binary->operate), L"union");
            }
        }
        if (a_value_logical_binary->operate == lex_type::l_larg
            || a_value_logical_binary->operate == lex_type::l_larg_or_equal
            || a_value_logical_binary->operate == lex_type::l_less
            || a_value_logical_binary->operate == lex_type::l_less_or_equal)
        {
            if (!(a_value_logical_binary->left->value_type->is_integer()
                || a_value_logical_binary->left->value_type->is_real()
                || a_value_logical_binary->left->value_type->is_string()))
            {
                lang_anylizer->lang_error(lexer::errorlevel::error, a_value_logical_binary->left, WO_ERR_RELATION_CANNOT_COMPARE,
                    lexer::lex_is_operate_type(a_value_logical_binary->operate),
                    a_value_logical_binary->left->value_type->get_type_name(false).c_str());
            }
            if (!(a_value_logical_binary->right->value_type->is_integer()
                || a_value_logical_binary->right->value_type->is_real()
                || a_value_logical_binary->right->value_type->is_string()))
            {
                lang_anylizer->lang_error(lexer::errorlevel::error, a_value_logical_binary->right, WO_ERR_RELATION_CANNOT_COMPARE,
                    lexer::lex_is_operate_type(a_value_logical_binary->operate),
                    a_value_logical_binary->right->value_type->get_type_name(false).c_str());
            }
        }

        if (a_value_logical_binary->operate == lex_type::l_lor ||
            a_value_logical_binary->operate == lex_type::l_land)
        {
            if (!a_value_logical_binary->left->value_type->is_bool())
                lang_anylizer->lang_error(lexer::errorlevel::error, a_value_logical_binary->left, WO_ERR_SHOULD_BE_TYPE_BUT_GET_UNEXCEPTED_TYPE, L"bool",
                    a_value_logical_binary->left->value_type->get_type_name(false).c_str());
            if (!a_value_logical_binary->right->value_type->is_bool())
                lang_anylizer->lang_error(lexer::errorlevel::error, a_value_logical_binary->right, WO_ERR_SHOULD_BE_TYPE_BUT_GET_UNEXCEPTED_TYPE, L"bool",
                    a_value_logical_binary->right->value_type->get_type_name(false).c_str());
        }

        size_t revert_pos = compiler->get_now_ip();

        bool eval_right_cr_modified = false;

        auto* _beoped_left_opnum = &analyze_value(a_value_logical_binary->left, compiler);
        auto* _op_right_opnum = &analyze_value_and_get_cr_modified(a_value_logical_binary->right, compiler, false, &eval_right_cr_modified);

        if ((is_cr_reg(*_op_right_opnum) || is_temp_reg(*_op_right_opnum) || eval_right_cr_modified) &&
            (a_value_logical_binary->operate == lex_type::l_lor ||
                a_value_logical_binary->operate == lex_type::l_land))
        {
            // Need make short cut
            complete_using_register(*_beoped_left_opnum);
            complete_using_register(*_op_right_opnum);
            compiler->revert_code_to(revert_pos);

            auto logic_short_cut_label = "logic_short_cut_" + compiler->get_unique_tag_based_command_ip();

            mov_value_to_cr(analyze_value(a_value_logical_binary->left, compiler), compiler);

            if (a_value_logical_binary->operate == lex_type::l_lor)
                compiler->jt(tag(logic_short_cut_label));
            else  if (a_value_logical_binary->operate == lex_type::l_land)
                compiler->jf(tag(logic_short_cut_label));
            else
                wo_error("Unknown operator.");

            mov_value_to_cr(analyze_value(a_value_logical_binary->right, compiler), compiler);

            compiler->tag(logic_short_cut_label);

            set_last_value_also_store_to_cr();

            if (!get_pure_value)
                return WO_NEW_OPNUM(reg(reg::cr));
            else
            {
                auto& result = get_useable_register_for_pure_value();
                compiler->mov(result, reg(reg::cr));
                return result;
            }
        }
        else
        {
            if (eval_right_cr_modified && is_cr_reg(*_beoped_left_opnum))
            {
                complete_using_register(*_beoped_left_opnum);
                complete_using_register(*_op_right_opnum);
                compiler->revert_code_to(revert_pos);

                // TODO: Optimize this, insert a command to generate a new register instead of regenerate.
                _beoped_left_opnum = &analyze_value(a_value_logical_binary->left, compiler, true);
                _op_right_opnum = &analyze_value(a_value_logical_binary->right, compiler);
            }

            auto& beoped_left_opnum = *_beoped_left_opnum;
            auto& op_right_opnum = *_op_right_opnum;

            switch (a_value_logical_binary->operate)
            {
            case lex_type::l_equal:
                if ((a_value_logical_binary->left->value_type->is_integer() && a_value_logical_binary->right->value_type->is_integer())
                    || (a_value_logical_binary->left->value_type->is_handle() && a_value_logical_binary->right->value_type->is_handle()))
                    compiler->equb(beoped_left_opnum, op_right_opnum);
                else if (a_value_logical_binary->left->value_type->is_real() && a_value_logical_binary->right->value_type->is_real())
                    compiler->equr(beoped_left_opnum, op_right_opnum);
                else if (a_value_logical_binary->left->value_type->is_string() && a_value_logical_binary->right->value_type->is_string())
                    compiler->equs(beoped_left_opnum, op_right_opnum);
                else
                    compiler->equb(beoped_left_opnum, op_right_opnum);
                break;
            case lex_type::l_not_equal:
                if ((a_value_logical_binary->left->value_type->is_integer() && a_value_logical_binary->right->value_type->is_integer())
                    || (a_value_logical_binary->left->value_type->is_handle() && a_value_logical_binary->right->value_type->is_handle()))
                    compiler->nequb(beoped_left_opnum, op_right_opnum);
                else if (a_value_logical_binary->left->value_type->is_real() && a_value_logical_binary->right->value_type->is_real())
                    compiler->nequr(beoped_left_opnum, op_right_opnum);
                else if (a_value_logical_binary->left->value_type->is_string() && a_value_logical_binary->right->value_type->is_string())
                    compiler->nequs(beoped_left_opnum, op_right_opnum);
                else
                    compiler->nequb(beoped_left_opnum, op_right_opnum);
                break;
            case lex_type::l_less:
                if (optype == value::valuetype::integer_type)
                    compiler->lti(beoped_left_opnum, op_right_opnum);
                else if (optype == value::valuetype::real_type)
                    compiler->ltr(beoped_left_opnum, op_right_opnum);
                else
                    compiler->ltx(beoped_left_opnum, op_right_opnum);
                break;
            case lex_type::l_less_or_equal:
                if (optype == value::valuetype::integer_type)
                    compiler->elti(beoped_left_opnum, op_right_opnum);
                else if (optype == value::valuetype::real_type)
                    compiler->eltr(beoped_left_opnum, op_right_opnum);
                else
                    compiler->eltx(beoped_left_opnum, op_right_opnum);
                break;
            case lex_type::l_larg:
                if (optype == value::valuetype::integer_type)
                    compiler->gti(beoped_left_opnum, op_right_opnum);
                else if (optype == value::valuetype::real_type)
                    compiler->gtr(beoped_left_opnum, op_right_opnum);
                else
                    compiler->gtx(beoped_left_opnum, op_right_opnum);
                break;
            case lex_type::l_larg_or_equal:
                if (optype == value::valuetype::integer_type)
                    compiler->egti(beoped_left_opnum, op_right_opnum);
                else if (optype == value::valuetype::real_type)
                    compiler->egtr(beoped_left_opnum, op_right_opnum);
                else
                    compiler->egtx(beoped_left_opnum, op_right_opnum);
                break;
            case lex_type::l_land:
                compiler->land(beoped_left_opnum, op_right_opnum);
                break;
            case lex_type::l_lor:
                compiler->lor(beoped_left_opnum, op_right_opnum);
                break;
            default:
                wo_error("Do not support this operator..");
                break;
            }

            complete_using_register(op_right_opnum);

            set_last_value_also_store_to_cr();

            if (!get_pure_value)
                return WO_NEW_OPNUM(reg(reg::cr));
            else
            {
                auto& result = get_useable_register_for_pure_value();
                compiler->mov(result, reg(reg::cr));
                return result;
            }
        }
    }

    WO_VALUE_PASS(ast_value_array)
    {
        auto* a_value_array = WO_AST();

        auto* _arr_item = a_value_array->array_items->children;
        uint16_t arr_item_count = 0;

        while (_arr_item)
        {
            auto* arr_val = dynamic_cast<ast_value*>(_arr_item);
            wo_test(arr_val);

            compiler->psh(complete_using_register(analyze_value(arr_val, compiler)));
            ++arr_item_count;

            _arr_item = _arr_item->sibling;
        }

        auto& treg = get_useable_register_for_pure_value();
        compiler->mkarr(treg, arr_item_count);
        return treg;

    }
    WO_VALUE_PASS(ast_value_mapping)
    {
        auto* a_value_mapping = WO_AST();

        auto* _map_item = a_value_mapping->mapping_pairs->children;
        size_t map_pair_count = 0;
        while (_map_item)
        {
            auto* _map_pair = dynamic_cast<ast_mapping_pair*>(_map_item);
            wo_test(_map_pair);

            compiler->psh(complete_using_register(analyze_value(_map_pair->key, compiler)));
            compiler->psh(complete_using_register(analyze_value(_map_pair->val, compiler)));

            _map_item = _map_item->sibling;
            map_pair_count++;
        }

        auto& treg = get_useable_register_for_pure_value();
        wo_assert(map_pair_count <= UINT16_MAX);
        compiler->mkmap(treg, (uint16_t)map_pair_count);
        return treg;

    }

    opnum::opnumbase& lang::do_index_value_impl(
        ast::ast_value_index* a_value_index,
        ir_compiler* compiler,
        bool get_pure_value,
        bool get_pure_tmp_for_assign,
        opnum::opnumbase** out_index_from,
        opnum::opnumbase** out_index_key)
    {
        if (a_value_index->from->value_type->is_struct() || a_value_index->from->value_type->is_tuple())
        {
            wo_assert(a_value_index->struct_offset != 0xFFFF);
            auto* _beoped_left_opnum = &analyze_value(a_value_index->from, compiler, get_pure_tmp_for_assign);

            auto& result = get_useable_register_for_pure_value();

            compiler->idstruct(result, *_beoped_left_opnum, a_value_index->struct_offset);

            *out_index_from = _beoped_left_opnum;
            *out_index_key = nullptr;

            return result;
        }
        else
        {
            size_t revert_pos = compiler->get_now_ip();

            bool eval_right_cr_modified = false;

            auto* _beoped_left_opnum = &analyze_value(a_value_index->from, compiler, get_pure_tmp_for_assign);
            auto* _op_right_opnum = &analyze_value_and_get_cr_modified(a_value_index->index, compiler, get_pure_tmp_for_assign, &eval_right_cr_modified);

            if (get_pure_tmp_for_assign)
            {
                // Do nothing
            }
            else if (eval_right_cr_modified && is_cr_reg(*_beoped_left_opnum))
            {
                complete_using_register(*_beoped_left_opnum);
                complete_using_register(*_op_right_opnum);
                compiler->revert_code_to(revert_pos);

                // TODO: Optimize this, insert a command to generate a new register instead of regenerate.
                _beoped_left_opnum = &analyze_value(a_value_index->from, compiler, true);
                _op_right_opnum = &analyze_value(a_value_index->index, compiler);
            }

            if (a_value_index->from->value_type->is_array() || a_value_index->from->value_type->is_vec())
                compiler->idarr(*_beoped_left_opnum, *_op_right_opnum);
            else if (a_value_index->from->value_type->is_dict() || a_value_index->from->value_type->is_map())
            {
                compiler->iddict(*_beoped_left_opnum, *_op_right_opnum);
            }
            else if (a_value_index->from->value_type->is_string())
                compiler->idstr(*_beoped_left_opnum, *_op_right_opnum);
            else
                wo_error("Unknown index operation.");

            *out_index_from = _beoped_left_opnum;
            *out_index_key = _op_right_opnum;

            set_last_value_also_store_to_cr();

            if (!get_pure_value)
                return WO_NEW_OPNUM(reg(reg::cr));
            else
            {
                auto& result = get_useable_register_for_pure_value();
                compiler->mov(result, reg(reg::cr));
                return result;
            }
        }
    }

    WO_VALUE_PASS(ast_value_index)
    {
        auto* a_value_index = WO_AST();

        opnum::opnumbase* _index_from = nullptr;
        opnum::opnumbase* _index_key = nullptr;
        auto& result = do_index_value_impl(a_value_index, compiler, get_pure_value, false, &_index_from, &_index_key);

        if (_index_from != nullptr)
            complete_using_register(*_index_from);
        if (_index_key != nullptr)
            complete_using_register(*_index_key);

        return result;
    }
    WO_VALUE_PASS(ast_value_packed_variadic_args)
    {
        auto* a_value_packed_variadic_args = WO_AST();

        if (!now_function_in_final_anylize
            || !now_function_in_final_anylize->value_type->is_variadic_function_type)
        {
            lang_anylizer->lang_error(lexer::errorlevel::error, a_value_packed_variadic_args, WO_ERR_USING_VARIADIC_IN_NON_VRIDIC_FUNC);
            return WO_NEW_OPNUM(reg(reg::ni));
        }
        else
        {
            auto& packed = get_useable_register_for_pure_value();

            compiler->ext_packargs(packed,
                (uint16_t)now_function_in_final_anylize->value_type->argument_types.size(),
                (uint16_t)now_function_in_final_anylize->capture_variables.size());
            return packed;
        }
    }
    WO_VALUE_PASS(ast_value_indexed_variadic_args)
    {
        auto* a_value_indexed_variadic_args = WO_AST();

        if (!now_function_in_final_anylize
            || !now_function_in_final_anylize->value_type->is_variadic_function_type)
        {
            lang_anylizer->lang_error(lexer::errorlevel::error, a_value_indexed_variadic_args, WO_ERR_USING_VARIADIC_IN_NON_VRIDIC_FUNC);
            return WO_NEW_OPNUM(reg(reg::ni));
        }

        auto capture_count = (uint16_t)now_function_in_final_anylize->capture_variables.size();
        auto function_arg_count = now_function_in_final_anylize->value_type->argument_types.size();

        if (a_value_indexed_variadic_args->argindex->is_constant)
        {
            const auto& _cv = a_value_indexed_variadic_args->argindex->get_constant_value();
            if (_cv.integer + capture_count + function_arg_count <= 63 - 2)
            {
                if (!get_pure_value)
                    return WO_NEW_OPNUM(reg(reg::bp_offset((int8_t)(_cv.integer + capture_count + 2
                        + function_arg_count))));
                else
                {
                    auto& result = get_useable_register_for_pure_value();

                    compiler->mov(result, reg(reg::bp_offset((int8_t)(_cv.integer + capture_count + 2
                        + function_arg_count))));

                    return result;
                }
            }
            else
            {
                auto& result = get_useable_register_for_pure_value();
                compiler->lds(result, imm(_cv.integer + capture_count + 2
                    + function_arg_count));
                return result;
            }
        }
        else
        {
            auto& index = analyze_value(a_value_indexed_variadic_args->argindex, compiler, true);
            compiler->addi(index, imm(2 + capture_count + function_arg_count));
            complete_using_register(index);
            auto& result = get_useable_register_for_pure_value();
            compiler->lds(result, index);
            return result;
        }
    }
    WO_VALUE_PASS(ast_fakevalue_unpacked_args)
    {
        auto* a_fakevalue_unpacked_args = WO_AST();
        lang_anylizer->lang_error(lexer::errorlevel::error, a_fakevalue_unpacked_args, 
            WO_ERR_UNPACK_ARGS_OUT_OF_FUNC_CALL,
            a_fakevalue_unpacked_args->unpacked_pack->value_type->get_type_name(false).c_str());
        return WO_NEW_OPNUM(reg(reg::ni));
    }
    WO_VALUE_PASS(ast_value_unary)
    {
        auto* a_value_unary = WO_AST();
        switch (a_value_unary->operate)
        {
        case lex_type::l_lnot:
            if (!a_value_unary->val->value_type->is_bool())
                lang_anylizer->lang_error(lexer::errorlevel::error, a_value_unary->val, WO_ERR_SHOULD_BE_TYPE_BUT_GET_UNEXCEPTED_TYPE,
                    L"bool",
                    a_value_unary->val->value_type->get_type_name(false).c_str());
            compiler->equb(analyze_value(a_value_unary->val, compiler), reg(reg::ni));
            break;
        case lex_type::l_sub:
            if (a_value_unary->val->value_type->is_integer())
            {
                auto& result = analyze_value(a_value_unary->val, compiler, true);
                compiler->mov(reg(reg::cr), imm(0));
                compiler->subi(reg(reg::cr), result);
                complete_using_register(result);
            }
            else if (a_value_unary->val->value_type->is_real())
            {
                auto& result = analyze_value(a_value_unary->val, compiler, true);
                compiler->mov(reg(reg::cr), imm(0.));
                compiler->subr(reg(reg::cr), result);
                complete_using_register(result);
            }
            else
                lang_anylizer->lang_error(lexer::errorlevel::error,
                    a_value_unary, WO_ERR_TYPE_CANNOT_NEGATIVE, a_value_unary->val->value_type->get_type_name().c_str());
            break;
        default:
            wo_error("Do not support this operator..");
            break;
        }

        set_last_value_also_store_to_cr();

        if (!get_pure_value)
            return WO_NEW_OPNUM(reg(reg::cr));
        else
        {
            auto& result = get_useable_register_for_pure_value();
            compiler->mov(result, reg(reg::cr));
            return result;
        }
    }
    WO_VALUE_PASS(ast_value_takeplace)
    {
        auto* a_value_takeplace = WO_AST();
        if (nullptr == a_value_takeplace->used_reg)
        {
            if (!get_pure_value)
                return WO_NEW_OPNUM(reg(reg::ni));
            else
                return get_useable_register_for_pure_value();
        }
        else
        {
            if (!get_pure_value)
                return *a_value_takeplace->used_reg;
            else
            {
                auto& result = get_useable_register_for_pure_value();
                compiler->mov(result, *a_value_takeplace->used_reg);
                return result;
            }
        }
    }
    WO_VALUE_PASS(ast_value_make_struct_instance)
    {
        auto* a_value_make_struct_instance = WO_AST();

        std::map<uint16_t, ast_value*> memb_values;
        auto* init_mem_val_pair = a_value_make_struct_instance->struct_member_vals->children;
        while (init_mem_val_pair)
        {
            auto* membpair = dynamic_cast<ast_struct_member_define*>(init_mem_val_pair);
            wo_assert(membpair && membpair->is_value_pair);
            init_mem_val_pair = init_mem_val_pair->sibling;

            memb_values[membpair->member_offset] = membpair->member_value_pair;
        }

        for (auto index = memb_values.begin(); index != memb_values.end(); ++index)
        {
            compiler->psh(complete_using_register(analyze_value(index->second, compiler)));
        }

        auto& result = get_useable_register_for_pure_value();
        compiler->mkstruct(result, (uint16_t)memb_values.size());

        return result;
    }
    WO_VALUE_PASS(ast_value_make_tuple_instance)
    {
        auto* a_value_make_tuple_instance = WO_AST();

        auto* tuple_elems = a_value_make_tuple_instance->tuple_member_vals->children;

        uint16_t tuple_count = 0;
        while (tuple_elems)
        {
            ast_value* val = dynamic_cast<ast_value*>(tuple_elems);
            wo_assert(val);

            
            if (auto* unpacks = dynamic_cast<ast_fakevalue_unpacked_args*>(val))
            {
                wo_assert(unpacks->unpacked_pack->value_type->is_tuple());
                wo_assert(unpacks->expand_count >= 0);

                auto& unpacking_tuple = complete_using_register(
                    analyze_value(unpacks->unpacked_pack, compiler));

                size_t ecount = std::min(
                    (size_t)unpacks->expand_count,
                    unpacks->unpacked_pack->value_type->template_arguments.size());
                
                tuple_count += (uint16_t)ecount;
                for (size_t i = 0; i < ecount; ++i)
                {
                    compiler->idstruct(reg(reg::tp), unpacking_tuple, (uint16_t)i);
                    compiler->psh(reg(reg::tp));
                }
                // compiler->idstruct(unpacking_tuple, )
            }
            else
            {
                ++tuple_count;
                compiler->psh(complete_using_register(analyze_value(val, compiler)));
            }

            tuple_elems = tuple_elems->sibling;
        }

        auto& result = get_useable_register_for_pure_value();
        compiler->mkstruct(result, tuple_count);

        return result;
    }
    WO_VALUE_PASS(ast_value_trib_expr)
    {
        auto* a_value_trib_expr = WO_AST();

        if (a_value_trib_expr->judge_expr->is_constant)
        {
            if (a_value_trib_expr->judge_expr->get_constant_value().integer)
                return analyze_value(a_value_trib_expr->val_if_true, compiler, get_pure_value);
            else
                return analyze_value(a_value_trib_expr->val_or, compiler, get_pure_value);
        }
        else
        {
            auto trib_expr_else = "trib_expr_else" + compiler->get_unique_tag_based_command_ip();
            auto trib_expr_end = "trib_expr_end" + compiler->get_unique_tag_based_command_ip();

            mov_value_to_cr(complete_using_register(analyze_value(a_value_trib_expr->judge_expr, compiler, false)), compiler);
            compiler->jf(tag(trib_expr_else));
            mov_value_to_cr(complete_using_register(analyze_value(a_value_trib_expr->val_if_true, compiler, false)), compiler);
            compiler->jmp(tag(trib_expr_end));
            compiler->tag(trib_expr_else);
            mov_value_to_cr(complete_using_register(analyze_value(a_value_trib_expr->val_or, compiler, false)), compiler);
            compiler->tag(trib_expr_end);

            set_last_value_also_store_to_cr();

            if (!get_pure_value)
                return WO_NEW_OPNUM(reg(reg::cr));
            else
            {
                auto& result = get_useable_register_for_pure_value();
                compiler->mov(result, reg(reg::cr));
                return result;
            }
        }
    }
}