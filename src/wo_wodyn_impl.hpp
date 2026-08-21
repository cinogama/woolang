#pragma once

#include <optional>

namespace wo::woodyn
{
    void try_init_woodyn(
        std::optional<const char*> specify_woort_name,
        wo_woort_dylib_load_f_t dylib_loader,
        wo_woort_dylib_load_func_f_t function_loader,
        wo_woort_dylib_unload_f_t dylib_unloader);
}