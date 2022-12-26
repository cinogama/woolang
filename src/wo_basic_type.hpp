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

            struct
            {
                uint32_t bp;
                uint32_t ret_ip;
            };

            const byte_t* native_function_addr;

            // std::atomic<gcbase*> atomic_gcunit_ptr;
            uint64_t value_space;
        };

        union
        {

            valuetype type;
            // uint32_t type_hash;

            // std::atomic_uint64_t atomic_type;
            uint64_t type_space;
        };

        inline value* set_gcunit_with_barrier(valuetype gcunit_type)
        {
            *std::launder(reinterpret_cast<std::atomic<wo_handle_t*>*>(&handle)) = nullptr;
            *std::launder(reinterpret_cast<std::atomic_uint8_t*>(&type)) = (uint8_t)gcunit_type;

            return this;
        }

        inline value* set_gcunit_with_barrier(valuetype gcunit_type, gcbase* gcunit_ptr)
        {
            *std::launder(reinterpret_cast<std::atomic<wo_handle_t*>*>(&handle)) = nullptr;
            *std::launder(reinterpret_cast<std::atomic_uint8_t*>(&type)) = (uint8_t)gcunit_type;
            *std::launder(reinterpret_cast<std::atomic<gcbase*>*>(&gcunit)) = gcunit_ptr;

            return this;
        }

        inline value* set_string(const char* str)
        {
            set_gcunit_with_barrier(valuetype::string_type);
            string_t::gc_new<gcbase::gctype::eden>(gcunit, str);
            return this;
        }
        inline value* set_string_nogc(const char* str)
        {
            // You must 'delete' it manual
            set_gcunit_with_barrier(valuetype::string_type);
            string_t::gc_new<gcbase::gctype::no_gc>(gcunit, str);
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
        inline value* set_native_callstack(const wo::byte_t* ipplace)
        {
            type = valuetype::nativecallstack;
            native_function_addr = ipplace;
            return this;
        }
        inline bool is_nil() const
        {
            return type == valuetype::invalid || (is_gcunit() && gcunit == nullptr);
        }

        inline gcbase* get_gcunit_with_barrier() const
        {
            do
            {
                gcbase* gcunit_addr = *std::launder(reinterpret_cast<const std::atomic<gcbase*>*>(&gcunit));
                if (*std::launder(reinterpret_cast<const std::atomic_uint8_t*>(&type)) & (uint8_t)valuetype::need_gc)
                {
                    if (gcunit_addr == *std::launder(reinterpret_cast<const std::atomic<gcbase*>*>(&gcunit)))
                        return gcunit_addr;

                    continue;
                }

                return nullptr;

            } while (true);
        }

        inline value* get_val_atomically(value* out_put_val) const
        {
            do
            {
                wo_handle_t data = *std::launder(reinterpret_cast<const std::atomic<wo_handle_t>*>(&handle));
                valuetype type = *std::launder(reinterpret_cast<const std::atomic<valuetype>*>(&type));
                if (data == *std::launder(reinterpret_cast<const std::atomic<wo_handle_t>*>(&handle)))
                {
                    out_put_val->handle = data;
                    out_put_val->type = type;
                    return out_put_val;
                }

            } while (true);
        }
        inline value get_val_atomically() const
        {
            value tmp;
            return *get_val_atomically(&tmp);
        }

        inline bool is_gcunit() const
        {
            return (uint8_t)type & (uint8_t)valuetype::need_gc;
        }

        inline value* set_val(const value* _val)
        {
            // PARALLEL GC FIX: Value to be set need check is gcunit, too;
            if (_val->is_gcunit())
                set_gcunit_with_barrier(_val->type, _val->gcunit);
            else
            {
                type = _val->type;
                handle = _val->handle;
            }

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
            case value::valuetype::integer_type:
                return lhs.integer < rhs.integer;
            case value::valuetype::real_type:
                return lhs.real < rhs.real;
            case value::valuetype::handle_type:
                return lhs.handle < rhs.handle;
            case value::valuetype::string_type:
                return (*lhs.string) < (*rhs.string);
            case value::valuetype::gchandle_type:
                return ((intptr_t)lhs.gchandle) < ((intptr_t)rhs.gchandle);
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
        uint16_t m_closure_args_count;
        union
        {
            wo_integer_t m_vm_func;
            wo_native_func m_native_func;
        };
        // TODO: Optimize, donot use vector to store args
        std::vector<value> m_closure_args;
    };

    struct gc_handle_base_t
    {
        gc_handle_base_t* last = nullptr;

        value holding_value = {};
        void* holding_handle = nullptr;
        void(*destructor)(void*) = nullptr;

        std::atomic_flag has_been_closed_af = {};
        bool has_been_closed = false;

        bool close()
        {
            if (!has_been_closed_af.test_and_set())
            {
                has_been_closed = true;
                if (destructor)
                    destructor(holding_handle);
                //if (auto* unit = holding_value.get_gcunit_with_barrier())
                //    unit->gc_type = gcbase::gctype::young;
                return true;
            }
            return false;
        }

        ~gc_handle_base_t()
        {
            close();
        }
    };

    inline value* value::set_dup(value* from)
    {
        // TODO: IF A VAL HAS IT REF; IN CONST VAL, WILL STILL HAS POSSIBLE TO MODIFY THE VAL;
        if (from->type == valuetype::array_type)
        {
            auto* dup_arrray = from->array;
            if (dup_arrray)
            {
                gcbase::gc_read_guard g1(dup_arrray);
                set_gcunit_with_barrier(valuetype::array_type);

                auto* created_arr = array_t::gc_new<gcbase::gctype::eden>(gcunit, dup_arrray->size());
                *created_arr = *dup_arrray;
            }
            else
                set_nil();

        }
        else if (from->type == valuetype::dict_type)
        {
            auto* dup_mapping = from->dict;
            if (dup_mapping)
            {
                gcbase::gc_read_guard g1(dup_mapping);
                set_gcunit_with_barrier(valuetype::dict_type);

                auto* created_map = dict_t::gc_new<gcbase::gctype::eden>(gcunit);
                *created_map = *dup_mapping;
            }
            else
                set_nil();
        }
        else if (from->type == valuetype::struct_type)
        {
            auto* dup_struct = from->structs;
            if (dup_struct)
            {
                gcbase::gc_read_guard g1(dup_struct);
                set_gcunit_with_barrier(valuetype::struct_type);

                auto* created_struct = struct_t::gc_new<gcbase::gctype::eden>(gcunit, dup_struct->m_count);
                for (uint16_t i = 0; i < dup_struct->m_count; ++i)
                    created_struct->m_values[i].set_val(&dup_struct->m_values[i]);
            }
            else
                set_nil();
        }
        else
            set_val(from);

        return this;
    }
}
