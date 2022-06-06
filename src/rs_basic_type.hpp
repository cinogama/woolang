#pragma once

#define RS_IMPL
#include "rs.h"

#include "rs_gc.hpp"
#include "rs_assert.hpp"


#include <cstdint>
#include <vector>
#include <unordered_map>
#include <map>
#include <set>
#include <vector>

namespace rs
{
    struct value;
    struct value_compare
    {
        bool operator()(const value& lhs, const value& rhs) const;
    };

    using byte_t = uint8_t;
    using hash_t = uint64_t;

    using string_t = gcunit<std::string>;
    using mapping_t = gcunit<std::map<value, value, value_compare>>;
    using array_t = gcunit<std::vector<value>>;

    template<typename ... TS>
    using cxx_vec_t = std::vector<TS...>;
    template<typename ... TS>
    using cxx_set_t = std::set<TS...>;
    template<typename ... TS>
    using cxx_map_t = std::map<TS...>;

    struct gc_handle_base_t;
    struct closure_function;

    using gchandle_t = gcunit<gc_handle_base_t>;
    using closure_t = gcunit<closure_function>;

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

            is_ref,
            callstack,
            nativecallstack,

            need_gc = 0xF0,

            string_type,
            mapping_type,
            array_type,
            gchandle_type,
            closure_type,

        };

        union
        {
            rs_real_t      real;
            rs_integer_t   integer;
            rs_handle_t    handle;

            gcbase* gcunit;
            string_t* string;     // ADD-ABLE TYPE
            array_t* array;
            mapping_t* mapping;
            gchandle_t* gchandle;
            closure_t* closure;

            struct
            {
                uint32_t bp;
                uint32_t ret_ip;
            };
            const byte_t* native_function_addr;

            value* ref;

            // std::atomic<gcbase*> atomic_gcunit_ptr;
        };

        union
        {
            valuetype type;
            // uint32_t type_hash;

            // std::atomic_uint64_t atomic_type;
        };

        inline value* get() const
        {
            if (type == valuetype::is_ref)
            {
                rs_assert(ref && ref->type != valuetype::is_ref,
                    "illegal reflect, 'ref' only able to store ONE layer of reflect, and should not be nullptr.");
                return ref;
            }
            return const_cast<value*>(this);
        }

        inline value* set_gcunit_with_barrier(valuetype gcunit_type)
        {
            *reinterpret_cast<std::atomic<rs_handle_t*>*>(&handle) = nullptr;
            *reinterpret_cast<std::atomic_uint8_t*>(&type) = (uint8_t)gcunit_type;

            return this;
        }

        inline value* set_gcunit_with_barrier(valuetype gcunit_type, gcbase* gcunit_ptr)
        {
            *reinterpret_cast<std::atomic<rs_handle_t*>*>(&handle) = nullptr;
            *reinterpret_cast<std::atomic_uint8_t*>(&type) = (uint8_t)gcunit_type;
            *reinterpret_cast<std::atomic<gcbase*>*>(&gcunit) = gcunit_ptr;

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
        inline value* set_integer(rs_integer_t val)
        {
            type = valuetype::integer_type;
            integer = val;
            return this;
        }
        inline value* set_real(rs_real_t val)
        {
            type = valuetype::real_type;
            real = val;
            return this;
        }
        inline value* set_handle(rs_handle_t val)
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
        inline value* set_native_callstack(const rs::byte_t* ipplace)
        {
            type = valuetype::nativecallstack;
            native_function_addr = ipplace;
            return this;
        }
        inline bool is_nil() const
        {
            return type == valuetype::invalid || (is_gcunit() && gcunit == nullptr);
        }
        inline bool is_ref() const
        {
            return type == valuetype::is_ref;
        }

        inline gcbase* get_gcunit_with_barrier() const
        {
            do
            {
                gcbase* gcunit_addr = *reinterpret_cast<const std::atomic<gcbase*>*>(&gcunit);
                if (*reinterpret_cast<const std::atomic_uint8_t*>(&type) & (uint8_t)valuetype::need_gc)
                {
                    if (gcunit_addr == *reinterpret_cast<const std::atomic<gcbase*>*>(&gcunit))
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
                rs_handle_t data = *reinterpret_cast<const std::atomic<rs_handle_t>*>(&handle);
                valuetype type = *reinterpret_cast<const std::atomic<valuetype>*>(&type);
                if (data == *reinterpret_cast<const std::atomic<rs_handle_t>*>(&handle))
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

        inline value* set_ref(value* _ref)
        {
            if (_ref != this)
            {
                rs_assert(_ref && _ref->type != valuetype::is_ref,
                    "illegal reflect, 'ref' only able to store ONE layer of reflect, and should not be nullptr.");

                type = valuetype::is_ref;
                ref = _ref;
            }
            return _ref;
        }

        inline value* set_trans(value* _ref)
        {
            if (_ref->is_ref())
                return set_ref(_ref->get());
            return set_val(_ref);
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
            case valuetype::mapping_type:
                return "map";
            case valuetype::invalid:
                return "nil";
            case valuetype::is_ref:
                return get()->get_type_name();
            default:
                rs_fail(RS_FAIL_TYPE_FAIL, "Unknown type name.");
                return "unknown";
            }
        }

        inline value* set_dup(value* from)
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
                    for (auto src_iter = dup_arrray->begin(), dst_iter = created_arr->begin();
                        dst_iter != created_arr->end();
                        src_iter++, dst_iter++
                        )
                    {
                        dst_iter->set_dup(&*src_iter);
                    }
                }
                else
                    set_nil();

            }
            else if (from->type == valuetype::mapping_type)
            {
                auto* dup_mapping = from->mapping;
                if (dup_mapping)
                {
                    gcbase::gc_read_guard g1(dup_mapping);
                    set_gcunit_with_barrier(valuetype::mapping_type);

                    auto* created_map = mapping_t::gc_new<gcbase::gctype::eden>(gcunit);
                    for (auto& [key, val] : *dup_mapping)
                        (*created_map)[key].set_dup(&val);
                }
                else
                    set_nil();
            }
            else
            {
                set_val(from);
            }

            return this;
        }
    };
    static_assert(sizeof(value) == 16);
    static_assert(sizeof(std::atomic<gcbase*>) == sizeof(gcbase*));
    static_assert(std::atomic<gcbase*>::is_always_lock_free);
    static_assert(sizeof(std::atomic<byte_t>) == sizeof(byte_t));
    static_assert(std::atomic<byte_t>::is_always_lock_free);
    using rs_extern_native_func_t = rs_native_func;

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
            default:
                rs_fail(RS_FAIL_TYPE_FAIL, "Values of this type cannot be compared.");
                return false;
            }
        }
        return lhs.type < rhs.type;
    }
    static_assert((int)value::valuetype::invalid == RS_INVALID_TYPE);
    static_assert((int)value::valuetype::integer_type == RS_INTEGER_TYPE);
    static_assert((int)value::valuetype::real_type == RS_REAL_TYPE);
    static_assert((int)value::valuetype::handle_type == RS_HANDLE_TYPE);
    static_assert((int)value::valuetype::is_ref == RS_IS_REF);
    static_assert((int)value::valuetype::callstack == RS_CALLSTACK_TYPE);
    static_assert((int)value::valuetype::nativecallstack == RS_NATIVE_CALLSTACK_TYPE);
    static_assert((int)value::valuetype::need_gc == RS_NEED_GC_FLAG);
    static_assert((int)value::valuetype::string_type == RS_STRING_TYPE);
    static_assert((int)value::valuetype::mapping_type == RS_MAPPING_TYPE);
    static_assert((int)value::valuetype::array_type == RS_ARRAY_TYPE);
    static_assert((int)value::valuetype::gchandle_type == RS_GCHANDLE_TYPE);

    struct closure_function
    {
        rs_integer_t m_function_addr;
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
                if (holding_value.is_gcunit())
                    holding_value.gcunit->gc_type = gcbase::gctype::eden;
                return true;
            }
            return false;
        }

        ~gc_handle_base_t()
        {
            close();
        }
    };
}
