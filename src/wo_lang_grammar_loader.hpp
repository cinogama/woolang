#pragma once

#include "wo_compiler_parser.hpp"

namespace wo
{
#ifndef WO_DISABLE_COMPILER
    void init_woolang_grammar();
    void shutdown_woolang_grammar();
    grammar* get_grammar_instance();
#endif
}