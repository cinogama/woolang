#include "rs_lang_ast_builder.hpp"

namespace rs
{
    namespace ast
    {
        ast_value* dude_dump_ast_value(ast_value* dude)
        {
            if (dude)
                return dynamic_cast<ast_value*>(dude->instance());
            return nullptr;
        }
    }
}