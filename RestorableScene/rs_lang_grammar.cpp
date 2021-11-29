#include "rs_lang_grammar_loader.hpp"
#include "rs_lang_grammar_lr1_autogen.hpp"
#include "rs_crc_64.hpp"

#include "rs_lang_ast_builder.hpp"

#include <fstream>

#define _RS_LSTR(X) L##X
#define RS_LSTR(X) _RS_LSTR(X)

#define RS_GRAMMAR_SKIP_GEN_LR1_TABLE_CACHE 0

namespace rs
{
    grammar* get_rs_grammar()
    {
        static grammar* rs_grammar = nullptr;

        if (rs_grammar)
            return rs_grammar;

        ast::init_builder();

        std::ifstream this_grammar_file("rs_lang_grammar.cpp");
        uint64_t rs_lang_grammar_crc64 = 0;

        if (this_grammar_file.fail())
        {
#if defined(RS_LANG_GRAMMAR_LR1_AUTO_GENED) && !RS_GRAMMAR_SKIP_GEN_LR1_TABLE_CACHE
            goto load_from_buffer;
#else
            rs_error("Fail to load grammar.");
#endif // RS_LANG_GRAMMAR_LR1_AUTO_GENED

        }
        rs_lang_grammar_crc64 = crc_64(this_grammar_file);

#if defined(RS_LANG_GRAMMAR_LR1_AUTO_GENED) && !RS_GRAMMAR_SKIP_GEN_LR1_TABLE_CACHE

        if (RS_LANG_GRAMMAR_CRC64 == rs_lang_grammar_crc64)
        {
        load_from_buffer:
            rs_grammar = new grammar;

            rs_read_lr1_to(rs_grammar->LR1_TABLE);
            rs_read_follow_set_to(rs_grammar->FOLLOW_SET);
            rs_read_origin_p_to(rs_grammar->ORGIN_P);

            goto register_ast_builder_function;
        }
        else
        {
#endif
            std::cout << ANSI_HIY "RSGramma: " ANSI_RST "Syntax update detected, LR table cache is being regenerating..." << std::endl;

            using gm = rs::grammar;
            rs_grammar = new grammar(
                {
                    //文法定义形如
                    // nt >> list{nt/te ... } [>> ast_create_function]
gm::nt(L"PROGRAM_AUGMENTED") >> gm::symlist{gm::nt(L"PROGRAM")},
gm::nt(L"PROGRAM") >> gm::symlist{gm::nt(L"PARAGRAPH")},

gm::nt(L"PARAGRAPH") >> gm::symlist{gm::nt(L"SENTENCE_LIST")}
>> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),

gm::nt(L"SENTENCE_LIST") >> gm::symlist{gm::nt(L"SENTENCE_LIST"),gm::nt(L"SENTENCE")}
>> RS_ASTBUILDER_INDEX(ast::pass_append_list<1,0>),
gm::nt(L"SENTENCE_LIST") >> gm::symlist{gm::nt(L"SENTENCE")}
>> RS_ASTBUILDER_INDEX(ast::pass_create_list<0>),

gm::nt(L"SENTENCE") >> gm::symlist{gm::te(gm::ttype::l_import), gm::nt(L"IMPORT_NAME"), gm::te(gm::ttype::l_semicolon)},

gm::nt(L"IMPORT_NAME") >> gm::symlist{gm::te(gm::ttype::l_identifier), gm::nt(L"INDEX_IMPORT_NAME")},
gm::nt(L"INDEX_IMPORT_NAME") >> gm::symlist{gm::te(gm::ttype::l_empty)},
gm::nt(L"INDEX_IMPORT_NAME") >> gm::symlist{gm::te(gm::ttype::l_index_point), gm::te(gm::ttype::l_identifier), gm::nt(L"INDEX_IMPORT_NAME")},

gm::nt(L"SENTENCE") >> gm::symlist{gm::nt(L"DECL_NAMESPACE")}
 >> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),

gm::nt(L"DECL_NAMESPACE") >> gm::symlist{gm::te(gm::ttype::l_namespace),gm::te(gm::ttype::l_identifier), gm::nt(L"SENTENCE")}
 >> RS_ASTBUILDER_INDEX(ast::pass_namespace),

gm::nt(L"BLOCKED_SENTENCE") >> gm::symlist{gm::nt(L"SENTENCE")}
 >> RS_ASTBUILDER_INDEX(ast::pass_sentence_block<0>),

gm::nt(L"SENTENCE") >> gm::symlist{gm::nt(L"SENTENCE_BLOCK")}
 >> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),

gm::nt(L"SENTENCE_BLOCK") >> gm::symlist{gm::te(gm::ttype::l_left_curly_braces),gm::nt(L"PARAGRAPH"),gm::te(gm::ttype::l_right_curly_braces)}
 >> RS_ASTBUILDER_INDEX(ast::pass_sentence_block<1>),

                // Because of CONSTANT MAP => ,,, => { l_empty } Following production will cause R-R Conflict
                // gm::nt(L"PARAGRAPH") >> gm::symlist{gm::te(gm::ttype::l_empty)},
                // So, just make a production like this:
gm::nt(L"SENTENCE_BLOCK") >> gm::symlist{gm::te(gm::ttype::l_left_curly_braces),gm::te(gm::ttype::l_right_curly_braces)}
>> RS_ASTBUILDER_INDEX(ast::pass_empty),
// NOTE: Why the production can solve the conflict?
//       A > {}
//       B > {l_empty}
//       In fact, A B have same production, but in rs_lr(1) parser, l_empty have a lower priority then production like A
//       This rule can solve many grammar conflict easily, but in some case, it will cause bug, so please use it carefully.

gm::nt(L"SENTENCE") >> gm::symlist{gm::te(gm::ttype::l_semicolon)}
>> RS_ASTBUILDER_INDEX(ast::pass_empty),

                gm::nt(L"SENTENCE") >> gm::symlist{gm::nt(L"FUNC_DEFINE_WITH_NAME")},
gm::nt(L"FUNC_DEFINE_WITH_NAME") >> gm::symlist{
                        gm::te(gm::ttype::l_func),
                        gm::te(gm::ttype::l_identifier),
                        gm::te(gm::ttype::l_left_brackets),gm::nt(L"ARGDEFINE"),gm::te(gm::ttype::l_right_brackets),
                        gm::nt(L"RETURN_TYPE_DECLEAR"),
                        gm::nt(L"SENTENCE_BLOCK")}
                        >> RS_ASTBUILDER_INDEX(ast::pass_function_define),


                //WHILE语句
gm::nt(L"SENTENCE") >> gm::symlist{gm::nt(L"WHILE")}
 >> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),
gm::nt(L"WHILE") >> gm::symlist{
                        gm::te(gm::ttype::l_while),
                        gm::te(gm::ttype::l_left_brackets),
                        gm::nt(L"EXPRESSION"),
                        gm::te(gm::ttype::l_right_brackets),
                        gm::nt(L"BLOCKED_SENTENCE")
                    },
                //IF语句
gm::nt(L"SENTENCE") >> gm::symlist{gm::nt(L"IF")}
 >> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),
gm::nt(L"IF") >> gm::symlist{
                        gm::te(gm::ttype::l_if),
                        gm::te(gm::ttype::l_left_brackets),
                        gm::nt(L"EXPRESSION"),
                        gm::te(gm::ttype::l_right_brackets),
                        gm::nt(L"BLOCKED_SENTENCE"),
                        gm::nt(L"ELSE")
                    },

gm::nt(L"ELSE") >> gm::symlist{gm::te(gm::ttype::l_empty)}
>> RS_ASTBUILDER_INDEX(ast::pass_empty),
gm::nt(L"ELSE") >> gm::symlist{gm::te(gm::ttype::l_else),gm::nt(L"BLOCKED_SENTENCE")}
 >> RS_ASTBUILDER_INDEX(ast::pass_direct<1>),

//变量定义表达式
gm::nt(L"SENTENCE") >> gm::symlist{
                        gm::te(gm::ttype::l_var),
                        gm::nt(L"VARDEFINE"),
                        gm::te(gm::ttype::l_semicolon)}
>> RS_ASTBUILDER_INDEX(ast::pass_direct<1>),//ASTVariableDefination

gm::nt(L"SENTENCE") >> gm::symlist{
                        gm::te(gm::ttype::l_ref),
                        gm::nt(L"REFDEFINE"),
                        gm::te(gm::ttype::l_semicolon)}
>> RS_ASTBUILDER_INDEX(ast::pass_mark_as_ref_define),//ASTVariableDefination

gm::nt(L"VARDEFINE") >> gm::symlist{gm::te(gm::ttype::l_identifier),
                        gm::te(gm::ttype::l_assign),
                        gm::nt(L"EXPRESSION")}
>> RS_ASTBUILDER_INDEX(ast::pass_begin_varref_define),//ASTVariableDefination

gm::nt(L"VARDEFINE") >> gm::symlist{
                        gm::nt(L"VARDEFINE"),
                        gm::te(gm::ttype::l_comma),
                        gm::te(gm::ttype::l_identifier),
                        gm::te(gm::ttype::l_assign,L"="),
                        gm::nt(L"EXPRESSION")}
>> RS_ASTBUILDER_INDEX(ast::pass_add_varref_define),//ASTVariableDefination

gm::nt(L"REFDEFINE") >> gm::symlist{ gm::te(gm::ttype::l_identifier),
                        gm::te(gm::ttype::l_assign),
                        gm::nt(L"LEFT") }
    >> RS_ASTBUILDER_INDEX(ast::pass_begin_varref_define),//ASTVariableDefination

    gm::nt(L"REFDEFINE") >> gm::symlist{
                            gm::nt(L"VARDEFINE"),
                            gm::te(gm::ttype::l_comma),
                            gm::te(gm::ttype::l_identifier),
                            gm::te(gm::ttype::l_assign,L"="),
                            gm::nt(L"LEFT") }
                            >> RS_ASTBUILDER_INDEX(ast::pass_add_varref_define),//ASTVariableDefination

                                //变量定义表达式
gm::nt(L"SENTENCE") >> gm::symlist{gm::te(gm::ttype::l_return),gm::nt(L"RETNVALUE"),gm::te(gm::ttype::l_semicolon)}
>> RS_ASTBUILDER_INDEX(ast::pass_return),

gm::nt(L"RETNVALUE") >> gm::symlist{gm::te(gm::ttype::l_empty)}
>> RS_ASTBUILDER_INDEX(ast::pass_empty),//返回值可以是空产生式
gm::nt(L"RETNVALUE") >> gm::symlist{gm::nt(L"EXPRESSION")}
>> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),//返回值也可以是一个表达式

//语句与表达式
gm::nt(L"SENTENCE") >> gm::symlist{gm::nt(L"EXPRESSION"),gm::te(gm::ttype::l_semicolon)}
>> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),
//	RIGHT当然是一个表达式
//gm::nt(L"EXPRESSION") >> gm::symlist{gm::nt(L"ASSIGNMENT")},
gm::nt(L"EXPRESSION") >> gm::symlist{gm::nt(L"RIGHT")}
 >> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),
                //gm::nt(L"EXPRESSION") >> gm::symlist{gm::nt(L"ASSIGNMENT")},

gm::nt(L"ASSIGNMENT") >> gm::symlist{ gm::nt(L"LEFT"),gm::te(gm::ttype::l_assign),gm::nt(L"RIGHT") }
>> RS_ASTBUILDER_INDEX(ast::pass_binary_op),
gm::nt(L"ASSIGNMENT") >> gm::symlist{ gm::nt(L"LEFT"),gm::te(gm::ttype::l_add_assign),gm::nt(L"RIGHT") }
>> RS_ASTBUILDER_INDEX(ast::pass_binary_op),
gm::nt(L"ASSIGNMENT") >> gm::symlist{ gm::nt(L"LEFT"),gm::te(gm::ttype::l_sub_assign),gm::nt(L"RIGHT") }
>> RS_ASTBUILDER_INDEX(ast::pass_binary_op),
gm::nt(L"ASSIGNMENT") >> gm::symlist{ gm::nt(L"LEFT"),gm::te(gm::ttype::l_mul_assign),gm::nt(L"RIGHT") }
>> RS_ASTBUILDER_INDEX(ast::pass_binary_op),
gm::nt(L"ASSIGNMENT") >> gm::symlist{ gm::nt(L"LEFT"),gm::te(gm::ttype::l_div_assign),gm::nt(L"RIGHT") }
>> RS_ASTBUILDER_INDEX(ast::pass_binary_op),
gm::nt(L"ASSIGNMENT") >> gm::symlist{ gm::nt(L"LEFT"),gm::te(gm::ttype::l_mod_assign),gm::nt(L"RIGHT") }
>> RS_ASTBUILDER_INDEX(ast::pass_binary_op),

//右值是仅取值的表达式 ，赋值语句当然也是一个右值		

gm::nt(L"RIGHT") >> gm::symlist{ gm::nt(L"ASSIGNMENT") }
>> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),
//		func定义式
gm::nt(L"RIGHT") >> gm::symlist{ gm::nt(L"FUNC_DEFINE") }
>> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),

gm::nt(L"FUNC_DEFINE") >> gm::symlist{
                        gm::te(gm::ttype::l_func),
                        gm::te(gm::ttype::l_left_brackets),gm::nt(L"ARGDEFINE"),gm::te(gm::ttype::l_right_brackets),
                        gm::nt(L"RETURN_TYPE_DECLEAR"),
                        gm::nt(L"SENTENCE_BLOCK") }
                        >> RS_ASTBUILDER_INDEX(ast::pass_function_define),

                // May empty

gm::nt(L"RETURN_TYPE_DECLEAR") >> gm::symlist{ gm::te(gm::ttype::l_empty) }
>> RS_ASTBUILDER_INDEX(ast::pass_empty),
gm::nt(L"RETURN_TYPE_DECLEAR") >> gm::symlist{ gm::nt(L"TYPE_DECLEAR") }
>> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),

gm::nt(L"TYPE_DECLEAR") >> gm::symlist{ gm::te(gm::ttype::l_typecast),gm::nt(L"TYPE") }
>> RS_ASTBUILDER_INDEX(ast::pass_direct<1>),

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

gm::nt(L"TYPE") >> gm::symlist{ gm::te(gm::ttype::l_identifier)}
>> RS_ASTBUILDER_INDEX(ast::pass_build_type),

gm::nt(L"TYPE") >> gm::symlist{ gm::nt(L"TYPE"),gm::nt(L"FUNC_ARG_TYPES") }
>> RS_ASTBUILDER_INDEX(ast::pass_build_type),

gm::nt(L"FUNC_ARG_TYPES") >> gm::symlist{ gm::te(gm::ttype::l_left_brackets),gm::nt(L"ARG_TYPES_WITH_VARIADIC"),gm::te(gm::ttype::l_right_brackets) }
>> RS_ASTBUILDER_INDEX(ast::pass_direct<1>),

gm::nt(L"ARG_TYPES_WITH_VARIADIC") >> gm::symlist{ gm::te(gm::ttype::l_empty) }
>> RS_ASTBUILDER_INDEX(ast::pass_create_list<0>),

gm::nt(L"ARG_TYPES_WITH_VARIADIC") >> gm::symlist{ gm::nt(L"ARG_TYPES") }
>> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),

gm::nt(L"ARG_TYPES_WITH_VARIADIC") >> gm::symlist{ gm::nt(L"VARIADIC_ARG_SIGN") }
>> RS_ASTBUILDER_INDEX(ast::pass_create_list<0>),

gm::nt(L"ARG_TYPES_WITH_VARIADIC") >> gm::symlist{ gm::nt(L"ARG_TYPES"), gm::te(gm::ttype::l_comma) ,gm::nt(L"VARIADIC_ARG_SIGN") }
>> RS_ASTBUILDER_INDEX(ast::pass_append_list<2, 0>),

gm::nt(L"ARG_TYPES") >> gm::symlist{ gm::nt(L"TYPE") }
>> RS_ASTBUILDER_INDEX(ast::pass_create_list<0>),

gm::nt(L"ARG_TYPES") >> gm::symlist{ gm::nt(L"ARG_TYPES"),gm::te(gm::ttype::l_comma),gm::nt(L"TYPE") }
>> RS_ASTBUILDER_INDEX(ast::pass_append_list<2,0>),


gm::nt(L"VARIADIC_ARG_SIGN") >> gm::symlist{gm::te(gm::ttype::l_variadic_sign) }
>> RS_ASTBUILDER_INDEX(ast::pass_token),

//////////////////////////////////////////////////////////////////////////////////////////////

gm::nt(L"ARGDEFINE") >> gm::symlist{ gm::te(gm::ttype::l_empty) }
>> RS_ASTBUILDER_INDEX(ast::pass_create_list<0>),

gm::nt(L"ARGDEFINE") >> gm::symlist{ gm::nt(L"ARGDEFINE_VAR_ITEM") }
>> RS_ASTBUILDER_INDEX(ast::pass_create_list<0>),

gm::nt(L"ARGDEFINE") >> gm::symlist{ gm::nt(L"ARGDEFINE_REF_ITEM") }
>> RS_ASTBUILDER_INDEX(ast::pass_create_list<0>),

gm::nt(L"ARGDEFINE") >> gm::symlist{ gm::nt(L"ARGDEFINE"),gm::te(gm::ttype::l_comma),gm::nt(L"ARGDEFINE_VAR_ITEM") }
>> RS_ASTBUILDER_INDEX(ast::pass_append_list<2,0>),
gm::nt(L"ARGDEFINE") >> gm::symlist{ gm::nt(L"ARGDEFINE"),gm::te(gm::ttype::l_comma),gm::nt(L"ARGDEFINE_REF_ITEM") }
>> RS_ASTBUILDER_INDEX(ast::pass_append_list<2,0>),

gm::nt(L"ARGDEFINE_VAR_ITEM") >> gm::symlist{ gm::te(gm::ttype::l_var),gm::te(gm::ttype::l_identifier),gm::nt(L"TYPE_DECLEAR") }
>> RS_ASTBUILDER_INDEX(ast::pass_func_argument),
gm::nt(L"ARGDEFINE_REF_ITEM") >> gm::symlist{ gm::te(gm::ttype::l_ref),gm::te(gm::ttype::l_identifier),gm::nt(L"TYPE_DECLEAR") }
>> RS_ASTBUILDER_INDEX(ast::pass_func_argument),
gm::nt(L"ARGDEFINE_VAR_ITEM") >> gm::symlist{ gm::te(gm::ttype::l_var),gm::te(gm::ttype::l_identifier)}
>> RS_ASTBUILDER_INDEX(ast::pass_func_argument),
gm::nt(L"ARGDEFINE_REF_ITEM") >> gm::symlist{ gm::te(gm::ttype::l_ref),gm::te(gm::ttype::l_identifier)}
>> RS_ASTBUILDER_INDEX(ast::pass_func_argument),

gm::nt(L"ARGDEFINE_REF_ITEM") >> gm::symlist{ gm::te(gm::ttype::l_variadic_sign) }
>> RS_ASTBUILDER_INDEX(ast::pass_token),

//		运算转换，左值可以转化为右值ASTUnary

gm::nt(L"RIGHT") >> gm::symlist{ gm::nt(L"LOGICAL_OR") }
>> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),
//	逻辑运算表达式
gm::nt(L"LOGICAL_OR") >> gm::symlist{ gm::nt(L"LOGICAL_AND") }
>> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),
gm::nt(L"LOGICAL_OR") >> gm::symlist{ gm::nt(L"LOGICAL_OR"),gm::te(gm::ttype::l_lor),gm::nt(L"LOGICAL_AND") },

gm::nt(L"LOGICAL_AND") >> gm::symlist{ gm::nt(L"RELATION") }
>> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),
gm::nt(L"LOGICAL_AND") >> gm::symlist{ gm::nt(L"LOGICAL_AND"),gm::te(gm::ttype::l_land),gm::nt(L"RELATION") },

//	关系判别表达式 < > >= <=
gm::nt(L"RELATION") >> gm::symlist{ gm::nt(L"EQUATION") }
>> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),
gm::nt(L"RELATION") >> gm::symlist{ gm::nt(L"RELATION"),gm::te(gm::ttype::l_larg),gm::nt(L"EQUATION") },
gm::nt(L"RELATION") >> gm::symlist{ gm::nt(L"RELATION"),gm::te(gm::ttype::l_less),gm::nt(L"EQUATION") },
gm::nt(L"RELATION") >> gm::symlist{ gm::nt(L"RELATION"),gm::te(gm::ttype::l_less_or_equal),gm::nt(L"EQUATION") },
gm::nt(L"RELATION") >> gm::symlist{ gm::nt(L"RELATION"),gm::te(gm::ttype::l_larg_or_equal),gm::nt(L"EQUATION") },
// 等价判别表达式
gm::nt(L"EQUATION") >> gm::symlist{ gm::nt(L"SUMMATION") }
>> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),
gm::nt(L"EQUATION") >> gm::symlist{ gm::nt(L"EQUATION"),gm::te(gm::ttype::l_equal),gm::nt(L"SUMMATION") },
gm::nt(L"EQUATION") >> gm::symlist{ gm::nt(L"EQUATION"),gm::te(gm::ttype::l_not_equal),gm::nt(L"SUMMATION") },
// 加减运算表达式		

gm::nt(L"SUMMATION") >> gm::symlist{ gm::nt(L"MULTIPLICATION") }
>> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),
gm::nt(L"SUMMATION") >> gm::symlist{ gm::nt(L"SUMMATION"),gm::te(gm::ttype::l_add),gm::nt(L"MULTIPLICATION") }
>> RS_ASTBUILDER_INDEX(ast::pass_binary_op),
gm::nt(L"SUMMATION") >> gm::symlist{ gm::nt(L"SUMMATION"),gm::te(gm::ttype::l_sub),gm::nt(L"MULTIPLICATION") }
>> RS_ASTBUILDER_INDEX(ast::pass_binary_op),

// 乘除运算表达式
gm::nt(L"MULTIPLICATION") >> gm::symlist{ gm::nt(L"FACTOR_TYPE_CASTING") }
>> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),
gm::nt(L"MULTIPLICATION") >> gm::symlist{ gm::nt(L"MULTIPLICATION"),gm::te(gm::ttype::l_mul),gm::nt(L"FACTOR_TYPE_CASTING") }
>> RS_ASTBUILDER_INDEX(ast::pass_binary_op),
gm::nt(L"MULTIPLICATION") >> gm::symlist{ gm::nt(L"MULTIPLICATION"),gm::te(gm::ttype::l_div),gm::nt(L"FACTOR_TYPE_CASTING") }
>> RS_ASTBUILDER_INDEX(ast::pass_binary_op),
gm::nt(L"MULTIPLICATION") >> gm::symlist{ gm::nt(L"MULTIPLICATION"),gm::te(gm::ttype::l_mod),gm::nt(L"FACTOR_TYPE_CASTING") }
>> RS_ASTBUILDER_INDEX(ast::pass_binary_op),

// TYPE CASTING..
gm::nt(L"FACTOR_TYPE_CASTING") >> gm::symlist{ gm::nt(L"FACTOR_TYPE_CASTING"), gm::nt(L"TYPE_DECLEAR") }
>> RS_ASTBUILDER_INDEX(ast::pass_type_cast),

gm::nt(L"FACTOR_TYPE_CASTING") >> gm::symlist{ gm::nt(L"FACTOR") }
>> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),

gm::nt(L"FACTOR") >> gm::symlist{ gm::nt(L"LEFT") }
>> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),
gm::nt(L"FACTOR") >> gm::symlist{ gm::te(gm::ttype::l_left_brackets),gm::nt(L"RIGHT"),gm::te(gm::ttype::l_right_brackets) }
>> RS_ASTBUILDER_INDEX(ast::pass_direct<1>),
gm::nt(L"FACTOR") >> gm::symlist{ gm::nt(L"UNIT") }
>> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),

gm::nt(L"UNIT") >> gm::symlist{ gm::te(gm::ttype::l_literal_integer) }
>> RS_ASTBUILDER_INDEX(ast::pass_literal),
gm::nt(L"UNIT") >> gm::symlist{ gm::te(gm::ttype::l_literal_real) }
>> RS_ASTBUILDER_INDEX(ast::pass_literal),
gm::nt(L"UNIT") >> gm::symlist{ gm::te(gm::ttype::l_literal_string) }
>> RS_ASTBUILDER_INDEX(ast::pass_literal),
gm::nt(L"UNIT") >> gm::symlist{ gm::te(gm::ttype::l_literal_handle) }
>> RS_ASTBUILDER_INDEX(ast::pass_literal),
gm::nt(L"UNIT") >> gm::symlist{ gm::te(gm::ttype::l_nil) }
>> RS_ASTBUILDER_INDEX(ast::pass_literal),
gm::nt(L"UNIT") >> gm::symlist{ gm::te(gm::ttype::l_inf) }
>> RS_ASTBUILDER_INDEX(ast::pass_literal),

gm::nt(L"UNIT") >> gm::symlist{ gm::nt(L"CONSTANT_MAP") }
>> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),

gm::nt(L"UNIT") >> gm::symlist{ gm::nt(L"CONSTANT_ARRAY") }
>> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),

gm::nt(L"CONSTANT_ARRAY") >> gm::symlist{
                                    gm::te(gm::ttype::l_index_begin),
                                    gm::nt(L"CONSTANT_ARRAY_ITEMS"),
                                    gm::te(gm::ttype::l_index_end) }
>> RS_ASTBUILDER_INDEX(ast::pass_array_builder),

gm::nt(L"CONSTANT_ARRAY_ITEMS") >> gm::symlist{ gm::nt(L"COMMA_EXPR") }
>> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),

gm::nt(L"CONSTANT_MAP") >> gm::symlist{
                                    gm::te(gm::ttype::l_left_curly_braces),
                                    gm::nt(L"CONSTANT_MAP_PAIRS"),
                                    gm::te(gm::ttype::l_right_curly_braces) }
>> RS_ASTBUILDER_INDEX(ast::pass_map_builder), 

gm::nt(L"CONSTANT_MAP_PAIRS") >> gm::symlist{ gm::nt(L"CONSTANT_MAP_PAIR") }
>> RS_ASTBUILDER_INDEX(ast::pass_create_list<0>),
gm::nt(L"CONSTANT_MAP_PAIRS") >> gm::symlist{ gm::nt(L"CONSTANT_MAP_PAIRS"),
                                                gm::te(gm::ttype::l_comma),
                                                gm::nt(L"CONSTANT_MAP_PAIR") }
    >> RS_ASTBUILDER_INDEX(ast::pass_append_list<2,0>),

gm::nt(L"CONSTANT_MAP_PAIR") >> gm::symlist{ gm::te(gm::ttype::l_empty) }
>> RS_ASTBUILDER_INDEX(ast::pass_empty),
gm::nt(L"CONSTANT_MAP_PAIR") >> gm::symlist{ gm::te(gm::ttype::l_left_curly_braces),
                                                gm::nt(L"RIGHT"), gm::te(gm::ttype::l_comma), gm::nt(L"RIGHT"),
                                                gm::te(gm::ttype::l_right_curly_braces) },

//单目运算符
gm::nt(L"UNIT") >> gm::symlist{ gm::te(gm::ttype::l_sub),gm::nt(L"UNIT") },
gm::nt(L"UNIT") >> gm::symlist{ gm::te(gm::ttype::l_lnot),gm::nt(L"UNIT") },

//左值是被赋值的对象，应该是一个标识符或者一个函数表达式
gm::nt(L"LEFT") >> gm::symlist{ gm::te(gm::ttype::l_identifier) }
>> RS_ASTBUILDER_INDEX(ast::pass_variable),

gm::nt(L"LEFT") >> gm::symlist{ gm::nt(L"SCOPING_IDENTIFIER") }
>> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),

gm::nt(L"SCOPING_IDENTIFIER") >> gm::symlist{ gm::nt(L"SCOPING_BEGIN_IDENT"),gm::nt(L"SCOPING_LIST") }
>> RS_ASTBUILDER_INDEX(ast::pass_finalize_serching_namespace),

gm::nt(L"SCOPING_BEGIN_IDENT") >> gm::symlist{ gm::te(gm::ttype::l_identifier) }
>> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),
gm::nt(L"SCOPING_BEGIN_IDENT") >> gm::symlist{ gm::te(gm::ttype::l_empty) }
>> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),

gm::nt(L"SCOPING_LIST") >> gm::symlist{ gm::te(gm::ttype::l_scopeing),gm::te(gm::ttype::l_identifier) }
>> RS_ASTBUILDER_INDEX(ast::pass_variable_in_namespace),// TODO HERE SHOULD BE IDENTIF IN NAMESPACE
gm::nt(L"SCOPING_LIST") >> gm::symlist{ gm::te(gm::ttype::l_scopeing),gm::te(gm::ttype::l_identifier),gm::nt(L"SCOPING_LIST") }
>> RS_ASTBUILDER_INDEX(ast::pass_append_serching_namespace),

gm::nt(L"LEFT") >> gm::symlist{ gm::nt(L"FACTOR_TYPE_CASTING"),gm::te(gm::ttype::l_index_point),gm::te(gm::ttype::l_identifier) }
>> RS_ASTBUILDER_INDEX(ast::pass_index_op),
gm::nt(L"LEFT") >> gm::symlist{ gm::nt(L"FACTOR_TYPE_CASTING"),gm::te(gm::ttype::l_index_begin),gm::nt(L"RIGHT"),gm::te(gm::ttype::l_index_end) }
>> RS_ASTBUILDER_INDEX(ast::pass_index_op),

gm::nt(L"LEFT") >> gm::symlist{ gm::nt(L"FUNCTION_CALL") }
>> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),
gm::nt(L"FUNCTION_CALL") >> gm::symlist{ gm::nt(L"FACTOR"),gm::te(gm::ttype::l_left_brackets),gm::nt(L"COMMA_EXPR"),gm::te(gm::ttype::l_right_brackets) }
>> RS_ASTBUILDER_INDEX(ast::pass_function_call),

gm::nt(L"COMMA_EXPR") >> gm::symlist{ gm::nt(L"COMMA_EXPR_ITEMS"), gm::nt(L"COMMA_MAY_EMPTY") }
>> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),

gm::nt(L"COMMA_EXPR") >> gm::symlist{gm::nt(L"COMMA_MAY_EMPTY") }
>> RS_ASTBUILDER_INDEX(ast::pass_create_list<0>),

gm::nt(L"COMMA_EXPR_ITEMS") >> gm::symlist{ gm::nt(L"MAY_REF_VALUE") }
>> RS_ASTBUILDER_INDEX(ast::pass_create_list<0>),

gm::nt(L"COMMA_EXPR_ITEMS") >> gm::symlist{ gm::nt(L"COMMA_EXPR_ITEMS"),gm::te(gm::ttype::l_comma), gm::nt(L"MAY_REF_VALUE") }
>> RS_ASTBUILDER_INDEX(ast::pass_append_list<2, 0>),

gm::nt(L"MAY_REF_VALUE") >> gm::symlist{ gm::nt(L"RIGHT") }
>> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),

gm::nt(L"MAY_REF_VALUE") >> gm::symlist{ gm::te(gm::ttype::l_ref),gm::nt(L"LEFT") }
>> RS_ASTBUILDER_INDEX(ast::pass_direct<1>),

gm::nt(L"COMMA_MAY_EMPTY") >> gm::symlist{gm::te(gm::ttype::l_comma)}
>> RS_ASTBUILDER_INDEX(ast::pass_empty),

gm::nt(L"COMMA_MAY_EMPTY") >> gm::symlist{ gm::te(gm::ttype::l_empty) }
>> RS_ASTBUILDER_INDEX(ast::pass_empty),
                }
            );

            std::cout << ANSI_HIY "RSGramma: " ANSI_RST "Checking LR(1) table..." << std::endl;

            if (rs_grammar->check_lr1())
            {
                std::cout << ANSI_HIR "RSGramma: " ANSI_RST "LR(1) have some problem, abort." << std::endl;
                exit(-1);
            }

            // rs_grammar->display();

            if (!RS_GRAMMAR_SKIP_GEN_LR1_TABLE_CACHE)
            {
                std::cout << ANSI_HIY "RSGramma: " ANSI_RST "OK, now writting cache..." << std::endl;

                using namespace std;
                const wchar_t* tab = L"    ";

                // Do expe
                std::wofstream cachefile("rs_lang_grammar_lr1_autogen.hpp");
                cachefile << L"// THIS FILE IS AUTO GENERATED BY RSTORABLESCENE." << endl;
                cachefile << L"// IF YOU WANT TO MODIFY GRAMMAR, PLEASE LOOK AT 'rs_lang_grammar.cpp'." << endl;
                cachefile << L"// " << endl;
                cachefile << L"#include \"rs_compiler_parser.hpp\"" << endl;
                cachefile << endl;
                cachefile << L"#define RS_LANG_GRAMMAR_LR1_AUTO_GENED" << endl;
                cachefile << L"#define RS_LANG_GRAMMAR_CRC64 0x" << std::hex << rs_lang_grammar_crc64 << "ull" << std::dec << endl;
                cachefile << endl;
                cachefile << endl;
                cachefile << L"namespace rs" << endl;
                cachefile << L"{" << endl;
                cachefile << L"#ifdef RS_LANG_GRAMMAR_LR1_IMPL" << endl;

                // Generate StateID - NontermName Pair & StateID - TermInt Pair
                ///////////////////////////////////////////////////////////////////////
                int max_length_producer = 0;
                std::map < std::wstring, int> nonte_list;
                std::map < lex_type, int> te_list;
                {
                    int nt_count = 0;
                    int te_count = 0;
                    for (auto& [op_nt, sym_list] : rs_grammar->ORGIN_P)
                    {
                        if (sym_list.size() > max_length_producer)
                            max_length_producer = (int)sym_list.size();

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

                    std::map <int, std::wstring> _real_nonte_list;
                    for (auto& [op_nt, nt_index] : nonte_list)
                    {
                        rs_test(_real_nonte_list.find(nt_index) == _real_nonte_list.end());
                        _real_nonte_list[nt_index] = op_nt;
                    }
                    std::map <int, lex_type> _real_te_list;
                    for (auto& [op_te, te_index] : te_list)
                    {
                        rs_test(_real_te_list.find(te_index) == _real_te_list.end());
                        _real_te_list.insert_or_assign(te_index, op_te);
                    }

                    cachefile << L"const wchar_t* rslang_id_nonterm_list[" << nt_count << L"+ 1] = {" << endl;
                    cachefile << tab << L"nullptr," << endl;
                    for (auto& [nt_index, op_nt] : _real_nonte_list)
                    {
                        cachefile << tab << L"L\"" << op_nt << L"\"," << endl;
                    }
                    cachefile << L"};" << endl;

                    cachefile << L"const lex_type rslang_id_term_list[" << te_count << L"+ 1] = {" << endl;
                    cachefile << tab << L"lex_type::l_error," << endl;
                    for (auto& [te_index, op_te] : _real_te_list)
                    {
                        cachefile << tab << L"lex_type::" << op_te._to_string() << L"," << endl;
                    }
                    cachefile << L"};" << endl;
                }
                ///////////////////////////////////////////////////////////////////////
                // Generate LR(1) action table;

                // GOTO : only nt have goto, 
                cachefile << L"const int rslang_lr1_act_goto[][" << nonte_list.size() << L" + 1] = {" << endl;
                cachefile << L"// {   STATE_ID,  NT_ID1, NT_ID2, ...  }" << endl;

                for (auto& [state_id, te_sym_list] : rs_grammar->LR1_TABLE)
                {
                    std::vector<int> nt_goto_state(nonte_list.size() + 1, -1);
                    bool has_action = false;
                    for (auto& [sym, actions] : te_sym_list)
                    {
                        rs_test(actions.size() <= 1);
                        if (std::holds_alternative<grammar::nt>(sym))
                        {
                            if (actions.size())
                            {
                                rs_test(actions.begin()->act == grammar::action::act_type::state_goto);
                                nt_goto_state[nonte_list[std::get<grammar::nt>(sym).nt_name]] = (int)actions.begin()->state;
                                has_action = true;
                            }
                        }
                    }

                    if (has_action)
                    {
                        cachefile << L" {   " << state_id << L",  ";
                        for (size_t i = 1; i < nt_goto_state.size(); i++)
                        {
                            cachefile << nt_goto_state[i] << L", ";
                        }
                        cachefile << L"}," << endl;
                    }
                }

                cachefile << L"};" << endl;

                // Stack/Reduce
                cachefile << L"const int rslang_lr1_act_stack_reduce[][" << te_list.size() << L" + 1] = {" << endl;
                cachefile << L"// {   STATE_ID,  TE_ID1, TE_ID2, ...  }" << endl;

                bool has_acc = false;
                int acc_state = 0, acc_term = 0;

                for (auto& [state_id, te_sym_list] : rs_grammar->LR1_TABLE)
                {
                    std::vector<int> te_stack_reduce_state(te_list.size() + 1, 0);
                    bool has_action = false;
                    for (auto& [sym, actions] : te_sym_list)
                    {
                        rs_test(actions.size() <= 1);
                        if (std::holds_alternative<grammar::te>(sym))
                        {
                            if (actions.size())
                            {
                                rs_test(actions.begin()->act == grammar::action::act_type::push_stack ||
                                    actions.begin()->act == grammar::action::act_type::reduction ||
                                    actions.begin()->act == grammar::action::act_type::accept
                                );

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
                                    rs_test(has_acc == false);
                                    has_acc = true;
                                    acc_state = (int)state_id;
                                    acc_term = te_list[std::get<grammar::te>(sym).t_type];
                                }

                            }
                        }
                    }

                    if (has_action)
                    {
                        cachefile << L" {   " << state_id << L",  ";
                        for (size_t i = 1; i < te_stack_reduce_state.size(); i++)
                        {
                            cachefile << te_stack_reduce_state[i] << L", ";
                        }
                        cachefile << L"}," << endl;
                    }
                }

                cachefile << L"};" << endl;

                ///////////////////////////////////////////////////////////////////////
                // Generate FOLLOW
                cachefile << L"const int rslang_follow_sets[][" << te_list.size() << L" + 1] = {" << endl;
                cachefile << L"// {   NONTERM_ID,  TE_ID1, TE_ID2, ...  }" << endl;
                for (auto& [follow_item_sym, follow_items] : rs_grammar->FOLLOW_SET)
                {
                    rs_test(std::holds_alternative<grammar::nt>(follow_item_sym));

                    std::vector<int> follow_set(te_list.size() + 1, 0);
                    cachefile << L"{ " << nonte_list[std::get<grammar::nt>(follow_item_sym).nt_name] << L",  ";
                    for (auto& tes : follow_items)
                    {
                        cachefile << te_list[tes.t_type] << L", ";
                    }
                    cachefile << L"}," << endl;

                }
                cachefile << L"};" << endl;
                ///////////////////////////////////////////////////////////////////////
                // Generate ORIGIN_P
                cachefile << L"const int rslang_origin_p[][" << max_length_producer << L" + 3] = {" << endl;
                cachefile << L"// {   NONTERM_ID, >> PFUNC_ID >> PNUM >>P01, P02, (te +, nt -)...  }" << endl;
                for (auto& [aim, rule] : rs_grammar->ORGIN_P)
                {
                    if (aim.builder_index == 0)
                    {
                        std::wcout << ANSI_HIY "RSGramma: " ANSI_RST "Producer: " ANSI_HIR << grammar::lr_item{ grammar::rule{ aim ,rule },size_t(-1),grammar::te(grammar::ttype::l_eof) } << ANSI_RST " have no ast builder, using default builder.." << std::endl;
                    }

                    cachefile << L"   { " << nonte_list[aim.nt_name] << L", " << aim.builder_index << L", " << rule.size() << ", ";
                    for (auto& sym : rule)
                    {
                        if (std::holds_alternative<grammar::te>(sym))
                            cachefile << te_list[std::get<grammar::te>(sym).t_type] << L",";
                        else
                            cachefile << -nonte_list[std::get<grammar::nt>(sym).nt_name] << L",";
                    }
                    cachefile << L"}," << endl;
                }
                cachefile << L"};" << endl;
                ///////////////////////////////////////////////////////////////////////

                rs_test(has_acc);

                cachefile << L"int rslang_accept_state = " << acc_state << L";" << std::endl;
                cachefile << L"int rslang_accept_term = " << acc_term << L";" << std::endl;
                cachefile << L"#else" << endl;
                cachefile << L"void rs_read_lr1_to(rs::grammar::lr1table_t & out_lr1table);" << endl;
                cachefile << L"void rs_read_follow_set_to(rs::grammar::sym_nts_t & out_followset);" << endl;
                cachefile << L"void rs_read_origin_p_to(std::vector<rs::grammar::rule> & out_origin_p);" << endl;
                cachefile << L"#endif" << endl;
                cachefile << L"}// end of namespace 'rs'" << endl;
                cachefile.flush();

                std::cout << ANSI_HIG "RSGramma: " ANSI_RST "Finished." << std::endl;
            }
            else
            {
                std::cout << ANSI_HIG "RSGramma: " ANSI_RST "Skip generating LR(1) table cache (RS_GRAMMAR_SKIP_GEN_LR1_TABLE_CACHE is true)." << std::endl;
            }

#if defined(RS_LANG_GRAMMAR_LR1_AUTO_GENED) && !RS_GRAMMAR_SKIP_GEN_LR1_TABLE_CACHE
        }
#undef RS_LANG_GRAMMAR_LR1_AUTO_GENED
#endif

        // DESCRIBE HOW TO GENERATE AST HERE:
        goto register_ast_builder_function; // used for hiding warning..
    register_ast_builder_function:
        // finally work

        for (auto& [rule_nt, _tokens] : rs_grammar->ORGIN_P)
        {
            if (rule_nt.builder_index)
                rule_nt.ast_create_func = ast::get_builder(rule_nt.builder_index);
        }

        rs_grammar->finish_rt();

        return rs_grammar;

    }
}



#undef RS_LSTR
#undef _RS_LSTR