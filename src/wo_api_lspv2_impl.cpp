#include "wo_lang.hpp"

#include <memory>
#include <optional>

bool _wo_compile_impl(
    wo_string_t virtual_src_path,
    const void* src,
    size_t      len,
    std::optional<wo::shared_pointer<wo::runtime_env>>* out_env_if_success,
    std::optional<std::unique_ptr<wo::LangContext>>* out_langcontext_if_success_and_not_binary,
    std::optional<std::unique_ptr<wo::lexer>>* out_lexer_if_failed);

static_assert(WO_NEED_LSP_API);

struct _wo_lspv2_source_meta
{
    std::optional<wo::shared_pointer<wo::runtime_env>> m_env_if_success;
    std::optional<std::unique_ptr<wo::LangContext>> m_langcontext_if_success_and_not_binary;
    std::optional<std::unique_ptr<wo::lexer>> m_lexer_if_failed;
};

const char* _wo_strdup(const char* str)
{
    size_t len = strlen(str);
    char* new_str = (char*)malloc(len + 1);
    memcpy(new_str, str, len);
    new_str[len] = '\0';
    return new_str;

}

wo_lspv2_source_meta* wo_lspv2_compile_to_meta(
    wo_string_t virtual_src_path,
    wo_string_t src)
{
    wo::wstring_pool::begin_new_pool();

    wo_lspv2_source_meta* meta = new wo_lspv2_source_meta();
    (void)_wo_compile_impl(
        virtual_src_path,
        src,
        src == nullptr ? 0 : strlen(src),
        &meta->m_env_if_success,
        &meta->m_langcontext_if_success_and_not_binary,
        &meta->m_lexer_if_failed);

    return meta;
}
void wo_lspv2_free_meta(wo_lspv2_source_meta* meta)
{
    delete meta;
    wo::wstring_pool::end_pool();
}

struct _wo_lspv2_error_iter
{
    using error_iter_t = decltype(wo::lexer::lex_error_list)::const_iterator;
    error_iter_t m_current;
    error_iter_t m_end;
};

wo_lspv2_error_iter* wo_lspv2_compile_err_iter(wo_lspv2_source_meta* meta)
{
    if (meta->m_lexer_if_failed.has_value())
    {
        auto& error_list = meta->m_lexer_if_failed->get()->lex_error_list;
        return new wo_lspv2_error_iter{
            error_list.cbegin(),
            error_list.cend() };
    }
    return nullptr;
}

wo_lspv2_error_info* wo_lspv2_compile_err_next(wo_lspv2_error_iter* iter)
{
    if (iter->m_current == iter->m_end)
    {
        delete iter;
        return nullptr;
    }
    else
    {
        auto& error = *(iter->m_current++);
        return new wo_lspv2_error_info{
            error.error_level == wo::lexer::errorlevel::error ? (
                WO_LSP_ERROR) : WO_LSP_INFORMATION,
            error.depth,
            _wo_strdup(wo::wstr_to_str(error.describe).c_str()),
            wo_lspv2_location{
                _wo_strdup(wo::wstr_to_str(error.filename).c_str()),
                { error.begin_row, error.begin_col },
                { error.end_row, error.end_col },
            },
        };
    }
}

void wo_lspv2_err_info_free(wo_lspv2_error_info* info)
{
    free((void*)info->m_describe);
    free((void*)info->m_location.m_file_name);
    delete info;
}

struct _wo_lspv2_scope
{
    wo::lang_Scope* m_scope;
};
struct _wo_lspv2_scope_iter
{
    using namespace_iter_t = decltype(wo::lang_Namespace::m_sub_namespaces)::const_iterator;
    using scope_iter_t = decltype(wo::lang_Scope::m_sub_scopes)::const_iterator;

    std::optional<namespace_iter_t> m_current_ns;
    std::optional<namespace_iter_t> m_end_ns;

    scope_iter_t m_current;
    scope_iter_t m_end;
};

wo_lspv2_scope* wo_lspv2_meta_get_global_scope(wo_lspv2_source_meta* meta)
{
    if (!meta->m_langcontext_if_success_and_not_binary.has_value())
        abort();

    return new wo_lspv2_scope{
        meta->m_langcontext_if_success_and_not_binary.value()->
            m_root_namespace->m_this_scope.get(),
    };
}
wo_lspv2_scope_iter* wo_lspv2_scope_sub_scope_iter(wo_lspv2_scope* scope)
{
    return new wo_lspv2_scope_iter{
        scope->m_scope->is_namespace_scope() ?
            std::optional(scope->m_scope->m_belongs_to_namespace->m_sub_namespaces.cbegin())
            : std::nullopt,
        scope->m_scope->is_namespace_scope() ?
            std::optional(scope->m_scope->m_belongs_to_namespace->m_sub_namespaces.cend())
            : std::nullopt,
        scope->m_scope->m_sub_scopes.cbegin(),
        scope->m_scope->m_sub_scopes.cend(),
    };
}
wo_lspv2_scope* wo_lspv2_scope_sub_scope_next(wo_lspv2_scope_iter* iter)
{
    if (iter->m_current_ns.has_value())
    {
        if (iter->m_current_ns.value() != iter->m_end_ns.value())
            return new wo_lspv2_scope{
                (iter->m_current_ns.value()++)->second->m_this_scope.get(),
        };
        else
        {
            iter->m_current_ns = std::nullopt;
            iter->m_end_ns = std::nullopt;
        }
    }

    if (iter->m_current != iter->m_end)
    {
        return new wo_lspv2_scope{
            (iter->m_current++)->get(),
        };
    }
    else
    {
        delete iter;
        return nullptr;
    }
}
void wo_lspv2_scope_free(wo_lspv2_scope* scope)
{
    delete scope;
}
wo_lspv2_scope_info* wo_lspv2_scope_get_info(wo_lspv2_scope* scope)
{
    auto* result = new wo_lspv2_scope_info{
       scope->m_scope->is_namespace_scope() ? _wo_strdup(
            wo::wstr_to_str(*scope->m_scope->m_belongs_to_namespace->m_name).c_str())
            : nullptr,
        scope->m_scope->m_scope_location.has_value() ? WO_TRUE : WO_FALSE,
        {},
    };

    if (result->m_has_location)
    {
        auto& loc = scope->m_scope->m_scope_location.value();
        result->m_location.m_file_name = _wo_strdup(wo::wstr_to_str(*loc.source_file).c_str());
        result->m_location.m_begin_location[0] = loc.begin_at.row;
        result->m_location.m_begin_location[1] = loc.begin_at.column;
        result->m_location.m_end_location[0] = loc.end_at.row;
        result->m_location.m_end_location[1] = loc.end_at.column;
    }

    return result;
}
void wo_lspv2_scope_info_free(wo_lspv2_scope_info* info)
{
    if (info->m_has_location)
        free((void*)info->m_location.m_file_name);

    delete info;
}

struct _wo_lspv2_symbol
{
    wo::lang_Symbol* m_symbol;
};
struct _wo_lspv2_symbol_iter
{
    using symbol_iter_t = decltype(wo::lang_Scope::m_defined_symbols)::const_iterator;
    symbol_iter_t m_current;
    symbol_iter_t m_end;
};

wo_lspv2_symbol_iter* wo_lspv2_scope_symbol_iter(wo_lspv2_scope* scope)
{
    return new wo_lspv2_symbol_iter{
        scope->m_scope->m_defined_symbols.cbegin(),
        scope->m_scope->m_defined_symbols.cend(),
    };
}
wo_lspv2_symbol* wo_lspv2_scope_symbol_next(wo_lspv2_symbol_iter* iter)
{
    if (iter->m_current == iter->m_end)
    {
        delete iter;
        return nullptr;
    }
    else
    {
        return new wo_lspv2_symbol{
            (iter->m_current++)->second.get(),
        };
    }
}
void wo_lspv2_symbol_free(wo_lspv2_symbol* symbol)
{
    delete symbol;
}
wo_lspv2_symbol_info* wo_lspv2_symbol_get_info(wo_lspv2_symbol* symbol)
{
    wo_lspv2_symbol_kind symbol_kind;
    wo_size_t template_param_count = 0;
    const char** template_params = nullptr;

    switch (symbol->m_symbol->m_symbol_kind)
    {
    case wo::lang_Symbol::kind::VARIABLE:
        symbol_kind = WO_LSPV2_SYMBOL_VARIBALE;
        if (symbol->m_symbol->m_is_template)
        {
            template_param_count = symbol->m_symbol->m_template_value_instances->m_template_params.size();
            template_params = (const char**)malloc(sizeof(const char*) * template_param_count);
            size_t count = 0;
            for (auto& param : symbol->m_symbol->m_template_value_instances->m_template_params)
            {
                template_params[count++] =
                    _wo_strdup(wo::wstr_to_str(*param).c_str());
            }
        }
        break;
    case wo::lang_Symbol::kind::ALIAS:
        symbol_kind = WO_LSPV2_SYMBOL_ALIAS;
        if (symbol->m_symbol->m_is_template)
        {
            template_param_count = symbol->m_symbol->m_template_alias_instances->m_template_params.size();
            template_params = (const char**)malloc(sizeof(const char*) * template_param_count);
            size_t count = 0;
            for (auto& param : symbol->m_symbol->m_template_alias_instances->m_template_params)
            {
                template_params[count++] = _wo_strdup(wo::wstr_to_str(*param).c_str());
            }
        }
        break;
    case wo::lang_Symbol::kind::TYPE:
        symbol_kind = WO_LSPV2_SYMBOL_TYPE;
        if (symbol->m_symbol->m_is_template)
        {
            template_param_count = symbol->m_symbol->m_template_type_instances->m_template_params.size();
            template_params = (const char**)malloc(sizeof(const char*) * template_param_count);
            size_t count = 0;
            for (auto& param : symbol->m_symbol->m_template_type_instances->m_template_params)
            {
                template_params[count++] = _wo_strdup(wo::wstr_to_str(*param).c_str());
            }
        }
        break;
    default:
        abort();
    }

    auto* result = new wo_lspv2_symbol_info{
        symbol_kind,
        _wo_strdup(wo::wstr_to_str(*symbol->m_symbol->m_name).c_str()),
        template_param_count,
        template_params,
        symbol->m_symbol->m_symbol_declare_location.has_value() ? WO_TRUE : WO_FALSE,
        {},
    };
    if (result->m_has_location)
    {
        auto& loc = symbol->m_symbol->m_symbol_declare_location.value();
        result->m_location.m_file_name = _wo_strdup(wo::wstr_to_str(*loc.source_file).c_str());
        result->m_location.m_begin_location[0] = loc.begin_at.row;
        result->m_location.m_begin_location[1] = loc.begin_at.column;
        result->m_location.m_end_location[0] = loc.end_at.row;
        result->m_location.m_end_location[1] = loc.end_at.column;
    }
    return result;
}
void wo_lspv2_symbol_info_free(wo_lspv2_symbol_info* info)
{
    free((void*)info->m_name);
    if (info->m_has_location)
        free((void*)info->m_location.m_file_name);
    if (info->m_template_params_count > 0)
    {
        for (size_t i = 0; i < info->m_template_params_count; i++)
        {
            free((void*)info->m_template_params[i]);
        }
        free((void*)info->m_template_params);
    }
    delete info;
}
