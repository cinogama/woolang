#include "wo_lang.hpp"

namespace wo
{
    using namespace ast;

#define WO_PASS1(NODETYPE) bool lang::pass1_##NODETYPE (ast::NODETYPE* astnode)
#define WO_AST() astnode; if (!astnode)return false

    WO_PASS1(ast_namespace)
    {
        auto* a_namespace = WO_AST();

        begin_namespace(a_namespace->scope_name);
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

        for (auto& varref : a_varref_defs->var_refs)
            analyze_pattern_in_pass1(varref.pattern, a_varref_defs->declear_attribute, varref.init_val);
        return true;
    }
    WO_PASS1(ast_value_binary)
    {
        auto* a_value_bin = WO_AST();

        analyze_pass1(a_value_bin->left);
        analyze_pass1(a_value_bin->right);

        a_value_bin->add_child(a_value_bin->left);
        a_value_bin->add_child(a_value_bin->right);

        if (!a_value_bin->left->value_type->is_builtin_basic_type()
            || !a_value_bin->right->value_type->is_builtin_basic_type())
            // IS CUSTOM TYPE, DELAY THE TYPE CALC TO PASS2
            a_value_bin->value_type = nullptr;
        else
            a_value_bin->value_type = ast_value_binary::binary_upper_type_with_operator(
                a_value_bin->left->value_type,
                a_value_bin->right->value_type,
                a_value_bin->operate);

        if (nullptr == a_value_bin->value_type)
        {
            a_value_bin->value_type = ast_type::create_type_at(a_value_bin, L"pending");

            ast_value_funccall* try_operator_func_overload = new ast_value_funccall();
            try_operator_func_overload->copy_source_info(a_value_bin);

            try_operator_func_overload->try_invoke_operator_override_function = true;
            try_operator_func_overload->arguments = new ast_list();
            try_operator_func_overload->value_type = ast_type::create_type_at(try_operator_func_overload, L"pending");

            try_operator_func_overload->called_func = new ast_value_variable(std::wstring(L"operator ") + lexer::lex_is_operate_type(a_value_bin->operate));
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
                        str_to_wstr(*a_value_idx->index->get_constant_value().string)
                    ); fnd != a_value_idx->from->value_type->struct_member_index.end())
                {
                    if (fnd->second.init_value_may_nil)
                    {
                        if (!fnd->second.init_value_may_nil->value_type->is_pending())
                        {
                            a_value_idx->value_type = fnd->second.init_value_may_nil->value_type;
                            a_value_idx->struct_offset = fnd->second.offset;
                        }
                    }
                }
            }
        }
        else if (a_value_idx->from->value_type->is_tuple())
        {
            if (a_value_idx->index->is_constant && a_value_idx->index->value_type->is_integer())
            {
                // Index tuple must with constant integer.
                auto index = a_value_idx->index->get_constant_value().integer;
                if ((size_t)index < a_value_idx->from->value_type->template_arguments.size() && index >= 0)
                {
                    a_value_idx->value_type = a_value_idx->from->value_type->template_arguments[index];
                    a_value_idx->struct_offset = (uint16_t)index;
                }
                else
                    a_value_idx->value_type = ast_type::create_type_at(a_value_idx, L"pending");
            }
        }
        else if (a_value_idx->from->value_type->is_string())
        {
            a_value_idx->value_type = ast_type::create_type_at(a_value_idx, L"string");
        }
        else if (!a_value_idx->from->value_type->is_pending())
        {
            if (a_value_idx->from->value_type->is_array())
            {
                a_value_idx->value_type = a_value_idx->from->value_type->template_arguments[0];
            }
            else if (a_value_idx->from->value_type->is_map())
            {
                a_value_idx->value_type = a_value_idx->from->value_type->template_arguments[1];
            }
            else
            {
                a_value_idx->value_type = ast_type::create_type_at(a_value_idx, L"dynamic");
            }
            if ((a_value_idx->is_const_value = a_value_idx->from->is_const_value))
                a_value_idx->can_be_assign = false;
        }
        return true;
    }
    WO_PASS1(ast_value_assign)
    {
        auto* a_value_assi = WO_AST();

        analyze_pass1(a_value_assi->left);
        analyze_pass1(a_value_assi->right);

        a_value_assi->add_child(a_value_assi->left);
        a_value_assi->add_child(a_value_assi->right);

        a_value_assi->value_type = ast_type::create_type_at(a_value_assi, L"pending");

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

        a_value_logic_bin->add_child(a_value_logic_bin->left);
        a_value_logic_bin->add_child(a_value_logic_bin->right);

        bool has_default_op = false;
        if (a_value_logic_bin->left->value_type->is_builtin_basic_type()
            && a_value_logic_bin->right->value_type->is_builtin_basic_type())
        {
            if (a_value_logic_bin->operate == +lex_type::l_lor || a_value_logic_bin->operate == +lex_type::l_land)
            {
                if (a_value_logic_bin->left->value_type->is_bool() && a_value_logic_bin->right->value_type->is_bool())
                {
                    a_value_logic_bin->value_type = ast_type::create_type_at(a_value_logic_bin, L"bool");
                    has_default_op = true;
                }
            }
            else if ((a_value_logic_bin->left->value_type->is_integer()
                || a_value_logic_bin->left->value_type->is_handle()
                || a_value_logic_bin->left->value_type->is_real()
                || a_value_logic_bin->left->value_type->is_string()
                || a_value_logic_bin->left->value_type->is_gchandle())
                && a_value_logic_bin->left->value_type->is_same(a_value_logic_bin->right->value_type, false))
            {
                a_value_logic_bin->value_type = ast_type::create_type_at(a_value_logic_bin, L"bool");
                has_default_op = true;
            }

        }
        if (!has_default_op)
        {
            a_value_logic_bin->value_type = ast_type::create_type_at(a_value_logic_bin, L"pending");
            ast_value_funccall* try_operator_func_overload = new ast_value_funccall();
            try_operator_func_overload->copy_source_info(a_value_logic_bin);

            try_operator_func_overload->try_invoke_operator_override_function = true;
            try_operator_func_overload->arguments = new ast_list();
            try_operator_func_overload->value_type = ast_type::create_type_at(try_operator_func_overload, L"pending");

            try_operator_func_overload->called_func = new ast_value_variable(std::wstring(L"operator ") + lexer::lex_is_operate_type(a_value_logic_bin->operate));
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
        return true;
    }
    WO_PASS1(ast_value_variable)
    {
        auto* a_value_var = WO_AST();
        auto* sym = find_value_in_this_scope(a_value_var);
        if (sym)
        {
            if (sym->type == lang_symbol::symbol_type::variable && sym->is_template_symbol)
            {
                // Here is template variable, delay it's type calc.
            }
            else
                a_value_var->value_type = sym->variable_value->value_type;
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
        a_value_cast->add_child(a_value_cast->_be_cast_value_node);
        return true;
    }
    WO_PASS1(ast_value_type_judge)
    {
        auto* ast_value_judge = WO_AST();
        if (ast_value_judge->is_mark_as_using_ref)
            ast_value_judge->_be_cast_value_node->is_mark_as_using_ref = true;

        analyze_pass1(ast_value_judge->_be_cast_value_node);
        return true;
    }
    WO_PASS1(ast_value_type_check)
    {
        auto* ast_value_check = WO_AST();
        analyze_pass1(ast_value_check->_be_check_value_node);
        if (ast_value_check->aim_type->is_pending())
        {
            // ready for update..
            fully_update_type(ast_value_check->aim_type, true);
        }
        if (ast_value_check->aim_type->is_pure_pending())
            ast_value_check->check_pending = true;

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

                        if (!argdef->symbol)
                        {
                            argdef->symbol = define_variable_in_this_scope(argdef->arg_name, argdef, argdef->declear_attribute, template_style::NORMAL);
                            argdef->symbol->decl = argdef->decl;
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
                            a_value_func->externed_func_info->externed_func =
                                rslib_extern_symbols::get_lib_symbol(
                                    a_value_func->source_file.c_str(),
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

                    extern_symb_func_definee[a_value_func->externed_func_info->externed_func]
                        .push_back(a_value_func);
                }
                else if (!a_value_func->has_return_value && a_value_func->value_type->get_return_type()->type_name == L"pending")
                {
                    // This function has no return, set it as void
                    a_value_func->value_type->set_ret_type(ast_type::create_type_at(a_value_func, L"void"));
                }
            }
            else
                a_value_func->value_type->set_type_with_name(L"pending");
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

        // NOTE There is no need for adding arguments and celled_func to child, pass2 must read them..
        //a_value_funccall->add_child(a_value_funccall->called_func);
        //a_value_funccall->add_child(a_value_funccall->arguments);

        // function call should be 'pending' type, then do override judgement in pass2
        return true;
    }
    WO_PASS1(ast_value_array)
    {
        auto* a_value_arr = WO_AST();
        analyze_pass1(a_value_arr->array_items);
        a_value_arr->add_child(a_value_arr->array_items);

        if (a_value_arr->value_type->is_pending() && !a_value_arr->value_type->is_custom())
        {
            // 
            ast_type* decide_array_item_type = ast_type::create_type_at(a_value_arr, L"anything");

            ast_value* val = dynamic_cast<ast_value*>(a_value_arr->array_items->children);
            if (val)
            {
                if (!val->value_type->is_pending())
                    decide_array_item_type->set_type(val->value_type);
                else
                    decide_array_item_type = nullptr;
            }

            while (val)
            {
                if (val->value_type->is_pending())
                {
                    decide_array_item_type = nullptr;
                    break;
                }

                if (!decide_array_item_type->accept_type(val->value_type, false))
                {
                    auto* mixed_type = ast_value_binary::binary_upper_type(decide_array_item_type, val->value_type);
                    if (mixed_type)
                        decide_array_item_type->set_type_with_name(mixed_type->type_name);
                    else
                        decide_array_item_type = nullptr;
                }
                val = dynamic_cast<ast_value*>(val->sibling);
            }

            if (decide_array_item_type)
            {
                a_value_arr->value_type->template_arguments[0] = decide_array_item_type;
            }
        }
        return true;
    }
    WO_PASS1(ast_value_mapping)
    {
        auto* a_value_map = WO_AST();
        analyze_pass1(a_value_map->mapping_pairs);
        a_value_map->add_child(a_value_map->mapping_pairs);

        if (a_value_map->value_type->is_pending() && !a_value_map->value_type->is_custom())
        {
            ast_type* decide_map_key_type = ast_type::create_type_at(a_value_map, L"anything");
            ast_type* decide_map_val_type = ast_type::create_type_at(a_value_map, L"anything");

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
                    decide_map_key_type = nullptr;
                    decide_map_val_type = nullptr;
                }
            }
            while (map_pair)
            {
                if (map_pair->key->value_type->is_pending() || map_pair->val->value_type->is_pending())
                {
                    decide_map_key_type = nullptr;
                    decide_map_val_type = nullptr;
                    break;
                }
                if (!decide_map_key_type->accept_type(map_pair->key->value_type, false))
                {
                    auto* mixed_type = ast_value_binary::binary_upper_type(decide_map_key_type, map_pair->key->value_type);
                    if (mixed_type)
                    {
                        decide_map_key_type->set_type_with_name(mixed_type->type_name);
                    }
                    else
                    {
                        decide_map_key_type->set_type_with_name(L"dynamic");
                        // lang_anylizer->lang_warning(0x0000, a_ret, WO_WARN_FUNC_WILL_RETURN_DYNAMIC);
                    }
                }
                if (!decide_map_val_type->accept_type(map_pair->val->value_type, false))
                {
                    auto* mixed_type = ast_value_binary::binary_upper_type(decide_map_val_type, map_pair->val->value_type);
                    if (mixed_type)
                    {
                        decide_map_val_type->set_type_with_name(mixed_type->type_name);
                    }
                    else
                    {
                        decide_map_val_type->set_type_with_name(L"dynamic");
                        // lang_anylizer->lang_warning(0x0000, a_ret, WO_WARN_FUNC_WILL_RETURN_DYNAMIC);
                    }
                }
                map_pair = dynamic_cast<ast_mapping_pair*>(map_pair->sibling);
            }

            if (decide_map_key_type && decide_map_val_type)
            {
                a_value_map->value_type->template_arguments[0] = decide_map_key_type;
                a_value_map->value_type->template_arguments[1] = decide_map_val_type;
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
            if (a_ret->return_value)
            {
                analyze_pass1(a_ret->return_value);

                // NOTE: DONOT JUDGE FUNCTION'S RETURN VAL TYPE IN PASS1 TO AVOID TYPE MIXED IN CONSTEXPR IF

                if (located_function_scope->function_node->auto_adjust_return_type)
                {
                    if (a_ret->return_value->value_type->is_pending() == false)
                    {
                        auto* func_return_type = located_function_scope->function_node->value_type->get_return_type();

                        if (func_return_type->is_pending())
                        {
                            located_function_scope->function_node->value_type->set_ret_type(a_ret->return_value->value_type);
                        }
                        else
                        {
                            if (!func_return_type->accept_type(a_ret->return_value->value_type, false))
                            {
                                auto* mixed_type = ast_value_binary::binary_upper_type(func_return_type, a_ret->return_value->value_type);
                                if (mixed_type)
                                {
                                    located_function_scope->function_node->value_type->set_type_with_name(mixed_type->type_name);
                                }
                                else
                                {
                                    located_function_scope->function_node->value_type->set_type_with_name(L"dynamic");
                                    lang_anylizer->lang_error(0x0000, a_ret, WO_ERR_FUNC_RETURN_DIFFERENT_TYPES);
                                }
                            }
                        }
                    }
                }

                a_ret->add_child(a_ret->return_value);
            }
            else
            {
                if (located_function_scope->function_node->auto_adjust_return_type)
                {
                    if (located_function_scope->function_node->value_type->is_pending())
                    {
                        located_function_scope->function_node->value_type->set_type_with_name(L"void");
                        located_function_scope->function_node->auto_adjust_return_type = false;
                    }
                    else
                    {
                        lang_anylizer->lang_error(0x0000, a_ret, WO_ERR_CANNOT_RET_TYPE_AND_TYPE_AT_SAME_TIME, L"void", located_function_scope->function_node->value_type->type_name.c_str());
                    }
                }
                else
                {
                    if (!located_function_scope->function_node->value_type->get_return_type()->is_void())
                        lang_anylizer->lang_error(0x0000, a_ret, WO_ERR_CANNOT_RET_TYPE_AND_TYPE_AT_SAME_TIME, L"void", located_function_scope->function_node->value_type->type_name.c_str());
                }
            }
        }
        return true;
    }
    WO_PASS1(ast_sentence_block)
    {
        auto* a_sentence_blk = WO_AST();
        this->begin_scope();
        analyze_pass1(a_sentence_blk->sentence_list);
        this->end_scope();

        return true;
    }
    WO_PASS1(ast_if)
    {
        auto* ast_if_sentence = WO_AST();
        analyze_pass1(ast_if_sentence->judgement_value);

        if (ast_if_sentence->judgement_value->is_constant
            && in_function()
            && in_function()->function_node->is_template_reification)
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
        analyze_pass1(ast_while_sentence->judgement_value);
        analyze_pass1(ast_while_sentence->execute_sentence);
        return true;
    }
    WO_PASS1(ast_except)
    {
        auto* ast_except_sent = WO_AST();
        analyze_pass1(ast_except_sent->execute_sentence);
        return true;
    }
    WO_PASS1(ast_forloop)
    {
        auto* a_forloop = WO_AST();
        begin_scope();

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
        a_value_unary->add_child(a_value_unary->val);

        if (a_value_unary->operate == +lex_type::l_lnot)
            a_value_unary->value_type = ast_type::create_type_at(a_value_unary, L"bool");
        else if (!a_value_unary->val->value_type->is_pending())
        {
            a_value_unary->value_type = a_value_unary->val->value_type;
        }
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
        // now_scope()->used_namespace.push_back(a_using_namespace);
        fully_update_type(a_using_type_as->old_type, true);

        auto* typing_symb = define_type_in_this_scope(a_using_type_as, a_using_type_as->old_type, a_using_type_as->declear_attribute);
        typing_symb->apply_template_setting(a_using_type_as);
        return true;
    }
    WO_PASS1(ast_foreach)
    {
        auto* a_foreach = WO_AST();
        begin_scope();

        a_foreach->used_iter_define->copy_source_info(a_foreach);
        analyze_pass1(a_foreach->used_iter_define);

        a_foreach->used_vawo_defines->copy_source_info(a_foreach);
        analyze_pass1(a_foreach->used_vawo_defines);

        a_foreach->iterator_var->copy_source_info(a_foreach);
        analyze_pass1(a_foreach->iterator_var);

        a_foreach->iter_next_judge_expr->copy_source_info(a_foreach);
        analyze_pass1(a_foreach->iter_next_judge_expr);

        a_foreach->execute_sentences->copy_source_info(a_foreach);
        analyze_pass1(a_foreach->execute_sentences);

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
            if (namings->is_same(a_check_naming->naming_const, false))
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
        a_match->match_scope_in_pass = begin_scope();

        if (a_match->match_value->value_type->is_union()
            && !a_match->match_value->value_type->is_pending()
            && a_match->match_value->value_type->using_type_name)
        {
            wo_assert(a_match->match_value->value_type->using_type_name->symbol);

            ast_using_namespace* ast_using = new ast_using_namespace;
            ast_using->used_namespace_chain = a_match->match_value->value_type->using_type_name->scope_namespaces;
            ast_using->used_namespace_chain.push_back(a_match->match_value->value_type->using_type_name->type_name);
            ast_using->from_global_namespace = true;
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
        begin_scope();
        wo_assert(a_match_union_case->in_match);

        if (ast_pattern_union_value* a_pattern_union_value = dynamic_cast<ast_pattern_union_value*>(a_match_union_case->union_pattern))
        {
            // Cannot pass a_match_union_case->union_pattern by analyze_pass1, we will set template in pass2.
            if (!a_pattern_union_value->union_expr->search_from_global_namespace)
                a_pattern_union_value->union_expr->searching_begin_namespace_in_pass2 = now_scope();

            // Calc type in pass2, here just define the variable with ast_value_takeplace
            if (a_pattern_union_value->pattern_arg_in_union_may_nil)
            {
                a_match_union_case->take_place_value_may_nil = new ast_value_takeplace;

                if (auto* a_pattern_identifier = dynamic_cast<ast::ast_pattern_identifier*>(a_pattern_union_value->pattern_arg_in_union_may_nil))
                {
                    if (a_pattern_identifier->decl == identifier_decl::REF)
                        a_match_union_case->take_place_value_may_nil->as_ref = true;
                }
                else
                    a_match_union_case->take_place_value_may_nil->as_ref = true;

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
        std::vector<ast_type*> types;
        bool tuple_type_not_pending = true;
        auto* tuple_elems = a_value_make_tuple_instance->tuple_member_vals->children;
        while (tuple_elems)
        {
            ast_value* val = dynamic_cast<ast_value*>(tuple_elems);
            if (!val->value_type->is_pending())
                types.push_back(val->value_type);
            else
            {
                tuple_type_not_pending = false;
                break;
            }
            tuple_elems = tuple_elems->sibling;
        }
        if (tuple_type_not_pending)
        {
            a_value_make_tuple_instance->value_type = ast_type::create_type_at(a_value_make_tuple_instance, L"tuple");
            a_value_make_tuple_instance->value_type->template_arguments = types;
        }
        else
        {
            a_value_make_tuple_instance->value_type = ast_type::create_type_at(a_value_make_tuple_instance, L"pending");
        }
        return true;
    }
    WO_PASS1(ast_struct_member_define)
    {
        auto* a_struct_member_define = WO_AST();
        analyze_pass1(a_struct_member_define->member_val_or_type_tkplace);

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

        if (a_value_trib_expr->val_if_true->value_type->is_same(
            a_value_trib_expr->val_or->value_type, false))
            a_value_trib_expr->value_type = a_value_trib_expr->val_if_true->value_type;
        else
            a_value_trib_expr->value_type = ast_type::create_type_at(a_value_trib_expr, L"pending");

        return true;
    }
}