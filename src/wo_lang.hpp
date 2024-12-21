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
    struct lang_TypeInstance
    {
        lang_TypeInstance(const lang_TypeInstance&) = delete;
        lang_TypeInstance(lang_TypeInstance&&) = delete;
        lang_TypeInstance& operator=(const lang_TypeInstance&) = delete;
        lang_TypeInstance& operator=(lang_TypeInstance&&) = delete;
    };
    struct lang_ValueInstance
    {
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
            ast::AstValueBase*  m_origin_value_ast;
            std::map<TemplateArgumentSetT, std::unique_ptr<lang_ValueInstance*>>
                                m_template_instances;
        };
        struct TemplateTypeOrAliasPrefab
        {
            ast::AstValueBase*  m_origin_value_ast;
            std::map<TemplateArgumentSetT, std::unique_ptr<lang_ValueInstance*>>
                                m_template_instances;
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
    };

    struct lang_Scope
    {
        std::unordered_map<wo_pstring_t, std::unique_ptr<lang_Symbol>> m_defined_symbols;

        lang_Scope(const lang_Scope&) = delete;
        lang_Scope(lang_Scope&&) = delete;
        lang_Scope& operator=(const lang_Scope&) = delete;
        lang_Scope& operator=(lang_Scope&&) = delete;
    };

    struct LangContext
    {
        
    };
}