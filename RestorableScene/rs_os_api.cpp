#include "rs_os_api.hpp"

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
            return LoadLibraryExA(dllpath, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
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
            return dlopen(dllpath, RTLD_LAZY);
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