#pragma once

#include <optional>

namespace wo::woodyn
{    
    void bootup_woort_dynamically(
        int argc,
        char** argv,
        std::optional<const char*> specify_woort_name,
        wo_woort_dylib_load_f_t dylib_loader,
        wo_woort_dylib_load_func_f_t function_loader,
        wo_woort_dylib_unload_f_t dylib_unloader);

    void shutdown_woort_dynamically(
        void(*do_after_shutdown)(void*), void* custom_data);
}