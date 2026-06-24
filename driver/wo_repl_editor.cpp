// Live syntax-highlighting line editor for the Woolang REPL.
//
// Re-tokenizes the current input with the public LSP lexer on every edit and
// re-renders `prompt + colored(input)`. Input is read in raw mode: POSIX uses
// termios, Windows uses ENABLE_VIRTUAL_TERMINAL_INPUT (which delivers arrow
// keys etc. as VT escape sequences), so both platforms share one escape
// decoder.

#define NOMINMAX

#include "wo_repl_common.hpp"
#include "wo_repl_editor.hpp"

#include <cstdio>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#   include <windows.h>
#else
#   include <unistd.h>
#   include <termios.h>
#endif

namespace {

// ====================================================================
// Syntax highlighting (token -> WOORT_ANSI_* color)
// ====================================================================

const char* color_for_token(wo_lspv2_lexer_token t)
{
    switch (t)
    {
    // Boolean / nil literals: bright cyan.
    case WO_LSPV2_TOKEN_NIL:
    case WO_LSPV2_TOKEN_TRUE:
    case WO_LSPV2_TOKEN_FALSE:
        return WOORT_ANSI_HIC;

    // Numeric literals: yellow.
    case WO_LSPV2_TOKEN_LITERAL_INTEGER:
    case WO_LSPV2_TOKEN_LITERAL_HANDLE:
    case WO_LSPV2_TOKEN_LITERAL_REAL:
        return WOORT_ANSI_YEL;

    // String / char literals (incl. format-string segments): green.
    case WO_LSPV2_TOKEN_LITERAL_STRING:
    case WO_LSPV2_TOKEN_LITERAL_RAW_STRING:
    case WO_LSPV2_TOKEN_LITERAL_CHAR:
    case WO_LSPV2_TOKEN_FORMAT_STRING_BEGIN:
    case WO_LSPV2_TOKEN_FORMAT_STRING:
    case WO_LSPV2_TOKEN_FORMAT_STRING_END:
        return WOORT_ANSI_GRE;

    // Comments: gray (bright black).
    case WO_LSPV2_TOKEN_LINE_COMMENT:
    case WO_LSPV2_TOKEN_BLOCK_COMMENT:
    case WO_LSPV2_TOKEN_SHEBANG_COMMENT:
        return WOORT_ANSI_GRY;

    // Lexer errors / unknown bytes: bright red.
    case WO_LSPV2_TOKEN_ERROR:
    case WO_LSPV2_TOKEN_UNKNOWN_TOKEN:
        return WOORT_ANSI_HIR;

    default:
        break;
    }

    // Remaining keywords (import..macro block, minus nil/true/false handled
    // above): bright magenta.
    if (t >= WO_LSPV2_TOKEN_IMPORT && t <= WO_LSPV2_TOKEN_MACRO)
        return WOORT_ANSI_HIM;

    // Identifiers / operators / punctuation: default (no color).
    return nullptr;
}

// Tokenize `src` with the public LSP lexer and return it wrapped in ANSI
// colors. Inter-token whitespace is emitted verbatim (uncolored) so the
// original byte layout is preserved exactly. Token columns are byte offsets
// (the lexer's column counter advances once per source byte), so slicing a
// UTF-8 std::string by [begin_col, end_col) is exact.
std::string highlight_source(std::string_view src)
{
    std::string out;
    if (src.empty())
        return out;

    std::string zterm(src);
    wo_lspv2_lexer* lex = wo_lspv2_lexer_create(zterm.c_str());
    if (lex == nullptr)
        return std::string(src);

    size_t pos = 0;
    for (;;)
    {
        wo_lspv2_token_info* ti = wo_lspv2_lexer_peek(lex);
        if (ti == nullptr)
            break;

        const wo_lspv2_lexer_token tt = ti->m_token;
        size_t b = ti->m_location.m_begin_location[1];
        size_t e = ti->m_location.m_end_location[1];
        wo_lspv2_token_info_free(ti);

        if (tt == WO_LSPV2_TOKEN_EOF)
            break;

        // Defensive clamping: never go backwards or past the end of the input.
        if (b > src.size()) b = src.size();
        if (e > src.size()) e = src.size();
        if (b < pos)        b = pos;
        if (e < b)          e = b;

        // Uncolored gap (whitespace / anything the lexer skipped).
        if (b > pos)
            out.append(src.data() + pos, b - pos);

        const char* color = color_for_token(tt);
        if (e > b)
        {
            if (color != nullptr)
            {
                out += color;
                out.append(src.data() + b, e - b);
                out += WOORT_ANSI_RST;
            }
            else
            {
                out.append(src.data() + b, e - b);
            }
        }
        pos = e;

        if (e <= b)
        {
            // Zero-width token: cannot make progress via the lexer, stop to
            // avoid an infinite loop. Any trailing bytes are emitted below.
            break;
        }

        wo_lspv2_lexer_consume(lex);
    }

    wo_lspv2_lexer_free(lex);

    if (pos < src.size())
        out.append(src.data() + pos, src.size() - pos);
    return out;
}

// ====================================================================
// UTF-8 helpers (operate on byte offsets within a std::string)
// ====================================================================

int utf8_seq_len(unsigned char b)
{
    if ((b & 0x80) == 0)   return 1;
    if ((b & 0xE0) == 0xC0) return 2;
    if ((b & 0xF0) == 0xE0) return 3;
    if ((b & 0xF8) == 0xF0) return 4;
    return 1; // invalid lead byte: treat as a single byte
}

size_t cp_len_at(const std::string& s, size_t i)
{
    if (i >= s.size())
        return 0;
    return static_cast<size_t>(utf8_seq_len(static_cast<unsigned char>(s[i])));
}

size_t prev_cp(const std::string& s, size_t cur)
{
    if (cur == 0)
        return 0;
    size_t i = cur - 1;
    while (i > 0 && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80)
        --i;
    return i;
}

// ====================================================================
// Console byte stream: thin wrapper over the woort raw input API
// (woort_console_getc / woort_console_ungetc). Provides the peek/get
// interface the key decoder expects, with a 1-deep lookahead.
// ====================================================================

struct console_in
{
    int ungot = -1;

    int peek()
    {
        if (ungot >= 0)
            return ungot;
        int c = woort_console_getc();
        if (c >= 0)
            ungot = c;
        return c;
    }

    int get()
    {
        if (ungot >= 0)
        {
            int c = ungot;
            ungot = -1;
            return c;
        }
        return woort_console_getc();
    }
};

// ====================================================================
// Raw-mode RAII guard
// ====================================================================

#if defined(_WIN32)

class raw_mode_guard
{
    bool   active_ = false;
    HANDLE hin_    = nullptr;
    DWORD  saved_  = 0;
public:
    explicit raw_mode_guard(bool enable)
    {
        if (!enable)
            return;
        hin_ = GetStdHandle(STD_INPUT_HANDLE);
        if (hin_ == INVALID_HANDLE_VALUE)
            return;
        if (!GetConsoleMode(hin_, &saved_))
            return;
        // ENABLE_VIRTUAL_TERMINAL_INPUT disables line buffering and echo and
        // delivers key events (arrows etc.) as VT sequences.
        if (!SetConsoleMode(hin_, ENABLE_VIRTUAL_TERMINAL_INPUT))
            return;
        active_ = true;
    }
    ~raw_mode_guard()
    {
        if (active_ && hin_ != INVALID_HANDLE_VALUE)
            (void)SetConsoleMode(hin_, saved_);
    }
    bool ok() const { return active_; }
};

#else // POSIX

class raw_mode_guard
{
    bool          active_ = false;
    int           fd_     = -1;
    struct termios saved_;
public:
    explicit raw_mode_guard(bool enable)
    {
        if (!enable)
            return;
        fd_ = fileno(stdin);
        if (fd_ < 0)
            return;
        if (tcgetattr(fd_, &saved_) != 0)
            return;
        struct termios raw = saved_;
        raw.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK | ISIG | IEXTEN);
        raw.c_iflag &= ~(IXON | ICRNL | INLCR);
        raw.c_cc[VMIN]  = 1;
        raw.c_cc[VTIME] = 0;
        if (tcsetattr(fd_, TCSANOW, &raw) != 0)
            return;
        active_ = true;
    }
    ~raw_mode_guard()
    {
        if (active_ && fd_ >= 0)
            (void)tcsetattr(fd_, TCSANOW, &saved_);
    }
    bool ok() const { return active_; }
};

#endif

// ====================================================================
// Key-event decoding
// ====================================================================

enum class key_kind
{
    eof,
    cancel,    // Ctrl-C
    ctrl_d,    // Ctrl-D (EOF on empty input)
    enter,
    tab,
    backspace,
    del,
    left,
    right,
    up,
    down,
    home,
    end,
    char_text,
    other,
};

struct key_event
{
    key_kind    kind = key_kind::other;
    std::string utf8;
};

key_event read_char_text(console_in& src, unsigned char lead)
{
    const int n = utf8_seq_len(lead);
    std::string s;
    s.push_back(static_cast<char>(lead));
    for (int i = 1; i < n; ++i)
    {
        int c = src.get();
        if (c < 0)
            break; // truncated at end-of-stream: keep what we have
        s.push_back(static_cast<char>(c));
    }
    return key_event{ key_kind::char_text, std::move(s) };
}

key_event decode_escape(console_in& src)
{
    int c1 = src.peek();
    if (c1 < 0)
        return key_event{ key_kind::other, {} }; // lone ESC

    if (c1 != '[' && c1 != 'O')
    {
        // ESC + char (e.g. Alt+key): consume and ignore.
        (void)src.get();
        return key_event{ key_kind::other, {} };
    }
    (void)src.get(); // consume '[' or 'O'

    int c2 = src.get();
    if (c2 < 0)
        return key_event{ key_kind::other, {} };

    if (c1 == '[')
    {
        switch (c2)
        {
        case 'C': return key_event{ key_kind::right, {} };
        case 'D': return key_event{ key_kind::left,  {} };
        case 'A': return key_event{ key_kind::up,    {} };
        case 'B': return key_event{ key_kind::down,  {} };
        case 'H': return key_event{ key_kind::home,  {} };
        case 'F': return key_event{ key_kind::end,   {} };
        case '3': // CSI 3 ~  -> Delete
            (void)src.get();
            return key_event{ key_kind::del, {} };
        case '1': case '7': // CSI 1 ~ / 7 ~  -> Home
            (void)src.get();
            return key_event{ key_kind::home, {} };
        case '4': case '8': // CSI 4 ~ / 8 ~  -> End
            (void)src.get();
            return key_event{ key_kind::end, {} };
        default:
            return key_event{ key_kind::other, {} };
        }
    }
    // c1 == 'O' (SS3 sequences)
    switch (c2)
    {
    case 'C': return key_event{ key_kind::right, {} };
    case 'D': return key_event{ key_kind::left,  {} };
    case 'A': return key_event{ key_kind::up,    {} };
    case 'B': return key_event{ key_kind::down,  {} };
    case 'H': return key_event{ key_kind::home,  {} };
    case 'F': return key_event{ key_kind::end,   {} };
    default:  return key_event{ key_kind::other, {} };
    }
}

key_event next_key(console_in& src)
{
    int b = src.get();
    if (b < 0)
        return key_event{ key_kind::eof, {} };

    switch (b)
    {
    case '\n':
    case '\r':
        return key_event{ key_kind::enter, {} };
    case '\t':
        return key_event{ key_kind::tab, {} };
    case 0x03: // Ctrl-C
        return key_event{ key_kind::cancel, {} };
    case 0x04: // Ctrl-D
        return key_event{ key_kind::ctrl_d, {} };
    case 0x7F:
    case 0x08: // Backspace / Ctrl-H
        return key_event{ key_kind::backspace, {} };
    case 0x01: // Ctrl-A -> Home
        return key_event{ key_kind::home, {} };
    case 0x05: // Ctrl-E -> End
        return key_event{ key_kind::end, {} };
    case 0x1B: // ESC
        return decode_escape(src);
    default:
        break;
    }

    if (b < 0x20)
        return key_event{ key_kind::other, {} }; // other control chars: ignore

    return read_char_text(src, static_cast<unsigned char>(b));
}

} // namespace

// ====================================================================
// Public API
// ====================================================================

std::string wo_repl_render_highlight(std::string_view src)
{
    return highlight_source(src);
}

std::optional<std::string> wo_repl_live_readline(
    std::string_view prompt, std::vector<std::string>& history)
{
    raw_mode_guard raw(true);
    if (!raw.ok())
        return std::nullopt; // caller falls back to the plain reader

    const bool   main_prompt = !prompt.empty() && prompt[0] == '>';
    const char*  prompt_color = main_prompt ? WOORT_ANSI_HIB : WOORT_ANSI_BLU;

    std::string  buf;   // input bytes
    size_t       cur = 0; // cursor byte offset
    console_in   src;

    // History navigation state. hist_idx == history.size() means the user is
    // editing a fresh line; draft preserves whatever they had typed before
    // they first pressed Up.
    size_t       hist_idx = history.size();
    std::string  draft;

    const auto render = [&]()
    {
        // \r: back to column 0; \033[K: erase to end-of-line (clears stale tail).
        std::cout << "\r\033[K" << prompt_color << prompt << WOORT_ANSI_RST;
        std::cout << highlight_source(buf);
        // Absolute cursor column (1-based). Byte-based: exact for ASCII,
        // approximate for wide characters.
        std::cout << "\033[" << (prompt.size() + cur + 1) << 'G' << std::flush;
    };

    render();
    for (;;)
    {
        const key_event k = next_key(src);
        switch (k.kind)
        {
        case key_kind::eof:
            // "\r\n": Windows VT output does not translate LF to CRLF the way
            // POSIX OPOST does, so emit CR explicitly to return to column 0.
            std::cout << "\r\n" << std::flush;
            if (buf.empty())
                return std::nullopt;
            return buf;

        case key_kind::ctrl_d:
            if (buf.empty())
            {
                std::cout << "\r\n" << std::flush;
                return std::nullopt;
            }
            break; // ignore Ctrl-D on non-empty input

        case key_kind::cancel: // Ctrl-C: clear current input, stay in editor.
            buf.clear();
            cur = 0;
            std::cout << "^C\r\n" << std::flush;
            render();
            continue;

        case key_kind::enter:
        {
            std::cout << "\r\n" << std::flush;

            // Decide whether the submitted line becomes a new history entry.
            // Skip: blank lines, lines recalled from history and resubmitted
            // unchanged (even non-consecutive), and consecutive duplicates.
            const bool blank =
                (buf.find_first_not_of(" \t") == std::string::npos);
            const bool recalled_unchanged =
                (hist_idx < history.size() && buf == history[hist_idx]);
            const bool dup_of_last =
                (!history.empty() && buf == history.back());
            if (!blank && !recalled_unchanged && !dup_of_last)
                history.push_back(buf);

            return buf;
        }

        case key_kind::backspace:
            if (cur > 0)
            {
                const size_t p = prev_cp(buf, cur);
                buf.erase(p, cur - p);
                cur = p;
                render();
            }
            break;

        case key_kind::del:
            if (cur < buf.size())
            {
                buf.erase(cur, cp_len_at(buf, cur));
                render();
            }
            break;

        case key_kind::left:
            if (cur > 0)
            {
                cur = prev_cp(buf, cur);
                render();
            }
            break;

        case key_kind::right:
            if (cur < buf.size())
            {
                cur += cp_len_at(buf, cur);
                render();
            }
            break;

        case key_kind::home:
            cur = 0;
            render();
            break;

        case key_kind::end:
            cur = buf.size();
            render();
            break;

        case key_kind::tab:
        {
            static const std::string four_spaces = "    ";
            buf.insert(cur, four_spaces);
            cur += four_spaces.size();
            render();
            break;
        }

        case key_kind::up:
            if (!history.empty())
            {
                if (hist_idx == history.size())
                    draft = buf; // first Up: preserve the in-progress line
                if (hist_idx > 0)
                {
                    --hist_idx;
                    buf = history[hist_idx];
                    cur = buf.size();
                    render();
                }
            }
            break;

        case key_kind::down:
            if (!history.empty() && hist_idx < history.size())
            {
                ++hist_idx;
                if (hist_idx == history.size())
                    buf = draft; // past newest: restore the saved draft
                else
                    buf = history[hist_idx];
                cur = buf.size();
                render();
            }
            break;

        case key_kind::char_text:
            if (!k.utf8.empty())
            {
                buf.insert(cur, k.utf8);
                cur += k.utf8.size();
                render();
            }
            break;

        case key_kind::other:
            break;
        }
    }
}
