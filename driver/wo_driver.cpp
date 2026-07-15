#include "wo.h"

#include "wo_repl_editor.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <cassert>
#include <vector>

namespace {

constexpr int EXIT_OK              = 0;
constexpr int EXIT_RUNTIME_FAILED  = -1;
constexpr int EXIT_COMPILE_FAILED  = -2;
constexpr int EXIT_INTERNAL_ERROR  = -3;

struct wo_driver_options {
    const char* source_path = nullptr;
    const char* output_path = nullptr;
    bool        check_only  = false;
    bool        force_repl  = false;
};

void _wo_driver_show_banner()
{
    std::cout << "Woolang (c) 2021 Cinogama project.\n\n";
    std::cout << "Compiler version: " << wo_version() << '\n';
    std::cout << "Runtime version: " << woort_version() << '\n';
    std::cout << "Commit:  " << wo_commit_sha() << '\n';
    std::cout << "Date:    " << wo_compile_date() << "\n\n";
    std::cout << "Usage:\n";
    std::cout << "    woolang\n";
    std::cout << "    woolang -h\n";
    std::cout << "    woolang --help\n";
    std::cout << "    woolang <path> [options...]\n";
    std::cout << "\nOptions:\n";
    std::cout << "    -o <output>   Compile and save bytecode to the given path.\n";
    std::cout << "    --check   Compile only, report syntax errors without running.\n";
    std::cout << "    --repl         Always enter REPL mode, ignoring <path>.\n";
    std::cout << "    -h, --help    Show this help message.\n";
    std::cout << '\n';
    wo_print_compiler_help();
    std::cout << '\n';
    woort_print_runtime_help();
}

bool _wo_driver_parse_option(int argc, char** argv, wo_driver_options* out)
{
    for (int i = 1; i < argc; ++i)
    {
        const std::string_view arg = argv[i];
        if (arg.empty())
            continue;

        if (arg == "-o")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "error: -o requires an output path\n";
                return false;
            }
            out->output_path = argv[++i];
        }
        else if (arg == "-h" || arg == "--help")
        {
            _wo_driver_show_banner();
            std::exit(EXIT_OK);
        }
        else if (arg == "-c" || arg == "--check")
        {
            out->check_only = true;
        }
        else if (arg == "--repl")
        {
            out->force_repl = true;
        }
        else if (out->source_path == nullptr)
        {
            out->source_path = argv[i];
        }
    }
    return true;
}

int _wo_driver_save_binary(const char* path, woort_CodeEnv* cenv)
{
    void* buf = nullptr;
    size_t len  = 0;
    if (!woort_CodeEnv_save_binary(cenv, &buf, &len))
        return EXIT_INTERNAL_ERROR;

    struct BufferGuard
    {
        void* p;
        ~BufferGuard() { woort_free(p); }
    } guard{buf};

    FILE* const f = std::fopen(path, "wb");
    if (f == nullptr)
        return errno != 0 ? errno : EXIT_INTERNAL_ERROR;

    int ret = EXIT_OK;
    if (std::fwrite(buf, 1, len, f) != len)
        ret = errno != 0 ? errno : EXIT_INTERNAL_ERROR;
    std::fclose(f);
    return ret;
}

int _wo_driver_run_program(woort_CodeEnv* cenv)
{
    woort_vm* const vm = woort_vm_create();
    if (vm == nullptr)
    {
        std::cerr << "Failed to create VM instance: out of memory.\n";
        return EXIT_INTERNAL_ERROR;
    }

    int ret;
    (void)woort_vm_swap(vm);

    woort_value v;
    if (!woort_push_reserve(1, &v))
    {
        std::cerr << "Failed to run: cannot reserve stack.\n";
        ret = EXIT_INTERNAL_ERROR;
    }
    else if (woort_bootup(v, cenv, true) == WOORT_VM_CALL_STATUS_NORMAL)
    {
        ret = static_cast<int>(woort_int(v));
    }
    else
    {
        ret = EXIT_RUNTIME_FAILED;
    }

    (void)woort_vm_swap(nullptr);
    woort_vm_close(vm);
    return ret;
}

int _wo_driver_run_repl()
{
    std::cout << "Woolang (c) 2021 Cinogama project.\n";
    std::cout << "Woolang REPL driver(c) 2026 Cinogama project.\n";
    std::cout << "Compiler version: " << wo_version() << '\n';
    std::cout << "Runtime version: " << woort_version() << '\n';

    std::cout << "\nType :help for available commands, :q to exit.\n\n";

    wo_ReplSession* session = wo_repl_create(nullptr);
    if (session == nullptr)
    {
        std::cerr << "Failed to create REPL session.\n";
        return -3;
    }

    std::string line;
    std::string buffer;

    // Command history for Up/Down recall (in-memory, current session only).
    std::vector<std::string> history;

    while (true)
    {
        const std::string prompt = buffer.empty() ? ">>> " : "... ";

        bool eof = false;

        // Interactive consoles get a live syntax-highlighting line editor; piped
        // or redirected stdin falls back to the plain blocking reader so automation
        // and tests keep working without ANSI noise.
        if (woort_stdin_isatty())
        {
            std::optional<std::string> got = wo_repl_live_readline(prompt, history);
            if (got.has_value())
                line = std::move(*got);
            else
                eof = true;
        }
        else
        {
            std::cout << prompt;
            std::cout.flush();

            char* const raw = woort_console_readline();
            if (raw == nullptr)
            {
                // EOF or read error.
                std::cout << '\n';
                eof = true;
            }
            else
            {
                line = raw;
                woort_free(raw);
            }
        }

        if (eof)
            break;

        // Trim trailing whitespace only; preserve leading whitespace so
        // indented input (e.g. continuation lines) reaches the compiler
        // verbatim. A whitespace-only line still collapses to empty.
        const auto rtrim = line.find_last_not_of(" \t\r\n");
        if (rtrim == std::string::npos)
            line.clear();
        else
            line.erase(rtrim + 1);

        // Colon commands — work in both normal and continuation mode.
        // In continuation mode they discard the pending multi-line buffer.
        if (line == ":q" || line == ":quit")
            break;

        if (line == ":help" || line == ":h" || line == ":?")
        {
            buffer.clear();
            std::cout << "REPL commands:\n"
                         "  :q, :quit      Exit the REPL.\n"
                         "  :help, :h, :?  Show this help.\n"
                         "  :cls, :clear   Clear the screen.\n"
                         "  :reset         Reset session (clear variables/state).\n";
            continue;
        }

        if (line == ":cls" || line == ":clear")
        {
            // ANSI clear-screen + cursor home. VT output processing is enabled
            // in woort_env bootup (Windows) and native on POSIX.
            buffer.clear();
            std::cout << "\033[2J\033[H" << std::flush;
            continue;
        }

        if (line == ":reset")
        {
            // Destroy and recreate the session: wipes variables, imports,
            // compiler/VM state. Command history is preserved.
            buffer.clear();
            wo_repl_destroy(session);
            session = wo_repl_create(nullptr);
            if (session == nullptr)
            {
                std::cerr << "Failed to recreate REPL session.\n";
                return -3;
            }
            std::cout << "Session reset.\n";
            continue;
        }

        // Skip empty lines when not in continuation mode.
        if (buffer.empty() && line.empty())
            continue;

        // Append to buffer.
        if (!buffer.empty())
            buffer += '\n';
        buffer += line;

        // Try to evaluate.
        wo_CompileErrors* errors = nullptr;
        const wo_repl_result result = wo_repl_eval(session, buffer.c_str(), &errors);

        if (result == WO_REPL_INCOMPLETE_INPUT)
        {
            // Need more input — keep accumulating.
            if (errors)
                wo_compile_errors_free(errors);
            continue;
        }

        assert(errors != nullptr || result != WO_REPL_COMPILE_ERROR);
        if (result == WO_REPL_COMPILE_ERROR)
        {
            std::cerr << wo_get_compile_error(errors, WO_COLORFUL) << std::endl;
            wo_compile_errors_free(errors);
        }
        else if (result == WO_REPL_RUNTIME_ERROR)
        {
            std::cerr << "Execution has been terminated." << std::endl;
        }
        else if (result == WO_REPL_OK)
        {
            // Do nothing.
        }

        // Clear buffer for next input.
        buffer.clear();
    }

    wo_repl_destroy(session);
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    wo_init(argc, argv);

    wo_driver_options opts;
    if (!_wo_driver_parse_option(argc, argv, &opts))
    {
        _wo_driver_show_banner();
        wo_finish(nullptr, nullptr);
        return EXIT_OK;
    }

    if (opts.source_path == nullptr || opts.force_repl)
    {
        // No source file (or -repl requested): enter REPL mode.
        int repl_ret = _wo_driver_run_repl();
        wo_finish(nullptr, nullptr);
        return repl_ret;
    }

    int ret;
    wo_CompileErrors* errors = nullptr;
    woort_CodeEnv* const cenv = wo_load_file(opts.source_path, &errors);

    if (cenv == nullptr)
    {
        if (errors != nullptr)
        {
            std::cerr << wo_get_compile_error(errors, WO_COLORFUL) << '\n';
            wo_compile_errors_free(errors);
        }
        ret = EXIT_COMPILE_FAILED;
    }
    else
    {
        if (opts.check_only)
        {
            std::cout << "No syntax errors detected.\n";
            ret = EXIT_OK;
        }
        else
        {
            ret = opts.output_path != nullptr
                ? _wo_driver_save_binary(opts.output_path, cenv)
                : _wo_driver_run_program(cenv);
        }
        woort_codeenv_drop(cenv);
    }

    wo_finish(nullptr, nullptr);
    return ret;
}
