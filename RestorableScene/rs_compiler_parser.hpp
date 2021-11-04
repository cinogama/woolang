#pragma once
/*
In order to speed up compile and use less memory,
RS will using 'hand-work' parser, there is not yacc/bison..
*/

#include "rs_compiler_lexer.hpp"

#include <variant>

namespace rs
{
    class parser
    {
        class ast_base
        {
        private:
            inline static std::vector<ast_base*> list;
            ast_base* parent;
            ast_base* children;
            ast_base* sibling;
        public:
            ast_base()
                : parent(nullptr)
                , children(nullptr)
                , sibling(nullptr)
            {
                list.push_back(this);
            }
            void add_child(ast_base* ast_node)
            {
                rs_test(ast_node->parent == nullptr);
                rs_test(ast_node->sibling == nullptr);

                ast_node->parent = this;
                ast_base* childs = children;
                while (childs)
                {
                    if (childs->sibling == nullptr)
                    {
                        childs->sibling = ast_node;
                        return;
                    }
                }
                children = childs;
            }
            void remove_child(ast_base* ast_node)
            {
                ast_base* last_childs = nullptr;
                ast_base* childs = children;
                while (childs)
                {
                    if (ast_node == childs)
                    {
                        if (last_childs)
                        {
                            last_childs->sibling = childs->sibling;
                            ast_node->parent = nullptr;
                            ast_node->sibling = nullptr;

                            return;
                        }
                    }

                    last_childs = childs;
                    childs = childs->sibling;
                }

                rs_error("There is no such a child node.");
            }
        public:
            virtual std::wstring to_wstring() = 0;
        };

        ///////////////////////////////////////////////

        lexer* lex;

    public:
        parser(lexer& rs_lex)
            :lex(&rs_lex)
        {

        }

        void LEFT_VALUE_handler()
        {
            // LEFT_VALUE   >>  l_identifier
            //                  LEFT_VALUE . l_identifier
            //                  LEFT_VALUE ( ARG_DEFINE )
            //                  LEFT_VALUE :: l_identifier
        }

        void ASSIGNMENT_handler()
        {
            // ASSIGNMENT   >>  LEFT_VALUE = RIGHT_VALUE
            //                  LEFT_VALUE += RIGHT_VALUE
            //                  LEFT_VALUE -= RIGHT_VALUE
            //                  LEFT_VALUE *= RIGHT_VALUE
            //                  LEFT_VALUE /= RIGHT_VALUE
            //                  LEFT_VALUE %= RIGHT_VALUE
        }

        void RIGHT_VALUE_handler()
        {
            // RIGHT_VALUE  >>  ASSIGNMENT
            //                  func( ARG_DEFINE ) RETURN_TYPE_DECLEAR SENTENCE
            //                  LOGICAL_OR
        }

        void EXPRESSION_handler()
        {
            // This function will product a expr, if failed return nullptr;
            // 
            // EXPRESSION   >>  RIGHT_VALUE

            return RIGHT_VALUE_handler();
        }

        void build_ast()
        {
            //
        }
    };
}