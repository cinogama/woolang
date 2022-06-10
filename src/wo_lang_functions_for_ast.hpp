#pragma once
#include <string>

namespace wo
{
    struct lang_symbol;
    struct lang_scope;
    class lang;

    namespace ast
    {
        struct ast_type;
    }

    inline thread_local lang* _this_thread_lang_context = nullptr;

    std::string get_belong_namespace_path_with_lang_scope(const lang_scope* scope);
    std::string get_belong_namespace_path_with_lang_scope(const lang_symbol* symbol);
    lang_symbol* find_type_in_this_scope(ast::ast_type*);
}