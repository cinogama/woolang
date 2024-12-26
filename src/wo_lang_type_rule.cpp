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
        if (accepter == provider)
            return true;

        auto fnd = accepter->m_LANG_accepted_types.find(accepter);
        if (fnd != accepter->m_LANG_accepted_types.end())
            return fnd->second;

        auto judge =
            [&]() {

            if (accepter->is_mutable() != provider->is_mutable())
                return false;

            lang_TypeInstance* immutable_accepter = immutable_type(accepter);
            lang_TypeInstance* immutable_provider = immutable_type(provider);

            // If provider is nothing, OK.
            if (immutable_provider == m_origin_types.m_nothing.m_type_instance)
                return true;

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

                if (ext_func_accepter->m_return_type != ext_func_provider->m_return_type)
                    return false;

                auto it_accepter = ext_func_accepter->m_param_types.begin();
                auto it_provider = ext_func_provider->m_param_types.begin();
                auto it_accepter_end = ext_func_accepter->m_param_types.end();

                for (; it_accepter != it_accepter_end; ++it_accepter, ++it_provider)
                {
                    if (*it_accepter != *it_provider)
                        return false;
                }
                break;
            }
            }

            return true;
            };

        bool result = judge();
        accepter->m_LANG_accepted_types.insert(std::make_pair(provider, result));

        return result;
    }
#endif
}