#include "wo_lang_ast_builder.hpp"

namespace wo
{
#ifndef WO_DISABLE_COMPILER
    namespace ast
    {
        void init_builder()
        {

        }
    }

    grammar::rule operator>>(grammar::rule ost, size_t builder_index)
    {
        ost.first.builder_index = builder_index;
        return ost;
    }
#endif
}