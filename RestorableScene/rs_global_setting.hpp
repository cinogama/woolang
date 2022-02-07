#pragma once
// Here to place some global variable for config..

#include <new>

namespace rs
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
#define RS_PLATRORM_OS_WINDOWS
            OSType::WINDOWS
#elif defined(__linux__)
#define RS_PLATRORM_OS_LINUX
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
#           define RS_PLATFORM_32 
#elif  defined(__x86_64__) || defined(_M_X64) 
            ArchType::X86 | ArchType::BIT64;
#           define RS_PLATFORM_64
#elif  defined(_M_ARM)||defined(__arm__)
            ArchType::ARM | 0;
#           define RS_PLATFORM_32 
#elif  defined(__aarch64__) ||defined(_M_ARM64) 
            ArchType::ARM | ArchType::BIT64;
#           define RS_PLATFORM_64
#else
            ArchType::UNKNOWN | (sizeof(size_t) == 64 ? ArchType::BIT64 : ArchType::UNKNOWN);
#endif
            
        constexpr size_t CPU_CACHELINE_SIZE =
#ifdef __cpp_lib_hardware_interference_size
            std::hardware_constructive_interference_size
#else
            64
#endif
            ;

    }
    namespace config
    {
        /*
        * ENABLE_IR_CODE_ACTIVE_ALLIGN = false
        * --------------------------------------------------------------------
        *   Virtual-machine may read 2byte, 4byte or 8byte data from ir code.
        * In some platform, reading multi-byte data from the address not match
        * the allign, the allign-fault will happend.
        * --------------------------------------------------------------------
        *   if ENABLE_IR_CODE_ACTIVE_ALLIGN is true, ir generator will fill the
        * ir command buffer with 'nop(sz)', and make sure the 2byte/4byte/8byte
        * data will be allign correctly.
        * --------------------------------------------------------------------
        * ATTENTION:
        *   nop(sz) will caused performance loss, so if this options work, vm's
        * proformance may be lost by 30%-50%
        * --------------------------------------------------------------------
        *
        * I hate allign.
        *                   ---- mr. cino. 2022.1.4.
        */
        inline bool ENABLE_IR_CODE_ACTIVE_ALLIGN = false;

        inline bool ENABLE_OUTPUT_ANSI_COLOR_CTRL = true;


        /*
        * ENABLE_AVOIDING_FALSE_SHARED = false
        * --------------------------------------------------------------------
        */
        inline bool ENABLE_AVOIDING_FALSE_SHARED = false;

        /*
        * ENABLE_JUST_IN_TIME = true
        * --------------------------------------------------------------------
        *   RScene will use asmjit to generate code in runtime.
        * --------------------------------------------------------------------
        *   if ENABLE_JUST_IN_TIME is true, compiler will generate 'jitcall' 
        * and 'ext0_jitend' in ir to notify jit work.
        * --------------------------------------------------------------------
        */
        inline bool ENABLE_JUST_IN_TIME = true;
    }
}