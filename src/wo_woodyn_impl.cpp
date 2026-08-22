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

    void bootup_woort_dynamically(
        int argc,
        char** argv,
        std::optional<const wo_WooDyn_Functions*> funcs)
    {
        wo_assert(nullptr == s_loaded_os_dylib);

#ifdef WOODYN_WOORT
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
    }

    void shutdown_woort_dynamically(void(*do_after_shutdown)(void*), void* custom_data)
    {
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


