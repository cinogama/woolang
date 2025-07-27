// wo_basic_type.cpp
#include "wo_afx.hpp"

namespace wo
{
    value* value::set_takeplace()
    {
        type = valuetype::stack_externed_flag;
        handle = 0;
        return this;
    }
    void value::set_string(const std::string& str)
    {
        set_gcunit<wo::value::valuetype::string_type>(
            string_t::gc_new<gcbase::gctype::young>(str));
    }
    void value::set_buffer(const void* buf, size_t sz)
    {
        set_gcunit<wo::value::valuetype::string_type>(
            string_t::gc_new<gcbase::gctype::young>((const char*)buf, sz));
    }
    void value::set_string_nogc(std::string_view str)
    {
        // You must reset the no-gc flag manually.
        set_gcunit<wo::value::valuetype::string_type>(
            string_t::gc_new<gcbase::gctype::no_gc>(str));
    }
    void value::set_val_with_compile_time_check(const value* val)
    {
        if (val->is_gcunit())
        {
            auto* attrib = val->fast_get_attrib_for_assert_check();
            wo_assert(attrib != nullptr);
            wo_assert(attrib->m_nogc != 0);
        }
        set_val(val);
    }
    void value::set_integer(wo_integer_t val)
    {
        type = valuetype::integer_type;
        integer = val;
    }
    void value::set_real(wo_real_t val)
    {
        type = valuetype::real_type;
        real = val;
    }
    void value::set_handle(wo_handle_t val)
    {
        type = valuetype::handle_type;
        handle = val;
    }
    void value::set_nil()
    {
        type = valuetype::invalid;
        handle = 0;
    }
    void value::set_bool(bool val)
    {
        type = valuetype::bool_type;
        integer = val ? 1 : 0;
    }
    void value::set_native_callstack(const wo::byte_t* ipplace)
    {
        type = valuetype::nativecallstack;
        native_function_addr = ipplace;
    }
    void value::set_callstack(uint32_t ip, uint32_t bp)
    {
        type = valuetype::callstack;
        vmcallstack.ret_ip = ip;
        vmcallstack.bp = bp;
    }

    // ATTENTION: Only work for gc-work-thread & no_gc unit. gc-unit might be freed
    //          after get_gcunit_and_attrib_ref.
    gcbase* value::get_gcunit_and_attrib_ref(gcbase::unit_attrib** attrib) const
    {
        if (type & valuetype::need_gc_flag)
            return std::launder(reinterpret_cast<gcbase*>(
                womem_verify(gcunit, std::launder(reinterpret_cast<womem_attrib_t**>(attrib)))));
        return nullptr;
    }

    // ATTENTION: Only work for gc-work-thread & no_gc unit. gc-unit might be freed
    //          after get_gcunit_and_attrib_ref.
    gcbase::unit_attrib* value::fast_get_attrib_for_assert_check() const
    {
        gcbase::unit_attrib* r;
        if (nullptr != get_gcunit_and_attrib_ref(&r))
            return r;
        return nullptr;
    }
    bool value::is_gcunit() const
    {
        return type & valuetype::need_gc_flag;
    }
    void value::set_val(const value* _val)
    {
        type = _val->type;
        handle = _val->handle;
    }

    std::string value::get_type_name() const
    {
        switch (type)
        {
        case valuetype::integer_type:
            return "int";
        case valuetype::real_type:
            return "real";
        case valuetype::handle_type:
            return "handle";
        case valuetype::string_type:
            return "string";
        case valuetype::array_type:
            return "array";
        case valuetype::dict_type:
            return "directory";
        case valuetype::invalid:
            return "nil";
        default:
            wo_fail(WO_FAIL_TYPE_FAIL, "Unknown type name.");
            return "unknown";
        }
    }

    void value::set_dup(value* from)
    {
        if (from->type == valuetype::array_type)
        {
            auto* dup_arrray = from->array;
            wo_assert(dup_arrray != nullptr);

            gcbase::gc_read_guard g1(dup_arrray);

            set_gcunit<valuetype::array_type>(
                array_t::gc_new<gcbase::gctype::young>(
                    *dup_arrray));
        }
        else if (from->type == valuetype::dict_type)
        {
            auto* dup_mapping = from->directory;
            wo_assert(dup_mapping != nullptr);

            gcbase::gc_read_guard g1(dup_mapping);

            set_gcunit<valuetype::dict_type>(
                directory_t::gc_new<gcbase::gctype::young>(
                    *dup_mapping));
        }
        else if (from->type == valuetype::struct_type)
        {
            auto* dup_struct = from->structure;
            wo_assert(dup_struct != nullptr);

            auto* maked_struct =
                structure_t::gc_new<gcbase::gctype::young>(
                    dup_struct->m_count);

            gcbase::gc_read_guard g1(dup_struct);
            memcpy(
                maked_struct->m_values,
                dup_struct->m_values,
                sizeof(value) * static_cast<size_t>(dup_struct->m_count));

            set_gcunit<valuetype::struct_type>(maked_struct);
        }
        else
            set_val(from);
    }
    void value::set_struct_nogc(uint16_t sz)
    {
        // You must reset the no-gc flag manually.
        set_gcunit<wo::value::valuetype::struct_type>(
            structure_t::gc_new<gcbase::gctype::no_gc>(sz));
    }

    bool value_ptr_compare::operator()(const value* lhs, const value* rhs) const
    {
        if (lhs->type == rhs->type)
        {
            if (lhs->type == value::valuetype::string_type)
                return *lhs->string < *rhs->string;

            return lhs->handle < rhs->handle;
        }
        return lhs->type < rhs->type;
    }
    bool value_equal::operator()(const value& lhs, const value& rhs) const
    {
        if (lhs.type == rhs.type)
        {
            if (lhs.type == value::valuetype::string_type)
                return *lhs.string == *rhs.string;

            return lhs.handle == rhs.handle;
        }
        return lhs.type == rhs.type && lhs.handle == rhs.handle;
    }
    size_t value_hasher::operator()(const value& val) const
    {
        if (val.type == value::valuetype::string_type)
            return std::hash<std::string>()(*val.string);

        return (size_t)val.handle;
    }

    structure_base_t::structure_base_t(uint16_t sz) noexcept
        : m_count(sz)
    {
        m_values = (value*)malloc(sz * sizeof(value));
    }
    structure_base_t::~structure_base_t()
    {
        wo_assert(m_values);
        free(m_values);
    }

    closure_base_t::closure_base_t(wo_integer_t vmfunc, uint16_t argc) noexcept
        : m_native_call(false)
        , m_vm_func(vmfunc)
        , m_closure_args_count(argc)
    {
        m_closure_args = (value*)malloc(argc * sizeof(value));
    }
    closure_base_t::closure_base_t(wo_native_func_t nfunc, uint16_t argc) noexcept
        : m_native_call(true)
        , m_native_func(nfunc)
        , m_closure_args_count(argc)
    {
        m_closure_args = (value*)malloc(argc * sizeof(value));
    }
    closure_base_t::~closure_base_t()
    {
        wo_assert(m_closure_args);
        free(m_closure_args);
    }

    void gchandle_base_t::set_custom_mark_callback(gcmark_func_t callback)
    {
        static_assert(sizeof(intptr_t) >= sizeof(gcmark_func_t));

        m_custom_marker.m_is_callback = true;
#ifdef WO_PLATFORM_64
        m_custom_marker.m_marker63 = reinterpret_cast<intptr_t>(callback);
#else
        m_custom_marker.m_marker31 = reinterpret_cast<intptr_t>(callback);
#endif
    }
    void gchandle_base_t::set_custom_mark_unit(gcbase* unit_may_null)
    {
        static_assert(sizeof(intptr_t) >= sizeof(gcbase*));

        m_custom_marker.m_is_callback = false;
#ifdef WO_PLATFORM_64
        m_custom_marker.m_marker63 = reinterpret_cast<intptr_t>(unit_may_null);
#else
        m_custom_marker.m_marker31 = reinterpret_cast<intptr_t>(unit_may_null);
#endif
    }
    gchandle_base_t::~gchandle_base_t()
    {
        do_close();
    }

    void gchandle_base_t::dec_destructable_instance_count()
    {
        wo_assert(m_hold_counter != nullptr);
#if WO_ENABLE_RUNTIME_CHECK
        size_t old_count =
#endif
            m_hold_counter->fetch_sub(1, std::memory_order::relaxed);
        wo_assert(old_count > 0);
    }
}