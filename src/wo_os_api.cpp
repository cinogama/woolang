#include "wo_afx.hpp"

#include "wo_env_locale.hpp"

#if WO_DISABLE_FUNCTION_FOR_WASM
#else
#   ifdef _WIN32
#       include <Windows.h>
#   elif defined(__linux__)
#       include <dlfcn.h>
#   elif defined(__APPLE__)
#       include <dlfcn.h>
#       include <mach-o/dyld.h>
#   endif

#   ifdef _WIN32
#       define WO_DYNAMIC_LIB_EXT L".dll"
#   elif defined(__linux__)
#       define WO_DYNAMIC_LIB_EXT L".so"
#   elif defined(__APPLE__)
#      define WO_DYNAMIC_LIB_EXT L".dylib"
#   endif
#endif

namespace wo
{
    namespace osapi
    {
#if WO_DISABLE_FUNCTION_FOR_WASM
#else
#   ifdef _WIN32
        void* _loadlib(const char* dllpath)
        {
            if (nullptr == dllpath)
                return GetModuleHandleA(NULL);

            return LoadLibraryExW(
                wo::str_to_wstr(dllpath).c_str(),
                NULL,
                LOAD_WITH_ALTERED_SEARCH_PATH);
        }
        void* _loadfunc(void* libhandle, const char* funcname)
        {
            return (void*)GetProcAddress((HINSTANCE)libhandle, funcname);
        }
        void _freelib(void* libhandle)
        {
            FreeLibrary((HINSTANCE)libhandle);
        }
#   elif defined(__linux__) || defined(__APPLE__)
        void* _loadlib(const char* dllpath)
        {
            if (nullptr == dllpath)
                return dlopen(nullptr, RTLD_LAZY);

            return dlopen(dllpath, RTLD_LAZY);
        }
        void* _loadfunc(void* libhandle, const char* funcname)
        {
            return (void*)dlsym(libhandle, funcname);
        }
        void _freelib(void* libhandle)
        {
            dlclose(libhandle);
        }
#   endif
        bool file_exists(const char* path)
        {
            struct stat st;
            return stat(path, &st) == 0;
        }
#endif


        std::optional<void*> try_open_lib(const char* dllpath)
        {
#if WO_DISABLE_FUNCTION_FOR_WASM
            // Do nothing.
#else
            if (file_exists(dllpath))
            {
                void* lib = _loadlib(dllpath);
                if (lib == nullptr)
                {
                    wo_fail(
                        WO_FAIL_BAD_LIB,
                        "Failed to load library `%s`, broken file or failed to load dependence?",
                        dllpath);
                }
                return lib;
            }
#endif
            return std::nullopt;
        }
        void* loadlib(const wchar_t* dllpath, const wchar_t* scriptpath_may_null)
        {
#if WO_DISABLE_FUNCTION_FOR_WASM
            // Do nothing.
            return nullptr;
#else
            if (dllpath == nullptr)
                return _loadlib(nullptr);

            const std::wstring filename = L"/" + std::wstring(dllpath) + WO_DYNAMIC_LIB_EXT;

            // 1) Try get dll from script_path
            if (scriptpath_may_null != nullptr)
                if (auto result = try_open_lib(wstr_to_str(get_file_loc(scriptpath_may_null) + filename).c_str()))
                    return result.value();

            // 2) Try get dll from work_path
            if (auto result = try_open_lib(wstr_to_str(work_path() + filename).c_str()))
                return result.value();

            // 3) Try get dll from exe_path
            if (auto result = try_open_lib(wstr_to_str(exe_path() + filename).c_str()))
                return result.value();

            // 4) Try load full path
            if (auto result = try_open_lib(wstr_to_str(dllpath).c_str()))
                return result.value();

            // 5) Load from system path
            // NOTE: Use _loadlib because system path might not exist.
            if (scriptpath_may_null == nullptr)
                return _loadlib(wstr_to_str(dllpath).c_str());
            else
                return nullptr;
#endif
        }
        void* loadfunc(void* libhandle, const char* funcname)
        {
#if WO_DISABLE_FUNCTION_FOR_WASM
            // Do nothing.
            return nullptr;
#else
            return _loadfunc(libhandle, funcname);
#endif
        }
        void freelib(void* libhandle)
        {
#if WO_DISABLE_FUNCTION_FOR_WASM
#else
            _freelib(libhandle);
#endif
        }
    }

    void normalize_path(std::wstring* inout_path)
    {
#ifdef _WIN32
        for (wchar_t& ch : *inout_path)
        {
            if (ch == L'\\')
                ch = L'/';
        }
        if (inout_path->length() >= 2 && inout_path->at(1) == L':')
        {
            wchar_t& p = inout_path->at(0);
            p = towupper(p);
        }
#endif
    }
}
