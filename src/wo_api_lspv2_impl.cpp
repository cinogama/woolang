#include "wo_lang.hpp"

#include <memory>
#include <optional>
#include <forward_list>
#include <map>
#include <unordered_map>

#ifndef WO_DISABLE_COMPILER

wo::compile_result _wo_compile_impl(
    wo_string_t virtual_src_path,
    const void* src,
    size_t      len,
    std::optional<wo::shared_pointer<wo::runtime_env>>* out_env_if_success,
    std::optional<std::unique_ptr<wo::lexer>>* out_lexer_if_failed,
    std::optional<std::unique_ptr<wo::LangContext>>* out_langcontext_if_pass_grammar);

static_assert(WO_NEED_LSP_API);

struct _wo_lspv2_source_meta
{
    wo::compile_result m_step;

    std::forward_list<wo::ast::AstBase*> m_origin_astbase_list;

    std::optional<wo::shared_pointer<wo::runtime_env>> m_env_if_success;
    std::optional<std::unique_ptr<wo::LangContext>> m_langcontext_if_passed_grammar;
    std::optional<std::unique_ptr<wo::lexer>> m_lexer_if_failed;

    using value_or_type_expr_t = wo::ast::AstBase*;
    using expr_map_t = std::multimap<
        wo::ast::AstBase::location_t /* end place */, value_or_type_expr_t>;
    using expr_location_map_t = std::map<wo::ast::AstBase::location_t /* begin place */, expr_map_t>;
    using source_expr_collection_t = std::unordered_map<wo_pstring_t, expr_location_map_t>;
    source_expr_collection_t m_source_expr_collection;
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

    std::forward_list<wo::ast::AstBase*> old_ast_list;
    bool need_exchange_back =
        wo::ast::AstBase::exchange_this_thread_ast(
            old_ast_list);

    meta->m_step = _wo_compile_impl(
        virtual_src_path,
        src,
        src == nullptr ? 0 : strlen(src),
        &meta->m_env_if_success,
        &meta->m_lexer_if_failed,
        &meta->m_langcontext_if_passed_grammar);

    wo::ast::AstBase::exchange_this_thread_ast(meta->m_origin_astbase_list);

    if (need_exchange_back)
        wo::ast::AstBase::exchange_this_thread_ast(old_ast_list);

    // Grammar passed, get more type information as possiable.
    if (meta->m_langcontext_if_passed_grammar.has_value())
    {
        for (auto* ast_base_instance : meta->m_origin_astbase_list)
        {
            if (((ast_base_instance->node_type < wo::ast::AstBase::node_type_t::AST_VALUE_begin
                || ast_base_instance->node_type >= wo::ast::AstBase::node_type_t::AST_VALUE_end)
                && ast_base_instance->node_type != wo::ast::AstBase::node_type_t::AST_TYPE_HOLDER)
                || ast_base_instance->source_location.source_file == nullptr)
                continue;

            auto& record =
                meta->m_source_expr_collection[
                    ast_base_instance->source_location.source_file];

            if (ast_base_instance->node_type == wo::ast::AstBase::node_type_t::AST_TYPE_HOLDER)
            {
                wo::ast::AstTypeHolder* ast_type_holder =
                    static_cast<wo::ast::AstTypeHolder*>(ast_base_instance);

                if (ast_type_holder->m_LANG_determined_type.has_value())
                {
                    record[ast_base_instance->source_location.begin_at].insert(
                        std::make_pair(ast_base_instance->source_location.end_at, ast_type_holder));
                }
            }
            else
            {
                wo::ast::AstValueBase* ast_value =
                    static_cast<wo::ast::AstValueBase*>(ast_base_instance);

                if (ast_value->m_LANG_determined_type.has_value())
                {
                    record[ast_base_instance->source_location.begin_at].insert(
                        std::make_pair(ast_base_instance->source_location.end_at, ast_value));
                }
            }
        }
    }
    return meta;
}
void wo_lspv2_meta_free(wo_lspv2_source_meta* meta)
{
    if (!meta->m_origin_astbase_list.empty())
    {
        // free ast
        std::forward_list<wo::ast::AstBase*> old_ast_list;
        bool need_exchange_back =
            wo::ast::AstBase::exchange_this_thread_ast(
                old_ast_list);

        (void)wo::ast::AstBase::exchange_this_thread_ast(
            meta->m_origin_astbase_list);

        wo::ast::AstBase::clean_this_thread_ast();

        if (need_exchange_back)
            wo::ast::AstBase::exchange_this_thread_ast(old_ast_list);
    }

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
    if (!meta->m_langcontext_if_passed_grammar.has_value())
        return nullptr;

    return reinterpret_cast<wo_lspv2_scope*>(
        meta->m_langcontext_if_passed_grammar.value()->m_root_namespace->m_this_scope.get());
}
wo_lspv2_scope_iter* wo_lspv2_scope_sub_scope_iter(wo_lspv2_scope* scope)
{
    wo::lang_Scope* lang_scope = std::launder(reinterpret_cast<wo::lang_Scope*>(scope));

    return new wo_lspv2_scope_iter{
        lang_scope->is_namespace_scope() ?
            std::optional(lang_scope->m_belongs_to_namespace->m_sub_namespaces.cbegin())
            : std::nullopt,
        lang_scope->is_namespace_scope() ?
            std::optional(lang_scope->m_belongs_to_namespace->m_sub_namespaces.cend())
            : std::nullopt,
        lang_scope->m_sub_scopes.cbegin(),
        lang_scope->m_sub_scopes.cend(),
    };
}
wo_lspv2_scope* wo_lspv2_scope_sub_scope_next(wo_lspv2_scope_iter* iter)
{
    if (iter->m_current_ns.has_value())
    {
        if (iter->m_current_ns.value() != iter->m_end_ns.value())
            return reinterpret_cast<wo_lspv2_scope*>(
                (iter->m_current_ns.value()++)->second->m_this_scope.get());
        else
        {
            iter->m_current_ns = std::nullopt;
            iter->m_end_ns = std::nullopt;
        }
    }

    if (iter->m_current != iter->m_end)
    {
        return reinterpret_cast<wo_lspv2_scope*>(
            (iter->m_current++)->get());
    }
    else
    {
        delete iter;
        return nullptr;
    }
}
wo_lspv2_scope_info* wo_lspv2_scope_get_info(wo_lspv2_scope* scope)
{
    wo::lang_Scope* lang_scope = std::launder(reinterpret_cast<wo::lang_Scope*>(scope));
    auto* result = new wo_lspv2_scope_info{
       lang_scope->is_namespace_scope() ? _wo_strdup(
            wo::wstr_to_str(*lang_scope->m_belongs_to_namespace->m_name).c_str())
            : nullptr,
        lang_scope->m_scope_location.has_value() ? WO_TRUE : WO_FALSE,
        {},
    };

    if (result->m_has_location)
    {
        auto& loc = lang_scope->m_scope_location.value();
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

struct _wo_lspv2_symbol_iter
{
    using symbol_iter_t = decltype(wo::lang_Scope::m_defined_symbols)::const_iterator;
    symbol_iter_t m_current;
    symbol_iter_t m_end;
};

wo_lspv2_symbol_iter* wo_lspv2_scope_symbol_iter(wo_lspv2_scope* scope)
{
    wo::lang_Scope* lang_scope = std::launder(reinterpret_cast<wo::lang_Scope*>(scope));
    return new wo_lspv2_symbol_iter{
        lang_scope->m_defined_symbols.cbegin(),
        lang_scope->m_defined_symbols.cend(),
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
        return reinterpret_cast<wo_lspv2_symbol*>(
            (iter->m_current++)->second.get());
    }
}
wo_lspv2_symbol_info* wo_lspv2_symbol_get_info(wo_lspv2_symbol* symbol)
{
    wo_lspv2_symbol_kind symbol_kind;
    wo_size_t template_param_count = 0;
    const char** template_params = nullptr;

    wo::lang_Symbol* lang_symbol = 
        std::launder(reinterpret_cast<wo::lang_Symbol*>(symbol));

    switch (lang_symbol->m_symbol_kind)
    {
    case wo::lang_Symbol::kind::VARIABLE:
        symbol_kind = WO_LSPV2_SYMBOL_VARIBALE;
        if (lang_symbol->m_is_template)
        {
            template_param_count = lang_symbol->m_template_value_instances->m_template_params.size();
            template_params = (const char**)malloc(sizeof(const char*) * template_param_count);
            size_t count = 0;
            for (auto& param : lang_symbol->m_template_value_instances->m_template_params)
            {
                template_params[count++] =
                    _wo_strdup(wo::wstr_to_str(*param).c_str());
            }
        }
        break;
    case wo::lang_Symbol::kind::ALIAS:
        symbol_kind = WO_LSPV2_SYMBOL_ALIAS;
        if (lang_symbol->m_is_template)
        {
            template_param_count = lang_symbol->m_template_alias_instances->m_template_params.size();
            template_params = (const char**)malloc(sizeof(const char*) * template_param_count);
            size_t count = 0;
            for (auto& param : lang_symbol->m_template_alias_instances->m_template_params)
            {
                template_params[count++] = _wo_strdup(wo::wstr_to_str(*param).c_str());
            }
        }
        break;
    case wo::lang_Symbol::kind::TYPE:
        symbol_kind = WO_LSPV2_SYMBOL_TYPE;
        if (lang_symbol->m_is_template)
        {
            template_param_count = lang_symbol->m_template_type_instances->m_template_params.size();
            template_params = (const char**)malloc(sizeof(const char*) * template_param_count);
            size_t count = 0;
            for (auto& param : lang_symbol->m_template_type_instances->m_template_params)
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
        _wo_strdup(wo::wstr_to_str(*lang_symbol->m_name).c_str()),
        template_param_count,
        template_params,
        lang_symbol->m_symbol_declare_location.has_value() ? WO_TRUE : WO_FALSE,
        {},
    };
    if (result->m_has_location)
    {
        auto& loc = lang_symbol->m_symbol_declare_location.value();
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

struct _wo_lspv2_expr_collection_iter
{
    using source_expr_collection_iter_t =
        _wo_lspv2_source_meta::source_expr_collection_t::const_iterator;

    source_expr_collection_iter_t m_current;
    source_expr_collection_iter_t m_end;
};
struct _wo_lspv2_expr_collection
{
    wo_pstring_t m_file_name;
    const _wo_lspv2_source_meta::expr_location_map_t* m_expr_collection;
};
struct _wo_lspv2_expr_iter
{
    wo::ast::AstBase::location_t m_begin_location;
    wo::ast::AstBase::location_t m_end_location;

    _wo_lspv2_source_meta::expr_location_map_t::const_iterator m_current_collection;
    _wo_lspv2_source_meta::expr_location_map_t::const_iterator m_end_collection;

    _wo_lspv2_source_meta::expr_map_t::const_iterator m_most_front;
    _wo_lspv2_source_meta::expr_map_t::const_iterator m_post_current;
};

wo_lspv2_expr_collection_iter* wo_lspv2_meta_expr_collection_iter(
    wo_lspv2_source_meta* meta)
{
    return new wo_lspv2_expr_collection_iter{
        meta->m_source_expr_collection.begin(),
        meta->m_source_expr_collection.end(),
    };
}
wo_lspv2_expr_collection* /* null if end */ wo_lspv2_expr_collection_next(
    wo_lspv2_expr_collection_iter* iter)
{
    if (iter->m_current == iter->m_end)
    {
        delete iter;
        return nullptr;
    }
    else
    {
        auto collect_iter = iter->m_current++;
        return new wo_lspv2_expr_collection{
            collect_iter->first,
            &collect_iter->second,
        };
    }
}
void wo_lspv2_expr_collection_free(wo_lspv2_expr_collection* collection)
{
    delete collection;
}
wo_lspv2_expr_collection_info* wo_lspv2_expr_collection_get_info(
    wo_lspv2_expr_collection* collection)
{
    return new wo_lspv2_expr_collection_info{
        _wo_strdup(wo::wstr_to_str(*collection->m_file_name).c_str()),
    };
}
void wo_lspv2_expr_collection_info_free(wo_lspv2_expr_collection_info* collection)
{
    free((void*)collection->m_file_name);
    delete collection;
}
wo_lspv2_expr_iter* /* null if not found */ wo_lspv2_expr_collection_get_by_range(
    wo_lspv2_expr_collection* collection,
    wo_size_t begin_row,
    wo_size_t begin_col,
    wo_size_t end_row,
    wo_size_t end_col)
{
    wo::ast::AstBase::location_t begin_location{ begin_row, begin_col };
    wo::ast::AstBase::location_t end_location{ end_row, end_col };

    auto begin_iter =
        collection->m_expr_collection->begin();
    auto upper_bound_iter =
        collection->m_expr_collection->upper_bound(begin_location);

    if (upper_bound_iter == begin_iter)
        return nullptr;

    return new wo_lspv2_expr_iter{
        begin_location,
        end_location,
        begin_iter,
        upper_bound_iter,
        begin_iter->second.lower_bound(wo::ast::AstBase::location_t{ end_row, end_col }),
        begin_iter->second.end(),
    };
}
wo_lspv2_expr* /* null if end */ wo_lspv2_expr_next(wo_lspv2_expr_iter* iter)
{
    if (iter == nullptr)
        return nullptr;

    if (iter->m_post_current == iter->m_most_front)
    {
        for (;;)
        {
            ++iter->m_current_collection;
            if (iter->m_current_collection == iter->m_end_collection)
            {
                delete iter;
                return nullptr;
            }

            iter->m_most_front = iter->m_current_collection->second.lower_bound(
                iter->m_begin_location);
            iter->m_post_current = iter->m_current_collection->second.end();

            if (iter->m_most_front != iter->m_post_current)
                break;
        }
    }

    return reinterpret_cast<wo_lspv2_expr*>(
        (--iter->m_post_current)->second);
}

wo_lspv2_expr_info* wo_lspv2_expr_get_info(wo_lspv2_expr* expr)
{
    wo::ast::AstBase* ast_base = std::launder(reinterpret_cast<wo::ast::AstBase*>(expr));
    wo::lang_TypeInstance* type_instance = nullptr;
    wo::lang_Symbol* variable_or_type_symbol = nullptr;
    wo_bool_t is_value = WO_TRUE;
    wo_value const_value = nullptr;

    if (ast_base->node_type == wo::ast::AstBase::node_type_t::AST_TYPE_HOLDER)
    {
        wo::ast::AstTypeHolder* type_holder = static_cast<wo::ast::AstTypeHolder*>(ast_base);

        is_value = WO_FALSE;
        type_instance = type_holder->m_LANG_determined_type.value();

        if (type_holder->m_formal == wo::ast::AstTypeHolder::IDENTIFIER)
        {
            variable_or_type_symbol =
                type_holder->m_typeform.m_identifier->m_LANG_determined_symbol.value();
        }
    }
    else
    {
        wo_assert(ast_base->node_type >= wo::ast::AstBase::node_type_t::AST_VALUE_begin
            && ast_base->node_type < wo::ast::AstBase::node_type_t::AST_VALUE_end);

        wo::ast::AstValueBase* value = static_cast<wo::ast::AstValueBase*>(ast_base);
        
        type_instance = value->m_LANG_determined_type.value();
        if (value->node_type == wo::ast::AstBase::node_type_t::AST_VALUE_VARIABLE)
        {
            auto* ast_value_variable = static_cast<wo::ast::AstValueVariable*>(value);
            if (ast_value_variable->m_LANG_variable_instance.has_value())
                variable_or_type_symbol = ast_value_variable->m_LANG_variable_instance.value()->m_symbol;
        }

        if (value->m_evaled_const_value.has_value())
        {
            const_value = 
                reinterpret_cast<wo_value>(
                    &value->m_evaled_const_value.value());
        }
    }

    return new wo_lspv2_expr_info{
        reinterpret_cast<wo_lspv2_type*>(type_instance),
        wo_lspv2_location {
            _wo_strdup(wo::wstr_to_str(*ast_base->source_location.source_file).c_str()),
            { ast_base->source_location.begin_at.row, ast_base->source_location.begin_at.column },
            { ast_base->source_location.end_at.row, ast_base->source_location.end_at.column },},
        variable_or_type_symbol == nullptr ? nullptr : reinterpret_cast<wo_lspv2_symbol*>(variable_or_type_symbol),
        is_value,
        const_value,
    };
}
void wo_lspv2_expr_info_free(wo_lspv2_expr_info* expr_info)
{
    free((void*)expr_info->m_location.m_file_name);
    delete expr_info;
}

// Type API
wo_lspv2_type_info* wo_lspv2_type_get_info(
    wo_lspv2_type* type, wo_lspv2_source_meta* meta)
{
    wo::lang_TypeInstance* type_instance =
        std::launder(reinterpret_cast<wo::lang_TypeInstance*>(type));

    auto* result = new wo_lspv2_type_info{
        meta->m_langcontext_if_passed_grammar.value()->get_type_name(type_instance),
        reinterpret_cast<wo_lspv2_symbol*>(type_instance->m_symbol),
        0,
        nullptr,
    };

    if (type_instance->m_instance_template_arguments.has_value())
    {
        auto& template_arguments = type_instance->m_instance_template_arguments.value();

        result->m_template_arguments_count = template_arguments.size();
        result->m_template_arguments = (wo_lspv2_type**)malloc(
            sizeof(wo_lspv2_type*) * result->m_template_arguments_count);

        size_t count = 0;
        for (auto& arg : template_arguments)
        {
            result->m_template_arguments[count++] =
                reinterpret_cast<wo_lspv2_type*>(arg);
        }
    }

    return result;
}
void wo_lspv2_type_info_free(wo_lspv2_type_info* info)
{
    // This string stored in context, no need to free
    // free((void*)info->m_name);

    free((void*)info->m_template_arguments);

    delete info;
}

wo_lspv2_type_struct_info* wo_lspv2_type_get_struct_info(
    wo_lspv2_type* type, wo_lspv2_source_meta* meta)
{
    wo::lang_TypeInstance* type_instance =
        std::launder(reinterpret_cast<wo::lang_TypeInstance*>(type));

    auto determined_type = type_instance->get_determined_type();
    if (determined_type.has_value())
    {
        auto* determined_type_instance = determined_type.value();
        if (determined_type_instance->m_base_type
            == wo::lang_TypeInstance::DeterminedType::base_type::STRUCT)
        {
            auto* struct_type_detail =
                determined_type_instance->m_external_type_description.m_struct;

            wo_lspv2_type_struct_info* result = new wo_lspv2_type_struct_info{
                struct_type_detail->m_member_types.size(),
            };
            result->m_member_names = (const char**)malloc(
                sizeof(const char*) * result->m_member_count);
            result->m_member_types = (wo_lspv2_type**)malloc(
                sizeof(wo_lspv2_type*) * result->m_member_count);

            for (auto& [member, member_detail] : struct_type_detail->m_member_types)
            {
                result->m_member_names[member_detail.m_offset] =
                    _wo_strdup(wo::wstr_to_str(*member).c_str());
                result->m_member_types[member_detail.m_offset] =
                    reinterpret_cast<wo_lspv2_type*>(
                        member_detail.m_member_type);
            }

            return result;
        }
    }
    return nullptr;
}

void wo_lspv2_type_struct_info_free(wo_lspv2_type_struct_info* info)
{
    for (size_t i = 0; i < info->m_member_count; i++)
    {
        free((void*)info->m_member_names[i]);
    }
    free((void*)info->m_member_names);
    free((void*)info->m_member_types);
    delete info;
}

struct _wo_lspv2_macro_iter
{
    using macro_iter_t = decltype(wo::LangContext::m_macros)::const_iterator;
    macro_iter_t m_current;
    macro_iter_t m_end;
};

wo_lspv2_macro_iter* wo_lspv2_meta_macro_iter(wo_lspv2_source_meta* meta)
{
    if (meta->m_langcontext_if_passed_grammar.has_value())
    {
        auto& macro_list = meta->m_langcontext_if_passed_grammar.value()->m_macros;
        return new _wo_lspv2_macro_iter{
            macro_list.cbegin(),
            macro_list.cend(),
        };
    }
    return nullptr;
}
wo_lspv2_macro* /* null if end */ wo_lspv2_macro_next(wo_lspv2_macro_iter* iter)
{
    if (iter->m_current == iter->m_end)
    {
        delete iter;
        return nullptr;
    }
    else
    {
        return reinterpret_cast<wo_lspv2_macro*>(
            (iter->m_current++)->get());
    }
}
wo_lspv2_macro_info* wo_lspv2_macro_get_info(wo_lspv2_macro* macro)
{
    wo::lang_Macro* lang_macro = std::launder(reinterpret_cast<wo::lang_Macro*>(macro));
    return new wo_lspv2_macro_info{
        _wo_strdup(wo::wstr_to_str(*lang_macro->m_name).c_str()),
        wo_lspv2_location{
            _wo_strdup(wo::wstr_to_str(*lang_macro->m_location.source_file).c_str()),
            { lang_macro->m_location.begin_at.row, lang_macro->m_location.begin_at.column },
            { lang_macro->m_location.end_at.row, lang_macro->m_location.end_at.column },
        },
    };
    
}
void wo_lspv2_macro_info_free(wo_lspv2_macro_info* info)
{
    free((void*)info->m_name);
    free((void*)info->m_location.m_file_name);
    delete info;
}

#endif