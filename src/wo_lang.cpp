#include "wo_lang.hpp"

namespace wo
{
    using namespace ast;

#define WO_PASS1(NODETYPE) bool lang::pass1_##NODETYPE (ast::NODETYPE* astnode)
#define WO_AST() astnode; if (!astnode)return false

    WO_PASS1(ast_namespace)
    {
        auto* a_namespace = WO_AST();

        begin_namespace(a_namespace);
        a_namespace->add_child(a_namespace->in_scope_sentence);
        grammar::ast_base* child = a_namespace->in_scope_sentence->children;
        while (child)
        {
            analyze_pass1(child);
            child = child->sibling;
        }
        end_namespace();

        return true;
    }
    WO_PASS1(ast_varref_defines)
    {
        auto* a_varref_defs = WO_AST();
        a_varref_defs->located_function = in_function();
        for (auto& varref : a_varref_defs->var_refs)
            analyze_pattern_in_pass1(varref.pattern, a_varref_defs->declear_attribute, varref.init_val);
        return true;
    }
    WO_PASS1(ast_value_binary)
    {
        auto* a_value_bin = WO_AST();

        analyze_pass1(a_value_bin->left);
        analyze_pass1(a_value_bin->right);

        ast_type* a_value_binary_target_type = nullptr;
        if (!a_value_bin->left->value_type->is_builtin_basic_type()
            || !a_value_bin->right->value_type->is_builtin_basic_type())
            // IS CUSTOM TYPE, DELAY THE TYPE CALC TO PASS2
            ;
        else
            a_value_binary_target_type = ast_value_binary::binary_upper_type_with_operator(
                a_value_bin->left->value_type,
                a_value_bin->right->value_type,
                a_value_bin->operate);

        if (nullptr == a_value_binary_target_type)
        {
            ast_value_funccall* try_operator_func_overload = new ast_value_funccall();
            try_operator_func_overload->copy_source_info(a_value_bin);

            try_operator_func_overload->try_invoke_operator_override_function = true;
            try_operator_func_overload->arguments = new ast_list();
            try_operator_func_overload->value_type;

            try_operator_func_overload->called_func = new ast_value_variable(
                wstring_pool::get_pstr(std::wstring(L"operator ") + lexer::lex_is_operate_type(a_value_bin->operate)));
            try_operator_func_overload->called_func->copy_source_info(a_value_bin);

            try_operator_func_overload->directed_value_from = a_value_bin->left;

            ast_value* arg1 = dynamic_cast<ast_value*>(a_value_bin->left->instance());
            try_operator_func_overload->arguments->add_child(arg1);

            ast_value* arg2 = dynamic_cast<ast_value*>(a_value_bin->right->instance());
            try_operator_func_overload->arguments->add_child(arg2);

            a_value_bin->overrided_operation_call = try_operator_func_overload;
            analyze_pass1(a_value_bin->overrided_operation_call);

            if (!a_value_bin->overrided_operation_call->value_type->is_pending())
                a_value_bin->value_type->set_type(a_value_bin->overrided_operation_call->value_type);
        }
        else
            a_value_bin->value_type->set_type(a_value_binary_target_type);
        return true;
    }
    WO_PASS1(ast_value_index)
    {
        auto* a_value_idx = WO_AST();

        analyze_pass1(a_value_idx->from);
        analyze_pass1(a_value_idx->index);

        if (!a_value_idx->from->value_type->struct_member_index.empty())
        {
            if (a_value_idx->index->is_constant && a_value_idx->index->value_type->is_string())
            {
                if (auto fnd =
                    a_value_idx->from->value_type->struct_member_index.find(
                        wstring_pool::get_pstr(str_to_wstr(*a_value_idx->index->get_constant_value().string)));
                    fnd != a_value_idx->from->value_type->struct_member_index.end())
                {
                    if (!fnd->second.member_type->is_pending())
                    {
                        a_value_idx->value_type->set_type(fnd->second.member_type);
                        a_value_idx->struct_offset = fnd->second.offset;
                    }

                }
            }
        }
        else
        {
            if (a_value_idx->from->value_type->is_tuple())
            {
                if (a_value_idx->index->is_constant && a_value_idx->index->value_type->is_integer())
                {
                    // Index tuple must with constant integer.
                    auto index = a_value_idx->index->get_constant_value().integer;
                    if ((size_t)index < a_value_idx->from->value_type->template_arguments.size() && index >= 0)
                    {
                        a_value_idx->value_type->set_type(a_value_idx->from->value_type->template_arguments[index]);
                        a_value_idx->struct_offset = (uint16_t)index;
                    }
                    else
                        wo_assert(a_value_idx->value_type->is_pure_pending());
                }
            }
            else if (a_value_idx->from->value_type->is_string())
            {
                a_value_idx->value_type->set_type_with_name(WO_PSTR(char));
            }
            else if (!a_value_idx->from->value_type->is_pending())
            {
                if (a_value_idx->from->value_type->is_array() || a_value_idx->from->value_type->is_vec())
                    a_value_idx->value_type->set_type(a_value_idx->from->value_type->template_arguments[0]);
                else if (a_value_idx->from->value_type->is_dict() || a_value_idx->from->value_type->is_map())
                    a_value_idx->value_type->set_type(a_value_idx->from->value_type->template_arguments[1]);
                else
                    a_value_idx->value_type->set_type_with_name(WO_PSTR(dynamic));
            }
        }
        return true;
    }
    WO_PASS1(ast_value_assign)
    {
        auto* a_value_assi = WO_AST();

        analyze_pass1(a_value_assi->left);
        analyze_pass1(a_value_assi->right);

        auto lsymb = dynamic_cast<ast_value_symbolable_base*>(a_value_assi->left);
        if (lsymb && lsymb->symbol && !lsymb->symbol->is_template_symbol)
        {
            // If symbol is template variable, delay the type calc.
            a_value_assi->value_type->set_type(a_value_assi->left->value_type);
        }
        return true;
    }
    WO_PASS1(ast_value_logical_binary)
    {
        auto* a_value_logic_bin = WO_AST();
        analyze_pass1(a_value_logic_bin->left);
        analyze_pass1(a_value_logic_bin->right);

        bool has_default_op = false;

        if (a_value_logic_bin->left->value_type->is_builtin_basic_type()
            && a_value_logic_bin->right->value_type->is_builtin_basic_type())
        {
            if (a_value_logic_bin->operate == +lex_type::l_lor || a_value_logic_bin->operate == +lex_type::l_land)
            {
                if (a_value_logic_bin->left->value_type->is_bool() && a_value_logic_bin->right->value_type->is_bool())
                    has_default_op = true;
            }
            else if ((a_value_logic_bin->left->value_type->is_integer()
                || a_value_logic_bin->left->value_type->is_handle()
                || a_value_logic_bin->left->value_type->is_real()
                || a_value_logic_bin->left->value_type->is_string()
                || a_value_logic_bin->left->value_type->is_gchandle())
                && a_value_logic_bin->left->value_type->is_same(a_value_logic_bin->right->value_type, false, true))
                has_default_op = true;
        }
        if (!has_default_op)
        {
            a_value_logic_bin->value_type->set_type_with_name(WO_PSTR(pending));

            ast_value_funccall* try_operator_func_overload = new ast_value_funccall();
            try_operator_func_overload->copy_source_info(a_value_logic_bin);
            try_operator_func_overload->try_invoke_operator_override_function = true;
            try_operator_func_overload->arguments = new ast_list();

            try_operator_func_overload->called_func = new ast_value_variable(
                wstring_pool::get_pstr(std::wstring(L"operator ") + lexer::lex_is_operate_type(a_value_logic_bin->operate)));
            try_operator_func_overload->called_func->copy_source_info(a_value_logic_bin);

            try_operator_func_overload->directed_value_from = a_value_logic_bin->left;

            ast_value* arg1 = dynamic_cast<ast_value*>(a_value_logic_bin->left->instance());
            try_operator_func_overload->arguments->add_child(arg1);

            ast_value* arg2 = dynamic_cast<ast_value*>(a_value_logic_bin->right->instance());
            try_operator_func_overload->arguments->add_child(arg2);

            a_value_logic_bin->overrided_operation_call = try_operator_func_overload;
            analyze_pass1(a_value_logic_bin->overrided_operation_call);

            if (!a_value_logic_bin->overrided_operation_call->value_type->is_pending())
                a_value_logic_bin->value_type->set_type(a_value_logic_bin->overrided_operation_call->value_type);
        }
        else
            a_value_logic_bin->value_type->set_type_with_name(WO_PSTR(bool));

        return true;
    }
    WO_PASS1(ast_value_variable)
    {
        auto* a_value_var = WO_AST();
        auto* sym = find_value_in_this_scope(a_value_var);
        if (sym)
        {
            if (sym->type == lang_symbol::symbol_type::variable)
            {
                if (!sym->is_template_symbol)
                {
                    a_value_var->value_type->set_type(sym->variable_value->value_type);
                    if (sym->type == lang_symbol::symbol_type::variable && sym->decl == identifier_decl::MUTABLE)
                        a_value_var->value_type->set_is_mutable(true);
                    else
                        a_value_var->value_type->set_is_mutable(false);
                }
            }
        }
        for (auto* a_type : a_value_var->template_reification_args)
        {
            if (a_type->is_pending())
            {
                // ready for update..
                fully_update_type(a_type, true);
            }
        }
        return true;
    }
    WO_PASS1(ast_value_type_cast)
    {
        auto* a_value_cast = WO_AST();
        analyze_pass1(a_value_cast->_be_cast_value_node);
        fully_update_type(a_value_cast->value_type, true);
        return true;
    }
    WO_PASS1(ast_value_type_judge)
    {
        auto* ast_value_judge = WO_AST();
        analyze_pass1(ast_value_judge->_be_cast_value_node);
        fully_update_type(ast_value_judge->value_type, true);
        return true;
    }
    WO_PASS1(ast_value_type_check)
    {
        auto* ast_value_check = WO_AST();

        if (ast_value_check->aim_type->is_pending())
        {
            // ready for update..
            fully_update_type(ast_value_check->aim_type, true);
        }

        analyze_pass1(ast_value_check->_be_check_value_node);

        ast_value_check->update_constant_value(lang_anylizer);
        return true;
    }
    WO_PASS1(ast_value_function_define)
    {
        auto* a_value_func = WO_AST();
        a_value_func->this_func_scope = begin_function(a_value_func);
        if (!a_value_func->is_template_define)
        {
            auto arg_child = a_value_func->argument_list->children;
            while (arg_child)
            {
                if (ast_value_arg_define* argdef = dynamic_cast<ast_value_arg_define*>(arg_child))
                {
                    if (!argdef->symbol)
                    {
                        if (argdef->value_type->is_custom())
                        {
                            // ready for update..
                            fully_update_type(argdef->value_type, true);
                        }
                        argdef->value_type->set_is_mutable(false);

                        if (!argdef->symbol)
                        {
                            argdef->symbol = define_variable_in_this_scope(argdef->arg_name, argdef, argdef->declear_attribute, template_style::NORMAL, argdef->decl);
                            argdef->symbol->is_argument = true;
                        }
                    }
                }
                else
                {
                    wo_assert(dynamic_cast<ast_token*>(arg_child));
                }

                arg_child = arg_child->sibling;
            }

            fully_update_type(a_value_func->value_type, true);

            // update return type info for
            // func xxx(var x:int): typeof(x)

            if (a_value_func->where_constraint)
            {
                a_value_func->where_constraint->binded_func_define = a_value_func;
                analyze_pass1(a_value_func->where_constraint);
            }

            if (a_value_func->where_constraint == nullptr || a_value_func->where_constraint->accept)
            {
                if (a_value_func->in_function_sentence)
                {
                    analyze_pass1(a_value_func->in_function_sentence);
                }

                if (a_value_func->externed_func_info)
                {
                    if (a_value_func->externed_func_info->load_from_lib != L"")
                    {
                        if (!a_value_func->externed_func_info->externed_func)
                        {
                            // Load lib,
                            wo_assert(a_value_func->source_file != nullptr);
                            a_value_func->externed_func_info->externed_func =
                                rslib_extern_symbols::get_lib_symbol(
                                    wstr_to_str(*a_value_func->source_file).c_str(),
                                    wstr_to_str(a_value_func->externed_func_info->load_from_lib).c_str(),
                                    wstr_to_str(a_value_func->externed_func_info->symbol_name).c_str(),
                                    extern_libs);
                            if (a_value_func->externed_func_info->externed_func)
                                a_value_func->is_constant = true;
                            else
                                lang_anylizer->lang_error(0x0000, a_value_func, WO_ERR_CANNOT_FIND_EXT_SYM_IN_LIB,
                                    a_value_func->externed_func_info->symbol_name.c_str(),
                                    a_value_func->externed_func_info->load_from_lib.c_str());
                        }
                        else
                            a_value_func->is_constant = true;
                    }
                }
                else if (!a_value_func->has_return_value && a_value_func->value_type->get_return_type()->type_name == WO_PSTR(pending))
                {
                    // This function has no return, set it as void
                    wo_assert(a_value_func->value_type->is_complex());
                    a_value_func->value_type->complex_type->set_type_with_name(WO_PSTR(void));
                }
            }
            else
                a_value_func->value_type->complex_type->set_type_with_name(WO_PSTR(pending));
        }

        if (a_value_func->externed_func_info)
        {
            // FIX 220829: If a extern function with template, wo should check it.
            //             Make sure if this function has different arg count, we should set 'tc'
            extern_symb_func_definee[a_value_func->externed_func_info->externed_func]
                .push_back(a_value_func);
        }

        end_function();
        return true;
    }
    WO_PASS1(ast_fakevalue_unpacked_args)
    {
        auto* a_fakevalue_unpacked_args = WO_AST();
        analyze_pass1(a_fakevalue_unpacked_args->unpacked_pack);
        return true;
    }
    WO_PASS1(ast_value_funccall)
    {
        auto* a_value_funccall = WO_AST();
        if (auto* symb_callee = dynamic_cast<ast_value_variable*>(a_value_funccall->called_func))
        {
            symb_callee->is_auto_judge_function_overload = true; // Current may used for auto judge for function invoke, here to skip!
            if (a_value_funccall->directed_value_from)
            {
                if (!symb_callee->search_from_global_namespace/*
                    && symb_callee->scope_namespaces.empty()*/)
                {
                    analyze_pass1(a_value_funccall->directed_value_from);

                    symb_callee->directed_function_call = true;

                    a_value_funccall->callee_symbol_in_type_namespace = new ast_value_variable(symb_callee->var_name);
                    a_value_funccall->callee_symbol_in_type_namespace->copy_source_info(a_value_funccall);
                    a_value_funccall->callee_symbol_in_type_namespace->search_from_global_namespace = true;
                    a_value_funccall->callee_symbol_in_type_namespace->searching_begin_namespace_in_pass2 = now_scope();
                    a_value_funccall->callee_symbol_in_type_namespace->scope_namespaces = symb_callee->scope_namespaces;
                    // a_value_funccall->callee_symbol_in_type_namespace wiil search in pass2..
                }
            }
        }

        analyze_pass1(a_value_funccall->called_func);
        analyze_pass1(a_value_funccall->arguments);

        // function call should be 'pending' type, then do override judgement in pass2
        return true;
    }
    WO_PASS1(ast_value_array)
    {
        auto* a_value_arr = WO_AST();
        analyze_pass1(a_value_arr->array_items);

        if (a_value_arr->value_type->is_pending() && !a_value_arr->value_type->is_custom())
        {
            auto* arr_elem_type = a_value_arr->value_type->template_arguments[0];
            arr_elem_type->set_type_with_name(WO_PSTR(nothing));

            ast_value* val = dynamic_cast<ast_value*>(a_value_arr->array_items->children);
            if (val)
            {
                if (!val->value_type->is_pending())
                    arr_elem_type->set_type(val->value_type);
                else
                    arr_elem_type->set_type_with_name(WO_PSTR(pending));
            }

            while (val)
            {
                if (val->value_type->is_pending())
                {
                    arr_elem_type->set_type_with_name(WO_PSTR(pending));
                    break;
                }
                if (!arr_elem_type->accept_type(val->value_type, false)
                    && !arr_elem_type->set_mix_types(val->value_type, false))
                {
                    arr_elem_type->set_type_with_name(WO_PSTR(pending));
                    break;
                }
                val = dynamic_cast<ast_value*>(val->sibling);
            }
        }
        return true;
    }
    WO_PASS1(ast_value_mapping)
    {
        auto* a_value_map = WO_AST();
        analyze_pass1(a_value_map->mapping_pairs);

        if (a_value_map->value_type->is_pending() && !a_value_map->value_type->is_custom())
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
                if (!decide_map_key_type->accept_type(map_pair->key->value_type, false)
                    && !decide_map_key_type->set_mix_types(map_pair->key->value_type, false))
                {
                    decide_map_key_type->set_type_with_name(WO_PSTR(pending));
                    decide_map_val_type->set_type_with_name(WO_PSTR(pending));
                    break;
                }
                if (!decide_map_val_type->accept_type(map_pair->val->value_type, false)
                    && !decide_map_val_type->set_mix_types(map_pair->val->value_type, false))
                {
                    decide_map_key_type->set_type_with_name(WO_PSTR(pending));
                    decide_map_val_type->set_type_with_name(WO_PSTR(pending));
                    break;
                }
                map_pair = dynamic_cast<ast_mapping_pair*>(map_pair->sibling);
            }
        }
        return true;
    }
    WO_PASS1(ast_value_indexed_variadic_args)
    {
        auto* a_value_variadic_args_idx = WO_AST();
        analyze_pass1(a_value_variadic_args_idx->argindex);

        return true;
    }
    WO_PASS1(ast_return)
    {
        auto* a_ret = WO_AST();

        auto* located_function_scope = in_function();
        if (!located_function_scope)
            lang_anylizer->lang_error(0x0000, a_ret, WO_ERR_CANNOT_DO_RET_OUSIDE_FUNC);
        else
        {
            a_ret->located_function = located_function_scope->function_node;
            a_ret->located_function->has_return_value = true;

            wo_assert(a_ret->located_function->value_type->is_complex());

            if (a_ret->return_value)
            {
                analyze_pass1(a_ret->return_value);

                // NOTE: DONOT JUDGE FUNCTION'S RETURN VAL TYPE IN PASS1 TO AVOID TYPE MIXED IN CONSTEXPR IF

                if (a_ret->located_function->auto_adjust_return_type)
                {
                    if (a_ret->located_function->delay_adjust_return_type
                        && a_ret->return_value->value_type->is_pending() == false)
                    {
                        auto* func_return_type = a_ret->located_function->value_type->get_return_type();

                        if (func_return_type->is_pending())
                            a_ret->located_function->value_type->set_ret_type(a_ret->return_value->value_type);
                        else
                        {
                            if (!func_return_type->accept_type(a_ret->return_value->value_type, false))
                            {
                                if (!func_return_type->set_mix_types(a_ret->return_value->value_type, false))
                                {
                                    // current function might has constexpr if, set delay_return_type_judge flag
                                    func_return_type->set_type_with_name(WO_PSTR(pending));
                                    a_ret->located_function->delay_adjust_return_type = true;
                                }
                            }
                        }
                    }
                }
            }
            else
            {
                if (located_function_scope->function_node->auto_adjust_return_type)
                {
                    if (located_function_scope->function_node->value_type->is_pending())
                    {
                        located_function_scope->function_node->value_type->get_return_type()->set_type_with_name(WO_PSTR(void));
                        located_function_scope->function_node->auto_adjust_return_type = false;
                    }
                    else
                    {
                        lang_anylizer->lang_error(0x0000, a_ret, WO_ERR_CANNOT_RET_TYPE_AND_TYPE_AT_SAME_TIME, L"void", located_function_scope->function_node->value_type->type_name->c_str());
                    }
                }
                else
                {
                    if (!located_function_scope->function_node->value_type->get_return_type()->is_void())
                        lang_anylizer->lang_error(0x0000, a_ret, WO_ERR_CANNOT_RET_TYPE_AND_TYPE_AT_SAME_TIME, L"void", located_function_scope->function_node->value_type->type_name->c_str());
                }
            }
        }
        return true;
    }
    WO_PASS1(ast_sentence_block)
    {
        auto* a_sentence_blk = WO_AST();
        this->begin_scope(a_sentence_blk);
        analyze_pass1(a_sentence_blk->sentence_list);
        this->end_scope();

        return true;
    }
    WO_PASS1(ast_if)
    {
        auto* ast_if_sentence = WO_AST();
        analyze_pass1(ast_if_sentence->judgement_value);

        if (ast_if_sentence->judgement_value->is_constant)
        {
            ast_if_sentence->is_constexpr_if = true;

            if (ast_if_sentence->judgement_value->get_constant_value().integer)
                analyze_pass1(ast_if_sentence->execute_if_true);
            else if (ast_if_sentence->execute_else)
                analyze_pass1(ast_if_sentence->execute_else);
        }
        else
        {
            analyze_pass1(ast_if_sentence->execute_if_true);
            if (ast_if_sentence->execute_else)
                analyze_pass1(ast_if_sentence->execute_else);
        }
        return true;
    }
    WO_PASS1(ast_while)
    {
        auto* ast_while_sentence = WO_AST();
        if (ast_while_sentence->judgement_value != nullptr)
            analyze_pass1(ast_while_sentence->judgement_value);
        analyze_pass1(ast_while_sentence->execute_sentence);
        return true;
    }
    WO_PASS1(ast_forloop)
    {
        auto* a_forloop = WO_AST();
        begin_scope(a_forloop);

        analyze_pass1(a_forloop->pre_execute);
        analyze_pass1(a_forloop->judgement_expr);
        analyze_pass1(a_forloop->execute_sentences);
        analyze_pass1(a_forloop->after_execute);

        end_scope();
        return true;
    }
    WO_PASS1(ast_value_unary)
    {
        auto* a_value_unary = WO_AST();
        analyze_pass1(a_value_unary->val);

        if (a_value_unary->operate == +lex_type::l_lnot)
            a_value_unary->value_type->set_type_with_name(WO_PSTR(bool));
        a_value_unary->value_type->set_type(a_value_unary->val->value_type);
        return true;
    }
    WO_PASS1(ast_mapping_pair)
    {
        auto* a_mapping_pair = WO_AST();
        analyze_pass1(a_mapping_pair->key);
        analyze_pass1(a_mapping_pair->val);
        return true;
    }
    WO_PASS1(ast_using_namespace)
    {
        // do using namespace op..
        // do check..

        // NOTE: Because of function's head insert a naming check list, this check is useless.

        //auto* parent_child = a_using_namespace->parent->children;
        //while (parent_child)
        //{
        //    if (auto* using_namespace_ch = dynamic_cast<ast_using_namespace*>(parent_child))
        //    {
        //        if (using_namespace_ch == a_using_namespace)
        //            break;
        //    }
        //    else
        //    {
        //        
        //        // lang_anylizer->lang_error(0x0000, a_using_namespace, WO_ERR_ERR_PLACE_FOR_USING_NAMESPACE);
        //        break;
        //    }
        //    parent_child = parent_child->sibling;
        //}
        auto* a_using_namespace = WO_AST();
        now_scope()->used_namespace.push_back(a_using_namespace);
        return true;
    }
    WO_PASS1(ast_using_type_as)
    {
        auto* a_using_type_as = WO_AST();

        if (a_using_type_as->old_type->typefrom == nullptr
            || a_using_type_as->template_type_name_list.empty())
            fully_update_type(a_using_type_as->old_type, true, a_using_type_as->template_type_name_list);

        auto* typing_symb = define_type_in_this_scope(a_using_type_as, a_using_type_as->old_type, a_using_type_as->declear_attribute);
        typing_symb->apply_template_setting(a_using_type_as);
        return true;
    }
    WO_PASS1(ast_foreach)
    {
        auto* a_foreach = WO_AST();
        begin_scope(a_foreach);

        analyze_pass1(a_foreach->used_iter_define);
        analyze_pass1(a_foreach->loop_sentences);
        a_foreach->loop_sentences->copy_source_info(a_foreach);

        end_scope();
        return true;
    }
    WO_PASS1(ast_check_type_with_naming_in_pass2)
    {
        auto* a_check_naming = WO_AST();
        fully_update_type(a_check_naming->template_type, true);
        fully_update_type(a_check_naming->naming_const, true);

        for (auto& namings : a_check_naming->template_type->template_impl_naming_checking)
        {
            if (a_check_naming->naming_const->is_pending() || namings->is_pending())
                break; // Continue..
            if (namings->is_same(a_check_naming->naming_const, false, false))
                goto checking_naming_end;
        }
    checking_naming_end:;

        return true;
    }
    WO_PASS1(ast_union_make_option_ob_to_cr_and_ret)
    {
        auto* a_union_make_option_ob_to_cr_and_ret = WO_AST();
        if (a_union_make_option_ob_to_cr_and_ret->argument_may_nil)
            analyze_pass1(a_union_make_option_ob_to_cr_and_ret->argument_may_nil);

        return true;
    }
    WO_PASS1(ast_match)
    {
        auto* a_match = WO_AST();
        analyze_pass1(a_match->match_value);
        a_match->match_scope_in_pass = begin_scope(a_match);

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
            now_scope()->used_namespace.push_back(ast_using);
            a_match->has_using_namespace = true;
        }

        auto* cases = a_match->cases->children;
        while (cases)
        {
            ast_match_case_base* match_case = dynamic_cast<ast_match_case_base*>(cases);
            wo_assert(match_case);

            match_case->in_match = a_match;

            cases = cases->sibling;
        }

        analyze_pass1(a_match->cases);

        end_scope();
        return true;
    }
    WO_PASS1(ast_match_union_case)
    {
        auto* a_match_union_case = WO_AST();
        begin_scope(a_match_union_case);
        wo_assert(a_match_union_case->in_match);

        if (ast_pattern_union_value* a_pattern_union_value = dynamic_cast<ast_pattern_union_value*>(a_match_union_case->union_pattern))
        {
            // Cannot pass a_match_union_case->union_pattern by analyze_pass1, we will set template in pass2.
            if (!a_pattern_union_value->union_expr->search_from_global_namespace)
            {
                a_pattern_union_value->union_expr->searching_begin_namespace_in_pass2 = now_scope();
                wo_assert(a_pattern_union_value->union_expr->source_file != nullptr);
            }

            // Calc type in pass2, here just define the variable with ast_value_takeplace
            if (a_pattern_union_value->pattern_arg_in_union_may_nil)
            {
                a_match_union_case->take_place_value_may_nil = new ast_value_takeplace;
                a_match_union_case->take_place_value_may_nil->copy_source_info(a_pattern_union_value->pattern_arg_in_union_may_nil);

                analyze_pattern_in_pass1(a_pattern_union_value->pattern_arg_in_union_may_nil, new ast_decl_attribute, a_match_union_case->take_place_value_may_nil);
            }
        }
        else
            lang_anylizer->lang_error(0x0000, a_match_union_case, WO_ERR_UNEXPECT_PATTERN_CASE);

        analyze_pass1(a_match_union_case->in_case_sentence);

        end_scope();
        return true;
    }
    WO_PASS1(ast_value_make_struct_instance)
    {
        auto* a_value_make_struct_instance = WO_AST();
        analyze_pass1(a_value_make_struct_instance->struct_member_vals);
        return true;
    }
    WO_PASS1(ast_value_make_tuple_instance)
    {
        auto* a_value_make_tuple_instance = WO_AST();
        analyze_pass1(a_value_make_tuple_instance->tuple_member_vals);

        auto* tuple_elems = a_value_make_tuple_instance->tuple_member_vals->children;

        size_t count = 0;
        while (tuple_elems)
        {
            ast_value* val = dynamic_cast<ast_value*>(tuple_elems);
            if (!val->value_type->is_pending())
                a_value_make_tuple_instance->value_type->template_arguments[count]->set_type(val->value_type);
            else
                break;

            tuple_elems = tuple_elems->sibling;
            ++count;
        }

        return true;
    }
    WO_PASS1(ast_struct_member_define)
    {
        auto* a_struct_member_define = WO_AST();

        if (a_struct_member_define->is_value_pair)
            analyze_pass1(a_struct_member_define->member_value_pair);
        else
            fully_update_type(a_struct_member_define->member_type, true);

        return true;
    }
    WO_PASS1(ast_where_constraint)
    {
        auto* a_where_constraint = WO_AST();
        analyze_pass1(a_where_constraint->where_constraint_list);
        return true;
    }
    WO_PASS1(ast_value_trib_expr)
    {
        auto* a_value_trib_expr = WO_AST();
        analyze_pass1(a_value_trib_expr->judge_expr);
        analyze_pass1(a_value_trib_expr->val_if_true);
        analyze_pass1(a_value_trib_expr->val_or);

        a_value_trib_expr->value_type->set_type(a_value_trib_expr->val_if_true->value_type);
        if (!a_value_trib_expr->value_type->set_mix_types(a_value_trib_expr->val_or->value_type, false))
            a_value_trib_expr->value_type->set_type_with_name(WO_PSTR(pending));

        return true;
    }

#define WO_PASS2(NODETYPE) bool lang::pass2_##NODETYPE(ast::NODETYPE* astnode)

    WO_PASS2(ast_mapping_pair)
    {
        auto* a_mapping_pair = WO_AST();
        analyze_pass2(a_mapping_pair->key);
        analyze_pass2(a_mapping_pair->val);

        return true;
    }
    WO_PASS2(ast_return)
    {
        auto* a_ret = WO_AST();
        if (a_ret->return_value)
        {
            analyze_pass2(a_ret->return_value);

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
                                func_return_type->set_type_with_name(WO_PSTR(void));
                                lang_anylizer->lang_error(0x0000, a_ret, WO_ERR_FUNC_RETURN_DIFFERENT_TYPES);
                            }
                        }
                    }
                }
                else
                {
                    if (!func_return_type->is_pending()
                        && !func_return_type->accept_type(a_ret->return_value->value_type, false))
                    {
                        lang_anylizer->lang_error(0x0000, a_ret, WO_ERR_FUNC_RETURN_DIFFERENT_TYPES);
                    }
                }
            }
        }
        else
        {
            if (a_ret->located_function->auto_adjust_return_type)
            {
                if (a_ret->located_function->value_type->is_pending())
                {
                    a_ret->located_function->value_type->get_return_type()->set_type_with_name(WO_PSTR(void));
                    a_ret->located_function->auto_adjust_return_type = false;
                }
                else
                {
                    lang_anylizer->lang_error(0x0000, a_ret, WO_ERR_CANNOT_RET_TYPE_AND_TYPE_AT_SAME_TIME, L"void", a_ret->located_function->value_type->type_name->c_str());
                }
            }
            else
            {
                if (!a_ret->located_function->value_type->get_return_type()->is_void())
                    lang_anylizer->lang_error(0x0000, a_ret, WO_ERR_CANNOT_RET_TYPE_AND_TYPE_AT_SAME_TIME, L"void", a_ret->located_function->value_type->type_name->c_str());
            }
        }
        return true;
    }
    WO_PASS2(ast_sentence_block)
    {
        auto* a_sentence_blk = WO_AST();
        analyze_pass2(a_sentence_blk->sentence_list);
        return true;
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

        return true;
    }
    WO_PASS2(ast_while)
    {
        auto* ast_while_sentence = WO_AST();
        if (ast_while_sentence->judgement_value != nullptr)
            analyze_pass2(ast_while_sentence->judgement_value);
        analyze_pass2(ast_while_sentence->execute_sentence);

        return true;
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

        return true;
    }
    WO_PASS2(ast_foreach)
    {
        auto* a_foreach = WO_AST();
        analyze_pass2(a_foreach->used_iter_define);
        analyze_pass2(a_foreach->loop_sentences);

        return true;
    }
    WO_PASS2(ast_varref_defines)
    {
        auto* a_varref_defs = WO_AST();
        for (auto& varref : a_varref_defs->var_refs)
        {
            analyze_pattern_in_pass2(varref.pattern, varref.init_val);
        }
        return true;
    }
    WO_PASS2(ast_check_type_with_naming_in_pass2)
    {
        auto* a_check_naming = WO_AST();
        fully_update_type(a_check_naming->template_type, false);
        fully_update_type(a_check_naming->naming_const, false);

        // TODO: Support or remove type class?

        return true;
    }
    WO_PASS2(ast_union_make_option_ob_to_cr_and_ret)
    {
        auto* a_union_make_option_ob_to_cr_and_ret = WO_AST();
        if (a_union_make_option_ob_to_cr_and_ret->argument_may_nil)
            analyze_pass2(a_union_make_option_ob_to_cr_and_ret->argument_may_nil);

        return true;
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
                lang_anylizer->lang_error(0x0000, a_match->match_value, WO_ERR_CANNOT_MATCH_SUCH_TYPE,
                    a_match->match_value->value_type->get_type_name(false).c_str());
            }
        }

        analyze_pass2(a_match->cases);

        // Must walk all possiable case, and no repeat case!
        std::set<wo_pstring_t> case_names;
        auto* cases = a_match->cases->children;
        while (cases)
        {
            auto* case_ast = dynamic_cast<ast_match_union_case*>(cases);
            wo_assert(case_ast);

            if (case_names.end() != case_names.find(case_ast->union_pattern->union_expr->var_name))
                lang_anylizer->lang_error(0x0000, case_ast->union_pattern->union_expr, WO_ERR_REPEAT_MATCH_CASE);
            else
                case_names.insert(case_ast->union_pattern->union_expr->var_name);
            cases = cases->sibling;
        }
        if (case_names.size() < a_match->match_value->value_type->struct_member_index.size())
            lang_anylizer->lang_error(0x0000, a_match, WO_ERR_MATCH_CASE_NOT_COMPLETE);

        return true;
    }
    WO_PASS2(ast_match_union_case)
    {
        auto* a_match_union_case = WO_AST();
        wo_assert(a_match_union_case->in_match);
        if (a_match_union_case->in_match && a_match_union_case->in_match->match_value->value_type->is_union())
        {
            if (ast_pattern_union_value* a_pattern_union_value = dynamic_cast<ast_pattern_union_value*>(a_match_union_case->union_pattern))
            {
                if (a_match_union_case->in_match->match_value->value_type->is_pending())
                    lang_anylizer->lang_error(0x0000, a_match_union_case, WO_ERR_UNKNOWN_MATCHING_VAL_TYPE);
                else
                {
                    if (!a_match_union_case->in_match->match_value->value_type->using_type_name->template_arguments.empty())
                    {
                        a_pattern_union_value->union_expr->symbol = find_value_in_this_scope(a_pattern_union_value->union_expr);

                        if (a_pattern_union_value->union_expr->symbol)
                        {
                            std::vector<ast_type*> fact_used_template;

                            auto fnd = a_match_union_case->in_match->match_value->value_type->struct_member_index.find(
                                a_pattern_union_value->union_expr->symbol->name);
                            if (fnd == a_match_union_case->in_match->match_value->value_type->struct_member_index.end())
                            {
                                lang_anylizer->lang_error(0x0000, a_pattern_union_value, WO_ERR_INVALID_ITEM_OF,
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
                                    a_pattern_union_value->union_expr->symbol = analyze_pass_template_reification(
                                        a_pattern_union_value->union_expr,
                                        fact_used_template);
                                else
                                {
                                    wo_assert(a_pattern_union_value->union_expr->symbol->type == lang_symbol::symbol_type::function);

                                    auto final_function = a_pattern_union_value->union_expr->symbol->get_funcdef();

                                    auto* dumped_func = analyze_pass_template_reification(
                                        dynamic_cast<ast_value_function_define*>(final_function),
                                        fact_used_template);
                                    if (dumped_func)
                                        a_pattern_union_value->union_expr->symbol = dumped_func->this_reification_lang_symbol;
                                    else
                                        lang_anylizer->lang_error(0x0000, a_pattern_union_value, WO_ERR_TEMPLATE_ARG_NOT_MATCH);
                                }
                            }
                        }
                        else
                            ;
                        /* Donot give error here, it will be given in following 'analyze_pass2' */
                        //lang_anylizer->lang_error(0x0000, a_pattern_union_value, WO_ERR_UNKNOWN_IDENTIFIER, a_pattern_union_value->union_expr->var_name.c_str());
                    }
                    analyze_pass2(a_pattern_union_value->union_expr);
                }
                if (a_pattern_union_value->pattern_arg_in_union_may_nil)
                {
                    wo_assert(a_match_union_case->take_place_value_may_nil);

                    if (a_pattern_union_value->union_expr->value_type->argument_types.size() != 1)
                        lang_anylizer->lang_error(0x0000, a_match_union_case, WO_ERR_INVALID_CASE_TYPE_NEED_ACCEPT_ARG);
                    else
                    {
                        a_match_union_case->take_place_value_may_nil->value_type->set_type(a_pattern_union_value->union_expr->value_type->argument_types.front());
                        a_match_union_case->take_place_value_may_nil->value_type->set_is_mutable(false);
                    }

                    analyze_pattern_in_pass2(a_pattern_union_value->pattern_arg_in_union_may_nil, a_match_union_case->take_place_value_may_nil);

                }
                else
                {
                    if (a_pattern_union_value->union_expr->value_type->argument_types.size() != 0)
                        lang_anylizer->lang_error(0x0000, a_match_union_case, WO_ERR_INVALID_CASE_TYPE_NO_ARG_RECV);
                }
            }
            else
                lang_anylizer->lang_error(0x0000, a_match_union_case, WO_ERR_UNEXPECT_PATTERN_CASE);
        }

        analyze_pass2(a_match_union_case->in_case_sentence);

        return true;
    }
    WO_PASS2(ast_struct_member_define)
    {
        auto* a_struct_member_define = WO_AST();

        if (a_struct_member_define->is_value_pair)
            analyze_pass2(a_struct_member_define->member_value_pair);
        else
            fully_update_type(a_struct_member_define->member_type, false);

        return true;
    }
    WO_PASS2(ast_where_constraint)
    {
        auto* a_where_constraint = WO_AST();
        ast_value* val = dynamic_cast<ast_value*>(a_where_constraint->where_constraint_list->children);
        while (val)
        {
            lang_anylizer->begin_trying_block();
            analyze_pass2(val);

            if (!lang_anylizer->get_cur_error_frame().empty())
            {
                // Current constraint failed, store it to list;
                a_where_constraint->unmatched_constraint.push_back(
                    lang_anylizer->make_error(0x0000, val, WO_ERR_WHERE_COND_GRAMMAR_ERR));
                a_where_constraint->unmatched_constraint.insert(
                    a_where_constraint->unmatched_constraint.end(),
                    lang_anylizer->get_cur_error_frame().begin(),
                    lang_anylizer->get_cur_error_frame().end());
                a_where_constraint->accept = false;
            }
            else
            {
                val->update_constant_value(lang_anylizer);
                if (!val->is_constant)
                {
                    a_where_constraint->unmatched_constraint.push_back(
                        lang_anylizer->make_error(0x0000, val, WO_ERR_WHERE_COND_SHOULD_BE_CONST));
                    a_where_constraint->accept = false;
                }
                else if (!val->value_type->is_bool())
                {
                    a_where_constraint->unmatched_constraint.push_back(
                        lang_anylizer->make_error(0x0000, val, WO_ERR_WHERE_COND_TYPE_ERR
                            , val->value_type->get_type_name(false).c_str()));
                    a_where_constraint->accept = false;
                }
                else if (0 == val->get_constant_value().handle)
                {
                    a_where_constraint->unmatched_constraint.push_back(
                        lang_anylizer->make_error(0x0000, val, WO_ERR_WHERE_COND_NOT_MEET));
                    a_where_constraint->accept = false;
                }
            }

            lang_anylizer->end_trying_block();
            val = dynamic_cast<ast_value*>(val->sibling);
        }

        return true;
    }

    WO_PASS2(ast_value_function_define)
    {
        auto* a_value_funcdef = WO_AST();

        if (!a_value_funcdef->is_template_define)
        {
            size_t anylizer_error_count = lang_anylizer->get_cur_error_frame().size();

            if (a_value_funcdef->argument_list)
            {
                // ast_value_function_define might be root-point created in lang.
                auto arg_child = a_value_funcdef->argument_list->children;
                while (arg_child)
                {
                    if (ast_value_arg_define* argdef = dynamic_cast<ast_value_arg_define*>(arg_child))
                    {
                        if (argdef->symbol)
                            argdef->symbol->has_been_defined_in_pass2 = true;
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
                if (a_value_funcdef->value_type->type_name == WO_PSTR(pending))
                {
                    // There is no return in function  return void
                    if (a_value_funcdef->auto_adjust_return_type)
                    {
                        if (a_value_funcdef->has_return_value)
                            lang_anylizer->lang_error(0x0000, a_value_funcdef, WO_ERR_CANNOT_DERIV_FUNCS_RET_TYPE, wo::str_to_wstr(a_value_funcdef->get_ir_func_signature_tag()).c_str());

                        a_value_funcdef->value_type->get_return_type()->set_type_with_name(WO_PSTR(void));
                    }
                }
            }
            else
                a_value_funcdef->value_type->get_return_type()->set_type_with_name(WO_PSTR(pending));

            if (lang_anylizer->get_cur_error_frame().size() != anylizer_error_count)
            {
                wo_assert(lang_anylizer->get_cur_error_frame().size() > anylizer_error_count);

                if (!a_value_funcdef->where_constraint)
                {
                    a_value_funcdef->where_constraint = new ast_where_constraint;
                    a_value_funcdef->where_constraint->copy_source_info(a_value_funcdef);
                    a_value_funcdef->where_constraint->binded_func_define = a_value_funcdef;
                    a_value_funcdef->where_constraint->where_constraint_list = new ast_list;
                }

                a_value_funcdef->where_constraint->accept = false;

                for (; anylizer_error_count < lang_anylizer->get_cur_error_frame().size(); ++anylizer_error_count)
                {
                    a_value_funcdef->where_constraint->unmatched_constraint.push_back(
                        lang_anylizer->get_cur_error_frame()[anylizer_error_count]);
                }


                // Error happend in cur function
                a_value_funcdef->value_type->get_return_type()->set_type_with_name(WO_PSTR(pending));
            }

        }
        return true;
    }
    WO_PASS2(ast_value_assign)
    {
        auto* a_value_assi = WO_AST();
        analyze_pass2(a_value_assi->left);
        analyze_pass2(a_value_assi->right);

        if (!a_value_assi->left->value_type->accept_type(a_value_assi->right->value_type, false))
        {
            lang_anylizer->lang_error(0x0000, a_value_assi->right, WO_ERR_SHOULD_BE_TYPE_BUT_GET_UNEXCEPTED_TYPE,
                a_value_assi->left->value_type->get_type_name(false).c_str(),
                a_value_assi->right->value_type->get_type_name(false).c_str());
        }

        if (!a_value_assi->left->can_be_assign)
            lang_anylizer->lang_error(0x0000, a_value_assi->left, WO_ERR_CANNOT_ASSIGN_TO_UNASSABLE_ITEM);

        a_value_assi->value_type->set_type(a_value_assi->left->value_type);
        return true;
    }
    WO_PASS2(ast_value_type_cast)
    {
        auto* a_value_typecast = WO_AST();
        // check: cast is valid?
        ast_value* origin_value = a_value_typecast->_be_cast_value_node;
        fully_update_type(a_value_typecast->value_type, false);
        analyze_pass2(origin_value);

        if (!ast_type::check_castable(a_value_typecast->value_type, origin_value->value_type))
        {
            lang_anylizer->lang_error(0x0000, a_value_typecast, WO_ERR_CANNOT_CAST_TYPE_TO_TYPE,
                origin_value->value_type->get_type_name(false).c_str(),
                a_value_typecast->value_type->get_type_name(false).c_str()
            );
        }

        return true;
    }
    WO_PASS2(ast_value_type_judge)
    {
        auto* ast_value_judge = WO_AST();
        analyze_pass2(ast_value_judge->_be_cast_value_node);
        return true;
    }
    WO_PASS2(ast_value_type_check)
    {
        auto* a_value_type_check = WO_AST();

        if (a_value_type_check->is_constant)
            return true;

        analyze_pass2(a_value_type_check->_be_check_value_node);

        a_value_type_check->update_constant_value(lang_anylizer);
        return true;
    }
    WO_PASS2(ast_value_index)
    {
        auto* a_value_index = WO_AST();
        analyze_pass2(a_value_index->from);
        analyze_pass2(a_value_index->index);

        if (a_value_index->value_type->is_pending())
        {
            if (!a_value_index->from->value_type->struct_member_index.empty())
            {
                if (a_value_index->index->is_constant && a_value_index->index->value_type->is_string())
                {
                    if (auto fnd =
                        a_value_index->from->value_type->struct_member_index.find(
                            wstring_pool::get_pstr(str_to_wstr(*a_value_index->index->get_constant_value().string))); fnd != a_value_index->from->value_type->struct_member_index.end())
                    {
                        if (!fnd->second.member_type->is_pending())
                        {
                            a_value_index->value_type->set_type(fnd->second.member_type);
                            a_value_index->struct_offset = fnd->second.offset;
                        }
                    }
                    else
                    {
                        lang_anylizer->lang_error(0x0000, a_value_index, WO_ERR_UNDEFINED_MEMBER,
                            str_to_wstr(*a_value_index->index->get_constant_value().string).c_str());
                    }
                }
                else
                {
                    lang_anylizer->lang_error(0x0000, a_value_index, WO_ERR_CANNOT_INDEX_MEMB_WITHOUT_STR);
                }

            }
            else
            {
                if (a_value_index->from->value_type->is_tuple())
                {
                    if (a_value_index->index->is_constant && a_value_index->index->value_type->is_integer())
                    {
                        // Index tuple must with constant integer.
                        auto index = a_value_index->index->get_constant_value().integer;
                        if ((size_t)index < a_value_index->from->value_type->template_arguments.size() && index >= 0)
                        {
                            a_value_index->value_type->set_type(a_value_index->from->value_type->template_arguments[index]);
                            a_value_index->struct_offset = (uint16_t)index;
                        }
                        else
                            lang_anylizer->lang_error(0x0000, a_value_index, WO_ERR_FAILED_TO_INDEX_TUPLE_ERR_INDEX,
                                (int)a_value_index->from->value_type->template_arguments.size(), (int)(index + 1));
                    }
                    else
                    {
                        lang_anylizer->lang_error(0x0000, a_value_index, WO_ERR_FAILED_TO_INDEX_TUPLE_ERR_TYPE);
                    }
                }
                else if (a_value_index->from->value_type->is_string())
                {
                    a_value_index->value_type->set_type_with_name(WO_PSTR(char));
                }
                else if (!a_value_index->from->value_type->is_pending())
                {
                    if (a_value_index->from->value_type->is_array() || a_value_index->from->value_type->is_vec())
                        a_value_index->value_type->set_type(a_value_index->from->value_type->template_arguments[0]);
                    else if (a_value_index->from->value_type->is_dict() || a_value_index->from->value_type->is_map())
                        a_value_index->value_type->set_type(a_value_index->from->value_type->template_arguments[1]);
                    else
                        a_value_index->value_type->set_type_with_name(WO_PSTR(dynamic));
                }
            }
        }

        if (!a_value_index->from->value_type->is_array()
            && !a_value_index->from->value_type->is_dict()
            && !a_value_index->from->value_type->is_vec()
            && !a_value_index->from->value_type->is_map()
            && !a_value_index->from->value_type->is_string()
            && !a_value_index->from->value_type->is_struct()
            && !a_value_index->from->value_type->is_tuple())
        {
            lang_anylizer->lang_error(0x0000, a_value_index->from, WO_ERR_UNINDEXABLE_TYPE
                , a_value_index->from->value_type->get_type_name().c_str());
        }

        if (a_value_index->from->value_type->is_array() || a_value_index->from->value_type->is_vec() || a_value_index->from->value_type->is_string())
        {
            if (!a_value_index->index->value_type->is_integer())
            {
                lang_anylizer->lang_error(0x0000, a_value_index->index, WO_ERR_SHOULD_BE_TYPE_BUT_GET_UNEXCEPTED_TYPE
                    , L"int"
                    , a_value_index->index->value_type->get_type_name().c_str());
            }
        }
        if (a_value_index->from->value_type->is_dict() || a_value_index->from->value_type->is_map())
        {
            if (!a_value_index->index->value_type->is_same(a_value_index->from->value_type->template_arguments[0], false, true))
            {
                lang_anylizer->lang_error(0x0000, a_value_index->index, WO_ERR_SHOULD_BE_TYPE_BUT_GET_UNEXCEPTED_TYPE
                    , a_value_index->from->value_type->template_arguments[0]->get_type_name(false).c_str()
                    , a_value_index->from->value_type->get_type_name().c_str());
            }
        }
        return true;
    }
    WO_PASS2(ast_value_indexed_variadic_args)
    {
        auto* a_value_variadic_args_idx = WO_AST();
        analyze_pass2(a_value_variadic_args_idx->argindex);

        if (!a_value_variadic_args_idx->argindex->value_type->is_integer())
        {
            lang_anylizer->lang_error(0x0000, a_value_variadic_args_idx, WO_ERR_FAILED_TO_INDEX_VAARG_ERR_TYPE);
        }
        return true;
    }
    WO_PASS2(ast_fakevalue_unpacked_args)
    {
        auto* a_fakevalue_unpacked_args = WO_AST();
        analyze_pass2(a_fakevalue_unpacked_args->unpacked_pack);
        if (!a_fakevalue_unpacked_args->unpacked_pack->value_type->is_array()
            && !a_fakevalue_unpacked_args->unpacked_pack->value_type->is_tuple())
        {
            lang_anylizer->lang_error(0x0000, a_fakevalue_unpacked_args, WO_ERR_NEED_TYPES, L"array, vec" WO_TERM_OR L"tuple");
        }
        return true;
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
                    a_value_bin->value_type->set_type(lnr_type);
                else
                    a_value_bin->value_type->set_type_with_name(WO_PSTR(pending));
            }
            else
            {
                // Apply this type to func
                if (nullptr == a_value_bin->value_type)
                    a_value_bin->value_type->set_type(a_value_bin->overrided_operation_call->value_type);
                else if (a_value_bin->value_type->is_pending())
                    a_value_bin->value_type->set_type(a_value_bin->overrided_operation_call->value_type);
            }

        }

        if (nullptr == a_value_bin->value_type)
        {
            lang_anylizer->lang_error(0x0000, a_value_bin, WO_ERR_CANNOT_CALC_WITH_L_AND_R,
                a_value_bin->left->value_type->get_type_name(false).c_str(),
                a_value_bin->right->value_type->get_type_name(false).c_str());
            a_value_bin->value_type->set_type_with_name(WO_PSTR(pending));
        }
        return true;
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
                if (a_value_logic_bin->operate == +lex_type::l_lor
                    || a_value_logic_bin->operate == +lex_type::l_land)
                {
                    if (a_value_logic_bin->left->value_type->is_bool()
                        && a_value_logic_bin->right->value_type->is_bool())
                        type_ok = true;
                }
                else if ((a_value_logic_bin->left->value_type->is_integer()
                    || a_value_logic_bin->left->value_type->is_handle()
                    || a_value_logic_bin->left->value_type->is_real()
                    || a_value_logic_bin->left->value_type->is_string()
                    || a_value_logic_bin->left->value_type->is_gchandle())
                    && a_value_logic_bin->left->value_type->is_same(a_value_logic_bin->right->value_type, false, true))
                    type_ok = true;
            }

            if (!type_ok)
                lang_anylizer->lang_error(0x0000, a_value_logic_bin, WO_ERR_CANNOT_CALC_WITH_L_AND_R,
                    a_value_logic_bin->left->value_type->get_type_name(false).c_str(),
                    a_value_logic_bin->right->value_type->get_type_name(false).c_str());
            else
                a_value_logic_bin->value_type->set_type_with_name(WO_PSTR(bool));
            fully_update_type(a_value_logic_bin->value_type, false);
        }

        return true;
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

                if (!decide_array_item_type->accept_type(val->value_type, false)
                    && !decide_array_item_type->set_mix_types(val->value_type, false))
                {
                    if (!a_value_arr->is_mutable_vector)
                        lang_anylizer->lang_error(0x0000, val, WO_ERR_DIFFERENT_VAL_TYPE_OF, L"array");
                    else
                        lang_anylizer->lang_error(0x0000, val, WO_ERR_DIFFERENT_VAL_TYPE_OF, L"vec");
                    break;
                }
                val = dynamic_cast<ast_value*>(val->sibling);
            }
        }

        if (!a_value_arr->is_mutable_vector && !a_value_arr->value_type->is_array())
        {
            lang_anylizer->lang_error(0x0000, a_value_arr, WO_ERR_CANNOT_CAST_TYPE_TO_TYPE,
                L"array<...>",
                a_value_arr->value_type->get_type_name().c_str()
            );
        }
        else if (a_value_arr->is_mutable_vector && !a_value_arr->value_type->is_vec())
        {
            lang_anylizer->lang_error(0x0000, a_value_arr, WO_ERR_CANNOT_CAST_TYPE_TO_TYPE,
                L"vec<...>",
                a_value_arr->value_type->get_type_name().c_str()
            );
        }
        else
        {
            std::vector<ast_value* > reenplace_array_items;

            ast_value* val = dynamic_cast<ast_value*>(a_value_arr->array_items->children);

            while (val)
            {
                if (!a_value_arr->value_type->template_arguments[0]->accept_type(val->value_type, false))
                {
                    if (!a_value_arr->is_mutable_vector)
                        lang_anylizer->lang_error(0x0000, val, WO_ERR_DIFFERENT_VAL_TYPE_OF_TEMPLATE, "array");
                    else
                        lang_anylizer->lang_error(0x0000, val, WO_ERR_DIFFERENT_VAL_TYPE_OF_TEMPLATE, L"vec");
                }
                reenplace_array_items.push_back(val);

                val = dynamic_cast<ast_value*>(val->sibling);
            }

            a_value_arr->array_items->remove_allnode();
            for (auto in_array_val : reenplace_array_items)
            {
                in_array_val->sibling = nullptr;
                a_value_arr->array_items->add_child(in_array_val);
            }

        }
        return true;
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

                if (!decide_map_key_type->accept_type(map_pair->key->value_type, false)
                    && !decide_map_key_type->set_mix_types(map_pair->key->value_type, false))
                {
                    if (!a_value_map->is_mutable_map)
                        lang_anylizer->lang_error(0x0000, map_pair->key, WO_ERR_DIFFERENT_KEY_TYPE_OF, L"dict");
                    else
                        lang_anylizer->lang_error(0x0000, map_pair->key, WO_ERR_DIFFERENT_KEY_TYPE_OF, L"map");
                    break;
                }
                if (!decide_map_val_type->accept_type(map_pair->val->value_type, false)
                    && !decide_map_val_type->set_mix_types(map_pair->val->value_type, false))
                {
                    if (!a_value_map->is_mutable_map)
                        lang_anylizer->lang_error(0x0000, map_pair->val, WO_ERR_DIFFERENT_VAL_TYPE_OF, L"dict");
                    else
                        lang_anylizer->lang_error(0x0000, map_pair->val, WO_ERR_DIFFERENT_VAL_TYPE_OF, L"map");
                    break;
                }
                map_pair = dynamic_cast<ast_mapping_pair*>(map_pair->sibling);
            }
        }

        if (!a_value_map->is_mutable_map && !a_value_map->value_type->is_dict())
        {
            lang_anylizer->lang_error(0x0000, a_value_map, WO_ERR_CANNOT_CAST_TYPE_TO_TYPE,
                L"dict<..., ...>",
                a_value_map->value_type->get_type_name().c_str()
            );
        }
        else if (a_value_map->is_mutable_map && !a_value_map->value_type->is_map())
        {
            lang_anylizer->lang_error(0x0000, a_value_map, WO_ERR_CANNOT_CAST_TYPE_TO_TYPE,
                L"map<..., ...>",
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
                    if (!a_value_map->value_type->template_arguments[0]->accept_type(pairs->key->value_type, false))
                    {
                        if (!a_value_map->is_mutable_map)
                            lang_anylizer->lang_error(0x0000, pairs->key, WO_ERR_DIFFERENT_KEY_TYPE_OF_TEMPLATE, L"dict");
                        else
                            lang_anylizer->lang_error(0x0000, pairs->key, WO_ERR_DIFFERENT_KEY_TYPE_OF_TEMPLATE, L"map");
                    }
                    if (!a_value_map->value_type->template_arguments[1]->accept_type(pairs->val->value_type, false))
                    {
                        if (!a_value_map->is_mutable_map)
                            lang_anylizer->lang_error(0x0000, pairs->val, WO_ERR_DIFFERENT_VAL_TYPE_OF_TEMPLATE, L"dict");
                        else
                            lang_anylizer->lang_error(0x0000, pairs->val, WO_ERR_DIFFERENT_VAL_TYPE_OF_TEMPLATE, L"map");
                    }

                }
                pairs = dynamic_cast<ast_mapping_pair*>(pairs->sibling);
            }
        }
        return true;
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
                lang_anylizer->lang_error(0x0000, val, WO_ERR_FAILED_TO_DECIDE_TUPLE_TYPE);
                break;
            }
            tuple_elems = tuple_elems->sibling;
            ++count;
        }

        return true;
    }
    WO_PASS2(ast_value_make_struct_instance)
    {
        auto* a_value_make_struct_instance = WO_AST();
        analyze_pass2(a_value_make_struct_instance->struct_member_vals);
        // Varify
        if (!a_value_make_struct_instance->value_type->is_pending())
        {
            if (a_value_make_struct_instance->value_type->is_struct())
            {
                auto* init_mem_val_pair = a_value_make_struct_instance->struct_member_vals->children;

                uint16_t member_count = 0;
                while (init_mem_val_pair)
                {
                    member_count++;
                    auto* membpair = dynamic_cast<ast_struct_member_define*>(init_mem_val_pair);
                    wo_assert(membpair);
                    init_mem_val_pair = init_mem_val_pair->sibling;

                    auto fnd = a_value_make_struct_instance->value_type->struct_member_index.find(membpair->member_name);
                    if (fnd != a_value_make_struct_instance->value_type->struct_member_index.end())
                    {
                        wo_assert(membpair->is_value_pair);

                        membpair->member_offset = fnd->second.offset;
                        fully_update_type(fnd->second.member_type, false);
                        if (auto result = judge_auto_type_of_funcdef_with_type(membpair,
                            fnd->second.member_type, membpair->member_value_pair, true, nullptr, nullptr))
                            membpair->member_value_pair = std::get<ast::ast_value_function_define*>(result.value());

                        if (!fnd->second.member_type->accept_type(membpair->member_value_pair->value_type, false, false))
                        {
                            lang_anylizer->lang_error(0x0000, membpair, WO_ERR_DIFFERENT_MEMBER_TYPE_OF
                                , membpair->member_name->c_str()
                                , fnd->second.member_type->get_type_name(false).c_str()
                                , membpair->member_value_pair->value_type->get_type_name(false).c_str());
                        }
                    }
                    else
                        lang_anylizer->lang_error(0x0000, membpair, WO_ERR_THERE_IS_NO_MEMBER_NAMED,
                            a_value_make_struct_instance->value_type->get_type_name(false).c_str(),
                            membpair->member_name->c_str());
                }

                if (member_count < a_value_make_struct_instance->value_type->struct_member_index.size())
                    lang_anylizer->lang_error(0x0000, a_value_make_struct_instance, WO_ERR_CONSTRUCT_STRUCT_NOT_FINISHED,
                        a_value_make_struct_instance->value_type->get_type_name(false).c_str());
            }
            else
                lang_anylizer->lang_error(0x0000, a_value_make_struct_instance, WO_ERR_ONLY_CONSTRUCT_STRUCT_BY_THIS_WAY);
        }
        else
            lang_anylizer->lang_error(0x0000, a_value_make_struct_instance, WO_ERR_UNKNOWN_TYPE,
                a_value_make_struct_instance->value_type->get_type_name(false).c_str());
        return true;
    }
    WO_PASS2(ast_value_trib_expr)
    {
        auto* a_value_trib_expr = WO_AST();
        analyze_pass2(a_value_trib_expr->judge_expr);
        if (!a_value_trib_expr->judge_expr->value_type->is_bool())
        {
            lang_anylizer->lang_error(0x0000, a_value_trib_expr->judge_expr, WO_ERR_NOT_BOOL_VAL_IN_COND_EXPR,
                a_value_trib_expr->judge_expr->value_type->get_type_name(false).c_str());
        }

        a_value_trib_expr->judge_expr->update_constant_value(lang_anylizer);

        if (a_value_trib_expr->judge_expr->is_constant)
        {
            if (a_value_trib_expr->judge_expr->get_constant_value().integer)
            {
                analyze_pass2(a_value_trib_expr->val_if_true);
                a_value_trib_expr->value_type->set_type(a_value_trib_expr->val_if_true->value_type);
            }
            else
            {
                analyze_pass2(a_value_trib_expr->val_or);
                a_value_trib_expr->value_type->set_type(a_value_trib_expr->val_or->value_type);
            }
        }
        else
        {
            analyze_pass2(a_value_trib_expr->val_if_true);
            analyze_pass2(a_value_trib_expr->val_or);

            if (a_value_trib_expr->value_type->is_pending())
            {
                a_value_trib_expr->value_type->set_type(a_value_trib_expr->val_if_true->value_type);
                if (!a_value_trib_expr->value_type->set_mix_types(a_value_trib_expr->val_or->value_type, false))
                {
                    lang_anylizer->lang_error(0x0000, a_value_trib_expr, WO_ERR_DIFFERENT_TYPES_IN_COND_EXPR
                        , a_value_trib_expr->val_if_true->value_type->get_type_name(false).c_str()
                        , a_value_trib_expr->val_or->value_type->get_type_name(false).c_str());
                }
            }
        }
        return true;
    }

    void check_function_where_constraint(grammar::ast_base* ast, lexer* lang_anylizer, ast::ast_symbolable_base* func)
    {
        wo_assert(func != nullptr && ast != nullptr);
        auto* funcdef = dynamic_cast<ast::ast_value_function_define*>(func);
        if (funcdef == nullptr
            && func->symbol != nullptr
            && func->symbol->type == lang_symbol::symbol_type::function)
            // Why not get `funcdef` from symbol by this way if `func` is not a `ast_value_function_define`
            // ast_value_function_define's symbol will point to origin define, if `func` is an instance of 
            // template function define, the check will not-able to get correct error.
            auto* funcdef = func->symbol->get_funcdef();
        if (funcdef != nullptr
            && funcdef->where_constraint != nullptr
            && !funcdef->where_constraint->accept)
        {
            lang_anylizer->lang_error(0x0000, ast, WO_ERR_FAILED_TO_INVOKE_BECAUSE);
            for (auto& error_info : funcdef->where_constraint->unmatched_constraint)
            {
                lang_anylizer->get_cur_error_frame().push_back(error_info);
            }
        }

    }

    WO_PASS2(ast_value_variable)
    {
        auto* a_value_var = WO_AST();

        if (a_value_var->value_type->is_pending())
        {
            auto* sym = find_value_in_this_scope(a_value_var);

            if (sym)
            {
                if (sym->define_in_function && !sym->has_been_defined_in_pass2 && !sym->is_captured_variable)
                    lang_anylizer->lang_error(0x0000, a_value_var, WO_ERR_UNKNOWN_IDENTIFIER, a_value_var->var_name->c_str());

                if (sym->is_template_symbol && (!a_value_var->is_auto_judge_function_overload || sym->type == lang_symbol::symbol_type::variable))
                {
                    sym = analyze_pass_template_reification(a_value_var, a_value_var->template_reification_args);
                    if (!sym)
                        lang_anylizer->lang_error(0x0000, a_value_var, WO_ERR_FAILED_TO_INSTANCE_TEMPLATE_ID,
                            a_value_var->var_name->c_str());
                }

                if (sym)
                {
                    analyze_pass2(sym->variable_value);
                    a_value_var->value_type->set_type(sym->variable_value->value_type);

                    if (sym->type == lang_symbol::symbol_type::variable && sym->decl == identifier_decl::MUTABLE)
                        a_value_var->value_type->set_is_mutable(true);
                    else
                        a_value_var->value_type->set_is_mutable(false);

                    a_value_var->symbol = sym;

                    if (a_value_var->symbol->type == lang_symbol::symbol_type::function)
                    {
                        auto* funcdef = a_value_var->symbol->get_funcdef();
                        check_function_where_constraint(a_value_var, lang_anylizer, funcdef);
                    }

                    if (a_value_var->value_type->is_pending())
                    {
                        if (a_value_var->symbol->type == lang_symbol::symbol_type::function
                            && a_value_var->symbol->is_template_symbol)
                            ; /* function call may be template, do not report error here~ */
                        else
                            lang_anylizer->lang_error(0x0000, a_value_var, WO_ERR_UNABLE_DECIDE_VAR_TYPE);
                    }
                }
            }
            else
            {
                lang_anylizer->lang_error(0x0000, a_value_var, WO_ERR_UNKNOWN_IDENTIFIER,
                    a_value_var->var_name->c_str());
                a_value_var->value_type->set_type_with_name(WO_PSTR(pending));
            }
        }
        return true;
    }
    WO_PASS2(ast_value_unary)
    {
        auto* a_value_unary = WO_AST();

        analyze_pass2(a_value_unary->val);

        if (a_value_unary->operate == +lex_type::l_lnot)
        {
            a_value_unary->value_type->set_type_with_name(WO_PSTR(bool));
            fully_update_type(a_value_unary->value_type, false);
        }
        a_value_unary->value_type->set_type(a_value_unary->val->value_type);

        return true;
    }

    WO_PASS2(ast_value_funccall)
    {
        auto* a_value_funccall = WO_AST();

        if (a_value_funccall->value_type->is_pending())
        {
            if (a_value_funccall->callee_symbol_in_type_namespace)
            {
                wo_assert(a_value_funccall->callee_symbol_in_type_namespace->completed_in_pass2 == false);

                analyze_pass2(a_value_funccall->directed_value_from);
                if (!a_value_funccall->directed_value_from->value_type->is_pending())
                {
                    // trying finding type_function
                    auto origin_namespace = a_value_funccall->callee_symbol_in_type_namespace->scope_namespaces;

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
                    lang_anylizer->lang_error(0x0000, a_value_funccall, WO_ERR_FAILED_TO_INVOKE_FUNC_FOR_TYPE,
                        a_value_funccall->callee_symbol_in_type_namespace->var_name->c_str(),
                        a_value_funccall->directed_value_from->value_type->get_type_name(false).c_str());
                }
            }

            auto updated_args_types = judge_auto_type_in_funccall(a_value_funccall, false, nullptr, nullptr);

            ast_value_function_define* calling_function_define = nullptr;

            if (auto* called_funcsymb = dynamic_cast<ast_value_symbolable_base*>(a_value_funccall->called_func))
            {
                // called_funcsymb might be lambda function, and have not symbol.
                if (called_funcsymb->symbol != nullptr
                    && called_funcsymb->symbol->type == lang_symbol::symbol_type::function)
                {
                    check_symbol_is_accessable(
                        called_funcsymb->symbol->get_funcdef(),
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
                        lang_anylizer->lang_error(0x0000, a_value_funccall, WO_ERR_NO_MATCHED_FUNC_TEMPLATE);
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

                            if (pending_template_arg = analyze_template_derivation(
                                calling_function_define->template_type_name_list[tempindex],
                                calling_function_define->template_type_name_list,
                                calling_function_define->value_type->argument_types[index],
                                updated_args_types[index] ? updated_args_types[index] : real_argument_types[index]
                            ))
                            {
                                if (pending_template_arg->is_pure_pending())
                                    pending_template_arg = nullptr;
                            }

                            if (pending_template_arg)
                                break;
                        }
                    }
                }
                // TODO; Some of template arguments might not able to judge if argument has `auto` type.
                // Update auto type here and re-check template
                judge_auto_type_in_funccall(a_value_funccall, true, calling_function_define, &template_args);

                // After judge, args might be changed, we need re-try to get them.
                real_argument_types.clear();
                funccall_arg = dynamic_cast<ast_value*>(a_value_funccall->arguments->children);
                while (funccall_arg)
                {
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
                    lang_anylizer->lang_error(0x0000, a_value_funccall, WO_ERR_FAILED_TO_DECIDE_ALL_TEMPLATE_ARGS);
                // failed getting each of template args, abandon this one
                else
                {
                    for (auto* templ_arg : template_args)
                    {
                        fully_update_type(templ_arg, false);
                        if (templ_arg->is_pending() && !templ_arg->is_hkt())
                        {
                            lang_anylizer->lang_error(0x0000, a_value_funccall, WO_ERR_UNKNOWN_TYPE,
                                templ_arg->get_type_name(false).c_str());
                            goto failed_to_judge_template_params;
                        }
                    }

                    calling_function_define = analyze_pass_template_reification(calling_function_define, template_args); //tara~ get analyze_pass_template_reification 
                    a_value_funccall->called_func = calling_function_define;
                }
            failed_to_judge_template_params:;
            }
            // End of template judge

            judge_auto_type_in_funccall(a_value_funccall, true, nullptr, nullptr);

            analyze_pass2(a_value_funccall->called_func);
            if (ast_symbolable_base* symbase = dynamic_cast<ast_symbolable_base*>(a_value_funccall->called_func))
                check_function_where_constraint(a_value_funccall, lang_anylizer, symbase);

            if (!a_value_funccall->called_func->value_type->is_pending()
                && a_value_funccall->called_func->value_type->is_func())
                a_value_funccall->value_type->set_type(a_value_funccall->called_func->value_type->get_return_type());

            if (a_value_funccall->called_func
                && a_value_funccall->called_func->value_type->is_func()
                && a_value_funccall->called_func->value_type->is_pending())
            {
                auto* funcsymb = dynamic_cast<ast_value_function_define*>(a_value_funccall->called_func);

                if (funcsymb && funcsymb->auto_adjust_return_type)
                {
                    // If call a pending type function. it means the function's type dudes may fail, mark void to continue..
                    if (funcsymb->has_return_value)
                        lang_anylizer->lang_error(0x0000, funcsymb, WO_ERR_CANNOT_DERIV_FUNCS_RET_TYPE, wo::str_to_wstr(funcsymb->get_ir_func_signature_tag()).c_str());

                    funcsymb->value_type->get_return_type()->set_type_with_name(WO_PSTR(void));
                    funcsymb->auto_adjust_return_type = false;
                }

            }

            bool failed_to_call_cur_func = false;

            if (a_value_funccall->called_func
                && a_value_funccall->called_func->value_type->is_func())
            {
                auto* real_args = a_value_funccall->arguments->children;
                a_value_funccall->arguments->remove_allnode();

                for (auto a_type_index = a_value_funccall->called_func->value_type->argument_types.begin();
                    a_type_index != a_value_funccall->called_func->value_type->argument_types.end();)
                {
                    bool donot_move_forward = false;
                    if (!real_args)
                    {
                        // default arg mgr here, now just kill
                        failed_to_call_cur_func = true;
                        lang_anylizer->lang_error(0x0000, a_value_funccall, WO_ERR_ARGUMENT_TOO_FEW, a_value_funccall->called_func->value_type->get_type_name(false).c_str());
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
                                    if (unpacking_tuple_type->using_type_name)
                                        unpacking_tuple_type = unpacking_tuple_type->using_type_name;
                                    ecount = unpacking_tuple_type->template_arguments.size();
                                    a_fakevalue_unpack_args->expand_count = ecount;
                                }
                                else
                                {
                                    ecount =
                                        (wo_integer_t)(
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
                                        if (unpacking_tuple_type->using_type_name)
                                            unpacking_tuple_type = unpacking_tuple_type->using_type_name;

                                        if (unpacking_tuple_type->template_arguments.size() <= unpack_tuple_index)
                                        {
                                            // There is no enough value for tuple to expand. match failed!
                                            failed_to_call_cur_func = true;
                                            lang_anylizer->lang_error(0x0000, a_value_funccall, WO_ERR_ARGUMENT_TOO_MANY, a_value_funccall->called_func->value_type->get_type_name(false).c_str());
                                            break;
                                        }
                                        else if (!(*a_type_index)->accept_type(unpacking_tuple_type->template_arguments[unpack_tuple_index], false))
                                        {
                                            failed_to_call_cur_func = true;
                                            lang_anylizer->lang_error(0x0000, a_value_funccall, WO_ERR_TYPE_CANNOT_BE_CALL, a_value_funccall->called_func->value_type->get_type_name(false).c_str());
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
                                    lang_anylizer->lang_error(0x0000, a_value_funccall, WO_ERR_ARGUMENT_TOO_MANY, a_value_funccall->called_func->value_type->get_type_name(false).c_str());
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
                            if (!(*a_type_index)->accept_type(arg_val->value_type, false))
                            {
                                failed_to_call_cur_func = true;
                                lang_anylizer->lang_error(0x0000, arg_val, WO_ERR_SHOULD_BE_TYPE_BUT_GET_UNEXCEPTED_TYPE,
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
                            size_t tuple_arg_sz = unpackval->unpacked_pack->value_type->using_type_name
                                ? unpackval->unpacked_pack->value_type->using_type_name->template_arguments.size()
                                : unpackval->unpacked_pack->value_type->template_arguments.size();

                            if (tuple_arg_sz != 0)
                            {
                                failed_to_call_cur_func = true;
                                lang_anylizer->lang_error(0x0000, a_value_funccall, WO_ERR_ARGUMENT_TOO_MANY, a_value_funccall->called_func->value_type->get_type_name(false).c_str());
                            }
                        }
                    }
                    else
                    {
                        failed_to_call_cur_func = true;
                        lang_anylizer->lang_error(0x0000, a_value_funccall, WO_ERR_ARGUMENT_TOO_MANY, a_value_funccall->called_func->value_type->get_type_name(false).c_str());
                    }
                }
            }
            else
            {
                failed_to_call_cur_func = true;
                lang_anylizer->lang_error(0x0000, a_value_funccall, WO_ERR_TYPE_CANNOT_BE_CALL,
                    a_value_funccall->called_func->value_type->get_type_name(false).c_str());
            }

            if (failed_to_call_cur_func)
                a_value_funccall->value_type->set_type_with_name(WO_PSTR(pending));
        }
        return true;
    }

    /*
    WO_TRY_BEGIN;
                    //
                    WO_TRY_PASS(ast_value_variable);
                    WO_TRY_PASS(ast_value_function_define);
                    WO_TRY_PASS(ast_value_assign);
                    WO_TRY_PASS(ast_value_type_cast);
                    WO_TRY_PASS(ast_value_type_judge);
                    WO_TRY_PASS(ast_value_type_check);
                    WO_TRY_PASS(ast_value_index);
                    WO_TRY_PASS(ast_value_indexed_variadic_args);
                    WO_TRY_PASS(ast_fakevalue_unpacked_args);
                    WO_TRY_PASS(ast_value_binary);
                    WO_TRY_PASS(ast_value_logical_binary);
                    WO_TRY_PASS(ast_value_array);
                    WO_TRY_PASS(ast_value_mapping);
                    WO_TRY_PASS(ast_value_make_tuple_instance);
                    WO_TRY_PASS(ast_value_make_struct_instance);
                    WO_TRY_PASS(ast_value_trib_expr);
                    WO_TRY_PASS(ast_value_unary);
                    WO_TRY_PASS(ast_value_funccall);

                    WO_TRY_END;
    */

    namespace ast
    {
        bool ast_type::is_same(const ast_type* another, bool ignore_using_type, bool ignore_mutable) const
        {
            if (is_pending_function() || another->is_pending_function())
                return false;

            if (!ignore_mutable && is_mutable() != another->is_mutable())
                return false;

            if (is_hkt() && another->is_hkt())
            {
                auto* ltsymb = symbol ? base_typedef_symbol(symbol) : nullptr;
                auto* rtsymb = another->symbol ? base_typedef_symbol(another->symbol) : nullptr;

                if (ltsymb && rtsymb && ltsymb == rtsymb)
                    return true;
                if (ltsymb == nullptr || rtsymb == nullptr)
                {
                    if (ltsymb && ltsymb->type == lang_symbol::symbol_type::type_alias)
                    {
                        wo_assert(another->value_type == wo::value::valuetype::dict_type
                            || another->value_type == wo::value::valuetype::array_type);
                        return ltsymb->type_informatiom->value_type == another->value_type
                            && ltsymb->type_informatiom->type_name == another->type_name;
                    }
                    if (rtsymb && rtsymb->type == lang_symbol::symbol_type::type_alias)
                    {
                        wo_assert(value_type == wo::value::valuetype::dict_type
                            || value_type == wo::value::valuetype::array_type);
                        return rtsymb->type_informatiom->value_type == value_type
                            && rtsymb->type_informatiom->type_name == type_name;
                    }
                    // both nullptr, check base type
                    wo_assert(another->value_type == wo::value::valuetype::dict_type
                        || another->value_type == wo::value::valuetype::array_type);
                    wo_assert(value_type == wo::value::valuetype::dict_type
                        || value_type == wo::value::valuetype::array_type);
                    return value_type == another->value_type
                        && type_name == another->type_name;
                }
                else if (ltsymb->type == lang_symbol::symbol_type::type_alias && rtsymb->type == lang_symbol::symbol_type::type_alias)
                {
                    // TODO: struct/pending type need check, struct!
                    return ltsymb->type_informatiom->value_type == rtsymb->type_informatiom->value_type;
                }
                return false;
            }

            if (is_pending() || another->is_pending())
                return false;

            if (!ignore_using_type && (using_type_name || another->using_type_name))
            {
                if (!using_type_name || !another->using_type_name)
                    return false;

                if (find_type_in_this_scope(using_type_name) != find_type_in_this_scope(another->using_type_name))
                    return false;

                if (using_type_name->template_arguments.size() != another->using_type_name->template_arguments.size())
                    return false;

                for (size_t i = 0; i < using_type_name->template_arguments.size(); ++i)
                    if (!using_type_name->template_arguments[i]->is_same(another->using_type_name->template_arguments[i], ignore_using_type, false))
                        return false;
            }
            if (has_template())
            {
                if (template_arguments.size() != another->template_arguments.size())
                    return false;
                for (size_t index = 0; index < template_arguments.size(); index++)
                {
                    if (!template_arguments[index]->is_same(another->template_arguments[index], ignore_using_type, false))
                        return false;
                }
            }
            if (is_func())
            {
                if (!another->is_func())
                    return false;

                if (argument_types.size() != another->argument_types.size())
                    return false;
                for (size_t index = 0; index < argument_types.size(); index++)
                {
                    if (!argument_types[index]->is_same(another->argument_types[index], ignore_using_type, true))
                        return false;
                }
                if (is_variadic_function_type != another->is_variadic_function_type)
                    return false;
            }
            else if (another->is_func())
                return false;

            if (is_complex() && another->is_complex())
                return complex_type->is_same(another->complex_type, ignore_using_type, false);
            else if (!is_complex() && !another->is_complex())
                return this->value_type == another->value_type && this->type_name == another->type_name;
            return false;
        }

        std::wstring ast_type::get_type_name(std::unordered_set<const ast_type*>& s, bool ignore_using_type, bool ignore_mut) const
        {
            if (s.find(this) != s.end())
                return L"..";
            s.insert(this);

            std::wstring result;

            if (is_mutable() && !ignore_mut)
                result += L"mut ";

            if (!ignore_using_type && using_type_name)
            {
                auto namespacechain = (search_from_global_namespace ? L"::" : L"") +
                    wo::str_to_wstr(get_belong_namespace_path_with_lang_scope(using_type_name->symbol));
                result += (namespacechain.empty() ? L"" : namespacechain + L"::")
                    + using_type_name->get_type_name(s, ignore_using_type, true);
            }
            else
            {
                if (is_function_type)
                {
                    result += L"(";
                    for (size_t index = 0; index < argument_types.size(); index++)
                    {
                        result += argument_types[index]->get_type_name(s, ignore_using_type, false);
                        if (index + 1 != argument_types.size() || is_variadic_function_type)
                            result += L", ";
                    }

                    if (is_variadic_function_type)
                    {
                        result += L"...";
                    }
                    result += L")=>";
                }
                if (is_hkt_typing() && symbol)
                {
                    auto* base_symbol = base_typedef_symbol(symbol);
                    wo_assert(base_symbol && base_symbol->name != nullptr);
                    result += *base_symbol->name;
                }
                else
                {
                    result += (is_complex() ? complex_type->get_type_name(s, ignore_using_type, false) : *type_name) /*+ (is_pending() ? L" !pending" : L"")*/;
                }

                if (has_template())
                {
                    result += L"<";
                    for (size_t index = 0; index < template_arguments.size(); index++)
                    {
                        result += template_arguments[index]->get_type_name(s, ignore_using_type, false);
                        if (is_hkt_typing())
                            result += L"?";
                        if (index + 1 != template_arguments.size())
                            result += L", ";
                    }
                    result += L">";
                }
            }
            s.erase(this);
            return result;
        }

        std::wstring ast_type::get_type_name(bool ignore_using_type, bool ignore_mut) const
        {
            std::unordered_set<const ast_type*> us;
            return get_type_name(us, ignore_using_type, ignore_mut);
        }

        bool ast_type::is_hkt() const
        {
            if (is_func())
                return false;   // HKT cannot be return-type of function

            if (is_hkt_typing())
                return true;

            if (template_arguments.empty())
            {
                if (symbol && symbol->is_template_symbol && is_custom())
                    return true;
                else if (is_array() || is_dict() || is_vec() || is_map())
                    return true;
            }
            return false;
        }

        bool ast_type::is_hkt_typing() const
        {
            if (symbol)
                return symbol->is_hkt_typing_symb;

            return false;
        }

        lang_symbol* ast_type::base_typedef_symbol(lang_symbol* symb)
        {
            wo_assert(symb->type == lang_symbol::symbol_type::typing
                || symb->type == lang_symbol::symbol_type::type_alias);

            if (symb->type == lang_symbol::symbol_type::typing)
                return symb;
            else if (symb->type_informatiom->symbol)
                return base_typedef_symbol(symb->type_informatiom->symbol);
            else
                return symb;
        }

        void ast_value_variable::update_constant_value(lexer* lex)
        {
            if (is_constant)
                return;

            // TODO: constant variable here..
            if (symbol
                && !symbol->is_captured_variable
                && symbol->decl == identifier_decl::IMMUTABLE)
            {
                symbol->variable_value->update_constant_value(lex);
                if (symbol->variable_value->is_constant)
                {
                    is_constant = true;
                    symbol->is_constexpr = true;
                    if (symbol->variable_value->get_constant_value().type == value::valuetype::string_type)
                        constant_value.set_string_nogc(symbol->variable_value->get_constant_value().string->c_str());
                    else
                    {
                        constant_value = symbol->variable_value->get_constant_value();
                        wo_assert(!constant_value.is_gcunit());
                    }
                }
            }
        }
    }
}