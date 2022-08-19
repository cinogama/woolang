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
                        if (!func_return_type->accept_type(a_ret->return_value->value_type, false))
                        {
                            auto* mixed_type = ast_value_binary::binary_upper_type(func_return_type, a_ret->return_value->value_type);
                            if (mixed_type)
                            {
                                a_ret->located_function->value_type->set_type_with_name(mixed_type->type_name);
                            }
                            else
                            {
                                a_ret->located_function->value_type->set_type_with_name(L"dynamic");
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
                    a_ret->located_function->value_type->set_type_with_name(L"void");
                    a_ret->located_function->auto_adjust_return_type = false;
                }
                else
                {
                    lang_anylizer->lang_error(0x0000, a_ret, WO_ERR_CANNOT_RET_TYPE_AND_TYPE_AT_SAME_TIME, L"void", a_ret->located_function->value_type->type_name.c_str());
                }
            }
            else
            {
                if (!a_ret->located_function->value_type->get_return_type()->is_void())
                    lang_anylizer->lang_error(0x0000, a_ret, WO_ERR_CANNOT_RET_TYPE_AND_TYPE_AT_SAME_TIME, L"void", a_ret->located_function->value_type->type_name.c_str());
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

        if (ast_if_sentence->is_constexpr_if)
        {
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
        analyze_pass2(ast_while_sentence->judgement_value);
        analyze_pass2(ast_while_sentence->execute_sentence);

        return true;
    }
    WO_PASS2(ast_except)
    {
        auto* ast_except_sent = WO_AST();
        analyze_pass2(ast_except_sent->execute_sentence);

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
        /*
        for (auto& variable : a_foreach->foreach_var)
            analyze_pass2(variable);
        */

        lang_anylizer->begin_trying_block();
        analyze_pass2(a_foreach->iterator_var);
        analyze_pass2(a_foreach->iter_next_judge_expr->directed_value_from);

        // Try Getting next Function

        ast_value_variable* next_func_symb_getter = new ast_value_variable(L"next");
        next_func_symb_getter->copy_source_info(a_foreach);
        next_func_symb_getter->searching_from_type = a_foreach->iter_next_judge_expr->directed_value_from->value_type;
        analyze_pass2(next_func_symb_getter);

        if (next_func_symb_getter->symbol
            && next_func_symb_getter->symbol->type != lang_symbol::symbol_type::type_alias
            && next_func_symb_getter->symbol->type != lang_symbol::symbol_type::typing)
        {
            ast_type* _next_executer_type = next_func_symb_getter->symbol
                ->variable_value->value_type;

            if (!next_func_symb_getter->symbol->function_overload_sets.empty())
            {
                auto* next_function = next_func_symb_getter->symbol->function_overload_sets.front();

                // TODO: Check if private 'next' function?

                _next_executer_type =
                    next_function->value_type;
            }

            int need_takeplace_count = (int)_next_executer_type->argument_types.size();
            need_takeplace_count -= (int)a_foreach->used_vawo_defines->var_refs.size();
            need_takeplace_count -= 1;//iter


            lang_anylizer->set_eror_at(1);
            if (need_takeplace_count < 0)
                lang_anylizer->lang_error(0x0000, a_foreach, WO_ERR_TOO_MANY_ITER_ITEM_FROM_NEXT
                    , a_foreach->iter_next_judge_expr->directed_value_from->value_type
                    ->get_type_name(false).c_str()
                    , a_foreach->used_vawo_defines->var_refs.size());
            lang_anylizer->set_eror_at(0);

            wo_assert(nullptr == a_foreach->iter_next_judge_expr->arguments->children);

            // Make it fast over..
            a_foreach->iter_next_judge_expr->arguments->append_at_end(a_foreach->iterator_var);
            for (size_t i = 1; i < _next_executer_type->argument_types.size(); i++)
            {
                a_foreach->iter_next_judge_expr->arguments->append_at_end(new ast_value_takeplace);
            }
            analyze_pass2(a_foreach->iter_next_judge_expr);
            a_foreach->iter_next_judge_expr->completed_in_pass2 = false;
            a_foreach->iter_next_judge_expr->arguments->remove_allnode();
            a_foreach->iterator_var->parent = nullptr;
            a_foreach->iterator_var->sibling = nullptr;

            lang_anylizer->set_eror_at(1);
            if (a_foreach->iter_next_judge_expr->called_func->value_type
                ->is_variadic_function_type)
            {
                lang_anylizer->lang_error(0x0000, a_foreach, WO_ERR_VARIADIC_NEXT_IS_ILEAGAL,
                    a_foreach->iter_next_judge_expr->directed_value_from->value_type
                    ->get_type_name(false).c_str());
            }
            lang_anylizer->set_eror_at(0);

            int rndidx = (int)a_foreach->used_vawo_defines->var_refs.size() - 1;
            a_foreach->foreach_patterns_vars_in_pass2.resize(rndidx + 1);
            for (auto ridx = a_foreach->iter_next_judge_expr->called_func->value_type->argument_types.rbegin();
                ridx != a_foreach->iter_next_judge_expr->called_func->value_type->argument_types.rend();
                ridx++)
            {
                auto* tkplaceval = a_foreach->foreach_patterns_vars_in_pass2[rndidx] =
                    dynamic_cast<ast_value_takeplace*>(a_foreach->used_vawo_defines->var_refs[rndidx].init_val);

                wo_assert(tkplaceval);
                tkplaceval->is_mark_as_using_ref = true;
                tkplaceval->completed_in_pass2 = false;
                tkplaceval->value_type->set_type(*ridx);

                rndidx--;

                if (rndidx < 0)
                    break;
            }

            wo_assert(nullptr == a_foreach->iter_next_judge_expr->arguments->children);

            a_foreach->iter_next_judge_expr->arguments->append_at_end(a_foreach->iterator_var);
            for (int i = 0; i < need_takeplace_count; i++)
                a_foreach->iter_next_judge_expr->arguments->append_at_end(new ast_value_takeplace);
            for (auto& variable : a_foreach->foreach_patterns_vars_in_pass2)
                // WARNING! Variable foreach_patterns_vars_in_pass should same as which in
                //          a_foreach->iter_next_judge_expr->arguments. it will used in finalize
                a_foreach->iter_next_judge_expr->arguments->append_at_end(variable);

            analyze_pass2(a_foreach->used_vawo_defines);
        }
        else
        {
            // Do not find symbol, but do nothing.. error has been reported by analyze_pass2
        }
        lang_anylizer->end_trying_block();

        analyze_pass2(a_foreach->iter_next_judge_expr);
        analyze_pass2(a_foreach->execute_sentences);

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

        for (auto& namings : a_check_naming->template_type->template_impl_naming_checking)
        {
            if (a_check_naming->naming_const->is_pending() || namings->is_pending())
                break;
            if (namings->is_same(a_check_naming->naming_const, false))
                goto checking_naming_end;
        }
        lang_anylizer->lang_error(0x0000, a_check_naming, L"泛型参数'%ls'没有具名'%ls'约束，继续",
            a_check_naming->template_type->get_type_name(false).c_str(),
            a_check_naming->naming_const->get_type_name(false).c_str());
    checking_naming_end:;

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
                now_scope()->used_namespace.push_back(ast_using);
                a_match->has_using_namespace = true;
            }
        }

        analyze_pass2(a_match->cases);

        // Must walk all possiable case, and no repeat case!
        std::set<std::wstring> case_names;
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
                                lang_anylizer->lang_error(0x0000, a_pattern_union_value, L"'%ls' 不是 '%ls' 的合法项，继续",
                                    a_pattern_union_value->union_expr->symbol->name.c_str(),
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
                                    if (a_pattern_union_value->union_expr->symbol->function_overload_sets.size() == 1)
                                    {
                                        auto final_function = a_pattern_union_value->union_expr->symbol->function_overload_sets.front();

                                        auto* dumped_func = analyze_pass_template_reification(
                                            dynamic_cast<ast_value_function_define*>(final_function),
                                            fact_used_template);
                                        if (dumped_func)
                                            a_pattern_union_value->union_expr->symbol = dumped_func->this_reification_lang_symbol;
                                        else
                                            lang_anylizer->lang_error(0x0000, a_pattern_union_value, WO_ERR_NO_MATCHED_TEMPLATE_FUNC);
                                    }
                                    else
                                        lang_anylizer->lang_error(0x0000, a_pattern_union_value, WO_ERR_UNABLE_DECIDE_FUNC_OVERRIDE);
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
                        lang_anylizer->lang_error(0x0000, a_match_union_case, WO_ERR_INVALID_CASE_TYPE_NO_ARG_RECV);
                    else
                        a_match_union_case->take_place_value_may_nil->value_type->set_type(a_pattern_union_value->union_expr->value_type->argument_types.front());

                    analyze_pattern_in_pass2(a_pattern_union_value->pattern_arg_in_union_may_nil, a_match_union_case->take_place_value_may_nil);

                }
                else
                {
                    if (a_pattern_union_value->union_expr->value_type->argument_types.size() != 0)
                        lang_anylizer->lang_error(0x0000, a_match_union_case, WO_ERR_INVALID_CASE_TYPE_NEED_ACCEPT_ARG);
                }
            }
            else
                lang_anylizer->lang_error(0x0000, a_match_union_case, WO_ERR_UNEXPECT_PATTERN_CASE);

            analyze_pass2(a_match_union_case->in_case_sentence);
        }

        return true;
    }
    WO_PASS2(ast_struct_member_define)
    {
        auto* a_struct_member_define = WO_AST();
        analyze_pass2(a_struct_member_define->member_val_or_type_tkplace);

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
                    lang_anylizer->make_error(0x0000, val, L"约束项存在语法错误，以下是错误内容，继续："));
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
                        lang_anylizer->make_error(0x0000, val, L"约束项必须得出一个常量结果，继续"));
                    a_where_constraint->accept = false;
                }
                else if (!val->value_type->is_bool())
                {
                    a_where_constraint->unmatched_constraint.push_back(
                        lang_anylizer->make_error(0x0000, val, L"约束项得出的结果类型应该是 'bool'，但此处是 '%ls'，继续"
                            , val->value_type->get_type_name(false).c_str()));
                    a_where_constraint->accept = false;
                }
                else if (0 == val->get_constant_value().handle)
                {
                    a_where_constraint->unmatched_constraint.push_back(
                        lang_anylizer->make_error(0x0000, val, L"检查发现不满足的条件，继续"));
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
                if (a_value_funcdef->value_type->type_name == L"pending")
                {
                    // There is no return in function  return void
                    if (a_value_funcdef->auto_adjust_return_type)
                    {
                        if (a_value_funcdef->has_return_value)
                            lang_anylizer->lang_error(0x0000, a_value_funcdef, WO_ERR_CANNOT_DERIV_FUNCS_RET_TYPE, wo::str_to_wstr(a_value_funcdef->get_ir_func_signature_tag()).c_str());

                        a_value_funcdef->value_type->set_type_with_name(L"void");
                    }
                }
            }
            else
                a_value_funcdef->value_type->set_type_with_name(L"pending");
        }
        return true;
    }
    WO_PASS2(ast_value_assign)
    {
        auto* a_value_assi = WO_AST();
        analyze_pass2(a_value_assi->left);
        analyze_pass2(a_value_assi->right);

        if (a_value_assi->right->value_type->is_pending_function())
        {
            // Function assign, auto find overload? no! type must be case by user
            if (a_value_assi->left->value_type->is_func())
                lang_anylizer->lang_error(0x0000, a_value_assi, WO_ERR_UNABLE_DECIDE_FUNC_OVERRIDE, a_value_assi->left->value_type->get_type_name(false));
            else
                lang_anylizer->lang_error(0x0000, a_value_assi, WO_ERR_UNABLE_DECIDE_FUNC_SYMBOL);
        }

        if (!a_value_assi->left->value_type->accept_type(a_value_assi->right->value_type, false))
        {
            lang_anylizer->lang_error(0x0000, a_value_assi, WO_ERR_CANNOT_ASSIGN_TYPE_TO_TYPE,
                a_value_assi->right->value_type->get_type_name(false).c_str(),
                a_value_assi->left->value_type->get_type_name(false).c_str());
        }

        if (!a_value_assi->left->can_be_assign)
        {
            lang_anylizer->lang_error(0x0000, a_value_assi, WO_ERR_CANNOT_ASSIGN_TO_UNASSABLE_ITEM);
        }
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

        if (auto* a_variable_sym = dynamic_cast<ast_value_variable*>(origin_value);
            a_variable_sym && a_variable_sym->value_type->is_pending_function())
        {
            // this function is in adjust..
            if (a_value_typecast->value_type->is_func())
            {
                auto& func_symbol = a_variable_sym->symbol->function_overload_sets;
                if (func_symbol.size())
                {
                    for (auto func_overload : func_symbol)
                    {
                        if (!check_symbol_is_accessable(func_overload, func_overload->symbol, a_variable_sym->searching_begin_namespace_in_pass2, a_variable_sym, false))
                            continue; // In function override judge, not accessable function will be skip!

                        auto* overload_func = dynamic_cast<ast_value_function_define*>(func_overload);
                        if (overload_func->value_type->is_same(a_value_typecast->value_type, false))
                        {
                            a_value_typecast->_be_cast_value_node = overload_func;
                            break;
                        }
                    }
                }
                else
                {
                    // symbol is not a function-symbol, can not do adjust, goto simple-cast;
                    goto just_do_simple_type_cast;
                }
            }
            if (a_value_typecast->_be_cast_value_node->value_type->is_pending())
            {
                lang_anylizer->lang_error(0x0000, a_value_typecast, WO_ERR_CANNOT_GET_FUNC_OVERRIDE_WITH_TYPE,
                    a_variable_sym->var_name.c_str(),
                    a_value_typecast->value_type->get_type_name().c_str());
            }
        }
        else
        {
        just_do_simple_type_cast:
            if (!ast_type::check_castable(a_value_typecast->value_type, origin_value->value_type))
            {
                lang_anylizer->lang_error(0x0000, a_value_typecast, WO_ERR_CANNOT_CAST_TYPE_TO_TYPE,
                    origin_value->value_type->get_type_name(false).c_str(),
                    a_value_typecast->value_type->get_type_name(false).c_str()
                );
                a_value_typecast->value_type = ast_type::create_type_at(a_value_typecast, L"pending");
            }
        }
        return true;
    }
    WO_PASS2(ast_value_type_judge)
    {
        auto* ast_value_judge = WO_AST();
        if (ast_value_judge->is_mark_as_using_ref)
            ast_value_judge->_be_cast_value_node->is_mark_as_using_ref = true;

        analyze_pass2(ast_value_judge->_be_cast_value_node);
        return true;
    }
    WO_PASS2(ast_value_type_check)
    {
        auto* a_value_type_check = WO_AST();
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
                            str_to_wstr(*a_value_index->index->get_constant_value().string)
                        ); fnd != a_value_index->from->value_type->struct_member_index.end())
                    {
                        if (fnd->second.init_value_may_nil)
                        {
                            if (!fnd->second.init_value_may_nil->value_type->is_pending())
                            {
                                a_value_index->value_type = fnd->second.init_value_may_nil->value_type;
                                a_value_index->struct_offset = fnd->second.offset;
                            }
                        }
                        else
                            lang_anylizer->lang_error(0x0000, a_value_index, WO_ERR_UNDEFINED_MEMBER,
                                str_to_wstr(*a_value_index->index->get_constant_value().string).c_str());
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
            else if (a_value_index->from->value_type->is_tuple())
            {
                if (a_value_index->index->is_constant && a_value_index->index->value_type->is_integer())
                {
                    // Index tuple must with constant integer.
                    auto index = a_value_index->index->get_constant_value().integer;
                    if ((size_t)index < a_value_index->from->value_type->template_arguments.size() && index >= 0)
                    {
                        a_value_index->value_type = a_value_index->from->value_type->template_arguments[index];
                        a_value_index->struct_offset = (uint16_t)index;
                    }
                    else
                        lang_anylizer->lang_error(0x0000, a_value_index, L"对元组的索引超出范围（元组包含 %d 项，而正在尝试索引 %d 项），继续",
                            (int)a_value_index->from->value_type->template_arguments.size(), (int)index);
                }
                else
                {
                    lang_anylizer->lang_error(0x0000, a_value_index, L"只允许使用 'int' 类型的常量索引元组，继续");
                }
            }
            else if (a_value_index->from->value_type->is_string())
            {
                a_value_index->value_type = ast_type::create_type_at(a_value_index, L"string");
            }
            else if (!a_value_index->from->value_type->is_pending())
            {
                if (a_value_index->from->value_type->is_array())
                {
                    a_value_index->value_type = a_value_index->from->value_type->template_arguments[0];
                }
                else if (a_value_index->from->value_type->is_map())
                {
                    a_value_index->value_type = a_value_index->from->value_type->template_arguments[1];
                }
                else
                {
                    a_value_index->value_type = ast_type::create_type_at(a_value_index, L"dynamic");
                }
                if ((a_value_index->is_const_value = a_value_index->from->is_const_value))
                    a_value_index->can_be_assign = false;
            }
        }

        if (!a_value_index->from->value_type->is_array()
            && !a_value_index->from->value_type->is_map()
            && !a_value_index->from->value_type->is_string()
            && !a_value_index->from->value_type->is_struct()
            && !a_value_index->from->value_type->is_tuple())
        {
            lang_anylizer->lang_error(0x0000, a_value_index->from, WO_ERR_UNINDEXABLE_TYPE
                , a_value_index->from->value_type->get_type_name().c_str());
        }

        if (a_value_index->from->value_type->is_array() || a_value_index->from->value_type->is_string())
        {
            if (!a_value_index->index->value_type->is_integer())
            {
                lang_anylizer->lang_error(0x0000, a_value_index, L"'%ls' 的索引只能是 'int' 类型的值，继续"
                    , a_value_index->from->value_type->get_type_name().c_str());
            }
        }
        if (a_value_index->from->value_type->is_map())
        {
            if (!a_value_index->index->value_type->is_same(a_value_index->from->value_type->template_arguments[0], false))
            {
                lang_anylizer->lang_error(0x0000, a_value_index, L"'%ls' 的索引只能是 '%ls' 类型的值，继续"
                    , a_value_index->from->value_type->get_type_name().c_str()
                    , a_value_index->from->value_type->template_arguments[0]->get_type_name(false).c_str());
            }
        }

        if (!a_value_index->from->value_type->is_string())
            if ((a_value_index->is_const_value = a_value_index->from->is_const_value))
                a_value_index->can_be_assign = false;
        return true;
    }
    WO_PASS2(ast_value_indexed_variadic_args)
    {
        auto* a_value_variadic_args_idx = WO_AST();
        analyze_pass2(a_value_variadic_args_idx->argindex);

        if (!a_value_variadic_args_idx->argindex->value_type->is_integer())
        {
            lang_anylizer->lang_error(0x0000, a_value_variadic_args_idx, L"'变长参数包' 的索引只能是 'int' 类型的值，继续");
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
            lang_anylizer->lang_error(0x0000, a_fakevalue_unpacked_args, WO_ERR_NEED_TYPES, L"array" WO_TERM_OR L"tuple");
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

                a_value_bin->value_type = ast_value_binary::binary_upper_type_with_operator(
                    a_value_bin->left->value_type,
                    a_value_bin->right->value_type,
                    a_value_bin->operate
                );
            }
            else
            {
                // Apply this type to func
                if (nullptr == a_value_bin->value_type)
                    a_value_bin->value_type = a_value_bin->overrided_operation_call->value_type;
                else if (a_value_bin->value_type->is_pending())
                    a_value_bin->value_type->set_type(a_value_bin->overrided_operation_call->value_type);
                else if (!a_value_bin->value_type->is_same(a_value_bin->overrided_operation_call->value_type, false))
                    lang_anylizer->lang_error(0x0000, a_value_bin, L"无法兼容重置运算操作和原始运算类型，这可能导致类型推导错误，继续");
            }

        }

        if (nullptr == a_value_bin->value_type)
        {
            lang_anylizer->lang_error(0x0000, a_value_bin, WO_ERR_CANNOT_CALC_WITH_L_AND_R,
                a_value_bin->left->value_type->get_type_name(false).c_str(),
                a_value_bin->right->value_type->get_type_name(false).c_str());
            a_value_bin->value_type = ast_type::create_type_at(a_value_bin, L"pending");
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
                if (nullptr == a_value_logic_bin->value_type)
                    a_value_logic_bin->value_type = a_value_logic_bin->overrided_operation_call->value_type;
                else if (a_value_logic_bin->value_type->is_pending())
                    a_value_logic_bin->value_type->set_type(a_value_logic_bin->overrided_operation_call->value_type);
                else if (!a_value_logic_bin->value_type->is_same(a_value_logic_bin->overrided_operation_call->value_type, false))
                    lang_anylizer->lang_error(0x0000, a_value_logic_bin, L"无法兼容重置运算操作和原始运算类型，这可能导致类型推导错误，继续");
            }

        }
        if (!a_value_logic_bin->value_type || a_value_logic_bin->value_type->is_pending())
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
                    && a_value_logic_bin->left->value_type->is_same(a_value_logic_bin->right->value_type, false))
                    type_ok = true;
            }

            if (!type_ok)
                lang_anylizer->lang_error(0x0000, a_value_logic_bin, WO_ERR_CANNOT_CALC_WITH_L_AND_R,
                    a_value_logic_bin->left->value_type->get_type_name(false).c_str(),
                    a_value_logic_bin->right->value_type->get_type_name(false).c_str());
            else
                a_value_logic_bin->value_type = ast_type::create_type_at(a_value_logic_bin, L"bool");
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
                    lang_anylizer->lang_error(0x0000, val, L"'array' 序列中的值类型不一致，无法为 'array' 推导类型，继续");
                    break;
                }
                val = dynamic_cast<ast_value*>(val->sibling);
            }

            if (decide_array_item_type)
            {
                a_value_arr->value_type->template_arguments[0] = decide_array_item_type;
            }
        }

        if (!a_value_arr->value_type->is_array())
        {
            lang_anylizer->lang_error(0x0000, a_value_arr, WO_ERR_CANNOT_CAST_TYPE_TO_TYPE,
                L"array",
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
                    lang_anylizer->lang_error(0x0000, val, L"'array' 序列中的值类型与泛型参数中指定的不一致，继续");
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
                    lang_anylizer->lang_error(0x0000, map_pair->key, L"'map' 序列中的键类型不一致，无法为 'map' 推导类型，继续");
                    break;
                }
                if (!decide_map_val_type->accept_type(map_pair->val->value_type, false))
                {
                    lang_anylizer->lang_error(0x0000, map_pair->val, L"'map' 序列中的值类型不一致，无法为 'map' 推导类型，继续");
                    break;
                }
                map_pair = dynamic_cast<ast_mapping_pair*>(map_pair->sibling);
            }

            if (decide_map_key_type && decide_map_val_type)
            {
                a_value_map->value_type->template_arguments[0] = decide_map_key_type;
                a_value_map->value_type->template_arguments[1] = decide_map_val_type;
            }
        }

        if (!a_value_map->value_type->is_map())
        {
            lang_anylizer->lang_error(0x0000, a_value_map, WO_ERR_CANNOT_CAST_TYPE_TO_TYPE,
                L"map",
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
                        lang_anylizer->lang_error(0x0000, pairs->key, L"'map' 序列中的键类型与泛型参数中指定的不一致，继续");
                    if (!a_value_map->value_type->template_arguments[1]->accept_type(pairs->val->value_type, false))
                        lang_anylizer->lang_error(0x0000, pairs->val, L"'map' 序列中的值类型与泛型参数中指定的不一致，继续");

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
            lang_anylizer->lang_error(0x0000, a_value_make_tuple_instance, L"元组元素类型未决，因此无法推导元组类型，继续");
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
                        membpair->member_offset = fnd->second.offset;
                        fully_update_type(fnd->second.init_value_may_nil->value_type, false);
                        if (!fnd->second.init_value_may_nil->value_type->accept_type(membpair->member_val_or_type_tkplace->value_type, false))
                        {
                            lang_anylizer->lang_error(0x0000, membpair, L"成员 '%ls' 的类型为 '%ls'，但给定的初始值类型为 '%ls'，继续"
                                , membpair->member_name.c_str()
                                , fnd->second.init_value_may_nil->value_type->get_type_name(false).c_str()
                                , membpair->member_val_or_type_tkplace->value_type->get_type_name(false).c_str());
                        }
                    }
                    else
                        lang_anylizer->lang_error(0x0000, membpair, WO_ERR_THERE_IS_NO_MEMBER_NAMED,
                            a_value_make_struct_instance->value_type->get_type_name(false).c_str(),
                            membpair->member_name.c_str());
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
            lang_anylizer->lang_error(0x0000, a_value_trib_expr, L"条件表达式的判断表达式应该是bool类型，但此处是 '%ls'，继续"
                , a_value_trib_expr->judge_expr->value_type->get_type_name(false).c_str());
        }

        a_value_trib_expr->judge_expr->update_constant_value(lang_anylizer);

        if (a_value_trib_expr->judge_expr->is_constant)
        {
            if (a_value_trib_expr->judge_expr->get_constant_value().integer)
            {
                analyze_pass2(a_value_trib_expr->val_if_true);
                a_value_trib_expr->value_type = a_value_trib_expr->val_if_true->value_type;
            }
            else
            {
                analyze_pass2(a_value_trib_expr->val_or);
                a_value_trib_expr->value_type = a_value_trib_expr->val_or->value_type;
            }
        }
        else
        {
            analyze_pass2(a_value_trib_expr->val_if_true);
            analyze_pass2(a_value_trib_expr->val_or);

            if (a_value_trib_expr->val_if_true->value_type->is_same(
                a_value_trib_expr->val_or->value_type, false))
            {
                a_value_trib_expr->value_type = a_value_trib_expr->val_if_true->value_type;
            }
            else
            {
                lang_anylizer->lang_error(0x0000, a_value_trib_expr, L"条件表达式的不同分支的值应该有相同的类型，但此处分别是 '%ls' 和 '%ls'，继续"
                    , a_value_trib_expr->val_if_true->value_type->get_type_name(false).c_str()
                    , a_value_trib_expr->val_or->value_type->get_type_name(false).c_str());
            }
        }
        return true;
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
                    lang_anylizer->lang_error(0x0000, a_value_var, WO_ERR_UNKNOWN_IDENTIFIER, a_value_var->var_name.c_str());

                if (sym->is_template_symbol && (!a_value_var->is_auto_judge_function_overload || sym->type == lang_symbol::symbol_type::variable))
                {
                    sym = analyze_pass_template_reification(a_value_var, a_value_var->template_reification_args);
                    if (!sym)
                        lang_anylizer->lang_error(0x0000, a_value_var, L"具体化泛型标识符 '%ls' 时失败，继续", a_value_var->var_name.c_str());
                }

                if (sym)
                {
                    analyze_pass2(sym->variable_value);
                    a_value_var->value_type = sym->variable_value->value_type;
                    a_value_var->symbol = sym;

                    if (a_value_var->value_type->is_pending())
                    {
                        if (a_value_var->symbol->type != lang_symbol::symbol_type::function)
                            lang_anylizer->lang_error(0x0000, a_value_var, WO_ERR_UNABLE_DECIDE_VAR_TYPE);
                        else if (a_value_var->symbol->function_overload_sets.size() == 1
                            && !a_value_var->symbol->is_template_symbol
                            && a_value_var->template_reification_args.empty())
                        {
                            // only you~
                            auto* result = sym->function_overload_sets.front();
                            check_symbol_is_accessable(result, result->symbol, a_value_var->searching_begin_namespace_in_pass2, a_value_var);
                            analyze_pass2(result);
                            a_value_var->value_type = result->value_type;

                            if (a_value_var->value_type->is_pending())
                                lang_anylizer->lang_error(0x0000, a_value_var, WO_ERR_CANNOT_DERIV_FUNCS_RET_TYPE, a_value_var->var_name.c_str());
                        }
                        else
                        {
                            // NOTE: A FUNCTION CALL MAY RUN HERE, SO WE DONOT REPORT ANY ERROR. 
                            /*if (a_value_var->symbol->is_template_symbol || !a_value_var->template_reification_args.empty())
                                lang_anylizer->lang_error(0x0000, a_value_var, L"给定的函数是一个泛型函数，需要指定泛型参数，继续");
                            else
                                lang_anylizer->lang_error(0x0000, a_value_var, L"给定的函数拥有多个重载，需要指定需要使用的重载函数，继续");*/
                        }
                    }
                }
            }
            else
            {
                lang_anylizer->lang_error(0x0000, a_value_var, WO_ERR_UNKNOWN_IDENTIFIER, a_value_var->var_name.c_str());
                a_value_var->value_type = ast_type::create_type_at(a_value_var, L"pending");
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
            a_value_unary->value_type = ast_type::create_type_at(a_value_unary, L"bool");
            fully_update_type(a_value_unary->value_type, false);
        }
        else if (!a_value_unary->val->value_type->is_pending())
            a_value_unary->value_type = a_value_unary->val->value_type;
        // else
            // not need to manage, if val is pending, other place will give error.


        return true;
    }

    WO_PASS2(ast_value_funccall)
    {
        auto* a_value_funccall = WO_AST();

        if (a_value_funccall->value_type->is_pending())

        {
            if (a_value_funccall->callee_symbol_in_type_namespace)
            {
                analyze_pass2(a_value_funccall->directed_value_from);
                if (!a_value_funccall->directed_value_from->value_type->is_pending() &&
                    !a_value_funccall->directed_value_from->value_type->is_func())
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

                        lang_anylizer->begin_trying_block();
                        analyze_pass2(a_value_funccall->callee_symbol_in_type_namespace);
                        lang_anylizer->end_trying_block();

                        if (!a_value_funccall->callee_symbol_in_type_namespace->value_type->is_pending()
                            || a_value_funccall->callee_symbol_in_type_namespace->value_type->is_pending_function()
                            || (a_value_funccall->callee_symbol_in_type_namespace->symbol
                                && a_value_funccall->callee_symbol_in_type_namespace->symbol->type == lang_symbol::symbol_type::function))
                        {
                            if (auto* old_callee_func = dynamic_cast<ast::ast_value_variable*>(a_value_funccall->called_func))
                                a_value_funccall->callee_symbol_in_type_namespace->template_reification_args
                                = old_callee_func->template_reification_args;

                            a_value_funccall->called_func = a_value_funccall->callee_symbol_in_type_namespace;
                            goto start_ast_op_calling;
                        }

                        a_value_funccall->callee_symbol_in_type_namespace->completed_in_pass2 = false;
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

                        lang_anylizer->begin_trying_block();
                        analyze_pass2(a_value_funccall->callee_symbol_in_type_namespace);
                        lang_anylizer->end_trying_block();


                        if (!a_value_funccall->callee_symbol_in_type_namespace->value_type->is_pending()
                            || a_value_funccall->callee_symbol_in_type_namespace->value_type->is_pending_function()
                            || (a_value_funccall->callee_symbol_in_type_namespace->symbol
                                && a_value_funccall->callee_symbol_in_type_namespace->symbol->type == lang_symbol::symbol_type::function))
                        {
                            if (auto* old_callee_func = dynamic_cast<ast::ast_value_variable*>(a_value_funccall->called_func))
                                a_value_funccall->callee_symbol_in_type_namespace->template_reification_args
                                = old_callee_func->template_reification_args;

                            a_value_funccall->called_func = a_value_funccall->callee_symbol_in_type_namespace;
                            goto start_ast_op_calling;
                        }
                    }

                    // End trying invoke from direct-type namespace
                }
            }
        start_ast_op_calling:

            analyze_pass2(a_value_funccall->called_func);
            analyze_pass2(a_value_funccall->arguments);

            // judge the function override..
            if (a_value_funccall->called_func->value_type->is_pending())
            {
                // function call for witch overrride not judge. do it.
                if (auto* called_funcsymb = dynamic_cast<ast_value_symbolable_base*>(a_value_funccall->called_func))
                {
                    if (nullptr == called_funcsymb->symbol)
                    {
                        // do nothing..
                    }
                    else if (!called_funcsymb->symbol->function_overload_sets.empty())
                    {

                        // have override set, judge with following rule:
                        // 1. best match <may be template>
                        // 2. need cast <may be template>
                        // 3. variadic func <may be template>
                        // -  bad match

                        std::vector<ast_value_function_define*> best_match_sets;
                        std::vector<ast_value_function_define*> best_match_sets_template;
                        std::vector<ast_value_function_define*> variadic_sets;
                        std::vector<ast_value_function_define*> variadic_sets_template;

                        std::vector<ast_value_function_define*> tried_function;

                        for (auto* _override_func : called_funcsymb->symbol->function_overload_sets)
                        {
                            if (!check_symbol_is_accessable(_override_func, _override_func->symbol, called_funcsymb->searching_begin_namespace_in_pass2, a_value_funccall, false))
                                continue; // In function override judge, not accessable function will be skip!

                            auto* override_func = dynamic_cast<ast_value_function_define*>(_override_func);
                            wo_test(override_func);

                            bool with_template = false;
                            grammar::ast_base* real_args = nullptr;
                            grammar::ast_base* form_args = nullptr;

                            if (override_func->is_template_define)
                            {
                                // try judge templates..
                                with_template = true;

                                std::vector<ast_type*> template_args(override_func->template_type_name_list.size(), nullptr);

                                if (auto* variable = dynamic_cast<ast_value_variable*>(a_value_funccall->called_func))
                                {
                                    if (variable->template_reification_args.size() > template_args.size())
                                        continue; // Give too many template arguments
                                    for (size_t index = 0; index < variable->template_reification_args.size(); index++)
                                    {
                                        template_args[index] = variable->template_reification_args[index];
                                    }
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

                                // begin auto template args
                                // FXXXXXXXXXXXXXXXXK!!!!!!
                                for (size_t tempindex = 0; tempindex < template_args.size(); tempindex++)
                                {
                                    auto& pending_template_arg = template_args[tempindex];

                                    if (!pending_template_arg)
                                    {
                                        for (size_t index = 0;
                                            index < real_argument_types.size() &&
                                            index < override_func->value_type->argument_types.size();
                                            index++)
                                        {

                                            fully_update_type(override_func->value_type->argument_types[index], false,
                                                override_func->template_type_name_list);
                                            //fully_update_type(real_argument_types[index], false); // USELESS

                                            pending_template_arg = analyze_template_derivation(
                                                override_func->template_type_name_list[tempindex],
                                                override_func->template_type_name_list,
                                                override_func->value_type->argument_types[index],
                                                real_argument_types[index]
                                            );

                                            if (pending_template_arg)
                                                break;
                                        }
                                    }
                                }

                                if (std::find(template_args.begin(), template_args.end(), nullptr) != template_args.end())
                                    continue; // failed getting each of template args, abandon this one

                                for (auto* templ_arg : template_args)
                                {
                                    fully_update_type(templ_arg, false);
                                    if (templ_arg->is_pending())
                                    {
                                        lang_anylizer->lang_error(0x0000, templ_arg, WO_ERR_UNKNOWN_TYPE,
                                            templ_arg->get_type_name(false).c_str());
                                        goto this_function_override_checking_over;
                                    }
                                }

                                override_func = analyze_pass_template_reification(override_func, template_args); //tara~ get analyze_pass_template_reification 
                                _override_func = override_func;
                            }
                            else if (auto callee_fun_variable = dynamic_cast<ast::ast_value_variable*>(a_value_funccall->called_func);
                                callee_fun_variable && !callee_fun_variable->template_reification_args.empty())
                            {
                                // force template call, go on..
                                continue;
                            }

                            lang_anylizer->begin_trying_block();
                            analyze_pass2(_override_func);
                            tried_function.push_back(_override_func);

                            if (!lang_anylizer->get_cur_error_frame().empty())
                            {
                                // error happend in func, abondon this overload.
                                if (_override_func->where_constraint == nullptr)
                                {
                                    _override_func->where_constraint = new ast_where_constraint;
                                    _override_func->where_constraint->copy_source_info(_override_func);
                                }

                                wo_assert(_override_func->where_constraint);
                                _override_func->where_constraint->accept = false;
                                _override_func->where_constraint->binded_func_define = _override_func;
                                _override_func->where_constraint->unmatched_constraint.insert(
                                    _override_func->where_constraint->unmatched_constraint.end(),
                                    lang_anylizer->get_cur_error_frame().begin(),
                                    lang_anylizer->get_cur_error_frame().end());
                            }
                            lang_anylizer->end_trying_block();

                            if (_override_func->where_constraint == nullptr
                                || _override_func->where_constraint->accept)
                            {
                                real_args = a_value_funccall->arguments->children;
                                form_args = override_func->argument_list->children;
                                do
                                {
                                    auto* form_arg = dynamic_cast<ast_value_arg_define*>(form_args);
                                    auto* real_arg = dynamic_cast<ast_value*>(real_args);

                                    wo_test(real_args == real_arg);

                                    if (!form_arg)
                                    {
                                        // variadic..
                                        if (nullptr == real_arg) // arg count match, just like best match/ need cast match
                                            goto match_check_end_for_variadic_func;
                                        else
                                        {
                                            if (with_template)
                                                variadic_sets_template.push_back(override_func);
                                            else
                                                variadic_sets.push_back(override_func);
                                        }

                                        goto this_function_override_checking_over;
                                    }

                                    if (!real_arg)
                                        goto this_function_override_checking_over;// real_args count didn't match, break..

                                    if (auto* a_fakevalue_unpack_args = dynamic_cast<ast_fakevalue_unpacked_args*>(real_arg))
                                    {
                                        auto ecount = a_fakevalue_unpack_args->expand_count;
                                        if (ast_fakevalue_unpacked_args::UNPACK_ALL_ARGUMENT == ecount)
                                        {
                                            if (a_fakevalue_unpack_args->unpacked_pack->value_type->is_tuple())
                                            {
                                                auto* unpacking_tuple_type = a_fakevalue_unpack_args->unpacked_pack->value_type;
                                                if (unpacking_tuple_type->using_type_name)
                                                    unpacking_tuple_type = unpacking_tuple_type->using_type_name;
                                                a_fakevalue_unpack_args->expand_count = ecount = unpacking_tuple_type->template_arguments.size();
                                            }
                                        }
                                        if (ast_fakevalue_unpacked_args::UNPACK_ALL_ARGUMENT == ecount)
                                        {
                                            // all in!!!
                                            if (with_template)
                                                variadic_sets.push_back(override_func);
                                            else
                                                variadic_sets_template.push_back(override_func);

                                            goto this_function_override_checking_over;
                                        }

                                        size_t unpack_type_index = 0; // Used for type check when unpack tuple.
                                        while (ecount)
                                        {
                                            if (form_arg)
                                            {
                                                if (a_fakevalue_unpack_args->unpacked_pack->value_type->is_tuple())
                                                {
                                                    // Varify tuple type here.
                                                    auto* unpacking_tuple_type = a_fakevalue_unpack_args->unpacked_pack->value_type;
                                                    if (unpacking_tuple_type->using_type_name)
                                                        unpacking_tuple_type = unpacking_tuple_type->using_type_name;

                                                    if (unpacking_tuple_type->template_arguments.size() <= unpack_type_index)
                                                        // There is no enough value for tuple to expand. match failed!
                                                        goto this_function_override_checking_over;
                                                    else if (!form_arg->value_type->accept_type(unpacking_tuple_type->template_arguments[unpack_type_index], false))
                                                        // Type didn't match, match failed!
                                                        goto this_function_override_checking_over;
                                                    else
                                                        // ok, do nothing.
                                                        ++unpack_type_index;
                                                }

                                                form_args = form_arg->sibling;
                                                form_arg = dynamic_cast<ast_value_arg_define*>(form_args);
                                            }
                                            else if (form_args)
                                            {
                                                // is variadic
                                                if (with_template)
                                                    variadic_sets_template.push_back(override_func);
                                                else
                                                    variadic_sets.push_back(override_func);
                                                goto this_function_override_checking_over;
                                            }
                                            else
                                            {
                                                // not match , over..
                                                goto this_function_override_checking_over;
                                            }
                                            ecount--;
                                        }

                                    }

                                    if (dynamic_cast<ast_value_takeplace*>(real_arg))
                                        ;
                                    else if (real_arg->value_type->is_pending() || form_arg->value_type->is_pending())
                                        break;
                                    else if (form_arg->value_type->accept_type(real_arg->value_type, false))
                                        ;// do nothing..
                                    else
                                        break; // bad match, break..


                                    real_args = real_args->sibling;
                                match_check_end_for_variadic_func:
                                    if (form_args)
                                        form_args = form_args->sibling;


                                    if (form_args == nullptr)
                                    {
                                        // finish match check, add it to set
                                        if (real_args == nullptr)
                                        {
                                            if (with_template)
                                                best_match_sets_template.push_back(override_func);
                                            else
                                                best_match_sets.push_back(override_func);
                                        }
                                        // else: bad match..
                                    }
                                } while (form_args);
                            }

                        this_function_override_checking_over:;

                        }

                        std::vector<ast_value_function_define*>* judge_sets = nullptr;
                        if (best_match_sets.size())
                            judge_sets = &best_match_sets;
                        else if (best_match_sets_template.size())
                            judge_sets = &best_match_sets_template;
                        else if (variadic_sets.size())
                            judge_sets = &variadic_sets;
                        else if (variadic_sets_template.size())
                            judge_sets = &variadic_sets_template;

                        if (judge_sets)
                        {
                            if (judge_sets->size() > 1)
                            {
                                std::wstring acceptable_func;
                                for (size_t index = 0; index < judge_sets->size(); index++)
                                {
                                    acceptable_func += L"'" + judge_sets->at(index)->function_name + L":"
                                        + judge_sets->at(index)->value_type->get_type_name(false)
                                        + L"' " WO_TERM_AT L" ("
                                        + std::to_wstring(judge_sets->at(index)->row_no)
                                        + L","
                                        + std::to_wstring(judge_sets->at(index)->col_no)
                                        + L")";

                                    if (index + 1 != judge_sets->size())
                                    {
                                        acceptable_func += L" " WO_TERM_OR L" ";
                                    }
                                }
                                this->lang_anylizer->lang_error(0x0000, a_value_funccall, WO_ERR_UNABLE_DECIDE_FUNC_OVERRIDE, acceptable_func.c_str());
                            }
                            else
                            {
                                a_value_funccall->called_func = judge_sets->front();
                                analyze_pass2(a_value_funccall->called_func);
                            }
                        }
                        else
                        {
                            this->lang_anylizer->lang_error(0x0000, a_value_funccall, WO_ERR_NO_MATCH_FUNC_OVERRIDE);
                            for (auto* tried_func : tried_function)
                            {
                                if (tried_func->where_constraint && !tried_func->where_constraint->accept)
                                {
                                    this->lang_anylizer->lang_error(0x0000, a_value_funccall, L"不满足函数的 'where' 约束要求，继续：");
                                    for (auto& error_info : tried_func->where_constraint->unmatched_constraint)
                                    {
                                        lang_anylizer->get_cur_error_frame().push_back(error_info);
                                    }
                                }
                                else
                                    this->lang_anylizer->lang_error(0x0000, a_value_funccall, WO_ERR_TYPE_CANNOT_BE_CALL,
                                        tried_func->value_type->get_type_name(false).c_str());
                            }
                        }
                    }
                }
            }

            if (!a_value_funccall->called_func->value_type->is_pending())
            {
                if (a_value_funccall->called_func->value_type->is_func())
                {
                    a_value_funccall->value_type = a_value_funccall->called_func->value_type->get_return_type();
                }
            }
            else
            {
                /*
                // for recurrence function callen, this check will cause lang error, just ignore the call type.
                // - if function's type can be judge, it will success outside.

                if(a_value_funccall->called_func->value_type->is_pending_function())
                    lang_anylizer->lang_error(0x0000, a_value, L"xxx '%s'.", a_value->value_type->get_type_name().c_str());
                */
            }

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

                    funcsymb->value_type->set_ret_type(ast_type::create_type_at(funcsymb, L"void"));
                    funcsymb->auto_adjust_return_type = false;
                }

            }

            bool failed_to_call_cur_func = false;

            if (a_value_funccall->called_func
                && a_value_funccall->called_func->value_type->is_func()
                && !a_value_funccall->called_func->value_type->is_pending())
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
                            while (ecount)
                            {
                                if (a_type_index != a_value_funccall->called_func->value_type->argument_types.end())
                                {
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
                            donot_move_forward = true;
                        }
                        else if (dynamic_cast<ast_value_takeplace*>(arg_val))
                        {
                            // Do nothing
                        }
                        else
                        {
                            if (!arg_val->value_type->is_pending() && !(*a_type_index)->accept_type(arg_val->value_type, false))
                            {
                                failed_to_call_cur_func = true;
                                lang_anylizer->lang_error(0x0000, a_value_funccall, WO_ERR_TYPE_CANNOT_BE_CALL, a_value_funccall->called_func->value_type->get_type_name(false).c_str());
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
            else if (!a_value_funccall->called_func->value_type->is_pending())
            {
                failed_to_call_cur_func = true;
                lang_anylizer->lang_error(0x0000, a_value_funccall, WO_ERR_TYPE_CANNOT_BE_CALL,
                    a_value_funccall->called_func->value_type->get_type_name(false).c_str());
            }

            if (failed_to_call_cur_func)
                a_value_funccall->value_type->set_type_with_name(L"pending");
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
}