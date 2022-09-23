#include "wo_macro.hpp"
#include "wo_lang_functions_for_ast.hpp"
#include "wo_lang.hpp"

namespace wo
{
    std::string get_belong_namespace_path_with_lang_scope(const lang_scope* scope)
    {
        if (!scope)
            return "";

        if (scope->type != lang_scope::scope_type::namespace_scope)
            return get_belong_namespace_path_with_lang_scope(scope->belong_namespace);

        if (scope->belong_namespace)
        {
            auto parent_name = get_belong_namespace_path_with_lang_scope(scope->belong_namespace);
            return (parent_name.empty() ? "" : parent_name + "::") + wstr_to_str(*scope->scope_namespace);
        }
        else
            return wstr_to_str(*scope->scope_namespace);
    }

    std::string get_belong_namespace_path_with_lang_scope(const lang_symbol* symbol)
    {
        if (!symbol)
            return "";

        return get_belong_namespace_path_with_lang_scope(symbol->defined_in_scope);
    }

    lang_symbol* find_type_in_this_scope(ast::ast_type* type)
    {
        wo_assert(_this_thread_lang_context);
        return _this_thread_lang_context->find_type_in_this_scope(type);
    }
}