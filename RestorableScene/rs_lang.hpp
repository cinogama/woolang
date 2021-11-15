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

        union
        {
            ast::ast_value* variable_value;
            grammar::ast_base* function_define;
        };
    };

    class lang
    {
    public:
        
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
            std::unordered_map<std::wstring, lang_symbol> symbols;

            // Only used when this scope is a namespace.
            std::unordered_map<std::wstring, lang_scope*> sub_namespaces;
        };

    private:
        lexer* lang_anylizer;
        std::vector<lang_scope*> lang_namespaces; // only used for storing namespace scope to release
        std::vector<lang_scope*> lang_scopes; // it is a stack like list;
        lang_scope* now_namespace;

    public:
        lang(lexer& lex) :
            lang_anylizer(&lex)
        {

        }

        lang_scope* begin_namespace(const std::wstring& scope_namespace)
        {
            if (now_namespace)
            {
                auto fnd = now_namespace->sub_namespaces.find(scope_namespace);
                if (fnd != now_namespace->sub_namespaces.end())
                    return fnd->second;
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
            rs_assert(lang_scopes.back()->is_namespace_flag);
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
            rs_assert(!lang_scopes.back()->is_namespace_flag);
            delete lang_scopes.back();
            lang_scopes.pop_back();
        }

        lang_symbol* find_symbol_in_this_scope(const std::wstring& names)
        {
            // symbols[];
        }
    };
}