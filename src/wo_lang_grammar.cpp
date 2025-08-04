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

            using nt = wo::grammar::nt;
            using te = wo::grammar::te;
            using symlist = wo::grammar::symlist;
            using token = wo::grammar::ttype;

            grammar_instance = std::make_unique<grammar>(std::vector<grammar::rule>{
                // nt >> list{nt/te ... } [>> ast_create_function]
                nt("PROGRAM_AUGMENTED") >> symlist{nt("PROGRAM")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("PROGRAM") >> symlist{te(token::l_empty)} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("PROGRAM") >> symlist{nt("USELESS_TOKEN")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("PROGRAM") >> symlist{nt("PARAGRAPH")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("PARAGRAPH") >> symlist{nt("SENTENCE_LIST")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("SENTENCE_LIST") >> symlist{nt("SENTENCE_LIST"), nt("SENTENCE")} >> WO_ASTBUILDER_INDEX(ast::pass_append_list<1, 0>),
                nt("SENTENCE_LIST") >> symlist{nt("SENTENCE")} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),
                // NOTE: macro might defined after import sentence. to make sure macro can be handle correctly.
                //      we make sure import happend before macro been peek and check.
                nt("SENTENCE") >> symlist{ te(token::l_defer), nt("BLOCKED_SENTENCE") } >> WO_ASTBUILDER_INDEX(ast::pass_defer),
                nt("SENTENCE") >> symlist{nt("IMPORT_SENTENCE"), te(token::l_semicolon)} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("IMPORT_SENTENCE") >> symlist{te(token::l_import), nt("SCOPED_LIST_NORMAL")} >> WO_ASTBUILDER_INDEX(ast::pass_import_files),
                nt("IMPORT_SENTENCE") >> symlist{te(token::l_export), te(token::l_import), nt("SCOPED_LIST_NORMAL")} >> WO_ASTBUILDER_INDEX(ast::pass_import_files),
                nt("SENTENCE") >> symlist{nt("DECL_ATTRIBUTE"), // useless
                                          te(token::l_using), nt("SCOPED_LIST_NORMAL"), te(token::l_semicolon)} >>
                    WO_ASTBUILDER_INDEX(ast::pass_using_namespace),
                nt("DECLARE_NEW_TYPE") >> symlist{nt("DECL_ATTRIBUTE"), te(token::l_using), nt("IDENTIFIER"), nt("DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY"), te(token::l_assign), nt("TYPE")} >> WO_ASTBUILDER_INDEX(ast::pass_using_type_as),
                nt("SENTENCE") >> symlist{nt("DECLARE_NEW_TYPE"), nt("SENTENCE_BLOCK_MAY_SEMICOLON")} >> WO_ASTBUILDER_INDEX(ast::pass_using_typename_space),
                nt("SENTENCE_BLOCK_MAY_SEMICOLON") >> symlist{te(token::l_semicolon)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                nt("SENTENCE_BLOCK_MAY_SEMICOLON") >> symlist{nt("SENTENCE_BLOCK")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("SENTENCE") >> symlist{nt("DECL_ATTRIBUTE"), te(token::l_alias), nt("IDENTIFIER"), nt("DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY"), te(token::l_assign), nt("TYPE"), te(token::l_semicolon)} >> WO_ASTBUILDER_INDEX(ast::pass_alias_type_as),
                //////////////////////////////////////////////////////////////////////////////////////
                nt("SENTENCE") >> symlist{nt("DECL_ENUM")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("DECL_ENUM") >> symlist{nt("DECL_ATTRIBUTE"), te(token::l_enum), nt("AST_TOKEN_IDENTIFER"), te(token::l_left_curly_braces), nt("ENUM_ITEMS"), te(token::l_right_curly_braces)} >> WO_ASTBUILDER_INDEX(ast::pass_enum_finalize),
                nt("ENUM_ITEMS") >> symlist{nt("ENUM_ITEM_LIST"), nt("COMMA_MAY_EMPTY")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("ENUM_ITEM_LIST") >> symlist{nt("ENUM_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),
                nt("ENUM_ITEM_LIST") >> symlist{nt("ENUM_ITEM_LIST"), te(token::l_comma), nt("ENUM_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_append_list<2, 0>),
                nt("ENUM_ITEM") >> symlist{nt("IDENTIFIER")} >> WO_ASTBUILDER_INDEX(ast::pass_enum_item_create),
                nt("ENUM_ITEM") >> symlist{nt("IDENTIFIER"), te(token::l_assign), nt("EXPRESSION")} >> WO_ASTBUILDER_INDEX(ast::pass_enum_item_create),
                nt("SENTENCE") >> symlist{nt("DECL_NAMESPACE")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("DECL_NAMESPACE") >> symlist{te(token::l_namespace), nt("SPACE_NAME_LIST"), nt("SENTENCE_BLOCK")} >> WO_ASTBUILDER_INDEX(ast::pass_namespace),
                nt("SPACE_NAME_LIST") >> symlist{nt("SPACE_NAME")} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),
                nt("SPACE_NAME_LIST") >> symlist{nt("SPACE_NAME_LIST"), te(token::l_scopeing), nt("SPACE_NAME")} >> WO_ASTBUILDER_INDEX(ast::pass_append_list<2, 0>),
                nt("SPACE_NAME") >> symlist{nt("IDENTIFIER")} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                nt("BLOCKED_SENTENCE") >> symlist{nt("SENTENCE")} >> WO_ASTBUILDER_INDEX(ast::pass_sentence_block<0>),
                nt("SENTENCE") >> symlist{nt("SENTENCE_BLOCK")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("SENTENCE_BLOCK") >> symlist{te(token::l_left_curly_braces), nt("PARAGRAPH"), te(token::l_right_curly_braces)} >> WO_ASTBUILDER_INDEX(ast::pass_sentence_block<1>),
                // Because of CONSTANT MAP => ,,, => { l_empty } Following production will cause R-R Conflict
                // nt("PARAGRAPH") >> symlist{te(token::l_empty)},
                // So, just make a production like this:
                nt("SENTENCE_BLOCK") >> symlist{te(token::l_left_curly_braces), te(token::l_right_curly_braces)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                // NOTE: Why the production can solve the conflict?
                //       A > {}
                //       B > {l_empty}
                //       In fact, A B have same production, but in wo_lr(1) parser, l_empty have a lower priority then production like A
                //       This rule can solve many grammar conflict easily, but in some case, it will cause bug, so please use it carefully.
                nt("DECL_ATTRIBUTE") >> symlist{nt("DECL_ATTRIBUTE_ITEMS")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("DECL_ATTRIBUTE") >> symlist{te(token::l_empty)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                nt("DECL_ATTRIBUTE_ITEMS") >> symlist{nt("DECL_ATTRIBUTE_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_attribute),
                nt("DECL_ATTRIBUTE_ITEMS") >> symlist{nt("DECL_ATTRIBUTE_ITEMS"), nt("DECL_ATTRIBUTE_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_attribute_append),
                nt("DECL_ATTRIBUTE_ITEM") >> symlist{nt("LIFECYCLE_MODIFER")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("DECL_ATTRIBUTE_ITEM") >> symlist{nt("EXTERNAL_MODIFER")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("DECL_ATTRIBUTE_ITEM") >> symlist{nt("ACCESS_MODIFIER")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("LIFECYCLE_MODIFER") >> symlist{te(token::l_static)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                nt("EXTERNAL_MODIFER") >> symlist{te(token::l_extern)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                nt("ACCESS_MODIFIER") >> symlist{te(token::l_public)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                nt("ACCESS_MODIFIER") >> symlist{te(token::l_private)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                nt("ACCESS_MODIFIER") >> symlist{te(token::l_protected)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                nt("SENTENCE") >> symlist{te(token::l_semicolon)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                nt("WHERE_CONSTRAINT_WITH_SEMICOLON") >> symlist{nt("WHERE_CONSTRAINT"), te(token::l_semicolon)} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("WHERE_CONSTRAINT_WITH_SEMICOLON") >> symlist{te(token::l_empty)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                nt("WHERE_CONSTRAINT_MAY_EMPTY") >> symlist{nt("WHERE_CONSTRAINT")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("WHERE_CONSTRAINT_MAY_EMPTY") >> symlist{te(token::l_empty)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                nt("WHERE_CONSTRAINT") >> symlist{te(token::l_where), nt("CONSTRAINT_LIST"), nt("COMMA_MAY_EMPTY")} >> WO_ASTBUILDER_INDEX(ast::pass_build_where_constraint),
                nt("CONSTRAINT_LIST") >> symlist{nt("EXPRESSION")} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),
                nt("CONSTRAINT_LIST") >> symlist{nt("CONSTRAINT_LIST"), te(token::l_comma), nt("EXPRESSION")} >> WO_ASTBUILDER_INDEX(ast::pass_append_list<2, 0>),
                nt("SENTENCE") >> symlist{nt("FUNC_DEFINE_WITH_NAME")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("FUNC_DEFINE_WITH_NAME") >> symlist{nt("DECL_ATTRIBUTE"), te(token::l_func), nt("AST_TOKEN_IDENTIFER"), nt("DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY"), te(token::l_left_brackets), nt("ARGDEFINE"), te(token::l_right_brackets), nt("RETURN_TYPE_DECLEAR_MAY_EMPTY"), nt("WHERE_CONSTRAINT_WITH_SEMICOLON"), nt("SENTENCE_BLOCK")} >> WO_ASTBUILDER_INDEX(ast::pass_func_def_named),
                nt("FUNC_DEFINE_WITH_NAME") >> symlist{nt("DECL_ATTRIBUTE"), te(token::l_func), te(token::l_operator), nt("OVERLOADINGABLE_OPERATOR"), nt("DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY"), te(token::l_left_brackets), nt("ARGDEFINE"), te(token::l_right_brackets), nt("RETURN_TYPE_DECLEAR_MAY_EMPTY"), nt("WHERE_CONSTRAINT_WITH_SEMICOLON"), nt("SENTENCE_BLOCK")} >> WO_ASTBUILDER_INDEX(ast::pass_func_def_oper),
                nt("OVERLOADINGABLE_OPERATOR") >> symlist{te(token::l_add)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                nt("OVERLOADINGABLE_OPERATOR") >> symlist{te(token::l_sub)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                nt("OVERLOADINGABLE_OPERATOR") >> symlist{te(token::l_mul)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                nt("OVERLOADINGABLE_OPERATOR") >> symlist{te(token::l_div)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                nt("OVERLOADINGABLE_OPERATOR") >> symlist{te(token::l_mod)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                nt("OVERLOADINGABLE_OPERATOR") >> symlist{te(token::l_less)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                nt("OVERLOADINGABLE_OPERATOR") >> symlist{te(token::l_larg)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                nt("OVERLOADINGABLE_OPERATOR") >> symlist{te(token::l_less_or_equal)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                nt("OVERLOADINGABLE_OPERATOR") >> symlist{te(token::l_larg_or_equal)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                nt("OVERLOADINGABLE_OPERATOR") >> symlist{te(token::l_equal)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                nt("OVERLOADINGABLE_OPERATOR") >> symlist{te(token::l_not_equal)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                nt("OVERLOADINGABLE_OPERATOR") >> symlist{te(token::l_land)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                nt("OVERLOADINGABLE_OPERATOR") >> symlist{te(token::l_lor)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                nt("OVERLOADINGABLE_OPERATOR") >> symlist{te(token::l_lnot)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                nt("EXTERN_FROM") >> symlist{te(token::l_extern), te(token::l_left_brackets), te(token::l_literal_string), te(token::l_comma), te(token::l_literal_string), nt("EXTERN_ATTRIBUTES"), te(token::l_right_brackets)} >> WO_ASTBUILDER_INDEX(ast::pass_extern),
                nt("EXTERN_FROM") >> symlist{te(token::l_extern), te(token::l_left_brackets), te(token::l_literal_string), nt("EXTERN_ATTRIBUTES"), te(token::l_right_brackets)} >> WO_ASTBUILDER_INDEX(ast::pass_extern),
                nt("EXTERN_ATTRIBUTES") >> symlist{te(token::l_empty)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                nt("EXTERN_ATTRIBUTES") >> symlist{te(token::l_comma), nt("EXTERN_ATTRIBUTE_LIST")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<1>),
                nt("EXTERN_ATTRIBUTE_LIST") >> symlist{nt("EXTERN_ATTRIBUTE")} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),
                nt("EXTERN_ATTRIBUTE_LIST") >> symlist{nt("EXTERN_ATTRIBUTE_LIST"), te(token::l_comma), nt("EXTERN_ATTRIBUTE")} >> WO_ASTBUILDER_INDEX(ast::pass_append_list<2, 0>),
                nt("EXTERN_ATTRIBUTE") >> symlist{te(token::l_identifier)} >>  WO_ASTBUILDER_INDEX(ast::pass_token),
                nt("FUNC_DEFINE_WITH_NAME") >> symlist{nt("EXTERN_FROM"), nt("DECL_ATTRIBUTE"), te(token::l_func), nt("AST_TOKEN_IDENTIFER"), nt("DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY"), te(token::l_left_brackets), nt("ARGDEFINE"), te(token::l_right_brackets), nt("RETURN_TYPE_DECLEAR"), nt("WHERE_CONSTRAINT_MAY_EMPTY"), te(token::l_semicolon)} >> WO_ASTBUILDER_INDEX(ast::pass_func_def_extn),
                nt("FUNC_DEFINE_WITH_NAME") >> symlist{nt("EXTERN_FROM"), nt("DECL_ATTRIBUTE"), te(token::l_func), te(token::l_operator), nt("OVERLOADINGABLE_OPERATOR"), nt("DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY"), te(token::l_left_brackets), nt("ARGDEFINE"), te(token::l_right_brackets), nt("RETURN_TYPE_DECLEAR"), nt("WHERE_CONSTRAINT_MAY_EMPTY"), te(token::l_semicolon)} >> WO_ASTBUILDER_INDEX(ast::pass_func_def_extn_oper),
                nt("SENTENCE") >> symlist{nt("MAY_LABELED_LOOP")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("MAY_LABELED_LOOP") >> symlist{ nt("IDENTIFIER"), te(token::l_at), nt("LOOP") } >> WO_ASTBUILDER_INDEX(ast::pass_mark_label),
                nt("MAY_LABELED_LOOP") >> symlist{ nt("LOOP") } >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("LOOP") >> symlist{ nt("WHILE") } >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("LOOP") >> symlist{ nt("FORLOOP") } >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("LOOP") >> symlist{ nt("FOREACH") } >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("WHILE") >> symlist{te(token::l_while), te(token::l_left_brackets), nt("EXPRESSION"), te(token::l_right_brackets), nt("BLOCKED_SENTENCE")} >> WO_ASTBUILDER_INDEX(ast::pass_while),
                nt("VAR_DEFINE_WITH_SEMICOLON") >> symlist{nt("VAR_DEFINE_LET_SENTENCE"), te(token::l_semicolon)} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("FORLOOP") >> symlist{te(token::l_for), te(token::l_left_brackets), nt("VAR_DEFINE_WITH_SEMICOLON"), nt("MAY_EMPTY_EXPRESSION"), te(token::l_semicolon), nt("MAY_EMPTY_EXPRESSION"), te(token::l_right_brackets), nt("BLOCKED_SENTENCE")} >> WO_ASTBUILDER_INDEX(ast::pass_for_defined),
                nt("FORLOOP") >> symlist{te(token::l_for), te(token::l_left_brackets), nt("MAY_EMPTY_EXPRESSION"), te(token::l_semicolon), nt("MAY_EMPTY_EXPRESSION"), te(token::l_semicolon), nt("MAY_EMPTY_EXPRESSION"), te(token::l_right_brackets), nt("BLOCKED_SENTENCE")} >> WO_ASTBUILDER_INDEX(ast::pass_for_expr),
                nt("SENTENCE") >> symlist{te(token::l_break), te(token::l_semicolon)} >> WO_ASTBUILDER_INDEX(ast::pass_break),
                nt("SENTENCE") >> symlist{te(token::l_continue), te(token::l_semicolon)} >> WO_ASTBUILDER_INDEX(ast::pass_continue),
                nt("SENTENCE") >> symlist{te(token::l_break), nt("IDENTIFIER"), te(token::l_semicolon)} >> WO_ASTBUILDER_INDEX(ast::pass_break_label),
                nt("SENTENCE") >> symlist{te(token::l_continue), nt("IDENTIFIER"), te(token::l_semicolon)} >> WO_ASTBUILDER_INDEX(ast::pass_continue_label),
                nt("MAY_EMPTY_EXPRESSION") >> symlist{nt("EXPRESSION")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("MAY_EMPTY_EXPRESSION") >> symlist{te(token::l_empty)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                nt("FOREACH") >> symlist{te(token::l_for), te(token::l_left_brackets), nt("DECL_ATTRIBUTE"), te(token::l_let), nt("DEFINE_PATTERN"), te(token::l_typecast), nt("EXPRESSION"), te(token::l_right_brackets), nt("BLOCKED_SENTENCE")} >> WO_ASTBUILDER_INDEX(ast::pass_foreach),
                nt("SENTENCE") >> symlist{nt("IF")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("IF") >> symlist{te(token::l_if), te(token::l_left_brackets), nt("EXPRESSION"), te(token::l_right_brackets), nt("BLOCKED_SENTENCE"), nt("ELSE")} >> WO_ASTBUILDER_INDEX(ast::pass_if),
                nt("ELSE") >> symlist{te(token::l_empty)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                nt("ELSE") >> symlist{te(token::l_else), nt("BLOCKED_SENTENCE")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<1>),
                nt("SENTENCE") >> symlist{nt("VAR_DEFINE_LET_SENTENCE"), te(token::l_semicolon)} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),                                // ASTVariableDefination
                nt("VAR_DEFINE_LET_SENTENCE") >> symlist{nt("DECL_ATTRIBUTE"), te(token::l_let), nt("VARDEFINES")} >> WO_ASTBUILDER_INDEX(ast::pass_variable_defines),       // ASTVariableDefination
                nt("VARDEFINES") >> symlist{nt("VARDEFINE")} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),                                                               // ASTVariableDefination
                nt("VARDEFINES") >> symlist{nt("VARDEFINES"), te(token::l_comma), nt("VARDEFINE")} >> WO_ASTBUILDER_INDEX(ast::pass_append_list<2, 0>),                      // ASTVariableDefination
                nt("VARDEFINE") >> symlist{nt("DEFINE_PATTERN_MAY_TEMPLATE"), te(token::l_assign), nt("EXPRESSION")} >> WO_ASTBUILDER_INDEX(ast::pass_variable_define_item), // ASTVariableDefination
                nt("DEFINE_PATTERN_MAY_TEMPLATE") >> symlist{nt("DEFINE_PATTERN")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("DEFINE_PATTERN_MAY_TEMPLATE") >> symlist{nt("DEFINE_PATTERN_WITH_TEMPLATE")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("SENTENCE") >> symlist{te(token::l_return), nt("RIGHT"), te(token::l_semicolon)} >> WO_ASTBUILDER_INDEX(ast::pass_return_value),
                nt("SENTENCE") >> symlist{te(token::l_return), te(token::l_semicolon)} >> WO_ASTBUILDER_INDEX(ast::pass_return),
                nt("SENTENCE") >> symlist{nt("EXPRESSION"), te(token::l_semicolon)} >> WO_ASTBUILDER_INDEX(ast::pass_direct_keep_source_location<0>),
                nt("EXPRESSION") >> symlist{nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("RIGHT") >> symlist{te(token::l_do), nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_do_void_cast),
                nt("RIGHT") >> symlist{te(token::l_mut), nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_mark_mut),
                nt("RIGHT") >> symlist{te(token::l_immut), nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_mark_immut),
                nt("RIGHT") >> symlist{nt("ASSIGNMENT")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("RIGHT") >> symlist{nt("LOGICAL_OR")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("ASSIGNMENT") >> symlist{nt("ASSIGNED_PATTERN"), te(token::l_assign), nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_assign_operation),
                nt("ASSIGNMENT") >> symlist{nt("ASSIGNED_PATTERN"), te(token::l_add_assign), nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_assign_operation),
                nt("ASSIGNMENT") >> symlist{nt("ASSIGNED_PATTERN"), te(token::l_sub_assign), nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_assign_operation),
                nt("ASSIGNMENT") >> symlist{nt("ASSIGNED_PATTERN"), te(token::l_mul_assign), nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_assign_operation),
                nt("ASSIGNMENT") >> symlist{nt("ASSIGNED_PATTERN"), te(token::l_div_assign), nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_assign_operation),
                nt("ASSIGNMENT") >> symlist{nt("ASSIGNED_PATTERN"), te(token::l_mod_assign), nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_assign_operation),
                nt("ASSIGNMENT") >> symlist{nt("ASSIGNED_PATTERN"), te(token::l_value_assign), nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_assign_operation),
                nt("ASSIGNMENT") >> symlist{nt("ASSIGNED_PATTERN"), te(token::l_value_add_assign), nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_assign_operation),
                nt("ASSIGNMENT") >> symlist{nt("ASSIGNED_PATTERN"), te(token::l_value_sub_assign), nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_assign_operation),
                nt("ASSIGNMENT") >> symlist{nt("ASSIGNED_PATTERN"), te(token::l_value_mul_assign), nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_assign_operation),
                nt("ASSIGNMENT") >> symlist{nt("ASSIGNED_PATTERN"), te(token::l_value_div_assign), nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_assign_operation),
                nt("ASSIGNMENT") >> symlist{nt("ASSIGNED_PATTERN"), te(token::l_value_mod_assign), nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_assign_operation),
                nt("ASSIGNED_PATTERN") >> symlist{nt("LEFT")} >> WO_ASTBUILDER_INDEX(ast::pass_pattern_for_assign),
                nt("RIGHT") >> symlist{nt("LOGICAL_OR"), te(token::l_question), nt("RIGHT"), te(token::l_or), nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_conditional_expression),
                nt("FACTOR") >> symlist{nt("FUNC_DEFINE")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("FUNC_DEFINE") >> symlist{nt("DECL_ATTRIBUTE"), te(token::l_func), nt("DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY"), te(token::l_left_brackets), nt("ARGDEFINE"), te(token::l_right_brackets), nt("RETURN_TYPE_DECLEAR_MAY_EMPTY"), nt("WHERE_CONSTRAINT_WITH_SEMICOLON"), nt("SENTENCE_BLOCK")} >> WO_ASTBUILDER_INDEX(ast::pass_func_lambda_ml),
                nt("FUNC_DEFINE") >> symlist{te(token::l_lambda), nt("DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY"), nt("ARGDEFINE"), te(token::l_assign), nt("RETURN_EXPR_BLOCK_IN_LAMBDA"), nt("WHERE_DECL_FOR_LAMBDA"), te(token::l_semicolon)} >> WO_ASTBUILDER_INDEX(ast::pass_func_lambda),
                // May empty
                nt("WHERE_DECL_FOR_LAMBDA") >> symlist{te(token::l_empty)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                nt("WHERE_DECL_FOR_LAMBDA") >> symlist{te(token::l_where), nt("VARDEFINES")} >> WO_ASTBUILDER_INDEX(ast::pass_reverse_vardef),
                nt("RETURN_EXPR_BLOCK_IN_LAMBDA") >> symlist{nt("RETURN_EXPR_IN_LAMBDA")} >> WO_ASTBUILDER_INDEX(ast::pass_sentence_block<0>),
                nt("RETURN_EXPR_IN_LAMBDA") >> symlist{nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_return_lambda),
                nt("RETURN_TYPE_DECLEAR_MAY_EMPTY") >> symlist{te(token::l_empty)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                nt("RETURN_TYPE_DECLEAR_MAY_EMPTY") >> symlist{nt("RETURN_TYPE_DECLEAR")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("RETURN_TYPE_DECLEAR") >> symlist{te(token::l_function_result), nt("TYPE")} >> WO_ASTBUILDER_INDEX(ast::pass_direct_keep_source_location<1>),
                nt("TYPE_DECLEAR") >> symlist{te(token::l_typecast), nt("TYPE")} >> WO_ASTBUILDER_INDEX(ast::pass_direct_keep_source_location<1>),
                /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
                nt("TYPE") >> symlist{nt("ORIGIN_TYPE")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("TYPE") >> symlist{te(token::l_mut), nt("ORIGIN_TYPE")} >> WO_ASTBUILDER_INDEX(ast::pass_type_mutable),
                nt("TYPE") >> symlist{te(token::l_immut), nt("ORIGIN_TYPE")} >> WO_ASTBUILDER_INDEX(ast::pass_type_immutable),

                nt("ORIGIN_TYPE") >> symlist{nt("AST_TOKEN_NIL")} >> WO_ASTBUILDER_INDEX(ast::pass_type_nil),
                nt("ORIGIN_TYPE") >> symlist{
                                         nt("TUPLE_TYPE_LIST"),
                                         te(token::l_function_result),
                                         nt("TYPE"),
                                     } >>
                    WO_ASTBUILDER_INDEX(ast::pass_type_func),

                nt("ORIGIN_TYPE") >> symlist{
                                         te(token::l_struct),
                                         te(token::l_left_curly_braces),
                                         nt("STRUCT_MEMBER_DEFINES"),
                                         te(token::l_right_curly_braces),
                                     } >>
                    WO_ASTBUILDER_INDEX(ast::pass_type_struct),
                nt("STRUCT_MEMBER_DEFINES") >> symlist{te(token::l_empty)} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),
                nt("STRUCT_MEMBER_DEFINES") >> symlist{nt("STRUCT_MEMBERS_LIST"), nt("COMMA_MAY_EMPTY")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("STRUCT_MEMBERS_LIST") >> symlist{nt("STRUCT_MEMBER_PAIR")} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),
                nt("STRUCT_MEMBERS_LIST") >> symlist{nt("STRUCT_MEMBERS_LIST"), te(token::l_comma), nt("STRUCT_MEMBER_PAIR")} >> WO_ASTBUILDER_INDEX(ast::pass_append_list<2, 0>),
                nt("STRUCT_MEMBER_PAIR") >> symlist{nt("ACCESS_MODIFIER_MAY_EMPTY"), nt("IDENTIFIER"), nt("TYPE_DECLEAR")} >> WO_ASTBUILDER_INDEX(ast::pass_type_struct_field),
                nt("ACCESS_MODIFIER_MAY_EMPTY") >> symlist{
                                                       nt("ACCESS_MODIFIER"),
                                                   } >>
                    WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("ACCESS_MODIFIER_MAY_EMPTY") >> symlist{
                                                       te(token::l_empty),
                                                   } >>
                    WO_ASTBUILDER_INDEX(ast::pass_empty),

                nt("ORIGIN_TYPE") >> symlist{nt("STRUCTABLE_TYPE")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("STRUCTABLE_TYPE") >> symlist{
                                             nt("SCOPED_IDENTIFIER_FOR_TYPE"),
                                         } >>
                    WO_ASTBUILDER_INDEX(ast::pass_type_from_identifier),
                nt("STRUCTABLE_TYPE") >> symlist{
                                             nt("TYPEOF"),
                                         } >>
                    WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("STRUCTABLE_TYPE_FOR_CONSTRUCT") >> symlist{
                                                           nt("SCOPED_IDENTIFIER_FOR_VAL"),
                                                       } >>
                    WO_ASTBUILDER_INDEX(ast::pass_type_from_identifier),
                nt("STRUCTABLE_TYPE_FOR_CONSTRUCT") >> symlist{
                                                           nt("TYPEOF"),
                                                       } >>
                    WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("TYPEOF") >> symlist{te(token::l_typeof), te(token::l_left_brackets), nt("RIGHT"), te(token::l_right_brackets)} >> WO_ASTBUILDER_INDEX(ast::pass_typeof),
                nt("TYPEOF") >> symlist{te(token::l_typeof), te(token::l_template_using_begin), nt("TYPE"), te(token::l_larg)} >> WO_ASTBUILDER_INDEX(ast::pass_direct<2>),
                nt("TYPEOF") >> symlist{te(token::l_typeof), te(token::l_less), nt("TYPE"), te(token::l_larg)} >> WO_ASTBUILDER_INDEX(ast::pass_direct<2>),
                nt("ORIGIN_TYPE") >> symlist{nt("TUPLE_TYPE_LIST")} >> WO_ASTBUILDER_INDEX(ast::pass_type_tuple),
                nt("TUPLE_TYPE_LIST") >> symlist{te(token::l_left_brackets), nt("TUPLE_TYPE_LIST_ITEMS_MAY_EMPTY"), te(token::l_right_brackets)} >> WO_ASTBUILDER_INDEX(ast::pass_direct<1>),
                nt("TUPLE_TYPE_LIST_ITEMS_MAY_EMPTY") >> symlist{nt("TUPLE_TYPE_LIST_ITEMS"), nt("COMMA_MAY_EMPTY")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("TUPLE_TYPE_LIST_ITEMS_MAY_EMPTY") >> symlist{nt("VARIADIC_MAY_EMPTY")} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),
                nt("TUPLE_TYPE_LIST_ITEMS") >> symlist{nt("TYPE")} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),
                nt("TUPLE_TYPE_LIST_ITEMS") >> symlist{nt("TUPLE_TYPE_LIST_ITEMS"), te(token::l_comma), nt("TYPE")} >> WO_ASTBUILDER_INDEX(ast::pass_append_list<2, 0>),
                nt("TUPLE_TYPE_LIST_ITEMS") >> symlist{nt("TUPLE_TYPE_LIST_ITEMS"), te(token::l_comma), nt("VARIADIC_MAY_EMPTY")} >> WO_ASTBUILDER_INDEX(ast::pass_append_list<2, 0>),
                nt("VARIADIC_MAY_EMPTY") >> symlist{te(token::l_variadic_sign)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                nt("VARIADIC_MAY_EMPTY") >> symlist{te(token::l_empty)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                //////////////////////////////////////////////////////////////////////////////////////////////
                nt("ARGDEFINE") >> symlist{te(token::l_empty)} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),
                nt("ARGDEFINE") >> symlist{nt("ARGDEFINE_VAR_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),
                nt("ARGDEFINE") >> symlist{nt("ARGDEFINE"), te(token::l_comma), nt("ARGDEFINE_VAR_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_append_list<2, 0>),
                nt("ARGDEFINE_VAR_ITEM") >> symlist{nt("DEFINE_PATTERN"), nt("TYPE_DECL_MAY_EMPTY")} >> WO_ASTBUILDER_INDEX(ast::pass_func_argument),
                nt("ARGDEFINE_VAR_ITEM") >> symlist{te(token::l_variadic_sign)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                nt("TYPE_DECL_MAY_EMPTY") >> symlist{nt("TYPE_DECLEAR")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("TYPE_DECL_MAY_EMPTY") >> symlist{te(token::l_empty)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                nt("LOGICAL_OR") >> symlist{nt("LOGICAL_AND")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("LOGICAL_OR") >> symlist{nt("LOGICAL_OR"), te(token::l_lor), nt("LOGICAL_AND")} >> WO_ASTBUILDER_INDEX(ast::pass_binary_operation),
                nt("LOGICAL_AND") >> symlist{nt("EQUATION")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("LOGICAL_AND") >> symlist{nt("LOGICAL_AND"), te(token::l_land), nt("EQUATION")} >> WO_ASTBUILDER_INDEX(ast::pass_binary_operation),
                nt("EQUATION") >> symlist{nt("RELATION")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("EQUATION") >> symlist{nt("EQUATION"), te(token::l_equal), nt("RELATION")} >> WO_ASTBUILDER_INDEX(ast::pass_binary_operation),
                nt("EQUATION") >> symlist{nt("EQUATION"), te(token::l_not_equal), nt("RELATION")} >> WO_ASTBUILDER_INDEX(ast::pass_binary_operation),
                nt("RELATION") >> symlist{nt("SUMMATION")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("RELATION") >> symlist{nt("RELATION"), te(token::l_larg), nt("SUMMATION")} >> WO_ASTBUILDER_INDEX(ast::pass_binary_operation),
                nt("RELATION") >> symlist{nt("RELATION"), te(token::l_less), nt("SUMMATION")} >> WO_ASTBUILDER_INDEX(ast::pass_binary_operation),
                nt("RELATION") >> symlist{nt("RELATION"), te(token::l_less_or_equal), nt("SUMMATION")} >> WO_ASTBUILDER_INDEX(ast::pass_binary_operation),
                nt("RELATION") >> symlist{nt("RELATION"), te(token::l_larg_or_equal), nt("SUMMATION")} >> WO_ASTBUILDER_INDEX(ast::pass_binary_operation),
                nt("SUMMATION") >> symlist{nt("MULTIPLICATION")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("SUMMATION") >> symlist{nt("SUMMATION"), te(token::l_add), nt("MULTIPLICATION")} >> WO_ASTBUILDER_INDEX(ast::pass_binary_operation),
                nt("SUMMATION") >> symlist{nt("SUMMATION"), te(token::l_sub), nt("MULTIPLICATION")} >> WO_ASTBUILDER_INDEX(ast::pass_binary_operation),
                nt("MULTIPLICATION") >> symlist{nt("SINGLE_VALUE")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("MULTIPLICATION") >> symlist{nt("MULTIPLICATION"), te(token::l_mul), nt("SINGLE_VALUE")} >> WO_ASTBUILDER_INDEX(ast::pass_binary_operation),
                nt("MULTIPLICATION") >> symlist{nt("MULTIPLICATION"), te(token::l_div), nt("SINGLE_VALUE")} >> WO_ASTBUILDER_INDEX(ast::pass_binary_operation),
                nt("MULTIPLICATION") >> symlist{nt("MULTIPLICATION"), te(token::l_mod), nt("SINGLE_VALUE")} >> WO_ASTBUILDER_INDEX(ast::pass_binary_operation),
                nt("SINGLE_VALUE") >> symlist{nt("UNARIED_FACTOR")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("UNARIED_FACTOR") >> symlist{nt("FACTOR_TYPE_CASTING")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("UNARIED_FACTOR") >> symlist{te(token::l_sub), nt("UNARIED_FACTOR")} >> WO_ASTBUILDER_INDEX(ast::pass_unary_operation),
                nt("UNARIED_FACTOR") >> symlist{te(token::l_lnot), nt("UNARIED_FACTOR")} >> WO_ASTBUILDER_INDEX(ast::pass_unary_operation),
                nt("UNARIED_FACTOR") >> symlist{nt("INV_FUNCTION_CALL")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("FACTOR_TYPE_CASTING") >> symlist{nt("FACTOR_TYPE_CASTING"), nt("AS_TYPE")} >> WO_ASTBUILDER_INDEX(ast::pass_check_type_as),
                nt("FACTOR_TYPE_CASTING") >> symlist{nt("FACTOR_TYPE_CASTING"), nt("IS_TYPE")} >> WO_ASTBUILDER_INDEX(ast::pass_check_type_is),
                nt("AS_TYPE") >> symlist{te(token::l_as), nt("TYPE")} >> WO_ASTBUILDER_INDEX(ast::pass_direct_keep_source_location<1>),
                nt("IS_TYPE") >> symlist{te(token::l_is), nt("TYPE")} >> WO_ASTBUILDER_INDEX(ast::pass_direct_keep_source_location<1>),

                nt("FACTOR_TYPE_CASTING") >> symlist{nt("FACTOR_TYPE_CASTING"), nt("TYPE_DECLEAR")} >> WO_ASTBUILDER_INDEX(ast::pass_cast_type),
                nt("FACTOR_TYPE_CASTING") >> symlist{nt("FACTOR")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("FACTOR") >> symlist{nt("LEFT")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("FACTOR") >> symlist{te(token::l_left_brackets), nt("RIGHT"), te(token::l_right_brackets)} >> WO_ASTBUILDER_INDEX(ast::pass_direct<1>),
                nt("FACTOR") >> symlist{nt("UNIT")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("RIGHT") >> symlist{nt("ARGUMENT_EXPAND")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("ARGUMENT_EXPAND") >> symlist{nt("FACTOR_TYPE_CASTING"), te(token::l_variadic_sign)} >> WO_ASTBUILDER_INDEX(ast::pass_expand_arguments),
                nt("UNIT") >> symlist{te(token::l_variadic_sign)} >> WO_ASTBUILDER_INDEX(ast::pass_variadic_arguments_pack),
                nt("UNIT") >> symlist{te(token::l_literal_integer)} >> WO_ASTBUILDER_INDEX(ast::pass_literal),
                nt("UNIT") >> symlist{te(token::l_literal_real)} >> WO_ASTBUILDER_INDEX(ast::pass_literal),
                nt("UNIT") >> symlist{te(token::l_literal_string)} >> WO_ASTBUILDER_INDEX(ast::pass_literal),
                nt("UNIT") >> symlist{te(token::l_literal_raw_string)} >> WO_ASTBUILDER_INDEX(ast::pass_literal),
                nt("UNIT") >> symlist{nt("LITERAL_CHAR")} >> WO_ASTBUILDER_INDEX(ast::pass_literal_char),
                nt("UNIT") >> symlist{te(token::l_literal_handle)} >> WO_ASTBUILDER_INDEX(ast::pass_literal),
                nt("UNIT") >> symlist{te(token::l_nil)} >> WO_ASTBUILDER_INDEX(ast::pass_literal),
                nt("UNIT") >> symlist{te(token::l_true)} >> WO_ASTBUILDER_INDEX(ast::pass_literal),
                nt("UNIT") >> symlist{te(token::l_false)} >> WO_ASTBUILDER_INDEX(ast::pass_literal),
                nt("UNIT") >> symlist{te(token::l_typeid), te(token::l_template_using_begin), nt("TYPE"), te(token::l_larg)} >> WO_ASTBUILDER_INDEX(ast::pass_typeid),
                nt("LITERAL_CHAR") >> symlist{te(token::l_literal_char)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                nt("UNIT") >> symlist{nt("FORMAT_STRING")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("FORMAT_STRING") >> symlist{nt("LITERAL_FORMAT_STRING_BEGIN"), nt("FORMAT_STRING_LIST"), nt("LITERAL_FORMAT_STRING_END")} >> WO_ASTBUILDER_INDEX(ast::pass_format_finish),
                nt("FORMAT_STRING_LIST") >> symlist{nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_format_cast_string),
                nt("FORMAT_STRING_LIST") >> symlist{nt("FORMAT_STRING_LIST"), nt("LITERAL_FORMAT_STRING"), nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_format_connect),
                nt("LITERAL_FORMAT_STRING_BEGIN") >> symlist{te(token::l_format_string_begin)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                nt("LITERAL_FORMAT_STRING") >> symlist{te(token::l_format_string)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                nt("LITERAL_FORMAT_STRING_END") >> symlist{te(token::l_format_string_end)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                nt("UNIT") >> symlist{nt("CONSTANT_MAP")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("UNIT") >> symlist{nt("CONSTANT_ARRAY")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("CONSTANT_ARRAY") >> symlist{te(token::l_index_begin), nt("CONSTANT_ARRAY_ITEMS"), te(token::l_index_end)} >> WO_ASTBUILDER_INDEX(ast::pass_array_instance),
                nt("CONSTANT_ARRAY") >> symlist{
                                            te(token::l_index_begin),
                                            nt("CONSTANT_ARRAY_ITEMS"),
                                            te(token::l_index_end),
                                            te(token::l_mut),
                                        } >>
                    WO_ASTBUILDER_INDEX(ast::pass_vec_instance),
                nt("CONSTANT_ARRAY_ITEMS") >> symlist{nt("COMMA_EXPR")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                //////////////////////
                nt("UNIT") >> symlist{nt("CONSTANT_TUPLE")} >> WO_ASTBUILDER_INDEX(ast::pass_tuple_instance),
                // NOTE: Avoid grammar conflict.
                nt("CONSTANT_TUPLE") >> symlist{te(token::l_left_brackets), nt("COMMA_MAY_EMPTY"), te(token::l_right_brackets)} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<1>),
                nt("CONSTANT_TUPLE") >> symlist{te(token::l_left_brackets), nt("RIGHT"), te(token::l_comma), nt("COMMA_EXPR"), te(token::l_right_brackets)} >> WO_ASTBUILDER_INDEX(ast::pass_append_list<1, 3>),
                ///////////////////////
                nt("CONSTANT_MAP") >> symlist{te(token::l_left_curly_braces), nt("CONSTANT_MAP_PAIRS"), te(token::l_right_curly_braces)} >> WO_ASTBUILDER_INDEX(ast::pass_dict_instance),
                nt("CONSTANT_MAP") >> symlist{
                                          te(token::l_left_curly_braces),
                                          nt("CONSTANT_MAP_PAIRS"),
                                          te(token::l_right_curly_braces),
                                          te(token::l_mut),
                                      } >>
                    WO_ASTBUILDER_INDEX(ast::pass_map_instance),
                nt("CONSTANT_MAP_PAIRS") >> symlist{nt("CONSTANT_MAP_PAIR")} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),
                nt("CONSTANT_MAP_PAIRS") >> symlist{nt("CONSTANT_MAP_PAIRS"), te(token::l_comma), nt("CONSTANT_MAP_PAIR")} >> WO_ASTBUILDER_INDEX(ast::pass_append_list<2, 0>),
                nt("CONSTANT_MAP_PAIR") >> symlist{te(token::l_empty)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                nt("CONSTANT_MAP_PAIR") >> symlist{nt("CONSTANT_ARRAY"), te(token::l_assign), nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_dict_field_init_pair),
                nt("CALLABLE_LEFT") >> symlist{nt("SCOPED_IDENTIFIER_FOR_VAL")} >> WO_ASTBUILDER_INDEX(ast::pass_variable),
                nt("CALLABLE_RIGHT_WITH_BRACKET") >> symlist{te(token::l_left_brackets), nt("RIGHT"), te(token::l_right_brackets)} >> WO_ASTBUILDER_INDEX(ast::pass_direct<1>),
                nt("SCOPED_IDENTIFIER_FOR_VAL") >> symlist{nt("SCOPED_LIST_TYPEOF"), nt("MAY_EMPTY_LEFT_TEMPLATE_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_build_identifier_typeof),
                nt("SCOPED_IDENTIFIER_FOR_VAL") >> symlist{nt("SCOPED_LIST_NORMAL"), nt("MAY_EMPTY_LEFT_TEMPLATE_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_build_identifier_normal),
                nt("SCOPED_IDENTIFIER_FOR_VAL") >> symlist{nt("SCOPED_LIST_GLOBAL"), nt("MAY_EMPTY_LEFT_TEMPLATE_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_build_identifier_global),
                nt("SCOPED_IDENTIFIER_FOR_TYPE") >> symlist{nt("SCOPED_LIST_TYPEOF"), nt("MAY_EMPTY_TEMPLATE_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_build_identifier_typeof),
                nt("SCOPED_IDENTIFIER_FOR_TYPE") >> symlist{nt("SCOPED_LIST_NORMAL"), nt("MAY_EMPTY_TEMPLATE_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_build_identifier_normal),
                nt("SCOPED_IDENTIFIER_FOR_TYPE") >> symlist{nt("SCOPED_LIST_GLOBAL"), nt("MAY_EMPTY_TEMPLATE_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_build_identifier_global),
                nt("SCOPED_LIST_TYPEOF") >> symlist{nt("TYPEOF"), nt("SCOPING_LIST")} >> WO_ASTBUILDER_INDEX(ast::pass_append_list<0, 1>),
                nt("SCOPED_LIST_NORMAL") >> symlist{nt("AST_TOKEN_IDENTIFER"), nt("SCOPING_LIST")} >> WO_ASTBUILDER_INDEX(ast::pass_append_list<0, 1>),
                nt("SCOPED_LIST_NORMAL") >> symlist{
                                                nt("AST_TOKEN_IDENTIFER"),
                                            } >>
                    WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),
                nt("SCOPED_LIST_GLOBAL") >> symlist{nt("SCOPING_LIST")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("SCOPING_LIST") >> symlist{te(token::l_scopeing), nt("AST_TOKEN_IDENTIFER")} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<1>), // TODO HERE SHOULD BE IDENTIF IN NAMESPACE
                nt("SCOPING_LIST") >> symlist{te(token::l_scopeing), nt("AST_TOKEN_IDENTIFER"), nt("SCOPING_LIST")} >> WO_ASTBUILDER_INDEX(ast::pass_append_list<1, 2>),
                nt("LEFT") >> symlist{nt("CALLABLE_LEFT")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("LEFT") >> symlist{nt("FACTOR_TYPE_CASTING"), te(token::l_index_begin), nt("RIGHT"), te(token::l_index_end)} >> WO_ASTBUILDER_INDEX(ast::pass_index_operation_regular),
                nt("LEFT") >> symlist{nt("FACTOR_TYPE_CASTING"), te(token::l_index_point), nt("INDEX_POINT_TARGET")} >> WO_ASTBUILDER_INDEX(ast::pass_index_operation_member),
                nt("INDEX_POINT_TARGET") >> symlist{nt("IDENTIFIER")} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                nt("INDEX_POINT_TARGET") >> symlist{te(token::l_literal_integer)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                nt("FACTOR") >> symlist{nt("FUNCTION_CALL")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("DIRECT_CALLABLE_TARGET") >> symlist{nt("CALLABLE_LEFT")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("DIRECT_CALLABLE_TARGET") >> symlist{nt("CALLABLE_RIGHT_WITH_BRACKET")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("DIRECT_CALLABLE_TARGET") >> symlist{nt("FUNC_DEFINE")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("DIRECT_CALL_FIRST_ARG") >> symlist{nt("FACTOR_TYPE_CASTING")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("DIRECT_CALL_FIRST_ARG") >> symlist{nt("ARGUMENT_EXPAND")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                // MONAD GRAMMAR~~~ SUGAR!
                nt("FACTOR") >> symlist{nt("FACTOR_TYPE_CASTING"), te(token::l_bind_monad), nt("CALLABLE_LEFT")} >> WO_ASTBUILDER_INDEX(ast::pass_build_bind_monad),
                nt("FACTOR") >> symlist{nt("FACTOR_TYPE_CASTING"), te(token::l_bind_monad), nt("CALLABLE_RIGHT_WITH_BRACKET")} >> WO_ASTBUILDER_INDEX(ast::pass_build_bind_monad),
                nt("FACTOR") >> symlist{nt("FACTOR_TYPE_CASTING"), te(token::l_bind_monad), nt("FUNC_DEFINE")} >> WO_ASTBUILDER_INDEX(ast::pass_build_bind_monad),
                nt("FACTOR") >> symlist{nt("FACTOR_TYPE_CASTING"), te(token::l_map_monad), nt("CALLABLE_LEFT")} >> WO_ASTBUILDER_INDEX(ast::pass_build_map_monad),
                nt("FACTOR") >> symlist{nt("FACTOR_TYPE_CASTING"), te(token::l_map_monad), nt("CALLABLE_RIGHT_WITH_BRACKET")} >> WO_ASTBUILDER_INDEX(ast::pass_build_map_monad),
                nt("FACTOR") >> symlist{nt("FACTOR_TYPE_CASTING"), te(token::l_map_monad), nt("FUNC_DEFINE")} >> WO_ASTBUILDER_INDEX(ast::pass_build_map_monad),
                nt("ARGUMENT_LISTS") >> symlist{te(token::l_left_brackets), nt("COMMA_EXPR"), te(token::l_right_brackets)} >> WO_ASTBUILDER_INDEX(ast::pass_direct<1>),
                nt("ARGUMENT_LISTS_MAY_EMPTY") >> symlist{nt("ARGUMENT_LISTS")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("ARGUMENT_LISTS_MAY_EMPTY") >> symlist{te(token::l_empty)} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),
                nt("FUNCTION_CALL") >> symlist{nt("FACTOR"), nt("ARGUMENT_LISTS")} >> WO_ASTBUILDER_INDEX(ast::pass_normal_function_call),
                nt("FUNCTION_CALL") >> symlist{nt("DIRECT_CALLABLE_VALUE"), nt("ARGUMENT_LISTS_MAY_EMPTY")} >> WO_ASTBUILDER_INDEX(ast::pass_directly_function_call_append_arguments),
                nt("DIRECT_CALLABLE_VALUE") >> symlist{nt("DIRECT_CALL_FIRST_ARG"), te(token::l_direct), nt("DIRECT_CALLABLE_TARGET")} >> WO_ASTBUILDER_INDEX(ast::pass_directly_function_call),
                nt("INV_DIRECT_CALL_FIRST_ARG") >> symlist{nt("SINGLE_VALUE")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("INV_DIRECT_CALL_FIRST_ARG") >> symlist{nt("ARGUMENT_EXPAND")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("INV_FUNCTION_CALL") >> symlist{nt("FUNCTION_CALL"), te(token::l_inv_direct), nt("INV_DIRECT_CALL_FIRST_ARG")} >> WO_ASTBUILDER_INDEX(ast::pass_inverse_function_call),
                nt("INV_FUNCTION_CALL") >> symlist{nt("DIRECT_CALLABLE_TARGET"), te(token::l_inv_direct), nt("INV_DIRECT_CALL_FIRST_ARG")} >> WO_ASTBUILDER_INDEX(ast::pass_inverse_function_call),
                nt("COMMA_EXPR") >> symlist{nt("COMMA_EXPR_ITEMS"), nt("COMMA_MAY_EMPTY")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("COMMA_EXPR") >> symlist{nt("COMMA_MAY_EMPTY")} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),
                nt("COMMA_EXPR_ITEMS") >> symlist{nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),
                nt("COMMA_EXPR_ITEMS") >> symlist{nt("COMMA_EXPR_ITEMS"), te(token::l_comma), nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_append_list<2, 0>),
                nt("COMMA_MAY_EMPTY") >> symlist{te(token::l_comma)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                nt("COMMA_MAY_EMPTY") >> symlist{te(token::l_empty)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                nt("SEMICOLON_MAY_EMPTY") >> symlist{te(token::l_semicolon)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                nt("SEMICOLON_MAY_EMPTY") >> symlist{te(token::l_empty)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                nt("MAY_EMPTY_TEMPLATE_ITEM") >> symlist{nt("TYPE_TEMPLATE_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("MAY_EMPTY_TEMPLATE_ITEM") >> symlist{te(token::l_empty)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                nt("MAY_EMPTY_LEFT_TEMPLATE_ITEM") >> symlist{te(token::l_empty)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                nt("MAY_EMPTY_LEFT_TEMPLATE_ITEM") >> symlist{nt("LEFT_TEMPLATE_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("LEFT_TEMPLATE_ITEM") >> symlist{te(token::l_template_using_begin), nt("TEMPLATE_ARGUMENT_LIST"), te(token::l_larg)} >> WO_ASTBUILDER_INDEX(ast::pass_direct<1>),
                nt("TYPE_TEMPLATE_ITEM") >> symlist{te(token::l_less), nt("TEMPLATE_ARGUMENT_LIST"), te(token::l_larg)} >> WO_ASTBUILDER_INDEX(ast::pass_direct<1>),
                // Template for type can be :< ... >
                nt("TYPE_TEMPLATE_ITEM") >> symlist{nt("LEFT_TEMPLATE_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("TEMPLATE_ARGUMENT_LIST") >> symlist{nt("TEMPLATE_ARGUMENT_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),
                nt("TEMPLATE_ARGUMENT_LIST") >> symlist{nt("TEMPLATE_ARGUMENT_LIST"), te(token::l_comma), nt("TEMPLATE_ARGUMENT_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_append_list<2, 0>),
                nt("TEMPLATE_ARGUMENT_ITEM") >> symlist{nt("TYPE")} >> WO_ASTBUILDER_INDEX(ast::pass_create_template_argument),
                nt("TEMPLATE_ARGUMENT_ITEM") >> symlist{te(token::l_left_curly_braces), nt("RIGHT"), te(token::l_right_curly_braces)} >> WO_ASTBUILDER_INDEX(ast::pass_create_template_argument),
                nt("DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY") >> symlist{te(token::l_empty)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                nt("DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY") >> symlist{nt("DEFINE_TEMPLATE_PARAM_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("DEFINE_TEMPLATE_PARAM_ITEM") >> symlist{te(token::l_less), nt("DEFINE_TEMPLATE_PARAM_LIST"), te(token::l_larg)} >> WO_ASTBUILDER_INDEX(ast::pass_direct<1>),
                nt("DEFINE_TEMPLATE_PARAM_LIST") >> symlist{nt("DEFINE_TEMPLATE_PARAM")} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),
                nt("DEFINE_TEMPLATE_PARAM_LIST") >> symlist{nt("DEFINE_TEMPLATE_PARAM_LIST"), te(token::l_comma), nt("DEFINE_TEMPLATE_PARAM")} >> WO_ASTBUILDER_INDEX(ast::pass_append_list<2, 0>),
                nt("DEFINE_TEMPLATE_PARAM") >> symlist{nt("IDENTIFIER")} >> WO_ASTBUILDER_INDEX(ast::pass_create_template_param),
                nt("DEFINE_TEMPLATE_PARAM") >> symlist{nt("IDENTIFIER"), te(token::l_typecast), nt("TYPE")} >> WO_ASTBUILDER_INDEX(ast::pass_create_template_param),
                nt("SENTENCE") >> symlist{nt("DECL_UNION")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("DECL_UNION") >> symlist{nt("DECL_ATTRIBUTE"), te(token::l_union), nt("AST_TOKEN_IDENTIFER"), nt("DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY"), te(token::l_left_curly_braces), nt("UNION_ITEMS"), te(token::l_right_curly_braces)} >> WO_ASTBUILDER_INDEX(ast::pass_union_declare),
                nt("UNION_ITEMS") >> symlist{nt("UNION_ITEM_LIST"), nt("COMMA_MAY_EMPTY")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("UNION_ITEM_LIST") >> symlist{nt("UNION_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),
                nt("UNION_ITEM_LIST") >> symlist{nt("UNION_ITEM_LIST"), te(token::l_comma), nt("UNION_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_append_list<2, 0>),
                nt("UNION_ITEM") >> symlist{nt("IDENTIFIER")} >> WO_ASTBUILDER_INDEX(ast::pass_union_item),
                nt("UNION_ITEM") >> symlist{nt("IDENTIFIER"), te(token::l_left_brackets), nt("TYPE"), te(token::l_right_brackets)} >> WO_ASTBUILDER_INDEX(ast::pass_union_item_constructor),
                nt("SENTENCE") >> symlist{nt("MATCH_BLOCK")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("MATCH_BLOCK") >> symlist{te(token::l_match), te(token::l_left_brackets), nt("EXPRESSION"), te(token::l_right_brackets), te(token::l_left_curly_braces), nt("MATCH_CASES"), te(token::l_right_curly_braces)} >> WO_ASTBUILDER_INDEX(ast::pass_match),
                nt("MATCH_CASES") >> symlist{nt("MATCH_CASE")} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),
                nt("MATCH_CASES") >> symlist{nt("MATCH_CASES"), nt("MATCH_CASE")} >> WO_ASTBUILDER_INDEX(ast::pass_append_list<1, 0>),
                nt("MATCH_CASE") >> symlist{nt("PATTERN_UNION_CASE"), te(token::l_question), nt("BLOCKED_SENTENCE")} >> WO_ASTBUILDER_INDEX(ast::pass_match_union_case),
                // PATTERN-CASE MAY BE A SINGLE-VARIABLE/TUPLE/STRUCT...
                nt("PATTERN_UNION_CASE") >> symlist{nt("IDENTIFIER")} >> WO_ASTBUILDER_INDEX(ast::pass_union_pattern_identifier_or_takeplace),
                // PATTERN-CASE MAY BE A UNION
                nt("PATTERN_UNION_CASE") >> symlist{nt("IDENTIFIER"), te(token::l_left_brackets), nt("DEFINE_PATTERN"), te(token::l_right_brackets)} >> WO_ASTBUILDER_INDEX(ast::pass_union_pattern_contain_element),
                //////////////////////////////////////////////////////////////////////////////////////
                nt("UNIT") >> symlist{
                                  nt("STRUCT_INSTANCE_BEGIN"), // Here we use Callable left stand for type. so we cannot support x here...
                                  te(token::l_left_curly_braces),
                                  nt("STRUCT_MEMBER_INITS"),
                                  te(token::l_right_curly_braces),
                              } >>
                    WO_ASTBUILDER_INDEX(ast::pass_struct_instance),
                nt("STRUCT_INSTANCE_BEGIN") >> symlist{nt("STRUCTABLE_TYPE_FOR_CONSTRUCT")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("STRUCT_INSTANCE_BEGIN") >> symlist{te(token::l_struct)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                nt("STRUCT_MEMBER_INITS") >> symlist{nt("STRUCT_MEMBER_INITS_EMPTY")} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),
                nt("STRUCT_MEMBER_INITS_EMPTY") >> symlist{te(token::l_empty)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                nt("STRUCT_MEMBER_INITS_EMPTY") >> symlist{te(token::l_comma)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                nt("STRUCT_MEMBER_INITS") >> symlist{nt("STRUCT_MEMBERS_INIT_LIST"), nt("COMMA_MAY_EMPTY")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("STRUCT_MEMBERS_INIT_LIST") >> symlist{nt("STRUCT_MEMBER_INIT_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),
                nt("STRUCT_MEMBERS_INIT_LIST") >> symlist{nt("STRUCT_MEMBERS_INIT_LIST"), te(token::l_comma), nt("STRUCT_MEMBER_INIT_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_append_list<2, 0>),
                nt("STRUCT_MEMBER_INIT_ITEM") >> symlist{nt("IDENTIFIER"), te(token::l_assign), nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_struct_member_init_pair),
                ////////////////////////////////////////////////////
                nt("DEFINE_PATTERN") >> symlist{nt("IDENTIFIER")} >> WO_ASTBUILDER_INDEX(ast::pass_pattern_identifier_or_takepace),
                nt("DEFINE_PATTERN") >> symlist{te(token::l_mut), nt("IDENTIFIER")} >> WO_ASTBUILDER_INDEX(ast::pass_pattern_mut_identifier_or_takepace),
                nt("DEFINE_PATTERN_WITH_TEMPLATE") >> symlist{nt("IDENTIFIER"), nt("DEFINE_TEMPLATE_PARAM_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_pattern_identifier_or_takepace_with_template),
                nt("DEFINE_PATTERN_WITH_TEMPLATE") >> symlist{te(token::l_mut), nt("IDENTIFIER"), nt("DEFINE_TEMPLATE_PARAM_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_pattern_mut_identifier_or_takepace_with_template),
                nt("DEFINE_PATTERN") >> symlist{te(token::l_left_brackets), nt("DEFINE_PATTERN_LIST"), te(token::l_right_brackets)} >> WO_ASTBUILDER_INDEX(ast::pass_pattern_tuple),
                nt("DEFINE_PATTERN") >> symlist{te(token::l_left_brackets), nt("COMMA_MAY_EMPTY"), te(token::l_right_brackets)} >> WO_ASTBUILDER_INDEX(ast::pass_pattern_tuple),
                nt("DEFINE_PATTERN_LIST") >> symlist{nt("DEFINE_PATTERN_ITEMS"), nt("COMMA_MAY_EMPTY")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("DEFINE_PATTERN_ITEMS") >> symlist{nt("DEFINE_PATTERN")} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),
                nt("DEFINE_PATTERN_ITEMS") >> symlist{nt("DEFINE_PATTERN_ITEMS"), te(token::l_comma), nt("DEFINE_PATTERN")} >> WO_ASTBUILDER_INDEX(ast::pass_append_list<2, 0>),
                //////////////////////////////////////////////////////////////////////////////////////
                nt("AST_TOKEN_IDENTIFER") >> symlist{nt("IDENTIFIER")} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                nt("AST_TOKEN_NIL") >> symlist{te(token::l_nil)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                nt("IDENTIFIER") >> symlist{te(token::l_identifier)} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                nt("USELESS_TOKEN") >> symlist{te(token::l_double_index_point)} >> WO_ASTBUILDER_INDEX(ast::pass_useless_token),
                nt("USELESS_TOKEN") >> symlist{te(token::l_unknown_token)} >> WO_ASTBUILDER_INDEX(ast::pass_useless_token),
                nt("USELESS_TOKEN") >> symlist{te(token::l_macro)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
            });

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
