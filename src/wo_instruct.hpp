#pragma once

#include "wo_assert.hpp"
#include "wo_basic_type.hpp"

#include <cstdint>

namespace wo
{
    /*
    R/S Addressing -127 ~ 95:
        ((0b01100000 ^ ads) & 0b11100000) ? stk + ads : reg + (ads & 0b00011111)
    */
    enum class irv2
    {               // [====== | == ======== ======== ========]
        WO_NOP,     //  NOP      00 00000000 00000000 00000000
        WO_END,     //  END      00 00000000 00000000 00000000
        WO_LOAD,    //  LOAD     [    C/G Adrsing   ] [R/SAds]
        WO_STORE,   //  STORE    [    C/G Adrsing   ] [R/SAds]
        WO_LOADEXT, //  LOADEXT  00 00000000 [  SAds 16bits  ] [ C/G Adrsing 32 bits ]
        WO_STOREEXT,//  STOREEXT 00 00000000 [  SAds 16bits  ] [ C/G Adrsing 32 bits ]
        WO_LOADLST, //  LOADLST  00 00000000 [ Count 16bits  ] [R/SAds] [   C/G Adrsing 24bit    ]...
        WO_PUSH,    //  PUSH     00 [  Stack reserved count  ]
                    //           01 [R/SAds] 00000000 00000000
                    //           10 [   C/G Adrsing 24bit    ]
                    //           11 00000000 00000000 00000000 [ C/G Adrsing 32 bits ]
        WO_POP,     //  POP      00 [    Pop stack count     ]
                    //           01 [R/SAds] 00000000 00000000
                    //           10 [   C/G Adrsing 24bit    ]
                    //           11 00000000 00000000 00000000 [ C/G Adrsing 32 bits ]
        WO_CAST,    //  CAST     00 [R/SAds] [R/SAds] [Type8b]
                    //           01 [R/SAds] [R/SAds] 00000000 // CASTITOR
                    //           10 [R/SAds] [R/SAds] 00000000 // CASTRTOI
                    //           11 ======== RESERVED ========
        WO_TYPECHK, //  TYPECHK  00 [R/SAds] [R/SAds] [Type8b] // TYPEIS
                    //           01 00000000 [R/SAds] [Type8b] // TYPEAS
                    //           10 ======== RESERVED ========
                    //           11 ======== RESERVED ========
        WO_OPIA,    //  OPIA     00 [R/SAds] [R/SAds] [R/SAds] // ADDI
                    //           01 [R/SAds] [R/SAds] [R/SAds] // SUBI
                    //           10 [R/SAds] [R/SAds] [R/SAds] // MULI
                    //           11 [R/SAds] [R/SAds] [R/SAds] // DIVI
        WO_OPIB,    //  OPIB     00 [R/SAds] [R/SAds] [R/SAds] // CDIVILR
                    //           01 [R/SAds] [R/SAds] [R/SAds] // CDIVIL
                    //           10 [R/SAds] [R/SAds] [R/SAds] // CDIVIR
                    //           11 [R/SAds] [R/SAds] [R/SAds] // CDIVIRZ
        WO_OPIC,    //  OPIC     00 [R/SAds] [R/SAds] [R/SAds] // CMODILR
                    //           01 [R/SAds] [R/SAds] [R/SAds] // CMODIL
                    //           10 [R/SAds] [R/SAds] [R/SAds] // CMODIR
                    //           11 [R/SAds] [R/SAds] [R/SAds] // CMODIRZ
        WO_OPID,    //  OPID     00 [R/SAds] [R/SAds] [R/SAds] // MODI
                    //           01 [R/SAds] [R/SAds] 00000000 // NEGI
                    //           10 [R/SAds] [R/SAds] [R/SAds] // LTI
                    //           11 [R/SAds] [R/SAds] [R/SAds] // GTI
        WO_OPIE,    //  OPIE     00 [R/SAds] [R/SAds] [R/SAds] // ELTI
                    //           01 [R/SAds] [R/SAds] 00000000 // EGTI
                    //           10 [R/SAds] [R/SAds] [R/SAds] // EQUB
                    //           11 [R/SAds] [R/SAds] [R/SAds] // NEQUB
        WO_OPRA,    //  OPRA     00 [R/SAds] [R/SAds] [R/SAds] // ADDR
                    //           01 [R/SAds] [R/SAds] [R/SAds] // SUBR
                    //           10 [R/SAds] [R/SAds] [R/SAds] // MULR
                    //           11 [R/SAds] [R/SAds] [R/SAds] // DIVR
        WO_OPRB,    //  OPRB     00 [R/SAds] [R/SAds] [R/SAds] // MODR
                    //           01 [R/SAds] [R/SAds] 00000000 // NEGR
                    //           10 [R/SAds] [R/SAds] [R/SAds] // LTR
                    //           11 [R/SAds] [R/SAds] [R/SAds] // GTR
        WO_OPRC,    //  OPRC     00 [R/SAds] [R/SAds] [R/SAds] // ELTR
                    //           01 [R/SAds] [R/SAds] [R/SAds] // EGTR
                    //           10 [R/SAds] [R/SAds] [R/SAds] // EQUR
                    //           11 [R/SAds] [R/SAds] [R/SAds] // NEQUR
        WO_OPSA,    //  OPSA     00 [R/SAds] [R/SAds] [R/SAds] // ADDS
                    //           01 [R/SAds] [R/SAds] [R/SAds] // LTS
                    //           10 [R/SAds] [R/SAds] [R/SAds] // GTS
                    //           11 [R/SAds] [R/SAds] [R/SAds] // ELTS
        WO_OPSB,    //  OPSB     00 [R/SAds] [R/SAds] [R/SAds] // EGTS
                    //           01 [R/SAds] [R/SAds] [R/SAds] // EQUS
                    //           10 [R/SAds] [R/SAds] [R/SAds] // NEQUS
                    //           11 ======== RESERVED ========
        WO_OPLA,    //  OPLA     00 [R/SAds] [R/SAds] [R/SAds] // LAND
                    //           01 [R/SAds] [R/SAds] [R/SAds] // LOR
                    //           01 [R/SAds] [R/SAds] [R/SAds] // LNOT
                    //           11 ======== RESERVED ========
        WO_IDX,     //  IDX      00 [R/SAds] [R/SAds] [R/SAds] // IDSTR
                    //           01 [R/SAds] [R/SAds] [R/SAds] // IDARR
                    //           10 [R/SAds] [R/SAds] [R/SAds] // IDDICT
                    //           11 [R/SAds] [R/SAds] [Idx 8b] // IDSTRUCT
        WO_SIDX,    //  SIDX     00 [R/SAds] [R/SAds] [R/SAds] // SIDARR
                    //           01 [R/SAds] [R/SAds] [R/SAds] // SIDDICT
                    //           10 [R/SAds] [R/SAds] [R/SAds] // SIDMAP 
                    //           11 [R/SAds] [R/SAds] [Idx 8b] // SIDSTRUCT
        WO_IDSTEXT, //  IDSTEXT  00 [R/SAds] [R/SAds] 00000000 [ C/G Adrsing 32 bits ]
        WO_SIDSTEXT,//  SIDSTEXT 00 [R/SAds] [R/SAds] 00000000 [ C/G Adrsing 32 bits ]
        WO_JMP,     //  JMP      00 [  Near wo-code abs addr ] // JMP
                    //           01 ======== RESERVED ========
                    //           10 [  Near wo-code abs addr ] // JMPF
                    //           11 [  Near wo-code abs addr ] // JMPT
        WO_JMPGC,   //  JMPGC    00 [  Near wo-code abs addr ] // JMPGC
                    //           01 ======== RESERVED ========
                    //           10 [  Near wo-code abs addr ] // JMPGCF
                    //           11 [  Near wo-code abs addr ] // JMPGCT
        WO_RET,     //  RET      00 ======== RESERVED ========
                    //           01 00000000 [PopCount 16bits]
        WO_CALLN,   //  CALLN    00 [  Near wo-code abs addr ] [ 00000000 00000000 00000000 00000000 ]  CALLNWO
                    //           01 [                 Native function address 56 bit                  ]  CALLNJIT
                    //           10 [                 Native function address 56 bit                  ]  CALLNFP
                    //           11 ======== RESERVED ========
        WO_CALL,    //  CALL     00 [R/SAds] 00000000 00000000
                    //           01 [    C/G Adrsing 24bit    ]
                    //           10 00000000 00000000 00000000 [ C/G Adrsing 32 bits ]
                    //           11 ======== RESERVED ========
        WO_CONS,    //  CONS     00 [R/SAds] [ Unit count 16b] // MKARR
                    //           01 [R/SAds] [ Unit count 16b] // MKMAP
                    //           10 [R/SAds] [ Unit count 16b] // MKSTRUCT
                    //           11 [R/SAds] [ Cap count 16b ] // MKCLOSURE
        WO_CONSEXT, //  CONSEXT  00 [R/SAds] 00000000 00000000 [   Unit count 32bits   ] // MKARREXT
                    //           01 [R/SAds] 00000000 00000000 [   Unit count 32bits   ] // MKMAPEXT
                    //           10 [R/SAds] 00000000 00000000 [   Unit count 32bits   ] // MKSTRUCTEXT
                    //           11 [R/SAds] 00000000 00000000 [    Cap count 32bits   ] // MKCLOSUREEXT
        WO_UNPACK,  //  UNPACK   00 [R/SAds] [  Unit count   ] // UNPACK
        WO_PACK,    //  PACK     00 [R/SAds] [FnArgc] [ClArgc] 
                    //           01 [R/SAds] [ FnArgc 16bits ] [     ClArgc 32bits     ]
        WO_LDS,     //  LDS      00 [R/SAds] [ StackBp offset]
                    //           01 [R/SAds] 00000000 00000000 [ StackBp offset 32bits ]
                    //           10 [R/SAds] [R/SAds] 00000000
                    //           11 ======== RESERVED ========
        WO_STS,     //  STS      00 [R/SAds] [ StackBp offset]
                    //           01 [R/SAds] 00000000 00000000 [ StackBp offset 32bits ]
                    //           10 [R/SAds] [R/SAds] 00000000
                    //           11 ======== RESERVED ========
        WO_PANIC,   //  PANIC    00 [R/SAds] 00000000 00000000
                    //           01 ======== RESERVED ======== 
                    //           10 [   C/G Adrsing 24bit    ]
                    //           11 00000000 00000000 00000000 [ C/G Adrsing 32 bits ]
    };

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
            callnjit = calln | 0b01,
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
            movicastr = WO_OPCODE_SPACE(MOVICASTR),
            WO_OPCODE_DR2SG(movicastr),
            ///////////////////////////////////
            movrcasti = WO_OPCODE_SPACE(MOVRCASTI),
            WO_OPCODE_DR2SG(movrcasti),
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
