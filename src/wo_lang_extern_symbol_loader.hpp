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
        inline static void* _current_wo_lib_handle = nullptr;

    public:
        inline static void init_wo_lib() 
        {
            free_wo_lib();

            wo_assert(_current_wo_lib_handle == nullptr);
#ifdef PLATFORM_M64
#   ifdef NDEBUG
            _current_wo_lib_handle = osapi::loadlib("libwoo");
#   else
            _current_wo_lib_handle = osapi::loadlib("libwoo_debug");
#   endif
#else /* PLATFORM_M32 */
#   ifdef NDEBUG
            _current_wo_lib_handle = osapi::loadlib("libwoo32");
#   else
            _current_wo_lib_handle = osapi::loadlib("libwoo32_debug");
#   endif
#endif
        }
        inline static void free_wo_lib()
        {
            if (_current_wo_lib_handle != nullptr)
            {
                osapi::freelib(_current_wo_lib_handle);
                _current_wo_lib_handle = nullptr;
            }
        }

        struct extern_lib_guard
        {
            bool load_by_os_api = true;
            void* extern_lib = nullptr;

            extern_lib_guard(const std::string& libpath, const std::string& script_path)
            {
#ifndef NDEBUG
#   if defined(PLATFORM_M32)
                if ((extern_lib = osapi::loadlib((libpath + "32_debug").c_str(), script_path.c_str())))
                    return;
#   else
                if ((extern_lib = osapi::loadlib((libpath + "_debug").c_str(), script_path.c_str())))
                    return;
#   endif
#else
#   if defined(PLATFORM_M32)
                if ((extern_lib = osapi::loadlib((libpath + "32").c_str(), script_path.c_str())))
                    return;
#   else
                if ((extern_lib = osapi::loadlib(libpath.c_str(), script_path.c_str())))
                    return;
#   endif
#endif
                // No such lib path? try lib alias here:
                load_by_os_api = false;
                extern_lib = wo_load_lib(libpath.c_str(), nullptr, WO_FALSE);
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
                    if (auto* leave = (void(*)(void))load_func("wolib_exit"))
                        leave();

                    if (load_by_os_api)
                        osapi::freelib(extern_lib);
                    else
                        wo_unload_lib(extern_lib);
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

                extern_lib elib = new extern_lib_guard(libpath, srcpath);
                srcloadedlibs[libpath] = elib;

                if (auto * entry = (void(*)(void))elib->load_func("wolib_entry"))
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