#pragma once

#define WO_IMPL
#include "wo.h"

#include "wo_gc.hpp"
#include "wo_assert.hpp"

#include <cstdint>
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

        inline value* set_takeplace()
        {
            type = valuetype::stack_externed_flag;
            handle = 0;
            return this;
        }
        inline value* set_string(const std::string& str)
        {
            set_gcunit<wo::value::valuetype::string_type>(
                string_t::gc_new<gcbase::gctype::young>(str));
            return this;
        }
        inline value* set_buffer(const void* buf, size_t sz)
        {
            set_gcunit<wo::value::valuetype::string_type>(
                string_t::gc_new<gcbase::gctype::young>((const char*)buf, sz));
            return this;
        }
        inline value* set_string_nogc(const std::string& str)
        {
            // You must 'delete' it manual
            set_gcunit<wo::value::valuetype::string_type>(new string_t(str));
            return this;
        }
        inline value* set_val_compile_time(const value* val)
        {
            if (val->type == valuetype::string_type)
                return set_string_nogc(*val->string);

            wo_assert(!val->is_gcunit());
            return set_val(val);
        }
        inline value* set_integer(wo_integer_t val)
        {
            type = valuetype::integer_type;
            integer = val;
            return this;
        }
        inline value* set_real(wo_real_t val)
        {
            type = valuetype::real_type;
            real = val;
            return this;
        }
        inline value* set_handle(wo_handle_t val)
        {
            type = valuetype::handle_type;
            handle = val;
            return this;
        }
        inline value* set_nil()
        {
            type = valuetype::invalid;
            handle = 0;
            return this;
        }
        inline value* set_bool(bool val)
        {
            type = valuetype::bool_type;
            integer = val ? 1 : 0;
            return this;
        }
        inline value* set_native_callstack(const wo::byte_t* ipplace)
        {
            type = valuetype::nativecallstack;
            native_function_addr = ipplace;
            return this;
        }
        inline value* set_callstack(uint32_t ip, uint32_t bp)
        {
            type = valuetype::callstack;
            vmcallstack.ret_ip = ip;
            vmcallstack.bp = bp;
            return this;
        }
        template<valuetype ty, typename T>
        inline value* set_gcunit(T* unit)
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
        // ATTENTION: Only work for gc-work-thread. gc-unit might be freed
        //          after get_gcunit_and_attrib_ref.
        inline gcbase* get_gcunit_and_attrib_ref(gcbase::unit_attrib** attrib) const
        {
            if (type & valuetype::need_gc_flag)
                return std::launder(reinterpret_cast<gcbase*>(
                    womem_verify(gcunit, std::launder(reinterpret_cast<womem_attrib_t**>(attrib)))));
            return nullptr;
        }
        inline gcbase::unit_attrib* fast_get_attrib_for_assert_check() const
        {
            gcbase::unit_attrib* r;
            if (nullptr != get_gcunit_and_attrib_ref(&r))
                return r;
            return nullptr;
        }
        inline bool is_gcunit() const
        {
            return type & valuetype::need_gc_flag;
        }
        inline value* set_val(const value* _val)
        {
            type = _val->type;
            handle = _val->handle;
            return this;
        }

        inline std::string get_type_name() const
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
                return "dict";
            case valuetype::invalid:
                return "nil";
            default:
                wo_fail(WO_FAIL_TYPE_FAIL, "Unknown type name.");
                return "unknown";
            }
        }

        inline value* set_dup(value* from);

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

    inline bool value_ptr_compare::operator()(const value* lhs, const value* rhs) const
    {
        if (lhs->type == rhs->type)
        {
            if (lhs->type == value::valuetype::string_type)
                return *lhs->string < *rhs->string;

            return lhs->handle < rhs->handle;
        }
        return lhs->type < rhs->type;
    }
    inline bool value_equal::operator()(const value& lhs, const value& rhs) const
    {
        if (lhs.type == rhs.type)
        {
            if (lhs.type == value::valuetype::string_type)
                return *lhs.string == *rhs.string;

            return lhs.handle == rhs.handle;
        }
        return lhs.type == rhs.type && lhs.handle == rhs.handle;
    }
    inline size_t value_hasher::operator()(const value& val) const
    {
        if (val.type == value::valuetype::string_type)
            return std::hash<std::string>()(*val.string);

        return (size_t)val.handle;
    }

    // optional_value used for store 1 value with a id(used for select or else)
    struct struct_values
    {
        value* m_values;
        const uint16_t m_count;

        struct_values(const struct_values&) = delete;
        struct_values(struct_values&&) = delete;
        struct_values& operator=(const struct_values&) = delete;
        struct_values& operator=(struct_values&&) = delete;

        struct_values(uint16_t sz) noexcept
            : m_count(sz)
        {
            m_values = (value*)malloc(sz * sizeof(value));
        }
        ~struct_values()
        {
            wo_assert(m_values);
            free(m_values);
        }
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

        closure_function(wo_integer_t vmfunc, uint16_t argc) noexcept
            : m_native_call(false)
            , m_vm_func(vmfunc)
            , m_closure_args_count(argc)
        {
            m_closure_args = (value*)malloc(argc * sizeof(value));
        }
        closure_function(wo_native_func_t nfunc, uint16_t argc) noexcept
            : m_native_call(true)
            , m_native_func(nfunc)
            , m_closure_args_count(argc)
        {
            m_closure_args = (value*)malloc(argc * sizeof(value));
        }
        ~closure_function()
        {
            wo_assert(m_closure_args);
            free(m_closure_args);
        }
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
        wo_vm                   m_gc_vm;
        custom_marker           m_custom_marker;

        void set_custom_mark_callback(gcmark_func_t callback)
        {
            static_assert(sizeof(intptr_t) >= sizeof(gcmark_func_t));

            m_custom_marker.m_is_callback = true;
#ifdef WO_PLATFORM_64
            m_custom_marker.m_marker63 = reinterpret_cast<intptr_t>(callback);
#else
            m_custom_marker.m_marker31 = reinterpret_cast<intptr_t>(callback);
#endif
        }
        void set_custom_mark_unit(gcbase* unit_may_null)
        {
            static_assert(sizeof(intptr_t) >= sizeof(gcbase*));

            m_custom_marker.m_is_callback = false;
#ifdef WO_PLATFORM_64
            m_custom_marker.m_marker63 = reinterpret_cast<intptr_t>(unit_may_null);
#else
            m_custom_marker.m_marker31 = reinterpret_cast<intptr_t>(unit_may_null);
#endif
        }

        bool do_close();
        void do_custom_mark(wo_gc_work_context_t ctx);

        ~gc_handle_base_t()
        {
            do_close();
        }
    };

    inline value* value::set_dup(value* from)
    {
        if (from->type == valuetype::array_type)
        {
            auto* dup_arrray = from->array;
            wo_assert(dup_arrray != nullptr);

            set_gcunit<valuetype::array_type>(
                array_t::gc_new<gcbase::gctype::young>(dup_arrray->size()));

            gcbase::gc_read_guard g1(dup_arrray);
            *array->elem() = *dup_arrray->elem();

        }
        else if (from->type == valuetype::dict_type)
        {
            auto* dup_mapping = from->dict;
            wo_assert(dup_mapping != nullptr);

            set_gcunit<valuetype::dict_type>(
                dict_t::gc_new<gcbase::gctype::young>());

            gcbase::gc_read_guard g1(dup_mapping);
            *dict->elem() = *dup_mapping->elem();
        }
        else if (from->type == valuetype::struct_type)
        {
            auto* dup_struct = from->structs;
            wo_assert(dup_struct != nullptr);

            set_gcunit<valuetype::struct_type>(
                struct_t::gc_new<gcbase::gctype::young>(dup_struct->m_count));

            gcbase::gc_read_guard g1(dup_struct);
            for (uint16_t i = 0; i < dup_struct->m_count; ++i)
                structs->m_values[i].set_val(&dup_struct->m_values[i]);
        }
        else
            set_val(from);
        return this;
    }
}
