#pragma once
// Here to place some global variable for config..
#include <cstddef>
#include <new>

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
#elif  defined(__x86_64__) || defined(_M_X64) 
            ArchType::X86 | ArchType::BIT64;
#           define WO_PLATFORM_64
#           define WO_PLATFORM_X64
#elif  defined(_M_ARM)||defined(__arm__)
            ArchType::ARM | 0;
#           define WO_PLATFORM_32 
#           define WO_PLATFORM_ARM
#elif  defined(__aarch64__) ||defined(_M_ARM64) 
            ArchType::ARM | ArchType::BIT64;
#           define WO_PLATFORM_64
#           define WO_PLATFORM_ARM64
#else
            ArchType::UNKNOWN | (sizeof(size_t) == 64 ? ArchType::BIT64 : ArchType::UNKNOWN);
#endif
            
    }
    namespace config
    {
        inline bool ENABLE_OUTPUT_ANSI_COLOR_CTRL = true;

        inline bool ENABLE_CHECK_GRAMMAR_AND_UPDATE = false;

        /*
        * ENABLE_JUST_IN_TIME = false
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

        inline bool ENABLE_PDB_INFORMATIONS = true;
        inline bool ENABLE_SHELL_PACKAGE = true;
        inline size_t MEMORY_CHUNK_SIZE = 1ull * 1024ull * 1024ull * 1024ull;
    }
}