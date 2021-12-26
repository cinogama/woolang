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

    if (argc >= 2)
    {
        rs_vm vmm = rs_create_vm();
        bool compile_successful_flag = rs_load_file(vmm, argv[1]);

        if (rs_has_compile_error(vmm))
            std::cerr << rs_get_compile_error(vmm, RS_NEED_COLOR) << std::endl;
        if (rs_has_compile_warning(vmm))
            std::cerr << rs_get_compile_warning(vmm, RS_NEED_COLOR) << std::endl;

        if (compile_successful_flag)
            rs_run(vmm);
        rs_close_vm(vmm);
    }
    else
    {
        std::cout << "RestorableScene ver." << rs_version() << " " << std::endl;
        std::cout << rs_compile_date() << std::endl;
    }
    

    return 0;
}