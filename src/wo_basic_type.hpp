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
    struct value_compare
    {
        bool operator()(const value& lhs, const value& rhs) const;
    };

    using byte_t = uint8_t;
    using hash_t = uint64_t;

    using string_t = gcunit<std::string>;
    using dict_t = gcunit<std::map<value, value, value_compare>>;
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
        enum class valuetype : uint8_t
        {
            invalid = 0x0,

            integer_type,
            real_type,
            handle_type,
            bool_type,

            callstack,
            nativecallstack,

            need_gc = 0xF0,
            string_type,
            dict_type,
            array_type,
            gchandle_type,
            closure_type,
            struct_type,
        };

        struct callstack
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
                     
            callstack vmcallstack;
            const byte_t* native_function_addr;

            // std::atomic<gcbase*> atomic_gcunit_ptr;
            uint64_t value_space;
        };

        union
        {
            valuetype type;
            uint64_t type_space;
        };

        inline value* set_string(const char* str)
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
        inline value* set_string_nogc(const char* str)
        {
            // You must 'delete' it manual
            set_gcunit<wo::value::valuetype::string_type>(new string_t(str));
            return this;
        }
        inline value* set_val_compile_time(value* val)
        {
            if (val->type == valuetype::string_type)
                return set_string_nogc(val->string->c_str());

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

        template<valuetype ty, typename T>
        inline value* set_gcunit(T* unit)
        {
            static_assert((uint8_t)ty & (uint8_t)valuetype::need_gc);
            
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
        inline gcbase* get_gcunit_with_barrier(gcbase::unit_attrib** attrib) const
        {
            if ((uint8_t)type & (uint8_t)valuetype::need_gc)
                return std::launder(reinterpret_cast<gcbase*>(womem_verify(gcunit,
                    std::launder(reinterpret_cast<womem_attrib_t**>(attrib)))));
            return nullptr;
        }
        inline bool is_gcunit() const
        {
            return (uint8_t)type & (uint8_t)valuetype::need_gc;
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
    };
    static_assert(sizeof(value) == 16);
    static_assert(sizeof(std::atomic<gcbase*>) == sizeof(gcbase*));
    static_assert(std::atomic<gcbase*>::is_always_lock_free);
    static_assert(sizeof(std::atomic<byte_t>) == sizeof(byte_t));
    static_assert(std::atomic<byte_t>::is_always_lock_free);
    using wo_extern_native_func_t = wo_native_func;

    inline bool value_compare::operator()(const value& lhs, const value& rhs) const
    {
        if (lhs.type == rhs.type)
        {
            switch (lhs.type)
            {
            case value::valuetype::invalid:
            case value::valuetype::bool_type:
            case value::valuetype::integer_type:
                return lhs.integer < rhs.integer;
            case value::valuetype::real_type:
                return lhs.real < rhs.real;
            case value::valuetype::handle_type:
                return lhs.handle < rhs.handle;
            case value::valuetype::string_type:
                return (*lhs.string) < (*rhs.string);
            case value::valuetype::array_type:
            case value::valuetype::dict_type:
            case value::valuetype::gchandle_type:
            case value::valuetype::closure_type:
            case value::valuetype::struct_type:
                return ((intptr_t)lhs.gcunit) < ((intptr_t)rhs.gcunit);
            default:
                wo_fail(WO_FAIL_TYPE_FAIL, "Values of this type cannot be compared.");
                return false;
            }
        }
        return lhs.type < rhs.type;
    }
    static_assert((int)value::valuetype::invalid == WO_INVALID_TYPE);
    static_assert((int)value::valuetype::integer_type == WO_INTEGER_TYPE);
    static_assert((int)value::valuetype::real_type == WO_REAL_TYPE);
    static_assert((int)value::valuetype::handle_type == WO_HANDLE_TYPE);
    static_assert((int)value::valuetype::bool_type == WO_BOOL_TYPE);
    static_assert((int)value::valuetype::callstack == WO_CALLSTACK_TYPE);
    static_assert((int)value::valuetype::nativecallstack == WO_NATIVE_CALLSTACK_TYPE);
    static_assert((int)value::valuetype::need_gc == WO_NEED_GC_FLAG);
    static_assert((int)value::valuetype::string_type == WO_STRING_TYPE);
    static_assert((int)value::valuetype::dict_type == WO_MAPPING_TYPE);
    static_assert((int)value::valuetype::array_type == WO_ARRAY_TYPE);
    static_assert((int)value::valuetype::gchandle_type == WO_GCHANDLE_TYPE);
    static_assert((int)value::valuetype::closure_type == WO_CLOSURE_TYPE);
    static_assert((int)value::valuetype::struct_type == WO_STRUCT_TYPE);
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
            for (uint16_t i = 0; i < sz; ++i)
                m_values[i].set_nil();
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
            wo_native_func m_native_func;
        };
        value* m_closure_args;

        closure_function(uint16_t sz) noexcept
            : m_closure_args_count(sz)
        {
            m_closure_args = (value*)malloc(sz * sizeof(value));
            for (uint16_t i = 0; i < sz; ++i)
                m_closure_args[i].set_nil();
        }
        ~closure_function()
        {
            wo_assert(m_closure_args);
            free(m_closure_args);
        }
    };

    struct gc_handle_base_t
    {
        using destructor_func_t = void(*)(void*);

        gcbase*             m_holding_gcbase;
        void*               m_holding_handle;
        destructor_func_t   m_destructor;
        // ATTENTION: Only used for decrease destructable count in env of vm;
        wo_vm               m_gc_vm;

        bool close();

        ~gc_handle_base_t()
        {
            close();
        }
    };

    inline value* value::set_dup(value* from)
    {
        if (from->type == valuetype::array_type)
        {
            auto* dup_arrray = from->array;
            if (dup_arrray)
            {
                set_gcunit<valuetype::array_type>(
                    array_t::gc_new<gcbase::gctype::young>(dup_arrray->size()));

                gcbase::gc_read_guard g1(dup_arrray);
                *array->elem() = *dup_arrray->elem();
            }
            else
                set_nil();

        }
        else if (from->type == valuetype::dict_type)
        {
            auto* dup_mapping = from->dict;
            if (dup_mapping)
            {
                set_gcunit<valuetype::dict_type>(
                    dict_t::gc_new<gcbase::gctype::young>());

                gcbase::gc_read_guard g1(dup_mapping);
                *dict->elem() = *dup_mapping->elem();
            }
            else
                set_nil();
        }
        else if (from->type == valuetype::struct_type)
        {
            auto* dup_struct = from->structs;
            if (dup_struct)
            {
                set_gcunit<valuetype::struct_type>(
                    struct_t::gc_new<gcbase::gctype::young>(dup_struct->m_count));

                gcbase::gc_read_guard g1(dup_struct);
                for (uint16_t i = 0; i < dup_struct->m_count; ++i)
                    structs->m_values[i].set_val(&dup_struct->m_values[i]);
            }
            else
                set_nil();
        }
        else
            set_val(from);
        return this;
    }
}
