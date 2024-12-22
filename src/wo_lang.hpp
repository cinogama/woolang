#pragma once

#include "wo_basic_type.hpp"
#include "wo_lang_ast.hpp"
#include "wo_compiler_ir.hpp"

#include <memory>
#include <unordered_map>
#include <set>
#include <variant>

namespace wo
{
    struct lang_Namespace;
    struct lang_Scope;

    struct lang_TypeInstance
    {
        struct DeterminedType
        {
            enum base_type
            {
                INTEGER,
                REAL,
                HANDLE,
                BOOLEAN,
                STRING,
                DICTIONARY,
                MAPPING,
                ARRAY,
                VECTOR,
                TUPLE,
                FUNCTION,
                STRUCT,
            };
            struct ArrayOrVector
            {
                lang_TypeInstance*              m_element_type;
            };
            struct DictionaryOrMapping
            {
                lang_TypeInstance*              m_key_type;
                lang_TypeInstance*              m_value_type;
            };
            struct Tuple
            {
                std::vector<lang_TypeInstance*> m_element_types;
            };
            struct Struct
            {
                std::unordered_map<wo_pstring_t, lang_TypeInstance*> 
                                                m_member_types;
            };
            struct Function
            {
                bool                            m_is_variadic;
                std::vector<lang_TypeInstance*> m_param_types;
                lang_TypeInstance*              m_return_type;
            };

            union ExternalTypeDescription
            {
                ArrayOrVector* m_array_or_vector;
                DictionaryOrMapping* m_dictionary_or_mapping;
                Tuple* m_tuple;
                Struct* m_struct;
                Function* m_function;
            };

            base_type m_base_type;
            ExternalTypeDescription m_external_type_description;

            DeterminedType(base_type type, const ExternalTypeDescription& desc);
            ~DeterminedType();

            DeterminedType(const DeterminedType&) = delete;
            DeterminedType(DeterminedType&&) = delete;
            DeterminedType& operator=(const DeterminedType&) = delete;
            DeterminedType& operator=(DeterminedType&&) = delete;
        };

        std::optional<DeterminedType> m_determined_type;

        lang_TypeInstance();
        lang_TypeInstance(const lang_TypeInstance&) = delete;
        lang_TypeInstance(lang_TypeInstance&&) = delete;
        lang_TypeInstance& operator=(const lang_TypeInstance&) = delete;
        lang_TypeInstance& operator=(lang_TypeInstance&&) = delete;
    };
    struct lang_ValueInstance
    {
        std::optional<ast::AstValueBase*> m_determined_constant;
        std::optional<lang_TypeInstance*> m_determined_type;

        lang_ValueInstance();
        lang_ValueInstance(const lang_ValueInstance&) = delete;
        lang_ValueInstance(lang_ValueInstance&&) = delete;
        lang_ValueInstance& operator=(const lang_ValueInstance&) = delete;
        lang_ValueInstance& operator=(lang_ValueInstance&&) = delete;
    };
    struct lang_Symbol
    {
        enum kind
        {
            VARIABLE,
            TYPE,
        };

        using TemplateArgumentSetT = std::set<lang_TypeInstance*>;

        struct TemplateValuePrefab
        {
            std::list<wo_pstring_t> m_template_params;
            ast::AstValueBase*  m_origin_value_ast;
            std::map<TemplateArgumentSetT, std::unique_ptr<lang_ValueInstance*>>
                                m_template_instances;

            TemplateValuePrefab(ast::AstValueBase* ast, const std::list<wo_pstring_t>& template_params);

            TemplateValuePrefab(const TemplateValuePrefab&) = delete;
            TemplateValuePrefab(TemplateValuePrefab&&) = delete;
            TemplateValuePrefab& operator=(const TemplateValuePrefab&) = delete;
            TemplateValuePrefab& operator=(TemplateValuePrefab&&) = delete;

        };
        struct TemplateTypeOrAliasPrefab
        {
            std::list<wo_pstring_t> m_template_params;
            ast::AstTypeHolder* m_origin_value_ast;
            std::map<TemplateArgumentSetT, std::unique_ptr<lang_ValueInstance*>>
                                m_template_instances;

            TemplateTypeOrAliasPrefab(ast::AstTypeHolder* ast, const std::list<wo_pstring_t>& template_params);

            TemplateTypeOrAliasPrefab(const TemplateTypeOrAliasPrefab&) = delete;
            TemplateTypeOrAliasPrefab(TemplateTypeOrAliasPrefab&&) = delete;
            TemplateTypeOrAliasPrefab& operator=(const TemplateTypeOrAliasPrefab&) = delete;
            TemplateTypeOrAliasPrefab& operator=(TemplateTypeOrAliasPrefab&&) = delete;
        };

        kind                            m_symbol_kind;
        bool                            m_is_template;
        union
        {
            TemplateTypeOrAliasPrefab*  m_template_type_instances;
            TemplateValuePrefab*        m_template_value_instances;
            lang_TypeInstance*          m_type_instance;
            lang_ValueInstance*         m_value_instance;
        };

        lang_Symbol(const lang_Symbol&) = delete;
        lang_Symbol(lang_Symbol&&) = delete;
        lang_Symbol& operator=(const lang_Symbol&) = delete;
        lang_Symbol& operator=(lang_Symbol&&) = delete;

        ~lang_Symbol();

        lang_Symbol(kind kind);
        lang_Symbol(ast::AstValueBase* template_value_base, const std::list<wo_pstring_t>& template_params);
        lang_Symbol(ast::AstTypeHolder* template_type_base, const std::list<wo_pstring_t>& template_params);
    };


    struct lang_Scope
    {
        std::unordered_map<wo_pstring_t, std::unique_ptr<lang_Symbol>> 
                        m_defined_symbols;
        std::list<std::unique_ptr<lang_Scope>>  
                        m_sub_scopes;

        lang_Namespace* m_belongs_to_namespace;

        lang_Scope(lang_Namespace* belongs);

        lang_Scope(const lang_Scope&) = delete;
        lang_Scope(lang_Scope&&) = delete;
        lang_Scope& operator=(const lang_Scope&) = delete;
        lang_Scope& operator=(lang_Scope&&) = delete;
    };
    struct lang_Namespace
    {
        std::unordered_map<wo_pstring_t, std::unique_ptr<lang_Namespace>> 
                                                m_sub_namespaces;
        std::unique_ptr<lang_Scope>             m_this_scope;

        lang_Namespace();

        lang_Namespace(const lang_Scope&) = delete;
        lang_Namespace(lang_Scope&&) = delete;
        lang_Namespace& operator=(const lang_Scope&) = delete;
        lang_Namespace& operator=(lang_Scope&&) = delete;
    };

    struct LangContext
    {
        std::unique_ptr<lang_Namespace> m_root_namespace;

        LangContext();
    };
}