#pragma once

#include <cstdint>

#include "rs_assert.hpp"

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
        nop = 1 RS_OPCODE_SPACE,    // nop()                                                        1 byte
        mov = 2 RS_OPCODE_SPACE,    // mov(dr)            REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF         3-9 byte
        set = 3 RS_OPCODE_SPACE,    // set(dr)            REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF         3-9 byte

        addi = 4 RS_OPCODE_SPACE,    // add(dr)        REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF         3-9 byte
        subi = 5 RS_OPCODE_SPACE,    // sub
        muli = 6 RS_OPCODE_SPACE,    // mul
        divi = 7 RS_OPCODE_SPACE,    // div
        modi = 8 RS_OPCODE_SPACE,    // mod

        addr = 9 RS_OPCODE_SPACE,    // add(dr)        REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF         3-9 byte
        subr = 10 RS_OPCODE_SPACE,    // sub
        mulr = 11 RS_OPCODE_SPACE,    // mul
        divr = 12 RS_OPCODE_SPACE,    // div
        modr = 13 RS_OPCODE_SPACE,    // mod

        addh = 14 RS_OPCODE_SPACE,    // add(dr)        REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF         3-9 byte
        subh = 15 RS_OPCODE_SPACE,    // sub

        adds = 16 RS_OPCODE_SPACE,    // add(dr)        REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF         3-9 byte

        psh = 17 RS_OPCODE_SPACE,    // psh(dr_0)            REGID(1BYTE)/DIFF(4BYTE)                    2-5 byte
        pop = 18 RS_OPCODE_SPACE,   // pop(dr_STORED?)   REGID(1BYTE)/DIFF(4BYTE)/COUNT(2BYTE)       3-5 byte
        pshr = 19 RS_OPCODE_SPACE,  // pshr(dr_0)           REGID(1BYTE)/DIFF(4BYTE)                    2-5 byte
        popr = 20 RS_OPCODE_SPACE,  // popr(dr_0)           REGID(1BYTE)/DIFF(4BYTE)                    2-5 byte

        lds = 21 RS_OPCODE_SPACE,   // lds(dr)            REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF   
        ldsr = 22 RS_OPCODE_SPACE,  // ldsr(dr)           REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF  

        //  Logic operator, the result will store to logic_state
        equ = 23 RS_OPCODE_SPACE,   // equ(dr)            REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF         3-9 byte
        nequ = 24 RS_OPCODE_SPACE,  // nequ

        lti = 25 RS_OPCODE_SPACE,    // lt
        gti = 26 RS_OPCODE_SPACE,    // gt
        elti = 27 RS_OPCODE_SPACE,   // elt
        egti = 28 RS_OPCODE_SPACE,   // egt
        land = 29 RS_OPCODE_SPACE,  // land             
        lor = 30 RS_OPCODE_SPACE,   // lor
        lnot = 31 RS_OPCODE_SPACE,   // lnot(dr_0)                REGID(1BYTE)/DIFF(4BYTE)

        lth = 32 RS_OPCODE_SPACE,    // lt
        gth = 33 RS_OPCODE_SPACE,    // gt
        elth = 34 RS_OPCODE_SPACE,   // elt
        egth = 35 RS_OPCODE_SPACE,   // egt

        ltr = 36 RS_OPCODE_SPACE,    // lt
        gtr = 37 RS_OPCODE_SPACE,    // gt
        eltr = 38 RS_OPCODE_SPACE,   // elt
        egtr = 39 RS_OPCODE_SPACE,   // egt

        call = 40 RS_OPCODE_SPACE,  // call(dr_0)  REGID(1BYTE)/DIFF(4BYTE) 
        calln = 41 RS_OPCODE_SPACE,  // calln(0_ISNATIVE)  VM_IR_DIFF(4BYTE)/NATIVE_FUNC(8BYTE)
        ret = 42 RS_OPCODE_SPACE,   // ret
        jt = 43 RS_OPCODE_SPACE,    // jt               DIFF(4BYTE)
        jf = 44 RS_OPCODE_SPACE,    // jf               DIFF(4BYTE)
        jmp = 45 RS_OPCODE_SPACE,   // jmp              DIFF(4BYTE)

        movr2i = 46 RS_OPCODE_SPACE,
        movi2r = 47 RS_OPCODE_SPACE,
        setr2i = 48 RS_OPCODE_SPACE,
        seti2r = 49 RS_OPCODE_SPACE,

        // exception handler
          = 50 RS_OPCODE_SPACE, // excep(RAISE?_ROLLBACK?) RAISE? DIFF(4BYTE):ROLLBACK ? 0BYTE : DIFF(4BYTE)

        abrt = 51 RS_OPCODE_SPACE,  // abrt()  (0xcc 0xcd can use it to abort)     

        equs = 52 RS_OPCODE_SPACE,   // equs(dr)            REGID(1BYTE)/DIFF(4BYTE) REGID/DIFF         3-9 byte
        nequs = 53 RS_OPCODE_SPACE,  // nequs

        end = 63 RS_OPCODE_SPACE,   // end()                                        1 byte
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