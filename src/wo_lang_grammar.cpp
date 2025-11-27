#include "wo_afx.hpp"

#include "wo_crc_64.hpp"
#include "wo_lang_ast_builder.hpp"

#ifndef WO_DISABLE_COMPILER
#include "wo_lang_grammar_loader.hpp"
#include "wo_lang_grammar_lr1_autogen.hpp"
#endif

#include <fstream>

#define WO_GRAMMAR_SKIP_GEN_LR1_TABLE_CACHE 0
#define WO_ASTBUILDER_INDEX(...) ast::index<__VA_ARGS__>()

namespace wo
{
#ifndef WO_DISABLE_COMPILER
    static std::unique_ptr<grammar> grammar_instance = nullptr;

    void init_woolang_grammar()
    {
        wo_assert(grammar_instance == nullptr);
        ast::init_builder();

        const char* GRAMMAR_SRC_FILE = WO_SRC_PATH "/src/wo_lang_grammar.cpp";

        uint64_t wo_lang_grammar_crc64 = 0;
        bool wo_check_grammar_need_update = wo::config::ENABLE_CHECK_GRAMMAR_AND_UPDATE;
        if (wo_check_grammar_need_update)
        {
            wo_check_grammar_need_update = false;

            std::ifstream this_grammar_file(GRAMMAR_SRC_FILE);
            if (this_grammar_file.fail())
            {
#if !defined(WO_LANG_GRAMMAR_LR1_AUTO_GENED) || WO_GRAMMAR_SKIP_GEN_LR1_TABLE_CACHE
                wo_error("Fail to load grammar.");
#endif
            }
            else
            {
                wo_lang_grammar_crc64 = crc_64(this_grammar_file, 0);
#ifdef WO_LANG_GRAMMAR_CRC64
                if (wo_lang_grammar_crc64 != WO_LANG_GRAMMAR_CRC64)
#endif
                {
                    wo_check_grammar_need_update = true;
                }
            }
        }

#if defined(WO_LANG_GRAMMAR_LR1_AUTO_GENED) && !WO_GRAMMAR_SKIP_GEN_LR1_TABLE_CACHE

        if (!wo_check_grammar_need_update)
        {
            grammar_instance = std::make_unique<grammar>();

            wo_read_lr1_cache(*grammar_instance);
            wo_read_lr1_to(grammar_instance->LR1_TABLE);
            wo_read_follow_set_to(grammar_instance->FOLLOW_SET);
            wo_read_origin_p_to(grammar_instance->ORGIN_P);

            goto register_ast_builder_function;
        }
        else
        {
#endif
            wo_stdout <<
                ANSI_HIY
                "WooGramma: "
                ANSI_RST
                "Syntax update detected, LR table cache is being regenerating..."
                << wo_endl;

            using SL = wo::grammar::symlist;

            using namespace ast;

#define B WO_ASTBUILDER_INDEX
#define SL(...) wo::grammar::symlist{__VA_ARGS__}
#define TE(IDENT) wo::grammar::te(wo::grammar::ttype::IDENT)
#define NT(IDENT) wo::grammar::nt(#IDENT)

            std::vector<grammar::rule> produces;


#define P(...) wo_macro_overload(P, __VA_ARGS__)
#define P_3(TARGET, BUILDER, PRODUCES)                              \
    produces.push_back(NT(TARGET) >> SL PRODUCES >> B(BUILDER))
#define P_4(TARGET, BUILDER_P1, BUILDER_P2, PRODUCES)               \
    produces.push_back(NT(TARGET) >> SL PRODUCES >> B(BUILDER_P1, BUILDER_P2))
#define P_5(TARGET, BUILDER_P1, BUILDER_P2, BUILDER_P3, PRODUCES)   \
    produces.push_back(NT(TARGET) >> SL PRODUCES >> B(BUILDER_P1, BUILDER_P2, BUILDER_P3))

            P(PROGRAM_AUGMENTED, pass_direct<0>, (NT(PROGRAM)));
            P(PROGRAM, pass_direct<0>, (TE(l_empty)));
            P(PROGRAM, pass_direct<0>, (NT(USELESS_TOKEN)));
            P(PROGRAM, pass_direct<0>, (NT(PARAGRAPH)));
            P(PARAGRAPH, pass_direct<0>, (NT(SENTENCE_LIST)));
            P(SENTENCE_LIST, pass_append_list<1, 0>, (NT(SENTENCE_LIST), NT(SENTENCE)));
            P(SENTENCE_LIST, pass_create_list<0>, (NT(SENTENCE)));
            // NOTE: macro might defined after import sentence. to make sure macro can be handle correctly.
            //      we make sure import happend before macro been peek and check.
            P(SENTENCE, pass_defer, (TE(l_defer), NT(BLOCKED_SENTENCE)));
            P(SENTENCE, pass_direct<0>, (NT(IMPORT_SENTENCE), TE(l_semicolon)));
            P(IMPORT_SENTENCE, pass_import_files, (TE(l_import), NT(SCOPED_LIST_NORMAL)));
            P(IMPORT_SENTENCE, pass_import_files, (TE(l_export), TE(l_import), NT(SCOPED_LIST_NORMAL)));
            P(SENTENCE, pass_using_namespace, (
                NT(DECL_ATTRIBUTE), TE(l_using), NT(SCOPED_LIST_NORMAL), TE(l_semicolon)));
            P(DECLARE_NEW_TYPE, pass_using_type_as, (
                NT(DECL_ATTRIBUTE),
                TE(l_using), 
                NT(IDENTIFIER),
                NT(DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY),
                TE(l_assign), 
                NT(TYPE)));
            P(SENTENCE, pass_using_typename_space, (NT(DECLARE_NEW_TYPE), NT(SENTENCE_BLOCK_MAY_SEMICOLON)));
            P(SENTENCE_BLOCK_MAY_SEMICOLON, pass_empty, (TE(l_semicolon)));
            P(SENTENCE_BLOCK_MAY_SEMICOLON, pass_direct<0>, (NT(SENTENCE_BLOCK)));
            P(SENTENCE, pass_alias_type_as, (
                NT(DECL_ATTRIBUTE), 
                TE(l_alias), 
                NT(IDENTIFIER),
                NT(DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY), 
                TE(l_assign),
                NT(TYPE),
                TE(l_semicolon)));
            //////////////////////////////////////////////////////////////////////////////////////////////
            P(SENTENCE, pass_direct<0>, (NT(DECL_ENUM)));
            P(DECL_ENUM, pass_enum_finalize, (
                NT(DECL_ATTRIBUTE), 
                TE(l_enum),
                NT(AST_TOKEN_IDENTIFER), 
                TE(l_left_curly_braces), 
                NT(ENUM_ITEMS), 
                TE(l_right_curly_braces)));
            P(ENUM_ITEMS, pass_direct<0>, (NT(ENUM_ITEM_LIST), NT(COMMA_MAY_EMPTY)));
            P(ENUM_ITEM_LIST, pass_create_list<0>, (NT(ENUM_ITEM)));
            P(ENUM_ITEM_LIST, pass_append_list<2, 0>, (NT(ENUM_ITEM_LIST), TE(l_comma), NT(ENUM_ITEM)));
            P(ENUM_ITEM, pass_enum_item_create, (NT(IDENTIFIER)));
            P(ENUM_ITEM, pass_enum_item_create, (NT(IDENTIFIER), TE(l_assign), NT(EXPRESSION)));
            P(SENTENCE, pass_direct<0>, (NT(DECL_NAMESPACE)));
            P(DECL_NAMESPACE, pass_namespace, (TE(l_namespace), NT(SPACE_NAME_LIST), NT(SENTENCE_BLOCK)));
            P(SPACE_NAME_LIST, pass_create_list<0>, (NT(SPACE_NAME)));
            P(SPACE_NAME_LIST, pass_append_list<2, 0>, (NT(SPACE_NAME_LIST), TE(l_scopeing), NT(SPACE_NAME)));
            P(SPACE_NAME, pass_token, (NT(IDENTIFIER)));
            P(BLOCKED_SENTENCE, pass_sentence_block<0>, (NT(SENTENCE)));
            P(SENTENCE, pass_direct<0>, (NT(SENTENCE_BLOCK)));
            P(SENTENCE_BLOCK, pass_sentence_block<1>, (
                TE(l_left_curly_braces), NT(PARAGRAPH), TE(l_right_curly_braces)));
            // Because of CONSTANT MAP => ,,, => { l_empty } Following production will cause R-R Conflict
            // NT(PARAGRAPH) >> SL(TE(l_empty));
            // So, just make a production like this:
            P(SENTENCE_BLOCK, pass_empty, (TE(l_left_curly_braces), TE(l_right_curly_braces)));
            // NOTE: Why the production can solve the conflict?
            // 
            //       A > {}
            //       B > {l_empty}
            // 
            //          In fact, A B have same production, but in wo_lr(1) parser, l_empty have a lower priority
            //      then production like A
            //          This rule can solve many grammar conflict easily, but in some case, it will cause bug, 
            //      so please use it carefully.
            //////////////////////////////////////////////////////////////////////////////////////////////
            P(DECL_ATTRIBUTE, pass_direct<0>, (NT(DECL_ATTRIBUTE_ITEMS)));
            P(DECL_ATTRIBUTE, pass_empty, (TE(l_empty)));
            P(DECL_ATTRIBUTE_ITEMS, pass_attribute, (NT(DECL_ATTRIBUTE_ITEM)));
            P(DECL_ATTRIBUTE_ITEMS, pass_attribute_append, (NT(DECL_ATTRIBUTE_ITEMS), NT(DECL_ATTRIBUTE_ITEM)));
            P(DECL_ATTRIBUTE_ITEM, pass_direct<0>, (NT(LIFECYCLE_MODIFER)));
            P(DECL_ATTRIBUTE_ITEM, pass_direct<0>, (NT(EXTERNAL_MODIFER)));
            P(DECL_ATTRIBUTE_ITEM, pass_direct<0>, (NT(ACCESS_MODIFIER)));
            P(LIFECYCLE_MODIFER, pass_token, (TE(l_static)));
            P(EXTERNAL_MODIFER, pass_token, (TE(l_extern)));
            P(ACCESS_MODIFIER, pass_token, (TE(l_public)));
            P(ACCESS_MODIFIER, pass_token, (TE(l_private)));
            P(ACCESS_MODIFIER, pass_token, (TE(l_protected)));
            P(SENTENCE, pass_empty, (TE(l_semicolon)));
            P(WHERE_CONSTRAINT_WITH_SEMICOLON, pass_direct<0>, (NT(WHERE_CONSTRAINT), TE(l_semicolon)));
            P(WHERE_CONSTRAINT_WITH_SEMICOLON, pass_empty, (TE(l_empty)));
            P(WHERE_CONSTRAINT_MAY_EMPTY, pass_direct<0>, (NT(WHERE_CONSTRAINT)));
            P(WHERE_CONSTRAINT_MAY_EMPTY, pass_empty, (TE(l_empty)));
            P(WHERE_CONSTRAINT, pass_build_where_constraint, (
                TE(l_where), NT(CONSTRAINT_LIST), NT(COMMA_MAY_EMPTY)));
            P(CONSTRAINT_LIST, pass_create_list<0>, (NT(EXPRESSION)));
            P(CONSTRAINT_LIST, pass_append_list<2, 0>, (NT(CONSTRAINT_LIST), TE(l_comma), NT(EXPRESSION)));
            P(SENTENCE, pass_direct<0>, (NT(FUNC_DEFINE_WITH_NAME)));
            P(FUNC_DEFINE_WITH_NAME, pass_func_def_named, (
                NT(DECL_ATTRIBUTE), 
                TE(l_func), 
                NT(AST_TOKEN_IDENTIFER),
                NT(DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY), 
                TE(l_left_brackets),
                NT(ARGDEFINE),
                TE(l_right_brackets), 
                NT(RETURN_TYPE_DECLEAR_MAY_EMPTY), 
                NT(WHERE_CONSTRAINT_WITH_SEMICOLON), 
                NT(SENTENCE_BLOCK)));
            P(FUNC_DEFINE_WITH_NAME, pass_func_def_oper, (
                NT(DECL_ATTRIBUTE), 
                TE(l_func), 
                TE(l_operator), 
                NT(OVERLOADINGABLE_OPERATOR), 
                NT(DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY), 
                TE(l_left_brackets), 
                NT(ARGDEFINE), 
                TE(l_right_brackets), 
                NT(RETURN_TYPE_DECLEAR_MAY_EMPTY), 
                NT(WHERE_CONSTRAINT_WITH_SEMICOLON), 
                NT(SENTENCE_BLOCK)));
            P(OVERLOADINGABLE_OPERATOR, pass_token, (TE(l_add)));
            P(OVERLOADINGABLE_OPERATOR, pass_token, (TE(l_sub)));
            P(OVERLOADINGABLE_OPERATOR, pass_token, (TE(l_mul)));
            P(OVERLOADINGABLE_OPERATOR, pass_token, (TE(l_div)));
            P(OVERLOADINGABLE_OPERATOR, pass_token, (TE(l_mod)));
            P(OVERLOADINGABLE_OPERATOR, pass_token, (TE(l_less)));
            P(OVERLOADINGABLE_OPERATOR, pass_token, (TE(l_larg)));
            P(OVERLOADINGABLE_OPERATOR, pass_token, (TE(l_less_or_equal)));
            P(OVERLOADINGABLE_OPERATOR, pass_token, (TE(l_larg_or_equal)));
            P(OVERLOADINGABLE_OPERATOR, pass_token, (TE(l_equal)));
            P(OVERLOADINGABLE_OPERATOR, pass_token, (TE(l_not_equal)));
            P(OVERLOADINGABLE_OPERATOR, pass_token, (TE(l_land)));
            P(OVERLOADINGABLE_OPERATOR, pass_token, (TE(l_lor)));
            // P(OVERLOADINGABLE_OPERATOR, pass_token, (TE(l_lnot)));
            P(OVERLOADINGABLE_OPERATOR, pass_token, (TE(l_index_begin), TE(l_index_end)));
            //////////////////////////////////////////////////////////////////////////////////////////////
            P(EXTERN_FROM, pass_extern, (
                TE(l_extern), 
                TE(l_left_brackets),
                TE(l_literal_string), 
                TE(l_comma), 
                TE(l_literal_string),
                NT(EXTERN_ATTRIBUTES),
                TE(l_right_brackets)));
            P(EXTERN_FROM, pass_extern, (
                TE(l_extern), 
                TE(l_left_brackets), 
                TE(l_literal_string), 
                NT(EXTERN_ATTRIBUTES),
                TE(l_right_brackets)));
            P(EXTERN_ATTRIBUTES, pass_empty, (TE(l_empty)));
            P(EXTERN_ATTRIBUTES, pass_direct<1>, (TE(l_comma), NT(EXTERN_ATTRIBUTE_LIST)));
            P(EXTERN_ATTRIBUTE_LIST, pass_create_list<0>, (NT(EXTERN_ATTRIBUTE)));
            P(EXTERN_ATTRIBUTE_LIST, pass_append_list<2, 0>, (
                NT(EXTERN_ATTRIBUTE_LIST), TE(l_comma), NT(EXTERN_ATTRIBUTE)));
            P(EXTERN_ATTRIBUTE, pass_token, (TE(l_identifier)));
            P(FUNC_DEFINE_WITH_NAME, pass_func_def_extn, (
                NT(EXTERN_FROM), 
                NT(DECL_ATTRIBUTE), 
                TE(l_func), 
                NT(AST_TOKEN_IDENTIFER),
                NT(DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY), 
                TE(l_left_brackets), 
                NT(ARGDEFINE), 
                TE(l_right_brackets), 
                NT(RETURN_TYPE_DECLEAR),
                NT(WHERE_CONSTRAINT_MAY_EMPTY), 
                TE(l_semicolon)));
            P(FUNC_DEFINE_WITH_NAME, pass_func_def_extn_oper, (
                NT(EXTERN_FROM), 
                NT(DECL_ATTRIBUTE), 
                TE(l_func), 
                TE(l_operator),
                NT(OVERLOADINGABLE_OPERATOR), 
                NT(DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY), 
                TE(l_left_brackets), 
                NT(ARGDEFINE),
                TE(l_right_brackets), 
                NT(RETURN_TYPE_DECLEAR), 
                NT(WHERE_CONSTRAINT_MAY_EMPTY), 
                TE(l_semicolon)));
            //////////////////////////////////////////////////////////////////////////////////////////////
            P(SENTENCE, pass_direct<0>, (NT(MAY_LABELED_LOOP)));
            P(MAY_LABELED_LOOP, pass_mark_label, (NT(IDENTIFIER), TE(l_at), NT(LOOP)));
            P(MAY_LABELED_LOOP, pass_direct<0>, (NT(LOOP)));
            P(LOOP, pass_direct<0>, (NT(WHILE)));
            P(LOOP, pass_direct<0>, (NT(FORLOOP)));
            P(LOOP, pass_direct<0>, (NT(FOREACH)));
            P(WHILE, pass_while, (
                TE(l_while), TE(l_left_brackets), NT(EXPRESSION), TE(l_right_brackets), NT(BLOCKED_SENTENCE)));
            P(VAR_DEFINE_WITH_SEMICOLON, pass_direct<0>, (NT(VAR_DEFINE_LET_SENTENCE), TE(l_semicolon)));
            P(FORLOOP, pass_for_defined, (
                TE(l_for),
                TE(l_left_brackets), 
                NT(VAR_DEFINE_WITH_SEMICOLON), 
                NT(MAY_EMPTY_EXPRESSION), 
                TE(l_semicolon), 
                NT(MAY_EMPTY_EXPRESSION), 
                TE(l_right_brackets),
                NT(BLOCKED_SENTENCE)));
            P(FORLOOP, pass_for_expr, (
                TE(l_for),
                TE(l_left_brackets),
                NT(MAY_EMPTY_EXPRESSION),
                TE(l_semicolon), 
                NT(MAY_EMPTY_EXPRESSION), 
                TE(l_semicolon),
                NT(MAY_EMPTY_EXPRESSION),
                TE(l_right_brackets), 
                NT(BLOCKED_SENTENCE)));
            P(SENTENCE, pass_break, (TE(l_break), TE(l_semicolon)));
            P(SENTENCE, pass_continue, (TE(l_continue), TE(l_semicolon)));
            P(SENTENCE, pass_break_label, (TE(l_break), NT(IDENTIFIER), TE(l_semicolon)));
            P(SENTENCE, pass_continue_label, (TE(l_continue), NT(IDENTIFIER), TE(l_semicolon)));
            //////////////////////////////////////////////////////////////////////////////////////////////
            P(MAY_EMPTY_EXPRESSION, pass_direct<0>, (NT(EXPRESSION)));
            P(MAY_EMPTY_EXPRESSION, pass_empty, (TE(l_empty)));
            P(FOREACH, pass_foreach, (
                TE(l_for),
                TE(l_left_brackets), 
                NT(DECL_ATTRIBUTE), 
                TE(l_let),
                NT(DEFINE_PATTERN), 
                TE(l_typecast), 
                NT(EXPRESSION), 
                TE(l_right_brackets), 
                NT(BLOCKED_SENTENCE)));
            P(SENTENCE, pass_direct<0>, (NT(IF)));
            P(IF, pass_if, (
                TE(l_if),
                TE(l_left_brackets),
                NT(EXPRESSION),
                TE(l_right_brackets), 
                NT(BLOCKED_SENTENCE), 
                NT(ELSE)));
            P(ELSE, pass_empty, (TE(l_empty)));
            P(ELSE, pass_direct<1>, (TE(l_else), NT(BLOCKED_SENTENCE)));
            P(SENTENCE, pass_direct<0>, (NT(VAR_DEFINE_LET_SENTENCE), TE(l_semicolon)));
            //////////////////////////////////////////////////////////////////////////////////////////////
            P(VAR_DEFINE_LET_SENTENCE, pass_variable_defines, (NT(DECL_ATTRIBUTE), TE(l_let), NT(VARDEFINES)));
            P(VARDEFINES, pass_create_list<0>, (NT(VARDEFINE)));
            P(VARDEFINES, pass_append_list<2, 0>, (NT(VARDEFINES), TE(l_comma), NT(VARDEFINE)));
            P(VARDEFINE, pass_variable_define_item, (
                NT(DEFINE_PATTERN_MAY_TEMPLATE), TE(l_assign), NT(EXPRESSION)));
            P(DEFINE_PATTERN_MAY_TEMPLATE, pass_direct<0>, (NT(DEFINE_PATTERN)));
            P(DEFINE_PATTERN_MAY_TEMPLATE, pass_direct<0>, (NT(DEFINE_PATTERN_WITH_TEMPLATE)));
            P(SENTENCE, pass_return_value, (TE(l_return), NT(EXPRESSION), TE(l_semicolon)));
            P(SENTENCE, pass_return, (TE(l_return), TE(l_semicolon)));
            P(SENTENCE, pass_direct_keep_source_location<0>, (NT(EXPRESSION), TE(l_semicolon)));
            P(EXPRESSION, pass_do_void_cast, (TE(l_do), NT(EXPRESSION)));
            P(EXPRESSION, pass_mark_mut, (TE(l_mut), NT(EXPRESSION)));
            P(EXPRESSION, pass_mark_immut, (TE(l_immut), NT(EXPRESSION)));
            P(EXPRESSION, pass_direct<0>, (NT(ASSIGNMENT)));
            P(EXPRESSION, pass_direct<0>, (NT(LOGICAL_OR)));
            P(ASSIGNMENT, pass_assign_operation, (NT(ASSIGNED_PATTERN), TE(l_assign), NT(EXPRESSION)));
            P(ASSIGNMENT, pass_assign_operation, (NT(ASSIGNED_PATTERN), TE(l_add_assign), NT(EXPRESSION)));
            P(ASSIGNMENT, pass_assign_operation, (NT(ASSIGNED_PATTERN), TE(l_sub_assign), NT(EXPRESSION)));
            P(ASSIGNMENT, pass_assign_operation, (NT(ASSIGNED_PATTERN), TE(l_mul_assign), NT(EXPRESSION)));
            P(ASSIGNMENT, pass_assign_operation, (NT(ASSIGNED_PATTERN), TE(l_div_assign), NT(EXPRESSION)));
            P(ASSIGNMENT, pass_assign_operation, (NT(ASSIGNED_PATTERN), TE(l_mod_assign), NT(EXPRESSION)));
            P(ASSIGNMENT, pass_assign_operation, (NT(ASSIGNED_PATTERN), TE(l_value_assign), NT(EXPRESSION)));
            P(ASSIGNMENT, pass_assign_operation, (
                NT(ASSIGNED_PATTERN), TE(l_value_add_assign), NT(EXPRESSION)));
            P(ASSIGNMENT, pass_assign_operation, (
                NT(ASSIGNED_PATTERN), TE(l_value_sub_assign), NT(EXPRESSION)));
            P(ASSIGNMENT, pass_assign_operation, (
                NT(ASSIGNED_PATTERN), TE(l_value_mul_assign), NT(EXPRESSION)));
            P(ASSIGNMENT, pass_assign_operation, (
                NT(ASSIGNED_PATTERN), TE(l_value_div_assign), NT(EXPRESSION)));
            P(ASSIGNMENT, pass_assign_operation, (
                NT(ASSIGNED_PATTERN), TE(l_value_mod_assign), NT(EXPRESSION)));
            P(ASSIGNED_PATTERN, pass_pattern_for_assign, (NT(LEFT)));
            //////////////////////////////////////////////////////////////////////////////////////////////
            P(EXPRESSION, pass_conditional_expression, (
                NT(LOGICAL_OR), TE(l_question), NT(EXPRESSION), TE(l_or), NT(EXPRESSION)));
            P(FACTOR, pass_direct<0>, (NT(FUNC_DEFINE)));
            P(FUNC_DEFINE, pass_func_lambda_ml, (
                NT(DECL_ATTRIBUTE), 
                TE(l_func),
                NT(DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY),
                TE(l_left_brackets),
                NT(ARGDEFINE), 
                TE(l_right_brackets),
                NT(RETURN_TYPE_DECLEAR_MAY_EMPTY),
                NT(WHERE_CONSTRAINT_WITH_SEMICOLON), 
                NT(SENTENCE_BLOCK)));
            P(FUNC_DEFINE, pass_func_lambda, (
                TE(l_lambda), 
                NT(DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY), 
                NT(ARGDEFINE),
                TE(l_assign),
                NT(RETURN_EXPR_BLOCK_IN_LAMBDA),
                NT(WHERE_DECL_FOR_LAMBDA), 
                TE(l_semicolon)));
            // May empty
            P(WHERE_DECL_FOR_LAMBDA, pass_empty, (TE(l_empty)));
            P(WHERE_DECL_FOR_LAMBDA, pass_reverse_vardef, (TE(l_where), NT(VARDEFINES)));
            P(RETURN_EXPR_BLOCK_IN_LAMBDA, pass_sentence_block<0>, (NT(RETURN_EXPR_IN_LAMBDA)));
            P(RETURN_EXPR_IN_LAMBDA, pass_return_lambda, (NT(EXPRESSION)));
            P(RETURN_TYPE_DECLEAR_MAY_EMPTY, pass_empty, (TE(l_empty)));
            P(RETURN_TYPE_DECLEAR_MAY_EMPTY, pass_direct<0>, (NT(RETURN_TYPE_DECLEAR)));
            P(RETURN_TYPE_DECLEAR, pass_direct_keep_source_location<1>, (TE(l_function_result), NT(TYPE)));
            P(TYPE_DECLEAR, pass_direct_keep_source_location<1>, (TE(l_typecast), NT(TYPE)));
            //////////////////////////////////////////////////////////////////////////////////////////////
            P(TYPE, pass_direct<0>, (NT(ORIGIN_TYPE)));
            P(TYPE, pass_type_mutable, (TE(l_mut), NT(ORIGIN_TYPE)));
            P(TYPE, pass_type_immutable, (TE(l_immut), NT(ORIGIN_TYPE)));
            //////////////////////////////////////////////////////////////////////////////////////////////
            P(ORIGIN_TYPE, pass_type_nil, (NT(AST_TOKEN_NIL)));
            P(ORIGIN_TYPE, pass_type_func, (NT(TUPLE_TYPE_LIST), TE(l_function_result), NT(TYPE)));
            //////////////////////////////////////////////////////////////////////////////////////////////
            P(ORIGIN_TYPE, pass_type_struct, (
                TE(l_struct), TE(l_left_curly_braces), NT(STRUCT_MEMBER_DEFINES), TE(l_right_curly_braces)));
            P(STRUCT_MEMBER_DEFINES, pass_create_list<0>, (TE(l_empty)));
            P(STRUCT_MEMBER_DEFINES, pass_direct<0>, (NT(STRUCT_MEMBERS_LIST), NT(COMMA_MAY_EMPTY)));
            P(STRUCT_MEMBERS_LIST, pass_create_list<0>, (NT(STRUCT_MEMBER_PAIR)));
            P(STRUCT_MEMBERS_LIST, pass_append_list<2, 0>, (
                NT(STRUCT_MEMBERS_LIST), TE(l_comma), NT(STRUCT_MEMBER_PAIR)));
            P(STRUCT_MEMBER_PAIR, pass_type_struct_field, (
                NT(ACCESS_MODIFIER_MAY_EMPTY), NT(IDENTIFIER), NT(TYPE_DECLEAR)));
            P(ACCESS_MODIFIER_MAY_EMPTY, pass_direct<0>, (NT(ACCESS_MODIFIER)));
            P(ACCESS_MODIFIER_MAY_EMPTY, pass_empty, (TE(l_empty)));
            //////////////////////////////////////////////////////////////////////////////////////////////
            P(ORIGIN_TYPE, pass_direct<0>, (NT(STRUCTABLE_TYPE)));
            P(STRUCTABLE_TYPE, pass_type_from_identifier, (NT(SCOPED_IDENTIFIER_FOR_TYPE)));
            P(STRUCTABLE_TYPE, pass_direct<0>, (NT(TYPEOF)));
            P(STRUCTABLE_TYPE_FOR_CONSTRUCT, pass_type_from_identifier, (NT(SCOPED_IDENTIFIER_FOR_VAL)));
            P(STRUCTABLE_TYPE_FOR_CONSTRUCT, pass_direct<0>, (NT(TYPEOF)));
            P(TYPEOF, pass_typeof, (TE(l_typeof), TE(l_left_brackets), NT(EXPRESSION), TE(l_right_brackets)));
            P(TYPEOF, pass_direct<2>, (TE(l_typeof), TE(l_template_using_begin), NT(TYPE), TE(l_larg)));
            P(TYPEOF, pass_direct<2>, (TE(l_typeof), TE(l_less), NT(TYPE), TE(l_larg)));
            P(ORIGIN_TYPE, pass_type_tuple, (NT(TUPLE_TYPE_LIST)));
            P(TUPLE_TYPE_LIST, pass_direct<1>, (
                TE(l_left_brackets), NT(TUPLE_TYPE_LIST_ITEMS_MAY_EMPTY), TE(l_right_brackets)));
            P(TUPLE_TYPE_LIST_ITEMS_MAY_EMPTY, pass_direct<0>, (
                NT(TUPLE_TYPE_LIST_ITEMS), NT(COMMA_MAY_EMPTY)));
            P(TUPLE_TYPE_LIST_ITEMS_MAY_EMPTY, pass_create_list<0>, (NT(VARIADIC_MAY_EMPTY)));
            P(TUPLE_TYPE_LIST_ITEMS, pass_create_list<0>, (NT(TYPE)));
            P(TUPLE_TYPE_LIST_ITEMS, pass_append_list<2, 0>, (
                NT(TUPLE_TYPE_LIST_ITEMS), TE(l_comma), NT(TYPE)));
            P(TUPLE_TYPE_LIST_ITEMS, pass_append_list<2, 0>, (
                NT(TUPLE_TYPE_LIST_ITEMS), TE(l_comma), NT(VARIADIC_MAY_EMPTY)));
            P(VARIADIC_MAY_EMPTY, pass_token, (TE(l_variadic_sign)));
            P(VARIADIC_MAY_EMPTY, pass_empty, (TE(l_empty)));
            //////////////////////////////////////////////////////////////////////////////////////////////
            P(ARGDEFINE, pass_create_list<0>, (TE(l_empty)));
            P(ARGDEFINE, pass_create_list<0>, (NT(ARGDEFINE_VAR_ITEM)));
            P(ARGDEFINE, pass_append_list<2, 0>, (NT(ARGDEFINE), TE(l_comma), NT(ARGDEFINE_VAR_ITEM)));
            P(ARGDEFINE_VAR_ITEM, pass_func_argument, (NT(DEFINE_PATTERN), NT(TYPE_DECL_MAY_EMPTY)));
            P(ARGDEFINE_VAR_ITEM, pass_token, (TE(l_variadic_sign)));
            P(TYPE_DECL_MAY_EMPTY, pass_direct<0>, (NT(TYPE_DECLEAR)));
            P(TYPE_DECL_MAY_EMPTY, pass_empty, (TE(l_empty)));
            P(LOGICAL_OR, pass_direct<0>, (NT(LOGICAL_AND)));
            P(LOGICAL_OR, pass_binary_operation, (NT(LOGICAL_OR), TE(l_lor), NT(LOGICAL_AND)));
            P(LOGICAL_AND, pass_direct<0>, (NT(EQUATION)));
            P(LOGICAL_AND, pass_binary_operation, (NT(LOGICAL_AND), TE(l_land), NT(EQUATION)));
            P(EQUATION, pass_direct<0>, (NT(RELATION)));
            P(EQUATION, pass_binary_operation, (NT(EQUATION), TE(l_equal), NT(RELATION)));
            P(EQUATION, pass_binary_operation, (NT(EQUATION), TE(l_not_equal), NT(RELATION)));
            P(RELATION, pass_direct<0>, (NT(SUMMATION)));
            P(RELATION, pass_binary_operation, (NT(RELATION), TE(l_larg), NT(SUMMATION)));
            P(RELATION, pass_binary_operation, (NT(RELATION), TE(l_less), NT(SUMMATION)));
            P(RELATION, pass_binary_operation, (NT(RELATION), TE(l_less_or_equal), NT(SUMMATION)));
            P(RELATION, pass_binary_operation, (NT(RELATION), TE(l_larg_or_equal), NT(SUMMATION)));
            P(SUMMATION, pass_direct<0>, (NT(MULTIPLICATION)));
            P(SUMMATION, pass_binary_operation, (NT(SUMMATION), TE(l_add), NT(MULTIPLICATION)));
            P(SUMMATION, pass_binary_operation, (NT(SUMMATION), TE(l_sub), NT(MULTIPLICATION)));
            P(MULTIPLICATION, pass_direct<0>, (NT(SINGLE_VALUE)));
            P(MULTIPLICATION, pass_binary_operation, (NT(MULTIPLICATION), TE(l_mul), NT(SINGLE_VALUE)));
            P(MULTIPLICATION, pass_binary_operation, (NT(MULTIPLICATION), TE(l_div), NT(SINGLE_VALUE)));
            P(MULTIPLICATION, pass_binary_operation, (NT(MULTIPLICATION), TE(l_mod), NT(SINGLE_VALUE)));
            P(SINGLE_VALUE, pass_direct<0>, (NT(UNARIED_FACTOR)));
            P(UNARIED_FACTOR, pass_direct<0>, (NT(FACTOR_TYPE_CASTING)));
            P(UNARIED_FACTOR, pass_unary_operation, (TE(l_sub), NT(UNARIED_FACTOR)));
            P(UNARIED_FACTOR, pass_unary_operation, (TE(l_lnot), NT(UNARIED_FACTOR)));
            P(UNARIED_FACTOR, pass_direct<0>, (NT(INV_FUNCTION_CALL)));
            P(FACTOR_TYPE_CASTING, pass_check_type_as, (NT(FACTOR_TYPE_CASTING), NT(AS_TYPE)));
            P(FACTOR_TYPE_CASTING, pass_check_type_is, (NT(FACTOR_TYPE_CASTING), NT(IS_TYPE)));
            P(AS_TYPE, pass_direct_keep_source_location<1>, (TE(l_as), NT(TYPE)));
            P(IS_TYPE, pass_direct_keep_source_location<1>, (TE(l_is), NT(TYPE)));
            //////////////////////////////////////////////////////////////////////////////////////////////
            P(FACTOR_TYPE_CASTING, pass_cast_type, (NT(FACTOR_TYPE_CASTING), NT(TYPE_DECLEAR)));
            P(FACTOR_TYPE_CASTING, pass_direct<0>, (NT(FACTOR)));
            P(FACTOR, pass_direct<0>, (NT(LEFT)));
            P(FACTOR, pass_direct<1>, (TE(l_left_brackets), NT(EXPRESSION), TE(l_right_brackets)));
            P(FACTOR, pass_direct<0>, (NT(UNIT)));
            P(EXPRESSION, pass_direct<0>, (NT(ARGUMENT_EXPAND)));
            P(ARGUMENT_EXPAND, pass_expand_arguments, (NT(FACTOR_TYPE_CASTING), TE(l_variadic_sign)));
            P(UNIT, pass_variadic_arguments_pack, (TE(l_variadic_sign)));
            P(UNIT, pass_literal, (TE(l_literal_integer)));
            P(UNIT, pass_literal, (TE(l_literal_real)));
            P(UNIT, pass_literal, (TE(l_literal_string)));
            P(UNIT, pass_literal, (TE(l_literal_raw_string)));
            P(UNIT, pass_literal_char, (NT(LITERAL_CHAR)));
            P(UNIT, pass_literal, (TE(l_literal_handle)));
            P(UNIT, pass_literal, (TE(l_nil)));
            P(UNIT, pass_literal, (TE(l_true)));
            P(UNIT, pass_literal, (TE(l_false)));
            P(UNIT, pass_typeid, (TE(l_typeid), TE(l_template_using_begin), NT(TYPE), TE(l_larg)));
            P(UNIT, pass_typeid, (TE(l_typeid), TE(l_less), NT(TYPE), TE(l_larg)));
            P(UNIT, pass_typeid_with_expr, (
                TE(l_typeid), TE(l_left_brackets), NT(EXPRESSION), TE(l_right_brackets)));
            P(LITERAL_CHAR, pass_token, (TE(l_literal_char)));
            P(UNIT, pass_direct<0>, (NT(FORMAT_STRING)));
            P(FORMAT_STRING, pass_format_finish, (
                NT(LITERAL_FORMAT_STRING_BEGIN), NT(FORMAT_STRING_LIST), NT(LITERAL_FORMAT_STRING_END)));
            P(FORMAT_STRING_LIST, pass_format_cast_string, (NT(EXPRESSION)));
            P(FORMAT_STRING_LIST, pass_format_connect, (
                NT(FORMAT_STRING_LIST), NT(LITERAL_FORMAT_STRING), NT(EXPRESSION)));
            P(LITERAL_FORMAT_STRING_BEGIN, pass_token, (TE(l_format_string_begin)));
            P(LITERAL_FORMAT_STRING, pass_token, (TE(l_format_string)));
            P(LITERAL_FORMAT_STRING_END, pass_token, (TE(l_format_string_end)));
            P(UNIT, pass_direct<0>, (NT(CONSTANT_MAP)));
            P(UNIT, pass_direct<0>, (NT(CONSTANT_ARRAY)));
            P(CONSTANT_ARRAY, pass_array_instance, (
                TE(l_index_begin), NT(CONSTANT_ARRAY_ITEMS), TE(l_index_end)));
            P(CONSTANT_ARRAY, pass_vec_instance, (
                TE(l_index_begin), NT(CONSTANT_ARRAY_ITEMS), TE(l_index_end), TE(l_mut)));
            P(CONSTANT_ARRAY_ITEMS, pass_direct<0>, (NT(COMMA_EXPR)));
            //////////////////////////////////////////////////////////////////////////////////////////////
            P(UNIT, pass_tuple_instance, (NT(CONSTANT_TUPLE)));
            // NOTE: Avoid grammar conflict.
            P(CONSTANT_TUPLE, pass_create_list<1>, (
                TE(l_left_brackets), NT(COMMA_MAY_EMPTY), TE(l_right_brackets)));
            P(CONSTANT_TUPLE, pass_append_list<1, 3>, (
                TE(l_left_brackets), NT(EXPRESSION), TE(l_comma), NT(COMMA_EXPR), TE(l_right_brackets)));
            //////////////////////////////////////////////////////////////////////////////////////////////
            P(CONSTANT_MAP, pass_dict_instance, (
                TE(l_left_curly_braces), NT(CONSTANT_MAP_PAIRS), TE(l_right_curly_braces)));
            P(CONSTANT_MAP, pass_map_instance, (
                TE(l_left_curly_braces), NT(CONSTANT_MAP_PAIRS), TE(l_right_curly_braces), TE(l_mut)));
            P(CONSTANT_MAP_PAIRS, pass_create_list<0>, (NT(CONSTANT_MAP_PAIR)));
            P(CONSTANT_MAP_PAIRS, pass_append_list<2, 0>, (
                NT(CONSTANT_MAP_PAIRS), TE(l_comma), NT(CONSTANT_MAP_PAIR)));
            P(CONSTANT_MAP_PAIR, pass_empty, (TE(l_empty)));
            P(CONSTANT_MAP_PAIR, pass_dict_field_init_pair, (
                NT(CONSTANT_ARRAY), TE(l_assign), NT(EXPRESSION)));
            P(CALLABLE_LEFT, pass_variable, (NT(SCOPED_IDENTIFIER_FOR_VAL)));
            P(CALLABLE_RIGHT_WITH_BRACKET, pass_direct<1>, (
                TE(l_left_brackets), NT(EXPRESSION), TE(l_right_brackets)));
            P(SCOPED_IDENTIFIER_FOR_VAL, pass_build_identifier_typeof, (
                NT(SCOPED_LIST_TYPEOF), NT(MAY_EMPTY_LEFT_TEMPLATE_ITEM)));
            P(SCOPED_IDENTIFIER_FOR_VAL, pass_build_identifier_normal, (
                NT(SCOPED_LIST_NORMAL), NT(MAY_EMPTY_LEFT_TEMPLATE_ITEM)));
            P(SCOPED_IDENTIFIER_FOR_VAL, pass_build_identifier_global, (
                NT(SCOPED_LIST_GLOBAL), NT(MAY_EMPTY_LEFT_TEMPLATE_ITEM)));
            P(SCOPED_IDENTIFIER_FOR_TYPE, pass_build_identifier_typeof, (
                NT(SCOPED_LIST_TYPEOF), NT(MAY_EMPTY_TEMPLATE_ITEM)));
            P(SCOPED_IDENTIFIER_FOR_TYPE, pass_build_identifier_normal, (
                NT(SCOPED_LIST_NORMAL), NT(MAY_EMPTY_TEMPLATE_ITEM)));
            P(SCOPED_IDENTIFIER_FOR_TYPE, pass_build_identifier_global, (
                NT(SCOPED_LIST_GLOBAL), NT(MAY_EMPTY_TEMPLATE_ITEM)));
            P(SCOPED_LIST_TYPEOF, pass_append_list<0, 1>, (NT(TYPEOF), NT(SCOPING_LIST)));
            P(SCOPED_LIST_NORMAL, pass_append_list<0, 1>, (NT(AST_TOKEN_IDENTIFER), NT(SCOPING_LIST)));
            P(SCOPED_LIST_NORMAL, pass_create_list<0>, (NT(AST_TOKEN_IDENTIFER)));
            P(SCOPED_LIST_GLOBAL, pass_direct<0>, (NT(SCOPING_LIST)));
            P(SCOPING_LIST, pass_create_list<1>, (TE(l_scopeing), NT(AST_TOKEN_IDENTIFER)));
            P(SCOPING_LIST, pass_append_list<1, 2>, (
                TE(l_scopeing), NT(AST_TOKEN_IDENTIFER), NT(SCOPING_LIST)));
            P(LEFT, pass_direct<0>, (NT(CALLABLE_LEFT)));
            P(LEFT, pass_index_operation_regular, (
                NT(FACTOR_TYPE_CASTING), TE(l_index_begin), NT(EXPRESSION), TE(l_index_end)));
            P(LEFT, pass_index_operation_member, (
                NT(FACTOR_TYPE_CASTING), TE(l_index_point), NT(INDEX_POINT_TARGET)));
            P(INDEX_POINT_TARGET, pass_token, (NT(IDENTIFIER)));
            P(INDEX_POINT_TARGET, pass_token, (TE(l_literal_integer)));
            P(FACTOR, pass_direct<0>, (NT(FUNCTION_CALL)));
            P(DIRECT_CALLABLE_TARGET, pass_direct<0>, (NT(CALLABLE_LEFT)));
            P(DIRECT_CALLABLE_TARGET, pass_direct<0>, (NT(CALLABLE_RIGHT_WITH_BRACKET)));
            P(DIRECT_CALLABLE_TARGET, pass_direct<0>, (NT(FUNC_DEFINE)));
            P(DIRECT_CALL_FIRST_ARG, pass_direct<0>, (NT(FACTOR_TYPE_CASTING)));
            P(DIRECT_CALL_FIRST_ARG, pass_direct<0>, (NT(ARGUMENT_EXPAND)));
            //////////////////////////////////////////////////////////////////////////////////////////////
            // MONAD GRAMMAR~~~ SUGAR!
            P(FACTOR, pass_build_bind_monad, (NT(FACTOR_TYPE_CASTING), TE(l_bind_monad), NT(CALLABLE_LEFT)));
            P(FACTOR, pass_build_bind_monad, (
                NT(FACTOR_TYPE_CASTING), TE(l_bind_monad), NT(CALLABLE_RIGHT_WITH_BRACKET)));
            P(FACTOR, pass_build_bind_monad, (NT(FACTOR_TYPE_CASTING), TE(l_bind_monad), NT(FUNC_DEFINE)));
            P(FACTOR, pass_build_map_monad, (NT(FACTOR_TYPE_CASTING), TE(l_map_monad), NT(CALLABLE_LEFT)));
            P(FACTOR, pass_build_map_monad, (
                NT(FACTOR_TYPE_CASTING), TE(l_map_monad), NT(CALLABLE_RIGHT_WITH_BRACKET)));
            P(FACTOR, pass_build_map_monad, (NT(FACTOR_TYPE_CASTING), TE(l_map_monad), NT(FUNC_DEFINE)));
            P(ARGUMENT_LISTS, pass_direct<1>, (TE(l_left_brackets), NT(COMMA_EXPR), TE(l_right_brackets)));
            P(ARGUMENT_LISTS_MAY_EMPTY, pass_direct<0>, (NT(ARGUMENT_LISTS)));
            P(ARGUMENT_LISTS_MAY_EMPTY, pass_create_list<0>, (TE(l_empty)));
            P(FUNCTION_CALL, pass_normal_function_call, (NT(FACTOR), NT(ARGUMENT_LISTS)));
            P(FUNCTION_CALL, pass_directly_function_call_append_arguments, (
                NT(DIRECT_CALLABLE_VALUE), NT(ARGUMENT_LISTS_MAY_EMPTY)));
            P(DIRECT_CALLABLE_VALUE, pass_directly_function_call, (
                NT(DIRECT_CALL_FIRST_ARG), TE(l_direct), NT(DIRECT_CALLABLE_TARGET)));
            P(INV_DIRECT_CALL_FIRST_ARG, pass_direct<0>, (NT(SINGLE_VALUE)));
            P(INV_DIRECT_CALL_FIRST_ARG, pass_direct<0>, (NT(ARGUMENT_EXPAND)));
            P(INV_FUNCTION_CALL, pass_inverse_function_call, (
                NT(FUNCTION_CALL), TE(l_inv_direct), NT(INV_DIRECT_CALL_FIRST_ARG)));
            P(INV_FUNCTION_CALL, pass_inverse_function_call, (
                NT(DIRECT_CALLABLE_TARGET), TE(l_inv_direct), NT(INV_DIRECT_CALL_FIRST_ARG)));
            P(COMMA_EXPR, pass_direct<0>, (NT(COMMA_EXPR_ITEMS), NT(COMMA_MAY_EMPTY)));
            P(COMMA_EXPR, pass_create_list<0>, (NT(COMMA_MAY_EMPTY)));
            P(COMMA_EXPR_ITEMS, pass_create_list<0>, (NT(EXPRESSION)));
            P(COMMA_EXPR_ITEMS, pass_append_list<2, 0>, (NT(COMMA_EXPR_ITEMS), TE(l_comma), NT(EXPRESSION)));
            P(COMMA_MAY_EMPTY, pass_empty, (TE(l_comma)));
            P(COMMA_MAY_EMPTY, pass_empty, (TE(l_empty)));
            P(SEMICOLON_MAY_EMPTY, pass_empty, (TE(l_semicolon)));
            P(SEMICOLON_MAY_EMPTY, pass_empty, (TE(l_empty)));
            //////////////////////////////////////////////////////////////////////////////////////////////
            P(MAY_EMPTY_TEMPLATE_ITEM, pass_direct<0>, (NT(TYPE_TEMPLATE_ITEM)));
            P(MAY_EMPTY_TEMPLATE_ITEM, pass_empty, (TE(l_empty)));
            P(MAY_EMPTY_LEFT_TEMPLATE_ITEM, pass_empty, (TE(l_empty)));
            P(MAY_EMPTY_LEFT_TEMPLATE_ITEM, pass_direct<0>, (NT(LEFT_TEMPLATE_ITEM)));
            P(LEFT_TEMPLATE_ITEM, pass_direct<1>, (
                TE(l_template_using_begin), NT(TEMPLATE_ARGUMENT_LIST), TE(l_larg)));
            P(TYPE_TEMPLATE_ITEM, pass_direct<1>, (TE(l_less), NT(TEMPLATE_ARGUMENT_LIST), TE(l_larg)));
            // Template for type can be :< ... >
            P(TYPE_TEMPLATE_ITEM, pass_direct<0>, (NT(LEFT_TEMPLATE_ITEM)));
            P(TEMPLATE_ARGUMENT_LIST, pass_create_list<0>, (NT(TEMPLATE_ARGUMENT_ITEM)));
            P(TEMPLATE_ARGUMENT_LIST, pass_append_list<2, 0>, (
                NT(TEMPLATE_ARGUMENT_LIST), TE(l_comma), NT(TEMPLATE_ARGUMENT_ITEM)));
            P(TEMPLATE_ARGUMENT_ITEM, pass_create_template_argument, (NT(TYPE)));
            P(TEMPLATE_ARGUMENT_ITEM, pass_create_template_argument, (
                TE(l_left_curly_braces), NT(EXPRESSION), TE(l_right_curly_braces)));
            P(DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY, pass_empty, (TE(l_empty)));
            P(DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY, pass_direct<0>, (NT(DEFINE_TEMPLATE_PARAM_ITEM)));
            P(DEFINE_TEMPLATE_PARAM_ITEM, pass_direct<1>, (
                TE(l_less), NT(DEFINE_TEMPLATE_PARAM_LIST), TE(l_larg)));
            P(DEFINE_TEMPLATE_PARAM_LIST, pass_create_list<0>, (NT(DEFINE_TEMPLATE_PARAM)));
            P(DEFINE_TEMPLATE_PARAM_LIST, pass_append_list<2, 0>, (
                NT(DEFINE_TEMPLATE_PARAM_LIST), TE(l_comma), NT(DEFINE_TEMPLATE_PARAM)));
            P(DEFINE_TEMPLATE_PARAM, pass_create_template_param, (NT(IDENTIFIER)));
            P(DEFINE_TEMPLATE_PARAM, pass_create_template_param, (NT(IDENTIFIER), TE(l_typecast), NT(TYPE)));
            //////////////////////////////////////////////////////////////////////////////////////////////
            P(SENTENCE, pass_direct<0>, (NT(DECL_UNION)));
            P(DECL_UNION, pass_union_declare, (
                NT(DECL_ATTRIBUTE),
                TE(l_union), 
                NT(AST_TOKEN_IDENTIFER), 
                NT(DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY),
                TE(l_left_curly_braces), 
                NT(UNION_ITEMS), 
                TE(l_right_curly_braces)));
            P(UNION_ITEMS, pass_direct<0>, (NT(UNION_ITEM_LIST), NT(COMMA_MAY_EMPTY)));
            P(UNION_ITEM_LIST, pass_create_list<0>, (NT(UNION_ITEM)));
            P(UNION_ITEM_LIST, pass_append_list<2, 0>, (NT(UNION_ITEM_LIST), TE(l_comma), NT(UNION_ITEM)));
            P(UNION_ITEM, pass_union_item, (NT(IDENTIFIER)));
            P(UNION_ITEM, pass_union_item_constructor, (
                NT(IDENTIFIER), TE(l_left_brackets), NT(TYPE), TE(l_right_brackets)));
            P(SENTENCE, pass_direct<0>, (NT(MATCH_BLOCK)));
            P(MATCH_BLOCK, pass_match, (
                TE(l_match), 
                TE(l_left_brackets),
                NT(EXPRESSION), 
                TE(l_right_brackets), 
                TE(l_left_curly_braces),
                NT(MATCH_CASES),
                TE(l_right_curly_braces)));
            P(MATCH_CASES, pass_create_list<0>, (NT(MATCH_CASE)));
            P(MATCH_CASES, pass_append_list<1, 0>, (NT(MATCH_CASES), NT(MATCH_CASE)));
            P(MATCH_CASE, pass_match_union_case, (
                NT(PATTERN_UNION_CASE), TE(l_question), NT(BLOCKED_SENTENCE)));
            // PATTERN-CASE MAY BE A SINGLE-VARIABLE/TUPLE/STRUCT...
            P(PATTERN_UNION_CASE, pass_union_pattern_identifier_or_takeplace, (NT(IDENTIFIER)));
            // PATTERN-CASE MAY BE A UNION
            P(PATTERN_UNION_CASE, pass_union_pattern_contain_element, (
                NT(IDENTIFIER), TE(l_left_brackets), NT(DEFINE_PATTERN), TE(l_right_brackets)));
            //////////////////////////////////////////////////////////////////////////////////////////////
            P(UNIT, pass_struct_instance, (
                NT(STRUCT_INSTANCE_BEGIN),
                TE(l_left_curly_braces), 
                NT(STRUCT_MEMBER_INITS), 
                TE(l_right_curly_braces)));
            P(STRUCT_INSTANCE_BEGIN, pass_direct<0>, (NT(STRUCTABLE_TYPE_FOR_CONSTRUCT)));
            P(STRUCT_INSTANCE_BEGIN, pass_empty, (TE(l_struct)));
            P(STRUCT_MEMBER_INITS, pass_create_list<0>, (NT(STRUCT_MEMBER_INITS_EMPTY)));
            P(STRUCT_MEMBER_INITS_EMPTY, pass_empty, (TE(l_empty)));
            P(STRUCT_MEMBER_INITS_EMPTY, pass_empty, (TE(l_comma)));
            P(STRUCT_MEMBER_INITS, pass_direct<0>, (NT(STRUCT_MEMBERS_INIT_LIST), NT(COMMA_MAY_EMPTY)));
            P(STRUCT_MEMBERS_INIT_LIST, pass_create_list<0>, (NT(STRUCT_MEMBER_INIT_ITEM)));
            P(STRUCT_MEMBERS_INIT_LIST, pass_append_list<2, 0>, (
                NT(STRUCT_MEMBERS_INIT_LIST), TE(l_comma), NT(STRUCT_MEMBER_INIT_ITEM)));
            P(STRUCT_MEMBER_INIT_ITEM, pass_struct_member_init_pair, (
                NT(IDENTIFIER), TE(l_assign), NT(EXPRESSION)));
            //////////////////////////////////////////////////////////////////////////////////////////////
            P(DEFINE_PATTERN, pass_pattern_identifier_or_takepace, (NT(IDENTIFIER)));
            P(DEFINE_PATTERN, pass_pattern_mut_identifier_or_takepace, (TE(l_mut), NT(IDENTIFIER)));
            P(DEFINE_PATTERN_WITH_TEMPLATE, pass_pattern_identifier_or_takepace_with_template, (
                NT(IDENTIFIER), NT(DEFINE_TEMPLATE_PARAM_ITEM)));
            P(DEFINE_PATTERN_WITH_TEMPLATE, pass_pattern_mut_identifier_or_takepace_with_template, (
                TE(l_mut), NT(IDENTIFIER), NT(DEFINE_TEMPLATE_PARAM_ITEM)));
            P(DEFINE_PATTERN, pass_pattern_tuple, (
                TE(l_left_brackets), NT(DEFINE_PATTERN_LIST), TE(l_right_brackets)));
            P(DEFINE_PATTERN, pass_pattern_tuple, (
                TE(l_left_brackets), NT(COMMA_MAY_EMPTY), TE(l_right_brackets)));
            P(DEFINE_PATTERN_LIST, pass_direct<0>, (NT(DEFINE_PATTERN_ITEMS), NT(COMMA_MAY_EMPTY)));
            P(DEFINE_PATTERN_ITEMS, pass_create_list<0>, (NT(DEFINE_PATTERN)));
            P(DEFINE_PATTERN_ITEMS, pass_append_list<2, 0>, (
                NT(DEFINE_PATTERN_ITEMS), TE(l_comma), NT(DEFINE_PATTERN)));
            //////////////////////////////////////////////////////////////////////////////////////////////
            P(AST_TOKEN_IDENTIFER, pass_token, (NT(IDENTIFIER)));
            P(AST_TOKEN_NIL, pass_token, (TE(l_nil)));
            P(IDENTIFIER, pass_direct<0>, (TE(l_identifier)));
            P(USELESS_TOKEN, pass_useless_token, (TE(l_double_index_point)));
            P(USELESS_TOKEN, pass_useless_token, (TE(l_unknown_token)));
            P(USELESS_TOKEN, pass_token, (TE(l_macro)));
#undef B
#undef SL
#undef TE
#undef NT
#undef P

            grammar_instance = std::make_unique<grammar>(produces);

            wo_stdout << ANSI_HIY "WooGramma: " ANSI_RST "Checking LR(1) table..." << wo_endl;
            if (grammar_instance->check_lr1())
            {
                wo_stdout << ANSI_HIR "WooGramma: " ANSI_RST "LR(1) have some problem, abort." << wo_endl;
                exit(-1);
            }

            if (!WO_GRAMMAR_SKIP_GEN_LR1_TABLE_CACHE)
            {
                using namespace std;
                const char* tab = "    ";

                wo_stdout << ANSI_HIY "WooGramma: " ANSI_RST "OK, now writting cache..." << wo_endl;

                // Do expe
                std::string cur_path = GRAMMAR_SRC_FILE;
                if (auto pos = cur_path.find_last_of("/\\"); pos >= 0 && pos < cur_path.size())
                    cur_path = cur_path.substr(0, pos) + "/wo_lang_grammar_lr1_autogen.hpp";
                else
                {
                    wo_stdout << ANSI_HIR "WooGramma: " ANSI_RST "File will be generated to current work path, may have some problem..." << wo_endl;
                    exit(-1);
                }

                std::ofstream cachefile(cur_path);

                wo_stdout << ANSI_HIY "WooGramma: " ANSI_RST "Write to " << cur_path << wo_endl;

                cachefile << "// THIS FILE IS AUTO GENERATED BY RSTORABLESCENE." << endl;
                cachefile << "// IF YOU WANT TO MODIFY GRAMMAR, PLEASE LOOK AT 'wo_lang_grammar.cpp'." << endl;
                cachefile << "// " << endl;
                cachefile << "#include \"wo_compiler_parser.hpp\"" << endl;
                cachefile << endl;
                cachefile << "#define WO_LANG_GRAMMAR_LR1_AUTO_GENED" << endl;
                cachefile << "#define WO_LANG_GRAMMAR_CRC64 0x" << std::hex << wo_lang_grammar_crc64 << "ull" << std::dec << endl;
                cachefile << endl;
                cachefile << endl;
                cachefile << "namespace wo" << endl;
                cachefile << "{" << endl;
                cachefile << "#ifndef WO_DISABLE_COMPILER" << endl;
                cachefile << "#ifdef WO_LANG_GRAMMAR_LR1_IMPL" << endl;

                // Generate StateID - NontermName Pair & StateID - TermInt Pair
                ///////////////////////////////////////////////////////////////////////
                size_t max_length_producer = 0;
                std::map<std::string, int> nonte_list;
                std::map<lex_type, int> te_list;
                do
                {
                    int nt_count = 0;
                    int te_count = 0;
                    for (auto& [op_nt, sym_list] : grammar_instance->ORGIN_P)
                    {
                        if (sym_list.size() > max_length_producer)
                            max_length_producer = sym_list.size();

                        if (nonte_list.find(op_nt.nt_name) == nonte_list.end())
                            nonte_list[op_nt.nt_name] = ++nt_count;

                        for (auto& sym : sym_list)
                        {
                            if (std::holds_alternative<grammar::te>(sym))
                            {
                                if (te_list.find(std::get<grammar::te>(sym).t_type) == te_list.end())
                                    te_list[std::get<grammar::te>(sym).t_type] = ++te_count;
                            }
                        }
                    }
                    if (te_list.find(lex_type::l_eof) == te_list.end())
                        te_list[lex_type::l_eof] = ++te_count;

                    std::map<int, std::string> _real_nonte_list;
                    for (auto& [op_nt, nt_index] : nonte_list)
                    {
                        wo_test(_real_nonte_list.find(nt_index) == _real_nonte_list.end());
                        _real_nonte_list.insert(std::make_pair(nt_index, op_nt));
                    }
                    std::map<int, lex_type> _real_te_list;
                    for (auto& [op_te, te_index] : te_list)
                    {
                        wo_test(_real_te_list.find(te_index) == _real_te_list.end());
                        _real_te_list.insert_or_assign(te_index, op_te);
                    }

                    cachefile << "const char* woolang_id_nonterm_list[" << nt_count << "+ 1] = {" << endl;
                    cachefile << tab << "nullptr," << endl;

                    for (auto& [nt_index, op_nt] : _real_nonte_list)
                        cachefile << tab << "\"" << op_nt << "\"," << endl;

                    cachefile << "};" << endl;

                    cachefile << "const lex_type woolang_id_term_list[" << te_count << "+ 1] = {" << endl;
                    cachefile << tab << "lex_type::l_error," << endl;

                    for (auto& [te_index, op_te] : _real_te_list)
                    {
                        cachefile
                            << tab
                            << "(lex_type)"
                            << static_cast<int>(static_cast<lex_type_base_t>(op_te))
                            << ","
                            << endl;
                    }

                    cachefile << "};" << endl;
                } while (0);
                ///////////////////////////////////////////////////////////////////////
                // Generate LR(1) action table;

                std::vector<std::pair<int, int>> STATE_GOTO_R_S_INDEX_MAP(
                    grammar_instance->LR1_TABLE.size(), std::pair<int, int>(-1, -1));

                // GOTO : only nt have goto;
                cachefile << "const int woolang_lr1_act_goto[][" << nonte_list.size() << " + 1] = {" << endl;
                cachefile << "// {   STATE_ID,  NT_ID1, NT_ID2, ...  }" << endl;
                size_t WO_LR1_ACT_GOTO_SIZE = 0;
                size_t WO_GENERATE_INDEX = 0;
                for (auto& [state_id, te_sym_list] : grammar_instance->LR1_TABLE)
                {
                    std::vector<int> nt_goto_state(nonte_list.size() + 1, -1);
                    bool has_action = false;
                    for (auto& [sym, actions] : te_sym_list)
                    {
                        wo_test(actions.size() <= 1);
                        if (std::holds_alternative<grammar::nt>(sym))
                        {
                            if (actions.size())
                            {
                                wo_test(actions.begin()->act == grammar::action::act_type::state_goto);
                                nt_goto_state[nonte_list.at(std::get<grammar::nt>(sym).nt_name)] = (int)actions.begin()->state;
                                has_action = true;
                            }
                        }
                    }

                    if (has_action)
                    {
                        if (STATE_GOTO_R_S_INDEX_MAP.size() <= state_id)
                            STATE_GOTO_R_S_INDEX_MAP.resize(state_id, std::pair<int, int>(-1, -1));

                        STATE_GOTO_R_S_INDEX_MAP[state_id].first = (int)(WO_GENERATE_INDEX++);

                        ++WO_LR1_ACT_GOTO_SIZE;
                        cachefile << " {   " << state_id << ",  ";
                        for (size_t i = 1; i < nt_goto_state.size(); i++)
                        {
                            cachefile << nt_goto_state[i] << ", ";
                        }
                        cachefile << "}," << endl;
                    }
                }

                cachefile << "};" << endl;
                cachefile << "#define WO_LR1_ACT_GOTO_SIZE (" << WO_LR1_ACT_GOTO_SIZE << ")" << endl;

                // Stack/Reduce
                cachefile << "const int woolang_lr1_act_stack_reduce[][" << te_list.size() << " + 1] = {" << endl;
                cachefile << "// {   STATE_ID,  TE_ID1, TE_ID2, ...  }" << endl;

                bool has_acc = false;
                int acc_state = 0, acc_term = 0;
                size_t WO_LR1_ACT_STACK_SIZE = 0;
                WO_GENERATE_INDEX = 0;
                for (auto& [state_id, te_sym_list] : grammar_instance->LR1_TABLE)
                {
                    std::vector<int> te_stack_reduce_state(te_list.size() + 1, 0);
                    bool has_action = false;
                    for (auto& [sym, actions] : te_sym_list)
                    {
                        wo_test(actions.size() <= 1);
                        if (std::holds_alternative<grammar::te>(sym))
                        {
                            if (actions.size())
                            {
                                wo_test(actions.begin()->act == grammar::action::act_type::push_stack ||
                                    actions.begin()->act == grammar::action::act_type::reduction ||
                                    actions.begin()->act == grammar::action::act_type::accept);

                                if (actions.begin()->act == grammar::action::act_type::push_stack)
                                {
                                    te_stack_reduce_state[te_list[std::get<grammar::te>(sym).t_type]] = (int)actions.begin()->state + 1;
                                    has_action = true;
                                }
                                else if (actions.begin()->act == grammar::action::act_type::reduction)
                                {
                                    te_stack_reduce_state[te_list[std::get<grammar::te>(sym).t_type]] = -((int)actions.begin()->state + 1);
                                    has_action = true;
                                }
                                else if (actions.begin()->act == grammar::action::act_type::accept)
                                {
                                    wo_test(has_acc == false);
                                    has_acc = true;
                                    acc_state = (int)state_id;
                                    acc_term = te_list[std::get<grammar::te>(sym).t_type];
                                }
                            }
                        }
                    }

                    if (has_action)
                    {
                        if (STATE_GOTO_R_S_INDEX_MAP.size() <= state_id)
                            STATE_GOTO_R_S_INDEX_MAP.resize(state_id, std::pair<int, int>(-1, -1));

                        STATE_GOTO_R_S_INDEX_MAP[state_id].second = (int)(WO_GENERATE_INDEX++);

                        ++WO_LR1_ACT_STACK_SIZE;
                        cachefile << " {   " << state_id << ",  ";
                        for (size_t i = 1; i < te_stack_reduce_state.size(); i++)
                        {
                            cachefile << te_stack_reduce_state[i] << ", ";
                        }
                        cachefile << "}," << endl;
                    }
                }

                cachefile << "};" << endl;

                cachefile << "#define WO_LR1_ACT_STACK_SIZE (" << WO_LR1_ACT_STACK_SIZE << ")" << endl;

                // Stack/Reduce
                cachefile << "const int woolang_lr1_goto_rs_map[][2] = {" << endl;
                cachefile << "// { GOTO_INDEX, RS_INDEX }" << endl;
                for (auto& [gotoidx, rsidx] : STATE_GOTO_R_S_INDEX_MAP)
                {
                    cachefile << "    { " << gotoidx << ", " << rsidx << " }," << endl;
                }
                cachefile << "};" << endl;
                ///////////////////////////////////////////////////////////////////////
                // Generate FOLLOW
                cachefile << "const int woolang_follow_sets[][" << te_list.size() << " + 1] = {" << endl;
                cachefile << "// {   NONTERM_ID,  TE_ID1, TE_ID2, ...  }" << endl;
                for (auto& [follow_item_sym, follow_items] : grammar_instance->FOLLOW_SET)
                {
                    wo_test(std::holds_alternative<grammar::nt>(follow_item_sym));

                    std::vector<int> follow_set(te_list.size() + 1, 0);
                    cachefile << "{ " << nonte_list[std::get<grammar::nt>(follow_item_sym).nt_name] << ",  ";
                    for (auto& tes : follow_items)
                    {
                        cachefile << te_list[tes.t_type] << ", ";
                    }
                    cachefile << "}," << endl;
                }
                cachefile << "};" << endl;
                ///////////////////////////////////////////////////////////////////////
                // Generate ORIGIN_P
                cachefile << "const int woolang_origin_p[][" << max_length_producer << " + 3] = {" << endl;
                cachefile << "// {   NONTERM_ID, >> PFUNC_ID >> PNUM >>P01, P02, (te +, nt -)...  }" << endl;
                for (auto& [aim, rule] : grammar_instance->ORGIN_P)
                {
                    if (aim.builder_index == 0)
                    {
                        wo_stdout
                            << ANSI_HIY "WooGramma: " ANSI_RST "Producer: " ANSI_HIR
                            << grammar::lr_item{ grammar::rule{aim, rule}, size_t(-1), grammar::te(grammar::ttype::l_eof) }
                            << ANSI_RST " have no ast builder, using default builder.."
                            << wo_endl;
                    }

                    cachefile << "   { " << nonte_list[aim.nt_name] << ", " << aim.builder_index << ", " << rule.size() << ", ";
                    for (auto& sym : rule)
                    {
                        if (std::holds_alternative<grammar::te>(sym))
                            cachefile << te_list[std::get<grammar::te>(sym).t_type] << ",";
                        else
                            cachefile << -nonte_list[std::get<grammar::nt>(sym).nt_name] << ",";
                    }
                    cachefile << "}," << endl;
                }
                cachefile << "};" << endl;
                ///////////////////////////////////////////////////////////////////////

                wo_test(has_acc);

                cachefile << "int woolang_accept_state = " << acc_state << ";" << std::endl;
                cachefile << "int woolang_accept_term = " << acc_term << ";" << std::endl;
                cachefile << "#else" << endl;
                cachefile << "void wo_read_lr1_cache(wo::grammar & gram);" << endl;
                cachefile << "void wo_read_lr1_to(wo::grammar::lr1table_t & out_lr1table);" << endl;
                cachefile << "void wo_read_follow_set_to(wo::grammar::sym_nts_t & out_followset);" << endl;
                cachefile << "void wo_read_origin_p_to(std::vector<wo::grammar::rule> & out_origin_p);" << endl;
                cachefile << "#endif" << endl;
                cachefile << "#endif" << endl;
                cachefile << "}// end of namespace 'wo'" << endl;
                cachefile.flush();

                wo_stdout << ANSI_HIG "WooGramma: " ANSI_RST "Finished." << wo_endl;
            }
            else
            {
                wo_stdout <<
                    ANSI_HIG
                    "WooGramma: "
                    ANSI_RST
                    "Skip generating LR(1) table cache (WO_GRAMMAR_SKIP_GEN_LR1_TABLE_CACHE is true)."
                    << wo_endl;
            }

#if defined(WO_LANG_GRAMMAR_LR1_AUTO_GENED) && !WO_GRAMMAR_SKIP_GEN_LR1_TABLE_CACHE
        }
#undef WO_LANG_GRAMMAR_LR1_AUTO_GENED
#endif

        // DESCRIBE HOW TO GENERATE AST HERE:

        // used for hiding warning..
        goto register_ast_builder_function;

    register_ast_builder_function:
        // finally work
        for (auto& [rule_nt, _tokens] : grammar_instance->ORGIN_P)
        {
            if (rule_nt.builder_index)
                rule_nt.ast_create_func = ast::get_builder(rule_nt.builder_index);
        }

        grammar_instance->finish_rt();
    }
    void shutdown_woolang_grammar()
    {
        wo_assert(grammar_instance != nullptr);

        grammar_instance.reset();
        ast::shutdown_builder();
    }
    grammar* get_grammar_instance()
    {
        wo_assert(grammar_instance != nullptr);

        return grammar_instance.get();
    }
#endif
}
