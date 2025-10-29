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

#define WO_OPCODE_SPACE(OPCODE)\
    ((uint8_t)((uint8_t)WO_##OPCODE << (uint8_t)2))

#define WO_OPCODE_DR2SG(OPCODE)\
    OPCODE##gg = OPCODE | 0b00, \
    OPCODE##gs = OPCODE | 0b01, \
    OPCODE##sg = OPCODE | 0b10, \
    OPCODE##ss = OPCODE | 0b11

        /*
        Checklist when adding instructions:
        1. Add instruction prototype in wo.h
        2. Add instruction here and provide DR parsing forms
        3. Add IR generation instruction interface in wo_compiler_ir.hpp
        4. Implement bytecode generation from instructions in wo_compiler_ir.cpp
        5. Add bytecode to readable form display in dump_program_bin in wo_vm.cpp
        6. Implement interpretation execution in wo_vm.cpp
        7. Add instruction mapping for IRS in wo_compiler_jit.cpp
        8. Implement JIT generation of instructions in wo_compiler_jit.cpp
        */

        enum opcode : uint8_t
        {
            nop = WO_OPCODE_SPACE(NOP),
            nop0 = nop | 0b00,
            nop1 = nop | 0b01,
            nop2 = nop | 0b10,
            nop3 = nop | 0b11,
            ///////////////////////////////////
            mov = WO_OPCODE_SPACE(MOV),
            WO_OPCODE_DR2SG(mov),
            ///////////////////////////////////
            psh = WO_OPCODE_SPACE(PSH),
            pshr = psh | 0b00,
            pshg = psh | 0b01,
            pshs = psh | 0b11,
            ///////////////////////////////////
            pop = WO_OPCODE_SPACE(POP),
            popr = pop | 0b00,
            popg = pop | 0b01,
            pops = pop | 0b11,
            ///////////////////////////////////
            addi = WO_OPCODE_SPACE(ADDI),
            WO_OPCODE_DR2SG(addi),
            subi = WO_OPCODE_SPACE(SUBI),
            WO_OPCODE_DR2SG(subi),
            muli = WO_OPCODE_SPACE(MULI),
            WO_OPCODE_DR2SG(muli),
            divi = WO_OPCODE_SPACE(DIVI),
            WO_OPCODE_DR2SG(divi),
            modi = WO_OPCODE_SPACE(MODI),
            WO_OPCODE_DR2SG(modi),
            addr = WO_OPCODE_SPACE(ADDR),
            WO_OPCODE_DR2SG(addr),
            subr = WO_OPCODE_SPACE(SUBR),
            WO_OPCODE_DR2SG(subr),
            mulr = WO_OPCODE_SPACE(MULR),
            WO_OPCODE_DR2SG(mulr),
            divr = WO_OPCODE_SPACE(DIVR),
            WO_OPCODE_DR2SG(divr),
            modr = WO_OPCODE_SPACE(MODR),
            WO_OPCODE_DR2SG(modr),
            addh = WO_OPCODE_SPACE(ADDH),
            WO_OPCODE_DR2SG(addh),
            subh = WO_OPCODE_SPACE(SUBH),
            WO_OPCODE_DR2SG(subh),
            adds = WO_OPCODE_SPACE(ADDS),
            WO_OPCODE_DR2SG(adds),
            idarr = WO_OPCODE_SPACE(IDARR),
            WO_OPCODE_DR2SG(idarr),
            sidarr = WO_OPCODE_SPACE(SIDARR),
            WO_OPCODE_DR2SG(sidarr),
            iddict = WO_OPCODE_SPACE(IDDICT),
            WO_OPCODE_DR2SG(iddict),
            siddict = WO_OPCODE_SPACE(SIDDICT),
            WO_OPCODE_DR2SG(siddict),
            sidmap = WO_OPCODE_SPACE(SIDMAP),
            WO_OPCODE_DR2SG(sidmap),
            idstruct = WO_OPCODE_SPACE(IDSTRUCT),
            WO_OPCODE_DR2SG(idstruct),
            sidstruct = WO_OPCODE_SPACE(SIDSTRUCT),
            WO_OPCODE_DR2SG(sidstruct),
            idstr = WO_OPCODE_SPACE(IDSTR),
            WO_OPCODE_DR2SG(idstr),
            lti = WO_OPCODE_SPACE(LTI),
            WO_OPCODE_DR2SG(lti),
            gti = WO_OPCODE_SPACE(GTI),
            WO_OPCODE_DR2SG(gti),
            elti = WO_OPCODE_SPACE(ELTI),
            WO_OPCODE_DR2SG(elti),
            egti = WO_OPCODE_SPACE(EGTI),
            WO_OPCODE_DR2SG(egti),
            land = WO_OPCODE_SPACE(LAND),
            WO_OPCODE_DR2SG(land),
            lor = WO_OPCODE_SPACE(LOR),
            WO_OPCODE_DR2SG(lor),
            ltx = WO_OPCODE_SPACE(LTX),
            WO_OPCODE_DR2SG(ltx),
            gtx = WO_OPCODE_SPACE(GTX),
            WO_OPCODE_DR2SG(gtx),
            eltx = WO_OPCODE_SPACE(ELTX),
            WO_OPCODE_DR2SG(eltx),
            egtx = WO_OPCODE_SPACE(EGTX),
            WO_OPCODE_DR2SG(egtx),
            ltr = WO_OPCODE_SPACE(LTR),
            WO_OPCODE_DR2SG(ltr),
            gtr = WO_OPCODE_SPACE(GTR),
            WO_OPCODE_DR2SG(gtr),
            eltr = WO_OPCODE_SPACE(ELTR),
            WO_OPCODE_DR2SG(eltr),
            egtr = WO_OPCODE_SPACE(EGTR),
            WO_OPCODE_DR2SG(egtr),
            equr = WO_OPCODE_SPACE(EQUR),
            WO_OPCODE_DR2SG(equr),
            nequr = WO_OPCODE_SPACE(NEQUR),
            WO_OPCODE_DR2SG(nequr),
            equs = WO_OPCODE_SPACE(EQUS),
            WO_OPCODE_DR2SG(equs),
            nequs = WO_OPCODE_SPACE(NEQUS),
            WO_OPCODE_DR2SG(nequs),
            equb = WO_OPCODE_SPACE(EQUB),
            WO_OPCODE_DR2SG(equb),
            nequb = WO_OPCODE_SPACE(NEQUB),
            WO_OPCODE_DR2SG(nequb),
            ///////////////////////////////////
            jnequb = WO_OPCODE_SPACE(JNEQUB),
            jnequbg = jnequb | 0b00,
            jnequbs = jnequb | 0b10,
            jnequbgcg = jnequb | 0b01,
            jnequbgcs = jnequb | 0b11,
            ///////////////////////////////////
            call = WO_OPCODE_SPACE(CALL),
            callg = call | 0b00,
            calls = call | 0b10,
            ///////////////////////////////////
            calln = WO_OPCODE_SPACE(CALLN),
            callnwo = calln | 0b00,
            callnfpslow = calln | 0b01,
            callnfp = calln | 0b11,
            ///////////////////////////////////
            movicas = WO_OPCODE_SPACE(MOVICAS),
            WO_OPCODE_DR2SG(movicas),
            ///////////////////////////////////
            setip = WO_OPCODE_SPACE(SETIP),
            jmp = setip | 0b00,
            jmpf = setip | 0b10,
            jmpt = setip | 0b11,
            ///////////////////////////////////
            mkcontain = WO_OPCODE_SPACE(MKCONTAIN),
            mkarrg = mkcontain | 0b00,
            mkarrs = mkcontain | 0b10,
            mkmapg = mkcontain | 0b01,
            mkmaps = mkcontain | 0b11,
            ///////////////////////////////////
            setipgc = WO_OPCODE_SPACE(SETIPGC),
            jmpgc = setipgc | 0b00,
            jmpgcf = setipgc | 0b10,
            jmpgct = setipgc | 0b11,
            ///////////////////////////////////
            mkstruct = WO_OPCODE_SPACE(MKSTRUCT),
            mkstructg = mkstruct | 0b00,
            mkstructs = mkstruct | 0b10,
            ///////////////////////////////////
            mkunion = WO_OPCODE_SPACE(MKUNION),
            WO_OPCODE_DR2SG(mkunion),
            ///////////////////////////////////
            mkclos = WO_OPCODE_SPACE(MKCLOS),
            mkcloswo = mkclos | 0b00,
            mkclosfp = mkclos | 0b10,
            ///////////////////////////////////
            unpack = WO_OPCODE_SPACE(UNPACK),
            unpackg = unpack | 0b00,
            unpacks = unpack | 0b10,
            ///////////////////////////////////
            movcast = WO_OPCODE_SPACE(MOVCAST),
            WO_OPCODE_DR2SG(movcast),
            ///////////////////////////////////
            typeas = WO_OPCODE_SPACE(TYPEAS),
            typeasg = typeas | 0b00,
            typeass = typeas | 0b10,
            typeisg = typeas | 0b01,
            typeiss = typeas | 0b11,
            ///////////////////////////////////
            endproc = WO_OPCODE_SPACE(ENDPROC),
            abrt = endproc | 0b00,
            end = endproc | 0b10,
            ret = endproc | 0b01,
            retn = endproc | 0b11,
            ///////////////////////////////////
            lds = WO_OPCODE_SPACE(LDS),
            WO_OPCODE_DR2SG(lds),
            sts = WO_OPCODE_SPACE(STS),
            WO_OPCODE_DR2SG(sts),
            ///////////////////////////////////
            ext = WO_OPCODE_SPACE(EXT),
            ext0 = ext | 0b00,
            ext1 = ext | 0b01,
            ext2 = ext | 0b10,
            ext3 = ext | 0b11,
        };

        enum extern_opcode_page_0 : uint8_t
        {
            panic = WO_OPCODE_SPACE(PANIC),
            panicg = panic | 0b00,
            panics = panic | 0b10,
            pack = WO_OPCODE_SPACE(PACK),
            packg = pack | 0b00,
            packs = pack | 0b10,
            cdivilr = WO_OPCODE_SPACE(CDIVILR),
            WO_OPCODE_DR2SG(cdivilr),
            cdivil = WO_OPCODE_SPACE(CDIVIL),
            cdivilg = cdivil | 0b00,
            cdivils = cdivil | 0b10,
            cdivir = WO_OPCODE_SPACE(CDIVIR),
            cdivirg = cdivir | 0b00,
            cdivirs = cdivir | 0b10,
            cdivirz = WO_OPCODE_SPACE(CDIVIRZ),
            cdivirzg = cdivirz | 0b00,
            cdivirzs = cdivirz | 0b10,
            popn = WO_OPCODE_SPACE(POPN),
            popng = popn | 0b00,
            popns = popn | 0b10,
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