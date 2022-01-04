#pragma once
// Here to place some global variable for config..

namespace rs
{
    namespace config
    {
        /*
        * ENABLE_IR_CODE_ACTIVE_ALLIGN = false
        * --------------------------------------------------------------------
        *   Virtual-machine may read 2byte, 4byte or 8byte data from ir code.
        * In some platform, reading multi-byte data from the address not match
        * the allign, the allign-fault will happend. 
        * --------------------------------------------------------------------
        *   if ENABLE_IR_CODE_ACTIVE_ALLIGN is true, ir generator will fill the 
        * ir command buffer with 'nop(sz)', and make sure the 2byte/4byte/8byte
        * data will be allign correctly.
        * --------------------------------------------------------------------
        * ATTENTION:
        *   nop(sz) will caused performance loss, so if this options work, vm's
        * proformance may be lost by 30%-50%
        * --------------------------------------------------------------------
        * 
        * I hate allign.
        *                   ---- mr. cino. 2022.1.4.
        */
        inline bool ENABLE_IR_CODE_ACTIVE_ALLIGN = false;

    }
}