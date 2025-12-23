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

        union ir
        {
            uint8_t             m_op;

            struct {
                uint32_t        m_ir32;     // Low
                union
                {
                    int32_t     m_ext32;    // High
                    uint32_t    m_extu32;
                };
                union
                {
                    int32_t     m_ext32_2;
                    uint32_t    m_extu32_2;
                };
            };
        };
        static_assert(alignof(ir) == 4);
        static_assert(0 == offsetof(ir, m_op));
        static_assert(0 == offsetof(ir, m_ir32));

#define WO_IR_FETCH_MASK(WIDTH) \
    ((1U << (WIDTH)) - 1)

#define WO_IR_FETCH_UNSIGNED(IR, WIDTH, SHIFT) \
    (((IR)->m_ir32 >> (32 - (WIDTH) - (SHIFT))) & WO_IR_FETCH_MASK(WIDTH))

#define WO_IR_FETCH_SIGNED(IR, WIDTH, SHIFT) \
    ((int32_t)((((uint32_t)((IR)->m_ir32 >> (32 - (WIDTH) - (SHIFT)))) & WO_IR_FETCH_MASK(WIDTH)) << (32 - (WIDTH))) >> (32 - (WIDTH)))

#define WO_OPNUM_TYPE_1 int8_t
#define WO_OPNUM_TYPE_2 int8_t
#define WO_OPNUM_TYPE_3 int8_t
#define WO_OPNUM_TYPE_4 int8_t
#define WO_OPNUM_TYPE_5 int8_t
#define WO_OPNUM_TYPE_6 int8_t
#define WO_OPNUM_TYPE_7 int8_t
#define WO_OPNUM_TYPE_8 int8_t
#define WO_OPNUM_TYPE_9 int16_t
#define WO_OPNUM_TYPE_10 int16_t
#define WO_OPNUM_TYPE_11 int16_t
#define WO_OPNUM_TYPE_12 int16_t
#define WO_OPNUM_TYPE_13 int16_t
#define WO_OPNUM_TYPE_14 int16_t
#define WO_OPNUM_TYPE_15 int16_t
#define WO_OPNUM_TYPE_16 int16_t
#define WO_OPNUM_TYPE_17 int32_t
#define WO_OPNUM_TYPE_18 int32_t
#define WO_OPNUM_TYPE_19 int32_t
#define WO_OPNUM_TYPE_20 int32_t
#define WO_OPNUM_TYPE_21 int32_t
#define WO_OPNUM_TYPE_22 int32_t
#define WO_OPNUM_TYPE_23 int32_t
#define WO_OPNUM_TYPE_24 int32_t

#define _WO_OPNUM_UNSIGNED_TYPE(TYPE) u##TYPE
#define WO_OPNUM_UNSIGNED_TYPE(TYPE) _WO_OPNUM_UNSIGNED_TYPE(TYPE)

#define WO_OPNUM_SIGNED(N, IR, WIDTH, SHIFT)                    \
    const WO_OPNUM_TYPE_##WIDTH p##N##_i##WIDTH =               \
        static_cast<WO_OPNUM_TYPE_##WIDTH>(WO_IR_FETCH_SIGNED(IR, WIDTH, SHIFT))

#define WO_OPNUM_UNSIGNED(N, IR, WIDTH, SHIFT)                  \
    const WO_OPNUM_UNSIGNED_TYPE(WO_OPNUM_TYPE_##WIDTH)         \
        p##N##_u##WIDTH = static_cast<                          \
            WO_OPNUM_UNSIGNED_TYPE(WO_OPNUM_TYPE_##WIDTH)>(     \
                WO_IR_FETCH_UNSIGNED(IR, WIDTH, SHIFT))

#define WO_FORMAL_IR6_I18_I8_IRCOUNTOF 1
#define WO_FORMAL_IR6_I18_I8(IR)                                \
        WO_OPNUM_SIGNED(1, IR, 18, 6);                          \
        WO_OPNUM_SIGNED(2, IR, 8, 24)

#define WO_FORMAL_IR8_8_I8_U8_IRCOUNTOF 1
#define WO_FORMAL_IR8_8_I8_U8(IR)                               \
        WO_OPNUM_SIGNED(1, IR, 8, 16);                          \
        WO_OPNUM_UNSIGNED(2, IR, 8, 24)

#define WO_FORMAL_IR8_8_I16_EI32_IRCOUNTOF 2
#define WO_FORMAL_IR8_8_I16_EI32(IR)                            \
        WO_OPNUM_SIGNED(1, IR, 16, 16);                          \
        const int32_t p2_i32 = IR->m_ext32

#define WO_FORMAL_IR8_8_U16_IRCOUNTOF 1
#define WO_FORMAL_IR8_8_U16(IR)                                 \
        WO_OPNUM_UNSIGNED(1, IR, 16, 16)                         

#define WO_FORMAL_IR8_24_IRCOUNTOF 1
#define WO_FORMAL_IR8_24(IR)

#define WO_FORMAL_IR8_24_EI32_IRCOUNTOF 2
#define WO_FORMAL_IR8_24_EI32(IR)                               \
        const int32_t p1_i32 = IR->m_ext32

#define WO_FORMAL_IR8_24_EU32_IRCOUNTOF 2
#define WO_FORMAL_IR8_24_EU32(IR)                               \
        const uint32_t p1_u32 = IR->m_extu32

#define WO_FORMAL_IR8_I8_I8_8_IRCOUNTOF 1
#define WO_FORMAL_IR8_I8_I8_8(IR)                               \
        WO_OPNUM_SIGNED(1, IR, 8, 8);                           \
        WO_OPNUM_SIGNED(2, IR, 8, 16)                          

#define WO_FORMAL_IR8_I8_I8_8_EU32_IRCOUNTOF 2
#define WO_FORMAL_IR8_I8_I8_8_EU32(IR)                          \
        WO_OPNUM_SIGNED(1, IR, 8, 8);                           \
        WO_OPNUM_SIGNED(2, IR, 8, 16);                          \
        const uint32_t p3_u32 = IR->m_extu32

#define WO_FORMAL_IR8_I8_I8_I8_IRCOUNTOF 1
#define WO_FORMAL_IR8_I8_I8_I8(IR)                              \
        WO_OPNUM_SIGNED(1, IR, 8, 8);                           \
        WO_OPNUM_SIGNED(2, IR, 8, 16);                          \
        WO_OPNUM_SIGNED(3, IR, 8, 24)

#define WO_FORMAL_IR8_I8_I8_U8_IRCOUNTOF 1
#define WO_FORMAL_IR8_I8_I8_U8(IR)                              \
        WO_OPNUM_SIGNED(1, IR, 8, 8);                           \
        WO_OPNUM_SIGNED(2, IR, 8, 16);                          \
        WO_OPNUM_UNSIGNED(3, IR, 8, 24)

#define WO_FORMAL_IR8_I8_U8_U8_IRCOUNTOF 1
#define WO_FORMAL_IR8_I8_U8_U8(IR)                              \
        WO_OPNUM_SIGNED(1, IR, 8, 8);                           \
        WO_OPNUM_UNSIGNED(2, IR, 8, 16);                        \
        WO_OPNUM_UNSIGNED(3, IR, 8, 24)

#define WO_FORMAL_IR8_I8_16_IRCOUNTOF 1
#define WO_FORMAL_IR8_I8_16(IR)                                 \
        WO_OPNUM_SIGNED(1, IR, 8, 8)

#define WO_FORMAL_IR8_I8_16_EI32_IRCOUNTOF 2
#define WO_FORMAL_IR8_I8_16_EI32(IR)                            \
        WO_OPNUM_SIGNED(1, IR, 8, 8);                           \
        const int32_t p2_i32 = IR->m_ext32

#define WO_FORMAL_IR8_I8_16_EU32_IRCOUNTOF 2
#define WO_FORMAL_IR8_I8_16_EU32(IR)                            \
        WO_OPNUM_SIGNED(1, IR, 8, 8);                           \
        const int32_t p2_u32 = IR->m_extu32

#define WO_FORMAL_IR8_I8_I16_IRCOUNTOF 1
#define WO_FORMAL_IR8_I8_I16(IR)                                \
        WO_OPNUM_SIGNED(1, IR, 8, 8);                           \
        WO_OPNUM_SIGNED(2, IR, 16, 16)                

#define WO_FORMAL_IR8_I8_U16_IRCOUNTOF 1
#define WO_FORMAL_IR8_I8_U16(IR)                                \
        WO_OPNUM_SIGNED(1, IR, 8, 8);                           \
        WO_OPNUM_UNSIGNED(2, IR, 16, 16)

#define WO_FORMAL_IR8_I8_U16_EU32_IRCOUNTOF 2
#define WO_FORMAL_IR8_I8_U16_EU32(IR)                           \
        WO_OPNUM_SIGNED(1, IR, 8, 8);                           \
        WO_OPNUM_UNSIGNED(2, IR, 16, 16);                       \
        const uint32_t p3_u32 = IR->m_extu32

#define WO_FORMAL_IR8_I8_U16_EU32_E32_IRCOUNTOF 3
#define WO_FORMAL_IR8_I8_U16_EU32_E32(IR)                       \
        WO_OPNUM_SIGNED(1, IR, 8, 8);                           \
        WO_OPNUM_UNSIGNED(2, IR, 16, 16);                       \
        const uint32_t p3_u32 = IR->m_extu32

#define WO_FORMAL_IR8_I8_U16_EU64_IRCOUNTOF 3
#define WO_FORMAL_IR8_I8_U16_EU64(IR)                           \
        WO_OPNUM_SIGNED(1, IR, 8, 8);                           \
        WO_OPNUM_UNSIGNED(2, IR, 16, 16);                       \
        const uint64_t p3_u64 =                                 \
            (static_cast<uint64_t>(IR->m_extu32) << static_cast<uint64_t>(32)) | IR->m_extu32_2

#define WO_FORMAL_IR8_I24_IRCOUNTOF 1
#define WO_FORMAL_IR8_I24(IR)                                   \
        WO_OPNUM_SIGNED(1, IR, 24, 8)

#define WO_FORMAL_IR8_I24_EI32_IRCOUNTOF 2
#define WO_FORMAL_IR8_I24_EI32(IR)                              \
        WO_OPNUM_SIGNED(1, IR, 24, 8);                          \
        const int32_t p2_i32 = IR->m_ext32

#define WO_FORMAL_IR8_U24_IRCOUNTOF 1
#define WO_FORMAL_IR8_U24(IR)                                   \
        WO_OPNUM_UNSIGNED(1, IR, 24, 8)

#define WO_FORMAL_IR8_U24_E32_IRCOUNTOF 2
#define WO_FORMAL_IR8_U24_E32(IR)                               \
        WO_OPNUM_UNSIGNED(1, IR, 24, 8)

#define WO_FORMAL_IR8_EU56_IRCOUNTOF 2
#define WO_FORMAL_IR8_EU56(IR)                                  \
        const uint64_t p1_u56 =                                 \
            (static_cast<uint64_t>(                             \
                WO_IR_FETCH_UNSIGNED(IR, 24, 8))                \
            << static_cast<uint64_t>(32))                       \
            | static_cast<uint64_t>(IR->m_extu32)
    }
}
