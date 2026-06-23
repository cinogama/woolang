#pragma once

// Common include for the REPL UI: pulls in the public woolang C API (wo.h) and,
// transitively, woort.h (which defines the WOORT_ANSI_* color macros used for
// highlighting). Retained as a single include point for the REPL sources.

#include "wo.h"
