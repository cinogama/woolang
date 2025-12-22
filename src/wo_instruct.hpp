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
            uint8_t m_op;

            struct {
                uint32_t m_ir32;        // Low
                union
                {
                    int32_t m_ext32;    // High
                    uint32_t m_extu32;
                };
            };
        };
        static_assert(alignof(ir) == 4);
        static_assert(0 == offsetof(ir, m_op));
        static_assert(0 == offsetof(ir, m_ir32));

#define WO_IR_FETCH_MASK(WIDTH)                                 \
        (static_cast<uint32_t>(0xFFFF'FFFF)                     \
            << static_cast<uint32_t>(32 - (WIDTH)))
#define WO_IR_SHIFT_FETCH_MASK(WIDTH, SHIFT)                    \
        (WO_IR_FETCH_MASK(WIDTH) >> static_cast<uint32_t>(WIDTH))

#define WO_IR_FETCH_UNSIGNED(IR, WIDTH, SHIFT)                  \
        ((IR->m_ir32 & WO_IR_SHIFT_FETCH_MASK(WIDTH, SHIFT))    \
        >> static_cast<uint32_t>(32 - (WIDTH) - (SHIFT)))
#define WO_IR_FETCH_SIGNED(IR, WIDTH, SHIFT)                    \
        (static_cast<int32_t>(                                  \
            (IR->m_ir32 & WO_IR_SHIFT_FETCH_MASK(WIDTH, SHIFT)) \
            << static_cast<uint32_t>(SHIFT))                    \
        >> static_cast<int32_t>(32 - (WIDTH)))

#define WO_OPNUM_SIGNED(N, IR, WIDTH, SHIFT)                    \
        const int32_t p##N##_i##WIDTH = WO_IR_FETCH_SIGNED(IR, WIDTH, SHIFT)
#define WO_OPNUM_UNSIGNED(N, IR, WIDTH, SHIFT)                  \
        const uint32_t p##N##_u##WIDTH = WO_IR_FETCH_UNSIGNED(IR, WIDTH, SHIFT)

#define WO_FORMAL_IR6_I18_I8_IRCOUNTOF 1
#define WO_FORMAL_IR6_I18_I8(IR)                                \
        WO_OPNUM_SIGNED(1, IR, 18, 6);                          \
        WO_OPNUM_SIGNED(2, IR, 8, 24)

#define WO_FORMAL_IR8_8_I8_U8_IRCOUNTOF 1
#define WO_FORMAL_IR8_8_I8_U8(IR)                               \
        WO_OPNUM_SIGNED(1, IR, 8, 16);                          \
        WO_OPNUM_UNSIGNED(2, IR, 8, 24)

#define WO_FORMAL_IR8_8_U16_IRCOUNTOF 1
#define WO_FORMAL_IR8_8_U16(IR)                                 \
        WO_OPNUM_SIGNED(1, IR, 16, 16)                         

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
        const int32_t p2_i32 = IR->m_extu32

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

#define WO_FORMAL_IR8_I24_IRCOUNTOF 1
#define WO_FORMAL_IR8_I24(IR)                                   \
        WO_OPNUM_SIGNED(1, IR, 24, 8)

#define WO_FORMAL_IR8_U24_IRCOUNTOF 1
#define WO_FORMAL_IR8_U24(IR)                                   \
        WO_OPNUM_UNSIGNED(1, IR, 24, 8)

#define WO_FORMAL_IR8_U24_E32_IRCOUNTOF 2
#define WO_FORMAL_IR8_U24_E32(IR)                               \
        WO_OPNUM_UNSIGNED(1, IR, 24, 8)

#define WO_FORMAL_IR8_EU56_IRCOUNTOF 2
#define WO_FORMAL_IR8_EU56(IR)                                  \
        const uint64_t p1_u56 = static_cast<uint64_t>(          \
            WO_IR_FETCH_UNSIGNED(IR, 24, 8))                    \
        << static_cast<uint64_t>(32) | static_cast<uint64_t>(IR->m_extu32)

    }
}
