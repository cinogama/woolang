#pragma once

#define WO_IMPL
#include "wo.h"

#include "wo_gc.hpp"
#include "wo_assert.hpp"

#include <cstdint>
#include <cstring>
#include <vector>
#include <unordered_map>
#include <map>
#include <set>
#include <vector>
#include <new>

namespace wo
{
    union value;

    using hash_t = uint64_t;

    struct gchandle_base_t;
    struct closure_bast_t;
    struct structure_base_t;
    struct dynamic_base_t;
    struct dynamic_base_equal
    {
        bool operator()(const dynamic_base_t& lhs, const dynamic_base_t& rhs) const;
    };
    struct dynamic_base_hasher
    {
        size_t operator()(const dynamic_base_t& val) const;
    };
    using dictionary_base_t =
        std::unordered_map<
        dynamic_base_t,
        value,
        dynamic_base_hasher,
        dynamic_base_equal>;

    using string_t = gcunit<std::string>;
    using dictionary_t = gcunit<dictionary_base_t>;
    using array_t = gcunit<std::vector<value>>;
    using gchandle_t = gcunit<gchandle_base_t>;
    using closure_t = gcunit<closure_bast_t>;
    using structure_t = gcunit<structure_base_t>;
    using dynamic_t = gcunit<dynamic_base_t>;

    enum callway : uintptr_t
    {
        CLOSURE = 0, // Only used in function type.
                     // ATTENTION: CLOSURE must be 0 to make GC can scan the value easier.
                     NEAR,
                     SCRIPT = NEAR,
                     FAR,         // Only used in callstack type.
                     NATIVE,
    };

    union WO_DECLARE_ALIGNAS(8) value
    {
        struct invoke_target_t
        {
#ifdef WO_PLATFORM_64
            static_assert(sizeof(uintptr_t) == sizeof(uint64_t));
            uintptr_t m_masked_invoke_target64;
#define WO_VALUE_INVOKE_TARGET(VAL)\
    static_cast<uintptr_t>(\
        VAL.m_function.m_masked_invoke_target64 & static_cast<uintptr_t>(0x3fffffffffffffff))
#define WO_VALUE_INVOKE_KIND(VAL)\
    static_cast<wo::callway>(\
        VAL.m_function.m_masked_invoke_target64 >> static_cast<uintptr_t>(62))
#define WO_VALUE_MAKE_MASK_TARGET64(WAY, PTR)\
    static_cast<uintptr_t>(\
        reinterpret_cast<uintptr_t>(PTR) | (\
            static_cast<uintptr_t>(WAY) << static_cast<uintptr_t>(62)))
#else
            static_assert(sizeof(uintptr_t) == sizeof(uint32_t));
            uintptr_t m_invoke_target32;
            callway m_invoke_way32;
#define WO_VALUE_INVOKE_TARGET(VAL) (VAL.m_function.m_invoke_target32)
#define WO_VALUE_INVOKE_KIND(VAL) (VAL.m_function.m_invoke_way32)
#endif
        };
        struct callstack_from_ip_t
        {
#ifdef WO_PLATFORM_64
            static_assert(sizeof(uintptr_t) == sizeof(uint64_t));
            callway m_invoke_way : 2;
            uintptr_t m_ret_abs_ip : 62;
#else
            static_assert(sizeof(uintptr_t) == sizeof(uint32_t));
            callway m_invoke_way;
            uintptr_t m_ret_abs_ip;
#endif
        };

        wo_real_t m_real;
        wo_integer_t m_integer;
        wo_handle_t m_handle;
        gcbase* m_gcunit;
        string_t* m_string;
        array_t* m_array;
        dictionary_t* m_dictionary;
        gchandle_t* m_gchandle;
        structure_t* m_structure;
        dynamic_t* m_dynamic;
        invoke_target_t m_function;
        callstack_from_ip_t m_callstack;

        uint64_t m_value_field;

        WO_FORCE_INLINE value* set_gcunit(gcbase * unit)
        {
            if constexpr (sizeof(gcbase*) < sizeof(wo_handle_t))
                m_value_field = 0;

            m_gcunit = unit;
            return this;
        }
        WO_FORCE_INLINE value* set_string(std::string_view str)
        {
            set_gcunit(string_t::gc_new<gcbase::gctype::young>(str));
        }
        WO_FORCE_INLINE value* set_buffer(const void* buf, size_t sz)
        {
            set_gcunit(string_t::gc_new<gcbase::gctype::young>((const char*)buf, sz));
            return this;
        }
        value* set_string_nogc(std::string_view str);
        value* set_struct_nogc(uint16_t sz);
        value* set_val_with_compile_time_check(const value * val);
        WO_FORCE_INLINE value* set_script_func(const byte_t * val)
        {
#ifdef WO_PLATFORM_64
            m_function.m_masked_invoke_target64 =
                WO_VALUE_MAKE_MASK_TARGET64(callway::SCRIPT, val);
#else
            m_function.m_invoke_target32 = reinterpret_cast<uintptr_t>(val);
            m_function.m_invoke_way32 = callway::SCRIPT;
#endif
            return this;
        }
        WO_FORCE_INLINE value* set_native_func(wo_native_func_t val)
        {
#ifdef WO_PLATFORM_64
            m_function.m_masked_invoke_target64 =
                WO_VALUE_MAKE_MASK_TARGET64(callway::NATIVE, val);
#else
            m_function.m_invoke_target32 = reinterpret_cast<uintptr_t>(val);
            m_function.m_invoke_way32 = callway::NATIVE;
#endif
            return this;
        }
        WO_FORCE_INLINE value* set_integer(wo_integer_t val)
        {
            m_integer = val;
            return this;
        }
        WO_FORCE_INLINE value* set_real(wo_real_t val)
        {
            m_real = val;
            return this;
        }
        WO_FORCE_INLINE value* set_handle(wo_handle_t val)
        {
            m_handle = val;
            return this;
        }
        WO_FORCE_INLINE value* set_nil()
        {
            m_value_field = 0;
            return this;
        }
        WO_FORCE_INLINE value* set_bool(bool val)
        {
            m_integer = val ? 1 : 0;
            return this;
        }

        // ATTENTION: Only work for gc-work-thread & no_gc unit. gc-unit might be freed
        //          after get_gcunit_and_attrib_ref.
        gcbase* get_gcunit_and_attrib_ref(gc::unit_attrib * *attrib) const;

        // ATTENTION: Only work for gc-work-thread & no_gc unit. gc-unit might be freed
        //          after get_gcunit_and_attrib_ref.
        gc::unit_attrib* fast_get_attrib_for_assert_check() const;
        WO_FORCE_INLINE value* set_val(const value * _val)
        {
            m_value_field = _val->m_value_field;
            return this;
        }
        WO_FORCE_INLINE static void write_barrier(const value * val)
        {
            gc::unit_attrib* attr;
            if (auto* unit = val->get_gcunit_and_attrib_ref(&attr))
            {
                if (attr->m_marked == (uint8_t)gcbase::gcmarkcolor::no_mark)
                    gc::m_memo_mark_gray_list.add_one(new gc::memo_unit{ unit, attr });
            }
        }

        // Used for storing key-value when deserilizing a map.
        static const value TAKEPLACE;
    };
    static_assert(sizeof(value) == 8);
    static_assert(alignof(value) == 8);
    static_assert(sizeof(value) == sizeof(_wo_value));
    static_assert(alignof(value) == alignof(_wo_value));
    static_assert(offsetof(value, m_gcunit) == 0);
    static_assert(offsetof(value, m_function) == 0);
#ifdef WO_PLATFORM_64
    // Make sure GC can scan correct place.
    static_assert(callway::CLOSURE == 0);
#else
    static_assert(offsetof(invoke_target_t, m_invoke_target32) == 0);
#endif
    static_assert(sizeof(std::atomic<gcbase*>) == sizeof(gcbase*));
    static_assert(std::atomic<gcbase*>::is_always_lock_free);
    static_assert(sizeof(std::atomic<byte_t>) == sizeof(byte_t));
    static_assert(std::atomic<byte_t>::is_always_lock_free);
    static_assert(std::is_standard_layout_v<value>);

    using wo_extern_native_func_t = wo_native_func_t;

    struct structure_base_t
    {
        value* m_values;
        const uint16_t m_count;

        structure_base_t(const structure_base_t&) = delete;
        structure_base_t(structure_base_t&&) = delete;
        structure_base_t& operator=(const structure_base_t&) = delete;
        structure_base_t& operator=(structure_base_t&&) = delete;

        structure_base_t(uint16_t sz) noexcept;
        ~structure_base_t();
    };
    struct closure_bast_t
    {
        bool m_native_call;
        const uint16_t m_closure_args_count;
        union
        {
            const byte_t* m_vm_func;
            wo_native_func_t m_native_func;
        };
        value* m_closure_args;

        closure_bast_t(const closure_bast_t&) = delete;
        closure_bast_t(closure_bast_t&&) = delete;
        closure_bast_t& operator=(const closure_bast_t&) = delete;
        closure_bast_t& operator=(closure_bast_t&&) = delete;

        closure_bast_t(const byte_t* vmfunc, uint16_t argc) noexcept;
        closure_bast_t(wo_native_func_t nfunc, uint16_t argc) noexcept;
        ~closure_bast_t();
    };
    struct gchandle_base_t
    {
        using destructor_func_t = wo_gchandle_close_func_t;
        using gcmark_func_t = wo_gcstruct_mark_func_t;

        void* m_holding_handle;
        destructor_func_t m_destructor;

        struct custom_marker
        {
#ifdef WO_PLATFORM_64
            static_assert(sizeof(intptr_t) == sizeof(int64_t));
            intptr_t m_is_callback : 1;
            intptr_t m_marker63 : 63;
#else
            static_assert(sizeof(intptr_t) == sizeof(int32_t));
            intptr_t m_is_callback;
            intptr_t m_marker32;
#endif
        };

        // ATTENTION: Only used for decrease destructable count in env of vm;
        std::atomic_size_t* m_hold_counter;
        custom_marker m_custom_marker;

        void set_custom_mark_callback(gcmark_func_t callback);
        void set_custom_mark_unit(gcbase* unit_may_null);

        bool do_close();
        void do_custom_mark(wo_gc_work_context_t ctx);
        void dec_destructable_instance_count();

        ~gchandle_base_t();
    };
    struct dynamic_base_t
    {
        wo_type_t m_type;
        value m_value;

        dynamic_base_t(wo_type_t type, value val)
            : m_type(type)
            , m_value(val)
        {
        }

        dynamic_base_t(const dynamic_base_t&) = delete;
        dynamic_base_t(dynamic_base_t&&) = delete;
        dynamic_base_t& operator=(const dynamic_base_t&) = delete;
        dynamic_base_t& operator=(dynamic_base_t&&) = delete;
    };
}
