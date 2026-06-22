#include "wo.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

constexpr int EXIT_OK              = 0;
constexpr int EXIT_RUNTIME_FAILED  = -1;
constexpr int EXIT_COMPILE_FAILED  = -2;
constexpr int EXIT_INTERNAL_ERROR  = -3;

struct CliOptions {
    const char* source_path = nullptr;
    const char* output_path = nullptr;
    bool        check_only  = false;
};

void PrintBanner()
{
    std::cout << "Woolang (c) 2021 Cinogama project.\n\n";
    std::cout << "Version: " << wo_version() << '\n';
    std::cout << "Commit:  " << wo_commit_sha() << '\n';
    std::cout << "Date:    " << wo_compile_date() << "\n\n";
    std::cout << "Usage:\n";
    std::cout << "    woolang -h\n";
    std::cout << "    woolang --help\n";
    std::cout << "    woolang <path> [options...]\n";
    std::cout << "\nOptions:\n";
    std::cout << "    -o <output>   Compile and save bytecode to the given path.\n";
    std::cout << "    --check       Compile only, report syntax errors without running.\n";
    std::cout << "    -h, --help    Show this help message.\n";
    std::cout << '\n';
    wo_print_compiler_help();
    std::cout << '\n';
    woort_print_runtime_help();
}

bool ParseOptions(int argc, char** argv, CliOptions& out)
{
    for (int i = 1; i < argc; ++i)
    {
        std::string_view arg = argv[i];
        if (arg.empty())
            continue;

        if (arg == "-o")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "error: -o requires an output path\n";
                return false;
            }
            out.output_path = argv[++i];
        }
        else if (arg == "-h" || arg == "--help")
        {
            PrintBanner();
            std::exit(EXIT_OK);
        }
        else if (arg == "--check")
        {
            out.check_only = true;
        }
        else if (out.source_path == nullptr)
        {
            out.source_path = argv[i];
        }
        else
        {
            std::cerr << "error: multiple source paths specified\n";
            return false;
        }
    }
    return true;
}

int SaveBinary(const char* path, woort_CodeEnv* cenv)
{
    void*  buf = nullptr;
    size_t len  = 0;
    if (!woort_CodeEnv_save_binary(cenv, &buf, &len))
        return EXIT_INTERNAL_ERROR;

    struct BufferGuard
    {
        void* p;
        ~BufferGuard() { woort_free(p); }
    } guard{buf};

    FILE* f = std::fopen(path, "wb");
    if (f == nullptr)
        return errno != 0 ? errno : EXIT_INTERNAL_ERROR;

    int ret = EXIT_OK;
    if (std::fwrite(buf, 1, len, f) != len)
        ret = errno != 0 ? errno : EXIT_INTERNAL_ERROR;
    std::fclose(f);
    return ret;
}

int RunProgram(woort_CodeEnv* cenv)
{
    woort_vm* vm = woort_vm_create();
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
    else if (woort_bootup_codeenv(v, cenv) == WOORT_VM_CALL_STATUS_NORMAL)
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

} // namespace

// Defined in wo_repl_loop.cpp.
int wo_driver_run_repl();

int main(int argc, char** argv)
{
    wo_init(argc, argv);

    CliOptions opts;
    if (!ParseOptions(argc, argv, opts))
    {
        PrintBanner();
        wo_finish(nullptr, nullptr);
        return EXIT_OK;
    }

    if (opts.source_path == nullptr)
    {
        // No source file: enter REPL mode.
        int repl_ret = wo_driver_run_repl();
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
                ? SaveBinary(opts.output_path, cenv)
                : RunProgram(cenv);
        }
        woort_codeenv_drop(cenv);
    }

    wo_finish(nullptr, nullptr);
    return ret;
}
