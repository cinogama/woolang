#pragma once

#include "rs_basic_type.hpp"
#include "rs_lang_ast_builder.hpp"

#include <unordered_map>

namespace rs
{
    struct lang_symbol
    {
        enum class symbol_type
        {
            variable,
            function,
        };
        symbol_type type;
        std::wstring name;
        bool static_symbol;

        union
        {
            ast::ast_value* variable_value;
            grammar::ast_base* function_define;
        };
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
        std::wstring scope_namespace;
        std::unordered_map<std::wstring, lang_symbol*> symbols;

        // Only used when this scope is a namespace.
        std::unordered_map<std::wstring, lang_scope*> sub_namespaces;
    };

    class lang
    {
    private:
        lexer* lang_anylizer;
        std::vector<lang_scope*> lang_namespaces; // only used for storing namespaces to release
        std::vector<lang_symbol*> lang_symbols; // only used for storing symbols to release

        std::vector<lang_scope*> lang_scopes; // it is a stack like list;
        lang_scope* now_namespace;

    public:
        lang(lexer& lex) :
            lang_anylizer(&lex)
        {
            begin_namespace(L"");   // global namespace
        }

        void analyze_pass1(grammar::ast_base* ast_node)
        {
            using namespace ast;

            if (!ast_node)return;

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
                    define_variable_in_this_scope(varref.ident_name, varref.init_val);

                    a_varref_defs->add_child(varref.init_val);
                }
            }
            else if (ast_value_binary* a_value_bin = dynamic_cast<ast_value_binary*>(ast_node))
            {
                analyze_pass1(a_value_bin->left);
                analyze_pass1(a_value_bin->right);

                a_value_bin->add_child(a_value_bin->left);
                a_value_bin->add_child(a_value_bin->right);

                a_value_bin->value_type = pass_binary_op::binary_upper_type(
                    a_value_bin->left->value_type,
                    a_value_bin->right->value_type
                );

                if (nullptr == a_value_bin->value_type)
                    a_value_bin->value_type = new ast_type(L"pending");
            }
            else if (ast_value_variable* a_value_var = dynamic_cast<ast_value_variable*>(ast_node))
            {
                auto* sym = find_symbol_in_this_scope(a_value_var, rs::lang_symbol::symbol_type::variable);
                if (sym)
                {
                    a_value_var->value_type = sym->variable_value->value_type;
                }
                else
                {
                    a_value_var->searching_begin_namespace_in_pass2 = now_namespace;
                }
            }
            else if (ast_value_type_cast* a_value_cast = dynamic_cast<ast_value_type_cast*>(ast_node))
            {
                analyze_pass1(a_value_cast->_be_cast_value_node);
                a_value_cast->add_child(a_value_cast->_be_cast_value_node);
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
        }

        void analyze_pass2(grammar::ast_base* ast_node)
        {
            using namespace ast;

            if (!ast_node)return;

            if (ast_value* a_value = dynamic_cast<ast_value*>(ast_node))
            {
                if (a_value->value_type->is_pending())
                {
                    if (ast_value_variable* a_value_var = dynamic_cast<ast_value_variable*>(a_value))
                    {
                        auto* sym = find_symbol_in_this_scope(a_value_var,
                            rs::lang_symbol::symbol_type::variable,
                            true);

                        if (sym)
                        {
                            analyze_pass2(sym->variable_value);
                            a_value_var->value_type = sym->variable_value->value_type;
                        }
                        else
                        {
                            lang_anylizer->lang_error(0x0000, a_value_var, L"Unknown identifier '%s'.", a_value_var->var_name.c_str());
                            a_value_var->value_type = new ast_type(L"pending");
                        }
                    }
                    else if (ast_value_binary* a_value_bin = dynamic_cast<ast_value_binary*>(a_value))
                    {
                        analyze_pass2(a_value_bin->left);
                        analyze_pass2(a_value_bin->right);

                        a_value_bin->value_type = pass_binary_op::binary_upper_type(
                            a_value_bin->left->value_type,
                            a_value_bin->right->value_type
                        );

                        if (nullptr == a_value_bin->value_type)
                        {
                            lang_anylizer->lang_error(0x0000, a_value_bin, L"Failed to analyze the type.");
                            a_value_bin->value_type = new ast_type(L"pending");
                        }
                    }
                    else
                    {
                        lang_anylizer->lang_error(0x0000, a_value, L"Unknown type '%s'.", a_value->value_type->get_type_name().c_str());
                    }
                }

                if (ast_value_type_cast * a_value_typecast = dynamic_cast<ast_value_type_cast*>(a_value))
                {
                    // check: cast is valid?
                    ast_value * origin_value = a_value_typecast->_be_cast_value_node;
                    
                }
            }

            grammar::ast_base* child = ast_node->children;
            while (child)
            {
                analyze_pass2(child);
                child = child->sibling;
            }

        }

        lang_scope* begin_namespace(const std::wstring& scope_namespace)
        {
            if (now_namespace)
            {
                auto fnd = now_namespace->sub_namespaces.find(scope_namespace);
                if (fnd != now_namespace->sub_namespaces.end())
                {
                    lang_scopes.push_back(fnd->second);
                    return now_namespace = fnd->second;
                }
            }

            lang_scope* scope = new lang_scope;
            lang_namespaces.push_back(scope);

            scope->stop_searching_in_last_scope_flag = false;
            scope->type = lang_scope::scope_type::namespace_scope;
            scope->belong_namespace = now_namespace;
            scope->scope_namespace = scope_namespace;

            if (now_namespace)
                now_namespace->sub_namespaces[scope_namespace] = scope;

            lang_scopes.push_back(scope);
            return now_namespace = scope;
        }

        void end_namespace()
        {
            rs_assert(lang_scopes.back()->type == lang_scope::scope_type::namespace_scope);
            lang_scopes.pop_back();

            now_namespace = now_namespace->belong_namespace;
        }

        lang_scope* begin_scope()
        {
            lang_scope* scope = new lang_scope;

            scope->stop_searching_in_last_scope_flag = false;
            scope->type = lang_scope::scope_type::just_scope;
            scope->belong_namespace = now_namespace;

            lang_scopes.push_back(scope);
            return scope;
        }

        void end_scope()
        {
            rs_assert(!lang_scopes.back()->type == lang_scope::scope_type::namespace_scope);
            delete lang_scopes.back();
            lang_scopes.pop_back();
        }

        lang_scope* begin_function()
        {

        }

        lang_symbol* define_variable_in_this_scope(const std::wstring& names, ast::ast_value* init_val)
        {
            rs_assert(lang_scopes.size());

            if (lang_scopes.back()->symbols.find(names) != lang_scopes.back()->symbols.end())
            {
                lang_anylizer->lang_error(0x0000, init_val, L"Redefined '%s' in this scope.", names.c_str());
                return lang_scopes.back()->symbols[names];
            }
            else
            {
                lang_symbol* sym = lang_scopes.back()->symbols[names] = new lang_symbol;
                sym->type = lang_symbol::symbol_type::variable;
                sym->name = names;
                sym->variable_value = init_val;

                /* if (*in_function?) */
                /*   sym->static_symbol = false; */
                /* else */
                sym->static_symbol = true;

                lang_symbols.push_back(sym);
                return sym;
            }
        }
        lang_symbol* find_symbol_in_this_scope(ast::ast_value_variable* var_ident, lang_symbol::symbol_type need_type, bool ignore_static = false)
        {
            rs_assert(lang_scopes.size());

            auto* searching = var_ident->search_from_global_namespace ?
                lang_scopes.front()
                :
                (
                    var_ident->searching_begin_namespace_in_pass2 ?
                    var_ident->searching_begin_namespace_in_pass2
                    :
                    lang_scopes.back()
                    );

            while (searching)
            {
                // search_in 
                if (var_ident->scope_namespaces.size())
                {
                    size_t namespace_index = 0;
                    lang_scope* begin_namespace = nullptr;
                    if (searching->type != lang_scope::scope_type::namespace_scope)
                        searching = searching->belong_namespace;

                    auto* stored_scope_for_next_try = searching;

                    while (namespace_index < var_ident->scope_namespaces.size())
                    {
                        if (auto fnd = searching->sub_namespaces.find(var_ident->scope_namespaces[namespace_index]);
                            fnd != searching->sub_namespaces.end())
                        {
                            namespace_index++;
                            searching = fnd->second;
                        }
                        else
                        {
                            searching = stored_scope_for_next_try;
                            goto there_is_no_such_namespace;
                        }
                    }
                }

                if (auto fnd = searching->symbols.find(var_ident->var_name);
                    fnd != searching->symbols.end())
                {
                    if (!ignore_static || fnd->second->static_symbol)
                    {
                        if (fnd->second->type == need_type)
                            return fnd->second;
                    }
                }

            there_is_no_such_namespace:
                searching = searching->belong_namespace;
            }

            return nullptr;
        }
    };
}