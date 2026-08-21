#define WO_IMPL
#define WOODYN_IMPL

#include "wo.h"

#include "wo_woodyn_impl.hpp"

#include <clocale>

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
#ifdef WOODYN
    static struct _woodyn_Context
    {
        woort_Dylib*                            m_dylib;
        WOODYN_FUNC_TYPE_NAME(woort_shutdown)   m_shutdown;
        wo_woort_dylib_unload_f_t               m_unloader;
    } _s_dyn_ctx;
    /*static woort_Dylib* _s_holding_raw_dylib;
    wo_woort_dylib_unload_f_t _s_raw_dylib_free_func;*/
#endif

    void bootup_woort_dynamically(
        int argc,
        char** argv,
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

        auto const fact_woort_init =
            static_cast<WOODYN_FUNC_TYPE_NAME(woort_init)>(
                function_loader(dylib, "woort_init"));
        auto const fact_woort_shutdown =
            static_cast<WOODYN_FUNC_TYPE_NAME(woort_shutdown)>(
                function_loader(dylib, "woort_shutdown"));

        if (fact_woort_init == nullptr
            || fact_woort_shutdown == nullptr)
        {
            dylib_unloader(dylib, WOORT_DYLIB_UNREF);

            fprintf(stderr, "Incompatible woort module.");
            abort();
        }

        // Init woort.
        fact_woort_init(argc, argv);

        // Ok, libwoort_woodyn is ready.
        auto const fact_woort_dylib_load =
            static_cast<WOODYN_FUNC_TYPE_NAME(woort_dylib_load)>(
                function_loader(dylib, "woort_dylib_load"));

        auto const fact_woort_dylib_load_func =
            static_cast<WOODYN_FUNC_TYPE_NAME(woort_dylib_load_func)>(
                function_loader(dylib, "woort_dylib_load_func"));

        auto const fact_woort_dylib_unload =
            static_cast<WOODYN_FUNC_TYPE_NAME(woort_dylib_unload)>(
                function_loader(dylib, "woort_dylib_unload"));

        woodyn_woort_entry(
            fact_woort_dylib_load, 
            fact_woort_dylib_load_func, 
            fact_woort_dylib_unload);

        // We can use woort-api now, init locale.
        setlocale(LC_CTYPE, woort_env_locale_name());

        _s_dyn_ctx.m_dylib = dylib;
        _s_dyn_ctx.m_shutdown = fact_woort_shutdown;
        _s_dyn_ctx.m_unloader = dylib_unloader;
#else
        woort_init(argc, argv);
#endif
    }

    void shutdown_woort_dynamically(void(*do_after_shutdown)(void*), void* custom_data)
    {
#ifdef WOODYN
        woodyn_woort_leave();
        _s_dyn_ctx.m_shutdown(do_after_shutdown, custom_data);
        _s_dyn_ctx.m_unloader(_s_dyn_ctx.m_dylib, WOORT_DYLIB_UNREF);

        _s_dyn_ctx.m_dylib = nullptr;
        _s_dyn_ctx.m_shutdown = nullptr;
        _s_dyn_ctx.m_unloader = nullptr;
#else
        woort_shutdown(do_after_shutdown, custom_data);
#endif
    }
}


