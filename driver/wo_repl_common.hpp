#pragma once

// Pulls in wo.h with the public LSP lexer API exposed. The LSP API section of
// wo.h is guarded by WO_NEED_LSP_API, which is otherwise only defined when
// building libwoo (WO_IMPL). The REPL UI tokenizes input lines through the
// public wo_lspv2_lexer_* API, so it must opt in here, before wo.h is seen.
#ifndef WO_NEED_LSP_API
#   define WO_NEED_LSP_API 1
#endif

#include "wo.h"
