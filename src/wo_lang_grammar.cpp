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

        const char *GRAMMAR_SRC_FILE = WO_SRC_PATH "/src/wo_lang_grammar.cpp";

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
            wo_stdout << ANSI_HIY "WooGramma: " ANSI_RST "Syntax update detected, LR table cache is being regenerating..." << wo_endl;

            using SL = wo::grammar::symlist;

            using namespace ast;

#define B WO_ASTBUILDER_INDEX
#define SL(...) wo::grammar::symlist{__VA_ARGS__}
#define TE(IDENT) wo::grammar::te(wo::grammar::ttype::IDENT)
#define NT(IDENT) wo::grammar::nt(#IDENT)

#define P(TARGET, PRODUCES, ...) NT(TARGET) >> SL PRODUCES >> B(__VA_ARGS__)

            grammar_instance = std::make_unique<grammar>(std::vector<grammar::rule>{
                P(PROGRAM_AUGMENTED, (NT(PROGRAM)), pass_direct<0>),
                P(PROGRAM, (TE(l_empty)), pass_direct<0>),
                P(PROGRAM, (NT(USELESS_TOKEN)), pass_direct<0>),
                P(PROGRAM, (NT(PARAGRAPH)), pass_direct<0>),
                P(PARAGRAPH, (NT(SENTENCE_LIST)), pass_direct<0>),
                P(SENTENCE_LIST, (NT(SENTENCE_LIST), NT(SENTENCE)), pass_append_list<1, 0>),
                P(SENTENCE_LIST, (NT(SENTENCE)), pass_create_list<0>),
                // NOTE: macro might defined after import sentence. to make sure macro can be handle correctly.
                //      we make sure import happend before macro been peek and check.
                P(SENTENCE, (TE(l_defer), NT(BLOCKED_SENTENCE)), pass_defer),
                P(SENTENCE, (NT(IMPORT_SENTENCE), TE(l_semicolon)), pass_direct<0>),
                P(IMPORT_SENTENCE, (TE(l_import), NT(SCOPED_LIST_NORMAL)), pass_import_files),
                P(IMPORT_SENTENCE, (TE(l_export), TE(l_import), NT(SCOPED_LIST_NORMAL)), pass_import_files),
                P(SENTENCE, (NT(DECL_ATTRIBUTE), TE(l_using), NT(SCOPED_LIST_NORMAL), TE(l_semicolon)), pass_using_namespace),
                P(DECLARE_NEW_TYPE, (NT(DECL_ATTRIBUTE), TE(l_using), NT(IDENTIFIER), NT(DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY), TE(l_assign), NT(TYPE)), pass_using_type_as),
                P(SENTENCE, (NT(DECLARE_NEW_TYPE), NT(SENTENCE_BLOCK_MAY_SEMICOLON)), pass_using_typename_space),
                P(SENTENCE_BLOCK_MAY_SEMICOLON, (TE(l_semicolon)), pass_empty),
                P(SENTENCE_BLOCK_MAY_SEMICOLON, (NT(SENTENCE_BLOCK)), pass_direct<0>),
                P(SENTENCE, (NT(DECL_ATTRIBUTE), TE(l_alias), NT(IDENTIFIER), NT(DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY), TE(l_assign), NT(TYPE), TE(l_semicolon)), pass_alias_type_as),
                //////////////////////////////////////////////////////////////////////////////////////
                P(SENTENCE, (NT(DECL_ENUM)), pass_direct<0>),
                P(DECL_ENUM, (NT(DECL_ATTRIBUTE), TE(l_enum), NT(AST_TOKEN_IDENTIFER), TE(l_left_curly_braces), NT(ENUM_ITEMS), TE(l_right_curly_braces)), pass_enum_finalize),
                P(ENUM_ITEMS, (NT(ENUM_ITEM_LIST), NT(COMMA_MAY_EMPTY)), pass_direct<0>),
                P(ENUM_ITEM_LIST, (NT(ENUM_ITEM)), pass_create_list<0>),
                P(ENUM_ITEM_LIST, (NT(ENUM_ITEM_LIST), TE(l_comma), NT(ENUM_ITEM)), pass_append_list<2, 0>),
                P(ENUM_ITEM, (NT(IDENTIFIER)), pass_enum_item_create),
                P(ENUM_ITEM, (NT(IDENTIFIER), TE(l_assign), NT(EXPRESSION)), pass_enum_item_create),
                P(SENTENCE, (NT(DECL_NAMESPACE)), pass_direct<0>),
                P(DECL_NAMESPACE, (TE(l_namespace), NT(SPACE_NAME_LIST), NT(SENTENCE_BLOCK)), pass_namespace),
                P(SPACE_NAME_LIST, (NT(SPACE_NAME)), pass_create_list<0>),
                P(SPACE_NAME_LIST, (NT(SPACE_NAME_LIST), TE(l_scopeing), NT(SPACE_NAME)), pass_append_list<2, 0>),
                P(SPACE_NAME, (NT(IDENTIFIER)), pass_token),
                P(BLOCKED_SENTENCE, (NT(SENTENCE)), pass_sentence_block<0>),
                P(SENTENCE, (NT(SENTENCE_BLOCK)), pass_direct<0>),
                P(SENTENCE_BLOCK, (TE(l_left_curly_braces), NT(PARAGRAPH), TE(l_right_curly_braces)), pass_sentence_block<1>),
                // Because of CONSTANT MAP => ,,, => { l_empty } Following production will cause R-R Conflict
                // NT(PARAGRAPH) >> SL(TE(l_empty)),
                // So, just make a production like this:
                P(SENTENCE_BLOCK, (TE(l_left_curly_braces), TE(l_right_curly_braces)), pass_empty),
                // NOTE: Why the production can solve the conflict?
                //       A > {}
                //       B > {l_empty}
                //       In fact, A B have same production, but in wo_lr(1) parser, l_empty have a lower priority then production like A
                //       This rule can solve many grammar conflict easily, but in some case, it will cause bug, so please use it carefully.
                P(DECL_ATTRIBUTE, (NT(DECL_ATTRIBUTE_ITEMS)), pass_direct<0>),
                P(DECL_ATTRIBUTE, (TE(l_empty)), pass_empty),
                P(DECL_ATTRIBUTE_ITEMS, (NT(DECL_ATTRIBUTE_ITEM)), pass_attribute),
                P(DECL_ATTRIBUTE_ITEMS, (NT(DECL_ATTRIBUTE_ITEMS), NT(DECL_ATTRIBUTE_ITEM)), pass_attribute_append),
                P(DECL_ATTRIBUTE_ITEM, (NT(LIFECYCLE_MODIFER)), pass_direct<0>),
                P(DECL_ATTRIBUTE_ITEM, (NT(EXTERNAL_MODIFER)), pass_direct<0>),
                P(DECL_ATTRIBUTE_ITEM, (NT(ACCESS_MODIFIER)), pass_direct<0>),
                P(LIFECYCLE_MODIFER, (TE(l_static)), pass_token),
                P(EXTERNAL_MODIFER, (TE(l_extern)), pass_token),
                P(ACCESS_MODIFIER, (TE(l_public)), pass_token),
                P(ACCESS_MODIFIER, (TE(l_private)), pass_token),
                P(ACCESS_MODIFIER, (TE(l_protected)), pass_token),
                P(SENTENCE, (TE(l_semicolon)), pass_empty),
                P(WHERE_CONSTRAINT_WITH_SEMICOLON, (NT(WHERE_CONSTRAINT), TE(l_semicolon)), pass_direct<0>),
                P(WHERE_CONSTRAINT_WITH_SEMICOLON, (TE(l_empty)), pass_empty),
                P(WHERE_CONSTRAINT_MAY_EMPTY, (NT(WHERE_CONSTRAINT)), pass_direct<0>),
                P(WHERE_CONSTRAINT_MAY_EMPTY, (TE(l_empty)), pass_empty),
                P(WHERE_CONSTRAINT, (TE(l_where), NT(CONSTRAINT_LIST), NT(COMMA_MAY_EMPTY)), pass_build_where_constraint),
                P(CONSTRAINT_LIST, (NT(EXPRESSION)), pass_create_list<0>),
                P(CONSTRAINT_LIST, (NT(CONSTRAINT_LIST), TE(l_comma), NT(EXPRESSION)), pass_append_list<2, 0>),
                P(SENTENCE, (NT(FUNC_DEFINE_WITH_NAME)), pass_direct<0>),
                P(FUNC_DEFINE_WITH_NAME, (NT(DECL_ATTRIBUTE), TE(l_func), NT(AST_TOKEN_IDENTIFER), NT(DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY), TE(l_left_brackets), NT(ARGDEFINE), TE(l_right_brackets), NT(RETURN_TYPE_DECLEAR_MAY_EMPTY), NT(WHERE_CONSTRAINT_WITH_SEMICOLON), NT(SENTENCE_BLOCK)), pass_func_def_named),
                P(FUNC_DEFINE_WITH_NAME, (NT(DECL_ATTRIBUTE), TE(l_func), TE(l_operator), NT(OVERLOADINGABLE_OPERATOR), NT(DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY), TE(l_left_brackets), NT(ARGDEFINE), TE(l_right_brackets), NT(RETURN_TYPE_DECLEAR_MAY_EMPTY), NT(WHERE_CONSTRAINT_WITH_SEMICOLON), NT(SENTENCE_BLOCK)), pass_func_def_oper),
                P(OVERLOADINGABLE_OPERATOR, (TE(l_add)), pass_token),
                P(OVERLOADINGABLE_OPERATOR, (TE(l_sub)), pass_token),
                P(OVERLOADINGABLE_OPERATOR, (TE(l_mul)), pass_token),
                P(OVERLOADINGABLE_OPERATOR, (TE(l_div)), pass_token),
                P(OVERLOADINGABLE_OPERATOR, (TE(l_mod)), pass_token),
                P(OVERLOADINGABLE_OPERATOR, (TE(l_less)), pass_token),
                P(OVERLOADINGABLE_OPERATOR, (TE(l_larg)), pass_token),
                P(OVERLOADINGABLE_OPERATOR, (TE(l_less_or_equal)), pass_token),
                P(OVERLOADINGABLE_OPERATOR, (TE(l_larg_or_equal)), pass_token),
                P(OVERLOADINGABLE_OPERATOR, (TE(l_equal)), pass_token),
                P(OVERLOADINGABLE_OPERATOR, (TE(l_not_equal)), pass_token),
                P(OVERLOADINGABLE_OPERATOR, (TE(l_land)), pass_token),
                P(OVERLOADINGABLE_OPERATOR, (TE(l_lor)), pass_token),
                // P(OVERLOADINGABLE_OPERATOR, (TE(l_lnot)), pass_token),
                P(OVERLOADINGABLE_OPERATOR, (TE(l_index_begin), TE(l_index_end)), pass_token),
                P(EXTERN_FROM, (TE(l_extern), TE(l_left_brackets), TE(l_literal_string), TE(l_comma), TE(l_literal_string), NT(EXTERN_ATTRIBUTES), TE(l_right_brackets)), pass_extern),
                P(EXTERN_FROM, (TE(l_extern), TE(l_left_brackets), TE(l_literal_string), NT(EXTERN_ATTRIBUTES), TE(l_right_brackets)), pass_extern),
                P(EXTERN_ATTRIBUTES, (TE(l_empty)), pass_empty),
                P(EXTERN_ATTRIBUTES, (TE(l_comma), NT(EXTERN_ATTRIBUTE_LIST)), pass_direct<1>),
                P(EXTERN_ATTRIBUTE_LIST, (NT(EXTERN_ATTRIBUTE)), pass_create_list<0>),
                P(EXTERN_ATTRIBUTE_LIST, (NT(EXTERN_ATTRIBUTE_LIST), TE(l_comma), NT(EXTERN_ATTRIBUTE)), pass_append_list<2, 0>),
                P(EXTERN_ATTRIBUTE, (TE(l_identifier)), pass_token),
                P(FUNC_DEFINE_WITH_NAME, (NT(EXTERN_FROM), NT(DECL_ATTRIBUTE), TE(l_func), NT(AST_TOKEN_IDENTIFER), NT(DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY), TE(l_left_brackets), NT(ARGDEFINE), TE(l_right_brackets), NT(RETURN_TYPE_DECLEAR), NT(WHERE_CONSTRAINT_MAY_EMPTY), TE(l_semicolon)), pass_func_def_extn),
                P(FUNC_DEFINE_WITH_NAME, (NT(EXTERN_FROM), NT(DECL_ATTRIBUTE), TE(l_func), TE(l_operator), NT(OVERLOADINGABLE_OPERATOR), NT(DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY), TE(l_left_brackets), NT(ARGDEFINE), TE(l_right_brackets), NT(RETURN_TYPE_DECLEAR), NT(WHERE_CONSTRAINT_MAY_EMPTY), TE(l_semicolon)), pass_func_def_extn_oper),
                P(SENTENCE, (NT(MAY_LABELED_LOOP)), pass_direct<0>),
                P(MAY_LABELED_LOOP, (NT(IDENTIFIER), TE(l_at), NT(LOOP)), pass_mark_label),
                P(MAY_LABELED_LOOP, (NT(LOOP)), pass_direct<0>),
                P(LOOP, (NT(WHILE)), pass_direct<0>),
                P(LOOP, (NT(FORLOOP)), pass_direct<0>),
                P(LOOP, (NT(FOREACH)), pass_direct<0>),
                P(WHILE, (TE(l_while), TE(l_left_brackets), NT(EXPRESSION), TE(l_right_brackets), NT(BLOCKED_SENTENCE)), pass_while),
                P(VAR_DEFINE_WITH_SEMICOLON, (NT(VAR_DEFINE_LET_SENTENCE), TE(l_semicolon)), pass_direct<0>),
                P(FORLOOP, (TE(l_for), TE(l_left_brackets), NT(VAR_DEFINE_WITH_SEMICOLON), NT(MAY_EMPTY_EXPRESSION), TE(l_semicolon), NT(MAY_EMPTY_EXPRESSION), TE(l_right_brackets), NT(BLOCKED_SENTENCE)), pass_for_defined),
                P(FORLOOP, (TE(l_for), TE(l_left_brackets), NT(MAY_EMPTY_EXPRESSION), TE(l_semicolon), NT(MAY_EMPTY_EXPRESSION), TE(l_semicolon), NT(MAY_EMPTY_EXPRESSION), TE(l_right_brackets), NT(BLOCKED_SENTENCE)), pass_for_expr),
                P(SENTENCE, (TE(l_break), TE(l_semicolon)), pass_break),
                P(SENTENCE, (TE(l_continue), TE(l_semicolon)), pass_continue),
                P(SENTENCE, (TE(l_break), NT(IDENTIFIER), TE(l_semicolon)), pass_break_label),
                P(SENTENCE, (TE(l_continue), NT(IDENTIFIER), TE(l_semicolon)), pass_continue_label),
                P(MAY_EMPTY_EXPRESSION, (NT(EXPRESSION)), pass_direct<0>),
                P(MAY_EMPTY_EXPRESSION, (TE(l_empty)), pass_empty),
                P(FOREACH, (TE(l_for), TE(l_left_brackets), NT(DECL_ATTRIBUTE), TE(l_let), NT(DEFINE_PATTERN), TE(l_typecast), NT(EXPRESSION), TE(l_right_brackets), NT(BLOCKED_SENTENCE)), pass_foreach),
                P(SENTENCE, (NT(IF)), pass_direct<0>),
                P(IF, (TE(l_if), TE(l_left_brackets), NT(EXPRESSION), TE(l_right_brackets), NT(BLOCKED_SENTENCE), NT(ELSE)), pass_if),
                P(ELSE, (TE(l_empty)), pass_empty),
                P(ELSE, (TE(l_else), NT(BLOCKED_SENTENCE)), pass_direct<1>),
                P(SENTENCE, (NT(VAR_DEFINE_LET_SENTENCE), TE(l_semicolon)), pass_direct<0>),
                P(VAR_DEFINE_LET_SENTENCE, (NT(DECL_ATTRIBUTE), TE(l_let), NT(VARDEFINES)), pass_variable_defines),
                P(VARDEFINES, (NT(VARDEFINE)), pass_create_list<0>),
                P(VARDEFINES, (NT(VARDEFINES), TE(l_comma), NT(VARDEFINE)), pass_append_list<2, 0>),
                P(VARDEFINE, (NT(DEFINE_PATTERN_MAY_TEMPLATE), TE(l_assign), NT(EXPRESSION)), pass_variable_define_item),
                P(DEFINE_PATTERN_MAY_TEMPLATE, (NT(DEFINE_PATTERN)), pass_direct<0>),
                P(DEFINE_PATTERN_MAY_TEMPLATE, (NT(DEFINE_PATTERN_WITH_TEMPLATE)), pass_direct<0>),
                P(SENTENCE, (TE(l_return), NT(EXPRESSION), TE(l_semicolon)), pass_return_value),
                P(SENTENCE, (TE(l_return), TE(l_semicolon)), pass_return),
                P(SENTENCE, (NT(EXPRESSION), TE(l_semicolon)), pass_direct_keep_source_location<0>),
                P(EXPRESSION, (TE(l_do), NT(EXPRESSION)), pass_do_void_cast),
                P(EXPRESSION, (TE(l_mut), NT(EXPRESSION)), pass_mark_mut),
                P(EXPRESSION, (TE(l_immut), NT(EXPRESSION)), pass_mark_immut),
                P(EXPRESSION, (NT(ASSIGNMENT)), pass_direct<0>),
                P(EXPRESSION, (NT(LOGICAL_OR)), pass_direct<0>),
                P(ASSIGNMENT, (NT(ASSIGNED_PATTERN), TE(l_assign), NT(EXPRESSION)), pass_assign_operation),
                P(ASSIGNMENT, (NT(ASSIGNED_PATTERN), TE(l_add_assign), NT(EXPRESSION)), pass_assign_operation),
                P(ASSIGNMENT, (NT(ASSIGNED_PATTERN), TE(l_sub_assign), NT(EXPRESSION)), pass_assign_operation),
                P(ASSIGNMENT, (NT(ASSIGNED_PATTERN), TE(l_mul_assign), NT(EXPRESSION)), pass_assign_operation),
                P(ASSIGNMENT, (NT(ASSIGNED_PATTERN), TE(l_div_assign), NT(EXPRESSION)), pass_assign_operation),
                P(ASSIGNMENT, (NT(ASSIGNED_PATTERN), TE(l_mod_assign), NT(EXPRESSION)), pass_assign_operation),
                P(ASSIGNMENT, (NT(ASSIGNED_PATTERN), TE(l_value_assign), NT(EXPRESSION)), pass_assign_operation),
                P(ASSIGNMENT, (NT(ASSIGNED_PATTERN), TE(l_value_add_assign), NT(EXPRESSION)), pass_assign_operation),
                P(ASSIGNMENT, (NT(ASSIGNED_PATTERN), TE(l_value_sub_assign), NT(EXPRESSION)), pass_assign_operation),
                P(ASSIGNMENT, (NT(ASSIGNED_PATTERN), TE(l_value_mul_assign), NT(EXPRESSION)), pass_assign_operation),
                P(ASSIGNMENT, (NT(ASSIGNED_PATTERN), TE(l_value_div_assign), NT(EXPRESSION)), pass_assign_operation),
                P(ASSIGNMENT, (NT(ASSIGNED_PATTERN), TE(l_value_mod_assign), NT(EXPRESSION)), pass_assign_operation),
                P(ASSIGNED_PATTERN, (NT(LEFT)), pass_pattern_for_assign),
                P(EXPRESSION, (NT(LOGICAL_OR), TE(l_question), NT(EXPRESSION), TE(l_or), NT(EXPRESSION)), pass_conditional_expression),
                P(FACTOR, (NT(FUNC_DEFINE)), pass_direct<0>),
                P(FUNC_DEFINE, (NT(DECL_ATTRIBUTE), TE(l_func), NT(DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY), TE(l_left_brackets), NT(ARGDEFINE), TE(l_right_brackets), NT(RETURN_TYPE_DECLEAR_MAY_EMPTY), NT(WHERE_CONSTRAINT_WITH_SEMICOLON), NT(SENTENCE_BLOCK)), pass_func_lambda_ml),
                P(FUNC_DEFINE, (TE(l_lambda), NT(DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY), NT(ARGDEFINE), TE(l_assign), NT(RETURN_EXPR_BLOCK_IN_LAMBDA), NT(WHERE_DECL_FOR_LAMBDA), TE(l_semicolon)), pass_func_lambda),
                // May empty
                P(WHERE_DECL_FOR_LAMBDA, (TE(l_empty)), pass_empty),
                P(WHERE_DECL_FOR_LAMBDA, (TE(l_where), NT(VARDEFINES)), pass_reverse_vardef),
                P(RETURN_EXPR_BLOCK_IN_LAMBDA, (NT(RETURN_EXPR_IN_LAMBDA)), pass_sentence_block<0>),
                P(RETURN_EXPR_IN_LAMBDA, (NT(EXPRESSION)), pass_return_lambda),
                P(RETURN_TYPE_DECLEAR_MAY_EMPTY, (TE(l_empty)), pass_empty),
                P(RETURN_TYPE_DECLEAR_MAY_EMPTY, (NT(RETURN_TYPE_DECLEAR)), pass_direct<0>),
                P(RETURN_TYPE_DECLEAR, (TE(l_function_result), NT(TYPE)), pass_direct_keep_source_location<1>),
                P(TYPE_DECLEAR, (TE(l_typecast), NT(TYPE)), pass_direct_keep_source_location<1>),
                /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
                P(TYPE, (NT(ORIGIN_TYPE)), pass_direct<0>),
                P(TYPE, (TE(l_mut), NT(ORIGIN_TYPE)), pass_type_mutable),
                P(TYPE, (TE(l_immut), NT(ORIGIN_TYPE)), pass_type_immutable),

                P(ORIGIN_TYPE, (NT(AST_TOKEN_NIL)), pass_type_nil),
                P(ORIGIN_TYPE, (NT(TUPLE_TYPE_LIST), TE(l_function_result), NT(TYPE)), pass_type_func),

                P(ORIGIN_TYPE, (TE(l_struct), TE(l_left_curly_braces), NT(STRUCT_MEMBER_DEFINES), TE(l_right_curly_braces)), pass_type_struct),
                P(STRUCT_MEMBER_DEFINES, (TE(l_empty)), pass_create_list<0>),
                P(STRUCT_MEMBER_DEFINES, (NT(STRUCT_MEMBERS_LIST), NT(COMMA_MAY_EMPTY)), pass_direct<0>),
                P(STRUCT_MEMBERS_LIST, (NT(STRUCT_MEMBER_PAIR)), pass_create_list<0>),
                P(STRUCT_MEMBERS_LIST, (NT(STRUCT_MEMBERS_LIST), TE(l_comma), NT(STRUCT_MEMBER_PAIR)), pass_append_list<2, 0>),
                P(STRUCT_MEMBER_PAIR, (NT(ACCESS_MODIFIER_MAY_EMPTY), NT(IDENTIFIER), NT(TYPE_DECLEAR)), pass_type_struct_field),
                P(ACCESS_MODIFIER_MAY_EMPTY, (NT(ACCESS_MODIFIER)), pass_direct<0>),
                P(ACCESS_MODIFIER_MAY_EMPTY, (TE(l_empty)), pass_empty),

                P(ORIGIN_TYPE, (NT(STRUCTABLE_TYPE)), pass_direct<0>),
                P(STRUCTABLE_TYPE, (NT(SCOPED_IDENTIFIER_FOR_TYPE)), pass_type_from_identifier),
                P(STRUCTABLE_TYPE, (NT(TYPEOF)), pass_direct<0>),
                P(STRUCTABLE_TYPE_FOR_CONSTRUCT, (NT(SCOPED_IDENTIFIER_FOR_VAL)), pass_type_from_identifier),
                P(STRUCTABLE_TYPE_FOR_CONSTRUCT, (NT(TYPEOF)), pass_direct<0>),
                P(TYPEOF, (TE(l_typeof), TE(l_left_brackets), NT(EXPRESSION), TE(l_right_brackets)), pass_typeof),
                P(TYPEOF, (TE(l_typeof), TE(l_template_using_begin), NT(TYPE), TE(l_larg)), pass_direct<2>),
                P(TYPEOF, (TE(l_typeof), TE(l_less), NT(TYPE), TE(l_larg)), pass_direct<2>),
                P(ORIGIN_TYPE, (NT(TUPLE_TYPE_LIST)), pass_type_tuple),
                P(TUPLE_TYPE_LIST, (TE(l_left_brackets), NT(TUPLE_TYPE_LIST_ITEMS_MAY_EMPTY), TE(l_right_brackets)), pass_direct<1>),
                P(TUPLE_TYPE_LIST_ITEMS_MAY_EMPTY, (NT(TUPLE_TYPE_LIST_ITEMS), NT(COMMA_MAY_EMPTY)), pass_direct<0>),
                P(TUPLE_TYPE_LIST_ITEMS_MAY_EMPTY, (NT(VARIADIC_MAY_EMPTY)), pass_create_list<0>),
                P(TUPLE_TYPE_LIST_ITEMS, (NT(TYPE)), pass_create_list<0>),
                P(TUPLE_TYPE_LIST_ITEMS, (NT(TUPLE_TYPE_LIST_ITEMS), TE(l_comma), NT(TYPE)), pass_append_list<2, 0>),
                P(TUPLE_TYPE_LIST_ITEMS, (NT(TUPLE_TYPE_LIST_ITEMS), TE(l_comma), NT(VARIADIC_MAY_EMPTY)), pass_append_list<2, 0>),
                P(VARIADIC_MAY_EMPTY, (TE(l_variadic_sign)), pass_token),
                P(VARIADIC_MAY_EMPTY, (TE(l_empty)), pass_empty),
                //////////////////////////////////////////////////////////////////////////////////////////////
                P(ARGDEFINE, (TE(l_empty)), pass_create_list<0>),
                P(ARGDEFINE, (NT(ARGDEFINE_VAR_ITEM)), pass_create_list<0>),
                P(ARGDEFINE, (NT(ARGDEFINE), TE(l_comma), NT(ARGDEFINE_VAR_ITEM)), pass_append_list<2, 0>),
                P(ARGDEFINE_VAR_ITEM, (NT(DEFINE_PATTERN), NT(TYPE_DECL_MAY_EMPTY)), pass_func_argument),
                P(ARGDEFINE_VAR_ITEM, (TE(l_variadic_sign)), pass_token),
                P(TYPE_DECL_MAY_EMPTY, (NT(TYPE_DECLEAR)), pass_direct<0>),
                P(TYPE_DECL_MAY_EMPTY, (TE(l_empty)), pass_empty),
                P(LOGICAL_OR, (NT(LOGICAL_AND)), pass_direct<0>),
                P(LOGICAL_OR, (NT(LOGICAL_OR), TE(l_lor), NT(LOGICAL_AND)), pass_binary_operation),
                P(LOGICAL_AND, (NT(EQUATION)), pass_direct<0>),
                P(LOGICAL_AND, (NT(LOGICAL_AND), TE(l_land), NT(EQUATION)), pass_binary_operation),
                P(EQUATION, (NT(RELATION)), pass_direct<0>),
                P(EQUATION, (NT(EQUATION), TE(l_equal), NT(RELATION)), pass_binary_operation),
                P(EQUATION, (NT(EQUATION), TE(l_not_equal), NT(RELATION)), pass_binary_operation),
                P(RELATION, (NT(SUMMATION)), pass_direct<0>),
                P(RELATION, (NT(RELATION), TE(l_larg), NT(SUMMATION)), pass_binary_operation),
                P(RELATION, (NT(RELATION), TE(l_less), NT(SUMMATION)), pass_binary_operation),
                P(RELATION, (NT(RELATION), TE(l_less_or_equal), NT(SUMMATION)), pass_binary_operation),
                P(RELATION, (NT(RELATION), TE(l_larg_or_equal), NT(SUMMATION)), pass_binary_operation),
                P(SUMMATION, (NT(MULTIPLICATION)), pass_direct<0>),
                P(SUMMATION, (NT(SUMMATION), TE(l_add), NT(MULTIPLICATION)), pass_binary_operation),
                P(SUMMATION, (NT(SUMMATION), TE(l_sub), NT(MULTIPLICATION)), pass_binary_operation),
                P(MULTIPLICATION, (NT(SINGLE_VALUE)), pass_direct<0>),
                P(MULTIPLICATION, (NT(MULTIPLICATION), TE(l_mul), NT(SINGLE_VALUE)), pass_binary_operation),
                P(MULTIPLICATION, (NT(MULTIPLICATION), TE(l_div), NT(SINGLE_VALUE)), pass_binary_operation),
                P(MULTIPLICATION, (NT(MULTIPLICATION), TE(l_mod), NT(SINGLE_VALUE)), pass_binary_operation),
                P(SINGLE_VALUE, (NT(UNARIED_FACTOR)), pass_direct<0>),
                P(UNARIED_FACTOR, (NT(FACTOR_TYPE_CASTING)), pass_direct<0>),
                P(UNARIED_FACTOR, (TE(l_sub), NT(UNARIED_FACTOR)), pass_unary_operation),
                P(UNARIED_FACTOR, (TE(l_lnot), NT(UNARIED_FACTOR)), pass_unary_operation),
                P(UNARIED_FACTOR, (NT(INV_FUNCTION_CALL)), pass_direct<0>),
                P(FACTOR_TYPE_CASTING, (NT(FACTOR_TYPE_CASTING), NT(AS_TYPE)), pass_check_type_as),
                P(FACTOR_TYPE_CASTING, (NT(FACTOR_TYPE_CASTING), NT(IS_TYPE)), pass_check_type_is),
                P(AS_TYPE, (TE(l_as), NT(TYPE)), pass_direct_keep_source_location<1>),
                P(IS_TYPE, (TE(l_is), NT(TYPE)), pass_direct_keep_source_location<1>),

                P(FACTOR_TYPE_CASTING, (NT(FACTOR_TYPE_CASTING), NT(TYPE_DECLEAR)), pass_cast_type),
                P(FACTOR_TYPE_CASTING, (NT(FACTOR)), pass_direct<0>),
                P(FACTOR, (NT(LEFT)), pass_direct<0>),
                P(FACTOR, (TE(l_left_brackets), NT(EXPRESSION), TE(l_right_brackets)), pass_direct<1>),
                P(FACTOR, (NT(UNIT)), pass_direct<0>),
                P(EXPRESSION, (NT(ARGUMENT_EXPAND)), pass_direct<0>),
                P(ARGUMENT_EXPAND, (NT(FACTOR_TYPE_CASTING), TE(l_variadic_sign)), pass_expand_arguments),
                P(UNIT, (TE(l_variadic_sign)), pass_variadic_arguments_pack),
                P(UNIT, (TE(l_literal_integer)), pass_literal),
                P(UNIT, (TE(l_literal_real)), pass_literal),
                P(UNIT, (TE(l_literal_string)), pass_literal),
                P(UNIT, (TE(l_literal_raw_string)), pass_literal),
                P(UNIT, (NT(LITERAL_CHAR)), pass_literal_char),
                P(UNIT, (TE(l_literal_handle)), pass_literal),
                P(UNIT, (TE(l_nil)), pass_literal),
                P(UNIT, (TE(l_true)), pass_literal),
                P(UNIT, (TE(l_false)), pass_literal),
                P(UNIT, (TE(l_typeid), TE(l_template_using_begin), NT(TYPE), TE(l_larg)), pass_typeid),
                P(UNIT, (TE(l_typeid), TE(l_less), NT(TYPE), TE(l_larg)), pass_typeid),
                P(UNIT, (TE(l_typeid), TE(l_left_brackets), NT(EXPRESSION), TE(l_right_brackets)), pass_typeid_with_expr),
                P(LITERAL_CHAR, (TE(l_literal_char)), pass_token),
                P(UNIT, (NT(FORMAT_STRING)), pass_direct<0>),
                P(FORMAT_STRING, (NT(LITERAL_FORMAT_STRING_BEGIN), NT(FORMAT_STRING_LIST), NT(LITERAL_FORMAT_STRING_END)), pass_format_finish),
                P(FORMAT_STRING_LIST, (NT(EXPRESSION)), pass_format_cast_string),
                P(FORMAT_STRING_LIST, (NT(FORMAT_STRING_LIST), NT(LITERAL_FORMAT_STRING), NT(EXPRESSION)), pass_format_connect),
                P(LITERAL_FORMAT_STRING_BEGIN, (TE(l_format_string_begin)), pass_token),
                P(LITERAL_FORMAT_STRING, (TE(l_format_string)), pass_token),
                P(LITERAL_FORMAT_STRING_END, (TE(l_format_string_end)), pass_token),
                P(UNIT, (NT(CONSTANT_MAP)), pass_direct<0>),
                P(UNIT, (NT(CONSTANT_ARRAY)), pass_direct<0>),
                P(CONSTANT_ARRAY, (TE(l_index_begin), NT(CONSTANT_ARRAY_ITEMS), TE(l_index_end)), pass_array_instance),
                P(CONSTANT_ARRAY, (TE(l_index_begin), NT(CONSTANT_ARRAY_ITEMS), TE(l_index_end), TE(l_mut)), pass_vec_instance),
                P(CONSTANT_ARRAY_ITEMS, (NT(COMMA_EXPR)), pass_direct<0>),
                //////////////////////
                P(UNIT, (NT(CONSTANT_TUPLE)), pass_tuple_instance),
                // NOTE: Avoid grammar conflict.
                P(CONSTANT_TUPLE, (TE(l_left_brackets), NT(COMMA_MAY_EMPTY), TE(l_right_brackets)), pass_create_list<1>),
                P(CONSTANT_TUPLE, (TE(l_left_brackets), NT(EXPRESSION), TE(l_comma), NT(COMMA_EXPR), TE(l_right_brackets)), pass_append_list<1, 3>),
                ///////////////////////
                P(CONSTANT_MAP, (TE(l_left_curly_braces), NT(CONSTANT_MAP_PAIRS), TE(l_right_curly_braces)), pass_dict_instance),
                P(CONSTANT_MAP, (TE(l_left_curly_braces), NT(CONSTANT_MAP_PAIRS), TE(l_right_curly_braces), TE(l_mut)), pass_map_instance),
                P(CONSTANT_MAP_PAIRS, (NT(CONSTANT_MAP_PAIR)), pass_create_list<0>),
                P(CONSTANT_MAP_PAIRS, (NT(CONSTANT_MAP_PAIRS), TE(l_comma), NT(CONSTANT_MAP_PAIR)), pass_append_list<2, 0>),
                P(CONSTANT_MAP_PAIR, (TE(l_empty)), pass_empty),
                P(CONSTANT_MAP_PAIR, (NT(CONSTANT_ARRAY), TE(l_assign), NT(EXPRESSION)), pass_dict_field_init_pair),
                P(CALLABLE_LEFT, (NT(SCOPED_IDENTIFIER_FOR_VAL)), pass_variable),
                P(CALLABLE_RIGHT_WITH_BRACKET, (TE(l_left_brackets), NT(EXPRESSION), TE(l_right_brackets)), pass_direct<1>),
                P(SCOPED_IDENTIFIER_FOR_VAL, (NT(SCOPED_LIST_TYPEOF), NT(MAY_EMPTY_LEFT_TEMPLATE_ITEM)), pass_build_identifier_typeof),
                P(SCOPED_IDENTIFIER_FOR_VAL, (NT(SCOPED_LIST_NORMAL), NT(MAY_EMPTY_LEFT_TEMPLATE_ITEM)), pass_build_identifier_normal),
                P(SCOPED_IDENTIFIER_FOR_VAL, (NT(SCOPED_LIST_GLOBAL), NT(MAY_EMPTY_LEFT_TEMPLATE_ITEM)), pass_build_identifier_global),
                P(SCOPED_IDENTIFIER_FOR_TYPE, (NT(SCOPED_LIST_TYPEOF), NT(MAY_EMPTY_TEMPLATE_ITEM)), pass_build_identifier_typeof),
                P(SCOPED_IDENTIFIER_FOR_TYPE, (NT(SCOPED_LIST_NORMAL), NT(MAY_EMPTY_TEMPLATE_ITEM)), pass_build_identifier_normal),
                P(SCOPED_IDENTIFIER_FOR_TYPE, (NT(SCOPED_LIST_GLOBAL), NT(MAY_EMPTY_TEMPLATE_ITEM)), pass_build_identifier_global),
                P(SCOPED_LIST_TYPEOF, (NT(TYPEOF), NT(SCOPING_LIST)), pass_append_list<0, 1>),
                P(SCOPED_LIST_NORMAL, (NT(AST_TOKEN_IDENTIFER), NT(SCOPING_LIST)), pass_append_list<0, 1>),
                P(SCOPED_LIST_NORMAL, (NT(AST_TOKEN_IDENTIFER)), pass_create_list<0>),
                P(SCOPED_LIST_GLOBAL, (NT(SCOPING_LIST)), pass_direct<0>),
                P(SCOPING_LIST, (TE(l_scopeing), NT(AST_TOKEN_IDENTIFER)), pass_create_list<1>),
                P(SCOPING_LIST, (TE(l_scopeing), NT(AST_TOKEN_IDENTIFER), NT(SCOPING_LIST)), pass_append_list<1, 2>),
                P(LEFT, (NT(CALLABLE_LEFT)), pass_direct<0>),
                P(LEFT, (NT(FACTOR_TYPE_CASTING), TE(l_index_begin), NT(EXPRESSION), TE(l_index_end)), pass_index_operation_regular),
                P(LEFT, (NT(FACTOR_TYPE_CASTING), TE(l_index_point), NT(INDEX_POINT_TARGET)), pass_index_operation_member),
                P(INDEX_POINT_TARGET, (NT(IDENTIFIER)), pass_token),
                P(INDEX_POINT_TARGET, (TE(l_literal_integer)), pass_token),
                P(FACTOR, (NT(FUNCTION_CALL)), pass_direct<0>),
                P(DIRECT_CALLABLE_TARGET, (NT(CALLABLE_LEFT)), pass_direct<0>),
                P(DIRECT_CALLABLE_TARGET, (NT(CALLABLE_RIGHT_WITH_BRACKET)), pass_direct<0>),
                P(DIRECT_CALLABLE_TARGET, (NT(FUNC_DEFINE)), pass_direct<0>),
                P(DIRECT_CALL_FIRST_ARG, (NT(FACTOR_TYPE_CASTING)), pass_direct<0>),
                P(DIRECT_CALL_FIRST_ARG, (NT(ARGUMENT_EXPAND)), pass_direct<0>),
                // MONAD GRAMMAR~~~ SUGAR!
                P(FACTOR, (NT(FACTOR_TYPE_CASTING), TE(l_bind_monad), NT(CALLABLE_LEFT)), pass_build_bind_monad),
                P(FACTOR, (NT(FACTOR_TYPE_CASTING), TE(l_bind_monad), NT(CALLABLE_RIGHT_WITH_BRACKET)), pass_build_bind_monad),
                P(FACTOR, (NT(FACTOR_TYPE_CASTING), TE(l_bind_monad), NT(FUNC_DEFINE)), pass_build_bind_monad),
                P(FACTOR, (NT(FACTOR_TYPE_CASTING), TE(l_map_monad), NT(CALLABLE_LEFT)), pass_build_map_monad),
                P(FACTOR, (NT(FACTOR_TYPE_CASTING), TE(l_map_monad), NT(CALLABLE_RIGHT_WITH_BRACKET)), pass_build_map_monad),
                P(FACTOR, (NT(FACTOR_TYPE_CASTING), TE(l_map_monad), NT(FUNC_DEFINE)), pass_build_map_monad),
                P(ARGUMENT_LISTS, (TE(l_left_brackets), NT(COMMA_EXPR), TE(l_right_brackets)), pass_direct<1>),
                P(ARGUMENT_LISTS_MAY_EMPTY, (NT(ARGUMENT_LISTS)), pass_direct<0>),
                P(ARGUMENT_LISTS_MAY_EMPTY, (TE(l_empty)), pass_create_list<0>),
                P(FUNCTION_CALL, (NT(FACTOR), NT(ARGUMENT_LISTS)), pass_normal_function_call),
                P(FUNCTION_CALL, (NT(DIRECT_CALLABLE_VALUE), NT(ARGUMENT_LISTS_MAY_EMPTY)), pass_directly_function_call_append_arguments),
                P(DIRECT_CALLABLE_VALUE, (NT(DIRECT_CALL_FIRST_ARG), TE(l_direct), NT(DIRECT_CALLABLE_TARGET)), pass_directly_function_call),
                P(INV_DIRECT_CALL_FIRST_ARG, (NT(SINGLE_VALUE)), pass_direct<0>),
                P(INV_DIRECT_CALL_FIRST_ARG, (NT(ARGUMENT_EXPAND)), pass_direct<0>),
                P(INV_FUNCTION_CALL, (NT(FUNCTION_CALL), TE(l_inv_direct), NT(INV_DIRECT_CALL_FIRST_ARG)), pass_inverse_function_call),
                P(INV_FUNCTION_CALL, (NT(DIRECT_CALLABLE_TARGET), TE(l_inv_direct), NT(INV_DIRECT_CALL_FIRST_ARG)), pass_inverse_function_call),
                P(COMMA_EXPR, (NT(COMMA_EXPR_ITEMS), NT(COMMA_MAY_EMPTY)), pass_direct<0>),
                P(COMMA_EXPR, (NT(COMMA_MAY_EMPTY)), pass_create_list<0>),
                P(COMMA_EXPR_ITEMS, (NT(EXPRESSION)), pass_create_list<0>),
                P(COMMA_EXPR_ITEMS, (NT(COMMA_EXPR_ITEMS), TE(l_comma), NT(EXPRESSION)), pass_append_list<2, 0>),
                P(COMMA_MAY_EMPTY, (TE(l_comma)), pass_empty),
                P(COMMA_MAY_EMPTY, (TE(l_empty)), pass_empty),
                P(SEMICOLON_MAY_EMPTY, (TE(l_semicolon)), pass_empty),
                P(SEMICOLON_MAY_EMPTY, (TE(l_empty)), pass_empty),
                P(MAY_EMPTY_TEMPLATE_ITEM, (NT(TYPE_TEMPLATE_ITEM)), pass_direct<0>),
                P(MAY_EMPTY_TEMPLATE_ITEM, (TE(l_empty)), pass_empty),
                P(MAY_EMPTY_LEFT_TEMPLATE_ITEM, (TE(l_empty)), pass_empty),
                P(MAY_EMPTY_LEFT_TEMPLATE_ITEM, (NT(LEFT_TEMPLATE_ITEM)), pass_direct<0>),
                P(LEFT_TEMPLATE_ITEM, (TE(l_template_using_begin), NT(TEMPLATE_ARGUMENT_LIST), TE(l_larg)), pass_direct<1>),
                P(TYPE_TEMPLATE_ITEM, (TE(l_less), NT(TEMPLATE_ARGUMENT_LIST), TE(l_larg)), pass_direct<1>),
                // Template for type can be :< ... >
                P(TYPE_TEMPLATE_ITEM, (NT(LEFT_TEMPLATE_ITEM)), pass_direct<0>),
                P(TEMPLATE_ARGUMENT_LIST, (NT(TEMPLATE_ARGUMENT_ITEM)), pass_create_list<0>),
                P(TEMPLATE_ARGUMENT_LIST, (NT(TEMPLATE_ARGUMENT_LIST), TE(l_comma), NT(TEMPLATE_ARGUMENT_ITEM)), pass_append_list<2, 0>),
                P(TEMPLATE_ARGUMENT_ITEM, (NT(TYPE)), pass_create_template_argument),
                P(TEMPLATE_ARGUMENT_ITEM, (TE(l_left_curly_braces), NT(EXPRESSION), TE(l_right_curly_braces)), pass_create_template_argument),
                P(DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY, (TE(l_empty)), pass_empty),
                P(DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY, (NT(DEFINE_TEMPLATE_PARAM_ITEM)), pass_direct<0>),
                P(DEFINE_TEMPLATE_PARAM_ITEM, (TE(l_less), NT(DEFINE_TEMPLATE_PARAM_LIST), TE(l_larg)), pass_direct<1>),
                P(DEFINE_TEMPLATE_PARAM_LIST, (NT(DEFINE_TEMPLATE_PARAM)), pass_create_list<0>),
                P(DEFINE_TEMPLATE_PARAM_LIST, (NT(DEFINE_TEMPLATE_PARAM_LIST), TE(l_comma), NT(DEFINE_TEMPLATE_PARAM)), pass_append_list<2, 0>),
                P(DEFINE_TEMPLATE_PARAM, (NT(IDENTIFIER)), pass_create_template_param),
                P(DEFINE_TEMPLATE_PARAM, (NT(IDENTIFIER), TE(l_typecast), NT(TYPE)), pass_create_template_param),
                P(SENTENCE, (NT(DECL_UNION)), pass_direct<0>),
                P(DECL_UNION, (NT(DECL_ATTRIBUTE), TE(l_union), NT(AST_TOKEN_IDENTIFER), NT(DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY), TE(l_left_curly_braces), NT(UNION_ITEMS), TE(l_right_curly_braces)), pass_union_declare),
                P(UNION_ITEMS, (NT(UNION_ITEM_LIST), NT(COMMA_MAY_EMPTY)), pass_direct<0>),
                P(UNION_ITEM_LIST, (NT(UNION_ITEM)), pass_create_list<0>),
                P(UNION_ITEM_LIST, (NT(UNION_ITEM_LIST), TE(l_comma), NT(UNION_ITEM)), pass_append_list<2, 0>),
                P(UNION_ITEM, (NT(IDENTIFIER)), pass_union_item),
                P(UNION_ITEM, (NT(IDENTIFIER), TE(l_left_brackets), NT(TYPE), TE(l_right_brackets)), pass_union_item_constructor),
                P(SENTENCE, (NT(MATCH_BLOCK)), pass_direct<0>),
                P(MATCH_BLOCK, (TE(l_match), TE(l_left_brackets), NT(EXPRESSION), TE(l_right_brackets), TE(l_left_curly_braces), NT(MATCH_CASES), TE(l_right_curly_braces)), pass_match),
                P(MATCH_CASES, (NT(MATCH_CASE)), pass_create_list<0>),
                P(MATCH_CASES, (NT(MATCH_CASES), NT(MATCH_CASE)), pass_append_list<1, 0>),
                P(MATCH_CASE, (NT(PATTERN_UNION_CASE), TE(l_question), NT(BLOCKED_SENTENCE)), pass_match_union_case),
                // PATTERN-CASE MAY BE A SINGLE-VARIABLE/TUPLE/STRUCT...
                P(PATTERN_UNION_CASE, (NT(IDENTIFIER)), pass_union_pattern_identifier_or_takeplace),
                // PATTERN-CASE MAY BE A UNION
                P(PATTERN_UNION_CASE, (NT(IDENTIFIER), TE(l_left_brackets), NT(DEFINE_PATTERN), TE(l_right_brackets)), pass_union_pattern_contain_element),
                //////////////////////////////////////////////////////////////////////////////////////
                P(UNIT, (NT(STRUCT_INSTANCE_BEGIN), TE(l_left_curly_braces), NT(STRUCT_MEMBER_INITS), TE(l_right_curly_braces)), pass_struct_instance),
                P(STRUCT_INSTANCE_BEGIN, (NT(STRUCTABLE_TYPE_FOR_CONSTRUCT)), pass_direct<0>),
                P(STRUCT_INSTANCE_BEGIN, (TE(l_struct)), pass_empty),
                P(STRUCT_MEMBER_INITS, (NT(STRUCT_MEMBER_INITS_EMPTY)), pass_create_list<0>),
                P(STRUCT_MEMBER_INITS_EMPTY, (TE(l_empty)), pass_empty),
                P(STRUCT_MEMBER_INITS_EMPTY, (TE(l_comma)), pass_empty),
                P(STRUCT_MEMBER_INITS, (NT(STRUCT_MEMBERS_INIT_LIST), NT(COMMA_MAY_EMPTY)), pass_direct<0>),
                P(STRUCT_MEMBERS_INIT_LIST, (NT(STRUCT_MEMBER_INIT_ITEM)), pass_create_list<0>),
                P(STRUCT_MEMBERS_INIT_LIST, (NT(STRUCT_MEMBERS_INIT_LIST), TE(l_comma), NT(STRUCT_MEMBER_INIT_ITEM)), pass_append_list<2, 0>),
                P(STRUCT_MEMBER_INIT_ITEM, (NT(IDENTIFIER), TE(l_assign), NT(EXPRESSION)), pass_struct_member_init_pair),
                ////////////////////////////////////////////////////
                P(DEFINE_PATTERN, (NT(IDENTIFIER)), pass_pattern_identifier_or_takepace),
                P(DEFINE_PATTERN, (TE(l_mut), NT(IDENTIFIER)), pass_pattern_mut_identifier_or_takepace),
                P(DEFINE_PATTERN_WITH_TEMPLATE, (NT(IDENTIFIER), NT(DEFINE_TEMPLATE_PARAM_ITEM)), pass_pattern_identifier_or_takepace_with_template),
                P(DEFINE_PATTERN_WITH_TEMPLATE, (TE(l_mut), NT(IDENTIFIER), NT(DEFINE_TEMPLATE_PARAM_ITEM)), pass_pattern_mut_identifier_or_takepace_with_template),
                P(DEFINE_PATTERN, (TE(l_left_brackets), NT(DEFINE_PATTERN_LIST), TE(l_right_brackets)), pass_pattern_tuple),
                P(DEFINE_PATTERN, (TE(l_left_brackets), NT(COMMA_MAY_EMPTY), TE(l_right_brackets)), pass_pattern_tuple),
                P(DEFINE_PATTERN_LIST, (NT(DEFINE_PATTERN_ITEMS), NT(COMMA_MAY_EMPTY)), pass_direct<0>),
                P(DEFINE_PATTERN_ITEMS, (NT(DEFINE_PATTERN)), pass_create_list<0>),
                P(DEFINE_PATTERN_ITEMS, (NT(DEFINE_PATTERN_ITEMS), TE(l_comma), NT(DEFINE_PATTERN)), pass_append_list<2, 0>),
                //////////////////////////////////////////////////////////////////////////////////////
                P(AST_TOKEN_IDENTIFER, (NT(IDENTIFIER)), pass_token),
                P(AST_TOKEN_NIL, (TE(l_nil)), pass_token),
                P(IDENTIFIER, (TE(l_identifier)), pass_direct<0>),
                P(USELESS_TOKEN, (TE(l_double_index_point)), pass_useless_token),
                P(USELESS_TOKEN, (TE(l_unknown_token)), pass_useless_token),
                P(USELESS_TOKEN, (TE(l_macro)), pass_token),
            });

#undef B
#undef SL
#undef TE
#undef NT
#undef P

            wo_stdout << ANSI_HIY "WooGramma: " ANSI_RST "Checking LR(1) table..." << wo_endl;

            if (grammar_instance->check_lr1())
            {
                wo_stdout << ANSI_HIR "WooGramma: " ANSI_RST "LR(1) have some problem, abort." << wo_endl;
                exit(-1);
            }

            // grammar_instance->display();

            if (!WO_GRAMMAR_SKIP_GEN_LR1_TABLE_CACHE)
            {
                using namespace std;
                const char *tab = "    ";

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
                    for (auto &[op_nt, sym_list] : grammar_instance->ORGIN_P)
                    {
                        if (sym_list.size() > max_length_producer)
                            max_length_producer = sym_list.size();

                        if (nonte_list.find(op_nt.nt_name) == nonte_list.end())
                            nonte_list[op_nt.nt_name] = ++nt_count;

                        for (auto &sym : sym_list)
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
                    for (auto &[op_nt, nt_index] : nonte_list)
                    {
                        wo_test(_real_nonte_list.find(nt_index) == _real_nonte_list.end());
                        _real_nonte_list.insert(std::make_pair(nt_index, op_nt));
                    }
                    std::map<int, lex_type> _real_te_list;
                    for (auto &[op_te, te_index] : te_list)
                    {
                        wo_test(_real_te_list.find(te_index) == _real_te_list.end());
                        _real_te_list.insert_or_assign(te_index, op_te);
                    }

                    cachefile << "const char* woolang_id_nonterm_list[" << nt_count << "+ 1] = {" << endl;
                    cachefile << tab << "nullptr," << endl;

                    for (auto &[nt_index, op_nt] : _real_nonte_list)
                        cachefile << tab << "\"" << op_nt << "\"," << endl;

                    cachefile << "};" << endl;

                    cachefile << "const lex_type woolang_id_term_list[" << te_count << "+ 1] = {" << endl;
                    cachefile << tab << "lex_type::l_error," << endl;

                    for (auto &[te_index, op_te] : _real_te_list)
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

                // GOTO : only nt have goto,
                cachefile << "const int woolang_lr1_act_goto[][" << nonte_list.size() << " + 1] = {" << endl;
                cachefile << "// {   STATE_ID,  NT_ID1, NT_ID2, ...  }" << endl;
                size_t WO_LR1_ACT_GOTO_SIZE = 0;
                size_t WO_GENERATE_INDEX = 0;
                for (auto &[state_id, te_sym_list] : grammar_instance->LR1_TABLE)
                {
                    std::vector<int> nt_goto_state(nonte_list.size() + 1, -1);
                    bool has_action = false;
                    for (auto &[sym, actions] : te_sym_list)
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
                for (auto &[state_id, te_sym_list] : grammar_instance->LR1_TABLE)
                {
                    std::vector<int> te_stack_reduce_state(te_list.size() + 1, 0);
                    bool has_action = false;
                    for (auto &[sym, actions] : te_sym_list)
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
                for (auto &[gotoidx, rsidx] : STATE_GOTO_R_S_INDEX_MAP)
                {
                    cachefile << "    { " << gotoidx << ", " << rsidx << " }," << endl;
                }
                cachefile << "};" << endl;
                ///////////////////////////////////////////////////////////////////////
                // Generate FOLLOW
                cachefile << "const int woolang_follow_sets[][" << te_list.size() << " + 1] = {" << endl;
                cachefile << "// {   NONTERM_ID,  TE_ID1, TE_ID2, ...  }" << endl;
                for (auto &[follow_item_sym, follow_items] : grammar_instance->FOLLOW_SET)
                {
                    wo_test(std::holds_alternative<grammar::nt>(follow_item_sym));

                    std::vector<int> follow_set(te_list.size() + 1, 0);
                    cachefile << "{ " << nonte_list[std::get<grammar::nt>(follow_item_sym).nt_name] << ",  ";
                    for (auto &tes : follow_items)
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
                for (auto &[aim, rule] : grammar_instance->ORGIN_P)
                {
                    if (aim.builder_index == 0)
                    {
                        wo_stdout
                            << ANSI_HIY "WooGramma: " ANSI_RST "Producer: " ANSI_HIR
                            << grammar::lr_item{grammar::rule{aim, rule}, size_t(-1), grammar::te(grammar::ttype::l_eof)}
                            << ANSI_RST " have no ast builder, using default builder.."
                            << wo_endl;
                    }

                    cachefile << "   { " << nonte_list[aim.nt_name] << ", " << aim.builder_index << ", " << rule.size() << ", ";
                    for (auto &sym : rule)
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
                wo_stdout << ANSI_HIG "WooGramma: " ANSI_RST "Skip generating LR(1) table cache (WO_GRAMMAR_SKIP_GEN_LR1_TABLE_CACHE is true)." << wo_endl;
            }

#if defined(WO_LANG_GRAMMAR_LR1_AUTO_GENED) && !WO_GRAMMAR_SKIP_GEN_LR1_TABLE_CACHE
        }
#undef WO_LANG_GRAMMAR_LR1_AUTO_GENED
#endif

        // DESCRIBE HOW TO GENERATE AST HERE:
        goto register_ast_builder_function; // used for hiding warning..
    register_ast_builder_function:
        // finally work

        for (auto &[rule_nt, _tokens] : grammar_instance->ORGIN_P)
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
    grammar *get_grammar_instance()
    {
        wo_assert(grammar_instance != nullptr);

        return grammar_instance.get();
    }
#endif
}
