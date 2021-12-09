#define RS_IMPL
#include "rs.h"
#include <iostream>

#include "rs_vm.hpp"

int main(int argc, char** argv)
{
    rs_init(argc, argv);

    std::cout << ANSI_RST;
    std::cout << "RestorableScene ver." << rs_version() << " " << std::endl;
    std::cout << rs_compile_date() << std::endl;

    auto src = (u8R"(
enum Enum
{
    XXX
}

const var fxxl = Enum::XXX;
var o0 = fxxl;

)");

    rs_vm vmm = rs_create_vm();
    rs_load_source(vmm, "rs_test.rsn", src);

    ((rs::vm*)vmm)->dump_program_bin();

    /* default_debuggee dgb; */
    // ((rs::vm*)vmm)->attach_debuggee(&dgb);
    // dgb.set_breakpoint("rs_test.rsn", 36);
    // ((rs::vm*)vmm)->attach_debuggee(nullptr);

    rs_run(vmm);
    rs_close_vm(vmm);

    // std::cout << "=================" << std::endl;



    return 0;
}