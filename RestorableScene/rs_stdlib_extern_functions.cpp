#define _CRT_SECURE_NO_WARNINGS
#include "rs_lang_extern_symbol_loader.hpp"

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
    rs_fail(RS_FAIL_CALL_FAIL, rs_string(args + 0));

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

const char* rs_stdlib_src_path = u8"rscene/std.rsn";
const char* rs_stdlib_src_data = 
u8R"(
const var true = 1;
const var false = 0;

namespace std
{
    extern("rslib_std_fail") func fail(var msg:string):void;
    extern("rslib_std_print") func print(...):int;
    extern("rslib_std_lengthof") func len(var val):int;
    extern("rslib_std_time_sec") func time():real;

    func println(...)
    {
        var c = print((...)...);
        print("\n");
        return c;
    }
    func assert(var judgement, var failed_info:string)
    {
        if (!judgement)
            fail(failed_info);
    }
}

)";