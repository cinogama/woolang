#pragma once
#ifndef RS_IMPL
#       define RS_IMPL
#       include "rs.h"
#endif
#include "rs_assert.hpp"
#include "rs_os_api.hpp"
#include "rs_compiler_ir.hpp"
#include <shared_mutex>

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

namespace rs
{
    class rslib_extern_symbols
    {
#ifdef PLATFORM_M64
#   ifdef NDEBUG
        inline static void* _current_rs_lib_handle = osapi::loadlib("librscene");
#   else
        inline static void* _current_rs_lib_handle = osapi::loadlib("librscene_debug");
#   endif
#else /* PLATFORM_M32 */
#   ifdef NDEBUG
        inline static void* _current_rs_lib_handle = osapi::loadlib("librscene32");
#   else
        inline static void* _current_rs_lib_handle = osapi::loadlib("librscene32_debug");
#   endif
#endif
    public:
        static rs_native_func get_global_symbol(const char* symbol)
        {
            static void* this_exe_handle = osapi::loadlib(nullptr);
            rs_assert(this_exe_handle);

            auto* loaded_symb = osapi::loadfunc(this_exe_handle, symbol);
            if (!loaded_symb && _current_rs_lib_handle)
                loaded_symb = osapi::loadfunc(_current_rs_lib_handle, symbol);
            return loaded_symb;
        }
    };
}