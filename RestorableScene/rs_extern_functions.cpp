#include "rs_extern_functions.hpp"

RS_API void rslib_std_print(rs_vm vm, rs_value args)
{
    auto argcount = rs_argc(vm);
    for (rs_integer_t i = 0; i < argcount; i++)
    {
        std::cout << rs_cast_string(args + i);

        if (i + 1 < argcount) 
            std::cout << " ";
    }
    return rs_ret_int(vm, argcount);
}

RS_API void rslib_std_fail(rs_vm vm, rs_value args)
{
    rs_fail(RS_ERR_CALL_FAIL, rs_string(args + 0));
}
