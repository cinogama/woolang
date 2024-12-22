#include "wo_lang.hpp"

namespace wo
{
#ifndef WO_DISABLE_COMPILER
    using namespace ast;

    void LangContext::init_pass0()
    {
        WO_LANG_REGISTER_PROCESSER(AstList, AstBase::AST_LIST, pass0);
        WO_LANG_REGISTER_PROCESSER(AstScope, AstBase::AST_SCOPE, pass0);
        WO_LANG_REGISTER_PROCESSER(AstNamespace, AstBase::AST_NAMESPACE, pass0);
        WO_LANG_REGISTER_PROCESSER(AstVariableDefines, AstBase::AST_VARIABLE_DEFINES, pass0);
        WO_LANG_REGISTER_PROCESSER(AstAliasTypeDeclare, AstBase::AST_ALIAS_TYPE_DECLARE, pass0);
        WO_LANG_REGISTER_PROCESSER(AstUsingTypeDeclare, AstBase::AST_USING_TYPE_DECLARE, pass0);
        WO_LANG_REGISTER_PROCESSER(AstEnumDeclare, AstBase::AST_ENUM_DECLARE, pass0);
        WO_LANG_REGISTER_PROCESSER(AstUnionDeclare, AstBase::AST_UNION_DECLARE, pass0);
    }

#define WO_PASS_PROCESSER(AST) WO_PASS_PROCESSER_IMPL(AST, pass0)

    WO_PASS_PROCESSER(AstList)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS_LIST(node->m_list);

            return HOLD;
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstScope)
    {
        if (state == UNPROCESSED)
        {
            begin_new_scope();

            WO_CONTINUE_PROCESS(node->m_body);

            return HOLD;
        }
        end_last_scope();
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstNamespace)
    {
        if (state == UNPROCESSED)
        {
            if (!begin_new_namespace(node->m_name))
            {
                lex.lang_error(lexer::errorlevel::error, node, WO_ERR_CANNOT_START_NAMESPACE);
                return FAILED;
            }

            WO_CONTINUE_PROCESS(node->m_body);

            return HOLD;
        }
        end_last_scope();
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstVariableDefines)
    {
        wo_assert(state == UNPROCESSED);
        
        bool success = true;
        for (auto& defines : node->m_definitions)
            success = success && declare_pattern_symbol(lex, defines->m_pattern, defines->m_init_value);
        
        return success ? OKAY : FAILED;
    }
    WO_PASS_PROCESSER(AstAliasTypeDeclare)
    {
        if (node->m_template_parameters)
            node->m_LANG_declared_symbol = define_symbol_in_current_scope(
                node->m_typename, get_current_scope(), node->m_type, node->m_template_parameters.value(), true);
        else
            node->m_LANG_declared_symbol = define_symbol_in_current_scope(
                node->m_typename, lang_Symbol::kind::ALIAS);

        if (!node->m_LANG_declared_symbol)
        {
            lex.lang_error(lexer::errorlevel::error, node, WO_ERR_REDEFINED, node->m_typename->c_str());
            return FAILED;
        }

        return OKAY;
    }
    WO_PASS_PROCESSER(AstUsingTypeDeclare)
    {
        if (state == UNPROCESSED)
        {
            if (node->m_template_parameters)
                node->m_LANG_declared_symbol = define_symbol_in_current_scope(
                    node->m_typename, get_current_scope(), node->m_type, node->m_template_parameters.value(), false);
            else
                node->m_LANG_declared_symbol = define_symbol_in_current_scope(
                    node->m_typename, lang_Symbol::kind::TYPE);

            if (node->m_in_type_namespace)
            {
                WO_CONTINUE_PROCESS(node->m_in_type_namespace.value());
                return HOLD;
            }
        }
        
        if (!node->m_LANG_declared_symbol)
        {
            lex.lang_error(lexer::errorlevel::error, node, WO_ERR_REDEFINED, node->m_typename->c_str());
            return FAILED;
        }

        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstEnumDeclare)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_enum_body);
            WO_CONTINUE_PROCESS(node->m_enum_type_declare);
            return HOLD;
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstUnionDeclare)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_union_type_declare);
            return HOLD;
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }

#undef WO_PASS_PROCESSER
#endif
}