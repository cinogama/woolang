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
    namespace irv2
    {
        union ir;
    }

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

    using hash_t = uint64_t;

    struct gchandle_base_t;
    struct closure_bast_t;
    struct structure_base_t;

    using string_t = gcunit<std::string>;
    using dictionary_t = gcunit<std::unordered_map<value, value, value_hasher, value_equal>>;
    using array_t = gcunit<std::vector<value>>;
    using gchandle_t = gcunit<gchandle_base_t>;
    using closure_t = gcunit<closure_bast_t>;
    using structure_t = gcunit<structure_base_t>;

    struct WO_DECLARE_ALIGNAS(8) value
    {
        //  value
        /*
         *
         */
        enum valuetype : uint8_t
        {
            invalid = WO_INVALID_TYPE,

            need_gc_flag = WO_NEED_GC_FLAG,

            integer_type = WO_INTEGER_TYPE,
            real_type = WO_REAL_TYPE,
            handle_type = WO_HANDLE_TYPE,
            bool_type = WO_BOOL_TYPE,

            script_func_type = WO_SCRIPT_FUNC_TYPE,
            native_func_type = WO_NATIVE_FUNC_TYPE,

            // When VM invoke a `NEAR` function, use `callstack_t` to store bp and ret_ip.
            callstack = WO_CALLSTACK_TYPE,
            // When VM invoke a function that is from another ENV, it's a `FAR` callstack.
            // Use `const byte_t* m_farcallstack` & `uint32_t m_ext_farcallstack_bp` to 
            // store far callstack info. 
            far_callstack = WO_FAR_CALLSTACK_TYPE,
            // When VM is invoked from native, use `const byte_t* m_nativecallstack` to store 
            // native callstack info, if:
            //  1) The native function is called by VM function directly. In this case, `ip` in 
            //    VM stores the address of native function. `m_nativecallstack` will store
            //    this address.
            //  2) Other case, `ip` in VM stores `nullptr`. `m_nativecallstack` will store 
            //    `nullptr` too.
            //
            // Whatever which case, vm's `sp` & `bp` should be keep and restore by invoker.
            native_callstack = WO_NATIVE_CALLSTACK_TYPE,
            // When a yieldable function called, store sp & bp here to make sure `sp` & `bp` 
            // can be restored correctly after job completed.
            yield_checkpoint = WO_YIELD_CHECK_POINT_TYPE,

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

        struct yieldcheckpt_t
        {
            uint32_t sp;
            uint32_t bp;
        };

        union
        {
            wo_real_t m_real;
            wo_integer_t m_integer;
            wo_handle_t m_handle;
            const irv2::ir* m_script_func;
            wo_native_func_t m_native_func;
            gcbase* m_gcunit;
            string_t* m_string;
            array_t* m_array;
            dictionary_t* m_dictionary;
            gchandle_t* m_gchandle;
            closure_t* m_closure;
            structure_t* m_structure;
            callstack_t m_vmcallstack;
            const irv2::ir* m_farcallstack;
            const irv2::ir* m_nativecallstack;
            yieldcheckpt_t m_yield_checkpoint;
            uint64_t m_value_field;
        };
        struct
        {
            valuetype m_type;

            // Value field is not engough for storing bp.
            uint32_t m_ext_farcallstack_bp;
        };

        WO_FORCE_INLINE value* set_takeplace()
        {
            m_type = valuetype::need_gc_flag;
            m_value_field = 0;
            return this;
        }
        WO_FORCE_INLINE value* set_string(const std::string & str)
        {
            set_gcunit<wo::value::valuetype::string_type>(
                string_t::gc_new<gcbase::gctype::young>(str));
            return this;
        }
        WO_FORCE_INLINE value* set_buffer(const void* buf, size_t sz)
        {
            set_gcunit<wo::value::valuetype::string_type>(
                string_t::gc_new<gcbase::gctype::young>((const char*)buf, sz));
            return this;
        }
        value* set_string_nogc(std::string_view str);
        value* set_struct_nogc(uint16_t sz);
        value* set_val_with_compile_time_check(const value * val);
        WO_FORCE_INLINE value* set_script_func(const irv2::ir * val)
        {
            m_type = valuetype::script_func_type;
            m_script_func = val;
            return this;
        }
        WO_FORCE_INLINE value* set_native_func(wo_native_func_t val)
        {
            m_type = valuetype::native_func_type;
            m_native_func = val;
            return this;
        }
        WO_FORCE_INLINE value* set_integer(wo_integer_t val)
        {
            m_type = valuetype::integer_type;
            m_integer = val;
            return this;
        }
        WO_FORCE_INLINE value* set_real(wo_real_t val)
        {
            m_type = valuetype::real_type;
            m_real = val;
            return this;
        }
        WO_FORCE_INLINE value* set_handle(wo_handle_t val)
        {
            m_type = valuetype::handle_type;
            m_handle = val;
            return this;
        }
        WO_FORCE_INLINE value* set_nil()
        {
            m_type = valuetype::invalid;
            m_value_field = 0;
            return this;
        }
        WO_FORCE_INLINE value* set_bool(bool val)
        {
            m_type = valuetype::bool_type;
            m_integer = val ? 1 : 0;
            return this;
        }
        WO_FORCE_INLINE value* set_callstack(uint32_t ip, uint32_t bp)
        {
            m_type = valuetype::callstack;
            m_vmcallstack.ret_ip = ip;
            m_vmcallstack.bp = bp;
            return this;
        }
        template <valuetype ty, typename T>
        WO_FORCE_INLINE value* set_gcunit(T * unit)
        {
            static_assert(ty & valuetype::need_gc_flag);

            m_type = ty;
            if constexpr (sizeof(gcbase*) < sizeof(wo_handle_t))
                m_value_field = 0;

            if constexpr (ty == valuetype::string_type)
                m_string = unit;
            else if constexpr (ty == valuetype::array_type)
                m_array = unit;
            else if constexpr (ty == valuetype::dict_type)
                m_dictionary = unit;
            else if constexpr (ty == valuetype::gchandle_type)
                m_gchandle = unit;
            else if constexpr (ty == valuetype::closure_type)
                m_closure = unit;
            else if constexpr (ty == valuetype::struct_type)
                m_structure = unit;

            return this;
        }

        // ATTENTION: Only work for gc-work-thread & no_gc unit. gc-unit might be freed
        //          after get_gcunit_and_attrib_ref.
        gcbase* get_gcunit_and_attrib_ref(gc::unit_attrib * *attrib) const;

        // ATTENTION: Only work for gc-work-thread & no_gc unit. gc-unit might be freed
        //          after get_gcunit_and_attrib_ref.
        gc::unit_attrib* fast_get_attrib_for_assert_check() const;
        WO_FORCE_INLINE bool is_gcunit() const
        {
            return m_type & valuetype::need_gc_flag;
        }
        WO_FORCE_INLINE value* set_val(const value * _val)
        {
            m_type = _val->m_type;
            m_value_field = _val->m_value_field;
            return this;
        }
        std::string get_type_name() const;

        value* set_dup(value * from);

        WO_FORCE_INLINE static void write_barrier(const value * val)
        {
            gc::unit_attrib* attr;
            if (auto* unit = val->get_gcunit_and_attrib_ref(&attr))
            {
                if (attr->m_marked == (uint8_t)gcbase::gcmarkcolor::no_mark)
                    gc::m_memo_mark_gray_list.add_one(gc::memo_unit::acquire_memo_unit(unit, attr));
            }
        }

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
    struct closure_bast_t
    {
        bool m_native_call;
        const uint16_t m_closure_args_count;
        union
        {
            const irv2::ir* m_vm_func;
            wo_native_func_t m_native_func;
        };
        value* m_closure_args;

        closure_bast_t(const closure_bast_t&) = delete;
        closure_bast_t(closure_bast_t&&) = delete;
        closure_bast_t& operator=(const closure_bast_t&) = delete;
        closure_bast_t& operator=(closure_bast_t&&) = delete;

        closure_bast_t(const irv2::ir* vmfunc, uint16_t argc) noexcept;
        closure_bast_t(wo_native_func_t nfunc, uint16_t argc) noexcept;
        ~closure_bast_t();
    };
    struct gchandle_base_t
    {
        using destructor_func_t = wo_gchandle_close_func_t;
        using gcmark_func_t = wo_gcstruct_mark_func_t;

        void* m_holding_handle;
        destructor_func_t m_destructor;

        struct custom_marker
        {
#ifdef WO_PLATFORM_64
            static_assert(sizeof(intptr_t) == sizeof(int64_t));
            uintptr_t m_is_callback : 1;
            uintptr_t m_marker63 : 63;
#else
            static_assert(sizeof(intptr_t) == sizeof(int32_t));
            uintptr_t m_is_callback;
            uintptr_t m_marker32;
#endif
        };

        // ATTENTION: Only used for decrease destructable count in env of vm;
        std::atomic_size_t* m_hold_counter;
        custom_marker m_custom_marker;

        void set_custom_mark_callback(gcmark_func_t callback);
        void set_custom_mark_unit(gcbase* unit_may_null);

        bool do_close();
        void do_custom_mark(wo_gc_work_context_t ctx);
        void dec_destructable_instance_count();

        ~gchandle_base_t();
    };
}
