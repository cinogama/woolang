#define _CRT_SECURE_NO_WARNINGS

#define RS_IMPL
#include "rs.h"
#include <iostream>

#include "rs_vm.hpp"
#include "rs_runtime_debuggee.hpp"

int main(int argc, char** argv)
{
    rs_init(argc, argv);

    std::cout << ANSI_RST;
    std::cout << "RestorableScene ver." << rs_version() << " " << std::endl;
    std::cout << rs_compile_date() << std::endl;

    auto src = (u8R"(
import rscene.std;

func branch_mark_fib40()
{
    func fib(var n:int)
    {
        if (n<=2)
            return 1;
        return fib(n-1) + fib(n-2);
    }

    var beg_tm = std::time();
    var i = 0;
    while (i<40)
    {
        std::println(i, ":", fib(i));
        i+=1;
    }
    var end_tm = std::time();
    std::println("branch_mark_fib40: cost time:", end_tm-beg_tm,);
}

func branch_mark_loop100000000()
{
    var beg_tm = std::time();
    var i = 0;
    while (i<1_0000_0000)
        i += 1;
    var end_tm = std::time();
    std::println("branch_mark_loop100000000: cost time:", end_tm-beg_tm,);
}

func invoke_repeat_n(var times, var fn, ...)
{
    while(times)
    {
        times-=1;
        (fn:dynamic(...))(......);
    }
}

func main()
{
    invoke_repeat_n(5, branch_mark_loop100000000);
    invoke_repeat_n(5, branch_mark_fib40());
}

main();
)");


    rs_vm vmm = rs_create_vm();
    rs_load_source(vmm, "rs_test.rsn", src);

    //((rs::vm*)vmm)->dump_program_bin();

    //rs::default_debuggee dgb;
    //((rs::vm*)vmm)->attach_debuggee(&dgb);
    //dgb.set_breakpoint("rs_test.rsn", 16);
    // ((rs::vm*)vmm)->attach_debuggee(nullptr);

    rs_run(vmm);
    rs_close_vm(vmm);

    // std::cout << "=================" << std::endl;



    return 0;
}