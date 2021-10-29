#pragma once

#define RS_IMPL
#include "rs.h"

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <map>
#include <set>
#include <vector>

#include "rs_gc.hpp"
#include "rs_assert.hpp"

namespace rs
{
    struct value;

    using byte_t = uint8_t;
    using real_t = double;
    using hash_t = uint64_t;
    using string_t = gcunit<std::string>;
    using mapping_t = gcunit<std::unordered_map<hash_t, value*>>;

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

        };

        union
        {
            rs_real_t      real;
            rs_integer_t   integer;
            rs_handle_t    handle;

            gcbase* gcunit;
            string_t* string;     // ADD-ABLE TYPE
            mapping_t* mapping;

            struct
            {
                uint32_t bp;
                uint32_t ret_ip;
            };

            value* ref;

            std::atomic<gcbase*> atomic_gcunit_ptr;
        };

        union
        {
            valuetype type;

            std::atomic_uint8_t atomic_type;
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
            atomic_gcunit_ptr = nullptr;
            atomic_type = (uint8_t)gcunit_type;
        }

        inline void set_gcunit_with_barrier(valuetype gcunit_type, gcbase* gcunit_ptr)
        {
            atomic_gcunit_ptr = nullptr;
            atomic_type = (uint8_t)gcunit_type;
            atomic_gcunit_ptr = gcunit_ptr;
        }

        inline gcbase* get_gcunit_with_barrier()
        {
            do
            {
                gcbase* gcunit_addr = atomic_gcunit_ptr;
                if (atomic_type.load() & (uint8_t)valuetype::need_gc)
                {
                    if (gcunit_addr == atomic_gcunit_ptr)
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
            return this;
        }

        inline value* set_val(value* _val)
        {
            type = _val->type;
            handle = _val->handle;

            return this;
        }
    };
    static_assert(sizeof(value) == 16);
    static_assert(sizeof(std::atomic<gcbase*>) == sizeof(gcbase*));
    static_assert(std::atomic<gcbase*>::is_always_lock_free);
    using native_func_t = rs_native_func;
}