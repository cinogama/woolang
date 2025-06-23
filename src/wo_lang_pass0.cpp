#include "wo_afx.hpp"

namespace wo
{
#ifndef WO_DISABLE_COMPILER
    using namespace ast;

    bool LangContext::declare_pattern_symbol_pass0_1(
        lexer& lex,
        bool is_pass0,
        const std::optional<AstDeclareAttribue*>& attribute,
        const std::optional<AstBase*>& var_defines,
        ast::AstPatternBase* pattern,
        const std::optional<ast::AstValueBase*>& init_value_only_used_for_template_or_function)
    {
        switch (pattern->node_type)
        {
        case ast::AstBase::AST_PATTERN_SINGLE:
        {
            ast::AstPatternSingle* single_pattern = static_cast<ast::AstPatternSingle*>(pattern);
            // Check has been declared?
            if (!single_pattern->m_LANG_declared_symbol)
            {
                bool symbol_defined_success = false;
                lang_Symbol* defined_symbol = nullptr;

                if (single_pattern->m_template_parameters)
                {
                    symbol_defined_success = define_symbol_in_current_scope(
                        &defined_symbol,
                        single_pattern->m_name,
                        attribute,
                        var_defines,
                        pattern->source_location,
                        get_current_scope(),
                        init_value_only_used_for_template_or_function.value(),
                        single_pattern->m_template_parameters.value(),
                        single_pattern->m_is_mutable);
                }
                else
                {
                    symbol_defined_success = define_symbol_in_current_scope(
                        &defined_symbol,
                        single_pattern->m_name,
                        attribute,
                        var_defines,
                        pattern->source_location,
                        get_current_scope(),
                        lang_Symbol::kind::VARIABLE,
                        single_pattern->m_is_mutable);
                }

                if (symbol_defined_success)
                {
                    wo_assert(defined_symbol != nullptr);
                    single_pattern->m_LANG_declared_symbol = defined_symbol;

                    if (!defined_symbol->m_value_instance->m_mutable
                        && !defined_symbol->m_is_template
                        && init_value_only_used_for_template_or_function.has_value())
                    {
                        AstValueBase* init_value = init_value_only_used_for_template_or_function.value();
                        if (init_value->node_type == AstBase::AST_VALUE_FUNCTION)
                        {
                            AstValueFunction* func = static_cast<AstValueFunction*>(init_value);
                            defined_symbol->m_value_instance->try_determine_function(func);
                        }
                    }

                    if (is_pass0)
                        defined_symbol->m_is_global = true;
                }
                else
                {
                    lex.record_lang_error(lexer::msglevel_t::error, single_pattern,
                        WO_ERR_REDEFINED,
                        single_pattern->m_name->c_str());
                    
                    if (defined_symbol->m_symbol_declare_ast.has_value())
                        lex.record_lang_error(lexer::msglevel_t::infom,
                            defined_symbol->m_symbol_declare_ast.value(),
                            WO_INFO_SYMBOL_NAMED_DEFINED_HERE,
                            get_symbol_name_w(defined_symbol));

                    return false;
                }
            }
            return true;
        }
        case ast::AstBase::AST_PATTERN_TUPLE:
        {
            ast::AstPatternTuple* tuple_pattern = static_cast<ast::AstPatternTuple*>(pattern);
            bool success = true;
            for (auto& sub_pattern : tuple_pattern->m_fields)
                success = success && declare_pattern_symbol_pass0_1(
                    lex,
                    is_pass0,
                    attribute,
                    var_defines,
                    sub_pattern,
                    std::nullopt);

            return success;
        }
        case ast::AstBase::AST_PATTERN_UNION:
        {
            ast::AstPatternUnion* union_pattern = static_cast<ast::AstPatternUnion*>(pattern);
            bool success = true;
            if (union_pattern->m_field)
                success = declare_pattern_symbol_pass0_1(
                    lex,
                    is_pass0,
                    attribute,
                    var_defines,
                    union_pattern->m_field.value(),
                    std::nullopt);

            return success;
        }
        case ast::AstBase::AST_PATTERN_TAKEPLACE:
        {
            // Nothing todo.
            return true;
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
        WO_LANG_REGISTER_PROCESSER(AstVariableDefineItem, AstBase::AST_VARIABLE_DEFINE_ITEM, pass0);
        WO_LANG_REGISTER_PROCESSER(AstAliasTypeDeclare, AstBase::AST_ALIAS_TYPE_DECLARE, pass0);
        WO_LANG_REGISTER_PROCESSER(AstUsingTypeDeclare, AstBase::AST_USING_TYPE_DECLARE, pass0);
        WO_LANG_REGISTER_PROCESSER(AstEnumDeclare, AstBase::AST_ENUM_DECLARE, pass0);
        WO_LANG_REGISTER_PROCESSER(AstUnionDeclare, AstBase::AST_UNION_DECLARE, pass0);
        WO_LANG_REGISTER_PROCESSER(AstUsingNamespace, AstBase::AST_USING_NAMESPACE, pass0);
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
    WO_PASS_PROCESSER(AstUsingNamespace)
    {
        // ATTENTION: Some global symbol will be advanced the processing 
        // of declaration nodes. In this case, used namespace must be 
        // declared here to make sure the processing can find the symbol 
        // correctly.
        wo_assert(state == UNPROCESSED);
        using_namespace_declare_for_current_scope(node);

        return OKAY;
    }
    WO_PASS_PROCESSER(AstScope)
    {
        if (state == UNPROCESSED)
        {
            wo_assert(!node->m_LANG_determined_scope);

            begin_new_scope(node->source_location);
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
                lex.record_lang_error(lexer::msglevel_t::error, node, WO_ERR_CANNOT_START_NAMESPACE);
                return FAILED;
            }
            node->m_LANG_determined_namespace = get_current_namespace();
            WO_CONTINUE_PROCESS(node->m_body);

            return HOLD;
        }
        end_last_scope();
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstVariableDefineItem)
    {
        wo_assert(state == UNPROCESSED);

        if (!declare_pattern_symbol_pass0_1(
            lex,
            true,
            node->m_LANG_declare_attribute,
            node,
            node->m_pattern,
            node->m_init_value))
            return FAILED;

        return OKAY;
    }
    WO_PASS_PROCESSER(AstVariableDefines)
    {
        if (state == UNPROCESSED)
        {
            for (auto& defines : node->m_definitions)
                defines->m_LANG_declare_attribute = node->m_attribute;

            WO_CONTINUE_PROCESS_LIST(node->m_definitions);
            return HOLD;
        }

        return WO_EXCEPT_ERROR(state, OKAY);
        /*   success = success &&

       return success ? OKAY : FAILED;*/
    }
    WO_PASS_PROCESSER(AstAliasTypeDeclare)
    {
        wo_assert(!node->m_LANG_declared_symbol);

        bool symbol_defined_success = false;
        lang_Symbol* defined_symbol = nullptr;

        if (node->m_template_parameters)
            symbol_defined_success = define_symbol_in_current_scope(
                &defined_symbol,
                node->m_typename,
                node->m_attribute,
                node,
                node->source_location,
                get_current_scope(),
                node->m_type,
                node->m_template_parameters.value(),
                true);
        else
            symbol_defined_success = define_symbol_in_current_scope(
                &defined_symbol,
                node->m_typename,
                node->m_attribute,
                node,
                node->source_location,
                get_current_scope(),
                lang_Symbol::kind::ALIAS,
                false);

        if (symbol_defined_success)
        {
            wo_assert(defined_symbol != nullptr);
            node->m_LANG_declared_symbol = defined_symbol;

            defined_symbol->m_is_global = true;
        }
        else
        {
            lex.record_lang_error(lexer::msglevel_t::error, node, WO_ERR_REDEFINED, node->m_typename->c_str());
            if (defined_symbol->m_symbol_declare_ast.has_value())
                lex.record_lang_error(lexer::msglevel_t::infom,
                    defined_symbol->m_symbol_declare_ast.value(),
                    WO_INFO_SYMBOL_NAMED_DEFINED_HERE,
                    get_symbol_name_w(defined_symbol));

            return FAILED;
        }
        return OKAY;
    }
    WO_PASS_PROCESSER(AstUsingTypeDeclare)
    {
        wo_assert(state == UNPROCESSED);
        wo_assert(!node->m_LANG_declared_symbol);

        bool symbol_defined_success = false;
        lang_Symbol* defined_symbol = nullptr;

        if (node->m_template_parameters)
            symbol_defined_success = define_symbol_in_current_scope(
                &defined_symbol,
                node->m_typename,
                node->m_attribute,
                node,
                node->source_location,
                get_current_scope(),
                node->m_type,
                node->m_template_parameters.value(),
                false);
        else
            symbol_defined_success = define_symbol_in_current_scope(
                &defined_symbol,
                node->m_typename,
                node->m_attribute,
                node,
                node->source_location,
                get_current_scope(),
                lang_Symbol::kind::TYPE,
                false);

        if (symbol_defined_success)
        {
            wo_assert(defined_symbol != nullptr);
            node->m_LANG_declared_symbol = defined_symbol;

            defined_symbol->m_is_global = true;
        }
        else
        {
            lex.record_lang_error(lexer::msglevel_t::error, node, WO_ERR_REDEFINED, node->m_typename->c_str());
            if (defined_symbol->m_symbol_declare_ast.has_value())
                lex.record_lang_error(lexer::msglevel_t::infom,
                    defined_symbol->m_symbol_declare_ast.value(),
                    WO_INFO_SYMBOL_NAMED_DEFINED_HERE,
                    get_symbol_name_w(defined_symbol));

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
            if (node->m_union_namespace.has_value())
                WO_CONTINUE_PROCESS(node->m_union_namespace.value());

            WO_CONTINUE_PROCESS(node->m_union_type_declare);
            return HOLD;
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }

#undef WO_PASS_PROCESSER

    LangContext::pass_behavior LangContext::pass_0_process_scope_and_non_local_defination(
        lexer& lex, const AstNodeWithState& node_state, PassProcessStackT& out_stack)
    {
        return m_pass0_processers->process_node(this, lex, node_state, out_stack);
    }

#endif
}