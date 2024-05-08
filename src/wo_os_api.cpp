#include "wo_os_api.hpp"
#include "wo_assert.hpp"
#include "wo_env_locale.hpp"

#include <string>
#include <unordered_map>
#include <shared_mutex>

#ifdef _WIN32
#include <Windows.h>
#elif defined(__linux__)
#include <dlfcn.h>
#elif defined(__APPLE__)
#include <dlfcn.h>
#include <mach-o/dyld.h>
#endif

namespace wo
{
    namespace osapi
    {
#ifdef _WIN32
        void* _loadlib(const char* dllpath, const char* scriptpath)
        {
            if (!dllpath)
                return GetModuleHandleA(NULL);

            void* result = nullptr;

            // 1) Try get dll from script_path
            if (scriptpath)
                if (result = LoadLibraryExW(wo::str_to_wstr(get_file_loc(scriptpath) + "/" + dllpath + ".dll").c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH))
                    return result;

            // 2) Try get dll from exe_path
            if (result = LoadLibraryExW(wo::str_to_wstr(std::string(exe_path()) + "/" + dllpath + ".dll").c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH))
                return result;

            // 3) Try get dll from work_path
            if (result = LoadLibraryExW(wo::str_to_wstr(std::string(work_path()) + "/" + dllpath + ".dll").c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH))
                return result;

            // 4) Try load full path
            if (result = LoadLibraryExW(wo::str_to_wstr(dllpath).c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH))
                return result;

            return nullptr;
        }
        void* _loadfunc(void* libhandle, const char* funcname)
        {
            return (void*)GetProcAddress((HINSTANCE)libhandle, funcname);
        }
        void _freelib(void* libhandle)
        {
            FreeLibrary((HINSTANCE)libhandle);
        }
#else
        void* _loadlib(const char* dllpath, const char* scriptpath)
        {
#if defined(__linux__) || defined(__APPLE__)
            if (!dllpath)
                return dlopen(nullptr, RTLD_LAZY);

            void* result = nullptr;

            // 1) Try get dll from script_path
            if (scriptpath)
                if ((result = dlopen((std::string(get_file_loc(scriptpath)) + "/" + dllpath + ".so").c_str(), RTLD_LAZY)))
                    return result;

            // 2) Try get dll from exe_path
            if ((result = dlopen((std::string(exe_path()) + "/" + dllpath + ".so").c_str(), RTLD_LAZY)))
                return result;

            // 3) Try get dll from work_path
            if ((result = dlopen((std::string(work_path()) + "/" + dllpath + ".so").c_str(), RTLD_LAZY)))
                return result;

            // 4) Try load full path
            if ((result = dlopen(dllpath, RTLD_LAZY)))
                return result;

            return nullptr;
#else
            wo_error("Unknown operating-system..");
            return nullptr;
#endif
        }
        void* _loadfunc(void* libhandle, const char* funcname)
        {
            return (void*)dlsym(libhandle, funcname);
        }
        void _freelib(void* libhandle)
        {
            dlclose(libhandle);
        }

#endif

        void* loadlib(const char* dllpath, const char* scriptpath)
        {
            return _loadlib(dllpath, scriptpath);
        }
        void* loadfunc(void* libhandle, const char* funcname)
        {
            return _loadfunc(libhandle, funcname);
        }
        void freelib(void* libhandle)
        {
            _freelib(libhandle);
        }
    }
}
