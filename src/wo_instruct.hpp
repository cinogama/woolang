#pragma once

#include "wo_assert.hpp"
#include "wo_basic_type.hpp"

#include <cstdint>

namespace wo
{
    struct instruct
    {

        /*
        *  OPCODE(DR) [OPARGS...]
        *
        *  OPCODE 6bit  The main command of instruct (0-63)
        *  DR     2bit  Used for describing OPCODE  (00 01 10 11)
        *
        *  RS will using variable length ircode.
        *
        */
#define WO_OPCODE_SPACE(OPCODE) ((uint8_t)((uint8_t)WO_##OPCODE << (uint8_t)2))

        enum opcode : uint8_t
        {
            nop = WO_OPCODE_SPACE(NOP),
            mov = WO_OPCODE_SPACE(MOV),
            psh = WO_OPCODE_SPACE(PSH),
            pop = WO_OPCODE_SPACE(POP),
            addi = WO_OPCODE_SPACE(ADDI),
            subi = WO_OPCODE_SPACE(SUBI),
            muli = WO_OPCODE_SPACE(MULI),
            divi = WO_OPCODE_SPACE(DIVI),
            modi = WO_OPCODE_SPACE(MODI),
            addr = WO_OPCODE_SPACE(ADDR),
            subr = WO_OPCODE_SPACE(SUBR),
            mulr = WO_OPCODE_SPACE(MULR),
            divr = WO_OPCODE_SPACE(DIVR),
            modr = WO_OPCODE_SPACE(MODR),
            addh = WO_OPCODE_SPACE(ADDH),
            subh = WO_OPCODE_SPACE(SUBH),
            adds = WO_OPCODE_SPACE(ADDS),
            idarr = WO_OPCODE_SPACE(IDARR),
            sidarr = WO_OPCODE_SPACE(SIDARR),
            iddict = WO_OPCODE_SPACE(IDDICT),
            siddict = WO_OPCODE_SPACE(SIDDICT),
            sidmap = WO_OPCODE_SPACE(SIDMAP),
            idstruct = WO_OPCODE_SPACE(IDSTRUCT),
            sidstruct = WO_OPCODE_SPACE(SIDSTRUCT),
            idstr = WO_OPCODE_SPACE(IDSTR),
            lti = WO_OPCODE_SPACE(LTI),
            gti = WO_OPCODE_SPACE(GTI),
            elti = WO_OPCODE_SPACE(ELTI),
            egti = WO_OPCODE_SPACE(EGTI),
            land = WO_OPCODE_SPACE(LAND),
            lor = WO_OPCODE_SPACE(LOR),
            ltx = WO_OPCODE_SPACE(LTX),
            gtx = WO_OPCODE_SPACE(GTX),
            eltx = WO_OPCODE_SPACE(ELTX),
            egtx = WO_OPCODE_SPACE(EGTX),
            ltr = WO_OPCODE_SPACE(LTR),
            gtr = WO_OPCODE_SPACE(GTR),
            eltr = WO_OPCODE_SPACE(ELTR),
            egtr = WO_OPCODE_SPACE(EGTR),
            equr = WO_OPCODE_SPACE(EQUR),
            nequr = WO_OPCODE_SPACE(NEQUR),
            equs = WO_OPCODE_SPACE(EQUS),
            nequs = WO_OPCODE_SPACE(NEQUS),
            equb = WO_OPCODE_SPACE(EQUB),
            nequb = WO_OPCODE_SPACE(NEQUB),
            jnequb = WO_OPCODE_SPACE(JNEQUB),
            call = WO_OPCODE_SPACE(CALL),
            calln = WO_OPCODE_SPACE(CALLN),
            ret = WO_OPCODE_SPACE(RET),
            jt = WO_OPCODE_SPACE(JT),
            jf = WO_OPCODE_SPACE(JF),
            jmp = WO_OPCODE_SPACE(JMP),
            mkarr = WO_OPCODE_SPACE(MKARR),
            mkmap = WO_OPCODE_SPACE(MKMAP),
            mkstruct = WO_OPCODE_SPACE(MKSTRUCT),
            mkunion = WO_OPCODE_SPACE(MKUNION),
            mkclos = WO_OPCODE_SPACE(MKCLOS),
            unpackargs = WO_OPCODE_SPACE(UNPACKARGS),
            movcast = WO_OPCODE_SPACE(MOVCAST),
            typeas = WO_OPCODE_SPACE(TYPEAS),
            abrt = WO_OPCODE_SPACE(ABRT),
            lds = WO_OPCODE_SPACE(LDS),
            sts = WO_OPCODE_SPACE(STS),
            ext = WO_OPCODE_SPACE(EXT),
        };

        enum extern_opcode_page_0 : uint8_t
        {
            panic = WO_OPCODE_SPACE(PANIC),
            packargs = WO_OPCODE_SPACE(PACKARGS),
            cdivilr = WO_OPCODE_SPACE(CDIVILR),
            cdivil = WO_OPCODE_SPACE(CDIVIL),
            cdivir = WO_OPCODE_SPACE(CDIVIR),
            cdivirz = WO_OPCODE_SPACE(CDIVIRZ),
            popn = WO_OPCODE_SPACE(POPN),
        };
        enum extern_opcode_page_1 : uint8_t
        {
        };
        enum extern_opcode_page_2 : uint8_t
        {
        };
        enum extern_opcode_page_3 : uint8_t
        {
            funcbegin = WO_OPCODE_SPACE(FUNCBEGIN),
            funcend = WO_OPCODE_SPACE(FUNCEND),

#undef WO_OPCODE_SPACE
        };

        opcode opcode_dr; wo_static_assert_size(opcode, 1);

        inline constexpr instruct(opcode _opcode, uint8_t _dr)
            : opcode_dr(opcode(_opcode | _dr))
        {
            wo_assert((_opcode & 0b00000011) == 0, "illegal value for '_opcode': it's low 2-bit should be 0.");
            wo_assert((_dr & 0b11111100) == 0, "illegal value for '_dr': it should be less then 0x04.");
        }

    };
    wo_static_assert_size(instruct, 1);
}