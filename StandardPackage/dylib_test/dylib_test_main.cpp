#include "../../RestorableScene/rs.h"

#ifdef _WIN32
#   ifdef _WIN64
#       ifdef NDEBUG
#           pragma comment(lib, "../../x64/Release/librscene.lib")
#       else
#           pragma comment(lib,"../../x64/Debug/librscene_debug.lib")
#       endif
#   else
#       ifdef NDEBUG
#           pragma comment(lib,"../../Release/librscene32.lib")
#       else
#           pragma comment(lib,"../../Debug/librscene32_debug.lib")
#       endif
#   endif
#endif

RS_API rs_result_t test_helloworld(rs_vm vm, rs_value args, size_t argc)
{
    return rs_ret_string(vm, "Helloworld");
}