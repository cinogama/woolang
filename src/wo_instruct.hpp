#pragma once

#include "wo_assert.hpp"
#include "wo_basic_type.hpp"

#include <cstdint>

namespace wo
{
    struct instruct
    {
        // IR CODE:
        /*
        *  OPCODE(DR) [OPARGS...]
        *
        *  OPCODE 6bit  The main command of instruct (0-63)
        *  DR     2bit  Used for describing OPCODE  (00 01 10 11)
        *
        *  RS will using variable length ircode.
        *
        */

        enum opcode : uint8_t
        {
#define WO_OPCODE_SPACE <<2
            nop = 0 WO_OPCODE_SPACE,        // nop(TKPLS)                                                   1 byte

            mov = 1 WO_OPCODE_SPACE,        // mov(dr)          REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF         3-9 byte
            psh = 2 WO_OPCODE_SPACE,        // psh(dr_0)        REGID(1BYTE)/DIFF(4BYTE)                    2-5 byte
            pop = 3 WO_OPCODE_SPACE,        // pop(dr_STORED?)  REGID(1BYTE)/DIFF(4BYTE)/COUNT(2BYTE)       2-5 byte

            addi = 4 WO_OPCODE_SPACE,       // add(dr)          REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF         3-9 byte
            subi = 5 WO_OPCODE_SPACE,       // sub
            muli = 6 WO_OPCODE_SPACE,       // mul
            divi = 7 WO_OPCODE_SPACE,       // div
            modi = 8 WO_OPCODE_SPACE,       // mod

            addr = 9 WO_OPCODE_SPACE,       // add(dr)          REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF         3-9 byte
            subr = 10 WO_OPCODE_SPACE,      // sub
            mulr = 11 WO_OPCODE_SPACE,      // mul
            divr = 12 WO_OPCODE_SPACE,      // div
            modr = 13 WO_OPCODE_SPACE,      // mod

            addh = 14 WO_OPCODE_SPACE,      // add(dr)          REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF         3-9 byte
            subh = 15 WO_OPCODE_SPACE,      // sub

            adds = 16 WO_OPCODE_SPACE,      // add(dr)          REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF         3-9 byte
     
            idarr = 17 WO_OPCODE_SPACE,     // idarr(dr)        REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF [Used for array]
            sidarr = 18 WO_OPCODE_SPACE,    // sidarr(dr)       REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF REGID

            iddict = 19 WO_OPCODE_SPACE,    // iddict(dr)       REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF [Used for dict]
            siddict = 20 WO_OPCODE_SPACE,   // siddict(dr)      REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF  REGID 
            sidmap = 21 WO_OPCODE_SPACE,    // sidmap(dr)       REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF REGID 

            idstruct = 22 WO_OPCODE_SPACE,  // idstruct(dr_0)   REGID(1BYTE)/DIFF(4BYTE) OFFSET(2BYTE)      4-7 byte
            sidstruct = 23 WO_OPCODE_SPACE, // sidstruct(dr_0)  REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF OFFSET(2BYTE) 5-11 byte

            idstr = 24 WO_OPCODE_SPACE,     // idstr(dr)       REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF [Used for string]

            //  Logic operator, the result will store to logic_state
            lti = 25 WO_OPCODE_SPACE,       // lt
            gti = 26 WO_OPCODE_SPACE,       // gt
            elti = 27 WO_OPCODE_SPACE,      // elt
            egti = 28 WO_OPCODE_SPACE,      // egt

            land = 29 WO_OPCODE_SPACE,      // land             
            lor = 30 WO_OPCODE_SPACE,       // lor

            ltx = 31 WO_OPCODE_SPACE,       // lt
            gtx = 32 WO_OPCODE_SPACE,       // gt
            eltx = 33 WO_OPCODE_SPACE,      // elt
            egtx = 34 WO_OPCODE_SPACE,      // egt

            ltr = 35 WO_OPCODE_SPACE,       // lt
            gtr = 36 WO_OPCODE_SPACE,       // gt
            eltr = 37 WO_OPCODE_SPACE,      // elt
            egtr = 38 WO_OPCODE_SPACE,      // egt

            equr = 39 WO_OPCODE_SPACE,      // equr(dr)         REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF  
            nequr = 40 WO_OPCODE_SPACE,     // nequr
            equs = 41 WO_OPCODE_SPACE,      // equs
            nequs = 42 WO_OPCODE_SPACE,     // nequs
            equb = 43 WO_OPCODE_SPACE,      // equb(dr)         REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF         3-9 byte
            nequb = 44 WO_OPCODE_SPACE,     // nequb

            jnequb = 45 WO_OPCODE_SPACE,    // jnequb(dr_0)     REGID(1BYTE)/DIFF(4BYTE) PLACE(4BYTE)       6-9 byte

            call = 46 WO_OPCODE_SPACE,      // call(dr_0)       REGID(1BYTE)/DIFF(4BYTE) 
            calln = 47 WO_OPCODE_SPACE,     // calln(0_NATIVE)  VM_IR_DIFF(4BYTE)/NATIVE_FUNC(8BYTE)
            ret = 48 WO_OPCODE_SPACE,       // ret(dr_0)        POP_SIZE(2 BYTE if dr)
            jt = 49 WO_OPCODE_SPACE,        // jt               DIFF(4BYTE)
            jf = 50 WO_OPCODE_SPACE,        // jf               DIFF(4BYTE)
            jmp = 51 WO_OPCODE_SPACE,       // jmp              DIFF(4BYTE)

            mkarr = 52 WO_OPCODE_SPACE,     // mkarr(dr_0)      REGID(1BYTE)/DIFF(4BYTE) SZ(2BYTE)
            mkmap = 53 WO_OPCODE_SPACE,     // mkmap(dr_0)      REGID(1BYTE)/DIFF(4BYTE) SZ(2BYTE)
            mkstruct = 54 WO_OPCODE_SPACE,  // mkstruct(dr_0)   REGID(1BYTE)/DIFF(4BYTE) SZ(2BYTE)          4-7 byte
            mkunion = 55 WO_OPCODE_SPACE,   // mkunion(dr)      REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF ID      5-11 byte
            mkclos = 56 WO_OPCODE_SPACE,    // mkclos(00)       FUNC(8BYTE) CAPTURED_COUNT(2BYTE)           11 byte

            unpackargs = 57 WO_OPCODE_SPACE,// unpackargs(dr)   REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF

            movcast = 58 WO_OPCODE_SPACE,   // movcast(dr)      REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF TYPE    4-10 byte
            typeas = 59 WO_OPCODE_SPACE,    // typeas(dr_0)     REGID(1BYTE)/DIFF(4BYTE) TYPE               3-6 byte
                                            // typeis(dr_1)

            abrt = 60 WO_OPCODE_SPACE,      // abrt(0_1/0)      (0xcc 0xcd can use it to abort)     
                                            // end(1_1/0)                                                   1 byte

            lds = 61 WO_OPCODE_SPACE,       // lds(dr)          REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF   
            sts = 62 WO_OPCODE_SPACE,       // sts(dr)          REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF  

            ext = 63 WO_OPCODE_SPACE,       // ext(PAGECODE)    extern code, it used for extern command of vm,
        };

        enum extern_opcode_page_0 : uint8_t
        {
            // Here to store extern_opcode. 
            // Here is no nop in extern code page.

            // THIS PAGE USED FOR STORING SIMPLE EXTERN OPCODE THAT IS NOT CONTAINED IN ORIGIN OP CODE
            panic = 0 WO_OPCODE_SPACE,      // panic(dr_0)      REGID(1BYTE)/DIFF(4BYTE)
            packargs = 1 WO_OPCODE_SPACE,   // packargs(dr)     REGID(1BYTE)/DIFF(4BYTE) NORMAL_ARGC(4BYTE) CLOSURE_CAPS(2BYTE)
            cdivilr = 2 WO_OPCODE_SPACE,    // cdivlr(dr)       REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF
            cdivil = 3 WO_OPCODE_SPACE,     // cdivl(dr_0)      REGID(1BYTE)/DIFF(4BYTE)
            cdivir = 4 WO_OPCODE_SPACE,     // cdivr(dr_0)      REGID(1BYTE)/DIFF(4BYTE)
            cdivirz = 5 WO_OPCODE_SPACE,    // cdivrz(dr_0)     REGID(1BYTE)/DIFF(4BYTE)      
        };
        enum extern_opcode_page_1 : uint8_t
        {
            // Here to store extern_opcode. 
            // Here is no nop in extern code page.
        };
        enum extern_opcode_page_2 : uint8_t
        {
            // Here to store extern_opcode. 
            // Here is no nop in extern code page.
        };
        enum extern_opcode_page_3 : uint8_t
        {
            // Here to store extern_opcode. 
            // Here is no nop in extern code page.

            // THIS PAGE USED FOR STORING FLAGS AND SHOULD NOT BE EXECUTE

            // funcbegin & end flag, used for notify jit to generate functions.
            funcbegin = 0 WO_OPCODE_SPACE,  // funcbegin(--)
            funcend = 1 WO_OPCODE_SPACE,    // funcend(--)

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