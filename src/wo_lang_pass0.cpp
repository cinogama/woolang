#include "wo_lang.hpp"

namespace wo
{
#ifndef WO_DISABLE_COMPILER
    using namespace ast;

    bool LangContext::declare_pattern_symbol_pass0_1(
        lexer& lex,
        const std::optional<ast::AstDeclareAttribue*>& decl_attrib,
        ast::AstPatternBase* pattern,
        const std::optional<ast::AstValueBase*>& init_value)
    {
        switch (pattern->node_type)
        {
        case ast::AstBase::AST_PATTERN_SINGLE:
        {
            ast::AstPatternSingle* single_pattern = static_cast<ast::AstPatternSingle*>(pattern);
            // Check has been declared?
            if (!single_pattern->m_LANG_declared_symbol)
            {
                if (single_pattern->m_template_parameters)
                {
                    wo_assert(init_value);
                    single_pattern->m_LANG_declared_symbol = define_symbol_in_current_scope(
                        single_pattern->m_name,
                        decl_attrib,
                        pattern->source_location.source_file,
                        get_current_scope(),
                        init_value.value(),
                        single_pattern->m_template_parameters.value(),
                        single_pattern->m_is_mutable);
                }
                else
                {
                    single_pattern->m_LANG_declared_symbol = define_symbol_in_current_scope(
                        single_pattern->m_name,
                        decl_attrib,
                        pattern->source_location.source_file,
                        get_current_scope(),
                        lang_Symbol::kind::VARIABLE,
                        single_pattern->m_is_mutable);
                }

                if (!single_pattern->m_LANG_declared_symbol)
                {
                    lex.lang_error(lexer::errorlevel::error, single_pattern,
                        WO_ERR_REDEFINED,
                        single_pattern->m_name->c_str());

                    return false;
                }
            }
            return true;
            break;
        }
        case ast::AstBase::AST_PATTERN_TUPLE:
        {
            ast::AstPatternTuple* tuple_pattern = static_cast<ast::AstPatternTuple*>(pattern);
            bool success = true;
            for (auto& sub_pattern : tuple_pattern->m_fields)
                success = success && declare_pattern_symbol_pass0_1(
                    lex,
                    decl_attrib,
                    sub_pattern,
                    std::nullopt);

            return success;
            break;
        }
        case ast::AstBase::AST_PATTERN_UNION:
        {
            ast::AstPatternUnion* union_pattern = static_cast<ast::AstPatternUnion*>(pattern);
            bool success = true;
            if (union_pattern->m_field)
                success = declare_pattern_symbol_pass0_1(
                    lex,
                    decl_attrib,
                    union_pattern->m_field.value(),
                    std::nullopt);

            return success;
            break;
        }
        case ast::AstBase::AST_PATTERN_TAKEPLACE:
        {
            // Nothing todo.
            break;
        }
        default:
            wo_error("Unexpected pattern type.");
        }
        return false;
    }

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
            wo_assert(!node->m_LANG_determined_scope);

            begin_new_scope();
            node->m_LANG_determined_scope = get_current_scope();

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
            wo_assert(!node->m_LANG_determined_namespace);

            if (!begin_new_namespace(node->m_name))
            {
                lex.lang_error(lexer::errorlevel::error, node, WO_ERR_CANNOT_START_NAMESPACE);
                return FAILED;
            }
            node->m_LANG_determined_namespace = get_current_namespace();
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
            success = success && declare_pattern_symbol_pass0_1(
                lex,
                node->m_attribute,
                defines->m_pattern, 
                defines->m_init_value);
        
        return success ? OKAY : FAILED;
    }
    WO_PASS_PROCESSER(AstAliasTypeDeclare)
    {
        wo_assert(!node->m_LANG_declared_symbol);
        if (node->m_template_parameters)
            node->m_LANG_declared_symbol = define_symbol_in_current_scope(
                node->m_typename, 
                node->m_attribute,
                node->source_location.source_file,
                get_current_scope(), 
                node->m_type, 
                node->m_template_parameters.value(), 
                true);
        else
            node->m_LANG_declared_symbol = define_symbol_in_current_scope(
                node->m_typename, 
                node->m_attribute,
                node->source_location.source_file,
                get_current_scope(),
                lang_Symbol::kind::ALIAS,
                false);

        if (!node->m_LANG_declared_symbol)
        {
            lex.lang_error(lexer::errorlevel::error, node, WO_ERR_REDEFINED, node->m_typename->c_str());
            return FAILED;
        }

        return OKAY;
    }
    WO_PASS_PROCESSER(AstUsingTypeDeclare)
    {
        wo_assert(!node->m_LANG_declared_symbol);
        if (state == UNPROCESSED)
        {
            if (node->m_template_parameters)
                node->m_LANG_declared_symbol = define_symbol_in_current_scope(
                    node->m_typename, 
                    node->m_attribute,
                    node->source_location.source_file,
                    get_current_scope(), 
                    node->m_type, 
                    node->m_template_parameters.value(),
                    false);
            else
                node->m_LANG_declared_symbol = define_symbol_in_current_scope(
                    node->m_typename, 
                    node->m_attribute,
                    node->source_location.source_file,
                    get_current_scope(),
                    lang_Symbol::kind::TYPE,
                    false);

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