#pragma once

#include "wo_basic_type.hpp"
#include "wo_lang_ast_builder.hpp"
#include "wo_compiler_ir.hpp"

#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <optional>

namespace wo
{
    struct lang_symbol
    {
        enum symbol_type
        {
            type_alias = 0x01,
            typing = 0x02,

            variable = 0x04,
            function = 0x08,
        };
        symbol_type type;
        wo_pstring_t name;

        ast::ast_defines* define_node = nullptr;
        ast::ast_decl_attribute* attribute;
        lang_scope* defined_in_scope;
        bool define_in_function = false;
        bool static_symbol = false;
        bool has_been_defined_in_pass2 = false;
        bool is_constexpr = false;
        ast::identifier_decl decl = ast::identifier_decl::IMMUTABLE;
        bool is_captured_variable = false;
        bool is_argument = false;
        bool is_hkt_typing_symb = false;

        union
        {
            wo_integer_t stackvalue_index_in_funcs = -99999999;
            size_t global_index_in_lang;
            wo_integer_t captured_index;
        };

        union
        {
            ast::ast_value* variable_value;
            ast::ast_type* type_informatiom;
        };
        std::vector<ast::ast_check_type_with_naming_in_pass2*> naming_list;
        bool is_template_symbol = false;
        std::vector<wo_pstring_t> template_types;

        std::map<std::vector<uint32_t>, lang_symbol*> template_typehashs_reification_instance_symbol_list;
        std::map<std::vector<uint32_t>, ast::ast_type*> template_type_instances;

        void apply_template_setting(ast::ast_defines* defs)
        {
            is_template_symbol = defs->is_template_define;
            if (is_template_symbol)
            {
                template_types = defs->template_type_name_list;
            }
        }
        ast::ast_value_function_define* get_funcdef()const noexcept
        {
            wo_assert(type == symbol_type::function);
            auto* result = dynamic_cast<ast::ast_value_function_define*>(variable_value);
            wo_assert(result);

            return result;
        }
        bool is_type_decl() const noexcept
        {
            return type == symbol_type::type_alias || type == symbol_type::typing;
        }
        wo_pstring_t defined_source() const noexcept
        {
            if (is_type_decl())
                return type_informatiom->source_file;
            else
                return variable_value->source_file;
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
        grammar::ast_base* last_entry_ast;
        lang_scope* belong_namespace;
        lang_scope* parent_scope;
        wo_pstring_t scope_namespace = nullptr;
        std::unordered_map<wo_pstring_t, lang_symbol*> symbols;

        // Only used when this scope is a namespace.
        std::unordered_map<wo_pstring_t, lang_scope*> sub_namespaces;

        std::vector<ast::ast_using_namespace*> used_namespace;
        std::vector<lang_symbol*> in_function_symbols;

        ast::ast_value_function_define* function_node;

        size_t max_used_stack_size_in_func = 0; // only used in function_scope
        size_t used_stackvalue_index = 0; // only used in function_scope

        size_t this_block_used_stackvalue_count = 0;

        size_t assgin_stack_index(lang_symbol* in_func_variable)
        {
            wo_assert(type == scope_type::function_scope);
            in_function_symbols.push_back(in_func_variable);

            if (used_stackvalue_index + 1 > max_used_stack_size_in_func)
                max_used_stack_size_in_func = used_stackvalue_index + 1;
            return used_stackvalue_index++;
        }

        void reduce_function_used_stack_size_at(wo_integer_t canceled_stack_pos)
        {
            max_used_stack_size_in_func--;
            for (auto* infuncvars : in_function_symbols)
            {
                wo_assert(infuncvars->type == lang_symbol::symbol_type::variable);

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
        using template_type_map = std::map<wo_pstring_t, lang_symbol*>;

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

            if (typing->is_func())
            {
                for (auto& arg : typing->argument_types)
                {
                    hashval <<= 1;
                    hashval += get_typing_hash_after_pass1(arg);
                    hashval *= hashval;
                }
            }

            if (typing->is_mutable())
                hashval *= hashval;

            uint32_t hash32 = hashval & 0xFFFFFFFF;
            while (hashed_typing.find(hash32) != hashed_typing.end())
            {
                if (hashed_typing[hash32]->is_same(typing, false, false))
                    return hash32;
                hash32++;
            }
            hashed_typing[hash32] = typing;

            return hash32;
        }

        std::map<wo_extern_native_func_t, std::vector<ast::ast_value_function_define*>> extern_symb_func_definee;
        ast::ast_value_function_define* now_function_in_final_anylize = nullptr;
        std::vector<template_type_map> template_stack;

        rslib_extern_symbols::extern_lib_set extern_libs;

        bool begin_template_scope(grammar::ast_base* reporterr, const std::vector<wo_pstring_t>& template_defines_args, const std::vector<ast::ast_type*>& template_args)
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

        bool begin_template_scope(grammar::ast_base* reporterr, ast::ast_defines* template_defines, const std::vector<ast::ast_type*>& template_args)
        {
            return begin_template_scope(reporterr, template_defines->template_type_name_list, template_args);
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
            ast::ast_namespace* global = new ast::ast_namespace;
            global->scope_name = wstring_pool::get_pstr(L"");
            global->source_file = wstring_pool::get_pstr(L"::");
            global->col_begin_no =
                global->col_end_no =
                global->row_begin_no =
                global->row_end_no = 1;
            begin_namespace(global);   // global namespace

            // Define 'bool' as built-in type
            ast::ast_using_type_as* using_type_def_bool = new ast::ast_using_type_as();
            using_type_def_bool->new_type_identifier = WO_PSTR(bool);
            using_type_def_bool->old_type = new ast::ast_type(WO_PSTR(int));
            using_type_def_bool->declear_attribute = new ast::ast_decl_attribute();
            using_type_def_bool->declear_attribute->add_attribute(lang_anylizer, +lex_type::l_public);
            define_type_in_this_scope(using_type_def_bool, using_type_def_bool->old_type, using_type_def_bool->declear_attribute);

            ast::ast_using_type_as* using_type_def_char = new ast::ast_using_type_as();
            using_type_def_char->new_type_identifier = WO_PSTR(char);
            using_type_def_char->old_type = new ast::ast_type(WO_PSTR(int));
            using_type_def_char->declear_attribute = new ast::ast_decl_attribute();
            using_type_def_char->declear_attribute->add_attribute(lang_anylizer, +lex_type::l_public);
            define_type_in_this_scope(using_type_def_char, using_type_def_char->old_type, using_type_def_char->declear_attribute);

            ast::ast_using_type_as* using_type_def_anything = new ast::ast_using_type_as();
            using_type_def_anything->is_alias = true;
            using_type_def_anything->new_type_identifier = WO_PSTR(anything);
            using_type_def_anything->old_type = new ast::ast_type(WO_PSTR(void));
            using_type_def_anything->declear_attribute = new ast::ast_decl_attribute();
            using_type_def_anything->declear_attribute->add_attribute(lang_anylizer, +lex_type::l_public);
            define_type_in_this_scope(using_type_def_anything, using_type_def_anything->old_type, using_type_def_anything->declear_attribute);
        }
        ~lang()
        {
            _this_thread_lang_context = nullptr;
            clean_and_close_lang();
        }

        ast::ast_type* generate_type_instance_by_templates(lang_symbol* symb, const std::vector<ast::ast_type*>& templates)
        {
            std::vector<uint32_t> hashs;
            for (auto& template_type : templates)
            {
                if (template_type->is_pending())
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

                for (auto& [_, info] : newtype->struct_member_index)
                {
                    if (info.member_type)
                        info.member_type = dynamic_cast<ast::ast_type*>(info.member_type->instance());
                }

                if (newtype->typefrom != nullptr)
                {
                    temporary_entry_scope_in_pass1(symb->defined_in_scope);
                    if (begin_template_scope(newtype->typefrom, symb->template_types, templates))
                    {
                        auto step_in_pass2 = has_step_in_step2;
                        has_step_in_step2 = false;

                        analyze_pass1(newtype->typefrom);

                        // origin_template_func_define->parent->add_child(dumpped_template_func_define);
                        end_template_scope();

                        has_step_in_step2 = step_in_pass2;
                    }
                    temporary_leave_scope_in_pass1();
                }
            }
            return symb->template_type_instances[hashs];
        }

        bool check_matching_naming(ast::ast_type* clstype, ast::ast_type* naming)
        {
            bool result = true;

            // TODO: Do some check here for typeclass?

            return result;
        }

        bool fully_update_type(ast::ast_type* type, bool in_pass_1, const std::vector<wo_pstring_t>& template_types, std::unordered_set<ast::ast_type*> s)
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

            if (type->typefrom)
            {
                bool is_mutable_typeof = type->is_mutable();
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
                if (is_mutable_typeof)
                    type->set_is_mutable(true);
            }

            if (type->using_type_name)
            {
                if (!type->using_type_name->symbol)
                    type->using_type_name->symbol = find_type_in_this_scope(type->using_type_name);
            }

            // todo: begin_template_scope here~
            if (type->is_custom())
            {
                bool stop_update = false;
                if (type->is_complex())
                {
                    if (type->complex_type->is_custom() && !type->complex_type->is_hkt())
                        if (fully_update_type(type->complex_type, in_pass_1, template_types, s))
                        {
                            if (type->complex_type->is_custom() && !type->complex_type->is_hkt())
                                stop_update = true;
                        }
                }
                if (type->is_func())
                    for (auto& a_t : type->argument_types)
                    {
                        if (a_t->is_custom() && !a_t->is_hkt())
                            if (fully_update_type(a_t, in_pass_1, template_types, s))
                            {
                                if (a_t->is_custom() && !a_t->is_hkt())
                                    stop_update = true;
                            }
                            else
                                a_t->set_is_mutable(false);
                    }

                if (type->has_template())
                {
                    for (auto* template_type : type->template_arguments)
                    {
                        if (template_type->is_custom() && !template_type->is_hkt())
                            if (fully_update_type(template_type, in_pass_1, template_types, s))
                                if (template_type->is_custom() && !template_type->is_hkt())
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

                            bool using_template = false;
                            auto using_template_args = type->template_arguments;

                            type->symbol = type_sym;
                            if (type_sym->template_types.size() != type->template_arguments.size())
                            {
                                // Template count is not match.
                                if (type->has_template())
                                {
                                    // Error! if template_arguments.size() is 0, it will be 
                                    // high-ranked-templated type.
                                    if (type->template_arguments.size() > type_sym->template_types.size())
                                        lang_anylizer->lang_error(lexer::errorlevel::error, type, WO_ERR_TOO_MANY_TEMPLATE_ARGS);
                                    else
                                    {
                                        wo_assert(type->template_arguments.size() < type_sym->template_types.size());
                                        lang_anylizer->lang_error(lexer::errorlevel::error, type, WO_ERR_TOO_FEW_TEMPLATE_ARGS);
                                    }

                                }
                            }
                            else
                            {
                                if (type->has_template())
                                    using_template = type_sym->define_node
                                    ?
                                    begin_template_scope(type, type_sym->define_node, type->template_arguments)
                                    : (type_sym->type_informatiom->using_type_name
                                        ? begin_template_scope(type, type_sym->type_informatiom->using_type_name->symbol->define_node, type->template_arguments)
                                        : begin_template_scope(type, type_sym->template_types, type->template_arguments));

                                auto* symboled_type = new ast::ast_type(WO_PSTR(pending));

                                if (using_template)
                                {
                                    // template arguments not anlyzed.
                                    if (auto* template_instance_type =
                                        generate_type_instance_by_templates(type_sym, type->template_arguments))
                                    {
                                        if (template_instance_type->is_pending())
                                            fully_update_type(template_instance_type, in_pass_1, template_types, s);

                                        symboled_type->set_type(template_instance_type);
                                    }
                                    else
                                        // Failed to instance current template type, skip.
                                        return false;
                                }
                                else
                                    *symboled_type = *type_sym->type_informatiom;

                                fully_update_type(symboled_type, in_pass_1, template_types, s);

                                if (type->is_func())
                                    type->set_ret_type(symboled_type);
                                else
                                    type->set_type(symboled_type);

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

                                if (!type->template_impl_naming_checking.empty())
                                {
                                    for (ast::ast_type* naming_type : type->template_impl_naming_checking)
                                    {
                                        fully_update_type(naming_type, in_pass_1, template_types, s);
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

        void fully_update_type(ast::ast_type* type, bool in_pass_1, const std::vector<wo_pstring_t>& template_types = {})
        {
            std::unordered_set<ast::ast_type*> us;
            wo_asure(!fully_update_type(type, in_pass_1, template_types, us));

            if (type->using_type_name != nullptr)
                wo_assert(type->is_mutable() == type->using_type_name->is_mutable());
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

#define WO_PASS(NODETYPE) \
        bool pass1_##NODETYPE (ast::NODETYPE* astnode);\
        bool pass2_##NODETYPE (ast::NODETYPE* astnode)

        WO_PASS(ast_namespace);
        WO_PASS(ast_varref_defines);
        WO_PASS(ast_value_binary);
        WO_PASS(ast_value_index);
        WO_PASS(ast_value_assign);
        WO_PASS(ast_value_logical_binary);
        WO_PASS(ast_value_variable);
        WO_PASS(ast_value_type_cast);
        WO_PASS(ast_value_type_judge);
        WO_PASS(ast_value_type_check);
        WO_PASS(ast_value_function_define);
        WO_PASS(ast_fakevalue_unpacked_args);
        WO_PASS(ast_value_funccall);
        WO_PASS(ast_value_array);
        WO_PASS(ast_value_mapping);
        WO_PASS(ast_value_indexed_variadic_args);
        WO_PASS(ast_return);
        WO_PASS(ast_sentence_block);
        WO_PASS(ast_if);
        WO_PASS(ast_while);
        WO_PASS(ast_forloop);
        WO_PASS(ast_value_unary);
        WO_PASS(ast_mapping_pair);
        WO_PASS(ast_using_namespace);
        WO_PASS(ast_using_type_as);
        WO_PASS(ast_foreach);
        WO_PASS(ast_check_type_with_naming_in_pass2);
        WO_PASS(ast_union_make_option_ob_to_cr_and_ret);
        WO_PASS(ast_match);
        WO_PASS(ast_match_union_case);
        WO_PASS(ast_value_make_struct_instance);
        WO_PASS(ast_value_make_tuple_instance);
        WO_PASS(ast_struct_member_define);
        WO_PASS(ast_where_constraint);
        WO_PASS(ast_value_trib_expr);

#undef WO_PASS

        void analyze_pattern_in_pass1(ast::ast_pattern_base* pattern, ast::ast_decl_attribute* attrib, ast::ast_value* initval)
        {
            using namespace ast;
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
                        a_pattern_tuple->tuple_takeplaces[i]->value_type->set_is_mutable(false);
                    }
                }
                for (size_t i = 0; i < a_pattern_tuple->tuple_takeplaces.size(); i++)
                    analyze_pattern_in_pass1(a_pattern_tuple->tuple_patterns[i], attrib, a_pattern_tuple->tuple_takeplaces[i]);
            }
            else if (ast_pattern_takeplace* a_pattern_takeplace = dynamic_cast<ast_pattern_takeplace*>(pattern))
            {
                // DO NOTHING
            }
            else
                lang_anylizer->lang_error(lexer::errorlevel::error, pattern, WO_ERR_UNEXPECT_PATTERN_MODE);
        }
        void analyze_pattern_in_pass2(ast::ast_pattern_base* pattern, ast::ast_value* initval)
        {
            using namespace ast;
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
                        a_pattern_tuple->tuple_takeplaces[i]->value_type->set_is_mutable(false);
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
            else if (ast_pattern_takeplace* a_pattern_takeplace = dynamic_cast<ast_pattern_takeplace*>(pattern))
            {
                // DO NOTHING
            }
            else
                lang_anylizer->lang_error(lexer::errorlevel::error, pattern, WO_ERR_UNEXPECT_PATTERN_MODE);
        }
        void analyze_pattern_in_finalize(ast::ast_pattern_base* pattern, ast::ast_value* initval, ir_compiler* compiler)
        {
            using namespace ast;
            using namespace opnum;

            if (ast_pattern_identifier* a_pattern_identifier = dynamic_cast<ast_pattern_identifier*>(pattern))
            {
                if (a_pattern_identifier->template_arguments.empty())
                {
                    if (!a_pattern_identifier->symbol->is_constexpr)
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
                        if (!symbol->is_constexpr)
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

                        analyze_pattern_in_finalize(a_pattern_tuple->tuple_patterns[i], a_pattern_tuple->tuple_takeplaces[i], compiler);
                    }
                }
                complete_using_register(current_values);
            }
            else if (ast_pattern_takeplace* a_pattern_takeplace = dynamic_cast<ast_pattern_takeplace*>(pattern))
            {
                // DO NOTHING
            }
            else
                lang_anylizer->lang_error(lexer::errorlevel::error, pattern, WO_ERR_UNEXPECT_PATTERN_MODE);
        }

        void collect_ast_nodes_for_pass1(grammar::ast_base* ast_node)
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

        void analyze_pass1(grammar::ast_base* ast_node)
        {
#define WO_TRY_BEGIN do{
#define WO_TRY_PASS(NODETYPE) if(pass1_##NODETYPE(dynamic_cast<NODETYPE*>(ast_node)))break;
#define WO_TRY_END }while(0)
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
            WO_TRY_PASS(ast_check_type_with_naming_in_pass2);
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
                        if (a_val->value_type->is_mutable())
                        {
                            a_val->can_be_assign = true;
                            a_val->value_type->set_is_mutable(false);
                        }

                        if (a_val->is_mark_as_using_mut)
                            a_val->value_type->set_is_mutable(true);
                    }
                    a_val->update_constant_value(lang_anylizer);
                }

            }
#undef WO_TRY_BEGIN
#undef WO_TRY_PASS
#undef WO_TRY_END
        }

        lang_symbol* analyze_pass_template_reification(ast::ast_value_variable* origin_variable, std::vector<ast::ast_type*> template_args_types)
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

        ast::ast_value_function_define* analyze_pass_template_reification(ast::ast_value_function_define* origin_template_func_define, std::vector<ast::ast_type*> template_args_types)
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
            dumpped_template_func_define->template_type_name_list.clear();
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

        using judge_result_t = std::variant<ast::ast_type*, ast::ast_value_function_define*>;

        std::optional<judge_result_t> judge_auto_type_of_funcdef_with_type(
            grammar::ast_base* errreport,
            ast::ast_type* param,
            ast::ast_value* callaim,
            bool update,
            ast::ast_defines* template_defines,
            const std::vector<ast::ast_type*>* template_args)
        {
            if (!param->is_func())
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
                wo_assert(new_type->is_func());

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

                // Auto judge here...
                if (update)
                {
                    if (!(template_defines && template_args) || begin_template_scope(errreport, template_defines, *template_args))
                    {
                        auto* reificated = analyze_pass_template_reification(function_define, arg_func_template_args);
                        if (template_defines && template_args)
                            end_template_scope();

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
                        fully_update_type(new_type, false);
                        end_template_scope();
                    }
                    return new_type;
                }
            }

            // no need to judge, 
            return std::nullopt;
        }
        std::vector<ast::ast_type*> judge_auto_type_in_funccall(ast::ast_value_funccall* funccall, bool update, ast::ast_defines* template_defines, const std::vector<ast::ast_type*>* template_args)
        {
            using namespace ast;

            std::vector<ast_value*> args;
            auto* arg = dynamic_cast<ast_value*>(funccall->arguments->children);
            while (arg)
            {
                args.push_back(arg);
                arg = dynamic_cast<ast_value*>(arg->sibling);
            }

            std::vector<std::optional<judge_result_t>> judge_result(args.size(), std::nullopt);
            std::vector<ast::ast_type*> new_arguments_types_result(args.size(), nullptr);

            // If a function has been implized, this flag will be setting and function's
            //  argument list will be updated later.
            bool has_updated_arguments = false;

            for (size_t i = 0; i < args.size() && i < funccall->called_func->value_type->argument_types.size(); ++i)
            {
                judge_result[i] = judge_auto_type_of_funcdef_with_type(
                    funccall, // Used for report error.
                    funccall->called_func->value_type->argument_types[i],
                    args[i], update, template_defines, template_args);
                if (judge_result[i].has_value())
                {
                    if (auto** realized_func = std::get_if<ast::ast_value_function_define*>(&judge_result[i].value()))
                    {
                        wo_assert((*realized_func)->is_template_define == false);
                        args[i] = *realized_func;
                        has_updated_arguments = true;
                    }
                    else
                    {
                        auto* updated_type = std::get<ast::ast_type*>(judge_result[i].value());
                        wo_assert(updated_type != nullptr);
                        new_arguments_types_result[i] = updated_type;
                    }
                }
            }

            if (has_updated_arguments)
            {
                // Re-generate argument list for current function-call;
                funccall->arguments->remove_allnode();
                for (auto* arg : args)
                {
                    arg->sibling = nullptr;
                    funccall->arguments->append_at_end(arg);
                }
            }

            return new_arguments_types_result;
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

            wo_assert(ast_node);

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
                            && a_value->value_type->is_custom())
                            lang_anylizer->lang_error(lexer::errorlevel::error, a_value, WO_ERR_UNKNOWN_TYPE
                                , a_value->value_type->get_type_name().c_str());
                    }
                    if (ast_value_type_check* ast_value_check = dynamic_cast<ast_value_type_check*>(a_value))
                    {
                        // ready for update..
                        fully_update_type(ast_value_check->aim_type, false);

                        if (ast_value_check->aim_type->is_custom())
                            lang_anylizer->lang_error(lexer::errorlevel::error, ast_value_check, WO_ERR_UNKNOWN_TYPE
                                , ast_value_check->aim_type->get_type_name().c_str());

                        ast_value_check->update_constant_value(lang_anylizer);
                    }
                    //

                    WO_TRY_BEGIN;
                    //
                    WO_TRY_PASS(ast_value_variable);
                    WO_TRY_PASS(ast_value_funccall);
                    WO_TRY_PASS(ast_value_function_define);
                    WO_TRY_PASS(ast_value_unary);
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
                        if (a_value->value_type->is_mutable())
                        {
                            a_value->can_be_assign = true;
                            a_value->value_type->set_is_mutable(false);
                        }
                        if (a_value->is_mark_as_using_mut)
                            a_value->value_type->set_is_mutable(true);
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
            WO_TRY_PASS(ast_check_type_with_naming_in_pass2);
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
            wo_pstring_t temp_form,
            const std::vector<wo_pstring_t>& termplate_set,
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

            if (para->type_name == temp_form
                && para->scope_namespaces.empty()
                && !para->search_from_global_namespace)
            {
                ast::ast_type* picked_type = nullptr;
                if (para->is_func())
                {
                    // T(...) should return args..
                    picked_type = args->get_return_type();
                }
                else
                    picked_type = args;

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

        // register dict.... fxxk
        enum class RegisterUsingState : uint8_t
        {
            FREE,
            NORMAL,
            BLOCKING
        };
        std::vector<RegisterUsingState> assigned_tr_register_list = std::vector<RegisterUsingState>(
            opnum::reg::T_REGISTER_COUNT + opnum::reg::R_REGISTER_COUNT);   // will assign t register

        opnum::opnumbase& get_useable_register_for_pure_value(bool must_release = false)
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
        void _complete_using_register_for_pure_value(opnum::opnumbase& completed_reg)
        {
            using namespace ast;
            using namespace opnum;
            if (auto* reg_ptr = dynamic_cast<opnum::reg*>(&completed_reg);
                reg_ptr && reg_ptr->id >= 0 && reg_ptr->id < opnum::reg::T_REGISTER_COUNT + opnum::reg::R_REGISTER_COUNT)
            {
                assigned_tr_register_list[reg_ptr->id] = RegisterUsingState::FREE;
            }
        }
        void _complete_using_all_register_for_pure_value()
        {
            for (size_t i = 0; i < opnum::reg::T_REGISTER_COUNT + opnum::reg::R_REGISTER_COUNT; i++)
            {
                if (assigned_tr_register_list[i] != RegisterUsingState::BLOCKING)
                    assigned_tr_register_list[i] = RegisterUsingState::FREE;
            }
        }

        opnum::opnumbase& complete_using_register(opnum::opnumbase& completed_reg)
        {
            _complete_using_register_for_pure_value(completed_reg);

            return completed_reg;
        }

        void complete_using_all_register()
        {
            _complete_using_all_register_for_pure_value();
        }

        bool is_reg(opnum::opnumbase& op_num)
        {
            using namespace opnum;
            if (auto* regist = dynamic_cast<reg*>(&op_num))
            {
                return true;
            }
            return false;
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
            compiler->mov(reg(reg::cr), op_num);
            return WO_NEW_OPNUM(reg(reg::cr));
        }

        std::vector<ast::ast_value_function_define* > in_used_functions;

        opnum::opnumbase& get_new_global_variable()
        {
            using namespace opnum;
            return WO_NEW_OPNUM(global((int32_t)global_symbol_index++));
        }
        std::variant<opnum::opnumbase*, int16_t> get_opnum_by_symbol(grammar::ast_base* error_prud, lang_symbol* symb, ir_compiler* compiler, bool get_pure_value = false)
        {
            using namespace opnum;

            wo_assert(symb != nullptr);

            if (symb->is_constexpr
                || (symb->decl == wo::ast::identifier_decl::IMMUTABLE
                    && !symb->is_argument
                    && !symb->is_captured_variable
                    && symb->type == lang_symbol::symbol_type::variable
                    && dynamic_cast<ast::ast_value_function_define*>(symb->variable_value)
                    // Only normal func (without capture vars) can use this way to optimize.
                    && dynamic_cast<ast::ast_value_function_define*>(symb->variable_value)->capture_variables.empty()))
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

        bool _last_value_stored_to_cr = false;
        bool _last_value_from_stack = false;
        int16_t _last_stack_offset_to_write = 0;

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

            void set_true()
            {
                clear_sign = true;
            }
            void set_false()
            {
                clear_sign = false;
            }
        };

        opnum::opnumbase& analyze_value(ast::ast_value* value, ir_compiler* compiler, bool get_pure_value = false)
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
                    if (a_value_binary->left->value_type->is_same(a_value_binary->right->value_type, false, true))
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
                        if (a_value_assign->operate == +lex_type::l_assign)
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
                            wo_assert(a_value_assign->left->value_type->accept_type(a_value_assign->right->value_type, false));
                            if (beassigned_value_from_stack)
                                compiler->sts(op_right_opnum, imm(_last_stack_offset_to_write));
                            else
                                compiler->mov(beoped_left_opnum, op_right_opnum);
                            break;
                        case lex_type::l_add_assign:
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
                            if (a_value_assign->operate == +lex_type::l_assign)
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
                                _final_store_value = &WO_NEW_OPNUM(reg(reg::pm));
                                compiler->mov(*_final_store_value, *_store_value);
                            }
                            // Do not generate any other command to make sure reg::pm usable!

                            if (a_value_index->from->value_type->is_array() || a_value_index->from->value_type->is_vec())
                                compiler->sidarr(from_value, index_value, *dynamic_cast<const opnum::reg*>(_final_store_value));
                            else if (a_value_index->from->value_type->is_map() || a_value_index->from->value_type->is_dict())
                                compiler->siddict(from_value, index_value, *dynamic_cast<const opnum::reg*>(_final_store_value));
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
                else if (auto* a_value_type_cast = dynamic_cast<ast_value_type_cast*>(value))
                {
                    if (a_value_type_cast->value_type->is_bool())
                    {
                        // ATTENTION: DO NOT USE ts REG TO STORE REF, lmov WILL MOVE A BOOL VALUE.
                        auto& treg = get_useable_register_for_pure_value();
                        compiler->lmov(treg,
                            complete_using_register(analyze_value(a_value_type_cast->_be_cast_value_node, compiler)));
                        return treg;
                    }

                    if (a_value_type_cast->value_type->is_dynamic()
                        || a_value_type_cast->value_type->accept_type(a_value_type_cast->_be_cast_value_node->value_type, true)
                        || a_value_type_cast->value_type->is_func())
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
                        if (a_value_type_judge->value_type->is_complex_type())
                            lang_anylizer->lang_error(lexer::errorlevel::error, a_value_type_judge, WO_ERR_CANNOT_TEST_COMPLEX_TYPE);

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
                        if (a_value_type_check->aim_type->is_complex_type())
                            lang_anylizer->lang_error(lexer::errorlevel::error, a_value_type_check, WO_ERR_CANNOT_TEST_COMPLEX_TYPE);

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

                    ast_value_symbolable_base* fdef = dynamic_cast<ast_value_symbolable_base*>(a_value_funccall->called_func);
                    ast_value_function_define* funcdef = dynamic_cast<ast_value_function_define*>(fdef);
                    if (funcdef == nullptr && fdef != nullptr)
                    {
                        if (fdef->symbol && fdef->symbol->type == lang_symbol::symbol_type::function)
                            funcdef = fdef->symbol->get_funcdef();
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
                    if (a_value_logical_binary->left->value_type->is_same(a_value_logical_binary->right->value_type, false, true))
                        optype = a_value_logical_binary->left->value_type->value_type;

                    if (!a_value_logical_binary->left->value_type->is_same(a_value_logical_binary->right->value_type, false, true))
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
                    std::vector<ast_value*> arr_list;
                    while (_arr_item)
                    {
                        auto* arr_val = dynamic_cast<ast_value*>(_arr_item);
                        wo_test(arr_val);

                        arr_list.insert(arr_list.begin(), arr_val);

                        _arr_item = _arr_item->sibling;
                    }

                    for (auto* in_arr_val : arr_list)
                        compiler->psh(complete_using_register(analyze_value(in_arr_val, compiler)));

                    auto& treg = get_useable_register_for_pure_value();
                    wo_assert(arr_list.size() <= UINT16_MAX);
                    compiler->mkarr(treg, (uint16_t)arr_list.size());
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
                            compiler->mov(reg(reg::cr), imm(0));
                            compiler->subi(reg(reg::cr), result);
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

                    for (auto index = memb_values.rbegin(); index != memb_values.rend(); ++index)
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
                    std::vector<ast_value*> arr_list;
                    while (tuple_elems)
                    {
                        ast_value* val = dynamic_cast<ast_value*>(tuple_elems);
                        wo_assert(val);
                        arr_list.insert(arr_list.begin(), val);

                        tuple_elems = tuple_elems->sibling;
                    }
                    for (auto val : arr_list)
                        compiler->psh(complete_using_register(analyze_value(val, compiler)));

                    auto& result = get_useable_register_for_pure_value();
                    compiler->mkstruct(result, (uint16_t)arr_list.size());

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

        opnum::opnumbase& auto_analyze_value(ast::ast_value* value, ir_compiler* compiler, bool get_pure_value = false, bool force_value = false)
        {
            auto& result = analyze_value(value, compiler, get_pure_value);
            complete_using_all_register();

            return result;
        }

        struct loop_label_info
        {
            wo_pstring_t current_loop_label;

            std::string current_loop_break_aim_tag;
            std::string current_loop_continue_aim_tag;
        };

        std::vector<loop_label_info> loop_stack_for_break_and_continue;

        void real_analyze_finalize(grammar::ast_base* ast_node, ir_compiler* compiler)
        {
            wo_assert(ast_node->completed_in_pass2);

            compiler->pdb_info->generate_debug_info_at_astnode(ast_node, compiler);

            if (traving_node.find(ast_node) != traving_node.end())
            {
                lang_anylizer->lang_error(lexer::errorlevel::error, ast_node, L"Bad ast node.");
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

                    analyze_pattern_in_finalize(varref_define.pattern, varref_define.init_val, compiler);

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
                    mov_value_to_cr(auto_analyze_value(a_return->return_value, compiler), compiler);

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
            else if (dynamic_cast<ast_check_type_with_naming_in_pass2*>(ast_node))
            {
                // do nothing..
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

                        analyze_pattern_in_finalize(a_pattern_union_value->pattern_arg_in_union_may_nil, a_match_union_case->take_place_value_may_nil, compiler);
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

        void analyze_finalize(grammar::ast_base* ast_node, ir_compiler* compiler)
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
                        wo_assert(last_fundef->value_type->is_func());
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

                    wo_assert(funcdef->completed_in_pass2);

                    size_t funcbegin_ip = compiler->get_now_ip();
                    now_function_in_final_anylize = funcdef;

                    if (config::ENABLE_JUST_IN_TIME)
                        compiler->ext_funcbegin(); // ATTENTION: WILL INSERT JIT_DET_FLAG HERE TO CHECK & COMPILE & INVOKE JIT CODE

                    compiler->tag(funcdef->get_ir_func_signature_tag());
                    if (funcdef->declear_attribute->is_extern_attr())
                    {
                        // this function is externed, put it into extern-table and update the value in ir-compiler
                        auto&& spacename = funcdef->get_namespace_chain();
                        auto&& fname = (spacename.empty() ? "" : spacename + "::") + wstr_to_str(*funcdef->function_name);

                        compiler->record_extern_script_function(fname);
                    }
                    compiler->pdb_info->generate_func_begin(funcdef, compiler);

                    // ATTENTION: WILL INSERT JIT_DET_FLAG HERE TO CHECK & COMPILE & INVOKE JIT CODE
                    //if (config::ENABLE_JUST_IN_TIME)
                    //{
                    //    wo_error("JIT-MODULE HAS BEEN REMOVED");
                    //}

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
                            funcdef->this_func_scope->
                                reduce_function_used_stack_size_at(a_value_arg_define->symbol->stackvalue_index_in_funcs);

                            wo_assert(0 == a_value_arg_define->symbol->stackvalue_index_in_funcs);
                            a_value_arg_define->symbol->stackvalue_index_in_funcs = -2 - arg_count - (wo_integer_t)funcdef->capture_variables.size();
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
                    compiler->tag(funcdef->get_ir_func_signature_tag() + "_do_ret");

                    wo_assert(funcdef->value_type->is_func() && funcdef->value_type->complex_type != nullptr);
                    if (!funcdef->value_type->complex_type->is_void())
                        compiler->ext_panic(opnum::imm("Function returned without valid value."));
                    /*else
                        compiler->mov(opnum::reg(opnum::reg::cr), opnum::reg(opnum::reg::ni));*/
                        // compiler->pop(reserved_stack_size);

                    if (funcdef->is_closure_function())
                        compiler->ret((uint16_t)funcdef->capture_variables.size());
                    else
                        compiler->ret();                                            // do return

                    compiler->pdb_info->generate_func_end(funcdef, temp_reg_to_stack_count, compiler);

                    if (config::ENABLE_JUST_IN_TIME)
                        compiler->ext_funcend(); // ATTENTION: WILL INSERT JIT_DET_FLAG HERE TO CHECK & COMPILE & INVOKE JIT CODE
                    else
                        compiler->nop();

                    for (auto funcvar : funcdef->this_func_scope->in_function_symbols)
                        compiler->pdb_info->add_func_variable(funcdef, *funcvar->name, funcvar->variable_value->row_end_no, funcvar->stackvalue_index_in_funcs);

                }
            }
            compiler->tag("__rsir_rtcode_seg_function_define_end");
            compiler->loaded_libs = extern_libs;
            compiler->pdb_info->finalize_generate_debug_info();

            wo::grammar::ast_base::exchange_this_thread_ast(generated_ast_nodes_buffers);
        }

        lang_scope* begin_namespace(ast::ast_namespace* a_namespace)
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

        void end_namespace()
        {
            wo_assert(lang_scopes.back()->type == lang_scope::scope_type::namespace_scope);

            now_namespace = lang_scopes.back()->belong_namespace;
            lang_scopes.pop_back();
        }

        lang_scope* begin_scope(grammar::ast_base* block_beginer)
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

        void end_scope()
        {
            wo_assert(lang_scopes.back()->type == lang_scope::scope_type::just_scope);

            auto scope = now_scope();
            if (auto* func = in_function())
            {
                func->used_stackvalue_index -= scope->this_block_used_stackvalue_count;
            }
            lang_scopes.pop_back();
        }

        lang_scope* begin_function(ast::ast_value_function_define* ast_value_funcdef)
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

        void end_function()
        {
            wo_assert(lang_scopes.back()->type == lang_scope::scope_type::function_scope);
            lang_scopes.pop_back();
        }

        lang_scope* now_scope() const
        {
            wo_assert(!lang_scopes.empty());
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

        enum class template_style
        {
            NORMAL,
            IS_TEMPLATE_VARIABLE_DEFINE,
            IS_TEMPLATE_VARIABLE_IMPL
        };

        lang_symbol* define_variable_in_this_scope(
            grammar::ast_base* errreporter,
            wo_pstring_t names,
            ast::ast_value* init_val,
            ast::ast_decl_attribute* attr,
            template_style is_template_value,
            ast::identifier_decl mutable_type,
            size_t captureindex = (size_t)-1)
        {
            wo_assert(lang_scopes.size());

            if (is_template_value != template_style::IS_TEMPLATE_VARIABLE_IMPL && (lang_scopes.back()->symbols.find(names) != lang_scopes.back()->symbols.end()))
            {
                auto* last_found_symbol = lang_scopes.back()->symbols[names];

                lang_anylizer->lang_error(lexer::errorlevel::error, errreporter, WO_ERR_REDEFINED, names->c_str());
                lang_anylizer->lang_error(lexer::errorlevel::infrom,
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
                        // TODO: following should generate same const things, 
                        // const var xxx = 0;
                        // const var ddd = xxx;

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
        lang_symbol* define_type_in_this_scope(ast::ast_using_type_as* def, ast::ast_type* as_type, ast::ast_decl_attribute* attr)
        {
            wo_assert(lang_scopes.size());

            if (lang_scopes.back()->symbols.find(def->new_type_identifier) != lang_scopes.back()->symbols.end())
            {
                auto* last_found_symbol = lang_scopes.back()->symbols[def->new_type_identifier];

                lang_anylizer->lang_error(lexer::errorlevel::error, as_type, WO_ERR_REDEFINED, def->new_type_identifier->c_str());
                lang_anylizer->lang_error(lexer::errorlevel::infrom,
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
            if (astdefine->declear_attribute)
            {
                if (astdefine->declear_attribute->is_protected_attr())
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
                if (astdefine->declear_attribute->is_private_attr())
                {
                    if (ast->source_file == astdefine->source_file)
                        return true;
                    if (give_error)
                        lang_anylizer->lang_error(lexer::errorlevel::error, ast, WO_ERR_CANNOT_REACH_PRIVATE_IN_OTHER_FUNC, symbol->name->c_str(),
                            astdefine->source_file->c_str());
                    return false;
                }
            }
            return true;
        }
        bool check_symbol_is_accessable(lang_symbol* symbol, lang_scope* current_scope, grammar::ast_base* ast, bool give_error = true)
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

        lang_symbol* find_symbol_in_this_scope(ast::ast_symbolable_base* var_ident, wo_pstring_t ident_str, int target_type_mask)
        {
            wo_assert(lang_scopes.size());

            if (var_ident->symbol)
                return var_ident->symbol;

            if (!var_ident->search_from_global_namespace && var_ident->scope_namespaces.empty())
                for (auto rind = template_stack.rbegin(); rind != template_stack.rend(); rind++)
                {
                    if (auto fnd = rind->find(ident_str); fnd != rind->end())
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

                if (auto fnd = indet_finding_namespace->symbols.find(ident_str);
                    fnd != indet_finding_namespace->symbols.end())
                {
                    if ((fnd->second->type & target_type_mask) != 0
                        && check_symbol_is_accessable(fnd->second, searching_from_scope, var_ident, false))
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
                    if (auto fnd = _searching->symbols.find(ident_str);
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

            if (searching_result.empty())
                return var_ident->symbol = nullptr;

            if (searching_result.size() > 1)
            {
                // Result might have un-accessable type? remove them
                std::set<lang_symbol*> selecting_results;
                selecting_results.swap(searching_result);
                for (auto fnd_result : selecting_results)
                    if (check_symbol_is_accessable(fnd_result, searching_from_scope, var_ident, false))
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
            if (check_symbol_is_accessable(result, searching_from_scope, var_ident, has_step_in_step2))
                return var_ident->symbol = result;
            return var_ident->symbol = nullptr;
        }
        lang_symbol* find_type_in_this_scope(ast::ast_type* var_ident)
        {
            auto* result = find_symbol_in_this_scope(var_ident, var_ident->type_name,
                lang_symbol::symbol_type::type_alias | lang_symbol::symbol_type::typing);

            return result;
        }

        // Only used for check symbol is exist?
        lang_symbol* find_value_symbol_in_this_scope(ast::ast_value_variable* var_ident)
        {
            return find_symbol_in_this_scope(var_ident, var_ident->var_name,
                lang_symbol::symbol_type::variable | lang_symbol::symbol_type::function);
        }

        lang_symbol* find_value_in_this_scope(ast::ast_value_variable* var_ident)
        {
            auto* result = find_value_symbol_in_this_scope(var_ident);

            if (result)
            {
                auto symb_defined_in_func = result->defined_in_scope;
                while (symb_defined_in_func->parent_scope &&
                    symb_defined_in_func->type != wo::lang_scope::scope_type::function_scope)
                    symb_defined_in_func = symb_defined_in_func->parent_scope;

                auto* current_function = in_function();

                /*
                if (!current_function)
                {
                    current_function = var_ident->searching_begin_namespace_in_pass2;
                    while (current_function && current_function->parent_scope && current_function->type != lang_scope::scope_type::function_scope)
                        current_function = current_function->parent_scope;
                    if (current_function->type != lang_scope::scope_type::function_scope)
                        current_function = nullptr;
                }
                */

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
        bool has_compile_error()const
        {
            return lang_anylizer->has_error();
        }
    };
}