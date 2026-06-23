// Live syntax-highlighting line editor for the Woolang REPL.
//
// Re-scans the current input with a small self-contained Woolang lexer on every
// edit and re-renders `prompt + colored(input)`. Input is read in raw mode:
// POSIX uses termios, Windows uses ENABLE_VIRTUAL_TERMINAL_INPUT (which
// delivers arrow keys etc. as VT escape sequences), so both platforms share one
// escape decoder.

#define NOMINMAX

#include "wo_repl_common.hpp"
#include "wo_repl_editor.hpp"

#include <cctype>
#include <cstdio>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_set>

#if defined(_WIN32)
#   include <windows.h>
#else
#   include <unistd.h>
#   include <termios.h>
#endif

namespace {

// ====================================================================
// Syntax highlighting: a small self-contained Woolang scanner.
//
// Categories -> WOORT_ANSI_* (defined in woort.h):
//   keywords            -> HIM (bright magenta)
//   true/false/nil      -> HIC (bright cyan)
//   numeric literals    -> YEL (yellow)
//   string/char/format  -> GRE (green)
//   comments            -> GRY (gray)
//   identifiers/op/punct-> default (no color)
// Whitespace between tokens is emitted verbatim so the byte layout is
// preserved. The scanner is intentionally lenient: it only needs to be good
// enough for coloring, not to reject invalid programs.
// ====================================================================

const std::unordered_set<std::string>& keyword_table()
{
    static const std::unordered_set<std::string> kw = {
        "import","export","while","if","else","namespace","for","extern",
        "let","mut","func","return","using","alias","enum","as","is","typeof",
        "private","public","protected","static","break","continue","lambda",
        "do","where","operator","union","match","struct","immut","typeid",
        "defer","macro"
    };
    return kw;
}

inline bool hl_is_ident_beg(unsigned char c)
{
    return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c >= 0x80;
}

inline bool hl_is_ident(unsigned char c)
{
    return hl_is_ident_beg(c) || (c >= '0' && c <= '9');
}

inline bool hl_is_digit(unsigned char c) { return c >= '0' && c <= '9'; }

std::string highlight_source(std::string_view src)
{
    std::string out;
    const size_t n = src.size();
    size_t i = 0;

    const auto plain = [&](size_t a, size_t b) {
        if (b > a) out.append(src.data() + a, b - a);
    };
    const auto colored = [&](size_t a, size_t b, const char* col) {
        if (b > a) { out += col; out.append(src.data() + a, b - a); out += WOORT_ANSI_RST; }
    };

    while (i < n)
    {
        const unsigned char c = static_cast<unsigned char>(src[i]);

        // Whitespace.
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
        {
            const size_t s = i++;
            while (i < n)
            {
                const unsigned char d = static_cast<unsigned char>(src[i]);
                if (d == ' ' || d == '\t' || d == '\r' || d == '\n') ++i; else break;
            }
            plain(s, i);
            continue;
        }

        // Line comment.
        if (c == '/' && i + 1 < n && src[i + 1] == '/')
        {
            const size_t s = i; i += 2;
            while (i < n && src[i] != '\n') ++i;
            colored(s, i, WOORT_ANSI_GRY);
            continue;
        }

        // Block comment.
        if (c == '/' && i + 1 < n && src[i + 1] == '*')
        {
            const size_t s = i; i += 2;
            while (i + 1 < n && !(src[i] == '*' && src[i + 1] == '/')) ++i;
            i = (i + 1 < n) ? i + 2 : n;
            colored(s, i, WOORT_ANSI_GRY);
            continue;
        }

        // String literal "..." (with escapes).
        if (c == '"')
        {
            const size_t s = i; ++i;
            while (i < n)
            {
                if (src[i] == '\\') { i += 2; continue; }
                if (src[i] == '"') { ++i; break; }
                ++i;
            }
            colored(s, i, WOORT_ANSI_GRE);
            continue;
        }

        // Char literal '...' (with escapes).
        if (c == '\'')
        {
            const size_t s = i; ++i;
            while (i < n)
            {
                if (src[i] == '\\') { i += 2; continue; }
                if (src[i] == '\'') { ++i; break; }
                ++i;
            }
            colored(s, i, WOORT_ANSI_GRE);
            continue;
        }

        // Format string F"..." / f"...".
        if ((c == 'F' || c == 'f') && i + 1 < n && src[i + 1] == '"')
        {
            const size_t s = i; i += 2;
            while (i < n)
            {
                if (src[i] == '\\') { i += 2; continue; }
                if (src[i] == '"') { ++i; break; }
                ++i;
            }
            colored(s, i, WOORT_ANSI_GRE);
            continue;
        }

        // Numeric literal.
        if (hl_is_digit(c))
        {
            const size_t s = i;
            if (src[i] == '0' && i + 1 < n &&
                (src[i + 1] == 'x' || src[i + 1] == 'X' ||
                 src[i + 1] == 'b' || src[i + 1] == 'B' ||
                 src[i + 1] == 'o' || src[i + 1] == 'O'))
            {
                i += 2;
                while (i < n &&
                       (std::isalnum(static_cast<unsigned char>(src[i])) || src[i] == '_'))
                    ++i;
            }
            else
            {
                while (i < n &&
                       (hl_is_digit(static_cast<unsigned char>(src[i])) || src[i] == '_'))
                    ++i;
                if (i < n && src[i] == '.')
                {
                    ++i;
                    while (i < n &&
                           (hl_is_digit(static_cast<unsigned char>(src[i])) || src[i] == '_'))
                        ++i;
                }
            }
            if (i < n && (src[i] == 'L' || src[i] == 'l')) ++i; // handle literal suffix
            colored(s, i, WOORT_ANSI_YEL);
            continue;
        }

        // Identifier / keyword. ('@' is the l_at keyword token.)
        if (hl_is_ident_beg(c))
        {
            const size_t s = i++;
            while (i < n && hl_is_ident(static_cast<unsigned char>(src[i]))) ++i;
            const std::string_view tok = src.substr(s, i - s);
            if (tok == "true" || tok == "false" || tok == "nil")
                colored(s, i, WOORT_ANSI_HIC);
            else if (keyword_table().count(std::string(tok)))
                colored(s, i, WOORT_ANSI_HIM);
            else
                plain(s, i);
            continue;
        }

        if (c == '@') // l_at keyword
        {
            colored(i, i + 1, WOORT_ANSI_HIM);
            ++i;
            continue;
        }

        // Operators / punctuation: default color.
        plain(i, i + 1);
        ++i;
    }
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
// Raw console input: a blocking byte source
// ====================================================================

struct byte_source
{
    std::string buf;
    size_t  pos      = 0;
    bool    eof_flag = false;

    // Drop the consumed prefix and pull more bytes. Returns false when no more
    // input is available (end of stream).
    bool refill();

    bool ensure()
    {
        while (pos >= buf.size())
        {
            if (eof_flag)
                return false;
            if (!refill())
            {
                eof_flag = true;
                return false;
            }
        }
        return true;
    }

    int peek()
    {
        return ensure() ? static_cast<unsigned char>(buf[pos]) : -1;
    }

    int get()
    {
        int b = peek();
        if (b >= 0)
            ++pos;
        return b;
    }
};

#if defined(_WIN32)

bool byte_source::refill()
{
    if (pos > 0)
    {
        buf.erase(0, pos);
        pos = 0;
    }

    wchar_t wbuf[64];
    DWORD   got = 0;
    HANDLE  hin = GetStdHandle(STD_INPUT_HANDLE);

    if (!ReadConsoleW(hin, wbuf, 64, &got, nullptr))
    {
        // Ctrl+C with processed input off still surfaces as
        // ERROR_OPERATION_ABORTED; synthesize a ^C byte so the shared decoder
        // maps it to cancel.
        if (GetLastError() == ERROR_OPERATION_ABORTED)
        {
            buf.push_back('\x03');
            return true;
        }
        return false;
    }
    if (got == 0)
        return false;

    int len = WideCharToMultiByte(CP_UTF8, 0, wbuf, static_cast<int>(got),
                                  nullptr, 0, nullptr, nullptr);
    if (len > 0)
    {
        const size_t old = buf.size();
        buf.resize(old + static_cast<size_t>(len));
        WideCharToMultiByte(CP_UTF8, 0, wbuf, static_cast<int>(got),
                            &buf[old], len, nullptr, nullptr);
    }
    return !buf.empty();
}

#else // POSIX

bool byte_source::refill()
{
    if (pos > 0)
    {
        buf.erase(0, pos);
        pos = 0;
    }
    char tmp[64];
    ssize_t n = ::read(STDIN_FILENO, tmp, sizeof(tmp));
    if (n <= 0)
        return false;
    buf.append(tmp, static_cast<size_t>(n));
    return true;
}

#endif

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
    backspace,
    del,
    left,
    right,
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

key_event read_char_text(byte_source& src, unsigned char lead)
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

key_event decode_escape(byte_source& src)
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
        case 'A': case 'B': return key_event{ key_kind::other, {} }; // up/down: unused
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
    case 'A': case 'B': return key_event{ key_kind::other, {} };
    case 'H': return key_event{ key_kind::home,  {} };
    case 'F': return key_event{ key_kind::end,   {} };
    default:  return key_event{ key_kind::other, {} };
    }
}

key_event next_key(byte_source& src)
{
    int b = src.get();
    if (b < 0)
        return key_event{ key_kind::eof, {} };

    switch (b)
    {
    case '\n':
    case '\r':
        return key_event{ key_kind::enter, {} };
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

bool wo_repl_stdin_is_tty()
{
#if defined(_WIN32)
    HANDLE hin = GetStdHandle(STD_INPUT_HANDLE);
    return hin != INVALID_HANDLE_VALUE
        && GetFileType(hin) == FILE_TYPE_CHAR;
#else
    return isatty(fileno(stdin)) != 0;
#endif
}

std::optional<std::string> wo_repl_live_readline(std::string_view prompt)
{
    raw_mode_guard raw(true);
    if (!raw.ok())
        return std::nullopt; // caller falls back to the plain reader

    const bool   main_prompt = !prompt.empty() && prompt[0] == '>';
    const char*  prompt_color = main_prompt ? WOORT_ANSI_HIB : WOORT_ANSI_BLU;

    std::string  buf;   // input bytes
    size_t       cur = 0; // cursor byte offset
    byte_source  src;

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
            std::cout << "\r\n" << std::flush;
            return buf;

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
