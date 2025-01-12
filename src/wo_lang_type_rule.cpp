#include "wo_lang.hpp"

namespace wo
{
#ifndef WO_DISABLE_COMPILER
    lang_TypeInstance::TypeCheckResult LangContext::is_type_accepted(
        lexer& lex,
        ast::AstBase* node,
        lang_TypeInstance* accepter,
        lang_TypeInstance* provider)
    {
        // TODO CHECK LIST:
        // 1. Make sure recursive-type will not cause infinite loop and have good habit.

        if (accepter == provider)
            return lang_TypeInstance::TypeCheckResult::ACCEPT;

        auto fnd = accepter->m_LANG_accepted_types.find(provider);
        if (fnd != accepter->m_LANG_accepted_types.end())
            return fnd->second;

        // Set pending state.
        accepter->m_LANG_accepted_types.insert(
            std::make_pair(provider, lang_TypeInstance::TypeCheckResult::PENDING));

        auto judge =
            [&]()-> lang_TypeInstance::TypeCheckResult
            {
                lang_TypeInstance* immutable_accepter = immutable_type(accepter);
                lang_TypeInstance* immutable_provider = immutable_type(provider);

                // If provider is nothing, OK.
                if (immutable_provider == m_origin_types.m_nothing.m_type_instance)
                    return lang_TypeInstance::TypeCheckResult::ACCEPT;

                // If mutable not match, except nothing, failed.
                if (accepter->is_mutable() != provider->is_mutable())
                    return lang_TypeInstance::TypeCheckResult::REJECT;

                // If not same symbol, fail.
                if (immutable_provider->m_symbol != immutable_accepter->m_symbol)
                    return lang_TypeInstance::TypeCheckResult::REJECT;

                bool base_type_not_determined = false;

                auto determined_accepter_may_null = immutable_accepter->get_determined_type();
                auto determined_provider_may_null = immutable_provider->get_determined_type();

                if (!determined_accepter_may_null)
                {
                    base_type_not_determined = true;
                    lex.lang_error(lexer::errorlevel::error, node,
                        WO_ERR_TYPE_NAMED_DETERMINED_FAILED,
                        get_type_name_w(accepter));
                }
                if (!determined_provider_may_null)
                {
                    base_type_not_determined = true;
                    lex.lang_error(lexer::errorlevel::error, node,
                        WO_ERR_TYPE_NAMED_DETERMINED_FAILED,
                        get_type_name_w(provider));
                }
                if (base_type_not_determined)
                    return lang_TypeInstance::TypeCheckResult::REJECT;

                auto* determined_accepter = determined_accepter_may_null.value();
                auto* determined_provider = determined_provider_may_null.value();

                if (determined_accepter->m_base_type != determined_provider->m_base_type)
                    return lang_TypeInstance::TypeCheckResult::REJECT;

                if (immutable_provider->m_symbol->m_is_builtin
                    || determined_accepter->m_base_type == lang_TypeInstance::DeterminedType::UNION)
                {
                    bool sub_match_is_pending = false;

                    switch (determined_accepter->m_base_type)
                    {
                    case lang_TypeInstance::DeterminedType::ARRAY:
                    case lang_TypeInstance::DeterminedType::VECTOR:
                    {
                        auto ext_array_accepter = determined_accepter->m_external_type_description.m_array_or_vector;
                        auto ext_array_provider = determined_provider->m_external_type_description.m_array_or_vector;

                        switch (is_type_accepted(
                            lex,
                            node,
                            ext_array_accepter->m_element_type,
                            ext_array_provider->m_element_type))
                        {
                        case lang_TypeInstance::TypeCheckResult::REJECT:
                            return lang_TypeInstance::TypeCheckResult::REJECT;
                        case lang_TypeInstance::TypeCheckResult::PENDING:
                            sub_match_is_pending = true;
                            break;
                        default:
                            break;
                        }

                        break;
                    }
                    case lang_TypeInstance::DeterminedType::DICTIONARY:
                    case lang_TypeInstance::DeterminedType::MAPPING:
                    {
                        auto ext_dict_accepter = determined_accepter->m_external_type_description.m_dictionary_or_mapping;
                        auto ext_dict_provider = determined_provider->m_external_type_description.m_dictionary_or_mapping;

                        switch (is_type_accepted(
                            lex,
                            node,
                            ext_dict_accepter->m_key_type,
                            ext_dict_provider->m_key_type))
                        {
                        case lang_TypeInstance::TypeCheckResult::REJECT:
                            return lang_TypeInstance::TypeCheckResult::REJECT;
                        case lang_TypeInstance::TypeCheckResult::PENDING:
                            sub_match_is_pending = true;
                            break;
                        default:
                            break;
                        }

                        switch (is_type_accepted(
                            lex,
                            node,
                            ext_dict_accepter->m_value_type,
                            ext_dict_provider->m_value_type))
                        {
                        case lang_TypeInstance::TypeCheckResult::REJECT:
                            return lang_TypeInstance::TypeCheckResult::REJECT;
                        case lang_TypeInstance::TypeCheckResult::PENDING:
                            sub_match_is_pending = true;
                            break;
                        default:
                            break;
                        }
                        break;
                    }
                    case lang_TypeInstance::DeterminedType::TUPLE:
                    {
                        auto ext_tuple_accepter = determined_accepter->m_external_type_description.m_tuple;
                        auto ext_tuple_provider = determined_provider->m_external_type_description.m_tuple;

                        if (ext_tuple_accepter->m_element_types.size()
                            != ext_tuple_provider->m_element_types.size())
                            return lang_TypeInstance::TypeCheckResult::REJECT;

                        auto it_accepter = ext_tuple_accepter->m_element_types.begin();
                        auto it_provider = ext_tuple_provider->m_element_types.begin();
                        auto it_accepter_end = ext_tuple_accepter->m_element_types.end();

                        for (; it_accepter != it_accepter_end; ++it_accepter, ++it_provider)
                        {
                            switch (is_type_accepted(
                                lex,
                                node,
                                *it_accepter,
                                *it_provider))
                            {
                            case lang_TypeInstance::TypeCheckResult::REJECT:
                                return lang_TypeInstance::TypeCheckResult::REJECT;
                            case lang_TypeInstance::TypeCheckResult::PENDING:
                                sub_match_is_pending = true;
                                break;
                            default:
                                break;
                            }
                        }

                        break;
                    }
                    case lang_TypeInstance::DeterminedType::STRUCT:
                    {
                        auto ext_struct_accepter = determined_accepter->m_external_type_description.m_struct;
                        auto ext_struct_provider = determined_provider->m_external_type_description.m_struct;

                        if (ext_struct_accepter->m_member_types.size()
                            != ext_struct_provider->m_member_types.size())
                            return lang_TypeInstance::TypeCheckResult::REJECT;

                        for (auto& [name, field] : ext_struct_accepter->m_member_types)
                        {
                            auto fnd = ext_struct_provider->m_member_types.find(name);
                            if (fnd == ext_struct_provider->m_member_types.end())
                                return lang_TypeInstance::TypeCheckResult::REJECT;

                            if (field.m_offset != fnd->second.m_offset)
                                return lang_TypeInstance::TypeCheckResult::REJECT;

                            switch (is_type_accepted(
                                lex,
                                node,
                                field.m_member_type,
                                fnd->second.m_member_type))
                            {
                            case lang_TypeInstance::TypeCheckResult::REJECT:
                                return lang_TypeInstance::TypeCheckResult::REJECT;
                            case lang_TypeInstance::TypeCheckResult::PENDING:
                                sub_match_is_pending = true;
                                break;
                            default:
                                break;
                            }
                        }
                        break;
                    }
                    case lang_TypeInstance::DeterminedType::UNION:
                    {
                        auto ext_union_accepter = determined_accepter->m_external_type_description.m_union;
                        auto ext_union_provider = determined_provider->m_external_type_description.m_union;

                        if (ext_union_accepter->m_union_label.size()
                            != ext_union_provider->m_union_label.size())
                            return lang_TypeInstance::TypeCheckResult::REJECT;

                        for (auto& [label, type] : ext_union_accepter->m_union_label)
                        {
                            auto fnd = ext_union_provider->m_union_label.find(label);
                            if (fnd == ext_union_provider->m_union_label.end())
                                return lang_TypeInstance::TypeCheckResult::REJECT;

                            if (type.m_label != fnd->second.m_label)
                                return lang_TypeInstance::TypeCheckResult::REJECT;

                            if (type.m_item_type.has_value() != fnd->second.m_item_type.has_value())
                                return lang_TypeInstance::TypeCheckResult::REJECT;
                            if (type.m_item_type.has_value())
                            {
                                switch (is_type_accepted(
                                    lex,
                                    node,
                                    type.m_item_type.value(),
                                    fnd->second.m_item_type.value()))
                                {
                                case lang_TypeInstance::TypeCheckResult::REJECT:
                                    return lang_TypeInstance::TypeCheckResult::REJECT;
                                case lang_TypeInstance::TypeCheckResult::PENDING:
                                    sub_match_is_pending = true;
                                    break;
                                default:
                                    break;
                                }
                            }
                        }
                        break;
                    }
                    case lang_TypeInstance::DeterminedType::FUNCTION:
                    {
                        auto ext_func_accepter = determined_accepter->m_external_type_description.m_function;
                        auto ext_func_provider = determined_provider->m_external_type_description.m_function;

                        if (ext_func_accepter->m_is_variadic
                            != ext_func_provider->m_is_variadic)
                            return lang_TypeInstance::TypeCheckResult::REJECT;

                        if (ext_func_accepter->m_param_types.size()
                            != ext_func_provider->m_param_types.size())
                            return lang_TypeInstance::TypeCheckResult::REJECT;

                        auto it_accepter = ext_func_accepter->m_param_types.begin();
                        auto it_provider = ext_func_provider->m_param_types.begin();
                        auto it_accepter_end = ext_func_accepter->m_param_types.end();

                        for (; it_accepter != it_accepter_end; ++it_accepter, ++it_provider)
                        {
                            if (*it_accepter != *it_provider)
                                return lang_TypeInstance::TypeCheckResult::REJECT;
                        }

                        switch (is_type_accepted(
                            lex,
                            node,
                            ext_func_accepter->m_return_type,
                            ext_func_provider->m_return_type))
                        {
                        case lang_TypeInstance::TypeCheckResult::REJECT:
                            return lang_TypeInstance::TypeCheckResult::REJECT;
                        case lang_TypeInstance::TypeCheckResult::PENDING:
                            sub_match_is_pending = true;
                            break;
                        default:
                            break;
                        }

                        break;
                    }
                    }

                    // TODO: If sub match PENDING, return PENDING here.
                    return sub_match_is_pending
                        ? lang_TypeInstance::TypeCheckResult::PENDING
                        : lang_TypeInstance::TypeCheckResult::ACCEPT;
                }
                else
                    return lang_TypeInstance::TypeCheckResult::REJECT;
            };

        auto result = judge();
        result = result == lang_TypeInstance::TypeCheckResult::PENDING
            ? lang_TypeInstance::TypeCheckResult::ACCEPT
            : result;

        wo_assert(accepter->m_LANG_accepted_types.at(provider)
            == lang_TypeInstance::TypeCheckResult::PENDING);

        accepter->m_LANG_accepted_types.at(provider) = result;

        return result;
    }

    lang_TypeInstance::TypeCheckResult LangContext::check_cast_able(
        lexer& lex,
        ast::AstBase* node,
        lang_TypeInstance* aimtype,
        lang_TypeInstance* srctype)
    {
        // TODO CHECK LIST:
        // 1. Make sure recursive-type will not cause infinite loop and have good habit.
        if (aimtype == srctype)
            return lang_TypeInstance::TypeCheckResult::ACCEPT;

        auto fnd = aimtype->m_LANG_castfrom_types.find(srctype);
        if (fnd != aimtype->m_LANG_castfrom_types.end())
            return fnd->second;

        // Set pending state.
        aimtype->m_LANG_castfrom_types.insert(
            std::make_pair(srctype, lang_TypeInstance::TypeCheckResult::PENDING));

        auto* immutable_aimtype = immutable_type(aimtype);
        auto* immutable_srctype = immutable_type(srctype);

        auto judge =
            [&]()-> lang_TypeInstance::TypeCheckResult
            {
                // 0. If aimtype accept srctype, Ok
                if (lang_TypeInstance::TypeCheckResult::ACCEPT == is_type_accepted(lex, node, aimtype, immutable_srctype))
                    return lang_TypeInstance::TypeCheckResult::ACCEPT;

                // 2. If aimtype is void, Ok
                if (immutable_aimtype == m_origin_types.m_void.m_type_instance)
                    return lang_TypeInstance::TypeCheckResult::ACCEPT;

                // 1. If srctype is void. reject.
                if (immutable_srctype == m_origin_types.m_void.m_type_instance)
                    return lang_TypeInstance::TypeCheckResult::REJECT;

                // 3. If aimtype is dynamic, Ok
                if (immutable_aimtype == m_origin_types.m_dynamic.m_type_instance)
                    return lang_TypeInstance::TypeCheckResult::ACCEPT;
             
                // Following step needs determined base type.
                auto determined_srctype_may_null = immutable_srctype->get_determined_type();
                auto determined_aimtype_may_null = immutable_aimtype->get_determined_type();

                if (!determined_srctype_may_null)
                {
                    lex.lang_error(lexer::errorlevel::error, node,
                        WO_ERR_TYPE_NAMED_DETERMINED_FAILED,
                        get_type_name_w(srctype));
                    return lang_TypeInstance::TypeCheckResult::REJECT;
                }
                if (!determined_aimtype_may_null)
                {
                    lex.lang_error(lexer::errorlevel::error, node,
                        WO_ERR_TYPE_NAMED_DETERMINED_FAILED,
                        get_type_name_w(aimtype));
                    return lang_TypeInstance::TypeCheckResult::REJECT;
                }

                auto* determined_srctype = determined_srctype_may_null.value();
                auto* determined_aimtype = determined_aimtype_may_null.value();

                // 4. All the type can cast to string, besides void.
                if (immutable_aimtype == m_origin_types.m_string.m_type_instance &&
                    determined_srctype->m_base_type != lang_TypeInstance::DeterminedType::VOID)
                {
                    return lang_TypeInstance::TypeCheckResult::ACCEPT;
                }

                // 5. bool, handle, real, string can cast to int.
                if (immutable_aimtype == m_origin_types.m_int.m_type_instance &&
                    (determined_srctype->m_base_type == lang_TypeInstance::DeterminedType::BOOLEAN ||
                        determined_srctype->m_base_type == lang_TypeInstance::DeterminedType::HANDLE ||
                        determined_srctype->m_base_type == lang_TypeInstance::DeterminedType::REAL ||
                        determined_srctype->m_base_type == lang_TypeInstance::DeterminedType::STRING))
                {
                    return lang_TypeInstance::TypeCheckResult::ACCEPT;
                }

                // 6. bool, int, handle, string can cast to real.
                if (immutable_aimtype == m_origin_types.m_real.m_type_instance &&
                    (determined_srctype->m_base_type == lang_TypeInstance::DeterminedType::BOOLEAN ||
                        determined_srctype->m_base_type == lang_TypeInstance::DeterminedType::INTEGER ||
                        determined_srctype->m_base_type == lang_TypeInstance::DeterminedType::HANDLE ||
                        determined_srctype->m_base_type == lang_TypeInstance::DeterminedType::STRING))
                {
                    return lang_TypeInstance::TypeCheckResult::ACCEPT;
                }

                // 7. bool, int, gchandle, real, string can cast to handle.
                if (immutable_aimtype == m_origin_types.m_handle.m_type_instance &&
                    (determined_srctype->m_base_type == lang_TypeInstance::DeterminedType::BOOLEAN ||
                        determined_srctype->m_base_type == lang_TypeInstance::DeterminedType::INTEGER ||
                        determined_srctype->m_base_type == lang_TypeInstance::DeterminedType::GCHANDLE ||
                        determined_srctype->m_base_type == lang_TypeInstance::DeterminedType::REAL ||
                        determined_srctype->m_base_type == lang_TypeInstance::DeterminedType::STRING))
                {
                    return lang_TypeInstance::TypeCheckResult::ACCEPT;
                }

                // 8. integer, handle, real, string can cast to bool.
                if (immutable_aimtype == m_origin_types.m_bool.m_type_instance &&
                    (determined_srctype->m_base_type == lang_TypeInstance::DeterminedType::INTEGER ||
                        determined_srctype->m_base_type == lang_TypeInstance::DeterminedType::HANDLE ||
                        determined_srctype->m_base_type == lang_TypeInstance::DeterminedType::REAL ||
                        determined_srctype->m_base_type == lang_TypeInstance::DeterminedType::STRING))
                {
                    return lang_TypeInstance::TypeCheckResult::ACCEPT;
                }

                // 8. User defined type can cast oto basic type, basic type can cast to user defined type.
                std::pair<
                    lang_TypeInstance::DeterminedType::base_type,
                    OriginTypeHolder::OriginNoTemplateSymbolAndInstance OriginTypeHolder::*>
                    base_type_record_table[] =
                {
                    std::make_pair(lang_TypeInstance::DeterminedType::VOID, &OriginTypeHolder::m_void),
                    std::make_pair(lang_TypeInstance::DeterminedType::DYNAMIC, &OriginTypeHolder::m_dynamic),
                    std::make_pair(lang_TypeInstance::DeterminedType::NIL, &OriginTypeHolder::m_nil),
                    std::make_pair(lang_TypeInstance::DeterminedType::INTEGER, &OriginTypeHolder::m_int),
                    std::make_pair(lang_TypeInstance::DeterminedType::REAL, &OriginTypeHolder::m_real),
                    std::make_pair(lang_TypeInstance::DeterminedType::HANDLE, &OriginTypeHolder::m_handle),
                    std::make_pair(lang_TypeInstance::DeterminedType::BOOLEAN, &OriginTypeHolder::m_bool),
                    std::make_pair(lang_TypeInstance::DeterminedType::STRING, &OriginTypeHolder::m_string),
                    std::make_pair(lang_TypeInstance::DeterminedType::GCHANDLE, &OriginTypeHolder::m_gchandle),
                };
                for (auto& [base_type, instance_member] : base_type_record_table)
                {
                    if ((immutable_aimtype == (m_origin_types.*instance_member).m_type_instance &&
                        determined_srctype->m_base_type == base_type)
                        || (immutable_srctype == (m_origin_types.*instance_member).m_type_instance &&
                            determined_aimtype->m_base_type == base_type))
                    {
                        return lang_TypeInstance::TypeCheckResult::ACCEPT;
                    }
                }

                // 9. User defined type based on:
                // function, struct, tuple, array, vec, map, dict, need to check detailed.
                if (determined_srctype->m_base_type == determined_aimtype->m_base_type)
                {
                    switch (determined_srctype->m_base_type)
                    {
                    case lang_TypeInstance::DeterminedType::DICTIONARY:
                    case lang_TypeInstance::DeterminedType::MAPPING:
                    {
                        auto ext_dict_srctype = determined_srctype
                            ->m_external_type_description.m_dictionary_or_mapping;
                        auto ext_dict_aimtype = determined_aimtype
                            ->m_external_type_description.m_dictionary_or_mapping;

                        if (lang_TypeInstance::TypeCheckResult::ACCEPT
                            == is_type_accepted(lex, node, ext_dict_aimtype->m_key_type, ext_dict_srctype->m_key_type)
                            && lang_TypeInstance::TypeCheckResult::ACCEPT
                            == is_type_accepted(lex, node, ext_dict_aimtype->m_value_type, ext_dict_srctype->m_value_type))
                        {
                            return lang_TypeInstance::TypeCheckResult::ACCEPT;
                        }
                        break;
                    }
                    case lang_TypeInstance::DeterminedType::ARRAY:
                    case lang_TypeInstance::DeterminedType::VECTOR:
                    {
                        auto ext_array_srctype = determined_srctype
                            ->m_external_type_description.m_array_or_vector;
                        auto ext_array_aimtype = determined_aimtype
                            ->m_external_type_description.m_array_or_vector;

                        if (lang_TypeInstance::TypeCheckResult::ACCEPT
                            == is_type_accepted(
                                lex,
                                node,
                                ext_array_aimtype->m_element_type,
                                ext_array_srctype->m_element_type))
                        {
                            return lang_TypeInstance::TypeCheckResult::ACCEPT;
                        }
                        break;
                    }
                    case lang_TypeInstance::DeterminedType::TUPLE:
                    {
                        auto ext_tuple_srctype = determined_srctype
                            ->m_external_type_description.m_tuple;
                        auto ext_tuple_aimtype = determined_aimtype
                            ->m_external_type_description.m_tuple;

                        if (ext_tuple_aimtype->m_element_types.size()
                            == ext_tuple_srctype->m_element_types.size())
                        {
                            auto it_srctype = ext_tuple_srctype->m_element_types.begin();
                            auto it_aimtype = ext_tuple_aimtype->m_element_types.begin();
                            auto it_srctype_end = ext_tuple_srctype->m_element_types.end();

                            bool accpet = true;
                            for (; it_srctype != it_srctype_end; ++it_srctype, ++it_aimtype)
                            {
                                if (lang_TypeInstance::TypeCheckResult::ACCEPT
                                    != is_type_accepted(lex, node, *it_aimtype, *it_srctype))
                                {
                                    accpet = false;
                                    break;
                                }
                            }

                            if (accpet)
                                return lang_TypeInstance::TypeCheckResult::ACCEPT;
                        }
                        break;
                    }
                    case lang_TypeInstance::DeterminedType::STRUCT:
                    {
                        auto ext_struct_srctype = determined_srctype
                            ->m_external_type_description.m_struct;
                        auto ext_struct_aimtype = determined_aimtype
                            ->m_external_type_description.m_struct;

                        if (ext_struct_aimtype->m_member_types.size()
                            == ext_struct_srctype->m_member_types.size())
                        {
                            bool accpet = true;
                            for (auto& [name, field] : ext_struct_aimtype->m_member_types)
                            {
                                auto fnd = ext_struct_srctype->m_member_types.find(name);
                                if (fnd == ext_struct_srctype->m_member_types.end()
                                    || field.m_offset != fnd->second.m_offset
                                    || lang_TypeInstance::TypeCheckResult::ACCEPT
                                    != is_type_accepted(lex, node, field.m_member_type, fnd->second.m_member_type))
                                {
                                    accpet = false;
                                    break;
                                }
                            }
                            if (accpet)
                                return lang_TypeInstance::TypeCheckResult::ACCEPT;
                        }
                        break;
                    }
                    case lang_TypeInstance::DeterminedType::FUNCTION:
                    {
                        auto ext_func_srctype = determined_srctype
                            ->m_external_type_description.m_function;
                        auto ext_func_aimtype = determined_aimtype
                            ->m_external_type_description.m_function;

                        if (ext_func_aimtype->m_is_variadic == ext_func_srctype->m_is_variadic
                            && ext_func_aimtype->m_param_types.size() == ext_func_srctype->m_param_types.size())
                        {
                            auto it_srctype = ext_func_srctype->m_param_types.begin();
                            auto it_aimtype = ext_func_aimtype->m_param_types.begin();
                            auto it_srctype_end = ext_func_srctype->m_param_types.end();

                            bool accept = true;
                            for (; it_srctype != it_srctype_end; ++it_srctype, ++it_aimtype)
                            {
                                if (*it_aimtype != *it_srctype)
                                {
                                    accept = false;
                                    break;
                                }
                            }

                            if (accept
                                && lang_TypeInstance::TypeCheckResult::ACCEPT == is_type_accepted(
                                    lex,
                                    node,
                                    ext_func_aimtype->m_return_type,
                                    ext_func_srctype->m_return_type))
                                return lang_TypeInstance::TypeCheckResult::ACCEPT;
                        }
                        break;
                    }
                    default:
                        break;
                    }
                }

                // 10. Following types can cast from dynamic:
                //  nil, int, real, handle, bool, string, gchandle, 
                //  array<dynamic>, map<dynamic, dynamic>
                if (immutable_srctype == m_origin_types.m_dynamic.m_type_instance)
                {
                    if ((immutable_aimtype == m_origin_types.m_nil.m_type_instance
                        || immutable_aimtype == m_origin_types.m_int.m_type_instance
                        || immutable_aimtype == m_origin_types.m_real.m_type_instance
                        || immutable_aimtype == m_origin_types.m_handle.m_type_instance
                        || immutable_aimtype == m_origin_types.m_bool.m_type_instance
                        || immutable_aimtype == m_origin_types.m_string.m_type_instance
                        || immutable_aimtype == m_origin_types.m_gchandle.m_type_instance
                        || immutable_aimtype == m_origin_types.m_array_dynamic
                        || immutable_aimtype == m_origin_types.m_dictionary_dynamic))
                    {
                        return lang_TypeInstance::TypeCheckResult::ACCEPT;
                    }
                }

                // All missmatch, failed.
                return lang_TypeInstance::TypeCheckResult::REJECT;
            };

        auto result = judge();
        result = result == lang_TypeInstance::TypeCheckResult::PENDING
            ? lang_TypeInstance::TypeCheckResult::ACCEPT
            : result;

        wo_assert(aimtype->m_LANG_castfrom_types.at(srctype)
            == lang_TypeInstance::TypeCheckResult::PENDING);

        aimtype->m_LANG_castfrom_types.at(srctype) = result;

        return result;
    }
#endif
}