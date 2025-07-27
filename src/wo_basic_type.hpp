#pragma once

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

    template<typename ... TS>
    using cxx_vec_t = std::vector<TS...>;
    template<typename ... TS>
    using cxx_set_t = std::set<TS...>;
    template<typename ... TS>
    using cxx_map_t = std::map<TS...>;

    using string_base_t = std::string;
    using directory_base_t = std::unordered_map<value, value, value_hasher, value_equal>;
    using array_base_t = std::vector<value>;
    struct gchandle_base_t;
    struct closure_base_t;
    struct structure_base_t;

    using string_t = gcunit<string_base_t>;
    using directory_t = gcunit<directory_base_t>;
    using array_t = gcunit<array_base_t>;
    using gchandle_t = gcunit<gchandle_base_t>;
    using closure_t = gcunit<closure_base_t>;
    using structure_t = gcunit<structure_base_t>;

    struct value
    {
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
            directory_t* directory;
            gchandle_t* gchandle;
            closure_t* closure;
            structure_t* structure;

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

        void set_string(const std::string& str);
        void set_buffer(const void* buf, size_t sz);
        void set_string_nogc(std::string_view str);
        void set_struct_nogc(uint16_t sz);
        void set_val_with_compile_time_check(const value* val);
        void set_integer(wo_integer_t val);
        void set_real(wo_real_t val);
        void set_handle(wo_handle_t val);
        void set_nil();
        void set_bool(bool val);
        void set_native_callstack(const wo::byte_t* ipplace);
        void set_callstack(uint32_t ip, uint32_t bp);
        template<valuetype ty, typename T>
        void set_gcunit(T* unit)
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
                directory = unit;
            else if constexpr (ty == valuetype::gchandle_type)
                gchandle = unit;
            else if constexpr (ty == valuetype::closure_type)
                closure = unit;
            else if constexpr (ty == valuetype::struct_type)
                structure = unit;
        }

        // ATTENTION: Only work for gc-work-thread & no_gc unit. gc-unit might be freed
        //          after get_gcunit_and_attrib_ref.
        gcbase* get_gcunit_and_attrib_ref(gcbase::unit_attrib** attrib) const;

        // ATTENTION: Only work for gc-work-thread & no_gc unit. gc-unit might be freed
        //          after get_gcunit_and_attrib_ref.
        gcbase::unit_attrib* fast_get_attrib_for_assert_check() const;
        bool is_gcunit() const;
        void set_val(const value* _val);
        std::string get_type_name() const;
        void set_dup(value* from);

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
    static_assert(value::valuetype::invalid == 0);

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
    struct closure_base_t
    {
        bool m_native_call;
        const uint16_t m_closure_args_count;
        union
        {
            wo_integer_t m_vm_func;
            wo_native_func_t m_native_func;
        };
        value* m_closure_args;

        closure_base_t(const closure_base_t&) = delete;
        closure_base_t(closure_base_t&&) = delete;
        closure_base_t& operator=(const closure_base_t&) = delete;
        closure_base_t& operator=(closure_base_t&&) = delete;

        closure_base_t(wo_integer_t vmfunc, uint16_t argc) noexcept;
        closure_base_t(wo_native_func_t nfunc, uint16_t argc) noexcept;
        ~closure_base_t();
    };
    struct gchandle_base_t
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

        ~gchandle_base_t();
    };
}
