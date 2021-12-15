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

    // std::cout << "=================" << std::endl;



    return 0;
}