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
    struct value;
    struct value_equal
    {
        bool operator()(const value& lhs, const value& rhs) const;
    };
    struct value_hasher
    {
        size_t operator()(const value& val) const;
    };
    struct value_ptr_compare
    {
        bool operator()(const value* lhs, const value* rhs) const;
    };

    using byte_t = uint8_t;
    using hash_t = uint64_t;

    using string_t = gcunit<std::string>;
    using dict_t = gcunit<std::unordered_map<value, value, value_hasher, value_equal>>;
    using array_t = gcunit<std::vector<value>>;

    template<typename ... TS>
    using cxx_vec_t = std::vector<TS...>;
    template<typename ... TS>
    using cxx_set_t = std::set<TS...>;
    template<typename ... TS>
    using cxx_map_t = std::map<TS...>;

    struct gc_handle_base_t;
    struct closure_function;
    struct struct_values;

    using gchandle_t = gcunit<gc_handle_base_t>;
    using closure_t = gcunit<closure_function>;
    using struct_t = gcunit<struct_values>;

    struct value
    {
        //  value
        /*
        *
        */
        enum valuetype : uint8_t
        {
            invalid = WO_INVALID_TYPE,

            need_gc_flag = WO_NEED_GC_FLAG,
            stack_externed_flag = WO_STACK_EXTERNED_FLAG,

            integer_type = WO_INTEGER_TYPE,
            real_type = WO_REAL_TYPE,
            handle_type = WO_HANDLE_TYPE,
            bool_type = WO_BOOL_TYPE,

            callstack = WO_CALLSTACK_TYPE,
            nativecallstack = WO_NATIVE_CALLSTACK_TYPE,

            string_type = WO_STRING_TYPE,
            dict_type = WO_MAPPING_TYPE,
            array_type = WO_ARRAY_TYPE,
            gchandle_type = WO_GCHANDLE_TYPE,
            closure_type = WO_CLOSURE_TYPE,
            struct_type = WO_STRUCT_TYPE,
        };

        struct callstack_t
        {
            uint32_t bp;
            uint32_t ret_ip;
        };

        union
        {
            wo_real_t      real;
            wo_integer_t   integer;
            wo_handle_t    handle;

            gcbase* gcunit;
            string_t* string;     // ADD-ABLE TYPE
            array_t* array;
            dict_t* dict;
            gchandle_t* gchandle;
            closure_t* closure;
            struct_t* structs;

            callstack_t vmcallstack;
            const byte_t* native_function_addr;

            // std::atomic<gcbase*> atomic_gcunit_ptr;
            uint64_t value_space;
        };
        union
        {
            valuetype type;
            uint64_t type_space;
        };

        value* set_takeplace();
        value* set_string(const std::string& str);
        value* set_buffer(const void* buf, size_t sz);
        value* set_string_nogc(std::string_view str);
        value* set_struct_nogc(uint16_t sz);
        value* set_val_with_compile_time_check(const value* val);
        value* set_integer(wo_integer_t val);
        value* set_real(wo_real_t val);
        value* set_handle(wo_handle_t val);
        value* set_nil();
        value* set_bool(bool val);
        value* set_native_callstack(const wo::byte_t* ipplace);
        value* set_callstack(uint32_t ip, uint32_t bp);
        template<valuetype ty, typename T>
        value* set_gcunit(T* unit)
        {
            static_assert(ty & valuetype::need_gc_flag);

            type = ty;
            if constexpr (sizeof(gcbase*) < sizeof(wo_handle_t))
                handle = 0;

            if constexpr (ty == valuetype::string_type)
                string = unit;
            else if constexpr (ty == valuetype::array_type)
                array = unit;
            else if constexpr (ty == valuetype::dict_type)
                dict = unit;
            else if constexpr (ty == valuetype::gchandle_type)
                gchandle = unit;
            else if constexpr (ty == valuetype::closure_type)
                closure = unit;
            else if constexpr (ty == valuetype::struct_type)
                structs = unit;

            return this;
        }

        // ATTENTION: Only work for gc-work-thread & no_gc unit. gc-unit might be freed
        //          after get_gcunit_and_attrib_ref.
        gcbase* get_gcunit_and_attrib_ref(gcbase::unit_attrib** attrib) const;

        // ATTENTION: Only work for gc-work-thread & no_gc unit. gc-unit might be freed
        //          after get_gcunit_and_attrib_ref.
        gcbase::unit_attrib* fast_get_attrib_for_assert_check() const;
        bool is_gcunit() const;
        value* set_val(const value* _val);
        std::string get_type_name() const;
        value* set_dup(value* from);

        // Used for storing key-value when deserilizing a map.
        static const value TAKEPLACE;
    };
    static_assert(sizeof(value) == 16);
    static_assert(alignof(value) == 8);
    static_assert(alignof(value) == alignof(_wo_value));
    static_assert(sizeof(std::atomic<gcbase*>) == sizeof(gcbase*));
    static_assert(std::atomic<gcbase*>::is_always_lock_free);
    static_assert(sizeof(std::atomic<byte_t>) == sizeof(byte_t));
    static_assert(std::atomic<byte_t>::is_always_lock_free);
    static_assert(std::is_standard_layout_v<value>);

    using wo_extern_native_func_t = wo_native_func_t;

    struct struct_values
    {
        value* m_values;
        const uint16_t m_count;

        struct_values(const struct_values&) = delete;
        struct_values(struct_values&&) = delete;
        struct_values& operator=(const struct_values&) = delete;
        struct_values& operator=(struct_values&&) = delete;

        struct_values(uint16_t sz) noexcept;
        ~struct_values();
    };
    struct closure_function
    {
        bool m_native_call;
        const uint16_t m_closure_args_count;
        union
        {
            wo_integer_t m_vm_func;
            wo_native_func_t m_native_func;
        };
        value* m_closure_args;

        closure_function(const closure_function&) = delete;
        closure_function(closure_function&&) = delete;
        closure_function& operator=(const closure_function&) = delete;
        closure_function& operator=(closure_function&&) = delete;

        closure_function(wo_integer_t vmfunc, uint16_t argc) noexcept;
        closure_function(wo_native_func_t nfunc, uint16_t argc) noexcept;
        ~closure_function();
    };
    struct gc_handle_base_t
    {
        using destructor_func_t = wo_gchandle_close_func_t;
        using gcmark_func_t = wo_gcstruct_mark_func_t;

        void* m_holding_handle;
        destructor_func_t       m_destructor;

        struct custom_marker
        {
            intptr_t            m_is_callback : 1;

#ifdef WO_PLATFORM_64
            static_assert(sizeof(intptr_t) == sizeof(int64_t));
            intptr_t            m_marker63 : 63;
#else
            static_assert(sizeof(intptr_t) == sizeof(int32_t));
            intptr_t            m_marker31 : 31;
#endif
        };

        // ATTENTION: Only used for decrease destructable count in env of vm;
        std::atomic_size_t*     m_hold_counter;
        custom_marker           m_custom_marker;

        void set_custom_mark_callback(gcmark_func_t callback);
        void set_custom_mark_unit(gcbase* unit_may_null);

        bool do_close();
        void do_custom_mark(wo_gc_work_context_t ctx);
        void dec_destructable_instance_count();

        ~gc_handle_base_t();
    };
}
