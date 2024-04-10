#include "wo_lang.hpp"

WO_API wo_api rslib_std_bad_function(wo_vm vm, wo_value args);

namespace wo
{
    using namespace ast;

#define WO_AST() astnode; wo_assert(astnode != nullptr)

#define WO_PASS1(NODETYPE) void lang::pass1_##NODETYPE (ast::NODETYPE* astnode)

    WO_PASS1(ast_namespace)
    {
        auto* a_namespace = WO_AST();

        begin_namespace(a_namespace);
        if (a_namespace->in_scope_sentence != nullptr)
        {
            a_namespace->add_child(a_namespace->in_scope_sentence);
            analyze_pass1(a_namespace->in_scope_sentence);
        }
        end_namespace();
    }
    WO_PASS1(ast_varref_defines)
    {
        auto* a_varref_defs = WO_AST();
        a_varref_defs->located_function = in_function();
        for (auto& varref : a_varref_defs->var_refs)
        {
            analyze_pattern_in_pass1(varref.pattern, a_varref_defs->declear_attribute, varref.init_val);
        }
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
    }
    WO_PASS1(ast_value_mutable)
    {
        auto* a_value_mutable_or_pure = WO_AST();

        analyze_pass1(a_value_mutable_or_pure->val);

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
    WO_PASS1(ast_value_index)
    {
        auto* a_value_idx = WO_AST();

        analyze_pass1(a_value_idx->from);
        analyze_pass1(a_value_idx->index);

        if (!a_value_idx->from->value_type->struct_member_index.empty())
        {
            if (a_value_idx->index->is_constant && a_value_idx->index->value_type->is_string())
            {
                auto member_field_name = wstring_pool::get_pstr(
                    str_to_wstr(*a_value_idx->index->get_constant_value().string));

                if (auto fnd = a_value_idx->from->value_type->struct_member_index.find(member_field_name);
                    fnd != a_value_idx->from->value_type->struct_member_index.end())
                {
                    fully_update_type(fnd->second.member_type, true);

                    if (!fnd->second.member_type->is_pending())
                    {
                        a_value_idx->value_type->set_type(fnd->second.member_type);
                        a_value_idx->struct_offset = fnd->second.offset;
                    }

                    // Get symbol from type, if symbol not exist, it is anonymous structure.
                    // Anonymous structure's field is `public`.
                    lang_symbol* struct_symb = nullptr;
                    if (a_value_idx->from->value_type->using_type_name != nullptr)
                    {
                        struct_symb = a_value_idx->from->value_type->using_type_name->symbol;
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
                        if (!a_value_idx->located_scope->belongs_to(
                            a_value_idx->from->value_type->searching_begin_namespace_in_pass2))
                        {
                            lang_anylizer->lang_error(lexer::errorlevel::error, a_value_idx, WO_ERR_UNACCABLE_PROTECTED_MEMBER,
                                a_value_idx->from->value_type->get_type_name(false, false).c_str(),
                                member_field_name->c_str());
                        }
                    }
                    else
                    {
                        if (a_value_idx->source_file != struct_symb->defined_source())
                        {
                            lang_anylizer->lang_error(lexer::errorlevel::error, a_value_idx, WO_ERR_UNACCABLE_PRIVATE_MEMBER,
                                a_value_idx->from->value_type->get_type_name(false, false).c_str(),
                                member_field_name->c_str());
                        }
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
                    || a_value_assi->operate == lex_type::l_value_assign
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
            if (a_value_logic_bin->operate == lex_type::l_lor || a_value_logic_bin->operate == lex_type::l_land)
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
    }
    WO_PASS1(ast_value_type_cast)
    {
        auto* a_value_cast = WO_AST();
        analyze_pass1(a_value_cast->_be_cast_value_node);
        fully_update_type(a_value_cast->value_type, true);
    }
    WO_PASS1(ast_value_type_judge)
    {
        auto* ast_value_judge = WO_AST();
        analyze_pass1(ast_value_judge->_be_cast_value_node);
        fully_update_type(ast_value_judge->value_type, true);
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

        ast_value_check->eval_constant_value(lang_anylizer);
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
            if (a_value_func->where_constraint == nullptr ||
                a_value_func->where_constraint->accept)
            {
                if (a_value_func->in_function_sentence)
                    analyze_pass1(a_value_func->in_function_sentence);

                if (a_value_func->externed_func_info == nullptr
                    && !a_value_func->has_return_value
                    && a_value_func->auto_adjust_return_type
                    && a_value_func->value_type->get_return_type()->is_pure_pending())
                {
                    // This function has no return, set it as void
                    wo_assert(a_value_func->value_type->is_function());
                    a_value_func->value_type->function_ret_type->set_type_with_name(WO_PSTR(void));
                }
            }
            else
                a_value_func->value_type->function_ret_type->set_type_with_name(WO_PSTR(pending));
        }
        else
        {
            if (a_value_func->declear_attribute != nullptr && a_value_func->declear_attribute->is_extern_attr())
                lang_anylizer->lang_error(lexer::errorlevel::error, a_value_func, WO_ERR_CANNOT_EXPORT_TEMPLATE_FUNC);

            auto arg_child = a_value_func->argument_list->children;
            size_t argid = 0;
            while (arg_child)
            {
                if (ast_value_arg_define* argdef = dynamic_cast<ast_value_arg_define*>(arg_child))
                {
                    wo_assert(a_value_func->value_type->is_function());
                    a_value_func->value_type->argument_types.at(argid)->searching_begin_namespace_in_pass2 =
                        argdef->searching_begin_namespace_in_pass2 =
                        a_value_func->this_func_scope;
                }

                ++argid;
                arg_child = arg_child->sibling;
            }
        }

        if (a_value_func->externed_func_info)
        {
            const auto& libname_may_null = a_value_func->externed_func_info->library_name;
            const auto& funcname = a_value_func->externed_func_info->symbol_name;

            if (a_value_func->externed_func_info->externed_func == nullptr)
            {
                if (libname_may_null.has_value())
                {
                    wo_assert(a_value_func->source_file != nullptr);
                    a_value_func->externed_func_info->externed_func =
                        rslib_extern_symbols::get_lib_symbol(
                            wstr_to_str(*a_value_func->source_file).c_str(),
                            wstr_to_str(libname_may_null.value()).c_str(),
                            wstr_to_str(funcname).c_str(),
                            extern_libs);
                }
                else
                {
                    a_value_func->externed_func_info->externed_func =
                        rslib_extern_symbols::get_global_symbol(wstr_to_str(funcname).c_str());
                }
            }

            if (a_value_func->externed_func_info->externed_func == nullptr)
            {
                if (config::ENABLE_IGNORE_NOT_FOUND_EXTERN_SYMBOL)
                    a_value_func->externed_func_info->externed_func = rslib_std_bad_function;
                else if (libname_may_null.has_value())
                    lang_anylizer->lang_error(lexer::errorlevel::error, a_value_func, WO_ERR_CANNOT_FIND_EXT_SYM_IN_LIB,
                        funcname.c_str(),
                        libname_may_null.value().c_str());
                else
                    lang_anylizer->lang_error(lexer::errorlevel::error, a_value_func, WO_ERR_CANNOT_FIND_EXT_SYM,
                        funcname.c_str());
            }
            else
            {
                // ISSUE 1.13: Check if this symbol has been imported as another function.

                auto* symb = &extern_symb_infos[libname_may_null.value_or(L"")][funcname];
                if (*symb != nullptr && *symb != a_value_func->externed_func_info)
                {
                    if ((*symb)->is_repeat_check_ignored != a_value_func->externed_func_info->is_repeat_check_ignored
                        || a_value_func->externed_func_info->is_repeat_check_ignored == false)
                    {
                        auto* last_symbol = *symb;
                        auto* current_symbol = a_value_func->externed_func_info;

                        if (current_symbol->is_repeat_check_ignored)
                            std::swap(last_symbol, current_symbol);

                        lang_anylizer->lang_error(lexer::errorlevel::error, current_symbol, WO_ERR_REPEATED_EXTERN_FUNC);
                        lang_anylizer->lang_error(lexer::errorlevel::infom, last_symbol, WO_INFO_ITEM_IS_DEFINED_HERE,
                            funcname.c_str());
                    }
                }
                *symb = a_value_func->externed_func_info;
            }

            a_value_func->constant_value.set_handle((wo_handle_t)a_value_func->externed_func_info->externed_func);
            a_value_func->is_constant = true;
        }

        end_function();
    }
    WO_PASS1(ast_fakevalue_unpacked_args)
    {
        auto* a_fakevalue_unpacked_args = WO_AST();
        analyze_pass1(a_fakevalue_unpacked_args->unpacked_pack);
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
                if (!arr_elem_type->accept_type(val->value_type, false, true)
                    && !arr_elem_type->set_mix_types(val->value_type, false))
                {
                    arr_elem_type->set_type_with_name(WO_PSTR(pending));
                    break;
                }
                val = dynamic_cast<ast_value*>(val->sibling);
            }
        }
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
                if (!decide_map_key_type->accept_type(map_pair->key->value_type, false, true)
                    && !decide_map_key_type->set_mix_types(map_pair->key->value_type, false))
                {
                    decide_map_key_type->set_type_with_name(WO_PSTR(pending));
                    decide_map_val_type->set_type_with_name(WO_PSTR(pending));
                    break;
                }
                if (!decide_map_val_type->accept_type(map_pair->val->value_type, false, true)
                    && !decide_map_val_type->set_mix_types(map_pair->val->value_type, false))
                {
                    decide_map_key_type->set_type_with_name(WO_PSTR(pending));
                    decide_map_val_type->set_type_with_name(WO_PSTR(pending));
                    break;
                }
                map_pair = dynamic_cast<ast_mapping_pair*>(map_pair->sibling);
            }
        }
    }
    WO_PASS1(ast_value_indexed_variadic_args)
    {
        auto* a_value_variadic_args_idx = WO_AST();
        analyze_pass1(a_value_variadic_args_idx->argindex);
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

            wo_assert(a_ret->located_function->value_type->is_function());

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
                            if (!func_return_type->accept_type(a_ret->return_value->value_type, false, true))
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
                auto* func_return_type = located_function_scope->function_node->value_type->get_return_type();
                if (located_function_scope->function_node->auto_adjust_return_type)
                {
                    if (located_function_scope->function_node->value_type->is_pending())
                    {
                        func_return_type->set_type_with_name(WO_PSTR(void));
                        located_function_scope->function_node->auto_adjust_return_type = false;
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
    WO_PASS1(ast_sentence_block)
    {
        auto* a_sentence_blk = WO_AST();
        this->begin_scope(a_sentence_blk);
        analyze_pass1(a_sentence_blk->sentence_list);
        this->end_scope();
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
    }
    WO_PASS1(ast_while)
    {
        auto* ast_while_sentence = WO_AST();
        if (ast_while_sentence->judgement_value != nullptr)
            analyze_pass1(ast_while_sentence->judgement_value);
        analyze_pass1(ast_while_sentence->execute_sentence);
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
    }
    WO_PASS1(ast_value_unary)
    {
        auto* a_value_unary = WO_AST();
        analyze_pass1(a_value_unary->val);

        if (a_value_unary->operate == lex_type::l_lnot)
            a_value_unary->value_type->set_type_with_name(WO_PSTR(bool));
        a_value_unary->value_type->set_type(a_value_unary->val->value_type);
    }
    WO_PASS1(ast_mapping_pair)
    {
        auto* a_mapping_pair = WO_AST();
        analyze_pass1(a_mapping_pair->key);
        analyze_pass1(a_mapping_pair->val);
    }
    WO_PASS1(ast_using_namespace)
    {
        auto* a_using_namespace = WO_AST();
        now_scope()->used_namespace.push_back(a_using_namespace);
    }
    WO_PASS1(ast_using_type_as)
    {
        auto* a_using_type_as = WO_AST();

        if (a_using_type_as->new_type_identifier == WO_PSTR(char)
            || ast_type::name_type_pair.find(a_using_type_as->new_type_identifier) != ast_type::name_type_pair.end())
        {
            lang_anylizer->lang_error(lexer::errorlevel::error, a_using_type_as,
                WO_ERR_DECL_BUILTIN_TYPE_IS_NOT_ALLOWED);
        }
        else
        {
            if (!a_using_type_as->old_type->has_typeof()
                || a_using_type_as->template_type_name_list.empty())
            {
                if (a_using_type_as->namespace_decl != nullptr)
                    begin_namespace(a_using_type_as->namespace_decl);

                fully_update_type(a_using_type_as->old_type, true, a_using_type_as->template_type_name_list);

                if (a_using_type_as->namespace_decl != nullptr)
                    end_namespace();
            }

            if (a_using_type_as->type_symbol == nullptr)
            {
                auto* typing_symb = define_type_in_this_scope(
                    a_using_type_as, a_using_type_as->old_type, a_using_type_as->declear_attribute);
                typing_symb->apply_template_setting(a_using_type_as);

                a_using_type_as->type_symbol = typing_symb;
            }
            analyze_pass1(a_using_type_as->namespace_decl);
        }

        a_using_type_as->type_symbol->has_been_completed_defined = true;
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
    }
    WO_PASS1(ast_union_make_option_ob_to_cr_and_ret)
    {
        auto* a_union_make_option_ob_to_cr_and_ret = WO_AST();
        if (a_union_make_option_ob_to_cr_and_ret->argument_may_nil)
            analyze_pass1(a_union_make_option_ob_to_cr_and_ret->argument_may_nil);
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
    }
    WO_PASS1(ast_match_union_case)
    {
        auto* a_match_union_case = WO_AST();
        begin_scope(a_match_union_case);
        wo_assert(a_match_union_case->in_match);

        if (ast_pattern_union_value* a_pattern_union_value =
            dynamic_cast<ast_pattern_union_value*>(a_match_union_case->union_pattern))
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
                a_match_union_case->take_place_value_may_nil->copy_source_info(
                    a_pattern_union_value->pattern_arg_in_union_may_nil);

                analyze_pattern_in_pass1(
                    a_pattern_union_value->pattern_arg_in_union_may_nil,
                    new ast_decl_attribute,
                    a_match_union_case->take_place_value_may_nil);
            }
        }
        else
            lang_anylizer->lang_error(lexer::errorlevel::error, a_match_union_case, WO_ERR_UNEXPECT_PATTERN_CASE);

        analyze_pass1(a_match_union_case->in_case_sentence);

        end_scope();
    }
    WO_PASS1(ast_value_make_struct_instance)
    {
        auto* a_value_make_struct_instance = WO_AST();
        analyze_pass1(a_value_make_struct_instance->struct_member_vals);
        if (a_value_make_struct_instance->build_pure_struct)
        {
            auto* member_iter = dynamic_cast<ast_struct_member_define*>(
                a_value_make_struct_instance->struct_member_vals->children);
            while (member_iter)
            {
                wo_assert(member_iter->is_value_pair);
                if (!member_iter->member_value_pair->value_type->is_pending())
                {
                    auto fnd = a_value_make_struct_instance->target_built_types
                        ->struct_member_index.find(member_iter->member_name);
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
    }
    WO_PASS1(ast_struct_member_define)
    {
        auto* a_struct_member_define = WO_AST();

        if (a_struct_member_define->is_value_pair)
            analyze_pass1(a_struct_member_define->member_value_pair);
        else
            fully_update_type(a_struct_member_define->member_type, true);
    }
    WO_PASS1(ast_where_constraint)
    {
        auto* a_where_constraint = WO_AST();

        analyze_pass1(a_where_constraint->where_constraint_list);
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

            a_value_trib_expr->judge_expr->eval_constant_value(lang_anylizer);
        }
        else
        {
            analyze_pass1(a_value_trib_expr->val_if_true, false);
            analyze_pass1(a_value_trib_expr->val_or, false);

            a_value_trib_expr->value_type->set_type(a_value_trib_expr->val_if_true->value_type);
            if (!a_value_trib_expr->value_type->set_mix_types(a_value_trib_expr->val_or->value_type, false))
                a_value_trib_expr->value_type->set_type_with_name(WO_PSTR(pending));
        }
    }
    WO_PASS1(ast_value_typeid)
    {
        auto* ast_value_typeid = WO_AST();

        fully_update_type(ast_value_typeid->type, true);
    }
}