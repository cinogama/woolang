/*
 * Compilation unit for the byacc-generated parser (woolang.tab.inc).
 *
 * byacc emits the tables and yyparse() at global scope; including the
 * generated file inside namespace wo::parser gives them the same scope as
 * the declarations in wo_lang_parser.hpp, so the skeleton's calls to
 * yylex/yyerror resolve to wo::parser::yylex / wo::parser::yyerror.
 *
 * The standard headers are included here first: the generated skeleton
 * includes <stdlib.h>/<string.h> (and <stdio.h> under YYDEBUG) itself, and
 * those nested includes must be no-ops inside the namespace.
 */
#include "wo_lang_parser.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace wo
{
namespace parser
{
#include "woolang.tab.inc"
}
}
