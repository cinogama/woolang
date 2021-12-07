#define _CRT_SECURE_NO_WARNINGS

#include "rs_os_api.hpp"
#include "rs_assert.hpp"
#include "rs_env_locale.hpp"
#include <string>

#ifdef _WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

namespace rs
{
    namespace osapi
    {
#ifdef _WIN32
        void* loadlib(const char* dllpath, const char* scriptpath)
        {
            if (!dllpath)
                return GetModuleHandleA(NULL);

            void* result = nullptr;

            // 1) Try get dll from script_path
            if (scriptpath)
                if (result = LoadLibraryExA((get_file_loc(scriptpath) + std::string(dllpath) + ".dll").c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH))
                    return result;

            // 2) Try get dll from exe_path
            if (result = LoadLibraryExA((exe_path() + std::string(dllpath) + ".dll").c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH))
                return result;

            // 3) Try get dll from work_path
            if (result = LoadLibraryExA((work_path() + std::string(dllpath) + ".dll").c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH))
                return result;

            return nullptr;
        }
        rs_native_func loadfunc(void* libhandle, const char* funcname)
        {
            return (rs_native_func)GetProcAddress((HINSTANCE)libhandle, funcname);
        }
        void freelib(void* libhandle)
        {
            FreeLibrary((HINSTANCE)libhandle);
        }
#else
        void* loadlib(const char* dllpath, const char* scriptpath)
        {
#ifdef __linux__
            if (!dllpath)
                return dlopen(nullptr, RTLD_LAZY);

            void* result = nullptr;

            // 1) Try get dll from script_path
            if (scriptpath)
                if (result = dlopen((get_file_loc(scriptpath) + std::string(dllpath) + ".so").c_str(), RTLD_LAZY))
                    return result;

            // 2) Try get dll from exe_path
            if (result = dlopen((exe_path() + std::string(dllpath) + ".so").c_str(), RTLD_LAZY))
                return result;

            // 3) Try get dll from work_path
            if (result = dlopen((work_path() + std::string(dllpath) + ".so").c_str(), RTLD_LAZY))
                return result;

            return nullptr;
#else
            rs_error("Unknown operating-system..") :
                return nullptr;
#endif
        }
        rs_native_func loadfunc(void* libhandle, const char* funcname)
        {
            return (rs_native_func)dlsym(libhandle, funcname);
        }
        void freelib(void* libhandle)
        {
            dlclose(libhandle);
        }

#endif
    }
}