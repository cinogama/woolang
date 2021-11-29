#define _CRT_SECURE_NO_WARNINGS

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
#include "rs_compiler_ir.hpp"
#include "rs_vm.hpp"

void example(rs::vmbase* vm)
{
    // printf("what the hell!\n");
    using namespace std;

    // std::this_thread::sleep_for(1s);

    vm->cr->integer = 9926;
    vm->cr->type = rs::value::valuetype::integer_type;
}

void veh_exception_test(rs::vmbase* vm)
{
    rs_fail(0, "veh_exception_test.");
}

void cost_time_test_gc(rs::vmbase* vm)
{
    using namespace std;
    printf("vvvvvv GC SHOULD WORK vvvvvv\n");
    std::this_thread::sleep_for(25s);
    printf("^^^^^^ IN THIS RANGE ^^^^^^^\n");
}


#define RS_IMPL
#include "rs.h"

#if defined(_WIN32)
#include <Windows.h>
#endif

#include "rs_lang_grammar_loader.hpp"
#include "rs_lang.hpp"
#include "rs_env_locale.hpp"

int main()
{
    using namespace rs;
    using namespace rs::opnum;

    rs::rs_init_locale();

    std::cout << ANSI_RST;
    std::cout << "RestorableScene ver." << rs_version() << " " << std::endl;
    std::cout << rs_compile_date() << std::endl;

    gc::gc_start();

    std::wstring src_code = LR"(

func fib(var x:int)
{
    if (x < 2)
        return 1;
    return fib(x-1) + fib(x-2);
}

func fib(var x:map)
{
   return 0:real;
}
func fib(var x:array)
{
   return x;
}

func fib(var d)
{
    return d;
}

var m = {
            {"age", 1},
            {"name", "rscene"},
            {"uid", 0x1234567890abcdefh}, 
            {"mapping", m}, 
        };

var a1 = [1, 2, 3, 4, 5, m];

var m0 = fib(m:map);
var m1 = fib(a1);
var m2 = fib(0);

var main = func(var arg:int){

    var a = 0, b = 0, c = 0;

    (a = b = c = "15":int):real;
    {
        var a = "Helloworld";
        return a;
    }
};

)";
    std::chrono::system_clock sc;

    rs::lexer lx1(
        src_code
    );

    auto* rs_grammar = get_rs_grammar();

    //rs_grammar->check(lx1);
    //lx1.reset();
    auto beg = sc.now();
    auto result = rs_grammar->gen(lx1);
    auto end = sc.now();

    std::cout << src_code.size() << "byte  " << (end - beg).count() / 10000000.0f << std::endl;

    if (result)
    {
        std::wcout << "AST_BUILD: " << std::endl;
        result->display();
    }

    rs::lang lng(lx1);
    lng.analyze_pass1(result);

    if (result)
    {
        std::wcout << "PASS 1: " << std::endl;
        //result->display();
    }

    lng.analyze_pass2(result);

    if (result)
    {
        std::wcout << "PASS 2: " << std::endl;
        result->display();
    }

    for (auto exp : lx1.lex_error_list)
    {
        std::wcout << exp.to_wstring() << std::endl;
    }

    for (auto exp : lx1.lex_warn_list)
    {
        std::wcout << exp.to_wstring() << std::endl;
    }

    grammar::ast_base::clean_this_thread_ast();

    return 0;

    vm vmm;
    ///////////////////////////////////////////////////////////////////////////////////////

    ir_compiler c17;
    c17.psh(imm("name"));                   //  psh     "name"
    c17.psh(imm("joy"));                    //  psh     "joy"
    c17.psh(imm("age"));                    //  psh     "age"
    c17.psh(imm("19"));                     //  psh     "19"
    c17.mkmap(reg(reg::t0), imm(2));        //  mkmap   t0,     2
    c17.idx(reg(reg::t0), imm("name"));     //  idx     t0,     "name"
    c17.mov(reg(reg::t1), reg(reg::cr));    //  mov     t1,     cr
    c17.idx(reg(reg::t0), imm("age"));      //  idx     t0,     "age"
    c17.addx(reg(reg::t1), reg(reg::cr));   //  addx    t1,     cr
    c17.idx(reg(reg::t0), imm("my_self"));  //  idx     t0,     "my_self"
    c17.mov(reg(reg::cr), reg(reg::t0));    //  mov     cr,     t0
    c17.psh(imm(1));
    c17.psh(imm(2));
    c17.psh(imm(3));
    c17.psh(imm(4));
    c17.psh(imm(5));
    c17.psh(imm(6));
    c17.mkarr(reg(reg::t2), imm(6));                //  mkarr   t2,     6
    c17.idx(reg(reg::t0), imm("my_array"));         //  idx     t0,     "my_array"
    c17.mov(reg(reg::cr), reg(reg::t2));            //  mov     cr,     t0
    c17.ext_setref(reg(reg::t3), reg(reg::t0));     //  ext setref     t3,     t0
    c17.set(reg(reg::cr), reg(reg::t1));            //  set     cr,     t1
    c17.end();
    vmm.set_runtime(c17);

    auto* v = (vm*)vmm.make_machine();
    v->run();

    std::cout << ANSI_HIR << rs_cast_string((rs_value)v->register_mem_begin + reg::t3) << ANSI_RST << std::endl;
    std::cout << ANSI_HIC << rs_cast_string((rs_value)v->register_mem_begin + reg::t0) << ANSI_RST << std::endl;

    rs_test(v->bp == v->sp);
    rs_test(v->cr->type == value::valuetype::string_type && *v->cr->string == "joy19");

    delete v;

    vmm.run();
    std::cout << ANSI_HIR << rs_cast_string((rs_value)vmm.register_mem_begin + reg::t3) << ANSI_RST << std::endl;
    std::cout << ANSI_HIC << rs_cast_string((rs_value)vmm.register_mem_begin + reg::t0) << ANSI_RST << std::endl;

    rs_test(vmm.bp == vmm.sp);
    rs_test(vmm.cr->type == value::valuetype::string_type && *vmm.cr->string == "joy19");

    ///////////////////////////////////////////////////////////////////////////////////////

    ir_compiler c16;
    c16.psh(imm("friend"));
    c16.psh(imm("my"));
    c16.psh(imm("world"));
    c16.psh(imm("hello"));
    c16.mkarr(reg(reg::t0), imm(4));
    c16.idx(reg(reg::t0), imm(0));
    c16.mov(reg(reg::t1), reg(reg::cr));
    c16.idx(reg(reg::t0), imm(1));
    c16.addx(reg(reg::t1), reg(reg::cr));
    c16.idx(reg(reg::t0), imm(2));
    c16.addx(reg(reg::t1), reg(reg::cr));
    c16.idx(reg(reg::t0), imm(3));
    c16.addx(reg(reg::t1), reg(reg::cr));
    c16.set(reg(reg::cr), reg(reg::cr));
    c16.end();

    vmm.set_runtime(c16);
    vmm.run();

    ///////////////////////////////////////////////////////////////////////////////////////

    ir_compiler c15;
    c15.psh(imm("friend"));
    c15.psh(imm("my"));
    c15.psh(imm("world"));
    c15.psh(imm("hello"));
    c15.mkarr(reg(reg::t0), imm(4));
    c15.idx(reg(reg::t0), imm(0));
    c15.mov(reg(reg::t1), reg(reg::cr));
    c15.idx(reg(reg::t0), imm(1));
    c15.addx(reg(reg::t1), reg(reg::cr));
    c15.idx(reg(reg::t0), imm(2));
    c15.addx(reg(reg::t1), reg(reg::cr));
    c15.idx(reg(reg::t0), imm(3));
    c15.addx(reg(reg::t1), reg(reg::cr));
    c15.set(reg(reg::cr), reg(reg::cr));
    c15.end();

    vmm.set_runtime(c15);
    vmm.run();

    rs_test(vmm.bp == vmm.env->stack_begin);
    rs_test(vmm.sp == vmm.env->stack_begin);
    rs_test(vmm.cr->type == value::valuetype::string_type && *vmm.cr->string == "helloworldmyfriend");

    ///////////////////////////////////////////////////////////////////////////////////////

    ir_compiler c14;                                // 
    c14.mov(reg(reg::t0), imm(0));
    c14.tag("loop_begin");                          //      jmp     program_begin
    c14.lti(reg(reg::t0), imm(100000000));
    c14.jf(tag("loop_end"));
    c14.addi(reg(reg::t0), imm(1));
    c14.jmp(tag("loop_begin"));
    c14.tag("loop_end");
    c14.end();                                      //      end


    for (int i = 0; i < 5; i++)
    {

        vmm.set_runtime(c14);

        //auto beg = clock();
        auto beg = sc.now();

        vmm.run();

        auto end = sc.now();
        //auto end = clock();

        std::cout << (end - beg).count() / 10000000.0f << std::endl;
    }

    ///////////////////////////////////////////////////////////////////////////////////////

    ir_compiler c13;                                // 
    c13.tag("program_begin");                       //  program_begin:
    c13.call(&example);                             //      call    example
    c13.mov(reg(reg::t0), imm("hello"));            //      mov     t0, "hello"
    c13.adds(reg(reg::t0), imm(",world"));          //      adds    t0, ",world"
    c13.set(reg(reg::t1), reg(reg::t0));
    c13.set(reg(reg::t0), imm(0xCCCCCCCC));
    c13.set(reg(reg::t0), reg(reg::t1));
    c13.set(reg(reg::t1), imm(0xCCCCCCCC));
    c13.set(reg(reg::t0), imm(0xCCCCCCCC));
    // c13.jmp(tag("program_begin"));                  //      jmp     program_begin
    c13.end();                                      //      end

    vmm.set_runtime(c13);

    vmm.run();
    ///////////////////////////////////////////////////////////////////////////////////////


    ir_compiler c12;                                // 
    c12.call(&cost_time_test_gc);                   //      call    cost_time_test_gc
    c12.end();                                      //      end

    vmm.set_runtime(c12);

    auto begin_gc_count = gc::gc_work_round_count();
    vmm.run();
    auto end_gc_count = gc::gc_work_round_count();

    rs_test(vmm.bp == vmm.env->stack_begin);
    rs_test(vmm.sp == vmm.env->stack_begin);
    rs_test(end_gc_count - begin_gc_count >= 2);

    ///////////////////////////////////////////////////////////////////////////////////////

    ir_compiler c11;                                // 
    c11.jmp(tag("demo_main"));                      //      jmp     demo_main;
    c11.tag("demo_func_01");                        //  :demo_func_01
    c11.mov(reg(reg::t0), imm(666));                //      mov     t0,     666;
    c11.set(reg(reg::er), imm("example exception"));//      set     er,     "example exception";
    c11.addi(reg(reg::t0), imm(233));               //      addi    t0,     233; 
    c11.ret();                                      //      ret
    c11.tag("demo_main");                           //  :demo_main
    c11.veh_begin(tag("jmp_excep_happend"));        //      veh beg jmp_excep_happend
    c11.call(tag("demo_func_01"));                  //      call    demo_func_01;
    c11.veh_clean(tag("jmp_no_excep"));             //      veh cle jmp_no_excep
    c11.tag("jmp_excep_happend");                   //  :jmp_excep_happend
    c11.addi(reg(reg::t0), imm(1024));              //      addi    t0,     1024
    c11.tag("jmp_no_excep");                        //  :jmp_no_excep
    c11.end();                                      //      end

    vmm.set_runtime(c11);
    vmm.run();

    rs_test(vmm.bp == vmm.env->stack_begin);
    rs_test(vmm.sp == vmm.env->stack_begin);
    rs_test(vmm.veh->last == nullptr);
    rs_test(vmm.cr->type == value::valuetype::is_ref && vmm.cr->get()->integer == 666 + 233);

    ///////////////////////////////////////////////////////////////////////////////////////

    ir_compiler c10;                                // 
    c10.jmp(tag("demo_main"));                      //      jmp     demo_main;
    c10.tag("demo_func_01");                        //  :demo_func_01
    c10.mov(reg(reg::t0), imm(666));                //      mov     t0,     666;
    c10.set(reg(reg::er), imm("example exception"));//      set     er,     "example exception";
    c10.veh_throw();                                //      veh throw
    c10.addi(reg(reg::t0), imm(233));               //      addi    t0,     233; 
    c10.ret();                                      //      ret
    c10.tag("demo_main");                           //  :demo_main
    c10.veh_begin(tag("jmp_excep_happend"));        //      veh beg jmp_excep_happend
    c10.call(tag("demo_func_01"));                  //      call    demo_func_01;
    c10.veh_clean(tag("jmp_no_excep"));             //      veh cle jmp_no_excep
    c10.tag("jmp_excep_happend");                   //  :jmp_excep_happend
    c10.addi(reg(reg::t0), imm(1024));              //      addi    t0,     1024
    c10.tag("jmp_no_excep");                        //  :jmp_no_excep
    c10.end();                                      //      end

    vmm.set_runtime(c10);
    vmm.run();

    rs_test(vmm.bp == vmm.env->stack_begin);
    rs_test(vmm.sp == vmm.env->stack_begin);
    rs_test(vmm.veh->last == nullptr);
    rs_test(vmm.cr->type == value::valuetype::is_ref && vmm.cr->get()->integer == 666 + 1024);
    ///////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////

    ir_compiler c9;                         // 
    c9.jmp(tag("demo_main"));               //      jmp     demo_main;
    c9.tag("demo_func_01");                 //  :demo_func_01
    c9.mov(reg(reg::t0), imm(666));         //      mov     t0,     666;
    c9.addi(reg(reg::t0), imm(233));        //      addi    t0,     233; 
    c9.ret();                               //      ret
    c9.tag("demo_main");                    //  :demo_main
    c9.call(tag("demo_func_01"));           //      call    demo_func_01;
    c9.end();                               //      end

    vmm.set_runtime(c9);
    vmm.run();

    rs_test(vmm.bp == vmm.env->stack_begin);
    rs_test(vmm.sp == vmm.env->stack_begin);
    rs_test(vmm.cr->type == value::valuetype::is_ref && vmm.cr->get()->integer == 233 + 666);
    ///////////////////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////////////////

    ir_compiler c8;                                     // 
    c8.call(&veh_exception_test);
    c8.end();                                          //      end

    vmm.set_runtime(c8);
    vmm.run();

    ///////////////////////////////////////////////////////////////////////////////////////

    ir_compiler c7;                                     // 
    c7.call(&example);
    c7.end();                                          //      end

    vmm.set_runtime(c7);
    vmm.run();

    rs_test(vmm.cr->type == value::valuetype::integer_type && vmm.cr->integer == 9926);
    ///////////////////////////////////////////////////////////////////////////////////////

    ir_compiler c6;                                     // 
    c6.land(imm(1), imm(1));
    c6.lor(imm(0), reg(reg::cr));
    c6.lnot(reg(reg::cr));
    c6.end();                                          //      end

    vmm.set_runtime(c6);
    vmm.run();

    rs_test(vmm.cr->type == value::valuetype::integer_type && vmm.cr->integer == 0);

    ///////////////////////////////////////////////////////////////////////////////////////

    ir_compiler c5;                                     // 
    c5.psh(imm("Helloworld"));
    c5.psh(imm(125.56));
    c5.psh(imm(2133));
    c5.lds(reg(reg::cr), imm(0));
    c5.end();                                           //      end

    vmm.set_runtime(c5);
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

    vmm.set_runtime(c4);
    vmm.run();

    rs_test(vmm.cr->type == value::valuetype::integer_type && vmm.cr->integer == 2);

    ///////////////////////////////////////////////////////////////////////////////////////

    ir_compiler c3;                                     // 
    c3.mov(reg(reg::cr), imm("hello,"));                //      mov     cr,   "hello,";
    c3.adds(reg(reg::cr), imm("world!"));               //      adds    cr,   "world!";
    c3.end();                                           //      end

    vmm.set_runtime(c3);
    vmm.run();

    rs_test(vmm.cr->type == value::valuetype::string_type && *vmm.cr->string == "hello,world!");
    ///////////////////////////////////////////////////////////////////////////////////////

    ir_compiler c2;                                     // fast stack addressing:    
    c2.pshr(imm(2333));                                 //      psh     2333;
    c2.mov(reg(reg::cr), reg(reg::bp_offset(0)));       //      mov     cr,   [bp+0]
    c2.end();                                           //      end

    vmm.set_runtime(c2);
    vmm.run();

    rs_test(vmm.cr->type == value::valuetype::integer_type && vmm.cr->integer == 2333);
    ///////////////////////////////////////////////////////////////////////////////////////

    ir_compiler c0;                                      // fast stack addressing:    
    c0.pshr(imm(2333.0456));                             //      pshr   2333.0456;
    c0.movcast(reg(reg::cr), reg(reg::bp_offset(0)), rs::value::valuetype::integer_type);     
                                                         //      movcast cr,   [bp+0]:int
    c0.end();                                            //      end

    vmm.set_runtime(c0);
    vmm.run();

    rs_test(vmm.cr->type == value::valuetype::integer_type && vmm.cr->integer == 2333);
    ///////////////////////////////////////////////////////////////////////////////////////

    ir_compiler c1;
    c1.tag("all_program_begin");
    c1.psh(imm(0));                                     //      psh     0
    c1.set(reg(reg::bp_offset(0)), imm(0));             //      set     [bp+0],  0              int  i=0
    c1.tag("loop_begin");                               //  :loop_begin
    c1.lti(reg(reg::bp_offset(0)), imm(10));     //      lti     [bp+0],   100000000     while i < 100000000
    c1.jf(tag("loop_end"));                             //      jf      loop_end                {
    c1.addi(reg(reg::bp_offset(0)), imm(1));            //      addi    [bp+0],  1                  i+=1;
    c1.jmp(tag("loop_begin"));                          //      jmp     loop_begin              }
    c1.tag("loop_end");                                 //  :loop_end
    c1.pop(1);                                          //      pop     1
    c1.set(reg(reg::t0), imm("Hello"));
    c1.adds(reg(reg::t0), imm("world"));
    c1.jmp(tag("all_program_begin"));
    c1.end();                                           //      end;


    while (true)
    {
        vmm.set_runtime(c1);

        auto beg = clock();
        vmm.run();
        auto end = clock();

        std::cout << (end - beg) << std::endl;
    }

    ///////////////////////////////////////////////////////////////////////////////////////

}