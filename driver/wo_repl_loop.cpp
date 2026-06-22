#include "wo.h"

#include <iostream>
#include <string>

namespace {

void PrintReplBanner()
{
    std::cout << "Woolang REPL (c) 2021 Cinogama project.\n";
    std::cout << "Version: " << wo_version() << '\n';
    std::cout << "Type :q to exit.\n\n";
}

int RunRepl()
{
    PrintReplBanner();

    wo_ReplSession* session = wo_repl_create();
    if (session == nullptr)
    {
        std::cerr << "Failed to create REPL session.\n";
        return -3;
    }

    std::string line;
    std::string buffer;

    while (true)
    {
        // Prompt: ">>> " for new input, "... " for continuation.
        std::cout << (buffer.empty() ? ">>> " : "... ");
        std::cout.flush();

        if (!std::getline(std::cin, line))
        {
            // EOF.
            std::cout << '\n';
            break;
        }

        // Exit command.
        if (buffer.empty() && (line == ":q" || line == ":quit"))
            break;

        // Append to buffer.
        if (!buffer.empty())
            buffer += '\n';
        buffer += line;

        // Try to evaluate.
        wo_CompileErrors* errors = nullptr;
        wo_repl_result result = wo_repl_eval(session, buffer.c_str(), &errors);

        if (result == WO_REPL_INCOMPLETE_INPUT)
        {
            // Need more input — keep accumulating.
            if (errors)
                wo_compile_errors_free(errors);
            continue;
        }

        if (result == WO_REPL_COMPILE_ERROR)
        {
            if (errors != nullptr)
            {
                std::cerr << wo_get_compile_error(errors, WO_COLORFUL) << '\n';
                wo_compile_errors_free(errors);
            }
        }
        else if (result == WO_REPL_RUNTIME_ERROR)
        {
            std::cerr << "Runtime error occurred.\n";
            if (errors)
                wo_compile_errors_free(errors);
        }
        else if (result == WO_REPL_OK)
        {
            if (errors)
                wo_compile_errors_free(errors);
        }

        // Clear buffer for next input.
        buffer.clear();
    }

    // Show session bindings.
    size_t count = wo_repl_binding_count(session);
    if (count > 0)
    {
        std::cout << "\nSession bindings:\n";
        for (size_t i = 0; i < count; ++i)
            std::cout << "  " << wo_repl_binding_name(session, i) << '\n';
    }

    wo_repl_destroy(session);
    return 0;
}

} // namespace

// Called from main() in wo_driver.cpp when no source file is given.
int wo_driver_run_repl()
{
    return RunRepl();
}
