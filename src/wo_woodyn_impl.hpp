#pragma once

#include <optional>

namespace wo::woodyn
{
    void bootup_woort_dynamically(
        int argc,
        char** argv,
        std::optional<const wo_WooDyn_Functions*> funcs);

    void shutdown_woort_dynamically(
        void(*do_after_shutdown)(void*), void* custom_data);
}