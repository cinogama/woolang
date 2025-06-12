#include "wo_lang.hpp"
#include <cmath>

WO_API wo_api rslib_std_bad_function(wo_vm vm, wo_value args);

namespace wo
{
#ifndef WO_DISABLE_COMPILER
    using namespace ast;

    bool LangContext::update_pattern_symbol_variable_type_pass1(
        lexer& lex,
        ast::AstPatternBase* pattern,
        const std::optional<AstValueBase*>& init_value,
        const std::optional<lang_TypeInstance*>& init_value_type)
    {
        switch (pattern->node_type)
        {
        case AstBase::AST_PATTERN_SINGLE:
        {
            AstPatternSingle* single_pattern = static_cast<AstPatternSingle*>(pattern);
            if (!single_pattern->m_template_parameters)
            {
                wo_assert(single_pattern->m_LANG_declared_symbol);

                auto* lang_symbol = single_pattern->m_LANG_declared_symbol.value();
                wo_assert(lang_symbol->m_symbol_kind == lang_Symbol::kind::VARIABLE);

                // NOTE: Donot decide constant value for mutable variable.
                lang_symbol->m_value_instance->m_determined_type = init_value_type.value();
                if (!lang_symbol->m_value_instance->m_mutable
                    && init_value.has_value())
                {
                    lang_symbol->m_value_instance->try_determine_const_value(
                        init_value.value());
                }
            }
            return true;
        }
        case AstBase::AST_PATTERN_TUPLE:
        {
            AstPatternTuple* tuple_pattern = static_cast<AstPatternTuple*>(pattern);

            auto determined_type_may_nullopt = init_value_type.value()->get_determined_type();
            if (!determined_type_may_nullopt.has_value())
            {
                lex.record_lang_error(lexer::msglevel_t::error, pattern,
                    WO_ERR_TYPE_DETERMINED_FAILED);

                return false;
            }
            auto* determined_type = determined_type_may_nullopt.value();

            if (determined_type->m_base_type == lang_TypeInstance::DeterminedType::TUPLE)
            {
                if (determined_type->m_external_type_description.m_tuple->m_element_types.size()
                    == tuple_pattern->m_fields.size())
                {
                    auto type_iter = determined_type->m_external_type_description.m_tuple->m_element_types.begin();
                    auto pattern_iter = tuple_pattern->m_fields.begin();
                    auto pattern_end = tuple_pattern->m_fields.end();

                    bool success = true;

                    for (; pattern_iter != pattern_end; ++pattern_iter, ++type_iter)
                        success = success && update_pattern_symbol_variable_type_pass1(
                            lex, *pattern_iter, std::nullopt, *type_iter);

                    return success;
                }
                else
                {
                    lex.record_lang_error(lexer::msglevel_t::error, pattern,
                        WO_ERR_UNEXPECTED_MATCH_COUNT_FOR_TUPLE,
                        determined_type->m_external_type_description.m_tuple->m_element_types.size(),
                        tuple_pattern->m_fields.size());
                }
            }
            else
            {
                // TODO: Give typename.
                lex.record_lang_error(lexer::msglevel_t::error, pattern,
                    WO_ERR_UNEXPECTED_MATCH_TYPE_FOR_TUPLE);
            }
            return false;
        }
        case AstBase::AST_PATTERN_UNION:
        {
            wo_error("TODO");
            break;
        }
        case AstBase::AST_PATTERN_TAKEPLACE:
            // Do nothing.
            return true;
        }
        return false;
    }
    bool LangContext::check_symbol_is_imported(
        lexer& lex,
        ast::AstBase* node,
        lang_Symbol* symbol_instance,
        wo_pstring_t path)
    {
        const ast::AstBase::source_location_t* symbol_location_may_null = nullptr;
        if (symbol_instance->m_symbol_declare_location.has_value())
        {
            symbol_location_may_null = &symbol_instance->m_symbol_declare_location.value();
            if (!lex.check_source_has_been_imported_by_specify_source(
                symbol_location_may_null->source_file, path))
            {
                lex.record_lang_error(lexer::msglevel_t::error, node,
                    WO_ERR_SOURCE_MUST_BE_IMPORTED,
                    get_symbol_name_w(symbol_instance),
                    symbol_location_may_null->source_file->c_str());

                return false;
            }
        }
        return true;
    }
    bool LangContext::check_symbol_is_reachable_in_current_scope(
        lexer& lex, AstBase* node, lang_Symbol* symbol_instance, wo_pstring_t path, bool need_import_check)
    {
        if (!symbol_instance->m_is_global)
            // Local symbol is always reachable.
            return true;

        AstDeclareAttribue::accessc_attrib attrib =
            AstDeclareAttribue::accessc_attrib::PRIVATE;

        if (symbol_instance->m_declare_attribute.has_value())
        {
            ast::AstDeclareAttribue* declare_attribute =
                symbol_instance->m_declare_attribute.value();

            if (declare_attribute->m_access.has_value())
                attrib = declare_attribute->m_access.value();
        }

        const ast::AstBase::source_location_t* symbol_location_may_null = nullptr;
        if (symbol_instance->m_symbol_declare_location.has_value())
            symbol_location_may_null = &symbol_instance->m_symbol_declare_location.value();

        if (need_import_check && !check_symbol_is_imported(lex, node, symbol_instance, path))
            return false;

        switch (attrib)
        {
        case AstDeclareAttribue::accessc_attrib::PUBLIC:
            return true;
        case AstDeclareAttribue::accessc_attrib::PROTECTED:
        {
            auto* symbol_defined_in_name_space =
                symbol_instance->m_belongs_to_scope->m_belongs_to_namespace;

            for (auto* namespace_ = get_current_namespace();;
                namespace_ = namespace_->m_parent_namespace.value())
            {
                if (namespace_ == symbol_defined_in_name_space)
                    return true;

                if (!namespace_->m_parent_namespace.has_value())
                    break;
            }

            lex.record_lang_error(lexer::msglevel_t::error, node,
                WO_ERR_SYMBOL_IS_PROTECTED,
                get_symbol_name_w(symbol_instance),
                _get_scope_name(symbol_defined_in_name_space->m_this_scope.get()).c_str());

            break;
        }
        case AstDeclareAttribue::accessc_attrib::PRIVATE:
        {
            if (symbol_location_may_null == nullptr)
                // Builtin & compiler generated symbol is always reachable.
                return true;

            if (symbol_location_may_null->source_file == path)
                return true;

            lex.record_lang_error(lexer::msglevel_t::error, node,
                WO_ERR_SYMBOL_IS_PRIVATE,
                get_symbol_name_w(symbol_instance),
                symbol_location_may_null->source_file->c_str());

            break;
        }
        default:
            wo_error("unknown access attribute");
            break;
        }

        if (symbol_instance->m_symbol_declare_ast.has_value())
        {
            lex.record_lang_error(lexer::msglevel_t::infom, symbol_instance->m_symbol_declare_ast.value(),
                WO_INFO_SYMBOL_NAMED_DEFINED_HERE,
                get_symbol_name_w(symbol_instance));
        }
        return false;
    }

    bool LangContext::check_struct_field_is_reachable_in_current_scope(
        lexer& lex,
        ast::AstBase* node,
        lang_Symbol* struct_type_inst,
        ast::AstDeclareAttribue::accessc_attrib attrib,
        wo_pstring_t field_name,
        wo_pstring_t path)
    {
        if (!struct_type_inst->m_is_global)
            // Local symbol is always reachable.
            return true;

        switch (attrib)
        {
        case AstDeclareAttribue::accessc_attrib::PUBLIC:
            return true;
        case AstDeclareAttribue::accessc_attrib::PROTECTED:
        {
            auto* symbol_defined_in_name_space =
                struct_type_inst->m_belongs_to_scope->m_belongs_to_namespace;

            for (auto* namespace_ = get_current_namespace();;
                namespace_ = namespace_->m_parent_namespace.value())
            {
                if (namespace_ == symbol_defined_in_name_space)
                    return true;

                if (!namespace_->m_parent_namespace.has_value())
                    break;
            }

            lex.record_lang_error(lexer::msglevel_t::error, node,
                WO_ERR_STRUCT_FIELD_IS_PROTECTED,
                field_name->c_str(),
                _get_scope_name(symbol_defined_in_name_space->m_this_scope.get()).c_str());

            break;
        }
        case AstDeclareAttribue::accessc_attrib::PRIVATE:
        {
            if (!struct_type_inst->m_symbol_declare_location.has_value())
                // Builtin & compiler generated symbol is always reachable.
                return true;

            auto& location = struct_type_inst->m_symbol_declare_location.value();

            if (location.source_file == path)
                return true;

            lex.record_lang_error(lexer::msglevel_t::error, node,
                WO_ERR_STRUCT_FIELD_IS_PRIVATE,
                field_name->c_str(),
                location.source_file->c_str());

            break;
        }
        default:
            wo_error("unknown access attribute");
            break;
        }

        if (struct_type_inst->m_symbol_declare_ast.has_value())
        {
            lex.record_lang_error(lexer::msglevel_t::infom, struct_type_inst->m_symbol_declare_ast.value(),
                WO_INFO_SYMBOL_NAMED_DEFINED_HERE,
                get_symbol_name_w(struct_type_inst));
        }
        return false;
    }

    void LangContext::init_pass1()
    {
        WO_LANG_REGISTER_PROCESSER(AstList, AstBase::AST_LIST, pass1);
        WO_LANG_REGISTER_PROCESSER(AstIdentifier, AstBase::AST_IDENTIFIER, pass1);
        WO_LANG_REGISTER_PROCESSER(AstTemplateArgument, AstBase::AST_TEMPLATE_ARGUMENT, pass1);
        WO_LANG_REGISTER_PROCESSER(AstTemplateParam, AstBase::AST_TEMPLATE_PARAM, pass1);
        WO_LANG_REGISTER_PROCESSER(AstStructFieldDefine, AstBase::AST_STRUCT_FIELD_DEFINE, pass1);
        WO_LANG_REGISTER_PROCESSER(AstTypeHolder, AstBase::AST_TYPE_HOLDER, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueNothing, AstBase::AST_VALUE_NOTHING, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueMarkAsMutable, AstBase::AST_VALUE_MARK_AS_MUTABLE, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueMarkAsImmutable, AstBase::AST_VALUE_MARK_AS_IMMUTABLE, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueLiteral, AstBase::AST_VALUE_LITERAL, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueTypeid, AstBase::AST_VALUE_TYPEID, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueTypeCast, AstBase::AST_VALUE_TYPE_CAST, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueDoAsVoid, AstBase::AST_VALUE_DO_AS_VOID, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueTypeCheckIs, AstBase::AST_VALUE_TYPE_CHECK_IS, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueTypeCheckAs, AstBase::AST_VALUE_TYPE_CHECK_AS, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueVariable, AstBase::AST_VALUE_VARIABLE, pass1);
        WO_LANG_REGISTER_PROCESSER(AstWhereConstraints, AstBase::AST_WHERE_CONSTRAINTS, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueFunctionCall_FakeAstArgumentDeductionContextA,
            AstBase::AST_VALUE_FUNCTION_CALL_FAKE_AST_ARGUMENT_DEDUCTION_CONTEXT_A, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueFunctionCall_FakeAstArgumentDeductionContextB,
            AstBase::AST_VALUE_FUNCTION_CALL_FAKE_AST_ARGUMENT_DEDUCTION_CONTEXT_B, pass1);
        WO_LANG_REGISTER_PROCESSER(AstTemplateConstantTypeCheckInPass1,
            AstBase::AST_TEMPLATE_CONSTANT_TYPE_CHECK_IN_PASS1, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueFunctionCall, AstBase::AST_VALUE_FUNCTION_CALL, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueBinaryOperator, AstBase::AST_VALUE_BINARY_OPERATOR, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueUnaryOperator, AstBase::AST_VALUE_UNARY_OPERATOR, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueTribleOperator, AstBase::AST_VALUE_TRIBLE_OPERATOR, pass1);
        WO_LANG_REGISTER_PROCESSER(AstFakeValueUnpack, AstBase::AST_FAKE_VALUE_UNPACK, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueVariadicArgumentsPack, AstBase::AST_VALUE_VARIADIC_ARGUMENTS_PACK, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueIndex, AstBase::AST_VALUE_INDEX, pass1);
        WO_LANG_REGISTER_PROCESSER(AstPatternVariable, AstBase::AST_PATTERN_VARIABLE, pass1);
        WO_LANG_REGISTER_PROCESSER(AstPatternIndex, AstBase::AST_PATTERN_INDEX, pass1);
        WO_LANG_REGISTER_PROCESSER(AstVariableDefineItem, AstBase::AST_VARIABLE_DEFINE_ITEM, pass1);
        WO_LANG_REGISTER_PROCESSER(AstVariableDefines, AstBase::AST_VARIABLE_DEFINES, pass1);
        WO_LANG_REGISTER_PROCESSER(AstFunctionParameterDeclare, AstBase::AST_FUNCTION_PARAMETER_DECLARE, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueFunction, AstBase::AST_VALUE_FUNCTION, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueArrayOrVec, AstBase::AST_VALUE_ARRAY_OR_VEC, pass1);
        WO_LANG_REGISTER_PROCESSER(AstKeyValuePair, AstBase::AST_KEY_VALUE_PAIR, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueDictOrMap, AstBase::AST_VALUE_DICT_OR_MAP, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueTuple, AstBase::AST_VALUE_TUPLE, pass1);
        WO_LANG_REGISTER_PROCESSER(AstStructFieldValuePair, AstBase::AST_STRUCT_FIELD_VALUE_PAIR, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueStruct, AstBase::AST_VALUE_STRUCT, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueAssign, AstBase::AST_VALUE_ASSIGN, pass1);
        WO_LANG_REGISTER_PROCESSER(AstNamespace, AstBase::AST_NAMESPACE, pass1);
        WO_LANG_REGISTER_PROCESSER(AstScope, AstBase::AST_SCOPE, pass1);
        WO_LANG_REGISTER_PROCESSER(AstMatchCase, AstBase::AST_MATCH_CASE, pass1);
        WO_LANG_REGISTER_PROCESSER(AstMatch, AstBase::AST_MATCH, pass1);
        WO_LANG_REGISTER_PROCESSER(AstIf, AstBase::AST_IF, pass1);
        WO_LANG_REGISTER_PROCESSER(AstWhile, AstBase::AST_WHILE, pass1);
        WO_LANG_REGISTER_PROCESSER(AstFor, AstBase::AST_FOR, pass1);
        WO_LANG_REGISTER_PROCESSER(AstForeach, AstBase::AST_FOREACH, pass1);
        WO_LANG_REGISTER_PROCESSER(AstBreak, AstBase::AST_BREAK, pass1);
        WO_LANG_REGISTER_PROCESSER(AstContinue, AstBase::AST_CONTINUE, pass1);
        WO_LANG_REGISTER_PROCESSER(AstReturn, AstBase::AST_RETURN, pass1);
        WO_LANG_REGISTER_PROCESSER(AstLabeled, AstBase::AST_LABELED, pass1);
        WO_LANG_REGISTER_PROCESSER(AstUsingTypeDeclare, AstBase::AST_USING_TYPE_DECLARE, pass1);
        WO_LANG_REGISTER_PROCESSER(AstAliasTypeDeclare, AstBase::AST_ALIAS_TYPE_DECLARE, pass1);
        WO_LANG_REGISTER_PROCESSER(AstEnumDeclare, AstBase::AST_ENUM_DECLARE, pass1);
        WO_LANG_REGISTER_PROCESSER(AstUnionDeclare, AstBase::AST_UNION_DECLARE, pass1);
        WO_LANG_REGISTER_PROCESSER(AstValueMakeUnion, AstBase::AST_VALUE_MAKE_UNION, pass1);
        WO_LANG_REGISTER_PROCESSER(AstUsingNamespace, AstBase::AST_USING_NAMESPACE, pass1);
        WO_LANG_REGISTER_PROCESSER(AstExternInformation, AstBase::AST_EXTERN_INFORMATION, pass1);
        WO_LANG_REGISTER_PROCESSER(AstNop, AstBase::AST_NOP, pass1);
    }

#define WO_PASS_PROCESSER(AST) WO_PASS_PROCESSER_IMPL(AST, pass1)

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
            if (node->m_LANG_determined_scope)
                entry_spcify_scope(node->m_LANG_determined_scope.value());
            else
            {
                begin_new_scope(node->source_location);
                node->m_LANG_determined_scope = get_current_scope();
            }

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
            entry_spcify_namespace(node->m_LANG_determined_namespace.value());

            WO_CONTINUE_PROCESS(node->m_body);

            return HOLD;
        }
        end_last_scope();
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstIdentifier)
    {
        if (state == UNPROCESSED)
        {
            switch (node->m_formal)
            {
            case AstIdentifier::identifier_formal::FROM_GLOBAL:
            case AstIdentifier::identifier_formal::FROM_CURRENT:
                break;
            case AstIdentifier::identifier_formal::FROM_TYPE:
            {
                wo_assert(node->m_from_type);

                AstTypeHolder** type_holder = std::get_if<AstTypeHolder*>(&node->m_from_type.value());
                if (type_holder != nullptr)
                    WO_CONTINUE_PROCESS(*type_holder);
                break;
            }
            }
            if (node->m_template_arguments)
            {
                // WO_CONTINUE_PROCESS_LIST(..)
                WO_CONTINUE_PROCESS_LIST(node->m_template_arguments.value());
            }
            return HOLD;
        }
        else if (state == HOLD)
        {
            bool ambiguous = false;
            if (!find_symbol_in_current_scope(lex, node, &ambiguous))
            {
                if (node->m_find_type_only)
                    lex.record_lang_error(lexer::msglevel_t::error, node,
                        WO_ERR_UNFOUND_TYPE_NAMED,
                        node->m_name->c_str());
                else
                    lex.record_lang_error(lexer::msglevel_t::error, node,
                        WO_ERR_UNFOUND_VARIABLE_NAMED,
                        node->m_name->c_str());

                return FAILED;
            }
            else if (ambiguous)
                return FAILED;
            else
            {
                lang_Symbol* symbol = node->m_LANG_determined_symbol.value();

                bool has_template_arguments =
                    node->m_template_arguments.has_value()
                    || node->m_LANG_determined_and_appended_template_arguments.has_value();

                bool accept_template_arguments = symbol->m_is_template;

                if ((has_template_arguments != accept_template_arguments)
                    && !symbol->m_is_builtin)
                {
                    if (accept_template_arguments)
                    {
                        lex.record_lang_error(lexer::msglevel_t::error, node,
                            WO_ERR_EXPECTED_TEMPLATE_ARGUMENT,
                            get_symbol_name_w(symbol));

                        if (symbol->m_symbol_declare_ast.has_value())
                        {
                            lex.record_lang_error(lexer::msglevel_t::infom, symbol->m_symbol_declare_ast.value(),
                                WO_INFO_SYMBOL_NAMED_DEFINED_HERE,
                                get_symbol_name_w(symbol));
                        }

                        return FAILED;
                    }
                    else
                    {
                        // Template argument refill is vaild for alias.
                        if (symbol->m_symbol_kind != lang_Symbol::kind::ALIAS)
                        {
                            lex.record_lang_error(lexer::msglevel_t::error, node,
                                WO_ERR_UNEXPECTED_TEMPLATE_ARGUMENT,
                                get_symbol_name_w(symbol));

                            if (symbol->m_symbol_declare_ast.has_value())
                            {
                                lex.record_lang_error(lexer::msglevel_t::infom, symbol->m_symbol_declare_ast.value(),
                                    WO_INFO_SYMBOL_NAMED_DEFINED_HERE,
                                    get_symbol_name_w(symbol));
                            }

                            return FAILED;
                        }
                    }
                }
            }
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstTemplateParam)
    {
        if (state == UNPROCESSED)
        {
            if (node->m_marked_type.has_value())
                WO_CONTINUE_PROCESS(node->m_marked_type.value());

            return HOLD;
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstTemplateArgument)
    {
        if (state == UNPROCESSED)
        {
            if (node->is_type())
                WO_CONTINUE_PROCESS(node->get_type());
            else
                WO_CONTINUE_PROCESS(node->get_constant());

            return HOLD;
        }
        else if (state == HOLD)
        {
            if (node->is_constant())
            {
                AstValueBase* val = node->get_constant();
                if (!val->m_evaled_const_value.has_value())
                {
                    lex.record_lang_error(lexer::msglevel_t::error, val,
                        WO_ERR_VALUE_SHOULD_BE_CONST_FOR_TEMPLATE_ARG);

                    return FAILED;
                }
            }
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstStructFieldDefine)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_type);
            return HOLD;
        }
        return WO_EXCEPT_ERROR(state, OKAY);

    }
    WO_PASS_PROCESSER(AstTypeHolder)
    {
        if (node->m_LANG_trying_advancing_type_judgement)
        {
            end_last_scope(); // Leave temporary advance the processing of declaration nodes.

            auto current_error_frame = std::move(lex.get_current_error_frame());
            lex.end_trying_block();

            auto& last_error_frame = lex.get_current_error_frame();
            const size_t current_error_frame_depth = lex.get_error_frame_layer();

            for (auto& errinform : current_error_frame)
            {
                // NOTE: Advanced judgement failed, only non-template will be here. 
                // make sure report it into error list.
                lex.append_message(errinform).m_layer = errinform.m_layer - 1;

                auto& root_error_frame = lex.get_root_error_frame();
                if (&last_error_frame != &root_error_frame)
                {
                    auto& supper_error = root_error_frame.emplace_back(errinform);
                    supper_error.m_layer -= current_error_frame_depth;
                }
            }
        }

        if (state == UNPROCESSED)
        {
            switch (node->m_formal)
            {
            case AstTypeHolder::IDENTIFIER:
                WO_CONTINUE_PROCESS(node->m_typeform.m_identifier);
                break;
            case AstTypeHolder::TYPEOF:
                WO_CONTINUE_PROCESS(node->m_typeform.m_typefrom);
                break;
            case AstTypeHolder::FUNCTION:
                WO_CONTINUE_PROCESS_LIST(node->m_typeform.m_function.m_parameters);
                WO_CONTINUE_PROCESS(node->m_typeform.m_function.m_return_type);
                break;
            case AstTypeHolder::STRUCTURE:
                WO_CONTINUE_PROCESS_LIST(node->m_typeform.m_structure.m_fields);
                break;
            case AstTypeHolder::TUPLE:
                WO_CONTINUE_PROCESS_LIST(node->m_typeform.m_tuple.m_fields);
                break;
            case AstTypeHolder::UNION:
                for (auto& field : node->m_typeform.m_union.m_fields)
                {
                    if (field.m_item)
                        WO_CONTINUE_PROCESS(field.m_item.value());
                }
                break;
            default:
                wo_error("unknown type holder formal");
                break;
            }
            return HOLD;
        }
        else if (state == HOLD)
        {
            switch (node->m_formal)
            {
            case AstTypeHolder::IDENTIFIER:
            {
                AstIdentifier* identifier = node->m_typeform.m_identifier;
                lang_Symbol* type_symbol = identifier->m_LANG_determined_symbol.value();

                // If is refilling, use the target symbol.
                if (node->m_LANG_refilling_template_target_symbol.has_value())
                    type_symbol = node->m_LANG_refilling_template_target_symbol.value();

                wo_assert(type_symbol->m_symbol_kind != lang_Symbol::VARIABLE);

                if (!type_symbol->m_is_builtin)
                {
                    union
                    {
                        lang_TypeInstance* type_instance;
                        lang_AliasInstance* alias_instance;
                    };

                    if (!node->m_LANG_template_evalating_state)
                    {
                        if (type_symbol->m_is_template)
                        {
                            lang_Symbol::TemplateArgumentListT template_args;
                            if (node->m_typeform.m_identifier->m_template_arguments.has_value())
                            {
                                for (auto& template_argument : node->m_typeform.m_identifier->m_template_arguments.value())
                                {
                                    if (template_argument->is_type())
                                        template_args.emplace_back(
                                            template_argument->get_type()->m_LANG_determined_type.value());
                                    else
                                        template_args.emplace_back(
                                            template_argument->get_constant());
                                }
                            }
                            if (node->m_typeform.m_identifier->m_LANG_determined_and_appended_template_arguments.has_value())
                            {
                                const auto& determined_tyope_list =
                                    node->m_typeform.m_identifier->m_LANG_determined_and_appended_template_arguments.value();
                                template_args.insert(template_args.end(),
                                    determined_tyope_list.begin(),
                                    determined_tyope_list.end());
                            }

                            bool continue_process = false;
                            auto template_eval_state_instance_may_nullopt = begin_eval_template_ast(
                                lex, node, type_symbol, template_args, out_stack, &continue_process);

                            if (!template_eval_state_instance_may_nullopt)
                                return FAILED;

                            auto* template_eval_state_instance =
                                template_eval_state_instance_may_nullopt.value();

                            if (continue_process)
                            {
                                node->m_LANG_template_evalating_state = template_eval_state_instance;
                                return HOLD;
                            }
                            else
                            {
                                if (type_symbol->m_symbol_kind == lang_Symbol::ALIAS)
                                    alias_instance = static_cast<lang_TemplateAstEvalStateAlias*>(
                                        template_eval_state_instance)->m_alias_instance.get();
                                else
                                    type_instance = static_cast<lang_TemplateAstEvalStateType*>(
                                        template_eval_state_instance)->m_type_instance.get();
                            }
                        }
                        else
                        {
                            if (type_symbol->m_symbol_kind == lang_Symbol::ALIAS)
                                alias_instance = type_symbol->m_alias_instance;
                            else
                                type_instance = type_symbol->m_type_instance;
                        }

                        bool type_or_alias_determined =
                            type_symbol->m_symbol_kind == lang_Symbol::ALIAS
                            ? alias_instance->m_determined_type.has_value()
                            : type_instance->get_determined_type().has_value()
                            ;

                        if (!type_or_alias_determined)
                        {
                            // NOTE: What ever type or alias, we should trying to determine the type.
                            if (!node->m_LANG_trying_advancing_type_judgement && type_symbol->m_symbol_declare_ast)
                            {
                                auto* define_ast = type_symbol->m_symbol_declare_ast.value();

                                if (define_ast->finished_state == UNPROCESSED)
                                {
                                    node->m_LANG_trying_advancing_type_judgement = true;

                                    wo_assert(!type_symbol->m_is_template && type_symbol->m_is_global);

                                    // Capture all error happend in this block.
                                    lex.begin_trying_block();

                                    // Immediately advance the processing of declaration nodes.
                                    entry_spcify_scope(type_symbol->m_belongs_to_scope);
                                    WO_CONTINUE_PROCESS(define_ast);
                                    return HOLD;
                                }
                            }

                            // NOTE: Type not decided is okay, but alias not.
                            if (type_symbol->m_symbol_kind == lang_Symbol::ALIAS)
                            {
                                lex.record_lang_error(lexer::msglevel_t::error, node,
                                    WO_ERR_TYPE_DETERMINED_FAILED);
                                return FAILED;
                            }
                        }

                        if (type_symbol->m_symbol_kind == lang_Symbol::ALIAS)
                        {
                            // Check symbol can be reach.
                            if (!check_symbol_is_reachable_in_current_scope(
                                lex,
                                node,
                                type_symbol,
                                node->source_location.source_file,
                                !node->duplicated_node /* TMP: Skip import check in template function. */))
                            {
                                return FAILED;
                            }
                            else if (!alias_instance->m_symbol->m_is_template
                                && identifier->m_template_arguments.has_value())
                            {
                                lang_Symbol* refiliing_symbol =
                                    alias_instance->m_determined_type.value()->m_symbol;

                                wo_assert(refiliing_symbol->m_symbol_kind == lang_Symbol::kind::TYPE);
                                if (!refiliing_symbol->m_is_template && !refiliing_symbol->m_is_builtin)
                                {
                                    lex.record_lang_error(lexer::msglevel_t::error, node,
                                        WO_ERR_UNEXPECTED_TEMPLATE_ARGUMENT,
                                        get_symbol_name_w(refiliing_symbol));

                                    lex.record_lang_error(lexer::msglevel_t::infom, node,
                                        WO_INFO_TRYING_REFILL_TEMPLATE_ARGUMENT,
                                        get_type_name_w(alias_instance->m_determined_type.value()),
                                        get_symbol_name_w(alias_instance->m_symbol));

                                    if (refiliing_symbol->m_symbol_declare_ast.has_value())
                                    {
                                        lex.record_lang_error(lexer::msglevel_t::infom, refiliing_symbol->m_symbol_declare_ast.value(),
                                            WO_INFO_SYMBOL_NAMED_DEFINED_HERE,
                                            get_symbol_name_w(refiliing_symbol));
                                    }

                                    return FAILED;
                                }

                                // Template refilling.
                                node->m_LANG_refilling_template_target_symbol = refiliing_symbol;

                                // NOTE: Reset m_LANG_trying_advancing_type_judgement, symbol has been determined.
                                if (node->m_LANG_trying_advancing_type_judgement)
                                    node->m_LANG_trying_advancing_type_judgement = false;

                                // Re-eval it.
                                return HOLD;
                            }
                        }
                        else if (!node->duplicated_node)
                        {
                            wo_assert(type_symbol->m_symbol_kind == lang_Symbol::TYPE);
                            if (!check_symbol_is_imported(
                                lex,
                                node,
                                type_symbol,
                                node->source_location.source_file))
                                return FAILED;
                        }
                    }
                    else
                    {
                        auto* state = static_cast<lang_TemplateAstEvalStateBase*>(
                            node->m_LANG_template_evalating_state.value());

                        finish_eval_template_ast(lex, state);

                        if (type_symbol->m_symbol_kind == lang_Symbol::ALIAS)
                            alias_instance = static_cast<lang_TemplateAstEvalStateAlias*>(
                                state)->m_alias_instance.get();
                        else
                            type_instance = static_cast<lang_TemplateAstEvalStateType*>(
                                state)->m_type_instance.get();
                    }

                    if (type_symbol->m_symbol_kind == lang_Symbol::ALIAS)
                    {
                        // Eval alias type.
                        node->m_LANG_alias_instance_only_for_lspv2 = alias_instance;
                        type_instance = alias_instance->m_determined_type.value();

                    }

                    wo_assert(type_instance != nullptr);
                    node->m_LANG_determined_type = type_instance;
                    break;
                }
                else
                {
                    /* FALL-THROUGH */
                }

                /* FALL-THROUGH */
            }
            [[fallthrough]];
            case AstTypeHolder::FUNCTION:
            case AstTypeHolder::TUPLE:
            case AstTypeHolder::UNION:
            case AstTypeHolder::STRUCTURE:
                node->m_LANG_determined_type = m_origin_types.create_or_find_origin_type(lex, this, node);
                if (!node->m_LANG_determined_type.has_value())
                    return FAILED;
                break;
            case AstTypeHolder::TYPEOF:
                wo_assert(node->m_typeform.m_typefrom->m_LANG_determined_type);
                node->m_LANG_determined_type = node->m_typeform.m_typefrom->m_LANG_determined_type.value();
                break;
            default:
                wo_error("unknown type holder formal");
                break;
            }

            switch (node->m_mutable_mark)
            {
            case AstTypeHolder::MARK_AS_IMMUTABLE:
                node->m_LANG_determined_type = immutable_type(node->m_LANG_determined_type.value());
                break;
            case AstTypeHolder::MARK_AS_MUTABLE:
                node->m_LANG_determined_type = mutable_type(node->m_LANG_determined_type.value());
                break;
            default:
                // Do nothing.
                break;
            }
        }
        else
        {
            if (node->m_LANG_template_evalating_state)
            {
                auto* state = node->m_LANG_template_evalating_state.value();

                failed_eval_template_ast(lex, node, state);
            }
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueLiteral)
    {
        wo_assert(state == UNPROCESSED);
        wo_assert(node->m_evaled_const_value);

        switch (node->m_evaled_const_value.value().type)
        {
        case value::valuetype::invalid:
            node->m_LANG_determined_type = m_origin_types.m_nil.m_type_instance;
            break;
        case value::valuetype::integer_type:
            node->m_LANG_determined_type = m_origin_types.m_int.m_type_instance;
            break;
        case value::valuetype::real_type:
            node->m_LANG_determined_type = m_origin_types.m_real.m_type_instance;
            break;
        case value::valuetype::handle_type:
            node->m_LANG_determined_type = m_origin_types.m_handle.m_type_instance;
            break;
        case value::valuetype::bool_type:
            node->m_LANG_determined_type = m_origin_types.m_bool.m_type_instance;
            break;
        case value::valuetype::string_type:
            node->m_LANG_determined_type = m_origin_types.m_string.m_type_instance;
            break;
        default:
            wo_error("unknown literal type");
            break;
        }

        return OKAY;
    }
    WO_PASS_PROCESSER(AstValueVariable)
    {
        if (node->m_LANG_trying_advancing_type_judgement)
        {
            end_last_scope(); // Leave temporary advance the processing of declaration nodes.

            auto current_error_frame = std::move(lex.get_current_error_frame());
            lex.end_trying_block();

            auto& last_error_frame = lex.get_current_error_frame();
            const size_t current_error_frame_depth = lex.get_error_frame_layer();

            for (auto& errinform : current_error_frame)
            {
                // NOTE: Advanced judgement failed, only non-template will be here. 
                // make sure report it into error list.
                lex.append_message(errinform).m_layer = errinform.m_layer - 1;

                auto& root_error_frame = lex.get_root_error_frame();
                if (&last_error_frame != &root_error_frame)
                {
                    auto& supper_error = root_error_frame.emplace_back(errinform);
                    supper_error.m_layer -= current_error_frame_depth;
                }
            }
        }

        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_identifier);
            return HOLD;
        }
        else if (state == HOLD)
        {
            lang_ValueInstance* determined_value_instance;
            if (!node->m_LANG_trying_advancing_type_judgement)
            {
                lang_ValueInstance* value_instance = nullptr;

                if (!node->m_LANG_template_evalating_state)
                {
                    wo_assert(node->m_identifier->m_LANG_determined_symbol);
                    lang_Symbol* var_symbol = node->m_identifier->m_LANG_determined_symbol.value();

                    wo_assert(var_symbol->m_symbol_kind == lang_Symbol::VARIABLE);
                    if (var_symbol->m_is_template)
                    {
                        // TEMPLATE!!!
                        // NOTE: In function call, template arguments will be 
                        //  derived and filled completely.
                        lang_Symbol::TemplateArgumentListT template_args;
                        if (node->m_identifier->m_template_arguments.has_value())
                        {
                            for (auto& template_argument : node->m_identifier->m_template_arguments.value())
                            {
                                if (template_argument->is_type())
                                    template_args.push_back(
                                        template_argument->get_type()->m_LANG_determined_type.value());
                                else
                                    template_args.push_back(
                                        template_argument->get_constant());
                            }
                        }
                        if (node->m_identifier->m_LANG_determined_and_appended_template_arguments.has_value())
                        {
                            const auto& determined_tyope_list =
                                node->m_identifier->m_LANG_determined_and_appended_template_arguments.value();
                            template_args.insert(template_args.end(),
                                determined_tyope_list.begin(),
                                determined_tyope_list.end());
                        }

                        bool continue_process = false;
                        auto template_eval_state_instance_may_nullopt = begin_eval_template_ast(
                            lex, node, var_symbol, template_args, out_stack, &continue_process);

                        if (!template_eval_state_instance_may_nullopt)
                            return FAILED;

                        auto* template_eval_state_instance =
                            static_cast<lang_TemplateAstEvalStateValue*>(
                                template_eval_state_instance_may_nullopt.value());

                        if (continue_process)
                        {
                            node->m_LANG_template_evalating_state = template_eval_state_instance;
                            return HOLD;
                        }
                        else
                            value_instance = template_eval_state_instance->m_value_instance.get();
                    }
                    else
                    {
                        value_instance = var_symbol->m_value_instance;
                    }
                }
                else
                {
                    auto* eval_template_state = static_cast<lang_TemplateAstEvalStateValue*>(
                        node->m_LANG_template_evalating_state.value());

                    finish_eval_template_ast(lex, eval_template_state);

                    value_instance = eval_template_state->m_value_instance.get();
                }

                wo_assert(value_instance != nullptr);

                determined_value_instance = value_instance;
            }
            else
            {
                // Value instance must be determined.
                determined_value_instance = node->m_LANG_variable_instance.value();
            }

            // Type has been determined?
            if (determined_value_instance->m_determined_type)
                node->m_LANG_determined_type = determined_value_instance->m_determined_type.value();
            else
            {
                lang_Symbol* var_symbol = node->m_identifier->m_LANG_determined_symbol.value();

                if (!node->m_LANG_trying_advancing_type_judgement
                    && var_symbol->m_symbol_declare_ast.has_value())
                {
                    auto* define_ast = var_symbol->m_symbol_declare_ast.value();
                    if (define_ast->finished_state == UNPROCESSED)
                    {
                        node->m_LANG_trying_advancing_type_judgement = true;
                        node->m_LANG_variable_instance = determined_value_instance;

                        wo_assert(!var_symbol->m_is_template
                            && var_symbol->m_is_global);

                        // Capture all error happend in this block.
                        lex.begin_trying_block();

                        // Type not determined, we need to determine it?
                        // NOTE: Immediately advance the processing of declaration nodes.
                        entry_spcify_scope(var_symbol->m_belongs_to_scope);
                        WO_CONTINUE_PROCESS(define_ast);
                        return HOLD;
                    }
                }

                // Type determined failed in AstVariableDefines, treat as failed.
                lex.record_lang_error(lexer::msglevel_t::error, node,
                    WO_ERR_VALUE_TYPE_DETERMINED_FAILED);

                if (var_symbol->m_symbol_declare_ast.has_value())
                {
                    lex.record_lang_error(
                        lexer::msglevel_t::infom,
                        var_symbol->m_symbol_declare_ast.value(),
                        WO_INFO_SYMBOL_NAMED_DEFINED_HERE,
                        get_symbol_name_w(var_symbol));
                }

                return FAILED;
            }

            if (determined_value_instance->m_determined_constant_or_function.has_value())
            {
                wo::value* determined_value =
                    std::get_if<wo::value>(
                        &determined_value_instance->m_determined_constant_or_function.value());

                if (determined_value != nullptr)
                    // Constant has been determined.
                    node->decide_final_constant_value(*determined_value);
            }

            // Check symbol can be reach.
            if (!check_symbol_is_reachable_in_current_scope(
                lex,
                node,
                determined_value_instance->m_symbol,
                node->source_location.source_file,
                !node->duplicated_node /* TMP: Skip import check in template function. */))
            {
                return FAILED;
            }

            // Mark as been used
            determined_value_instance->m_symbol->m_has_been_used = true;

            // Check and update value instance for captured variable.
            node->m_LANG_variable_instance =
                check_and_update_captured_varibale_in_current_scope(
                    node,
                    determined_value_instance);

            // NOTE: Value in advancing_type_judgement must not be a captured variable.
            wo_assert(!node->m_LANG_trying_advancing_type_judgement
                || node->m_LANG_variable_instance == determined_value_instance);
        }
        else
        {
            // Failed to evaluate template.
            if (node->m_LANG_template_evalating_state)
            {
                auto* state = node->m_LANG_template_evalating_state.value();
                failed_eval_template_ast(lex, node, state);
            }
        }
        return WO_EXCEPT_ERROR(state, OKAY);
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
    }
    WO_PASS_PROCESSER(AstVariableDefineItem)
    {
        if (state == UNPROCESSED)
        {
            if (!declare_pattern_symbol_pass0_1(
                lex,
                false,
                node->m_LANG_declare_attribute,
                node,
                node->m_pattern,
                node->m_init_value))
                return FAILED;

            if (node->m_pattern->node_type == AstBase::AST_PATTERN_SINGLE)
            {
                AstPatternSingle* single_pattern = static_cast<AstPatternSingle*>(node->m_pattern);
                if (single_pattern->m_template_parameters)
                    // Template variable, skip process init value.
                    return HOLD;

                else if (node->m_init_value->node_type == AstBase::AST_VALUE_FUNCTION)
                {
                    AstValueFunction* value_function = static_cast<AstValueFunction*>(node->m_init_value);
                    value_function->m_LANG_value_instance_to_update =
                        single_pattern->m_LANG_declared_symbol.value()->m_value_instance;
                }
            }

            WO_CONTINUE_PROCESS(node->m_init_value);
            return HOLD;
        }
        else if (state == HOLD)
        {
            if (!update_pattern_symbol_variable_type_pass1(
                lex,
                node->m_pattern,
                node->m_init_value,
                node->m_init_value->m_LANG_determined_type))
                return FAILED;
        }

        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstAliasTypeDeclare)
    {
        if (state == UNPROCESSED)
        {
            if (!node->m_LANG_declared_symbol)
            {
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
                }
                else
                {
                    lex.record_lang_error(lexer::msglevel_t::error, node,
                        WO_ERR_REDEFINED,
                        node->m_typename->c_str());

                    if (defined_symbol->m_symbol_declare_ast.has_value())
                        lex.record_lang_error(lexer::msglevel_t::infom,
                            defined_symbol->m_symbol_declare_ast.value(),
                            WO_INFO_SYMBOL_NAMED_DEFINED_HERE,
                            get_symbol_name_w(defined_symbol));

                    return FAILED;
                }
            }
            if (!node->m_template_parameters)
            {
                // Update type instance.
                WO_CONTINUE_PROCESS(node->m_type);
                return HOLD;
            }
            return OKAY;
        }
        else if (state == HOLD)
        {
            wo_assert(!node->m_template_parameters);
            lang_Symbol* symbol = node->m_LANG_declared_symbol.value();

            symbol->m_alias_instance->m_determined_type =
                node->m_type->m_LANG_determined_type.value();
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstUsingTypeDeclare)
    {
        if (state == UNPROCESSED)
        {
            bool symbol_defined_success = false;
            lang_Symbol* defined_symbol = nullptr;

            if (!node->m_LANG_declared_symbol)
            {
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
                }
                else
                {
                    lex.record_lang_error(lexer::msglevel_t::error, node,
                        WO_ERR_REDEFINED,
                        node->m_typename->c_str());

                    if (defined_symbol->m_symbol_declare_ast.has_value())
                        lex.record_lang_error(lexer::msglevel_t::infom,
                            defined_symbol->m_symbol_declare_ast.value(),
                            WO_INFO_SYMBOL_NAMED_DEFINED_HERE,
                            get_symbol_name_w(defined_symbol));

                    return FAILED;
                }
            }

            if (!node->m_template_parameters)
            {
                lang_Symbol* type_symbol = node->m_LANG_declared_symbol.value();
                if (type_symbol->m_belongs_to_scope->is_namespace_scope())
                {
                    lang_Namespace* type_namespace = type_symbol->m_belongs_to_scope->m_belongs_to_namespace;
                    auto fnd = type_namespace->m_sub_namespaces.find(type_symbol->m_name);
                    if (fnd != type_namespace->m_sub_namespaces.end())
                    {
                        node->m_LANG_type_namespace_entried = true;
                        entry_spcify_scope(fnd->second->m_this_scope.get());
                    }
                }

                // Update type instance.
                WO_CONTINUE_PROCESS(node->m_type);
            }
            return HOLD;
        }
        else if (state == HOLD)
        {
            if (!node->m_template_parameters)
            {
                if (node->m_LANG_type_namespace_entried)
                    end_last_scope();

                // TYPE HAS BEEN DETERMINED, UPDATE THE SYMBOL;
                lang_Symbol* symbol = node->m_LANG_declared_symbol.value();
                symbol->m_type_instance->determine_base_type_by_another_type(
                    node->m_type->m_LANG_determined_type.value());
            }
        }
        else
        {
            if (node->m_LANG_type_namespace_entried)
                end_last_scope();
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstWhereConstraints)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS_LIST(node->m_constraints);
            return HOLD;
        }
        else if (state == HOLD)
        {
            bool failed = false;
            for (auto& constraint : node->m_constraints)
            {
                if (!constraint->m_evaled_const_value)
                {
                    failed = true;
                    lex.record_lang_error(lexer::msglevel_t::error, constraint,
                        WO_ERR_CONSTRAINT_SHOULD_BE_CONST);
                    continue;
                }

                auto* constraint_type = constraint->m_LANG_determined_type.value();
                if (constraint_type != m_origin_types.m_bool.m_type_instance)
                {
                    failed = true;
                    lex.record_lang_error(lexer::msglevel_t::error, constraint,
                        WO_ERR_CONSTRAINT_SHOULD_BE_BOOL);
                    continue;
                }

                if (!constraint->m_evaled_const_value.value().integer)
                {
                    failed = true;
                    lex.record_lang_error(lexer::msglevel_t::error, constraint,
                        WO_ERR_CONSTRAINT_FAILED);
                    continue;
                }
            }

            if (failed)
                return FAILED;
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstFunctionParameterDeclare)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_type.value());
            return HOLD;
        }
        else if (state == HOLD)
        {
            if (!declare_pattern_symbol_pass0_1(
                lex,
                false,
                std::nullopt,
                node,
                node->m_pattern,
                std::nullopt))
            {
                // Failed.
                return FAILED;
            }

            // Update pattern symbol type.
            if (!update_pattern_symbol_variable_type_pass1(
                lex,
                node->m_pattern,
                std::nullopt,
                node->m_type.value()->m_LANG_determined_type))
            {
                // Failed.
                return FAILED;
            }
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstTemplateConstantTypeCheckInPass1)
    {
        if (state == UNPROCESSED)
        {
            for (auto& pair : node->m_LANG_constant_check_pairs)
                WO_CONTINUE_PROCESS(pair.m_cloned_param_type);

            node->m_LANG_hold_state =
                AstTemplateConstantTypeCheckInPass1::HOLD_FOR_TYPE_UPDATE;

            return HOLD;
        }
        else if (state == HOLD)
        {
            switch (node->m_LANG_hold_state)
            {
            case AstTemplateConstantTypeCheckInPass1::HOLD_FOR_TYPE_UPDATE:
            {
                bool failed = false;
                for (auto& [param_type, argument_type] : node->m_LANG_constant_check_pairs)
                {
                    if (lang_TypeInstance::TypeCheckResult::ACCEPT != is_type_accepted(
                        lex, param_type, param_type->m_LANG_determined_type.value(), argument_type))
                    {
                        failed = true;

                        lex.record_lang_error(lexer::msglevel_t::error, param_type,
                            WO_ERR_CANNOT_ACCEPTABLE_TYPE_NAMED,
                            get_type_name_w(argument_type),
                            get_type_name_w(param_type->m_LANG_determined_type.value()));
                    }
                }

                if (failed)
                    return FAILED;

                node->m_LANG_hold_state =
                    AstTemplateConstantTypeCheckInPass1::HOLD_FOR_MAKING_TEMPALTE_INSTANCE;

                WO_CONTINUE_PROCESS(node->m_template_instance);

                return HOLD;
            }
            case AstTemplateConstantTypeCheckInPass1::HOLD_FOR_MAKING_TEMPALTE_INSTANCE:
                break;
            default:
                wo_error("unknown hold state");
                break;
            }
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueFunction)
    {
        auto judge_function_return_type =
            [&](lang_TypeInstance* ret_type)
        {
            std::list<lang_TypeInstance*> parameters;
            for (auto& param : node->m_parameters)
                parameters.push_back(param->m_type.value()->m_LANG_determined_type.value());

            node->m_LANG_determined_type = m_origin_types.create_function_type(
                node->m_is_variadic, parameters, ret_type);

            wo_assert(node->m_LANG_determined_type.has_value());

            if (node->m_LANG_value_instance_to_update)
            {
                node->m_LANG_value_instance_to_update.value()->m_determined_type =
                    node->m_LANG_determined_type;
            }
        };

        // Huston, we have a problem.
        if (state == UNPROCESSED)
        {
            if (node->m_pending_param_type_mark_template.has_value()
                && !node->m_LANG_in_template_reification_context)
            {
                lex.record_lang_error(lexer::msglevel_t::error, node,
                    WO_ERR_NOT_IN_REIFICATION_TEMPLATE_FUNC);
                return FAILED;
            }

            if (node->m_LANG_determined_template_arguments.has_value())
            {
                begin_new_scope(std::nullopt);

                wo_assert(node->m_LANG_determined_template_arguments.value().size()
                    == node->m_pending_param_type_mark_template.value().size());

                if (!fast_check_and_create_template_type_alias_and_constant_in_current_scope(
                    lex,
                    node->m_pending_param_type_mark_template.value(),
                    node->m_LANG_determined_template_arguments.value(),
                    std::nullopt))
                {
                    end_last_scope();
                    return FAILED;
                }
            }

            node->m_LANG_hold_state = AstValueFunction::HOLD_FOR_PARAMETER_EVAL;

            // Begin new function.
            begin_new_function(node);

            node->m_LANG_function_scope = get_current_scope();

            WO_CONTINUE_PROCESS_LIST(node->m_parameters);
            return HOLD;
        }
        else if (state == HOLD)
        {
            switch (node->m_LANG_hold_state)
            {
            case AstValueFunction::HOLD_FOR_PARAMETER_EVAL:
            {
                if (node->m_marked_return_type)
                {
                    node->m_LANG_hold_state = AstValueFunction::HOLD_FOR_RETURN_TYPE_EVAL;
                    WO_CONTINUE_PROCESS(node->m_marked_return_type.value());
                    return HOLD;
                }
                /* FALL THROUGH */
            }
            [[fallthrough]];
            case AstValueFunction::HOLD_FOR_RETURN_TYPE_EVAL:
            {
                // Eval function type for outside.
                if (node->m_marked_return_type)
                {
                    auto* return_type_instance = node->m_marked_return_type.value()->m_LANG_determined_type.value();
                    judge_function_return_type(return_type_instance);
                }

                if (node->m_where_constraints.has_value())
                {
                    node->m_LANG_hold_state = AstValueFunction::HOLD_FOR_EVAL_WHERE_CONSTRAINTS;
                    WO_CONTINUE_PROCESS(node->m_where_constraints.value());
                    return HOLD;
                }
                /* FALL THROUGH */
            }
            [[fallthrough]];
            case AstValueFunction::HOLD_FOR_EVAL_WHERE_CONSTRAINTS:
            {
                node->m_LANG_hold_state = AstValueFunction::HOLD_FOR_BODY_EVAL;
                WO_CONTINUE_PROCESS(node->m_body);
                return HOLD;
            }
            case AstValueFunction::HOLD_FOR_BODY_EVAL:
            {
                end_last_function();
                if (node->m_LANG_determined_template_arguments.has_value())
                    end_last_scope();

                if (!node->m_LANG_determined_return_type)
                    node->m_LANG_determined_return_type = m_origin_types.m_void.m_type_instance;

                if (node->m_marked_return_type)
                {
                    // Function type has been determined.

                    auto* marked_return_type = node->m_marked_return_type.value();

                    auto* return_type_instance = marked_return_type->m_LANG_determined_type.value();
                    auto* determined_return_type = node->m_LANG_determined_return_type.value();

                    if (lang_TypeInstance::TypeCheckResult::ACCEPT != is_type_accepted(
                        lex,
                        marked_return_type,
                        return_type_instance,
                        determined_return_type))
                    {
                        lex.record_lang_error(lexer::msglevel_t::error, node,
                            WO_ERR_UNMATCHED_RETURN_TYPE_NAMED,
                            get_type_name_w(determined_return_type),
                            get_type_name_w(return_type_instance));
                        return FAILED;
                    }
                }
                else
                {
                    judge_function_return_type(
                        node->m_LANG_determined_return_type.value());
                }

                node->m_LANG_captured_context.m_finished = true;

                if (!node->m_LANG_captured_context.m_captured_variables.empty())
                {
                    if (node->m_LANG_captured_context.m_self_referenced)
                    {
                        lex.record_lang_error(lexer::msglevel_t::error, node,
                            WO_ERR_UNABLE_CAPTURE_IN_RECURSIVE_FUNC);

                        for (auto& [captured_from, capture_instance] : node->m_LANG_captured_context.m_captured_variables)
                        {
                            for (auto& ref_variable : capture_instance.m_referenced_variables)
                            {
                                lex.record_lang_error(lexer::msglevel_t::infom,
                                    ref_variable,
                                    WO_INFO_CAPTURED_VARIABLE_USED_HERE,
                                    get_value_name_w(captured_from));
                            }
                        }
                        return FAILED;
                    }
                }
                else if (node->m_LANG_value_instance_to_update.has_value())
                {
                    // This function no need capture variables in runtime.
                    node->m_LANG_value_instance_to_update.value()->m_IR_normal_function = node;
                }
                break;
            }
            default:
                wo_error("unknown hold state");
                break;
            }
        }
        else
        {
            end_last_function(); // Failed, leave the function.
            if (node->m_LANG_determined_template_arguments.has_value())
                end_last_scope(); // Failed, leave the template type alias.
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstReturn)
    {
        if (state == UNPROCESSED)
        {
            node->m_LANG_belong_function_may_null_if_outside = get_current_function();

            if (node->m_value.has_value())
                WO_CONTINUE_PROCESS(node->m_value.value());

            return HOLD;
        }
        else if (state == HOLD)
        {
            if (node->m_LANG_belong_function_may_null_if_outside.has_value())
            {
                lang_TypeInstance* return_value_type;
                if (node->m_value)
                    return_value_type = node->m_value.value()->m_LANG_determined_type.value();
                else
                    return_value_type = m_origin_types.m_void.m_type_instance;

                auto* function_instance =
                    node->m_LANG_belong_function_may_null_if_outside.value();

                // Has marked return type?
                if (function_instance->m_marked_return_type.has_value())
                {
                    if (!function_instance->m_LANG_determined_return_type.has_value())
                        function_instance->m_LANG_determined_return_type =
                        function_instance->m_marked_return_type.value()->m_LANG_determined_type.value();

                    // Cannot mixture deduce type.
                }
                else if (function_instance->m_LANG_determined_return_type.has_value())
                {
                    if (node->m_LANG_template_evalating_state_is_mutable.has_value())
                    {
                        auto& eval_content = node->m_LANG_template_evalating_state_is_mutable.value();
                        finish_eval_template_ast(lex, eval_content.first);

                        function_instance->m_LANG_determined_return_type =
                            eval_content.second
                            ? mutable_type(eval_content.first->m_type_instance.get())
                            : eval_content.first->m_type_instance.get();
                    }
                    else
                    {
                        auto* last_function_return_type = function_instance->m_LANG_determined_return_type.value();

                        // Mixture it!
                        auto mixture_branch_type = easy_mixture_types(
                            lex,
                            node,
                            return_value_type,
                            last_function_return_type,
                            out_stack);

                        if (mixture_branch_type.m_state == TypeMixtureResult::ACCEPT)
                        {
                            function_instance->m_LANG_determined_return_type = mixture_branch_type.m_result;
                        }
                        else if (mixture_branch_type.m_state == TypeMixtureResult::TEMPLATE_MUTABLE
                            || mixture_branch_type.m_state == TypeMixtureResult::TEMPLATE_NORMAL)
                        {
                            node->m_LANG_template_evalating_state_is_mutable = std::make_pair(
                                mixture_branch_type.m_template_instance,
                                mixture_branch_type.m_state == TypeMixtureResult::TEMPLATE_MUTABLE);

                            return HOLD;
                        }
                        else
                        {
                            lex.record_lang_error(lexer::msglevel_t::error, node,
                                WO_ERR_UNABLE_TO_MIX_TYPES,
                                get_type_name_w(last_function_return_type),
                                get_type_name_w(return_value_type));

                            lex.record_lang_error(lexer::msglevel_t::infom, function_instance,
                                WO_INFO_OLD_FUNCTION_RETURN_TYPE_IS,
                                get_type_name_w(last_function_return_type));

                            return FAILED;
                        }
                    }
                }

                if (function_instance->m_LANG_determined_return_type.has_value())
                {
                    // Check acceptable?
                    auto* return_type_instance =
                        function_instance->m_LANG_determined_return_type.value();

                    if (lang_TypeInstance::TypeCheckResult::ACCEPT != is_type_accepted(
                        lex,
                        node,
                        return_type_instance,
                        return_value_type))
                    {
                        lex.record_lang_error(lexer::msglevel_t::error, node,
                            WO_ERR_UNMATCHED_RETURN_TYPE_NAMED,
                            get_type_name_w(return_value_type),
                            get_type_name_w(return_type_instance));
                        return FAILED;
                    }
                }
                else
                    function_instance->m_LANG_determined_return_type = return_value_type;

            }
        }
        else
        {
            if (node->m_LANG_template_evalating_state_is_mutable.has_value())
            {
                auto& eval_content = node->m_LANG_template_evalating_state_is_mutable.value();
                failed_eval_template_ast(lex, node, eval_content.first);
            }

        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueArrayOrVec)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS_LIST(node->m_elements);
            return HOLD;
        }
        else if (state == HOLD)
        {
            lang_TypeInstance* array_elemnet_type = m_origin_types.m_nothing.m_type_instance;
            if (!node->m_elements.empty())
            {
                auto element_iter = node->m_elements.begin();
                auto element_end = node->m_elements.end();

                array_elemnet_type = (*(element_iter++))->m_LANG_determined_type.value();
                for (; element_iter != element_end; ++element_iter)
                {
                    AstValueBase* element = *element_iter;
                    lang_TypeInstance* element_type = element->m_LANG_determined_type.value();
                    if (lang_TypeInstance::TypeCheckResult::ACCEPT
                        != is_type_accepted(lex, element, array_elemnet_type, element_type))
                    {
                        lex.record_lang_error(lexer::msglevel_t::error,
                            element,
                            WO_ERR_UNMATCHED_ARRAY_ELEMENT_TYPE_NAMED,
                            get_type_name_w(element_type),
                            get_type_name_w(array_elemnet_type));
                        return FAILED;
                    }
                }
            }
            if (node->m_making_vec)
            {
                node->m_LANG_determined_type =
                    m_origin_types.create_vector_type(array_elemnet_type);
            }
            else
            {
                node->m_LANG_determined_type =
                    m_origin_types.create_array_type(array_elemnet_type);
            }
        }

        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueNothing)
    {
        wo_assert(state == UNPROCESSED);

        node->m_LANG_determined_type =
            m_origin_types.m_nothing.m_type_instance;

        node->decide_final_constant_value(wo::value{});

        return OKAY;
    }
    WO_PASS_PROCESSER(AstValueMarkAsMutable)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_marked_value);
            return HOLD;
        }
        else if (state == HOLD)
        {
            node->m_LANG_determined_type = mutable_type(
                node->m_marked_value->m_LANG_determined_type.value());

            if (node->m_marked_value->m_evaled_const_value.has_value())
                node->decide_final_constant_value(node->m_marked_value->m_evaled_const_value.value());
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueMarkAsImmutable)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_marked_value);
            return HOLD;
        }
        else if (state == HOLD)
        {
            node->m_LANG_determined_type = immutable_type(
                node->m_marked_value->m_LANG_determined_type.value());

            if (node->m_marked_value->m_evaled_const_value.has_value())
                node->decide_final_constant_value(node->m_marked_value->m_evaled_const_value.value());
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstKeyValuePair)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_key);
            WO_CONTINUE_PROCESS(node->m_value);
            return HOLD;
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueDictOrMap)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS_LIST(node->m_elements);
            return HOLD;
        }
        else if (state == HOLD)
        {
            lang_TypeInstance* key_type = m_origin_types.m_nothing.m_type_instance;
            lang_TypeInstance* value_type = m_origin_types.m_nothing.m_type_instance;

            if (!node->m_elements.empty())
            {
                auto element_iter = node->m_elements.begin();
                auto element_end = node->m_elements.end();

                key_type = (*element_iter)->m_key->m_LANG_determined_type.value();
                value_type = (*element_iter)->m_value->m_LANG_determined_type.value();

                for (++element_iter; element_iter != element_end; ++element_iter)
                {
                    AstKeyValuePair* element = *element_iter;
                    lang_TypeInstance* element_key_type = element->m_key->m_LANG_determined_type.value();
                    lang_TypeInstance* element_value_type = element->m_value->m_LANG_determined_type.value();

                    if (lang_TypeInstance::TypeCheckResult::ACCEPT
                        != is_type_accepted(lex, element, key_type, element_key_type))
                    {
                        lex.record_lang_error(lexer::msglevel_t::error, element,
                            WO_ERR_UNMATCHED_DICT_KEY_TYPE_NAMED,
                            get_type_name_w(element_key_type),
                            get_type_name_w(key_type));
                        return FAILED;
                    }

                    if (lang_TypeInstance::TypeCheckResult::ACCEPT
                        != is_type_accepted(lex, element, value_type, element_value_type))
                    {
                        lex.record_lang_error(lexer::msglevel_t::error, element,
                            WO_ERR_UNMATCHED_DICT_VALUE_TYPE_NAMED,
                            get_type_name_w(element_value_type),
                            get_type_name_w(value_type));
                        return FAILED;
                    }
                }
            }
            if (node->m_making_map)
            {
                node->m_LANG_determined_type =
                    m_origin_types.create_mapping_type(key_type, value_type);
            }
            else
            {
                node->m_LANG_determined_type =
                    m_origin_types.create_dictionary_type(key_type, value_type);
            }
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueTuple)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS_LIST(node->m_elements);
            return HOLD;
        }
        else if (state == HOLD)
        {
            std::list<lang_TypeInstance*> element_types;
            for (auto& element : node->m_elements)
            {
                auto determined_type = element->m_LANG_determined_type.value();
                if (element->node_type == AstBase::AST_FAKE_VALUE_UNPACK)
                {
                    auto* unpack = static_cast<AstFakeValueUnpack*>(element);
                    unpack->m_IR_unpack_method = AstFakeValueUnpack::UNPACK_FOR_TUPLE;
                    auto* determined_base_type_instance = determined_type->get_determined_type().value();
                    // Unpacks base has been check in AstFakeValueUnpack.

                    if (determined_base_type_instance->m_base_type != lang_TypeInstance::DeterminedType::TUPLE)
                    {
                        lex.record_lang_error(lexer::msglevel_t::error, element,
                            WO_ERR_ONLY_EXPAND_TUPLE,
                            get_type_name_w(determined_type));
                        return FAILED;
                    }

                    for (auto& element_type : determined_base_type_instance->m_external_type_description.m_tuple->m_element_types)
                        element_types.push_back(element_type);
                }
                else
                    element_types.push_back(determined_type);
            }

            node->m_LANG_determined_type = m_origin_types.create_tuple_type(element_types);
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueIndex)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_container);
            WO_CONTINUE_PROCESS(node->m_index);
            return HOLD;
        }
        else if (state == HOLD)
        {
            // Check is index able?
            auto* container_type_instance = node->m_container->m_LANG_determined_type.value();
            auto container_determined_base_type =
                container_type_instance->get_determined_type();
            auto* indexer_type_instance = node->m_index->m_LANG_determined_type.value();
            auto indexer_determined_base_type =
                indexer_type_instance->get_determined_type();

            if (!container_determined_base_type)
            {
                lex.record_lang_error(lexer::msglevel_t::error, node->m_container,
                    WO_ERR_TYPE_NAMED_DETERMINED_FAILED,
                    get_type_name_w(container_type_instance));

                return FAILED;
            }
            if (!indexer_determined_base_type)
            {
                lex.record_lang_error(lexer::msglevel_t::error, node->m_index,
                    WO_ERR_TYPE_NAMED_DETERMINED_FAILED,
                    get_type_name_w(indexer_type_instance));

                return FAILED;
            }

            auto* container_determined_base_type_instance =
                container_determined_base_type.value();
            auto* indexer_determined_base_type_instance =
                indexer_determined_base_type.value();

            lang_TypeInstance* index_raw_result;
            switch (container_determined_base_type_instance->m_base_type)
            {
            case lang_TypeInstance::DeterminedType::ARRAY:
            case lang_TypeInstance::DeterminedType::VECTOR:
            {
                if (indexer_determined_base_type_instance->m_base_type
                    != lang_TypeInstance::DeterminedType::INTEGER)
                {
                    lex.record_lang_error(lexer::msglevel_t::error, node->m_index,
                        WO_ERR_CANNOT_INDEX_TYPE_WITH_TYPE,
                        get_type_name_w(container_type_instance),
                        get_type_name_w(indexer_type_instance));
                    return FAILED;
                }
                index_raw_result =
                    container_determined_base_type_instance
                    ->m_external_type_description.m_array_or_vector
                    ->m_element_type;
                break;
            }
            case lang_TypeInstance::DeterminedType::DICTIONARY:
            case lang_TypeInstance::DeterminedType::MAPPING:
            {
                if (container_determined_base_type_instance
                    ->m_external_type_description.m_dictionary_or_mapping
                    ->m_key_type != indexer_type_instance)
                {
                    lex.record_lang_error(lexer::msglevel_t::error, node->m_index,
                        WO_ERR_CANNOT_INDEX_TYPE_WITH_TYPE,
                        get_type_name_w(container_type_instance),
                        get_type_name_w(indexer_type_instance));
                    return FAILED;
                }
                index_raw_result =
                    container_determined_base_type_instance
                    ->m_external_type_description.m_dictionary_or_mapping
                    ->m_value_type;
                break;
            }
            case lang_TypeInstance::DeterminedType::STRUCT:
            {
                if (indexer_determined_base_type_instance->m_base_type
                    != lang_TypeInstance::DeterminedType::STRING)
                {
                    lex.record_lang_error(lexer::msglevel_t::error, node->m_index,
                        WO_ERR_CANNOT_INDEX_TYPE_WITH_TYPE,
                        get_type_name_w(container_type_instance),
                        get_type_name_w(indexer_type_instance));
                    return FAILED;
                }
                if (!node->m_index->m_evaled_const_value.has_value())
                {
                    lex.record_lang_error(lexer::msglevel_t::error, node->m_index,
                        WO_ERR_CANNOT_INDEX_STRUCT_WITH_NON_CONST);
                    return FAILED;
                }

                wo_assert(node->m_index->m_evaled_const_value.value().type
                    == value::valuetype::string_type);

                wo_pstring_t member_name = wo::wstring_pool::get_pstr(
                    str_to_wstr(*node->m_index->m_evaled_const_value.value().string));

                auto* struct_type = container_determined_base_type_instance
                    ->m_external_type_description.m_struct;

                auto fnd = struct_type->m_member_types.find(member_name);
                if (fnd == struct_type->m_member_types.end())
                {
                    lex.record_lang_error(lexer::msglevel_t::error, node->m_index,
                        WO_ERR_STRUCT_DONOT_HAVE_MAMBER_NAMED,
                        get_type_name_w(container_type_instance),
                        member_name->c_str());
                    return FAILED;
                }

                if (!check_struct_field_is_reachable_in_current_scope(
                    lex,
                    node,
                    container_type_instance->m_symbol,
                    fnd->second.m_attrib,
                    fnd->first,
                    node->source_location.source_file))
                {
                    return FAILED;
                }

                index_raw_result = fnd->second.m_member_type;
                node->m_LANG_fast_index_for_struct = fnd->second.m_offset;

                break;
            }
            case lang_TypeInstance::DeterminedType::TUPLE:
            {
                if (indexer_determined_base_type_instance->m_base_type
                    != lang_TypeInstance::DeterminedType::INTEGER)
                {
                    lex.record_lang_error(lexer::msglevel_t::error, node->m_index,
                        WO_ERR_CANNOT_INDEX_TYPE_WITH_TYPE,
                        get_type_name_w(container_type_instance),
                        get_type_name_w(indexer_type_instance));
                    return FAILED;
                }
                if (!node->m_index->m_evaled_const_value.has_value())
                {
                    lex.record_lang_error(lexer::msglevel_t::error, node->m_index,
                        WO_ERR_CANNOT_INDEX_TUPLE_WITH_NON_CONST);
                    return FAILED;
                }

                wo_assert(node->m_index->m_evaled_const_value.value().type
                    == value::valuetype::integer_type);

                auto index = node->m_index->m_evaled_const_value.value().integer;
                auto* tuple_type = container_determined_base_type_instance
                    ->m_external_type_description.m_tuple;

                if (index < 0 || (size_t)index >= tuple_type->m_element_types.size())
                {
                    lex.record_lang_error(lexer::msglevel_t::error, node->m_index,
                        WO_ERR_TUPLE_INDEX_OUT_OF_RANGE,
                        get_type_name_w(container_type_instance),
                        tuple_type->m_element_types.size(),
                        index);
                    return FAILED;
                }

                index_raw_result = tuple_type->m_element_types[index];
                node->m_LANG_fast_index_for_struct = index;

                break;
            }
            case lang_TypeInstance::DeterminedType::STRING:
            {
                if (indexer_determined_base_type_instance->m_base_type
                    != lang_TypeInstance::DeterminedType::INTEGER)
                {
                    lex.record_lang_error(lexer::msglevel_t::error, node->m_index,
                        WO_ERR_CANNOT_INDEX_TYPE_WITH_TYPE,
                        get_type_name_w(container_type_instance),
                        get_type_name_w(indexer_type_instance));
                    return FAILED;
                }
                index_raw_result = m_origin_types.m_char.m_type_instance;

                if (node->m_container->m_evaled_const_value.has_value()
                    && node->m_index->m_evaled_const_value.has_value())
                {
                    auto* string_instance = node->m_container->m_evaled_const_value.value().string;
                    auto index = node->m_index->m_evaled_const_value.value().integer;

                    wo_char_t ch = wo::u8strnidx(
                        string_instance->c_str(), string_instance->size(), (size_t)index);
                    if (ch == 0
                        && wo::u8strnlen(
                            string_instance->c_str(), string_instance->size()) <= (size_t)index)
                    {
                        lex.record_lang_error(lexer::msglevel_t::error, node->m_index,
                            WO_ERR_STRING_INDEX_OUT_OF_RANGE);
                        return FAILED;
                    }

                    wo::value const_result_value;
                    const_result_value.set_integer(ch);

                    node->decide_final_constant_value(const_result_value);
                }
                break;
            }
            default:
                lex.record_lang_error(lexer::msglevel_t::error, node->m_container,
                    WO_ERR_UNINDEXABLE_TYPE_NAMED,
                    get_type_name_w(container_type_instance));

                return FAILED;
            }

            node->m_LANG_result_is_mutable = index_raw_result->is_mutable();
            node->m_LANG_determined_type = immutable_type(index_raw_result);
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstFakeValueUnpack)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_unpack_value);
            return HOLD;
        }
        else if (state == HOLD)
        {
            auto* unpack_value_type_instance =
                node->m_unpack_value->m_LANG_determined_type.value();
            auto unpack_value_type_determined_base_type =
                unpack_value_type_instance->get_determined_type();

            if (!unpack_value_type_determined_base_type)
            {
                lex.record_lang_error(lexer::msglevel_t::error, node->m_unpack_value,
                    WO_ERR_TYPE_NAMED_DETERMINED_FAILED,
                    get_type_name_w(unpack_value_type_instance));
                return FAILED;
            }

            auto* unpack_value_determined_base_type_instance =
                unpack_value_type_determined_base_type.value();

            switch (unpack_value_determined_base_type_instance->m_base_type)
            {
            case lang_TypeInstance::DeterminedType::ARRAY:
            case lang_TypeInstance::DeterminedType::VECTOR:
            case lang_TypeInstance::DeterminedType::TUPLE:
                break;
            default:
                lex.record_lang_error(lexer::msglevel_t::error, node->m_unpack_value,
                    WO_ERR_ONLY_EXPAND_ARRAY_VEC_AND_TUPLE,
                    get_type_name_w(unpack_value_type_instance));
                return FAILED;
            }

            node->m_LANG_determined_type = unpack_value_type_instance;
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueMakeUnion)
    {
        if (state == UNPROCESSED)
        {
            if (node->m_packed_value)
                WO_CONTINUE_PROCESS(node->m_packed_value.value());

            return HOLD;
        }
        else if (state == HOLD)
        {
            // Trick to make compiler happy~.
            node->m_LANG_determined_type =
                m_origin_types.m_nothing.m_type_instance;
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstEnumDeclare)
    {
        if (state == UNPROCESSED)
        {
            node->m_LANG_hold_state = AstEnumDeclare::HOLD_FOR_ENUM_TYPE_DECL;
            WO_CONTINUE_PROCESS(node->m_enum_type_declare);

            return HOLD;
        }
        else if (state == HOLD)
        {
            switch (node->m_LANG_hold_state)
            {
            case AstEnumDeclare::HOLD_FOR_ENUM_TYPE_DECL:
            {
                node->m_LANG_hold_state = AstEnumDeclare::HOLD_FOR_ENUM_ITEMS_DECL;
                WO_CONTINUE_PROCESS(node->m_enum_body);

                return HOLD;
            }
            case AstEnumDeclare::HOLD_FOR_ENUM_ITEMS_DECL:
                break;
            default:
                wo_error("unknown hold state");
                break;
            }
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstPatternVariable)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_variable);
            return HOLD;
        }
        else if (state == HOLD)
        {
            lang_ValueInstance* variable_instance =
                node->m_variable->m_LANG_variable_instance.value();

            if (!variable_instance->m_mutable)
            {
                lex.record_lang_error(lexer::msglevel_t::error, node,
                    WO_ERR_PATTERN_VARIABLE_SHOULD_BE_MUTABLE);
                return FAILED;
            }
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstPatternIndex)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_index);
            return HOLD;
        }
        else if (state == HOLD)
        {
            lang_TypeInstance* index_result_type = node->m_index->m_LANG_determined_type.value();
            if (!node->m_index->m_LANG_result_is_mutable)
            {
                lex.record_lang_error(lexer::msglevel_t::error, node->m_index,
                    WO_ERR_PATTERN_INDEX_SHOULD_BE_MUTABLE_TYPE);
                return FAILED;
            }
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueTypeCast)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_cast_type);
            WO_CONTINUE_PROCESS(node->m_cast_value);
            return HOLD;
        }
        else if (state == HOLD)
        {
            auto* target_type = node->m_cast_type->m_LANG_determined_type.value();
            auto* casting_value_type = node->m_cast_value->m_LANG_determined_type.value();

            // Check symbol can be reach.
            if (!check_symbol_is_reachable_in_current_scope(
                lex,
                node,
                target_type->m_symbol,
                node->source_location.source_file,
                !node->duplicated_node /* TMP: Skip import check in template function. */))
            {
                return FAILED;
            }

            if (lang_TypeInstance::TypeCheckResult::ACCEPT
                != check_cast_able(lex, node, target_type, casting_value_type))
            {
                lex.record_lang_error(lexer::msglevel_t::error, node,
                    WO_ERR_CANNOT_CAST_TYPE_TO_TYPE,
                    get_type_name_w(casting_value_type),
                    get_type_name_w(target_type));

                return FAILED;
            }

            node->m_LANG_determined_type = target_type;

            if (node->m_cast_value->m_evaled_const_value.has_value())
            {
                auto& cast_from_const = node->m_cast_value->m_evaled_const_value.value();

                // ATTENTION: If constant value evaled and can pass cast check, 
                //  I think we can get determined type from the constant value.
                //  But, I can't prove it's right.
                auto* cast_from_determined_type = casting_value_type->get_determined_type().value();
                auto* cast_target_determined_type = target_type->get_determined_type().value();

                if (cast_from_determined_type->m_base_type != cast_target_determined_type->m_base_type)
                {
                    bool casted = true;
                    wo::value casted_value;
                    switch (cast_target_determined_type->m_base_type)
                    {
                    case lang_TypeInstance::DeterminedType::INTEGER:
                        casted_value.set_integer(wo_cast_int(std::launder(reinterpret_cast<wo_value>(&cast_from_const))));
                        break;
                    case lang_TypeInstance::DeterminedType::REAL:
                        casted_value.set_real(wo_cast_real(std::launder(reinterpret_cast<wo_value>(&cast_from_const))));
                        break;
                    case lang_TypeInstance::DeterminedType::HANDLE:
                        casted_value.set_handle(wo_cast_handle(std::launder(reinterpret_cast<wo_value>(&cast_from_const))));
                        break;
                    case lang_TypeInstance::DeterminedType::BOOLEAN:
                        casted_value.set_bool(
                            wo_cast_bool(std::launder(reinterpret_cast<wo_value>(&cast_from_const)))
                            == WO_TRUE
                            ? true
                            : false);
                        break;
                    case lang_TypeInstance::DeterminedType::STRING:
                        casted_value.set_string(
                            wo_cast_string(std::launder(reinterpret_cast<wo_value>(&cast_from_const))));
                        break;
                    default:
                        // Cannot cast to constant.
                        casted = false;
                        break;
                    }

                    if (casted)
                        node->decide_final_constant_value(casted_value);
                }
                else
                    node->decide_final_constant_value(cast_from_const);
            }
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueDoAsVoid)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_do_value);
            return HOLD;
        }
        else if (state == HOLD)
        {
            node->m_LANG_determined_type
                = m_origin_types.m_void.m_type_instance;
        }

        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueFunctionCall_FakeAstArgumentDeductionContextA)
    {
        if (state == UNPROCESSED)
        {
            entry_spcify_scope(node->m_apply_template_argument_scope);

            for (auto& [param_name, arg_type_inst] : node->m_deduction_results)
            {
                bool success_defined =
                    fast_create_one_template_type_alias_and_constant_in_current_scope(
                        param_name, arg_type_inst);

                (void)success_defined;
                wo_assert(success_defined);

                auto fnd = std::find_if(
                    node->m_undetermined_template_params.begin(),
                    node->m_undetermined_template_params.end(),
                    [param_name = param_name](ast::AstTemplateParam* param) {
                        return param->m_param_name == param_name;
                    });

                if (fnd != node->m_undetermined_template_params.end())
                    node->m_undetermined_template_params.erase(fnd);
            }

            node->m_LANG_hold_state =
                AstValueFunctionCall_FakeAstArgumentDeductionContextA::HOLD_FOR_PREPARE;

            return HOLD;
        }
        else if (state == HOLD)
        {
            switch (node->m_LANG_hold_state)
            {
            case AstValueFunctionCall_FakeAstArgumentDeductionContextA::HOLD_FOR_PREPARE:
            {
                if (node->m_arguments_tobe_deduct.empty())
                {
                    // All arguments have been deduced.
                    end_last_scope();
                    break;
                }

                node->m_current_argument = node->m_arguments_tobe_deduct.begin();
                node->m_LANG_hold_state = AstValueFunctionCall_FakeAstArgumentDeductionContextA::HOLD_FOR_EVAL_PARAM_TYPE;

                return HOLD;
            }
            case AstValueFunctionCall_FakeAstArgumentDeductionContextA::HOLD_FOR_EVAL_PARAM_TYPE:
            {
                AstValueFunctionCall_FakeAstArgumentDeductionContextA::ArgumentMatch& current_argument =
                    *node->m_current_argument;

                if (!check_type_may_dependence_template_parameters(
                    current_argument.m_duped_param_type, node->m_undetermined_template_params))
                {
                    // Greate! we can update the type now.
                    WO_CONTINUE_PROCESS(current_argument.m_duped_param_type);
                }
                else
                {
                    if (current_argument.m_duped_param_type->m_formal != AstTypeHolder::FUNCTION)
                    {
                        node->m_LANG_hold_state =
                            AstValueFunctionCall_FakeAstArgumentDeductionContextA::HOLD_TO_NEXT_ARGUMENT;
                        return HOLD;
                    }

                    std::list<AstTypeHolder*> type_to_eval;

                    auto& function_formal = current_argument.m_duped_param_type->m_typeform.m_function;
                    for (auto* param : function_formal.m_parameters)
                    {
                        if (!check_type_may_dependence_template_parameters(
                            param, node->m_undetermined_template_params))
                            type_to_eval.push_back(param);
                    }
                    if (!check_type_may_dependence_template_parameters(
                        function_formal.m_return_type, node->m_undetermined_template_params))
                        type_to_eval.push_back(function_formal.m_return_type);

                    WO_CONTINUE_PROCESS_LIST(type_to_eval);
                }
                node->m_LANG_hold_state =
                    AstValueFunctionCall_FakeAstArgumentDeductionContextA::HOLD_FOR_DEDUCT_ARGUMENT;
                return HOLD;
            }
            case AstValueFunctionCall_FakeAstArgumentDeductionContextA::HOLD_FOR_DEDUCT_ARGUMENT:
            {
                AstValueFunctionCall_FakeAstArgumentDeductionContextA::ArgumentMatch& current_argument =
                    *node->m_current_argument;

                std::list<std::optional<lang_TypeInstance*>> param_argument_types;
                std::optional<lang_TypeInstance*> param_return_type = std::nullopt;

                if (current_argument.m_duped_param_type->m_formal == AstTypeHolder::FUNCTION)
                {
                    // That's good!
                    for (auto* param_type_holder :
                        current_argument.m_duped_param_type->m_typeform.m_function.m_parameters)
                    {
                        param_argument_types.push_back(param_type_holder->m_LANG_determined_type);
                    }
                    param_return_type =
                        current_argument.m_duped_param_type->m_typeform.m_function.m_return_type->m_LANG_determined_type;
                }
                else
                {
                    if (current_argument.m_duped_param_type->m_LANG_determined_type.has_value())
                    {
                        lang_TypeInstance* determined_type =
                            current_argument.m_duped_param_type->m_LANG_determined_type.value();
                        auto determined_base_type = determined_type->get_determined_type();
                        if (determined_base_type.has_value())
                        {
                            auto* determined_base_type_instance = determined_base_type.value();
                            if (determined_base_type_instance->m_base_type == lang_TypeInstance::DeterminedType::FUNCTION)
                            {
                                for (auto* param_type :
                                    determined_base_type_instance->m_external_type_description.m_function->m_param_types)
                                {
                                    param_argument_types.push_back(param_type);
                                }
                                param_return_type =
                                    determined_base_type_instance->m_external_type_description.m_function->m_return_type;
                            }
                        }
                    }
                }

                if (param_argument_types.empty() && !param_return_type.has_value())
                    node->m_LANG_hold_state =
                    AstValueFunctionCall_FakeAstArgumentDeductionContextA::HOLD_TO_NEXT_ARGUMENT;
                else
                {
                    // Begin our main work!
                    auto* argument = get_marked_origin_value_node(current_argument.m_argument);

                    switch (argument->node_type)
                    {
                    case AstBase::AST_VALUE_VARIABLE:
                    {
                        AstValueVariable* argument_variable = static_cast<AstValueVariable*>(argument);
                        lang_Symbol* symbol = argument_variable->m_identifier->m_LANG_determined_symbol.value();

                        wo_assert(symbol->m_is_template
                            && symbol->m_symbol_kind == lang_Symbol::kind::VARIABLE
                            && !symbol->m_template_value_instances->m_mutable);

                        auto& template_instance_prefab = symbol->m_template_value_instances;

                        std::unordered_map<wo_pstring_t, ast::AstIdentifier::TemplateArgumentInstance> deduction_results;
                        std::list<ast::AstTemplateParam*> pending_template_params;
                        auto it_template_param = symbol->m_template_value_instances->m_template_params.begin();
                        auto it_template_param_end = symbol->m_template_value_instances->m_template_params.end();
                        if (argument_variable->m_identifier->m_template_arguments.has_value())
                        {
                            for (auto& _useless : argument_variable->m_identifier->m_template_arguments.value())
                            {
                                (void)_useless;
                                ++it_template_param;
                            }
                        }
                        for (; it_template_param != it_template_param_end; ++it_template_param)
                            pending_template_params.push_back(*it_template_param);

                        entry_spcify_scope(symbol->m_belongs_to_scope);

                        template_function_deduction_extraction_with_complete_type(
                            lex,
                            static_cast<AstValueFunction*>(symbol->m_template_value_instances->m_origin_value_ast),
                            param_argument_types,
                            param_return_type,
                            pending_template_params,
                            &deduction_results);

                        end_last_scope();

                        if (deduction_results.size() == pending_template_params.size())
                        {
                            // We can decided this argument now.
                            std::list<ast::AstIdentifier::TemplateArgumentInstance> template_arguments;
                            for (ast::AstTemplateParam* param : pending_template_params)
                            {
                                template_arguments.push_back(deduction_results.at(param->m_param_name));
                            }

                            argument_variable->m_identifier->m_LANG_determined_and_appended_template_arguments
                                = template_arguments;

                            entry_spcify_scope(node->m_scope_before_deduction);

                            WO_CONTINUE_PROCESS(current_argument.m_argument);

                            node->m_LANG_hold_state =
                                AstValueFunctionCall_FakeAstArgumentDeductionContextA::HOLD_FOR_REVERSE_DEDUCT;
                        }
                        else
                            node->m_LANG_hold_state =
                            AstValueFunctionCall_FakeAstArgumentDeductionContextA::HOLD_TO_NEXT_ARGUMENT;

                        break;
                    }
                    case AstBase::AST_VALUE_FUNCTION:
                    {
                        AstValueFunction* argument_function = static_cast<AstValueFunction*>(argument);
                        auto& pending_template_arguments = argument_function->m_pending_param_type_mark_template.value();

                        std::unordered_map<wo_pstring_t, ast::AstIdentifier::TemplateArgumentInstance> deduction_results;
                        template_function_deduction_extraction_with_complete_type(
                            lex,
                            argument_function,
                            param_argument_types,
                            param_return_type,
                            pending_template_arguments,
                            &deduction_results);

                        if (deduction_results.size() == pending_template_arguments.size())
                        {
                            // We can decided this argument now.
                            std::list<ast::AstIdentifier::TemplateArgumentInstance> template_arguments;
                            for (ast::AstTemplateParam* param : pending_template_arguments)
                            {
                                template_arguments.push_back(deduction_results.at(param->m_param_name));
                            }

                            argument_function->m_LANG_determined_template_arguments = template_arguments;
                            argument_function->m_LANG_in_template_reification_context = true;

                            entry_spcify_scope(node->m_scope_before_deduction);

                            WO_CONTINUE_PROCESS(current_argument.m_argument);

                            node->m_LANG_hold_state =
                                AstValueFunctionCall_FakeAstArgumentDeductionContextA::HOLD_FOR_REVERSE_DEDUCT;
                        }
                        else
                            node->m_LANG_hold_state =
                            AstValueFunctionCall_FakeAstArgumentDeductionContextA::HOLD_TO_NEXT_ARGUMENT;

                        break;
                    }
                    default:
                        wo_error("Unknown argument type.");
                    }
                }

                return HOLD;
            }
            case AstValueFunctionCall_FakeAstArgumentDeductionContextA::HOLD_FOR_REVERSE_DEDUCT:
            {
                end_last_scope(); // End the scope.

                AstValueFunctionCall_FakeAstArgumentDeductionContextA::ArgumentMatch& current_argument =
                    *node->m_current_argument;

                lang_TypeInstance* argument_final_type =
                    current_argument.m_argument->m_LANG_determined_type.value();

                if (!template_arguments_deduction_extraction_with_type(
                    lex,
                    current_argument.m_duped_param_type,
                    argument_final_type,
                    node->m_undetermined_template_params,
                    &node->m_deduction_results))
                {
                    // Error happend in deduction.
                    end_last_scope(); // End the scope.

                    return FAILED;
                }

                // Now we make progress, we can do more things.
                node->m_arguments_tobe_deduct.erase(node->m_current_argument);
                node->m_LANG_hold_state =
                    AstValueFunctionCall_FakeAstArgumentDeductionContextA::HOLD_FOR_PREPARE;

                return HOLD;
            }
            case AstValueFunctionCall_FakeAstArgumentDeductionContextA::HOLD_TO_NEXT_ARGUMENT:
            {
                if (++node->m_current_argument == node->m_arguments_tobe_deduct.end())
                {
                    // Failed...
                    lex.record_lang_error(lexer::msglevel_t::error, node->m_arguments_tobe_deduct.front().m_argument,
                        WO_ERR_FAILED_TO_DEDUCE_TEMPLATE_TYPE);

                    end_last_scope(); // End the scope.

                    return FAILED;
                }
                node->m_LANG_hold_state =
                    AstValueFunctionCall_FakeAstArgumentDeductionContextA::HOLD_FOR_EVAL_PARAM_TYPE;

                return HOLD;
            }
            }
        }
        else
        {
            if (node->m_LANG_hold_state == AstValueFunctionCall_FakeAstArgumentDeductionContextA::HOLD_FOR_REVERSE_DEDUCT)
                end_last_scope();

            end_last_scope(); // Child failed, and end the scope.
        }

        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueFunctionCall_FakeAstArgumentDeductionContextB)
    {
        if (state == UNPROCESSED)
        {
            node->m_LANG_hold_state =
                AstValueFunctionCall_FakeAstArgumentDeductionContextB::HOLD_FOR_DEDUCE;
            return HOLD;
        }
        else if (state == HOLD)
        {
            switch (node->m_LANG_hold_state)
            {
            case AstValueFunctionCall_FakeAstArgumentDeductionContextB::HOLD_FOR_DEDUCE:
            {
                if (node->m_arguments_tobe_deduct.empty())
                {
                    // All arguments have been deduced.
                    break;
                }
                auto& param_and_argument_pair = node->m_arguments_tobe_deduct.front();

                lang_TypeInstance* param_type = param_and_argument_pair.m_param_type;

                std::list<std::optional<lang_TypeInstance*>> param_argument_types;
                std::optional<lang_TypeInstance*> param_return_type = std::nullopt;

                auto param_type_determined_base_type = param_type->get_determined_type();
                if (!param_type_determined_base_type.has_value())
                {
                    lex.record_lang_error(lexer::msglevel_t::error, param_and_argument_pair.m_argument,
                        WO_ERR_TYPE_NAMED_DETERMINED_FAILED,
                        get_type_name_w(param_type));
                    return FAILED;
                }

                auto* param_type_determined_base_type_instance = param_type_determined_base_type.value();
                if (param_type_determined_base_type_instance->m_base_type != lang_TypeInstance::DeterminedType::FUNCTION)
                {
                    lex.record_lang_error(lexer::msglevel_t::error, param_and_argument_pair.m_argument,
                        WO_ERR_FAILED_TO_DEDUCE_TEMPLATE_TYPE);
                    return FAILED;
                }

                auto* param_type_determined_function_base_type =
                    param_type_determined_base_type_instance->m_external_type_description.m_function;

                for (auto& param_type : param_type_determined_function_base_type->m_param_types)
                    param_argument_types.push_back(param_type);
                param_return_type = param_type_determined_function_base_type->m_return_type;

                AstValueBase* argument = get_marked_origin_value_node(param_and_argument_pair.m_argument);

                switch (argument->node_type)
                {
                case AstBase::AST_VALUE_FUNCTION:
                {
                    AstValueFunction* argument_function = static_cast<AstValueFunction*>(argument);
                    auto& pending_template_arguments = argument_function->m_pending_param_type_mark_template.value();

                    std::unordered_map<wo_pstring_t, ast::AstIdentifier::TemplateArgumentInstance> deduction_results;
                    template_function_deduction_extraction_with_complete_type(
                        lex,
                        argument_function,
                        param_argument_types,
                        param_return_type,
                        pending_template_arguments,
                        &deduction_results);

                    if (deduction_results.size() == pending_template_arguments.size())
                    {
                        // We can decided this argument now.
                        std::list<ast::AstIdentifier::TemplateArgumentInstance> template_arguments;
                        for (ast::AstTemplateParam* param : pending_template_arguments)
                        {
                            template_arguments.push_back(deduction_results.at(param->m_param_name));
                        }

                        argument_function->m_LANG_determined_template_arguments = template_arguments;
                        argument_function->m_LANG_in_template_reification_context = true;
                        WO_CONTINUE_PROCESS(param_and_argument_pair.m_argument);
                    }
                    else
                    {
                        lex.record_lang_error(lexer::msglevel_t::error, argument,
                            WO_ERR_FAILED_TO_DEDUCE_TEMPLATE_TYPE);
                        return FAILED;
                    }
                    break;
                }
                case AstBase::AST_VALUE_VARIABLE:
                {
                    AstValueVariable* argument_variable = static_cast<AstValueVariable*>(argument);
                    lang_Symbol* symbol = argument_variable->m_identifier->m_LANG_determined_symbol.value();

                    wo_assert(symbol->m_is_template
                        && symbol->m_symbol_kind == lang_Symbol::kind::VARIABLE
                        && !symbol->m_template_value_instances->m_mutable);

                    auto& template_instance_prefab = symbol->m_template_value_instances;

                    std::unordered_map<wo_pstring_t, ast::AstIdentifier::TemplateArgumentInstance> deduction_results;
                    std::list<ast::AstTemplateParam*> pending_template_params;
                    auto it_template_param = symbol->m_template_value_instances->m_template_params.begin();
                    auto it_template_param_end = symbol->m_template_value_instances->m_template_params.end();
                    if (argument_variable->m_identifier->m_template_arguments.has_value())
                    {
                        for (auto& _useless : argument_variable->m_identifier->m_template_arguments.value())
                        {
                            (void)_useless;
                            ++it_template_param;
                        }
                    }
                    for (; it_template_param != it_template_param_end; ++it_template_param)
                        pending_template_params.push_back(*it_template_param);

                    entry_spcify_scope(symbol->m_belongs_to_scope);

                    template_function_deduction_extraction_with_complete_type(
                        lex,
                        static_cast<AstValueFunction*>(symbol->m_template_value_instances->m_origin_value_ast),
                        param_argument_types,
                        param_return_type,
                        pending_template_params,
                        &deduction_results);

                    end_last_scope();

                    if (deduction_results.size() == pending_template_params.size())
                    {
                        // We can decided this argument now.
                        std::list<ast::AstIdentifier::TemplateArgumentInstance> template_arguments;
                        for (ast::AstTemplateParam* param : pending_template_params)
                        {
                            template_arguments.push_back(deduction_results.at(param->m_param_name));
                        }

                        argument_variable->m_identifier->m_LANG_determined_and_appended_template_arguments
                            = template_arguments;

                        WO_CONTINUE_PROCESS(param_and_argument_pair.m_argument);
                    }
                    else
                    {
                        lex.record_lang_error(lexer::msglevel_t::error, argument,
                            WO_ERR_FAILED_TO_DEDUCE_TEMPLATE_TYPE);
                        return FAILED;
                    }

                    break;
                }
                default:
                    wo_error("Unknown argument type.");
                }

                node->m_arguments_tobe_deduct.pop_front();
                return HOLD;
            }
            default:
                wo_error("unknown hold state");
            }
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueFunctionCall)
    {
        // Huston, we have a problem, again.

        if (state == UNPROCESSED)
        {
            if (node->m_is_direct_call)
                // direct call(-> |> <|). first argument must be eval first;
                WO_CONTINUE_PROCESS(node->m_arguments.front());

            if (node->m_function->node_type == AstBase::AST_VALUE_VARIABLE)
            {
                // Eval from type first;
                AstValueVariable* invoking_variable = static_cast<AstValueVariable*>(node->m_function);
                if (invoking_variable->m_identifier->m_formal == AstIdentifier::identifier_formal::FROM_TYPE)
                {
                    AstTypeHolder** type_holder = std::get_if<AstTypeHolder*>(
                        &invoking_variable->m_identifier->m_from_type.value());

                    if (type_holder != nullptr)
                        WO_CONTINUE_PROCESS(*type_holder);
                }
            }
            for (auto& argument : node->m_arguments)
            {
                auto* origin_argument = get_marked_origin_value_node(argument);
                if (origin_argument->node_type == AstBase::AST_VALUE_VARIABLE)
                {
                    // Eval from type first;
                    AstValueVariable* argument_variabl = static_cast<AstValueVariable*>(origin_argument);
                    if (argument_variabl->m_identifier->m_formal == AstIdentifier::identifier_formal::FROM_TYPE)
                    {
                        AstTypeHolder** type_holder = std::get_if<AstTypeHolder*>(
                            &argument_variabl->m_identifier->m_from_type.value());

                        if (type_holder != nullptr)
                            WO_CONTINUE_PROCESS(*type_holder);
                    }
                }
            }

            node->m_LANG_hold_state = AstValueFunctionCall::HOLD_FOR_FIRST_ARGUMENT_EVAL;
            return HOLD;
        }
        else if (state == HOLD)
        {
            switch (node->m_LANG_hold_state)
            {
            case AstValueFunctionCall::HOLD_FOR_FIRST_ARGUMENT_EVAL:
            {
                if (node->m_is_direct_call)
                {
                    AstValueBase* first_argument = node->m_arguments.front();

                    if (first_argument->node_type != AstBase::AST_FAKE_VALUE_UNPACK
                        && node->m_function->node_type == AstBase::AST_VALUE_VARIABLE)
                    {
                        lang_TypeInstance* first_argument_type_instance =
                            first_argument->m_LANG_determined_type.value();
                        lang_Symbol* first_argument_type_symbol =
                            first_argument_type_instance->m_symbol;

                        AstValueVariable* function_variable = static_cast<AstValueVariable*>(node->m_function);
                        AstIdentifier* function_variable_identifier = function_variable->m_identifier;

                        if (first_argument_type_symbol->m_belongs_to_scope->is_namespace_scope())
                        {
                            if (function_variable_identifier->m_formal == AstIdentifier::identifier_formal::FROM_CURRENT)
                            {
                                function_variable_identifier->m_formal = AstIdentifier::identifier_formal::FROM_TYPE;
                                function_variable_identifier->m_from_type = first_argument_type_instance;

                                wo_assert(!function_variable_identifier->m_find_type_only);
                                if (!find_symbol_in_current_scope(
                                    lex, function_variable_identifier, std::nullopt))
                                {
                                    // Failed, restore.
                                    function_variable_identifier->m_formal = AstIdentifier::identifier_formal::FROM_CURRENT;
                                    function_variable_identifier->m_from_type = std::nullopt;
                                }
                            }
                        }
                    }
                }

                node->m_LANG_target_function_need_deduct_template_arguments =
                    check_need_template_deduct_function(lex, node->m_function, out_stack);

                if (!node->m_LANG_target_function_need_deduct_template_arguments)
                {
                    // TARGET FUNCTION HAS NO TEMPLATE PARAM TO BE DEDUCT, EVAL IT NOW.
                    WO_CONTINUE_PROCESS(node->m_function);
                }

                node->m_LANG_hold_state = AstValueFunctionCall::HOLD_FOR_FUNCTION_EVAL;
                return HOLD;
            }
            case AstValueFunctionCall::HOLD_FOR_FUNCTION_EVAL:
            {
                // Eval all arguments beside AstValueFunction or AstValueVariable which refer to
               // uncomplete template function.
                std::list<AstValueBase*> arguments;
                for (auto* argument_value : node->m_arguments)
                {
                    if (check_need_template_deduct_function(lex, argument_value, out_stack))
                        continue;

                    arguments.push_back(argument_value);
                }

                WO_CONTINUE_PROCESS_LIST(arguments);

                node->m_LANG_hold_state = AstValueFunctionCall::HOLD_FOR_ARGUMENTS_EVAL;
                return HOLD;
            }
            case AstValueFunctionCall::HOLD_FOR_ARGUMENTS_EVAL:
            {
                // If target function is a template function, we need to deduce template arguments.
                // HERE!
                auto* current_scope_before_deduction = get_current_scope();

                if (node->m_LANG_target_function_need_deduct_template_arguments)
                {
                    std::unordered_map<wo_pstring_t, ast::AstIdentifier::TemplateArgumentInstance> deduction_results;

                    std::list<ast::AstTemplateParam*> pending_template_params;
                    std::list<AstTypeHolder*> target_param_holders;

                    bool entry_function_located_scope = false;

                    lang_Scope* target_function_scope;
                    lang_Scope* current_scope = get_current_scope();

                    switch (node->m_function->node_type)
                    {
                    case AstBase::AST_VALUE_VARIABLE:
                    {
                        AstValueVariable* function_variable = static_cast<AstValueVariable*>(node->m_function);
                        lang_Symbol* function_symbol = function_variable->m_identifier->m_LANG_determined_symbol.value();

                        target_function_scope = function_symbol->m_belongs_to_scope;

                        wo_assert(function_symbol->m_is_template
                            && function_symbol->m_symbol_kind == lang_Symbol::kind::VARIABLE);

                        auto it_template_param = function_symbol->m_template_value_instances->m_template_params.begin();
                        auto it_template_param_end = function_symbol->m_template_value_instances->m_template_params.end();
                        if (function_variable->m_identifier->m_template_arguments.has_value())
                        {
                            for (auto& template_arg : function_variable->m_identifier->m_template_arguments.value())
                            {
                                if (it_template_param == it_template_param_end)
                                    // Too much template arguments.
                                    break;

                                wo_pstring_t param_name = (*(it_template_param++))->m_param_name;

                                if (template_arg->is_type())
                                    deduction_results.insert(
                                        std::make_pair(
                                            param_name, template_arg->get_type()->m_LANG_determined_type.value()));
                                else
                                    deduction_results.insert(
                                        std::make_pair(
                                            param_name, template_arg->get_constant()));
                            }
                        }
                        for (; it_template_param != it_template_param_end; ++it_template_param)
                            pending_template_params.push_back(*it_template_param);

                        // NOTE: node type has been checked in HOLD_FOR_FIRST_ARGUMENT_EVAL->HOLD_FOR_FUNCTION_EVAL
                        wo_assert(function_symbol->m_template_value_instances->m_origin_value_ast->node_type
                            == AstBase::AST_VALUE_FUNCTION);
                        AstValueFunction* origin_ast_value_function = static_cast<AstValueFunction*>(
                            function_symbol->m_template_value_instances->m_origin_value_ast);

                        for (auto* template_param_declare : origin_ast_value_function->m_parameters)
                            target_param_holders.push_back(template_param_declare->m_type.value());

                        // Entry function located scope, template_type_deduction_extraction_with_complete_type require todo so.
                        entry_function_located_scope = true;
                        entry_spcify_scope(function_symbol->m_belongs_to_scope);

                        break;
                    }
                    case AstBase::AST_VALUE_FUNCTION:
                    {
                        AstValueFunction* function = static_cast<AstValueFunction*>(node->m_function);

                        target_function_scope = current_scope;
                        pending_template_params = function->m_pending_param_type_mark_template.value();

                        for (auto* template_param_declare : function->m_parameters)
                            target_param_holders.push_back(template_param_declare->m_type.value());

                        break;
                    }
                    default:
                        wo_error("Unexpected template node type.");
                    }

                    entry_spcify_scope(target_function_scope);
                    begin_new_scope(std::nullopt);

                    AstValueFunctionCall_FakeAstArgumentDeductionContextA* branch_a_context =
                        new AstValueFunctionCall_FakeAstArgumentDeductionContextA(
                            current_scope_before_deduction, get_current_scope());

                    node->m_LANG_branch_argument_deduction_context = branch_a_context;

                    end_last_scope();
                    end_last_scope();

                    bool deduction_error = false;

                    // Prepare for argument deduction. 
                    auto it_target_param = target_param_holders.begin();
                    const auto it_target_param_end = target_param_holders.end();
                    auto it_argument = node->m_arguments.begin();
                    const auto it_argument_end = node->m_arguments.end();

                    for (; it_target_param != it_target_param_end
                        && it_argument != it_argument_end;
                        ++it_target_param, ++it_argument)
                    {
                        AstTypeHolder* param_type_holder = *it_target_param;
                        AstValueBase* argument_value = *it_argument;

                        if (!argument_value->m_LANG_determined_type.has_value())
                        {
                            AstTypeHolder* param_type_holder =
                                static_cast<AstTypeHolder*>((*it_target_param)->clone());

                            branch_a_context->m_arguments_tobe_deduct.push_back(
                                AstValueFunctionCall_FakeAstArgumentDeductionContextA::ArgumentMatch{
                                    argument_value, param_type_holder });
                        }
                        else
                        {
                            // NOTE: Some template arguments may not be deduced, it's okay.
                            //   for example:
                            //
                            //  func map<T, R>(a: array<T>, f: (T)=> R)
                            //
                            //   in this case, R will not able to be determined in first step.

                            if (argument_value->node_type == AstBase::AST_FAKE_VALUE_UNPACK)
                            {
                                // Get unpacking type.
                                AstFakeValueUnpack* unpack_value = static_cast<AstFakeValueUnpack*>(argument_value);
                                lang_TypeInstance* unpack_value_type_instance =
                                    unpack_value->m_LANG_determined_type.value();

                                // NOTE: It's okay, fake value has been determined.
                                auto* unpack_value_determined_base_type =
                                    unpack_value_type_instance->get_determined_type().value();

                                if (unpack_value_determined_base_type->m_base_type ==
                                    lang_TypeInstance::DeterminedType::TUPLE)
                                {
                                    auto* tuple_type_dat =
                                        unpack_value_determined_base_type->m_external_type_description.m_tuple;
                                    auto it_unpacking_arg_type = tuple_type_dat->m_element_types.begin();
                                    auto it_unpacking_arg_type_end = tuple_type_dat->m_element_types.end();

                                    for (; it_unpacking_arg_type != it_unpacking_arg_type_end; ++it_unpacking_arg_type)
                                    {
                                        AstTypeHolder* unpacking_param_type_holder = *it_target_param;

                                        deduction_error = !template_arguments_deduction_extraction_with_type(
                                            lex,
                                            unpacking_param_type_holder,
                                            *it_unpacking_arg_type,
                                            pending_template_params,
                                            &deduction_results);

                                        if (deduction_error)
                                            break;

                                        if (++it_target_param == it_target_param_end)
                                        {
                                            --it_target_param;
                                            break;
                                        }
                                    }
                                }
                                else
                                {
                                    it_target_param = it_target_param_end;
                                    --it_target_param;
                                }
                            }
                            else
                            {
                                lang_TypeInstance* argument_type_instance =
                                    argument_value->m_LANG_determined_type.value();

                                deduction_error = !template_arguments_deduction_extraction_with_type(
                                    lex,
                                    param_type_holder,
                                    argument_type_instance,
                                    pending_template_params,
                                    &deduction_results);
                            }

                            // If deduction error, break.
                            if (deduction_error)
                                break;
                        }
                    }

                    // Prepare for template constant deduction.
                    branch_a_context->m_deduction_results = std::move(deduction_results);
                    branch_a_context->m_undetermined_template_params = std::move(pending_template_params);
                    if (entry_function_located_scope)
                        end_last_scope();

                    if (deduction_error)
                    {
                        wo_assert(it_argument != it_argument_end);

                        lex.record_lang_error(lexer::msglevel_t::error, *it_argument,
                            WO_ERR_FAILED_TO_DEDUCE_TEMPLATE_TYPE);

                        return FAILED;
                    }

                    for (; it_argument != it_argument_end; ++it_argument)
                    {
                        // Here still have some argument, too much arguments.
                        AstValueBase* argument_value = *it_argument;

                        if (!argument_value->m_LANG_determined_type.has_value())
                        {
                            // Eval it, report `WO_ERR_NOT_IN_REIFICATION_TEMPLATE_FUNC` error.
                            WO_CONTINUE_PROCESS(argument_value);
                        }
                    }

                    WO_CONTINUE_PROCESS(branch_a_context);
                    node->m_LANG_hold_state = AstValueFunctionCall::HOLD_BRANCH_A_TEMPLATE_ARGUMENT_DEDUCTION;
                }
                else
                {
                    AstValueFunctionCall_FakeAstArgumentDeductionContextB* branch_b_context =
                        new AstValueFunctionCall_FakeAstArgumentDeductionContextB();

                    lang_TypeInstance* target_function_type_instance =
                        node->m_function->m_LANG_determined_type.value();
                    auto target_function_type_instance_determined_base_type =
                        target_function_type_instance->get_determined_type();

                    if (!target_function_type_instance_determined_base_type.has_value())
                    {
                        lex.record_lang_error(lexer::msglevel_t::error, node->m_function,
                            WO_ERR_FAILED_TO_DEDUCE_TEMPLATE_TYPE);
                        return FAILED;
                    }

                    auto* target_function_type_instance_determined_base_type_instance =
                        target_function_type_instance_determined_base_type.value();

                    if (target_function_type_instance_determined_base_type_instance->m_base_type
                        != lang_TypeInstance::DeterminedType::FUNCTION)
                    {
                        // TODO: More detail.
                        lex.record_lang_error(lexer::msglevel_t::error, node->m_function,
                            WO_ERR_TARGET_TYPE_IS_NOT_A_FUNCTION,
                            get_type_name_w(target_function_type_instance));
                        return FAILED;
                    }

                    auto it_param_type = target_function_type_instance_determined_base_type_instance
                        ->m_external_type_description.m_function->m_param_types.begin();
                    auto it_param_type_end = target_function_type_instance_determined_base_type_instance
                        ->m_external_type_description.m_function->m_param_types.end();
                    auto it_argument = node->m_arguments.begin();
                    auto it_argument_end = node->m_arguments.end();

                    for (; it_param_type != it_param_type_end
                        && it_argument != it_argument_end;
                        ++it_param_type, ++it_argument)
                    {
                        lang_TypeInstance* param_type_instance = *it_param_type;
                        AstValueBase* argument_value = *it_argument;

                        if (argument_value->m_LANG_determined_type.has_value())
                            // has been determined, skip;
                            continue;

                        branch_b_context->m_arguments_tobe_deduct.push_back(
                            AstValueFunctionCall_FakeAstArgumentDeductionContextB::ArgumentMatch{
                                argument_value, param_type_instance });
                    }

                    node->m_LANG_branch_argument_deduction_context = branch_b_context;

                    // Function's parameters type has been determined, here will be more easier.
                    WO_CONTINUE_PROCESS(branch_b_context);
                    node->m_LANG_hold_state = AstValueFunctionCall::HOLD_BRANCH_B_TEMPLATE_ARGUMENT_DEDUCTION;
                }
                return HOLD;
            }
            case AstValueFunctionCall::HOLD_BRANCH_A_TEMPLATE_ARGUMENT_DEDUCTION:
            {
                // Template & arguments has been determined.
                // Make invoking function instance!.
                ///////////////////////////////////////////////////////////////////////////////
                // ATTENTION:
                //   Invoking function's type has not been checked until here, it might not a func
                ///////////////////////////////////////////////////////////////////////////////

                // Make instance!
                auto* branch_a_context = std::get<AstValueFunctionCall_FakeAstArgumentDeductionContextA*>(
                    node->m_LANG_branch_argument_deduction_context.value());

                switch (node->m_function->node_type)
                {
                case AstBase::AST_VALUE_FUNCTION:
                {
                    AstValueFunction* function = static_cast<AstValueFunction*>(node->m_function);
                    auto& pending_template_arguments = function->m_pending_param_type_mark_template.value();

                    std::list<ast::AstIdentifier::TemplateArgumentInstance> template_arguments;

                    std::list<ast::AstTemplateParam*> pending_template_params;
                    for (ast::AstTemplateParam* param : pending_template_arguments)
                    {
                        auto fnd = branch_a_context->m_deduction_results.find(param->m_param_name);
                        if (fnd == branch_a_context->m_deduction_results.end())
                            pending_template_params.push_back(param);
                        else
                            template_arguments.push_back(fnd->second);
                    }
                    if (!pending_template_params.empty())
                    {
                        std::wstring pending_type_list;
                        bool first_param = true;

                        for (ast::AstTemplateParam* param : pending_template_params)
                        {
                            if (!first_param)
                                pending_type_list += L", ";
                            else
                                first_param = false;

                            pending_type_list += *param->m_param_name;
                        }

                        lex.record_lang_error(lexer::msglevel_t::error, function,
                            WO_ERR_NOT_ALL_TEMPLATE_ARGUMENT_DETERMINED,
                            pending_type_list.c_str());

                        return FAILED;
                    }

                    function->m_LANG_determined_template_arguments = template_arguments;
                    function->m_LANG_in_template_reification_context = true;

                    WO_CONTINUE_PROCESS(function);
                    break;
                }
                case AstBase::AST_VALUE_VARIABLE:
                {
                    AstValueVariable* function_variable = static_cast<AstValueVariable*>(node->m_function);
                    lang_Symbol* symbol = function_variable->m_identifier->m_LANG_determined_symbol.value();

                    wo_assert(symbol->m_is_template
                        && symbol->m_symbol_kind == lang_Symbol::kind::VARIABLE
                        && !symbol->m_template_value_instances->m_mutable);

                    auto& template_instance_prefab = symbol->m_template_value_instances;

                    std::unordered_map<wo_pstring_t, lang_TypeInstance*> deduction_results;
                    auto it_template_param = template_instance_prefab->m_template_params.begin();
                    auto it_template_param_end = template_instance_prefab->m_template_params.end();
                    if (function_variable->m_identifier->m_template_arguments.has_value())
                    {
                        for (auto& _useless : function_variable->m_identifier->m_template_arguments.value())
                        {
                            (void)_useless;

                            if (it_template_param == it_template_param_end)
                                break;

                            ++it_template_param;
                        }
                    }
                    std::list<ast::AstIdentifier::TemplateArgumentInstance> template_argument_list;

                    std::list<ast::AstTemplateParam*> pending_template_params;
                    for (; it_template_param != it_template_param_end; ++it_template_param)
                    {
                        ast::AstTemplateParam* param_name = *it_template_param;
                        auto fnd = branch_a_context->m_deduction_results.find(param_name->m_param_name);
                        if (fnd == branch_a_context->m_deduction_results.end())
                            pending_template_params.push_back(param_name);
                        else
                            template_argument_list.push_back(fnd->second);
                    }

                    if (!pending_template_params.empty())
                    {
                        std::wstring pending_type_list;
                        bool first_param = true;

                        for (ast::AstTemplateParam* param : pending_template_params)
                        {
                            if (!first_param)
                                pending_type_list += L", ";
                            else
                                first_param = false;

                            pending_type_list += *param->m_param_name;
                        }

                        lex.record_lang_error(lexer::msglevel_t::error, function_variable,
                            WO_ERR_NOT_ALL_TEMPLATE_ARGUMENT_DETERMINED,
                            pending_type_list.c_str());

                        if (symbol->m_symbol_declare_ast.has_value())
                        {
                            lex.record_lang_error(lexer::msglevel_t::infom, symbol->m_symbol_declare_ast.value(),
                                WO_INFO_SYMBOL_NAMED_DEFINED_HERE,
                                get_symbol_name_w(symbol));
                        }

                        return FAILED;
                    }

                    function_variable->m_identifier->m_LANG_determined_and_appended_template_arguments
                        = template_argument_list;

                    WO_CONTINUE_PROCESS(function_variable);
                    break;
                }
                default:
                    wo_error("Unknown function type.");
                }

                node->m_LANG_hold_state = AstValueFunctionCall::HOLD_BRANCH_B_TEMPLATE_ARGUMENT_DEDUCTION;
                return HOLD;
            }
            case AstValueFunctionCall::HOLD_BRANCH_B_TEMPLATE_ARGUMENT_DEDUCTION:
            {
                // All argument & function has been determined.
                // Now we can do type checking.

                lang_TypeInstance* target_function_type_instance =
                    node->m_function->m_LANG_determined_type.value();

                auto target_function_type_instance_determined_base_type =
                    target_function_type_instance->get_determined_type();

                if (!target_function_type_instance_determined_base_type.has_value())
                {
                    lex.record_lang_error(lexer::msglevel_t::error, node->m_function,
                        WO_ERR_TYPE_NAMED_DETERMINED_FAILED,
                        get_type_name_w(target_function_type_instance));
                    return FAILED;
                }

                auto* target_function_type_instance_determined_base_type_instance =
                    target_function_type_instance_determined_base_type.value();

                if (target_function_type_instance_determined_base_type_instance->m_base_type
                    != lang_TypeInstance::DeterminedType::FUNCTION)
                {
                    lex.record_lang_error(lexer::msglevel_t::error, node->m_function,
                        WO_ERR_TARGET_TYPE_IS_NOT_A_FUNCTION,
                        get_type_name_w(target_function_type_instance));
                    return FAILED;
                }

                auto* target_function_type_instance_determined_base_type_function =
                    target_function_type_instance_determined_base_type_instance->m_external_type_description.m_function;
                auto& target_function_param_types =
                    target_function_type_instance_determined_base_type_function->m_param_types;

                node->m_LANG_invoking_variadic_function =
                    target_function_type_instance_determined_base_type_function->m_is_variadic;

                std::list<std::pair<lang_TypeInstance*, AstValueBase*>> argument_types;
                bool expaned_array_or_vec = false;

                wo_assert(node->m_LANG_certenly_function_argument_count == 0
                    && !node->m_LANG_has_runtime_full_unpackargs);

                for (auto* argument_value : node->m_arguments)
                {
                    if (expaned_array_or_vec)
                    {
                        // ATTENTION: It is technically feasible to continue passing after parameter
                        //  expansion; However, during the instruction generation phase, if the packargs
                        //  instruction appears in the parameters, the instruction will depend on the tc
                        //  register; And the unpackargs instruction working in full unpacking mode will 
                        //  modify the value of the tc register; Therefore, if the calling behavior has
                        //  unpackargs, the value of tc can only be set before this instruction occurs

                        lex.record_lang_error(lexer::msglevel_t::error, argument_value,
                            WO_ERR_ARG_DEFINE_AFTER_EXPAND_VECARR);
                        return FAILED;
                    }

                    if (argument_value->node_type == AstBase::AST_FAKE_VALUE_UNPACK)
                    {
                        AstFakeValueUnpack* unpack = static_cast<AstFakeValueUnpack*>(argument_value);
                        unpack->m_IR_unpack_method = AstFakeValueUnpack::UNPACK_FOR_FUNCTION_CALL;
                        auto* unpacked_value_determined_value =
                            unpack->m_unpack_value->m_LANG_determined_type.value()->get_determined_type().value();

                        switch (unpacked_value_determined_value->m_base_type)
                        {
                        case lang_TypeInstance::DeterminedType::ARRAY:
                        case lang_TypeInstance::DeterminedType::VECTOR:
                        {
                            expaned_array_or_vec = true;
                            const size_t elem_count_to_be_expand =
                                target_function_param_types.size() >= argument_types.size()
                                ? target_function_param_types.size() - argument_types.size()
                                : 0;

                            unpack->m_IR_need_to_be_unpack_count =
                                AstFakeValueUnpack::IR_unpack_requirement{
                                    elem_count_to_be_expand,
                                    node->m_LANG_invoking_variadic_function,
                            };

                            if (node->m_LANG_invoking_variadic_function)
                                node->m_LANG_has_runtime_full_unpackargs = true;
                            else
                                node->m_LANG_certenly_function_argument_count +=
                                elem_count_to_be_expand;
                            break;
                        }
                        case lang_TypeInstance::DeterminedType::TUPLE:
                        {
                            auto* tuple_determined_type =
                                unpacked_value_determined_value->m_external_type_description.m_tuple;

                            for (auto* type : tuple_determined_type->m_element_types)
                            {
                                argument_types.push_back(
                                    std::make_pair(type, unpack->m_unpack_value));
                            }

                            unpack->m_IR_need_to_be_unpack_count =
                                AstFakeValueUnpack::IR_unpack_requirement{
                                    tuple_determined_type->m_element_types.size(),
                                    false,
                            };

                            node->m_LANG_certenly_function_argument_count +=
                                tuple_determined_type->m_element_types.size();
                            break;
                        }
                        default:
                            wo_error("Unexpected unpacked value type.");
                        }
                    }
                    else
                    {
                        lang_TypeInstance* argument_type_instance =
                            argument_value->m_LANG_determined_type.value();

                        argument_types.push_back(
                            std::make_pair(argument_type_instance, argument_value));

                        ++node->m_LANG_certenly_function_argument_count;
                    }
                }

                bool failed = false;
                if (argument_types.size() < target_function_param_types.size()
                    && !expaned_array_or_vec)
                {
                    lex.record_lang_error(lexer::msglevel_t::error, node,
                        WO_ERR_ARGUMENT_TOO_LESS);

                    failed = true; // FAILED;
                }
                else if (argument_types.size() > target_function_param_types.size()
                    && !node->m_LANG_invoking_variadic_function)
                {
                    lex.record_lang_error(lexer::msglevel_t::error, node,
                        WO_ERR_ARGUMENT_TOO_MUCH);

                    failed = true; // FAILED;
                }

                if (failed)
                {
                    lex.record_lang_error(lexer::msglevel_t::infom, node->m_function,
                        WO_INFO_THIS_VALUE_IS_TYPE_NAMED,
                        get_type_name_w(target_function_type_instance));

                    return FAILED;
                }

                auto it_param_type = target_function_param_types.begin();
                auto it_param_type_end = target_function_param_types.end();

                auto it_argument_type = argument_types.begin();
                auto it_argument_type_end = argument_types.end();

                for (; it_param_type != it_param_type_end
                    && it_argument_type != it_argument_type_end;
                    ++it_param_type, ++it_argument_type)
                {
                    auto& [type, arg_node] = *it_argument_type;
                    lang_TypeInstance* param_type = *it_param_type;

                    if (lang_TypeInstance::TypeCheckResult::ACCEPT
                        != is_type_accepted(lex, arg_node, param_type, type))
                    {
                        lex.record_lang_error(lexer::msglevel_t::error, arg_node,
                            WO_ERR_CANNOT_ACCEPTABLE_TYPE_NAMED,
                            get_type_name_w(type),
                            get_type_name_w(param_type));

                        return FAILED;
                    }
                }

                // Finished, all job done!.
                node->m_LANG_determined_type =
                    target_function_type_instance_determined_base_type_function->m_return_type;

                break;
            }
            default:
                wo_error("Unknown hold state.");
            }
        }
        else
        {
            if (node->m_is_direct_call
                && node->m_LANG_hold_state == AstValueFunctionCall::HOLD_FOR_FUNCTION_EVAL)
            {
                AstValueBase* first_argument = node->m_arguments.front();
                lex.record_lang_error(lexer::msglevel_t::infom, first_argument,
                    WO_INFO_TYPE_NAMED_BEFORE_DIRECT_SIGN,
                    get_type_name_w(first_argument->m_LANG_determined_type.value()));
            }
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
    WO_PASS_PROCESSER(AstValueTypeid)
    {
        if (state == UNPROCESSED)
        {
            // Suppress errors generated by the compiler.
            lex.begin_trying_block();

            WO_CONTINUE_PROCESS(node->m_id_type);
            return HOLD;
        }
        else
        {
            wo_integer_t hash = 0;
            if (state == HOLD)
            {
                wo_assert(lex.get_current_error_frame().empty());

                lang_TypeInstance* id_type_instance = node->m_id_type->m_LANG_determined_type.value();
                hash = (wo_integer_t)(intptr_t)id_type_instance;
            }
            else
                wo_assert(!lex.get_current_error_frame().empty());

            lex.end_trying_block();

            wo::value cval;
            cval.set_integer(hash);

            node->decide_final_constant_value(cval);
            node->m_LANG_determined_type = m_origin_types.m_int.m_type_instance;
        }
        return OKAY;
    }
    WO_PASS_PROCESSER(AstValueTypeCheckIs)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_check_type);
            WO_CONTINUE_PROCESS(node->m_check_value);

            return HOLD;
        }
        else if (state == HOLD)
        {
            lang_TypeInstance* target_type =
                node->m_check_type->m_LANG_determined_type.value();
            lang_TypeInstance* value_type =
                node->m_check_value->m_LANG_determined_type.value();

            if (immutable_type(value_type) == m_origin_types.m_dynamic.m_type_instance
                && immutable_type(target_type) != m_origin_types.m_dynamic.m_type_instance
                && immutable_type(target_type) != m_origin_types.m_void.m_type_instance)
            {
                if (lang_TypeInstance::TypeCheckResult::ACCEPT
                    != check_cast_able(lex, node, target_type, value_type))
                {
                    lex.record_lang_error(lexer::msglevel_t::error, node->m_check_type,
                        WO_ERR_CANNOT_CAST_TYPE_NAMED_FROM_DYNMAIC,
                        get_type_name_w(target_type));

                    return FAILED;
                }
                // Dynamic type, do check in runtime.
            }
            else
            {
                wo::value cval;
                cval.set_bool(
                    lang_TypeInstance::TypeCheckResult::ACCEPT == is_type_accepted(
                        lex,
                        node,
                        target_type,
                        value_type));
                node->decide_final_constant_value(cval);
            }
            node->m_LANG_determined_type =
                m_origin_types.m_bool.m_type_instance;
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueTypeCheckAs)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_check_type);
            WO_CONTINUE_PROCESS(node->m_check_value);

            return HOLD;
        }
        else if (state == HOLD)
        {
            lang_TypeInstance* target_type =
                node->m_check_type->m_LANG_determined_type.value();
            lang_TypeInstance* value_type =
                node->m_check_value->m_LANG_determined_type.value();

            if (immutable_type(value_type) == m_origin_types.m_dynamic.m_type_instance
                && immutable_type(target_type) != m_origin_types.m_dynamic.m_type_instance
                && immutable_type(target_type) != m_origin_types.m_void.m_type_instance)
            {
                if (lang_TypeInstance::TypeCheckResult::ACCEPT
                    != check_cast_able(lex, node, target_type, value_type))
                {
                    lex.record_lang_error(lexer::msglevel_t::error, node->m_check_type,
                        WO_ERR_CANNOT_CAST_TYPE_NAMED_FROM_DYNMAIC,
                        get_type_name_w(target_type));

                    return FAILED;
                }
                // Dynamic type, do check in runtime.
                node->m_IR_dynamic_need_runtime_check = true;
            }
            else
            {
                if (lang_TypeInstance::TypeCheckResult::ACCEPT != is_type_accepted(
                    lex,
                    node,
                    target_type,
                    value_type))
                {
                    lex.record_lang_error(lexer::msglevel_t::error, node->m_check_value,
                        WO_ERR_CANNOT_ACCEPTABLE_TYPE_NAMED,
                        get_type_name_w(value_type),
                        get_type_name_w(target_type));
                    return FAILED;
                }

                if (node->m_check_value->m_evaled_const_value.has_value())
                    node->decide_final_constant_value(
                        node->m_check_value->m_evaled_const_value.value());
            }
            node->m_LANG_determined_type = target_type;
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstLabeled)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_body);
            return HOLD;
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstStructFieldValuePair)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_value);
            return HOLD;
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueStruct)
    {
        if (state == UNPROCESSED)
        {
            if (node->m_marked_struct_type.has_value())
            {
                for (auto* member_pair : node->m_fields)
                {
                    auto* origin_value = get_marked_origin_value_node(member_pair->m_value);
                    if (origin_value->node_type == AstBase::AST_VALUE_VARIABLE)
                    {
                        // Eval from type first;
                        AstValueVariable* value_variabl = static_cast<AstValueVariable*>(origin_value);
                        if (value_variabl->m_identifier->m_formal == AstIdentifier::identifier_formal::FROM_TYPE)
                        {
                            AstTypeHolder** type_holder = std::get_if<AstTypeHolder*>(
                                &value_variabl->m_identifier->m_from_type.value());

                            if (type_holder != nullptr)
                                WO_CONTINUE_PROCESS(*type_holder);
                        }
                    }
                }
                node->m_LANG_hold_state = AstValueStruct::HOLD_FOR_MEMBER_TYPE_EVAL;
            }
            else
            {
                // Make raw struct.
                WO_CONTINUE_PROCESS_LIST(node->m_fields);
                node->m_LANG_hold_state = AstValueStruct::HOLD_TO_STRUCT_TYPE_CHECK;
            }
            return HOLD;
        }
        else if (state == HOLD)
        {
            switch (node->m_LANG_hold_state)
            {
            case AstValueStruct::HOLD_FOR_MEMBER_TYPE_EVAL:
            {
                std::list<AstValueBase*> member_init_values;

                for (auto* member_pair : node->m_fields)
                {
                    if (check_need_template_deduct_function(lex, member_pair->m_value, out_stack))
                        continue;

                    member_init_values.push_back(member_pair->m_value);
                }

                WO_CONTINUE_PROCESS_LIST(member_init_values);

                AstTypeHolder* struct_type = node->m_marked_struct_type.value();
                bool need_template_deduct = check_need_template_deduct_struct_type(lex, struct_type, out_stack);

                if (need_template_deduct)
                {
                    node->m_LANG_hold_state = AstValueStruct::HOLD_FOR_EVAL_MEMBER_VALUE_BESIDE_TEMPLATE;
                }
                else
                {
                    WO_CONTINUE_PROCESS(struct_type);
                    node->m_LANG_hold_state = AstValueStruct::HOLD_FOR_ANYLIZE_ARGUMENTS_TEMAPLTE_INSTANCE;
                }
                return HOLD;
            }
            case AstValueStruct::HOLD_FOR_EVAL_MEMBER_VALUE_BESIDE_TEMPLATE:
            {
                // First deduct
                auto* current_scope_before_deduction = get_current_scope();

                std::unordered_map<wo_pstring_t, ast::AstIdentifier::TemplateArgumentInstance> deduction_results;

                std::list<ast::AstTemplateParam*> pending_template_params;
                std::list<AstTypeHolder*> target_param_holders;

                wo_assert(node->m_marked_struct_type.value()->m_formal == AstTypeHolder::IDENTIFIER);
                AstTypeHolder* struct_type = node->m_marked_struct_type.value();
                AstIdentifier* struct_type_identifier = struct_type->m_typeform.m_identifier;

                lang_Symbol* symbol = struct_type_identifier->m_LANG_determined_symbol.value();
                auto& struct_type_def_prefab = symbol->m_template_type_instances;

                auto it_template_param = symbol->m_template_type_instances->m_template_params.begin();
                auto it_template_param_end = symbol->m_template_type_instances->m_template_params.end();
                if (struct_type_identifier->m_template_arguments.has_value())
                {
                    for (auto& exist_template_argument : struct_type_identifier->m_template_arguments.value())
                    {
                        if (exist_template_argument->is_type())
                            deduction_results.insert(
                                std::make_pair(
                                    (*it_template_param)->m_param_name,
                                    exist_template_argument->get_type()->m_LANG_determined_type.value()));
                        else
                            deduction_results.insert(
                                std::make_pair(
                                    (*it_template_param)->m_param_name,
                                    exist_template_argument->get_constant()));

                        ++it_template_param;
                    }
                }

                for (; it_template_param != it_template_param_end; ++it_template_param)
                    pending_template_params.push_back(*it_template_param);

                AstTypeHolder* struct_def_type_holder = struct_type_def_prefab->m_origin_value_ast;
                std::unordered_map<wo_pstring_t, AstTypeHolder*> struct_template_deduction_target;

                for (auto& field_type : struct_def_type_holder->m_typeform.m_structure.m_fields)
                {
                    struct_template_deduction_target.insert(
                        std::make_pair(field_type->m_name, field_type->m_type));
                }

                entry_spcify_scope(symbol->m_belongs_to_scope);

                // Begin new scope for template deduction.
                begin_new_scope(std::nullopt);

                AstValueFunctionCall_FakeAstArgumentDeductionContextA* branch_a_context =
                    new AstValueFunctionCall_FakeAstArgumentDeductionContextA(
                        current_scope_before_deduction, get_current_scope());
                node->m_LANG_branch_argument_deduction_context = branch_a_context;

                end_last_scope();

                bool deduction_error = false;

                auto it_field = node->m_fields.begin();
                auto it_field_end = node->m_fields.end();
                for (; it_field != it_field_end; ++it_field)
                {
                    auto* field_instance = *it_field;

                    auto fnd = struct_template_deduction_target.find(field_instance->m_name);
                    if (fnd == struct_template_deduction_target.end())
                    {
                        // Bad field, but we don't care.
                        continue;
                    }

                    if (!field_instance->m_value->m_LANG_determined_type.has_value())
                    {
                        // Need to be determined.
                        AstTypeHolder* param_type_holder = static_cast<AstTypeHolder*>(
                            fnd->second->clone());

                        branch_a_context->m_arguments_tobe_deduct.push_back(
                            AstValueFunctionCall_FakeAstArgumentDeductionContextA::ArgumentMatch{
                                field_instance->m_value, param_type_holder });
                    }
                    else
                    {
                        lang_TypeInstance* field_instance_type = field_instance->m_value->m_LANG_determined_type.value();

                        deduction_error = !template_arguments_deduction_extraction_with_type(
                            lex,
                            fnd->second,
                            field_instance_type,
                            pending_template_params,
                            &deduction_results);

                        if (deduction_error)
                            break;
                    }
                }

                branch_a_context->m_deduction_results = std::move(deduction_results);
                branch_a_context->m_undetermined_template_params = std::move(pending_template_params);
                end_last_scope();

                if (deduction_error)
                {
                    wo_assert(it_field != it_field_end);

                    lex.record_lang_error(lexer::msglevel_t::error, *it_field,
                        WO_ERR_FAILED_TO_DEDUCE_TEMPLATE_TYPE);

                    return FAILED;
                }

                WO_CONTINUE_PROCESS(branch_a_context);
                node->m_LANG_hold_state = AstValueStruct::HOLD_FOR_TEMPLATE_DEDUCTION;

                return HOLD;
            }
            case AstValueStruct::HOLD_FOR_TEMPLATE_DEDUCTION:
            {
                AstValueFunctionCall_FakeAstArgumentDeductionContextA* branch_a_context =
                    std::get<AstValueFunctionCall_FakeAstArgumentDeductionContextA*>(
                        node->m_LANG_branch_argument_deduction_context.value());

                // All template has been determined.
                // Now we can do type checking.
                AstTypeHolder* target_struct_typeholder = node->m_marked_struct_type.value();

                lang_Symbol* symbol =
                    target_struct_typeholder->m_typeform.m_identifier->m_LANG_determined_symbol.value();

                wo_assert(symbol->m_is_template
                    && symbol->m_symbol_kind == lang_Symbol::kind::TYPE);

                auto& template_instance_prefab = symbol->m_template_type_instances;

                auto it_template_param = symbol->m_template_type_instances->m_template_params.begin();
                auto it_template_param_end = symbol->m_template_type_instances->m_template_params.end();
                if (target_struct_typeholder->m_typeform.m_identifier->m_template_arguments.has_value())
                {
                    for (auto& _useless : target_struct_typeholder->m_typeform.m_identifier->m_template_arguments.value())
                    {
                        (void)_useless;
                        ++it_template_param;
                    }
                }
                std::list<ast::AstIdentifier::TemplateArgumentInstance> template_argument_list;
                std::list<ast::AstTemplateParam*> pending_template_params;

                for (; it_template_param != it_template_param_end; ++it_template_param)
                {
                    ast::AstTemplateParam* param_name = *it_template_param;
                    auto fnd = branch_a_context->m_deduction_results.find(param_name->m_param_name);
                    if (fnd == branch_a_context->m_deduction_results.end())
                        pending_template_params.push_back(param_name);
                    else
                        template_argument_list.push_back(fnd->second);
                }

                if (!pending_template_params.empty())
                {
                    std::wstring pending_type_list;
                    bool first_param = true;

                    for (ast::AstTemplateParam* param : pending_template_params)
                    {
                        if (!first_param)
                            pending_type_list += L", ";
                        else
                            first_param = false;

                        pending_type_list += *param->m_param_name;
                    }

                    lex.record_lang_error(lexer::msglevel_t::error, target_struct_typeholder,
                        WO_ERR_NOT_ALL_TEMPLATE_ARGUMENT_DETERMINED,
                        pending_type_list.c_str());

                    if (symbol->m_symbol_declare_ast.has_value())
                    {
                        lex.record_lang_error(lexer::msglevel_t::infom, symbol->m_symbol_declare_ast.value(),
                            WO_INFO_SYMBOL_NAMED_DEFINED_HERE,
                            get_symbol_name_w(symbol));
                    }

                    return FAILED;
                }

                target_struct_typeholder->m_typeform.m_identifier->m_LANG_determined_and_appended_template_arguments
                    = template_argument_list;

                WO_CONTINUE_PROCESS(target_struct_typeholder);

                node->m_LANG_hold_state = AstValueStruct::HOLD_TO_STRUCT_TYPE_CHECK;
                return HOLD;
            }
            case AstValueStruct::HOLD_FOR_ANYLIZE_ARGUMENTS_TEMAPLTE_INSTANCE:
            {
                AstTypeHolder* struct_type = node->m_marked_struct_type.value();

                // It has been determined in UNPROCESSED state.
                lang_TypeInstance* struct_type_instance = struct_type->m_LANG_determined_type.value();
                auto determined_base_type = struct_type_instance->get_determined_type();

                if (determined_base_type.has_value())
                {
                    std::list<AstValueFunctionCall_FakeAstArgumentDeductionContextB::ArgumentMatch>
                        arguments_tobe_deduct;

                    auto* determined_base_type_instance = determined_base_type.value();
                    if (determined_base_type_instance->m_base_type == lang_TypeInstance::DeterminedType::STRUCT)
                    {
                        std::unordered_map<wo_pstring_t, lang_TypeInstance*> deduction_results;
                        for (auto& field : determined_base_type_instance->m_external_type_description.m_struct->m_member_types)
                        {
                            deduction_results.insert(
                                std::make_pair(field.first, field.second.m_member_type));
                        }

                        for (auto& field_instance : node->m_fields)
                        {
                            if (!field_instance->m_value->m_LANG_determined_type.has_value())
                            {
                                auto fnd = deduction_results.find(field_instance->m_name);

                                arguments_tobe_deduct.push_back(
                                    AstValueFunctionCall_FakeAstArgumentDeductionContextB::ArgumentMatch{
                                        field_instance->m_value, fnd->second });
                            }
                        }
                    }

                    if (!arguments_tobe_deduct.empty())
                    {
                        AstValueFunctionCall_FakeAstArgumentDeductionContextB* branch_b_context =
                            new AstValueFunctionCall_FakeAstArgumentDeductionContextB();

                        branch_b_context->m_arguments_tobe_deduct = std::move(arguments_tobe_deduct);

                        node->m_LANG_branch_argument_deduction_context = branch_b_context;

                        WO_CONTINUE_PROCESS(branch_b_context);
                    }
                }
                node->m_LANG_hold_state = AstValueStruct::HOLD_TO_STRUCT_TYPE_CHECK;
                return HOLD;
            }
            case AstValueStruct::HOLD_TO_STRUCT_TYPE_CHECK:
            {
                if (node->m_marked_struct_type.has_value())
                {
                    AstTypeHolder* struct_type_holder = node->m_marked_struct_type.value();
                    lang_TypeInstance* struct_type_instance = struct_type_holder->m_LANG_determined_type.value();
                    auto determined_base_type = struct_type_instance->get_determined_type();

                    if (!determined_base_type.has_value())
                    {
                        lex.record_lang_error(lexer::msglevel_t::error, node,
                            WO_ERR_TYPE_NAMED_DETERMINED_FAILED,
                            get_type_name_w(struct_type_instance));

                        return FAILED;
                    }

                    if (determined_base_type.value()->m_base_type != lang_TypeInstance::DeterminedType::STRUCT)
                    {
                        lex.record_lang_error(lexer::msglevel_t::error, node,
                            WO_ERR_TYPE_NAMED_IS_NOT_STRUCT,
                            get_type_name_w(struct_type_instance));

                        return FAILED;
                    }

                    // Check symbol can be reach.
                    if (!check_symbol_is_reachable_in_current_scope(
                        lex,
                        node,
                        struct_type_instance->m_symbol,
                        node->source_location.source_file,
                        !node->duplicated_node /* TMP: Skip import check in template function. */))
                    {
                        return FAILED;
                    }
                }

                node->m_LANG_hold_state = AstValueStruct::HOLD_FOR_FIELD_EVAL;
                return HOLD;
            }
            case AstValueStruct::HOLD_FOR_FIELD_EVAL:
            {
                lang_TypeInstance* struct_type_instanc;
                if (node->m_marked_struct_type.has_value())
                    struct_type_instanc = node->m_marked_struct_type.value()->m_LANG_determined_type.value();
                else
                {
                    std::list<std::tuple<ast::AstDeclareAttribue::accessc_attrib, wo_pstring_t, lang_TypeInstance*>>
                        struct_field_info_list;

                    for (auto* field : node->m_fields)
                    {
                        struct_field_info_list.push_back(
                            std::make_tuple(
                                ast::AstDeclareAttribue::accessc_attrib::PUBLIC,
                                field->m_name,
                                field->m_value->m_LANG_determined_type.value()));
                    }

                    struct_type_instanc = m_origin_types.create_struct_type(struct_field_info_list);
                }

                auto struct_determined_base_type = struct_type_instanc->get_determined_type();
                if (!struct_determined_base_type.has_value())
                {
                    lex.record_lang_error(lexer::msglevel_t::error, node,
                        WO_ERR_TYPE_NAMED_DETERMINED_FAILED,
                        get_type_name_w(struct_type_instanc));

                    return FAILED;
                }

                auto* struct_determined_base_type_instance = struct_determined_base_type.value();
                wo_assert(struct_determined_base_type_instance->m_base_type
                    == lang_TypeInstance::DeterminedType::STRUCT);

                auto* struct_type_info =
                    struct_determined_base_type_instance->m_external_type_description.m_struct;

                if (node->m_fields.size() < struct_type_info->m_member_types.size())
                {
                    lex.record_lang_error(lexer::msglevel_t::error, node,
                        WO_ERR_NOT_ALL_FIELD_INITIALIZED);

                    return FAILED;
                }
                else if (node->m_fields.size() > struct_type_info->m_member_types.size())
                {
                    lex.record_lang_error(lexer::msglevel_t::error, node,
                        WO_ERR_TOO_MUCH_FIELD_INITIALIZED);

                    return FAILED;
                }

                bool failed = false;

                for (auto* field : node->m_fields)
                {
                    auto fnd = struct_type_info->m_member_types.find(field->m_name);
                    if (fnd == struct_type_info->m_member_types.end())
                    {
                        lex.record_lang_error(lexer::msglevel_t::error, field,
                            WO_ERR_STRUCT_DONOT_HAVE_MAMBER_NAMED,
                            get_type_name_w(struct_type_instanc),
                            field->m_name->c_str());

                        failed = true;
                        continue;
                    }

                    if (!check_struct_field_is_reachable_in_current_scope(
                        lex,
                        field,
                        struct_type_instanc->m_symbol,
                        fnd->second.m_attrib,
                        fnd->first,
                        node->source_location.source_file))
                    {
                        failed = true;
                        continue;
                    }

                    lang_TypeInstance* accpet_field_type = fnd->second.m_member_type;
                    if (lang_TypeInstance::TypeCheckResult::ACCEPT
                        != is_type_accepted(
                            lex,
                            field,
                            accpet_field_type,
                            field->m_value->m_LANG_determined_type.value()))
                    {
                        lex.record_lang_error(lexer::msglevel_t::error, field->m_value,
                            WO_ERR_CANNOT_ACCEPTABLE_TYPE_NAMED,
                            get_type_name_w(field->m_value->m_LANG_determined_type.value()),
                            get_type_name_w(accpet_field_type));

                        failed = true;
                        continue;
                    }
                }

                if (failed)
                    return FAILED;

                node->m_LANG_determined_type = struct_type_instanc;
                break;
            }
            default:
                wo_error("Unknown hold state.");
            }
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstUsingNamespace)
    {
        wo_assert(state == UNPROCESSED);
        using_namespace_declare_for_current_scope(node);

        return OKAY;
    }

    WO_PASS_PROCESSER(AstValueBinaryOperator)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_right);
            WO_CONTINUE_PROCESS(node->m_left);

            node->m_LANG_hold_state = AstValueBinaryOperator::HOLD_FOR_OPNUM_EVAL;
            return HOLD;
        }
        else if (state == HOLD)
        {
            switch (node->m_LANG_hold_state)
            {
            case AstValueBinaryOperator::HOLD_FOR_OPNUM_EVAL:
            {
                // Get first operand type symbol.
                lang_TypeInstance* left_type = node->m_left->m_LANG_determined_type.value();
                lang_Symbol* left_type_symbol = left_type->m_symbol;

                wo_pstring_t operator_name;
                switch (node->m_operator)
                {
                case AstValueBinaryOperator::ADD:
                    operator_name = WO_PSTR(operator_ADD);
                    break;
                case AstValueBinaryOperator::SUBSTRACT:
                    operator_name = WO_PSTR(operator_SUB);
                    break;
                case AstValueBinaryOperator::MULTIPLY:
                    operator_name = WO_PSTR(operator_MUL);
                    break;
                case AstValueBinaryOperator::DIVIDE:
                    operator_name = WO_PSTR(operator_DIV);
                    break;
                case AstValueBinaryOperator::MODULO:
                    operator_name = WO_PSTR(operator_MOD);
                    break;
                case AstValueBinaryOperator::LOGICAL_AND:
                    operator_name = WO_PSTR(operator_LAND);
                    break;
                case AstValueBinaryOperator::LOGICAL_OR:
                    operator_name = WO_PSTR(operator_LOR);
                    break;
                case AstValueBinaryOperator::GREATER:
                    operator_name = WO_PSTR(operator_GREAT);
                    break;
                case AstValueBinaryOperator::GREATER_EQUAL:
                    operator_name = WO_PSTR(operator_GREATEQ);
                    break;
                case AstValueBinaryOperator::LESS:
                    operator_name = WO_PSTR(operator_LESS);
                    break;
                case AstValueBinaryOperator::LESS_EQUAL:
                    operator_name = WO_PSTR(operator_LESSEQ);
                    break;
                case AstValueBinaryOperator::EQUAL:
                    operator_name = WO_PSTR(operator_EQ);
                    break;
                case AstValueBinaryOperator::NOT_EQUAL:
                    operator_name = WO_PSTR(operator_NEQ);
                    break;
                default:
                    wo_error("Unknown operator.");
                }

                AstIdentifier* operator_identifier = new AstIdentifier(operator_name);
                operator_identifier->m_formal = AstIdentifier::identifier_formal::FROM_TYPE;
                operator_identifier->m_from_type = left_type;
                operator_identifier->m_find_type_only = false;
                operator_identifier->duplicated_node = node->duplicated_node;

                // Update source location.
                operator_identifier->source_location = node->source_location;

                bool ambiguous = false;
                if (find_symbol_in_current_scope(lex, operator_identifier, &ambiguous))
                {
                    // Has overload function.
                    AstValueVariable* overload_function = new AstValueVariable(operator_identifier);
                    overload_function->duplicated_node = node->duplicated_node;

                    AstValueFunctionCall* overload_function_call = new AstValueFunctionCall(
                        false /* symbol has ben determined */, overload_function, { node->m_left, node->m_right });

                    // Update source location.
                    overload_function->source_location = node->source_location;
                    overload_function_call->source_location = node->source_location;

                    node->m_LANG_overload_call = overload_function_call;

                    WO_CONTINUE_PROCESS(overload_function_call);

                    node->m_LANG_hold_state = AstValueBinaryOperator::HOLD_FOR_OVERLOAD_FUNCTION_CALL_EVAL;
                    return HOLD;
                }
                else if (ambiguous)
                    return FAILED;
                else
                {
                    // No function overload, type check.
                    // 1) Base typecheck.
                    lang_TypeInstance* left_type = node->m_left->m_LANG_determined_type.value();
                    lang_TypeInstance* right_type = node->m_right->m_LANG_determined_type.value();

                    if (left_type != right_type)
                    {
                        lex.record_lang_error(lexer::msglevel_t::error, node,
                            WO_ERR_DIFFERENT_TYPE_IN_BINARY,
                            get_type_name_w(left_type),
                            get_type_name_w(right_type));

                        lex.record_lang_error(lexer::msglevel_t::infom, node->m_left,
                            WO_INFO_THIS_VALUE_IS_TYPE_NAMED,
                            get_type_name_w(left_type));

                        lex.record_lang_error(lexer::msglevel_t::infom, node->m_right,
                            WO_INFO_THIS_VALUE_IS_TYPE_NAMED,
                            get_type_name_w(right_type));

                        return FAILED;
                    }
                    auto left_base_type = left_type->get_determined_type();
                    if (!left_base_type.has_value())
                    {
                        lex.record_lang_error(lexer::msglevel_t::error, node->m_left,
                            WO_ERR_TYPE_NAMED_DETERMINED_FAILED,
                            get_type_name_w(left_type));

                        return FAILED;
                    }

                    auto base_type = left_base_type.value()->m_base_type;
                    bool accept_type = false;

                    switch (node->m_operator)
                    {
                    case AstValueBinaryOperator::ADD:
                        accept_type =
                            base_type == lang_TypeInstance::DeterminedType::INTEGER
                            || base_type == lang_TypeInstance::DeterminedType::REAL
                            || base_type == lang_TypeInstance::DeterminedType::HANDLE
                            || base_type == lang_TypeInstance::DeterminedType::STRING;
                        break;
                    case AstValueBinaryOperator::SUBSTRACT:
                        accept_type =
                            base_type == lang_TypeInstance::DeterminedType::INTEGER
                            || base_type == lang_TypeInstance::DeterminedType::REAL
                            || base_type == lang_TypeInstance::DeterminedType::HANDLE;
                        break;
                    case AstValueBinaryOperator::MULTIPLY:
                        accept_type =
                            base_type == lang_TypeInstance::DeterminedType::INTEGER
                            || base_type == lang_TypeInstance::DeterminedType::REAL;
                        break;
                    case AstValueBinaryOperator::DIVIDE:
                        accept_type =
                            base_type == lang_TypeInstance::DeterminedType::INTEGER
                            || base_type == lang_TypeInstance::DeterminedType::REAL;
                        break;
                    case AstValueBinaryOperator::MODULO:
                        accept_type =
                            base_type == lang_TypeInstance::DeterminedType::INTEGER
                            || base_type == lang_TypeInstance::DeterminedType::REAL;
                        break;
                    case AstValueBinaryOperator::LOGICAL_AND:
                        accept_type =
                            base_type == lang_TypeInstance::DeterminedType::BOOLEAN;
                        break;
                    case AstValueBinaryOperator::LOGICAL_OR:
                        accept_type =
                            base_type == lang_TypeInstance::DeterminedType::BOOLEAN;
                        break;
                    case AstValueBinaryOperator::GREATER:
                        accept_type =
                            base_type == lang_TypeInstance::DeterminedType::INTEGER
                            || base_type == lang_TypeInstance::DeterminedType::REAL
                            || base_type == lang_TypeInstance::DeterminedType::HANDLE
                            || base_type == lang_TypeInstance::DeterminedType::STRING;
                        break;
                    case AstValueBinaryOperator::GREATER_EQUAL:
                        accept_type =
                            base_type == lang_TypeInstance::DeterminedType::INTEGER
                            || base_type == lang_TypeInstance::DeterminedType::REAL
                            || base_type == lang_TypeInstance::DeterminedType::HANDLE
                            || base_type == lang_TypeInstance::DeterminedType::STRING;
                        break;
                    case AstValueBinaryOperator::LESS:
                        accept_type =
                            base_type == lang_TypeInstance::DeterminedType::INTEGER
                            || base_type == lang_TypeInstance::DeterminedType::REAL
                            || base_type == lang_TypeInstance::DeterminedType::HANDLE
                            || base_type == lang_TypeInstance::DeterminedType::STRING;
                        break;
                    case AstValueBinaryOperator::LESS_EQUAL:
                        accept_type =
                            base_type == lang_TypeInstance::DeterminedType::INTEGER
                            || base_type == lang_TypeInstance::DeterminedType::REAL
                            || base_type == lang_TypeInstance::DeterminedType::HANDLE
                            || base_type == lang_TypeInstance::DeterminedType::STRING;
                        break;
                    case AstValueBinaryOperator::EQUAL:
                        accept_type =
                            base_type == lang_TypeInstance::DeterminedType::INTEGER
                            || base_type == lang_TypeInstance::DeterminedType::REAL
                            || base_type == lang_TypeInstance::DeterminedType::HANDLE
                            || base_type == lang_TypeInstance::DeterminedType::STRING
                            || base_type == lang_TypeInstance::DeterminedType::BOOLEAN;
                        break;
                    case AstValueBinaryOperator::NOT_EQUAL:
                        accept_type =
                            base_type == lang_TypeInstance::DeterminedType::INTEGER
                            || base_type == lang_TypeInstance::DeterminedType::REAL
                            || base_type == lang_TypeInstance::DeterminedType::HANDLE
                            || base_type == lang_TypeInstance::DeterminedType::STRING
                            || base_type == lang_TypeInstance::DeterminedType::BOOLEAN;
                        break;
                    default:
                        wo_error("Unknown operator.");
                    }

                    if (!accept_type)
                    {
                        lex.record_lang_error(lexer::msglevel_t::error, node->m_left,
                            WO_ERR_UNACCEPTABLE_TYPE_IN_OPERATE,
                            get_type_name_w(left_type));

                        return FAILED;
                    }

                    if (node->m_operator > AstValueBinaryOperator::LOGICAL_AND)
                        node->m_LANG_determined_type = m_origin_types.m_bool.m_type_instance;
                    else
                        node->m_LANG_determined_type = left_type;

                    // Check for divide by zero.
                    if ((node->m_operator == AstValueBinaryOperator::DIVIDE
                        || node->m_operator == AstValueBinaryOperator::MODULO)
                        && base_type == lang_TypeInstance::DeterminedType::INTEGER
                        && node->m_right->m_evaled_const_value.has_value())
                    {
                        wo_integer_t right_int_value = node->m_right->m_evaled_const_value.value().integer;
                        if (right_int_value == 0)
                        {
                            lex.record_lang_error(lexer::msglevel_t::error, node->m_right, WO_ERR_BAD_DIV_ZERO);
                            return FAILED;
                        }
                        else if (right_int_value == -1
                            && node->m_left->m_evaled_const_value.has_value()
                            && node->m_left->m_evaled_const_value.value().integer == INT64_MIN)
                        {
                            lex.record_lang_error(lexer::msglevel_t::error, node, WO_ERR_BAD_DIV_OVERFLOW);
                            return FAILED;
                        }
                    }

                    bool constant_has_been_determined = false;
                    if (node->m_left->m_evaled_const_value.has_value())
                    {
                        wo::value left_value = node->m_left->m_evaled_const_value.value();
                        switch (node->m_operator)
                        {
                        case AstValueBinaryOperator::LOGICAL_AND:
                            if (left_value.integer == 0)
                            {
                                node->decide_final_constant_value(wo::value{});

                                wo::value& result_value = node->m_evaled_const_value.value();
                                result_value.set_bool(false);
                                constant_has_been_determined = true;
                            }
                            break;
                        case AstValueBinaryOperator::LOGICAL_OR:
                            if (left_value.integer != 0)
                            {
                                node->decide_final_constant_value(wo::value{});

                                wo::value& result_value = node->m_evaled_const_value.value();
                                result_value.set_bool(true);
                                constant_has_been_determined = true;
                            }
                            break;
                        }
                    }

                    if (!constant_has_been_determined
                        && node->m_left->m_evaled_const_value.has_value()
                        && node->m_right->m_evaled_const_value.has_value())
                    {
                        // Decide constant value.
                        node->decide_final_constant_value(wo::value{});

                        wo::value& result_value = node->m_evaled_const_value.value();
                        wo::value left_value = node->m_left->m_evaled_const_value.value();
                        wo::value right_value = node->m_right->m_evaled_const_value.value();
                        wo_assert(left_value.type == right_value.type);

                        switch (node->m_operator)
                        {
                        case AstValueBinaryOperator::ADD:
                            switch (left_value.type)
                            {
                            case wo::value::valuetype::integer_type:
                                result_value.set_integer(left_value.integer + right_value.integer);
                                break;
                            case wo::value::valuetype::real_type:
                                result_value.set_real(left_value.real + right_value.real);
                                break;
                            case wo::value::valuetype::string_type:
                                result_value.set_string_nogc(*left_value.string + *right_value.string);
                                break;
                            case wo::value::valuetype::handle_type:
                                result_value.set_handle(left_value.handle + right_value.handle);
                                break;
                            default:
                                wo_error("Unknown type.");
                            }
                            break;
                        case AstValueBinaryOperator::SUBSTRACT:
                            switch (left_value.type)
                            {
                            case wo::value::valuetype::integer_type:
                                result_value.set_integer(left_value.integer - right_value.integer);
                                break;
                            case wo::value::valuetype::real_type:
                                result_value.set_real(left_value.real - right_value.real);
                                break;
                            case wo::value::valuetype::handle_type:
                                result_value.set_handle(left_value.handle - right_value.handle);
                                break;
                            default:
                                wo_error("Unknown type.");
                            }
                            break;
                        case AstValueBinaryOperator::MULTIPLY:
                            switch (left_value.type)
                            {
                            case wo::value::valuetype::integer_type:
                                result_value.set_integer(left_value.integer * right_value.integer);
                                break;
                            case wo::value::valuetype::real_type:
                                result_value.set_real(left_value.real * right_value.real);
                                break;
                            default:
                                wo_error("Unknown type.");
                            }
                            break;
                        case AstValueBinaryOperator::DIVIDE:
                            switch (left_value.type)
                            {
                            case wo::value::valuetype::integer_type:
                                result_value.set_integer(left_value.integer / right_value.integer);
                                break;
                            case wo::value::valuetype::real_type:
                                result_value.set_real(left_value.real / right_value.real);
                                break;
                            default:
                                wo_error("Unknown type.");
                            }
                            break;
                        case AstValueBinaryOperator::MODULO:
                            switch (left_value.type)
                            {
                            case wo::value::valuetype::integer_type:
                                result_value.set_integer(left_value.integer % right_value.integer);
                                break;
                            case wo::value::valuetype::real_type:
                                result_value.set_real(fmod(left_value.real, right_value.real));
                                break;
                            default:
                                wo_error("Unknown type.");
                            }
                            break;
                        case AstValueBinaryOperator::LOGICAL_AND:
                            result_value.set_bool((bool)left_value.integer && (bool)right_value.integer);
                            break;
                        case AstValueBinaryOperator::LOGICAL_OR:
                            result_value.set_bool((bool)left_value.integer || (bool)right_value.integer);
                            break;
                        case AstValueBinaryOperator::GREATER:
                            switch (left_value.type)
                            {
                            case wo::value::valuetype::integer_type:
                                result_value.set_bool(left_value.integer > right_value.integer);
                                break;
                            case wo::value::valuetype::real_type:
                                result_value.set_bool(left_value.real > right_value.real);
                                break;
                            case wo::value::valuetype::string_type:
                                result_value.set_bool(*left_value.string > *right_value.string);
                                break;
                            case wo::value::valuetype::handle_type:
                                result_value.set_bool(left_value.handle > right_value.handle);
                                break;
                            default:
                                wo_error("Unknown type.");
                            }
                            break;
                        case AstValueBinaryOperator::GREATER_EQUAL:
                            switch (left_value.type)
                            {
                            case wo::value::valuetype::integer_type:
                                result_value.set_bool(left_value.integer >= right_value.integer);
                                break;
                            case wo::value::valuetype::real_type:
                                result_value.set_bool(left_value.real >= right_value.real);
                                break;
                            case wo::value::valuetype::string_type:
                                result_value.set_bool(*left_value.string >= *right_value.string);
                                break;
                            case wo::value::valuetype::handle_type:
                                result_value.set_bool(left_value.handle >= right_value.handle);
                                break;
                            default:
                                wo_error("Unknown type.");
                            }
                            break;
                        case AstValueBinaryOperator::LESS:
                            switch (left_value.type)
                            {
                            case wo::value::valuetype::integer_type:
                                result_value.set_bool(left_value.integer < right_value.integer);
                                break;
                            case wo::value::valuetype::real_type:
                                result_value.set_bool(left_value.real < right_value.real);
                                break;
                            case wo::value::valuetype::string_type:
                                result_value.set_bool(*left_value.string < *right_value.string);
                                break;
                            case wo::value::valuetype::handle_type:
                                result_value.set_bool(left_value.handle < right_value.handle);
                                break;
                            default:
                                wo_error("Unknown type.");
                            }
                            break;
                        case AstValueBinaryOperator::LESS_EQUAL:
                            switch (left_value.type)
                            {
                            case wo::value::valuetype::integer_type:
                                result_value.set_bool(left_value.integer <= right_value.integer);
                                break;
                            case wo::value::valuetype::real_type:
                                result_value.set_bool(left_value.real <= right_value.real);
                                break;
                            case wo::value::valuetype::string_type:
                                result_value.set_bool(*left_value.string <= *right_value.string);
                                break;
                            case wo::value::valuetype::handle_type:
                                result_value.set_bool(left_value.handle <= right_value.handle);
                                break;
                            default:
                                wo_error("Unknown type.");
                            }
                            break;
                        case AstValueBinaryOperator::EQUAL:
                            switch (left_value.type)
                            {
                            case wo::value::valuetype::integer_type:
                                result_value.set_bool(left_value.integer == right_value.integer);
                                break;
                            case wo::value::valuetype::real_type:
                                result_value.set_bool(left_value.real == right_value.real);
                                break;
                            case wo::value::valuetype::string_type:
                                result_value.set_bool(*left_value.string == *right_value.string);
                                break;
                            case wo::value::valuetype::handle_type:
                                result_value.set_bool(left_value.handle == right_value.handle);
                                break;
                            case wo::value::valuetype::bool_type:
                                result_value.set_bool(left_value.integer == right_value.integer);
                                break;
                            default:
                                wo_error("Unknown type.");
                            }
                            break;
                        case AstValueBinaryOperator::NOT_EQUAL:
                            switch (left_value.type)
                            {
                            case wo::value::valuetype::integer_type:
                                result_value.set_bool(left_value.integer != right_value.integer);
                                break;
                            case wo::value::valuetype::real_type:
                                result_value.set_bool(left_value.real != right_value.real);
                                break;
                            case wo::value::valuetype::string_type:
                                result_value.set_bool(*left_value.string != *right_value.string);
                                break;
                            case wo::value::valuetype::handle_type:
                                result_value.set_bool(left_value.handle != right_value.handle);
                                break;
                            case wo::value::valuetype::bool_type:
                                result_value.set_bool(left_value.integer != right_value.integer);
                                break;
                            default:
                                wo_error("Unknown type.");
                            }
                            break;
                        default:
                            wo_error("Unknown operator.");
                        }
                    }

                    break;
                }
            }
            case AstValueBinaryOperator::HOLD_FOR_OVERLOAD_FUNCTION_CALL_EVAL:
            {
                AstValueFunctionCall* overload_function_call =
                    node->m_LANG_overload_call.value();

                node->m_LANG_determined_type =
                    overload_function_call->m_LANG_determined_type;
                break;
            }
            default:
                wo_error("Unknown hold state.");
            }
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueUnaryOperator)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_operand);
            return HOLD;
        }
        else if (state == HOLD)
        {
            switch (node->m_operator)
            {
            case AstValueUnaryOperator::NEGATIVE:
            {
                lang_TypeInstance* operand_type = node->m_operand->m_LANG_determined_type.value();
                auto detrmined_type = operand_type->get_determined_type();
                if (!detrmined_type.has_value())
                {
                    lex.record_lang_error(lexer::msglevel_t::error, node->m_operand,
                        WO_ERR_TYPE_NAMED_DETERMINED_FAILED,
                        get_type_name_w(operand_type));

                    return FAILED;
                }

                auto* detrmined_base_type = detrmined_type.value();
                if (detrmined_base_type->m_base_type != lang_TypeInstance::DeterminedType::INTEGER
                    && detrmined_base_type->m_base_type != lang_TypeInstance::DeterminedType::REAL)
                {
                    lex.record_lang_error(lexer::msglevel_t::error, node->m_operand,
                        WO_ERR_UNACCEPTABLE_TYPE_IN_OPERATE,
                        get_type_name_w(operand_type));

                    return FAILED;
                }

                node->m_LANG_determined_type = operand_type;

                if (node->m_operand->m_evaled_const_value.has_value())
                {
                    wo::value result_value;
                    wo::value operand_value = node->m_operand->m_evaled_const_value.value();

                    switch (operand_value.type)
                    {
                    case wo::value::valuetype::integer_type:
                        result_value.set_integer(-operand_value.integer);
                        break;
                    case wo::value::valuetype::real_type:
                        result_value.set_real(-operand_value.real);
                        break;
                    default:
                        wo_error("Unknown type.");
                    }
                    node->decide_final_constant_value(result_value);
                }
                break;
            }
            case AstValueUnaryOperator::LOGICAL_NOT:
            {
                lang_TypeInstance* operand_type = node->m_operand->m_LANG_determined_type.value();
                auto detrmined_type = operand_type->get_determined_type();
                if (!detrmined_type.has_value())
                {
                    lex.record_lang_error(lexer::msglevel_t::error, node->m_operand,
                        WO_ERR_TYPE_NAMED_DETERMINED_FAILED,
                        get_type_name_w(operand_type));

                    return FAILED;
                }

                auto* detrmined_base_type = detrmined_type.value();
                if (detrmined_base_type->m_base_type != lang_TypeInstance::DeterminedType::BOOLEAN)
                {
                    lex.record_lang_error(lexer::msglevel_t::error, node->m_operand,
                        WO_ERR_UNACCEPTABLE_TYPE_IN_OPERATE,
                        get_type_name_w(operand_type));

                    return FAILED;
                }

                node->m_LANG_determined_type = operand_type;

                if (node->m_operand->m_evaled_const_value.has_value())
                {
                    wo::value result_value;
                    wo::value operand_value = node->m_operand->m_evaled_const_value.value();

                    result_value.set_bool(!operand_value.integer);
                    node->decide_final_constant_value(result_value);
                }
                break;
            }
            default:
                wo_error("Unknown operator.");
            }
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueTribleOperator)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_condition);

            node->m_LANG_hold_state = AstValueTribleOperator::HOLD_FOR_COND_EVAL;
            return HOLD;
        }
        else if (state == HOLD)
        {
            switch (node->m_LANG_hold_state)
            {
            case AstValueTribleOperator::HOLD_FOR_COND_EVAL:
            {
                lang_TypeInstance* type_instance =
                    node->m_condition->m_LANG_determined_type.value();

                if (immutable_type(type_instance) != m_origin_types.m_bool.m_type_instance)
                {
                    lex.record_lang_error(lexer::msglevel_t::error, node->m_condition,
                        WO_ERR_UNACCEPTABLE_TYPE_IN_COND,
                        get_type_name_w(type_instance));

                    return FAILED;
                }

                // Conditional compile.
                if (node->m_condition->m_evaled_const_value.has_value())
                {
                    if (node->m_condition->m_evaled_const_value.value().integer)
                        // TRUE BRANCH.
                        WO_CONTINUE_PROCESS(node->m_true_value);
                    else
                        // FALSE BRANCH
                        WO_CONTINUE_PROCESS(node->m_false_value);
                }
                else
                {
                    WO_CONTINUE_PROCESS(node->m_false_value);
                    WO_CONTINUE_PROCESS(node->m_true_value);
                }

                node->m_LANG_hold_state = AstValueTribleOperator::HOLD_FOR_BRANCH_EVAL;
                return HOLD;
            }
            case AstValueTribleOperator::HOLD_FOR_BRANCH_EVAL:
            {
                lang_TypeInstance* node_final_type;
                if (node->m_condition->m_evaled_const_value.has_value())
                {
                    if (node->m_condition->m_evaled_const_value.value().integer)
                    {
                        // TRUE BRANCH.
                        node_final_type = node->m_true_value->m_LANG_determined_type.value();
                        if (node->m_true_value->m_evaled_const_value.has_value())
                            node->decide_final_constant_value(
                                node->m_true_value->m_evaled_const_value.value());
                    }
                    else
                    {
                        // FALSE BRANCH
                        node_final_type = node->m_false_value->m_LANG_determined_type.value();
                        if (node->m_false_value->m_evaled_const_value.has_value())
                            node->decide_final_constant_value(
                                node->m_false_value->m_evaled_const_value.value());
                    }
                }
                else
                {
                    auto* true_type_instance = node->m_true_value->m_LANG_determined_type.value();
                    auto* false_type_instance = node->m_false_value->m_LANG_determined_type.value();

                    if (node->m_LANG_template_evalating_state_is_mutable.has_value())
                    {
                        auto& eval_content = node->m_LANG_template_evalating_state_is_mutable.value();
                        finish_eval_template_ast(lex, eval_content.first);

                        node_final_type =
                            eval_content.second
                            ? mutable_type(eval_content.first->m_type_instance.get())
                            : eval_content.first->m_type_instance.get();
                    }
                    else
                    {
                        auto mixture_branch_type = easy_mixture_types(
                            lex,
                            node,
                            true_type_instance,
                            false_type_instance,
                            out_stack);

                        node_final_type = true_type_instance;
                        if (mixture_branch_type.m_state == TypeMixtureResult::ACCEPT)
                        {
                            node_final_type = mixture_branch_type.m_result;
                        }
                        else if (mixture_branch_type.m_state == TypeMixtureResult::TEMPLATE_MUTABLE
                            || mixture_branch_type.m_state == TypeMixtureResult::TEMPLATE_NORMAL)
                        {
                            node->m_LANG_template_evalating_state_is_mutable = std::make_pair(
                                mixture_branch_type.m_template_instance,
                                mixture_branch_type.m_state == TypeMixtureResult::TEMPLATE_MUTABLE);

                            return HOLD;
                        }
                        else
                        {
                            lex.record_lang_error(lexer::msglevel_t::error, node,
                                WO_ERR_UNABLE_TO_MIX_TYPES,
                                get_type_name_w(true_type_instance),
                                get_type_name_w(false_type_instance));

                            lex.record_lang_error(lexer::msglevel_t::infom, node->m_true_value,
                                WO_INFO_THIS_VALUE_IS_TYPE_NAMED,
                                get_type_name_w(true_type_instance));

                            lex.record_lang_error(lexer::msglevel_t::infom, node->m_false_value,
                                WO_INFO_THIS_VALUE_IS_TYPE_NAMED,
                                get_type_name_w(false_type_instance));

                            return FAILED;
                        }
                    }

                    bool failed = false;
                    if (lang_TypeInstance::TypeCheckResult::ACCEPT
                        != is_type_accepted(
                            lex,
                            node,
                            node_final_type,
                            true_type_instance))
                    {
                        lex.record_lang_error(lexer::msglevel_t::error, node->m_true_value,
                            WO_ERR_CANNOT_ACCEPTABLE_TYPE_NAMED,
                            get_type_name_w(node_final_type),
                            get_type_name_w(true_type_instance));

                        failed = true;
                    }

                    if (lang_TypeInstance::TypeCheckResult::ACCEPT
                        != is_type_accepted(
                            lex,
                            node,
                            node_final_type,
                            false_type_instance))
                    {
                        lex.record_lang_error(lexer::msglevel_t::error, node->m_false_value,
                            WO_ERR_CANNOT_ACCEPTABLE_TYPE_NAMED,
                            get_type_name_w(node_final_type),
                            get_type_name_w(false_type_instance));

                        failed = true;
                    }

                    if (failed)
                        return FAILED;
                }

                node->m_LANG_determined_type = node_final_type;
                break;
            }
            default:
                wo_error("Unexpected hold state.");
            }
        }
        else
        {
            if (node->m_LANG_template_evalating_state_is_mutable.has_value())
            {
                auto& eval_content = node->m_LANG_template_evalating_state_is_mutable.value();
                failed_eval_template_ast(lex, node, eval_content.first);
            }

        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstValueVariadicArgumentsPack)
    {
        wo_assert(state == UNPROCESSED);

        auto current_function = get_current_function();
        if (!current_function.has_value() || !current_function.value()->m_is_variadic)
        {
            lex.record_lang_error(lexer::msglevel_t::error, node, WO_ERR_UNEXPECTED_PACKEDARGS);
            return FAILED;
        }
        node->m_LANG_function_instance = current_function;
        node->m_LANG_determined_type = m_origin_types.m_array_dynamic;
        return OKAY;
    }
    WO_PASS_PROCESSER(AstMatch)
    {
        if (state == UNPROCESSED)
        {
            begin_new_scope(node->source_location);

            WO_CONTINUE_PROCESS(node->m_matched_value);

            node->m_LANG_hold_state = AstMatch::HOLD_FOR_EVAL_MATCHING_VALUE;
            return HOLD;
        }
        else if (state == HOLD)
        {
            switch (node->m_LANG_hold_state)
            {
            case AstMatch::HOLD_FOR_EVAL_MATCHING_VALUE:
            {
                lang_TypeInstance* matching_typeinstance =
                    node->m_matched_value->m_LANG_determined_type.value();

                // Check is union.
                auto determined_base_type =
                    matching_typeinstance->get_determined_type();
                if (!determined_base_type.has_value())
                {
                    end_last_scope();

                    lex.record_lang_error(lexer::msglevel_t::error, node->m_matched_value,
                        WO_ERR_TYPE_NAMED_DETERMINED_FAILED,
                        get_type_name_w(matching_typeinstance));

                    return FAILED;
                }

                auto* determined_base_type_instance =
                    determined_base_type.value();

                if (determined_base_type_instance->m_base_type
                    != lang_TypeInstance::DeterminedType::UNION)
                {
                    end_last_scope();

                    lex.record_lang_error(lexer::msglevel_t::error, node->m_matched_value,
                        WO_ERR_UNEXPECTED_MACTHING_TYPE,
                        get_type_name_w(matching_typeinstance));

                    return FAILED;
                }

                auto* determined_base_type_instance_union_dat =
                    determined_base_type_instance->m_external_type_description.m_union;

                // Check if branch covered.
                std::unordered_set<wo_pstring_t> covered_branch;
                bool has_take_place_pattern = false;

                for (AstMatchCase* match_case : node->m_cases)
                {
                    if (has_take_place_pattern)
                    {
                        end_last_scope();

                        lex.record_lang_error(lexer::msglevel_t::error, match_case,
                            WO_ERR_TAKEPLACE_PATTERN_MATCHED);
                        return FAILED;
                    }
                    switch (match_case->m_pattern->node_type)
                    {
                    case AstBase::AST_PATTERN_UNION:
                    {
                        AstPatternUnion* union_pattern =
                            static_cast<AstPatternUnion*>(match_case->m_pattern);
                        if (!covered_branch.insert(union_pattern->m_tag).second)
                        {
                            end_last_scope();

                            lex.record_lang_error(lexer::msglevel_t::error, union_pattern,
                                WO_ERR_EXISTS_CASE_NAMED_IN_MATCH,
                                union_pattern->m_tag->c_str());

                            return FAILED;
                        }

                        auto fnd = determined_base_type_instance_union_dat->m_union_label.find(union_pattern->m_tag);
                        if (determined_base_type_instance_union_dat->m_union_label.end() == fnd)
                        {
                            end_last_scope();

                            lex.record_lang_error(lexer::msglevel_t::error, union_pattern,
                                WO_ERR_UNEXISTS_CASE_NAMED_IN_MATCH,
                                union_pattern->m_tag->c_str());

                            return FAILED;
                        }

                        bool pattern_include_value = union_pattern->m_field.has_value();
                        if (pattern_include_value != fnd->second.m_item_type.has_value())
                        {
                            end_last_scope();

                            if (pattern_include_value)
                                lex.record_lang_error(lexer::msglevel_t::error, union_pattern,
                                    WO_ERR_HAVE_VALUE_CASE_IN_MATCH,
                                    get_type_name_w(matching_typeinstance),
                                    union_pattern->m_tag->c_str());
                            else
                                lex.record_lang_error(lexer::msglevel_t::error, union_pattern,
                                    WO_ERR_HAVE_NOT_VALUE_CASE_IN_MATCH,
                                    get_type_name_w(matching_typeinstance),
                                    union_pattern->m_tag->c_str());

                            return FAILED;
                        }

                        match_case->m_LANG_pattern_value_apply_type = fnd->second.m_item_type;
                        match_case->m_LANG_case_label_or_takeplace = fnd->second.m_label;

                        break;
                    }
                    case AstBase::AST_PATTERN_TAKEPLACE:
                    {
                        match_case->m_LANG_pattern_value_apply_type = std::nullopt;
                        match_case->m_LANG_case_label_or_takeplace = std::nullopt;

                        has_take_place_pattern = true;
                        break;
                    }
                    default:
                        wo_error("Unknown pattern.");
                    }
                }

                if (!has_take_place_pattern
                    && covered_branch.size() != determined_base_type_instance_union_dat->m_union_label.size())
                {
                    end_last_scope();

                    lex.record_lang_error(lexer::msglevel_t::error, node,
                        WO_ERR_ALL_CASES_SHOULD_BE_MATCHED);
                    return FAILED;
                }

                WO_CONTINUE_PROCESS_LIST(node->m_cases);

                node->m_LANG_hold_state = AstMatch::HOLD_FOR_EVAL_CASES;
                return HOLD;
            }
            case AstMatch::HOLD_FOR_EVAL_CASES:
                // Cases has been evaled.
                end_last_scope();
                break;
            default:
                wo_error("Unknown hold state.");
            }
        }
        else
        {
            end_last_scope();
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstMatchCase)
    {
        if (state == UNPROCESSED)
        {
            begin_new_scope(node->source_location);

            // Decalare pattern if contained.
            switch (node->m_pattern->node_type)
            {
            case AstBase::AST_PATTERN_UNION:
            {
                AstPatternUnion* union_pattern = static_cast<AstPatternUnion*>(node->m_pattern);
                if (union_pattern->m_field.has_value())
                {
                    AstPatternBase* apply_pattern = union_pattern->m_field.value();
                    if (!declare_pattern_symbol_pass0_1(
                        lex,
                        false,
                        std::nullopt,
                        apply_pattern,
                        apply_pattern,
                        std::nullopt))
                    {
                        // Failed.
                        end_last_scope();
                        return FAILED;
                    }

                    // Update pattern symbol type.
                    if (!update_pattern_symbol_variable_type_pass1(
                        lex,
                        apply_pattern,
                        std::nullopt,
                        node->m_LANG_pattern_value_apply_type.value()))
                    {
                        // Failed.
                        end_last_scope();
                        return FAILED;
                    }
                }
                break;
            }
            case AstBase::AST_PATTERN_TAKEPLACE:
                break;
            }

            WO_CONTINUE_PROCESS(node->m_body);
            return HOLD;
        }
        else if (state == HOLD)
        {
            end_last_scope();
        }
        else
        {
            end_last_scope();
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstIf)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_condition);

            node->m_LANG_hold_state = AstIf::HOLD_FOR_CONDITION_EVAL;
            return HOLD;
        }
        else if (state == HOLD)
        {
            switch (node->m_LANG_hold_state)
            {
            case AstIf::HOLD_FOR_CONDITION_EVAL:
            {
                lang_TypeInstance* condition_typeinstance =
                    node->m_condition->m_LANG_determined_type.value();

                if (condition_typeinstance != m_origin_types.m_bool.m_type_instance)
                {
                    lex.record_lang_error(lexer::msglevel_t::error, node->m_condition,
                        WO_ERR_UNACCEPTABLE_TYPE_IN_COND,
                        get_type_name_w(condition_typeinstance));
                    return FAILED;
                }

                if (node->m_condition->m_evaled_const_value.has_value())
                {
                    if (node->m_condition->m_evaled_const_value.value().integer)
                        WO_CONTINUE_PROCESS(node->m_true_body);
                    else if (node->m_false_body.has_value())
                        WO_CONTINUE_PROCESS(node->m_false_body.value());
                }
                else
                {
                    if (node->m_false_body.has_value())
                        WO_CONTINUE_PROCESS(node->m_false_body.value());

                    WO_CONTINUE_PROCESS(node->m_true_body);
                }

                node->m_LANG_hold_state = AstIf::HOLD_FOR_BODY_EVAL;
                return HOLD;
            }
            case AstIf::HOLD_FOR_BODY_EVAL:
                break;
            default:
                wo_error("Unknown hold state.");
            }
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstWhile)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_condition);

            node->m_LANG_hold_state = AstWhile::HOLD_FOR_CONDITION_EVAL;
            return HOLD;
        }
        else if (state == HOLD)
        {
            switch (node->m_LANG_hold_state)
            {
            case AstWhile::HOLD_FOR_CONDITION_EVAL:
            {
                lang_TypeInstance* condition_typeinstance =
                    node->m_condition->m_LANG_determined_type.value();

                if (condition_typeinstance != m_origin_types.m_bool.m_type_instance)
                {
                    lex.record_lang_error(lexer::msglevel_t::error, node->m_condition,
                        WO_ERR_UNACCEPTABLE_TYPE_IN_COND,
                        get_type_name_w(condition_typeinstance));
                    return FAILED;
                }

                WO_CONTINUE_PROCESS(node->m_body);

                node->m_LANG_hold_state = AstWhile::HOLD_FOR_BODY_EVAL;
                return HOLD;
            }
            case AstWhile::HOLD_FOR_BODY_EVAL:
                break;
            default:
                wo_error("Unknown hold state.");
            }
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstFor)
    {
        if (state == UNPROCESSED)
        {
            begin_new_scope(node->source_location);

            if (node->m_initial.has_value())
                WO_CONTINUE_PROCESS(node->m_initial.value());

            node->m_LANG_hold_state = AstFor::HOLD_FOR_INITIAL_EVAL;
            return HOLD;
        }
        else if (state == HOLD)
        {
            switch (node->m_LANG_hold_state)
            {
            case AstFor::HOLD_FOR_INITIAL_EVAL:
            {
                if (node->m_condition.has_value())
                {
                    WO_CONTINUE_PROCESS(node->m_condition.value());

                    node->m_LANG_hold_state = AstFor::HOLD_FOR_CONDITION_EVAL;
                    return HOLD;
                }
                /*FALL THROUGH*/
            }
            [[fallthrough]];
            case AstFor::HOLD_FOR_CONDITION_EVAL:
            {
                if (node->m_condition.has_value())
                {
                    lang_TypeInstance* condition_typeinstance =
                        node->m_condition.value()->m_LANG_determined_type.value();

                    if (condition_typeinstance != m_origin_types.m_bool.m_type_instance)
                    {
                        lex.record_lang_error(lexer::msglevel_t::error, node->m_condition.value(),
                            WO_ERR_UNACCEPTABLE_TYPE_IN_COND,
                            get_type_name_w(condition_typeinstance));

                        end_last_scope();
                        return FAILED;
                    }
                }
                if (node->m_step.has_value())
                {
                    WO_CONTINUE_PROCESS(node->m_step.value());

                    node->m_LANG_hold_state = AstFor::HOLD_FOR_STEP_EVAL;
                    return HOLD;
                }
                /*FALL THROUGH*/
            }
            [[fallthrough]];
            case AstFor::HOLD_FOR_STEP_EVAL:
            {
                WO_CONTINUE_PROCESS(node->m_body);

                node->m_LANG_hold_state = AstFor::HOLD_FOR_BODY_EVAL;
                return HOLD;
            }
            case AstFor::HOLD_FOR_BODY_EVAL:
                end_last_scope();
                break;
            default:
                wo_error("Unknown hold state.");
            }
        }
        else
        {
            end_last_scope();
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstForeach)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_forloop_body);
            return HOLD;
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstBreak)
    {
        wo_assert(state == UNPROCESSED);
        // Nothing todo.
        return OKAY;
    }
    WO_PASS_PROCESSER(AstContinue)
    {
        wo_assert(state == UNPROCESSED);
        // Nothing todo.
        return OKAY;
    }
    WO_PASS_PROCESSER(AstValueAssign)
    {
        if (state == UNPROCESSED)
        {
            WO_CONTINUE_PROCESS(node->m_right);
            WO_CONTINUE_PROCESS(node->m_assign_place);

            node->m_LANG_hold_state = AstValueAssign::HOLD_FOR_OPNUM_EVAL;
            return HOLD;
        }
        else if (state == HOLD)
        {
            switch (node->m_LANG_hold_state)
            {
            case AstValueAssign::HOLD_FOR_OPNUM_EVAL:
            {
                AstValueBase* left_value;
                switch (node->m_assign_place->node_type)
                {
                case AstBase::AST_PATTERN_VARIABLE:
                {
                    AstPatternVariable* pattern_variable =
                        static_cast<AstPatternVariable*>(node->m_assign_place);

                    left_value = pattern_variable->m_variable;
                    break;
                }
                case AstBase::AST_PATTERN_INDEX:
                {
                    AstPatternIndex* pattern_index =
                        static_cast<AstPatternIndex*>(node->m_assign_place);

                    left_value = pattern_index->m_index;
                    break;
                }
                default:
                    wo_error("Unknown assign place.");
                }

                // Check if operator overload.
                std::optional<wo_pstring_t> operator_name;
                switch (node->m_assign_type)
                {
                case AstValueAssign::ASSIGN:
                    // Nothing todo.
                    break;
                case AstValueAssign::ADD_ASSIGN:
                    operator_name = WO_PSTR(operator_ADD);
                    break;
                case AstValueAssign::SUBSTRACT_ASSIGN:
                    operator_name = WO_PSTR(operator_SUB);
                    break;
                case AstValueAssign::MULTIPLY_ASSIGN:
                    operator_name = WO_PSTR(operator_MUL);
                    break;
                case AstValueAssign::DIVIDE_ASSIGN:
                    operator_name = WO_PSTR(operator_DIV);
                    break;
                case AstValueAssign::MODULO_ASSIGN:
                    operator_name = WO_PSTR(operator_MOD);
                    break;
                default:
                    wo_error("Unknown assign type.");
                }

                lang_TypeInstance* left_type = left_value->m_LANG_determined_type.value();

                if (operator_name.has_value())
                {
                    wo_pstring_t operator_name_str = operator_name.value();

                    AstIdentifier* operator_identifier = new AstIdentifier(operator_name_str);
                    operator_identifier->m_formal = AstIdentifier::identifier_formal::FROM_TYPE;
                    operator_identifier->m_from_type = left_type;
                    operator_identifier->m_find_type_only = false;
                    operator_identifier->duplicated_node = node->duplicated_node;

                    // Update source location.
                    operator_identifier->source_location = node->source_location;

                    bool ambiguous = false;
                    if (find_symbol_in_current_scope(lex, operator_identifier, &ambiguous))
                    {
                        // Has overload function.
                        AstValueVariable* overload_function = new AstValueVariable(operator_identifier);
                        overload_function->duplicated_node = node->duplicated_node;

                        AstValueFunctionCall* overload_function_call = new AstValueFunctionCall(
                            false /* symbol has ben determined */, overload_function, { left_value, node->m_right });

                        // Update source location.
                        overload_function->source_location = node->source_location;
                        overload_function_call->source_location = node->source_location;

                        node->m_LANG_overload_call = overload_function_call;

                        WO_CONTINUE_PROCESS(overload_function_call);

                        node->m_LANG_hold_state = AstValueAssign::HOLD_FOR_OVERLOAD_FUNCTION_CALL_EVAL;
                        return HOLD;
                    }
                    else if (ambiguous)
                        return FAILED;
                    else
                        ; // No function overload, continue;
                }

                // No function overload, type check.
                // 1) Base typecheck.
                lang_TypeInstance* right_type = node->m_right->m_LANG_determined_type.value();

                if (lang_TypeInstance::TypeCheckResult::ACCEPT != is_type_accepted(
                    lex,
                    node,
                    left_type,
                    right_type))
                {
                    lex.record_lang_error(lexer::msglevel_t::error, node->m_right,
                        WO_ERR_CANNOT_ACCEPTABLE_TYPE_NAMED,
                        get_type_name_w(right_type),
                        get_type_name_w(left_type));

                    return FAILED;
                }

                auto left_base_type = left_type->get_determined_type();
                if (!left_base_type.has_value())
                {
                    lex.record_lang_error(lexer::msglevel_t::error, node->m_assign_place,
                        WO_ERR_TYPE_NAMED_DETERMINED_FAILED,
                        get_type_name_w(left_type));

                    return FAILED;
                }

                auto base_type = left_base_type.value()->m_base_type;
                bool accept_type = false;

                switch (node->m_assign_type)
                {
                case AstValueAssign::ASSIGN:
                    accept_type = true;
                    break;
                case AstValueAssign::ADD_ASSIGN:
                    accept_type =
                        base_type == lang_TypeInstance::DeterminedType::INTEGER
                        || base_type == lang_TypeInstance::DeterminedType::REAL
                        || base_type == lang_TypeInstance::DeterminedType::HANDLE
                        || base_type == lang_TypeInstance::DeterminedType::STRING;
                    break;
                case AstValueAssign::SUBSTRACT_ASSIGN:
                    accept_type =
                        base_type == lang_TypeInstance::DeterminedType::INTEGER
                        || base_type == lang_TypeInstance::DeterminedType::REAL
                        || base_type == lang_TypeInstance::DeterminedType::HANDLE;
                    break;
                case AstValueAssign::MULTIPLY_ASSIGN:
                    accept_type =
                        base_type == lang_TypeInstance::DeterminedType::INTEGER
                        || base_type == lang_TypeInstance::DeterminedType::REAL;
                    break;
                case AstValueAssign::DIVIDE_ASSIGN:
                    accept_type =
                        base_type == lang_TypeInstance::DeterminedType::INTEGER
                        || base_type == lang_TypeInstance::DeterminedType::REAL;
                    break;
                case AstValueAssign::MODULO_ASSIGN:
                    accept_type =
                        base_type == lang_TypeInstance::DeterminedType::INTEGER
                        || base_type == lang_TypeInstance::DeterminedType::REAL;
                    break;
                default:
                    wo_error("Unknown operator.");
                }

                if (!accept_type)
                {
                    lex.record_lang_error(lexer::msglevel_t::error, node->m_right,
                        WO_ERR_CANNOT_ACCEPTABLE_TYPE_NAMED,
                        get_type_name_w(right_type),
                        get_type_name_w(left_type));

                    return FAILED;
                }

                if (node->m_valued_assign)
                    node->m_LANG_determined_type = left_type;
                else
                    node->m_LANG_determined_type = m_origin_types.m_void.m_type_instance;

                if ((node->m_assign_type == AstValueAssign::DIVIDE_ASSIGN
                    || node->m_assign_type == AstValueAssign::MODULO_ASSIGN)
                    && base_type == lang_TypeInstance::DeterminedType::INTEGER
                    && node->m_right->m_evaled_const_value.has_value())
                {
                    wo_integer_t right_int_value = node->m_right->m_evaled_const_value.value().integer;
                    if (right_int_value == 0)
                    {
                        lex.record_lang_error(lexer::msglevel_t::error, node->m_right, WO_ERR_BAD_DIV_ZERO);
                        return FAILED;
                    }
                }

                break; // Finish.
            }
            case AstValueAssign::HOLD_FOR_OVERLOAD_FUNCTION_CALL_EVAL:
            {
                AstValueFunctionCall* overload_function_call =
                    node->m_LANG_overload_call.value();

                lang_TypeInstance* func_result_type =
                    overload_function_call->m_LANG_determined_type.value();
                lang_TypeInstance* left_type;

                switch (node->m_assign_place->node_type)
                {
                case AstBase::AST_PATTERN_VARIABLE:
                {
                    AstPatternVariable* pattern_variable =
                        static_cast<AstPatternVariable*>(node->m_assign_place);

                    left_type = pattern_variable->m_variable->m_LANG_determined_type.value();
                    break;
                }
                case AstBase::AST_PATTERN_INDEX:
                {
                    AstPatternIndex* pattern_index =
                        static_cast<AstPatternIndex*>(node->m_assign_place);

                    left_type = pattern_index->m_index->m_LANG_determined_type.value();
                    break;
                }
                default:
                    wo_error("Unknown assign place.");
                }

                if (lang_TypeInstance::TypeCheckResult::ACCEPT !=
                    is_type_accepted(lex, node, left_type, func_result_type))
                {
                    lex.record_lang_error(lexer::msglevel_t::error, node,
                        WO_ERR_CANNOT_ACCEPTABLE_TYPE_NAMED,
                        get_type_name_w(func_result_type),
                        get_type_name_w(left_type));

                    return FAILED;
                }

                if (node->m_valued_assign)
                    node->m_LANG_determined_type =
                    overload_function_call->m_LANG_determined_type;
                else
                    node->m_LANG_determined_type = m_origin_types.m_void.m_type_instance;

                break;
            }
            default:
                wo_error("Unknown hold state.");
            }
        }
        return WO_EXCEPT_ERROR(state, OKAY);
    }
    WO_PASS_PROCESSER(AstExternInformation)
    {
        wo_assert(state == UNPROCESSED);
        // Mark return type as extern.

        auto* function_instance = get_current_function().value();

        wo_native_func_t extern_function;

        if (node->m_extern_from_library.has_value())
        {
            if (config::DISABLE_LOAD_EXTERN_FUNCTION)
                extern_function = rslib_std_bad_function;
            else
                extern_function = rslib_extern_symbols::get_lib_symbol(
                    wstr_to_str(*node->source_location.source_file).c_str(),
                    wstr_to_str(*node->m_extern_from_library.value()).c_str(),
                    wstr_to_str(*node->m_extern_symbol).c_str(),
                    m_ircontext.m_extern_libs);
        }
        else
        {
            extern_function =
                rslib_extern_symbols::get_global_symbol(
                    wstr_to_str(*node->m_extern_symbol).c_str());
        }

        if (extern_function != nullptr)
            node->m_IR_externed_function = extern_function;
        else
        {
            lex.record_lang_error(lexer::msglevel_t::error, node,
                WO_ERR_UNABLE_TO_FIND_EXTERN_FUNCTION,
                node->m_extern_from_library.value_or(WO_PSTR(woolang))->c_str(),
                node->m_extern_symbol->c_str());

            return FAILED;
        }
        function_instance->m_IR_extern_information = node;

        function_instance->m_LANG_determined_return_type =
            function_instance->m_marked_return_type.value()->m_LANG_determined_type.value();

        return OKAY;
    }
    WO_PASS_PROCESSER(AstNop)
    {
        wo_assert(state == UNPROCESSED);
        return OKAY;
    }

#undef WO_PASS_PROCESSER

    LangContext::pass_behavior LangContext::pass_1_process_basic_type_marking_and_constant_eval(
        lexer& lex, const AstNodeWithState& node_state, PassProcessStackT& out_stack)
    {
        if (node_state.m_ast_node->finished_state != pass_behavior::UNPROCESSED)
        {
            if (node_state.m_ast_node->finished_state == pass_behavior::HOLD)
            {
                if (node_state.m_state != pass_behavior::HOLD
                    && node_state.m_state != pass_behavior::HOLD_BUT_CHILD_FAILED)
                {
                    // RECURSIVE EVALUATION.
                    lex.record_lang_error(lexer::msglevel_t::error, node_state.m_ast_node,
                        WO_ERR_RECURSIVE_EVAL_PASS1);

                    return FAILED;
                }
                else
                {
                    // CONTINUE PROCESSING.
                }
            }
            else
                return (LangContext::pass_behavior)node_state.m_ast_node->finished_state;
        }

        // PASS1 must process all nodes.
        wo_assert(
            node_state.m_ast_node->node_type == AstBase::AST_EMPTY
            || m_pass1_processers->check_has_processer(node_state.m_ast_node->node_type));

        auto result = m_pass1_processers->process_node(this, lex, node_state, out_stack);
        node_state.m_ast_node->finished_state = result;

        wo_assert(result != pass_behavior::UNPROCESSED);

        return result;
    }

#endif
}