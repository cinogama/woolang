#include "rs_extern_functions.hpp"

RS_API void rslib_std_print(rs_vm vm, rs_value args)
{
    for (rs_integer_t i = 0; i < rs_argc(vm); i++)
    {
        std::cout << rs_cast_string(args + i);
    }
}

RS_API void rslib_std_fail(rs_vm vm, rs_value args)
{
    rs_fail(RS_ERR_CALL_FAIL, rs_string(args + 0));
}
