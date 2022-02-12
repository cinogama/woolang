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
#include <locale.h>

int main(int argc, char** argv)
{
    rs_init(argc, argv);

    if (argc >= 2)
    {
        rs_vm vmm = rs_create_vm();
        bool compile_successful_flag = rs_load_file(vmm, argv[1]);

        if (rs_has_compile_error(vmm))
            std::cerr << rs_get_compile_error(vmm, RS_DEFAULT) << std::endl;
        if (rs_has_compile_warning(vmm))
            std::cerr << rs_get_compile_warning(vmm, RS_DEFAULT) << std::endl;

        rs_value return_state = nullptr;

        if (compile_successful_flag)
            return_state = rs_run(vmm);
        rs_close_vm(vmm);

        rs_finish();

        if (return_state)
            return 0;
        if (!compile_successful_flag)
            return -2;
        return -1;
    }
    else
    {
        std::cout << "RestorableScene ver." << rs_version() << " " << std::endl;
        std::cout << rs_compile_date() << std::endl;
    }

    rs_finish();
    return 0;
}