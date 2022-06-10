#define _CRT_SECURE_NO_WARNINGS

#include "wo_lang_ast_builder.hpp"

namespace wo
{
    namespace ast
    {
        ast_value* dude_dump_ast_value(ast_value* dude)
        {
            if (dude)
                return dynamic_cast<ast_value*>(dude->instance());
            return nullptr;
        }
        ast_type* dude_dump_ast_type(ast_type* dude)
        {
            if (dude)
                return dynamic_cast<ast_type*>(dude->instance());
            return nullptr;
        }
    }
}