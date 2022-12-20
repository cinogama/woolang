#include "wo.h"

#include <iostream>
#include <string>
#include <locale.h>

#include <sstream>

int main(int argc, char** argv)
{
    wo_init(argc, argv);

    if (argc >= 2)
    {
        wo_vm vmm = wo_create_vm();
        bool compile_successful_flag = wo_load_file(vmm, argv[1]);

        if (wo_has_compile_error(vmm))
            std::cerr << wo_get_compile_error(vmm, WO_DEFAULT) << std::endl;

        wo_value return_state = nullptr;

        if (compile_successful_flag)
            return_state = wo_run(vmm);
        wo_close_vm(vmm);

        wo_finish();

        if (return_state)
            return 0;
        if (!compile_successful_flag)
            return -2;
        return -1;
    }
    else
    {
        std::cout << "Woolang ver." << wo_version() << " " << std::endl;
        std::cout << wo_compile_date() << std::endl;
    }

    wo_finish();
    return 0;
}