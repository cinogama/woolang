#define RS_IMPL
#include "rs.h"
#include <iostream>

int main(int argc, char** argv)
{
    rs_init(argc, argv);

    std::cout << ANSI_RST;
    std::cout << "RestorableScene ver." << rs_version() << " " << std::endl;
    std::cout << rs_compile_date() << std::endl;

    auto src = (R"(
import rscene.std;

namespace std
{
    func boom()
    {
        println("emm... just , boom?");
    }
}

func fib(var n:int)
{
    if (n<=2)
        return 1;
    return fib(n-1) + fib(n-2);
}

func main()
{
    using std;

    boom();

    println("I'm Restorable Scene~");
    println(
@"This is a multi-line string
and we can write string like this....
and we can using println("...") to display."@
    );

    var arr = nil:dynamic(...);
    arr();

    var begin_tm = time();
    var i = 0;
    while (i < 40)
    {
        print(i, ":", fib(i), "\n");
        i += 1;
    }
    var finish_tm = time();

    println("cost time:", finish_tm-begin_tm);
    return 0;
}
main();

)");

    rs_vm vmm = rs_create_vm();
    rs_load_source(vmm, "rs_test.rsn", src);

    // ((rs::vm*)vmm)->dump_program_bin();

    /* default_debuggee dgb; */
    // ((rs::vm*)vmm)->attach_debuggee(&dgb);
    // dgb.set_breakpoint("rs_test.rsn", 36);
    // ((rs::vm*)vmm)->attach_debuggee(nullptr);

    rs_run(vmm);
    rs_close_vm(vmm);

    // std::cout << "=================" << std::endl;



    return 0;
}