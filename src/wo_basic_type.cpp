// wo_basic_type.cpp
#include "wo_afx.hpp"

namespace wo
{
    value* value::set_string_nogc(std::string_view str)
    {
        // You must reset the no-gc flag manually.
        set_gcunit(string_t::gc_new<gcbase::gctype::no_gc>(str));
        return this;
    }
    value* value::set_val_with_compile_time_check(const value* val)
    {
        auto* attrib = val->fast_get_attrib_for_assert_check();
        wo_test(attrib != nullptr && attrib->m_nogc != 0);

        return set_val(val);
    }

    // ATTENTION: Only work for gc-work-thread & no_gc unit. gc-unit might be freed
    //          after get_gcunit_and_attrib_ref.
    gcbase* value::get_gcunit_and_attrib_ref(gc::unit_attrib** attrib) const
    {
        return std::launder(reinterpret_cast<gcbase*>(
            womem_verify(m_gcunit, std::launder(reinterpret_cast<womem_attrib_t**>(attrib)))));
    }

    // ATTENTION: Only work for gc-work-thread & no_gc unit. gc-unit might be freed
    //          after get_gcunit_and_attrib_ref.
    gc::unit_attrib* value::fast_get_attrib_for_assert_check() const
    {
        gc::unit_attrib* r;
        if (nullptr != get_gcunit_and_attrib_ref(&r))
            return r;
        return nullptr;
    }

    value* value::set_struct_nogc(uint16_t sz)
    {
        // You must reset the no-gc flag manually.
        set_gcunit(structure_t::gc_new<gcbase::gctype::no_gc>(sz));
        return this;
    }
    bool dynamic_base_equal::operator()(
        const dynamic_base_t& lhs, const dynamic_base_t& rhs) const
    {
        if (lhs.m_type == rhs.m_type)
        {
            if (lhs.m_type == WO_STRING_TYPE)
                return *lhs.m_value.m_string == *rhs.m_value.m_string;

            return lhs.m_value.m_value_field == rhs.m_value.m_value_field;
        }
        return false;
    }
    size_t dynamic_base_hasher::operator()(const dynamic_base_t& val) const
    {
        if (val.m_type == WO_STRING_TYPE)
            return std::hash<std::string>()(*val.m_value.m_string);

        return static_cast<size_t>(val.m_value.m_handle);
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

    closure_bast_t::closure_bast_t(const byte_t* vmfunc, uint16_t argc) noexcept
        : m_native_call(false)
        , m_closure_args_count(argc)
        , m_vm_func(vmfunc)
    {
        m_closure_args = (value*)malloc(argc * sizeof(value));
    }
    closure_bast_t::closure_bast_t(wo_native_func_t nfunc, uint16_t argc) noexcept
        : m_native_call(true)
        , m_closure_args_count(argc)
        , m_native_func(nfunc)
    {
        m_closure_args = (value*)malloc(argc * sizeof(value));
    }
    closure_bast_t::~closure_bast_t()
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
        m_custom_marker.m_marker32 = reinterpret_cast<intptr_t>(callback);
#endif
    }
    void gchandle_base_t::set_custom_mark_unit(gcbase* unit_may_null)
    {
        static_assert(sizeof(intptr_t) >= sizeof(gcbase*));

        m_custom_marker.m_is_callback = false;
#ifdef WO_PLATFORM_64
        m_custom_marker.m_marker63 = reinterpret_cast<intptr_t>(unit_may_null);
#else
        m_custom_marker.m_marker32 = reinterpret_cast<intptr_t>(unit_may_null);
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
            m_hold_counter->fetch_sub(1, std::memory_order::memory_order_relaxed);
        wo_assert(old_count > 0);
    }
    }