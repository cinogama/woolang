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

#ifdef _WIN32
#   define WO_DYNAMIC_LIB_EXT ".dll"
#elif defined(__linux__)
#   define WO_DYNAMIC_LIB_EXT ".so"
#elif defined(__APPLE__)
#   define WO_DYNAMIC_LIB_EXT ".dylib"
#endif

namespace wo
{
    namespace osapi
    {
#ifdef _WIN32
        void* _loadlib(const char* dllpath)
        {
            if (!dllpath)
                return GetModuleHandleA(NULL);

            return LoadLibraryExW(
                wo::str_to_wstr(dllpath).c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
        }
        void* _loadfunc(void* libhandle, const char* funcname)
        {
            return (void*)GetProcAddress((HINSTANCE)libhandle, funcname);
        }
        void _freelib(void* libhandle)
        {
            FreeLibrary((HINSTANCE)libhandle);
        }
        std::optional<std::string> _geterror()
        {
            DWORD error = GetLastError();
            if (error)
            {
                LPVOID lpMsgBuf;
                DWORD bufLen = FormatMessage(
                    FORMAT_MESSAGE_ALLOCATE_BUFFER |
                    FORMAT_MESSAGE_FROM_SYSTEM |
                    FORMAT_MESSAGE_IGNORE_INSERTS,
                    NULL,
                    error,
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                    (LPTSTR)&lpMsgBuf,
                    0, NULL);
                if (bufLen)
                {
                    LPCSTR lpMsgStr = (LPCSTR)lpMsgBuf;
                    std::string result(lpMsgStr, lpMsgStr + bufLen);
                    LocalFree(lpMsgBuf);
                    return result;
                }
            }
            return std::nullopt;
        }
#elif defined(__linux__) || defined(__APPLE__)
        void* _loadlib(const char* dllpath)
        {
            if (!dllpath)
                return dlopen(nullptr, RTLD_LAZY);

            return dlopen(dllpath, RTLD_LAZY));
        }
        void* _loadfunc(void* libhandle, const char* funcname)
        {
            return (void*)dlsym(libhandle, funcname);
        }
        void _freelib(void* libhandle)
        {
            dlclose(libhandle);
        }
        std::optional<std::string> _geterror()
        {
            const char* error = dlerror();
            if (error)
                return error;
            return std::nullopt;
        }
#endif
        bool file_exists(const char* path)
        {
            struct stat st;
            return stat(path, &st) == 0;
        }
        std::optional<void*> try_open_lib(const char* dllpath)
        {
            if (file_exists(dllpath))
            {
                void* lib = _loadlib(dllpath);
                if (lib == nullptr)
                {
                    wo_fail(
                        WO_FAIL_BAD_LIB,
                        "Failed to load library `%s`: `%s`.",
                        dllpath,
                        _geterror().value_or("unknown"));
                }
                return lib;
            }
            return std::nullopt;
        }
        void* loadlib(const char* dllpath, const char* scriptpath)
        {
            if (dllpath == nullptr)
                return _loadlib(nullptr);

            const std::string filename = "/" + std::string(dllpath) + WO_DYNAMIC_LIB_EXT;

            // 1) Try get dll from script_path
            if (scriptpath)
                if (auto result = try_open_lib((get_file_loc(scriptpath) + filename).c_str()))
                    return result.value();

            // 2) Try get dll from exe_path
            if (auto result = try_open_lib((exe_path() + filename).c_str()))
                return result.value();

            // 3) Try get dll from work_path
            if (auto result = try_open_lib((work_path() + filename).c_str()))
                return result.value();

            // 4) Try load full path
            if (auto result = try_open_lib(dllpath))
                return result.value();

            return nullptr;
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
