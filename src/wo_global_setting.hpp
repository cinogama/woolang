#pragma once
#include "woort_platform.h"
// Here to place some global variable for config..
#include <cstddef>
#include <new>
#include <thread>
#include <algorithm>

namespace wo
{
    namespace platform_info
    {
        enum class OSType: uint8_t
        {
            UNKNOWN,
            WINDOWS,
            LINUX,
            MACOSX,
        };

        constexpr auto OS_TYPE =
#ifdef _WIN32
            OSType::WINDOWS
#elif defined(__linux__)
            OSType::LINUX
#else
            OSType::UNKNOWN
#endif
            ;
        struct ArchMask
        {
            enum Type
            {
                UNKNOWN = 0x00'00'00'00,
                X86 = 0b0000'0001,
                ARM = 0b0000'0010,

                BIT64 = 0b0000'0001 << 8,
            };
        };

        constexpr auto ARCH_TYPE =
#if defined(_M_IX86) || defined(__i386__)
            ArchMask::X86 | 0;
#elif  defined(__x86_64__) || defined(_M_X64) 
            ArchMask::X86 | ArchMask::BIT64;
#elif  defined(_M_ARM)||defined(__arm__)
            ArchMask::ARM | 0;
#elif  defined(__aarch64__) ||defined(_M_ARM64) 
            ArchMask::ARM | ArchMask::BIT64;
#else
#   if !defined(WO_PLATFORM_32) && !defined(WO_PLATFORM_64)
#       error "Unknown platform, you must specify platform manually."
#   endif
            ArchMask::UNKNOWN
#   ifdef WO_PLATFORM_64
            | ArchMask::BIT64
#   endif
            ;
#endif
#ifdef WO_PLATFORM_32
        static_assert(0 == (ARCH_TYPE & ArchMask::BIT64));
#else
        static_assert(0 != (ARCH_TYPE & ArchMask::BIT64));
#endif
    }
    namespace config
    {
        inline bool ENABLE_CHECK_GRAMMAR_AND_UPDATE = false;

        /*
        * ENABLE_IGNORE_NOT_FOUND_EXTERN_SYMBOL = false
        * --------------------------------------------------------------------
        *   When importing external functions using the extern syntax, ignore
        *   missing function symbols and replace them with the default panic
        *   function.
        * --------------------------------------------------------------------
        * ATTENTION:
        *   This configuration item is only meaningful when used for LSP or
        *   compiled into a binary, and should not be abused. Please properly
        *   handle the situation where symbols are indeed lost.
        */
        inline bool ENABLE_IGNORE_NOT_FOUND_EXTERN_SYMBOL = false;

        /*
        * ENABLE_RUNTIME_CHECKING_INTEGER_DIVISION = true
        * --------------------------------------------------------------------
        *   Whether to check both the divisor is zero and the overflow when the
        * division operation is performed in runtime.
        *   If enabled, compiler will generate some extra code for checking.
        * --------------------------------------------------------------------
        * ATTENTION:
        *   Enabled by default for safety, disable it for better performance.
        *
        *   Check for integer division only.
        */
        inline bool ENABLE_RUNTIME_CHECKING_INTEGER_DIVISION = true;

        /*
        * INTERRUPT_CHECK_TIME_LIMIT = 50
        * --------------------------------------------------------------------
        *   When the virtual machine is within the GC scope and does not respond
        * to the specified interrupt request beyond this time limit, a warning
        * will be given and the wait will end.
        * --------------------------------------------------------------------
        * ATTENTION:
        *   The unit is 10ms, if you set INTERRUPT_CHECK_TIME_LIMIT = 100, it means
        * 1s;
        *
        *   The GC check signal timeout is not an expected behavior. Once this warning
        * is triggered, you should rule out behaviors that may cause blocking.
        *
        *   Do not set this value too large or too small, the recommended value is
        * 50 (500ms). Excessively large values will block the GC workflow
        * and affect recycling efficiency. A value that is too small will cause
        * warnings to be triggered frequently and may cause GC marking to change from
        * parallel to serial.
        *
        *   0 does not mean unlimited wait.
        */
        inline size_t INTERRUPT_CHECK_TIME_LIMIT = 50;

        /*
        * ENABLE_SKIP_UNSAFE_CAST_INVOKE = true
        * --------------------------------------------------------------------
        *   When checking that `unsafe::cast` is ready to be called, simply
        * eliminate this function call.
        * --------------------------------------------------------------------
        */
        inline bool ENABLE_SKIP_INVOKE_UNSAFE_CAST = true;
    }
}
