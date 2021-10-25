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

int main()
{
    using namespace rs;
    using namespace rs::opnum;

    vm vmm;

    ///////////////////////////////////////////////////////////////////////////////////////

    ir_compiler c6;                                     // 
    c6.land(imm(1), imm(1));
    c6.lor(imm(0), reg(reg::cr));
    c6.lnot(reg(reg::cr));
    c6.end();                                          //      end

    vmm.set_runtime(c6.finalize());
    vmm.run();

    rs_test(vmm.cr->type == value::valuetype::integer_type && vmm.cr->integer == 0);

    ///////////////////////////////////////////////////////////////////////////////////////

    ir_compiler c5;                                     // 
    c5.psh(imm("Helloworld"));
    c5.psh(imm(125.56));
    c5.psh(imm(2133));
    c5.lds(reg(reg::cr), imm(0));
    c5.end();                                           //      end

    vmm.set_runtime(c5.finalize());
    vmm.run();

    rs_test(vmm.cr->type == value::valuetype::string_type && *vmm.cr->string == "Helloworld");

    ///////////////////////////////////////////////////////////////////////////////////////

    ir_compiler c4;                                     // 
    c4.psh(imm(1));
    c4.psh(imm(2));
    c4.psh(imm(3));
    c4.psh(imm(4));
    c4.pop(2);
    c4.pop(reg(reg::cr));
    c4.end();                                           //      end

    vmm.set_runtime(c4.finalize());
    vmm.run();

    rs_test(vmm.cr->type == value::valuetype::integer_type && vmm.cr->integer == 2);

    ///////////////////////////////////////////////////////////////////////////////////////

    ir_compiler c3;                                     // 
    c3.mov(reg(reg::cr), imm("hello,"));                //      mov     cr,   "hello,";
    c3.adds(reg(reg::cr), imm("world!"));               //      adds    cr,   "world!";
    c3.end();                                           //      end

    vmm.set_runtime(c3.finalize());
    vmm.run();

    rs_test(vmm.cr->type == value::valuetype::string_type && *vmm.cr->string == "hello,world!");
    ///////////////////////////////////////////////////////////////////////////////////////

    ir_compiler c2;                                     // fast stack addressing:    
    c2.pshr(imm(2333));                                 //      psh     2333;
    c2.mov(reg(reg::cr), reg(reg::bp_offset(0)));       //      mov     cr,   [bp+0]
    c2.end();                                           //      end

    vmm.set_runtime(c2.finalize());
    vmm.run();

    rs_test(vmm.cr->type == value::valuetype::integer_type && vmm.cr->integer == 2333);
    ///////////////////////////////////////////////////////////////////////////////////////

    ir_compiler c0;                                      // fast stack addressing:    
    c0.pshr(imm(2333.0456));                             //      pshr   2333.0456;
    c0.movr2i(reg(reg::cr), reg(reg::bp_offset(0)));     //      movr2i cr,   [bp+0]
    c0.end();                                            //      end

    vmm.set_runtime(c0.finalize());
    vmm.run();

    rs_test(vmm.cr->type == value::valuetype::integer_type && vmm.cr->integer == 2333);
    ///////////////////////////////////////////////////////////////////////////////////////

    ir_compiler c1;
    c1.psh(imm(0));                                     //      psh     0
    c1.set(reg(reg::bp_offset(0)), imm(0));             //      set     [bp+0],  0              int  i=0
    c1.tag("loop_begin");                               //  :loop_begin
    c1.lti(reg(reg::bp_offset(0)), imm(100000000));     //      lti     [bp+0],   100000000     while i < 100000000
    c1.jf(tag("loop_end"));                             //      jf      loop_end                {
    c1.addi(reg(reg::bp_offset(0)), imm(1));            //      addi    [bp+0],  1                  i+=1;
    c1.jmp(tag("loop_begin"));                          //      jmp     loop_begin              }
    c1.tag("loop_end");                                 //  :loop_end
    c1.pop(1);                                     //      pop     1
    c1.end();                                           //      end;


    while (true)
    {
        vmm.set_runtime(c1.finalize());

        auto beg = clock();
        vmm.run();
        auto end = clock();

        std::cout << (end - beg) << std::endl;
    }

    ///////////////////////////////////////////////////////////////////////////////////////

}