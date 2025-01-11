#include "wo_lang.hpp"

namespace wo
{
#ifndef WO_DISABLE_COMPILER
    bool LangContext::is_type_accepted(
        lexer& lex,
        ast::AstBase* node,
        lang_TypeInstance* accepter,
        lang_TypeInstance* provider)
    {
        // TODO CHECK LIST:
        // 1. Make sure recursive-type will not cause infinite loop and have good habit.

        if (accepter == provider)
            return true;

        auto fnd = accepter->m_LANG_accepted_types.find(accepter);
        if (fnd != accepter->m_LANG_accepted_types.end())
            return fnd->second;

        auto judge =
            [&]()-> bool {

            lang_TypeInstance* immutable_accepter = immutable_type(accepter);
            lang_TypeInstance* immutable_provider = immutable_type(provider);

            // If provider is nothing, OK.
            if (immutable_provider == m_origin_types.m_nothing.m_type_instance)
                return true;

            // If mutable not match, except nothing, failed.
            if (accepter->is_mutable() != provider->is_mutable())
                return false;

            // If not same symbol, fail.
            if (immutable_provider->m_symbol != immutable_accepter->m_symbol)
                return false;

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
                return false;

            auto* determined_accepter = determined_accepter_may_null.value();
            auto* determined_provider = determined_provider_may_null.value();

            if (determined_accepter->m_base_type != determined_provider->m_base_type)
                return false;

            switch (determined_accepter->m_base_type)
            {
            case lang_TypeInstance::DeterminedType::ARRAY:
            case lang_TypeInstance::DeterminedType::VECTOR:
            {
                auto ext_array_accepter = determined_accepter->m_external_type_description.m_array_or_vector;
                auto ext_array_provider = determined_provider->m_external_type_description.m_array_or_vector;

                if (!is_type_accepted(
                    lex,
                    node,
                    ext_array_accepter->m_element_type,
                    ext_array_provider->m_element_type))
                    return false;
                break;
            }
            case lang_TypeInstance::DeterminedType::DICTIONARY:
            case lang_TypeInstance::DeterminedType::MAPPING:
            {
                auto ext_dict_accepter = determined_accepter->m_external_type_description.m_dictionary_or_mapping;
                auto ext_dict_provider = determined_provider->m_external_type_description.m_dictionary_or_mapping;

                if (!is_type_accepted(
                    lex,
                    node,
                    ext_dict_accepter->m_key_type,
                    ext_dict_provider->m_key_type))
                    return false;
                if (!is_type_accepted(
                    lex,
                    node,
                    ext_dict_accepter->m_value_type,
                    ext_dict_provider->m_value_type))
                    return false;
                break;
            }
            case lang_TypeInstance::DeterminedType::TUPLE:
            {
                auto ext_tuple_accepter = determined_accepter->m_external_type_description.m_tuple;
                auto ext_tuple_provider = determined_provider->m_external_type_description.m_tuple;

                if (ext_tuple_accepter->m_element_types.size()
                    != ext_tuple_provider->m_element_types.size())
                    return false;

                auto it_accepter = ext_tuple_accepter->m_element_types.begin();
                auto it_provider = ext_tuple_provider->m_element_types.begin();
                auto it_accepter_end = ext_tuple_accepter->m_element_types.end();

                for (; it_accepter != it_accepter_end; ++it_accepter, ++it_provider)
                {
                    if (!is_type_accepted(lex, node, *it_accepter, *it_provider))
                        return false;
                }

                break;
            }
            case lang_TypeInstance::DeterminedType::STRUCT:
            {
                auto ext_struct_accepter = determined_accepter->m_external_type_description.m_struct;
                auto ext_struct_provider = determined_provider->m_external_type_description.m_struct;

                if (ext_struct_accepter->m_member_types.size()
                    != ext_struct_provider->m_member_types.size())
                    return false;

                for (auto& [name, field] : ext_struct_accepter->m_member_types)
                {
                    auto fnd = ext_struct_provider->m_member_types.find(name);
                    if (fnd == ext_struct_provider->m_member_types.end())
                        return false;

                    if (field.m_offset != fnd->second.m_offset)
                        return false;

                    if (!is_type_accepted(lex, node, field.m_member_type, fnd->second.m_member_type))
                        return false;
                }
                break;
            }
            case lang_TypeInstance::DeterminedType::UNION:
            {
                auto ext_union_accepter = determined_accepter->m_external_type_description.m_union;
                auto ext_union_provider = determined_provider->m_external_type_description.m_union;

                if (ext_union_accepter->m_union_label.size()
                    != ext_union_provider->m_union_label.size())
                    return false;

                for (auto& [label, type] : ext_union_accepter->m_union_label)
                {
                    auto fnd = ext_union_provider->m_union_label.find(label);
                    if (fnd == ext_union_provider->m_union_label.end())
                        return false;

                    if (type.m_label != fnd->second.m_label)
                        return false;

                    if (type.m_item_type.has_value() != fnd->second.m_item_type.has_value())
                        return false;
                    if (type.m_item_type.has_value())
                    {
                        if (!is_type_accepted(
                            lex,
                            node,
                            type.m_item_type.value(),
                            fnd->second.m_item_type.value()))
                            return false;
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
                    return false;

                if (ext_func_accepter->m_param_types.size()
                    != ext_func_provider->m_param_types.size())
                    return false;

                auto it_accepter = ext_func_accepter->m_param_types.begin();
                auto it_provider = ext_func_provider->m_param_types.begin();
                auto it_accepter_end = ext_func_accepter->m_param_types.end();

                for (; it_accepter != it_accepter_end; ++it_accepter, ++it_provider)
                {
                    if (*it_accepter != *it_provider)
                        return false;
                }

                if (!is_type_accepted(
                    lex,
                    node,
                    ext_func_accepter->m_return_type,
                    ext_func_provider->m_return_type))
                    return false;

                break;
            }
            }

            return true;
            };

        bool result = judge();
        accepter->m_LANG_accepted_types.insert(std::make_pair(provider, result));

        return result;
    }

    bool LangContext::check_cast_able(
        lexer& lex,
        ast::AstBase* node,
        lang_TypeInstance* aimtype,
        lang_TypeInstance* srctype)
    {
        // TODO CHECK LIST:
        // 1. Make sure recursive-type will not cause infinite loop and have good habit.
        if (aimtype == srctype)
            return true;

        auto fnd = aimtype->m_LANG_castfrom_types.find(srctype);
        if (fnd != aimtype->m_LANG_castfrom_types.end())
            return fnd->second;

        auto judge =
            [&]()-> bool
            {
                // -1. If mutable not match, failed.
                if (srctype->is_mutable() != aimtype->is_mutable())
                    return false;

                // 0. If aimtype accept srctype, Ok
                if (is_type_accepted(lex, node, aimtype, srctype))
                    return true;

                // 1. If aimtype is void or dynamic, Ok
                if (aimtype == m_origin_types.m_void.m_type_instance
                    || aimtype == m_origin_types.m_dynamic.m_type_instance)
                    return true;

                // 2. If srctype is nothing, Ok
                if (srctype == m_origin_types.m_nothing.m_type_instance)
                    return true;

                // Following step needs determined base type.
                auto determined_srctype_may_null = srctype->get_determined_type();
                auto determined_aimtype_may_null = aimtype->get_determined_type();

                if (!determined_srctype_may_null)
                {
                    lex.lang_error(lexer::errorlevel::error, node,
                        WO_ERR_TYPE_NAMED_DETERMINED_FAILED,
                        get_type_name_w(srctype));
                    return false;
                }
                if (!determined_aimtype_may_null)
                {
                    lex.lang_error(lexer::errorlevel::error, node,
                        WO_ERR_TYPE_NAMED_DETERMINED_FAILED,
                        get_type_name_w(aimtype));
                    return false;
                }

                auto* determined_srctype = determined_srctype_may_null.value();
                auto* determined_aimtype = determined_aimtype_may_null.value();

                // 3. All the type can cast to string, besides void.
                if (aimtype == m_origin_types.m_string.m_type_instance &&
                    determined_srctype->m_base_type != lang_TypeInstance::DeterminedType::VOID)
                {
                    return true;
                }

                // 4. bool, handle, real, string can cast to int.
                if (aimtype == m_origin_types.m_int.m_type_instance &&
                    (determined_srctype->m_base_type == lang_TypeInstance::DeterminedType::BOOLEAN ||
                        determined_srctype->m_base_type == lang_TypeInstance::DeterminedType::HANDLE ||
                        determined_srctype->m_base_type == lang_TypeInstance::DeterminedType::REAL ||
                        determined_srctype->m_base_type == lang_TypeInstance::DeterminedType::STRING))
                {
                    return true;
                }

                // 5. bool, int, handle, string can cast to real.
                if (aimtype == m_origin_types.m_real.m_type_instance &&
                    (determined_srctype->m_base_type == lang_TypeInstance::DeterminedType::BOOLEAN ||
                        determined_srctype->m_base_type == lang_TypeInstance::DeterminedType::INTEGER ||
                        determined_srctype->m_base_type == lang_TypeInstance::DeterminedType::HANDLE ||
                        determined_srctype->m_base_type == lang_TypeInstance::DeterminedType::STRING))
                {
                    return true;
                }

                // 6. bool, int, gchandle, real, string can cast to handle.
                if (aimtype == m_origin_types.m_handle.m_type_instance &&
                    (determined_srctype->m_base_type == lang_TypeInstance::DeterminedType::BOOLEAN ||
                        determined_srctype->m_base_type == lang_TypeInstance::DeterminedType::INTEGER ||
                        determined_srctype->m_base_type == lang_TypeInstance::DeterminedType::GCHANDLE ||
                        determined_srctype->m_base_type == lang_TypeInstance::DeterminedType::REAL ||
                        determined_srctype->m_base_type == lang_TypeInstance::DeterminedType::STRING))
                {
                    return true;
                }

                // 7. integer, handle, real, string can cast to bool.
                if (aimtype == m_origin_types.m_bool.m_type_instance &&
                    (determined_srctype->m_base_type == lang_TypeInstance::DeterminedType::INTEGER ||
                        determined_srctype->m_base_type == lang_TypeInstance::DeterminedType::HANDLE ||
                        determined_srctype->m_base_type == lang_TypeInstance::DeterminedType::REAL ||
                        determined_srctype->m_base_type == lang_TypeInstance::DeterminedType::STRING))
                {
                    return true;
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
                    if ((aimtype == (m_origin_types.*instance_member).m_type_instance &&
                        determined_srctype->m_base_type == base_type)
                        || (srctype == (m_origin_types.*instance_member).m_type_instance &&
                            determined_aimtype->m_base_type == base_type))
                    {
                        return true;
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

                        if (is_type_accepted(lex, node, ext_dict_aimtype->m_key_type, ext_dict_srctype->m_key_type)
                            && is_type_accepted(lex, node, ext_dict_aimtype->m_value_type, ext_dict_srctype->m_value_type))
                        {
                            return true;
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

                        if (is_type_accepted(
                            lex,
                            node,
                            ext_array_aimtype->m_element_type,
                            ext_array_srctype->m_element_type))
                        {
                            return true;
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
                                if (!is_type_accepted(lex, node, *it_aimtype, *it_srctype))
                                {
                                    accpet = false;
                                    break;
                                }
                            }

                            if (accpet)
                                return true;
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
                                    || !is_type_accepted(lex, node, field.m_member_type, fnd->second.m_member_type))
                                {
                                    accpet = false;
                                    break;
                                }
                            }
                            if (accpet)
                                return true;
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

                            if (accept && is_type_accepted(
                                lex,
                                node,
                                ext_func_aimtype->m_return_type,
                                ext_func_srctype->m_return_type))
                                return true;
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
                if (immutable_type(srctype) == m_origin_types.m_dynamic.m_type_instance)
                {
                    auto* immutable_aimtype = immutable_type(aimtype);
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
                        return true;
                    }
                }

                // All missmatch, failed.
                return false;
            };

        bool result = judge();
        aimtype->m_LANG_castfrom_types.insert(std::make_pair(srctype, result));

        return result;
    }
#endif
}