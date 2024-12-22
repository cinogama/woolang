#include "wo_lang.hpp"

namespace wo
{
#ifndef WO_DISABLE_COMPILER
    lang_Symbol::~lang_Symbol()
    {
        if (m_is_template)
        {
            if (m_symbol_kind == VARIABLE)
                delete m_template_value_instances;
            else
                delete m_template_type_instances;
        }
        else
        {
            if (m_symbol_kind == VARIABLE)
                delete m_value_instance;
            else
                delete m_type_instance;
        }
    }
    lang_Symbol::lang_Symbol(kind kind)
        : m_symbol_kind(kind)
        , m_is_template(false)
    {
        switch (kind)
        {
        case VARIABLE:
            m_value_instance = new lang_ValueInstance();
            break;
        case TYPE:
            m_type_instance = new lang_TypeInstance();
            break;
        }
    }
    lang_Symbol::lang_Symbol(ast::AstValueBase* template_value_base, const std::list<wo_pstring_t>& template_params)
        : m_symbol_kind(VARIABLE)
        , m_is_template(true)
    {
        m_template_value_instances = new TemplateValuePrefab(template_value_base, template_params);
        m_template_value_instances->m_origin_value_ast = template_value_base;
        m_template_value_instances->m_template_params = template_params;
    }
    lang_Symbol::lang_Symbol(ast::AstTypeHolder* template_type_base, const std::list<wo_pstring_t>& template_params)
        : m_symbol_kind(TYPE)
        , m_is_template(true)
    {
        m_template_type_instances = new TemplateTypeOrAliasPrefab(template_type_base, template_params);
        m_template_type_instances->m_origin_value_ast = template_type_base;
        m_template_type_instances->m_template_params = template_params;
    }

    //////////////////////////////////////

    lang_Symbol::TemplateValuePrefab::TemplateValuePrefab(ast::AstValueBase* ast, const std::list<wo_pstring_t>& template_params)
        : m_origin_value_ast(ast)
        , m_template_params(template_params)
    {
    }
    lang_Symbol::TemplateTypeOrAliasPrefab::TemplateTypeOrAliasPrefab(ast::AstTypeHolder* ast, const std::list<wo_pstring_t>& template_params)
        : m_origin_value_ast(ast)
        , m_template_params(template_params)
    {
    }

    //////////////////////////////////////

    lang_TypeInstance::lang_TypeInstance()
    {
    }

    //////////////////////////////////////

    lang_ValueInstance::lang_ValueInstance()
    {
    }

    //////////////////////////////////////

    lang_TypeInstance::DeterminedType::DeterminedType(base_type type, const ExternalTypeDescription& desc)
        : m_base_type(type)
        , m_external_type_description(desc)
    {
    }
    lang_TypeInstance::DeterminedType::~DeterminedType()
    {
        switch (m_base_type)
        {
        case base_type::ARRAY:
        case base_type::VECTOR:
            delete m_external_type_description.m_array_or_vector;
            break;
        case base_type::DICTIONARY:
        case base_type::MAPPING:
            delete m_external_type_description.m_dictionary_or_mapping;
            break;
        case base_type::TUPLE:
            delete m_external_type_description.m_tuple;
            break;
        case base_type::FUNCTION:
            delete m_external_type_description.m_function;
            break;
        case base_type::STRUCT:
            delete m_external_type_description.m_struct;
            break;
        }
    }

    //////////////////////////////////////

    lang_Scope::lang_Scope(lang_Namespace* belongs)
        : m_belongs_to_namespace(belongs)
    {
    }

    //////////////////////////////////////

    lang_Namespace::lang_Namespace()
    {
        m_this_scope = std::make_unique<lang_Scope>(this);
    }

    //////////////////////////////////////

    LangContext::LangContext()
        : m_root_namespace(std::make_unique<lang_Namespace>())
    {

    }

#endif
}
