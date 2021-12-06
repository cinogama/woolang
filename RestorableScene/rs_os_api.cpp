#include "rs_os_api.hpp"
#include "rs_assert.hpp"
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
        void* loadlib(const char* dllpath)
        {
            if (!dllpath)
                return GetModuleHandleA(NULL);
            return LoadLibraryExA((std::string(dllpath)+".dll").c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
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
        void* loadlib(const char* dllpath)
        {
#ifdef __linux__
            return dlopen((std::string(dllpath) + ".so").c_str(), RTLD_LAZY);
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