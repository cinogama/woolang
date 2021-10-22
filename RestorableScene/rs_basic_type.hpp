#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <map>
#include <vector>

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

        union
        {
            real_t      real;
            int64_t     integer;
            uint64_t    handle;
            string_t* string;     // ADD-ABLE TYPE
            mapping_t* mapping;

            value* ref;
        };

        enum class valuetype : uint8_t
        {
            real_type,
            integer_type,
            handle_type,
            string_type,

            is_ref,

            invalid = 0xff,
        };
        valuetype type;

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
            type = _val->type;
            handle = _val->handle;

            return this;
        }

        inline bool is_nil()const
        {
            return handle;
        }
    };
}