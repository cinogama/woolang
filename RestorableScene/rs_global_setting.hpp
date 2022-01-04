#pragma once
// Here to place some global variable for config..

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
            OSType::WINDOWS
#elif defined(__linux__)
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
#if defined(_M_IX86) || defined(_M_X64) || defined(__i386__) || defined(__x86_64__)
            ArchType::X86
#elif  defined(_M_ARM) ||defined(_M_ARM64) ||defined(__arm__) || defined(__aarch64__)
            ArchType::ARM
#else
            ArchType::UNKNOWN
#endif
            | (sizeof(size_t) == 64 ? ArchType::BIT64 : ArchType::UNKNOWN);


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

    }
}