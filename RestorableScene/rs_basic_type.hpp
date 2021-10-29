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

        union _rs_value_data_seg_type
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
            uint64_t _data_seg;
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

            std::atomic_uint64_t _rs_value_data_seg;
        };

        union _rs_value_type_seg_type
        {
            valuetype type;
            uint64_t _type_seg;
        };

        union
        {
            valuetype type;

            std::atomic_uint64_t _rs_value_type_seg;
        };

        inline void atomic_read_value(_rs_value_data_seg_type* out_data_seg, _rs_value_type_seg_type* out_type_seg) const
        {
            do
            {
                out_data_seg->_data_seg = _rs_value_data_seg;
                out_type_seg->_type_seg = _rs_value_type_seg;
            } while (out_data_seg->_data_seg != _rs_value_data_seg);
        }

        inline void atomic_write_value(_rs_value_data_seg_type data_seg, _rs_value_type_seg_type type_seg)
        {
            do
            {
                _rs_value_data_seg = data_seg._data_seg;
                _rs_value_type_seg = type_seg._type_seg;
            } while (_rs_value_data_seg != data_seg._data_seg);
        }

        inline value* set_integer(rs_integer_t val)
        {
            _rs_value_data_seg_type _ds;
            _rs_value_type_seg_type _ts;

            _ds.integer = val;
            _ts.type = valuetype::integer_type;

            atomic_write_value(_ds, _ts);

            return this;
        }
        inline value* set_real(rs_real_t val)
        {
            _rs_value_data_seg_type _ds;
            _rs_value_type_seg_type _ts;

            _ds.real = val;
            _ts.type = valuetype::integer_type;

            atomic_write_value(_ds, _ts);

            return this;
        }

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
            handle = _val->handle;
            type = _val->type;

            return this;
        }

        inline bool is_nil()const
        {
            return handle;
        }
    };
    static_assert(sizeof(value) == 16);
    static_assert(sizeof(std::atomic_uint64_t) == 8);
    static_assert(sizeof(value::_rs_value_data_seg_type) == 8);
    static_assert(sizeof(value::_rs_value_type_seg_type) == 8);
    static_assert(std::atomic_uint64_t::is_always_lock_free);

    using native_func_t = rs_native_func;
}