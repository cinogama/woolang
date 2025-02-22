#pragma once
// Here to place some global variable for config..
#include <cstddef>
#include <new>
#include <thread>
#include <algorithm>

#if WO_BUILD_WITH_MINGW
#   include <mingw.thread.h>
#endif

namespace wo
{
    namespace platform_info
    {
        struct OSType
        {
            enum Type
            {
                UNKNOWN = 0x00'00'00'00,
                WINDOWS = 0b0000'0001,
                LINUX = 0b0000'0010,
                MACOSX = 0b0000'0100,
            };
        };

        constexpr auto OS_TYPE =
#ifdef _WIN32
#define WO_PLATRORM_OS_WINDOWS
            OSType::WINDOWS
#elif defined(__linux__)
#define WO_PLATRORM_OS_LINUX
            OSType::LINUX
#else
            OSType::UNKNOWN
#endif
            ;
        struct ArchType
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
            ArchType::X86 | 0;
#           define WO_PLATFORM_32 
#           define WO_PLATFORM_X86
#           define WO_VM_SUPPORT_FAST_NO_ALIGN
#elif  defined(__x86_64__) || defined(_M_X64) 
            ArchType::X86 | ArchType::BIT64;
#           define WO_PLATFORM_64
#           define WO_PLATFORM_X64
#           define WO_VM_SUPPORT_FAST_NO_ALIGN
#elif  defined(_M_ARM)||defined(__arm__)
            ArchType::ARM | 0;
#           define WO_PLATFORM_32 
#           define WO_PLATFORM_ARM
#elif  defined(__aarch64__) ||defined(_M_ARM64) 
            ArchType::ARM | ArchType::BIT64;
#           define WO_PLATFORM_64
#           define WO_PLATFORM_ARM64
#else
#   if not defined(WO_PLATFORM_32) and not defined(WO_PLATFORM_64)
#       error "Unknown platform, you must specify platform manually."
#   endif
            ArchType::UNKNOWN
#   ifdef WO_PLATFORM_64
            | ArchType::BIT64
#   endif
            ;
#endif
#ifdef WO_PLATFORM_32
        static_assert(0 == (ARCH_TYPE & ArchType::BIT64));
#else
        static_assert(0 != (ARCH_TYPE & ArchType::BIT64));
#endif
    }
    namespace config
    {
        inline bool ENABLE_OUTPUT_ANSI_COLOR_CTRL =
#if WO_BUILD_WITH_MINGW
            false
#else
            true
#endif
            ;

        inline bool ENABLE_CHECK_GRAMMAR_AND_UPDATE = false;

        /*
        * ENABLE_JUST_IN_TIME = true
        * --------------------------------------------------------------------
        *   Woolang will use asmjit to generate code in runtime.
        * --------------------------------------------------------------------
        */
        inline bool ENABLE_JUST_IN_TIME =
#if defined(NDEBUG) && defined(WO_JIT_SUPPORT_ASMJIT)
            true
#else
            false
#endif
            ;

        /*
        * ENABLE_PDB_INFORMATIONS = true
        * --------------------------------------------------------------------
        *   Woolang will generate pdb information for programs.
        * --------------------------------------------------------------------
        */
        inline bool ENABLE_PDB_INFORMATIONS = true;

        /*
        * ENABLE_SHELL_PACKAGE = true
        * --------------------------------------------------------------------
        *   Woolang will enable woo/shell.wo for execute shell command.
        * --------------------------------------------------------------------
        */
        inline bool ENABLE_SHELL_PACKAGE = true;

        /*
        * MEMORY_CHUNK_SIZE = 512MB/256MB
        * --------------------------------------------------------------------
        *   Maximum managed heap memory used by Woolang.
        * 
        *     Managed heap memory is not equivalent to all used memory. This area
        *   is only used to store GC objects.
        * --------------------------------------------------------------------
        */
        inline size_t MEMORY_CHUNK_SIZE = 
#ifdef WO_PLATFORM_64
            512ull * 1024ull * 1024ull;
#else
            128ull * 1024ull * 1024ull;
#endif

        /*
        * ENABLE_HALT_WHEN_PANIC = false
        * --------------------------------------------------------------------
        *   Whether to allow the thread to be terminated directly instead of 
        *   blocking when PANIC occurs.
        * --------------------------------------------------------------------
        */
        inline bool ENABLE_HALT_WHEN_PANIC = false;

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
        * ENABLE_RUNTIME_CHECKING_INTEGER_DIVISION = false
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
        * INTERRUPT_CHECK_TIME_LIMIT = 25
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
        *   Do not set this value too large or too small, the recommended range is 
        * 10-50 (100ms - 500ms). Excessively large values will block the GC workflow 
        * and affect recycling efficiency. A value that is too small will cause 
        * warnings to be triggered frequently and may cause GC marking to change from
        * parallel to serial.
        * 
        *   0 does not mean unlimited wait.
        */
        inline size_t INTERRUPT_CHECK_TIME_LIMIT = 25;

        /*
        * ENABLE_SKIP_UNSAFE_CAST_INVOKE = true
        * --------------------------------------------------------------------
        *   When checking that `unsafe::cast` is ready to be called, simply 
        * eliminate this function call.
        * -------------------------------------------------------------------- 
        */
        inline bool ENABLE_SKIP_INVOKE_UNSAFE_CAST = true;

        /*
        * GC_WORKER_THREAD_COUNT = [1/4 of hardware_concurrency] or 1(in wasm)
        * --------------------------------------------------------------------
        *   The number of threads used by the GC worker.
        * --------------------------------------------------------------------
        *   
        */
        inline size_t GC_WORKER_THREAD_COUNT =
#if WO_DISABLE_FUNCTION_FOR_WASM
            1;
#else
            std::max(((size_t)std::thread::hardware_concurrency()) / 4, (size_t)1);
#endif
    }
}