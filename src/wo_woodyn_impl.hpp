#pragma once

#include <optional>

namespace wo::woodyn
{
    void bootup_woort_dynamically(
        int argc,
        char** argv,
        std::optional<const char*> specify_woort_name,
        wo_dylib_loader_t dylib_loader,
        wo_dylib_func_loader_t function_loader,
        wo_dylib_unloader_t dylib_unloader);

    void shutdown_woort_dynamically(
        void(*do_after_shutdown)(void*), void* custom_data);
}