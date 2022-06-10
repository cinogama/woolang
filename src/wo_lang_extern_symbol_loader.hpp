#pragma once
#ifndef WO_IMPL
#       define WO_IMPL
#       include "wo.h"
#endif
#include "wo_assert.hpp"
#include "wo_os_api.hpp"
#include "wo_shared_ptr.hpp"

#include <shared_mutex>
#include <map>

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

namespace wo
{
    class rslib_extern_symbols
    {
#ifdef PLATFORM_M64
#   ifdef NDEBUG
        inline static void* _current_wo_lib_handle = osapi::loadlib("libwoo");
#   else
        inline static void* _current_wo_lib_handle = osapi::loadlib("libwoo_debug");
#   endif
#else /* PLATFORM_M32 */
#   ifdef NDEBUG
        inline static void* _current_wo_lib_handle = osapi::loadlib("libwoo32");
#   else
        inline static void* _current_wo_lib_handle = osapi::loadlib("libwoo32_debug");
#   endif
#endif
    public:
        struct extern_lib_guard
        {
            void* extern_lib = nullptr;
            extern_lib_guard(const char* libpath)
            {
                extern_lib = osapi::loadlib(libpath);
            }
            wo_native_func load_func(const char* funcname)
            {
                if (extern_lib)
                    return osapi::loadfunc(extern_lib, funcname);
                return nullptr;
            }
            ~extern_lib_guard()
            {
                if (extern_lib)
                {
                    if (auto* entry = (void(*)(void))load_func("rslib_exit"))
                        entry();
                    osapi::freelib(extern_lib);
                }
            }
        };

        struct extern_lib_set
        {
            using extern_lib = shared_pointer<extern_lib_guard>;
            using srcpath_externlib_pairs = std::map<std::string, std::map<std::string, extern_lib>>;

            srcpath_externlib_pairs loaded_libsrc;

            wo_native_func try_load_func_from_in(
                const char* srcpath,
                const char* libpath,
                const char* funcname)
            {
                auto &srcloadedlibs = loaded_libsrc[srcpath];

                if (auto fnd = srcloadedlibs.find(libpath);
                    fnd != srcloadedlibs.end())
                {
                    return fnd->second->load_func(funcname);
                }

                extern_lib elib = new extern_lib_guard(libpath);
                srcloadedlibs[libpath] = elib;

                if (auto * entry = (void(*)(void))elib->load_func("rslib_entry"))
                    entry();
                return elib->load_func(funcname);

            }
        };


        static wo_native_func get_global_symbol(const char* symbol)
        {
            static void* this_exe_handle = osapi::loadlib(nullptr);
            wo_assert(this_exe_handle);

            auto* loaded_symb = osapi::loadfunc(this_exe_handle, symbol);
            if (!loaded_symb && _current_wo_lib_handle)
                loaded_symb = osapi::loadfunc(_current_wo_lib_handle, symbol);
            return loaded_symb;
        }

        static wo_native_func get_lib_symbol(const char* src, const char* lib, const char* symb, extern_lib_set& elibs)
        {
            return elibs.try_load_func_from_in(src, lib, symb);
        }
    };
}