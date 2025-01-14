#pragma once

#include "wo_compiler_parser.hpp"

namespace wo
{
#ifndef WO_DISABLE_COMPILER
    grammar* get_wo_grammar();
#endif
}