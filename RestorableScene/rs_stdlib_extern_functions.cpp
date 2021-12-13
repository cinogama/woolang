#define _CRT_SECURE_NO_WARNINGS
#include "rs_lang_extern_symbol_loader.hpp"
#include "rs_utf8.hpp"

#include <chrono>
#include <random>

RS_API rs_api rslib_std_print(rs_vm vm, rs_value args, size_t argc)
{
    for (size_t i = 0; i < argc; i++)
    {
        std::cout << rs_cast_string(args + i);

        if (i + 1 < argc)
            std::cout << " ";
    }
    return rs_ret_int(vm, argc);
}
RS_API rs_api rslib_std_fail(rs_vm vm, rs_value args, size_t argc)
{
    rs_fail(RS_FAIL_CALL_FAIL, rs_string(args + 0));

    return rs_ret_nil(vm);
}
RS_API rs_api rslib_std_lengthof(rs_vm vm, rs_value args, size_t argc)
{
    return rs_ret_int(vm, rs_lengthof(args));
}
RS_API rs_api rslib_std_time_sec(rs_vm vm, rs_value args, size_t argc)
{
    static std::chrono::system_clock _sys_clock;
    static auto _first_invoke_time = _sys_clock.now();

    auto _time_ms = rs_real_t((_sys_clock.now() - _first_invoke_time).count() * std::chrono::system_clock::period::num)
        / std::chrono::system_clock::period::den;
    return rs_ret_real(vm, _time_ms);
}
RS_API rs_api rslib_std_randomint(rs_vm vm, rs_value args, size_t argc)
{
    static std::random_device rd;
    static std::mt19937_64 mt64(rd());

    rs_int_t from = rs_int(args + 0);
    rs_int_t to = rs_int(args + 1);

    if (to < from)
        std::swap(from, to);

    std::uniform_int_distribution<rs_int_t> dis(from, to);
    return rs_ret_int(vm, dis(mt64));
}
RS_API rs_api rslib_std_randomreal(rs_vm vm, rs_value args)
{
    static std::random_device rd;
    static std::mt19937_64 mt64(rd());

    rs_real_t from = rs_real(args + 0);
    rs_real_t to = rs_real(args + 1);

    if (to < from)
        std::swap(from, to);

    std::uniform_real_distribution<rs_real_t> dis(from, to);
    return rs_ret_real(vm, dis(mt64));
}

RS_API rs_api rslib_std_array_resize(rs_vm vm, rs_value args, size_t argc)
{
    rs_arr_resize(args + 0, rs_int(args + 1));
    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_array_add(rs_vm vm, rs_value args, size_t argc)
{
    return rs_ret_ref(vm, rs_arr_add(args + 0, args + 1));
}

RS_API rs_api rslib_std_map_find(rs_vm vm, rs_value args, size_t argc)
{
    rs_map_find(args + 0, args + 1);
    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_map_only_get(rs_vm vm, rs_value args, size_t argc)
{
    bool _map_has_indexed_val = rs_map_find(args + 0, args + 1);

    if (_map_has_indexed_val)
        return rs_ret_ref(vm, rs_map_get(args + 0, args + 1));

    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_map_get_by_default(rs_vm vm, rs_value args, size_t argc)
{
    bool _map_has_indexed_val = rs_map_find(args + 0, args + 1);

    if (_map_has_indexed_val)
        return rs_ret_ref(vm, rs_map_get(args + 0, args + 1));

    rs_value mapping_indexed = rs_map_get(args + 0, args + 1);
    rs_set_val(mapping_indexed, args + 2);

    return rs_ret_ref(vm, mapping_indexed);
}

RS_API rs_api rslib_std_sub(rs_vm vm, rs_value args, size_t argc)
{
    if (rs_valuetype(args + 0) == RS_STRING_TYPE)
    {
        // return substr
        size_t sub_str_len = 0;
        if (argc == 2)
        {
            auto* substring = rs::u8substr(rs_string(args + 0), rs_int(args + 1), rs::u8str_npos, &sub_str_len);
            return rs_ret_string(vm, std::string(substring, sub_str_len).c_str());
        }
        auto* substring = rs::u8substr(rs_string(args + 0), rs_int(args + 1), rs_int(args + 2), &sub_str_len);
        return rs_ret_string(vm, std::string(substring, sub_str_len).c_str());
    }

    //return rs_ret_ref(vm, mapping_indexed);
    return rs_ret_nil(vm);
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

    namespace rand
    {
        extern("rslib_std_randomint") 
            func randint(var from:int, var to:int):int;
        extern("rslib_std_randomreal") 
            func randreal(var from:real, var to:real):real;
    }
}

namespace string
{
    extern("rslib_std_lengthof") 
        func len(var val:string):int;
    extern("rslib_std_sub")
        func sub(var val:string, var begin:int):string;
    extern("rslib_std_sub")
        func sub(var val:string, var begin:int, var length:int):string;
}

namespace array
{
    extern("rslib_std_lengthof") 
        func len(var val:array):int;
    extern("rslib_std_array_resize") 
        func resize(var val:array, var newsz:int):void;
    func get(var a:array, var index:int)
    {
        return a[index];
    }
    extern("rslib_std_array_add") 
        func add(var val:array, var elem):dynamic;
}

namespace map
{
    extern("rslib_std_lengthof") 
        func len(var val:map):int;
    extern("rslib_std_map_find") 
        func find(var val:map, var index):int;
    extern("rslib_std_map_only_get") 
        func get(var m:map, var index:dynamic):dynamic;
    extern("rslib_std_map_get_by_default") 
        func get(var m:map, var index:dynamic, var default_val:dynamic):dynamic;
}
)";