#define WO_IMPL
#define WOODYN_IMPL

#include "wo.h"

#include "wo_assert.hpp"
#include "wo_woodyn_impl.hpp"

#include <clocale>
#include <string>

#if WO_OS_DYLIB_ENABLED
#   if defined(_WIN32)
#       ifndef WIN32_LEAN_AND_MEAN
#           define WIN32_LEAN_AND_MEAN
#       endif
#       ifndef NOMINMAX
#           define NOMINMAX
#       endif
#       include <windows.h>
#       include <cstdlib>
#   elif defined(__APPLE__) || defined(__unix__) || defined(__HAIKU__)
#       include <dlfcn.h>
#       include <sys/stat.h>
#       include <unistd.h>
#       if defined(__APPLE__)
#           include <mach-o/dyld.h>
#       elif defined(__FreeBSD__)
#           include <sys/sysctl.h>
#       elif defined(__HAIKU__)
#           include <FindDirectory.h>
#           include <image.h>
#       endif
#   else
#       error "wo: no OS dylib interface for this platform, disable WO_ENABLE_OS_DYLIB_LOADER."
#   endif
#endif

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
#ifdef WOODYN_WOORT
    static wo_WooDyn_Functions s_woort_functions;
    static void* s_loaded_os_dylib;

#   if WO_OS_DYLIB_ENABLED
    // Does a regular file (or a symlink to one) exist at the given path?
    static bool _s_file_exists(const char* path)
    {
#       if defined(_WIN32)
        const DWORD attr = GetFileAttributesA(path);
        return attr != INVALID_FILE_ATTRIBUTES
            && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
#       else
        struct stat st;
        return stat(path, &st) == 0 && S_ISREG(st.st_mode);
#       endif
    }

    // Directory that contains the running executable, with a trailing
    // separator; empty when it cannot be determined.
    static std::string _s_executable_dir()
    {
        std::string exe_path;
#       if defined(_WIN32)
        char buffer[4096];
        const DWORD len = GetModuleFileNameA(nullptr, buffer, (DWORD)sizeof(buffer));
        if (len > 0 && len < sizeof(buffer))
            exe_path.assign(buffer, (size_t)len);
#       elif defined(__APPLE__)
        // Two calls: the first one only reports the required buffer size.
        uint32_t size = 0;
        if (_NSGetExecutablePath(nullptr, &size) == -1 && size > 0)
        {
            std::string buffer(size, '\0');
            if (_NSGetExecutablePath(&buffer[0], &size) == 0)
                exe_path.assign(buffer.c_str());
        }
#       elif defined(__linux__)
        char buffer[4096];
        const ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
        if (len > 0)
        {
            buffer[len] = '\0';
            exe_path.assign(buffer, (size_t)len);
        }
#       elif defined(__NetBSD__)
        char buffer[4096];
        const ssize_t len = readlink("/proc/curproc/exe", buffer, sizeof(buffer) - 1);
        if (len > 0)
        {
            buffer[len] = '\0';
            exe_path.assign(buffer, (size_t)len);
        }
#       elif defined(__FreeBSD__)
        int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };
        size_t size = 0;
        sysctl(mib, 4, nullptr, &size, nullptr, 0);
        if (size > 1)
        {
            std::string buffer(size, '\0');
            if (sysctl(mib, 4, &buffer[0], &size, nullptr, 0) == 0)
                exe_path.assign(buffer.c_str());
        }
#       elif defined(__HAIKU__)
        char buffer[B_PATH_NAME_BUFFER];
        if (find_path(B_APP_IMAGE_SYMBOL, B_FIND_PATH_IMAGE_PATH,
            nullptr, buffer, sizeof(buffer)) == B_OK)
            exe_path.assign(buffer);
#       endif

        const size_t last_sep = exe_path.find_last_of("/\\");
        if (last_sep == std::string::npos)
            return {};
        exe_path.resize(last_sep + 1);
        return exe_path;
    }

    // Absolute current working directory with a trailing separator, so the
    // load candidates are fully qualified: LoadLibraryA() re-runs its own
    // search order (application directory first) for relative paths, which
    // would defeat the cwd-first lookup order.
    static std::string _s_working_dir()
    {
#       if defined(_WIN32)
        std::string dir;
        const DWORD need = GetCurrentDirectoryA(0, nullptr);
        if (need > 0)
        {
            std::string buffer(need, '\0');
            if (GetCurrentDirectoryA(need, &buffer[0]) > 0)
                dir.assign(buffer.c_str());
        }
        if (!dir.empty())
            dir += '/';
        return dir;
#       else
        return "./";
#       endif
    }

    static void* _s_os_load_dylib(const char* path)
    {
#       if defined(_WIN32)
        return (void*)LoadLibraryA(path);
#       else
        return dlopen(path, RTLD_LAZY | RTLD_GLOBAL);
#       endif
    }
#   endif

    void* default_dylib_loader(const char* path)
    {
#   if WO_OS_DYLIB_ENABLED
        if (path == nullptr || *path == '\0')
            return nullptr;

        std::string candidate;

        // 1. Look for the same-named file next to the executable.
        const std::string exe_dir = _s_executable_dir();
        if (!exe_dir.empty())
        {
            candidate = exe_dir + path;
            if (_s_file_exists(candidate.c_str()))
                return _s_os_load_dylib(candidate.c_str());
        }

        // 2. Look for the same-named file in the current working directory.
        candidate = _s_working_dir() + path;
        if (_s_file_exists(candidate.c_str()))
            return _s_os_load_dylib(candidate.c_str());

        // 3. Neither file exists: assume the given name is itself a complete
        //    path and let the OS loader resolve it in one direct attempt.
        return _s_os_load_dylib(path);
#   else
        (void)path;
        return nullptr;
#   endif
    }
    void* default_function_loader(void* lib, const char* fname)
    {
#   if WO_OS_DYLIB_ENABLED
#       if defined(_WIN32)
        return (void*)GetProcAddress((HMODULE)lib, fname);
#       else
        return dlsym(lib, fname);
#       endif
#   else
        (void)lib;
        (void)fname;
        return nullptr;
#   endif
    }
    void default_dylib_unloader(void* lib)
    {
#   if WO_OS_DYLIB_ENABLED
#       if defined(_WIN32)
        FreeLibrary((HMODULE)lib);
#       else
        dlclose(lib);
#       endif
#   else
        (void)lib;
#   endif
    }
#endif

    // The complete set of wo_* entry points this library exports. Keep in
    // sync with the WOODYN_WO_FUNCTION_LIST in the woort.h/wo.h proxy
    // headers shipped by woodyn.
    static const woort_ExternLibFunc s_libwoo_woodyn_funcs[] = {
{"wo_commit_sha", (void*)&wo_commit_sha},
        {"wo_compile_date", (void*)&wo_compile_date},
        {"wo_compile_errors_free", (void*)&wo_compile_errors_free},
        {"wo_compile_errors_next", (void*)&wo_compile_errors_next},
        {"wo_crc64_file", (void*)&wo_crc64_file},
        {"wo_crc64_file_from_path", (void*)&wo_crc64_file_from_path},
        {"wo_crc64_str", (void*)&wo_crc64_str},
        {"wo_crc64_u8", (void*)&wo_crc64_u8},
        {"wo_finish", (void*)&wo_finish},
        {"wo_get_compile_error", (void*)&wo_get_compile_error},
        {"wo_get_woort_dylib_load", (void*)&wo_get_woort_dylib_load},
        {"wo_get_woort_dylib_load_func", (void*)&wo_get_woort_dylib_load_func},
        {"wo_get_woort_dylib_unload", (void*)&wo_get_woort_dylib_unload},
        {"wo_get_woort_env_locale_name", (void*)&wo_get_woort_env_locale_name},
        {"wo_init", (void*)&wo_init},
        {"wo_load_binary", (void*)&wo_load_binary},
        {"wo_load_file", (void*)&wo_load_file},
        {"wo_load_source", (void*)&wo_load_source},
        {"wo_lspv2_compile_err_iter", (void*)&wo_lspv2_compile_err_iter},
        {"wo_lspv2_compile_err_next", (void*)&wo_lspv2_compile_err_next},
        {"wo_lspv2_compile_to_meta", (void*)&wo_lspv2_compile_to_meta},
        {"wo_lspv2_constant_get_info", (void*)&wo_lspv2_constant_get_info},
        {"wo_lspv2_constant_info_free", (void*)&wo_lspv2_constant_info_free},
        {"wo_lspv2_err_info_free", (void*)&wo_lspv2_err_info_free},
        {"wo_lspv2_expr_collection_free", (void*)&wo_lspv2_expr_collection_free},
        {"wo_lspv2_expr_collection_get_by_range", (void*)&wo_lspv2_expr_collection_get_by_range},
        {"wo_lspv2_expr_collection_get_info", (void*)&wo_lspv2_expr_collection_get_info},
        {"wo_lspv2_expr_collection_info_free", (void*)&wo_lspv2_expr_collection_info_free},
        {"wo_lspv2_expr_collection_next", (void*)&wo_lspv2_expr_collection_next},
        {"wo_lspv2_expr_get_info", (void*)&wo_lspv2_expr_get_info},
        {"wo_lspv2_expr_info_free", (void*)&wo_lspv2_expr_info_free},
        {"wo_lspv2_expr_next", (void*)&wo_lspv2_expr_next},
        {"wo_lspv2_lexer_consume", (void*)&wo_lspv2_lexer_consume},
        {"wo_lspv2_lexer_create", (void*)&wo_lspv2_lexer_create},
        {"wo_lspv2_lexer_free", (void*)&wo_lspv2_lexer_free},
        {"wo_lspv2_lexer_peek", (void*)&wo_lspv2_lexer_peek},
        {"wo_lspv2_macro_get_info", (void*)&wo_lspv2_macro_get_info},
        {"wo_lspv2_macro_info_free", (void*)&wo_lspv2_macro_info_free},
        {"wo_lspv2_macro_next", (void*)&wo_lspv2_macro_next},
        {"wo_lspv2_meta_expr_collection_iter", (void*)&wo_lspv2_meta_expr_collection_iter},
        {"wo_lspv2_meta_free", (void*)&wo_lspv2_meta_free},
        {"wo_lspv2_meta_get_global_scope", (void*)&wo_lspv2_meta_get_global_scope},
        {"wo_lspv2_meta_get_semantic_token_iter", (void*)&wo_lspv2_meta_get_semantic_token_iter},
        {"wo_lspv2_meta_macro_iter", (void*)&wo_lspv2_meta_macro_iter},
        {"wo_lspv2_scope_get_info", (void*)&wo_lspv2_scope_get_info},
        {"wo_lspv2_scope_info_free", (void*)&wo_lspv2_scope_info_free},
        {"wo_lspv2_scope_sub_scope_iter", (void*)&wo_lspv2_scope_sub_scope_iter},
        {"wo_lspv2_scope_sub_scope_next", (void*)&wo_lspv2_scope_sub_scope_next},
        {"wo_lspv2_scope_symbol_iter", (void*)&wo_lspv2_scope_symbol_iter},
        {"wo_lspv2_scope_symbol_next", (void*)&wo_lspv2_scope_symbol_next},
        {"wo_lspv2_semantic_token_info_free", (void*)&wo_lspv2_semantic_token_info_free},
        {"wo_lspv2_semantic_token_next", (void*)&wo_lspv2_semantic_token_next},
        {"wo_lspv2_sub_version", (void*)&wo_lspv2_sub_version},
        {"wo_lspv2_symbol_get_info", (void*)&wo_lspv2_symbol_get_info},
        {"wo_lspv2_symbol_info_free", (void*)&wo_lspv2_symbol_info_free},
        {"wo_lspv2_token_info_enstring", (void*)&wo_lspv2_token_info_enstring},
        {"wo_lspv2_token_info_free", (void*)&wo_lspv2_token_info_free},
        {"wo_lspv2_type_get_info", (void*)&wo_lspv2_type_get_info},
        {"wo_lspv2_type_get_struct_info", (void*)&wo_lspv2_type_get_struct_info},
        {"wo_lspv2_type_info_free", (void*)&wo_lspv2_type_info_free},
        {"wo_lspv2_type_struct_info_free", (void*)&wo_lspv2_type_struct_info_free},
        {"wo_print_compiler_help", (void*)&wo_print_compiler_help},
        {"wo_repl_create", (void*)&wo_repl_create},
        {"wo_repl_destroy", (void*)&wo_repl_destroy},
        {"wo_repl_eval", (void*)&wo_repl_eval},
        {"wo_version", (void*)&wo_version},
        {"wo_version_int", (void*)&wo_version_int},
        WOORT_EXTERN_LIB_FUNC_END
    };

    static woort_Dylib* s_libwoo_woodyn = nullptr;

    static void register_libwoo_woodyn(void)
    {
        if (s_libwoo_woodyn == nullptr)
        {
            s_libwoo_woodyn = woort_dylib_fake(
                "libwoo_woodyn", s_libwoo_woodyn_funcs, nullptr);
            wo_assert(s_libwoo_woodyn != nullptr);
        }
    }

    static void unregister_libwoo_woodyn(void)
    {
        if (s_libwoo_woodyn != nullptr)
        {
            woort_dylib_unload(s_libwoo_woodyn, WOORT_DYLIB_UNREF);
            s_libwoo_woodyn = nullptr;
        }
    }

    void bootup_woort_dynamically(
        int argc,
        char** argv,
        std::optional<const wo_WooDyn_Functions*> funcs)
    {
#ifdef WOODYN_WOORT
        wo_assert(nullptr == s_loaded_os_dylib);

        if (funcs.has_value())
            memcpy(&s_woort_functions, funcs.value(), sizeof(wo_WooDyn_Functions));
        else
        {
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
            const char* const library_name =
                "libwoort" WO_DYLIB_DEBUG_SUFFIX WO_DEFAULT_DYLIB_SUFFIX;

            s_loaded_os_dylib = default_dylib_loader(library_name);

            if (s_loaded_os_dylib == nullptr)
            {
                fprintf(stderr, "Failed to load woort: '%s'.\n", library_name);
                abort();
            }

            s_woort_functions.m_init =
                reinterpret_cast<WOODYN_FUNC_TYPE_NAME(woort_init)>(
                    reinterpret_cast<intptr_t>(
                        default_function_loader(s_loaded_os_dylib, "woort_init")));
            s_woort_functions.m_shutdown =
                reinterpret_cast<WOODYN_FUNC_TYPE_NAME(woort_shutdown)>(
                    reinterpret_cast<intptr_t>(
                        default_function_loader(s_loaded_os_dylib, "woort_shutdown")));
            s_woort_functions.m_dylib_load =
                reinterpret_cast<WOODYN_FUNC_TYPE_NAME(woort_dylib_load)>(
                    reinterpret_cast<intptr_t>(
                        default_function_loader(s_loaded_os_dylib, "woort_dylib_load")));
            s_woort_functions.m_dyfunc_load =
                reinterpret_cast<WOODYN_FUNC_TYPE_NAME(woort_dylib_load_func)>(
                    reinterpret_cast<intptr_t>(
                        default_function_loader(s_loaded_os_dylib, "woort_dylib_load_func")));
            s_woort_functions.m_dylib_unload =
                reinterpret_cast<WOODYN_FUNC_TYPE_NAME(woort_dylib_unload)>(
                    reinterpret_cast<intptr_t>(
                        default_function_loader(s_loaded_os_dylib, "woort_dylib_unload")));
        }

        // Verify s_woort_functions.
        if (s_woort_functions.m_init == nullptr
            || s_woort_functions.m_shutdown == nullptr
            || s_woort_functions.m_dylib_load == nullptr
            || s_woort_functions.m_dyfunc_load == nullptr
            || s_woort_functions.m_dylib_unload == nullptr)
        {
            fprintf(stderr, "Incompatible woort module.\n");
            abort();
        }

        // Init woort.
        s_woort_functions.m_init(argc, argv);

        // Ok, libwoort_woodyn is ready.
        woodyn_woort_entry(
            s_woort_functions.m_dylib_load,
            s_woort_functions.m_dyfunc_load,
            s_woort_functions.m_dylib_unload);

        // We can use woort-api now, init locale.
        setlocale(LC_CTYPE, woort_env_locale_name());
#else
        woort_init(argc, argv);
#endif

        // Publish this library under the reserved "libwoo_woodyn" name so
        // that woort_dylib_load("libwoo_woodyn", ...) issued by woodyn
        // consumers (the proxy variant of wo.h) resolves against the
        // woolang already loaded in this process.
        register_libwoo_woodyn();
    }

    void shutdown_woort_dynamically(void(*do_after_shutdown)(void*), void* custom_data)
    {
        // Drop the "libwoo_woodyn" registration before woort tears down
        // its dylib registry. WOORT_DYLIB_UNREF only decrements the
        // reference count: extension libraries that hold a reference
        // release it while woort unloads them next, so the fake dies
        // with its last user.
        unregister_libwoo_woodyn();

#ifdef WOODYN_WOORT
        woodyn_woort_leave();
        s_woort_functions.m_shutdown(do_after_shutdown, custom_data);

        if (s_loaded_os_dylib != nullptr)
        {
            default_dylib_unloader(s_loaded_os_dylib);
            s_loaded_os_dylib = nullptr;
        }
#else
        woort_shutdown(do_after_shutdown, custom_data);
#endif
    }
}


