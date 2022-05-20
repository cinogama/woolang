#pragma once

#include "rs_basic_type.hpp"
#include "rs_lang_ast_builder.hpp"
#include "rs_compiler_ir.hpp"

#include <unordered_map>
#include <unordered_set>
namespace rs
{
    struct lang_symbol
    {
        enum class symbol_type
        {
            template_typing,
            typing,

            variable,
            function,
        };
        symbol_type type;
        std::wstring name;

        ast::ast_defines* define_node = nullptr;
        ast::ast_decl_attribute* attribute;
        lang_scope* defined_in_scope;
        bool define_in_function = false;
        bool static_symbol = false;
        bool has_been_defined_in_pass2 = false;
        bool is_constexpr = false;
        bool has_been_assigned = false;
        bool is_ref = false;

        union
        {
            rs_integer_t stackvalue_index_in_funcs = -99999999;
            size_t global_index_in_lang;
        };

        union
        {
            ast::ast_value* variable_value;
            ast::ast_type* type_informatiom;
        };
        std::vector<ast::ast_value_function_define*> function_overload_sets;
        std::vector<ast::ast_check_type_with_naming_in_pass2*> naming_list;
        bool is_template_symbol = false;
        std::vector<std::wstring> template_types;

        void apply_template_setting(ast::ast_defines* defs)
        {
            is_template_symbol = defs->is_template_define;
            if (is_template_symbol)
            {
                template_types = defs->template_type_name_list;
            }
        }
    };

    struct lang_scope
    {
        bool stop_searching_in_last_scope_flag;

        enum scope_type
        {
            namespace_scope,    // namespace xx{}
            function_scope,     // func xx(){}
            just_scope,         //{} if{} while{}
        };

        scope_type type;
        lang_scope* belong_namespace;
        lang_scope* parent_scope;
        std::wstring scope_namespace;
        std::unordered_map<std::wstring, lang_symbol*> symbols;

        // Only used when this scope is a namespace.
        std::unordered_map<std::wstring, lang_scope*> sub_namespaces;

        std::vector<ast::ast_using_namespace*> used_namespace;
        std::vector<lang_symbol*> in_function_symbols;

        ast::ast_value_function_define* function_node;

        size_t max_used_stack_size_in_func = 0; // only used in function_scope
        size_t used_stackvalue_index = 0; // only used in function_scope

        size_t this_block_used_stackvalue_count = 0;

        size_t assgin_stack_index(lang_symbol* in_func_variable)
        {
            rs_assert(type == scope_type::function_scope);
            in_function_symbols.push_back(in_func_variable);

            if (used_stackvalue_index + 1 > max_used_stack_size_in_func)
                max_used_stack_size_in_func = used_stackvalue_index + 1;
            return used_stackvalue_index++;
        }

        void reduce_function_used_stack_size_at(rs_integer_t canceled_stack_pos)
        {
            max_used_stack_size_in_func--;
            for (auto* infuncvars : in_function_symbols)
            {
                rs_assert(infuncvars->type == lang_symbol::symbol_type::variable);

                if (!infuncvars->static_symbol)
                {
                    if (infuncvars->stackvalue_index_in_funcs == canceled_stack_pos)
                        infuncvars->stackvalue_index_in_funcs = 0;
                    else if (infuncvars->stackvalue_index_in_funcs > canceled_stack_pos)
                        infuncvars->stackvalue_index_in_funcs--;
                }

            }
        }

    };

    class lang
    {
    private:
        using template_type_map = std::map<std::wstring, lang_symbol*>;

        lexer* lang_anylizer;
        std::vector<lang_scope*> lang_scopes_buffers;
        std::vector<lang_symbol*> lang_symbols; // only used for storing symbols to release
        std::vector<opnum::opnumbase*> generated_opnum_list_for_clean;
        std::forward_list<grammar::ast_base*> generated_ast_nodes_buffers;
        std::unordered_set<grammar::ast_base*> traving_node;
        std::unordered_set<lang_symbol*> traving_symbols;
        std::vector<lang_scope*> lang_scopes; // it is a stack like list;
        lang_scope* now_namespace = nullptr;

        std::map<uint32_t, ast::ast_type*> hashed_typing;
        uint32_t get_typing_hash_after_pass1(ast::ast_type* typing)
        {
            rs_assert(!typing->is_pending());

            uint64_t hashval = (uint64_t)typing->value_type;
            if (typing->is_complex())
                hashval += get_typing_hash_after_pass1(typing->complex_type);

            hashval *= hashval;

            if (lang_symbol* using_type_symb = typing->using_type_name ? find_type_in_this_scope(typing->using_type_name) : nullptr)
            {
                hashval <<= 1;
                hashval += (uint64_t)using_type_symb;
                hashval *= hashval;
            }

            if (typing->has_template())
            {
                for (auto& template_arg : typing->template_arguments)
                {
                    hashval <<= 1;
                    hashval += get_typing_hash_after_pass1(template_arg);
                    hashval *= hashval;
                }
            }
            if (typing->is_func())
            {
                for (auto& arg : typing->argument_types)
                {
                    hashval <<= 1;
                    hashval += get_typing_hash_after_pass1(arg);
                    hashval *= hashval;
                }
            }

            hashval >>= 16;

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

        std::map<rs_extern_native_func_t, std::vector<ast::ast_value_function_define*>> extern_symb_func_definee;
        ast::ast_value_function_define* now_function_in_final_anylize = nullptr;
        std::vector<template_type_map> template_stack;
        std::unordered_set<ast::ast_value_variable*> template_variable_list_for_pass_template;

        rslib_extern_symbols::extern_lib_set extern_libs;

        bool begin_template_scope(ast::ast_defines* template_defines, const std::vector<ast::ast_type*>& template_args)
        {
            rs_test(template_args.size());
            if (template_defines->template_type_name_list.size() != template_args.size())
            {
                lang_anylizer->lang_error(0x0000, template_args.back(), RS_ERR_TEMPLATE_ARG_NOT_MATCH);
                return false;
            }

            template_stack.push_back(template_type_map());
            auto& current_template = template_stack.back();
            for (size_t index = 0; index < template_defines->template_type_name_list.size(); index++)
            {
                lang_symbol* sym = new lang_symbol;
                sym->attribute = new ast::ast_decl_attribute();
                sym->type = lang_symbol::symbol_type::template_typing;
                sym->name = template_defines->template_type_name_list[index];
                sym->type_informatiom = template_args[index];
                sym->defined_in_scope = lang_scopes.back();

                lang_symbols.push_back(current_template[sym->name] = sym);
            }
            return true;
        }

        void end_template_scope()
        {
            template_stack.pop_back();
        }

        void temporary_entry_scope_in_pass1(lang_scope* scop)
        {
            lang_scopes.push_back(scop);
        }
        lang_scope* temporary_leave_scope_in_pass1()
        {
            auto* scop = lang_scopes.back();

            lang_scopes.pop_back();

            return scop;
        }
    public:
        lang(lexer& lex) :
            lang_anylizer(&lex)
        {
            _this_thread_lang_context = this;
            begin_namespace(L"");   // global namespace
        }
        ~lang()
        {
            _this_thread_lang_context = nullptr;
            clean_and_close_lang();
        }

        bool check_matching_naming(ast::ast_type* clstype, ast::ast_type* naming)
        {
            bool result = true;
            // Check function

            if (clstype->using_type_name && naming->using_type_name &&
                clstype->using_type_name->symbol && naming->using_type_name->symbol)
            {
                auto cls_using = dynamic_cast<ast::ast_using_type_as*>(clstype->using_type_name->symbol->define_node);
                auto naming_using = dynamic_cast<ast::ast_using_type_as*>(naming->using_type_name->symbol->define_node);

                for (auto& [naming_func_name, naming_funcs] : naming_using->class_methods_list)
                {
                    if (auto fnd = cls_using->class_methods_list.find(naming_func_name); fnd != cls_using->class_methods_list.end())
                    {
                        if (fnd->second.size() != naming_funcs.size())
                        {
                            lang_anylizer->lang_error(0x0000, naming, L"类型%ls不满足具名%ls的要求: 方法%ls的重载集不符，继续",
                                clstype->get_type_name(false).c_str(),
                                naming->get_type_name(false).c_str(),
                                naming_func_name.c_str());
                            result = false;
                        }
                        // TODO: do more check.. here just so~
                    }
                    else
                    {
                        lang_anylizer->lang_error(0x0000, naming, L"类型%ls不满足具名%ls的要求: 缺少方法%ls，继续",
                            clstype->get_type_name(false).c_str(),
                            naming->get_type_name(false).c_str(),
                            naming_func_name.c_str());
                        result = false;
                    }
                }

            }

            // Check member
            for (auto& [naming_memb_name, naming_memb_name_val] : naming->class_member_index)
            {
                if (auto fnd = clstype->class_member_index.find(naming_memb_name); fnd != clstype->class_member_index.end())
                {
                    if (naming_memb_name_val->value_type->is_pending())
                        ; // member type not computed, just pass
                    else if (fnd->second->value_type->is_pending()
                        || !fnd->second->value_type->is_same(naming_memb_name_val->value_type))
                    {
                        lang_anylizer->lang_error(0x0000, naming, L"类型%ls不满足具名%ls的要求: 成员%ls类型不同，继续",
                            clstype->get_type_name(false).c_str(),
                            naming->get_type_name(false).c_str(),
                            naming_memb_name.c_str());
                        result = false;
                    }
                }
                else
                {
                    lang_anylizer->lang_error(0x0000, naming, L"类型%ls不满足具名%ls的要求: 缺少成员%ls，继续",
                        clstype->get_type_name(false).c_str(),
                        naming->get_type_name(false).c_str(),
                        naming_memb_name.c_str());
                    result = false;
                }
            }
            return result;
        }

        void fully_update_type(ast::ast_type* type, bool in_pass_1, const std::vector<std::wstring>& template_types = {})
        {
            if (type->typefrom)
            {
                auto used_type_info = type->using_type_name;

                if (in_pass_1)
                    analyze_pass1(type->typefrom);
                if (has_step_in_step2)
                    analyze_pass2(type->typefrom);

                if (!type->typefrom->value_type->is_pending())
                {
                    if (type->is_func())
                        type->set_ret_type(type->typefrom->value_type);
                    else
                        type->set_type(type->typefrom->value_type);
                }

                if (used_type_info)
                    type->using_type_name = used_type_info;
            }

            if (type->using_type_name)
            {
                if (!type->using_type_name->symbol)
                    type->using_type_name->symbol = find_type_in_this_scope(type->using_type_name);
            }

            // todo: begin_template_scope here~
            if (type->is_custom())
            {
                if (type->is_complex())
                    fully_update_type(type->complex_type, in_pass_1);
                if (type->is_func())
                    for (auto& a_t : type->argument_types)
                        fully_update_type(a_t, in_pass_1);

                if (type->has_template())
                {
                    for (auto* template_type : type->template_arguments)
                        fully_update_type(template_type, in_pass_1);
                }

                // ready for update..
                if (ast::ast_type::is_custom_type(type->type_name))
                {
                    if (!type->scope_namespaces.empty() ||
                        type->search_from_global_namespace ||
                        template_types.end() == std::find(template_types.begin(), template_types.end(), type->type_name))
                    {
                        auto* type_sym = find_type_in_this_scope(type);

                        if (traving_symbols.find(type_sym) != traving_symbols.end())
                            return;

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

                        traving_guard g1(this, type_sym);

                        if (type_sym)
                        {
                            auto already_has_using_type_name = type->using_type_name;

                            bool using_template = false;
                            auto using_template_args = type->template_arguments;
                            if (type->has_template())
                                using_template = type_sym->define_node ?
                                begin_template_scope(type_sym->define_node, type->template_arguments)
                                : begin_template_scope(type_sym->type_informatiom->using_type_name->symbol->define_node, type->template_arguments);

                            auto* symboled_type = new ast::ast_type(L"pending");
                            symboled_type->set_type(type_sym->type_informatiom);
                            fully_update_type(symboled_type, in_pass_1);

                            if (type->is_func())
                                type->set_ret_type(symboled_type);
                            else
                                type->set_type(symboled_type);

                            // Update member typing index;
                            if (using_template)
                                for (auto& [name, initval] : type->class_member_index)
                                    initval = dynamic_cast<ast::ast_value*>(initval->instance());

                            if (already_has_using_type_name)
                                type->using_type_name = already_has_using_type_name;
                            else if (type_sym->type != lang_symbol::symbol_type::template_typing)
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

                            if (!type->template_impl_naming_checking.empty())
                            {
                                for (ast::ast_type* naming_type : type->template_impl_naming_checking)
                                {
                                    fully_update_type(naming_type, in_pass_1);

                                    check_matching_naming(type, naming_type);
                                }
                            }

                            for (auto& naming : type_sym->naming_list)
                            {
                                if (in_pass_1)
                                    analyze_pass1(type->typefrom);
                                auto inpass2 = has_step_in_step2;
                                analyze_pass2(naming);  // FORCE PASS2
                                has_step_in_step2 = inpass2;
                            }

                            if (using_template)
                                end_template_scope();
                        }
                    }
                }
            }

            for (auto& [name, initval] : type->class_member_index)
            {
                if (in_pass_1)
                    analyze_pass1(initval);
                if (has_step_in_step2)
                    analyze_pass2(initval);
            }

            rs_test(!type->using_type_name || !type->using_type_name->using_type_name);
        }

        std::vector<bool> in_pass2 = { false };
        struct entry_pass
        {
            std::vector<bool>* _set;
            entry_pass(std::vector<bool>& state_set, bool inpass2)
                :_set(&state_set)
            {
                state_set.push_back(inpass2);
            }
            ~entry_pass()
            {
                _set->pop_back();
            }
        };

        void analyze_pass1(grammar::ast_base* ast_node)
        {
            entry_pass ep1(in_pass2, false);

            if (traving_node.find(ast_node) != traving_node.end())
                return;

            struct traving_guard
            {
                lang* _lang;
                grammar::ast_base* _tving_node;
                traving_guard(lang* _lg, grammar::ast_base* ast_ndoe)
                    : _lang(_lg)
                    , _tving_node(ast_ndoe)  
                {
                    _lang->traving_node.insert(_tving_node);
                }
                ~traving_guard()
                {
                    _lang->traving_node.erase(_tving_node);
                }
            };

            traving_guard g1(this, ast_node);

            using namespace ast;

            if (!ast_node)return;

            if (ast_symbolable_base* a_symbol_ob = dynamic_cast<ast_symbolable_base*>(ast_node))
            {
                a_symbol_ob->searching_begin_namespace_in_pass2 = now_scope();
            }

            ///////////////////////////////////////////////////////////////////////////////////////////

            if (ast_namespace* a_namespace = dynamic_cast<ast_namespace*>(ast_node))
            {
                begin_namespace(a_namespace->scope_name);
                a_namespace->add_child(a_namespace->in_scope_sentence);
                grammar::ast_base* child = a_namespace->in_scope_sentence->children;
                while (child)
                {
                    analyze_pass1(child);
                    child = child->sibling;
                }
                end_namespace();
            }
            else if (ast_varref_defines* a_varref_defs = dynamic_cast<ast_varref_defines*>(ast_node))
            {
                for (auto& varref : a_varref_defs->var_refs)
                {
                    analyze_pass1(varref.init_val);
                    varref.symbol = define_variable_in_this_scope(varref.ident_name, varref.init_val, a_varref_defs->declear_attribute);
                    varref.symbol->is_ref = varref.is_ref;
                    a_varref_defs->add_child(varref.init_val);

                    if (varref.is_ref)
                        varref.init_val->is_mark_as_using_ref = true;
                }
            }
            else if (ast_value_binary* a_value_bin = dynamic_cast<ast_value_binary*>(ast_node))
            {
                analyze_pass1(a_value_bin->left);
                analyze_pass1(a_value_bin->right);

                a_value_bin->add_child(a_value_bin->left);
                a_value_bin->add_child(a_value_bin->right);

                a_value_bin->value_type = ast_value_binary::binary_upper_type_with_operator(
                    a_value_bin->left->value_type,
                    a_value_bin->right->value_type,
                    a_value_bin->operate
                );

                if (nullptr == a_value_bin->value_type)
                {
                    a_value_bin->value_type = new ast_type(L"pending");

                    ast_value_funccall* try_operator_func_overload = new ast_value_funccall();

                    try_operator_func_overload->arguments = new ast_list();
                    try_operator_func_overload->value_type = new ast_type(L"pending");
                    try_operator_func_overload->called_func = new ast_value_variable(std::wstring(L"operator ") + lexer::lex_is_operate_type(a_value_bin->operate));
                    try_operator_func_overload->directed_value_from = a_value_bin->left;

                    ast_value* arg1 = dynamic_cast<ast_value*>(a_value_bin->left->instance());
                    arg1->is_mark_as_using_ref = true;
                    try_operator_func_overload->arguments->add_child(arg1);

                    ast_value* arg2 = dynamic_cast<ast_value*>(a_value_bin->right->instance());
                    arg2->is_mark_as_using_ref = true;
                    try_operator_func_overload->arguments->add_child(arg2);

                    try_operator_func_overload->called_func->source_file = a_value_bin->source_file;
                    try_operator_func_overload->called_func->row_no = a_value_bin->row_no;
                    try_operator_func_overload->called_func->col_no = a_value_bin->col_no;

                    try_operator_func_overload->source_file = a_value_bin->source_file;
                    try_operator_func_overload->row_no = a_value_bin->row_no;
                    try_operator_func_overload->col_no = a_value_bin->col_no;

                    a_value_bin->overrided_operation_call = try_operator_func_overload;
                    analyze_pass1(a_value_bin->overrided_operation_call);

                    if (!a_value_bin->overrided_operation_call->value_type->is_pending())
                        a_value_bin->value_type->set_type(a_value_bin->overrided_operation_call->value_type);
                }
            }
            else if (ast_value_index* a_value_idx = dynamic_cast<ast_value_index*>(ast_node))
            {
                analyze_pass1(a_value_idx->from);
                analyze_pass1(a_value_idx->index);

                if (!a_value_idx->from->value_type->class_member_index.empty())
                {
                    if (a_value_idx->index->is_constant && a_value_idx->index->value_type->is_string())
                    {
                        if (auto fnd =
                            a_value_idx->from->value_type->class_member_index.find(
                                str_to_wstr(*a_value_idx->index->get_constant_value().string)
                            ); fnd != a_value_idx->from->value_type->class_member_index.end())
                        {
                            if (!fnd->second->value_type->is_pending())
                            {
                                a_value_idx->value_type = fnd->second->value_type;
                            }
                        }
                    }
                }
                else if (a_value_idx->from->value_type->is_string())
                {
                    a_value_idx->value_type = new ast_type(L"string");
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
                        a_value_idx->value_type = new ast_type(L"dynamic");
                    }
                    a_value_idx->can_be_assign = a_value_idx->from->can_be_assign;
                }

            }
            else if (ast_value_assign* a_value_assi = dynamic_cast<ast_value_assign*>(ast_node))
            {
                analyze_pass1(a_value_assi->left);
                analyze_pass1(a_value_assi->right);

                a_value_assi->add_child(a_value_assi->left);
                a_value_assi->add_child(a_value_assi->right);

                if (auto lsymb = dynamic_cast<ast_value_symbolable_base*>(a_value_assi->left)
                    ; lsymb && lsymb->symbol)
                    lsymb->symbol->has_been_assigned = true;

                a_value_assi->value_type = new ast_type(L"pending");
                a_value_assi->value_type->set_type(a_value_assi->left->value_type);

                /*if (!a_value_assi->value_type->is_pending() && !a_value_assi->right->value_type->is_pending())
                {
                    if (!ast_type::check_castable(a_value_assi->left->value_type, a_value_assi->right->value_type, false))
                    {
                        lang_anylizer->lang_error(0x0000, a_value_assi, RS_ERR_CANNOT_ASSIGN_TYPE_TO_TYPE,
                            a_value_assi->right->value_type->get_type_name().c_str(),
                            a_value_assi->left->value_type->get_type_name().c_str());
                    }
                }*/
            }
            else if (ast_value_logical_binary* a_value_logic_bin = dynamic_cast<ast_value_logical_binary*>(ast_node))
            {
                analyze_pass1(a_value_logic_bin->left);
                analyze_pass1(a_value_logic_bin->right);

                a_value_logic_bin->add_child(a_value_logic_bin->left);
                a_value_logic_bin->add_child(a_value_logic_bin->right);

                ast_value_funccall* try_operator_func_overload = new ast_value_funccall();

                try_operator_func_overload->arguments = new ast_list();
                try_operator_func_overload->value_type = new ast_type(L"pending");
                try_operator_func_overload->called_func = new ast_value_variable(std::wstring(L"operator ") + lexer::lex_is_operate_type(a_value_logic_bin->operate));
                try_operator_func_overload->directed_value_from = a_value_logic_bin->left;

                ast_value* arg1 = dynamic_cast<ast_value*>(a_value_logic_bin->left->instance());
                arg1->is_mark_as_using_ref = true;
                try_operator_func_overload->arguments->add_child(arg1);

                ast_value* arg2 = dynamic_cast<ast_value*>(a_value_logic_bin->right->instance());
                arg2->is_mark_as_using_ref = true;
                try_operator_func_overload->arguments->add_child(arg2);

                try_operator_func_overload->called_func->source_file = a_value_logic_bin->source_file;
                try_operator_func_overload->called_func->row_no = a_value_logic_bin->row_no;
                try_operator_func_overload->called_func->col_no = a_value_logic_bin->col_no;

                try_operator_func_overload->source_file = a_value_logic_bin->source_file;
                try_operator_func_overload->row_no = a_value_logic_bin->row_no;
                try_operator_func_overload->col_no = a_value_logic_bin->col_no;

                a_value_logic_bin->overrided_operation_call = try_operator_func_overload;
                analyze_pass1(a_value_logic_bin->overrided_operation_call);

                if (!a_value_logic_bin->overrided_operation_call->value_type->is_pending())
                    a_value_logic_bin->value_type->set_type(a_value_logic_bin->overrided_operation_call->value_type);
            }
            else if (ast_value_variable* a_value_var = dynamic_cast<ast_value_variable*>(ast_node))
            {
                auto* sym = find_value_in_this_scope(a_value_var);

                if (sym)
                {
                    a_value_var->value_type = sym->variable_value->value_type;
                    if (sym->type == lang_symbol::symbol_type::variable && !a_value_var->template_reification_args.empty())
                    {
                        lang_anylizer->lang_error(0x0000, a_value_var, RS_ERR_NO_TEMPLATE_VARIABLE);
                    }

                }
                for (auto* a_type : a_value_var->template_reification_args)
                {
                    if (a_type->is_pending())
                    {
                        // ready for update..
                        fully_update_type(a_type, true);
                    }
                    a_type->searching_begin_namespace_in_pass2 = now_scope();
                }

                template_variable_list_for_pass_template.insert(a_value_var);
            }
            else if (ast_value_type_cast* a_value_cast = dynamic_cast<ast_value_type_cast*>(ast_node))
            {
                analyze_pass1(a_value_cast->_be_cast_value_node);
                a_value_cast->add_child(a_value_cast->_be_cast_value_node);
            }
            else if (ast_value_type_judge* ast_value_judge = dynamic_cast<ast_value_type_judge*>(ast_node))
            {
                if (ast_value_judge->is_mark_as_using_ref)
                    ast_value_judge->_be_cast_value_node->is_mark_as_using_ref = true;

                analyze_pass1(ast_value_judge->_be_cast_value_node);

            }
            else if (ast_value_type_check* ast_value_check = dynamic_cast<ast_value_type_check*>(ast_node))
            {
                analyze_pass1(ast_value_check->_be_check_value_node);
                if (ast_value_check->aim_type->is_pending())
                {
                    // ready for update..
                    fully_update_type(ast_value_check->aim_type, true);
                }

                ast_value_check->update_constant_value(lang_anylizer);
            }
            else if (ast_value_function_define* a_value_func = dynamic_cast<ast_value_function_define*>(ast_node))
            {
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
                                argdef->symbol = define_variable_in_this_scope(argdef->arg_name, argdef, argdef->declear_attribute);
                                argdef->symbol->is_ref = argdef->is_ref;
                            }
                        }
                        else
                        {
                            rs_assert(dynamic_cast<ast_token*>(arg_child));
                        }

                        arg_child = arg_child->sibling;
                    }

                    fully_update_type(a_value_func->value_type, true);
                    // update return type info for
                    // func xxx(var x:int): typeof(x)

                    if (a_value_func->in_function_sentence)
                    {
                        analyze_pass1(a_value_func->in_function_sentence);
                        a_value_func->add_child(a_value_func->in_function_sentence);
                    }

                    if (a_value_func->externed_func_info && !a_value_func->externed_func_info->externed_func)
                    {
                        if (a_value_func->externed_func_info->load_from_lib != L"")
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
                                lang_anylizer->lang_error(0x0000, a_value_func, RS_ERR_CANNOT_FIND_EXT_SYM_IN_LIB,
                                    a_value_func->externed_func_info->symbol_name.c_str(),
                                    a_value_func->externed_func_info->load_from_lib.c_str());
                        }
                    }

                    if (a_value_func->externed_func_info)
                        extern_symb_func_definee[a_value_func->externed_func_info->externed_func]
                        .push_back(a_value_func);

                    if (a_value_func->value_type->type_name == L"pending")
                    {
                        // NOTE: JUST CHECK RETURN TYPE;
                        // type compute fail, delay them..
                    }
                }

                end_function();
            }
            else if (ast_fakevalue_unpacked_args* a_fakevalue_unpacked_args = dynamic_cast<ast_fakevalue_unpacked_args*>(ast_node))
            {
                analyze_pass1(a_fakevalue_unpacked_args->unpacked_pack);
            }
            else if (ast_value_funccall* a_value_funccall = dynamic_cast<ast_value_funccall*>(ast_node))
            {
                if (auto* symb_callee = dynamic_cast<ast_value_variable*>(a_value_funccall->called_func))
                {
                    if (a_value_funccall->directed_value_from)
                    {
                        if (!symb_callee->search_from_global_namespace
                            && symb_callee->scope_namespaces.empty())
                        {
                            analyze_pass1(a_value_funccall->directed_value_from);

                            a_value_funccall->callee_symbol_in_type_namespace = new ast_value_variable(symb_callee->var_name);
                            a_value_funccall->callee_symbol_in_type_namespace->search_from_global_namespace = true;
                            a_value_funccall->callee_symbol_in_type_namespace->searching_begin_namespace_in_pass2 = now_scope();
                            // a_value_funccall->callee_symbol_in_type_namespace wiil search in pass2..
                        }
                    }
                }

                analyze_pass1(a_value_funccall->called_func);
                analyze_pass1(a_value_funccall->arguments);

                if (auto* symb_callee = dynamic_cast<ast_value_variable*>(a_value_funccall->called_func))
                    template_variable_list_for_pass_template.erase(symb_callee);

                // NOTE There is no need for adding arguments and celled_func to child, pass2 must read them..
                //a_value_funccall->add_child(a_value_funccall->called_func);
                //a_value_funccall->add_child(a_value_funccall->arguments);

                // function call should be 'pending' type, then do override judgement in pass2
            }
            else if (ast_value_array* a_value_arr = dynamic_cast<ast_value_array*>(ast_node))
            {
                analyze_pass1(a_value_arr->array_items);
                a_value_arr->add_child(a_value_arr->array_items);

                if (a_value_arr->value_type->is_pending() && !a_value_arr->value_type->is_custom())
                {
                    // 
                    ast_type* decide_array_item_type = new ast_type(L"dynamic");

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

                        if (!val->value_type->is_same(decide_array_item_type, false))
                        {
                            auto* mixed_type = ast_value_binary::binary_upper_type(decide_array_item_type, val->value_type);
                            if (mixed_type)
                            {
                                decide_array_item_type->set_type_with_name(mixed_type->type_name);
                            }
                            else
                            {
                                decide_array_item_type->set_type_with_name(L"dynamic");
                                // lang_anylizer->lang_warning(0x0000, a_ret, RS_WARN_FUNC_WILL_RETURN_DYNAMIC);
                            }
                        }
                        val = dynamic_cast<ast_value*>(val->sibling);
                    }

                    if (decide_array_item_type)
                    {
                        a_value_arr->value_type->template_arguments[0] = decide_array_item_type;
                    }
                }
            }
            else if (ast_value_mapping* a_value_map = dynamic_cast<ast_value_mapping*>(ast_node))
            {
                analyze_pass1(a_value_map->mapping_pairs);
                a_value_map->add_child(a_value_map->mapping_pairs);

                if (a_value_map->value_type->is_pending() && !a_value_map->value_type->is_custom())
                {
                    ast_type* decide_map_key_type = new ast_type(L"dynamic");
                    ast_type* decide_map_val_type = new ast_type(L"dynamic");

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

                        if (!map_pair->key->value_type->is_same(decide_map_key_type, false))
                        {
                            auto* mixed_type = ast_value_binary::binary_upper_type(decide_map_key_type, map_pair->key->value_type);
                            if (mixed_type)
                            {
                                decide_map_key_type->set_type_with_name(mixed_type->type_name);
                            }
                            else
                            {
                                decide_map_key_type->set_type_with_name(L"dynamic");
                                // lang_anylizer->lang_warning(0x0000, a_ret, RS_WARN_FUNC_WILL_RETURN_DYNAMIC);
                            }
                        }
                        if (!map_pair->val->value_type->is_same(decide_map_val_type, false))
                        {
                            auto* mixed_type = ast_value_binary::binary_upper_type(decide_map_val_type, map_pair->val->value_type);
                            if (mixed_type)
                            {
                                decide_map_val_type->set_type_with_name(mixed_type->type_name);
                            }
                            else
                            {
                                decide_map_val_type->set_type_with_name(L"dynamic");
                                // lang_anylizer->lang_warning(0x0000, a_ret, RS_WARN_FUNC_WILL_RETURN_DYNAMIC);
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
            }
            else if (ast_value_indexed_variadic_args* a_value_variadic_args_idx = dynamic_cast<ast_value_indexed_variadic_args*>(ast_node))
            {
                analyze_pass1(a_value_variadic_args_idx->argindex);
            }
            else if (ast_return* a_ret = dynamic_cast<ast_return*>(ast_node))
            {
                auto* located_function_scope = in_function();
                if (!located_function_scope)
                    lang_anylizer->lang_error(0x0000, a_ret, RS_ERR_CANNOT_DO_RET_OUSIDE_FUNC);
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
                                    if (!func_return_type->is_same(a_ret->return_value->value_type, false))
                                    {
                                        auto* mixed_type = ast_value_binary::binary_upper_type(func_return_type, a_ret->return_value->value_type);
                                        if (mixed_type)
                                        {
                                            located_function_scope->function_node->value_type->set_type_with_name(mixed_type->type_name);
                                        }
                                        else
                                        {
                                            located_function_scope->function_node->value_type->set_type_with_name(L"dynamic");
                                            lang_anylizer->lang_warning(0x0000, a_ret, RS_WARN_FUNC_WILL_RETURN_DYNAMIC);
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
                                lang_anylizer->lang_error(0x0000, a_ret, RS_ERR_CANNOT_RET_TYPE_AND_TYPE_AT_SAME_TIME, L"void", located_function_scope->function_node->value_type->type_name.c_str());
                            }
                        }
                        else
                        {
                            if (!located_function_scope->function_node->value_type->is_void())
                                lang_anylizer->lang_error(0x0000, a_ret, RS_ERR_CANNOT_RET_TYPE_AND_TYPE_AT_SAME_TIME, L"void", located_function_scope->function_node->value_type->type_name.c_str());
                        }
                    }
                }

            }
            else if (ast_sentence_block* a_sentence_blk = dynamic_cast<ast_sentence_block*>(ast_node))
            {
                this->begin_scope();
                analyze_pass1(a_sentence_blk->sentence_list);
                this->end_scope();
            }
            else if (ast_if* ast_if_sentence = dynamic_cast<ast_if*>(ast_node))
            {
                analyze_pass1(ast_if_sentence->judgement_value);

                if (ast_if_sentence->judgement_value->is_constant
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
            }
            else if (ast_while* ast_while_sentence = dynamic_cast<ast_while*>(ast_node))
            {
                analyze_pass1(ast_while_sentence->judgement_value);
                analyze_pass1(ast_while_sentence->execute_sentence);
            }
            else if (ast_except* ast_except_sent = dynamic_cast<ast_except*>(ast_node))
            {
                analyze_pass1(ast_except_sent->execute_sentence);
            }
            else if (ast_forloop* a_forloop = dynamic_cast<ast_forloop*>(ast_node))
            {
                begin_scope();

                analyze_pass1(a_forloop->pre_execute);
                analyze_pass1(a_forloop->judgement_expr);
                analyze_pass1(a_forloop->execute_sentences);
                analyze_pass1(a_forloop->after_execute);

                end_scope();
            }
            else if (ast_value_unary* a_value_unary = dynamic_cast<ast_value_unary*>(ast_node))
            {
                analyze_pass1(a_value_unary->val);
                a_value_unary->add_child(a_value_unary->val);

                if (a_value_unary->operate == +lex_type::l_lnot)
                    a_value_unary->value_type = new ast_type(L"int");
                else if (!a_value_unary->val->value_type->is_pending())
                {
                    a_value_unary->value_type = a_value_unary->val->value_type;
                }
            }
            else if (ast_mapping_pair* a_mapping_pair = dynamic_cast<ast_mapping_pair*>(ast_node))
            {
                analyze_pass1(a_mapping_pair->key);
                analyze_pass1(a_mapping_pair->val);
            }
            else if (ast_using_namespace* a_using_namespace = dynamic_cast<ast_using_namespace*>(ast_node))
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
                //        // lang_anylizer->lang_error(0x0000, a_using_namespace, RS_ERR_ERR_PLACE_FOR_USING_NAMESPACE);
                //        break;
                //    }
                //    parent_child = parent_child->sibling;
                //}

                now_scope()->used_namespace.push_back(a_using_namespace);
            }
            else if (ast_using_type_as* a_using_type_as = dynamic_cast<ast_using_type_as*>(ast_node))
            {
                // now_scope()->used_namespace.push_back(a_using_namespace);
                auto* typing_symb = define_type_in_this_scope(a_using_type_as, a_using_type_as->old_type, a_using_type_as->declear_attribute);
                typing_symb->apply_template_setting(a_using_type_as);
            }
            else if (ast_foreach* a_foreach = dynamic_cast<ast_foreach*>(ast_node))
            {
                begin_scope();

                analyze_pass1(a_foreach->used_vars_defines);

                for (auto& variable : a_foreach->foreach_var)
                    analyze_pass1(variable);
                analyze_pass1(a_foreach->iterator_var);

                analyze_pass1(a_foreach->iter_next_judge_expr);

                analyze_pass1(a_foreach->execute_sentences);

                end_scope();
            }
            else if (ast_check_type_with_naming_in_pass2* a_check_naming = dynamic_cast<ast_check_type_with_naming_in_pass2*>(ast_node))
            {
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
            }
            else
            {
                grammar::ast_base* child = ast_node->children;
                while (child)
                {
                    analyze_pass1(child);
                    child = child->sibling;
                }
            }

            if (ast_value* a_val = dynamic_cast<ast_value*>(ast_node))
            {
                if (ast_defines* a_def = dynamic_cast<ast_defines*>(ast_node);
                    a_def && a_def->is_template_define)
                {
                    // Do nothing
                }
                else
                {
                    if (a_val->value_type->is_pending())
                    {
                        // ready for update..
                        fully_update_type(a_val->value_type, true);
                    }
                    a_val->value_type->searching_begin_namespace_in_pass2 = now_scope();
                    // end if (ast_value* a_val = dynamic_cast<ast_value*>(ast_node))

                    a_val->update_constant_value(lang_anylizer);
                }

            }

        }

        ast::ast_value_function_define* analyze_pass_template_reification(ast::ast_value_function_define* origin_template_func_define, std::vector<ast::ast_type*> template_args_types)
        {
            using namespace ast;

            std::vector<uint32_t> template_args_hashtypes;
            for (auto temtype : template_args_types)
                template_args_hashtypes.push_back(get_typing_hash_after_pass1(temtype));

            ast_value_function_define* dumpped_template_func_define = nullptr;

            if (auto fnd = origin_template_func_define->template_typehashs_reification_instance_list.find(template_args_hashtypes);
                fnd != origin_template_func_define->template_typehashs_reification_instance_list.end())
            {
                return  dynamic_cast<ast_value_function_define*>(fnd->second);
            }

            dumpped_template_func_define = dynamic_cast<ast_value_function_define*>(origin_template_func_define->instance());
            rs_assert(dumpped_template_func_define);

            // Reset compile state..
            dumpped_template_func_define->symbol = nullptr;
            dumpped_template_func_define->searching_begin_namespace_in_pass2 = nullptr;
            dumpped_template_func_define->completed_in_pass2 = false;
            dumpped_template_func_define->is_template_define = false;
            dumpped_template_func_define->template_type_name_list.clear();
            dumpped_template_func_define->is_template_reification = true;

            dumpped_template_func_define->this_reification_template_args = template_args_types;

            origin_template_func_define->template_typehashs_reification_instance_list[template_args_hashtypes] =
                dumpped_template_func_define;

            temporary_entry_scope_in_pass1(origin_template_func_define->symbol->defined_in_scope);
            begin_template_scope(origin_template_func_define, template_args_types);

            analyze_pass1(dumpped_template_func_define);

            // origin_template_func_define->parent->add_child(dumpped_template_func_define);
            end_template_scope();
            temporary_leave_scope_in_pass1();

            lang_symbol* template_reification_symb = new lang_symbol;

            template_reification_symb->type = lang_symbol::symbol_type::function;
            template_reification_symb->name = dumpped_template_func_define->function_name;
            template_reification_symb->defined_in_scope = now_scope();
            template_reification_symb->attribute = dumpped_template_func_define->declear_attribute;
            template_reification_symb->attribute->add_attribute(lang_anylizer, +lex_type::l_const); // for stop: function = xxx;
            template_reification_symb->variable_value = dumpped_template_func_define;

            dumpped_template_func_define->this_reification_lang_symbol = template_reification_symb;

            lang_symbols.push_back(template_reification_symb);

            return dumpped_template_func_define;
        }

        void analyze_pass_template()
        {
            using namespace ast;
            do
            {
                auto _duped_template_variable_list_for_pass_template = template_variable_list_for_pass_template;
                template_variable_list_for_pass_template.clear();
                for (ast_value_variable* a_value_var : _duped_template_variable_list_for_pass_template)
                {
                    auto* sym = find_value_in_this_scope(a_value_var);
                    if (sym && sym->is_template_symbol)
                    {
                        if (!a_value_var->template_reification_args.empty())
                        {
                            if (sym->type != lang_symbol::symbol_type::function)
                                lang_anylizer->lang_error(0x0000, a_value_var, RS_ERR_NO_TEMPLATE_VARIABLE);
                            else
                            {
                                ast_value_function_define* dumpped_template_func_define = nullptr;
                                ast_value_function_define* origin_template_func_define = nullptr;

                                rs_assert(sym->function_overload_sets.size());
                                for (auto& func_overload : sym->function_overload_sets)
                                {
                                    if (!check_symbol_is_accessable(func_overload, func_overload->symbol, a_value_var->searching_begin_namespace_in_pass2, a_value_var, false))
                                        continue; // In template function, not accessable function will be skip!

                                    if (func_overload->is_template_define
                                        && func_overload->template_type_name_list.size() == a_value_var->template_reification_args.size())
                                    {
                                        // TODO: finding repeated template? goon

                                        dumpped_template_func_define = analyze_pass_template_reification(func_overload, a_value_var->template_reification_args);

                                        break;
                                    }
                                }

                                if (!dumpped_template_func_define)
                                    lang_anylizer->lang_error(0x0000, a_value_var, RS_ERR_NO_MATCHED_TEMPLATE_FUNC);
                                else
                                    a_value_var->symbol = dumpped_template_func_define->this_reification_lang_symbol; // apply symbol
                            }
                        }
                        else
                        {
                            // No template args.. continue..

                            continue;
                        }

                        a_value_var->value_type = a_value_var->symbol->variable_value->value_type;
                    }
                    else;
                    // Do nothing, error will be reported by pass2

                }
            } while (!template_variable_list_for_pass_template.empty());
        }

        bool write_flag_complete_in_pass2 = true;
        bool has_step_in_step2 = false;

        void start_trying_pass2()
        {
            write_flag_complete_in_pass2 = false;
        }
        void end_trying_pass2()
        {
            write_flag_complete_in_pass2 = true;
        }

        void analyze_pass2(grammar::ast_base* ast_node)
        {
            has_step_in_step2 = true;

            entry_pass ep1(in_pass2, true);

            rs_assert(ast_node);

            if (ast_node->completed_in_pass2)
                return;

            if (traving_node.find(ast_node) != traving_node.end())
                return;

            ast_node->completed_in_pass2 = write_flag_complete_in_pass2;

            struct traving_guard
            {
                lang* _lang;
                grammar::ast_base* _tving_node;
                traving_guard(lang* _lg, grammar::ast_base* ast_ndoe)
                    : _lang(_lg)
                    , _tving_node(ast_ndoe)
                {
                    _lang->traving_node.insert(_tving_node);
                }
                ~traving_guard()
                {
                    _lang->traving_node.erase(_tving_node);
                }
            };

            traving_guard g1(this, ast_node);

            using namespace ast;

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

                    if (a_value->value_type->is_pending())
                    {
                        // ready for update..
                        fully_update_type(a_value->value_type, false);

                        if (a_value->value_type->is_custom())
                            lang_anylizer->lang_error(0x0000, a_value, RS_ERR_UNKNOWN_TYPE
                                , a_value->value_type->get_type_name().c_str());
                    }
                    if (ast_value_type_check* ast_value_check = dynamic_cast<ast_value_type_check*>(a_value))
                    {
                        // ready for update..
                        fully_update_type(ast_value_check->aim_type, false);

                        if (ast_value_check->aim_type->is_custom())
                            lang_anylizer->lang_error(0x0000, ast_value_check, RS_ERR_UNKNOWN_TYPE
                                , ast_value_check->aim_type->get_type_name().c_str());

                        ast_value_check->update_constant_value(lang_anylizer);
                    }
                    if (a_value->value_type->is_pending())
                    {
                        if (ast_value_variable* a_value_var = dynamic_cast<ast_value_variable*>(a_value))
                        {
                            auto* sym = find_value_in_this_scope(a_value_var);
                            if (sym)
                            {
                                if (sym->type == lang_symbol::symbol_type::variable && !a_value_var->template_reification_args.empty())
                                    lang_anylizer->lang_error(0x0000, a_value_var, RS_ERR_NO_TEMPLATE_VARIABLE);

                                if (sym->define_in_function && !sym->has_been_defined_in_pass2)
                                    lang_anylizer->lang_error(0x0000, a_value_var, RS_ERR_UNKNOWN_IDENTIFIER, a_value_var->var_name.c_str());

                                analyze_pass2(sym->variable_value);
                                a_value_var->value_type = sym->variable_value->value_type;
                                a_value_var->symbol = sym;

                                if (a_value_var->value_type->is_pending())
                                {
                                    if (a_value_var->symbol->type != lang_symbol::symbol_type::function)
                                        lang_anylizer->lang_error(0x0000, a_value_var, RS_ERR_UNABLE_DECIDE_VAR_TYPE);
                                    else if (a_value_var->symbol->function_overload_sets.size() == 1
                                        && !a_value_var->symbol->is_template_symbol
                                        && a_value_var->template_reification_args.empty())
                                    {
                                        // only you~
                                        auto* result = sym->function_overload_sets.front();
                                        check_symbol_is_accessable(result, result->symbol, a_value_var->searching_begin_namespace_in_pass2, a_value_var);
                                        a_value_var->value_type = result->value_type;
                                    }
                                }

                            }
                            else
                            {
                                lang_anylizer->lang_error(0x0000, a_value_var, RS_ERR_UNKNOWN_IDENTIFIER, a_value_var->var_name.c_str());
                                a_value_var->value_type = new ast_type(L"pending");
                            }
                        }
                        else if (ast_value_index* a_value_idx = dynamic_cast<ast_value_index*>(ast_node))
                        {
                            analyze_pass2(a_value_idx->from);
                            analyze_pass2(a_value_idx->index);

                            if (!a_value_idx->from->value_type->class_member_index.empty())
                            {
                                if (a_value_idx->index->is_constant && a_value_idx->index->value_type->is_string())
                                {
                                    if (auto fnd =
                                        a_value_idx->from->value_type->class_member_index.find(
                                            str_to_wstr(*a_value_idx->index->get_constant_value().string)
                                        ); fnd != a_value_idx->from->value_type->class_member_index.end())
                                    {
                                        if (!fnd->second->value_type->is_pending())
                                        {
                                            a_value_idx->value_type = fnd->second->value_type;
                                        }
                                    }
                                    else
                                    {
                                        lang_anylizer->lang_error(0x0000, a_value_idx, RS_ERR_UNDEFINED_MEMBER,
                                            str_to_wstr(*a_value_idx->index->get_constant_value().string).c_str());
                                    }
                                }
                                else
                                {
                                    lang_anylizer->lang_error(0x0000, a_value_idx, RS_ERR_CANNOT_INDEX_MEMB_WITHOUT_STR);
                                }

                            }
                            else if (a_value_idx->from->value_type->is_string())
                            {
                                a_value_idx->value_type = new ast_type(L"string");
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
                                    a_value_idx->value_type = new ast_type(L"dynamic");
                                }
                                a_value_idx->can_be_assign = a_value_idx->from->can_be_assign;
                            }
                        }
                        else if (ast_value_function_define* a_value_funcdef = dynamic_cast<ast_value_function_define*>(a_value))
                        {
                            if (!a_value_funcdef->is_template_define)
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
                                            lang_anylizer->lang_error(0x0000, a_value_funcdef, RS_ERR_CANNOT_DERIV_FUNCS_RET_TYPE, rs::str_to_wstr(a_value_funcdef->get_ir_func_signature_tag()).c_str());

                                        a_value_funcdef->value_type->set_type_with_name(L"void");
                                    }
                                }
                            }
                        }
                        else if (ast_value_funccall* a_value_funccall = dynamic_cast<ast_value_funccall*>(ast_node))
                        {
                            if (a_value_funccall->callee_symbol_in_type_namespace)
                            {
                                analyze_pass2(a_value_funccall->directed_value_from);
                                if (!a_value_funccall->directed_value_from->value_type->is_pending() &&
                                    !a_value_funccall->directed_value_from->value_type->is_func())
                                {
                                    // trying finding type_function
                                    rs_assert(a_value_funccall->callee_symbol_in_type_namespace->scope_namespaces.empty());

                                    // TODO : CUSTOM TYPE INFORM..
                                    if (a_value_funccall->directed_value_from->value_type->using_type_name)
                                    {
                                        a_value_funccall->callee_symbol_in_type_namespace->scope_namespaces =
                                            a_value_funccall->directed_value_from->value_type->using_type_name->scope_namespaces;

                                        a_value_funccall->callee_symbol_in_type_namespace->scope_namespaces.push_back
                                        (a_value_funccall->directed_value_from->value_type->using_type_name->type_name);

                                        auto state = lang_anylizer->lex_enable_error_warn;
                                        lang_anylizer->lex_enable_error_warn = false;
                                        analyze_pass2(a_value_funccall->callee_symbol_in_type_namespace);
                                        lang_anylizer->lex_enable_error_warn = state;

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

                                    a_value_funccall->callee_symbol_in_type_namespace->scope_namespaces.clear();
                                    a_value_funccall->callee_symbol_in_type_namespace->scope_namespaces.push_back
                                    (a_value_funccall->directed_value_from->value_type->type_name);

                                    auto state = lang_anylizer->lex_enable_error_warn;
                                    lang_anylizer->lex_enable_error_warn = false;
                                    analyze_pass2(a_value_funccall->callee_symbol_in_type_namespace);
                                    lang_anylizer->lex_enable_error_warn = state;


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

                                    // not find func in custom type and basic type, try dynamic 
                                    if (!a_value_funccall->directed_value_from->value_type->is_dynamic())
                                    {
                                        a_value_funccall->callee_symbol_in_type_namespace->scope_namespaces.clear();
                                        a_value_funccall->callee_symbol_in_type_namespace->scope_namespaces.push_back(L"dynamic");

                                        auto state = lang_anylizer->lex_enable_error_warn;
                                        lang_anylizer->lex_enable_error_warn = false;
                                        analyze_pass2(a_value_funccall->callee_symbol_in_type_namespace);
                                        lang_anylizer->lex_enable_error_warn = state;

                                        if (!a_value_funccall->callee_symbol_in_type_namespace->value_type->is_pending()
                                            || a_value_funccall->callee_symbol_in_type_namespace->value_type->is_pending_function()
                                            || (a_value_funccall->callee_symbol_in_type_namespace->symbol
                                                && a_value_funccall->callee_symbol_in_type_namespace->symbol->type == lang_symbol::symbol_type::function))
                                        {
                                            if (auto* old_callee_func = dynamic_cast<ast::ast_value_variable*>(a_value_funccall->called_func))
                                                a_value_funccall->callee_symbol_in_type_namespace->template_reification_args
                                                = old_callee_func->template_reification_args;

                                            a_value_funccall->called_func = a_value_funccall->callee_symbol_in_type_namespace;
                                        }
                                    }
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
                                        std::vector<ast_value_function_define*> need_cast_sets;
                                        std::vector<ast_value_function_define*> need_cast_sets_template;
                                        std::vector<ast_value_function_define*> variadic_sets;
                                        std::vector<ast_value_function_define*> variadic_sets_template;

                                        for (auto* _override_func : called_funcsymb->symbol->function_overload_sets)
                                        {
                                            if (!check_symbol_is_accessable(_override_func, _override_func->symbol, called_funcsymb->searching_begin_namespace_in_pass2, called_funcsymb, false))
                                                continue; // In function override judge, not accessable function will be skip!

                                            auto* override_func = dynamic_cast<ast_value_function_define*>(_override_func);
                                            rs_test(override_func);

                                            bool best_match = true;
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
                                                        lang_anylizer->lang_error(0x0000, templ_arg, RS_ERR_UNKNOWN_TYPE,
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

                                            analyze_pass2(_override_func);

                                            real_args = a_value_funccall->arguments->children;
                                            form_args = override_func->argument_list->children;
                                            do
                                            {
                                                auto* form_arg = dynamic_cast<ast_value_arg_define*>(form_args);
                                                auto* real_arg = dynamic_cast<ast_value*>(real_args);

                                                rs_test(real_args == real_arg);

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
                                                    best_match = false;
                                                    auto ecount = a_fakevalue_unpack_args->expand_count;
                                                    if (0 == ecount)
                                                    {
                                                        // all in!!!
                                                        if (with_template)
                                                            variadic_sets.push_back(override_func);
                                                        else
                                                            variadic_sets_template.push_back(override_func);

                                                        goto this_function_override_checking_over;
                                                    }

                                                    while (ecount)
                                                    {
                                                        if (form_arg)
                                                        {
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
                                                else if (real_arg->value_type->is_same(form_arg->value_type, false))
                                                    ;// do nothing..
                                                else if (ast_type::check_castable(form_arg->value_type, real_arg->value_type, false))
                                                    best_match = false;
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
                                                        if (best_match)
                                                        {
                                                            if (with_template)
                                                                best_match_sets_template.push_back(override_func);
                                                            else
                                                                best_match_sets.push_back(override_func);
                                                        }
                                                        else
                                                        {
                                                            if (with_template)
                                                                need_cast_sets_template.push_back(override_func);
                                                            else
                                                                need_cast_sets.push_back(override_func);
                                                        }

                                                    }
                                                    // else: bad match..
                                                }
                                            } while (form_args);

                                        this_function_override_checking_over:;

                                        }

                                        std::vector<ast_value_function_define*>* judge_sets = nullptr;
                                        if (best_match_sets.size())
                                            judge_sets = &best_match_sets;
                                        else if (best_match_sets_template.size())
                                            judge_sets = &best_match_sets_template;
                                        else if (need_cast_sets.size())
                                            judge_sets = &need_cast_sets;
                                        else if (need_cast_sets_template.size())
                                            judge_sets = &need_cast_sets_template;
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
                                                        + judge_sets->at(index)->value_type->get_type_name()
                                                        + L"' " RS_TERM_AT L" ("
                                                        + std::to_wstring(judge_sets->at(index)->row_no)
                                                        + L","
                                                        + std::to_wstring(judge_sets->at(index)->col_no)
                                                        + L")";

                                                    if (index + 1 != judge_sets->size())
                                                    {
                                                        acceptable_func += L" " RS_TERM_OR L" ";
                                                    }
                                                }
                                                this->lang_anylizer->lang_error(0x0000, a_value_funccall, RS_ERR_UNABLE_DECIDE_FUNC_OVERRIDE, acceptable_func.c_str());
                                            }
                                            else
                                            {
                                                a_value_funccall->called_func = judge_sets->front();
                                                analyze_pass2(a_value_funccall->called_func);
                                            }
                                        }
                                        else
                                        {
                                            this->lang_anylizer->lang_error(0x0000, a_value_funccall, RS_ERR_NO_MATCH_FUNC_OVERRIDE);
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
                                        lang_anylizer->lang_error(0x0000, funcsymb, RS_ERR_CANNOT_DERIV_FUNCS_RET_TYPE, rs::str_to_wstr(funcsymb->get_ir_func_signature_tag()).c_str());

                                    funcsymb->value_type->set_ret_type(new ast_type(L"void"));
                                    funcsymb->auto_adjust_return_type = false;
                                }

                            }

                            if (a_value_funccall->called_func
                                && a_value_funccall->called_func->value_type->is_func()
                                && !a_value_funccall->called_func->value_type->is_pending())
                            {
                                auto* real_args = a_value_funccall->arguments->children;
                                a_value_funccall->arguments->remove_allnode();

                                for (auto a_type_index = a_value_funccall->called_func->value_type->argument_types.begin();
                                    a_type_index != a_value_funccall->called_func->value_type->argument_types.end();
                                    a_type_index++)
                                {
                                    if (!real_args)
                                    {
                                        // default arg mgr here, now just kill
                                        lang_anylizer->lang_error(0x0000, a_value_funccall, RS_ERR_ARGUMENT_TOO_FEW, a_value_funccall->called_func->value_type->get_type_name().c_str());
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
                                            if (0 == ecount)
                                            {
                                                // all in!!!
                                                ecount =
                                                    (rs_integer_t)(
                                                        a_value_funccall->called_func->value_type->argument_types.end()
                                                        - a_type_index);
                                                if (a_value_funccall->called_func->value_type->is_variadic_function_type)
                                                    a_fakevalue_unpack_args->expand_count = -ecount;
                                                else
                                                    a_fakevalue_unpack_args->expand_count = ecount;
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
                                                    lang_anylizer->lang_error(0x0000, a_value_funccall, RS_ERR_ARGUMENT_TOO_MANY, a_value_funccall->called_func->value_type->get_type_name().c_str());
                                                    break;
                                                }
                                                ecount--;
                                            }
                                            a_type_index--;
                                        }
                                        else if (dynamic_cast<ast_value_takeplace*>(arg_val))
                                            continue;
                                        else
                                        {
                                            if (!arg_val->value_type->is_pending() && !arg_val->value_type->is_same(*a_type_index, false))
                                            {
                                                auto* cast_arg_type = new ast_value_type_cast(arg_val, *a_type_index, true);

                                                cast_arg_type->col_no = arg_val->col_no;
                                                cast_arg_type->row_no = arg_val->row_no;
                                                cast_arg_type->source_file = arg_val->source_file;
                                                analyze_pass2(cast_arg_type);

                                                cast_arg_type->update_constant_value(lang_anylizer);

                                                a_value_funccall->arguments->add_child(cast_arg_type);
                                            }
                                            else
                                            {
                                                a_value_funccall->arguments->add_child(arg_val);
                                            }
                                        }
                                    }
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
                                    if (dynamic_cast<ast_fakevalue_unpacked_args*>(real_args))
                                    {
                                        // TODO MARK NOT NEED EXPAND HERE
                                    }
                                    else
                                        lang_anylizer->lang_error(0x0000, a_value_funccall, RS_ERR_ARGUMENT_TOO_MANY, a_value_funccall->called_func->value_type->get_type_name().c_str());
                                }
                            }
                            else if (!a_value_funccall->called_func->value_type->is_pending())
                            {
                                lang_anylizer->lang_error(0x0000, a_value, RS_ERR_TYPE_CANNOT_BE_CALL,
                                    a_value_funccall->called_func->value_type->get_type_name(false).c_str());
                            }
                        }
                        else if (ast_value_unary* a_value_unary = dynamic_cast<ast_value_unary*>(ast_node))
                        {
                            analyze_pass2(a_value_unary->val);

                            if (a_value_unary->operate == +lex_type::l_lnot)
                                a_value_unary->value_type = new ast_type(L"int");
                            else if (!a_value_unary->val->value_type->is_pending())
                                a_value_unary->value_type = a_value_unary->val->value_type;
                            // else
                                // not need to manage, if val is pending, other place will give error.
                        }
                        else if (ast_value_array* a_value_arr = dynamic_cast<ast_value_array*>(ast_node))
                        {
                            analyze_pass2(a_value_arr->array_items);
                            // 
                            ast_type* decide_array_item_type = new ast_type(L"dynamic");

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

                                if (!val->value_type->is_same(decide_array_item_type, false))
                                {
                                    auto* mixed_type = ast_value_binary::binary_upper_type(decide_array_item_type, val->value_type);
                                    if (mixed_type)
                                    {
                                        decide_array_item_type->set_type_with_name(mixed_type->type_name);
                                    }
                                    else
                                    {
                                        decide_array_item_type->set_type_with_name(L"dynamic");
                                        // lang_anylizer->lang_warning(0x0000, a_ret, RS_WARN_FUNC_WILL_RETURN_DYNAMIC);
                                    }
                                }
                                val = dynamic_cast<ast_value*>(val->sibling);
                            }

                            if (decide_array_item_type)
                            {
                                a_value_arr->value_type->template_arguments[0] = decide_array_item_type;
                            }
                        }
                        else if (ast_value_mapping* a_value_map = dynamic_cast<ast_value_mapping*>(ast_node))
                        {
                            analyze_pass2(a_value_map->mapping_pairs);

                            ast_type* decide_map_key_type = new ast_type(L"dynamic");
                            ast_type* decide_map_val_type = new ast_type(L"dynamic");

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

                                if (!map_pair->key->value_type->is_same(decide_map_key_type, false))
                                {
                                    auto* mixed_type = ast_value_binary::binary_upper_type(decide_map_key_type, map_pair->key->value_type);
                                    if (mixed_type)
                                    {
                                        decide_map_key_type->set_type_with_name(mixed_type->type_name);
                                    }
                                    else
                                    {
                                        decide_map_key_type->set_type_with_name(L"dynamic");
                                        // lang_anylizer->lang_warning(0x0000, a_ret, RS_WARN_FUNC_WILL_RETURN_DYNAMIC);
                                    }
                                }
                                if (!map_pair->val->value_type->is_same(decide_map_val_type, false))
                                {
                                    auto* mixed_type = ast_value_binary::binary_upper_type(decide_map_val_type, map_pair->val->value_type);
                                    if (mixed_type)
                                    {
                                        decide_map_val_type->set_type_with_name(mixed_type->type_name);
                                    }
                                    else
                                    {
                                        decide_map_val_type->set_type_with_name(L"dynamic");
                                        // lang_anylizer->lang_warning(0x0000, a_ret, RS_WARN_FUNC_WILL_RETURN_DYNAMIC);
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
                    }

                    //
                    if (ast_value_function_define* a_value_funcdef = dynamic_cast<ast_value_function_define*>(a_value))
                    {
                        if (!a_value_funcdef->is_template_define)
                        {
                            // return-type adjust complete. do 'return' cast;
                            a_value_funcdef->auto_adjust_return_type = false;// stop using auto-adjust
                            if (a_value_funcdef->in_function_sentence)
                            {
                                analyze_pass2(a_value_funcdef->in_function_sentence);
                            }
                        }
                    }
                    else if (ast_value_assign* a_value_assi = dynamic_cast<ast_value_assign*>(ast_node))
                    {
                        analyze_pass2(a_value_assi->left);
                        analyze_pass2(a_value_assi->right);

                        if (auto lsymb = dynamic_cast<ast_value_symbolable_base*>(a_value_assi->left)
                            ; lsymb && lsymb->symbol)
                            lsymb->symbol->has_been_assigned = true;

                        if (a_value_assi->right->value_type->is_pending_function())
                        {
                            auto* try_finding_override = new ast_value_type_cast(a_value_assi->right, a_value_assi->value_type, true);
                            try_finding_override->col_no = a_value_assi->right->col_no;
                            try_finding_override->row_no = a_value_assi->right->row_no;
                            try_finding_override->source_file = a_value_assi->right->source_file;

                            analyze_pass2(try_finding_override);
                            try_finding_override->update_constant_value(lang_anylizer);

                            a_value_assi->right = try_finding_override;
                        }

                        if (!ast_type::check_castable(a_value_assi->left->value_type, a_value_assi->right->value_type, false))
                        {
                            lang_anylizer->lang_error(0x0000, a_value_assi, RS_ERR_CANNOT_ASSIGN_TYPE_TO_TYPE,
                                a_value_assi->right->value_type->get_type_name(false).c_str(),
                                a_value_assi->left->value_type->get_type_name(false).c_str());
                        }

                        if (!a_value_assi->left->can_be_assign)
                        {
                            lang_anylizer->lang_error(0x0000, a_value_assi, RS_ERR_CANNOT_ASSIGN_TO_UNASSABLE_ITEM);
                        }
                    }
                    else if (ast_value_type_cast* a_value_typecast = dynamic_cast<ast_value_type_cast*>(a_value))
                    {
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
                                        if (overload_func->value_type->is_same(a_value_typecast->value_type))
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
                                lang_anylizer->lang_error(0x0000, a_value, RS_ERR_CANNOT_GET_FUNC_OVERRIDE_WITH_TYPE,
                                    a_variable_sym->var_name.c_str(),
                                    a_value_typecast->value_type->get_type_name().c_str());
                            }
                        }
                        else
                        {
                        just_do_simple_type_cast:
                            if (!ast_type::check_castable(a_value_typecast->value_type, origin_value->value_type, !a_value_typecast->implicit))
                            {
                                if (a_value_typecast->implicit)
                                    lang_anylizer->lang_error(0x0000, a_value, RS_ERR_CANNOT_IMPLCAST_TYPE_TO_TYPE,
                                        origin_value->value_type->get_type_name(false).c_str(),
                                        a_value_typecast->value_type->get_type_name(false).c_str()
                                    );
                                else
                                    lang_anylizer->lang_error(0x0000, a_value, RS_ERR_CANNOT_CAST_TYPE_TO_TYPE,
                                        origin_value->value_type->get_type_name(false).c_str(),
                                        a_value_typecast->value_type->get_type_name(false).c_str()
                                    );
                            }
                        }
                    }
                    else if (ast_value_type_judge* ast_value_judge = dynamic_cast<ast_value_type_judge*>(ast_node))
                    {
                        if (ast_value_judge->is_mark_as_using_ref)
                            ast_value_judge->_be_cast_value_node->is_mark_as_using_ref = true;

                        analyze_pass2(ast_value_judge->_be_cast_value_node);
                    }
                    else if (ast_value_type_check* a_value_type_check = dynamic_cast<ast_value_type_check*>(ast_node))
                    {
                        analyze_pass2(a_value_type_check->_be_check_value_node);
                        a_value_type_check->update_constant_value(lang_anylizer);
                    }
                    else if (ast_value_index* a_value_index = dynamic_cast<ast_value_index*>(ast_node))
                    {
                        analyze_pass2(a_value_index->from);
                        analyze_pass2(a_value_index->index);

                        if (!a_value_index->from->value_type->is_array()
                            && !a_value_index->from->value_type->is_map()
                            && !a_value_index->from->value_type->is_string()
                            && !a_value_index->from->value_type->is_dynamic())
                        {
                            lang_anylizer->lang_error(0x0000, a_value_index->from, RS_ERR_UNINDEXABLE_TYPE
                                , a_value_index->from->value_type->get_type_name().c_str());
                        }

                        if (a_value_index->from->value_type->is_array() || a_value_index->from->value_type->is_string())
                        {
                            if (!a_value_index->index->value_type->is_integer() && !a_value_index->index->value_type->is_handle())
                            {
                                auto* index_val = new ast_value_type_cast(a_value_index->index, new ast_type(L"int"), true);
                                index_val->col_no = a_value_index->index->col_no;
                                index_val->row_no = a_value_index->index->row_no;
                                index_val->source_file = a_value_index->index->source_file;

                                analyze_pass2(index_val);

                                index_val->update_constant_value(lang_anylizer);

                                a_value_index->index = index_val;
                            }
                        }
                        if (a_value_index->from->value_type->is_map())
                        {
                            if (!a_value_index->index->value_type->is_same(a_value_index->from->value_type->template_arguments[0]))
                            {
                                auto* index_val = new ast_value_type_cast(a_value_index->index, a_value_index->from->value_type->template_arguments[0], true);
                                // pass_type_cast::do_cast(*lang_anylizer, a_value_index->index, );
                                index_val->col_no = a_value_index->index->col_no;
                                index_val->row_no = a_value_index->index->row_no;
                                index_val->source_file = a_value_index->index->source_file;

                                analyze_pass2(index_val);

                                index_val->update_constant_value(lang_anylizer);

                                a_value_index->index = index_val;
                            }
                        }

                        if (!a_value_index->from->value_type->is_string())
                            a_value_index->can_be_assign = a_value_index->from->can_be_assign;
                    }
                    else if (ast_value_indexed_variadic_args* a_value_variadic_args_idx = dynamic_cast<ast_value_indexed_variadic_args*>(ast_node))
                    {
                        analyze_pass2(a_value_variadic_args_idx->argindex);

                        if (!a_value_variadic_args_idx->argindex->value_type->is_integer())
                        {
                            auto* cast_return_type = new ast_value_type_cast(a_value_variadic_args_idx->argindex, new ast_type(L"int"), true);
                            //pass_type_cast::do_cast(*lang_anylizer, a_value_variadic_args_idx->argindex, new ast_type(L"int"));
                            cast_return_type->col_no = a_value_variadic_args_idx->col_no;
                            cast_return_type->row_no = a_value_variadic_args_idx->row_no;
                            cast_return_type->source_file = a_value_variadic_args_idx->source_file;

                            analyze_pass2(cast_return_type);

                            cast_return_type->update_constant_value(lang_anylizer);

                            a_value_variadic_args_idx->argindex = cast_return_type;
                        }
                    }
                    else if (ast_fakevalue_unpacked_args* a_fakevalue_unpacked_args = dynamic_cast<ast_fakevalue_unpacked_args*>(ast_node))
                    {
                        analyze_pass2(a_fakevalue_unpacked_args->unpacked_pack);
                        if (!a_fakevalue_unpacked_args->unpacked_pack->value_type->is_array() && !a_fakevalue_unpacked_args->unpacked_pack->value_type->is_dynamic())
                        {
                            lang_anylizer->lang_error(0x0000, a_fakevalue_unpacked_args, RS_ERR_NEED_TYPES, L"array");
                        }
                    }
                    else if (ast_value_binary* a_value_bin = dynamic_cast<ast_value_binary*>(a_value))
                    {
                        analyze_pass2(a_value_bin->left);
                        analyze_pass2(a_value_bin->right);

                        if (a_value_bin->overrided_operation_call)
                        {
                            auto state = lang_anylizer->lex_enable_error_warn;
                            lang_anylizer->lex_enable_error_warn = false;

                            analyze_pass2(a_value_bin->overrided_operation_call);

                            lang_anylizer->lex_enable_error_warn = state;

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
                                else if (!a_value_bin->value_type->is_same(a_value_bin->overrided_operation_call->value_type))
                                    lang_anylizer->lang_warning(0x0000, a_value_bin, L"无法兼容重置运算操作和原始运算类型，这可能导致类型推导错误，继续");
                            }

                        }

                        if (nullptr == a_value_bin->value_type)
                        {
                            lang_anylizer->lang_error(0x0000, a_value_bin, RS_ERR_CANNOT_CALC_WITH_L_AND_R);
                            a_value_bin->value_type = new ast_type(L"pending");
                        }
                    }
                    else if (ast_value_logical_binary* a_value_logic_bin = dynamic_cast<ast_value_logical_binary*>(ast_node))
                    {
                        analyze_pass2(a_value_logic_bin->left);
                        analyze_pass2(a_value_logic_bin->right);

                        if (a_value_logic_bin->overrided_operation_call)
                        {
                            auto state = lang_anylizer->lex_enable_error_warn;
                            lang_anylizer->lex_enable_error_warn = false;

                            analyze_pass2(a_value_logic_bin->overrided_operation_call);

                            lang_anylizer->lex_enable_error_warn = state;

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
                                else if (!a_value_logic_bin->value_type->is_same(a_value_logic_bin->overrided_operation_call->value_type))
                                    lang_anylizer->lang_warning(0x0000, a_value_logic_bin, L"无法兼容重置运算操作和原始运算类型，这可能导致类型推导错误，继续");
                            }

                        }
                    }
                    else if (ast_value_array* a_value_arr = dynamic_cast<ast_value_array*>(ast_node))
                    {
                        analyze_pass2(a_value_arr->array_items);

                        if (!a_value_arr->value_type->is_array())
                        {
                            lang_anylizer->lang_error(0x0000, a_value_arr, RS_ERR_CANNOT_CAST_TYPE_TO_TYPE,
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
                                if (!val->value_type->is_same(a_value_arr->value_type->template_arguments[0], false))
                                {
                                    auto* cast_array_item = new ast_value_type_cast(val, a_value_arr->value_type->template_arguments[0], false);
                                    //pass_type_cast::do_cast(*lang_anylizer, val, a_value_arr->value_type->template_arguments[0]);
                                    cast_array_item->col_no = val->col_no;
                                    cast_array_item->row_no = val->row_no;
                                    cast_array_item->source_file = val->source_file;

                                    analyze_pass2(cast_array_item);

                                    cast_array_item->update_constant_value(lang_anylizer);

                                    reenplace_array_items.push_back(cast_array_item);
                                }
                                else
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

                    }
                    else if (ast_value_mapping* a_value_map = dynamic_cast<ast_value_mapping*>(ast_node))
                    {
                        analyze_pass2(a_value_map->mapping_pairs);

                        if (!a_value_map->value_type->is_map())
                        {
                            lang_anylizer->lang_error(0x0000, a_value_map, RS_ERR_CANNOT_CAST_TYPE_TO_TYPE,
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
                                    if (!pairs->key->value_type->is_same(a_value_map->value_type->template_arguments[0], false))
                                    {
                                        auto* cast_key_item = new ast_value_type_cast(pairs->key, a_value_map->value_type->template_arguments[0], false);
                                        //pass_type_cast::do_cast(*lang_anylizer, );
                                        cast_key_item->col_no = pairs->key->col_no;
                                        cast_key_item->row_no = pairs->key->row_no;
                                        cast_key_item->source_file = pairs->key->source_file;

                                        analyze_pass2(cast_key_item);

                                        cast_key_item->update_constant_value(lang_anylizer);

                                        pairs->key = cast_key_item;
                                    }
                                    if (!pairs->val->value_type->is_same(a_value_map->value_type->template_arguments[1], false))
                                    {
                                        auto* cast_val_item = new ast_value_type_cast(pairs->val, a_value_map->value_type->template_arguments[1], false);
                                        //pass_type_cast::do_cast(*lang_anylizer, pairs->val, a_value_map->value_type->template_arguments[1]);
                                        cast_val_item->col_no = pairs->key->col_no;
                                        cast_val_item->row_no = pairs->key->row_no;
                                        cast_val_item->source_file = pairs->key->source_file;

                                        analyze_pass2(cast_val_item);

                                        cast_val_item->update_constant_value(lang_anylizer);

                                        pairs->val = cast_val_item;
                                    }
                                }
                                pairs = dynamic_cast<ast_mapping_pair*>(pairs->sibling);
                            }
                        }
                    }

                }
            }

            /////////////////////////////////////////////////////////////////////////////////////////////////
            if (ast_mapping_pair* a_mapping_pair = dynamic_cast<ast_mapping_pair*>(ast_node))
            {
                analyze_pass2(a_mapping_pair->key);
                analyze_pass2(a_mapping_pair->val);
            }
            else if (ast_return* a_ret = dynamic_cast<ast_return*>(ast_node))
            {
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
                                if (!func_return_type->is_same(a_ret->return_value->value_type, false))
                                {
                                    auto* mixed_type = ast_value_binary::binary_upper_type(func_return_type, a_ret->return_value->value_type);
                                    if (mixed_type)
                                    {
                                        a_ret->located_function->value_type->set_type_with_name(mixed_type->type_name);
                                    }
                                    else
                                    {
                                        a_ret->located_function->value_type->set_type_with_name(L"dynamic");
                                        lang_anylizer->lang_warning(0x0000, a_ret, RS_WARN_FUNC_WILL_RETURN_DYNAMIC);
                                    }
                                }
                            }
                        }
                        else
                        {
                            if (!func_return_type->is_pending()
                                && !func_return_type->is_same(a_ret->return_value->value_type))
                            {
                                auto* cast_return_type = new ast_value_type_cast(a_ret->return_value, func_return_type, true);
                                // pass_type_cast::do_cast(*lang_anylizer, a_ret->return_value, func_return_type);
                                cast_return_type->col_no = a_ret->col_no;
                                cast_return_type->row_no = a_ret->row_no;
                                cast_return_type->source_file = a_ret->source_file;

                                analyze_pass2(cast_return_type);

                                cast_return_type->update_constant_value(lang_anylizer);

                                a_ret->return_value = cast_return_type;
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
                            lang_anylizer->lang_error(0x0000, a_ret, RS_ERR_CANNOT_RET_TYPE_AND_TYPE_AT_SAME_TIME, L"void", a_ret->located_function->value_type->type_name.c_str());
                        }
                    }
                    else
                    {
                        if (!a_ret->located_function->value_type->is_void())
                            lang_anylizer->lang_error(0x0000, a_ret, RS_ERR_CANNOT_RET_TYPE_AND_TYPE_AT_SAME_TIME, L"void", a_ret->located_function->value_type->type_name.c_str());
                    }
                }

            }
            else if (ast_sentence_block* a_sentence_blk = dynamic_cast<ast_sentence_block*>(ast_node))
            {
                analyze_pass2(a_sentence_blk->sentence_list);
            }
            else if (ast_if* ast_if_sentence = dynamic_cast<ast_if*>(ast_node))
            {
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


            }
            else if (ast_while* ast_while_sentence = dynamic_cast<ast_while*>(ast_node))
            {
                analyze_pass2(ast_while_sentence->judgement_value);
                analyze_pass2(ast_while_sentence->execute_sentence);
            }
            else if (ast_except* ast_except_sent = dynamic_cast<ast_except*>(ast_node))
            {
                analyze_pass2(ast_except_sent->execute_sentence);
            }
            else if (ast_forloop* a_forloop = dynamic_cast<ast_forloop*>(ast_node))
            {
                if (a_forloop->pre_execute)
                    analyze_pass2(a_forloop->pre_execute);
                if (a_forloop->judgement_expr)
                    analyze_pass2(a_forloop->judgement_expr);

                analyze_pass2(a_forloop->execute_sentences);

                if (a_forloop->after_execute)
                    analyze_pass2(a_forloop->after_execute);
            }
            else if (ast_foreach* a_foreach = dynamic_cast<ast_foreach*>(ast_node))
            {

                analyze_pass2(a_foreach->used_vars_defines);

                /*
                for (auto& variable : a_foreach->foreach_var)
                    analyze_pass2(variable);
                */

                auto state = lang_anylizer->lex_enable_error_warn;
                lang_anylizer->lex_enable_error_warn = false;

                analyze_pass2(a_foreach->iterator_var);
                analyze_pass2(a_foreach->iter_next_judge_expr->directed_value_from);

                // Try Getting next Function

                ast_value_variable* next_func_symb_getter = new ast_value_variable(L"next");
                next_func_symb_getter->searching_from_type = a_foreach->iter_next_judge_expr->directed_value_from->value_type;
                analyze_pass2(next_func_symb_getter);

                if (next_func_symb_getter->symbol
                    && next_func_symb_getter->symbol->type != lang_symbol::symbol_type::template_typing
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
                    need_takeplace_count -= (int)a_foreach->foreach_var.size();
                    need_takeplace_count -= 1;//iter

                    lang_anylizer->lex_enable_error_warn = true;
                    if (need_takeplace_count < 0)
                        lang_anylizer->lang_error(0x0000, a_foreach, RS_ERR_TOO_MANY_ITER_ITEM_FROM_NEXT
                            , a_foreach->iter_next_judge_expr->directed_value_from->value_type
                            ->get_type_name(false).c_str()
                            , a_foreach->foreach_var.size());
                    lang_anylizer->lex_enable_error_warn = false;

                    rs_assert(nullptr == a_foreach->iter_next_judge_expr->arguments->children);

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

                    lang_anylizer->lex_enable_error_warn = true;
                    if (a_foreach->iter_next_judge_expr->called_func->value_type
                        ->is_variadic_function_type)
                    {
                        lang_anylizer->lang_error(0x0000, a_foreach, RS_ERR_VARIADIC_NEXT_IS_ILEAGAL,
                            a_foreach->iter_next_judge_expr->directed_value_from->value_type
                            ->get_type_name(false).c_str());
                    }
                    lang_anylizer->lex_enable_error_warn = false;

                    int rndidx = (int)a_foreach->foreach_var.size() - 1;
                    for (auto ridx = a_foreach->iter_next_judge_expr->called_func->value_type->argument_types.rbegin();
                        ridx != a_foreach->iter_next_judge_expr->called_func->value_type->argument_types.rend();
                        ridx++)
                    {
                        a_foreach->foreach_var[rndidx]->completed_in_pass2 = false;
                        a_foreach->foreach_var[rndidx--]->symbol->variable_value->value_type->set_type(*ridx);

                        if (rndidx < 0)
                            break;
                    }

                    rs_assert(nullptr == a_foreach->iter_next_judge_expr->arguments->children);

                    a_foreach->iter_next_judge_expr->arguments->append_at_end(a_foreach->iterator_var);
                    for (int i = 0; i < need_takeplace_count; i++)
                        a_foreach->iter_next_judge_expr->arguments->append_at_end(new ast_value_takeplace);
                    for (auto& variable : a_foreach->foreach_var)
                        a_foreach->iter_next_judge_expr->arguments->append_at_end(variable);
                }
                else
                {
                    // Do not find symbol, but do nothing.. error has been reported by analyze_pass2
                }
                lang_anylizer->lex_enable_error_warn = state;

                analyze_pass2(a_foreach->iter_next_judge_expr);
                analyze_pass2(a_foreach->execute_sentences);
            }
            else if (ast_varref_defines* a_varref_defs = dynamic_cast<ast_varref_defines*>(ast_node))
            {
                for (auto& varref : a_varref_defs->var_refs)
                {
                    varref.symbol->has_been_defined_in_pass2 = true;
                    if (varref.is_ref)
                    {
                        varref.init_val->is_mark_as_using_ref = true;

                        if (auto* a_val_symb = dynamic_cast<ast_value_symbolable_base*>(varref.init_val);
                            (a_val_symb && a_val_symb->symbol && a_val_symb->symbol->attribute->is_constant_attr())
                            || !varref.init_val->can_be_assign)
                        {
                            lang_anylizer->lang_error(0x0000, varref.init_val, RS_ERR_CANNOT_MAKE_UNASSABLE_ITEM_REF);
                        }
                    }
                }
            }
            else if (ast_check_type_with_naming_in_pass2* a_check_naming = dynamic_cast<ast_check_type_with_naming_in_pass2*>(ast_node))
            {
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
            }

            ast_value_type_judge* a_value_type_judge_for_attrb = dynamic_cast<ast_value_type_judge*>(ast_node);
            ast_value_symbolable_base* a_value_base_for_attrb = dynamic_cast<ast_value_symbolable_base*>(ast_node);
            if (ast_value_symbolable_base* a_value_base = a_value_base_for_attrb;
                a_value_base ||
                (a_value_type_judge_for_attrb &&
                    (a_value_base = dynamic_cast<ast_value_symbolable_base*>(a_value_type_judge_for_attrb->_be_cast_value_node))
                    )
                )
            {
                if (a_value_base->symbol && a_value_base->symbol->attribute->is_constant_attr())
                    a_value_base->can_be_assign = false;

                if (a_value_base->is_mark_as_using_ref)
                {
                    if (a_value_base->symbol && a_value_base->can_be_assign)
                        a_value_base->symbol->has_been_assigned = true;
                    else
                        lang_anylizer->lang_error(0x0000, a_value_base, RS_ERR_CANNOT_MAKE_UNASSABLE_ITEM_REF);
                }

                // DONOT SWAP THESE TWO SENTENCES, BECAUSE has_been_assigned IS NOT 
                // DECIDED BY a_value_base->symbol->is_ref

                if (a_value_base->symbol && a_value_base->symbol->is_ref)
                    a_value_base->is_ref_ob_in_finalize = true;

                if (!a_value_base_for_attrb)
                {
                    a_value_type_judge_for_attrb->is_mark_as_using_ref = a_value_base->is_mark_as_using_ref;
                    a_value_type_judge_for_attrb->is_ref_ob_in_finalize = a_value_base->is_ref_ob_in_finalize;
                }
            }

            grammar::ast_base* child = ast_node->children;
            while (child)
            {
                analyze_pass2(child);
                child = child->sibling;
            }

            ast_node->completed_in_pass2 = write_flag_complete_in_pass2;
        }

        void clean_and_close_lang()
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

        ast::ast_type* analyze_template_derivation(
            const std::wstring& temp_form,
            const std::vector<std::wstring>& termplate_set,
            ast::ast_type* para, ast::ast_type* args)
        {
            // Must match all formal
            if (!para->is_like(args, termplate_set, &para, &args))
            {
                if (!para->is_func()
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

            if (para->is_complex() && args->is_complex())
            {
                if (auto* derivation_result = analyze_template_derivation(
                    temp_form,
                    termplate_set,
                    para->complex_type,
                    args->complex_type))
                    return derivation_result;
            }

            if (para->type_name == temp_form
                && para->scope_namespaces.empty()
                && !para->search_from_global_namespace)
            {
                if (para->is_func())
                {
                    // T(...) should return args..
                    return args->get_return_type();
                }
                else
                    return args;
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

        // register mapping.... fxxk
        std::vector<bool> assigned_t_register_list = std::vector<bool>(opnum::reg::T_REGISTER_COUNT);   // will assign t register
        std::vector<bool> assigned_r_register_list = std::vector<bool>(opnum::reg::R_REGISTER_COUNT);   // will assign r register

        opnum::opnumbase& get_useable_register_for_pure_value()
        {
            using namespace ast;
            using namespace opnum;
#define RS_NEW_OPNUM(...) (*generated_opnum_list_for_clean.emplace_back(new __VA_ARGS__))
            for (size_t i = 0; i < opnum::reg::T_REGISTER_COUNT; i++)
            {
                if (!assigned_t_register_list[i])
                {
                    assigned_t_register_list[i] = true;
                    return RS_NEW_OPNUM(reg(reg::t0 + (uint8_t)i));
                }
            }
            rs_error("cannot get a useable register..");
            return RS_NEW_OPNUM(reg(reg::cr));
        }
        void _complete_using_register_for_pure_value(opnum::opnumbase& completed_reg)
        {
            using namespace ast;
            using namespace opnum;
            if (auto* reg_ptr = dynamic_cast<opnum::reg*>(&completed_reg);
                reg_ptr && reg_ptr->id >= 0 && reg_ptr->id < opnum::reg::T_REGISTER_COUNT)
            {
                assigned_t_register_list[reg_ptr->id] = false;
            }
        }
        void _complete_using_all_register_for_pure_value()
        {
            for (size_t i = 0; i < opnum::reg::T_REGISTER_COUNT; i++)
            {
                assigned_t_register_list[i] = 0;
            }
        }

        bool is_need_dup_when_mov(ast::ast_value* val)
        {
            if (dynamic_cast<ast::ast_value_symbolable_base*>(val))
            {
                if (!val->can_be_assign && (
                    val->value_type->is_dynamic()
                    || val->value_type->is_map()
                    || val->value_type->is_array()))
                    return true;
            }
            return false;
        }

        opnum::opnumbase& get_useable_register_for_ref_value()
        {
            using namespace ast;
            using namespace opnum;
#define RS_NEW_OPNUM(...) (*generated_opnum_list_for_clean.emplace_back(new __VA_ARGS__))
            for (size_t i = 0; i < opnum::reg::R_REGISTER_COUNT; i++)
            {
                if (!assigned_r_register_list[i])
                {
                    assigned_r_register_list[i] = true;
                    return RS_NEW_OPNUM(reg(reg::r0 + (uint8_t)i));
                }
            }
            rs_error("cannot get a useable register..");
            return RS_NEW_OPNUM(reg(reg::cr));
        }
        void _complete_using_register_for_ref_value(opnum::opnumbase& completed_reg)
        {
            using namespace ast;
            using namespace opnum;
            if (auto* reg_ptr = dynamic_cast<opnum::reg*>(&completed_reg);
                reg_ptr && reg_ptr->id >= opnum::reg::T_REGISTER_COUNT
                && reg_ptr->id < opnum::reg::T_REGISTER_COUNT + opnum::reg::R_REGISTER_COUNT)
            {
                rs_test(assigned_r_register_list[reg_ptr->id - opnum::reg::T_REGISTER_COUNT]);
                assigned_r_register_list[reg_ptr->id - opnum::reg::T_REGISTER_COUNT] = false;
            }
        }
        void _complete_using_all_register_for_ref_value()
        {
            for (size_t i = 0; i < opnum::reg::R_REGISTER_COUNT; i++)
            {
                assigned_r_register_list[i] = 0;
            }
        }

        opnum::opnumbase& complete_using_register(opnum::opnumbase& completed_reg)
        {
            _complete_using_register_for_pure_value(completed_reg);
            _complete_using_register_for_ref_value(completed_reg);

            return completed_reg;
        }

        void complete_using_all_register()
        {
            _complete_using_all_register_for_pure_value();
            _complete_using_all_register_for_ref_value();
        }

        bool is_cr_reg(opnum::opnumbase& op_num)
        {
            using namespace opnum;
            if (auto* regist = dynamic_cast<reg*>(&op_num))
            {
                if (regist->id == reg::cr)
                    return true;
            }
            return false;
        }

        bool is_non_ref_tem_reg(opnum::opnumbase& op_num)
        {
            using namespace opnum;
            if (auto* regist = dynamic_cast<reg*>(&op_num))
            {
                if (regist->id >= reg::t0 && regist->id <= reg::t15)
                    return true;
            }
            return false;
        }
        bool is_temp_reg(opnum::opnumbase& op_num)
        {
            using namespace opnum;
            if (auto* regist = dynamic_cast<reg*>(&op_num))
            {
                if (regist->id >= reg::t0 && regist->id <= reg::r15)
                    return true;
            }
            return false;
        }

        opnum::opnumbase& mov_value_to_cr(opnum::opnumbase& op_num, ir_compiler* compiler)
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
            compiler->set(reg(reg::cr), op_num);
            return RS_NEW_OPNUM(reg(reg::cr));
        }

        opnum::opnumbase& set_ref_value_to_cr(opnum::opnumbase& op_num, ir_compiler* compiler)
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
            compiler->ext_setref(reg(reg::cr), op_num);
            return RS_NEW_OPNUM(reg(reg::cr));
        }

        std::vector<ast::ast_value_function_define* > in_used_functions;

        opnum::opnumbase& get_opnum_by_symbol(grammar::ast_base* error_prud, lang_symbol* symb, ir_compiler* compiler, bool get_pure_value = false)
        {
            using namespace opnum;

            if (symb->is_constexpr)
                return analyze_value(symb->variable_value, compiler, get_pure_value, false);

            if (symb->type == lang_symbol::symbol_type::variable)
            {
                if (symb->static_symbol)
                {
                    if (!get_pure_value)
                        return RS_NEW_OPNUM(global((int32_t)symb->global_index_in_lang));
                    else
                    {
                        auto& loaded_pure_glb_opnum = get_useable_register_for_pure_value();
                        compiler->set(loaded_pure_glb_opnum, global((int32_t)symb->global_index_in_lang));
                        return loaded_pure_glb_opnum;
                    }
                }
                else
                {
                    if (!get_pure_value)
                    {
                        if (symb->stackvalue_index_in_funcs <= 64 || symb->stackvalue_index_in_funcs >= -63)
                            return RS_NEW_OPNUM(reg(reg::bp_offset(-(int8_t)symb->stackvalue_index_in_funcs)));
                        else
                        {
                            auto& ldr_aim = get_useable_register_for_ref_value();
                            compiler->ldsr(ldr_aim, imm(-(int16_t)symb->stackvalue_index_in_funcs));
                            return ldr_aim;
                        }
                    }
                    else
                    {
                        if (symb->stackvalue_index_in_funcs <= 64 || symb->stackvalue_index_in_funcs >= -63)
                        {
                            auto& loaded_pure_glb_opnum = get_useable_register_for_pure_value();
                            compiler->set(loaded_pure_glb_opnum, reg(reg::bp_offset(-(int8_t)symb->stackvalue_index_in_funcs)));
                            return loaded_pure_glb_opnum;
                        }
                        else
                        {
                            auto& lds_aim = get_useable_register_for_ref_value();
                            compiler->lds(lds_aim, imm(-(int16_t)symb->stackvalue_index_in_funcs));
                            return lds_aim;
                        }
                    }
                }
            }
            else
            {
                if (symb->function_overload_sets.size() == 0)
                    return analyze_value(symb->variable_value, compiler, get_pure_value, false);
                else if (symb->function_overload_sets.size() == 1)
                    return analyze_value(symb->function_overload_sets.front(), compiler, get_pure_value, false);
                else
                {
                    lang_anylizer->lang_error(0x0000, error_prud, RS_ERR_UNABLE_DECIDE_FUNC_SYMBOL);
                    return RS_NEW_OPNUM(imm(0));
                }
            }
        }

        bool _last_value_stored_to_cr = false;

        struct auto_cancel_value_store_to_cr
        {
            bool clear_sign = false;
            bool* aim_flag;
            auto_cancel_value_store_to_cr(bool& flag)
                :aim_flag(&flag)
            {

            }
            ~auto_cancel_value_store_to_cr()
            {
                *aim_flag = clear_sign;
            }

            void write_to_cr()
            {
                clear_sign = true;
            }
            void not_write_to_cr()
            {
                clear_sign = false;
            }
        };

        opnum::opnumbase& analyze_value(ast::ast_value* value, ir_compiler* compiler, bool get_pure_value = false, bool need_symbol = true, bool force_value = false)
        {
            if (need_symbol)
                compiler->pdb_info->generate_debug_info_at_astnode(value, compiler);

            auto_cancel_value_store_to_cr last_value_stored_to_cr_flag(_last_value_stored_to_cr);
            using namespace ast;
            using namespace opnum;
            if (value->is_constant && !force_value)
            {
                auto const_value = value->get_constant_value();
                switch (const_value.type)
                {
                case value::valuetype::integer_type:
                    if (!get_pure_value)
                        return RS_NEW_OPNUM(imm(const_value.integer));
                    else
                    {
                        auto& treg = get_useable_register_for_pure_value();
                        compiler->set(treg, imm(const_value.integer));
                        return treg;
                    }
                case value::valuetype::real_type:
                    if (!get_pure_value)
                        return RS_NEW_OPNUM(imm(const_value.real));
                    else
                    {
                        auto& treg = get_useable_register_for_pure_value();
                        compiler->set(treg, imm(const_value.real));
                        return treg;
                    }
                case value::valuetype::handle_type:
                    if (!get_pure_value)
                        return RS_NEW_OPNUM(imm((void*)const_value.handle));
                    else
                    {
                        auto& treg = get_useable_register_for_pure_value();
                        compiler->set(treg, imm((void*)const_value.handle));
                        return treg;
                    }
                case value::valuetype::string_type:
                    if (!get_pure_value)
                        return RS_NEW_OPNUM(imm(const_value.string->c_str()));
                    else
                    {
                        auto& treg = get_useable_register_for_pure_value();
                        compiler->set(treg, imm(const_value.string->c_str()));
                        return treg;
                    }
                case value::valuetype::invalid:  // for nil
                case value::valuetype::array_type:  // for nil
                case value::valuetype::mapping_type:  // for nil
                case value::valuetype::gchandle_type:  // for nil
                    if (!get_pure_value)
                        return RS_NEW_OPNUM(reg(reg::ni));
                    else
                    {
                        auto& treg = get_useable_register_for_pure_value();
                        compiler->set(treg, reg(reg::ni));
                        return treg;
                    }
                default:
                    rs_error("error constant type..");
                    break;
                }
            }
            else if (dynamic_cast<ast_value_literal*>(value))
            {
                if (force_value)
                    return analyze_value(value, compiler, get_pure_value);

                rs_error("ast_value_literal should be 'constant'..");
            }
            else if (auto* a_value_binary = dynamic_cast<ast_value_binary*>(value))
            {
                if (a_value_binary->overrided_operation_call)
                    return analyze_value(a_value_binary->overrided_operation_call, compiler, get_pure_value, need_symbol, force_value);

                // if mixed type, do opx
                value::valuetype optype = value::valuetype::invalid;
                if (a_value_binary->left->value_type->is_same(a_value_binary->right->value_type)
                    && !a_value_binary->left->value_type->is_dynamic())
                    optype = a_value_binary->left->value_type->value_type;

                auto& beoped_left_opnum = analyze_value(a_value_binary->left, compiler, true);
                auto& op_right_opnum = analyze_value(a_value_binary->right, compiler);

                switch (a_value_binary->operate)
                {
                case lex_type::l_add:
                    if (optype == value::valuetype::invalid)
                        compiler->addx(beoped_left_opnum, op_right_opnum);
                    else
                    {
                        switch (optype)
                        {
                        case rs::value::valuetype::integer_type:
                            compiler->addi(beoped_left_opnum, op_right_opnum); break;
                        case rs::value::valuetype::real_type:
                            compiler->addr(beoped_left_opnum, op_right_opnum); break;
                        case rs::value::valuetype::handle_type:
                            compiler->addh(beoped_left_opnum, op_right_opnum); break;
                        case rs::value::valuetype::string_type:
                            compiler->adds(beoped_left_opnum, op_right_opnum); break;
                        case rs::value::valuetype::array_type:
                            compiler->addx(beoped_left_opnum, op_right_opnum); break;
                        case rs::value::valuetype::mapping_type:
                            compiler->addx(beoped_left_opnum, op_right_opnum); break;
                        default:
                            lang_anylizer->lang_error(0xC000, a_value_binary, RS_ERR_CANNOT_CALC_WITH_L_AND_R);
                            break;
                        }
                    }
                    break;
                case lex_type::l_sub:
                    if (optype == value::valuetype::invalid)
                        compiler->subx(beoped_left_opnum, op_right_opnum);
                    else
                    {
                        switch (optype)
                        {
                        case rs::value::valuetype::integer_type:
                            compiler->subi(beoped_left_opnum, op_right_opnum); break;
                        case rs::value::valuetype::real_type:
                            compiler->subr(beoped_left_opnum, op_right_opnum); break;
                        case rs::value::valuetype::handle_type:
                            compiler->subh(beoped_left_opnum, op_right_opnum); break;
                        default:
                            lang_anylizer->lang_error(0xC000, a_value_binary, RS_ERR_CANNOT_CALC_WITH_L_AND_R);
                            break;
                        }
                    }
                    break;
                case lex_type::l_mul:
                    if (optype == value::valuetype::invalid)
                        compiler->mulx(beoped_left_opnum, op_right_opnum);
                    else
                    {
                        switch (optype)
                        {
                        case rs::value::valuetype::integer_type:
                            compiler->muli(beoped_left_opnum, op_right_opnum); break;
                        case rs::value::valuetype::real_type:
                            compiler->mulr(beoped_left_opnum, op_right_opnum); break;
                        default:
                            lang_anylizer->lang_error(0xC000, a_value_binary, RS_ERR_CANNOT_CALC_WITH_L_AND_R);
                            break;
                        }
                    }
                    break;
                case lex_type::l_div:
                    if (optype == value::valuetype::invalid)
                        compiler->divx(beoped_left_opnum, op_right_opnum);
                    else
                    {
                        switch (optype)
                        {
                        case rs::value::valuetype::integer_type:
                            compiler->divi(beoped_left_opnum, op_right_opnum); break;
                        case rs::value::valuetype::real_type:
                            compiler->divr(beoped_left_opnum, op_right_opnum); break;
                        default:
                            lang_anylizer->lang_error(0xC000, a_value_binary, RS_ERR_CANNOT_CALC_WITH_L_AND_R);
                            break;
                        }
                    }
                    break;
                case lex_type::l_mod:
                    if (optype == value::valuetype::invalid)
                        compiler->modx(beoped_left_opnum, op_right_opnum);
                    else
                    {
                        switch (optype)
                        {
                        case rs::value::valuetype::integer_type:
                            compiler->modi(beoped_left_opnum, op_right_opnum); break;
                        case rs::value::valuetype::real_type:
                            compiler->modr(beoped_left_opnum, op_right_opnum); break;
                        default:
                            lang_anylizer->lang_error(0xC000, a_value_binary, RS_ERR_CANNOT_CALC_WITH_L_AND_R);
                            break;
                        }
                    }
                    break;
                default:
                    rs_error("Do not support this operator..");
                    break;
                }
                complete_using_register(op_right_opnum);
                return beoped_left_opnum;
            }
            else if (auto* a_value_assign = dynamic_cast<ast_value_assign*>(value))
            {
                // if mixed type, do opx
                bool same_type = a_value_assign->left->value_type->is_same(a_value_assign->right->value_type);
                value::valuetype optype = value::valuetype::invalid;
                if (same_type && !a_value_assign->left->value_type->is_dynamic())
                    optype = a_value_assign->left->value_type->value_type;

                if (auto symb_left = dynamic_cast<ast_value_symbolable_base*>(a_value_assign->left);
                    symb_left && symb_left->symbol->attribute->is_constant_attr())
                {
                    lang_anylizer->lang_error(0x0000, value, RS_ERR_CANNOT_ASSIGN_TO_CONSTANT);
                }
                size_t revert_pos = compiler->get_now_ip();

                auto* beoped_left_opnum_ptr = &analyze_value(a_value_assign->left, compiler);
                auto* op_right_opnum_ptr = &analyze_value(a_value_assign->right, compiler);

                if (is_cr_reg(*beoped_left_opnum_ptr)
                    && (is_cr_reg(*op_right_opnum_ptr) || is_temp_reg(*op_right_opnum_ptr) || _last_value_stored_to_cr))
                {
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
                    if (is_need_dup_when_mov(a_value_assign->right))
                        // TODO Right may be 'nil', do not dup nil..
                        compiler->ext_movdup(beoped_left_opnum, op_right_opnum);
                    else if (optype == value::valuetype::invalid &&
                        !a_value_assign->left->value_type->is_dynamic()
                        && !a_value_assign->left->value_type->is_func()
                        && !a_value_assign->right->value_type->is_func())
                        compiler->movx(beoped_left_opnum, op_right_opnum);
                    else
                        compiler->mov(beoped_left_opnum, op_right_opnum);
                    break;
                case lex_type::l_add_assign:
                    if (optype == value::valuetype::invalid)
                        // TODO : NEED WARNING..
                        compiler->addmovx(beoped_left_opnum, op_right_opnum);
                    else
                    {
                        switch (optype)
                        {
                        case rs::value::valuetype::integer_type:
                            compiler->addi(beoped_left_opnum, op_right_opnum); break;
                        case rs::value::valuetype::real_type:
                            compiler->addr(beoped_left_opnum, op_right_opnum); break;
                        case rs::value::valuetype::handle_type:
                            compiler->addh(beoped_left_opnum, op_right_opnum); break;
                        case rs::value::valuetype::string_type:
                            compiler->adds(beoped_left_opnum, op_right_opnum); break;
                        case rs::value::valuetype::array_type:
                            compiler->addx(beoped_left_opnum, op_right_opnum); break;
                        case rs::value::valuetype::mapping_type:
                            compiler->addx(beoped_left_opnum, op_right_opnum); break;
                        default:
                            lang_anylizer->lang_error(0xC000, a_value_assign, RS_ERR_CANNOT_CALC_WITH_L_AND_R);
                            break;
                        }
                    }
                    break;
                case lex_type::l_sub_assign:
                    if (optype == value::valuetype::invalid)
                        // TODO : NEED WARNING..
                        compiler->submovx(beoped_left_opnum, op_right_opnum);
                    else
                    {
                        switch (optype)
                        {
                        case rs::value::valuetype::integer_type:
                            compiler->subi(beoped_left_opnum, op_right_opnum); break;
                        case rs::value::valuetype::real_type:
                            compiler->subr(beoped_left_opnum, op_right_opnum); break;
                        case rs::value::valuetype::handle_type:
                            compiler->subh(beoped_left_opnum, op_right_opnum); break;
                        default:
                            lang_anylizer->lang_error(0xC000, a_value_assign, RS_ERR_CANNOT_CALC_WITH_L_AND_R);
                            break;
                        }
                    }
                    break;
                case lex_type::l_mul_assign:
                    if (optype == value::valuetype::invalid)
                        // TODO : NEED WARNING..
                        compiler->mulmovx(beoped_left_opnum, op_right_opnum);
                    else
                    {
                        switch (optype)
                        {
                        case rs::value::valuetype::integer_type:
                            compiler->muli(beoped_left_opnum, op_right_opnum); break;
                        case rs::value::valuetype::real_type:
                            compiler->mulr(beoped_left_opnum, op_right_opnum); break;
                        default:
                            lang_anylizer->lang_error(0xC000, a_value_assign, RS_ERR_CANNOT_CALC_WITH_L_AND_R);
                            break;
                        }
                    }
                    break;
                case lex_type::l_div_assign:
                    if (optype == value::valuetype::invalid)
                        // TODO : NEED WARNING..
                        compiler->divmovx(beoped_left_opnum, op_right_opnum);
                    else
                    {
                        switch (optype)
                        {
                        case rs::value::valuetype::integer_type:
                            compiler->divi(beoped_left_opnum, op_right_opnum); break;
                        case rs::value::valuetype::real_type:
                            compiler->divr(beoped_left_opnum, op_right_opnum); break;
                        default:
                            lang_anylizer->lang_error(0xC000, a_value_assign, RS_ERR_CANNOT_CALC_WITH_L_AND_R);
                            break;
                        }
                    }
                    break;
                case lex_type::l_mod_assign:
                    if (optype == value::valuetype::invalid)
                        // TODO : NEED WARNING..
                        compiler->modmovx(beoped_left_opnum, op_right_opnum);
                    else
                    {
                        switch (optype)
                        {
                        case rs::value::valuetype::integer_type:
                            compiler->modi(beoped_left_opnum, op_right_opnum); break;
                        case rs::value::valuetype::real_type:
                            compiler->modr(beoped_left_opnum, op_right_opnum); break;
                        default:
                            lang_anylizer->lang_error(0xC000, a_value_assign, RS_ERR_CANNOT_CALC_WITH_L_AND_R);
                            break;
                        }
                    }
                    break;
                default:
                    rs_error("Do not support this operator..");
                    break;
                }
                complete_using_register(op_right_opnum);
                return beoped_left_opnum;
            }
            else if (auto* a_value_variable = dynamic_cast<ast_value_variable*>(value))
            {
                // ATTENTION: HERE JUST VALUE , NOT JUDGE FUNCTION
                auto symb = a_value_variable->symbol;
                return get_opnum_by_symbol(a_value_variable, symb, compiler, get_pure_value);
            }
            else if (auto* a_value_type_cast = dynamic_cast<ast_value_type_cast*>(value))
            {
                if (force_value)
                    return analyze_value(a_value_type_cast->_be_cast_value_node, compiler, get_pure_value);

                if (a_value_type_cast->value_type->is_dynamic()
                    || a_value_type_cast->value_type->is_same(a_value_type_cast->_be_cast_value_node->value_type)
                    || a_value_type_cast->value_type->is_func())
                    // no cast, just as origin value
                    return analyze_value(a_value_type_cast->_be_cast_value_node, compiler, get_pure_value);

                auto& treg = get_useable_register_for_pure_value();
                compiler->setcast(treg,
                    analyze_value(a_value_type_cast->_be_cast_value_node, compiler),
                    a_value_type_cast->value_type->value_type);
                return treg;

            }
            else if (ast_value_type_judge* a_value_type_judge = dynamic_cast<ast_value_type_judge*>(value))
            {
                if (force_value)
                    return analyze_value(a_value_type_judge->_be_cast_value_node, compiler, get_pure_value);

                auto& result = analyze_value(a_value_type_judge->_be_cast_value_node, compiler);

                if (a_value_type_judge->value_type->is_same(a_value_type_judge->_be_cast_value_node->value_type, false))
                    return result;
                else if (a_value_type_judge->_be_cast_value_node->value_type->is_dynamic())
                {
                    if (a_value_type_judge->value_type->is_complex_type())
                        lang_anylizer->lang_error(0x0000, a_value_type_judge, RS_ERR_CANNOT_TEST_COMPLEX_TYPE);

                    if (!a_value_type_judge->value_type->is_dynamic())
                    {
                        rs_test(a_value_type_judge->value_type->value_type != value::valuetype::invalid);
                        compiler->typeas(result, a_value_type_judge->value_type->value_type);

                        return result;
                    }
                }

                lang_anylizer->lang_error(0x0000, a_value_type_judge, RS_ERR_CANNOT_AS_TYPE,
                    a_value_type_judge->_be_cast_value_node->value_type->get_type_name(false).c_str(),
                    a_value_type_judge->value_type->get_type_name(false).c_str());

                return result;
            }
            else if (ast_value_type_check* a_value_type_check = dynamic_cast<ast_value_type_check*>(value))
            {
                if (force_value)
                    return analyze_value(a_value_type_check->_be_check_value_node, compiler, get_pure_value);

                if (a_value_type_check->aim_type->is_same(a_value_type_check->_be_check_value_node->value_type, false))
                    return RS_NEW_OPNUM(imm(1));
                if (a_value_type_check->_be_check_value_node->value_type->is_dynamic())
                {
                    if (a_value_type_check->value_type->is_complex_type())
                        lang_anylizer->lang_error(0x0000, a_value_type_check, RS_ERR_CANNOT_TEST_COMPLEX_TYPE);

                    if (!a_value_type_check->aim_type->is_dynamic())
                    {
                        // is dynamic do check..
                        auto& result = analyze_value(a_value_type_check->_be_check_value_node, compiler);

                        rs_test(a_value_type_check->aim_type->value_type != value::valuetype::invalid);
                        compiler->typeis(result, a_value_type_check->aim_type->value_type);

                        return RS_NEW_OPNUM(reg(reg::cr));
                    }
                }

                return RS_NEW_OPNUM(imm(0));

            }
            else if (auto* a_value_function_define = dynamic_cast<ast_value_function_define*>(value))
            {
                // function defination
                if (a_value_function_define->ir_func_has_been_generated == false)
                {
                    in_used_functions.push_back(a_value_function_define);
                    a_value_function_define->ir_func_has_been_generated = true;
                }
                return RS_NEW_OPNUM(opnum::tagimm_rsfunc(a_value_function_define->get_ir_func_signature_tag()));
            }
            else if (auto* a_value_funccall = dynamic_cast<ast_value_funccall*>(value))
            {
                if (now_function_in_final_anylize && now_function_in_final_anylize->value_type->is_variadic_function_type)
                    compiler->psh(reg(reg::tc));

                std::vector<ast_value* >arg_list;
                auto arg = a_value_funccall->arguments->children;

                bool full_unpack_arguments = false;
                rs_integer_t extern_unpack_arg_count = 0;

                while (arg)
                {
                    ast_value* arg_val = dynamic_cast<ast_value*>(arg);
                    rs_assert(arg_val);

                    if (full_unpack_arguments)
                    {
                        lang_anylizer->lang_error(0x0000, arg_val, RS_ERR_ARG_DEFINE_AFTER_VARIADIC);
                        break;
                    }

                    if (auto* a_fakevalue_unpacked_args = dynamic_cast<ast_fakevalue_unpacked_args*>(arg_val))
                    {
                        if (a_fakevalue_unpacked_args->expand_count <= 0)
                            full_unpack_arguments = true;
                    }

                    arg_list.insert(arg_list.begin(), arg_val);
                    arg = arg->sibling;
                }

                auto* called_func_aim = &analyze_value(a_value_funccall->called_func, compiler);

                ast_value_function_define* fdef = dynamic_cast<ast_value_function_define*>(a_value_funccall->called_func);
                bool need_using_tc = !dynamic_cast<opnum::immbase*>(called_func_aim)
                    || a_value_funccall->called_func->value_type->is_variadic_function_type
                    || (fdef && fdef->is_different_arg_count_in_same_extern_symbol);


                for (auto* argv : arg_list)
                {
                    if (auto* a_fakevalue_unpacked_args = dynamic_cast<ast_fakevalue_unpacked_args*>(argv))
                    {
                        auto& packing = analyze_value(a_fakevalue_unpacked_args->unpacked_pack, compiler,
                            a_fakevalue_unpacked_args->expand_count <= 0);

                        if (a_fakevalue_unpacked_args->expand_count <= 0)
                            compiler->set(reg(reg::tc), imm(arg_list.size() + extern_unpack_arg_count - 1));
                        else
                            extern_unpack_arg_count += a_fakevalue_unpacked_args->expand_count - 1;

                        compiler->ext_unpackargs(packing,
                            a_fakevalue_unpacked_args->expand_count);
                    }
                    else
                    {
                        if (argv->is_mark_as_using_ref)
                            compiler->pshr(complete_using_register(analyze_value(argv, compiler)));
                        else if (is_need_dup_when_mov(argv))
                        {
                            auto& tmpreg = analyze_value(argv, compiler, true);
                            compiler->ext_movdup(tmpreg, tmpreg);
                            compiler->psh(complete_using_register(tmpreg));
                        }
                        else
                            compiler->psh(complete_using_register(analyze_value(argv, compiler)));
                    }
                }


                if (is_cr_reg(*called_func_aim))
                {
                    auto& callaimreg = get_useable_register_for_pure_value();
                    compiler->set(callaimreg, *called_func_aim);
                    called_func_aim = &callaimreg;
                    complete_using_register(callaimreg);
                }

                if (!full_unpack_arguments && need_using_tc)
                    compiler->set(reg(reg::tc), imm(arg_list.size() + extern_unpack_arg_count));

                opnumbase* reg_for_current_funccall_argc = nullptr;
                if (full_unpack_arguments)
                {
                    reg_for_current_funccall_argc = &get_useable_register_for_pure_value();
                    compiler->set(*reg_for_current_funccall_argc, reg(reg::tc));
                }

                compiler->call(complete_using_register(*called_func_aim));

                last_value_stored_to_cr_flag.write_to_cr();

                opnum::opnumbase* result_storage_place = nullptr;

                if (full_unpack_arguments)
                {
                    last_value_stored_to_cr_flag.not_write_to_cr();

                    if (a_value_funccall->is_mark_as_using_ref)
                    {
                        result_storage_place = &get_useable_register_for_ref_value();
                        compiler->ext_setref(*result_storage_place, reg(reg::cr));
                    }
                    else
                    {
                        result_storage_place = &get_useable_register_for_pure_value();
                        compiler->set(*result_storage_place, reg(reg::cr));
                    }

                    rs_assert(reg_for_current_funccall_argc);
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
                    result_storage_place = &RS_NEW_OPNUM(reg(reg::cr));
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
                    if (full_unpack_arguments && !a_value_funccall->is_mark_as_using_ref)
                        return *result_storage_place;

                    auto& funcresult = get_useable_register_for_pure_value();
                    compiler->set(funcresult, *result_storage_place);
                    return funcresult;
                }
            }
            else if (auto* a_value_logical_binary = dynamic_cast<ast_value_logical_binary*>(value))
            {
                if (a_value_logical_binary->overrided_operation_call)
                    return analyze_value(a_value_logical_binary->overrided_operation_call, compiler, get_pure_value, need_symbol, force_value);

                value::valuetype optype = value::valuetype::invalid;
                if (a_value_logical_binary->left->value_type->is_same(a_value_logical_binary->right->value_type)
                    && !a_value_logical_binary->left->value_type->is_dynamic())
                    optype = a_value_logical_binary->left->value_type->value_type;

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
                        rs_error("Unknown operator.");

                    mov_value_to_cr(analyze_value(a_value_logical_binary->right, compiler), compiler);

                    compiler->tag(logic_short_cut_label);

                    return RS_NEW_OPNUM(reg(reg::cr));
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
                    if ((a_value_logical_binary->left->value_type->is_nil()
                        || a_value_logical_binary->right->value_type->is_nil()
                        || a_value_logical_binary->left->value_type->is_func()
                        || a_value_logical_binary->right->value_type->is_func()
                        || a_value_logical_binary->left->value_type->is_gc_type()
                        || a_value_logical_binary->right->value_type->is_gc_type()
                        || optype == value::valuetype::integer_type
                        || optype == value::valuetype::handle_type)
                        && (!a_value_logical_binary->left->value_type->is_string()
                            && !a_value_logical_binary->right->value_type->is_string()))
                        compiler->equb(beoped_left_opnum, op_right_opnum);
                    else
                        compiler->equx(beoped_left_opnum, op_right_opnum);
                    break;
                case lex_type::l_not_equal:
                    if ((a_value_logical_binary->left->value_type->is_nil()
                        || a_value_logical_binary->right->value_type->is_nil()
                        || a_value_logical_binary->left->value_type->is_func()
                        || a_value_logical_binary->right->value_type->is_func()
                        || a_value_logical_binary->left->value_type->is_gc_type()
                        || a_value_logical_binary->right->value_type->is_gc_type()
                        || optype == value::valuetype::integer_type
                        || optype == value::valuetype::handle_type)
                        && (!a_value_logical_binary->left->value_type->is_string()
                            && !a_value_logical_binary->right->value_type->is_string()))
                        compiler->nequb(beoped_left_opnum, op_right_opnum);
                    else
                        compiler->nequx(beoped_left_opnum, op_right_opnum);
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
                    rs_error("Do not support this operator..");
                    break;
                }

                complete_using_register(op_right_opnum);

                if (!get_pure_value)
                    return RS_NEW_OPNUM(reg(reg::cr));
                else
                {
                    auto& result = get_useable_register_for_pure_value();
                    compiler->set(result, reg(reg::cr));
                    return result;
                }
            }
            else if (auto* a_value_array = dynamic_cast<ast_value_array*>(value))
            {
                auto* _arr_item = a_value_array->array_items->children;
                std::vector<ast_value*> arr_list;
                while (_arr_item)
                {
                    auto* arr_val = dynamic_cast<ast_value*>(_arr_item);
                    rs_test(arr_val);

                    arr_list.insert(arr_list.begin(), arr_val);

                    _arr_item = _arr_item->sibling;
                }

                for (auto* in_arr_val : arr_list)
                {
                    if (in_arr_val->is_mark_as_using_ref)
                        compiler->pshr(complete_using_register(analyze_value(in_arr_val, compiler)));
                    else
                        compiler->psh(complete_using_register(analyze_value(in_arr_val, compiler)));
                }

                auto& treg = get_useable_register_for_pure_value();
                compiler->mkarr(treg, imm(arr_list.size()));
                return treg;

            }
            else if (auto* a_value_mapping = dynamic_cast<ast_value_mapping*>(value))
            {
                auto* _map_item = a_value_mapping->mapping_pairs->children;
                size_t map_pair_count = 0;
                while (_map_item)
                {
                    auto* _map_pair = dynamic_cast<ast_mapping_pair*>(_map_item);
                    rs_test(_map_pair);

                    compiler->psh(complete_using_register(analyze_value(_map_pair->key, compiler)));

                    if (_map_pair->val->is_mark_as_using_ref)
                        compiler->pshr(complete_using_register(analyze_value(_map_pair->val, compiler)));
                    else
                        compiler->psh(complete_using_register(analyze_value(_map_pair->val, compiler)));

                    _map_item = _map_item->sibling;
                    map_pair_count++;
                }

                auto& treg = get_useable_register_for_pure_value();
                compiler->mkmap(treg, imm(map_pair_count));
                return treg;

            }
            else if (auto* a_value_index = dynamic_cast<ast_value_index*>(value))
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

                last_value_stored_to_cr_flag.write_to_cr();

                compiler->idx(beoped_left_opnum, op_right_opnum);

                complete_using_register(beoped_left_opnum);
                complete_using_register(op_right_opnum);

                if (!get_pure_value)
                    return RS_NEW_OPNUM(reg(reg::cr));
                else
                {
                    auto& result = get_useable_register_for_pure_value();
                    compiler->set(result, reg(reg::cr));
                    return result;
                }
            }
            else if (auto* a_value_packed_variadic_args = dynamic_cast<ast_value_packed_variadic_args*>(value))
            {
                if (!now_function_in_final_anylize
                    || !now_function_in_final_anylize->value_type->is_variadic_function_type)
                {
                    lang_anylizer->lang_error(0x0000, a_value_packed_variadic_args, RS_ERR_USING_VARIADIC_IN_NON_VRIDIC_FUNC);
                    return RS_NEW_OPNUM(reg(reg::cr));
                }
                else
                {
                    auto& packed = get_useable_register_for_pure_value();

                    compiler->ext_packargs(packed, imm(
                        now_function_in_final_anylize->value_type->argument_types.size()
                    ));
                    return packed;
                }
            }
            else if (auto* a_value_indexed_variadic_args = dynamic_cast<ast_value_indexed_variadic_args*>(value))
            {
                if (!now_function_in_final_anylize
                    || !now_function_in_final_anylize->value_type->is_variadic_function_type)
                {
                    lang_anylizer->lang_error(0x0000, a_value_packed_variadic_args, RS_ERR_USING_VARIADIC_IN_NON_VRIDIC_FUNC);
                    return RS_NEW_OPNUM(reg(reg::cr));
                }

                if (!get_pure_value)
                {
                    if (a_value_indexed_variadic_args->argindex->is_constant)
                    {
                        auto _cv = a_value_indexed_variadic_args->argindex->get_constant_value();
                        if (_cv.integer <= 63 - 2)
                            return RS_NEW_OPNUM(reg(reg::bp_offset((int8_t)(_cv.integer + 2
                                + now_function_in_final_anylize->value_type->argument_types.size()))));
                        else
                        {
                            auto& result = get_useable_register_for_ref_value();
                            compiler->ldsr(result, imm(_cv.integer + 2
                                + now_function_in_final_anylize->value_type->argument_types.size()));
                            return result;
                        }
                    }
                    else
                    {
                        auto& index = analyze_value(a_value_indexed_variadic_args->argindex, compiler, true);
                        compiler->addi(index, imm(2
                            + now_function_in_final_anylize->value_type->argument_types.size()));
                        complete_using_register(index);
                        auto& result = get_useable_register_for_ref_value();
                        compiler->ldsr(result, index);
                        return result;
                    }
                }
                else
                {
                    auto& result = get_useable_register_for_pure_value();

                    if (a_value_indexed_variadic_args->argindex->is_constant)
                    {
                        auto _cv = a_value_indexed_variadic_args->argindex->get_constant_value();
                        if (_cv.integer <= 63 - 2)
                        {
                            last_value_stored_to_cr_flag.write_to_cr();
                            compiler->set(result, reg(reg::bp_offset((int8_t)(_cv.integer + 2
                                + now_function_in_final_anylize->value_type->argument_types.size()))));
                        }
                        else
                        {
                            compiler->lds(result, imm(_cv.integer + 2
                                + now_function_in_final_anylize->value_type->argument_types.size()));
                        }
                    }
                    else
                    {
                        auto& index = analyze_value(a_value_indexed_variadic_args->argindex, compiler, true);
                        compiler->addi(index, imm(2
                            + now_function_in_final_anylize->value_type->argument_types.size()));
                        complete_using_register(index);
                        compiler->lds(result, index);

                    }
                    return result;
                }
            }
            else if (auto* a_fakevalue_unpacked_args = dynamic_cast<ast_fakevalue_unpacked_args*>(value))
            {
                lang_anylizer->lang_error(0x0000, a_fakevalue_unpacked_args, RS_ERR_UNPACK_ARGS_OUT_OF_FUNC_CALL);
                return RS_NEW_OPNUM(reg(reg::cr));
            }
            else if (auto* a_value_unary = dynamic_cast<ast_value_unary*>(value))
            {
                switch (a_value_unary->operate)
                {
                case lex_type::l_lnot:
                    compiler->lnot(analyze_value(a_value_unary->val, compiler));
                    break;
                case lex_type::l_sub:
                    if (a_value_unary->val->value_type->is_dynamic())
                    {
                        auto& result = analyze_value(a_value_unary->val, compiler, true);
                        compiler->set(reg(reg::cr), imm(0));
                        compiler->subx(reg(reg::cr), result);
                        complete_using_register(result);
                    }
                    else if (a_value_unary->val->value_type->is_integer())
                    {
                        auto& result = analyze_value(a_value_unary->val, compiler, true);
                        compiler->set(reg(reg::cr), imm(0));
                        compiler->subi(reg(reg::cr), result);
                        complete_using_register(result);
                    }
                    else if (a_value_unary->val->value_type->is_real())
                    {
                        auto& result = analyze_value(a_value_unary->val, compiler, true);
                        compiler->set(reg(reg::cr), imm(0));
                        compiler->subi(reg(reg::cr), result);
                        complete_using_register(result);
                    }
                    else
                        lang_anylizer->lang_error(0x0000, a_value_unary, RS_ERR_TYPE_CANNOT_NEGATIVE, a_value_unary->val->value_type->get_type_name().c_str());
                    break;
                default:
                    rs_error("Do not support this operator..");
                    break;
                }

                if (!get_pure_value)
                    return RS_NEW_OPNUM(reg(reg::cr));
                else
                {
                    auto& result = get_useable_register_for_pure_value();
                    compiler->set(result, reg(reg::cr));
                    return result;
                }
            }
            else if (dynamic_cast<ast_value_takeplace*>(value))
            {
                if (!get_pure_value)
                    return RS_NEW_OPNUM(reg(reg::ni));
                else
                    return get_useable_register_for_pure_value();
            }
            else
            {
                rs_error("unknown value type..");
            }

            rs_error("run to err place..");
            static opnum::opnumbase err;
            return err;
#undef RS_NEW_OPNUM
        }

        opnum::opnumbase& auto_analyze_value(ast::ast_value* value, ir_compiler* compiler, bool get_pure_value = false, bool force_value = false)
        {
            auto& result = analyze_value(value, compiler, get_pure_value, true, force_value);
            complete_using_all_register();

            return result;
        }

        struct loop_label_info
        {
            std::wstring current_loop_label;

            std::string current_loop_break_aim_tag;
            std::string current_loop_continue_aim_tag;
        };

        std::vector<loop_label_info> loop_stack_for_break_and_continue;

        void real_analyze_finalize(grammar::ast_base* ast_node, ir_compiler* compiler)
        {
            compiler->pdb_info->generate_debug_info_at_astnode(ast_node, compiler);

            if (traving_node.find(ast_node) != traving_node.end())
            {
                lang_anylizer->lang_error(0x0000, ast_node, L"Bad ast node.");
                return;
            }

            struct traving_guard
            {
                lang* _lang;
                grammar::ast_base* _tving_node;
                traving_guard(lang* _lg, grammar::ast_base* ast_ndoe)
                    : _lang(_lg)
                    , _tving_node(ast_ndoe)
                {
                    _lang->traving_node.insert(_tving_node);
                }
                ~traving_guard()
                {
                    _lang->traving_node.erase(_tving_node);
                }
            };

            traving_guard g1(this, ast_node);

            using namespace ast;
            using namespace opnum;

            if (auto* a_varref_defines = dynamic_cast<ast_varref_defines*>(ast_node))
            {
                for (auto& varref_define : a_varref_defines->var_refs)
                {
                    if (varref_define.is_ref)
                    {
                        rs_assert(nullptr == dynamic_cast<ast_value_takeplace*>(varref_define.init_val));
                        auto& ref_ob = get_opnum_by_symbol(a_varref_defines, varref_define.symbol, compiler);

                        std::string init_static_flag_check_tag;
                        if (varref_define.symbol->static_symbol && varref_define.symbol->define_in_function)
                        {
                            init_static_flag_check_tag = compiler->get_unique_tag_based_command_ip();

                            compiler->equb(ref_ob, reg(reg::ni));
                            compiler->jf(tag(init_static_flag_check_tag));
                        }
                        auto& aim_ob = auto_analyze_value(varref_define.init_val, compiler);

                        if (is_non_ref_tem_reg(aim_ob))
                            lang_anylizer->lang_error(0x0000, varref_define.init_val, RS_ERR_NOT_REFABLE_INIT_ITEM);

                        compiler->ext_setref(ref_ob, aim_ob);

                        if (varref_define.symbol->static_symbol && varref_define.symbol->define_in_function)
                            compiler->tag(init_static_flag_check_tag);
                    }
                    else
                    {
                        if (!varref_define.symbol->is_constexpr)
                        {
                            auto& ref_ob = get_opnum_by_symbol(a_varref_defines, varref_define.symbol, compiler);

                            std::string init_static_flag_check_tag;
                            if (varref_define.symbol->static_symbol && varref_define.symbol->define_in_function)
                            {
                                init_static_flag_check_tag = compiler->get_unique_tag_based_command_ip();

                                compiler->equb(ref_ob, reg(reg::ni));
                                compiler->jf(tag(init_static_flag_check_tag));
                            }

                            if (nullptr == dynamic_cast<ast_value_takeplace*>(varref_define.init_val))
                            {
                                if (is_need_dup_when_mov(varref_define.init_val))
                                    compiler->ext_movdup(ref_ob, auto_analyze_value(varref_define.init_val, compiler));
                                else
                                    compiler->mov(ref_ob, auto_analyze_value(varref_define.init_val, compiler));
                            }
                            if (varref_define.symbol->static_symbol && varref_define.symbol->define_in_function)
                                compiler->tag(init_static_flag_check_tag);
                        }
                    }
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

                auto while_begin_tag = "while_begin_" + compiler->get_unique_tag_based_command_ip();
                auto while_end_tag = "while_end_" + compiler->get_unique_tag_based_command_ip();

                loop_stack_for_break_and_continue.push_back({
                   a_while->marking_label,

                     while_end_tag,
                   while_begin_tag,

                    });

                compiler->tag(while_begin_tag);                                                         // while_begin_tag:
                mov_value_to_cr(auto_analyze_value(a_while->judgement_value, compiler), compiler);      //    * do expr
                compiler->jf(tag(while_end_tag));                                                       //    jf    while_end_tag;

                real_analyze_finalize(a_while->execute_sentence, compiler);                             //              ...

                compiler->jmp(tag(while_begin_tag));                                                    //    jmp   while_begin_tag;
                compiler->tag(while_end_tag);                                                           // while_end_tag:

                loop_stack_for_break_and_continue.pop_back();
            }
            else if (auto* a_except = dynamic_cast<ast_except*>(ast_node))
            {
                auto except_end_tag = "except_end_" + compiler->get_unique_tag_based_command_ip();

                compiler->veh_begin(tag(except_end_tag));
                real_analyze_finalize(a_except->execute_sentence, compiler);
                compiler->veh_clean(tag(except_end_tag));

                compiler->tag(except_end_tag);
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
                auto_analyze_value(a_value, compiler);
            }
            else if (auto* a_sentence_block = dynamic_cast<ast_sentence_block*>(ast_node))
            {
                real_analyze_finalize(a_sentence_block->sentence_list, compiler);
            }
            else if (auto* a_return = dynamic_cast<ast_return*>(ast_node))
            {
                if (a_return->return_value)
                {
                    if (is_need_dup_when_mov(a_return->return_value))
                        compiler->ext_movdup(reg(reg::cr), auto_analyze_value(a_return->return_value, compiler));
                    else if (a_return->return_value->is_mark_as_using_ref)
                        set_ref_value_to_cr(auto_analyze_value(a_return->return_value, compiler), compiler);
                    else if (a_return->return_value->is_mark_as_using_ref)
                        mov_value_to_cr(auto_analyze_value(a_return->return_value, compiler), compiler);
                    else
                        mov_value_to_cr(auto_analyze_value(a_return->return_value, compiler), compiler);
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
                real_analyze_finalize(a_foreach->used_vars_defines, compiler);

                auto foreach_begin_tag = "foreach_begin_" + compiler->get_unique_tag_based_command_ip();
                auto foreach_end_tag = "foreach_end_" + compiler->get_unique_tag_based_command_ip();

                loop_stack_for_break_and_continue.push_back({
                  a_foreach->marking_label,

                  foreach_end_tag,
                  foreach_begin_tag,
                    });

                compiler->tag(foreach_begin_tag);
                mov_value_to_cr(auto_analyze_value(a_foreach->iter_next_judge_expr, compiler), compiler);
                compiler->jf(tag(foreach_end_tag));

                real_analyze_finalize(a_foreach->execute_sentences, compiler);

                compiler->jmp(tag(foreach_begin_tag));
                compiler->tag(foreach_end_tag);

                loop_stack_for_break_and_continue.pop_back();
            }
            else if (ast_break* a_break = dynamic_cast<ast_break*>(ast_node))
            {
                if (a_break->label == L"")
                {
                    if (loop_stack_for_break_and_continue.empty())
                        lang_anylizer->lang_error(0x0000, a_break, RS_ERR_INVALID_OPERATE, L"break");
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

                    lang_anylizer->lang_error(0x0000, a_break, RS_ERR_INVALID_OPERATE, L"break");

                break_label_successful:
                    ;
                }
            }
            else if (ast_continue* a_continue = dynamic_cast<ast_continue*>(ast_node))
            {
                if (a_continue->label == L"")
                {
                    if (loop_stack_for_break_and_continue.empty())
                        lang_anylizer->lang_error(0x0000, a_continue, RS_ERR_INVALID_OPERATE, L"continue");
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

                    lang_anylizer->lang_error(0x0000, a_continue, RS_ERR_INVALID_OPERATE, L"continue");

                continue_label_successful:
                    ;
                }
            }
            else if (dynamic_cast<ast_check_type_with_naming_in_pass2*>(ast_node))
            {
                // do nothing..
            }
            else
                lang_anylizer->lang_error(0x0000, ast_node, L"Bad ast node.");
        }

        void analyze_finalize(grammar::ast_base* ast_node, ir_compiler* compiler)
        {
            // first, check each extern func
            for (auto& [ext_func, funcdef_list] : extern_symb_func_definee)
            {
                ast::ast_value_function_define* last_fundef = nullptr;
                for (auto funcdef : funcdef_list)
                {
                    if (last_fundef)
                    {
                        rs_assert(last_fundef->value_type->is_func());
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

                    size_t funcbegin_ip = compiler->get_now_ip();
                    now_function_in_final_anylize = funcdef;

                    compiler->tag(funcdef->get_ir_func_signature_tag());
                    if (funcdef->declear_attribute->is_extern_attr())
                    {
                        // this function is externed, put it into extern-table and update the value in ir-compiler
                        auto&& spacename = funcdef->get_namespace_chain();

                        auto&& fname = (spacename.empty() ? "" : spacename + "::") + wstr_to_str(funcdef->function_name);
                        if (compiler->pdb_info->extern_function_map.find(fname)
                            != compiler->pdb_info->extern_function_map.end())
                        {
                            this->lang_anylizer->lang_error(0x0000, funcdef,
                                L"函数符号 '%ws' 此前已经被导出，导出同一命名空间下的同名函数是不允许的，继续",
                                str_to_wstr(fname).c_str());
                        }
                        else
                            compiler->pdb_info->extern_function_map[fname] = compiler->get_now_ip();

                    }

                    compiler->pdb_info->generate_func_begin(funcdef, compiler);

                    // ATTENTION: WILL INSERT JIT_DET_FLAG HERE TO CHECK & COMPILE & INVOKE JIT CODE
                    if (config::ENABLE_JUST_IN_TIME)
                        compiler->calljit();

                    auto res_ip = compiler->reserved_stackvalue();                      // reserved..

                    // apply args.
                    int arg_count = 0;
                    auto arg_index = funcdef->argument_list->children;
                    while (arg_index)
                    {
                        if (auto* a_value_arg_define = dynamic_cast<ast::ast_value_arg_define*>(arg_index))
                        {
                            if (a_value_arg_define->is_ref || !a_value_arg_define->symbol->has_been_assigned)
                            {
                                funcdef->this_func_scope->
                                    reduce_function_used_stack_size_at(a_value_arg_define->symbol->stackvalue_index_in_funcs);

                                rs_assert(0 == a_value_arg_define->symbol->stackvalue_index_in_funcs);
                                a_value_arg_define->symbol->stackvalue_index_in_funcs = -2 - arg_count;

                            }
                            else
                                compiler->set(get_opnum_by_symbol(a_value_arg_define, a_value_arg_define->symbol, compiler),
                                    opnum::reg(opnum::reg::bp_offset(+2 + arg_count)));
                        }
                        else//variadic
                            break;
                        arg_count++;
                        arg_index = arg_index->sibling;
                    }

                    real_analyze_finalize(funcdef->in_function_sentence, compiler);

                    auto temp_reg_to_stack_count = compiler->update_all_temp_regist_to_stack(funcbegin_ip);
                    auto reserved_stack_size =
                        funcdef->this_func_scope->max_used_stack_size_in_func
                        + temp_reg_to_stack_count;

                    compiler->reserved_stackvalue(res_ip, (uint16_t)reserved_stack_size); // set reserved size

                    compiler->pdb_info->generate_debug_info_at_funcend(funcdef, compiler);

                    compiler->tag(funcdef->get_ir_func_signature_tag() + "_do_ret");
                    compiler->set(opnum::reg(opnum::reg::cr), opnum::reg(opnum::reg::ni));
                    // compiler->pop(reserved_stack_size);
                    compiler->ret();                                            // do return

                    compiler->pdb_info->generate_func_end(funcdef, temp_reg_to_stack_count, compiler);

                    if (config::ENABLE_JUST_IN_TIME)
                        compiler->ext_endjit(); // ATTENTION: WILL INSERT JIT_DET_FLAG HERE TO CHECK & COMPILE & INVOKE JIT CODE
                    else
                        compiler->nop();

                    for (auto funcvar : funcdef->this_func_scope->in_function_symbols)
                        compiler->pdb_info->add_func_variable(funcdef, funcvar->name, funcvar->variable_value->row_no, funcvar->stackvalue_index_in_funcs);

                }
            }
            compiler->tag("__rsir_rtcode_seg_function_define_end");
            compiler->pdb_info->loaded_libs = extern_libs;
            compiler->pdb_info->finalize_generate_debug_info();

            rs::grammar::ast_base::exchange_this_thread_ast(generated_ast_nodes_buffers);
        }

        lang_scope* begin_namespace(const std::wstring& scope_namespace)
        {
            if (lang_scopes.size())
            {
                auto fnd = lang_scopes.back()->sub_namespaces.find(scope_namespace);
                if (fnd != lang_scopes.back()->sub_namespaces.end())
                {
                    lang_scopes.push_back(fnd->second);
                    return now_namespace = lang_scopes.back();
                }
            }

            lang_scope* scope = new lang_scope;
            lang_scopes_buffers.push_back(scope);

            scope->stop_searching_in_last_scope_flag = false;
            scope->type = lang_scope::scope_type::namespace_scope;
            scope->belong_namespace = now_namespace;
            scope->parent_scope = lang_scopes.empty() ? nullptr : lang_scopes.back();
            scope->scope_namespace = scope_namespace;

            if (lang_scopes.size())
                lang_scopes.back()->sub_namespaces[scope_namespace] = scope;

            lang_scopes.push_back(scope);
            return now_namespace = lang_scopes.back();
        }

        void end_namespace()
        {
            rs_assert(lang_scopes.back()->type == lang_scope::scope_type::namespace_scope);

            now_namespace = lang_scopes.back()->belong_namespace;
            lang_scopes.pop_back();
        }

        lang_scope* begin_scope()
        {
            lang_scope* scope = new lang_scope;
            lang_scopes_buffers.push_back(scope);

            scope->stop_searching_in_last_scope_flag = false;
            scope->type = lang_scope::scope_type::just_scope;
            scope->belong_namespace = now_namespace;
            scope->parent_scope = lang_scopes.empty() ? nullptr : lang_scopes.back();

            lang_scopes.push_back(scope);
            return scope;
        }

        void end_scope()
        {
            rs_assert(lang_scopes.back()->type == lang_scope::scope_type::just_scope);

            auto scope = now_scope();
            if (auto* func = in_function())
            {
                func->used_stackvalue_index -= scope->this_block_used_stackvalue_count;
            }
            lang_scopes.pop_back();
        }

        lang_scope* begin_function(ast::ast_value_function_define* ast_value_funcdef)
        {
            lang_scope* scope = new lang_scope;
            lang_scopes_buffers.push_back(scope);

            scope->stop_searching_in_last_scope_flag = false;
            scope->type = lang_scope::scope_type::function_scope;
            scope->belong_namespace = now_namespace;
            scope->parent_scope = lang_scopes.empty() ? nullptr : lang_scopes.back();
            scope->function_node = ast_value_funcdef;

            if (ast_value_funcdef->function_name != L"" && !ast_value_funcdef->is_template_reification)
            {
                // Not anymous function or template_reification , define func-symbol..
                define_variable_in_this_scope(ast_value_funcdef->function_name, ast_value_funcdef, ast_value_funcdef->declear_attribute);
            }

            lang_scopes.push_back(scope);
            return scope;
        }

        void end_function()
        {
            rs_assert(lang_scopes.back()->type == lang_scope::scope_type::function_scope);
            lang_scopes.pop_back();
        }

        lang_scope* now_scope() const
        {
            rs_assert(!lang_scopes.empty());
            return lang_scopes.back();
        }

        lang_scope* in_function() const
        {
            for (auto rindex = lang_scopes.rbegin(); rindex != lang_scopes.rend(); rindex++)
            {
                if ((*rindex)->type == lang_scope::scope_type::function_scope)
                    return *rindex;
            }
            return nullptr;
        }

        size_t global_symbol_index = 0;

        lang_symbol* define_variable_in_this_scope(const std::wstring& names, ast::ast_value* init_val, ast::ast_decl_attribute* attr)
        {
            rs_assert(lang_scopes.size());

            if (auto* func_def = dynamic_cast<ast::ast_value_function_define*>(init_val))
            {
                if (func_def->function_name != L"")
                {
                    lang_symbol* sym;
                    if (lang_scopes.back()->symbols.find(names) != lang_scopes.back()->symbols.end())
                    {
                        sym = lang_scopes.back()->symbols[names];
                        if (sym->type != lang_symbol::symbol_type::function)
                        {
                            lang_anylizer->lang_error(0x0000, init_val, RS_ERR_REDEFINED, names.c_str());
                            return sym;
                        }
                    }
                    else
                    {
                        sym = lang_scopes.back()->symbols[names] = new lang_symbol;
                        sym->type = lang_symbol::symbol_type::function;
                        sym->name = names;
                        sym->defined_in_scope = lang_scopes.back();
                        sym->attribute = new ast::ast_decl_attribute();
                        sym->attribute->add_attribute(lang_anylizer, +lex_type::l_const); // for stop: function = xxx;

                        auto* pending_function = new ast::ast_value_function_define;
                        pending_function->value_type = new ast::ast_type(L"pending");
                        pending_function->value_type->set_as_function_type();
                        pending_function->value_type->set_as_variadic_arg_func();
                        pending_function->in_function_sentence = nullptr;
                        pending_function->auto_adjust_return_type = false;
                        pending_function->function_name = func_def->function_name;
                        pending_function->symbol = sym;
                        sym->variable_value = pending_function;

                        lang_symbols.push_back(sym);
                    }

                    if (dynamic_cast<ast::ast_value_function_define*>(sym->variable_value)
                        && dynamic_cast<ast::ast_value_function_define*>(sym->variable_value)->function_name != L"")
                    {
                        func_def->symbol = sym;
                        sym->function_overload_sets.push_back(func_def);

                        if (func_def->is_template_define)
                        {
                            sym->is_template_symbol = true;
                        }

                        return sym;
                    }
                }
            }

            if (lang_scopes.back()->symbols.find(names) != lang_scopes.back()->symbols.end())
            {
                auto* last_func_symbol = lang_scopes.back()->symbols[names];

                lang_anylizer->lang_error(0x0000, init_val, RS_ERR_REDEFINED, names.c_str());
                return last_func_symbol;
            }
            else
            {
                lang_symbol* sym = lang_scopes.back()->symbols[names] = new lang_symbol;
                sym->attribute = attr;
                sym->type = lang_symbol::symbol_type::variable;
                sym->name = names;
                sym->variable_value = init_val;
                sym->defined_in_scope = lang_scopes.back();
                auto* func = in_function();
                if (attr->is_extern_attr() && func)
                {
                    lang_anylizer->lang_error(0x0000, attr, RS_ERR_CANNOT_EXPORT_SYMB_IN_FUNC);
                }
                if (func && !sym->attribute->is_static_attr())
                {
                    sym->define_in_function = true;
                    sym->static_symbol = false;

                    // TODO: following should generate same const things, 
                    // const var xxx = 0;
                    // const var ddd = xxx;

                    if (!attr->is_constant_attr() || !init_val->is_constant)
                    {
                        sym->stackvalue_index_in_funcs = func->assgin_stack_index(sym);
                        lang_scopes.back()->this_block_used_stackvalue_count++;
                    }
                    else
                        sym->is_constexpr = true;

                }
                else
                {
                    if (func)
                        sym->define_in_function = true;
                    else
                        sym->define_in_function = false;

                    sym->static_symbol = true;

                    if (!attr->is_constant_attr() || !init_val->is_constant)
                        sym->global_index_in_lang = global_symbol_index++;
                    else
                        sym->is_constexpr = true;

                }

                lang_symbols.push_back(sym);
                return sym;
            }
        }
        lang_symbol* define_type_in_this_scope(ast::ast_using_type_as* def, ast::ast_type* as_type, ast::ast_decl_attribute* attr)
        {
            rs_assert(lang_scopes.size());

            if (lang_scopes.back()->symbols.find(def->new_type_identifier) != lang_scopes.back()->symbols.end())
            {
                auto* last_func_symbol = lang_scopes.back()->symbols[def->new_type_identifier];

                lang_anylizer->lang_error(0x0000, as_type, RS_ERR_REDEFINED, def->new_type_identifier.c_str());
                return last_func_symbol;
            }
            else
            {
                lang_symbol* sym = lang_scopes.back()->symbols[def->new_type_identifier] = new lang_symbol;
                sym->attribute = attr;
                sym->type = lang_symbol::symbol_type::typing;
                sym->name = def->new_type_identifier;
                sym->type_informatiom = as_type;
                sym->defined_in_scope = lang_scopes.back();
                sym->define_node = def;

                if (def->naming_check_list)
                {
                    ast::ast_check_type_with_naming_in_pass2* naming =
                        dynamic_cast<ast::ast_check_type_with_naming_in_pass2*>(def->naming_check_list->children);
                    while (naming)
                    {
                        sym->naming_list.push_back(naming);
                        naming = dynamic_cast<ast::ast_check_type_with_naming_in_pass2*>(naming->sibling);
                    }
                }

                lang_symbols.push_back(sym);
                return sym;
            }
        }

        bool check_symbol_is_accessable(ast::ast_defines* astdefine, lang_symbol* symbol, lang_scope* current_scope, grammar::ast_base* ast, bool give_error = true)
        {
            if (astdefine->declear_attribute && astdefine->declear_attribute->is_private_attr())
            {
                auto* symbol_defined_space = symbol->defined_in_scope;
                while (current_scope)
                {
                    if (current_scope == symbol_defined_space)
                        return true;
                    current_scope = current_scope->parent_scope;
                }
                if (give_error)
                    lang_anylizer->lang_error(0x0000, ast, L"无法访问'%ls'，这是一个私有对象，只能在其定义所在的命名空间内访问，继续", symbol->name.c_str());
                return false;
            }
            return true;
        }
        bool check_symbol_is_accessable(lang_symbol* symbol, lang_scope* current_scope, grammar::ast_base* ast, bool give_error = true)
        {
            if (symbol->attribute && symbol->attribute->is_private_attr())
            {
                auto* symbol_defined_space = symbol->defined_in_scope;
                while (current_scope)
                {
                    if (current_scope == symbol_defined_space)
                        return true;
                    current_scope = current_scope->parent_scope;
                }
                if (give_error)
                    lang_anylizer->lang_error(0x0000, ast, L"无法访问'%ls'，这是一个私有对象，只能在其定义所在的命名空间内访问，继续", symbol->name.c_str());
                return true;
            }
            return true;
        }

        lang_symbol* find_symbol_in_this_scope(ast::ast_symbolable_base* var_ident, const std::wstring& ident_str)
        {
            rs_assert(lang_scopes.size());

            if (!var_ident->search_from_global_namespace && var_ident->scope_namespaces.empty())
                for (auto rind = template_stack.rbegin(); rind != template_stack.rend(); rind++)
                {
                    if (auto fnd = rind->find(ident_str); fnd != rind->end())
                        return fnd->second;
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
                        var_ident->searching_begin_namespace_in_pass2 =
                        finding_from_type->symbol->defined_in_scope;
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
                                var_ident->searching_begin_namespace_in_pass2 = fnd_template_type->symbol->defined_in_scope;
                            else
                                var_ident->search_from_global_namespace = true;

                            var_ident->scope_namespaces[0] = fnd_template_type->type_name;;
                            break;
                        }
                    }

            }

            if (var_ident->symbol)
                return var_ident->symbol;

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

            auto* _searching_in_all = searching;
            while (_searching_in_all)
            {
                for (auto* a_using_namespace : _searching_in_all->used_namespace)
                {
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
                    if (auto fnd = _searching->symbols.find(ident_str);
                        fnd != _searching->symbols.end())
                    {
                        searching_result.insert(var_ident->symbol = fnd->second);
                        goto next_searching_point;
                    }

                there_is_no_such_namespace:
                    _searching = _searching->parent_scope;
                }

            next_searching_point:;
            }

            if (searching_result.empty())
                return nullptr;

            if (searching_result.size() > 1)
            {
                std::wstring err_info = RS_ERR_SYMBOL_IS_AMBIGUOUS;
                size_t fnd_count = 0;
                for (auto fnd_result : searching_result)
                {
                    auto _full_namespace_ = rs::str_to_wstr(get_belong_namespace_path_with_lang_scope(fnd_result->defined_in_scope));
                    if (_full_namespace_ == L"")
                        err_info += RS_TERM_GLOBAL_NAMESPACE;
                    else
                        err_info += L"'" + _full_namespace_ + L"'";
                    fnd_count++;
                    if (fnd_count + 1 == searching_result.size())
                        err_info += L" " RS_TERM_AND L" ";
                    else
                        err_info += L", ";
                }

                lang_anylizer->lang_error(0x0000, var_ident, err_info.c_str(), ident_str.c_str());
            }
            // Check symbol is accessable?
            auto* result = *searching_result.begin();
            check_symbol_is_accessable(result, searching_from_scope, var_ident);
            return result;
        }
        lang_symbol* find_type_in_this_scope(ast::ast_type* var_ident)
        {
            auto* result = find_symbol_in_this_scope(var_ident, var_ident->type_name);
            if (result
                && result->type != lang_symbol::symbol_type::typing
                && result->type != lang_symbol::symbol_type::template_typing)
            {
                lang_anylizer->lang_error(0x0000, var_ident, RS_ERR_IS_NOT_A_TYPE, var_ident->type_name.c_str());
                return nullptr;
            }
            return result;
        }
        lang_symbol* find_value_in_this_scope(ast::ast_value_variable* var_ident)
        {
            auto* result = find_symbol_in_this_scope(var_ident, var_ident->var_name);

            if (result
                && (result->type == lang_symbol::symbol_type::typing
                    || result->type == lang_symbol::symbol_type::template_typing))
            {
                var_ident->symbol = nullptr;

                std::wstring used_template_impl_typing_name;

                ast::ast_type* typing_index = nullptr;

                if (result->type == lang_symbol::symbol_type::typing)
                    used_template_impl_typing_name = var_ident->var_name;
                else
                {
                    typing_index = result->type_informatiom;

                    if (typing_index->using_type_name)
                        typing_index = typing_index->using_type_name;

                    used_template_impl_typing_name = typing_index->type_name;

                    var_ident->scope_namespaces = typing_index->scope_namespaces;
                    var_ident->template_reification_args = typing_index->template_arguments;

                }

                var_ident->scope_namespaces.push_back(used_template_impl_typing_name);
                auto type_name = var_ident->var_name;
                var_ident->var_name = L"create";

                result = find_symbol_in_this_scope(var_ident, var_ident->var_name);
                if (result)
                    return result;

                lang_anylizer->lang_error(0x0000, var_ident, RS_ERR_IS_A_TYPE,
                    used_template_impl_typing_name.c_str()
                );
                return nullptr;
            }
            return result;
        }
        bool has_compile_error()const
        {
            return lang_anylizer->has_error();
        }
    };
}