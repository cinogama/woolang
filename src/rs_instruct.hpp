#pragma once

#include "rs_assert.hpp"
#include "rs_basic_type.hpp"

#include <cstdint>

namespace rs
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
#define RS_OPCODE_SPACE <<2
            nop = 0 RS_OPCODE_SPACE,    // nop(TKPLS)                                                        1 byte

            mov = 1 RS_OPCODE_SPACE,    // mov(dr)            REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF         3-9 byte
            set = 2 RS_OPCODE_SPACE,    // set(dr)            REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF         3-9 byte

            addi = 3 RS_OPCODE_SPACE,    // add(dr)        REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF         3-9 byte
            subi = 4 RS_OPCODE_SPACE,    // sub
            muli = 5 RS_OPCODE_SPACE,    // mul
            divi = 6 RS_OPCODE_SPACE,    // div
            modi = 7 RS_OPCODE_SPACE,    // mod

            addr = 8 RS_OPCODE_SPACE,    // add(dr)        REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF         3-9 byte
            subr = 9 RS_OPCODE_SPACE,    // sub
            mulr = 10 RS_OPCODE_SPACE,    // mul
            divr = 11 RS_OPCODE_SPACE,    // div
            modr = 12 RS_OPCODE_SPACE,    // mod

            addh = 13 RS_OPCODE_SPACE,    // add(dr)        REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF         3-9 byte
            subh = 14 RS_OPCODE_SPACE,    // sub

            adds = 15 RS_OPCODE_SPACE,    // add(dr)        REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF         3-9 byte

            psh = 16 RS_OPCODE_SPACE,    // psh(dr_0)            REGID(1BYTE)/DIFF(4BYTE)                    2-5 byte
            pop = 17 RS_OPCODE_SPACE,   // pop(dr_STORED?)   REGID(1BYTE)/DIFF(4BYTE)/COUNT(2BYTE)       2-5 byte
            pshr = 18 RS_OPCODE_SPACE,  // pshr(dr_0)           REGID(1BYTE)/DIFF(4BYTE)                    2-5 byte
            popr = 19 RS_OPCODE_SPACE,  // popr(dr_0)           REGID(1BYTE)/DIFF(4BYTE)                    2-5 byte

            lds = 20 RS_OPCODE_SPACE,   // lds(dr)            REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF   
            ldsr = 21 RS_OPCODE_SPACE,  // ldsr(dr)           REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF  

            //  Logic operator, the result will store to logic_state
            equb = 22 RS_OPCODE_SPACE,   // equb(dr)            REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF         3-9 byte
            nequb = 23 RS_OPCODE_SPACE,  // nequb

            lti = 24 RS_OPCODE_SPACE,    // lt
            gti = 25 RS_OPCODE_SPACE,    // gt
            elti = 26 RS_OPCODE_SPACE,   // elt
            egti = 27 RS_OPCODE_SPACE,   // egt

            land = 28 RS_OPCODE_SPACE,  // land             
            lor = 29 RS_OPCODE_SPACE,   // lor
            lnot = 30 RS_OPCODE_SPACE,   // lnot(dr_0)                REGID(1BYTE)/DIFF(4BYTE)

            ltx = 31 RS_OPCODE_SPACE,    // lt
            gtx = 32 RS_OPCODE_SPACE,    // gt
            eltx = 33 RS_OPCODE_SPACE,   // elt
            egtx = 34 RS_OPCODE_SPACE,   // egt

            ltr = 35 RS_OPCODE_SPACE,    // lt
            gtr = 36 RS_OPCODE_SPACE,    // gt
            eltr = 37 RS_OPCODE_SPACE,   // elt
            egtr = 38 RS_OPCODE_SPACE,   // egt

            call = 39 RS_OPCODE_SPACE,  // call(dr_0)  REGID(1BYTE)/DIFF(4BYTE) 
            calln = 40 RS_OPCODE_SPACE,  // calln(0_ISNATIVE)  VM_IR_DIFF(4BYTE)/NATIVE_FUNC(8BYTE)
            ret = 41 RS_OPCODE_SPACE,   // ret
            jt = 42 RS_OPCODE_SPACE,    // jt               DIFF(4BYTE)
            jf = 43 RS_OPCODE_SPACE,    // jf               DIFF(4BYTE)
            jmp = 44 RS_OPCODE_SPACE,   // jmp              DIFF(4BYTE)

            movcast = 45 RS_OPCODE_SPACE,   // movcast(dr)       REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF TYPE  4-10 byte
            setcast = 46 RS_OPCODE_SPACE,   // setcast(dr)       REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF TYPE  4-10 byte
            movx = 47 RS_OPCODE_SPACE,      // movx(dr)          REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF

            typeas = 48 RS_OPCODE_SPACE,    // typeas(dr_0)      REGID(1BYTE)/DIFF(4BYTE) TYPE             3-6 byte
                                            //  typeis(dr_1)
            // exception handler
            veh = 49 RS_OPCODE_SPACE,   // excep(RAISE?_ROLLBACK?) 
                                        //  10 begin ? DIFF(4BYTE):ROLLBACK ? 0BYTE : DIFF(4BYTE)
                                        //  01 thorw
                                        //  00 clean

            ext = 50 RS_OPCODE_SPACE,       // ext(PAGECODE)     extern code, it used for extern command of vm,

            abrt = 51 RS_OPCODE_SPACE,  // abrt(0_1/0)  (0xcc 0xcd can use it to abort)     
                                        // end(1_1/0)   1 byte

            equx = 52 RS_OPCODE_SPACE,   // equx(dr)            REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF         3-9 byte
            nequx = 53 RS_OPCODE_SPACE,  // nequx

            mkarr = 54 RS_OPCODE_SPACE,     // mkarr(dr_0)      REGID(1BYTE)/DIFF(4BYTE)
            mkmap = 55 RS_OPCODE_SPACE,     // mkmap(dr_0)      REGID(1BYTE)/DIFF(4BYTE)
            idx = 56 RS_OPCODE_SPACE,       // idx(dr_dr)       REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF [Used for string array mapping]

            addx = 57 RS_OPCODE_SPACE,      // addx(dr)         CHANGE_TYPE(1B) REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF  
            subx = 58 RS_OPCODE_SPACE,      // subx
            mulx = 59 RS_OPCODE_SPACE,      // mulx
            divx = 60 RS_OPCODE_SPACE,      // divx
            modx = 61 RS_OPCODE_SPACE,      // modx

            calljit = 62 RS_OPCODE_SPACE,    //  calljit(00) JIT_STATE(1B) NATIVE_ADDRESS(8B)       10 byte
                                            //               NONE       0
                                            //               GENERATING 1
                                            //               FAIL       2
                                            //               READY      3
            RESERVED_1 = 63 RS_OPCODE_SPACE,   //                                     

        };

        enum extern_opcode_page_0 : uint8_t
        {
            // Here to store extern_opcode. 
            // Here is no nop in extern code page.

            // THIS PAGE USED FOR STORING SIMPLE EXTERN OPCODE THAT IS NOT CONTAINED IN ORIGIN OP CODE

            setref = 0 RS_OPCODE_SPACE,     // ext(00) setref(dr) REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF
            // mknilarr = 1 RS_OPCODE_SPACE,   // ext(00) mknilarr(dr_0) REGID(1BYTE)/DIFF(4BYTE)
            // mknilmap = 2 RS_OPCODE_SPACE,   // ext(00) mknilmap(dr_0) REGID(1BYTE)/DIFF(4BYTE)
            packargs = 3 RS_OPCODE_SPACE,   // ext(00) packargs(dr) REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF
            unpackargs = 4 RS_OPCODE_SPACE, // ext(00) packargs(dr) REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF
            movdup = 5 RS_OPCODE_SPACE,     // ext(00) movdup(dr) REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF

        };
        enum extern_opcode_page_1 : uint8_t
        {
            // Here to store extern_opcode. 
            // Here is no nop in extern code page.

            // THIS PAGE USED FOR STORING DEBUG OPCODE
            endjit = 0 RS_OPCODE_SPACE,     // ext(01) endjit(--)  notify jit compiler stop work.
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

#undef RS_OPCODE_SPACE
        };

        opcode opcode_dr; rs_static_assert_size(opcode, 1);

        inline constexpr instruct(opcode _opcode, uint8_t _dr)
            : opcode_dr(opcode(_opcode | _dr))
        {
            rs_assert((_opcode & 0b00000011) == 0, "illegal value for '_opcode': it's low 2-bit should be 0.");
            rs_assert((_dr & 0b11111100) == 0, "illegal value for '_dr': it should be less then 0x04.");
        }

    };
    rs_static_assert_size(instruct, 1);
}