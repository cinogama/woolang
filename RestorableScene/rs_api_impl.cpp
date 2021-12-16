// rs_api_impl.cpp
#define _CRT_SECURE_NO_WARNINGS
#include "rs_vm.hpp"
#include "rs_source_file_manager.hpp"
#include "rs_compiler_parser.hpp"
#include "rs_exceptions.hpp"
#include "rs_stdlib.hpp"
#include "rs_lang_grammar_loader.hpp"
#include "rs_lang.hpp"
#include "rs_utf8.hpp"
#include "rs_runtime_debuggee.hpp"

#include <csignal>

// TODO LIST
// 1. ALL GC_UNIT OPERATE SHOULD BE ATOMIC

#define RS_VERSION(DEV,MAIN,SUB,CORRECT) ((0x##DEV##ull)<<(3*16))|((0x##MAIN##ull)<<(2*16))|((0x##SUB##ull)<<(1*16))|((0x##CORRECT##ull)<<(0*16))
#define RS_VERSION_STR(DEV,MAIN,SUB,CORRECT) #DEV "." #MAIN "." #SUB "." #CORRECT "."

#ifdef _DEBUG
#define RS_DEBUG_SFX "debug"
#else
#define RS_DEBUG_SFX ""
#endif

constexpr rs_integer_t version = RS_VERSION(de, 0, 0, 0);
constexpr char         version_str[] = RS_VERSION_STR(de, 0, 0, 0) RS_DEBUG_SFX;

#undef RS_DEBUG_SFX
#undef RS_VERSION_STR
#undef RS_VERSION


#include <atomic>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <string>


void _default_fail_handler(rs_string_t src_file, uint32_t lineno, rs_string_t functionname, uint32_t rterrcode, rs_string_t reason)
{
    std::cerr << ANSI_HIR "RS Runtime happend a failure: "
        << ANSI_HIY << reason << " (E" << std::hex << rterrcode << std::dec << ")" << ANSI_RST << std::endl;
    std::cerr << "\tAt source: \t" << src_file << std::endl;
    std::cerr << "\tAt line: \t" << lineno << std::endl;
    std::cerr << "\tAt function: \t" << functionname << std::endl;
    std::cerr << std::endl;

    std::cerr << ANSI_HIR "callstack: " ANSI_RST << std::endl;

    if (rs::vmbase::_this_thread_vm)
        rs::vmbase::_this_thread_vm->dump_call_stack(32, true, std::cerr);

    std::cerr << std::endl;

    if ((rterrcode & RS_FAIL_TYPE_MASK) == RS_FAIL_MINOR)
    {
        std::cerr << ANSI_HIY "This is a minor failure, ignore it." ANSI_RST << std::endl;
        // Just ignore it..
    }
    else if ((rterrcode & RS_FAIL_TYPE_MASK) == RS_FAIL_MEDIUM)
    {
        // Just throw it..
        std::cerr << ANSI_HIY "This is a medium failure, it will be throw." ANSI_RST << std::endl;
        throw rs::rsruntime_exception(rterrcode, reason);
    }
    else if ((rterrcode & RS_FAIL_TYPE_MASK) == RS_FAIL_HEAVY)
    {
        // Just throw it..
        std::cerr << ANSI_HIY "This is a heavy failure, it will be throw." ANSI_RST << std::endl;
        throw rs::rsruntime_exception(rterrcode, reason);
    }
    else
    {
        std::cerr << "This failure may cause a crash or nothing happens." << std::endl;
        std::cerr << "1) Abort program.(You can attatch debuggee.)" << std::endl;
        std::cerr << "2) Continue.(May cause unknown errors.)" << std::endl;
        std::cerr << "3) Roll back to last RS-EXCEPTION-RECOVERY.(Safe, but may cause memory leak and dead-lock.)" << std::endl;
        std::cerr << "4) Throw exception.(Not exactly safe.)" << std::endl;
        do
        {
            int choice;
            std::cerr << "Please input your choice: " ANSI_HIY;
            std::cin >> choice;
            std::cerr << ANSI_RST;
            switch (choice)
            {
            case 1:
                rs_error(reason);
            case 2:
                return;
            case 3:
                if (rs::vmbase::_this_thread_vm)
                {
                    rs::vmbase::_this_thread_vm->er->set_gcunit_with_barrier(rs::value::valuetype::string_type);
                    rs::string_t::gc_new<rs::gcbase::gctype::eden>(rs::vmbase::_this_thread_vm->er->gcunit, reason);
                    rs::exception_recovery::rollback(rs::vmbase::_this_thread_vm);
                }
                else
                    std::cerr << ANSI_HIR "No virtual machine running in this thread." ANSI_RST << std::endl;

                break;
            case 4:
                throw rs::rsruntime_exception(rterrcode, reason);

                // in debug, if there is no catcher for rs_runtime_error, 
                // the program may continue working.
                // Abort here.
                rs_error(reason);

            default:
                std::cerr << ANSI_HIR "Invalid choice" ANSI_RST << std::endl;
            }

            char _useless_for_clear = 0;
            std::cin.clear();
            while (std::cin.readsome(&_useless_for_clear, 1));

        } while (true);
    }
}
static std::atomic<rs_fail_handler> _rs_fail_handler_function = &_default_fail_handler;

rs_fail_handler rs_regist_fail_handler(rs_fail_handler new_handler)
{
    return _rs_fail_handler_function.exchange(new_handler);
}
void rs_cause_fail(rs_string_t src_file, uint32_t lineno, rs_string_t functionname, uint32_t rterrcode, rs_string_t reason)
{
    _rs_fail_handler_function.load()(src_file, lineno, functionname, rterrcode, reason);
}

void _rs_ctrl_c_signal_handler(int sig)
{
    // CTRL + C, 
    std::cerr << ANSI_HIR "CTRL+C:" ANSI_RST " Pause all virtual-machine by default debuggee immediately." << std::endl;
    
    std::lock_guard g1(rs::vmbase::_alive_vm_list_mx);
    for (auto vm : rs::vmbase::_alive_vm_list)
    {
        if (!rs_has_attached_debuggee((rs_vm)vm))
            rs_attach_default_debuggee((rs_vm)vm);
        rs_break_immediately((rs_vm)vm);
    }

    rs_handle_ctrl_c(_rs_ctrl_c_signal_handler);

}

void rs_handle_ctrl_c(void(*handler)(int))
{
    signal(SIGINT, handler ? handler : _rs_ctrl_c_signal_handler);
}

void rs_init(int argc, char** argv)
{
    rs::rs_init_locale();
    rs::gc::gc_start();
    rs_virtual_source(rs_stdlib_src_path, rs_stdlib_src_data, false);

    rs_handle_ctrl_c(nullptr);
}

rs_string_t  rs_compile_date(void)
{
    return __DATE__ " " __TIME__;
}
rs_string_t  rs_version(void)
{
    return version_str;
}
rs_integer_t rs_version_int(void)
{
    return version;
}

#define RS_VAL(v) (reinterpret_cast<rs::value*>(v)->get())
#define RS_VM(v) (reinterpret_cast<rs::vmbase*>(v))

rs_type rs_valuetype(rs_value value)
{
    auto _rsvalue = RS_VAL(value);

    return (rs_type)_rsvalue->type;
}
rs_integer_t rs_int(rs_value value)
{
    auto _rsvalue = RS_VAL(value);
    if (_rsvalue->type != rs::value::valuetype::integer_type)
    {
        rs_fail(RS_FAIL_TYPE_FAIL, "This value is not an integer.");
        return rs_cast_int(value);
    }
    return _rsvalue->integer;
}
rs_real_t rs_real(rs_value value)
{
    auto _rsvalue = RS_VAL(value);
    if (_rsvalue->type != rs::value::valuetype::real_type)
    {
        rs_fail(RS_FAIL_TYPE_FAIL, "This value is not an real.");
        return rs_cast_real(value);
    }
    return _rsvalue->real;
}
rs_handle_t rs_handle(rs_value value)
{
    auto _rsvalue = RS_VAL(value);
    if (_rsvalue->type != rs::value::valuetype::handle_type)
    {
        rs_fail(RS_FAIL_TYPE_FAIL, "This value is not a handle.");
        return rs_cast_handle(value);
    }
    return _rsvalue->handle;
}
rs_string_t rs_string(rs_value value)
{
    auto _rsvalue = RS_VAL(value);
    if (_rsvalue->type != rs::value::valuetype::string_type)
    {
        rs_fail(RS_FAIL_TYPE_FAIL, "This value is not a string.");
        return rs_cast_string(value);
    }
    rs::gcbase::gc_read_guard rg1(_rsvalue->string);
    return _rsvalue->string->c_str();
}

void rs_set_int(rs_value value, rs_integer_t val)
{
    auto _rsvalue = RS_VAL(value);
    _rsvalue->set_integer(val);
}
void rs_set_real(rs_value value, rs_real_t val)
{
    auto _rsvalue = RS_VAL(value);
    _rsvalue->set_real(val);
}
void rs_set_handle(rs_value value, rs_handle_t val)
{
    auto _rsvalue = RS_VAL(value);
    _rsvalue->set_handle(val);
}
void rs_set_string(rs_value value, rs_string_t val)
{
    auto _rsvalue = RS_VAL(value);
    _rsvalue->set_string(val);
}
void rs_set_val(rs_value value, rs_value val)
{
    auto _rsvalue = RS_VAL(value);
    _rsvalue->set_val(RS_VAL(val));
}
void rs_set_ref(rs_value value, rs_value val)
{
    auto _rsvalue = RS_VAL(value);
    _rsvalue->set_ref(RS_VAL(val));
}

rs_integer_t rs_cast_int(rs_value value)
{
    auto _rsvalue = RS_VAL(value);

    switch (_rsvalue->type)
    {
    case rs::value::valuetype::integer_type:
        return _rsvalue->integer;
    case rs::value::valuetype::handle_type:
        return (rs_integer_t)_rsvalue->handle;
    case rs::value::valuetype::real_type:
        return (rs_integer_t)_rsvalue->real;
    case rs::value::valuetype::string_type:
    {
        rs::gcbase::gc_read_guard rg1(_rsvalue->string);
        return (rs_integer_t)atoll(_rsvalue->string->c_str());
    }
    default:
        rs_fail(RS_FAIL_TYPE_FAIL, "This value can not cast to integer.");
        return 0;
        break;
    }
}
rs_real_t rs_cast_real(rs_value value)
{
    auto _rsvalue = RS_VAL(value);

    switch (reinterpret_cast<rs::value*>(value)->type)
    {
    case rs::value::valuetype::integer_type:
        return (rs_real_t)_rsvalue->integer;
    case rs::value::valuetype::handle_type:
        return (rs_real_t)_rsvalue->handle;
    case rs::value::valuetype::real_type:
        return _rsvalue->real;
    case rs::value::valuetype::string_type:
    {
        rs::gcbase::gc_read_guard rg1(_rsvalue->string);
        return atof(_rsvalue->string->c_str());
    }
    default:
        rs_fail(RS_FAIL_TYPE_FAIL, "This value can not cast to real.");
        return 0;
        break;
    }
}
rs_handle_t rs_cast_handle(rs_value value)
{
    auto _rsvalue = RS_VAL(value);

    switch (reinterpret_cast<rs::value*>(value)->type)
    {
    case rs::value::valuetype::integer_type:
        return (rs_handle_t)_rsvalue->integer;
    case rs::value::valuetype::handle_type:
        return _rsvalue->handle;
    case rs::value::valuetype::real_type:
        return (rs_handle_t)_rsvalue->real;
    case rs::value::valuetype::string_type:
    {
        rs::gcbase::gc_read_guard rg1(_rsvalue->string);
        return (rs_handle_t)atoll(_rsvalue->string->c_str());
    }
    default:
        rs_fail(RS_FAIL_TYPE_FAIL, "This value can not cast to handle.");
        return 0;
        break;
    }
}
void _rs_cast_string(rs::value* value, std::map<rs::gcbase*, int>* traveled_gcunit, bool _fit_layout, std::string* out_str, int depth)
{
    auto _rsvalue = value->get();

    //if (value->type == rs::value::valuetype::is_ref)
    //    *out_str += "<is_ref>";

    switch (_rsvalue->type)
    {
    case rs::value::valuetype::integer_type:
        *out_str += std::to_string(_rsvalue->integer);
        return;
    case rs::value::valuetype::handle_type:
        *out_str += std::to_string(_rsvalue->handle);
        return;
    case rs::value::valuetype::real_type:
        *out_str += std::to_string(_rsvalue->real);
        return;
    case rs::value::valuetype::string_type:
    {
        rs::gcbase::gc_read_guard rg1(_rsvalue->string);
        *out_str += *_rsvalue->string;
        return;
    }
    case rs::value::valuetype::mapping_type:
    {
        if (rs::mapping_t* map = _rsvalue->mapping)
        {
            rs::gcbase::gc_read_guard rg1(_rsvalue->mapping);
            if ((*traveled_gcunit)[map] >= 1)
            {
                _fit_layout = true;
                if ((*traveled_gcunit)[map] >= 2)
                {
                    *out_str += "{ ... }";
                    return;
                }
            }
            (*traveled_gcunit)[map]++;

            *out_str += _fit_layout ? "{" : "{\n";
            for (auto& [v_key, v_val] : *map)
            {
                for (int i = 0; !_fit_layout && i <= depth; i++)
                    *out_str += "    ";
                _rs_cast_string(const_cast<rs::value*>(&v_key), traveled_gcunit, _fit_layout, out_str, depth + 1);
                *out_str += _fit_layout ? ":" : " : ";
                _rs_cast_string(&v_val, traveled_gcunit, _fit_layout, out_str, depth + 1);
                *out_str += _fit_layout ? ", " : ",\n";
            }
            for (int i = 0; !_fit_layout && i < depth; i++)
                *out_str += "    ";
            *out_str += "}";

            (*traveled_gcunit)[map]--;
        }
        else
            *out_str += "nil";
        return;
    }
    case rs::value::valuetype::array_type:
    {
        if (rs::array_t* arr = _rsvalue->array)
        {
            rs::gcbase::gc_read_guard rg1(_rsvalue->array);
            if ((*traveled_gcunit)[arr] >= 1)
            {
                _fit_layout = true;
                if ((*traveled_gcunit)[arr] >= 2)
                {
                    *out_str += "[ ... ]";
                    return;
                }
            }
            (*traveled_gcunit)[arr]++;

            *out_str += _fit_layout ? "[" : "[\n";
            for (auto& v_val : *arr)
            {
                for (int i = 0; !_fit_layout && i <= depth; i++)
                    *out_str += "    ";
                _rs_cast_string(&v_val, traveled_gcunit, _fit_layout, out_str, depth + 1);
                *out_str += _fit_layout ? "," : ",\n";
            }
            for (int i = 0; !_fit_layout && i < depth; i++)
                *out_str += "    ";
            *out_str += "]";

            (*traveled_gcunit)[arr]--;
        }
        else
            *out_str += "nil";
        return;
    }
    case rs::value::valuetype::invalid:
        *out_str += "nil";
        return;
    default:
        rs_fail(RS_FAIL_TYPE_FAIL, "This value can not cast to string.");
        *out_str += "";
        break;
    }
}
rs_string_t rs_cast_string(const rs_value value)
{
    thread_local std::string _buf;
    _buf = "";

    auto _rsvalue = RS_VAL(value);
    switch (_rsvalue->type)
    {
    case rs::value::valuetype::integer_type:
        _buf = std::to_string(_rsvalue->integer);
        return _buf.c_str();
    case rs::value::valuetype::handle_type:
        _buf = std::to_string(_rsvalue->handle);
        return _buf.c_str();
    case rs::value::valuetype::real_type:
        _buf = std::to_string(_rsvalue->real);
        return _buf.c_str();
    case rs::value::valuetype::string_type:
    {
        rs::gcbase::gc_read_guard rg1(_rsvalue->string);
        return _rsvalue->string->c_str();
    }
    case rs::value::valuetype::invalid:
        return "nil";
    }

    std::map<rs::gcbase*, int> _tved_gcunit;
    _rs_cast_string(reinterpret_cast<rs::value*>(value), &_tved_gcunit, false, &_buf, 0);

    return _buf.c_str();
}

rs_string_t rs_type_name(const rs_value value)
{
    auto _rsvalue = RS_VAL(value);
    switch (_rsvalue->type)
    {
    case rs::value::valuetype::integer_type:
        return "int";
    case rs::value::valuetype::handle_type:
        return "handle";
    case rs::value::valuetype::real_type:
        return "real";
    case rs::value::valuetype::string_type:
        return "string";
    case rs::value::valuetype::array_type:
        return "array";
    case rs::value::valuetype::mapping_type:
        return "map";
    case rs::value::valuetype::invalid:
        return "nil";
    }
    return "unknown";
}

rs_integer_t rs_argc(const rs_vm vm)
{
    return reinterpret_cast<const rs::vmbase*>(vm)->tc->integer;
}

rs_result_t rs_ret_int(rs_vm vm, rs_integer_t result)
{
    return reinterpret_cast<rs_result_t>(RS_VM(vm)->cr->set_integer(result));
}
rs_result_t rs_ret_real(rs_vm vm, rs_real_t result)
{
    return reinterpret_cast<rs_result_t>(RS_VM(vm)->cr->set_real(result));
}
rs_result_t rs_ret_handle(rs_vm vm, rs_handle_t result)
{
    return reinterpret_cast<rs_result_t>(RS_VM(vm)->cr->set_handle(result));
}
rs_result_t rs_ret_string(rs_vm vm, rs_string_t result)
{
    return reinterpret_cast<rs_result_t>(RS_VM(vm)->cr->set_string(result));
}
rs_result_t rs_ret_nil(rs_vm vm)
{
    return reinterpret_cast<rs_result_t>(RS_VM(vm)->cr->set_nil());
}
rs_result_t  rs_ret_val(rs_vm vm, rs_value result)
{
    return reinterpret_cast<rs_result_t>(
        RS_VM(vm)->cr->set_val(
            reinterpret_cast<rs::value*>(result)->get()
        ));
}
rs_result_t  rs_ret_ref(rs_vm vm, rs_value result)
{
    return reinterpret_cast<rs_result_t>(
        RS_VM(vm)->cr->set_ref(
            reinterpret_cast<rs::value*>(result)->get()
        ));
}

rs_integer_t rs_lengthof(rs_value value)
{
    auto _rsvalue = RS_VAL(value);
    if (_rsvalue->is_nil())
        return 0;
    if (_rsvalue->type == rs::value::valuetype::array_type)
    {
        rs::gcbase::gc_read_guard rg1(_rsvalue->array);
        return _rsvalue->array->size();
    }
    else if (_rsvalue->type == rs::value::valuetype::mapping_type)
    {
        rs::gcbase::gc_read_guard rg1(_rsvalue->mapping);
        return _rsvalue->mapping->size();
    }
    else if (_rsvalue->type == rs::value::valuetype::string_type)
    {
        rs::gcbase::gc_read_guard rg1(_rsvalue->string);
        return rs::u8strlen(_rsvalue->string->c_str());
    }
    else
    {
        rs_fail(RS_FAIL_TYPE_FAIL, "Only 'string','array' or 'map' can get length.");
        return 0;
    }
}

rs_bool_t rs_virtual_source(rs_string_t filepath, rs_string_t data, rs_bool_t enable_modify)
{
    return rs::create_virtual_source(rs::str_to_wstr(data), rs::str_to_wstr(filepath), enable_modify);
}

rs_vm rs_create_vm()
{
    return (rs_vm)new rs::vm;
}

void rs_close_vm(rs_vm vm)
{
    delete (rs::vmbase*)vm;
}

rs_bool_t rs_load_source(rs_vm vm, const char* virtual_src_path, const char* src)
{
    if (!virtual_src_path)
        virtual_src_path = "__runtime_script__";

    rs_virtual_source(virtual_src_path, src, true);

    // 1. Prepare lexer..
    rs::lexer lex(rs::str_to_wstr(src), virtual_src_path);

    // 2. Lexer will create ast_tree;
    auto result = rs::get_rs_grammar()->gen(lex);
    if (!result)
    {
        // Clean all ast created by this thread..
        rs::grammar::ast_base::clean_this_thread_ast();

        std::string src_file_path = "";
        for (auto& err_info : lex.lex_error_list)
        {
            if (src_file_path != err_info.filename)
                std::cerr << ANSI_HIR "In file: '" ANSI_RST << (src_file_path = err_info.filename) << ANSI_HIR "'" ANSI_RST << std::endl;
            std::wcerr << err_info.to_wstring() << std::endl;
        }
        src_file_path = "";
        for (auto& war_info : lex.lex_warn_list)
        {
            if (src_file_path != war_info.filename)
                std::cerr << ANSI_HIY "In file: '" ANSI_RST << (src_file_path = war_info.filename) << ANSI_HIY "'" ANSI_RST << std::endl;
            std::wcerr << war_info.to_wstring() << std::endl;
        }
        return false;
    }

    // 3. Create lang, most anything store here..
    rs::lang lang(lex);
    lang.analyze_pass1(result);
    lang.analyze_pass2(result);

    // result->display();

    if (lang.has_compile_error())
    {
        // Clean all ast & lang's template things.
        rs::grammar::ast_base::clean_this_thread_ast();

        std::string src_file_path = "";
        for (auto& err_info : lex.lex_error_list)
        {
            if (src_file_path != err_info.filename)
                std::cerr << ANSI_HIR "In file: '" ANSI_RST << (src_file_path = err_info.filename) << ANSI_HIR "'" ANSI_RST << std::endl;
            std::wcerr << err_info.to_wstring() << std::endl;
        }
        src_file_path = "";
        for (auto& war_info : lex.lex_warn_list)
        {
            if (src_file_path != war_info.filename)
                std::cerr << ANSI_HIY "In file: '" ANSI_RST << (src_file_path = war_info.filename) << ANSI_HIY "'" ANSI_RST << std::endl;
            std::wcerr << war_info.to_wstring() << std::endl;
        }


        return false;
    }

    rs::ir_compiler compiler;
    lang.analyze_finalize(result, &compiler);
    if (lang.has_compile_error())
    {
        // Clean all ast & lang's template things.
        rs::grammar::ast_base::clean_this_thread_ast();

        std::string src_file_path = "";
        for (auto& err_info : lex.lex_error_list)
        {
            if (src_file_path != err_info.filename)
                std::cerr << ANSI_HIR "In file: '" ANSI_RST << (src_file_path = err_info.filename) << ANSI_HIR "'" ANSI_RST << std::endl;
            std::wcerr << err_info.to_wstring() << std::endl;
        }
        src_file_path = "";
        for (auto& war_info : lex.lex_warn_list)
        {
            if (src_file_path != war_info.filename)
                std::cerr << ANSI_HIY "In file: '" ANSI_RST << (src_file_path = war_info.filename) << ANSI_HIY "'" ANSI_RST << std::endl;
            std::wcerr << war_info.to_wstring() << std::endl;
        }

        return false;
    }

    compiler.end();
    ((rs::vm*)vm)->set_runtime(compiler);

    return true;
}

rs_value rs_run(rs_vm vm)
{
    if (RS_VM(vm)->env)
    {
        reinterpret_cast<rs::vm*>(vm)->run();
        return reinterpret_cast<rs_value>(RS_VM(vm)->cr);
    }
    return nullptr;
}

// CONTAINER OPERATE

void rs_arr_resize(rs_value arr, rs_int_t newsz)
{
    auto _arr = RS_VAL(arr);
    if (_arr->type == rs::value::valuetype::array_type)
    {
        rs::gcbase::gc_write_guard g1(_arr->array);

        if (_arr->is_nil())
            rs_fail(RS_FAIL_TYPE_FAIL, "Value is 'nil'.");
        else
            _arr->array->resize((size_t)newsz);
    }
    else
        rs_fail(RS_FAIL_TYPE_FAIL, "Value is not a array.");
}

rs_value rs_arr_add(rs_value arr, rs_value elem)
{
    auto _arr = RS_VAL(arr);
    if (_arr->type == rs::value::valuetype::array_type)
    {
        if (_arr->is_nil())
            rs_fail(RS_FAIL_TYPE_FAIL, "Value is 'nil'.");
        else
        {
            rs::gcbase::gc_write_guard g1(_arr->array);

            _arr->array->push_back(*RS_VAL(elem));
            return reinterpret_cast<rs_value>(&_arr->array->back());
        }
    }
    else
        rs_fail(RS_FAIL_TYPE_FAIL, "Value is not a array.");

    return nullptr;
}

rs_bool_t rs_map_find(rs_value map, rs_value index)
{
    auto _map = RS_VAL(map);
    if (_map->type == rs::value::valuetype::mapping_type)
    {
        if (_map->is_nil())
            rs_fail(RS_FAIL_TYPE_FAIL, "Value is 'nil'.");
        else
        {
            rs::gcbase::gc_write_guard g1(_map->mapping);
            return _map->mapping->find(*RS_VAL(index)) != _map->mapping->end();
        }
    }
    else
        rs_fail(RS_FAIL_TYPE_FAIL, "Value is not a map.");

    return false;
}

rs_value rs_map_get(rs_value map, rs_value index)
{
    auto _map = RS_VAL(map);
    if (_map->type == rs::value::valuetype::mapping_type)
    {
        if (_map->is_nil())
            rs_fail(RS_FAIL_TYPE_FAIL, "Value is 'nil'.");
        else
        {
            rs::gcbase::gc_write_guard g1(_map->mapping);
            return reinterpret_cast<rs_value>(&((*_map->mapping)[*RS_VAL(index)]));
        }
    }
    else
        rs_fail(RS_FAIL_TYPE_FAIL, "Value is not a map.");

    return nullptr;
}

// DEBUGGEE TOOLS

void rs_attach_default_debuggee(rs_vm vm)
{
    rs::default_debuggee* dgb = new rs::default_debuggee;
    if (auto* old_debuggee = RS_VM(vm)->attach_debuggee(dgb))
        delete old_debuggee;
}

rs_bool_t rs_has_attached_debuggee(rs_vm vm)
{
    if (RS_VM(vm)->current_debuggee())
        return true;
    return false;
}

void rs_disattach_debuggee(rs_vm vm)
{
    RS_VM(vm)->attach_debuggee(nullptr);
}

void rs_disattach_and_free_debuggee(rs_vm vm)
{
    if (auto* dbg = RS_VM(vm)->attach_debuggee(nullptr))
        delete dbg;
}

void rs_break_immediately(rs_vm vm)
{
    if (auto* debuggee = dynamic_cast<rs::default_debuggee*>(RS_VM(vm)->current_debuggee()))
        debuggee->breakdown_immediately();
    else
        rs_fail(RS_FAIL_DEBUGGEE_FAIL, "'rs_break_immediately' can only break the vm attached default debuggee.");

}