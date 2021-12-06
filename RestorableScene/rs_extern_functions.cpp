#include "rs_extern_functions.hpp"

#include <chrono>

RS_API rs_api rslib_std_print(rs_vm vm, rs_value args)
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

RS_API rs_api rslib_std_fail(rs_vm vm, rs_value args)
{
    rs_fail(RS_ERR_CALL_FAIL, rs_string(args + 0));

    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_lengthof(rs_vm vm, rs_value args)
{
    return rs_ret_int(vm, rs_lengthof(args));
}

RS_API rs_api rslib_std_time_sec(rs_vm vm, rs_value args)
{
    static std::chrono::system_clock _sys_clock;
    static auto _first_invoke_time = _sys_clock.now();

    auto _time_ms = rs_real_t((_sys_clock.now() - _first_invoke_time).count() * std::chrono::system_clock::period::num)
                / std::chrono::system_clock::period::den;
    return rs_ret_real(vm, _time_ms);
}
