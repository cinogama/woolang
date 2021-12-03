#pragma once
#include <string>

namespace rs
{
    struct lang_symbol;
    struct lang_scope;

    std::string get_belong_namespace_path_with_lang_scope(const lang_scope* scope);
}