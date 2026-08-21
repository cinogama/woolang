#define WOODYN_IMPL

#define WO_IMPL
#include "wo.h"

#include "wo_wodyn_impl.hpp"

wo_woort_env_locale_name_f_t wo_get_woort_env_locale_name(void)
{
    return woort_env_locale_name;
}
wo_woort_dylib_load_f_t wo_get_woort_dylib_load(void)
{
    return woort_dylib_load;
}
wo_woort_dylib_load_func_f_t wo_get_woort_dylib_load_func(void)
{
    return woort_dylib_load_func;
}
wo_woort_dylib_unload_f_t wo_get_woort_dylib_unload(void)
{
    return woort_dylib_unload;
}

namespace wo::woodyn
{
    void try_init_woodyn(
        std::optional<const char*> specify_woort_name,
        wo_woort_dylib_load_f_t dylib_loader,
        wo_woort_dylib_load_func_f_t function_loader,
        wo_woort_dylib_unload_f_t dylib_unloader)
    {
#ifdef WOODYN
#   if defined(_WIN32)
#       define WO_DEFAULT_DYLIB_SUFFIX ".dll"
#   elif defined(__APPLE__)
#       define WO_DEFAULT_DYLIB_SUFFIX ".dylib"
#   else
#       define WO_DEFAULT_DYLIB_SUFFIX ".so"
#   endif

#   ifdef NDEBUG
#       define WO_DYLIB_DEBUG_SUFFIX ""
#   else
#       define WO_DYLIB_DEBUG_SUFFIX "_debug"
#   endif

        const char* woort_lib_name = specify_woort_name.value_or(
            "libwoort" WO_DYLIB_DEBUG_SUFFIX WO_DEFAULT_DYLIB_SUFFIX);

        woort_Dylib* const dylib = 
            dylib_loader("libwoort", woort_lib_name, nullptr, true);

        if (dylib == nullptr)
        {
            fprintf(stderr, "Failed to load woort.");
            abort();
        }

        auto const fact_woort_init_woodyn =
            static_cast<WOODYN_FUNC_TYPE_NAME(woort_init_woodyn)>(
                function_loader(dylib, "woort_init_woodyn"));

        if (fact_woort_init_woodyn == nullptr)
        {
            dylib_unloader(dylib, WOORT_DYLIB_UNREF);

            fprintf(stderr, "Incompatible woort module.");
            abort();
        }

        fact_woort_init_woodyn();

        // Ok, libwoort_woodyn is ready.
        auto const fact_woort_init_woodyn =
            static_cast<WOODYN_FUNC_TYPE_NAME(woort_init_woodyn)>(
                function_loader(dylib, "woort_init_woodyn"));

#endif
    }
}


