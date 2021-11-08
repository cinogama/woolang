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

            need_gc = 0xF0,

            string_type,
            mapping_type,
            array_type,

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

            struct
            {
                uint32_t bp;
                uint32_t ret_ip;
            };

            value* ref;

            // std::atomic<gcbase*> atomic_gcunit_ptr;
        };

        union
        {
            valuetype type;

            // std::atomic_uint8_t atomic_type;
        };

        inline value* get()
        {
            if (type == valuetype::is_ref)
            {
                rs_assert(ref && ref->type != valuetype::is_ref,
                    "illegal reflect, 'ref' only able to store ONE layer of reflect, and should not be nullptr.");
                return ref;
            }
            return this;
        }

        inline void set_gcunit_with_barrier(valuetype gcunit_type)
        {
            *reinterpret_cast<std::atomic<gcbase*>*>(&gcunit) = nullptr;
            *reinterpret_cast<std::atomic_uint8_t*>(&type) = (uint8_t)gcunit_type;
        }

        inline void set_gcunit_with_barrier(valuetype gcunit_type, gcbase* gcunit_ptr)
        {
            *reinterpret_cast<std::atomic<gcbase*>*>(&gcunit) = nullptr;
            *reinterpret_cast<std::atomic_uint8_t*>(&type) = (uint8_t)gcunit_type;
            *reinterpret_cast<std::atomic<gcbase*>*>(&gcunit) = gcunit_ptr;
        }

        inline void set_string(const char* str)
        {
            set_gcunit_with_barrier(valuetype::string_type);
            string_t::gc_new<gcbase::gctype::eden>(gcunit, str);
        }
        inline void set_string_nogc(const char* str)
        {
            // You must 'delete' it manual
            set_gcunit_with_barrier(valuetype::string_type);
            string_t::gc_new<gcbase::gctype::no_gc>(gcunit, str);
        }
        inline void set_integer(rs_integer_t val)
        {
            type = valuetype::integer_type;
            integer = val;
        }
        inline void set_real(rs_real_t val)
        {
            type = valuetype::integer_type;
            real = val;
        }
        inline void set_handle(rs_handle_t val)
        {
            type = valuetype::handle_type;
            handle = val;
        }
        inline void set_nil()
        {
            type = valuetype::invalid;
            handle = 0;
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
        inline bool is_gcunit() const
        {
            return (uint8_t)type & (uint8_t)valuetype::need_gc;
        }

        inline value* set_val(const value* _val)
        {
            if (_val->is_gcunit())
                set_gcunit_with_barrier(_val->type, _val->gcunit);
            else
            {
                type = _val->type;
                handle = _val->handle;
            }

            return this;
        }
    };
    static_assert(sizeof(value) == 16);
    static_assert(sizeof(std::atomic<gcbase*>) == sizeof(gcbase*));
    static_assert(std::atomic<gcbase*>::is_always_lock_free);
    using native_func_t = rs_native_func;
}