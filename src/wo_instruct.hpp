#pragma once

#include "wo_assert.hpp"
#include "wo_basic_type.hpp"

#include <cstdint>

namespace wo
{
    namespace irv2
    {
        /*
        R/S Addressing -127 ~ 95:
            ((ads >> 5) == 0b011) ? (reg - 0b01100000) + ads : stk + ads
        */

        // Normal formal
        struct ir6_i18_i8
        {
            uint32_t m_op : 6;
            int32_t m_arg1_i18 : 18;
            int32_t m_arg2_i8 : 8;
        };
        static_assert(sizeof(ir6_i18_i8) == 4);

        struct ir6_i10_i16
        {
            uint32_t m_op : 6;
            int32_t m_arg1_i10 : 10;
            int32_t m_arg2_i16 : 16;
        };
        static_assert(sizeof(ir6_i18_i8) == 4);

        struct ir8_i24
        {
            uint32_t m_op : 8;
            int32_t m_arg1_i24 : 24;
        };
        static_assert(sizeof(ir8_i24) == 4);

        struct ir8_u24
        {
            uint32_t m_op : 8;
            uint32_t m_arg1_u24 : 24;
        };
        static_assert(sizeof(ir8_u24) == 4);

        struct ir8_i8_i8_i8
        {
            uint32_t m_op : 8;
            int32_t m_arg1_i8 : 8;
            int32_t m_arg2_i8 : 8;
            int32_t m_arg3_i8 : 8;
        };
        static_assert(sizeof(ir8_i8_i8_i8) == 4);

        struct ir8_i8_i8_u8
        {
            uint32_t m_op : 8;
            int32_t m_arg1_i8 : 8;
            int32_t m_arg2_i8 : 8;
            uint32_t m_arg3_u8 : 8;
        };
        static_assert(sizeof(ir8_i8_i8_u8) == 4);

        struct ir8_i8_i16
        {
            uint32_t m_op : 8;
            int32_t m_arg1 : 8;
            int32_t m_arg2 : 16;
        };
        static_assert(sizeof(ir8_i8_i16) == 4);

        struct ir8_i8_u16
        {
            uint32_t m_op : 8;
            int32_t m_arg1 : 8;
            uint32_t m_arg2 : 16;
        };
        static_assert(sizeof(ir8_i8_u16) == 4);

        // External argument formal
        struct ir8_i8_i16_ext_i32
        {
            uint32_t m_op : 8;
            int32_t m_arg1 : 8;
            int32_t m_arg2 : 16;
            int32_t m_ext_arg1 : 32;
        };
        static_assert(sizeof(ir8_i8_i16_ext_i32) == 8);

        struct ir8_i24_ext_i32
        {
            uint32_t m_op : 8;
            int32_t m_arg1 : 24;
            int32_t m_ext_arg1 : 32;
        };
        static_assert(sizeof(ir8_i24_ext_i32) == 8);

        struct ir8_i8_i8_i8_ext_u32
        {
            uint32_t m_op : 8;
            int32_t m_arg1 : 8;
            int32_t m_arg2 : 8;
            int32_t m_arg3 : 8;
            uint32_t m_ext_arg1 : 32;
        };
        static_assert(sizeof(ir8_i8_i8_i8_ext_u32) == 8);

        struct ir8_i8_u16_ext_u32
        {
            uint32_t m_op : 8;
            int32_t m_arg1 : 8;
            uint32_t m_arg2 : 16;
            uint32_t m_ext_arg1 : 32;
        };
        static_assert(sizeof(ir8_i8_u16_ext_u32) == 8);

        struct ir8_i24_ext_u32
        {
            uint32_t m_op : 8;
            int32_t m_arg1 : 24;
            uint32_t m_ext_arg1 : 32;
        };
        static_assert(sizeof(ir8_i24_ext_u32) == 8);

#pragma pack(push, 4)
        struct ir8_ext_i56
        {
            uint64_t m_op : 8;
            int64_t m_ext_arg1 : 56;
        };
        static_assert(sizeof(ir8_ext_i56) == 8);
#pragma pack(pop)

        union ir
        {
            uint8_t         m_op;

            ir6_i18_i8      m_ir6_i18_i8;
            ir6_i10_i16     m_ir6_i10_i16;
            ir8_i8_i8_i8    m_ir8_i8_i8_i8;
            ir8_i8_i8_u8    m_ir8_i8_i8_u8;
            ir8_i8_i16      m_ir8_i8_i16;
            ir8_i8_u16      m_ir8_i8_u16;
            ir8_i24         m_ir8_i24;
            ir8_u24         m_ir8_u24;

            ir8_i8_i8_i8_ext_u32    m_ir8_i8_i8_i8_ext_u32;
            ir8_i8_i16_ext_i32      m_ir8_i8_i16_ext_i32;
            ir8_i8_u16_ext_u32      m_ir8_i8_u16_ext_u32;
            ir8_i24_ext_i32         m_ir8_i24_ext_i32;
            ir8_i24_ext_u32         m_ir8_i24_ext_u32;
            ir8_ext_i56             m_ir8_ext_i56;
        };
        static_assert(alignof(ir) == 4);
        static_assert(offsetof(ir, m_op) == 0);
        static_assert(offsetof(ir, m_ir6_i18_i8) == 0);
    }
}
