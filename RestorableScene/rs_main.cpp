#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <set>


#include "rs_assert.hpp"
#include "rs_meta.hpp"
#include "rs_gc.hpp"
#include "rs_instruct.hpp"
#include "rs_basic_type.hpp"
#include "rs_ir_compiler.hpp"
#include "rs_vm.hpp"


namespace rs
{

} //namespace rs;

int main()
{
    using namespace rs;
    using namespace rs::opnum;

    ir_compiler c;                                  // fast stack addressing:    
    //c.pshr(opnum::imm(2333.0456));                        //      psh 2333;
    //c.movr2i(reg(reg::cr), reg(reg::bp_offset(0)));    //      mov cr,   [bp+0]
    //c.end();                                       //      end

    c.nop();
    c.set(reg(reg::t0), opnum::imm(0));         //      set     t0,  0              int  i=0
    c.tag("loop_begin");                        //  :loop_begin
    c.lti(reg(reg::t0), opnum::imm(100000000)); //      lti     t0,   100000000     while i < 100000000
    c.jf(tag("loop_end"));                      //      jf      loop_end            {
    c.addi(reg(reg::t0), opnum::imm(1));        //      addi    t0,  1                  i+=1;
    c.jmp(tag("loop_begin"));                   //      jmp     loop_begin          }
    c.tag("loop_end");                          //  :loop_end
    c.end();                                    //      end;

    auto env = c.finalize();

    vm vmm;
    vmm.set_runtime(env);

    auto beg = clock();
    vmm.run();
    auto end = clock();

    std::cout << (end - beg) << std::endl;
}