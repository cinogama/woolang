#include "../RestorableScene/rs.h"

#ifdef _WIN32
#   ifdef _WIN64
#       ifdef NDEBUG
#           pragma comment(lib, "../x64/Release/librscene.lib")
#       else
#           pragma comment(lib,"../x64/Debug/librscene_debug.lib")
#       endif
#   else
#       ifdef NDEBUG
#           pragma comment(lib,"../Release/librscene32.lib")
#       else
#           pragma comment(lib,"../Debug/librscene32_debug.lib")
#       endif
#   endif
#endif

#include <iostream>
#include <string>

int main(int argc, char** argv)
{
    rs_init(argc, argv);

    std::cout << "RestorableScene ver." << rs_version() << " " << std::endl;
    std::cout << rs_compile_date() << std::endl;

    auto src = (u8R"(
import rscene.std;

func invoke(var f, ...)
{
    return (f:dynamic(...))(......);
}

func foo(var n:int)
{ 
    if (n)
        invoke(foo, n-1);
}

func main()
{
    while (true)
        foo(6);
}

main();

)");

    rs_vm vmm = rs_create_vm();
    rs_load_source(vmm, "rs_test.rsn", src);
    rs_run(vmm);
    rs_close_vm(vmm);

    return 0;
}