#define _CRT_SECURE_NO_WARNINGS
#include "rs_lang_extern_symbol_loader.hpp"
#include "rs_utf8.hpp"
#include "rs_vm.hpp"
#include "rs_roroutine_simulate_mgr.hpp"
#include "rs_roroutine_thread_mgr.hpp"
#include "rs_io.hpp"

#include <chrono>
#include <random>
#include <thread>

RS_API rs_api rslib_std_print(rs_vm vm, rs_value args, size_t argc)
{
    for (size_t i = 0; i < argc; i++)
    {
        rs::rs_stdout << rs_cast_string(args + i);

        if (i + 1 < argc)
            rs::rs_stdout << " ";
    }
    return rs_ret_int(vm, argc);
}
RS_API rs_api rslib_std_panic(rs_vm vm, rs_value args, size_t argc)
{
    rs_fail(RS_FAIL_DEADLY, rs_string(args + 0));
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
    rs_arr_resize(args + 0, rs_int(args + 1), args + 2);
    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_array_add(rs_vm vm, rs_value args, size_t argc)
{
    return rs_ret_ref(vm, rs_arr_add(args + 0, args + 1));
}

RS_API rs_api rslib_std_array_remove(rs_vm vm, rs_value args, size_t argc)
{
    rs_arr_remove(args + 0, rs_int(args + 1));

    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_array_find(rs_vm vm, rs_value args, size_t argc)
{
    return rs_ret_int(vm, rs_arr_find(args + 0, args + 1));
}

RS_API rs_api rslib_std_array_clear(rs_vm vm, rs_value args, size_t argc)
{
    rs_arr_clear(args);

    return rs_ret_nil(vm);
}

struct array_iter
{
    using array_iter_t = decltype(std::declval<rs::array_t>().begin());

    array_iter_t iter;
    array_iter_t end_place;
    rs_int_t     index_count;
};

RS_API rs_api rslib_std_array_iter(rs_vm vm, rs_value args, size_t argc)
{
    rs::value* arr = reinterpret_cast<rs::value*>(args)->get();

    if (arr->is_nil())
    {
        rs_fail(RS_FAIL_TYPE_FAIL, "Value is 'nil'.");
        return rs_ret_nil(vm);
    }
    else
    {
        return rs_ret_gchandle(vm,
            new array_iter{ arr->array->begin(), arr->array->end(), 0 },
            args + 0,
            [](void* array_iter_t_ptr)
            {
                delete (array_iter*)array_iter_t_ptr;
            }
        );
    }
}

RS_API rs_api rslib_std_array_iter_next(rs_vm vm, rs_value args, size_t argc)
{
    array_iter& iter = *(array_iter*)rs_pointer(args);

    if (iter.iter == iter.end_place)
        return rs_ret_bool(vm, false);

    rs_set_int(args + 1, iter.index_count++); // key
    rs_set_val(args + 2, reinterpret_cast<rs_value>(&*(iter.iter++))); // val

    return rs_ret_bool(vm, true);
}

RS_API rs_api rslib_std_map_find(rs_vm vm, rs_value args, size_t argc)
{
    return rs_ret_bool(vm, rs_map_find(args + 0, args + 1));
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

RS_API rs_api rslib_std_map_remove(rs_vm vm, rs_value args, size_t argc)
{
    rs_fail(RS_FAIL_NOT_SUPPORT, "This function not support yet.");
    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_map_clear(rs_vm vm, rs_value args, size_t argc)
{
    rs_fail(RS_FAIL_NOT_SUPPORT, "This function not support yet.");
    return rs_ret_nil(vm);
}

struct map_iter
{
    using mapping_iter_t = decltype(std::declval<rs::mapping_t>().begin());

    mapping_iter_t iter;
    mapping_iter_t end_place;
};

RS_API rs_api rslib_std_map_iter(rs_vm vm, rs_value args, size_t argc)
{
    rs::value* mapp = reinterpret_cast<rs::value*>(args)->get();

    if (mapp->is_nil())
    {
        rs_fail(RS_FAIL_TYPE_FAIL, "Value is 'nil'.");
        return rs_ret_nil(vm);
    }
    else
    {
        return rs_ret_gchandle(vm,
            new map_iter{ mapp->mapping->begin(), mapp->mapping->end() },
            args + 0,
            [](void* array_iter_t_ptr)
            {
                delete (map_iter*)array_iter_t_ptr;
            }
        );
    }
}

RS_API rs_api rslib_std_map_iter_next(rs_vm vm, rs_value args, size_t argc)
{
    map_iter& iter = *(map_iter*)rs_pointer(args);

    if (iter.iter == iter.end_place)
        return rs_ret_bool(vm, false);

    rs_set_val(args + 1, reinterpret_cast<rs_value>(const_cast<rs::value*>(&iter.iter->first))); // key
    rs_set_val(args + 2, reinterpret_cast<rs_value>(&iter.iter->second)); // val
    iter.iter++;

    return rs_ret_bool(vm, true);
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
    return rs_ret_gchandle(vm,
        rs_create_vm(),
        nullptr,
        [](void* vm_ptr) {
            rs_close_vm((rs_vm)vm_ptr);
        });
}

RS_API rs_api rslib_std_vm_load_src(rs_vm vm, rs_value args, size_t argc)
{
    rs_vm vmm = (rs_vm)rs_pointer(args);

    bool compile_result;
    if (argc < 3)
        compile_result = rs_load_source(vmm, "_temp_source.rsn", rs_string(args + 1));
    else
        compile_result = rs_load_source(vmm, rs_string(args + 1), rs_string(args + 2));

    return rs_ret_bool(vm, compile_result);
}

RS_API rs_api rslib_std_vm_load_file(rs_vm vm, rs_value args, size_t argc)
{
    rs_vm vmm = (rs_vm)rs_pointer(args);
    bool compile_result = rs_load_file(vmm, rs_string(args + 1));
    return rs_ret_bool(vm, compile_result);
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
    return rs_ret_bool(vm, rs_has_compile_error(vmm));
}

RS_API rs_api rslib_std_vm_has_compile_warning(rs_vm vm, rs_value args, size_t argc)
{
    rs_vm vmm = (rs_vm)rs_pointer(args);
    return rs_ret_bool(vm, rs_has_compile_warning(vmm));
}

RS_API rs_api rslib_std_vm_get_compile_error(rs_vm vm, rs_value args, size_t argc)
{
    rs_vm vmm = (rs_vm)rs_pointer(args);
    _rs_inform_style style = argc > 1 ? (_rs_inform_style)rs_int(args + 1) : RS_DEFAULT;

    return rs_ret_string(vm, rs_get_compile_error(vmm, style));
}

RS_API rs_api rslib_std_vm_get_compile_warning(rs_vm vm, rs_value args, size_t argc)
{
    rs_vm vmm = (rs_vm)rs_pointer(args);
    _rs_inform_style style = argc > 1 ? (_rs_inform_style)rs_int(args + 1) : RS_DEFAULT;

    return rs_ret_string(vm, rs_get_compile_warning(vmm, style));
}

RS_API rs_api rslib_std_vm_virtual_source(rs_vm vm, rs_value args, size_t argc)
{
    return rs_ret_bool(vm, rs_virtual_source(
        rs_string(args + 0),
        rs_string(args + 1),
        rs_int(args + 2)
    ));
}

RS_API rs_api rslib_std_gchandle_close(rs_vm vm, rs_value args, size_t argc)
{
    return rs_gchandle_close(args);
}

RS_API rs_api rslib_std_thread_yield(rs_vm vm, rs_value args, size_t argc)
{
    rs_co_yield();
    return rs_ret_nil(vm);
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
    extern("rslib_std_panic") func panic(var msg:string):void;
    extern("rslib_std_print") func print(...):int;
    extern("rslib_std_time_sec") func time():real;

    func println(...)
    {
        var c = print((...)...);
        print("\n");
        return c;
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
        func resize<T>(var val:array<T>, var newsz:int, var init_val:T):void;

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

    extern("rslib_std_array_remove")
        func remove<T>(var val:array<T>, var index:int):void;

    extern("rslib_std_array_find")
        func find<T>(var val:array<T>, var elem:dynamic):int;

    extern("rslib_std_array_clear")
        func clear<T>(var val:array<T>):void;

    using iterator<T> = gchandle;
    namespace iterator 
    {
        extern("rslib_std_array_iter_next")
            func next<T>(var iter:iterator<T>, ref out_key:int, ref out_val:T):bool;
    }

    extern("rslib_std_array_iter")
        func iter<T>(var val:array<T>):iterator<T>;
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

    extern("rslib_std_map_remove")
        func remove<KT, VT>(var val:map<KT, VT>, var index:int):void;
    extern("rslib_std_map_clear")
        func clear<KT, VT>(var val:map<KT, VT>):void;

    using iterator<KT, VT> = gchandle;
    namespace iterator 
    {
        extern("rslib_std_map_iter_next")
            func next<KT, VT>(var iter:iterator<KT, VT>, ref out_key:KT, ref out_val:VT):bool;
    }

    extern("rslib_std_map_iter")
        func iter<KT, VT>(var val:map<KT, VT>):iterator<KT, VT>;
}

namespace gchandle
{
    extern("rslib_std_gchandle_close")
        func close(var handle:gchandle):void;
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
        func invoke<FT>(var foo:FT, ...):typeof(foo(......));
    }
}
)" };

const char* rs_stdlib_vm_src_path = u8"rscene/vm.rsn";
const char* rs_stdlib_vm_src_data = {
u8R"(
import rscene.basic;

namespace std
{
    using vm = gchandle;
    namespace vm
    {
        enum info_style
        {
            RS_DEFAULT = 0,

            RS_NOTHING = 1,
            RS_NEED_COLOR = 2,
        }

        extern("rslib_std_vm_create")
        func create():vm;

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

struct rs_thread_pack
{
    std::thread* _thread;
    rs_vm _vm;
};

RS_API rs_api rslib_std_thread_create(rs_vm vm, rs_value args, size_t argc)
{
    rs_vm new_thread_vm = rs_sub_vm(vm);

    rs_int_t funcaddr_vm = 0;
    rs_handle_t funcaddr_native = 0;

    if (rs_valuetype(args) == RS_INTEGER_TYPE)
        funcaddr_vm = rs_int(args);
    else if (rs_valuetype(args) == RS_HANDLE_TYPE)
        funcaddr_native = rs_handle(args);
    else
        rs_fail(RS_FAIL_TYPE_FAIL, "Cannot invoke this type of value.");

    for (size_t argidx = argc - 1; argidx > 0; argidx--)
    {
        rs_push_valref(new_thread_vm, args + argidx);
    }

    auto* _vmthread = new std::thread([=]() {
        try
        {
            if (funcaddr_vm)
                rs_invoke_rsfunc((rs_vm)new_thread_vm, funcaddr_vm, argc - 1);
            else
                rs_invoke_exfunc((rs_vm)new_thread_vm, funcaddr_native, argc - 1);
        }
        catch (...)
        {
            // ?
        }

        rs_close_vm(new_thread_vm);
        });

    return rs_ret_gchandle(vm,
        new rs_thread_pack{ _vmthread , new_thread_vm },
        nullptr,
        [](void* rs_thread_pack_ptr)
        {
            if (((rs_thread_pack*)rs_thread_pack_ptr)->_thread->joinable())
                ((rs_thread_pack*)rs_thread_pack_ptr)->_thread->detach();
            delete ((rs_thread_pack*)rs_thread_pack_ptr)->_thread;
        });
}

RS_API rs_api rslib_std_thread_wait(rs_vm vm, rs_value args, size_t argc)
{
    rs_thread_pack* rtp = (rs_thread_pack*)rs_pointer(args);

    if (rtp->_thread->joinable())
        rtp->_thread->join();

    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_thread_abort(rs_vm vm, rs_value args, size_t argc)
{
    rs_thread_pack* rtp = (rs_thread_pack*)rs_pointer(args);
    return rs_ret_int(vm, rs_abort_vm(rtp->_vm));
}


RS_API rs_api rslib_std_thread_mutex_create(rs_vm vm, rs_value args, size_t argc)
{
    return rs_ret_gchandle(vm,
        new std::shared_mutex,
        nullptr,
        [](void* mtx_ptr)
        {
            delete (std::shared_mutex*)mtx_ptr;
        });
}

RS_API rs_api rslib_std_thread_mutex_read(rs_vm vm, rs_value args, size_t argc)
{
    std::shared_mutex* smtx = (std::shared_mutex*)rs_pointer(args);
    smtx->lock_shared();

    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_thread_mutex_write(rs_vm vm, rs_value args, size_t argc)
{
    std::shared_mutex* smtx = (std::shared_mutex*)rs_pointer(args);
    smtx->lock();

    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_thread_mutex_read_end(rs_vm vm, rs_value args, size_t argc)
{
    std::shared_mutex* smtx = (std::shared_mutex*)rs_pointer(args);
    smtx->unlock_shared();

    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_thread_mutex_write_end(rs_vm vm, rs_value args, size_t argc)
{
    std::shared_mutex* smtx = (std::shared_mutex*)rs_pointer(args);
    smtx->unlock();

    return rs_ret_nil(vm);
}

////////////////////////////////////////////////////////////////////////

RS_API rs_api rslib_std_thread_spin_create(rs_vm vm, rs_value args, size_t argc)
{
    return rs_ret_gchandle(vm,
        new rs::gcbase::rw_lock,
        nullptr,
        [](void* mtx_ptr)
        {
            delete (rs::gcbase::rw_lock*)mtx_ptr;
        });
}

RS_API rs_api rslib_std_thread_spin_read(rs_vm vm, rs_value args, size_t argc)
{
    rs::gcbase::rw_lock* smtx = (rs::gcbase::rw_lock*)rs_pointer(args);
    smtx->lock_shared();

    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_thread_spin_write(rs_vm vm, rs_value args, size_t argc)
{
    rs::gcbase::rw_lock* smtx = (rs::gcbase::rw_lock*)rs_pointer(args);
    smtx->lock();

    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_thread_spin_read_end(rs_vm vm, rs_value args, size_t argc)
{
    rs::gcbase::rw_lock* smtx = (rs::gcbase::rw_lock*)rs_pointer(args);
    smtx->unlock_shared();

    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_thread_spin_write_end(rs_vm vm, rs_value args, size_t argc)
{
    rs::gcbase::rw_lock* smtx = (rs::gcbase::rw_lock*)rs_pointer(args);
    smtx->unlock();

    return rs_ret_nil(vm);
}



const char* rs_stdlib_thread_src_path = u8"rscene/thread.rsn";
const char* rs_stdlib_thread_src_data = {
u8R"(
import rscene.basic;

namespace std
{
    using thread = gchandle;
    namespace thread
    {
        extern("rslib_std_thread_create")
            func create<FuncT>(var thread_work:FuncT, ...):thread;

        extern("rslib_std_thread_wait")
            func wait(var threadhandle : thread):void;

        extern("rslib_std_thread_abort")
            func abort(var threadhandle : thread):bool;
    }

    using mutex = gchandle;
    namespace mutex
    {
        extern("rslib_std_thread_mutex_create")
            func create():mutex;

        extern("rslib_std_thread_mutex_read")
            func read(var mtx : mutex):void;

        extern("rslib_std_thread_mutex_write")
            func lock(var mtx : mutex):void;

        extern("rslib_std_thread_mutex_read_end")
            func unread(var mtx : mutex):void;

        extern("rslib_std_thread_mutex_write_end")
            func unlock(var mtx : mutex):void;
    }

    using spin = gchandle;
    namespace spin
    {
        extern("rslib_std_thread_spin_create")
            func create():spin;

        extern("rslib_std_thread_spin_read")
            func read(var mtx : spin):void;

        extern("rslib_std_thread_spin_write")
            func lock(var mtx : spin):void;

        extern("rslib_std_thread_spin_read_end")
            func unread(var mtx : spin):void;

        extern("rslib_std_thread_spin_write_end")
            func unlock(var mtx : spin):void;
    }
}

)" };


///////////////////////////////////////////////////////////////////////////////////////
// roroutine APIs
///////////////////////////////////////////////////////////////////////////////////////

RS_API rs_api rslib_std_roroutine_launch(rs_vm vm, rs_value args, size_t argc)
{
    // rslib_std_roroutine_launch(...)   
    auto* _nvm = RSCO_WorkerPool::get_usable_vm(reinterpret_cast<rs::vmbase*>(vm));
    for (size_t i = 1; i < argc; i++)
    {
        rs_push_valref(reinterpret_cast<rs_vm>(_nvm), args + i);
    }

    rs::shared_pointer<rs::RSCO_Waitter>* gchandle_roroutine = new rs::shared_pointer<rs::RSCO_Waitter>;

    if (RS_INTEGER_TYPE == rs_valuetype(args + 0))
        *gchandle_roroutine = rs::fvmscheduler::new_work(_nvm, rs_int(args + 0), argc - 1);
    else
        *gchandle_roroutine = rs::fvmscheduler::new_work(_nvm, rs_handle(args + 0), argc - 1);

    return rs_ret_gchandle(vm,
        gchandle_roroutine,
        nullptr,
        [](void* gchandle_roroutine_ptr)
        {
            delete (rs::shared_pointer<rs::RSCO_Waitter>*)gchandle_roroutine_ptr;
        });
}

RS_API rs_api rslib_std_roroutine_abort(rs_vm vm, rs_value args, size_t argc)
{
    auto* gchandle_roroutine = (rs::shared_pointer<rs::RSCO_Waitter>*) rs_pointer(args);
    if ((*gchandle_roroutine))
        (*gchandle_roroutine)->abort();

    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_roroutine_completed(rs_vm vm, rs_value args, size_t argc)
{
    auto* gchandle_roroutine = (rs::shared_pointer<rs::RSCO_Waitter>*) rs_pointer(args);
    if ((*gchandle_roroutine))
        return rs_ret_bool(vm, (*gchandle_roroutine)->complete_flag);
    else
        return rs_ret_bool(vm, true);
}

RS_API rs_api rslib_std_roroutine_pause_all(rs_vm vm, rs_value args, size_t argc)
{
    rs_coroutine_pauseall();
    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_roroutine_resume_all(rs_vm vm, rs_value args, size_t argc)
{
    rs_coroutine_resumeall();
    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_roroutine_stop_all(rs_vm vm, rs_value args, size_t argc)
{
    rs_coroutine_stopall();
    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_roroutine_sleep(rs_vm vm, rs_value args, size_t argc)
{
    using namespace std;
    rs::fthread::wait(rs::fvmscheduler::wait(rs_real(args)));

    return rs_ret_nil(vm);
}

RS_API rs_api rslib_std_roroutine_yield(rs_vm vm, rs_value args, size_t argc)
{
    rs::fthread::yield();
    return rs_ret_nil(vm);
}

const char* rs_stdlib_roroutine_src_path = u8"rscene/coroutine.rsn";
const char* rs_stdlib_roroutine_src_data = {
u8R"(
import rscene.basic;

namespace std
{
    using coroutine = gchandle;
    namespace coroutine
    {
        extern("rslib_std_roroutine_launch")
            func create<FT>(var f:FT, ...):coroutine;
        
        extern("rslib_std_roroutine_abort")
            func abort(var co:coroutine):void;

        extern("rslib_std_roroutine_completed")
            func completed(var co:coroutine):bool;

        // Static functions:

        extern("rslib_std_roroutine_pause_all")
            func pause_all():void;

        extern("rslib_std_roroutine_resume_all")
            func resume_all():void;

        extern("rslib_std_roroutine_stop_all")
            func stop_all():void;

        extern("rslib_std_roroutine_sleep")
            func sleep(var time:real):void;

        extern("rslib_std_thread_yield")
                func yield():bool;
    }
}

)" };