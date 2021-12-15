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

int main(int argc, char** argv)
{
    rs_init(argc, argv);

    std::cout << "RestorableScene ver." << rs_version() << " " << std::endl;
    std::cout << rs_compile_date() << std::endl;

    auto src = (u8R"(
import rscene.std;

func deepin()
{
    var n = [];
    var i = 0;
    while (i<10)
    {
        n->add(std::rand::randreal(0,1));
        i+=1;
    }
    return n;
}

func main()
{
    while (true)
        var result = deepin();
}

main();

)");

    rs_vm vmm = rs_create_vm();
    rs_load_source(vmm, "rs_test.rsn", src);
    rs_run(vmm);
    rs_close_vm(vmm);

    return 0;
}