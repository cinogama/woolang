// rs_api_impl.cpp

#include "rs_vm.hpp"


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

#include "rs_exceptions.hpp"

void _default_fail_handler(rs_string_t src_file, uint32_t lineno, rs_string_t functionname, uint32_t rterrcode, rs_string_t reason)
{
    std::cerr << ANSI_HIR "RS Runtime happend a failure: "
        << ANSI_HIY << reason << " (E" << std::hex << rterrcode << std::dec << ")" << ANSI_RST << std::endl;
    std::cerr << "\tAt source: \t" << src_file << std::endl;
    std::cerr << "\tAt line: \t" << lineno << std::endl;
    std::cerr << "\tAt function: \t" << functionname << std::endl;
    std::cerr << std::endl;
    std::cerr << "This failure may cause a crash or nothing happens." << std::endl;
    std::cerr << "1) Abort program.(You can attatch debugee.)" << std::endl;
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
static std::atomic<rs_fail_handler> _rs_fail_handler_function = &_default_fail_handler;

RS_API rs_fail_handler rs_regist_fail_handler(rs_fail_handler new_handler)
{
    return _rs_fail_handler_function.exchange(new_handler);
}
RS_API void rs_cause_fail(rs_string_t src_file, uint32_t lineno, rs_string_t functionname, uint32_t rterrcode, rs_string_t reason)
{
    _rs_fail_handler_function.load()(src_file, lineno, functionname, rterrcode, reason);
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

rs_type rs_valuetype(rs_value value)
{
    auto _rsvalue = reinterpret_cast<rs::value*>(value)->get();

    return (rs_type)_rsvalue->type;
}
rs_integer_t rs_integer(rs_value value)
{
    auto _rsvalue = reinterpret_cast<rs::value*>(value)->get();

    return _rsvalue->integer;
}
rs_real_t rs_real(rs_value value)
{
    auto _rsvalue = reinterpret_cast<rs::value*>(value)->get();

    return _rsvalue->real;
}
rs_handle_t rs_handle(rs_value value)
{
    auto _rsvalue = reinterpret_cast<rs::value*>(value)->get();

    return _rsvalue->handle;
}
rs_string_t rs_string(rs_value value)
{
    auto _rsvalue = reinterpret_cast<rs::value*>(value)->get();

    rs::gcbase::gc_read_guard rg1(_rsvalue->string);
    return _rsvalue->string->c_str();
}

rs_integer_t rs_cast_integer(rs_value value)
{
    auto _rsvalue = reinterpret_cast<rs::value*>(value)->get();

    switch (reinterpret_cast<rs::value*>(value)->type)
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
        rs_fail(RS_ERR_TYPE_FAIL, "This value can not cast to integer.");
        return 0;
        break;
    }
}
rs_real_t rs_cast_real(rs_value value)
{
    auto _rsvalue = reinterpret_cast<rs::value*>(value)->get();

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
        rs_fail(RS_ERR_TYPE_FAIL, "This value can not cast to real.");
        return 0;
        break;
    }
}
rs_handle_t rs_cast_handle(rs_value value)
{
    auto _rsvalue = reinterpret_cast<rs::value*>(value)->get();

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
        rs_fail(RS_ERR_TYPE_FAIL, "This value can not cast to handle.");
        return 0;
        break;
    }
}

void _rs_cast_string(rs::value* value, std::map<rs::gcbase*, int>* traveled_gcunit, bool _fit_layout, std::string* out_str, int depth)
{
    auto _rsvalue = value->get();

    if (value->type == rs::value::valuetype::is_ref)
        *out_str += "<is_ref>";

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
        rs_fail(RS_ERR_TYPE_FAIL, "This value can not cast to string.");
        *out_str += "";
        break;
    }
}

rs_string_t rs_cast_string(const rs_value value)
{
    thread_local std::string _buf;

    _buf = "";

    std::map<rs::gcbase*, int> _tved_gcunit;
    _rs_cast_string(reinterpret_cast<rs::value*>(value), &_tved_gcunit, false, &_buf, 0);

    return _buf.c_str();
}

rs_value* rs_args(rs_vm vm)
{
    return (rs_value*)(reinterpret_cast<rs::vmbase*>(vm)->sp);
}
rs_integer_t rs_argc(rs_vm vm)
{
    return reinterpret_cast<rs::vmbase*>(vm)->tc->integer;
}