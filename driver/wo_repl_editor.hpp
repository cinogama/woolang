#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Returns true when stdin is an interactive console (terminal), i.e. raw-mode
// live editing is appropriate. When false, the caller should fall back to the
// plain blocking line reader (woort_console_readline).
bool wo_repl_stdin_is_tty();

// Tokenize `src` with the public LSP lexer and return it wrapped in
// WOORT_ANSI_* colors (whitespace preserved verbatim). Exposed so the driver
// can render highlighted snippets (e.g. for the --hl-demo diagnostic).
std::string wo_repl_render_highlight(std::string_view src);

// Reads one logical line from an interactive console with live syntax
// highlighting.
//
// On each keystroke the current line is re-rendered as: the colored prompt
// followed by the input colored via the public LSP lexer. Editing supported:
// UTF-8 text entry, Backspace, Delete, Left/Right, Home/End, Ctrl-A/E,
// Ctrl-C (clear current input), Ctrl-D (EOF on empty input), Enter (submit),
// Tab (insert 4 spaces), Up/Down (navigate command history).
//
// The cursor is positioned by byte offset, so it is exact for ASCII input and
// only approximate for wide (e.g. CJK) characters.
//
// prompt is the prefix to render (">>> " or "... "); its first character
// selects the prompt color.
//
// history holds previously submitted physical lines (oldest first). It is read
// for Up/Down navigation and appended to on submit, skipping empty/all-space
// lines, consecutive duplicates, and lines recalled from history unchanged.
//
// Returns the entered line (without trailing newline). An empty optional
// signals end-of-file; the entered text is otherwise returned verbatim.
// If raw-mode setup fails, returns an empty optional so the caller can fall
// back to the plain reader.
std::optional<std::string> wo_repl_live_readline(
    std::string_view prompt, std::vector<std::string>& history);
