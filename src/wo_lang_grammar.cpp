#include "wo_afx.hpp"

#include "wo_crc_64.hpp"
#include "wo_lang_ast_builder.hpp"

#ifndef WO_DISABLE_COMPILER
#include "wo_lang_grammar_loader.hpp"
#include "wo_lang_grammar_lr1_autogen.hpp"
#endif

#include <fstream>

#define _WO_LSTR(X) L##X
#define WO_LSTR(X) _WO_LSTR(X)

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
            wo_stdout << ANSI_HIY "WooGramma: " ANSI_RST "Syntax update detected, LR table cache is being regenerating..." << wo_endl;

            using gm = wo::grammar;
            grammar_instance = std::make_unique<grammar>(std::vector<grammar::rule>{
                // nt >> list{nt/te ... } [>> ast_create_function]
                gm::nt("PROGRAM_AUGMENTED") >> gm::symlist{gm::nt("PROGRAM")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("PROGRAM") >> gm::symlist{gm::te(gm::ttype::l_empty)} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("PROGRAM") >> gm::symlist{gm::nt("USELESS_TOKEN")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("PROGRAM") >> gm::symlist{gm::nt("PARAGRAPH")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("PARAGRAPH") >> gm::symlist{gm::nt("SENTENCE_LIST")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("SENTENCE_LIST") >> gm::symlist{gm::nt("SENTENCE_LIST"), gm::nt("LABELED_SENTENCE")} >> WO_ASTBUILDER_INDEX(ast::pass_append_list<1, 0>),
                gm::nt("SENTENCE_LIST") >> gm::symlist{gm::nt("LABELED_SENTENCE")} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),
                gm::nt("LABELED_SENTENCE") >> gm::symlist{gm::nt("IDENTIFIER"), gm::te(gm::ttype::l_at), gm::nt("SENTENCE")} >> WO_ASTBUILDER_INDEX(ast::pass_mark_label),
                gm::nt("LABELED_SENTENCE") >> gm::symlist{gm::nt("SENTENCE")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                // NOTE: macro might defined after import sentence. to make sure macro can be handle correctly.
                //      we make sure import happend before macro been peek and check.
                gm::nt("SENTENCE") >> gm::symlist{
                                           gm::nt("IMPORT_SENTENCE"),
                                           gm::te(gm::ttype::l_semicolon)} >>
                    WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("IMPORT_SENTENCE") >> gm::symlist{gm::te(gm::ttype::l_import), gm::nt("SCOPED_LIST_NORMAL")} >> WO_ASTBUILDER_INDEX(ast::pass_import_files),
                gm::nt("IMPORT_SENTENCE") >> gm::symlist{gm::te(gm::ttype::l_export), gm::te(gm::ttype::l_import), gm::nt("SCOPED_LIST_NORMAL")} >> WO_ASTBUILDER_INDEX(ast::pass_import_files),
                gm::nt("SENTENCE") >> gm::symlist{gm::nt("DECL_ATTRIBUTE"), // useless
                                                   gm::te(gm::ttype::l_using), gm::nt("SCOPED_LIST_NORMAL"), gm::te(gm::ttype::l_semicolon)} >>
                    WO_ASTBUILDER_INDEX(ast::pass_using_namespace),
                gm::nt("DECLARE_NEW_TYPE") >> gm::symlist{gm::nt("DECL_ATTRIBUTE"), gm::te(gm::ttype::l_using), gm::nt("IDENTIFIER"), gm::nt("DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY"), gm::te(gm::ttype::l_assign), gm::nt("TYPE")} >> WO_ASTBUILDER_INDEX(ast::pass_using_type_as),
                gm::nt("SENTENCE") >> gm::symlist{gm::nt("DECLARE_NEW_TYPE"), gm::nt("SENTENCE_BLOCK_MAY_SEMICOLON")} >> WO_ASTBUILDER_INDEX(ast::pass_using_typename_space),
                gm::nt("SENTENCE_BLOCK_MAY_SEMICOLON") >> gm::symlist{gm::te(gm::ttype::l_semicolon)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                gm::nt("SENTENCE_BLOCK_MAY_SEMICOLON") >> gm::symlist{gm::nt("SENTENCE_BLOCK")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("SENTENCE") >> gm::symlist{gm::nt("DECL_ATTRIBUTE"), gm::te(gm::ttype::l_alias), gm::nt("IDENTIFIER"), gm::nt("DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY"), gm::te(gm::ttype::l_assign), gm::nt("TYPE"), gm::te(gm::ttype::l_semicolon)} >> WO_ASTBUILDER_INDEX(ast::pass_alias_type_as),
                //////////////////////////////////////////////////////////////////////////////////////
                gm::nt("SENTENCE") >> gm::symlist{gm::nt("DECL_ENUM")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("DECL_ENUM") >> gm::symlist{gm::nt("DECL_ATTRIBUTE"), gm::te(gm::ttype::l_enum), gm::nt("IDENTIFIER"), gm::te(gm::ttype::l_left_curly_braces), gm::nt("ENUM_ITEMS"), gm::te(gm::ttype::l_right_curly_braces)} >> WO_ASTBUILDER_INDEX(ast::pass_enum_finalize),
                gm::nt("ENUM_ITEMS") >> gm::symlist{gm::nt("ENUM_ITEM_LIST"), gm::nt("COMMA_MAY_EMPTY")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("ENUM_ITEM_LIST") >> gm::symlist{gm::nt("ENUM_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),
                gm::nt("ENUM_ITEM_LIST") >> gm::symlist{gm::nt("ENUM_ITEM_LIST"), gm::te(gm::ttype::l_comma), gm::nt("ENUM_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_append_list<2, 0>),
                gm::nt("ENUM_ITEM") >> gm::symlist{gm::nt("IDENTIFIER")} >> WO_ASTBUILDER_INDEX(ast::pass_enum_item_create),
                gm::nt("ENUM_ITEM") >> gm::symlist{gm::nt("IDENTIFIER"), gm::te(gm::ttype::l_assign), gm::nt("EXPRESSION")} >> WO_ASTBUILDER_INDEX(ast::pass_enum_item_create),
                gm::nt("SENTENCE") >> gm::symlist{gm::nt("DECL_NAMESPACE")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("DECL_NAMESPACE") >> gm::symlist{gm::te(gm::ttype::l_namespace), gm::nt("SPACE_NAME_LIST"), gm::nt("SENTENCE_BLOCK")} >> WO_ASTBUILDER_INDEX(ast::pass_namespace),
                gm::nt("SPACE_NAME_LIST") >> gm::symlist{gm::nt("SPACE_NAME")} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),
                gm::nt("SPACE_NAME_LIST") >> gm::symlist{gm::nt("SPACE_NAME_LIST"), gm::te(gm::ttype::l_scopeing), gm::nt("SPACE_NAME")} >> WO_ASTBUILDER_INDEX(ast::pass_append_list<2, 0>),
                gm::nt("SPACE_NAME") >> gm::symlist{gm::nt("IDENTIFIER")} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                gm::nt("BLOCKED_SENTENCE") >> gm::symlist{gm::nt("LABELED_SENTENCE")} >> WO_ASTBUILDER_INDEX(ast::pass_sentence_block<0>),
                gm::nt("SENTENCE") >> gm::symlist{gm::nt("SENTENCE_BLOCK")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("SENTENCE_BLOCK") >> gm::symlist{gm::te(gm::ttype::l_left_curly_braces), gm::nt("PARAGRAPH"), gm::te(gm::ttype::l_right_curly_braces)} >> WO_ASTBUILDER_INDEX(ast::pass_sentence_block<1>),
                // Because of CONSTANT MAP => ,,, => { l_empty } Following production will cause R-R Conflict
                // gm::nt("PARAGRAPH") >> gm::symlist{gm::te(gm::ttype::l_empty)},
                // So, just make a production like this:
                gm::nt("SENTENCE_BLOCK") >> gm::symlist{gm::te(gm::ttype::l_left_curly_braces), gm::te(gm::ttype::l_right_curly_braces)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                // NOTE: Why the production can solve the conflict?
                //       A > {}
                //       B > {l_empty}
                //       In fact, A B have same production, but in wo_lr(1) parser, l_empty have a lower priority then production like A
                //       This rule can solve many grammar conflict easily, but in some case, it will cause bug, so please use it carefully.
                gm::nt("DECL_ATTRIBUTE") >> gm::symlist{gm::nt("DECL_ATTRIBUTE_ITEMS")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("DECL_ATTRIBUTE") >> gm::symlist{gm::te(gm::ttype::l_empty)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                gm::nt("DECL_ATTRIBUTE_ITEMS") >> gm::symlist{gm::nt("DECL_ATTRIBUTE_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_attribute),
                gm::nt("DECL_ATTRIBUTE_ITEMS") >> gm::symlist{gm::nt("DECL_ATTRIBUTE_ITEMS"), gm::nt("DECL_ATTRIBUTE_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_attribute_append),
                gm::nt("DECL_ATTRIBUTE_ITEM") >> gm::symlist{gm::nt("LIFECYCLE_MODIFER")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("DECL_ATTRIBUTE_ITEM") >> gm::symlist{gm::nt("EXTERNAL_MODIFER")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("DECL_ATTRIBUTE_ITEM") >> gm::symlist{gm::nt("ACCESS_MODIFIER")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("LIFECYCLE_MODIFER") >> gm::symlist{gm::te(gm::ttype::l_static)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                gm::nt("EXTERNAL_MODIFER") >> gm::symlist{gm::te(gm::ttype::l_extern)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                gm::nt("ACCESS_MODIFIER") >> gm::symlist{gm::te(gm::ttype::l_public)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                gm::nt("ACCESS_MODIFIER") >> gm::symlist{gm::te(gm::ttype::l_private)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                gm::nt("ACCESS_MODIFIER") >> gm::symlist{gm::te(gm::ttype::l_protected)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                gm::nt("SENTENCE") >> gm::symlist{gm::te(gm::ttype::l_semicolon)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                gm::nt("WHERE_CONSTRAINT_WITH_SEMICOLON") >> gm::symlist{gm::nt("WHERE_CONSTRAINT"), gm::te(gm::ttype::l_semicolon)} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("WHERE_CONSTRAINT_WITH_SEMICOLON") >> gm::symlist{gm::te(gm::ttype::l_empty)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                gm::nt("WHERE_CONSTRAINT_MAY_EMPTY") >> gm::symlist{gm::nt("WHERE_CONSTRAINT")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("WHERE_CONSTRAINT_MAY_EMPTY") >> gm::symlist{gm::te(gm::ttype::l_empty)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                gm::nt("WHERE_CONSTRAINT") >> gm::symlist{gm::te(gm::ttype::l_where), gm::nt("CONSTRAINT_LIST"), gm::nt("COMMA_MAY_EMPTY")} >> WO_ASTBUILDER_INDEX(ast::pass_build_where_constraint),
                gm::nt("CONSTRAINT_LIST") >> gm::symlist{gm::nt("EXPRESSION")} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),
                gm::nt("CONSTRAINT_LIST") >> gm::symlist{gm::nt("CONSTRAINT_LIST"), gm::te(gm::ttype::l_comma), gm::nt("EXPRESSION")} >> WO_ASTBUILDER_INDEX(ast::pass_append_list<2, 0>),
                gm::nt("SENTENCE") >> gm::symlist{gm::nt("FUNC_DEFINE_WITH_NAME")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("FUNC_DEFINE_WITH_NAME") >> gm::symlist{gm::nt("DECL_ATTRIBUTE"), gm::te(gm::ttype::l_func), gm::nt("AST_TOKEN_IDENTIFER"), gm::nt("DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY"), gm::te(gm::ttype::l_left_brackets), gm::nt("ARGDEFINE"), gm::te(gm::ttype::l_right_brackets), gm::nt("RETURN_TYPE_DECLEAR_MAY_EMPTY"), gm::nt("WHERE_CONSTRAINT_WITH_SEMICOLON"), gm::nt("SENTENCE_BLOCK")} >> WO_ASTBUILDER_INDEX(ast::pass_func_def_named),
                gm::nt("FUNC_DEFINE_WITH_NAME") >> gm::symlist{gm::nt("DECL_ATTRIBUTE"), gm::te(gm::ttype::l_func), gm::te(gm::ttype::l_operator), gm::nt("OVERLOADINGABLE_OPERATOR"), gm::nt("DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY"), gm::te(gm::ttype::l_left_brackets), gm::nt("ARGDEFINE"), gm::te(gm::ttype::l_right_brackets), gm::nt("RETURN_TYPE_DECLEAR_MAY_EMPTY"), gm::nt("WHERE_CONSTRAINT_WITH_SEMICOLON"), gm::nt("SENTENCE_BLOCK")} >> WO_ASTBUILDER_INDEX(ast::pass_func_def_oper),
                gm::nt("OVERLOADINGABLE_OPERATOR") >> gm::symlist{gm::te(gm::ttype::l_add)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                gm::nt("OVERLOADINGABLE_OPERATOR") >> gm::symlist{gm::te(gm::ttype::l_sub)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                gm::nt("OVERLOADINGABLE_OPERATOR") >> gm::symlist{gm::te(gm::ttype::l_mul)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                gm::nt("OVERLOADINGABLE_OPERATOR") >> gm::symlist{gm::te(gm::ttype::l_div)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                gm::nt("OVERLOADINGABLE_OPERATOR") >> gm::symlist{gm::te(gm::ttype::l_mod)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                gm::nt("OVERLOADINGABLE_OPERATOR") >> gm::symlist{gm::te(gm::ttype::l_less)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                gm::nt("OVERLOADINGABLE_OPERATOR") >> gm::symlist{gm::te(gm::ttype::l_larg)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                gm::nt("OVERLOADINGABLE_OPERATOR") >> gm::symlist{gm::te(gm::ttype::l_less_or_equal)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                gm::nt("OVERLOADINGABLE_OPERATOR") >> gm::symlist{gm::te(gm::ttype::l_larg_or_equal)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                gm::nt("OVERLOADINGABLE_OPERATOR") >> gm::symlist{gm::te(gm::ttype::l_equal)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                gm::nt("OVERLOADINGABLE_OPERATOR") >> gm::symlist{gm::te(gm::ttype::l_not_equal)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                gm::nt("OVERLOADINGABLE_OPERATOR") >> gm::symlist{gm::te(gm::ttype::l_land)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                gm::nt("OVERLOADINGABLE_OPERATOR") >> gm::symlist{gm::te(gm::ttype::l_lor)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                gm::nt("OVERLOADINGABLE_OPERATOR") >> gm::symlist{gm::te(gm::ttype::l_lnot)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                gm::nt("EXTERN_FROM") >> gm::symlist{gm::te(gm::ttype::l_extern), gm::te(gm::ttype::l_left_brackets), gm::te(gm::ttype::l_literal_string), gm::te(gm::ttype::l_comma), gm::te(gm::ttype::l_literal_string), gm::nt("EXTERN_ATTRIBUTES"), gm::te(gm::ttype::l_right_brackets)} >> WO_ASTBUILDER_INDEX(ast::pass_extern),
                gm::nt("EXTERN_FROM") >> gm::symlist{gm::te(gm::ttype::l_extern), gm::te(gm::ttype::l_left_brackets), gm::te(gm::ttype::l_literal_string), gm::nt("EXTERN_ATTRIBUTES"), gm::te(gm::ttype::l_right_brackets)} >> WO_ASTBUILDER_INDEX(ast::pass_extern),
                gm::nt("EXTERN_ATTRIBUTES") >> gm::symlist{gm::te(gm::ttype::l_empty)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                gm::nt("EXTERN_ATTRIBUTES") >> gm::symlist{gm::te(gm::ttype::l_comma), gm::nt("EXTERN_ATTRIBUTE_LIST")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<1>),
                gm::nt("EXTERN_ATTRIBUTE_LIST") >> gm::symlist{gm::nt("EXTERN_ATTRIBUTE")} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),
                gm::nt("EXTERN_ATTRIBUTE_LIST") >> gm::symlist{gm::nt("EXTERN_ATTRIBUTE_LIST"), gm::te(gm::ttype::l_comma), gm::nt("EXTERN_ATTRIBUTE")} >> WO_ASTBUILDER_INDEX(ast::pass_append_list<2, 0>),
                gm::nt("EXTERN_ATTRIBUTE") >> gm::symlist{
                                                   gm::te(gm::ttype::l_identifier),
                                               } >>
                    WO_ASTBUILDER_INDEX(ast::pass_token),
                gm::nt("FUNC_DEFINE_WITH_NAME") >> gm::symlist{gm::nt("EXTERN_FROM"), gm::nt("DECL_ATTRIBUTE"), gm::te(gm::ttype::l_func), gm::nt("AST_TOKEN_IDENTIFER"), gm::nt("DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY"), gm::te(gm::ttype::l_left_brackets), gm::nt("ARGDEFINE"), gm::te(gm::ttype::l_right_brackets), gm::nt("RETURN_TYPE_DECLEAR"), gm::nt("WHERE_CONSTRAINT_MAY_EMPTY"), gm::te(gm::ttype::l_semicolon)} >> WO_ASTBUILDER_INDEX(ast::pass_func_def_extn),
                gm::nt("FUNC_DEFINE_WITH_NAME") >> gm::symlist{gm::nt("EXTERN_FROM"), gm::nt("DECL_ATTRIBUTE"), gm::te(gm::ttype::l_func), gm::te(gm::ttype::l_operator), gm::nt("OVERLOADINGABLE_OPERATOR"), gm::nt("DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY"), gm::te(gm::ttype::l_left_brackets), gm::nt("ARGDEFINE"), gm::te(gm::ttype::l_right_brackets), gm::nt("RETURN_TYPE_DECLEAR"), gm::nt("WHERE_CONSTRAINT_MAY_EMPTY"), gm::te(gm::ttype::l_semicolon)} >> WO_ASTBUILDER_INDEX(ast::pass_func_def_extn_oper),
                gm::nt("SENTENCE") >> gm::symlist{gm::nt("WHILE")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("WHILE") >> gm::symlist{gm::te(gm::ttype::l_while), gm::te(gm::ttype::l_left_brackets), gm::nt("EXPRESSION"), gm::te(gm::ttype::l_right_brackets), gm::nt("BLOCKED_SENTENCE")} >> WO_ASTBUILDER_INDEX(ast::pass_while),
                gm::nt("SENTENCE") >> gm::symlist{gm::nt("FORLOOP")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("VAR_DEFINE_WITH_SEMICOLON") >> gm::symlist{gm::nt("VAR_DEFINE_LET_SENTENCE"), gm::te(gm::ttype::l_semicolon)} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("FORLOOP") >> gm::symlist{gm::te(gm::ttype::l_for), gm::te(gm::ttype::l_left_brackets), gm::nt("VAR_DEFINE_WITH_SEMICOLON"), gm::nt("MAY_EMPTY_EXPRESSION"), gm::te(gm::ttype::l_semicolon), gm::nt("MAY_EMPTY_EXPRESSION"), gm::te(gm::ttype::l_right_brackets), gm::nt("BLOCKED_SENTENCE")} >> WO_ASTBUILDER_INDEX(ast::pass_for_defined),
                gm::nt("FORLOOP") >> gm::symlist{gm::te(gm::ttype::l_for), gm::te(gm::ttype::l_left_brackets), gm::nt("MAY_EMPTY_EXPRESSION"), gm::te(gm::ttype::l_semicolon), gm::nt("MAY_EMPTY_EXPRESSION"), gm::te(gm::ttype::l_semicolon), gm::nt("MAY_EMPTY_EXPRESSION"), gm::te(gm::ttype::l_right_brackets), gm::nt("BLOCKED_SENTENCE")} >> WO_ASTBUILDER_INDEX(ast::pass_for_expr),
                gm::nt("SENTENCE") >> gm::symlist{gm::te(gm::ttype::l_break), gm::te(gm::ttype::l_semicolon)} >> WO_ASTBUILDER_INDEX(ast::pass_break),
                gm::nt("SENTENCE") >> gm::symlist{gm::te(gm::ttype::l_continue), gm::te(gm::ttype::l_semicolon)} >> WO_ASTBUILDER_INDEX(ast::pass_continue),
                gm::nt("SENTENCE") >> gm::symlist{gm::te(gm::ttype::l_break), gm::nt("IDENTIFIER"), gm::te(gm::ttype::l_semicolon)} >> WO_ASTBUILDER_INDEX(ast::pass_break_label),
                gm::nt("SENTENCE") >> gm::symlist{gm::te(gm::ttype::l_continue), gm::nt("IDENTIFIER"), gm::te(gm::ttype::l_semicolon)} >> WO_ASTBUILDER_INDEX(ast::pass_continue_label),
                gm::nt("MAY_EMPTY_EXPRESSION") >> gm::symlist{gm::nt("EXPRESSION")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("MAY_EMPTY_EXPRESSION") >> gm::symlist{gm::te(gm::ttype::l_empty)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                gm::nt("SENTENCE") >> gm::symlist{gm::nt("FOREACH")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("FOREACH") >> gm::symlist{gm::te(gm::ttype::l_for), gm::te(gm::ttype::l_left_brackets), gm::nt("DECL_ATTRIBUTE"), gm::te(gm::ttype::l_let), gm::nt("DEFINE_PATTERN"), gm::te(gm::ttype::l_typecast), gm::nt("EXPRESSION"), gm::te(gm::ttype::l_right_brackets), gm::nt("BLOCKED_SENTENCE")} >> WO_ASTBUILDER_INDEX(ast::pass_foreach),
                gm::nt("SENTENCE") >> gm::symlist{gm::nt("IF")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("IF") >> gm::symlist{gm::te(gm::ttype::l_if), gm::te(gm::ttype::l_left_brackets), gm::nt("EXPRESSION"), gm::te(gm::ttype::l_right_brackets), gm::nt("BLOCKED_SENTENCE"), gm::nt("ELSE")} >> WO_ASTBUILDER_INDEX(ast::pass_if),
                gm::nt("ELSE") >> gm::symlist{gm::te(gm::ttype::l_empty)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                gm::nt("ELSE") >> gm::symlist{gm::te(gm::ttype::l_else), gm::nt("BLOCKED_SENTENCE")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<1>),
                gm::nt("SENTENCE") >> gm::symlist{gm::nt("VAR_DEFINE_LET_SENTENCE"), gm::te(gm::ttype::l_semicolon)} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),                                     // ASTVariableDefination
                gm::nt("VAR_DEFINE_LET_SENTENCE") >> gm::symlist{gm::nt("DECL_ATTRIBUTE"), gm::te(gm::ttype::l_let), gm::nt("VARDEFINES")} >> WO_ASTBUILDER_INDEX(ast::pass_variable_defines),       // ASTVariableDefination
                gm::nt("VARDEFINES") >> gm::symlist{gm::nt("VARDEFINE")} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),                                                                            // ASTVariableDefination
                gm::nt("VARDEFINES") >> gm::symlist{gm::nt("VARDEFINES"), gm::te(gm::ttype::l_comma), gm::nt("VARDEFINE")} >> WO_ASTBUILDER_INDEX(ast::pass_append_list<2, 0>),                      // ASTVariableDefination
                gm::nt("VARDEFINE") >> gm::symlist{gm::nt("DEFINE_PATTERN_MAY_TEMPLATE"), gm::te(gm::ttype::l_assign), gm::nt("EXPRESSION")} >> WO_ASTBUILDER_INDEX(ast::pass_variable_define_item), // ASTVariableDefination
                gm::nt("DEFINE_PATTERN_MAY_TEMPLATE") >> gm::symlist{gm::nt("DEFINE_PATTERN")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("DEFINE_PATTERN_MAY_TEMPLATE") >> gm::symlist{gm::nt("DEFINE_PATTERN_WITH_TEMPLATE")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("SENTENCE") >> gm::symlist{gm::te(gm::ttype::l_return), gm::nt("RIGHT"), gm::te(gm::ttype::l_semicolon)} >> WO_ASTBUILDER_INDEX(ast::pass_return_value),
                gm::nt("SENTENCE") >> gm::symlist{gm::te(gm::ttype::l_return), gm::te(gm::ttype::l_semicolon)} >> WO_ASTBUILDER_INDEX(ast::pass_return),
                gm::nt("SENTENCE") >> gm::symlist{gm::nt("EXPRESSION"), gm::te(gm::ttype::l_semicolon)} >> WO_ASTBUILDER_INDEX(ast::pass_direct_keep_source_location<0>),
                gm::nt("EXPRESSION") >> gm::symlist{gm::nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("RIGHT") >> gm::symlist{gm::te(gm::ttype::l_do), gm::nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_do_void_cast),
                gm::nt("RIGHT") >> gm::symlist{gm::te(gm::ttype::l_mut), gm::nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_mark_mut),
                gm::nt("RIGHT") >> gm::symlist{gm::te(gm::ttype::l_immut), gm::nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_mark_immut),
                gm::nt("RIGHT") >> gm::symlist{gm::nt("ASSIGNMENT")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("RIGHT") >> gm::symlist{gm::nt("LOGICAL_OR")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("ASSIGNMENT") >> gm::symlist{gm::nt("ASSIGNED_PATTERN"), gm::te(gm::ttype::l_assign), gm::nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_assign_operation),
                gm::nt("ASSIGNMENT") >> gm::symlist{gm::nt("ASSIGNED_PATTERN"), gm::te(gm::ttype::l_add_assign), gm::nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_assign_operation),
                gm::nt("ASSIGNMENT") >> gm::symlist{gm::nt("ASSIGNED_PATTERN"), gm::te(gm::ttype::l_sub_assign), gm::nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_assign_operation),
                gm::nt("ASSIGNMENT") >> gm::symlist{gm::nt("ASSIGNED_PATTERN"), gm::te(gm::ttype::l_mul_assign), gm::nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_assign_operation),
                gm::nt("ASSIGNMENT") >> gm::symlist{gm::nt("ASSIGNED_PATTERN"), gm::te(gm::ttype::l_div_assign), gm::nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_assign_operation),
                gm::nt("ASSIGNMENT") >> gm::symlist{gm::nt("ASSIGNED_PATTERN"), gm::te(gm::ttype::l_mod_assign), gm::nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_assign_operation),
                gm::nt("ASSIGNMENT") >> gm::symlist{gm::nt("ASSIGNED_PATTERN"), gm::te(gm::ttype::l_value_assign), gm::nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_assign_operation),
                gm::nt("ASSIGNMENT") >> gm::symlist{gm::nt("ASSIGNED_PATTERN"), gm::te(gm::ttype::l_value_add_assign), gm::nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_assign_operation),
                gm::nt("ASSIGNMENT") >> gm::symlist{gm::nt("ASSIGNED_PATTERN"), gm::te(gm::ttype::l_value_sub_assign), gm::nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_assign_operation),
                gm::nt("ASSIGNMENT") >> gm::symlist{gm::nt("ASSIGNED_PATTERN"), gm::te(gm::ttype::l_value_mul_assign), gm::nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_assign_operation),
                gm::nt("ASSIGNMENT") >> gm::symlist{gm::nt("ASSIGNED_PATTERN"), gm::te(gm::ttype::l_value_div_assign), gm::nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_assign_operation),
                gm::nt("ASSIGNMENT") >> gm::symlist{gm::nt("ASSIGNED_PATTERN"), gm::te(gm::ttype::l_value_mod_assign), gm::nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_assign_operation),
                gm::nt("ASSIGNED_PATTERN") >> gm::symlist{gm::nt("LEFT")} >> WO_ASTBUILDER_INDEX(ast::pass_pattern_for_assign),
                gm::nt("RIGHT") >> gm::symlist{gm::nt("LOGICAL_OR"), gm::te(gm::ttype::l_question), gm::nt("RIGHT"), gm::te(gm::ttype::l_or), gm::nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_conditional_expression),
                gm::nt("FACTOR") >> gm::symlist{gm::nt("FUNC_DEFINE")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("FUNC_DEFINE") >> gm::symlist{gm::nt("DECL_ATTRIBUTE"), gm::te(gm::ttype::l_func), gm::nt("DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY"), gm::te(gm::ttype::l_left_brackets), gm::nt("ARGDEFINE"), gm::te(gm::ttype::l_right_brackets), gm::nt("RETURN_TYPE_DECLEAR_MAY_EMPTY"), gm::nt("WHERE_CONSTRAINT_WITH_SEMICOLON"), gm::nt("SENTENCE_BLOCK")} >> WO_ASTBUILDER_INDEX(ast::pass_func_lambda_ml),
                gm::nt("FUNC_DEFINE") >> gm::symlist{gm::te(gm::ttype::l_lambda), gm::nt("DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY"), gm::nt("ARGDEFINE"), gm::te(gm::ttype::l_assign), gm::nt("RETURN_EXPR_BLOCK_IN_LAMBDA"), gm::nt("WHERE_DECL_FOR_LAMBDA"), gm::te(gm::ttype::l_semicolon)} >> WO_ASTBUILDER_INDEX(ast::pass_func_lambda),
                // May empty
                gm::nt("WHERE_DECL_FOR_LAMBDA") >> gm::symlist{gm::te(gm::ttype::l_empty)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                gm::nt("WHERE_DECL_FOR_LAMBDA") >> gm::symlist{gm::te(gm::ttype::l_where), gm::nt("VARDEFINES")} >> WO_ASTBUILDER_INDEX(ast::pass_reverse_vardef),
                gm::nt("RETURN_EXPR_BLOCK_IN_LAMBDA") >> gm::symlist{gm::nt("RETURN_EXPR_IN_LAMBDA")} >> WO_ASTBUILDER_INDEX(ast::pass_sentence_block<0>),
                gm::nt("RETURN_EXPR_IN_LAMBDA") >> gm::symlist{gm::nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_return_lambda),
                gm::nt("RETURN_TYPE_DECLEAR_MAY_EMPTY") >> gm::symlist{gm::te(gm::ttype::l_empty)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                gm::nt("RETURN_TYPE_DECLEAR_MAY_EMPTY") >> gm::symlist{gm::nt("RETURN_TYPE_DECLEAR")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("RETURN_TYPE_DECLEAR") >> gm::symlist{gm::te(gm::ttype::l_function_result), gm::nt("TYPE")} >> WO_ASTBUILDER_INDEX(ast::pass_direct_keep_source_location<1>),
                gm::nt("TYPE_DECLEAR") >> gm::symlist{gm::te(gm::ttype::l_typecast), gm::nt("TYPE")} >> WO_ASTBUILDER_INDEX(ast::pass_direct_keep_source_location<1>),
                /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
                gm::nt("TYPE") >> gm::symlist{gm::nt("ORIGIN_TYPE")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("TYPE") >> gm::symlist{gm::te(gm::ttype::l_mut), gm::nt("ORIGIN_TYPE")} >> WO_ASTBUILDER_INDEX(ast::pass_type_mutable),
                gm::nt("TYPE") >> gm::symlist{gm::te(gm::ttype::l_immut), gm::nt("ORIGIN_TYPE")} >> WO_ASTBUILDER_INDEX(ast::pass_type_immutable),

                gm::nt("ORIGIN_TYPE") >> gm::symlist{gm::nt("AST_TOKEN_NIL")} >> WO_ASTBUILDER_INDEX(ast::pass_type_nil),
                gm::nt("ORIGIN_TYPE") >> gm::symlist{
                                              gm::nt("TUPLE_TYPE_LIST"),
                                              gm::te(gm::ttype::l_function_result),
                                              gm::nt("TYPE"),
                                          } >>
                    WO_ASTBUILDER_INDEX(ast::pass_type_func),

                gm::nt("ORIGIN_TYPE") >> gm::symlist{
                                              gm::te(gm::ttype::l_struct),
                                              gm::te(gm::ttype::l_left_curly_braces),
                                              gm::nt("STRUCT_MEMBER_DEFINES"),
                                              gm::te(gm::ttype::l_right_curly_braces),
                                          } >>
                    WO_ASTBUILDER_INDEX(ast::pass_type_struct),
                gm::nt("STRUCT_MEMBER_DEFINES") >> gm::symlist{gm::te(gm::ttype::l_empty)} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),
                gm::nt("STRUCT_MEMBER_DEFINES") >> gm::symlist{gm::nt("STRUCT_MEMBERS_LIST"), gm::nt("COMMA_MAY_EMPTY")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("STRUCT_MEMBERS_LIST") >> gm::symlist{gm::nt("STRUCT_MEMBER_PAIR")} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),
                gm::nt("STRUCT_MEMBERS_LIST") >> gm::symlist{gm::nt("STRUCT_MEMBERS_LIST"), gm::te(gm::ttype::l_comma), gm::nt("STRUCT_MEMBER_PAIR")} >> WO_ASTBUILDER_INDEX(ast::pass_append_list<2, 0>),
                gm::nt("STRUCT_MEMBER_PAIR") >> gm::symlist{gm::nt("ACCESS_MODIFIER_MAY_EMPTY"), gm::nt("IDENTIFIER"), gm::nt("TYPE_DECLEAR")} >> WO_ASTBUILDER_INDEX(ast::pass_type_struct_field),
                gm::nt("ACCESS_MODIFIER_MAY_EMPTY") >> gm::symlist{
                                                            gm::nt("ACCESS_MODIFIER"),
                                                        } >>
                    WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("ACCESS_MODIFIER_MAY_EMPTY") >> gm::symlist{
                                                            gm::te(gm::ttype::l_empty),
                                                        } >>
                    WO_ASTBUILDER_INDEX(ast::pass_empty),

                gm::nt("ORIGIN_TYPE") >> gm::symlist{gm::nt("STRUCTABLE_TYPE")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("STRUCTABLE_TYPE") >> gm::symlist{
                                                  gm::nt("SCOPED_IDENTIFIER_FOR_TYPE"),
                                              } >>
                    WO_ASTBUILDER_INDEX(ast::pass_type_from_identifier),
                gm::nt("STRUCTABLE_TYPE") >> gm::symlist{
                                                  gm::nt("TYPEOF"),
                                              } >>
                    WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("STRUCTABLE_TYPE_FOR_CONSTRUCT") >> gm::symlist{
                                                                gm::nt("SCOPED_IDENTIFIER_FOR_VAL"),
                                                            } >>
                    WO_ASTBUILDER_INDEX(ast::pass_type_from_identifier),
                gm::nt("STRUCTABLE_TYPE_FOR_CONSTRUCT") >> gm::symlist{
                                                                gm::nt("TYPEOF"),
                                                            } >>
                    WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("TYPEOF") >> gm::symlist{gm::te(gm::ttype::l_typeof), gm::te(gm::ttype::l_left_brackets), gm::nt("RIGHT"), gm::te(gm::ttype::l_right_brackets)} >> WO_ASTBUILDER_INDEX(ast::pass_typeof),
                gm::nt("TYPEOF") >> gm::symlist{gm::te(gm::ttype::l_typeof), gm::te(gm::ttype::l_template_using_begin), gm::nt("TYPE"), gm::te(gm::ttype::l_larg)} >> WO_ASTBUILDER_INDEX(ast::pass_direct<2>),
                gm::nt("TYPEOF") >> gm::symlist{gm::te(gm::ttype::l_typeof), gm::te(gm::ttype::l_less), gm::nt("TYPE"), gm::te(gm::ttype::l_larg)} >> WO_ASTBUILDER_INDEX(ast::pass_direct<2>),
                gm::nt("ORIGIN_TYPE") >> gm::symlist{gm::nt("TUPLE_TYPE_LIST")} >> WO_ASTBUILDER_INDEX(ast::pass_type_tuple),
                gm::nt("TUPLE_TYPE_LIST") >> gm::symlist{gm::te(gm::ttype::l_left_brackets), gm::nt("TUPLE_TYPE_LIST_ITEMS_MAY_EMPTY"), gm::te(gm::ttype::l_right_brackets)} >> WO_ASTBUILDER_INDEX(ast::pass_direct<1>),
                gm::nt("TUPLE_TYPE_LIST_ITEMS_MAY_EMPTY") >> gm::symlist{gm::nt("TUPLE_TYPE_LIST_ITEMS"), gm::nt("COMMA_MAY_EMPTY")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("TUPLE_TYPE_LIST_ITEMS_MAY_EMPTY") >> gm::symlist{gm::nt("VARIADIC_MAY_EMPTY")} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),
                gm::nt("TUPLE_TYPE_LIST_ITEMS") >> gm::symlist{gm::nt("TYPE")} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),
                gm::nt("TUPLE_TYPE_LIST_ITEMS") >> gm::symlist{gm::nt("TUPLE_TYPE_LIST_ITEMS"), gm::te(gm::ttype::l_comma), gm::nt("TYPE")} >> WO_ASTBUILDER_INDEX(ast::pass_append_list<2, 0>),
                gm::nt("TUPLE_TYPE_LIST_ITEMS") >> gm::symlist{gm::nt("TUPLE_TYPE_LIST_ITEMS"), gm::te(gm::ttype::l_comma), gm::nt("VARIADIC_MAY_EMPTY")} >> WO_ASTBUILDER_INDEX(ast::pass_append_list<2, 0>),
                gm::nt("VARIADIC_MAY_EMPTY") >> gm::symlist{gm::te(gm::ttype::l_variadic_sign)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                gm::nt("VARIADIC_MAY_EMPTY") >> gm::symlist{gm::te(gm::ttype::l_empty)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                //////////////////////////////////////////////////////////////////////////////////////////////
                gm::nt("ARGDEFINE") >> gm::symlist{gm::te(gm::ttype::l_empty)} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),
                gm::nt("ARGDEFINE") >> gm::symlist{gm::nt("ARGDEFINE_VAR_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),
                gm::nt("ARGDEFINE") >> gm::symlist{gm::nt("ARGDEFINE"), gm::te(gm::ttype::l_comma), gm::nt("ARGDEFINE_VAR_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_append_list<2, 0>),
                gm::nt("ARGDEFINE_VAR_ITEM") >> gm::symlist{gm::nt("DEFINE_PATTERN"), gm::nt("TYPE_DECL_MAY_EMPTY")} >> WO_ASTBUILDER_INDEX(ast::pass_func_argument),
                gm::nt("ARGDEFINE_VAR_ITEM") >> gm::symlist{gm::te(gm::ttype::l_variadic_sign)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                gm::nt("TYPE_DECL_MAY_EMPTY") >> gm::symlist{gm::nt("TYPE_DECLEAR")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("TYPE_DECL_MAY_EMPTY") >> gm::symlist{gm::te(gm::ttype::l_empty)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                gm::nt("LOGICAL_OR") >> gm::symlist{gm::nt("LOGICAL_AND")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("LOGICAL_OR") >> gm::symlist{gm::nt("LOGICAL_OR"), gm::te(gm::ttype::l_lor), gm::nt("LOGICAL_AND")} >> WO_ASTBUILDER_INDEX(ast::pass_binary_operation),
                gm::nt("LOGICAL_AND") >> gm::symlist{gm::nt("EQUATION")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("LOGICAL_AND") >> gm::symlist{gm::nt("LOGICAL_AND"), gm::te(gm::ttype::l_land), gm::nt("EQUATION")} >> WO_ASTBUILDER_INDEX(ast::pass_binary_operation),
                gm::nt("EQUATION") >> gm::symlist{gm::nt("RELATION")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("EQUATION") >> gm::symlist{gm::nt("EQUATION"), gm::te(gm::ttype::l_equal), gm::nt("RELATION")} >> WO_ASTBUILDER_INDEX(ast::pass_binary_operation),
                gm::nt("EQUATION") >> gm::symlist{gm::nt("EQUATION"), gm::te(gm::ttype::l_not_equal), gm::nt("RELATION")} >> WO_ASTBUILDER_INDEX(ast::pass_binary_operation),
                gm::nt("RELATION") >> gm::symlist{gm::nt("SUMMATION")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("RELATION") >> gm::symlist{gm::nt("RELATION"), gm::te(gm::ttype::l_larg), gm::nt("SUMMATION")} >> WO_ASTBUILDER_INDEX(ast::pass_binary_operation),
                gm::nt("RELATION") >> gm::symlist{gm::nt("RELATION"), gm::te(gm::ttype::l_less), gm::nt("SUMMATION")} >> WO_ASTBUILDER_INDEX(ast::pass_binary_operation),
                gm::nt("RELATION") >> gm::symlist{gm::nt("RELATION"), gm::te(gm::ttype::l_less_or_equal), gm::nt("SUMMATION")} >> WO_ASTBUILDER_INDEX(ast::pass_binary_operation),
                gm::nt("RELATION") >> gm::symlist{gm::nt("RELATION"), gm::te(gm::ttype::l_larg_or_equal), gm::nt("SUMMATION")} >> WO_ASTBUILDER_INDEX(ast::pass_binary_operation),
                gm::nt("SUMMATION") >> gm::symlist{gm::nt("MULTIPLICATION")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("SUMMATION") >> gm::symlist{gm::nt("SUMMATION"), gm::te(gm::ttype::l_add), gm::nt("MULTIPLICATION")} >> WO_ASTBUILDER_INDEX(ast::pass_binary_operation),
                gm::nt("SUMMATION") >> gm::symlist{gm::nt("SUMMATION"), gm::te(gm::ttype::l_sub), gm::nt("MULTIPLICATION")} >> WO_ASTBUILDER_INDEX(ast::pass_binary_operation),
                gm::nt("MULTIPLICATION") >> gm::symlist{gm::nt("SINGLE_VALUE")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("MULTIPLICATION") >> gm::symlist{gm::nt("MULTIPLICATION"), gm::te(gm::ttype::l_mul), gm::nt("SINGLE_VALUE")} >> WO_ASTBUILDER_INDEX(ast::pass_binary_operation),
                gm::nt("MULTIPLICATION") >> gm::symlist{gm::nt("MULTIPLICATION"), gm::te(gm::ttype::l_div), gm::nt("SINGLE_VALUE")} >> WO_ASTBUILDER_INDEX(ast::pass_binary_operation),
                gm::nt("MULTIPLICATION") >> gm::symlist{gm::nt("MULTIPLICATION"), gm::te(gm::ttype::l_mod), gm::nt("SINGLE_VALUE")} >> WO_ASTBUILDER_INDEX(ast::pass_binary_operation),
                gm::nt("SINGLE_VALUE") >> gm::symlist{gm::nt("UNARIED_FACTOR")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("UNARIED_FACTOR") >> gm::symlist{gm::nt("FACTOR_TYPE_CASTING")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("UNARIED_FACTOR") >> gm::symlist{gm::te(gm::ttype::l_sub), gm::nt("UNARIED_FACTOR")} >> WO_ASTBUILDER_INDEX(ast::pass_unary_operation),
                gm::nt("UNARIED_FACTOR") >> gm::symlist{gm::te(gm::ttype::l_lnot), gm::nt("UNARIED_FACTOR")} >> WO_ASTBUILDER_INDEX(ast::pass_unary_operation),
                gm::nt("UNARIED_FACTOR") >> gm::symlist{gm::nt("INV_FUNCTION_CALL")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("FACTOR_TYPE_CASTING") >> gm::symlist{gm::nt("FACTOR_TYPE_CASTING"), gm::nt("AS_TYPE")} >> WO_ASTBUILDER_INDEX(ast::pass_check_type_as),
                gm::nt("FACTOR_TYPE_CASTING") >> gm::symlist{gm::nt("FACTOR_TYPE_CASTING"), gm::nt("IS_TYPE")} >> WO_ASTBUILDER_INDEX(ast::pass_check_type_is),
                gm::nt("AS_TYPE") >> gm::symlist{gm::te(gm::ttype::l_as), gm::nt("TYPE")} >> WO_ASTBUILDER_INDEX(ast::pass_direct_keep_source_location<1>),
                gm::nt("IS_TYPE") >> gm::symlist{gm::te(gm::ttype::l_is), gm::nt("TYPE")} >> WO_ASTBUILDER_INDEX(ast::pass_direct_keep_source_location<1>),

                gm::nt("FACTOR_TYPE_CASTING") >> gm::symlist{gm::nt("FACTOR_TYPE_CASTING"), gm::nt("TYPE_DECLEAR")} >> WO_ASTBUILDER_INDEX(ast::pass_cast_type),
                gm::nt("FACTOR_TYPE_CASTING") >> gm::symlist{gm::nt("FACTOR")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("FACTOR") >> gm::symlist{gm::nt("LEFT")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("FACTOR") >> gm::symlist{gm::te(gm::ttype::l_left_brackets), gm::nt("RIGHT"), gm::te(gm::ttype::l_right_brackets)} >> WO_ASTBUILDER_INDEX(ast::pass_direct<1>),
                gm::nt("FACTOR") >> gm::symlist{gm::nt("UNIT")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("RIGHT") >> gm::symlist{gm::nt("ARGUMENT_EXPAND")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("ARGUMENT_EXPAND") >> gm::symlist{gm::nt("FACTOR_TYPE_CASTING"), gm::te(gm::ttype::l_variadic_sign)} >> WO_ASTBUILDER_INDEX(ast::pass_expand_arguments),
                gm::nt("UNIT") >> gm::symlist{gm::te(gm::ttype::l_variadic_sign)} >> WO_ASTBUILDER_INDEX(ast::pass_variadic_arguments_pack),
                gm::nt("UNIT") >> gm::symlist{gm::te(gm::ttype::l_literal_integer)} >> WO_ASTBUILDER_INDEX(ast::pass_literal),
                gm::nt("UNIT") >> gm::symlist{gm::te(gm::ttype::l_literal_real)} >> WO_ASTBUILDER_INDEX(ast::pass_literal),
                gm::nt("UNIT") >> gm::symlist{gm::te(gm::ttype::l_literal_string)} >> WO_ASTBUILDER_INDEX(ast::pass_literal),
                gm::nt("UNIT") >> gm::symlist{gm::nt("LITERAL_CHAR")} >> WO_ASTBUILDER_INDEX(ast::pass_literal_char),
                gm::nt("UNIT") >> gm::symlist{gm::te(gm::ttype::l_literal_handle)} >> WO_ASTBUILDER_INDEX(ast::pass_literal),
                gm::nt("UNIT") >> gm::symlist{gm::te(gm::ttype::l_nil)} >> WO_ASTBUILDER_INDEX(ast::pass_literal),
                gm::nt("UNIT") >> gm::symlist{gm::te(gm::ttype::l_true)} >> WO_ASTBUILDER_INDEX(ast::pass_literal),
                gm::nt("UNIT") >> gm::symlist{gm::te(gm::ttype::l_false)} >> WO_ASTBUILDER_INDEX(ast::pass_literal),
                gm::nt("UNIT") >> gm::symlist{gm::te(gm::ttype::l_typeid), gm::te(gm::ttype::l_template_using_begin), gm::nt("TYPE"), gm::te(gm::ttype::l_larg)} >> WO_ASTBUILDER_INDEX(ast::pass_typeid),
                gm::nt("LITERAL_CHAR") >> gm::symlist{gm::te(gm::ttype::l_literal_char)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                gm::nt("UNIT") >> gm::symlist{gm::nt("FORMAT_STRING")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("FORMAT_STRING") >> gm::symlist{gm::nt("LITERAL_FORMAT_STRING_BEGIN"), gm::nt("FORMAT_STRING_LIST"), gm::nt("LITERAL_FORMAT_STRING_END")} >> WO_ASTBUILDER_INDEX(ast::pass_format_finish),
                gm::nt("FORMAT_STRING_LIST") >> gm::symlist{gm::nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_format_cast_string),
                gm::nt("FORMAT_STRING_LIST") >> gm::symlist{gm::nt("FORMAT_STRING_LIST"), gm::nt("LITERAL_FORMAT_STRING"), gm::nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_format_connect),
                gm::nt("LITERAL_FORMAT_STRING_BEGIN") >> gm::symlist{gm::te(gm::ttype::l_format_string_begin)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                gm::nt("LITERAL_FORMAT_STRING") >> gm::symlist{gm::te(gm::ttype::l_format_string)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                gm::nt("LITERAL_FORMAT_STRING_END") >> gm::symlist{gm::te(gm::ttype::l_format_string_end)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                gm::nt("UNIT") >> gm::symlist{gm::nt("CONSTANT_MAP")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("UNIT") >> gm::symlist{gm::nt("CONSTANT_ARRAY")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("CONSTANT_ARRAY") >> gm::symlist{gm::te(gm::ttype::l_index_begin), gm::nt("CONSTANT_ARRAY_ITEMS"), gm::te(gm::ttype::l_index_end)} >> WO_ASTBUILDER_INDEX(ast::pass_array_instance),
                gm::nt("CONSTANT_ARRAY") >> gm::symlist{
                                                 gm::te(gm::ttype::l_index_begin),
                                                 gm::nt("CONSTANT_ARRAY_ITEMS"),
                                                 gm::te(gm::ttype::l_index_end),
                                                 gm::te(gm::ttype::l_mut),
                                             } >>
                    WO_ASTBUILDER_INDEX(ast::pass_vec_instance),
                gm::nt("CONSTANT_ARRAY_ITEMS") >> gm::symlist{gm::nt("COMMA_EXPR")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                //////////////////////
                gm::nt("UNIT") >> gm::symlist{gm::nt("CONSTANT_TUPLE")} >> WO_ASTBUILDER_INDEX(ast::pass_tuple_instance),
                // NOTE: Avoid grammar conflict.
                gm::nt("CONSTANT_TUPLE") >> gm::symlist{gm::te(gm::ttype::l_left_brackets), gm::nt("COMMA_MAY_EMPTY"), gm::te(gm::ttype::l_right_brackets)} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<1>),
                gm::nt("CONSTANT_TUPLE") >> gm::symlist{gm::te(gm::ttype::l_left_brackets), gm::nt("RIGHT"), gm::te(gm::ttype::l_comma), gm::nt("COMMA_EXPR"), gm::te(gm::ttype::l_right_brackets)} >> WO_ASTBUILDER_INDEX(ast::pass_append_list<1, 3>),
                ///////////////////////
                gm::nt("CONSTANT_MAP") >> gm::symlist{gm::te(gm::ttype::l_left_curly_braces), gm::nt("CONSTANT_MAP_PAIRS"), gm::te(gm::ttype::l_right_curly_braces)} >> WO_ASTBUILDER_INDEX(ast::pass_dict_instance),
                gm::nt("CONSTANT_MAP") >> gm::symlist{
                                               gm::te(gm::ttype::l_left_curly_braces),
                                               gm::nt("CONSTANT_MAP_PAIRS"),
                                               gm::te(gm::ttype::l_right_curly_braces),
                                               gm::te(gm::ttype::l_mut),
                                           } >>
                    WO_ASTBUILDER_INDEX(ast::pass_map_instance),
                gm::nt("CONSTANT_MAP_PAIRS") >> gm::symlist{gm::nt("CONSTANT_MAP_PAIR")} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),
                gm::nt("CONSTANT_MAP_PAIRS") >> gm::symlist{gm::nt("CONSTANT_MAP_PAIRS"), gm::te(gm::ttype::l_comma), gm::nt("CONSTANT_MAP_PAIR")} >> WO_ASTBUILDER_INDEX(ast::pass_append_list<2, 0>),
                gm::nt("CONSTANT_MAP_PAIR") >> gm::symlist{gm::te(gm::ttype::l_empty)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                gm::nt("CONSTANT_MAP_PAIR") >> gm::symlist{gm::nt("CONSTANT_ARRAY"), gm::te(gm::ttype::l_assign), gm::nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_dict_field_init_pair),
                gm::nt("CALLABLE_LEFT") >> gm::symlist{gm::nt("SCOPED_IDENTIFIER_FOR_VAL")} >> WO_ASTBUILDER_INDEX(ast::pass_variable),
                gm::nt("CALLABLE_RIGHT_WITH_BRACKET") >> gm::symlist{gm::te(gm::ttype::l_left_brackets), gm::nt("RIGHT"), gm::te(gm::ttype::l_right_brackets)} >> WO_ASTBUILDER_INDEX(ast::pass_direct<1>),
                gm::nt("SCOPED_IDENTIFIER_FOR_VAL") >> gm::symlist{gm::nt("SCOPED_LIST_TYPEOF"), gm::nt("MAY_EMPTY_LEFT_TEMPLATE_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_build_identifier_typeof),
                gm::nt("SCOPED_IDENTIFIER_FOR_VAL") >> gm::symlist{gm::nt("SCOPED_LIST_NORMAL"), gm::nt("MAY_EMPTY_LEFT_TEMPLATE_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_build_identifier_normal),
                gm::nt("SCOPED_IDENTIFIER_FOR_VAL") >> gm::symlist{gm::nt("SCOPED_LIST_GLOBAL"), gm::nt("MAY_EMPTY_LEFT_TEMPLATE_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_build_identifier_global),
                gm::nt("SCOPED_IDENTIFIER_FOR_TYPE") >> gm::symlist{gm::nt("SCOPED_LIST_TYPEOF"), gm::nt("MAY_EMPTY_TEMPLATE_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_build_identifier_typeof),
                gm::nt("SCOPED_IDENTIFIER_FOR_TYPE") >> gm::symlist{gm::nt("SCOPED_LIST_NORMAL"), gm::nt("MAY_EMPTY_TEMPLATE_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_build_identifier_normal),
                gm::nt("SCOPED_IDENTIFIER_FOR_TYPE") >> gm::symlist{gm::nt("SCOPED_LIST_GLOBAL"), gm::nt("MAY_EMPTY_TEMPLATE_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_build_identifier_global),
                gm::nt("SCOPED_LIST_TYPEOF") >> gm::symlist{gm::nt("TYPEOF"), gm::nt("SCOPING_LIST")} >> WO_ASTBUILDER_INDEX(ast::pass_append_list<0, 1>),
                gm::nt("SCOPED_LIST_NORMAL") >> gm::symlist{gm::nt("AST_TOKEN_IDENTIFER"), gm::nt("SCOPING_LIST")} >> WO_ASTBUILDER_INDEX(ast::pass_append_list<0, 1>),
                gm::nt("SCOPED_LIST_NORMAL") >> gm::symlist{
                                                     gm::nt("AST_TOKEN_IDENTIFER"),
                                                 } >>
                    WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),
                gm::nt("SCOPED_LIST_GLOBAL") >> gm::symlist{gm::nt("SCOPING_LIST")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("SCOPING_LIST") >> gm::symlist{gm::te(gm::ttype::l_scopeing), gm::nt("AST_TOKEN_IDENTIFER")} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<1>), // TODO HERE SHOULD BE IDENTIF IN NAMESPACE
                gm::nt("SCOPING_LIST") >> gm::symlist{gm::te(gm::ttype::l_scopeing), gm::nt("AST_TOKEN_IDENTIFER"), gm::nt("SCOPING_LIST")} >> WO_ASTBUILDER_INDEX(ast::pass_append_list<1, 2>),
                gm::nt("LEFT") >> gm::symlist{gm::nt("CALLABLE_LEFT")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("LEFT") >> gm::symlist{gm::nt("FACTOR_TYPE_CASTING"), gm::te(gm::ttype::l_index_begin), gm::nt("RIGHT"), gm::te(gm::ttype::l_index_end)} >> WO_ASTBUILDER_INDEX(ast::pass_index_operation_regular),
                gm::nt("LEFT") >> gm::symlist{gm::nt("FACTOR_TYPE_CASTING"), gm::te(gm::ttype::l_index_point), gm::nt("INDEX_POINT_TARGET")} >> WO_ASTBUILDER_INDEX(ast::pass_index_operation_member),
                gm::nt("INDEX_POINT_TARGET") >> gm::symlist{gm::nt("IDENTIFIER")} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                gm::nt("INDEX_POINT_TARGET") >> gm::symlist{gm::te(gm::ttype::l_literal_integer)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                gm::nt("FACTOR") >> gm::symlist{gm::nt("FUNCTION_CALL")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("DIRECT_CALLABLE_TARGET") >> gm::symlist{gm::nt("CALLABLE_LEFT")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("DIRECT_CALLABLE_TARGET") >> gm::symlist{gm::nt("CALLABLE_RIGHT_WITH_BRACKET")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("DIRECT_CALLABLE_TARGET") >> gm::symlist{gm::nt("FUNC_DEFINE")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("DIRECT_CALL_FIRST_ARG") >> gm::symlist{gm::nt("FACTOR_TYPE_CASTING")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("DIRECT_CALL_FIRST_ARG") >> gm::symlist{gm::nt("ARGUMENT_EXPAND")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                // MONAD GRAMMAR~~~ SUGAR!
                gm::nt("FACTOR") >> gm::symlist{gm::nt("FACTOR_TYPE_CASTING"), gm::te(gm::ttype::l_bind_monad), gm::nt("CALLABLE_LEFT")} >> WO_ASTBUILDER_INDEX(ast::pass_build_bind_monad),
                gm::nt("FACTOR") >> gm::symlist{gm::nt("FACTOR_TYPE_CASTING"), gm::te(gm::ttype::l_bind_monad), gm::nt("CALLABLE_RIGHT_WITH_BRACKET")} >> WO_ASTBUILDER_INDEX(ast::pass_build_bind_monad),
                gm::nt("FACTOR") >> gm::symlist{gm::nt("FACTOR_TYPE_CASTING"), gm::te(gm::ttype::l_bind_monad), gm::nt("FUNC_DEFINE")} >> WO_ASTBUILDER_INDEX(ast::pass_build_bind_monad),
                gm::nt("FACTOR") >> gm::symlist{gm::nt("FACTOR_TYPE_CASTING"), gm::te(gm::ttype::l_map_monad), gm::nt("CALLABLE_LEFT")} >> WO_ASTBUILDER_INDEX(ast::pass_build_map_monad),
                gm::nt("FACTOR") >> gm::symlist{gm::nt("FACTOR_TYPE_CASTING"), gm::te(gm::ttype::l_map_monad), gm::nt("CALLABLE_RIGHT_WITH_BRACKET")} >> WO_ASTBUILDER_INDEX(ast::pass_build_map_monad),
                gm::nt("FACTOR") >> gm::symlist{gm::nt("FACTOR_TYPE_CASTING"), gm::te(gm::ttype::l_map_monad), gm::nt("FUNC_DEFINE")} >> WO_ASTBUILDER_INDEX(ast::pass_build_map_monad),
                gm::nt("ARGUMENT_LISTS") >> gm::symlist{gm::te(gm::ttype::l_left_brackets), gm::nt("COMMA_EXPR"), gm::te(gm::ttype::l_right_brackets)} >> WO_ASTBUILDER_INDEX(ast::pass_direct<1>),
                gm::nt("ARGUMENT_LISTS_MAY_EMPTY") >> gm::symlist{gm::nt("ARGUMENT_LISTS")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("ARGUMENT_LISTS_MAY_EMPTY") >> gm::symlist{gm::te(gm::ttype::l_empty)} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),
                gm::nt("FUNCTION_CALL") >> gm::symlist{gm::nt("FACTOR"), gm::nt("ARGUMENT_LISTS")} >> WO_ASTBUILDER_INDEX(ast::pass_normal_function_call),
                gm::nt("FUNCTION_CALL") >> gm::symlist{gm::nt("DIRECT_CALLABLE_VALUE"), gm::nt("ARGUMENT_LISTS_MAY_EMPTY")} >> WO_ASTBUILDER_INDEX(ast::pass_directly_function_call_append_arguments),
                gm::nt("DIRECT_CALLABLE_VALUE") >> gm::symlist{gm::nt("DIRECT_CALL_FIRST_ARG"), gm::te(gm::ttype::l_direct), gm::nt("DIRECT_CALLABLE_TARGET")} >> WO_ASTBUILDER_INDEX(ast::pass_directly_function_call),
                gm::nt("INV_DIRECT_CALL_FIRST_ARG") >> gm::symlist{gm::nt("SINGLE_VALUE")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("INV_DIRECT_CALL_FIRST_ARG") >> gm::symlist{gm::nt("ARGUMENT_EXPAND")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("INV_FUNCTION_CALL") >> gm::symlist{gm::nt("FUNCTION_CALL"), gm::te(gm::ttype::l_inv_direct), gm::nt("INV_DIRECT_CALL_FIRST_ARG")} >> WO_ASTBUILDER_INDEX(ast::pass_inverse_function_call),
                gm::nt("INV_FUNCTION_CALL") >> gm::symlist{gm::nt("DIRECT_CALLABLE_TARGET"), gm::te(gm::ttype::l_inv_direct), gm::nt("INV_DIRECT_CALL_FIRST_ARG")} >> WO_ASTBUILDER_INDEX(ast::pass_inverse_function_call),
                gm::nt("COMMA_EXPR") >> gm::symlist{gm::nt("COMMA_EXPR_ITEMS"), gm::nt("COMMA_MAY_EMPTY")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("COMMA_EXPR") >> gm::symlist{gm::nt("COMMA_MAY_EMPTY")} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),
                gm::nt("COMMA_EXPR_ITEMS") >> gm::symlist{gm::nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),
                gm::nt("COMMA_EXPR_ITEMS") >> gm::symlist{gm::nt("COMMA_EXPR_ITEMS"), gm::te(gm::ttype::l_comma), gm::nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_append_list<2, 0>),
                gm::nt("COMMA_MAY_EMPTY") >> gm::symlist{gm::te(gm::ttype::l_comma)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                gm::nt("COMMA_MAY_EMPTY") >> gm::symlist{gm::te(gm::ttype::l_empty)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                gm::nt("SEMICOLON_MAY_EMPTY") >> gm::symlist{gm::te(gm::ttype::l_semicolon)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                gm::nt("SEMICOLON_MAY_EMPTY") >> gm::symlist{gm::te(gm::ttype::l_empty)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                gm::nt("MAY_EMPTY_TEMPLATE_ITEM") >> gm::symlist{gm::nt("TYPE_TEMPLATE_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("MAY_EMPTY_TEMPLATE_ITEM") >> gm::symlist{gm::te(gm::ttype::l_empty)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                gm::nt("MAY_EMPTY_LEFT_TEMPLATE_ITEM") >> gm::symlist{gm::te(gm::ttype::l_empty)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                gm::nt("MAY_EMPTY_LEFT_TEMPLATE_ITEM") >> gm::symlist{gm::nt("LEFT_TEMPLATE_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("LEFT_TEMPLATE_ITEM") >> gm::symlist{gm::te(gm::ttype::l_template_using_begin), gm::nt("TEMPLATE_ARGUMENT_LIST"), gm::te(gm::ttype::l_larg)} >> WO_ASTBUILDER_INDEX(ast::pass_direct<1>),
                gm::nt("TYPE_TEMPLATE_ITEM") >> gm::symlist{gm::te(gm::ttype::l_less), gm::nt("TEMPLATE_ARGUMENT_LIST"), gm::te(gm::ttype::l_larg)} >> WO_ASTBUILDER_INDEX(ast::pass_direct<1>),
                // Template for type can be :< ... >
                gm::nt("TYPE_TEMPLATE_ITEM") >> gm::symlist{gm::nt("LEFT_TEMPLATE_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("TEMPLATE_ARGUMENT_LIST") >> gm::symlist{gm::nt("TEMPLATE_ARGUMENT_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),
                gm::nt("TEMPLATE_ARGUMENT_LIST") >> gm::symlist{gm::nt("TEMPLATE_ARGUMENT_LIST"), gm::te(gm::ttype::l_comma), gm::nt("TEMPLATE_ARGUMENT_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_append_list<2, 0>),
                gm::nt("TEMPLATE_ARGUMENT_ITEM") >> gm::symlist{gm::nt("TYPE")} >> WO_ASTBUILDER_INDEX(ast::pass_create_template_argument),
                gm::nt("TEMPLATE_ARGUMENT_ITEM") >> gm::symlist{gm::te(gm::ttype::l_left_curly_braces), gm::nt("RIGHT"), gm::te(gm::ttype::l_right_curly_braces)} >> WO_ASTBUILDER_INDEX(ast::pass_create_template_argument),
                gm::nt("DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY") >> gm::symlist{gm::te(gm::ttype::l_empty)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                gm::nt("DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY") >> gm::symlist{gm::nt("DEFINE_TEMPLATE_PARAM_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("DEFINE_TEMPLATE_PARAM_ITEM") >> gm::symlist{gm::te(gm::ttype::l_less), gm::nt("DEFINE_TEMPLATE_PARAM_LIST"), gm::te(gm::ttype::l_larg)} >> WO_ASTBUILDER_INDEX(ast::pass_direct<1>),
                gm::nt("DEFINE_TEMPLATE_PARAM_LIST") >> gm::symlist{gm::nt("DEFINE_TEMPLATE_PARAM")} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),
                gm::nt("DEFINE_TEMPLATE_PARAM_LIST") >> gm::symlist{gm::nt("DEFINE_TEMPLATE_PARAM_LIST"), gm::te(gm::ttype::l_comma), gm::nt("DEFINE_TEMPLATE_PARAM")} >> WO_ASTBUILDER_INDEX(ast::pass_append_list<2, 0>),
                gm::nt("DEFINE_TEMPLATE_PARAM") >> gm::symlist{gm::nt("IDENTIFIER")} >> WO_ASTBUILDER_INDEX(ast::pass_create_template_param),
                gm::nt("DEFINE_TEMPLATE_PARAM") >> gm::symlist{gm::nt("IDENTIFIER"), gm::te(gm::ttype::l_typecast), gm::nt("TYPE")} >> WO_ASTBUILDER_INDEX(ast::pass_create_template_param),
                gm::nt("SENTENCE") >> gm::symlist{gm::nt("DECL_UNION")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("DECL_UNION") >> gm::symlist{gm::nt("DECL_ATTRIBUTE"), gm::te(gm::ttype::l_union), gm::nt("AST_TOKEN_IDENTIFER"), gm::nt("DEFINE_TEMPLATE_PARAM_ITEM_MAY_EMPTY"), gm::te(gm::ttype::l_left_curly_braces), gm::nt("UNION_ITEMS"), gm::te(gm::ttype::l_right_curly_braces)} >> WO_ASTBUILDER_INDEX(ast::pass_union_declare),
                gm::nt("UNION_ITEMS") >> gm::symlist{gm::nt("UNION_ITEM_LIST"), gm::nt("COMMA_MAY_EMPTY")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("UNION_ITEM_LIST") >> gm::symlist{gm::nt("UNION_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),
                gm::nt("UNION_ITEM_LIST") >> gm::symlist{gm::nt("UNION_ITEM_LIST"), gm::te(gm::ttype::l_comma), gm::nt("UNION_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_append_list<2, 0>),
                gm::nt("UNION_ITEM") >> gm::symlist{gm::nt("IDENTIFIER")} >> WO_ASTBUILDER_INDEX(ast::pass_union_item),
                gm::nt("UNION_ITEM") >> gm::symlist{gm::nt("IDENTIFIER"), gm::te(gm::ttype::l_left_brackets), gm::nt("TYPE"), gm::te(gm::ttype::l_right_brackets)} >> WO_ASTBUILDER_INDEX(ast::pass_union_item_constructor),
                gm::nt("SENTENCE") >> gm::symlist{gm::nt("MATCH_BLOCK")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("MATCH_BLOCK") >> gm::symlist{gm::te(gm::ttype::l_match), gm::te(gm::ttype::l_left_brackets), gm::nt("EXPRESSION"), gm::te(gm::ttype::l_right_brackets), gm::te(gm::ttype::l_left_curly_braces), gm::nt("MATCH_CASES"), gm::te(gm::ttype::l_right_curly_braces)} >> WO_ASTBUILDER_INDEX(ast::pass_match),
                gm::nt("MATCH_CASES") >> gm::symlist{gm::nt("MATCH_CASE")} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),
                gm::nt("MATCH_CASES") >> gm::symlist{gm::nt("MATCH_CASES"), gm::nt("MATCH_CASE")} >> WO_ASTBUILDER_INDEX(ast::pass_append_list<1, 0>),
                gm::nt("MATCH_CASE") >> gm::symlist{gm::nt("PATTERN_UNION_CASE"), gm::te(gm::ttype::l_question), gm::nt("BLOCKED_SENTENCE")} >> WO_ASTBUILDER_INDEX(ast::pass_match_union_case),
                // PATTERN-CASE MAY BE A SINGLE-VARIABLE/TUPLE/STRUCT...
                gm::nt("PATTERN_UNION_CASE") >> gm::symlist{gm::nt("IDENTIFIER")} >> WO_ASTBUILDER_INDEX(ast::pass_union_pattern_identifier_or_takeplace),
                // PATTERN-CASE MAY BE A UNION
                gm::nt("PATTERN_UNION_CASE") >> gm::symlist{gm::nt("IDENTIFIER"), gm::te(gm::ttype::l_left_brackets), gm::nt("DEFINE_PATTERN"), gm::te(gm::ttype::l_right_brackets)} >> WO_ASTBUILDER_INDEX(ast::pass_union_pattern_contain_element),
                //////////////////////////////////////////////////////////////////////////////////////
                gm::nt("UNIT") >> gm::symlist{
                                       gm::nt("STRUCT_INSTANCE_BEGIN"), // Here we use Callable left stand for type. so we cannot support x here...
                                       gm::te(gm::ttype::l_left_curly_braces),
                                       gm::nt("STRUCT_MEMBER_INITS"),
                                       gm::te(gm::ttype::l_right_curly_braces),
                                   } >>
                    WO_ASTBUILDER_INDEX(ast::pass_struct_instance),
                gm::nt("STRUCT_INSTANCE_BEGIN") >> gm::symlist{gm::nt("STRUCTABLE_TYPE_FOR_CONSTRUCT")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("STRUCT_INSTANCE_BEGIN") >> gm::symlist{gm::te(gm::ttype::l_struct)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                gm::nt("STRUCT_MEMBER_INITS") >> gm::symlist{gm::nt("STRUCT_MEMBER_INITS_EMPTY")} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),
                gm::nt("STRUCT_MEMBER_INITS_EMPTY") >> gm::symlist{gm::te(gm::ttype::l_empty)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                gm::nt("STRUCT_MEMBER_INITS_EMPTY") >> gm::symlist{gm::te(gm::ttype::l_comma)} >> WO_ASTBUILDER_INDEX(ast::pass_empty),
                gm::nt("STRUCT_MEMBER_INITS") >> gm::symlist{gm::nt("STRUCT_MEMBERS_INIT_LIST"), gm::nt("COMMA_MAY_EMPTY")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("STRUCT_MEMBERS_INIT_LIST") >> gm::symlist{gm::nt("STRUCT_MEMBER_INIT_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),
                gm::nt("STRUCT_MEMBERS_INIT_LIST") >> gm::symlist{gm::nt("STRUCT_MEMBERS_INIT_LIST"), gm::te(gm::ttype::l_comma), gm::nt("STRUCT_MEMBER_INIT_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_append_list<2, 0>),
                gm::nt("STRUCT_MEMBER_INIT_ITEM") >> gm::symlist{gm::nt("IDENTIFIER"), gm::te(gm::ttype::l_assign), gm::nt("RIGHT")} >> WO_ASTBUILDER_INDEX(ast::pass_struct_member_init_pair),
                ////////////////////////////////////////////////////
                gm::nt("DEFINE_PATTERN") >> gm::symlist{gm::nt("IDENTIFIER")} >> WO_ASTBUILDER_INDEX(ast::pass_pattern_identifier_or_takepace),
                gm::nt("DEFINE_PATTERN") >> gm::symlist{gm::te(gm::ttype::l_mut), gm::nt("IDENTIFIER")} >> WO_ASTBUILDER_INDEX(ast::pass_pattern_mut_identifier_or_takepace),
                gm::nt("DEFINE_PATTERN_WITH_TEMPLATE") >> gm::symlist{gm::nt("IDENTIFIER"), gm::nt("DEFINE_TEMPLATE_PARAM_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_pattern_identifier_or_takepace_with_template),
                gm::nt("DEFINE_PATTERN_WITH_TEMPLATE") >> gm::symlist{gm::te(gm::ttype::l_mut), gm::nt("IDENTIFIER"), gm::nt("DEFINE_TEMPLATE_PARAM_ITEM")} >> WO_ASTBUILDER_INDEX(ast::pass_pattern_mut_identifier_or_takepace_with_template),
                gm::nt("DEFINE_PATTERN") >> gm::symlist{gm::te(gm::ttype::l_left_brackets), gm::nt("DEFINE_PATTERN_LIST"), gm::te(gm::ttype::l_right_brackets)} >> WO_ASTBUILDER_INDEX(ast::pass_pattern_tuple),
                gm::nt("DEFINE_PATTERN") >> gm::symlist{gm::te(gm::ttype::l_left_brackets), gm::nt("COMMA_MAY_EMPTY"), gm::te(gm::ttype::l_right_brackets)} >> WO_ASTBUILDER_INDEX(ast::pass_pattern_tuple),
                gm::nt("DEFINE_PATTERN_LIST") >> gm::symlist{gm::nt("DEFINE_PATTERN_ITEMS"), gm::nt("COMMA_MAY_EMPTY")} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("DEFINE_PATTERN_ITEMS") >> gm::symlist{gm::nt("DEFINE_PATTERN")} >> WO_ASTBUILDER_INDEX(ast::pass_create_list<0>),
                gm::nt("DEFINE_PATTERN_ITEMS") >> gm::symlist{gm::nt("DEFINE_PATTERN_ITEMS"), gm::te(gm::ttype::l_comma), gm::nt("DEFINE_PATTERN")} >> WO_ASTBUILDER_INDEX(ast::pass_append_list<2, 0>),
                //////////////////////////////////////////////////////////////////////////////////////
                gm::nt("AST_TOKEN_IDENTIFER") >> gm::symlist{gm::nt("IDENTIFIER")} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                gm::nt("AST_TOKEN_NIL") >> gm::symlist{gm::te(gm::ttype::l_nil)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
                gm::nt("IDENTIFIER") >> gm::symlist{gm::te(gm::ttype::l_identifier)} >> WO_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt("USELESS_TOKEN") >> gm::symlist{gm::te(gm::ttype::l_double_index_point)} >> WO_ASTBUILDER_INDEX(ast::pass_useless_token),
                gm::nt("USELESS_TOKEN") >> gm::symlist{gm::te(gm::ttype::l_unknown_token)} >> WO_ASTBUILDER_INDEX(ast::pass_useless_token),
                gm::nt("USELESS_TOKEN") >> gm::symlist{gm::te(gm::ttype::l_macro)} >> WO_ASTBUILDER_INDEX(ast::pass_token),
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
                        cachefile << tab << "(lex_type)" << static_cast<int>(static_cast<lex_type_base_t>(op_te)) << "," << endl;

                    cachefile << "};" << endl;
                }
                ///////////////////////////////////////////////////////////////////////
                // Generate LR(1) action table;

                std::vector<std::pair<int, int>> STATE_GOTO_R_S_INDEX_MAP(
                    grammar_instance->LR1_TABLE.size(), std::pair<int, int>(-1, -1));

                // GOTO : only nt have goto,
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

#undef WO_LSTR
#undef _WO_LSTR
