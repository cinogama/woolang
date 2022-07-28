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
            nop = 0 WO_OPCODE_SPACE,    // nop(TKPLS)                                                        1 byte

            mov = 1 WO_OPCODE_SPACE,    // mov(dr)            REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF         3-9 byte
            set = 2 WO_OPCODE_SPACE,    // set(dr)            REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF         3-9 byte

            addi = 3 WO_OPCODE_SPACE,    // add(dr)        REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF         3-9 byte
            subi = 4 WO_OPCODE_SPACE,    // sub
            muli = 5 WO_OPCODE_SPACE,    // mul
            divi = 6 WO_OPCODE_SPACE,    // div
            modi = 7 WO_OPCODE_SPACE,    // mod

            addr = 8 WO_OPCODE_SPACE,    // add(dr)        REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF         3-9 byte
            subr = 9 WO_OPCODE_SPACE,    // sub
            mulr = 10 WO_OPCODE_SPACE,    // mul
            divr = 11 WO_OPCODE_SPACE,    // div
            modr = 12 WO_OPCODE_SPACE,    // mod

            addh = 13 WO_OPCODE_SPACE,    // add(dr)        REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF         3-9 byte
            subh = 14 WO_OPCODE_SPACE,    // sub

            adds = 15 WO_OPCODE_SPACE,    // add(dr)        REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF         3-9 byte

            psh = 16 WO_OPCODE_SPACE,    // psh(dr_0)            REGID(1BYTE)/DIFF(4BYTE)                    2-5 byte
            pop = 17 WO_OPCODE_SPACE,   // pop(dr_STORED?)   REGID(1BYTE)/DIFF(4BYTE)/COUNT(2BYTE)       2-5 byte
            pshr = 18 WO_OPCODE_SPACE,  // pshr(dr_0)           REGID(1BYTE)/DIFF(4BYTE)                    2-5 byte
            popr = 19 WO_OPCODE_SPACE,  // popr(dr_0)           REGID(1BYTE)/DIFF(4BYTE)                    2-5 byte

            lds = 20 WO_OPCODE_SPACE,   // lds(dr)            REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF   
            ldsr = 21 WO_OPCODE_SPACE,  // ldsr(dr)           REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF  

            //  Logic operator, the result will store to logic_state
            equb = 22 WO_OPCODE_SPACE,   // equb(dr)            REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF         3-9 byte
            nequb = 23 WO_OPCODE_SPACE,  // nequb

            lti = 24 WO_OPCODE_SPACE,    // lt
            gti = 25 WO_OPCODE_SPACE,    // gt
            elti = 26 WO_OPCODE_SPACE,   // elt
            egti = 27 WO_OPCODE_SPACE,   // egt

            land = 28 WO_OPCODE_SPACE,  // land             
            lor = 29 WO_OPCODE_SPACE,   // lor
            lmov = 30 WO_OPCODE_SPACE,   // lmov(dr)            REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF

            ltx = 31 WO_OPCODE_SPACE,    // lt
            gtx = 32 WO_OPCODE_SPACE,    // gt
            eltx = 33 WO_OPCODE_SPACE,   // elt
            egtx = 34 WO_OPCODE_SPACE,   // egt

            ltr = 35 WO_OPCODE_SPACE,    // lt
            gtr = 36 WO_OPCODE_SPACE,    // gt
            eltr = 37 WO_OPCODE_SPACE,   // elt
            egtr = 38 WO_OPCODE_SPACE,   // egt

            call = 39 WO_OPCODE_SPACE,  // call(dr_0)  REGID(1BYTE)/DIFF(4BYTE) 
            calln = 40 WO_OPCODE_SPACE,  // calln(0_ISNATIVE)  VM_IR_DIFF(4BYTE)/NATIVE_FUNC(8BYTE)
            ret = 41 WO_OPCODE_SPACE,   // ret(dr_0)        POP_SIZE(2 BYTE if dr)
            jt = 42 WO_OPCODE_SPACE,    // jt               DIFF(4BYTE)
            jf = 43 WO_OPCODE_SPACE,    // jf               DIFF(4BYTE)
            jmp = 44 WO_OPCODE_SPACE,   // jmp              DIFF(4BYTE)

            movcast = 45 WO_OPCODE_SPACE,   // movcast(dr)       REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF TYPE  4-10 byte
            setcast = 46 WO_OPCODE_SPACE,   // setcast(dr)       REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF TYPE  4-10 byte
            mkclos = 47 WO_OPCODE_SPACE,      //mkclos(00) CAPTURE_ARG_COUNT(2BYTE) REAL_RSFUNC(4BYTE)

            typeas = 48 WO_OPCODE_SPACE,    // typeas(dr_0)      REGID(1BYTE)/DIFF(4BYTE) TYPE             3-6 byte
                                            //  typeis(dr_1)

            mkstruct = 49 WO_OPCODE_SPACE,   // mkstruct(dr_0) REGID(1BYTE)/DIFF(4BYTE) SZ(2BYTE)               4-7 byte

            ext = 50 WO_OPCODE_SPACE,       // ext(PAGECODE)     extern code, it used for extern command of vm,

            abrt = 51 WO_OPCODE_SPACE,  // abrt(0_1/0)  (0xcc 0xcd can use it to abort)     
                                        // end(1_1/0)   1 byte

            idarr = 52 WO_OPCODE_SPACE,  // idarr(dr_dr)       REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF [Used for string array mapping]
            idmap = 53 WO_OPCODE_SPACE,  // idmap(dr_dr)       REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF [Used for string array mapping]

            mkarr = 54 WO_OPCODE_SPACE,     // mkarr(dr_0)      REGID(1BYTE)/DIFF(4BYTE)
            mkmap = 55 WO_OPCODE_SPACE,     // mkmap(dr_0)      REGID(1BYTE)/DIFF(4BYTE)
            idstr = 56 WO_OPCODE_SPACE,     // idstr(dr_dr)       REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF [Used for string array mapping]

            equr = 57 WO_OPCODE_SPACE,      // equr(dr)         REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF  
            nequr = 58 WO_OPCODE_SPACE,     // nequr
            equs = 59 WO_OPCODE_SPACE,      // equs
            nequs = 60 WO_OPCODE_SPACE,     // nequs
            RESERVED_0 = 61 WO_OPCODE_SPACE,      // 

            jnequb = 62 WO_OPCODE_SPACE,    //  jnequb(dr_0) REGID(1BYTE)/DIFF(4BYTE) PLACE(4BYTE)            6-9 byte
            idstruct = 63 WO_OPCODE_SPACE,     // idstruct(dr_0) REGID(1BYTE)/DIFF(4BYTE) OFFSET(2BYTE)   4-7 byte

        };

        enum extern_opcode_page_0 : uint8_t
        {
            // Here to store extern_opcode. 
            // Here is no nop in extern code page.

            // THIS PAGE USED FOR STORING SIMPLE EXTERN OPCODE THAT IS NOT CONTAINED IN ORIGIN OP CODE

            setref = 0 WO_OPCODE_SPACE,     // ext(00) setref(dr) REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF
            trans = 1 WO_OPCODE_SPACE,   // ext(00) settrans(dr) REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF
            // mknilmap = 2 WO_OPCODE_SPACE,   // ext(00) mknilmap(dr_0) REGID(1BYTE)/DIFF(4BYTE)
            packargs = 3 WO_OPCODE_SPACE,   // ext(00) packargs(dr) REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF
            unpackargs = 4 WO_OPCODE_SPACE, // ext(00) packargs(dr) REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF
            movdup = 5 WO_OPCODE_SPACE,     // ext(00) movdup(dr) REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF
            // mkclos = 6 WO_OPCODE_SPACE,     // ext(00) mkclos(00) CAPTURE_ARG_COUNT(2BYTE) REAL_RSFUNC(4BYTE)

            veh = 7 WO_OPCODE_SPACE,    // excep(RAISE?_ROLLBACK?) 
                                        //  10 begin ? DIFF(4BYTE):ROLLBACK ? 0BYTE : DIFF(4BYTE)
                                        //  01 thorw
                                        //  00 clean
            mkunion = 8 WO_OPCODE_SPACE   // mkunion(dr_0) REGID(1BYTE)/DIFF(4BYTE) id(2BYTE)

        };
        enum extern_opcode_page_1 : uint8_t
        {
            // Here to store extern_opcode. 
            // Here is no nop in extern code page.

            // THIS PAGE USED FOR STORING DEBUG OPCODE
            endjit = 0 WO_OPCODE_SPACE,     // ext(01) endjit(--)  notify jit compiler stop work.
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