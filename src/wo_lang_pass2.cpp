#include "wo_lang.hpp"

namespace wo
{
    using namespace ast;

#define WO_AST() astnode; wo_assert(astnode != nullptr)

#define WO_PASS2(NODETYPE) void lang::pass2_##NODETYPE(ast::NODETYPE* astnode)

    WO_PASS2(ast_mapping_pair)
    {
        auto* a_mapping_pair = WO_AST();
        analyze_pass2(a_mapping_pair->key);
        analyze_pass2(a_mapping_pair->val);
    }
    WO_PASS2(ast_using_type_as)
    {
        auto* a_using_type_as = WO_AST();

        if (a_using_type_as->namespace_decl != nullptr)
            analyze_pass2(a_using_type_as->namespace_decl);
    }
    WO_PASS2(ast_return)
    {
        auto* a_ret = WO_AST();
        if (a_ret->return_value)
            analyze_pass2(a_ret->return_value);

        if (a_ret->located_function != nullptr)
        {
            if (a_ret->return_value)
            {
                if (a_ret->return_value->value_type->is_pending())
                {
                    // error will report in analyze_pass2(a_ret->return_value), so here do nothing.. 
                    a_ret->located_function->value_type->set_ret_type(a_ret->return_value->value_type);
                    a_ret->located_function->auto_adjust_return_type = false;
                }
                else
                {
                    auto* func_return_type = a_ret->located_function->value_type->get_return_type();
                    if (a_ret->located_function->auto_adjust_return_type)
                    {
                        if (func_return_type->is_pending())
                        {
                            a_ret->located_function->value_type->set_ret_type(a_ret->return_value->value_type);
                        }
                        else
                        {
                            if (!func_return_type->accept_type(a_ret->return_value->value_type, false, false))
                            {
                                if (!func_return_type->set_mix_types(a_ret->return_value->value_type, false))
                                {
                                    lang_anylizer->lang_error(lexer::errorlevel::error, a_ret,
                                        WO_ERR_FUNC_RETURN_DIFFERENT_TYPES,
                                        func_return_type->get_type_name(false).c_str(),
                                        a_ret->return_value->value_type->get_type_name(false).c_str());
                                }
                            }
                        }
                    }
                    else
                    {
                        if (!func_return_type->is_pending()
                            && !func_return_type->accept_type(a_ret->return_value->value_type, false, true))
                        {
                            lang_anylizer->lang_error(lexer::errorlevel::error, a_ret,
                                WO_ERR_FUNC_RETURN_DIFFERENT_TYPES,
                                func_return_type->get_type_name(false).c_str(),
                                a_ret->return_value->value_type->get_type_name(false).c_str());
                        }
                    }
                }
            }
            else
            {
                auto* func_return_type = a_ret->located_function->value_type->get_return_type();
                if (a_ret->located_function->auto_adjust_return_type)
                {
                    if (a_ret->located_function->value_type->is_pending())
                    {
                        func_return_type->set_type_with_name(WO_PSTR(void));
                        a_ret->located_function->auto_adjust_return_type = false;
                    }
                    else
                    {
                        lang_anylizer->lang_error(lexer::errorlevel::error, a_ret,
                            WO_ERR_CANNOT_RET_TYPE_AND_TYPE_AT_SAME_TIME,
                            L"void", func_return_type->get_type_name(false, false).c_str());
                    }
                }
                else
                {
                    if (!func_return_type->is_void())
                        lang_anylizer->lang_error(lexer::errorlevel::error, a_ret,
                            WO_ERR_CANNOT_RET_TYPE_AND_TYPE_AT_SAME_TIME,
                            L"void", func_return_type->get_type_name(false, false).c_str());
                }
            }
        }
    }
    WO_PASS2(ast_sentence_block)
    {
        auto* a_sentence_blk = WO_AST();
        analyze_pass2(a_sentence_blk->sentence_list);
    }
    WO_PASS2(ast_if)
    {
        auto* ast_if_sentence = WO_AST();
        analyze_pass2(ast_if_sentence->judgement_value);

        if (ast_if_sentence->judgement_value->is_constant)
        {
            ast_if_sentence->is_constexpr_if = true;

            if (ast_if_sentence->judgement_value->get_constant_value().integer)
                analyze_pass2(ast_if_sentence->execute_if_true);
            else if (ast_if_sentence->execute_else)
                analyze_pass2(ast_if_sentence->execute_else);
        }
        else
        {
            analyze_pass2(ast_if_sentence->execute_if_true);
            if (ast_if_sentence->execute_else)
                analyze_pass2(ast_if_sentence->execute_else);
        }
    }
    WO_PASS2(ast_value_mutable)
    {
        auto* a_value_mutable_or_pure = WO_AST();

        analyze_pass2(a_value_mutable_or_pure->val);

        if (a_value_mutable_or_pure->val->value_type->is_pending() == false)
        {
            a_value_mutable_or_pure->value_type->set_type(a_value_mutable_or_pure->val->value_type);

            if (a_value_mutable_or_pure->mark_type == lex_type::l_mut)
                a_value_mutable_or_pure->value_type->set_is_mutable(true);
            else
            {
                wo_assert(a_value_mutable_or_pure->mark_type == lex_type::l_immut);
                a_value_mutable_or_pure->value_type->set_is_force_immutable();
            }
        }
    }
    WO_PASS2(ast_while)
    {
        auto* ast_while_sentence = WO_AST();
        if (ast_while_sentence->judgement_value != nullptr)
            analyze_pass2(ast_while_sentence->judgement_value);
        analyze_pass2(ast_while_sentence->execute_sentence);
    }
    WO_PASS2(ast_forloop)
    {
        auto* a_forloop = WO_AST();
        if (a_forloop->pre_execute)
            analyze_pass2(a_forloop->pre_execute);
        if (a_forloop->judgement_expr)
            analyze_pass2(a_forloop->judgement_expr);

        analyze_pass2(a_forloop->execute_sentences);

        if (a_forloop->after_execute)
            analyze_pass2(a_forloop->after_execute);
    }
    WO_PASS2(ast_foreach)
    {
        auto* a_foreach = WO_AST();

        wo_assert((a_foreach->marking_label == a_foreach->loop_sentences->marking_label));

        analyze_pass2(a_foreach->used_iter_define);
        analyze_pass2(a_foreach->loop_sentences);
    }
    WO_PASS2(ast_varref_defines)
    {
        auto* a_varref_defs = WO_AST();
        for (auto& varref : a_varref_defs->var_refs)
        {
            analyze_pattern_in_pass2(varref.pattern, varref.init_val);
        }
    }
    WO_PASS2(ast_union_make_option_ob_to_cr_and_ret)
    {
        auto* a_union_make_option_ob_to_cr_and_ret = WO_AST();
        if (a_union_make_option_ob_to_cr_and_ret->argument_may_nil)
            analyze_pass2(a_union_make_option_ob_to_cr_and_ret->argument_may_nil);
    }
    WO_PASS2(ast_match)
    {
        auto* a_match = WO_AST();
        analyze_pass2(a_match->match_value);
        if (!a_match->has_using_namespace)
        {
            if (a_match->match_value->value_type->is_union()
                && !a_match->match_value->value_type->is_pending()
                && a_match->match_value->value_type->using_type_name)
            {
                wo_assert(a_match->match_value->value_type->using_type_name->symbol);

                ast_using_namespace* ast_using = new ast_using_namespace;
                ast_using->used_namespace_chain = a_match->match_value->value_type->using_type_name->scope_namespaces;
                ast_using->used_namespace_chain.push_back(a_match->match_value->value_type->using_type_name->type_name);
                ast_using->from_global_namespace = true;
                ast_using->copy_source_info(a_match);
                a_match->match_scope_in_pass->used_namespace.push_back(ast_using);
                a_match->has_using_namespace = true;
            }
            else
            {
                lang_anylizer->lang_error(lexer::errorlevel::error, a_match->match_value,
                    WO_ERR_CANNOT_MATCH_SUCH_TYPE,
                    a_match->match_value->value_type->get_type_name(false).c_str());
            }
        }

        analyze_pass2(a_match->cases);

        // Must walk all possiable case, and no repeat case!
        std::set<wo_pstring_t> case_names;
        auto* cases = a_match->cases->children;
        bool has_default_pattern = false;
        while (cases)
        {
            auto* case_ast = dynamic_cast<ast_match_union_case*>(cases);
            wo_assert(case_ast);

            if (has_default_pattern)
                lang_anylizer->lang_error(lexer::errorlevel::error, case_ast->union_pattern,
                    WO_ERR_CASE_AFTER_DEFAULT_PATTERN);

            if (case_ast->union_pattern->union_expr == nullptr)
            {
                if (case_names.size() >= a_match->match_value->value_type->struct_member_index.size())
                    lang_anylizer->lang_error(lexer::errorlevel::error, case_ast->union_pattern,
                        WO_ERR_USELESS_DEFAULT_PATTERN);

                has_default_pattern = true;
            }
            else if (case_names.end() != case_names.find(case_ast->union_pattern->union_expr->var_name))
                lang_anylizer->lang_error(lexer::errorlevel::error, case_ast->union_pattern->union_expr,
                    WO_ERR_REPEAT_MATCH_CASE);
            else
                case_names.insert(case_ast->union_pattern->union_expr->var_name);
            cases = cases->sibling;
        }
        if (!has_default_pattern && case_names.size() < a_match->match_value->value_type->struct_member_index.size())
            lang_anylizer->lang_error(lexer::errorlevel::error, a_match, WO_ERR_MATCH_CASE_NOT_COMPLETE);
    }
    WO_PASS2(ast_match_union_case)
    {
        auto* a_match_union_case = WO_AST();
        wo_assert(a_match_union_case->in_match);
        if (a_match_union_case->in_match && a_match_union_case->in_match->match_value->value_type->is_union())
        {
            if (ast_pattern_union_value* a_pattern_union_value =
                dynamic_cast<ast_pattern_union_value*>(a_match_union_case->union_pattern))
            {
                if (a_match_union_case->in_match->match_value->value_type->is_pending())
                    lang_anylizer->lang_error(lexer::errorlevel::error, a_match_union_case,
                        WO_ERR_UNKNOWN_MATCHING_VAL_TYPE);
                else
                {
                    if (!a_match_union_case->in_match->match_value->value_type->using_type_name->template_arguments.empty())
                    {
                        if (a_pattern_union_value->union_expr == nullptr)
                            ;
                        else
                        {
                            a_pattern_union_value->union_expr->symbol =
                                find_value_in_this_scope(a_pattern_union_value->union_expr);

                            if (a_pattern_union_value->union_expr->symbol)
                            {
                                std::vector<ast_type*> fact_used_template;

                                auto fnd = a_match_union_case->in_match->match_value->value_type->struct_member_index.find(
                                    a_pattern_union_value->union_expr->symbol->name);
                                if (fnd == a_match_union_case->in_match->match_value->value_type->struct_member_index.end())
                                {
                                    lang_anylizer->lang_error(lexer::errorlevel::error, a_pattern_union_value, WO_ERR_INVALID_ITEM_OF,
                                        a_pattern_union_value->union_expr->symbol->name->c_str(),
                                        a_match_union_case->in_match->match_value->value_type->get_type_name(false).c_str());
                                }
                                else
                                    for (auto id : fnd->second.union_used_template_index)
                                    {
                                        wo_assert(id < a_match_union_case->in_match->match_value->value_type->using_type_name->template_arguments.size());
                                        fact_used_template.push_back(a_match_union_case->in_match->match_value->value_type->using_type_name->template_arguments[id]);
                                    }

                                if (!fact_used_template.empty())
                                {
                                    a_pattern_union_value->union_expr->template_reification_args = fact_used_template;
                                    if (a_pattern_union_value->union_expr->symbol->type == lang_symbol::symbol_type::variable)
                                        a_pattern_union_value->union_expr->symbol = analyze_pass_template_reification_var(
                                            a_pattern_union_value->union_expr,
                                            fact_used_template);
                                    else
                                    {
                                        wo_assert(a_pattern_union_value->union_expr->symbol->type == lang_symbol::symbol_type::function);

                                        auto final_function = a_pattern_union_value->union_expr->symbol->get_funcdef();

                                        auto* dumped_func = analyze_pass_template_reification_func(
                                            dynamic_cast<ast_value_function_define*>(final_function),
                                            fact_used_template);
                                        if (dumped_func)
                                            a_pattern_union_value->union_expr->symbol = dumped_func->this_reification_lang_symbol;
                                        else
                                            lang_anylizer->lang_error(lexer::errorlevel::error, a_pattern_union_value, WO_ERR_TEMPLATE_ARG_NOT_MATCH);
                                    }
                                }
                            }
                            else
                                ;
                        }
                    }
                    if (a_pattern_union_value->union_expr != nullptr)
                        analyze_pass2(a_pattern_union_value->union_expr);
                }
                if (a_pattern_union_value->pattern_arg_in_union_may_nil)
                {
                    wo_assert(a_match_union_case->take_place_value_may_nil);
                    wo_assert(a_pattern_union_value->union_expr != nullptr);

                    if (a_pattern_union_value->union_expr->value_type->argument_types.size() != 1)
                        lang_anylizer->lang_error(lexer::errorlevel::error, a_match_union_case, WO_ERR_INVALID_CASE_TYPE_NEED_ACCEPT_ARG);
                    else
                    {
                        a_match_union_case->take_place_value_may_nil->value_type->set_type(
                            a_pattern_union_value->union_expr->value_type->argument_types.front());
                    }

                    analyze_pattern_in_pass2(a_pattern_union_value->pattern_arg_in_union_may_nil, a_match_union_case->take_place_value_may_nil);

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
        }

        analyze_pass2(a_match_union_case->in_case_sentence);
    }
    WO_PASS2(ast_struct_member_define)
    {
        auto* a_struct_member_define = WO_AST();

        if (a_struct_member_define->is_value_pair)
            analyze_pass2(a_struct_member_define->member_value_pair);
        else
            fully_update_type(a_struct_member_define->member_type, false);
    }
    WO_PASS2(ast_where_constraint)
    {
        auto* a_where_constraint = WO_AST();

        ast_value* val = dynamic_cast<ast_value*>(a_where_constraint->where_constraint_list->children);
        while (val)
        {
            analyze_pass2(val);

            val->eval_constant_value(lang_anylizer);
            if (!val->is_constant)
            {
                a_where_constraint->unmatched_constraint.push_back(
                    lang_anylizer->make_error(lexer::errorlevel::error, val, WO_ERR_WHERE_COND_SHOULD_BE_CONST));
                a_where_constraint->accept = false;
            }
            else if (!val->value_type->is_bool())
            {
                a_where_constraint->unmatched_constraint.push_back(
                    lang_anylizer->make_error(lexer::errorlevel::error, val, WO_ERR_WHERE_COND_TYPE_ERR
                        , val->value_type->get_type_name(false).c_str()));
                a_where_constraint->accept = false;
            }
            else if (0 == val->get_constant_value().handle)
            {
                a_where_constraint->unmatched_constraint.push_back(
                    lang_anylizer->make_error(lexer::errorlevel::error, val, WO_ERR_WHERE_COND_NOT_MEET));
                a_where_constraint->accept = false;
            }

            val = dynamic_cast<ast_value*>(val->sibling);
        }
    }
    WO_PASS2(ast_value_function_define)
    {
        auto* a_value_funcdef = WO_AST();

        // NOTE: Reset this flag for conditional compilation.
        a_value_funcdef->has_return_value = false;

        if (!a_value_funcdef->is_template_define)
        {
            if (a_value_funcdef->is_template_reification)
            {
                wo_assure(begin_template_scope(a_value_funcdef,
                    a_value_funcdef->this_func_scope,
                    a_value_funcdef->template_type_name_list,
                    a_value_funcdef->this_reification_template_args));
            }

            auto* last_function = this->current_function_in_pass2;
            this->current_function_in_pass2 = a_value_funcdef->this_func_scope;
            wo_assert(this->current_function_in_pass2 != nullptr);

            const size_t anylizer_error_count =
                lang_anylizer->get_cur_error_frame().size();

            if (a_value_funcdef->argument_list)
            {
                // ast_value_function_define might be root-point created in lang.
                auto arg_child = a_value_funcdef->argument_list->children;
                while (arg_child)
                {
                    if (ast_value_arg_define* argdef = dynamic_cast<ast_value_arg_define*>(arg_child))
                    {
                        argdef->completed_in_pass2 = true;
                        if (argdef->symbol)
                            argdef->symbol->has_been_completed_defined = true;
                    }
                    else
                    {
                        wo_assert(dynamic_cast<ast_token*>(arg_child));
                    }

                    arg_child = arg_child->sibling;
                }
            }

            // return-type adjust complete. do 'return' cast;
            if (a_value_funcdef->where_constraint)
                analyze_pass2(a_value_funcdef->where_constraint);

            if (a_value_funcdef->where_constraint == nullptr || a_value_funcdef->where_constraint->accept)
            {
                if (a_value_funcdef->in_function_sentence)
                {
                    analyze_pass2(a_value_funcdef->in_function_sentence);
                }
                if (a_value_funcdef->value_type->get_return_type()->is_pure_pending())
                {
                    // There is no return in function return void
                    if (a_value_funcdef->auto_adjust_return_type)
                    {
                        if (a_value_funcdef->has_return_value)
                            lang_anylizer->lang_error(lexer::errorlevel::error,
                                a_value_funcdef,
                                WO_ERR_CANNOT_DERIV_FUNCS_RET_TYPE,
                                wo::str_to_wstr(a_value_funcdef->get_ir_func_signature_tag()).c_str());

                        a_value_funcdef->value_type->get_return_type()->set_type_with_name(WO_PSTR(void));
                    }
                }
            }
            else
                a_value_funcdef->value_type->get_return_type()->set_type_with_name(WO_PSTR(pending));

            auto& current_error_frame = lang_anylizer->get_cur_error_frame();
            if (current_error_frame.size() != anylizer_error_count)
            {
                wo_assert(current_error_frame.size() > anylizer_error_count);

                if (a_value_funcdef->is_template_reification)
                {
                    if (a_value_funcdef->where_constraint == nullptr)
                    {
                        a_value_funcdef->where_constraint = new ast_where_constraint;
                        a_value_funcdef->where_constraint->copy_source_info(a_value_funcdef);
                        a_value_funcdef->where_constraint->binded_func_define = a_value_funcdef;
                        a_value_funcdef->where_constraint->where_constraint_list = new ast_list;
                    }

                    a_value_funcdef->where_constraint->accept = false;

                    for (size_t i = anylizer_error_count; i < current_error_frame.size(); ++i)
                    {
                        a_value_funcdef->where_constraint->unmatched_constraint.push_back(
                            current_error_frame.at(i));
                    }
                    current_error_frame.resize(anylizer_error_count);
                }

                // Error happend in cur function
                a_value_funcdef->value_type->get_return_type()->set_type_with_name(WO_PSTR(pending));
            }

            this->current_function_in_pass2 = last_function;

            if (a_value_funcdef->is_template_reification)
                end_template_scope(a_value_funcdef->this_func_scope);
            else
            {
                check_function_where_constraint(
                    a_value_funcdef, lang_anylizer, a_value_funcdef);
            }
        }
    }
    WO_PASS2(ast_value_assign)
    {
        auto* a_value_assi = WO_AST();

        bool left_value_is_variable_and_has_been_used = true;
        auto* variable = dynamic_cast<ast_value_variable*>(a_value_assi->left);
        if (variable != nullptr && a_value_assi->is_value_assgin == false)
        {
            auto* symbol = find_value_in_this_scope(variable);
            if (symbol == nullptr || (
                !symbol->is_captured_variable
                && symbol->define_in_function
                && symbol->is_marked_as_used_variable == false
                && (a_value_assi->is_value_assgin == false
                    || a_value_assi->operate == lex_type::l_value_assign
                    || symbol->static_symbol == false)))
                left_value_is_variable_and_has_been_used = false;
        }
        analyze_pass2(a_value_assi->left);
        if (!left_value_is_variable_and_has_been_used && variable->symbol != nullptr)
            variable->symbol->is_marked_as_used_variable = false;

        analyze_pass2(a_value_assi->right);

        if (a_value_assi->left->value_type->is_function())
        {
            auto* symbinfo = dynamic_cast<ast_value_variable*>(a_value_assi->left);

            auto* scope = symbinfo == nullptr || symbinfo->symbol == nullptr
                ? a_value_assi->located_scope
                : symbinfo->symbol->defined_in_scope;

            if (auto right_func_instance = judge_auto_type_of_funcdef_with_type(
                a_value_assi->located_scope,
                a_value_assi->left->value_type, a_value_assi->right, true, nullptr, nullptr))
            {
                if (dynamic_cast<ast::ast_value_mutable*>(a_value_assi->right) == nullptr)
                    a_value_assi->right = std::get<ast::ast_value_function_define*>(right_func_instance.value());
            }
        }

        if (!a_value_assi->left->value_type->accept_type(a_value_assi->right->value_type, false, false))
        {
            lang_anylizer->lang_error(lexer::errorlevel::error, a_value_assi->right, WO_ERR_SHOULD_BE_TYPE_BUT_GET_UNEXCEPTED_TYPE,
                a_value_assi->left->value_type->get_type_name(false).c_str(),
                a_value_assi->right->value_type->get_type_name(false).c_str());
        }

        if (!a_value_assi->left->can_be_assign)
            lang_anylizer->lang_error(lexer::errorlevel::error, a_value_assi->left, WO_ERR_CANNOT_ASSIGN_TO_UNASSABLE_ITEM);

        if (a_value_assi->is_value_assgin)
            a_value_assi->value_type->set_type(a_value_assi->left->value_type);
    }
    WO_PASS2(ast_value_type_cast)
    {
        auto* a_value_typecast = WO_AST();
        // check: cast is valid?
        ast_value* origin_value = a_value_typecast->_be_cast_value_node;
        fully_update_type(a_value_typecast->value_type, false);
        analyze_pass2(origin_value);

        if (a_value_typecast->value_type->using_type_name != nullptr)
        {
            auto* cast_type_symbol = a_value_typecast->value_type->using_type_name->symbol;
            if (cast_type_symbol != nullptr && cast_type_symbol->attribute != nullptr)
            {
                if (!check_symbol_is_accessable(
                    cast_type_symbol,
                    a_value_typecast->located_scope,
                    a_value_typecast,
                    true))
                {
                    lang_anylizer->lang_error(
                        lexer::errorlevel::infom,
                        cast_type_symbol->get_typedef(),
                        WO_INFO_CANNOT_USE_UNREACHABLE_TYPE);
                }
            }
        }

        if (a_value_typecast->value_type->is_pending())
            lang_anylizer->lang_error(lexer::errorlevel::error, a_value_typecast, WO_ERR_UNKNOWN_TYPE,
                a_value_typecast->value_type->get_type_name(false).c_str()
            );

        if (!ast_type::check_castable(a_value_typecast->value_type, origin_value->value_type))
        {
            lang_anylizer->lang_error(lexer::errorlevel::error, a_value_typecast, WO_ERR_CANNOT_CAST_TYPE_TO_TYPE,
                origin_value->value_type->get_type_name(false).c_str(),
                a_value_typecast->value_type->get_type_name(false).c_str()
            );
        }
        else if (a_value_typecast->value_type->using_type_name != nullptr
            && a_value_typecast->value_type->is_struct()
            && !a_value_typecast->value_type->is_same(origin_value->value_type, true))
        {
            // Check if trying cast struct.
            for (auto& [member_name, member_info] : a_value_typecast->value_type->struct_member_index)
            {
                wo_assert(member_info.member_decl_attribute != nullptr);
                if (member_info.member_decl_attribute->is_public_attr())
                {
                    // Public, nothing to check.
                }
                else if (member_info.member_decl_attribute->is_protected_attr())
                {
                    if (!a_value_typecast->located_scope->belongs_to(
                        a_value_typecast->value_type->searching_begin_namespace_in_pass2))
                    {
                        lang_anylizer->lang_error(lexer::errorlevel::error, a_value_typecast, WO_ERR_UNACCABLE_PROTECTED_MEMBER,
                            a_value_typecast->value_type->get_type_name(false, false).c_str(),
                            member_name->c_str());
                    }
                }
                else // if (member_info.member_decl_attribute->is_private_attr())
                {
                    lang_symbol* struct_symb = a_value_typecast->value_type->using_type_name->symbol;
                    wo_assert(struct_symb != nullptr);
                    wo_assert(struct_symb->type == lang_symbol::symbol_type::typing);

                    if (a_value_typecast->source_file != struct_symb->defined_source())
                    {
                        lang_anylizer->lang_error(lexer::errorlevel::error, a_value_typecast, WO_ERR_UNACCABLE_PRIVATE_MEMBER,
                            a_value_typecast->value_type->get_type_name(false, false).c_str(),
                            member_name->c_str());
                    }
                }
            }
        }
    }
    WO_PASS2(ast_value_type_judge)
    {
        auto* ast_value_judge = WO_AST();
        analyze_pass2(ast_value_judge->_be_cast_value_node);

        if (ast_value_judge->value_type->is_pending())
            lang_anylizer->lang_error(lexer::errorlevel::error, ast_value_judge, WO_ERR_UNKNOWN_TYPE,
                ast_value_judge->value_type->get_type_name(false).c_str()
            );
    }
    WO_PASS2(ast_value_type_check)
    {
        auto* a_value_type_check = WO_AST();

        if (a_value_type_check->is_constant == false)
        {
            fully_update_type(a_value_type_check->aim_type, false);

            if (a_value_type_check->aim_type->is_pure_pending())
                lang_anylizer->begin_trying_block();

            analyze_pass2(a_value_type_check->_be_check_value_node);

            if (a_value_type_check->aim_type->is_pure_pending())
            {
                a_value_type_check->is_constant = true;

                a_value_type_check->constant_value.set_bool(
                    a_value_type_check->_be_check_value_node->value_type->is_pending()
                    || !lang_anylizer->get_cur_error_frame().empty());

                lang_anylizer->end_trying_block();
            }
            else if (a_value_type_check->aim_type->is_pending())
            {
                lang_anylizer->lang_error(lexer::errorlevel::error, a_value_type_check, WO_ERR_UNKNOWN_TYPE,
                    a_value_type_check->aim_type->get_type_name(false).c_str()
                );
            }

            a_value_type_check->eval_constant_value(lang_anylizer);
        }
    }
    WO_PASS2(ast_value_index)
    {
        auto* a_value_index = WO_AST();
        analyze_pass2(a_value_index->from);
        analyze_pass2(a_value_index->index);

        if (a_value_index->value_type->is_pending())
        {
            if (a_value_index->from->value_type->is_struct())
            {
                if (a_value_index->index->is_constant && a_value_index->index->value_type->is_string())
                {
                    auto member_field_name = wstring_pool::get_pstr(
                        str_to_wstr(*a_value_index->index->get_constant_value().string));

                    if (auto fnd =
                        a_value_index->from->value_type->struct_member_index.find(member_field_name);
                        fnd != a_value_index->from->value_type->struct_member_index.end())
                    {
                        fully_update_type(fnd->second.member_type, false);

                        if (fnd->second.member_type->is_pending())
                        {
                            lang_anylizer->lang_error(lexer::errorlevel::error, a_value_index, WO_ERR_UNKNOWN_TYPE,
                                fnd->second.member_type->get_type_name(false).c_str());
                        }
                        else
                        {
                            a_value_index->value_type->set_type(fnd->second.member_type);
                            a_value_index->struct_offset = fnd->second.offset;
                        }

                        // Get symbol from type, if symbol not exist, it is anonymous structure.
                        // Anonymous structure's field is `public`.
                        lang_symbol* struct_symb = nullptr;
                        if (a_value_index->from->value_type->using_type_name != nullptr)
                        {
                            struct_symb = a_value_index->from->value_type->using_type_name->symbol;
                            wo_assert(struct_symb != nullptr);
                            wo_assert(struct_symb->type == lang_symbol::symbol_type::typing);
                        }

                        // Anonymous structure's member donot contain decl attribute.
                        // But named structure must have.
                        wo_assert(struct_symb == nullptr || fnd->second.member_decl_attribute != nullptr);

                        if (struct_symb == nullptr || fnd->second.member_decl_attribute->is_public_attr())
                        {
                            // Nothing to check.
                        }
                        else if (fnd->second.member_decl_attribute->is_protected_attr())
                        {
                            if (!a_value_index->located_scope->belongs_to(
                                a_value_index->from->value_type->searching_begin_namespace_in_pass2))
                            {
                                lang_anylizer->lang_error(lexer::errorlevel::error, a_value_index, WO_ERR_UNACCABLE_PROTECTED_MEMBER,
                                    a_value_index->from->value_type->get_type_name(false, false).c_str(),
                                    member_field_name->c_str());
                            }
                        }
                        else
                        {
                            if (a_value_index->source_file != struct_symb->defined_source())
                            {
                                lang_anylizer->lang_error(lexer::errorlevel::error, a_value_index, WO_ERR_UNACCABLE_PRIVATE_MEMBER,
                                    a_value_index->from->value_type->get_type_name(false, false).c_str(),
                                    member_field_name->c_str());
                            }
                        }
                    }
                    else
                    {
                        lang_anylizer->lang_error(lexer::errorlevel::error, a_value_index, WO_ERR_UNDEFINED_MEMBER,
                            member_field_name->c_str());
                    }
                }
                else
                {
                    lang_anylizer->lang_error(lexer::errorlevel::error, a_value_index, WO_ERR_CANNOT_INDEX_MEMB_WITHOUT_STR);
                }

            }
            else if (!a_value_index->from->value_type->is_pending() && !a_value_index->from->value_type->is_hkt())
            {
                if (a_value_index->from->value_type->is_array() || a_value_index->from->value_type->is_vec())
                    a_value_index->value_type->set_type(a_value_index->from->value_type->template_arguments[0]);
                else if (a_value_index->from->value_type->is_dict() || a_value_index->from->value_type->is_map())
                    a_value_index->value_type->set_type(a_value_index->from->value_type->template_arguments[1]);
                else if (a_value_index->from->value_type->is_tuple())
                {
                    if (a_value_index->index->is_constant && a_value_index->index->value_type->is_integer())
                    {
                        // Index tuple must with constant integer.
                        auto index = a_value_index->index->get_constant_value().integer;
                        if ((size_t)index < a_value_index->from->value_type->template_arguments.size() && index >= 0)
                        {
                            a_value_index->value_type->set_type(a_value_index->from->value_type->template_arguments[(size_t)index]);
                            a_value_index->struct_offset = (uint16_t)index;
                        }
                        else
                            lang_anylizer->lang_error(lexer::errorlevel::error, a_value_index, WO_ERR_FAILED_TO_INDEX_TUPLE_ERR_INDEX,
                                (int)a_value_index->from->value_type->template_arguments.size(), (int)(index + 1));
                    }
                    else
                    {
                        lang_anylizer->lang_error(lexer::errorlevel::error, a_value_index, WO_ERR_FAILED_TO_INDEX_TUPLE_ERR_TYPE);
                    }
                }
                else if (a_value_index->from->value_type->is_string())
                {
                    a_value_index->value_type->set_type_with_name(WO_PSTR(char));
                }
            }
        }

        if ((!a_value_index->from->value_type->is_array()
            && !a_value_index->from->value_type->is_dict()
            && !a_value_index->from->value_type->is_vec()
            && !a_value_index->from->value_type->is_map()
            && !a_value_index->from->value_type->is_string()
            && !a_value_index->from->value_type->is_struct()
            && !a_value_index->from->value_type->is_tuple())
            || a_value_index->from->value_type->is_hkt())
        {
            lang_anylizer->lang_error(lexer::errorlevel::error, a_value_index->from, WO_ERR_UNINDEXABLE_TYPE
                , a_value_index->from->value_type->get_type_name(false).c_str());
        }
        // Checking for indexer type
        else if (a_value_index->from->value_type->is_array() || a_value_index->from->value_type->is_vec() || a_value_index->from->value_type->is_string())
        {
            if (!a_value_index->index->value_type->is_integer())
            {
                lang_anylizer->lang_error(lexer::errorlevel::error, a_value_index->index, WO_ERR_SHOULD_BE_TYPE_BUT_GET_UNEXCEPTED_TYPE
                    , L"int"
                    , a_value_index->index->value_type->get_type_name(false).c_str());
            }
        }
        else if (a_value_index->from->value_type->is_dict() || a_value_index->from->value_type->is_map())
        {
            if (!a_value_index->index->value_type->is_same(a_value_index->from->value_type->template_arguments[0], true))
            {
                lang_anylizer->lang_error(lexer::errorlevel::error, a_value_index->index, WO_ERR_SHOULD_BE_TYPE_BUT_GET_UNEXCEPTED_TYPE
                    , a_value_index->from->value_type->template_arguments[0]->get_type_name(false).c_str()
                    , a_value_index->index->value_type->get_type_name(false).c_str());
            }
        }
    }
    WO_PASS2(ast_value_indexed_variadic_args)
    {
        auto* a_value_variadic_args_idx = WO_AST();
        analyze_pass2(a_value_variadic_args_idx->argindex);

        if (!a_value_variadic_args_idx->argindex->value_type->is_integer())
        {
            lang_anylizer->lang_error(lexer::errorlevel::error,
                a_value_variadic_args_idx, WO_ERR_FAILED_TO_INDEX_VAARG_ERR_TYPE);
        }
    }
    WO_PASS2(ast_fakevalue_unpacked_args)
    {
        auto* a_fakevalue_unpacked_args = WO_AST();
        analyze_pass2(a_fakevalue_unpacked_args->unpacked_pack);
        if (!a_fakevalue_unpacked_args->unpacked_pack->value_type->is_array()
            && !a_fakevalue_unpacked_args->unpacked_pack->value_type->is_vec()
            && !a_fakevalue_unpacked_args->unpacked_pack->value_type->is_tuple())
        {
            lang_anylizer->lang_error(lexer::errorlevel::error,
                a_fakevalue_unpacked_args, WO_ERR_NEED_TYPES, L"array, vec" WO_TERM_OR L"tuple");
        }
    }
    WO_PASS2(ast_value_binary)
    {
        auto* a_value_bin = WO_AST();
        analyze_pass2(a_value_bin->left);
        analyze_pass2(a_value_bin->right);

        if (a_value_bin->overrided_operation_call)
        {
            lang_anylizer->begin_trying_block();
            analyze_pass2(a_value_bin->overrided_operation_call);
            lang_anylizer->end_trying_block();

            if (a_value_bin->overrided_operation_call->value_type->is_pending())
            {
                // Failed to call override func
                a_value_bin->overrided_operation_call = nullptr;
                auto* lnr_type = ast_value_binary::binary_upper_type_with_operator(
                    a_value_bin->left->value_type,
                    a_value_bin->right->value_type,
                    a_value_bin->operate
                );
                if (lnr_type != nullptr)
                {
                    a_value_bin->value_type->set_type(lnr_type);
                }
                else
                    a_value_bin->value_type->set_type_with_name(WO_PSTR(pending));
            }
            else
            {
                // Apply this type to func
                if (a_value_bin->value_type->is_pending())
                    a_value_bin->value_type->set_type(a_value_bin->overrided_operation_call->value_type);
            }

        }

        if (a_value_bin->value_type->is_pending())
        {
            lang_anylizer->lang_error(lexer::errorlevel::error, a_value_bin, WO_ERR_CANNOT_CALC_WITH_L_AND_R,
                a_value_bin->left->value_type->get_type_name(false).c_str(),
                a_value_bin->right->value_type->get_type_name(false).c_str());
            a_value_bin->value_type->set_type_with_name(WO_PSTR(pending));
        }
    }
    WO_PASS2(ast_value_logical_binary)
    {
        auto* a_value_logic_bin = WO_AST();
        analyze_pass2(a_value_logic_bin->left);
        analyze_pass2(a_value_logic_bin->right);

        if (a_value_logic_bin->overrided_operation_call)
        {
            lang_anylizer->begin_trying_block();
            analyze_pass2(a_value_logic_bin->overrided_operation_call);
            lang_anylizer->end_trying_block();

            if (a_value_logic_bin->overrided_operation_call->value_type->is_pending())
            {
                // Failed to call override func
                a_value_logic_bin->overrided_operation_call = nullptr;
            }
            else
            {
                // Apply this type to func
                wo_assert(a_value_logic_bin->value_type != nullptr);
                a_value_logic_bin->value_type->set_type(a_value_logic_bin->overrided_operation_call->value_type);
            }

        }
        if (a_value_logic_bin->value_type->is_pending())
        {
            bool type_ok = false;
            /*if (a_value_logic_bin->left->value_type->is_builtin_basic_type()
                && a_value_logic_bin->right->value_type->is_builtin_basic_type())*/
            {
                if (a_value_logic_bin->operate == lex_type::l_lor
                    || a_value_logic_bin->operate == lex_type::l_land)
                {
                    if (a_value_logic_bin->left->value_type->is_bool()
                        && a_value_logic_bin->right->value_type->is_bool())
                        type_ok = true;
                }
                else if ((a_value_logic_bin->left->value_type->is_integer()
                    || a_value_logic_bin->left->value_type->is_handle()
                    || a_value_logic_bin->left->value_type->is_real()
                    || a_value_logic_bin->left->value_type->is_string()
                    || a_value_logic_bin->left->value_type->is_bool())
                    && a_value_logic_bin->left->value_type->is_same(a_value_logic_bin->right->value_type, true))
                    type_ok = true;
            }

            if (!type_ok)
                lang_anylizer->lang_error(lexer::errorlevel::error, a_value_logic_bin, WO_ERR_CANNOT_CALC_WITH_L_AND_R,
                    a_value_logic_bin->left->value_type->get_type_name(false).c_str(),
                    a_value_logic_bin->right->value_type->get_type_name(false).c_str());
            else
                a_value_logic_bin->value_type->set_type_with_name(WO_PSTR(bool));
            fully_update_type(a_value_logic_bin->value_type, false);
        }
    }
    WO_PASS2(ast_value_array)
    {
        auto* a_value_arr = WO_AST();
        analyze_pass2(a_value_arr->array_items);

        if (a_value_arr->value_type->is_pending())
        {
            ast_type* decide_array_item_type = a_value_arr->value_type->template_arguments[0];
            decide_array_item_type->set_type_with_name(WO_PSTR(nothing));

            ast_value* val = dynamic_cast<ast_value*>(a_value_arr->array_items->children);
            if (val)
            {
                if (!val->value_type->is_pending())
                    decide_array_item_type->set_type(val->value_type);
                else
                    decide_array_item_type->set_type_with_name(WO_PSTR(pending));
            }

            while (val)
            {
                if (val->value_type->is_pending())
                {
                    decide_array_item_type->set_type_with_name(WO_PSTR(pending));
                    break;
                }

                if (!decide_array_item_type->accept_type(val->value_type, false, true)
                    && !decide_array_item_type->set_mix_types(val->value_type, false))
                {
                    if (!a_value_arr->is_mutable_vector)
                        lang_anylizer->lang_error(lexer::errorlevel::error, val, WO_ERR_DIFFERENT_VAL_TYPE_OF, L"array", L"array");
                    else
                        lang_anylizer->lang_error(lexer::errorlevel::error, val, WO_ERR_DIFFERENT_VAL_TYPE_OF, L"vec", L"vec");
                    break;
                }
                val = dynamic_cast<ast_value*>(val->sibling);
            }
        }

        if (!a_value_arr->is_mutable_vector && !a_value_arr->value_type->is_array())
        {
            lang_anylizer->lang_error(lexer::errorlevel::error, a_value_arr, WO_ERR_CANNOT_CAST_TYPE_TO_TYPE,
                L"array<..>",
                a_value_arr->value_type->get_type_name().c_str()
            );
        }
        else if (a_value_arr->is_mutable_vector && !a_value_arr->value_type->is_vec())
        {
            lang_anylizer->lang_error(lexer::errorlevel::error, a_value_arr, WO_ERR_CANNOT_CAST_TYPE_TO_TYPE,
                L"vec<..>",
                a_value_arr->value_type->get_type_name().c_str()
            );
        }
        else
        {
            std::vector<ast_value*> reenplace_array_items;

            ast_value* val = dynamic_cast<ast_value*>(a_value_arr->array_items->children);

            while (val)
            {
                if (val->value_type->is_pending())
                {
                    lang_anylizer->lang_error(lexer::errorlevel::error, val, WO_ERR_UNKNOWN_TYPE,
                        val->value_type->get_type_name(false, false).c_str());
                }
                else if (!a_value_arr->value_type->template_arguments[0]->accept_type(val->value_type, false, true))
                {
                    if (!a_value_arr->is_mutable_vector)
                        lang_anylizer->lang_error(lexer::errorlevel::error, val, WO_ERR_DIFFERENT_VAL_TYPE_OF_TEMPLATE, L"array");
                    else
                        lang_anylizer->lang_error(lexer::errorlevel::error, val, WO_ERR_DIFFERENT_VAL_TYPE_OF_TEMPLATE, L"vec");
                }
                reenplace_array_items.push_back(val);

                val = dynamic_cast<ast_value*>(val->sibling);
            }

            a_value_arr->array_items->remove_all_childs();
            for (auto in_array_val : reenplace_array_items)
            {
                in_array_val->sibling = nullptr;
                a_value_arr->array_items->add_child(in_array_val);
            }

        }
    }
    WO_PASS2(ast_value_mapping)
    {
        auto* a_value_map = WO_AST();
        analyze_pass2(a_value_map->mapping_pairs);

        if (a_value_map->value_type->is_pending())
        {
            ast_type* decide_map_key_type = a_value_map->value_type->template_arguments[0];
            ast_type* decide_map_val_type = a_value_map->value_type->template_arguments[1];

            decide_map_key_type->set_type_with_name(WO_PSTR(nothing));
            decide_map_val_type->set_type_with_name(WO_PSTR(nothing));

            ast_mapping_pair* map_pair = dynamic_cast<ast_mapping_pair*>(a_value_map->mapping_pairs->children);
            if (map_pair)
            {
                if (!map_pair->key->value_type->is_pending() && !map_pair->val->value_type->is_pending())
                {
                    decide_map_key_type->set_type(map_pair->key->value_type);
                    decide_map_val_type->set_type(map_pair->val->value_type);
                }
                else
                {
                    decide_map_key_type->set_type_with_name(WO_PSTR(pending));
                    decide_map_val_type->set_type_with_name(WO_PSTR(pending));
                }
            }
            while (map_pair)
            {
                if (map_pair->key->value_type->is_pending() || map_pair->val->value_type->is_pending())
                {
                    decide_map_key_type->set_type_with_name(WO_PSTR(pending));
                    decide_map_val_type->set_type_with_name(WO_PSTR(pending));
                    break;
                }

                if (!decide_map_key_type->accept_type(map_pair->key->value_type, false, true)
                    && !decide_map_key_type->set_mix_types(map_pair->key->value_type, false))
                {
                    if (!a_value_map->is_mutable_map)
                        lang_anylizer->lang_error(lexer::errorlevel::error, map_pair->key, WO_ERR_DIFFERENT_KEY_TYPE_OF, L"dict", L"dict");
                    else
                        lang_anylizer->lang_error(lexer::errorlevel::error, map_pair->key, WO_ERR_DIFFERENT_KEY_TYPE_OF, L"map", L"map");
                    break;
                }
                if (!decide_map_val_type->accept_type(map_pair->val->value_type, false, true)
                    && !decide_map_val_type->set_mix_types(map_pair->val->value_type, false))
                {
                    if (!a_value_map->is_mutable_map)
                        lang_anylizer->lang_error(lexer::errorlevel::error, map_pair->val, WO_ERR_DIFFERENT_VAL_TYPE_OF, L"dict", L"dict");
                    else
                        lang_anylizer->lang_error(lexer::errorlevel::error, map_pair->val, WO_ERR_DIFFERENT_VAL_TYPE_OF, L"map", L"map");
                    break;
                }
                map_pair = dynamic_cast<ast_mapping_pair*>(map_pair->sibling);
            }
        }

        if (!a_value_map->is_mutable_map && !a_value_map->value_type->is_dict())
        {
            lang_anylizer->lang_error(lexer::errorlevel::error, a_value_map, WO_ERR_CANNOT_CAST_TYPE_TO_TYPE,
                L"dict<.., ..>",
                a_value_map->value_type->get_type_name().c_str()
            );
        }
        else if (a_value_map->is_mutable_map && !a_value_map->value_type->is_map())
        {
            lang_anylizer->lang_error(lexer::errorlevel::error, a_value_map, WO_ERR_CANNOT_CAST_TYPE_TO_TYPE,
                L"map<.., ..>",
                a_value_map->value_type->get_type_name().c_str()
            );
        }
        else
        {
            ast_mapping_pair* pairs = dynamic_cast<ast_mapping_pair*>(a_value_map->mapping_pairs->children);

            while (pairs)
            {
                if (pairs->key->value_type->is_pending()
                    || pairs->val->value_type->is_pending()
                    || a_value_map->value_type->template_arguments[0]->is_pending()
                    || a_value_map->value_type->template_arguments[1]->is_pending())
                {
                    // Do nothing..
                }
                else
                {
                    if (pairs->key->value_type->is_pending())
                    {
                        lang_anylizer->lang_error(lexer::errorlevel::error, pairs->key, WO_ERR_UNKNOWN_TYPE,
                            pairs->key->value_type->get_type_name(false, false).c_str());
                    }
                    else if (!a_value_map->value_type->template_arguments[0]->accept_type(pairs->key->value_type, false, true))
                    {
                        if (!a_value_map->is_mutable_map)
                            lang_anylizer->lang_error(lexer::errorlevel::error, pairs->key, WO_ERR_DIFFERENT_KEY_TYPE_OF_TEMPLATE, L"dict");
                        else
                            lang_anylizer->lang_error(lexer::errorlevel::error, pairs->key, WO_ERR_DIFFERENT_KEY_TYPE_OF_TEMPLATE, L"map");
                    }

                    if (pairs->val->value_type->is_pending())
                    {
                        lang_anylizer->lang_error(lexer::errorlevel::error, pairs->val, WO_ERR_UNKNOWN_TYPE,
                            pairs->val->value_type->get_type_name(false, false).c_str());
                    }
                    else if (!a_value_map->value_type->template_arguments[1]->accept_type(pairs->val->value_type, false, true))
                    {
                        if (!a_value_map->is_mutable_map)
                            lang_anylizer->lang_error(lexer::errorlevel::error, pairs->val, WO_ERR_DIFFERENT_VAL_TYPE_OF_TEMPLATE, L"dict");
                        else
                            lang_anylizer->lang_error(lexer::errorlevel::error, pairs->val, WO_ERR_DIFFERENT_VAL_TYPE_OF_TEMPLATE, L"map");
                    }

                }
                pairs = dynamic_cast<ast_mapping_pair*>(pairs->sibling);
            }
        }
    }
    WO_PASS2(ast_value_make_tuple_instance)
    {
        auto* a_value_make_tuple_instance = WO_AST();
        analyze_pass2(a_value_make_tuple_instance->tuple_member_vals);

        auto* tuple_elems = a_value_make_tuple_instance->tuple_member_vals->children;

        size_t count = 0;
        while (tuple_elems)
        {
            ast_value* val = dynamic_cast<ast_value*>(tuple_elems);
            if (!val->value_type->is_pending())
                a_value_make_tuple_instance->value_type->template_arguments[count]->set_type(val->value_type);
            else
            {
                lang_anylizer->lang_error(lexer::errorlevel::error, val, WO_ERR_FAILED_TO_DECIDE_TUPLE_TYPE);
                break;
            }
            tuple_elems = tuple_elems->sibling;
            ++count;
        }
    }
    WO_PASS2(ast_value_make_struct_instance)
    {
        auto* a_value_make_struct_instance = WO_AST();
        analyze_pass2(a_value_make_struct_instance->struct_member_vals);
        if (a_value_make_struct_instance->build_pure_struct)
        {
            auto* member_iter = dynamic_cast<ast_struct_member_define*>(a_value_make_struct_instance->struct_member_vals->children);
            while (member_iter)
            {
                wo_assert(member_iter->is_value_pair);
                if (!member_iter->member_value_pair->value_type->is_pending())
                {
                    auto fnd = a_value_make_struct_instance->target_built_types->struct_member_index.find(member_iter->member_name);
                    if (fnd != a_value_make_struct_instance->target_built_types->struct_member_index.end())
                    {
                        fnd->second.member_type->set_type(member_iter->member_value_pair->value_type);
                    }
                }
                member_iter = dynamic_cast<ast_struct_member_define*>(member_iter->sibling);
            }
        }
        fully_update_type(a_value_make_struct_instance->target_built_types, false);

        if (a_value_make_struct_instance->target_built_types->using_type_name != nullptr)
        {
            auto* struct_type_symbol = a_value_make_struct_instance->target_built_types->using_type_name->symbol;
            if (struct_type_symbol != nullptr && struct_type_symbol->attribute != nullptr)
            {
                if (!check_symbol_is_accessable(
                    struct_type_symbol,
                    a_value_make_struct_instance->located_scope,
                    a_value_make_struct_instance,
                    true))
                {
                    lang_anylizer->lang_error(
                        lexer::errorlevel::infom,
                        struct_type_symbol->get_typedef(),
                        WO_INFO_CANNOT_USE_UNREACHABLE_TYPE);
                }
            }
        }

        // Woolang 1.10: Struct's template param might be judge here, we just need do like ast_value_funccall.
        bool need_judge_template_args = a_value_make_struct_instance->target_built_types->is_pending()
            && a_value_make_struct_instance->target_built_types->symbol != nullptr
            && a_value_make_struct_instance->target_built_types->symbol->type == lang_symbol::symbol_type::typing
            && a_value_make_struct_instance->target_built_types->symbol->is_template_symbol
            && a_value_make_struct_instance->target_built_types->template_arguments.size()
            < a_value_make_struct_instance->target_built_types->symbol->template_types.size();

        auto* struct_defined_scope =
            a_value_make_struct_instance->target_built_types->symbol == nullptr
            ? a_value_make_struct_instance->located_scope
            : a_value_make_struct_instance->target_built_types->symbol->defined_in_scope;

        if (need_judge_template_args)
        {
            auto* origin_define_struct_type = a_value_make_struct_instance->target_built_types->symbol->type_informatiom;

            std::vector<ast_type*> template_args(a_value_make_struct_instance->target_built_types->symbol->template_types.size(), nullptr);

            // Fill template_args with already exists arguments.
            for (size_t index = 0; index < std::min(
                a_value_make_struct_instance->target_built_types->symbol->template_types.size(),
                a_value_make_struct_instance->target_built_types->template_arguments.size()); index++)
                template_args[index] = a_value_make_struct_instance->target_built_types->template_arguments[index];

            std::unordered_map<wo_pstring_t, ast_type*> updated_member_types;

            // 1st, trying auto judge member init expr's instance(if is template function.)
            auto* init_mem_val_pair = a_value_make_struct_instance->struct_member_vals->children;
            while (init_mem_val_pair)
            {
                auto* membpair = dynamic_cast<ast_struct_member_define*>(init_mem_val_pair);
                wo_assert(membpair);
                init_mem_val_pair = init_mem_val_pair->sibling;

                auto fnd = origin_define_struct_type->struct_member_index.find(membpair->member_name);
                if (fnd != origin_define_struct_type->struct_member_index.end())
                {
                    wo_assert(membpair->is_value_pair);

                    membpair->member_offset = fnd->second.offset;
                    fully_update_type(fnd->second.member_type, false,
                        a_value_make_struct_instance->target_built_types->symbol->template_types);

                    if (auto result = judge_auto_type_of_funcdef_with_type(struct_defined_scope,
                        fnd->second.member_type, membpair->member_value_pair, false, nullptr, nullptr))
                    {
                        updated_member_types[membpair->member_name] = std::get<ast::ast_type*>(result.value());
                    }
                }
                else
                {
                    // TODO: No such member? just continue, error will be reported in following step.
                }
            }

            // 2nd, Try derivation template arguments, it will used for next round of judge.
            for (size_t tempindex = 0; tempindex < template_args.size(); ++tempindex)
            {
                auto& pending_template_arg = template_args[tempindex];

                if (pending_template_arg == nullptr)
                {
                    // Walk through all member to get template arguments.
                    auto* init_mem_val_pair = a_value_make_struct_instance->struct_member_vals->children;
                    while (init_mem_val_pair)
                    {
                        auto* membpair = dynamic_cast<ast_struct_member_define*>(init_mem_val_pair);
                        wo_assert(membpair);

                        init_mem_val_pair = init_mem_val_pair->sibling;

                        auto fnd = origin_define_struct_type->struct_member_index.find(membpair->member_name);
                        if (fnd != origin_define_struct_type->struct_member_index.end())
                        {
                            fully_update_type(fnd->second.member_type, false,
                                a_value_make_struct_instance->target_built_types->symbol->template_types);

                            pending_template_arg = analyze_template_derivation(
                                a_value_make_struct_instance->target_built_types->symbol->template_types[tempindex],
                                a_value_make_struct_instance->target_built_types->symbol->template_types,
                                fnd->second.member_type,
                                updated_member_types[membpair->member_name]
                                ? updated_member_types[membpair->member_name]
                                : membpair->member_value_pair->value_type
                            );

                            if (pending_template_arg != nullptr)
                            {
                                if (pending_template_arg->is_pure_pending() ||
                                    (pending_template_arg->search_from_global_namespace == false
                                        && pending_template_arg->scope_namespaces.empty()
                                        && a_value_make_struct_instance->target_built_types->symbol->template_types[tempindex]
                                        == pending_template_arg->type_name))
                                    pending_template_arg = nullptr;
                            }

                            if (pending_template_arg)
                                break;
                        }
                        else
                        {
                            // TODO: No such member? just continue, error will be reported in following step.
                        }
                    }
                }
            }

            // 3rd, Make instance of member init expr.
            init_mem_val_pair = a_value_make_struct_instance->struct_member_vals->children;
            while (init_mem_val_pair)
            {
                auto* membpair = dynamic_cast<ast_struct_member_define*>(init_mem_val_pair);
                wo_assert(membpair);
                init_mem_val_pair = init_mem_val_pair->sibling;

                auto fnd = origin_define_struct_type->struct_member_index.find(membpair->member_name);
                if (fnd != origin_define_struct_type->struct_member_index.end())
                {
                    wo_assert(membpair->is_value_pair);

                    membpair->member_offset = fnd->second.offset;

                    fully_update_type(fnd->second.member_type, false,
                        a_value_make_struct_instance->target_built_types->symbol->template_types);

                    if (auto result = judge_auto_type_of_funcdef_with_type(struct_defined_scope,
                        fnd->second.member_type, membpair->member_value_pair, true,
                        a_value_make_struct_instance->target_built_types->symbol->define_node, &template_args))
                    {
                        if (dynamic_cast<ast::ast_value_mutable*>(membpair->member_value_pair) == nullptr)
                            membpair->member_value_pair = std::get<ast::ast_value_function_define*>(result.value());
                    }
                }
                else
                {
                    // TODO: No such member? just continue, error will be reported in following step.
                }
            }

            // 4th! re-judge real-template arguments.
            for (size_t tempindex = 0; tempindex < template_args.size(); ++tempindex)
            {
                auto& pending_template_arg = template_args[tempindex];

                if (pending_template_arg == nullptr)
                {
                    // Walk through all member to get template arguments.
                    auto* init_mem_val_pair = a_value_make_struct_instance->struct_member_vals->children;
                    while (init_mem_val_pair)
                    {
                        auto* membpair = dynamic_cast<ast_struct_member_define*>(init_mem_val_pair);
                        wo_assert(membpair);

                        init_mem_val_pair = init_mem_val_pair->sibling;

                        auto fnd = origin_define_struct_type->struct_member_index.find(membpair->member_name);
                        if (fnd != origin_define_struct_type->struct_member_index.end())
                        {
                            fully_update_type(fnd->second.member_type, false,
                                a_value_make_struct_instance->target_built_types->symbol->template_types);

                            pending_template_arg = analyze_template_derivation(
                                a_value_make_struct_instance->target_built_types->symbol->template_types[tempindex],
                                a_value_make_struct_instance->target_built_types->symbol->template_types,
                                fnd->second.member_type,
                                membpair->member_value_pair->value_type
                            );
                            if (pending_template_arg != nullptr)
                            {
                                if (pending_template_arg->is_pure_pending())
                                    pending_template_arg = nullptr;
                            }

                            if (pending_template_arg)
                                break;
                        }
                        else
                        {
                            // TODO: No such member? just continue, error will be reported in following step.
                        }
                    }
                }
            }

            // 5th, apply & update template struct.
            if (std::find(template_args.begin(), template_args.end(), nullptr) != template_args.end())
            {
                lang_anylizer->lang_error(lexer::errorlevel::error, a_value_make_struct_instance,
                    WO_ERR_FAILED_TO_DECIDE_ALL_TEMPLATE_ARGS);
                lang_anylizer->lang_error(lexer::errorlevel::infom, a_value_make_struct_instance->target_built_types->symbol->define_node,
                    WO_INFO_ITEM_IS_DEFINED_HERE,
                    a_value_make_struct_instance->target_built_types->get_type_name(false, true).c_str());
            }
            else
            {
                for (auto* templ_arg : template_args)
                {
                    fully_update_type(templ_arg, false);
                    if (templ_arg->is_pending() && !templ_arg->is_hkt())
                    {
                        lang_anylizer->lang_error(lexer::errorlevel::error, a_value_make_struct_instance, WO_ERR_UNKNOWN_TYPE,
                            templ_arg->get_type_name(false).c_str());
                        goto failed_to_judge_template_params;
                    }
                }

                a_value_make_struct_instance->target_built_types->template_arguments = template_args;
                fully_update_type(a_value_make_struct_instance->target_built_types, false);
            }
        failed_to_judge_template_params:;
        }

        // Varify
        if (!a_value_make_struct_instance->target_built_types->is_pending())
        {
            if (a_value_make_struct_instance->target_built_types->is_struct())
            {
                auto* init_mem_val_pair = a_value_make_struct_instance->struct_member_vals->children;

                uint16_t member_count = 0;
                while (init_mem_val_pair)
                {
                    member_count++;
                    auto* membpair = dynamic_cast<ast_struct_member_define*>(init_mem_val_pair);
                    wo_assert(membpair);
                    init_mem_val_pair = init_mem_val_pair->sibling;

                    auto fnd = a_value_make_struct_instance->target_built_types->struct_member_index.find(membpair->member_name);
                    if (fnd != a_value_make_struct_instance->target_built_types->struct_member_index.end())
                    {
                        wo_assert(membpair->is_value_pair);

                        membpair->member_offset = fnd->second.offset;
                        fully_update_type(fnd->second.member_type, false);
                        if (auto result = judge_auto_type_of_funcdef_with_type(struct_defined_scope,
                            fnd->second.member_type, membpair->member_value_pair, true, nullptr, nullptr))
                        {
                            if (dynamic_cast<ast::ast_value_mutable*>(membpair->member_value_pair) == nullptr)
                                membpair->member_value_pair = std::get<ast::ast_value_function_define*>(result.value());
                        }

                        lang_symbol* struct_symb = nullptr;
                        if (a_value_make_struct_instance->target_built_types->using_type_name != nullptr)
                        {
                            struct_symb = a_value_make_struct_instance->target_built_types->using_type_name->symbol;
                            wo_assert(struct_symb != nullptr);
                            wo_assert(struct_symb->type == lang_symbol::symbol_type::typing);
                        }

                        wo_assert(struct_symb == nullptr || fnd->second.member_decl_attribute != nullptr);
                        if (struct_symb == nullptr || fnd->second.member_decl_attribute->is_public_attr())
                        {
                            // Public member, donot need anycheck.
                        }
                        else if (fnd->second.member_decl_attribute->is_protected_attr())
                        {
                            if (!a_value_make_struct_instance->located_scope->belongs_to(
                                a_value_make_struct_instance->target_built_types->searching_begin_namespace_in_pass2))
                            {
                                lang_anylizer->lang_error(lexer::errorlevel::error, membpair->member_value_pair,
                                    WO_ERR_UNACCABLE_PROTECTED_MEMBER,
                                    a_value_make_struct_instance->target_built_types->get_type_name(false, false).c_str(),
                                    membpair->member_name->c_str());
                            }
                        }
                        else // if (fnd->second.member_decl_attribute->is_private_attr())
                        {
                            if (a_value_make_struct_instance->source_file != struct_symb->defined_source())
                            {
                                lang_anylizer->lang_error(lexer::errorlevel::error, membpair->member_value_pair,
                                    WO_ERR_UNACCABLE_PRIVATE_MEMBER,
                                    a_value_make_struct_instance->target_built_types->get_type_name(false, false).c_str(),
                                    membpair->member_name->c_str());
                            }
                        }

                        if (!fnd->second.member_type->accept_type(membpair->member_value_pair->value_type, false, false))
                        {
                            lang_anylizer->lang_error(lexer::errorlevel::error, membpair, WO_ERR_DIFFERENT_MEMBER_TYPE_OF
                                , membpair->member_name->c_str()
                                , fnd->second.member_type->get_type_name(false).c_str()
                                , membpair->member_value_pair->value_type->get_type_name(false).c_str());
                        }
                    }
                    else
                        lang_anylizer->lang_error(lexer::errorlevel::error, membpair, WO_ERR_THERE_IS_NO_MEMBER_NAMED,
                            a_value_make_struct_instance->target_built_types->get_type_name(false).c_str(),
                            membpair->member_name->c_str());
                }

                if (member_count < a_value_make_struct_instance->target_built_types->struct_member_index.size())
                    lang_anylizer->lang_error(lexer::errorlevel::error, a_value_make_struct_instance, WO_ERR_CONSTRUCT_STRUCT_NOT_FINISHED,
                        a_value_make_struct_instance->target_built_types->get_type_name(false).c_str());
                else
                    a_value_make_struct_instance->value_type->set_type(a_value_make_struct_instance->target_built_types);
            }
            else
                lang_anylizer->lang_error(lexer::errorlevel::error, a_value_make_struct_instance, WO_ERR_ONLY_CONSTRUCT_STRUCT_BY_THIS_WAY);
        }
        else
            lang_anylizer->lang_error(lexer::errorlevel::error, a_value_make_struct_instance, WO_ERR_UNKNOWN_TYPE,
                a_value_make_struct_instance->target_built_types->get_type_name(false).c_str());
    }
    WO_PASS2(ast_value_trib_expr)
    {
        auto* a_value_trib_expr = WO_AST();
        analyze_pass2(a_value_trib_expr->judge_expr);
        if (!a_value_trib_expr->judge_expr->value_type->is_bool())
        {
            lang_anylizer->lang_error(lexer::errorlevel::error, a_value_trib_expr->judge_expr, WO_ERR_NOT_BOOL_VAL_IN_COND_EXPR,
                a_value_trib_expr->judge_expr->value_type->get_type_name(false).c_str());
        }

        if (a_value_trib_expr->judge_expr->is_constant)
        {
            if (a_value_trib_expr->judge_expr->get_constant_value().integer)
            {
                analyze_pass2(a_value_trib_expr->val_if_true, false);
                a_value_trib_expr->value_type->set_type(a_value_trib_expr->val_if_true->value_type);
            }
            else
            {
                analyze_pass2(a_value_trib_expr->val_or, false);
                a_value_trib_expr->value_type->set_type(a_value_trib_expr->val_or->value_type);
            }

            a_value_trib_expr->judge_expr->eval_constant_value(lang_anylizer);
        }
        else
        {
            analyze_pass2(a_value_trib_expr->val_if_true, false);
            analyze_pass2(a_value_trib_expr->val_or, false);

            if (a_value_trib_expr->value_type->is_pending())
            {
                a_value_trib_expr->value_type->set_type(a_value_trib_expr->val_if_true->value_type);
                if (!a_value_trib_expr->value_type->set_mix_types(a_value_trib_expr->val_or->value_type, false))
                {
                    lang_anylizer->lang_error(lexer::errorlevel::error, a_value_trib_expr, WO_ERR_DIFFERENT_TYPES_IN_COND_EXPR
                        , a_value_trib_expr->val_if_true->value_type->get_type_name(false).c_str()
                        , a_value_trib_expr->val_or->value_type->get_type_name(false).c_str());
                }
            }
        }
    }
    WO_PASS2(ast_value_variable)
    {
        auto* a_value_var = WO_AST();

        auto* const variable_origin_symbol = find_value_in_this_scope(a_value_var);
        auto* final_sym = variable_origin_symbol;

        if (variable_origin_symbol != nullptr)
            variable_origin_symbol->is_marked_as_used_variable = true;

        if (a_value_var->value_type->is_pending())
        {
            if (variable_origin_symbol &&
                (!variable_origin_symbol->define_in_function
                    || variable_origin_symbol->has_been_completed_defined
                    || variable_origin_symbol->is_captured_variable))
            {
                if (variable_origin_symbol->is_template_symbol
                    && (false == a_value_var->is_this_value_used_for_function_call
                        || variable_origin_symbol->type == lang_symbol::symbol_type::variable))
                {
                    final_sym = analyze_pass_template_reification_var(a_value_var, a_value_var->template_reification_args);
                    if (nullptr == final_sym)
                        lang_anylizer->lang_error(lexer::errorlevel::error, a_value_var, WO_ERR_FAILED_TO_INSTANCE_TEMPLATE_ID,
                            a_value_var->var_name->c_str());
                }
                if (nullptr != final_sym)
                {
                    wo_assert(variable_origin_symbol != nullptr);
                    bool need_update_template_scope =
                        variable_origin_symbol->is_template_symbol
                        && variable_origin_symbol->type == lang_symbol::symbol_type::variable;

                    if (need_update_template_scope)
                        wo_assure(begin_template_scope(
                            a_value_var, 
                            final_sym->defined_in_scope, 
                            a_value_var->symbol->template_types, 
                            a_value_var->template_reification_args));

                    analyze_pass2(final_sym->variable_value);

                    if (need_update_template_scope)
                        end_template_scope(final_sym->defined_in_scope);

                    a_value_var->value_type->set_type(final_sym->variable_value->value_type);

                    if (final_sym->type == lang_symbol::symbol_type::variable && final_sym->decl == identifier_decl::MUTABLE)
                        a_value_var->value_type->set_is_mutable(true);
                    else
                        a_value_var->value_type->set_is_mutable(false);

                    a_value_var->symbol = final_sym;

                    if (a_value_var->symbol->type == lang_symbol::symbol_type::function)
                    {
                        auto* funcdef = a_value_var->symbol->get_funcdef();
                        check_function_where_constraint(a_value_var, lang_anylizer, funcdef);
                    }

                    if (a_value_var->value_type->is_pending())
                    {
                        if (a_value_var->symbol->type == lang_symbol::symbol_type::function)
                        {
                            if (a_value_var->symbol->is_template_symbol)
                                ; /* function call may be template, do not report error here~ */
                            else
                            {
                                lang_anylizer->lang_error(lexer::errorlevel::error, a_value_var, WO_ERR_UNABLE_DECIDE_EXPR_TYPE);
                                lang_anylizer->lang_error(lexer::errorlevel::infom, final_sym->variable_value, WO_INFO_ITEM_IS_DEFINED_HERE,
                                    a_value_var->var_name->c_str());
                            }
                        }
                        else
                        {
                            lang_anylizer->lang_error(lexer::errorlevel::error, a_value_var, WO_ERR_UNABLE_DECIDE_EXPR_TYPE);
                            lang_anylizer->lang_error(lexer::errorlevel::infom, final_sym->variable_value, WO_INFO_INIT_EXPR_IS_HERE,
                                a_value_var->var_name->c_str());
                        }
                    }
                }
            }
            else
            {
                lang_anylizer->lang_error(lexer::errorlevel::error, a_value_var, WO_ERR_UNKNOWN_IDENTIFIER,
                    a_value_var->get_full_variable_name().c_str());

                auto fuzz_symbol = find_symbol_in_this_scope(
                    a_value_var, a_value_var->var_name,
                    lang_symbol::symbol_type::variable | lang_symbol::symbol_type::function, true);
                if (fuzz_symbol != nullptr)
                {
                    auto fuzz_symbol_full_name = str_to_wstr(get_belong_namespace_path_with_lang_scope(fuzz_symbol));
                    if (!fuzz_symbol_full_name.empty())
                        fuzz_symbol_full_name += L"::";
                    fuzz_symbol_full_name += *fuzz_symbol->name;
                    lang_anylizer->lang_error(lexer::errorlevel::infom,
                        fuzz_symbol->variable_value,
                        WO_INFO_IS_THIS_ONE,
                        fuzz_symbol_full_name.c_str());
                }
                a_value_var->value_type->set_type_with_name(WO_PSTR(pending));
            }
        }
    }
    WO_PASS2(ast_value_unary)
    {
        auto* a_value_unary = WO_AST();

        analyze_pass2(a_value_unary->val);

        if (a_value_unary->operate == lex_type::l_lnot)
        {
            a_value_unary->value_type->set_type_with_name(WO_PSTR(bool));
            fully_update_type(a_value_unary->value_type, false);
        }
        a_value_unary->value_type->set_type(a_value_unary->val->value_type);
    }
    WO_PASS2(ast_value_funccall)
    {
        auto* a_value_funccall = WO_AST();

        if (a_value_funccall->callee_symbol_in_type_namespace)
        {
            wo_assert(a_value_funccall->callee_symbol_in_type_namespace->completed_in_pass2 == false);

            analyze_pass2(a_value_funccall->directed_value_from);
            if (!a_value_funccall->directed_value_from->value_type->is_pending())
            {
                // trying finding type_function
                const auto origin_namespace = a_value_funccall->callee_symbol_in_type_namespace->scope_namespaces;

                // Is using type?
                if (a_value_funccall->directed_value_from->value_type->using_type_name)
                {
                    a_value_funccall->callee_symbol_in_type_namespace->scope_namespaces =
                        a_value_funccall->directed_value_from->value_type->using_type_name->scope_namespaces;

                    a_value_funccall->callee_symbol_in_type_namespace->scope_namespaces.push_back
                    (a_value_funccall->directed_value_from->value_type->using_type_name->type_name);

                    a_value_funccall->callee_symbol_in_type_namespace->scope_namespaces.insert(
                        a_value_funccall->callee_symbol_in_type_namespace->scope_namespaces.end(),
                        origin_namespace.begin(),
                        origin_namespace.end()
                    );

                    auto* direct_called_func_symbol = find_value_symbol_in_this_scope(a_value_funccall->callee_symbol_in_type_namespace);

                    if (direct_called_func_symbol != nullptr)
                    {
                        if (auto* old_callee_func = dynamic_cast<ast::ast_value_variable*>(a_value_funccall->called_func))
                            a_value_funccall->callee_symbol_in_type_namespace->template_reification_args
                            = old_callee_func->template_reification_args;

                        a_value_funccall->called_func = a_value_funccall->callee_symbol_in_type_namespace;
                        if (direct_called_func_symbol->type == lang_symbol::symbol_type::function)
                        {
                            // Template function may get pure-pending type here; inorder to make sure `auto` judge, here get template type.
                            auto* funcdef = direct_called_func_symbol->get_funcdef();
                            if (funcdef->is_template_define)
                            {
                                a_value_funccall->called_func->value_type->set_type(funcdef->value_type);
                                // Make sure it will not be analyze in pass2
                                a_value_funccall->callee_symbol_in_type_namespace->completed_in_pass2 = true;
                            }
                        }
                        goto start_ast_op_calling;
                    }
                    else
                        a_value_funccall->callee_symbol_in_type_namespace->scope_namespaces = origin_namespace;
                }
                // base type?
                else
                {
                    a_value_funccall->callee_symbol_in_type_namespace->scope_namespaces.clear();
                    a_value_funccall->callee_symbol_in_type_namespace->scope_namespaces.push_back
                    (a_value_funccall->directed_value_from->value_type->type_name);
                    a_value_funccall->callee_symbol_in_type_namespace->scope_namespaces.insert(
                        a_value_funccall->callee_symbol_in_type_namespace->scope_namespaces.end(),
                        origin_namespace.begin(),
                        origin_namespace.end()
                    );

                    auto* direct_called_func_symbol = find_value_symbol_in_this_scope(a_value_funccall->callee_symbol_in_type_namespace);

                    if (direct_called_func_symbol != nullptr)
                    {
                        if (auto* old_callee_func = dynamic_cast<ast::ast_value_variable*>(a_value_funccall->called_func))
                            a_value_funccall->callee_symbol_in_type_namespace->template_reification_args
                            = old_callee_func->template_reification_args;

                        a_value_funccall->called_func = a_value_funccall->callee_symbol_in_type_namespace;
                        if (direct_called_func_symbol->type == lang_symbol::symbol_type::function)
                        {
                            // Template function may get pure-pending type here; inorder to make sure `auto` judge, here get template type.
                            auto* funcdef = direct_called_func_symbol->get_funcdef();
                            if (funcdef->is_template_define)
                            {
                                a_value_funccall->called_func->value_type->set_type(funcdef->value_type);
                                // Make sure it will not be analyze in pass2
                                a_value_funccall->callee_symbol_in_type_namespace->completed_in_pass2 = true;
                            }
                        }
                        goto start_ast_op_calling;
                    }
                    else
                        a_value_funccall->callee_symbol_in_type_namespace->scope_namespaces = origin_namespace;
                }
                // End trying invoke from direct-type namespace
            }
        }
    start_ast_op_calling:

        analyze_pass2(a_value_funccall->called_func);
        analyze_pass2(a_value_funccall->arguments);

        if (a_value_funccall->callee_symbol_in_type_namespace != nullptr
            && a_value_funccall->called_func->value_type->is_pure_pending())
        {
            if (auto* callee_variable = dynamic_cast<ast_value_variable*>(a_value_funccall->called_func);
                callee_variable != nullptr && callee_variable->symbol == nullptr)
            {
                lang_anylizer->lang_error(lexer::errorlevel::infom, a_value_funccall, WO_INFO_FAILED_TO_INVOKE_FUNC_FOR_TYPE,
                    a_value_funccall->callee_symbol_in_type_namespace->var_name->c_str(),
                    a_value_funccall->directed_value_from->value_type->get_type_name(false).c_str());

                // Here to truing to find fuzzy name function.
                lang_symbol* fuzzy_method_function_symbol = nullptr;
                if (!a_value_funccall->directed_value_from->value_type->is_pending())
                {
                    // trying finding type_function
                    const auto origin_namespace = a_value_funccall->callee_symbol_in_type_namespace->scope_namespaces;

                    // Is using type?
                    if (a_value_funccall->directed_value_from->value_type->using_type_name)
                    {
                        a_value_funccall->callee_symbol_in_type_namespace->scope_namespaces =
                            a_value_funccall->directed_value_from->value_type->using_type_name->scope_namespaces;
                        a_value_funccall->callee_symbol_in_type_namespace->scope_namespaces.push_back
                        (a_value_funccall->directed_value_from->value_type->using_type_name->type_name);
                        a_value_funccall->callee_symbol_in_type_namespace->scope_namespaces.insert(
                            a_value_funccall->callee_symbol_in_type_namespace->scope_namespaces.end(),
                            origin_namespace.begin(),
                            origin_namespace.end()
                        );
                    }
                    // base type?
                    else
                    {
                        a_value_funccall->callee_symbol_in_type_namespace->scope_namespaces.clear();
                        a_value_funccall->callee_symbol_in_type_namespace->scope_namespaces.push_back
                        (a_value_funccall->directed_value_from->value_type->type_name);
                        a_value_funccall->callee_symbol_in_type_namespace->scope_namespaces.insert(
                            a_value_funccall->callee_symbol_in_type_namespace->scope_namespaces.end(),
                            origin_namespace.begin(),
                            origin_namespace.end()
                        );
                    }

                    fuzzy_method_function_symbol = find_symbol_in_this_scope(
                        a_value_funccall->callee_symbol_in_type_namespace,
                        a_value_funccall->callee_symbol_in_type_namespace->var_name,
                        lang_symbol::symbol_type::variable | lang_symbol::symbol_type::function, true);

                    a_value_funccall->callee_symbol_in_type_namespace->scope_namespaces = origin_namespace;
                }

                if (fuzzy_method_function_symbol != nullptr)
                {
                    auto fuzz_symbol_full_name = str_to_wstr(get_belong_namespace_path_with_lang_scope(fuzzy_method_function_symbol));
                    if (!fuzz_symbol_full_name.empty())
                        fuzz_symbol_full_name += L"::";
                    fuzz_symbol_full_name += *fuzzy_method_function_symbol->name;
                    lang_anylizer->lang_error(lexer::errorlevel::infom,
                        fuzzy_method_function_symbol->variable_value,
                        WO_INFO_IS_THIS_ONE,
                        fuzz_symbol_full_name.c_str());
                }
            }
        }

        auto* called_funcsymb = dynamic_cast<ast_symbolable_base*>(a_value_funccall->called_func);
        auto* judge_auto_call_func_located_scope =
            called_funcsymb == nullptr || called_funcsymb->symbol == nullptr
            ? a_value_funccall->located_scope
            : called_funcsymb->symbol->defined_in_scope;

        auto updated_args_types = judge_auto_type_in_funccall(
            a_value_funccall, judge_auto_call_func_located_scope, false, nullptr, nullptr);

        ast_value_function_define* calling_function_define = nullptr;
        if (called_funcsymb != nullptr)
        {
            // called_funcsymb might be lambda function, and have not symbol.
            if (called_funcsymb->symbol != nullptr
                && called_funcsymb->symbol->type == lang_symbol::symbol_type::function)
            {
                check_symbol_is_accessable(
                    called_funcsymb->symbol,
                    called_funcsymb->searching_begin_namespace_in_pass2,
                    a_value_funccall,
                    true);

                calling_function_define = called_funcsymb->symbol->get_funcdef();
            }
            else
                calling_function_define = dynamic_cast<ast_value_function_define*>(a_value_funccall->called_func);
        }

        if (calling_function_define != nullptr
            && calling_function_define->is_template_define)
        {
            // Judge template here.
            std::vector<ast_type*> template_args(calling_function_define->template_type_name_list.size(), nullptr);
            if (auto* variable = dynamic_cast<ast_value_variable*>(a_value_funccall->called_func))
            {
                if (variable->template_reification_args.size() > template_args.size())
                    lang_anylizer->lang_error(lexer::errorlevel::error, a_value_funccall, WO_ERR_NO_MATCHED_FUNC_TEMPLATE);
                else
                    for (size_t index = 0; index < variable->template_reification_args.size(); index++)
                        template_args[index] = variable->template_reification_args[index];

            }
            // finish template args spcified by : xxxxx:<a,b,c> 
            // trying auto judge type..

            std::vector<ast_type*> real_argument_types;
            ast_value* funccall_arg = dynamic_cast<ast_value*>(a_value_funccall->arguments->children);
            while (funccall_arg)
            {
                if (auto* fake_unpack_value = dynamic_cast<ast_fakevalue_unpacked_args*>(funccall_arg))
                {
                    if (fake_unpack_value->unpacked_pack->value_type->is_tuple())
                    {
                        wo_assert(fake_unpack_value->expand_count >= 0);

                        size_t expand_count = std::min((size_t)fake_unpack_value->expand_count,
                            fake_unpack_value->unpacked_pack->value_type->template_arguments.size());

                        for (size_t i = 0; i < expand_count; ++i)
                            real_argument_types.push_back(
                                fake_unpack_value->unpacked_pack->value_type->template_arguments[i]);
                    }
                }
                else
                    real_argument_types.push_back(funccall_arg->value_type);
                funccall_arg = dynamic_cast<ast_value*>(funccall_arg->sibling);
            }

            for (size_t tempindex = 0; tempindex < template_args.size(); tempindex++)
            {
                auto& pending_template_arg = template_args[tempindex];

                if (!pending_template_arg)
                {
                    for (size_t index = 0;
                        index < real_argument_types.size() &&
                        index < calling_function_define->value_type->argument_types.size();
                        index++)
                    {
                        fully_update_type(calling_function_define->value_type->argument_types[index], false,
                            calling_function_define->template_type_name_list);
                        //fully_update_type(real_argument_types[index], false); // USELESS

                        if ((pending_template_arg = analyze_template_derivation(
                            calling_function_define->template_type_name_list[tempindex],
                            calling_function_define->template_type_name_list,
                            calling_function_define->value_type->argument_types[index],
                            updated_args_types[index] ? updated_args_types[index] : real_argument_types[index]
                        )))
                        {
                            if (pending_template_arg->is_pure_pending() ||
                                (pending_template_arg->search_from_global_namespace == false
                                    && pending_template_arg->scope_namespaces.empty()
                                    && calling_function_define->template_type_name_list[tempindex]
                                    == pending_template_arg->type_name))
                                pending_template_arg = nullptr;
                        }

                        if (pending_template_arg)
                            break;
                    }
                }
            }
            // TODO; Some of template arguments might not able to judge if argument has `auto` type.
            // Update auto type here and re-check template
            judge_auto_type_in_funccall(a_value_funccall, judge_auto_call_func_located_scope, true, calling_function_define, &template_args);

            // After judge, args might be changed, we need re-try to get them.
            real_argument_types.clear();
            funccall_arg = dynamic_cast<ast_value*>(a_value_funccall->arguments->children);
            while (funccall_arg)
            {
                if (auto* fake_unpack_value = dynamic_cast<ast_fakevalue_unpacked_args*>(funccall_arg))
                {
                    if (fake_unpack_value->unpacked_pack->value_type->is_tuple())
                    {
                        wo_assert(fake_unpack_value->expand_count >= 0);

                        size_t expand_count = std::min((size_t)fake_unpack_value->expand_count,
                            fake_unpack_value->unpacked_pack->value_type->template_arguments.size());

                        for (size_t i = 0; i < expand_count; ++i)
                            real_argument_types.push_back(
                                fake_unpack_value->unpacked_pack->value_type->template_arguments[i]);
                    }
                }
                else
                    real_argument_types.push_back(funccall_arg->value_type);
                funccall_arg = dynamic_cast<ast_value*>(funccall_arg->sibling);
            }

            for (size_t tempindex = 0; tempindex < template_args.size(); tempindex++)
            {
                auto& pending_template_arg = template_args[tempindex];

                if (!pending_template_arg)
                {
                    for (size_t index = 0;
                        index < real_argument_types.size() &&
                        index < calling_function_define->value_type->argument_types.size();
                        index++)
                    {
                        fully_update_type(calling_function_define->value_type->argument_types[index], false,
                            calling_function_define->template_type_name_list);
                        //fully_update_type(real_argument_types[index], false); // USELESS

                        pending_template_arg = analyze_template_derivation(
                            calling_function_define->template_type_name_list[tempindex],
                            calling_function_define->template_type_name_list,
                            calling_function_define->value_type->argument_types[index],
                            real_argument_types[index]
                        );

                        if (pending_template_arg)
                            break;
                    }
                }
            }

            if (std::find(template_args.begin(), template_args.end(), nullptr) != template_args.end())
            {
                lang_anylizer->lang_error(lexer::errorlevel::error, a_value_funccall, WO_ERR_FAILED_TO_DECIDE_ALL_TEMPLATE_ARGS);

                // Skip when invoke anonymous function.
                if (calling_function_define->function_name != nullptr)
                {
                    lang_anylizer->lang_error(lexer::errorlevel::infom, calling_function_define, WO_INFO_ITEM_IS_DEFINED_HERE,
                        calling_function_define->function_name->c_str());
                }
            }
            // failed getting each of template args, abandon this one
            else
            {
                for (auto* templ_arg : template_args)
                {
                    fully_update_type(templ_arg, false);
                    if (templ_arg->is_pending() && !templ_arg->is_hkt())
                    {
                        lang_anylizer->lang_error(lexer::errorlevel::error, a_value_funccall, WO_ERR_UNKNOWN_TYPE,
                            templ_arg->get_type_name(false).c_str());
                        goto failed_to_judge_template_params;
                    }
                }

                //tara~ get analyze_pass_template_reification_func 
                calling_function_define = analyze_pass_template_reification_func(calling_function_define, template_args);
                a_value_funccall->called_func = calling_function_define;
            }
        failed_to_judge_template_params:;
        }
        // End of template judge

        judge_auto_type_in_funccall(a_value_funccall, judge_auto_call_func_located_scope, true, nullptr, nullptr);

        analyze_pass2(a_value_funccall->called_func);
        if (ast_symbolable_base* symbase = dynamic_cast<ast_symbolable_base*>(a_value_funccall->called_func))
            check_function_where_constraint(a_value_funccall, lang_anylizer, symbase);

        if (!a_value_funccall->called_func->value_type->is_pending()
            && a_value_funccall->called_func->value_type->is_function())
            a_value_funccall->value_type->set_type(a_value_funccall->called_func->value_type->get_return_type());

        if (a_value_funccall->called_func
            && a_value_funccall->called_func->value_type->is_function()
            && a_value_funccall->called_func->value_type->is_pending())
        {
            auto* funcsymb = dynamic_cast<ast_value_function_define*>(a_value_funccall->called_func);

            if (funcsymb && funcsymb->auto_adjust_return_type)
            {
                // If call a pending type function. it means the function's type dudes may fail, mark void to continue..
                if (funcsymb->has_return_value)
                    lang_anylizer->lang_error(lexer::errorlevel::error, funcsymb,
                        WO_ERR_CANNOT_DERIV_FUNCS_RET_TYPE,
                        wo::str_to_wstr(funcsymb->get_ir_func_signature_tag()).c_str());

                funcsymb->value_type->get_return_type()->set_type_with_name(WO_PSTR(void));
                funcsymb->auto_adjust_return_type = false;
            }

        }

        bool failed_to_call_cur_func = false;

        if (a_value_funccall->called_func
            && a_value_funccall->called_func->value_type->is_function())
        {
            auto* real_args = a_value_funccall->arguments->children;
            a_value_funccall->arguments->remove_all_childs();

            for (auto a_type_index = a_value_funccall->called_func->value_type->argument_types.begin();
                a_type_index != a_value_funccall->called_func->value_type->argument_types.end();)
            {
                bool donot_move_forward = false;
                if (!real_args)
                {
                    // default arg mgr here, now just kill
                    failed_to_call_cur_func = true;
                    lang_anylizer->lang_error(
                        lexer::errorlevel::error, a_value_funccall,
                        WO_ERR_ARGUMENT_TOO_FEW,
                        a_value_funccall->called_func->value_type->get_type_name(false).c_str());
                    break;
                }
                else
                {
                    auto tmp_sib = real_args->sibling;

                    real_args->parent = nullptr;
                    real_args->sibling = nullptr;

                    auto* arg_val = dynamic_cast<ast_value*>(real_args);
                    real_args = tmp_sib;

                    if (auto* a_fakevalue_unpack_args = dynamic_cast<ast_fakevalue_unpacked_args*>(arg_val))
                    {
                        a_value_funccall->arguments->add_child(a_fakevalue_unpack_args);

                        auto ecount = a_fakevalue_unpack_args->expand_count;
                        if (ast_fakevalue_unpacked_args::UNPACK_ALL_ARGUMENT == ecount)
                        {
                            // all in!!!
                            // If unpacking type is tuple. cannot run here.
                            if (a_fakevalue_unpack_args->unpacked_pack->value_type->is_tuple())
                            {
                                auto* unpacking_tuple_type = a_fakevalue_unpack_args->unpacked_pack->value_type;
                                ecount = (int32_t)unpacking_tuple_type->template_arguments.size();
                                a_fakevalue_unpack_args->expand_count = ecount;
                            }
                            else
                            {
                                ecount =
                                    (int32_t)(
                                        a_value_funccall->called_func->value_type->argument_types.end()
                                        - a_type_index);
                                if (a_value_funccall->called_func->value_type->is_variadic_function_type)
                                    a_fakevalue_unpack_args->expand_count = -ecount;
                                else
                                    a_fakevalue_unpack_args->expand_count = ecount;
                            }
                        }

                        size_t unpack_tuple_index = 0;
                        while (ecount)
                        {
                            if (a_type_index != a_value_funccall->called_func->value_type->argument_types.end())
                            {
                                if (a_fakevalue_unpack_args->unpacked_pack->value_type->is_tuple())
                                {
                                    // Varify tuple type here.
                                    auto* unpacking_tuple_type = a_fakevalue_unpack_args->unpacked_pack->value_type;
                                    if (unpacking_tuple_type->template_arguments.size() <= unpack_tuple_index)
                                    {
                                        // There is no enough value for tuple to expand. match failed!
                                        failed_to_call_cur_func = true;
                                        lang_anylizer->lang_error(lexer::errorlevel::error,
                                            a_value_funccall, WO_ERR_ARGUMENT_TOO_MANY, a_value_funccall->called_func->value_type->get_type_name(false).c_str());
                                        break;
                                    }
                                    else if (!(*a_type_index)->accept_type(unpacking_tuple_type->template_arguments[unpack_tuple_index], false, true))
                                    {
                                        failed_to_call_cur_func = true;
                                        lang_anylizer->lang_error(lexer::errorlevel::error,
                                            a_value_funccall, WO_ERR_TYPE_CANNOT_BE_CALL, a_value_funccall->called_func->value_type->get_type_name(false).c_str());
                                        break;
                                    }
                                    else
                                        // ok, do nothing.
                                        ++unpack_tuple_index;
                                }
                                a_type_index++;
                            }
                            else if (a_value_funccall->called_func->value_type->is_variadic_function_type)
                            {
                                // is variadic
                                break;
                            }
                            else
                            {
                                failed_to_call_cur_func = true;
                                lang_anylizer->lang_error(lexer::errorlevel::error,
                                    a_value_funccall, WO_ERR_ARGUMENT_TOO_MANY, a_value_funccall->called_func->value_type->get_type_name(false).c_str());
                                break;
                            }
                            ecount--;
                        }

                        if (failed_to_call_cur_func)
                            break;

                        donot_move_forward = true;
                    }
                    else if (dynamic_cast<ast_value_takeplace*>(arg_val))
                    {
                        // Do nothing
                    }
                    else
                    {
                        if (!(*a_type_index)->accept_type(arg_val->value_type, false, true))
                        {
                            failed_to_call_cur_func = true;
                            lang_anylizer->lang_error(lexer::errorlevel::error, arg_val, WO_ERR_SHOULD_BE_TYPE_BUT_GET_UNEXCEPTED_TYPE,
                                (*a_type_index)->get_type_name(false, false).c_str(),
                                arg_val->value_type->get_type_name(false, true).c_str());
                            break;
                        }
                        else
                        {
                            a_value_funccall->arguments->add_child(arg_val);
                        }
                    }
                }
                if (!donot_move_forward)
                    a_type_index++;
            }
            if (a_value_funccall->called_func->value_type->is_variadic_function_type)
            {
                while (real_args)
                {
                    auto tmp_sib = real_args->sibling;

                    real_args->parent = nullptr;
                    real_args->sibling = nullptr;

                    a_value_funccall->arguments->add_child(real_args);
                    real_args = tmp_sib;
                }
            }
            if (real_args)
            {
                if (ast_fakevalue_unpacked_args* unpackval = dynamic_cast<ast_fakevalue_unpacked_args*>(real_args))
                {
                    // TODO MARK NOT NEED EXPAND HERE
                    if (unpackval->unpacked_pack->value_type->is_tuple())
                    {
                        size_t tuple_arg_sz = unpackval->unpacked_pack->value_type->template_arguments.size();

                        if (tuple_arg_sz != 0)
                        {
                            failed_to_call_cur_func = true;
                            lang_anylizer->lang_error(lexer::errorlevel::error, a_value_funccall,
                                WO_ERR_ARGUMENT_TOO_MANY,
                                a_value_funccall->called_func->value_type->get_type_name(false).c_str());
                        }
                    }
                }
                else
                {
                    failed_to_call_cur_func = true;
                    lang_anylizer->lang_error(lexer::errorlevel::error, a_value_funccall,
                        WO_ERR_ARGUMENT_TOO_MANY,
                        a_value_funccall->called_func->value_type->get_type_name(false).c_str());
                }
            }
        }
        else
        {
            failed_to_call_cur_func = true;
            lang_anylizer->lang_error(lexer::errorlevel::error, a_value_funccall, WO_ERR_TYPE_CANNOT_BE_CALL,
                a_value_funccall->called_func->value_type->get_type_name(false).c_str());
        }

        if (failed_to_call_cur_func)
            a_value_funccall->value_type->set_type_with_name(WO_PSTR(pending));
    }
    WO_PASS2(ast_value_typeid)
    {
        auto* ast_value_typeid = WO_AST();

        fully_update_type(ast_value_typeid->type, false);

        if (auto opthash = lang::get_typing_hash_after_pass1(ast_value_typeid->type))
        {
            ast_value_typeid->constant_value.set_integer(opthash.value());
            ast_value_typeid->is_constant = true;
        }
    }
}