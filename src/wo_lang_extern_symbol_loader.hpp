#pragma once
#ifndef WO_IMPL
#       define WO_IMPL
#       include "wo.h"
#endif
#include "wo_assert.hpp"
#include "wo_os_api.hpp"
#include "wo_shared_ptr.hpp"

#include <shared_mutex>
#include <unordered_map>

#ifdef _WIN32
#		define OS_WINDOWS
#elif defined(__ANDROID__)
#		define OS_ANDROID
#elif defined(__linux__)
#		define OS_LINUX
#else
#		define OS_UNKNOWN
#endif

#if defined(_X86_)||defined(__i386)||(defined(_WIN32)&&!defined(_WIN64))
#		define PLATFORM_X86
#		define PLATFORM_M32
#elif defined(__x86_64)||defined(_M_X64)
#		define PLATFORM_X64
#		define PLATFORM_M64
#elif defined(__arm)
#		define PLATFORM_ARM
#		define PLATFORM_M32
#elif defined(__aarch64__)
#		define PLATFORM_ARM64
#		define PLATFORM_M64
#else
#		define PLATFORM_UNKNOWN
#endif

#if defined(PLATFORM_M32)
#   define WO_EXT_LIB_ARCH_TYPE_SUFFIX "32"
#else
#   define WO_EXT_LIB_ARCH_TYPE_SUFFIX ""
#endif
#ifdef NDEBUG
#   define WO_EXT_LIB_DEBUG_SUFFIX ""
#else
#   define WO_EXT_LIB_DEBUG_SUFFIX "_debug"
#endif

#define WO_EXT_LIB_SUFFIX WO_EXT_LIB_ARCH_TYPE_SUFFIX WO_EXT_LIB_DEBUG_SUFFIX

namespace wo
{
    class rslib_extern_symbols
    {
        inline static void* _current_wo_lib_handle = nullptr;

    public:
        static void init_wo_lib();
        static void free_wo_lib()
        {
            if (_current_wo_lib_handle != nullptr)
            {
                wo_unregister_extern_lib(_current_wo_lib_handle);
                _current_wo_lib_handle = nullptr;
            }
        }

        struct extern_lib_guard
        {
            void* m_extern_library = nullptr;

            extern_lib_guard(const std::string& libpath, const std::string& script_path)
            {
                m_extern_library = wo_load_lib(
                    libpath.c_str(),
                    (libpath + WO_EXT_LIB_SUFFIX).c_str(),
                    script_path.c_str(), WO_FALSE);
            }
            wo_native_func_t load_func(const char* funcname)
            {
                if (m_extern_library != nullptr)
                    return (wo_native_func_t)wo_load_func(m_extern_library, funcname);

                return nullptr;
            }
            ~extern_lib_guard()
            {
                if (m_extern_library != nullptr)
                {
                    if (auto* leave = (void(*)(void))load_func("wolib_exit"))
                        leave();

                    wo_unload_lib(m_extern_library);
                }
            }
        };

        struct extern_lib_set
        {
            using extern_lib = shared_pointer<extern_lib_guard>;
            using srcpath_externlib_pairs =
                std::unordered_map<std::string, std::unordered_map<std::string, extern_lib>>;

            srcpath_externlib_pairs loaded_libsrc;

            wo_native_func_t try_load_func_from_in(
                const char* srcpath,
                const char* libpath,
                const char* funcname)
            {
                auto& srcloadedlibs = loaded_libsrc[srcpath];

                if (auto fnd = srcloadedlibs.find(libpath);
                    fnd != srcloadedlibs.end())
                {
                    return fnd->second->load_func(funcname);
                }

                extern_lib elib = new extern_lib_guard(libpath, srcpath);
                srcloadedlibs[libpath] = elib;

                if (auto* entry = (void(*)(void))elib->load_func("wolib_entry"))
                    entry();
                return elib->load_func(funcname);
            }
        };

        static wo_native_func_t get_global_symbol(const char* symbol)
        {
            return (wo_native_func_t)wo_load_func(_current_wo_lib_handle, symbol);
        }

        static wo_native_func_t get_lib_symbol(const char* src, const char* lib, const char* symb, extern_lib_set& elibs)
        {
            return elibs.try_load_func_from_in(src, lib, symb);
        }
    };
}