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
        analyze_pass1(a_namespace->in_scope_sentence);
        end_namespace();

        return true;
    }
    WO_PASS1(ast_varref_defines)
    {
        auto* a_varref_defs = WO_AST();
        a_varref_defs->located_function = in_function();
        for (auto& varref : a_varref_defs->var_refs)
        {
            analyze_pattern_in_pass1(varref.pattern, a_varref_defs->declear_attribute, varref.init_val);
        }
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
    WO_PASS1(ast_value_mutable)
    {
        auto* a_value_mutable_or_pure = WO_AST();

        analyze_pass1(a_value_mutable_or_pure->val);

        if (a_value_mutable_or_pure->val->value_type->is_pending() == false)
        {
            a_value_mutable_or_pure->value_type->set_type(a_value_mutable_or_pure->val->value_type);

            if (a_value_mutable_or_pure->mark_type == +lex_type::l_mut)
                a_value_mutable_or_pure->value_type->set_is_mutable(true);
            else
            {
                wo_assert(a_value_mutable_or_pure->mark_type == +lex_type::l_immut);
                a_value_mutable_or_pure->value_type->set_is_force_immutable();
            }
        }

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
                    fully_update_type(fnd->second.member_type, true);

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
            if (!a_value_idx->from->value_type->is_pending() && !a_value_idx->from->value_type->is_hkt())
            {
                if (a_value_idx->from->value_type->is_array() || a_value_idx->from->value_type->is_vec())
                    a_value_idx->value_type->set_type(a_value_idx->from->value_type->template_arguments[0]);
                else if (a_value_idx->from->value_type->is_dict() || a_value_idx->from->value_type->is_map())
                    a_value_idx->value_type->set_type(a_value_idx->from->value_type->template_arguments[1]);
                else if (a_value_idx->from->value_type->is_tuple())
                {
                    if (a_value_idx->index->is_constant && a_value_idx->index->value_type->is_integer())
                    {
                        // Index tuple must with constant integer.
                        auto index = a_value_idx->index->get_constant_value().integer;
                        if ((size_t)index < a_value_idx->from->value_type->template_arguments.size() && index >= 0)
                        {
                            a_value_idx->value_type->set_type(a_value_idx->from->value_type->template_arguments[(size_t)index]);
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
            }
        }

        return true;
    }
    WO_PASS1(ast_value_assign)
    {
        auto* a_value_assi = WO_AST();

        bool left_value_is_variable_and_has_been_used = true;
        auto* variable = dynamic_cast<ast_value_variable*>(a_value_assi->left);
        if (variable != nullptr)
        {
            auto* symbol = find_value_in_this_scope(variable);
            if (symbol == nullptr || (
                !symbol->is_captured_variable
                && symbol->define_in_function
                && symbol->is_marked_as_used_variable == false
                && (a_value_assi->is_value_assgin == false
                    || a_value_assi->operate == +lex_type::l_value_assign
                    || symbol->static_symbol == false)))
                left_value_is_variable_and_has_been_used = false;
        }
        analyze_pass1(a_value_assi->left);
        if (!left_value_is_variable_and_has_been_used && variable->symbol != nullptr)
            variable->symbol->is_marked_as_used_variable = false;

        analyze_pass1(a_value_assi->right);

        if (a_value_assi->is_value_assgin)
        {
            auto lsymb = dynamic_cast<ast_value_variable*>(a_value_assi->left);
            if (lsymb && lsymb->symbol && !lsymb->symbol->is_template_symbol)
            {
                // If symbol is template variable, delay the type calc.
                a_value_assi->value_type->set_type(a_value_assi->left->value_type);
            }
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
                || a_value_logic_bin->left->value_type->is_bool())
                && a_value_logic_bin->left->value_type->is_same(a_value_logic_bin->right->value_type, true))
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
            sym->is_marked_as_used_variable = true;
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

        if (ast_value_check->aim_type->is_pure_pending())
            lang_anylizer->begin_trying_block();
        else if (ast_value_check->aim_type->is_pending())
        {
            // ready for update..
            fully_update_type(ast_value_check->aim_type, true);
        }

        analyze_pass1(ast_value_check->_be_check_value_node);

        if (ast_value_check->aim_type->is_pure_pending())
            lang_anylizer->end_trying_block();

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
                    argdef->completed_in_pass1 = true;
                    if (!argdef->symbol)
                    {
                        if (argdef->value_type->may_need_update())
                        {
                            // ready for update..
                            fully_update_type(argdef->value_type, true);
                        }

                        if (!argdef->symbol)
                        {
                            if (argdef->arg_name != WO_PSTR(_))
                            {
                                argdef->symbol = define_variable_in_this_scope(
                                    argdef,
                                    argdef->arg_name,
                                    argdef,
                                    argdef->declear_attribute,
                                    template_style::NORMAL, argdef->decl);
                                argdef->symbol->is_argument = true;
                            }
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
                                lang_anylizer->lang_error(lexer::errorlevel::error, a_value_func, WO_ERR_CANNOT_FIND_EXT_SYM_IN_LIB,
                                    a_value_func->externed_func_info->symbol_name.c_str(),
                                    a_value_func->externed_func_info->load_from_lib.c_str());
                        }
                        else
                            a_value_func->is_constant = true;
                    }
                }
                else if (!a_value_func->has_return_value
                    && a_value_func->auto_adjust_return_type
                    && a_value_func->value_type->get_return_type()->type_name == WO_PSTR(pending))
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
                if (!symb_callee->search_from_global_namespace
                    /*&& symb_callee->scope_namespaces.empty()*/)
                {
                    analyze_pass1(a_value_funccall->directed_value_from);

                    symb_callee->directed_function_call = true;

                    a_value_funccall->callee_symbol_in_type_namespace = new ast_value_variable(symb_callee->var_name);
                    a_value_funccall->callee_symbol_in_type_namespace->copy_source_info(a_value_funccall);
                    a_value_funccall->callee_symbol_in_type_namespace->search_from_global_namespace = true;
                    a_value_funccall->callee_symbol_in_type_namespace->searching_begin_namespace_in_pass2 = now_scope();
                    a_value_funccall->callee_symbol_in_type_namespace->scope_namespaces = symb_callee->scope_namespaces;
                    // a_value_funccall->callee_symbol_in_type_namespace wiil search in pass2..
                    a_value_funccall->callee_symbol_in_type_namespace->completed_in_pass1 = true;
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

        wo_assert((a_value_arr->value_type->is_array() || a_value_arr->value_type->is_vec())
            && a_value_arr->value_type->template_arguments.size() == 1);
        if (a_value_arr->value_type->template_arguments[0]->is_pure_pending())
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

        wo_assert((a_value_map->value_type->is_map() || a_value_map->value_type->is_dict())
            && a_value_map->value_type->template_arguments.size() == 2);

        if (a_value_map->value_type->template_arguments[0]->is_pure_pending()
            || a_value_map->value_type->template_arguments[1]->is_pure_pending())
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

        if (a_ret->return_value)
            analyze_pass1(a_ret->return_value);

        auto* located_function_scope = in_function();
        if (!located_function_scope)
        {
            auto* current_namespace_scope = now_scope();

            if (current_namespace_scope->type != lang_scope::scope_type::namespace_scope)
                current_namespace_scope = current_namespace_scope->belong_namespace;
            wo_assert(current_namespace_scope != nullptr);

            if (current_namespace_scope->parent_scope != nullptr)
                lang_anylizer->lang_error(lexer::errorlevel::error, a_ret, WO_ERR_CANNOT_DO_RET_OUSIDE_FUNC);

            a_ret->located_function = nullptr;
        }
        else
        {
            a_ret->located_function = located_function_scope->function_node;
            a_ret->located_function->has_return_value = true;

            wo_assert(a_ret->located_function->value_type->is_complex());

            if (a_ret->return_value)
            {
                // NOTE: DONOT JUDGE FUNCTION'S RETURN VAL TYPE IN PASS1 TO AVOID TYPE MIXED IN CONSTEXPR IF
                if (a_ret->located_function->auto_adjust_return_type)
                {
                    if (a_ret->located_function->delay_adjust_return_type
                        && a_ret->return_value->value_type->is_pending() == false)
                    {
                        auto* func_return_type = a_ret->located_function->value_type->get_return_type();

                        if (func_return_type->is_pending())
                        {
                            a_ret->located_function->value_type->set_ret_type(a_ret->return_value->value_type);
                        }
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
                        lang_anylizer->lang_error(lexer::errorlevel::error, a_ret, WO_ERR_CANNOT_RET_TYPE_AND_TYPE_AT_SAME_TIME, L"void", located_function_scope->function_node->value_type->type_name->c_str());
                    }
                }
                else
                {
                    if (!located_function_scope->function_node->value_type->get_return_type()->is_void())
                        lang_anylizer->lang_error(lexer::errorlevel::error, a_ret, WO_ERR_CANNOT_RET_TYPE_AND_TYPE_AT_SAME_TIME, L"void", located_function_scope->function_node->value_type->type_name->c_str());
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
        //        // lang_anylizer->lang_error(lexer::errorlevel::error, a_using_namespace, WO_ERR_ERR_PLACE_FOR_USING_NAMESPACE);
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

        if (a_using_type_as->new_type_identifier == WO_PSTR(char)
            || ast_type::name_type_pair.find(a_using_type_as->new_type_identifier) != ast_type::name_type_pair.end())
        {
            return lang_anylizer->lang_error(lexer::errorlevel::error, a_using_type_as, WO_ERR_DECL_BUILTIN_TYPE_IS_NOT_ALLOWED);
        }

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

        a_foreach->loop_sentences->marking_label = a_foreach->marking_label;

        analyze_pass1(a_foreach->used_iter_define);
        analyze_pass1(a_foreach->loop_sentences);
        a_foreach->loop_sentences->copy_source_info(a_foreach);

        end_scope();
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
            if (a_pattern_union_value->union_expr)
            {
                a_pattern_union_value->union_expr->completed_in_pass1 = true;
                if (!a_pattern_union_value->union_expr->search_from_global_namespace)
                {
                    a_pattern_union_value->union_expr->searching_begin_namespace_in_pass2 = now_scope();
                    wo_assert(a_pattern_union_value->union_expr->source_file != nullptr);
                }
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
            lang_anylizer->lang_error(lexer::errorlevel::error, a_match_union_case, WO_ERR_UNEXPECT_PATTERN_CASE);

        analyze_pass1(a_match_union_case->in_case_sentence);

        end_scope();
        return true;
    }
    WO_PASS1(ast_value_make_struct_instance)
    {
        auto* a_value_make_struct_instance = WO_AST();
        analyze_pass1(a_value_make_struct_instance->struct_member_vals);
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
        fully_update_type(a_value_make_struct_instance->target_built_types, true);
        if (a_value_make_struct_instance->target_built_types->is_pending() == false)
            a_value_make_struct_instance->value_type->set_type(a_value_make_struct_instance->target_built_types);
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

        if (a_value_trib_expr->judge_expr->is_constant
            && a_value_trib_expr->judge_expr->value_type->is_bool())
        {
            if (a_value_trib_expr->judge_expr->get_constant_value().integer)
            {
                analyze_pass1(a_value_trib_expr->val_if_true, false);
                if (!a_value_trib_expr->val_if_true->value_type->is_pending())
                    a_value_trib_expr->value_type->set_type(a_value_trib_expr->val_if_true->value_type);
            }
            else
            {
                analyze_pass1(a_value_trib_expr->val_or, false);
                if (!a_value_trib_expr->val_or->value_type->is_pending())
                    a_value_trib_expr->value_type->set_type(a_value_trib_expr->val_or->value_type);
            }

            a_value_trib_expr->judge_expr->update_constant_value(lang_anylizer);
        }
        else
        {
            analyze_pass1(a_value_trib_expr->val_if_true, false);
            analyze_pass1(a_value_trib_expr->val_or, false);

            a_value_trib_expr->value_type->set_type(a_value_trib_expr->val_if_true->value_type);
            if (!a_value_trib_expr->value_type->set_mix_types(a_value_trib_expr->val_or->value_type, false))
                a_value_trib_expr->value_type->set_type_with_name(WO_PSTR(pending));
        }
        return true;
    }

    WO_PASS1(ast_value_typeid)
    {
        auto* ast_value_typeid = WO_AST();

        fully_update_type(ast_value_typeid->type, true);

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
                                    lang_anylizer->lang_error(lexer::errorlevel::error, a_ret, WO_ERR_FUNC_RETURN_DIFFERENT_TYPES,
                                        func_return_type->get_type_name(false).c_str(),
                                        a_ret->return_value->value_type->get_type_name(false).c_str());
                                }
                            }
                        }
                    }
                    else
                    {
                        if (!func_return_type->is_pending()
                            && !func_return_type->accept_type(a_ret->return_value->value_type, false))
                        {
                            lang_anylizer->lang_error(lexer::errorlevel::error, a_ret, WO_ERR_FUNC_RETURN_DIFFERENT_TYPES,
                                func_return_type->get_type_name(false).c_str(),
                                a_ret->return_value->value_type->get_type_name(false).c_str());
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
                        lang_anylizer->lang_error(lexer::errorlevel::error, a_ret, WO_ERR_CANNOT_RET_TYPE_AND_TYPE_AT_SAME_TIME, L"void", a_ret->located_function->value_type->type_name->c_str());
                    }
                }
                else
                {
                    if (!a_ret->located_function->value_type->get_return_type()->is_void())
                        lang_anylizer->lang_error(lexer::errorlevel::error, a_ret, WO_ERR_CANNOT_RET_TYPE_AND_TYPE_AT_SAME_TIME, L"void", a_ret->located_function->value_type->type_name->c_str());
                }
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
    WO_PASS2(ast_value_mutable)
    {
        auto* a_value_mutable_or_pure = WO_AST();

        analyze_pass2(a_value_mutable_or_pure->val);

        if (a_value_mutable_or_pure->val->value_type->is_pending() == false)
        {
            a_value_mutable_or_pure->value_type->set_type(a_value_mutable_or_pure->val->value_type);

            if (a_value_mutable_or_pure->mark_type == +lex_type::l_mut)
                a_value_mutable_or_pure->value_type->set_is_mutable(true);
            else
            {
                wo_assert(a_value_mutable_or_pure->mark_type == +lex_type::l_immut);
                a_value_mutable_or_pure->value_type->set_is_force_immutable();
            }
        }
        else
            lang_anylizer->lang_error(lexer::errorlevel::error, a_value_mutable_or_pure->val, WO_ERR_UNABLE_DECIDE_EXPR_TYPE);

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

        wo_assert((a_foreach->marking_label == a_foreach->loop_sentences->marking_label));

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
                lang_anylizer->lang_error(lexer::errorlevel::error, a_match->match_value, WO_ERR_CANNOT_MATCH_SUCH_TYPE,
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
                lang_anylizer->lang_error(lexer::errorlevel::error, case_ast->union_pattern, WO_ERR_CASE_AFTER_DEFAULT_PATTERN);

            if (case_ast->union_pattern->union_expr == nullptr)
            {
                if (case_names.size() >= a_match->match_value->value_type->struct_member_index.size())
                    lang_anylizer->lang_error(lexer::errorlevel::error, case_ast->union_pattern, WO_ERR_USELESS_DEFAULT_PATTERN);

                has_default_pattern = true;
            }
            else if (case_names.end() != case_names.find(case_ast->union_pattern->union_expr->var_name))
                lang_anylizer->lang_error(lexer::errorlevel::error, case_ast->union_pattern->union_expr, WO_ERR_REPEAT_MATCH_CASE);
            else
                case_names.insert(case_ast->union_pattern->union_expr->var_name);
            cases = cases->sibling;
        }
        if (!has_default_pattern && case_names.size() < a_match->match_value->value_type->struct_member_index.size())
            lang_anylizer->lang_error(lexer::errorlevel::error, a_match, WO_ERR_MATCH_CASE_NOT_COMPLETE);

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
                    lang_anylizer->lang_error(lexer::errorlevel::error, a_match_union_case, WO_ERR_UNKNOWN_MATCHING_VAL_TYPE);
                else
                {
                    if (!a_match_union_case->in_match->match_value->value_type->using_type_name->template_arguments.empty())
                    {
                        if (a_pattern_union_value->union_expr == nullptr)
                            ;
                        else
                        {
                            a_pattern_union_value->union_expr->symbol = find_value_in_this_scope(a_pattern_union_value->union_expr);

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
                                            lang_anylizer->lang_error(lexer::errorlevel::error, a_pattern_union_value, WO_ERR_TEMPLATE_ARG_NOT_MATCH);
                                    }
                                }
                            }
                            else
                                ;
                        }
                        /* Donot give error here, it will be given in following 'analyze_pass2' */
                        //lang_anylizer->lang_error(lexer::errorlevel::error, a_pattern_union_value, WO_ERR_UNKNOWN_IDENTIFIER, a_pattern_union_value->union_expr->var_name.c_str());
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
                        a_match_union_case->take_place_value_may_nil->value_type->set_type(a_pattern_union_value->union_expr->value_type->argument_types.front());
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
            analyze_pass2(val);

            val->update_constant_value(lang_anylizer);
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

        return true;
    }
    WO_PASS2(ast_value_function_define)
    {
        auto* a_value_funcdef = WO_AST();

        if (!a_value_funcdef->is_template_define)
        {
            if (a_value_funcdef->is_template_reification)
            {
                wo_asure(begin_template_scope(a_value_funcdef,
                    a_value_funcdef->template_type_name_list,
                    a_value_funcdef->this_reification_template_args));
            }

            auto* last_function = this->current_function_in_pass2;
            this->current_function_in_pass2 = a_value_funcdef->this_func_scope;
            wo_assert(this->current_function_in_pass2 != nullptr);

            size_t anylizer_error_count = lang_anylizer->get_cur_error_frame().size();

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

            if (lang_anylizer->get_cur_error_frame().size() != anylizer_error_count)
            {
                wo_assert(lang_anylizer->get_cur_error_frame().size() > anylizer_error_count);

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

                    for (; anylizer_error_count < lang_anylizer->get_cur_error_frame().size(); ++anylizer_error_count)
                    {
                        a_value_funcdef->where_constraint->unmatched_constraint.push_back(
                            lang_anylizer->get_cur_error_frame()[anylizer_error_count]);
                    }
                }

                // Error happend in cur function
                a_value_funcdef->value_type->get_return_type()->set_type_with_name(WO_PSTR(pending));
            }

            this->current_function_in_pass2 = last_function;

            if (a_value_funcdef->is_template_reification)
                end_template_scope();
        }
        return true;
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
                    || a_value_assi->operate == +lex_type::l_value_assign
                    || symbol->static_symbol == false)))
                left_value_is_variable_and_has_been_used = false;
        }
        analyze_pass2(a_value_assi->left);
        if (!left_value_is_variable_and_has_been_used && variable->symbol != nullptr)
            variable->symbol->is_marked_as_used_variable = false;

        analyze_pass2(a_value_assi->right);

        if (a_value_assi->left->value_type->is_complex())
        {
            auto* symbinfo = dynamic_cast<ast_value_variable*>(a_value_assi->left);

            auto* scope = symbinfo == nullptr || symbinfo->symbol == nullptr
                ? a_value_assi->located_scope
                : symbinfo->symbol->defined_in_scope;

            if (auto right_func_instance = judge_auto_type_of_funcdef_with_type(a_value_assi->right,
                a_value_assi->located_scope,
                a_value_assi->left->value_type, a_value_assi->right, true, nullptr, nullptr))
                a_value_assi->right = std::get<ast::ast_value_function_define*>(right_func_instance.value());
        }

        if (!a_value_assi->left->value_type->accept_type(a_value_assi->right->value_type, false))
        {
            lang_anylizer->lang_error(lexer::errorlevel::error, a_value_assi->right, WO_ERR_SHOULD_BE_TYPE_BUT_GET_UNEXCEPTED_TYPE,
                a_value_assi->left->value_type->get_type_name(false).c_str(),
                a_value_assi->right->value_type->get_type_name(false).c_str());
        }
        else
        {
            if ((a_value_assi->operate == +lex_type::l_div_assign
                || a_value_assi->operate == +lex_type::l_value_div_assign
                || a_value_assi->operate == +lex_type::l_mod_assign
                || a_value_assi->operate == +lex_type::l_value_mod_assign) &&
                a_value_assi->right->is_constant &&
                a_value_assi->right->value_type->is_integer() &&
                a_value_assi->right->get_constant_value().integer == 0)
            {
                lang_anylizer->lang_error(lexer::errorlevel::error, a_value_assi->right,
                    WO_ERR_CANNOT_DIV_ZERO);
            }
        }

        if (!a_value_assi->left->can_be_assign)
            lang_anylizer->lang_error(lexer::errorlevel::error, a_value_assi->left, WO_ERR_CANNOT_ASSIGN_TO_UNASSABLE_ITEM);

        if (a_value_assi->is_value_assgin)
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

        return true;
    }
    WO_PASS2(ast_value_type_judge)
    {
        auto* ast_value_judge = WO_AST();
        analyze_pass2(ast_value_judge->_be_cast_value_node);

        if (ast_value_judge->value_type->is_pending())
            lang_anylizer->lang_error(lexer::errorlevel::error, ast_value_judge, WO_ERR_UNKNOWN_TYPE,
                ast_value_judge->value_type->get_type_name(false).c_str()
            );

        return true;
    }
    WO_PASS2(ast_value_type_check)
    {
        auto* a_value_type_check = WO_AST();

        if (a_value_type_check->is_constant)
            return true;

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
                    }
                    else
                    {
                        lang_anylizer->lang_error(lexer::errorlevel::error, a_value_index, WO_ERR_UNDEFINED_MEMBER,
                            str_to_wstr(*a_value_index->index->get_constant_value().string).c_str());
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

        return true;
    }
    WO_PASS2(ast_value_indexed_variadic_args)
    {
        auto* a_value_variadic_args_idx = WO_AST();
        analyze_pass2(a_value_variadic_args_idx->argindex);

        if (!a_value_variadic_args_idx->argindex->value_type->is_integer())
        {
            lang_anylizer->lang_error(lexer::errorlevel::error, a_value_variadic_args_idx, WO_ERR_FAILED_TO_INDEX_VAARG_ERR_TYPE);
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
            lang_anylizer->lang_error(lexer::errorlevel::error, a_fakevalue_unpacked_args, WO_ERR_NEED_TYPES, L"array, vec" WO_TERM_OR L"tuple");
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
        else if ((a_value_bin->operate == +lex_type::l_div || a_value_bin->operate == +lex_type::l_mod) &&
            a_value_bin->right->is_constant &&
            a_value_bin->right->value_type->is_integer() &&
            a_value_bin->right->get_constant_value().integer == 0)
        {
            lang_anylizer->lang_error(lexer::errorlevel::error, a_value_bin->right, WO_ERR_CANNOT_DIV_ZERO);
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
                L"array<...>",
                a_value_arr->value_type->get_type_name().c_str()
            );
        }
        else if (a_value_arr->is_mutable_vector && !a_value_arr->value_type->is_vec())
        {
            lang_anylizer->lang_error(lexer::errorlevel::error, a_value_arr, WO_ERR_CANNOT_CAST_TYPE_TO_TYPE,
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
                        lang_anylizer->lang_error(lexer::errorlevel::error, val, WO_ERR_DIFFERENT_VAL_TYPE_OF_TEMPLATE, L"array");
                    else
                        lang_anylizer->lang_error(lexer::errorlevel::error, val, WO_ERR_DIFFERENT_VAL_TYPE_OF_TEMPLATE, L"vec");
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
                        lang_anylizer->lang_error(lexer::errorlevel::error, map_pair->key, WO_ERR_DIFFERENT_KEY_TYPE_OF, L"dict", L"dict");
                    else
                        lang_anylizer->lang_error(lexer::errorlevel::error, map_pair->key, WO_ERR_DIFFERENT_KEY_TYPE_OF, L"map", L"map");
                    break;
                }
                if (!decide_map_val_type->accept_type(map_pair->val->value_type, false)
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
                L"dict<..., ...>",
                a_value_map->value_type->get_type_name().c_str()
            );
        }
        else if (a_value_map->is_mutable_map && !a_value_map->value_type->is_map())
        {
            lang_anylizer->lang_error(lexer::errorlevel::error, a_value_map, WO_ERR_CANNOT_CAST_TYPE_TO_TYPE,
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
                            lang_anylizer->lang_error(lexer::errorlevel::error, pairs->key, WO_ERR_DIFFERENT_KEY_TYPE_OF_TEMPLATE, L"dict");
                        else
                            lang_anylizer->lang_error(lexer::errorlevel::error, pairs->key, WO_ERR_DIFFERENT_KEY_TYPE_OF_TEMPLATE, L"map");
                    }
                    if (!a_value_map->value_type->template_arguments[1]->accept_type(pairs->val->value_type, false))
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
                lang_anylizer->lang_error(lexer::errorlevel::error, val, WO_ERR_FAILED_TO_DECIDE_TUPLE_TYPE);
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

                    if (auto result = judge_auto_type_of_funcdef_with_type(membpair, struct_defined_scope,
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

                            if (pending_template_arg = analyze_template_derivation(
                                a_value_make_struct_instance->target_built_types->symbol->template_types[tempindex],
                                a_value_make_struct_instance->target_built_types->symbol->template_types,
                                fnd->second.member_type,
                                updated_member_types[membpair->member_name]
                                ? updated_member_types[membpair->member_name]
                                : membpair->member_value_pair->value_type
                            ))
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

                    if (auto result = judge_auto_type_of_funcdef_with_type(membpair, struct_defined_scope,
                        fnd->second.member_type, membpair->member_value_pair, true,
                        a_value_make_struct_instance->target_built_types->symbol->define_node, &template_args))
                    {
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

                            if (pending_template_arg = analyze_template_derivation(
                                a_value_make_struct_instance->target_built_types->symbol->template_types[tempindex],
                                a_value_make_struct_instance->target_built_types->symbol->template_types,
                                fnd->second.member_type,
                                membpair->member_value_pair->value_type
                            ))
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
                lang_anylizer->lang_error(lexer::errorlevel::error, a_value_make_struct_instance, WO_ERR_FAILED_TO_DECIDE_ALL_TEMPLATE_ARGS);
                lang_anylizer->lang_error(lexer::errorlevel::infom, a_value_make_struct_instance->target_built_types->symbol->define_node, WO_INFO_ITEM_IS_DEFINED_HERE,
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
                        if (auto result = judge_auto_type_of_funcdef_with_type(membpair, struct_defined_scope,
                            fnd->second.member_type, membpair->member_value_pair, true, nullptr, nullptr))
                            membpair->member_value_pair = std::get<ast::ast_value_function_define*>(result.value());

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
        return true;
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

            a_value_trib_expr->judge_expr->update_constant_value(lang_anylizer);
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
            lang_anylizer->lang_error(lexer::errorlevel::error, ast, WO_ERR_FAILED_TO_INVOKE_BECAUSE);
            for (auto& error_info : funcdef->where_constraint->unmatched_constraint)
            {
                lang_anylizer->get_cur_error_frame().push_back(error_info);
            }
        }

    }

    WO_PASS2(ast_value_variable)
    {
        auto* a_value_var = WO_AST();

        auto* sym = find_value_in_this_scope(a_value_var);
        if (sym != nullptr)
            sym->is_marked_as_used_variable = true;

        if (a_value_var->value_type->is_pending())
        {
            if (sym && (!sym->define_in_function || sym->has_been_defined_in_pass2 || sym->is_captured_variable))
            {
                if (sym->is_template_symbol && (!a_value_var->is_auto_judge_function_overload || sym->type == lang_symbol::symbol_type::variable))
                {
                    sym = analyze_pass_template_reification(a_value_var, a_value_var->template_reification_args);
                    if (!sym)
                        lang_anylizer->lang_error(lexer::errorlevel::error, a_value_var, WO_ERR_FAILED_TO_INSTANCE_TEMPLATE_ID,
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
                        if (a_value_var->symbol->type == lang_symbol::symbol_type::function)
                        {
                            if (a_value_var->symbol->is_template_symbol)
                                ; /* function call may be template, do not report error here~ */
                            else
                            {
                                lang_anylizer->lang_error(lexer::errorlevel::error, a_value_var, WO_ERR_UNABLE_DECIDE_EXPR_TYPE);
                                lang_anylizer->lang_error(lexer::errorlevel::infom, sym->variable_value, WO_INFO_ITEM_IS_DEFINED_HERE,
                                    a_value_var->var_name->c_str());
                            }
                        }
                        else
                        {
                            lang_anylizer->lang_error(lexer::errorlevel::error, a_value_var, WO_ERR_UNABLE_DECIDE_EXPR_TYPE);
                            lang_anylizer->lang_error(lexer::errorlevel::infom, sym->variable_value, WO_INFO_INIT_EXPR_IS_HERE,
                                a_value_var->var_name->c_str());
                        }
                    }
                }
            }
            else
            {
                lang_anylizer->lang_error(lexer::errorlevel::error, a_value_var, WO_ERR_UNKNOWN_IDENTIFIER,
                    a_value_var->var_name->c_str());
                auto fuzz_symbol = find_symbol_in_this_scope(a_value_var, a_value_var->var_name, lang_symbol::symbol_type::variable | lang_symbol::symbol_type::function, true);
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
                lang_anylizer->lang_error(lexer::errorlevel::error, a_value_funccall, WO_ERR_FAILED_TO_INVOKE_FUNC_FOR_TYPE,
                    a_value_funccall->callee_symbol_in_type_namespace->var_name->c_str(),
                    a_value_funccall->directed_value_from->value_type->get_type_name(false).c_str());

                // Here to truing to find fuzzy name function.
                lang_symbol* fuzzy_method_function_symbol = nullptr;
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

                calling_function_define = analyze_pass_template_reification(calling_function_define, template_args); //tara~ get analyze_pass_template_reification 
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
            && a_value_funccall->called_func->value_type->is_complex())
            a_value_funccall->value_type->set_type(a_value_funccall->called_func->value_type->get_return_type());

        if (a_value_funccall->called_func
            && a_value_funccall->called_func->value_type->is_complex()
            && a_value_funccall->called_func->value_type->is_pending())
        {
            auto* funcsymb = dynamic_cast<ast_value_function_define*>(a_value_funccall->called_func);

            if (funcsymb && funcsymb->auto_adjust_return_type)
            {
                // If call a pending type function. it means the function's type dudes may fail, mark void to continue..
                if (funcsymb->has_return_value)
                    lang_anylizer->lang_error(lexer::errorlevel::error, funcsymb, WO_ERR_CANNOT_DERIV_FUNCS_RET_TYPE, wo::str_to_wstr(funcsymb->get_ir_func_signature_tag()).c_str());

                funcsymb->value_type->get_return_type()->set_type_with_name(WO_PSTR(void));
                funcsymb->auto_adjust_return_type = false;
            }

        }

        bool failed_to_call_cur_func = false;

        if (a_value_funccall->called_func
            && a_value_funccall->called_func->value_type->is_complex())
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
                    lang_anylizer->lang_error(lexer::errorlevel::error, a_value_funccall, WO_ERR_ARGUMENT_TOO_FEW, a_value_funccall->called_func->value_type->get_type_name(false).c_str());
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
                                        lang_anylizer->lang_error(lexer::errorlevel::error, a_value_funccall, WO_ERR_ARGUMENT_TOO_MANY, a_value_funccall->called_func->value_type->get_type_name(false).c_str());
                                        break;
                                    }
                                    else if (!(*a_type_index)->accept_type(unpacking_tuple_type->template_arguments[unpack_tuple_index], false))
                                    {
                                        failed_to_call_cur_func = true;
                                        lang_anylizer->lang_error(lexer::errorlevel::error, a_value_funccall, WO_ERR_TYPE_CANNOT_BE_CALL, a_value_funccall->called_func->value_type->get_type_name(false).c_str());
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
                                lang_anylizer->lang_error(lexer::errorlevel::error, a_value_funccall, WO_ERR_ARGUMENT_TOO_MANY, a_value_funccall->called_func->value_type->get_type_name(false).c_str());
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
                        size_t tuple_arg_sz = unpackval->unpacked_pack->value_type->using_type_name
                            ? unpackval->unpacked_pack->value_type->using_type_name->template_arguments.size()
                            : unpackval->unpacked_pack->value_type->template_arguments.size();

                        if (tuple_arg_sz != 0)
                        {
                            failed_to_call_cur_func = true;
                            lang_anylizer->lang_error(lexer::errorlevel::error, a_value_funccall, WO_ERR_ARGUMENT_TOO_MANY, a_value_funccall->called_func->value_type->get_type_name(false).c_str());
                        }
                    }
                }
                else
                {
                    failed_to_call_cur_func = true;
                    lang_anylizer->lang_error(lexer::errorlevel::error, a_value_funccall, WO_ERR_ARGUMENT_TOO_MANY, a_value_funccall->called_func->value_type->get_type_name(false).c_str());
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

        return true;
    }
    WO_PASS2(ast_value_typeid)
    {
        auto* ast_value_typeid = WO_AST();

        fully_update_type(ast_value_typeid->type, false);

        if (ast_value_typeid->type->is_pending() && !ast_value_typeid->type->is_hkt())
        {
            lang_anylizer->lang_error(lexer::errorlevel::error, ast_value_typeid->type, WO_ERR_UNKNOWN_TYPE,
                ast_value_typeid->type->get_type_name(false, false).c_str());
        }
        else
        {
            ast_value_typeid->constant_value.set_integer(lang::get_typing_hash_after_pass1(ast_value_typeid->type));
            ast_value_typeid->is_constant = true;
        }

        return true;
    }

    namespace ast
    {
        bool ast_type::is_same(const ast_type* another, bool ignore_prefix) const
        {
            if (is_pending_function() || another->is_pending_function())
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

            if (!ignore_prefix)
            {
                if (is_mutable() != another->is_mutable())
                    return false;
            }

            if (is_complex())
            {
                if (!another->is_complex())
                    return false;

                if (argument_types.size() != another->argument_types.size())
                    return false;
                for (size_t index = 0; index < argument_types.size(); index++)
                {
                    if (!argument_types[index]->is_same(another->argument_types[index], true))
                        return false;
                }
                if (is_variadic_function_type != another->is_variadic_function_type)
                    return false;

                wo_assert(is_complex() && another->is_complex());
                if (!complex_type->is_same(another->complex_type, false))
                    return false;
            }
            else if (another->is_complex())
                return false;

            if (type_name != another->type_name)
                return false;

            if (using_type_name || another->using_type_name)
            {
                if (!using_type_name || !another->using_type_name)
                    return false;

                if (find_type_in_this_scope(using_type_name) != find_type_in_this_scope(another->using_type_name))
                    return false;

                if (using_type_name->template_arguments.size() != another->using_type_name->template_arguments.size())
                    return false;

                for (size_t i = 0; i < using_type_name->template_arguments.size(); ++i)
                    if (!using_type_name->template_arguments[i]->is_same(another->using_type_name->template_arguments[i], false))
                        return false;
            }
            if (has_template())
            {
                if (template_arguments.size() != another->template_arguments.size())
                    return false;
                for (size_t index = 0; index < template_arguments.size(); index++)
                {
                    if (!template_arguments[index]->is_same(another->template_arguments[index], false))
                        return false;
                }
            }
            // NOTE: Only anonymous structs need this check
            if (is_struct() && using_type_name == nullptr)
            {
                wo_assert(another->is_struct());
                if (struct_member_index.size() != another->struct_member_index.size())
                    return false;

                for (auto& [memname, memberinfo] : struct_member_index)
                {
                    auto fnd = another->struct_member_index.find(memname);
                    if (fnd == another->struct_member_index.end())
                        return false;

                    if (memberinfo.offset != fnd->second.offset)
                        return false;

                    if (!memberinfo.member_type->is_same(fnd->second.member_type, false))
                        return false;
                }
            }
            return true;
        }

        bool ast_type::accept_type(const ast_type* another, bool ignore_using_type, bool ignore_prefix) const
        {
            if (is_pending_function() || another->is_pending_function())
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

            if (another->is_nothing())
                return true; // Buttom type, OK

            if (!ignore_prefix)
            {
                if (is_mutable() != another->is_mutable())
                    return false;
            }

            if (is_complex())
            {
                if (!another->is_complex())
                    return false;

                if (argument_types.size() != another->argument_types.size())
                    return false;
                for (size_t index = 0; index < argument_types.size(); index++)
                {
                    // Woolang 1.12.6: Argument types of functions are no longer covariant but
                    //                  invariant during associating and receiving
                    if (!argument_types[index]->is_same(another->argument_types[index], false))
                        return false;
                }
                if (is_variadic_function_type != another->is_variadic_function_type)
                    return false;

                wo_assert(is_complex() && another->is_complex());
                if (!complex_type->accept_type(another->complex_type, ignore_using_type, false))
                    return false;
            }
            else if (another->is_complex())
                return false;

            if (type_name != another->type_name)
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
                    if (!using_type_name->template_arguments[i]->accept_type(another->using_type_name->template_arguments[i], ignore_using_type, false))
                        return false;
            }
            if (has_template())
            {
                if (template_arguments.size() != another->template_arguments.size())
                    return false;
                for (size_t index = 0; index < template_arguments.size(); index++)
                {
                    if (!template_arguments[index]->accept_type(another->template_arguments[index], ignore_using_type, false))
                        return false;
                }
            }
            // NOTE: Only anonymous structs need this check
            if (is_struct() && (using_type_name == nullptr || another->using_type_name == nullptr))
            {
                wo_assert(another->is_struct());
                if (struct_member_index.size() != another->struct_member_index.size())
                    return false;

                for (auto& [memname, memberinfo] : struct_member_index)
                {
                    auto fnd = another->struct_member_index.find(memname);
                    if (fnd == another->struct_member_index.end())
                        return false;

                    if (memberinfo.offset != fnd->second.offset)
                        return false;

                    if (!memberinfo.member_type->is_same(fnd->second.member_type, false))
                        return false;
                }
            }
            return true;
        }
        std::wstring ast_type::get_type_name(bool ignore_using_type, bool ignore_prefix) const
        {
            std::wstring result;

            if (!ignore_prefix)
            {
                if (is_mutable())
                    result += L"mut ";
            }

            if (!ignore_using_type && using_type_name)
            {
                auto namespacechain = (search_from_global_namespace ? L"::" : L"") +
                    wo::str_to_wstr(get_belong_namespace_path_with_lang_scope(using_type_name->symbol));
                result += (namespacechain.empty() ? L"" : namespacechain + L"::")
                    + using_type_name->get_type_name(ignore_using_type, true);
            }
            else
            {
                if (is_complex())
                {
                    result += L"(";
                    for (size_t index = 0; index < argument_types.size(); index++)
                    {
                        result += argument_types[index]->get_type_name(ignore_using_type, false);
                        if (index + 1 != argument_types.size() || is_variadic_function_type)
                            result += L", ";
                    }

                    if (is_variadic_function_type)
                    {
                        result += L"...";
                    }
                    result += L")=>" + complex_type->get_type_name(ignore_using_type, false);
                }
                else
                {
                    if (is_hkt_typing() && symbol)
                    {
                        auto* base_symbol = base_typedef_symbol(symbol);
                        wo_assert(base_symbol && base_symbol->name != nullptr);
                        result += *base_symbol->name;
                    }
                    else if (type_name != WO_PSTR(tuple))
                    {
                        result += *type_name;
                    }

                    if (has_template())
                    {
                        result += (type_name != WO_PSTR(tuple)) ? L"<" : L"(";
                        for (size_t index = 0; index < template_arguments.size(); index++)
                        {
                            result += template_arguments[index]->get_type_name(ignore_using_type, false);
                            if (index + 1 != template_arguments.size())
                                result += L", ";
                        }
                        result += (type_name != WO_PSTR(tuple)) ? L">" : L")";
                    }

                    if (is_struct())
                    {
                        result += L"{";
                        std::vector<const ast_type::struct_member_infos_t::value_type*> memberinfors;
                        memberinfors.reserve(struct_member_index.size());
                        for (auto& memberinfo : this->struct_member_index)
                        {
                            memberinfors.push_back(&memberinfo);
                        }
                        std::sort(memberinfors.begin(), memberinfors.end(),
                            [](const ast_type::struct_member_infos_t::value_type* a,
                                const ast_type::struct_member_infos_t::value_type* b)
                            {
                                return a->second.offset < b->second.offset;
                            });
                        bool first_member = true;
                        for (auto* memberinfo : memberinfors)
                        {
                            if (first_member)
                                first_member = false;
                            else
                                result += L",";

                            result += *memberinfo->first + L":" + memberinfo->second.member_type->get_type_name(false, false);
                        }
                        result += L"}";
                    }
                }
            }
            return result;
        }
        bool ast_type::is_hkt() const
        {
            if (is_complex())
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

    //////////////////////////////////////////////////////////////////////////////////////////////

    uint32_t lang::get_typing_hash_after_pass1(ast::ast_type* typing)
    {
        wo_assert(!typing->is_pending() || typing->is_hkt());

        uint64_t hashval = (uint64_t)typing->value_type;

        if (typing->is_hkt())
        {
            if (typing->value_type != wo::value::valuetype::array_type
                && typing->value_type != wo::value::valuetype::dict_type)
            {
                wo_assert(typing->symbol);
                auto* base_type_symb = ast::ast_type::base_typedef_symbol(typing->symbol);

                if (base_type_symb->type == lang_symbol::symbol_type::type_alias)
                {
                    if (base_type_symb->type_informatiom->using_type_name)
                        hashval = (uint64_t)base_type_symb->type_informatiom->using_type_name->symbol;
                    else
                        hashval = (uint64_t)base_type_symb->type_informatiom->value_type;
                }
                else
                    hashval = (uint64_t)base_type_symb;
            }
            else
            {
                hashval = (uint64_t)reinterpret_cast<intptr_t>(typing->type_name);
            }
        }

        ++hashval;

        if (typing->is_complex())
            hashval += get_typing_hash_after_pass1(typing->complex_type);

        hashval *= hashval;

        if (lang_symbol* using_type_symb = typing->using_type_name ? find_type_in_this_scope(typing->using_type_name) : nullptr)
        {
            hashval <<= 1;
            hashval += (uint64_t)using_type_symb;
            hashval *= hashval;
        }

        if (!typing->is_hkt_typing() && typing->has_template())
        {
            for (auto& template_arg : typing->template_arguments)
            {
                hashval <<= 1;
                hashval += get_typing_hash_after_pass1(template_arg);
                hashval *= hashval;
            }
        }

        if (typing->is_complex())
        {
            for (auto& arg : typing->argument_types)
            {
                hashval <<= 1;
                hashval += get_typing_hash_after_pass1(arg);
                hashval *= hashval;
            }
        }

        if (typing->is_struct() && typing->using_type_name == nullptr)
        {
            for (auto& [member_name, member_info] : typing->struct_member_index)
            {
                hashval ^= std::hash<std::wstring>()(*member_name);
                hashval *= member_info.offset;
                hashval += get_typing_hash_after_pass1(member_info.member_type);
            }
        }

        if (typing->is_mutable())
            hashval *= hashval;

        uint32_t hash32 = hashval & 0xFFFFFFFF;
        while (hashed_typing.find(hash32) != hashed_typing.end())
        {
            if (hashed_typing[hash32]->is_same(typing, false))
                return hash32;
            hash32++;
        }
        hashed_typing[hash32] = typing;

        return hash32;
    }
    bool lang::begin_template_scope(grammar::ast_base* reporterr, const std::vector<wo_pstring_t>& template_defines_args, const std::vector<ast::ast_type*>& template_args)
    {
        wo_assert(reporterr);
        if (template_defines_args.size() != template_args.size())
        {
            lang_anylizer->lang_error(lexer::errorlevel::error, reporterr, WO_ERR_TEMPLATE_ARG_NOT_MATCH);
            return false;
        }

        template_stack.push_back(template_type_map());
        auto& current_template = template_stack.back();
        for (size_t index = 0; index < template_defines_args.size(); index++)
        {
            auto* applying_type = template_args[index];
            if (applying_type == nullptr)
                continue;

            lang_symbol* sym = new lang_symbol;
            sym->attribute = new ast::ast_decl_attribute();
            sym->type = lang_symbol::symbol_type::type_alias;
            sym->name = template_defines_args[index];
            sym->type_informatiom = new ast::ast_type(WO_PSTR(pending));

            sym->type_informatiom->set_type(applying_type);

            sym->defined_in_scope = lang_scopes.back();

            if (sym->type_informatiom->is_hkt())
            {
                sym->is_hkt_typing_symb = true;
                sym->is_template_symbol = true;

                sym->type_informatiom->template_arguments.clear();

                // Update template setting info...
                if (sym->type_informatiom->is_array() || sym->type_informatiom->is_vec())
                {
                    sym->template_types = { WO_PSTR(VT) };
                    sym->type_informatiom->template_arguments.push_back(
                        new ast::ast_type(WO_PSTR(VT)));
                }
                else if (sym->type_informatiom->is_dict() || sym->type_informatiom->is_map())
                {
                    sym->template_types = { WO_PSTR(KT), WO_PSTR(VT) };
                    sym->type_informatiom->template_arguments.push_back(
                        new ast::ast_type(WO_PSTR(KT)));
                    sym->type_informatiom->template_arguments.push_back(
                        new ast::ast_type(WO_PSTR(VT)));
                }
                else
                {
                    wo_assert(sym->type_informatiom->symbol && sym->type_informatiom->symbol->is_template_symbol);

                    sym->template_types = sym->type_informatiom->symbol->template_types;
                    wo_assert(sym->type_informatiom->symbol);

                    for (auto& template_def_name : sym->type_informatiom->symbol->template_types)
                    {
                        sym->type_informatiom->template_arguments.push_back(
                            new ast::ast_type(template_def_name));
                    }
                }
            }

            lang_symbols.push_back(current_template[sym->name] = sym);
        }
        return true;
    }
    bool lang::begin_template_scope(grammar::ast_base* reporterr, ast::ast_defines* template_defines, const std::vector<ast::ast_type*>& template_args)
    {
        return begin_template_scope(reporterr, template_defines->template_type_name_list, template_args);
    }
    void lang::end_template_scope()
    {
        template_stack.pop_back();
    }
    void lang::temporary_entry_scope_in_pass1(lang_scope* scop)
    {
        lang_scopes.push_back(scop);
    }
    lang_scope* lang::temporary_leave_scope_in_pass1()
    {
        auto* scop = lang_scopes.back();

        lang_scopes.pop_back();

        return scop;
    }

    lang::lang(lexer& lex) :
        lang_anylizer(&lex)
    {
        _this_thread_lang_context = this;
        ast::ast_namespace* global = new ast::ast_namespace;
        global->scope_name = wstring_pool::get_pstr(L"");
        global->source_file = wstring_pool::get_pstr(L"::");
        global->col_begin_no =
            global->col_end_no =
            global->row_begin_no =
            global->row_end_no = 1;
        begin_namespace(global);   // global namespace

        // Define 'char' as built-in type
        ast::ast_using_type_as* using_type_def_char = new ast::ast_using_type_as();
        using_type_def_char->new_type_identifier = WO_PSTR(char);
        using_type_def_char->old_type = new ast::ast_type(WO_PSTR(int));
        using_type_def_char->declear_attribute = new ast::ast_decl_attribute();
        using_type_def_char->declear_attribute->add_attribute(lang_anylizer, +lex_type::l_public);
        define_type_in_this_scope(using_type_def_char, using_type_def_char->old_type, using_type_def_char->declear_attribute);
    }
    lang::~lang()
    {
        _this_thread_lang_context = nullptr;
        clean_and_close_lang();
    }
    ast::ast_type* lang::generate_type_instance_by_templates(lang_symbol* symb, const std::vector<ast::ast_type*>& templates)
    {
        std::vector<uint32_t> hashs;
        for (auto& template_type : templates)
        {
            if (template_type->is_pending() && !template_type->is_hkt())
                return nullptr;

            hashs.push_back(get_typing_hash_after_pass1(template_type));
        }

        auto fnd = symb->template_type_instances.find(hashs);
        if (fnd == symb->template_type_instances.end())
        {
            auto& newtype = symb->template_type_instances[hashs];
            newtype = new ast::ast_type(WO_PSTR(pending));

            newtype->set_type(symb->type_informatiom);
            symb->type_informatiom->instance(newtype);

            if (newtype->typefrom != nullptr)
            {
                temporary_entry_scope_in_pass1(symb->defined_in_scope);
                if (begin_template_scope(newtype->typefrom, symb->template_types, templates))
                {
                    auto step_in_pass2 = has_step_in_step2;
                    has_step_in_step2 = false;

                    analyze_pass1(newtype->typefrom, false);

                    // origin_template_func_define->parent->add_child(dumpped_template_func_define);
                    end_template_scope();

                    has_step_in_step2 = step_in_pass2;
                }
                temporary_leave_scope_in_pass1();
            }
        }
        return symb->template_type_instances[hashs];
    }
    bool lang::fully_update_type(ast::ast_type* type, bool in_pass_1, const std::vector<wo_pstring_t>& template_types, std::unordered_set<ast::ast_type*>& s)
    {
        if (s.find(type) != s.end())
            return true;

        s.insert(type);

        if (in_pass_1 && type->searching_begin_namespace_in_pass2 == nullptr)
        {
            type->searching_begin_namespace_in_pass2 = now_scope();
            if (type->source_file == nullptr)
                type->copy_source_info(type->searching_begin_namespace_in_pass2->last_entry_ast);
        }

        if (type->typefrom != nullptr)
        {
            bool is_mutable_typeof = type->is_mutable();
            bool is_force_immut_typeof = type->is_force_immutable();

            auto used_type_info = type->using_type_name;

            if (in_pass_1)
                analyze_pass1(type->typefrom, false);
            if (has_step_in_step2)
                analyze_pass2(type->typefrom, false);

            if (!type->typefrom->value_type->is_pending())
            {
                if (type->is_complex())
                    type->set_ret_type(type->typefrom->value_type);
                else
                    type->set_type(type->typefrom->value_type);
            }

            if (used_type_info)
                type->using_type_name = used_type_info;
            if (is_mutable_typeof)
                type->set_is_mutable(true);
            if (is_force_immut_typeof)
                type->set_is_force_immutable();
        }

        if (type->using_type_name)
        {
            if (!type->using_type_name->symbol)
                type->using_type_name->symbol = find_type_in_this_scope(type->using_type_name);
        }

        // todo: begin_template_scope here~
        if (type->has_custom())
        {
            bool stop_update = false;
            if (type->is_complex())
            {
                if (type->complex_type->has_custom() && !type->complex_type->is_hkt())
                {
                    if (fully_update_type(type->complex_type, in_pass_1, template_types, s))
                    {
                        if (type->complex_type->has_custom() && !type->complex_type->is_hkt())
                            stop_update = true;
                    }
                }
                for (auto& a_t : type->argument_types)
                {
                    if (a_t->has_custom() && !a_t->is_hkt())
                        if (fully_update_type(a_t, in_pass_1, template_types, s))
                        {
                            if (a_t->has_custom() && !a_t->is_hkt())
                                stop_update = true;
                        }
                }
            }

            if (type->has_template())
            {
                for (auto* template_type : type->template_arguments)
                {
                    if (template_type->has_custom() && !template_type->is_hkt())
                        if (fully_update_type(template_type, in_pass_1, template_types, s))
                            if (template_type->has_custom() && !template_type->is_hkt())
                                stop_update = true;
                }
            }

            // ready for update..
            if (!stop_update && ast::ast_type::is_custom_type(type->type_name))
            {
                if (!type->scope_namespaces.empty() ||
                    type->search_from_global_namespace ||
                    template_types.end() == std::find(template_types.begin(), template_types.end(), type->type_name))
                {
                    lang_symbol* type_sym = find_type_in_this_scope(type);

                    if (nullptr == type_sym)
                        type_sym = type->symbol;

                    if (traving_symbols.find(type_sym) != traving_symbols.end())
                        return false;

                    struct traving_guard
                    {
                        lang* _lang;
                        lang_symbol* _tving_node;
                        traving_guard(lang* _lg, lang_symbol* ast_ndoe)
                            : _lang(_lg)
                            , _tving_node(ast_ndoe)
                        {
                            _lang->traving_symbols.insert(_tving_node);
                        }
                        ~traving_guard()
                        {
                            _lang->traving_symbols.erase(_tving_node);
                        }
                    };

                    if (type_sym
                        && type_sym->type != lang_symbol::symbol_type::typing
                        && type_sym->type != lang_symbol::symbol_type::type_alias)
                    {
                        lang_anylizer->lang_error(lexer::errorlevel::error, type, WO_ERR_IS_NOT_A_TYPE, type_sym->name->c_str());
                        type_sym = nullptr;
                    }

                    traving_guard g1(this, type_sym);

                    if (type_sym)
                    {
                        auto already_has_using_type_name = type->using_type_name;

                        auto type_has_mutable_mark = type->is_mutable();
                        bool is_force_immut_typeof = type->is_force_immutable();

                        bool using_template = false;
                        auto using_template_args = type->template_arguments;

                        type->symbol = type_sym;
                        if (type_sym->template_types.size() != type->template_arguments.size())
                        {
                            // Template count is not match.
                            if (type->has_template())
                            {
                                if (in_pass_1 == false)
                                {
                                    // Error! if template_arguments.size() is 0, it will be 
                                    // high-ranked-templated type.
                                    if (type->template_arguments.size() > type_sym->template_types.size())
                                    {
                                        lang_anylizer->lang_error(lexer::errorlevel::error, type, WO_ERR_TOO_MANY_TEMPLATE_ARGS,
                                            type_sym->name->c_str());
                                    }
                                    else
                                    {
                                        wo_assert(type->template_arguments.size() < type_sym->template_types.size());
                                        lang_anylizer->lang_error(lexer::errorlevel::error, type, WO_ERR_TOO_FEW_TEMPLATE_ARGS,
                                            type_sym->name->c_str());
                                    }

                                    lang_anylizer->lang_error(lexer::errorlevel::infom, type, WO_INFO_THE_TYPE_IS_ALIAS_OF,
                                        type_sym->name->c_str(),
                                        type_sym->type_informatiom->get_type_name(false).c_str());
                                }

                            }
                        }
                        else
                        {
                            if (type->has_template())
                                using_template =
                                type_sym->define_node != nullptr ?
                                begin_template_scope(type, type_sym->define_node, type->template_arguments)
                                : (type_sym->type_informatiom->using_type_name
                                    ? begin_template_scope(type, type_sym->type_informatiom->using_type_name->symbol->define_node, type->template_arguments)
                                    : begin_template_scope(type, type_sym->template_types, type->template_arguments));

                            ast::ast_type* symboled_type = nullptr;

                            if (using_template)
                            {
                                // template arguments not anlyzed.
                                if (auto* template_instance_type =
                                    generate_type_instance_by_templates(type_sym, type->template_arguments))
                                    // Get template type instance.
                                    symboled_type = template_instance_type;
                                else
                                {
                                    // Failed to instance current template type, skip.
                                    end_template_scope();
                                    return false;
                                }
                            }
                            else
                                symboled_type = type_sym->type_informatiom;
                            //symboled_type->set_type(type_sym->type_informatiom);

                            wo_assert(symboled_type != nullptr);
                            fully_update_type(symboled_type, in_pass_1, template_types, s);

                            // NOTE: In old version, function's return type is not stores in complex.
                            //       But now, the type here cannot be a function.
                            wo_assert(type->is_complex() == false);

                            // NOTE: Set type will make new instance of typefrom, but it will cause symbol loss.
                            //       To avoid dup-copy, we will let new type use typedef's typefrom directly.
                            do {
                                auto* defined_typefrom = symboled_type->typefrom;
                                symboled_type->typefrom = nullptr;
                                type->set_type(symboled_type);
                                type->typefrom = symboled_type->typefrom = defined_typefrom;
                            } while (false);

                            if (already_has_using_type_name)
                                type->using_type_name = already_has_using_type_name;
                            else if (type_sym->type != lang_symbol::symbol_type::type_alias)
                            {
                                auto* using_type = new ast::ast_type(type_sym->name);
                                using_type->template_arguments = type->template_arguments;

                                type->using_type_name = using_type;
                                type->using_type_name->template_arguments = using_template_args;

                                // Gen namespace chain
                                auto* inscopes = type_sym->defined_in_scope;
                                while (inscopes && inscopes->belong_namespace)
                                {
                                    if (inscopes->type == lang_scope::scope_type::namespace_scope)
                                    {
                                        type->using_type_name->scope_namespaces.insert(
                                            type->using_type_name->scope_namespaces.begin(),
                                            inscopes->scope_namespace);
                                    }
                                    inscopes = inscopes->belong_namespace;
                                }

                                type->using_type_name->symbol = type_sym;
                            }

                            if (type_has_mutable_mark)
                                // TODO; REPEATED MUT SIGN NEED REPORT ERROR?
                                type->set_is_mutable(true);
                            if (is_force_immut_typeof)
                                type->set_is_force_immutable();
                            if (using_template)
                                end_template_scope();
                        }// end template argu check & update
                    }
                }
            }
        }

        for (auto& [name, struct_info] : type->struct_member_index)
        {
            if (struct_info.member_type)
            {
                if (in_pass_1)
                    fully_update_type(struct_info.member_type, in_pass_1, template_types, s);
                if (has_step_in_step2)
                    fully_update_type(struct_info.member_type, in_pass_1, template_types, s);
            }
        }

        wo_test(!type->using_type_name || !type->using_type_name->using_type_name);
        return false;
    }
    void lang::fully_update_type(ast::ast_type* type, bool in_pass_1, const std::vector<wo_pstring_t>& template_types)
    {
        std::unordered_set<ast::ast_type*> us;
        wo_assert(type != nullptr);
        wo_asure(!fully_update_type(type, in_pass_1, template_types, us));
        wo_assert(type->using_type_name == nullptr || (type->is_mutable() == type->using_type_name->is_mutable()));
    }

    void lang::analyze_pattern_in_pass1(ast::ast_pattern_base* pattern, ast::ast_decl_attribute* attrib, ast::ast_value* initval)
    {
        using namespace ast;

        if (ast_pattern_takeplace* a_pattern_takeplace = dynamic_cast<ast_pattern_takeplace*>(pattern))
        {
            analyze_pass1(initval);
        }
        else
        {
            if (ast_pattern_identifier* a_pattern_identifier = dynamic_cast<ast_pattern_identifier*>(pattern))
            {
                // Merge all attrib 
                a_pattern_identifier->attr->attributes.insert(attrib->attributes.begin(), attrib->attributes.end());

                if (a_pattern_identifier->template_arguments.empty())
                {
                    analyze_pass1(initval);

                    if (!a_pattern_identifier->symbol)
                    {
                        a_pattern_identifier->symbol = define_variable_in_this_scope(
                            a_pattern_identifier,
                            a_pattern_identifier->identifier,
                            initval,
                            a_pattern_identifier->attr,
                            template_style::NORMAL,
                            a_pattern_identifier->decl);
                    }
                }
                else
                {
                    // Template variable!!! we just define symbol here.
                    if (!a_pattern_identifier->symbol)
                    {
                        auto* symb = define_variable_in_this_scope(
                            a_pattern_identifier,
                            a_pattern_identifier->identifier,
                            initval,
                            a_pattern_identifier->attr,
                            template_style::IS_TEMPLATE_VARIABLE_DEFINE,
                            a_pattern_identifier->decl);
                        symb->is_template_symbol = true;
                        wo_assert(symb->template_types.empty());
                        symb->template_types = a_pattern_identifier->template_arguments;
                        a_pattern_identifier->symbol = symb;
                    }
                }
            }
            else if (ast_pattern_tuple* a_pattern_tuple = dynamic_cast<ast_pattern_tuple*>(pattern))
            {
                analyze_pass1(initval);
                for (auto* take_place : a_pattern_tuple->tuple_takeplaces)
                {
                    take_place->copy_source_info(pattern);
                }
                if (initval->value_type->is_tuple() && !initval->value_type->is_pending()
                    && initval->value_type->template_arguments.size() == a_pattern_tuple->tuple_takeplaces.size())
                {
                    for (size_t i = 0; i < a_pattern_tuple->tuple_takeplaces.size(); i++)
                    {
                        a_pattern_tuple->tuple_takeplaces[i]->value_type->set_type(initval->value_type->template_arguments[i]);
                    }
                }
                for (size_t i = 0; i < a_pattern_tuple->tuple_takeplaces.size(); i++)
                    analyze_pattern_in_pass1(a_pattern_tuple->tuple_patterns[i], attrib, a_pattern_tuple->tuple_takeplaces[i]);
            }
            else
                lang_anylizer->lang_error(lexer::errorlevel::error, pattern, WO_ERR_UNEXPECT_PATTERN_MODE);
        }
    }
    void lang::analyze_pattern_in_pass2(ast::ast_pattern_base* pattern, ast::ast_value* initval)
    {
        using namespace ast;

        if (ast_pattern_takeplace* a_pattern_takeplace = dynamic_cast<ast_pattern_takeplace*>(pattern))
        {
            analyze_pass2(initval);
        }
        else
        {
            if (ast_pattern_identifier* a_pattern_identifier = dynamic_cast<ast_pattern_identifier*>(pattern))
            {
                if (a_pattern_identifier->template_arguments.empty())
                {
                    analyze_pass2(initval);
                    a_pattern_identifier->symbol->has_been_defined_in_pass2 = true;
                }
                else
                {
                    a_pattern_identifier->symbol->has_been_defined_in_pass2 = true;
                    for (auto& [_, impl_symbol] : a_pattern_identifier->symbol->template_typehashs_reification_instance_symbol_list)
                    {
                        impl_symbol->has_been_defined_in_pass2 = true;
                    }
                }
            }
            else if (ast_pattern_tuple* a_pattern_tuple = dynamic_cast<ast_pattern_tuple*>(pattern))
            {
                analyze_pass2(initval);

                if (initval->value_type->is_tuple() && !initval->value_type->is_pending()
                    && initval->value_type->template_arguments.size() == a_pattern_tuple->tuple_takeplaces.size())
                {
                    for (size_t i = 0; i < a_pattern_tuple->tuple_takeplaces.size(); i++)
                    {
                        a_pattern_tuple->tuple_takeplaces[i]->value_type->set_type(initval->value_type->template_arguments[i]);
                    }
                    for (size_t i = 0; i < a_pattern_tuple->tuple_takeplaces.size(); i++)
                        analyze_pattern_in_pass2(a_pattern_tuple->tuple_patterns[i], a_pattern_tuple->tuple_takeplaces[i]);

                }
                else
                {
                    if (initval->value_type->is_pending())
                        lang_anylizer->lang_error(lexer::errorlevel::error, pattern, WO_ERR_UNMATCHED_PATTERN_TYPE_NOT_DECIDED);
                    else if (!initval->value_type->is_tuple())
                        lang_anylizer->lang_error(lexer::errorlevel::error, pattern, WO_ERR_UNMATCHED_PATTERN_TYPE_EXPECT_TUPLE,
                            initval->value_type->get_type_name(false).c_str());
                    else if (initval->value_type->template_arguments.size() != a_pattern_tuple->tuple_takeplaces.size())
                        lang_anylizer->lang_error(lexer::errorlevel::error, pattern, WO_ERR_UNMATCHED_PATTERN_TYPE_TUPLE_DNT_MATCH,
                            (int)a_pattern_tuple->tuple_takeplaces.size(),
                            (int)initval->value_type->template_arguments.size());
                }
            }
            else
                lang_anylizer->lang_error(lexer::errorlevel::error, pattern, WO_ERR_UNEXPECT_PATTERN_MODE);
        }
    }
    void lang::analyze_pattern_in_finalize(ast::ast_pattern_base* pattern, ast::ast_value* initval, bool in_pattern_expr, ir_compiler* compiler)
    {
        using namespace ast;
        using namespace opnum;

        if (ast_pattern_identifier* a_pattern_identifier = dynamic_cast<ast_pattern_identifier*>(pattern))
        {
            if (a_pattern_identifier->template_arguments.empty())
            {
                if (a_pattern_identifier->symbol->is_marked_as_used_variable == false
                    && a_pattern_identifier->symbol->define_in_function == true)
                {
                    auto* scope = a_pattern_identifier->symbol->defined_in_scope;
                    while (scope->type != wo::lang_scope::scope_type::function_scope)
                        scope = scope->parent_scope;

                    lang_anylizer->lang_error(lexer::errorlevel::error, pattern, WO_ERR_UNUSED_VARIABLE_DEFINE,
                        a_pattern_identifier->identifier->c_str(),
                        str_to_wstr(scope->function_node->get_ir_func_signature_tag()).c_str()
                    );
                }
                if (!a_pattern_identifier->symbol->is_constexpr_or_immut_no_closure_func())
                {
                    auto ref_ob = get_opnum_by_symbol(pattern, a_pattern_identifier->symbol, compiler);

                    if (auto** opnum = std::get_if<opnum::opnumbase*>(&ref_ob))
                    {
                        if (ast_value_takeplace* valtkpls = dynamic_cast<ast_value_takeplace*>(initval);
                            !valtkpls || valtkpls->used_reg)
                        {
                            compiler->mov(**opnum, analyze_value(initval, compiler));
                        }
                    }
                    else
                    {
                        auto stackoffset = std::get<int16_t>(ref_ob);
                        compiler->sts(analyze_value(initval, compiler), imm(stackoffset));
                    }
                }
            }
            else
            {
                // Template variable impl here, give init value to correct place.
                const auto& all_template_impl_variable_symbol
                    = a_pattern_identifier->symbol->template_typehashs_reification_instance_symbol_list;
                for (auto& [_, symbol] : all_template_impl_variable_symbol)
                {
                    if (!symbol->is_constexpr_or_immut_no_closure_func())
                    {
                        auto ref_ob = get_opnum_by_symbol(a_pattern_identifier, symbol, compiler);

                        if (auto** opnum = std::get_if<opnum::opnumbase*>(&ref_ob))
                        {
                            if (ast_value_takeplace* valtkpls = dynamic_cast<ast_value_takeplace*>(initval);
                                !valtkpls || valtkpls->used_reg)
                            {
                                compiler->mov(**opnum, analyze_value(symbol->variable_value, compiler));
                            }
                        }
                        else
                        {
                            auto stackoffset = std::get<int16_t>(ref_ob);
                            compiler->sts(analyze_value(initval, compiler), imm(stackoffset));
                        }
                    }
                } // end of for (auto& [_, symbol] : all_template_impl_variable_symbol)

            }
        }
        else if (ast_pattern_tuple* a_pattern_tuple = dynamic_cast<ast_pattern_tuple*>(pattern))
        {
            auto& struct_val = analyze_value(initval, compiler);
            auto& current_values = get_useable_register_for_pure_value();
            for (size_t i = 0; i < a_pattern_tuple->tuple_takeplaces.size(); i++)
            {
                // FOR OPTIMIZE, SKIP TAKEPLACE PATTERN
                if (dynamic_cast<ast_pattern_takeplace*>(a_pattern_tuple->tuple_patterns[i]) == nullptr)
                {
                    compiler->idstruct(current_values, struct_val, (uint16_t)i);
                    a_pattern_tuple->tuple_takeplaces[i]->used_reg = &current_values;

                    analyze_pattern_in_finalize(a_pattern_tuple->tuple_patterns[i], a_pattern_tuple->tuple_takeplaces[i], true, compiler);
                }
            }
            complete_using_register(current_values);
        }
        else if (ast_pattern_takeplace* a_pattern_takeplace = dynamic_cast<ast_pattern_takeplace*>(pattern))
        {
            // DO NOTHING
            if (!in_pattern_expr)
                analyze_value(initval, compiler);
        }
        else
            lang_anylizer->lang_error(lexer::errorlevel::error, pattern, WO_ERR_UNEXPECT_PATTERN_MODE);
    }

    void lang::collect_ast_nodes_for_pass1(grammar::ast_base* ast_node)
    {
        std::vector<grammar::ast_base*> _pass1_analyze_ast_nodes_list;
        std::stack<grammar::ast_base*> _pending_ast_nodes;

        _pending_ast_nodes.push(ast_node);

        while (!_pending_ast_nodes.empty())
        {
            auto* cur_node = _pending_ast_nodes.top();
            _pending_ast_nodes.pop();

            if (_pass1_analyze_ast_nodes_list.end() !=
                std::find(_pass1_analyze_ast_nodes_list.begin(),
                    _pass1_analyze_ast_nodes_list.end(),
                    cur_node))
                // Cur node has been append to list. return!
                continue;

            _pass1_analyze_ast_nodes_list.push_back(cur_node);
        }
    }

    void lang::analyze_pass1(grammar::ast_base* ast_node, bool type_degradation)
    {
#define WO_TRY_BEGIN do{
#define WO_TRY_PASS(NODETYPE) if(pass1_##NODETYPE(dynamic_cast<NODETYPE*>(ast_node)))break;
#define WO_TRY_END }while(0)
        entry_pass ep1(in_pass2, false);

        if (!ast_node)return;

        if (ast_node->completed_in_pass1)
            return;

        ast_node->completed_in_pass1 = true;

        using namespace ast;

        ast_node->located_scope = now_scope();

        if (ast_symbolable_base* a_symbol_ob = dynamic_cast<ast_symbolable_base*>(ast_node))
        {
            a_symbol_ob->searching_begin_namespace_in_pass2 = now_scope();
            if (a_symbol_ob->source_file == nullptr)
                a_symbol_ob->copy_source_info(a_symbol_ob->searching_begin_namespace_in_pass2->last_entry_ast);
        }
        if (ast_value* a_value = dynamic_cast<ast_value*>(ast_node))
        {
            a_value->value_type->copy_source_info(a_value);
        }

        ///////////////////////////////////////////////////////////////////////////////////////////
        WO_TRY_BEGIN;
        WO_TRY_PASS(ast_namespace);
        WO_TRY_PASS(ast_varref_defines);
        WO_TRY_PASS(ast_value_binary);
        WO_TRY_PASS(ast_value_mutable);
        WO_TRY_PASS(ast_value_index);
        WO_TRY_PASS(ast_value_assign);
        WO_TRY_PASS(ast_value_logical_binary);
        WO_TRY_PASS(ast_value_variable);
        WO_TRY_PASS(ast_value_type_cast);
        WO_TRY_PASS(ast_value_type_judge);
        WO_TRY_PASS(ast_value_type_check);
        WO_TRY_PASS(ast_value_function_define);
        WO_TRY_PASS(ast_fakevalue_unpacked_args);
        WO_TRY_PASS(ast_value_funccall);
        WO_TRY_PASS(ast_value_array);
        WO_TRY_PASS(ast_value_mapping);
        WO_TRY_PASS(ast_value_indexed_variadic_args);
        WO_TRY_PASS(ast_value_typeid);
        WO_TRY_PASS(ast_return);
        WO_TRY_PASS(ast_sentence_block);
        WO_TRY_PASS(ast_if);
        WO_TRY_PASS(ast_while);
        WO_TRY_PASS(ast_forloop);
        WO_TRY_PASS(ast_value_unary);
        WO_TRY_PASS(ast_mapping_pair);
        WO_TRY_PASS(ast_using_namespace);
        WO_TRY_PASS(ast_using_type_as);
        WO_TRY_PASS(ast_foreach);
        WO_TRY_PASS(ast_union_make_option_ob_to_cr_and_ret);
        WO_TRY_PASS(ast_match);
        WO_TRY_PASS(ast_match_union_case);
        WO_TRY_PASS(ast_value_make_struct_instance);
        WO_TRY_PASS(ast_value_make_tuple_instance);
        WO_TRY_PASS(ast_struct_member_define);
        WO_TRY_PASS(ast_where_constraint);
        WO_TRY_PASS(ast_value_trib_expr);

        grammar::ast_base* child = ast_node->children;
        while (child)
        {
            analyze_pass1(child);
            child = child->sibling;
        }

        WO_TRY_END;

        if (ast_value* a_val = dynamic_cast<ast_value*>(ast_node))
        {
            if (ast_defines* a_def = dynamic_cast<ast_defines*>(ast_node);
                a_def && a_def->is_template_define)
            {
                // Do nothing
            }
            else
            {
                wo_assert(a_val->value_type != nullptr);

                if (a_val->value_type->is_pending())
                {
                    if (!dynamic_cast<ast_value_function_define*>(a_val))
                        // ready for update..
                        fully_update_type(a_val->value_type, true);
                }
                if (!a_val->value_type->is_pending())
                {
                    if (type_degradation && dynamic_cast<ast_value_mutable*>(a_val) == nullptr)
                    {
                        if (a_val->value_type->is_mutable())
                        {
                            a_val->can_be_assign = true;
                            a_val->value_type->set_is_mutable(false);
                        }
                    }
                }
                a_val->update_constant_value(lang_anylizer);
            }

        }
#undef WO_TRY_BEGIN
#undef WO_TRY_PASS
#undef WO_TRY_END

        wo_assert(ast_node->completed_in_pass1);
    }

    lang_symbol* lang::analyze_pass_template_reification(ast::ast_value_variable* origin_variable, std::vector<ast::ast_type*> template_args_types)
    {
        // NOTE: template_args_types will be modified in this function. donot use ref type.

        using namespace ast;
        std::vector<uint32_t> template_args_hashtypes;

        wo_assert(origin_variable->symbol != nullptr);
        if (origin_variable->symbol->type == lang_symbol::symbol_type::function)
        {
            if (origin_variable->symbol->get_funcdef()->is_template_define
                && origin_variable->symbol->get_funcdef()->template_type_name_list.size() == template_args_types.size())
            {
                // TODO: finding repeated template? goon
                ast_value_function_define* dumpped_template_func_define =
                    analyze_pass_template_reification(origin_variable->symbol->get_funcdef(), template_args_types);

                if (dumpped_template_func_define)
                    return dumpped_template_func_define->this_reification_lang_symbol;
                return nullptr;
            }
            else if (!template_args_types.empty())
            {
                lang_anylizer->lang_error(lexer::errorlevel::error, origin_variable, WO_ERR_NO_MATCHED_FUNC_TEMPLATE);
                return nullptr;
            }
            else
                // NOTE: Not specify template arguments, might want auto impl?
                // Give error in finalize step.
                return origin_variable->symbol;

        }
        else if (origin_variable->symbol->type == lang_symbol::symbol_type::variable)
        {
            // Is variable?
            for (auto temtype : template_args_types)
            {
                auto step_in_pass2 = has_step_in_step2;
                has_step_in_step2 = true;
                fully_update_type(temtype, true, origin_variable->symbol->template_types);
                has_step_in_step2 = step_in_pass2;

                template_args_hashtypes.push_back(get_typing_hash_after_pass1(temtype));
            }

            if (auto fnd = origin_variable->symbol->template_typehashs_reification_instance_symbol_list.find(template_args_hashtypes);
                fnd != origin_variable->symbol->template_typehashs_reification_instance_symbol_list.end())
            {
                return fnd->second;
            }

            ast_value* dumpped_template_init_value = dynamic_cast<ast_value*>(origin_variable->symbol->variable_value->instance());
            wo_assert(dumpped_template_init_value);

            lang_symbol* template_reification_symb = nullptr;

            temporary_entry_scope_in_pass1(origin_variable->symbol->defined_in_scope);
            if (begin_template_scope(origin_variable, origin_variable->symbol->template_types, template_args_types))
            {
                auto step_in_pass2 = has_step_in_step2;
                has_step_in_step2 = false;

                analyze_pass1(dumpped_template_init_value);

                template_reification_symb = define_variable_in_this_scope(
                    dumpped_template_init_value,
                    origin_variable->var_name,
                    dumpped_template_init_value,
                    origin_variable->symbol->attribute,
                    template_style::IS_TEMPLATE_VARIABLE_IMPL,
                    origin_variable->symbol->decl);
                end_template_scope();

                has_step_in_step2 = step_in_pass2;
            }
            temporary_leave_scope_in_pass1();
            origin_variable->symbol->template_typehashs_reification_instance_symbol_list[template_args_hashtypes] = template_reification_symb;
            return template_reification_symb;
        }
        else
        {
            lang_anylizer->lang_error(lexer::errorlevel::error, origin_variable, WO_ERR_NO_TEMPLATE_VARIABLE_OR_FUNCTION);
            return nullptr;
        }
    }

    ast::ast_value_function_define* lang::analyze_pass_template_reification(ast::ast_value_function_define* origin_template_func_define, std::vector<ast::ast_type*> template_args_types)
    {
        // NOTE: template_args_types will be modified in this function. donot use ref type.

        using namespace ast;

        std::vector<uint32_t> template_args_hashtypes;
        for (auto temtype : template_args_types)
        {
            auto step_in_pass2 = has_step_in_step2;
            has_step_in_step2 = true;
            fully_update_type(temtype, true, origin_template_func_define->template_type_name_list);

            has_step_in_step2 = step_in_pass2;

            if (temtype->is_pending() && !temtype->is_hkt())
            {
                lang_anylizer->lang_error(lexer::errorlevel::error, temtype, WO_ERR_UNKNOWN_TYPE, temtype->get_type_name(false).c_str());
                return nullptr;
            }
            template_args_hashtypes.push_back(get_typing_hash_after_pass1(temtype));
        }

        ast_value_function_define* dumpped_template_func_define = nullptr;

        if (auto fnd = origin_template_func_define->template_typehashs_reification_instance_list.find(template_args_hashtypes);
            fnd != origin_template_func_define->template_typehashs_reification_instance_list.end())
        {
            return  dynamic_cast<ast_value_function_define*>(fnd->second);
        }

        dumpped_template_func_define = dynamic_cast<ast_value_function_define*>(origin_template_func_define->instance());
        wo_assert(dumpped_template_func_define);

        // Reset compile state..
        dumpped_template_func_define->symbol = nullptr;
        dumpped_template_func_define->searching_begin_namespace_in_pass2 = nullptr;
        dumpped_template_func_define->completed_in_pass2 = false;
        dumpped_template_func_define->is_template_define = false;
        dumpped_template_func_define->is_template_reification = true;
        dumpped_template_func_define->this_reification_template_args = template_args_types;

        origin_template_func_define->template_typehashs_reification_instance_list[template_args_hashtypes] =
            dumpped_template_func_define;

        wo_assert(origin_template_func_define->symbol == nullptr
            || origin_template_func_define->symbol->defined_in_scope == origin_template_func_define->this_func_scope->parent_scope);

        temporary_entry_scope_in_pass1(origin_template_func_define->this_func_scope->parent_scope);
        if (begin_template_scope(dumpped_template_func_define, origin_template_func_define, template_args_types))
        {
            auto step_in_pass2 = has_step_in_step2;
            has_step_in_step2 = false;

            analyze_pass1(dumpped_template_func_define);

            // origin_template_func_define->parent->add_child(dumpped_template_func_define);
            end_template_scope();

            has_step_in_step2 = step_in_pass2;
        }
        temporary_leave_scope_in_pass1();

        lang_symbol* template_reification_symb = new lang_symbol;

        template_reification_symb->type = lang_symbol::symbol_type::function;
        template_reification_symb->name = dumpped_template_func_define->function_name;
        template_reification_symb->defined_in_scope = now_scope();
        template_reification_symb->attribute = dumpped_template_func_define->declear_attribute;
        template_reification_symb->variable_value = dumpped_template_func_define;

        dumpped_template_func_define->this_reification_lang_symbol = template_reification_symb;

        lang_symbols.push_back(template_reification_symb);

        return dumpped_template_func_define;
    }

    std::optional<lang::judge_result_t> lang::judge_auto_type_of_funcdef_with_type(
        grammar::ast_base* errreport,
        lang_scope* located_scope,
        ast::ast_type* param,
        ast::ast_value* callaim,
        bool update,
        ast::ast_defines* template_defines,
        const std::vector<ast::ast_type*>* template_args)
    {
        wo_assert(located_scope != nullptr);
        if (!param->is_complex())
            return std::nullopt;

        ast::ast_value_function_define* function_define = nullptr;
        if (auto* variable = dynamic_cast<ast::ast_symbolable_base*>(callaim))
        {
            if (variable->symbol != nullptr && variable->symbol->type == lang_symbol::symbol_type::function)
                function_define = variable->symbol->get_funcdef();
        }
        if (auto* funcdef = dynamic_cast<ast::ast_value_function_define*>(callaim))
            function_define = funcdef;

        if (function_define != nullptr && function_define->is_template_define
            && function_define->value_type->argument_types.size() == param->argument_types.size())
        {
            ast::ast_type* new_type = dynamic_cast<ast::ast_type*>(function_define->value_type->instance(nullptr));
            wo_assert(new_type->is_complex());

            std::vector<ast::ast_type*> arg_func_template_args(function_define->template_type_name_list.size(), nullptr);

            for (size_t tempindex = 0; tempindex < function_define->template_type_name_list.size(); ++tempindex)
            {
                if (arg_func_template_args[tempindex] == nullptr)
                    for (size_t index = 0; index < new_type->argument_types.size(); ++index)
                    {
                        if (auto* pending_template_arg = analyze_template_derivation(
                            function_define->template_type_name_list[tempindex],
                            function_define->template_type_name_list,
                            new_type->argument_types[index],
                            param->argument_types[index]))
                        {
                            ast::ast_type* template_value_type = new ast::ast_type(WO_PSTR(pending));
                            template_value_type->set_type(pending_template_arg);
                            template_value_type->copy_source_info(function_define);

                            arg_func_template_args[tempindex] = template_value_type;
                        }
                    }
            }

            if (std::find(arg_func_template_args.begin(), arg_func_template_args.end(), nullptr) != arg_func_template_args.end())
                return std::nullopt;

            // Auto judge here...
            if (update)
            {
                if (!(template_defines && template_args) || begin_template_scope(errreport, template_defines, *template_args))
                {
                    temporary_entry_scope_in_pass1(located_scope);
                    {
                        for (auto* template_arg : arg_func_template_args)
                            fully_update_type(template_arg, true);
                    }
                    temporary_leave_scope_in_pass1();

                    for (auto* template_arg : arg_func_template_args)
                        fully_update_type(template_arg, false);

                    if (template_defines && template_args)
                        end_template_scope();

                    auto* reificated = analyze_pass_template_reification(function_define, arg_func_template_args);

                    if (reificated != nullptr)
                    {
                        analyze_pass2(reificated);
                        return reificated;
                    }
                }
                return std::nullopt;
            }
            else
            {

                if (begin_template_scope(errreport, function_define, arg_func_template_args))
                {
                    temporary_entry_scope_in_pass1(located_scope);
                    {
                        fully_update_type(new_type, true);
                    }
                    temporary_leave_scope_in_pass1();

                    fully_update_type(new_type, false);

                    end_template_scope();
                }

                return new_type;
            }
        }

        // no need to judge, 
        return std::nullopt;
    }

    std::vector<ast::ast_type*> lang::judge_auto_type_in_funccall(
        ast::ast_value_funccall* funccall,
        lang_scope* located_scope,
        bool update,
        ast::ast_defines* template_defines,
        const std::vector<ast::ast_type*>* template_args)
    {
        using namespace ast;

        std::vector<ast_value*> args_might_be_nullptr_if_unpack;
        std::vector<ast::ast_type*> new_arguments_types_result;

        auto* arg = dynamic_cast<ast_value*>(funccall->arguments->children);
        while (arg)
        {
            if (auto* fake_unpack_value = dynamic_cast<ast_fakevalue_unpacked_args*>(arg))
            {
                if (fake_unpack_value->unpacked_pack->value_type->is_tuple())
                {
                    wo_assert(fake_unpack_value->expand_count >= 0);

                    size_t expand_count = std::min((size_t)fake_unpack_value->expand_count,
                        fake_unpack_value->unpacked_pack->value_type->template_arguments.size());

                    for (size_t i = 0; i < expand_count; ++i)
                    {
                        args_might_be_nullptr_if_unpack.push_back(nullptr);
                        new_arguments_types_result.push_back(nullptr);
                    }
                }
            }
            else
            {
                args_might_be_nullptr_if_unpack.push_back(arg);
                new_arguments_types_result.push_back(nullptr);
            }
            arg = dynamic_cast<ast_value*>(arg->sibling);
        }

        // If a function has been implized, this flag will be setting and function's
        //  argument list will be updated later.
        bool has_updated_arguments = false;

        for (size_t i = 0; i < args_might_be_nullptr_if_unpack.size() && i < funccall->called_func->value_type->argument_types.size(); ++i)
        {
            if (args_might_be_nullptr_if_unpack[i] == nullptr)
                continue;

            std::optional<judge_result_t> judge_result = judge_auto_type_of_funcdef_with_type(
                funccall, // Used for report error.
                located_scope,
                funccall->called_func->value_type->argument_types[i],
                args_might_be_nullptr_if_unpack[i], update, template_defines, template_args);

            if (judge_result.has_value())
            {
                if (auto** realized_func = std::get_if<ast::ast_value_function_define*>(&judge_result.value()))
                {
                    wo_assert((*realized_func)->is_template_define == false);

                    auto* pending_variable = dynamic_cast<ast::ast_value_variable*>(args_might_be_nullptr_if_unpack[i]);
                    if (pending_variable != nullptr)
                    {
                        pending_variable->value_type->set_type((*realized_func)->value_type);
                        pending_variable->symbol = (*realized_func)->this_reification_lang_symbol;
                        check_function_where_constraint(pending_variable, lang_anylizer, *realized_func);
                    }
                    else
                    {
                        wo_assert(dynamic_cast<ast::ast_value_function_define*>(args_might_be_nullptr_if_unpack[i]) != nullptr);
                        args_might_be_nullptr_if_unpack[i] = *realized_func;
                    }
                    has_updated_arguments = true;
                }
                else
                {
                    auto* updated_type = std::get<ast::ast_type*>(judge_result.value());
                    wo_assert(updated_type != nullptr);
                    new_arguments_types_result[i] = updated_type;
                }
            }
        }

        if (has_updated_arguments)
        {
            // Re-generate argument list for current function-call;
            funccall->arguments->remove_allnode();
            for (auto* arg : args_might_be_nullptr_if_unpack)
            {
                arg->sibling = nullptr;
                funccall->arguments->append_at_end(arg);
            }
        }

        return new_arguments_types_result;
    }

    void lang::analyze_pass2(grammar::ast_base* ast_node, bool type_degradation)
    {
        has_step_in_step2 = true;

        entry_pass ep1(in_pass2, true);

        wo_assert(ast_node);

        if (ast_node->completed_in_pass2)
            return;

        wo_assert(ast_node->completed_in_pass1 == true);
        ast_node->completed_in_pass2 = true;

        using namespace ast;

#define WO_TRY_BEGIN do{
#define WO_TRY_PASS(NODETYPE) if(pass2_##NODETYPE(dynamic_cast<NODETYPE*>(ast_node)))break;
#define WO_TRY_END }while(0)

        if (ast_value* a_value = dynamic_cast<ast_value*>(ast_node))
        {
            a_value->update_constant_value(lang_anylizer);

            if (ast_defines* a_defines = dynamic_cast<ast_defines*>(a_value);
                a_defines && a_defines->is_template_define)
            {
                for (auto& [typehashs, astnode] : a_defines->template_typehashs_reification_instance_list)
                {
                    analyze_pass2(astnode);
                }
            }
            else
            {
                // TODO: REPORT THE REAL UNKNOWN TYPE HERE, EXAMPLE:
                //       'void(ERRTYPE, int)' should report 'ERRTYPE', not 'void' or 'void(ERRTYPE, int)'
                if (a_value->value_type->may_need_update())
                {
                    // ready for update..
                    fully_update_type(a_value->value_type, false);

                    if (dynamic_cast<ast_value_function_define*>(a_value) == nullptr
                        // ast_value_make_struct_instance might need to auto judge types.
                        && dynamic_cast<ast_value_make_struct_instance*>(a_value) == nullptr
                        && a_value->value_type->has_custom())
                    {
                        lang_anylizer->lang_error(lexer::errorlevel::error, a_value, WO_ERR_UNKNOWN_TYPE
                            , a_value->value_type->get_type_name().c_str());

                        auto fuzz_symbol = find_symbol_in_this_scope(a_value->value_type, a_value->value_type->type_name, lang_symbol::symbol_type::typing | lang_symbol::symbol_type::type_alias, true);
                        if (fuzz_symbol)
                        {
                            auto fuzz_symbol_full_name = str_to_wstr(get_belong_namespace_path_with_lang_scope(fuzz_symbol));
                            if (!fuzz_symbol_full_name.empty())
                                fuzz_symbol_full_name += L"::";
                            fuzz_symbol_full_name += *fuzz_symbol->name;
                            lang_anylizer->lang_error(lexer::errorlevel::infom,
                                fuzz_symbol->type_informatiom,
                                WO_INFO_IS_THIS_ONE,
                                fuzz_symbol_full_name.c_str());
                        }
                    }
                }

                WO_TRY_BEGIN;
                //
                WO_TRY_PASS(ast_value_variable);
                WO_TRY_PASS(ast_value_funccall);
                WO_TRY_PASS(ast_value_function_define);
                WO_TRY_PASS(ast_value_unary);
                WO_TRY_PASS(ast_value_mutable);
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
                WO_TRY_PASS(ast_value_typeid);
                WO_TRY_END;

            }

            if (ast_defines* a_def = dynamic_cast<ast_defines*>(ast_node);
                a_def && a_def->is_template_define)
            {
                // Do nothing
            }
            else
            {
                a_value->update_constant_value(lang_anylizer);

                // some expr may set 'bool'/'char'..., it cannot used directly. update it.
                if (a_value->value_type->is_builtin_using_type())
                    fully_update_type(a_value->value_type, false);

                if (!a_value->value_type->is_pending())
                {
                    if (type_degradation && dynamic_cast<ast_value_mutable*>(a_value) == nullptr)
                    {
                        if (a_value->value_type->is_mutable())
                        {
                            a_value->can_be_assign = true;
                            a_value->value_type->set_is_mutable(false);
                        }
                    }
                }
            }
        }

        WO_TRY_BEGIN;
        /////////////////////////////////////////////////////////////////////////////////////////////////
        WO_TRY_PASS(ast_mapping_pair);
        WO_TRY_PASS(ast_return);
        WO_TRY_PASS(ast_sentence_block);
        WO_TRY_PASS(ast_if);
        WO_TRY_PASS(ast_while);
        WO_TRY_PASS(ast_forloop);
        WO_TRY_PASS(ast_foreach);
        WO_TRY_PASS(ast_varref_defines);
        WO_TRY_PASS(ast_union_make_option_ob_to_cr_and_ret);
        WO_TRY_PASS(ast_match);
        WO_TRY_PASS(ast_match_union_case);
        WO_TRY_PASS(ast_struct_member_define);
        WO_TRY_PASS(ast_where_constraint);

        WO_TRY_END;

        grammar::ast_base* child = ast_node->children;
        while (child)
        {
            analyze_pass2(child);
            child = child->sibling;
        }

        wo_assert(ast_node->completed_in_pass2);
    }
    void lang::clean_and_close_lang()
    {
        for (auto& created_symbols : lang_symbols)
            delete created_symbols;
        for (auto* created_scopes : lang_scopes_buffers)
        {
            delete created_scopes;
        }
        for (auto* created_temp_opnum : generated_opnum_list_for_clean)
            delete created_temp_opnum;
        for (auto generated_ast_node : generated_ast_nodes_buffers)
            delete generated_ast_node;

        lang_symbols.clear();
        lang_scopes_buffers.clear();
        generated_opnum_list_for_clean.clear();
        generated_ast_nodes_buffers.clear();
    }

    ast::ast_type* lang::analyze_template_derivation(
        wo_pstring_t temp_form,
        const std::vector<wo_pstring_t>& termplate_set,
        ast::ast_type* para, ast::ast_type* args)
    {
        // Must match all formal
        if (!para->is_like(args, termplate_set, &para, &args))
        {
            if (!para->is_complex()
                && !para->has_template()
                && para->scope_namespaces.empty()
                && !para->search_from_global_namespace
                && para->type_name == temp_form)
            {
                // do nothing..
            }
            else
                return nullptr;
        }
        if (para->is_mutable() && !args->is_mutable())
            return nullptr;

        if (para->is_complex() && args->is_complex())
        {
            if (auto* derivation_result = analyze_template_derivation(
                temp_form,
                termplate_set,
                para->complex_type,
                args->complex_type))
                return derivation_result;
        }

        if (para->is_struct() && para->using_type_name == nullptr
            && args->is_struct() && args->using_type_name == nullptr)
        {
            if (para->struct_member_index.size() == args->struct_member_index.size())
            {
                for (auto& [memname, memberinfo] : para->struct_member_index)
                {
                    auto fnd = args->struct_member_index.find(memname);
                    if (fnd == args->struct_member_index.end())
                        continue;

                    if (memberinfo.offset != fnd->second.offset)
                        continue;

                    if (auto* derivation_result = analyze_template_derivation(
                        temp_form,
                        termplate_set,
                        memberinfo.member_type,
                        fnd->second.member_type))
                        return derivation_result;
                }
            }
        }

        if (para->type_name == temp_form
            && para->scope_namespaces.empty()
            && !para->search_from_global_namespace)
        {
            ast::ast_type* picked_type = args;

            wo_assert(picked_type);
            if (!para->template_arguments.empty())
            {
                ast::ast_type* type_hkt = new ast::ast_type(WO_PSTR(pending));
                if (picked_type->using_type_name)
                    type_hkt->set_type(picked_type->using_type_name);
                else
                    type_hkt->set_type(picked_type);

                type_hkt->template_arguments.clear();
                return type_hkt;
            }

            if ((picked_type->is_mutable() && para->is_mutable()))
            {
                // mut T <<== mut InstanceT
                // Should get (immutable) InstanceT.

                ast::ast_type* immutable_type = new ast::ast_type(WO_PSTR(pending));
                immutable_type->set_type(picked_type);

                if (picked_type->is_mutable() && para->is_mutable())
                    immutable_type->set_is_mutable(false);

                return immutable_type;
            }

            return picked_type;
        }

        for (size_t index = 0;
            index < para->template_arguments.size()
            && index < args->template_arguments.size();
            index++)
        {
            if (auto* derivation_result = analyze_template_derivation(temp_form,
                termplate_set,
                para->template_arguments[index],
                args->template_arguments[index]))
                return derivation_result;
        }

        if (para->using_type_name && args->using_type_name)
        {
            if (auto* derivation_result =
                analyze_template_derivation(temp_form, termplate_set, para->using_type_name, args->using_type_name))
                return derivation_result;
        }

        for (size_t index = 0;
            index < para->argument_types.size()
            && index < args->argument_types.size();
            index++)
        {
            if (auto* derivation_result = analyze_template_derivation(temp_form,
                termplate_set,
                para->argument_types[index],
                args->argument_types[index]))
                return derivation_result;
        }

        return nullptr;
    }

    opnum::opnumbase& lang::get_useable_register_for_pure_value(bool must_release)
    {
        using namespace ast;
        using namespace opnum;
#define WO_NEW_OPNUM(...) (*generated_opnum_list_for_clean.emplace_back(new __VA_ARGS__))
        for (size_t i = 0; i < opnum::reg::T_REGISTER_COUNT + opnum::reg::R_REGISTER_COUNT; i++)
        {
            if (RegisterUsingState::FREE == assigned_tr_register_list[i])
            {
                assigned_tr_register_list[i] = must_release ? RegisterUsingState::BLOCKING : RegisterUsingState::NORMAL;
                return WO_NEW_OPNUM(reg(reg::t0 + (uint8_t)i));
            }
        }
        wo_error("cannot get a useable register..");
        return WO_NEW_OPNUM(reg(reg::cr));
    }
    void lang::_complete_using_register_for_pure_value(opnum::opnumbase& completed_reg)
    {
        using namespace ast;
        using namespace opnum;
        if (auto* reg_ptr = dynamic_cast<opnum::reg*>(&completed_reg);
            reg_ptr && reg_ptr->id >= 0 && reg_ptr->id < opnum::reg::T_REGISTER_COUNT + opnum::reg::R_REGISTER_COUNT)
        {
            assigned_tr_register_list[reg_ptr->id] = RegisterUsingState::FREE;
        }
    }
    void lang::_complete_using_all_register_for_pure_value()
    {
        for (size_t i = 0; i < opnum::reg::T_REGISTER_COUNT + opnum::reg::R_REGISTER_COUNT; i++)
        {
            if (assigned_tr_register_list[i] != RegisterUsingState::BLOCKING)
                assigned_tr_register_list[i] = RegisterUsingState::FREE;
        }
    }
    opnum::opnumbase& lang::complete_using_register(opnum::opnumbase& completed_reg)
    {
        _complete_using_register_for_pure_value(completed_reg);

        return completed_reg;
    }
    void lang::complete_using_all_register()
    {
        _complete_using_all_register_for_pure_value();
    }
    bool lang::is_reg(opnum::opnumbase& op_num)
    {
        using namespace opnum;
        if (auto* regist = dynamic_cast<reg*>(&op_num))
        {
            return true;
        }
        return false;
    }
    bool lang::is_cr_reg(opnum::opnumbase& op_num)
    {
        using namespace opnum;
        if (auto* regist = dynamic_cast<reg*>(&op_num))
        {
            if (regist->id == reg::cr)
                return true;
        }
        return false;
    }
    bool lang::is_non_ref_tem_reg(opnum::opnumbase& op_num)
    {
        using namespace opnum;
        if (auto* regist = dynamic_cast<reg*>(&op_num))
        {
            if (regist->id >= reg::t0 && regist->id <= reg::t15)
                return true;
        }
        return false;
    }
    bool lang::is_temp_reg(opnum::opnumbase& op_num)
    {
        using namespace opnum;
        if (auto* regist = dynamic_cast<reg*>(&op_num))
        {
            if (regist->id >= reg::t0 && regist->id <= reg::r15)
                return true;
        }
        return false;
    }
    opnum::opnumbase& lang::mov_value_to_cr(opnum::opnumbase& op_num, ir_compiler* compiler)
    {
        using namespace ast;
        using namespace opnum;
        if (_last_value_stored_to_cr)
            return op_num;

        if (auto* regist = dynamic_cast<reg*>(&op_num))
        {
            if (regist->id == reg::cr)
                return op_num;
        }
        compiler->mov(reg(reg::cr), op_num);
        return WO_NEW_OPNUM(reg(reg::cr));
    }

    opnum::opnumbase& lang::get_new_global_variable()
    {
        using namespace opnum;
        return WO_NEW_OPNUM(global((int32_t)global_symbol_index++));
    }
    std::variant<opnum::opnumbase*, int16_t> lang::get_opnum_by_symbol(grammar::ast_base* error_prud, lang_symbol* symb, ir_compiler* compiler, bool get_pure_value)
    {
        using namespace opnum;

        wo_assert(symb != nullptr);

        if (symb->is_constexpr_or_immut_no_closure_func())
            return &analyze_value(symb->variable_value, compiler, get_pure_value);

        if (symb->type == lang_symbol::symbol_type::variable)
        {
            if (symb->static_symbol)
            {
                if (!get_pure_value)
                    return &WO_NEW_OPNUM(global((int32_t)symb->global_index_in_lang));
                else
                {
                    auto& loaded_pure_glb_opnum = get_useable_register_for_pure_value();
                    compiler->mov(loaded_pure_glb_opnum, global((int32_t)symb->global_index_in_lang));
                    return &loaded_pure_glb_opnum;
                }
            }
            else
            {
                wo_integer_t stackoffset = 0;

                if (symb->is_captured_variable)
                    stackoffset = -2 - symb->captured_index;
                else
                    stackoffset = symb->stackvalue_index_in_funcs;
                if (!get_pure_value)
                {
                    if (stackoffset <= 64 && stackoffset >= -63)
                        return &WO_NEW_OPNUM(reg(reg::bp_offset(-(int8_t)stackoffset)));
                    else
                    {
                        // Fuck GCC!
                        return (int16_t)-stackoffset;
                    }
                }
                else
                {
                    if (stackoffset <= 64 && stackoffset >= -63)
                    {
                        auto& loaded_pure_glb_opnum = get_useable_register_for_pure_value();
                        compiler->mov(loaded_pure_glb_opnum, reg(reg::bp_offset(-(int8_t)stackoffset)));
                        return &loaded_pure_glb_opnum;
                    }
                    else
                    {
                        auto& lds_aim = get_useable_register_for_pure_value();
                        compiler->lds(lds_aim, imm(-(int16_t)stackoffset));
                        return &lds_aim;
                    }
                }
            }
        }
        else
            return &analyze_value(symb->get_funcdef(), compiler, get_pure_value);
    }

    opnum::opnumbase& lang::analyze_value(ast::ast_value* value, ir_compiler* compiler, bool get_pure_value)
    {
        auto_cancel_value_store_to_cr last_value_stored_to_cr_flag(_last_value_stored_to_cr);
        auto_cancel_value_store_to_cr last_value_stored_to_stack_flag(_last_value_from_stack);
        using namespace ast;
        using namespace opnum;
        if (value->is_constant)
        {
            if (ast_value_trib_expr* a_value_trib_expr = dynamic_cast<ast_value_trib_expr*>(value))
                // Only generate expr if const-expr is a function call
                analyze_value(a_value_trib_expr->judge_expr, compiler, false);

            auto const_value = value->get_constant_value();
            switch (const_value.type)
            {
            case value::valuetype::bool_type:
                if (!get_pure_value)
                    return WO_NEW_OPNUM(imm((bool)(const_value.integer != 0)));
                else
                {
                    auto& treg = get_useable_register_for_pure_value();
                    compiler->mov(treg, imm((bool)(const_value.integer != 0)));
                    return treg;
                }
            case value::valuetype::integer_type:
                if (!get_pure_value)
                    return WO_NEW_OPNUM(imm(const_value.integer));
                else
                {
                    auto& treg = get_useable_register_for_pure_value();
                    compiler->mov(treg, imm(const_value.integer));
                    return treg;
                }
            case value::valuetype::real_type:
                if (!get_pure_value)
                    return WO_NEW_OPNUM(imm(const_value.real));
                else
                {
                    auto& treg = get_useable_register_for_pure_value();
                    compiler->mov(treg, imm(const_value.real));
                    return treg;
                }
            case value::valuetype::handle_type:
                if (!get_pure_value)
                    return WO_NEW_OPNUM(imm((void*)const_value.handle));
                else
                {
                    auto& treg = get_useable_register_for_pure_value();
                    compiler->mov(treg, imm((void*)const_value.handle));
                    return treg;
                }
            case value::valuetype::string_type:
                if (!get_pure_value)
                    return WO_NEW_OPNUM(imm(const_value.string->c_str()));
                else
                {
                    auto& treg = get_useable_register_for_pure_value();
                    compiler->mov(treg, imm(const_value.string->c_str()));
                    return treg;
                }
            case value::valuetype::invalid:  // for nil
            case value::valuetype::array_type:  // for nil
            case value::valuetype::dict_type:  // for nil
            case value::valuetype::gchandle_type:  // for nil
                if (!get_pure_value)
                    return WO_NEW_OPNUM(reg(reg::ni));
                else
                {
                    auto& treg = get_useable_register_for_pure_value();
                    compiler->mov(treg, reg(reg::ni));
                    return treg;
                }
            default:
                wo_error("error constant type..");
                break;
            }
        }
        else if (auto* a_value_function_define = dynamic_cast<ast_value_function_define*>(value))
        {
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
                            _last_stack_offset_to_write = std::get<int16_t>(opnum_or_stackoffset);
                            wo_assert(get_pure_value == false);

                            last_value_stored_to_stack_flag.set_true();

                            auto& usable_stack = get_useable_register_for_pure_value();
                            compiler->lds(usable_stack, imm(_last_stack_offset_to_write));
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
        else if (auto* a_value_variable = dynamic_cast<ast_value_variable*>(value))
        {
            // ATTENTION: HERE JUST VALUE , NOT JUDGE FUNCTION
            auto symb = a_value_variable->symbol;

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
                _last_stack_offset_to_write = std::get<int16_t>(opnum_or_stackoffset);
                wo_assert(get_pure_value == false);

                last_value_stored_to_stack_flag.set_true();

                auto& usable_stack = get_useable_register_for_pure_value();
                compiler->lds(usable_stack, imm(_last_stack_offset_to_write));

                return usable_stack;
            }
        }
        else
        {
            compiler->pdb_info->generate_debug_info_at_astnode(value, compiler);

            if (dynamic_cast<ast_value_literal*>(value))
            {
                wo_error("ast_value_literal should be 'constant'..");
            }
            else if (auto* a_value_binary = dynamic_cast<ast_value_binary*>(value))
            {
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
                        lang_anylizer->lang_error(lexer::errorlevel::error, a_value_binary, WO_ERR_CANNOT_CALC_WITH_L_AND_R,
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
                        lang_anylizer->lang_error(lexer::errorlevel::error, a_value_binary, WO_ERR_CANNOT_CALC_WITH_L_AND_R,
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
                        lang_anylizer->lang_error(lexer::errorlevel::error, a_value_binary, WO_ERR_CANNOT_CALC_WITH_L_AND_R,
                            a_value_binary->left->value_type->get_type_name(false).c_str(),
                            a_value_binary->right->value_type->get_type_name(false).c_str());
                        break;
                    }

                    break;
                case lex_type::l_div:
                    switch (optype)
                    {
                    case wo::value::valuetype::integer_type:
                        compiler->divi(beoped_left_opnum, op_right_opnum); break;
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
                        compiler->modi(beoped_left_opnum, op_right_opnum); break;
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
            else if (auto* a_value_assign = dynamic_cast<ast_value_assign*>(value))
            {
                ast_value_index* a_value_index = dynamic_cast<ast_value_index*>(a_value_assign->left);
                opnumbase* _store_value = nullptr;
                bool beassigned_value_from_stack = false;
                int16_t beassigned_value_stack_place = 0;

                if (!(a_value_index != nullptr && a_value_assign->operate == +lex_type::l_assign))
                {
                    // if mixed type, do opx
                    bool same_type = a_value_assign->left->value_type->accept_type(a_value_assign->right->value_type, false);
                    value::valuetype optype = value::valuetype::invalid;
                    if (same_type)
                        optype = a_value_assign->left->value_type->value_type;

                    size_t revert_pos = compiler->get_now_ip();

                    auto* beoped_left_opnum_ptr = &analyze_value(a_value_assign->left, compiler, a_value_index != nullptr);
                    beassigned_value_from_stack = _last_value_from_stack;
                    beassigned_value_stack_place = _last_stack_offset_to_write;

                    // Assign not need for this variable, revert it.
                    if (a_value_assign->operate == +lex_type::l_assign
                        || a_value_assign->operate == +lex_type::l_value_assign)
                    {
                        complete_using_register(*beoped_left_opnum_ptr);
                        compiler->revert_code_to(revert_pos);
                    }

                    auto* op_right_opnum_ptr = &analyze_value(a_value_assign->right, compiler);

                    if (is_cr_reg(*beoped_left_opnum_ptr)
                        //      FUNC CALL                           A + B ...                       A[X] (.E.G)
                        && (is_cr_reg(*op_right_opnum_ptr) || is_temp_reg(*op_right_opnum_ptr) || _last_value_stored_to_cr))
                    {
                        // if assigned value is an index, it's result will be placed to non-cr place. so cannot be here.
                        wo_assert(a_value_index == nullptr);

                        // If beassigned_value_from_stack, it will generate a template register. so cannot be here.
                        wo_assert(beassigned_value_from_stack == false);

                        complete_using_register(*beoped_left_opnum_ptr);
                        complete_using_register(*op_right_opnum_ptr);
                        compiler->revert_code_to(revert_pos);
                        op_right_opnum_ptr = &analyze_value(a_value_assign->right, compiler, true);
                        beoped_left_opnum_ptr = &analyze_value(a_value_assign->left, compiler);
                    }

                    auto& beoped_left_opnum = *beoped_left_opnum_ptr;
                    auto& op_right_opnum = *op_right_opnum_ptr;

                    switch (a_value_assign->operate)
                    {
                    case lex_type::l_assign:
                    case lex_type::l_value_assign:
                        wo_assert(a_value_assign->left->value_type->accept_type(a_value_assign->right->value_type, false));
                        if (beassigned_value_from_stack)
                            compiler->sts(op_right_opnum, imm(_last_stack_offset_to_write));
                        else
                            compiler->mov(beoped_left_opnum, op_right_opnum);
                        break;
                    case lex_type::l_add_assign:
                    case lex_type::l_value_add_assign:
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
                            compiler->subi(beoped_left_opnum, op_right_opnum); break;
                        case wo::value::valuetype::real_type:
                            compiler->subr(beoped_left_opnum, op_right_opnum); break;
                        case wo::value::valuetype::handle_type:
                            compiler->subh(beoped_left_opnum, op_right_opnum); break;
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
                            compiler->muli(beoped_left_opnum, op_right_opnum); break;
                        case wo::value::valuetype::real_type:
                            compiler->mulr(beoped_left_opnum, op_right_opnum); break;
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
                            compiler->divi(beoped_left_opnum, op_right_opnum); break;
                        case wo::value::valuetype::real_type:
                            compiler->divr(beoped_left_opnum, op_right_opnum); break;
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
                            compiler->modi(beoped_left_opnum, op_right_opnum); break;
                        case wo::value::valuetype::real_type:
                            compiler->modr(beoped_left_opnum, op_right_opnum); break;
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

                    _store_value = &beoped_left_opnum;

                    if (beassigned_value_from_stack)
                    {
                        if (a_value_assign->operate == +lex_type::l_assign
                            || a_value_assign->operate == +lex_type::l_value_assign)
                            _store_value = &op_right_opnum;
                        else
                        {
                            compiler->sts(op_right_opnum, imm(_last_stack_offset_to_write));
                            complete_using_register(op_right_opnum);
                        }
                    }
                    else
                        complete_using_register(op_right_opnum);
                }
                else
                {
                    wo_assert(beassigned_value_from_stack == false);
                    wo_assert(a_value_index != nullptr && a_value_assign->operate == +lex_type::l_assign);
                    _store_value = &analyze_value(a_value_assign->right, compiler);

                    if (is_cr_reg(*_store_value))
                    {
                        auto* _store_value_place = &get_useable_register_for_pure_value();
                        compiler->mov(*_store_value_place, *_store_value);
                        _store_value = _store_value_place;
                    }
                }

                wo_assert(_store_value != nullptr);

                if (a_value_index != nullptr)
                {
                    wo_assert(beassigned_value_from_stack == false);

                    if (a_value_index->from->value_type->is_struct()
                        || a_value_index->from->value_type->is_tuple())
                    {
                        auto* _from_value = &analyze_value(a_value_index->from, compiler);
                        compiler->sidstruct(*_from_value, *_store_value, a_value_index->struct_offset);
                    }
                    else
                    {
                        size_t revert_pos = compiler->get_now_ip();

                        auto* _from_value = &analyze_value(a_value_index->from, compiler);
                        auto* _index_value = &analyze_value(a_value_index->index, compiler);

                        if (is_cr_reg(*_from_value)
                            && (is_cr_reg(*_index_value) || is_temp_reg(*_index_value) || _last_value_stored_to_cr))
                        {
                            complete_using_register(*_from_value);
                            complete_using_register(*_from_value);
                            compiler->revert_code_to(revert_pos);
                            _index_value = &analyze_value(a_value_index->index, compiler, true);
                            _from_value = &analyze_value(a_value_index->from, compiler);
                        }
                        auto& from_value = *_from_value;
                        auto& index_value = *_index_value;

                        auto* _final_store_value = _store_value;
                        if (!is_reg(*_store_value) || is_temp_reg(*_store_value))
                        {
                            // Use pm reg here because here has no other command to generate.
                            _final_store_value = &WO_NEW_OPNUM(reg(reg::tp));
                            compiler->mov(*_final_store_value, *_store_value);
                        }
                        // Do not generate any other command to make sure reg::tp usable!

                        if (a_value_index->from->value_type->is_array() || a_value_index->from->value_type->is_vec())
                            compiler->sidarr(from_value, index_value, *dynamic_cast<const opnum::reg*>(_final_store_value));
                        else if (a_value_index->from->value_type->is_dict())
                            compiler->siddict(from_value, index_value, *dynamic_cast<const opnum::reg*>(_final_store_value));
                        else if (a_value_index->from->value_type->is_map())
                            compiler->sidmap(from_value, index_value, *dynamic_cast<const opnum::reg*>(_final_store_value));
                        else
                            wo_error("Unknown unindex & storable type.");
                    }
                }

                if (get_pure_value)
                {
                    auto& treg = get_useable_register_for_pure_value();
                    compiler->mov(treg, complete_using_register(*_store_value));
                    return treg;
                }
                else
                    return *_store_value;
            }
            else if (auto* a_value_mutable_or_pure = dynamic_cast<ast_value_mutable*>(value))
            {
                return analyze_value(a_value_mutable_or_pure->val, compiler, get_pure_value);
            }
            else if (auto* a_value_type_cast = dynamic_cast<ast_value_type_cast*>(value))
            {
                if (a_value_type_cast->value_type->is_dynamic()
                    || a_value_type_cast->value_type->is_void()
                    || a_value_type_cast->value_type->accept_type(a_value_type_cast->_be_cast_value_node->value_type, true)
                    || a_value_type_cast->value_type->is_complex())
                    // no cast, just as origin value
                    return analyze_value(a_value_type_cast->_be_cast_value_node, compiler, get_pure_value);

                auto& treg = get_useable_register_for_pure_value();
                compiler->movcast(treg,
                    complete_using_register(analyze_value(a_value_type_cast->_be_cast_value_node, compiler)),
                    a_value_type_cast->value_type->value_type);
                return treg;
            }
            else if (ast_value_type_judge* a_value_type_judge = dynamic_cast<ast_value_type_judge*>(value))
            {
                auto& result = analyze_value(a_value_type_judge->_be_cast_value_node, compiler, get_pure_value);

                if (a_value_type_judge->value_type->accept_type(a_value_type_judge->_be_cast_value_node->value_type, false))
                    return result;
                else if (a_value_type_judge->_be_cast_value_node->value_type->is_dynamic())
                {
                    if (a_value_type_judge->value_type->is_pure_base_type()
                        || a_value_type_judge->value_type->is_void()
                        || a_value_type_judge->value_type->is_nothing())
                        lang_anylizer->lang_error(lexer::errorlevel::error, a_value_type_judge,
                            WO_ERR_CANNOT_TEST_COMPLEX_TYPE, a_value_type_judge->value_type->get_type_name(false).c_str());

                    if (!a_value_type_judge->value_type->is_dynamic())
                    {
                        wo_test(!a_value_type_judge->value_type->is_pending());
                        compiler->typeas(result, a_value_type_judge->value_type->value_type);

                        return result;
                    }
                }

                lang_anylizer->lang_error(lexer::errorlevel::error, a_value_type_judge, WO_ERR_CANNOT_AS_TYPE,
                    a_value_type_judge->_be_cast_value_node->value_type->get_type_name(false).c_str(),
                    a_value_type_judge->value_type->get_type_name(false).c_str());

                return result;
            }
            else if (ast_value_type_check* a_value_type_check = dynamic_cast<ast_value_type_check*>(value))
            {
                if (a_value_type_check->aim_type->accept_type(a_value_type_check->_be_check_value_node->value_type, false))
                    return WO_NEW_OPNUM(imm(1));
                if (a_value_type_check->_be_check_value_node->value_type->is_dynamic())
                {
                    if (a_value_type_check->aim_type->is_pure_base_type()
                        || a_value_type_check->aim_type->is_void()
                        || a_value_type_check->aim_type->is_nothing())
                        lang_anylizer->lang_error(lexer::errorlevel::error, a_value_type_check,
                            WO_ERR_CANNOT_TEST_COMPLEX_TYPE, a_value_type_check->aim_type->get_type_name(false).c_str());

                    if (!a_value_type_check->aim_type->is_dynamic())
                    {
                        // is dynamic do check..
                        auto& result = analyze_value(a_value_type_check->_be_check_value_node, compiler);

                        wo_assert(!a_value_type_check->aim_type->is_pending());
                        compiler->typeis(complete_using_register(result), a_value_type_check->aim_type->value_type);

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

                return WO_NEW_OPNUM(imm(0));

            }
            else if (auto* a_value_funccall = dynamic_cast<ast_value_funccall*>(value))
            {
                if (now_function_in_final_anylize && now_function_in_final_anylize->value_type->is_variadic_function_type)
                    compiler->psh(reg(reg::tc));

                std::vector<ast_value* >arg_list;
                auto arg = a_value_funccall->arguments->children;

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
                                a_fakevalue_unpacked_args->expand_count = unpacking_tuple_type->template_arguments.size();
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
                            compiler->ext_unpackargs(packing, a_fakevalue_unpacked_args->expand_count);
                        }
                        else
                        {
                            if (a_fakevalue_unpacked_args->expand_count <= 0)
                                compiler->mov(reg(reg::tc), imm(arg_list.size() + extern_unpack_arg_count - 1));
                            else
                                extern_unpack_arg_count += a_fakevalue_unpacked_args->expand_count - 1;

                            compiler->ext_unpackargs(packing,
                                a_fakevalue_unpacked_args->expand_count);
                        }
                        complete_using_register(packing);
                    }
                    else
                    {
                        compiler->psh(complete_using_register(analyze_value(argv, compiler)));
                    }
                }

                auto* called_func_aim = &analyze_value(a_value_funccall->called_func, compiler);

                ast_value_variable* funcvariable = dynamic_cast<ast_value_variable*>(a_value_funccall->called_func);
                ast_value_function_define* funcdef = dynamic_cast<ast_value_function_define*>(a_value_funccall->called_func);

                if (funcvariable != nullptr)
                {
                    if (funcvariable->symbol != nullptr && funcvariable->symbol->type == lang_symbol::symbol_type::function)
                        funcdef = funcvariable->symbol->get_funcdef();
                }

                bool need_using_tc = !dynamic_cast<opnum::immbase*>(called_func_aim)
                    || a_value_funccall->called_func->value_type->is_variadic_function_type
                    || (funcdef != nullptr && funcdef->is_different_arg_count_in_same_extern_symbol);

                if (!full_unpack_arguments && need_using_tc)
                    compiler->mov(reg(reg::tc), imm(arg_list.size() + extern_unpack_arg_count));

                opnumbase* reg_for_current_funccall_argc = nullptr;
                if (full_unpack_arguments)
                {
                    reg_for_current_funccall_argc = &get_useable_register_for_pure_value();
                    compiler->mov(*reg_for_current_funccall_argc, reg(reg::tc));
                }

                if (funcdef != nullptr
                    && funcdef->externed_func_info != nullptr
                    && funcdef->externed_func_info->leaving_call == false)
                    compiler->callfast((void*)funcdef->externed_func_info->externed_func);
                else
                    compiler->call(complete_using_register(*called_func_aim));

                last_value_stored_to_cr_flag.set_true();

                opnum::opnumbase* result_storage_place = nullptr;

                if (full_unpack_arguments)
                {
                    last_value_stored_to_cr_flag.set_false();

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
            else if (auto* a_value_logical_binary = dynamic_cast<ast_value_logical_binary*>(value))
            {
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
                if (a_value_logical_binary->operate == +lex_type::l_equal || a_value_logical_binary->operate == +lex_type::l_not_equal)
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
                if (a_value_logical_binary->operate == +lex_type::l_larg
                    || a_value_logical_binary->operate == +lex_type::l_larg_or_equal
                    || a_value_logical_binary->operate == +lex_type::l_less
                    || a_value_logical_binary->operate == +lex_type::l_less_or_equal)
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

                if (a_value_logical_binary->operate == +lex_type::l_lor ||
                    a_value_logical_binary->operate == +lex_type::l_land)
                {
                    if (!a_value_logical_binary->left->value_type->is_bool())
                        lang_anylizer->lang_error(lexer::errorlevel::error, a_value_logical_binary->left, WO_ERR_SHOULD_BE_TYPE_BUT_GET_UNEXCEPTED_TYPE, L"bool",
                            a_value_logical_binary->left->value_type->get_type_name(false).c_str());
                    if (!a_value_logical_binary->right->value_type->is_bool())
                        lang_anylizer->lang_error(lexer::errorlevel::error, a_value_logical_binary->right, WO_ERR_SHOULD_BE_TYPE_BUT_GET_UNEXCEPTED_TYPE, L"bool",
                            a_value_logical_binary->right->value_type->get_type_name(false).c_str());
                }

                size_t revert_pos = compiler->get_now_ip();

                auto* _beoped_left_opnum = &analyze_value(a_value_logical_binary->left, compiler);
                auto* _op_right_opnum = &analyze_value(a_value_logical_binary->right, compiler);

                if ((is_cr_reg(*_op_right_opnum) || is_temp_reg(*_op_right_opnum) || _last_value_stored_to_cr) &&
                    (a_value_logical_binary->operate == +lex_type::l_lor ||
                        a_value_logical_binary->operate == +lex_type::l_land))
                {
                    // Need make short cut
                    complete_using_register(*_beoped_left_opnum);
                    complete_using_register(*_op_right_opnum);
                    compiler->revert_code_to(revert_pos);

                    auto logic_short_cut_label = "logic_short_cut_" + compiler->get_unique_tag_based_command_ip();

                    mov_value_to_cr(analyze_value(a_value_logical_binary->left, compiler), compiler);

                    if (a_value_logical_binary->operate == +lex_type::l_lor)
                        compiler->jt(tag(logic_short_cut_label));
                    else  if (a_value_logical_binary->operate == +lex_type::l_land)
                        compiler->jf(tag(logic_short_cut_label));
                    else
                        wo_error("Unknown operator.");

                    mov_value_to_cr(analyze_value(a_value_logical_binary->right, compiler), compiler);

                    compiler->tag(logic_short_cut_label);

                    return WO_NEW_OPNUM(reg(reg::cr));
                }

                if (is_cr_reg(*_beoped_left_opnum)
                    && (is_cr_reg(*_op_right_opnum) || is_temp_reg(*_op_right_opnum) || _last_value_stored_to_cr))
                {
                    complete_using_register(*_beoped_left_opnum);
                    complete_using_register(*_op_right_opnum);
                    compiler->revert_code_to(revert_pos);
                    _op_right_opnum = &analyze_value(a_value_logical_binary->right, compiler, true);
                    _beoped_left_opnum = &analyze_value(a_value_logical_binary->left, compiler);
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

                if (!get_pure_value)
                    return WO_NEW_OPNUM(reg(reg::cr));
                else
                {
                    auto& result = get_useable_register_for_pure_value();
                    compiler->mov(result, reg(reg::cr));
                    return result;
                }
            }
            else if (auto* a_value_array = dynamic_cast<ast_value_array*>(value))
            {
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
            else if (auto* a_value_mapping = dynamic_cast<ast_value_mapping*>(value))
            {
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
            else if (auto* a_value_index = dynamic_cast<ast_value_index*>(value))
            {
                if (a_value_index->from->value_type->is_struct() || a_value_index->from->value_type->is_tuple())
                {
                    wo_assert(a_value_index->struct_offset != 0xFFFF);
                    auto* _beoped_left_opnum = &analyze_value(a_value_index->from, compiler);

                    compiler->idstruct(reg(reg::cr), complete_using_register(*_beoped_left_opnum), a_value_index->struct_offset);
                }
                else
                {
                    size_t revert_pos = compiler->get_now_ip();

                    auto* _beoped_left_opnum = &analyze_value(a_value_index->from, compiler);
                    auto* _op_right_opnum = &analyze_value(a_value_index->index, compiler);

                    if (is_cr_reg(*_beoped_left_opnum)
                        && (is_cr_reg(*_op_right_opnum) || is_temp_reg(*_op_right_opnum) || _last_value_stored_to_cr))
                    {
                        complete_using_register(*_beoped_left_opnum);
                        complete_using_register(*_beoped_left_opnum);
                        compiler->revert_code_to(revert_pos);
                        _op_right_opnum = &analyze_value(a_value_index->index, compiler, true);
                        _beoped_left_opnum = &analyze_value(a_value_index->from, compiler);
                    }
                    auto& beoped_left_opnum = *_beoped_left_opnum;
                    auto& op_right_opnum = *_op_right_opnum;

                    last_value_stored_to_cr_flag.set_true();

                    if (a_value_index->from->value_type->is_array() || a_value_index->from->value_type->is_vec())
                        compiler->idarr(beoped_left_opnum, op_right_opnum);
                    else if (a_value_index->from->value_type->is_dict() || a_value_index->from->value_type->is_map())
                    {
                        compiler->iddict(beoped_left_opnum, op_right_opnum);
                    }
                    else if (a_value_index->from->value_type->is_string())
                        compiler->idstr(beoped_left_opnum, op_right_opnum);
                    else
                        wo_error("Unknown index operation.");

                    complete_using_register(beoped_left_opnum);
                    complete_using_register(op_right_opnum);
                }


                if (!get_pure_value)
                    return WO_NEW_OPNUM(reg(reg::cr));
                else
                {
                    auto& result = get_useable_register_for_pure_value();
                    compiler->mov(result, reg(reg::cr));
                    return result;
                }

            }
            else if (auto* a_value_packed_variadic_args = dynamic_cast<ast_value_packed_variadic_args*>(value))
            {
                if (!now_function_in_final_anylize
                    || !now_function_in_final_anylize->value_type->is_variadic_function_type)
                {
                    lang_anylizer->lang_error(lexer::errorlevel::error, a_value_packed_variadic_args, WO_ERR_USING_VARIADIC_IN_NON_VRIDIC_FUNC);
                    return WO_NEW_OPNUM(reg(reg::cr));
                }
                else
                {
                    auto& packed = get_useable_register_for_pure_value();

                    compiler->ext_packargs(packed, imm(
                        now_function_in_final_anylize->value_type->argument_types.size()
                    ), (uint16_t)now_function_in_final_anylize->capture_variables.size());
                    return packed;
                }
            }
            else if (auto* a_value_indexed_variadic_args = dynamic_cast<ast_value_indexed_variadic_args*>(value))
            {
                if (!now_function_in_final_anylize
                    || !now_function_in_final_anylize->value_type->is_variadic_function_type)
                {
                    lang_anylizer->lang_error(lexer::errorlevel::error, a_value_packed_variadic_args, WO_ERR_USING_VARIADIC_IN_NON_VRIDIC_FUNC);
                    return WO_NEW_OPNUM(reg(reg::cr));
                }

                auto capture_count = (uint16_t)now_function_in_final_anylize->capture_variables.size();
                auto function_arg_count = now_function_in_final_anylize->value_type->argument_types.size();

                if (a_value_indexed_variadic_args->argindex->is_constant)
                {
                    auto _cv = a_value_indexed_variadic_args->argindex->get_constant_value();
                    if (_cv.integer + capture_count + function_arg_count <= 63 - 2)
                    {
                        if (!get_pure_value)
                            return WO_NEW_OPNUM(reg(reg::bp_offset((int8_t)(_cv.integer + capture_count + 2
                                + function_arg_count))));
                        else
                        {
                            auto& result = get_useable_register_for_pure_value();

                            last_value_stored_to_cr_flag.set_true();
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
                    compiler->addi(index, imm(2
                        + capture_count + function_arg_count));
                    complete_using_register(index);
                    auto& result = get_useable_register_for_pure_value();
                    compiler->lds(result, index);
                    return result;
                }
            }
            else if (auto* a_fakevalue_unpacked_args = dynamic_cast<ast_fakevalue_unpacked_args*>(value))
            {
                lang_anylizer->lang_error(lexer::errorlevel::error, a_fakevalue_unpacked_args, WO_ERR_UNPACK_ARGS_OUT_OF_FUNC_CALL);
                return WO_NEW_OPNUM(reg(reg::cr));
            }
            else if (auto* a_value_unary = dynamic_cast<ast_value_unary*>(value))
            {
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
                        lang_anylizer->lang_error(lexer::errorlevel::error, a_value_unary, WO_ERR_TYPE_CANNOT_NEGATIVE, a_value_unary->val->value_type->get_type_name().c_str());
                    break;
                default:
                    wo_error("Do not support this operator..");
                    break;
                }

                if (!get_pure_value)
                    return WO_NEW_OPNUM(reg(reg::cr));
                else
                {
                    auto& result = get_useable_register_for_pure_value();
                    compiler->mov(result, reg(reg::cr));
                    return result;
                }
            }
            else if (ast_value_takeplace* a_value_takeplace = dynamic_cast<ast_value_takeplace*>(value))
            {
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
            else if (ast_value_make_struct_instance* a_value_make_struct_instance = dynamic_cast<ast_value_make_struct_instance*>(value))
            {
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
            else if (ast_value_make_tuple_instance* a_value_make_tuple_instance = dynamic_cast<ast_value_make_tuple_instance*>(value))
            {
                auto* tuple_elems = a_value_make_tuple_instance->tuple_member_vals->children;

                uint16_t tuple_count = 0;
                while (tuple_elems)
                {
                    ast_value* val = dynamic_cast<ast_value*>(tuple_elems);
                    wo_assert(val);

                    ++tuple_count;
                    compiler->psh(complete_using_register(analyze_value(val, compiler)));

                    tuple_elems = tuple_elems->sibling;
                }

                auto& result = get_useable_register_for_pure_value();
                compiler->mkstruct(result, tuple_count);

                return result;
            }
            else if (ast_value_trib_expr* a_value_trib_expr = dynamic_cast<ast_value_trib_expr*>(value))
            {
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

                    return WO_NEW_OPNUM(reg(reg::cr));
                }
            }
            else if (ast_value_typeid* a_value_typeid = dynamic_cast<ast_value_typeid*>(value))
            {
                wo_error("ast_value_typeid must be constant or abort in pass2.");
            }
            else
            {
                wo_error("unknown value type..");
            }
        }
        wo_error("run to err place..");
        static opnum::opnumbase err;
        return err;
#undef WO_NEW_OPNUM
    }
    opnum::opnumbase& lang::auto_analyze_value(ast::ast_value* value, ir_compiler* compiler, bool get_pure_value, bool force_value)
    {
        auto& result = analyze_value(value, compiler, get_pure_value);
        complete_using_all_register();

        return result;
    }

    void lang::real_analyze_finalize(grammar::ast_base* ast_node, ir_compiler* compiler)
    {
        wo_assert(ast_node->completed_in_pass2);

        compiler->pdb_info->generate_debug_info_at_astnode(ast_node, compiler);

        using namespace ast;
        using namespace opnum;

        if (auto* a_varref_defines = dynamic_cast<ast_varref_defines*>(ast_node))
        {
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
                    compiler->mov(static_inited_flag, imm(1));
                }

                analyze_pattern_in_finalize(varref_define.pattern, varref_define.init_val, false, compiler);

                if (need_init_check)
                    compiler->tag(init_static_flag_check_tag);
            }
        }
        else if (auto* a_list = dynamic_cast<ast_list*>(ast_node))
        {
            auto* child = a_list->children;
            while (child)
            {
                real_analyze_finalize(child, compiler);
                child = child->sibling;
            }
        }
        else if (auto* a_if = dynamic_cast<ast_if*>(ast_node))
        {
            if (!a_if->judgement_value->value_type->is_bool())
                lang_anylizer->lang_error(lexer::errorlevel::error, a_if->judgement_value, WO_ERR_SHOULD_BE_TYPE_BUT_GET_UNEXCEPTED_TYPE,
                    L"bool", a_if->judgement_value->value_type->get_type_name(false).c_str());

            if (a_if->judgement_value->is_constant)
            {
                auto_analyze_value(a_if->judgement_value, compiler, false, true);

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
        else if (auto* a_while = dynamic_cast<ast_while*>(ast_node))
        {
            if (a_while->judgement_value != nullptr
                && !a_while->judgement_value->value_type->is_bool())
                lang_anylizer->lang_error(lexer::errorlevel::error, a_while->judgement_value, WO_ERR_SHOULD_BE_TYPE_BUT_GET_UNEXCEPTED_TYPE, L"bool",
                    a_while->judgement_value->value_type->get_type_name(false).c_str());

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
        else if (ast_forloop* a_forloop = dynamic_cast<ast_forloop*>(ast_node))
        {
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
                if (!a_forloop->judgement_expr->value_type->is_bool())
                    lang_anylizer->lang_error(lexer::errorlevel::error, a_forloop->judgement_expr, WO_ERR_SHOULD_BE_TYPE_BUT_GET_UNEXCEPTED_TYPE, L"bool",
                        a_forloop->judgement_expr->value_type->get_type_name(false).c_str());

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
        else if (auto* a_value = dynamic_cast<ast_value*>(ast_node))
        {
            if (auto* a_val_funcdef = dynamic_cast<ast_value_function_define*>(ast_node);
                a_val_funcdef == nullptr || a_val_funcdef->function_name == nullptr)
            {
                // Woolang 1.10.2: The value is not void type, cannot be a sentence.
                if (!a_value->value_type->is_void() && !a_value->value_type->is_nothing())
                    lang_anylizer->lang_error(lexer::errorlevel::error, a_value, WO_ERR_NOT_ALLOW_IGNORE_VALUE,
                        a_value->value_type->get_type_name(false).c_str());
            }
            auto_analyze_value(a_value, compiler);
        }
        else if (auto* a_sentence_block = dynamic_cast<ast_sentence_block*>(ast_node))
        {
            real_analyze_finalize(a_sentence_block->sentence_list, compiler);
        }
        else if (auto* a_return = dynamic_cast<ast_return*>(ast_node))
        {
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
        else if (auto* a_namespace = dynamic_cast<ast_namespace*>(ast_node))
        {
            real_analyze_finalize(a_namespace->in_scope_sentence, compiler);
        }
        else if (dynamic_cast<ast_using_namespace*>(ast_node))
        {
            // do nothing
        }
        else if (dynamic_cast<ast_using_type_as*>(ast_node))
        {
            // do nothing
        }
        else if (dynamic_cast<ast_nop*>(ast_node))
        {
            compiler->nop();
        }
        else if (ast_foreach* a_foreach = dynamic_cast<ast_foreach*>(ast_node))
        {
            real_analyze_finalize(a_foreach->used_iter_define, compiler);
            real_analyze_finalize(a_foreach->loop_sentences, compiler);
        }
        else if (ast_break* a_break = dynamic_cast<ast_break*>(ast_node))
        {
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
        else if (ast_continue* a_continue = dynamic_cast<ast_continue*>(ast_node))
        {
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
        else if (ast_union_make_option_ob_to_cr_and_ret* a_union_make_option_ob_to_cr_and_ret
            = dynamic_cast<ast_union_make_option_ob_to_cr_and_ret*>(ast_node))
        {
            if (a_union_make_option_ob_to_cr_and_ret->argument_may_nil)
                compiler->mkunion(reg(reg::cr), auto_analyze_value(a_union_make_option_ob_to_cr_and_ret->argument_may_nil, compiler),
                    a_union_make_option_ob_to_cr_and_ret->id);
            else
                compiler->mkunion(reg(reg::cr), reg(reg::ni), a_union_make_option_ob_to_cr_and_ret->id);

            // TODO: ast_union_make_option_ob_to_cr_and_ret not exist in closure function, so we just ret here.
            //       need check!
            compiler->ret();
        }
        else if (ast_match* a_match = dynamic_cast<ast_match*>(ast_node))
        {
            a_match->match_end_tag_in_final_pass = compiler->get_unique_tag_based_command_ip() + "match_end";

            compiler->mov(reg(reg::pm), auto_analyze_value(a_match->match_value, compiler));
            // 1. Get id in cr.
            compiler->idstruct(reg(reg::cr), reg(reg::pm), 0);

            real_analyze_finalize(a_match->cases, compiler);
            compiler->ext_panic(opnum::imm("All cases failed to match, may be wrong type value returned by the external function."));

            compiler->tag(a_match->match_end_tag_in_final_pass);

        }
        else if (ast_match_union_case* a_match_union_case = dynamic_cast<ast_match_union_case*>(ast_node))
        {
            wo_assert(a_match_union_case->in_match);

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

                    analyze_pattern_in_finalize(a_pattern_union_value->pattern_arg_in_union_may_nil, a_match_union_case->take_place_value_may_nil, true, compiler);
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
        else
            lang_anylizer->lang_error(lexer::errorlevel::error, ast_node, L"Bad ast node.");
    }
    void lang::analyze_finalize(grammar::ast_base* ast_node, ir_compiler* compiler)
    {
        // first, check each extern func
        for (auto& [ext_func, funcdef_list] : extern_symb_func_definee)
        {
            wo_assert(!funcdef_list.empty() && funcdef_list.front()->externed_func_info != nullptr);

            compiler->record_extern_native_function(
                (intptr_t)ext_func,
                *funcdef_list.front()->source_file,
                funcdef_list.front()->externed_func_info->load_from_lib,
                funcdef_list.front()->externed_func_info->symbol_name);

            ast::ast_value_function_define* last_fundef = nullptr;
            for (auto funcdef : funcdef_list)
            {
                if (last_fundef)
                {
                    wo_assert(last_fundef->value_type->is_complex());
                    if (last_fundef->value_type->is_variadic_function_type
                        != funcdef->value_type->is_variadic_function_type
                        || (last_fundef->value_type->argument_types.size() !=
                            funcdef->value_type->argument_types.size()))
                    {
                        goto _update_all_func_using_tc;
                    }
                }
                last_fundef = funcdef;
            }
            continue;
        _update_all_func_using_tc:;
            for (auto funcdef : funcdef_list)
                funcdef->is_different_arg_count_in_same_extern_symbol = true;
        }

        size_t public_block_begin = compiler->get_now_ip();
        auto res_ip = compiler->reserved_stackvalue();                      // reserved..
        real_analyze_finalize(ast_node, compiler);
        auto used_tmp_regs = compiler->update_all_temp_regist_to_stack(public_block_begin);
        compiler->reserved_stackvalue(res_ip, used_tmp_regs); // set reserved size

        compiler->mov(opnum::reg(opnum::reg::spreg::cr), opnum::imm(0));
        compiler->jmp(opnum::tag("__rsir_rtcode_seg_function_define_end"));

        while (!in_used_functions.empty())
        {
            auto tmp_build_func_list = in_used_functions;
            in_used_functions.clear();
            for (auto* funcdef : tmp_build_func_list)
            {
                // If current is template, the node will not be compile, just skip it.
                if (funcdef->is_template_define)
                    continue;

                wo_assert(funcdef->completed_in_pass2);

                size_t funcbegin_ip = compiler->get_now_ip();
                now_function_in_final_anylize = funcdef;

                compiler->ext_funcbegin();

                compiler->tag(funcdef->get_ir_func_signature_tag());
                if (funcdef->declear_attribute->is_extern_attr())
                {
                    // this function is externed, put it into extern-table and update the value in ir-compiler
                    auto&& spacename = funcdef->get_namespace_chain();
                    auto&& fname = (spacename.empty() ? "" : spacename + "::") + wstr_to_str(*funcdef->function_name);

                    compiler->record_extern_script_function(fname);
                }
                compiler->pdb_info->generate_func_begin(funcdef, compiler);

                auto res_ip = compiler->reserved_stackvalue();                      // reserved..

                // apply args.
                int arg_count = 0;
                auto arg_index = funcdef->argument_list->children;
                while (arg_index)
                {
                    if (auto* a_value_arg_define = dynamic_cast<ast::ast_value_arg_define*>(arg_index))
                    {
                        // Issue N221109: Reference will not support.
                        // All arguments will 'psh' to stack & no 'pshr' command in future.
                        if (a_value_arg_define->symbol != nullptr)
                        {
                            if (a_value_arg_define->symbol->is_marked_as_used_variable == false
                                && a_value_arg_define->symbol->define_in_function == true)
                            {
                                lang_anylizer->lang_error(lexer::errorlevel::error, a_value_arg_define, WO_ERR_UNUSED_VARIABLE_DEFINE,
                                    a_value_arg_define->arg_name->c_str(),
                                    str_to_wstr(funcdef->get_ir_func_signature_tag()).c_str());
                            }

                            funcdef->this_func_scope->
                                reduce_function_used_stack_size_at(a_value_arg_define->symbol->stackvalue_index_in_funcs);

                            wo_assert(0 == a_value_arg_define->symbol->stackvalue_index_in_funcs);
                            a_value_arg_define->symbol->stackvalue_index_in_funcs = -2 - arg_count - (wo_integer_t)funcdef->capture_variables.size();
                        }

                        // NOTE: If argument's name is `_`, still need continue;
                    }
                    else // variadic
                        break;
                    arg_count++;
                    arg_index = arg_index->sibling;
                }

                for (auto& symb : funcdef->this_func_scope->in_function_symbols)
                {
                    if (symb->is_constexpr_or_immut_no_closure_func())
                        symb->is_constexpr = true;
                }

                funcdef->this_func_scope->in_function_symbols.erase(
                    std::remove_if(
                        funcdef->this_func_scope->in_function_symbols.begin(),
                        funcdef->this_func_scope->in_function_symbols.end(),
                        [](auto* symb) {return symb->is_constexpr; }),
                    funcdef->this_func_scope->in_function_symbols.end());

                real_analyze_finalize(funcdef->in_function_sentence, compiler);

                auto temp_reg_to_stack_count = compiler->update_all_temp_regist_to_stack(funcbegin_ip);
                auto reserved_stack_size =
                    funcdef->this_func_scope->max_used_stack_size_in_func
                    + temp_reg_to_stack_count;

                compiler->reserved_stackvalue(res_ip, (uint16_t)reserved_stack_size); // set reserved size
                compiler->tag(funcdef->get_ir_func_signature_tag() + "_do_ret");

                wo_assert(funcdef->value_type->is_complex());
                if (!funcdef->value_type->complex_type->is_void())
                    compiler->ext_panic(opnum::imm("Function returned without valid value."));

                // do default return
                if (funcdef->is_closure_function())
                    compiler->ret((uint16_t)funcdef->capture_variables.size());
                else
                    compiler->ret();

                compiler->pdb_info->generate_func_end(funcdef, temp_reg_to_stack_count, compiler);
                compiler->ext_funcend();

                for (auto funcvar : funcdef->this_func_scope->in_function_symbols)
                    compiler->pdb_info->add_func_variable(funcdef, *funcvar->name, funcvar->variable_value->row_end_no, funcvar->stackvalue_index_in_funcs);
            }
        }
        compiler->tag("__rsir_rtcode_seg_function_define_end");
        compiler->loaded_libs = extern_libs;
        compiler->pdb_info->finalize_generate_debug_info();

        wo::grammar::ast_base::exchange_this_thread_ast(generated_ast_nodes_buffers);
    }
    lang_scope* lang::begin_namespace(ast::ast_namespace* a_namespace)
    {
        wo_assert(a_namespace->source_file != nullptr);
        if (lang_scopes.size())
        {
            auto fnd = lang_scopes.back()->sub_namespaces.find(a_namespace->scope_name);
            if (fnd != lang_scopes.back()->sub_namespaces.end())
            {
                lang_scopes.push_back(fnd->second);
                now_namespace = lang_scopes.back();
                now_namespace->last_entry_ast = a_namespace;
                return now_namespace;
            }
        }

        lang_scope* scope = new lang_scope;
        lang_scopes_buffers.push_back(scope);

        scope->stop_searching_in_last_scope_flag = false;
        scope->type = lang_scope::scope_type::namespace_scope;
        scope->belong_namespace = now_namespace;
        scope->parent_scope = lang_scopes.empty() ? nullptr : lang_scopes.back();
        scope->scope_namespace = a_namespace->scope_name;

        if (lang_scopes.size())
            lang_scopes.back()->sub_namespaces[a_namespace->scope_name] = scope;

        lang_scopes.push_back(scope);
        now_namespace = lang_scopes.back();
        now_namespace->last_entry_ast = a_namespace;
        return now_namespace;
    }
    void lang::end_namespace()
    {
        wo_assert(lang_scopes.back()->type == lang_scope::scope_type::namespace_scope);

        now_namespace = lang_scopes.back()->belong_namespace;
        lang_scopes.pop_back();
    }
    lang_scope* lang::begin_scope(grammar::ast_base* block_beginer)
    {
        lang_scope* scope = new lang_scope;
        lang_scopes_buffers.push_back(scope);

        scope->last_entry_ast = block_beginer;
        wo_assert(block_beginer->source_file != nullptr);
        scope->stop_searching_in_last_scope_flag = false;
        scope->type = lang_scope::scope_type::just_scope;
        scope->belong_namespace = now_namespace;
        scope->parent_scope = lang_scopes.empty() ? nullptr : lang_scopes.back();

        lang_scopes.push_back(scope);
        return scope;
    }
    void lang::end_scope()
    {
        wo_assert(lang_scopes.back()->type == lang_scope::scope_type::just_scope);

        auto scope = now_scope();
        if (auto* func = in_function())
        {
            func->used_stackvalue_index -= scope->this_block_used_stackvalue_count;
        }
        lang_scopes.pop_back();
    }
    lang_scope* lang::begin_function(ast::ast_value_function_define* ast_value_funcdef)
    {
        bool already_created_func_scope = ast_value_funcdef->this_func_scope;
        lang_scope* scope =
            already_created_func_scope ? ast_value_funcdef->this_func_scope : new lang_scope;
        scope->last_entry_ast = ast_value_funcdef;
        wo_assert(ast_value_funcdef->source_file != nullptr);
        if (!already_created_func_scope)
        {
            lang_scopes_buffers.push_back(scope);

            scope->stop_searching_in_last_scope_flag = false;
            scope->type = lang_scope::scope_type::function_scope;
            scope->belong_namespace = now_namespace;
            scope->parent_scope = lang_scopes.empty() ? nullptr : lang_scopes.back();
            scope->function_node = ast_value_funcdef;

            if (ast_value_funcdef->function_name != nullptr && !ast_value_funcdef->is_template_reification)
            {
                // Not anymous function or template_reification , define func-symbol..
                auto* sym = define_variable_in_this_scope(
                    ast_value_funcdef,
                    ast_value_funcdef->function_name,
                    ast_value_funcdef,
                    ast_value_funcdef->declear_attribute,
                    template_style::NORMAL,
                    ast::identifier_decl::IMMUTABLE);
                ast_value_funcdef->symbol = sym;
            }
        }
        lang_scopes.push_back(scope);
        return scope;
    }
    void lang::end_function()
    {
        wo_assert(lang_scopes.back()->type == lang_scope::scope_type::function_scope);
        lang_scopes.pop_back();
    }
    lang_scope* lang::now_scope() const
    {
        wo_assert(!lang_scopes.empty());
        return lang_scopes.back();
    }
    lang_scope* lang::in_function() const
    {
        for (auto rindex = lang_scopes.rbegin(); rindex != lang_scopes.rend(); rindex++)
        {
            if ((*rindex)->type == lang_scope::scope_type::function_scope)
                return *rindex;
        }
        return nullptr;
    }
    lang_scope* lang::in_function_pass2() const
    {
        return current_function_in_pass2;
    }

    lang_symbol* lang::define_variable_in_this_scope(
        grammar::ast_base* errreporter,
        wo_pstring_t names,
        ast::ast_value* init_val,
        ast::ast_decl_attribute* attr,
        template_style is_template_value,
        ast::identifier_decl mutable_type,
        size_t captureindex)
    {
        wo_assert(lang_scopes.size());

        if (is_template_value != template_style::IS_TEMPLATE_VARIABLE_IMPL && (lang_scopes.back()->symbols.find(names) != lang_scopes.back()->symbols.end()))
        {
            auto* last_found_symbol = lang_scopes.back()->symbols[names];

            lang_anylizer->lang_error(lexer::errorlevel::error, errreporter, WO_ERR_REDEFINED, names->c_str());
            lang_anylizer->lang_error(lexer::errorlevel::infom,
                (last_found_symbol->type == lang_symbol::symbol_type::typing || last_found_symbol->type == lang_symbol::symbol_type::type_alias)
                ? (grammar::ast_base*)last_found_symbol->type_informatiom
                : (grammar::ast_base*)last_found_symbol->variable_value
                , WO_INFO_ITEM_IS_DEFINED_HERE, names->c_str());

            return last_found_symbol;
        }

        if (auto* func_def = dynamic_cast<ast::ast_value_function_define*>(init_val);
            func_def != nullptr && func_def->function_name != nullptr)
        {
            wo_assert(template_style::NORMAL == is_template_value);

            lang_symbol* sym;

            sym = lang_scopes.back()->symbols[names] = new lang_symbol;
            sym->type = lang_symbol::symbol_type::function;
            sym->attribute = attr;
            sym->name = names;
            sym->defined_in_scope = lang_scopes.back();
            sym->variable_value = func_def;
            sym->is_template_symbol = func_def->is_template_define;
            sym->decl = mutable_type;

            lang_symbols.push_back(sym);

            return sym;
        }
        else
        {
            lang_symbol* sym = new lang_symbol;
            if (is_template_value != template_style::IS_TEMPLATE_VARIABLE_IMPL)
                lang_scopes.back()->symbols[names] = sym;

            sym->type = lang_symbol::symbol_type::variable;
            sym->attribute = attr;
            sym->name = names;
            sym->variable_value = init_val;
            sym->defined_in_scope = lang_scopes.back();
            sym->decl = mutable_type;

            auto* func = in_function();
            if (attr->is_extern_attr() && func)
                lang_anylizer->lang_error(lexer::errorlevel::error, attr, WO_ERR_CANNOT_EXPORT_SYMB_IN_FUNC);
            if (func && !sym->attribute->is_static_attr())
            {
                sym->define_in_function = true;
                sym->static_symbol = false;

                if (captureindex == (size_t)-1)
                {
                    if (sym->decl == ast::identifier_decl::MUTABLE || !init_val->is_constant)
                    {
                        if (is_template_value != template_style::IS_TEMPLATE_VARIABLE_DEFINE)
                        {
                            sym->stackvalue_index_in_funcs = func->assgin_stack_index(sym);
                            lang_scopes.back()->this_block_used_stackvalue_count++;
                        }
                    }
                    else
                        sym->is_constexpr = true;
                }
                else
                {
                    // Is capture symbol;
                    sym->is_captured_variable = true;
                    sym->captured_index = captureindex;
                }
            }
            else
            {
                wo_assert(captureindex == (size_t)-1);

                if (func)
                    sym->define_in_function = true;
                else
                    sym->define_in_function = false;

                sym->static_symbol = true;

                if (sym->decl == ast::identifier_decl::MUTABLE || !init_val->is_constant)
                {
                    if (is_template_value != template_style::IS_TEMPLATE_VARIABLE_DEFINE)
                        sym->global_index_in_lang = global_symbol_index++;
                }
                else
                    sym->is_constexpr = true;
            }
            lang_symbols.push_back(sym);
            return sym;
        }
    }
    lang_symbol* lang::define_type_in_this_scope(ast::ast_using_type_as* def, ast::ast_type* as_type, ast::ast_decl_attribute* attr)
    {
        wo_assert(lang_scopes.size());

        if (lang_scopes.back()->symbols.find(def->new_type_identifier) != lang_scopes.back()->symbols.end())
        {
            auto* last_found_symbol = lang_scopes.back()->symbols[def->new_type_identifier];

            lang_anylizer->lang_error(lexer::errorlevel::error, as_type, WO_ERR_REDEFINED, def->new_type_identifier->c_str());
            lang_anylizer->lang_error(lexer::errorlevel::infom,
                (last_found_symbol->type == lang_symbol::symbol_type::typing || last_found_symbol->type == lang_symbol::symbol_type::type_alias)
                ? (grammar::ast_base*)last_found_symbol->type_informatiom
                : (grammar::ast_base*)last_found_symbol->variable_value
                , WO_INFO_ITEM_IS_DEFINED_HERE, def->new_type_identifier->c_str());
            return last_found_symbol;
        }
        else
        {
            lang_symbol* sym = lang_scopes.back()->symbols[def->new_type_identifier] = new lang_symbol;
            sym->attribute = attr;
            if (def->is_alias)
                sym->type = lang_symbol::symbol_type::type_alias;
            else
                sym->type = lang_symbol::symbol_type::typing;
            sym->name = def->new_type_identifier;

            sym->type_informatiom = as_type;
            sym->defined_in_scope = lang_scopes.back();
            sym->define_node = def;

            lang_symbols.push_back(sym);
            return sym;
        }
    }

    bool lang::check_symbol_is_accessable(lang_symbol* symbol, lang_scope* current_scope, grammar::ast_base* ast, bool give_error)
    {
        if (symbol->attribute)
        {
            if (symbol->attribute->is_protected_attr())
            {
                auto* symbol_defined_space = symbol->defined_in_scope;
                while (current_scope)
                {
                    if (current_scope == symbol_defined_space)
                        return true;
                    current_scope = current_scope->parent_scope;
                }
                if (give_error)
                    lang_anylizer->lang_error(lexer::errorlevel::error, ast, WO_ERR_CANNOT_REACH_PROTECTED_IN_OTHER_FUNC, symbol->name->c_str());
                return false;
            }
            if (symbol->attribute->is_private_attr())
            {
                if (ast->source_file == symbol->defined_source())
                    return true;
                if (give_error)
                    lang_anylizer->lang_error(lexer::errorlevel::error, ast, WO_ERR_CANNOT_REACH_PRIVATE_IN_OTHER_FUNC, symbol->name->c_str(),
                        symbol->defined_source()->c_str());
                return false;
            }
        }
        return true;
    }

    template<typename T>
    double levenshtein_distance(const T& s, const T& t)
    {
        size_t n = s.size();
        size_t m = t.size();
        std::vector<size_t> d((n + 1) * (m + 1));

        auto df = [&d, n](size_t x, size_t y)->size_t& {return d[y * (n + 1) + x]; };

        if (n == m && n == 0)
            return 0.;
        if (n == 0)
            return (double)m / (double)std::max(n, m);
        if (m == 0)
            return (double)n / (double)std::max(n, m);

        for (size_t i = 0; i <= n; ++i)
            df(i, 0) = i;
        for (size_t j = 0; j <= m; ++j)
            df(0, j) = j;

        for (size_t i = 1; i <= n; i++)
        {
            for (size_t j = 1; j <= m; j++)
            {
                size_t cost = (t[j - 1] == s[i - 1]) ? 0 : 1;
                df(i, j) = std::min(
                    std::min(df(i - 1, j) + 1, df(i, j - 1) + 1),
                    df(i - 1, j - 1) + cost);
            }
        }

        return (double)df(n, m) / (double)std::max(n, m);
    }

    lang_symbol* lang::find_symbol_in_this_scope(ast::ast_symbolable_base* var_ident, wo_pstring_t ident_str, int target_type_mask, bool fuzzy_for_err_report)
    {
        wo_assert(lang_scopes.size());

        if (var_ident->symbol && !fuzzy_for_err_report)
            return var_ident->symbol;

        lang_symbol* fuzzy_nearest_symbol = nullptr;
        auto update_fuzzy_nearest_symbol =
            [&fuzzy_nearest_symbol, fuzzy_for_err_report, ident_str, target_type_mask]
        (lang_symbol* symbol) {
            if (fuzzy_for_err_report)
            {
                auto distance = levenshtein_distance(*symbol->name, *ident_str);
                if (distance <= 0.3 && (symbol->type & target_type_mask) != 0)
                {
                    if (fuzzy_nearest_symbol == nullptr
                        || distance < levenshtein_distance(*fuzzy_nearest_symbol->name, *ident_str))
                        fuzzy_nearest_symbol = symbol;
                }
            }
        };

        if (!var_ident->search_from_global_namespace && var_ident->scope_namespaces.empty())
            for (auto rind = template_stack.rbegin(); rind != template_stack.rend(); rind++)
            {
                if (fuzzy_for_err_report)
                {
                    for (auto& finding_symbol : *rind)
                        update_fuzzy_nearest_symbol(finding_symbol.second);
                }
                else if (auto fnd = rind->find(ident_str); fnd != rind->end())
                {
                    if ((fnd->second->type & target_type_mask) != 0)
                        return fnd->second;
                }
            }

        auto* searching_from_scope = var_ident->searching_begin_namespace_in_pass2;

        if (var_ident->searching_from_type)
        {
            fully_update_type(var_ident->searching_from_type, !in_pass2.back());

            if (!var_ident->searching_from_type->is_pending() || var_ident->searching_from_type->using_type_name)
            {
                auto* finding_from_type = var_ident->searching_from_type;
                if (var_ident->searching_from_type->using_type_name)
                    finding_from_type = var_ident->searching_from_type->using_type_name;

                if (finding_from_type->symbol)
                {
                    var_ident->searching_begin_namespace_in_pass2 =
                        finding_from_type->symbol->defined_in_scope;
                    wo_assert(var_ident->source_file != nullptr);
                }
                else
                    var_ident->search_from_global_namespace = true;

                var_ident->scope_namespaces.insert(var_ident->scope_namespaces.begin(),
                    finding_from_type->type_name);

                var_ident->searching_from_type = nullptr;
            }
            else
            {
                return nullptr;
            }
        }
        else if (!var_ident->scope_namespaces.empty())
        {
            if (!var_ident->search_from_global_namespace)
                for (auto rind = template_stack.rbegin(); rind != template_stack.rend(); rind++)
                {
                    if (auto fnd = rind->find(var_ident->scope_namespaces.front()); fnd != rind->end())
                    {
                        auto* fnd_template_type = fnd->second->type_informatiom;
                        if (fnd_template_type->using_type_name)
                            fnd_template_type = fnd_template_type->using_type_name;

                        if (fnd_template_type->symbol)
                        {
                            var_ident->searching_begin_namespace_in_pass2 = fnd_template_type->symbol->defined_in_scope;
                            wo_assert(var_ident->source_file != nullptr);
                        }
                        else
                            var_ident->search_from_global_namespace = true;

                        var_ident->scope_namespaces[0] = fnd_template_type->type_name;
                        break;
                    }
                }

        }

        auto* searching = var_ident->search_from_global_namespace ?
            lang_scopes.front()
            :
            (
                var_ident->searching_begin_namespace_in_pass2 ?
                var_ident->searching_begin_namespace_in_pass2
                :
                lang_scopes.back()
                );

        std::vector<lang_scope*> searching_namespace;
        searching_namespace.push_back(searching);
        // ATTENTION: IF SYMBOL WITH SAME NAME, IT MAY BE DUP HERE BECAUSE BY USING-NAMESPACE,
        //            WE NEED CHOOSE NEAREST SYMBOL
        //            So we should search in scope chain, if found, return it immediately.
        auto* _first_searching = searching;
        while (_first_searching)
        {
            size_t namespace_index = 0;

            auto* indet_finding_namespace = _first_searching;
            while (namespace_index < var_ident->scope_namespaces.size())
            {
                if (auto fnd = indet_finding_namespace->sub_namespaces.find(var_ident->scope_namespaces[namespace_index]);
                    fnd != indet_finding_namespace->sub_namespaces.end())
                {
                    namespace_index++;
                    indet_finding_namespace = fnd->second;
                }
                else
                    goto TRY_UPPER_SCOPE;
            }

            if (fuzzy_for_err_report)
            {
                for (auto& finding_symbol : indet_finding_namespace->symbols)
                    update_fuzzy_nearest_symbol(finding_symbol.second);
            }
            else if (auto fnd = indet_finding_namespace->symbols.find(ident_str);
                fnd != indet_finding_namespace->symbols.end())
            {
                if ((fnd->second->type & target_type_mask) != 0
                    && (fnd->second->type == lang_symbol::symbol_type::typing
                        || fnd->second->type == lang_symbol::symbol_type::type_alias
                        || check_symbol_is_accessable(fnd->second, searching_from_scope, var_ident, false)))
                    return var_ident->symbol = fnd->second;
            }

        TRY_UPPER_SCOPE:
            _first_searching = _first_searching->parent_scope;
        }

        // Not found in current scope, trying to find it in using-namespace/root namespace
        auto* _searching_in_all = searching;
        while (_searching_in_all)
        {
            for (auto* a_using_namespace : _searching_in_all->used_namespace)
            {
                wo_assert(a_using_namespace->source_file != nullptr);
                if (a_using_namespace->source_file != var_ident->source_file)
                    continue;

                if (!a_using_namespace->from_global_namespace)
                {
                    auto* finding_namespace = _searching_in_all;
                    while (finding_namespace)
                    {
                        auto* _deep_in_namespace = finding_namespace;
                        for (auto& nspace : a_using_namespace->used_namespace_chain)
                        {
                            if (auto fnd = _deep_in_namespace->sub_namespaces.find(nspace);
                                fnd != _deep_in_namespace->sub_namespaces.end())
                                _deep_in_namespace = fnd->second;
                            else
                            {
                                // fail
                                goto failed_in_this_namespace;
                            }
                        }
                        // ok!
                        searching_namespace.push_back(_deep_in_namespace);
                    failed_in_this_namespace:;
                        finding_namespace = finding_namespace->belong_namespace;
                    }
                }
                else
                {
                    auto* _deep_in_namespace = lang_scopes.front();
                    for (auto& nspace : a_using_namespace->used_namespace_chain)
                    {
                        if (auto fnd = _deep_in_namespace->sub_namespaces.find(nspace);
                            fnd != _deep_in_namespace->sub_namespaces.end())
                            _deep_in_namespace = fnd->second;
                        else
                        {
                            // fail
                            goto failed_in_this_namespace_from_global;
                        }
                    }
                    // ok!
                    searching_namespace.push_back(_deep_in_namespace);
                failed_in_this_namespace_from_global:;
                }
            }
            _searching_in_all = _searching_in_all->parent_scope;
        }
        std::set<lang_symbol*> searching_result;
        std::set<lang_scope*> searched_scopes;

        for (auto _searching : searching_namespace)
        {
            bool deepin_search = _searching == searching;
            while (_searching)
            {
                // search_in 
                if (var_ident->scope_namespaces.size())
                {
                    size_t namespace_index = 0;
                    if (_searching->type != lang_scope::scope_type::namespace_scope)
                        _searching = _searching->belong_namespace;

                    auto* stored_scope_for_next_try = _searching;

                    while (namespace_index < var_ident->scope_namespaces.size())
                    {
                        if (auto fnd = _searching->sub_namespaces.find(var_ident->scope_namespaces[namespace_index]);
                            fnd != _searching->sub_namespaces.end() && searched_scopes.find(fnd->second) == searched_scopes.end())
                        {
                            namespace_index++;
                            _searching = fnd->second;
                        }
                        else
                        {
                            _searching = stored_scope_for_next_try;
                            goto there_is_no_such_namespace;
                        }
                    }
                }

                searched_scopes.insert(_searching);
                if (fuzzy_for_err_report)
                {
                    for (auto& finding_symbol : _searching->symbols)
                        update_fuzzy_nearest_symbol(finding_symbol.second);
                }
                else if (auto fnd = _searching->symbols.find(ident_str);
                    fnd != _searching->symbols.end())
                {
                    if ((fnd->second->type & target_type_mask) != 0)
                        searching_result.insert(fnd->second);
                    goto next_searching_point;
                }

            there_is_no_such_namespace:
                if (deepin_search)
                    _searching = _searching->parent_scope;
                else
                    _searching = nullptr;
            }

        next_searching_point:;
        }

        if (fuzzy_for_err_report)
            return fuzzy_nearest_symbol;

        if (searching_result.empty())
            return var_ident->symbol = nullptr;

        if (searching_result.size() > 1)
        {
            // Result might have un-accessable type? remove them
            std::set<lang_symbol*> selecting_results;
            selecting_results.swap(searching_result);
            for (auto fnd_result : selecting_results)
                if (fnd_result->type == lang_symbol::symbol_type::typing
                    || fnd_result->type == lang_symbol::symbol_type::type_alias
                    || check_symbol_is_accessable(fnd_result, searching_from_scope, var_ident, false))
                    searching_result.insert(fnd_result);

            if (searching_result.empty())
                return var_ident->symbol = nullptr;
            else if (searching_result.size() > 1)
            {
                std::wstring err_info = WO_ERR_SYMBOL_IS_AMBIGUOUS;
                size_t fnd_count = 0;
                for (auto fnd_result : searching_result)
                {
                    auto _full_namespace_ = wo::str_to_wstr(get_belong_namespace_path_with_lang_scope(fnd_result->defined_in_scope));
                    if (_full_namespace_ == L"")
                        err_info += WO_TERM_GLOBAL_NAMESPACE;
                    else
                        err_info += L"'" + _full_namespace_ + L"'";
                    fnd_count++;
                    if (fnd_count + 1 == searching_result.size())
                        err_info += L" " WO_TERM_AND L" ";
                    else
                        err_info += L", ";
                }

                lang_anylizer->lang_error(lexer::errorlevel::error, var_ident, err_info.c_str(), ident_str->c_str());
            }
        }
        // Check symbol is accessable?
        auto* result = *searching_result.begin();
        if (result->type == lang_symbol::symbol_type::typing
            || result->type == lang_symbol::symbol_type::type_alias
            || check_symbol_is_accessable(result, searching_from_scope, var_ident, has_step_in_step2))
            return var_ident->symbol = result;
        return var_ident->symbol = nullptr;
    }
    lang_symbol* lang::find_type_in_this_scope(ast::ast_type* var_ident)
    {
        auto* result = find_symbol_in_this_scope(var_ident, var_ident->type_name,
            lang_symbol::symbol_type::type_alias | lang_symbol::symbol_type::typing, false);

        return result;
    }

    // Only used for check symbol is exist?
    lang_symbol* lang::find_value_symbol_in_this_scope(ast::ast_value_variable* var_ident)
    {
        return find_symbol_in_this_scope(var_ident, var_ident->var_name,
            lang_symbol::symbol_type::variable | lang_symbol::symbol_type::function, false);
    }

    lang_symbol* lang::find_value_in_this_scope(ast::ast_value_variable* var_ident)
    {
        auto* result = find_value_symbol_in_this_scope(var_ident);

        if (result)
        {
            auto symb_defined_in_func = result->defined_in_scope;
            while (symb_defined_in_func->parent_scope &&
                symb_defined_in_func->type != wo::lang_scope::scope_type::function_scope)
                symb_defined_in_func = symb_defined_in_func->parent_scope;

            auto* current_function = in_function();

            if (current_function &&
                result->define_in_function
                && !result->static_symbol
                && symb_defined_in_func != current_function
                && symb_defined_in_func->function_node != current_function->function_node)
            {
                if (result->is_template_symbol)
                    lang_anylizer->lang_error(lexer::errorlevel::error, var_ident, WO_ERR_CANNOT_CAPTURE_TEMPLATE_VAR,
                        result->name->c_str());

                // The variable is not static and define outside the function. ready to capture it!
                if (current_function->function_node->function_name != nullptr)
                    // Only anonymous can capture variablel;
                    lang_anylizer->lang_error(lexer::errorlevel::error, var_ident, WO_ERR_CANNOT_CAPTURE_IN_NAMED_FUNC,
                        result->name->c_str());

                if (result->decl == identifier_decl::IMMUTABLE)
                {
                    result->variable_value->update_constant_value(lang_anylizer);
                    if (result->variable_value->is_constant)
                        // Woolang 1.10.4: Constant variable not need to capture.
                        return result;
                }

                auto* current_func_defined_in_function = current_function->parent_scope;
                while (current_func_defined_in_function->parent_scope &&
                    current_func_defined_in_function->type != wo::lang_scope::scope_type::function_scope)
                    current_func_defined_in_function = current_func_defined_in_function->parent_scope;

                std::vector<lang_scope*> need_capture_func_list;

                while (current_func_defined_in_function
                    && current_func_defined_in_function != symb_defined_in_func)
                {
                    need_capture_func_list.insert(need_capture_func_list.begin(), current_func_defined_in_function);

                    // goto last function
                    current_func_defined_in_function = current_func_defined_in_function->parent_scope;
                    while (current_func_defined_in_function->parent_scope &&
                        current_func_defined_in_function->type != wo::lang_scope::scope_type::function_scope)
                        current_func_defined_in_function = current_func_defined_in_function->parent_scope;
                }
                need_capture_func_list.push_back(current_function);

                // Add symbol to capture list.
                for (auto* cur_capture_func_scope : need_capture_func_list)
                {
                    auto& capture_list = cur_capture_func_scope->function_node->capture_variables;
                    wo_assert(std::find(capture_list.begin(), capture_list.end(), result) == capture_list.end()
                        || cur_capture_func_scope == cur_capture_func_scope);

                    if (std::find(capture_list.begin(), capture_list.end(), result) == capture_list.end())
                    {
                        result->is_marked_as_used_variable = true;
                        capture_list.push_back(result);
                        // Define a closure symbol instead of current one.
                        temporary_entry_scope_in_pass1(cur_capture_func_scope);
                        result = define_variable_in_this_scope(
                            result->variable_value,
                            result->name,
                            result->variable_value,
                            result->attribute,
                            template_style::NORMAL,
                            ast::identifier_decl::IMMUTABLE,
                            capture_list.size() - 1);
                        temporary_leave_scope_in_pass1();
                    }
                }
                var_ident->symbol = result;
            }
        }
        return result;
    }
    bool lang::has_compile_error()const
    {
        return lang_anylizer->has_error();
    }
}
