#include "rs_lang_grammar_loader.hpp"
#include "rs_lang_grammar_lr1_autogen.hpp"
#include "rs_crc_64.hpp"

#include "rs_lang_ast_builder.hpp"

#include <fstream>

#define _RS_LSTR(X) L##X
#define RS_LSTR(X) _RS_LSTR(X)

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
#ifdef RS_LANG_GRAMMAR_LR1_AUTO_GENED
            goto load_from_buffer;
#else
            rs_error("Fail to load grammar.");
#endif // RS_LANG_GRAMMAR_LR1_AUTO_GENED

        }
        rs_lang_grammar_crc64 = crc_64(this_grammar_file);

#ifdef RS_LANG_GRAMMAR_LR1_AUTO_GENED

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

gm::nt(L"PARAGRAPH") >> gm::symlist{gm::nt(L"PARAGRAPH"),gm::nt(L"SENTENCE")},
gm::nt(L"PARAGRAPH") >> gm::symlist{gm::nt(L"SENTENCE")},
gm::nt(L"PARAGRAPH") >> gm::symlist{gm::te(gm::ttype::l_empty)},
gm::nt(L"SENTENCE") >> gm::symlist{gm::te(gm::ttype::l_import), gm::nt(L"IMPORT_NAME"), gm::te(gm::ttype::l_semicolon)},

gm::nt(L"IMPORT_NAME") >> gm::symlist{gm::te(gm::ttype::l_identifier), gm::nt(L"INDEX_IMPORT_NAME")},

gm::nt(L"INDEX_IMPORT_NAME") >> gm::symlist{gm::te(gm::ttype::l_empty)},
gm::nt(L"INDEX_IMPORT_NAME") >> gm::symlist{gm::te(gm::ttype::l_index_point), gm::te(gm::ttype::l_identifier), gm::nt(L"INDEX_IMPORT_NAME")},

gm::nt(L"SENTENCE") >> gm::symlist{gm::te(gm::ttype::l_left_curly_braces),gm::nt(L"PARAGRAPH"),gm::te(gm::ttype::l_right_curly_braces)},
gm::nt(L"SENTENCE") >> gm::symlist{gm::te(gm::ttype::l_semicolon)},

gm::nt(L"SENTENCE") >> gm::symlist{gm::nt(L"FUNC_DEFINE_WITH_NAME")},
gm::nt(L"FUNC_DEFINE_WITH_NAME") >> gm::symlist{
                        gm::te(gm::ttype::l_func),
                        gm::te(gm::ttype::l_identifier),
                        gm::te(gm::ttype::l_left_brackets),gm::nt(L"ARGDEFINE"),gm::te(gm::ttype::l_right_brackets),
                        gm::nt(L"RETURN_TYPE_DECLEAR"),
                        gm::nt(L"SENTENCE")},


                        //WHILE语句
                        gm::nt(L"SENTENCE") >> gm::symlist{gm::nt(L"WHILE")},
                        gm::nt(L"WHILE") >> gm::symlist{
                                                gm::te(gm::ttype::l_while),
                                                gm::te(gm::ttype::l_left_brackets),
                                                gm::nt(L"EXPRESSION"),
                                                gm::te(gm::ttype::l_right_brackets),
                                                gm::nt(L"SENTENCE")
                                            },
                //IF语句
gm::nt(L"SENTENCE") >> gm::symlist{gm::nt(L"IF")},
gm::nt(L"IF") >> gm::symlist{
                        gm::te(gm::ttype::l_if),
                        gm::te(gm::ttype::l_left_brackets),
                        gm::nt(L"EXPRESSION"),
                        gm::te(gm::ttype::l_right_brackets),
                        gm::nt(L"SENTENCE"),
                        gm::nt(L"ELSE")
                    },
gm::nt(L"ELSE") >> gm::symlist{gm::te(gm::ttype::l_empty)},
gm::nt(L"ELSE") >> gm::symlist{gm::te(gm::ttype::l_else),gm::nt(L"SENTENCE")},

//变量定义表达式
gm::nt(L"SENTENCE") >> gm::symlist{
                        gm::te(gm::ttype::l_var),
                        gm::nt(L"VARDEFINE"),
                        gm::te(gm::ttype::l_semicolon)},//ASTVariableDefination

                        gm::nt(L"SENTENCE") >> gm::symlist{
                        gm::te(gm::ttype::l_ref),
                        gm::nt(L"REFDEFINE"),
                        gm::te(gm::ttype::l_semicolon)},//ASTVariableDefination

gm::nt(L"REFDEFINE") >> gm::symlist{gm::te(gm::ttype::l_identifier),
                        gm::te(gm::ttype::l_assign),
                        gm::nt(L"LEFT")},//ASTVariableDefination

gm::nt(L"REFDEFINE") >> gm::symlist{
                        gm::nt(L"REFDEFINE"),
                        gm::te(gm::ttype::l_comma),
                        gm::te(gm::ttype::l_identifier),
                        gm::te(gm::ttype::l_assign),
                        gm::nt(L"LEFT")},//ASTVariableDefination


gm::nt(L"VARDEFINE") >> gm::symlist{gm::te(gm::ttype::l_identifier),
                        gm::te(gm::ttype::l_assign),
                        gm::nt(L"EXPRESSION")},//ASTVariableDefination

gm::nt(L"VARDEFINE") >> gm::symlist{
                        gm::nt(L"VARDEFINE"),
                        gm::te(gm::ttype::l_comma),
                        gm::te(gm::ttype::l_identifier),
                        gm::te(gm::ttype::l_assign,L"="),
                        gm::nt(L"EXPRESSION")},//ASTVariableDefination

                                //变量定义表达式
gm::nt(L"SENTENCE") >> gm::symlist{gm::te(gm::ttype::l_return),gm::nt(L"RETNVALUE"),gm::te(gm::ttype::l_semicolon)},

gm::nt(L"RETNVALUE") >> gm::symlist{gm::te(gm::ttype::l_empty)},//返回值可以是空产生式
gm::nt(L"RETNVALUE") >> gm::symlist{gm::nt(L"EXPRESSION")},//返回值也可以是一个表达式

//语句与表达式
gm::nt(L"SENTENCE") >> gm::symlist{gm::nt(L"EXPRESSION"),gm::te(gm::ttype::l_semicolon)}
>> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),
//	RIGHT当然是一个表达式
//gm::nt(L"EXPRESSION") >> gm::symlist{gm::nt(L"ASSIGNMENT")},
gm::nt(L"EXPRESSION") >> gm::symlist{gm::nt(L"RIGHT")}
 >> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),
//gm::nt(L"EXPRESSION") >> gm::symlist{gm::nt(L"ASSIGNMENT")},

gm::nt(L"ASSIGNMENT") >> gm::symlist{gm::nt(L"LEFT"),gm::te(gm::ttype::l_assign),gm::nt(L"RIGHT")},
gm::nt(L"ASSIGNMENT") >> gm::symlist{gm::nt(L"LEFT"),gm::te(gm::ttype::l_add_assign),gm::nt(L"RIGHT")},
gm::nt(L"ASSIGNMENT") >> gm::symlist{gm::nt(L"LEFT"),gm::te(gm::ttype::l_sub_assign),gm::nt(L"RIGHT")},
gm::nt(L"ASSIGNMENT") >> gm::symlist{gm::nt(L"LEFT"),gm::te(gm::ttype::l_mul_assign),gm::nt(L"RIGHT")},
gm::nt(L"ASSIGNMENT") >> gm::symlist{gm::nt(L"LEFT"),gm::te(gm::ttype::l_div_assign),gm::nt(L"RIGHT")},
gm::nt(L"ASSIGNMENT") >> gm::symlist{gm::nt(L"LEFT"),gm::te(gm::ttype::l_mod_assign),gm::nt(L"RIGHT")},

//右值是仅取值的表达式 ，赋值语句当然也是一个右值		

gm::nt(L"RIGHT") >> gm::symlist{gm::nt(L"ASSIGNMENT")}
>> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),
//		func定义式
gm::nt(L"RIGHT") >> gm::symlist{gm::nt(L"FUNC_DEFINE")}
>> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),

gm::nt(L"FUNC_DEFINE") >> gm::symlist{
                        gm::te(gm::ttype::l_func),
                        gm::te(gm::ttype::l_left_brackets),gm::nt(L"ARGDEFINE"),gm::te(gm::ttype::l_right_brackets),
                        gm::nt(L"RETURN_TYPE_DECLEAR"),
                        gm::nt(L"SENTENCE")},

                        // May empty

                        gm::nt(L"RETURN_TYPE_DECLEAR") >> gm::symlist{ gm::te(gm::ttype::l_empty) },
                        gm::nt(L"RETURN_TYPE_DECLEAR") >> gm::symlist{ gm::nt(L"TYPE_DECLEAR") },
                        gm::nt(L"TYPE_DECLEAR") >> gm::symlist{ gm::te(gm::ttype::l_typecast),gm::te(gm::ttype::l_identifier) },

                        gm::nt(L"ARGDEFINE") >> gm::symlist{gm::te(gm::ttype::l_empty)},
                        gm::nt(L"ARGDEFINE") >> gm::symlist{gm::te(gm::ttype::l_var),gm::te(gm::ttype::l_identifier)},
                        gm::nt(L"ARGDEFINE") >> gm::symlist{ gm::te(gm::ttype::l_ref),gm::te(gm::ttype::l_identifier) },
                        gm::nt(L"ARGDEFINE") >> gm::symlist{gm::nt(L"ARGDEFINE"),gm::te(gm::ttype::l_comma),gm::te(gm::ttype::l_var),gm::te(gm::ttype::l_identifier)},
                        gm::nt(L"ARGDEFINE") >> gm::symlist{gm::nt(L"ARGDEFINE"),gm::te(gm::ttype::l_comma),gm::te(gm::ttype::l_ref),gm::te(gm::ttype::l_identifier)},


                        //		运算转换，左值可以转化为右值ASTUnary


                        gm::nt(L"RIGHT") >> gm::symlist{gm::nt(L"LOGICAL_OR")} 
                        >> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),
                        //	逻辑运算表达式
                        gm::nt(L"LOGICAL_OR") >> gm::symlist{gm::nt(L"LOGICAL_AND")}
                        >> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),
                        gm::nt(L"LOGICAL_OR") >> gm::symlist{gm::nt(L"LOGICAL_OR"),gm::te(gm::ttype::l_lor),gm::nt(L"LOGICAL_AND")} ,

                        gm::nt(L"LOGICAL_AND") >> gm::symlist{gm::nt(L"RELATION")}
                        >> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),
                        gm::nt(L"LOGICAL_AND") >> gm::symlist{gm::nt(L"LOGICAL_AND"),gm::te(gm::ttype::l_land),gm::nt(L"RELATION")} ,

                //	关系判别表达式 < > >= <=
                gm::nt(L"RELATION") >> gm::symlist{gm::nt(L"EQUATION")}
                >> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt(L"RELATION") >> gm::symlist{gm::nt(L"RELATION"),gm::te(gm::ttype::l_larg),gm::nt(L"EQUATION")} ,
                gm::nt(L"RELATION") >> gm::symlist{gm::nt(L"RELATION"),gm::te(gm::ttype::l_less),gm::nt(L"EQUATION")} ,
                gm::nt(L"RELATION") >> gm::symlist{gm::nt(L"RELATION"),gm::te(gm::ttype::l_less_or_equal),gm::nt(L"EQUATION")} ,
                gm::nt(L"RELATION") >> gm::symlist{gm::nt(L"RELATION"),gm::te(gm::ttype::l_larg_or_equal),gm::nt(L"EQUATION")} ,
                // 等价判别表达式
                gm::nt(L"EQUATION") >> gm::symlist{gm::nt(L"SUMMATION")}
                >> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt(L"EQUATION") >> gm::symlist{gm::nt(L"EQUATION"),gm::te(gm::ttype::l_equal),gm::nt(L"SUMMATION")} ,
                gm::nt(L"EQUATION") >> gm::symlist{gm::nt(L"EQUATION"),gm::te(gm::ttype::l_not_equal),gm::nt(L"SUMMATION")} ,
                // 加减运算表达式		

                gm::nt(L"SUMMATION") >> gm::symlist{gm::nt(L"MULTIPLICATION")}
                >> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt(L"SUMMATION") >> gm::symlist{gm::nt(L"SUMMATION"),gm::te(gm::ttype::l_add),gm::nt(L"MULTIPLICATION")} ,
                gm::nt(L"SUMMATION") >> gm::symlist{ gm::nt(L"SUMMATION"),gm::te(gm::ttype::l_sub),gm::nt(L"MULTIPLICATION") },

                // 乘除运算表达式
                gm::nt(L"MULTIPLICATION") >> gm::symlist{ gm::nt(L"FACTOR") }
                >> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt(L"MULTIPLICATION") >> gm::symlist{gm::nt(L"MULTIPLICATION"),gm::te(gm::ttype::l_mul),gm::nt(L"FACTOR")} ,
                gm::nt(L"MULTIPLICATION") >> gm::symlist{ gm::nt(L"MULTIPLICATION"),gm::te(gm::ttype::l_div),gm::nt(L"FACTOR") },
                gm::nt(L"MULTIPLICATION") >> gm::symlist{gm::nt(L"MULTIPLICATION"),gm::te(gm::ttype::l_mod),gm::nt(L"FACTOR")} ,

                //	RIGHT可以作为因子
                gm::nt(L"FACTOR") >> gm::symlist{ gm::nt(L"FACTOR"), gm::nt(L"TYPE_DECLEAR") },
                gm::nt(L"FACTOR") >> gm::symlist{gm::nt(L"LEFT")} 
                >> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt(L"FACTOR") >> gm::symlist{gm::te(gm::ttype::l_left_brackets),gm::nt(L"RIGHT"),gm::te(gm::ttype::l_right_brackets)},
                gm::nt(L"FACTOR") >> gm::symlist{ gm::nt(L"UNIT") } 
                >> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),

                gm::nt(L"UNIT") >> gm::symlist{ gm::te(gm::ttype::l_literal_integer) }
                >> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt(L"UNIT") >> gm::symlist{gm::te(gm::ttype::l_literal_real)}
                >> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt(L"UNIT") >> gm::symlist{gm::te(gm::ttype::l_literal_string)}
                >> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt(L"UNIT") >> gm::symlist{ gm::te(gm::ttype::l_literal_handle) }
                >> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),

                //单目运算符
                gm::nt(L"UNIT") >> gm::symlist{ gm::te(gm::ttype::l_sub),gm::nt(L"UNIT") },
                gm::nt(L"UNIT") >> gm::symlist{ gm::te(gm::ttype::l_lnot),gm::nt(L"UNIT") },

                //左值是被赋值的对象，应该是一个标识符或者一个函数表达式
                gm::nt(L"LEFT") >> gm::symlist{gm::te(gm::ttype::l_identifier)}
                >> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),

                gm::nt(L"LEFT") >> gm::symlist{gm::nt(L"LEFT"),gm::te(gm::ttype::l_index_point),gm::te(gm::ttype::l_identifier)},
                gm::nt(L"LEFT") >> gm::symlist{ gm::nt(L"FUNCTION_CALL") }
                >> RS_ASTBUILDER_INDEX(ast::pass_direct<0>),
                gm::nt(L"FUNCTION_CALL") >> gm::symlist{gm::nt(L"LEFT"),gm::te(gm::ttype::l_left_brackets),gm::nt(L"ARGLIST"),gm::te(gm::ttype::l_right_brackets)},
                gm::nt(L"ARGLIST") >> gm::symlist{gm::te(gm::ttype::l_empty)},//参数表可以是空的
                gm::nt(L"ARGLIST") >> gm::symlist{gm::nt(L"RIGHT")},
                gm::nt(L"ARGLIST") >> gm::symlist{gm::nt(L"ARGLIST"),gm::te(gm::ttype::l_comma),gm::nt(L"RIGHT")},

                gm::nt(L"LEFT") >> gm::symlist{gm::nt(L"LEFT"),gm::te(gm::ttype::l_scopeing),gm::te(gm::ttype::l_identifier)},
                }
            );

            std::cout << ANSI_HIY "RSGramma: " ANSI_RST "Checking LR(1) table..." << std::endl;

            if (rs_grammar->check_lr1())
            {
                std::cout << ANSI_HIR "RSGramma: " ANSI_RST "LR(1) have some problem, abort." << std::endl;
                exit(-1);
            }

            rs_grammar->display();

            std::cout << ANSI_HIY "RSGramma: " ANSI_RST "OK, now writting cache..." << std::endl;

            std::wofstream cachefile("rs_lang_grammar_lr1_autogen.hpp");
            if (cachefile.fail())
            {
                std::cout << ANSI_HIR "RSGramma: " ANSI_RST "Failed to open file." << std::endl;
                exit(-1);
            }

            using namespace std;
            const wchar_t* tab = L"    ";

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


            {
                int state_lr1_block_count = 0;
                cachefile << L"void rs_read_lr1_to_" << state_lr1_block_count << L"(rs::grammar::lr1table_t & out_lr1table)" << endl;
                cachefile << L"#ifdef RS_LANG_GRAMMAR_LR1_IMPL" << endl;
                cachefile << L"{" << endl;
                cachefile << tab << "using gm = rs::grammar;" << endl;
                cachefile << endl;

                int state_lr1_count = 0;
                for (auto& lr1_item : rs_grammar->LR1_TABLE)
                {
                    if (lr1_item.second.empty())
                        continue;


                    cachefile << tab << L"out_lr1table[" << lr1_item.first << L"] = { " << endl;

                    int i = 0;
                    for (auto& [lr1_item_sym, lr1_item_act_set] : lr1_item.second)
                    {
                        if (lr1_item_act_set.empty())
                            continue;

                        cachefile << tab << tab << L"{ ";
                        if (std::holds_alternative<gm::te>(lr1_item_sym))
                        {
                            // if is a term
                            cachefile << L"gm::te(lex_type::" << std::get<gm::te>(lr1_item_sym).t_type << L", L\"" << std::get<gm::te>(lr1_item_sym).t_name << L"\")";
                        }
                        else
                        {
                            cachefile << L"gm::nt(L\"" << std::get<gm::nt>(lr1_item_sym).nt_name << L"\")";
                        }
                        cachefile << L", {" << endl;

                        for (auto& action_set : lr1_item_act_set)
                        {
                            cachefile << tab << tab << tab << L"gm::action{gm::action::act_type::";

                            switch (action_set.act)
                            {
                            case gm::action::act_type::error:
                                cachefile << L"error"; break;
                            case gm::action::act_type::accept:
                                cachefile << L"accept"; break;
                            case gm::action::act_type::push_stack:
                                cachefile << L"push_stack"; break;
                            case gm::action::act_type::reduction:
                                cachefile << L"reduction"; break;
                            case gm::action::act_type::state_goto:
                                cachefile << L"state_goto"; break;
                            default:
                                break;
                            }
                            cachefile << L", gm::te(lex_type::" << action_set.prospect.t_type << L", L\"" << action_set.prospect.t_name << L"\"), " << action_set.state << "}, " << endl;
                        }

                        cachefile << tab << tab << tab << "}," << endl;
                        cachefile << tab << tab << "}," << endl;
                        i++;

                    }

                    cachefile << tab << "};" << endl;

                    state_lr1_count++;
                    if (state_lr1_count % 5 == 0)
                    {
                        cachefile << "}" << endl;
                        cachefile << L"#else" << endl;
                        cachefile << L";" << endl;
                        cachefile << L"#endif" << endl;
                        state_lr1_block_count++;
                        cachefile << L"void rs_read_lr1_to_" << state_lr1_block_count << L"(rs::grammar::lr1table_t & out_lr1table)" << endl;
                        cachefile << L"#ifdef RS_LANG_GRAMMAR_LR1_IMPL" << endl;
                        cachefile << L"{" << endl;
                        cachefile << tab << "using gm = rs::grammar;" << endl;
                        cachefile << endl;
                    }
                }

                cachefile << "}" << endl;
                cachefile << L"#else" << endl;
                cachefile << L";" << endl;
                cachefile << L"#endif" << endl;

                cachefile << L"void rs_read_lr1_to(rs::grammar::lr1table_t & out_lr1table)" << endl;
                cachefile << L"#ifdef RS_LANG_GRAMMAR_LR1_IMPL" << endl;
                cachefile << L"{" << endl;

                for (int i = 0; i <= state_lr1_block_count; i++)
                {
                    cachefile << tab << L"rs_read_lr1_to_" << i << L"(out_lr1table);" << endl;
                }
                cachefile << "}" << endl;
                cachefile << L"#else" << endl;
                cachefile << L";" << endl;
                cachefile << L"#endif" << endl;
            }

            {

                cachefile << L"void rs_read_follow_set_to(rs::grammar::sym_nts_t & out_followset)" << endl;
                cachefile << L"#ifdef RS_LANG_GRAMMAR_LR1_IMPL" << endl;
                cachefile << L"{" << endl;
                cachefile << tab << "using gm = rs::grammar;" << endl;
                cachefile << endl;

                int follow_index = 0;
                for (auto& [follow_item_sym, follow_items] : rs_grammar->FOLLOW_SET)
                {
                    if (follow_items.empty())
                        continue;

                    std::wstring now_follow_set = L"lr1_follow_" + std::to_wstring(follow_index);
                    cachefile << tab << L"auto & " << now_follow_set << L" = out_followset[";
                    if (std::holds_alternative<gm::te>(follow_item_sym))
                    {
                        // if is a term
                        cachefile << L"gm::te(lex_type::" << std::get<gm::te>(follow_item_sym).t_type << L", L\"" << std::get<gm::te>(follow_item_sym).t_name << L"\")";
                    }
                    else
                    {
                        cachefile << L"gm::nt(L\"" << std::get<gm::nt>(follow_item_sym).nt_name << L"\")";
                    }
                    cachefile << L"];" << endl;

                    for (auto& follow_te : follow_items)
                    {
                        cachefile << tab << tab << now_follow_set << L".insert(gm::te(lex_type::" << follow_te.t_type << ", L\"" << follow_te.t_name << L"\"));" << endl;
                    }

                    follow_index++;
                }
                cachefile << "}" << endl;
                cachefile << L"#else" << endl;
                cachefile << L";" << endl;
                cachefile << L"#endif" << endl;
            }

            {
                cachefile << L"void rs_read_origin_p_to(std::vector<rs::grammar::rule> & out_origin_p)" << endl;
                cachefile << L"#ifdef RS_LANG_GRAMMAR_LR1_IMPL" << endl;
                cachefile << L"{" << endl;
                cachefile << tab << "using gm = rs::grammar;" << endl;
                cachefile << endl;

                for (auto& op : rs_grammar->ORGIN_P)
                {
                    if (op.second.empty())
                        continue;

                    cachefile << tab << "out_origin_p.emplace_back(std::make_pair<gm::nt, gm::symlist>(";
                    cachefile << L"gm::nt(L\"" << op.first.nt_name << L"\", " << op.first.builder_index << L"), {";
                    for (auto sym_item : op.second)
                    {
                        if (std::holds_alternative<gm::te>(sym_item))
                        {
                            // if is a term
                            cachefile << L"gm::te(lex_type::" << std::get<gm::te>(sym_item).t_type << L", L\"" << std::get<gm::te>(sym_item).t_name << L"\")";
                        }
                        else
                        {
                            cachefile << L"gm::nt(L\"" << std::get<gm::nt>(sym_item).nt_name << L"\")";
                        }
                        cachefile << L", ";
                    }
                    cachefile << L"}));" << endl;
                }

                cachefile << "}" << endl;
                cachefile << L"#else" << endl;
                cachefile << L";" << endl;
                cachefile << L"#endif" << endl;
            }

            cachefile << endl;
            cachefile << "}" << endl;

            cachefile.flush();

            std::cout << ANSI_HIG "RSGramma: " ANSI_RST "Finished." << std::endl;

#ifdef RS_LANG_GRAMMAR_LR1_AUTO_GENED
        }
#undef RS_LANG_GRAMMAR_LR1_AUTO_GENED
#endif

        // DESCRIBE HOW TO GENERATE AST HERE:
    register_ast_builder_function:
        // finally work

        for (auto& [rule_nt, _tokens] : rs_grammar->ORGIN_P)
        {
            if (rule_nt.builder_index)
                rule_nt.ast_create_func = ast::get_builder(rule_nt.builder_index);
        }

        return rs_grammar;

    }
}



#undef RS_LSTR
#undef _RS_LSTR