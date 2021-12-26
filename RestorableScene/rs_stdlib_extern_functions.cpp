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
    return rs_ret_int(vm, rs_map_find(args + 0, args + 1));
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

RS_API rs_api rslib_std_thread_sleep(rs_vm vm, rs_value args, size_t argc)
{
    using namespace std;

    std::this_thread::sleep_for(rs_real(args) * 1s);
    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_vm_create(rs_vm vm, rs_value args, size_t argc)
{
    return rs_ret_pointer(vm, rs_create_vm());
}

RS_API rs_api rslib_std_vm_close(rs_vm vm, rs_value args, size_t argc)
{
    rs_close_vm((rs_vm)rs_pointer(args));

    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_vm_load_src(rs_vm vm, rs_value args, size_t argc)
{
    rs_vm vmm = (rs_vm)rs_pointer(args);

    bool compile_result;
    if (argc < 3)
        compile_result = rs_load_source(vmm, "_temp_source.rsn", rs_string(args + 1));
    else
        compile_result = rs_load_source(vmm, rs_string(args + 1), rs_string(args + 2));

    return rs_ret_int(vm, compile_result);
}

RS_API rs_api rslib_std_vm_load_file(rs_vm vm, rs_value args, size_t argc)
{
    rs_vm vmm = (rs_vm)rs_pointer(args);
    bool compile_result = rs_load_file(vmm, rs_string(args + 1));
    return rs_ret_int(vm, compile_result);
}

RS_API rs_api rslib_std_vm_run(rs_vm vm, rs_value args, size_t argc)
{
    rs_vm vmm = (rs_vm)rs_pointer(args);
    rs_value ret = rs_run(vmm);

    return rs_ret_val(vm, ret);
}

RS_API rs_api rslib_std_vm_has_compile_error(rs_vm vm, rs_value args, size_t argc)
{
    rs_vm vmm = (rs_vm)rs_pointer(args);
    return rs_ret_int(vm, rs_has_compile_error(vmm));
}

RS_API rs_api rslib_std_vm_has_compile_warning(rs_vm vm, rs_value args, size_t argc)
{
    rs_vm vmm = (rs_vm)rs_pointer(args);
    return rs_ret_int(vm, rs_has_compile_warning(vmm));
}

RS_API rs_api rslib_std_vm_get_compile_error(rs_vm vm, rs_value args, size_t argc)
{
    rs_vm vmm = (rs_vm)rs_pointer(args);
    _rs_inform_style style = argc > 1 ? (_rs_inform_style)rs_int(args + 1) : RS_NOTHING;

    return rs_ret_string(vm, rs_get_compile_error(vmm, style));
}

RS_API rs_api rslib_std_vm_get_compile_warning(rs_vm vm, rs_value args, size_t argc)
{
    rs_vm vmm = (rs_vm)rs_pointer(args);
    _rs_inform_style style = argc > 1 ? (_rs_inform_style)rs_int(args + 1) : RS_NOTHING;

    return rs_ret_string(vm, rs_get_compile_warning(vmm, style));
}

RS_API rs_api rslib_std_vm_virtual_source(rs_vm vm, rs_value args, size_t argc)
{
    return rs_ret_int(vm, rs_virtual_source(
        rs_string(args + 0),
        rs_string(args + 1),
        rs_int(args + 2)
    ));
}

const char* rs_stdlib_basic_src_path = u8"rscene/basic.rsn";
const char* rs_stdlib_basic_src_data = { u8R"(
using bool = int;

const var true = 1:bool;
const var false = 0:bool;
)" };

const char* rs_stdlib_src_path = u8"rscene/std.rsn";
const char* rs_stdlib_src_data = {
u8R"(
import rscene.basic;

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

    extern("rslib_std_randomint") 
        func rand(var from:int, var to:int):int;

    extern("rslib_std_randomreal") 
        func rand(var from:real, var to:real):real;

    extern("rslib_std_thread_sleep")
    func sleep(var tm:real):void;
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
        func len<T>(var val:array<T>):int;

    extern("rslib_std_array_resize") 
        func resize<T>(var val:array<T>, var newsz:int):void;

    func get<T>(var a:array<T>, var index:int)
    {
        return a[index];
    }

    extern("rslib_std_array_add") 
        func add<T>(var val:array<T>, var elem:T):T;
  
    func dup<T>(var val:array<T>)
    {
        const var _dupval = val;
        return _dupval;
    }
}

namespace map
{
    extern("rslib_std_lengthof") 
        func len<KT, VT>(var val:map<KT, VT>):int;
    extern("rslib_std_map_find") 
        func find<KT, VT>(var val:map<KT, VT>, var index:KT):bool;
    extern("rslib_std_map_only_get") 
        func get<KT, VT>(var m:map<KT, VT>, var index:KT):VT;
    extern("rslib_std_map_get_by_default") 
        func get<KT, VT>(var m:map<KT, VT>, var index:KT, var default_val:VT):VT;
    func dup<KT, VT>(var val:map<KT, VT>)
    {
        const var _dupval = val;
        return _dupval;
    }
}
)" };

RS_API rs_api rslib_std_debug_attach_default_debuggee(rs_vm vm, rs_value args, size_t argc)
{
    rs_attach_default_debuggee(vm);
    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_debug_disattach_default_debuggee(rs_vm vm, rs_value args, size_t argc)
{
    rs_disattach_and_free_debuggee(vm);
    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_debug_breakpoint(rs_vm vm, rs_value args, size_t argc)
{
    rs_break_immediately(vm);
    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_debug_invoke(rs_vm vm, rs_value args, size_t argc)
{
    for (size_t index = argc - 1; index > 0; index--)
        rs_push_ref(vm, args + index);

    return rs_ret_val(vm, rs_invoke_value(vm, args, argc - 1));
}

const char* rs_stdlib_debug_src_path = u8"rscene/debug.rsn";
const char* rs_stdlib_debug_src_data = {
u8R"(
namespace std
{
    namespace debug
    {
        extern("rslib_std_debug_breakpoint")
            func breakpoint():void;
        extern("rslib_std_debug_attach_default_debuggee")
            func attach_debuggee():void;
        extern("rslib_std_debug_disattach_default_debuggee")
            func disattach_debuggee():void;

        func run(var foo, ...)
        {
            attach_debuggee();
            var result = (foo:dynamic(...))(......);
            disattach_debuggee();
    
            return result;
        }

        extern("rslib_std_debug_invoke")
        func invoke(var foo:dynamic, ...):dynamic;
    }
}
)" };

const char* rs_stdlib_vm_src_path = u8"rscene/vm.rsn";
const char* rs_stdlib_vm_src_data = {
u8R"(
import rscene.basic;

namespace std
{
    using vm = handle;
    namespace vm
    {
        enum info_style
        {
            RS_NOTHING = 0,
            RS_NEED_COLOR = 1,
        }

        extern("rslib_std_vm_create")
        func create():vm;

        extern("rslib_std_vm_close")
        func close(var vmhandle:vm):void;

        extern("rslib_std_vm_load_src")
        func load_source(var vmhandle:vm, var src:string):bool;
        extern("rslib_std_vm_load_src")
        func load_source(var vmhandle:vm, var vfilepath:string, var src:string):bool;

        extern("rslib_std_vm_load_file")
        func load_file(var vmhandle:vm, var vfilepath:string):bool;

        extern("rslib_std_vm_run")
        func run(var vmhandle:vm):dynamic;
        
        extern("rslib_std_vm_has_compile_error")
        func has_error(var vmhandle:vm):bool;

        extern("rslib_std_vm_has_compile_warning")
        func has_warning(var vmhandle:vm):bool;

        extern("rslib_std_vm_get_compile_error")
        func error_msg(var vmhandle:vm):string;

        extern("rslib_std_vm_get_compile_warning")
        func warning_msg(var vmhandle:vm):string;

        extern("rslib_std_vm_get_compile_error")
        func error_msg(var vmhandle:vm, var style:info_style):string;

        extern("rslib_std_vm_get_compile_warning")
        func warning_msg(var vmhandle:vm, var style:info_style):string;
        
        extern("rslib_std_vm_virtual_source")
        func virtual_source(var vfilepath:string, var src:string, var enable_overwrite:bool):bool;
    }
}
)" };