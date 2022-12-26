#pragma once

#include "wo_assert.hpp"
#include "wo_basic_type.hpp"
#include "wo_instruct.hpp"
#include "wo_meta.hpp"
#include "wo_compiler_parser.hpp"
#include "wo_env_locale.hpp"
#include "wo_global_setting.hpp"
#include "wo_lang_extern_symbol_loader.hpp"
#include "wo_shared_ptr.hpp"
#include "wo_memory.hpp"
#include "wo_compiler_jit.hpp"

#include <cstring>
#include <string>

namespace wo
{
    namespace opnum
    {
        struct opnumbase
        {
            virtual ~opnumbase() = default;
            virtual size_t generate_opnum_to_buffer(cxx_vec_t<byte_t>& buffer) const
            {
                wo_error("This type can not generate opnum.");

                return 0;
            }
        };

        struct stack :virtual opnumbase
        {
            int16_t offset;

            stack(int16_t _offset) noexcept
                :offset(_offset)
            {

            }
        };

        struct global :virtual opnumbase
        {
            int32_t offset;
            int32_t real_offset_const_glb;

            global(int32_t _offset) noexcept
                :offset(_offset)
                , real_offset_const_glb(0xFFFFFFFF)
            {

            }

            size_t generate_opnum_to_buffer(cxx_vec_t<byte_t>& buffer) const override
            {
                byte_t* buf = (byte_t*)&real_offset_const_glb;

                buffer.push_back(buf[0]);
                buffer.push_back(buf[1]);
                buffer.push_back(buf[2]);
                buffer.push_back(buf[3]);

                return 4;
            }
        };

        struct reg :virtual opnumbase
        {
            // REGID
            // 0B 1 1 000000
            //  isreg? stackbp? offset

            uint8_t id;

            static constexpr uint32_t T_REGISTER_COUNT = 16;
            static constexpr uint32_t R_REGISTER_COUNT = 16;
            static constexpr uint32_t ALL_REGISTER_COUNT = 64;

            enum spreg : uint8_t
            {
                // normal regist
                t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14, t15,
                r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, r13, r14, r15,

                // special regist
                op_trace_result = 0b00100000, cr = op_trace_result,
                argument_count, tc = argument_count,
                exception_inform, er = exception_inform,
                nil_constant, ni = nil_constant,
                pattern_match, pm = pattern_match,

                last_special_register = 0b00111111,
            };

            reg(uint8_t _id) noexcept
                :id(_id)
            {
            }

            static constexpr uint8_t bp_offset(int8_t offset)
            {
                wo_assert(offset >= -64 && offset <= 63);

                return 0b10000000 | offset;
            }

            bool is_tmp_regist() const
            {
                return id >= opnum::reg::t0 && id <= opnum::reg::r15;
            }

            bool is_bp_offset() const
            {
                return id & (uint8_t)0b10000000;
            }

            int8_t get_bp_offset() const
            {
                wo_assert(is_bp_offset());
#define WO_SIGNED_SHIFT(VAL) (((signed char)((unsigned char)(((unsigned char)(VAL))<<1)))>>1)
                return WO_SIGNED_SHIFT(id);
#undef WO_SIGNED_SHIFT
            }

            size_t generate_opnum_to_buffer(cxx_vec_t<byte_t>& buffer) const override
            {
                buffer.push_back(id);
                return 1;
            }
        };

        struct immbase :virtual opnumbase
        {
            int32_t constant_index = 0;

            virtual value::valuetype type()const = 0;
            virtual bool operator < (const immbase&) const = 0;
            virtual void apply(value*) const = 0;

            size_t generate_opnum_to_buffer(cxx_vec_t<byte_t>& buffer) const override
            {
                byte_t* buf = (byte_t*)&constant_index;

                buffer.push_back(buf[0]);
                buffer.push_back(buf[1]);
                buffer.push_back(buf[2]);
                buffer.push_back(buf[3]);

                return 4;
            }

            virtual bool is_true()const = 0;
            virtual int64_t try_int()const = 0;
            virtual int64_t try_set_int(int64_t _val) = 0;
        };

        struct tag :virtual opnumbase
        {
            std::string name;

            tag(const std::string& _name)
                : name(_name)
            {

            }
        };

        template<typename T>
        struct imm :virtual immbase
        {
            T val;

            imm(T _val) noexcept
                :val(_val)
            {
                wo_assert(type() != value::valuetype::invalid, "Invalid immediate.");
            }

            value::valuetype type()const noexcept override
            {
                if constexpr (meta::is_string<T>::value)
                    return value::valuetype::string_type;
                else if constexpr (std::is_pointer<T>::value)
                    return value::valuetype::handle_type;
                else if constexpr (std::is_integral<T>::value)
                    return value::valuetype::integer_type;
                else if constexpr (std::is_floating_point<T>::value)
                    return value::valuetype::real_type;
                else
                    return value::valuetype::invalid;

                // If changed this function, don't forget to modify 'apply'.
            }

            bool operator < (const immbase& _another) const override
            {
                if (dynamic_cast<const tag*>(&_another))
                    return false;

                auto t = type();
                auto t2 = _another.type();
                if (t == t2)
                {
                    if (t == value::valuetype::integer_type)
                        return try_int() < _another.try_int();
                    else
                        return val < (dynamic_cast<const imm&>(_another)).val;
                }
                return t < t2;
            }

            void apply(value* v) const override
            {
                v->type = type();
                if constexpr (meta::is_string<T>::value)
                    string_t::gc_new<gcbase::gctype::no_gc>(v->gcunit, val);
                else if constexpr (std::is_pointer<T>::value)
                    v->handle = (uint64_t)val;
                else if constexpr (std::is_integral<T>::value)
                    v->integer = val;
                else if constexpr (std::is_floating_point<T>::value)
                    v->real = val;
                else
                    wo_error("Invalid immediate.");
            }

            virtual int64_t try_int()const override
            {
                if constexpr (std::is_integral<T>::value)
                    return val;

                wo_error("Immediate is not integer.");
                return 0;
            }
            virtual int64_t try_set_int(int64_t _val) override
            {
                if constexpr (std::is_integral<T>::value)
                    return val = (T)_val;

                wo_error("Immediate is not integer.");
                return 0;
            }
            virtual bool is_true()const
            {
                if constexpr (meta::is_string<T>::value)
                {
                    wo_error("Cannot eval string here.");
                    return true;
                }
                else
                    return (bool)val;
            }
        };

        struct tagimm_rsfunc :virtual tag, virtual imm<int>
        {
            tagimm_rsfunc(const std::string& name)
                : tag(name)
                , imm<int>(0xFFFFFFFF)
            {

            }
            bool operator < (const immbase& _another) const override
            {
                if (auto* another_tag = dynamic_cast<const tagimm_rsfunc*>(&_another))
                    return name < another_tag->name;
                return true;
            }

            size_t generate_opnum_to_buffer(cxx_vec_t<byte_t>& buffer) const override
            {
                return imm<int>::generate_opnum_to_buffer(buffer);
            }
        };
    } // namespace opnum;
    namespace ast
    {
        struct ast_value_function_define;
    }

    struct vmbase;
    class ir_compiler;
    struct runtime_env;

    struct program_debug_data_info
    {
        struct location
        {
            size_t      ip;
            size_t      begin_row_no;
            size_t      begin_col_no;
            size_t      end_row_no;
            size_t      end_col_no;
            std::wstring source_file = L"not_found";
        };
        struct function_symbol_infor
        {
            struct variable_symbol_infor
            {
                std::string name;
                size_t define_place;
                wo_integer_t bp_offset;
            };
            size_t ir_begin;
            size_t ir_end;
            size_t in_stack_reg_count;

            std::map<std::string, std::vector<variable_symbol_infor>> variables;

            void add_variable_define(const std::wstring varname, size_t rowno, wo_integer_t locat)
            {
                variables[wstr_to_str(varname)].push_back(
                    variable_symbol_infor
                    {
                        wstr_to_str(varname),
                        rowno,
                        (wo_integer_t)(locat >= 0 ? locat + in_stack_reg_count : locat)
                    }
                );
            }
        };

        using filename_rowno_colno_ip_info_t = std::map<std::wstring, std::vector<location>>;
        using ip_src_location_info_t = std::map<size_t, location>;
        using runtime_ip_compile_ip_info_t = std::map<size_t, size_t>;
        using function_signature_ip_info_t = std::map<std::string, function_symbol_infor>;

        filename_rowno_colno_ip_info_t  _general_src_data_buf_a;
        ip_src_location_info_t          _general_src_data_buf_b;
        function_signature_ip_info_t    _function_ip_data_buf;
        runtime_ip_compile_ip_info_t    pdd_rt_code_byte_offset_to_ir;

        const byte_t* runtime_codes_base;
        size_t runtime_codes_length;

        // for lang
        void generate_debug_info_at_astnode(grammar::ast_base* ast_node, ir_compiler* compiler);
        void finalize_generate_debug_info();

        void generate_func_begin(ast::ast_value_function_define* funcdef, ir_compiler* compiler);
        void generate_func_end(ast::ast_value_function_define* funcdef, size_t tmpreg_count, ir_compiler* compiler);
        void add_func_variable(ast::ast_value_function_define* funcdef, const std::wstring& varname, size_t rowno, wo_integer_t loc);

        const location& get_src_location_by_runtime_ip(const  byte_t* rt_pos) const;
        size_t get_ip_by_src_location(const std::wstring& src_name, size_t rowno, bool strict = false)const;
        size_t get_ip_by_runtime_ip(const  byte_t* rt_pos) const;
        size_t get_runtime_ip_by_ip(size_t ip) const;
        std::string get_current_func_signature_by_runtime_ip(const byte_t* rt_pos) const;
    };

    struct runtime_env
    {
        value* constant_global_reg_rtstack = nullptr;
        value* reg_begin = nullptr;
        value* stack_begin = nullptr;

        size_t constant_and_global_value_takeplace_count = 0;
        size_t constant_value_count = 0;
        size_t real_register_count = 0;

        size_t runtime_stack_count = 0;

        size_t rt_code_len = 0;
        const byte_t* rt_codes = nullptr;

        std::atomic_size_t _running_on_vm_count = 0;
        std::atomic_size_t _created_destructable_instance_count = 0;

        std::vector<size_t> _functions_offsets;
        std::vector<size_t> _calln_opcode_offsets;

        std::vector<void*> _jit_functions;

        shared_pointer<program_debug_data_info> program_debug_info;
        rslib_extern_symbols::extern_lib_set loaded_libs;

        struct extern_native_function_location
        {
            std::string script_name;
            std::string library_name;
            std::string function_name;

            // Will be fill in finalize of env.
            std::vector<size_t> caller_offset_in_ir;

            // Will be fill in saving binary of env.
            std::vector<size_t> constant_offset_in_binary;
        };
        using extern_native_functions_t = std::map<intptr_t, extern_native_function_location>;
        extern_native_functions_t extern_native_functions;

        using extern_function_map_t = std::map<std::string, size_t>;
        extern_function_map_t extern_script_functions;

        ~runtime_env()
        {
            free_jit(this);

            for (size_t ci = 0; ci < constant_value_count; ++ci)
                if (constant_global_reg_rtstack[ci].is_gcunit())
                {
                    wo_assert(constant_global_reg_rtstack[ci].type == wo::value::valuetype::string_type);

                    constant_global_reg_rtstack[ci].gcunit->~gcbase();
                    free64(constant_global_reg_rtstack[ci].gcunit);
                }

            if (constant_global_reg_rtstack)
                free64(constant_global_reg_rtstack);

            if (rt_codes)
                free64((byte_t*)rt_codes);
        }

        struct binary_source_stream
        {
            const char* stream;
            size_t stream_size;

            size_t readed_size;

            binary_source_stream(const void* bytestream, size_t streamsz)
                : stream((const char*)bytestream)
                , stream_size(streamsz)
                , readed_size(0)
            {

            }

            bool read_buffer(void* dst, size_t count) noexcept
            {
                if (readed_size + count <= stream_size)
                {
                    memcpy(dst, stream + readed_size, count);
                    readed_size += count;

                    return true;
                }
                return false;
            }

            template<typename T>
            bool read_elem(T* out_elem) noexcept
            {
                return read_buffer(out_elem, sizeof(T));
            }

            size_t readed_offset() const noexcept
            {
                return readed_size;
            }

        };

        std::tuple<void*, size_t> create_env_binary() noexcept
        {
            std::vector<wo::byte_t> binary_buffer;
            auto write_buffer_to_buffer = [&binary_buffer](const void* written_data, size_t written_length, size_t allign) {
                const size_t write_begin_place = binary_buffer.size();

                wo_assert(write_begin_place % allign == 0);

                binary_buffer.resize(write_begin_place + written_length);

                memcpy(binary_buffer.data() + write_begin_place, written_data, written_length);
                return binary_buffer.size();
            };

            auto write_binary_to_buffer = [&write_buffer_to_buffer](const auto& d, size_t size_for_assert) {
                const wo::byte_t* written_data = std::launder(reinterpret_cast<const wo::byte_t*>(&d));
                const size_t written_length = sizeof(d);

                wo_assert(written_length == size_for_assert);

                return write_buffer_to_buffer(written_data, written_length, sizeof(d) > 8 ? 8 : sizeof(d));
            };

            std::vector<const char*> constant_string_pool;

            // 1.1 (+0) Magic number(0x3001A26B look like WOOLANG B)
            write_binary_to_buffer((uint32_t)0x3001A26B, 4);
            write_binary_to_buffer((uint32_t)0x00000000, 4);
            // 1.2 (+8) Version info
            write_binary_to_buffer((uint64_t)wo_version_int(), 8);
            // 1.3 (+16) Source CRC64(TODO)

            // 2.1 (+16 + 2N * 8) Global space size * sizeof(Value)
            write_binary_to_buffer(
                (uint64_t)(this->constant_and_global_value_takeplace_count
                    - this->constant_value_count), 8);

            // 2.2 Default register size
            write_binary_to_buffer(
                (uint64_t)this->real_register_count, 8);

            // 2.3 Constant data
            size_t string_buffer_size = 0;
            write_binary_to_buffer(
                (uint64_t)this->constant_value_count, 8);
            for (size_t ci = 0; ci < this->constant_value_count; ++ci)
            {
                auto& constant_value = this->constant_global_reg_rtstack[ci];
                wo_assert(constant_value.type == wo::value::valuetype::integer_type
                    || constant_value.type == wo::value::valuetype::real_type
                    || constant_value.type == wo::value::valuetype::handle_type
                    || constant_value.type == wo::value::valuetype::string_type
                    || constant_value.type == wo::value::valuetype::invalid);

                write_binary_to_buffer((uint64_t)constant_value.type_space, 8);

                if (constant_value.type == wo::value::valuetype::string_type)
                {
                    // Record for string
                    wo_assert(constant_value.string->gc_type == wo::gcbase::gctype::no_gc);

                    write_binary_to_buffer((uint32_t)string_buffer_size, 4);
                    constant_string_pool.push_back(constant_value.string->c_str());
                    write_binary_to_buffer((uint32_t)constant_value.string->size(), 4);

                    string_buffer_size += constant_value.string->size();
                }
                else
                    // Record for value
                    write_binary_to_buffer((uint64_t)constant_value.value_space, 8);

                if (constant_value.type == wo::value::valuetype::handle_type)
                {
                    // Check if constant_value is function address from native?
                    auto fnd = this->extern_native_functions.find((intptr_t)constant_value.handle);
                    if (fnd != this->extern_native_functions.end())
                    {
                        wo_assert(!fnd->second.function_name.empty());
                        fnd->second.constant_offset_in_binary.push_back(ci);
                    }
                }
            }

            // 3.1 Code data
            //  3.1.1 Code data length
            size_t padding_length_for_rt_coding = (8ull - (this->rt_code_len % 8ull)) % 8ull;
            write_binary_to_buffer((uint64_t)(this->rt_code_len + padding_length_for_rt_coding), 8);
            write_buffer_to_buffer(this->rt_codes, this->rt_code_len, 1);
            write_buffer_to_buffer("\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC", padding_length_for_rt_coding, 1);

            // 4.1 Extern native function information
            //  4.1.1 Extern function libname & symbname & used constant offset / code offset
            write_binary_to_buffer((uint64_t)this->extern_native_functions.size(), 8);
            for (auto& [funcptr, extfuncloc] : this->extern_native_functions)
            {
                // 4.1.1.1 extern script name
                write_binary_to_buffer((uint32_t)string_buffer_size, 4);
                constant_string_pool.push_back(extfuncloc.script_name.c_str());
                write_binary_to_buffer((uint32_t)extfuncloc.script_name.size(), 4);
                string_buffer_size += extfuncloc.script_name.size();

                // 4.1.1.2 extern library name
                write_binary_to_buffer((uint32_t)string_buffer_size, 4);
                constant_string_pool.push_back(extfuncloc.library_name.c_str());
                write_binary_to_buffer((uint32_t)extfuncloc.library_name.size(), 4);
                string_buffer_size += extfuncloc.library_name.size();

                // 4.1.1.3 extern function name
                write_binary_to_buffer((uint32_t)string_buffer_size, 4);
                constant_string_pool.push_back(extfuncloc.function_name.c_str());
                write_binary_to_buffer((uint32_t)extfuncloc.function_name.size(), 4);
                string_buffer_size += extfuncloc.function_name.size();

                // 4.1.1.4 used function in constant index
                write_binary_to_buffer((uint64_t)extfuncloc.constant_offset_in_binary.size(), 8);
                for (auto constant_index : extfuncloc.constant_offset_in_binary)
                    write_binary_to_buffer((uint64_t)constant_index, 8);

                // 4.1.1.5 used function in ir binary code
                write_binary_to_buffer((uint64_t)extfuncloc.caller_offset_in_ir.size(), 8);
                for (auto constant_index : extfuncloc.caller_offset_in_ir)
                    write_binary_to_buffer((uint64_t)constant_index, 8);
            }

            // 4.2 Extern woolang function define & offset
            //  4.2.1 Extern woolang function symbol name & offset
            write_binary_to_buffer((uint64_t)this->extern_script_functions.size(), 8);
            for (auto& [funcname, offset] : this->extern_script_functions)
            {
                // 4.2.1.1 extern function name
                write_binary_to_buffer((uint32_t)string_buffer_size, 4);
                constant_string_pool.push_back(funcname.c_str());
                write_binary_to_buffer((uint32_t)funcname.size(), 4);
                string_buffer_size += funcname.size();

                write_binary_to_buffer((uint64_t)offset, 8);
            }

            // 5.1 Constant string buffer
            size_t padding_length_for_constant_string_buf = (8ull - (string_buffer_size % 8ull)) % 8ull;

            write_binary_to_buffer(
                (uint64_t)(string_buffer_size + padding_length_for_constant_string_buf), 8);

            for (size_t si = 0; si < constant_string_pool.size(); ++si)
                write_buffer_to_buffer(constant_string_pool[si], strlen(constant_string_pool[si]), 1);

            write_buffer_to_buffer("_padding", padding_length_for_constant_string_buf, 1);

            // 6.1 Debug information(TODO)

            auto* finalbinary = wo::alloc64(binary_buffer.size());
            memcpy(finalbinary, binary_buffer.data(), binary_buffer.size());

            return std::make_tuple(finalbinary, binary_buffer.size());
        }

        static shared_pointer<runtime_env> _create_from_stream(binary_source_stream* stream, size_t stackcount, wo_string_t* out_reason, bool* out_is_binary)
        {
            *out_is_binary = true;

#define WO_LOAD_BIN_FAILED(reason) do{*out_reason = "Magic number failed."; return nullptr;}while(0)
            // 1.1 (+0) Magic number(0x3001A26B look like WOOLANG B)
            uint32_t magic_number;
            if (!stream->read_elem(&magic_number) || magic_number != (uint32_t)0x3001A26B)
            {
                *out_is_binary = false;
                WO_LOAD_BIN_FAILED("Magic number failed.");
            }

            stream->read_elem(&magic_number); // padding 4 byte.

            // 1.2 (+8) Version info
            uint64_t version_infrom;
            if (!stream->read_elem(&version_infrom) || version_infrom != (uint64_t)wo_version_int())
                WO_LOAD_BIN_FAILED("Woolang version missmatch.");

            // 1.3 (+16) Source CRC64(TODO)

            // 2.1 (+16 + 2N * 8) Global space size * sizeof(Value)
            uint64_t global_value_count;
            if (!stream->read_elem(&global_value_count))
                WO_LOAD_BIN_FAILED("Failed to restore global value count.");

            // 2.2 Default register size
            uint64_t register_count;
            if (!stream->read_elem(&register_count))
                WO_LOAD_BIN_FAILED("Failed to restore register count.");

            // 2.3 Constant data
            uint64_t constant_value_count;
            if (!stream->read_elem(&constant_value_count))
                WO_LOAD_BIN_FAILED("Failed to restore constant count.");

            shared_pointer<runtime_env> result = new runtime_env;

            result->real_register_count = register_count;
            result->constant_and_global_value_takeplace_count = constant_value_count + global_value_count;
            result->constant_value_count = constant_value_count;

            result->runtime_stack_count = stackcount;

            size_t global_allign_takeplace_for_avoiding_false_shared =
                config::ENABLE_AVOIDING_FALSE_SHARED ?
                ((size_t)(platform_info::CPU_CACHELINE_SIZE / (double)sizeof(wo::value) + 0.5)) : (1);

            size_t preserve_memory_size =
                result->constant_and_global_value_takeplace_count
                + register_count
                + result->runtime_stack_count;

            value* preserved_memory = (value*)alloc64(preserve_memory_size * sizeof(value));
            memset(preserved_memory, 0, preserve_memory_size * sizeof(value));

            result->constant_global_reg_rtstack = preserved_memory;
            result->reg_begin = result->constant_global_reg_rtstack
                + result->constant_and_global_value_takeplace_count;

            result->stack_begin = result->constant_global_reg_rtstack + (preserve_memory_size - 1);

            struct string_buffer_index
            {
                uint32_t index;
                uint32_t size;
            };
            std::map<uint64_t, string_buffer_index> constant_string_index_for_update;

            for (uint64_t ci = 0; ci < constant_value_count; ++ci)
            {
                uint64_t constant_type_scope, constant_value_scope;
                if (!stream->read_elem(&constant_type_scope))
                    WO_LOAD_BIN_FAILED("Failed to restore constant type.");

                preserved_memory[ci].type_space = constant_type_scope;
                if (preserved_memory[ci].type == wo::value::valuetype::string_type)
                {
                    uint32_t constant_string_pool_loc, constant_string_pool_size;
                    if (!stream->read_elem(&constant_string_pool_loc) || !stream->read_elem(&constant_string_pool_size))
                        WO_LOAD_BIN_FAILED("Failed to restore constant string.");

                    auto& loc = constant_string_index_for_update[ci];
                    loc.index = constant_string_pool_loc;
                    loc.size = constant_string_pool_size;
                }
                else
                {
                    if (!stream->read_elem(&constant_value_scope))
                        WO_LOAD_BIN_FAILED("Failed to restore constant value.");
                    preserved_memory[ci].value_space = constant_value_scope;
                }
            }

            // 3.1 Code data
            //  3.1.1 Code data length
            uint64_t rt_code_with_padding_length;
            if (!stream->read_elem(&rt_code_with_padding_length))
                WO_LOAD_BIN_FAILED("Failed to restore code length.");

            byte_t* code_buf = (byte_t*)alloc64(rt_code_with_padding_length * sizeof(byte_t));
            result->rt_codes = code_buf;
            result->rt_code_len = rt_code_with_padding_length * sizeof(byte_t);

            wo_assert(code_buf != nullptr);

            if (!stream->read_buffer(code_buf, rt_code_with_padding_length * sizeof(byte_t)))
                WO_LOAD_BIN_FAILED("Failed to restore code.");

            struct extern_native_function
            {
                string_buffer_index script_path_idx;
                string_buffer_index library_name_idx;
                string_buffer_index function_name_idx;

                std::vector<size_t> constant_offsets;
                std::vector<size_t> ir_command_offsets;
            };
            std::vector<extern_native_function> extern_native_functions;

            // 4.1 Extern native function information
            //  4.1.1 Extern function libname & symbname & used constant offset / code offset
            uint64_t extern_native_function_count;
            if (!stream->read_elem(&extern_native_function_count))
                WO_LOAD_BIN_FAILED("Failed to restore extern native function count.");

            for (uint64_t i = 0; i < extern_native_function_count; ++i)
            {
                extern_native_function loading_function;

                // 4.1.1.1 extern script name
                if (!stream->read_elem(&loading_function.script_path_idx.index)
                    || !stream->read_elem(&loading_function.script_path_idx.size))
                    WO_LOAD_BIN_FAILED("Failed to restore extern native function symbol loader script.");

                // 4.1.1.2 extern library name
                if (!stream->read_elem(&loading_function.library_name_idx.index)
                    || !stream->read_elem(&loading_function.library_name_idx.size))
                    WO_LOAD_BIN_FAILED("Failed to restore extern native function library name.");

                // 4.1.1.3 extern function name
                if (!stream->read_elem(&loading_function.function_name_idx.index)
                    || !stream->read_elem(&loading_function.function_name_idx.size))
                    WO_LOAD_BIN_FAILED("Failed to restore extern native function symbol name.");

                // 4.1.1.4 used function in constant index
                uint64_t used_constant_offset_count;
                if (!stream->read_elem(&used_constant_offset_count))
                    WO_LOAD_BIN_FAILED("Failed to restore extern native function constant count.");

                for (uint64_t i = 0; i < used_constant_offset_count; ++i)
                {
                    uint64_t constant_index;
                    if (!stream->read_elem(&constant_index))
                        WO_LOAD_BIN_FAILED("Failed to restore extern native function constant index.");

                    loading_function.constant_offsets.push_back(constant_index);
                }

                // 4.1.1.5 used function in ir binary code
                uint64_t used_ir_offset_count;
                if (!stream->read_elem(&used_ir_offset_count))
                    WO_LOAD_BIN_FAILED("Failed to restore extern native function ir-offset count.");

                for (uint64_t i = 0; i < used_ir_offset_count; ++i)
                {
                    uint64_t ir_code_offset;
                    if (!stream->read_elem(&ir_code_offset))
                        WO_LOAD_BIN_FAILED("Failed to restore extern native function ir-offset.");

                    loading_function.ir_command_offsets.push_back(ir_code_offset);
                }
                extern_native_functions.emplace_back(std::move(loading_function));
            }

            // 4.2 Extern woolang function define & offset
            //  4.2.1 Extern woolang function symbol name & offset
            struct extern_script_function
            {
                string_buffer_index function_name;
                size_t ir_offset;
            };
            std::vector<extern_script_function> extern_script_functions;

            uint64_t extern_script_function_count;
            if (!stream->read_elem(&extern_script_function_count))
                WO_LOAD_BIN_FAILED("Failed to restore extern script function count.");

            for (uint64_t i = 0; i < extern_script_function_count; ++i)
            {
                extern_script_function loading_function;

                if (!stream->read_elem(&loading_function.function_name.index)
                    || !stream->read_elem(&loading_function.function_name.size))
                    WO_LOAD_BIN_FAILED("Failed to restore extern script function count.");

                uint64_t iroffset;
                if (!stream->read_elem(&iroffset))
                    WO_LOAD_BIN_FAILED("Failed to restore extern script function ir-offset.");

                loading_function.ir_offset = iroffset;

                extern_script_functions.emplace_back(std::move(loading_function));
            }

            // 5.1 Constant string buffer
            uint64_t string_buffer_size_with_padding;
            if (!stream->read_elem(&string_buffer_size_with_padding))
                WO_LOAD_BIN_FAILED("Failed to restore string buffer size.");

            auto string_buffer_begin_offset = stream->readed_offset();

            auto restore_string_from_buffer = [stream, string_buffer_begin_offset](const string_buffer_index& string_index, std::string* out_str)->bool {
                auto current_string_begin_idx = stream->readed_offset() - string_buffer_begin_offset;
                if (string_index.index != current_string_begin_idx)
                    return false;

                std::vector<char> tmp_string_buffer(string_index.size + 1, 0);
                stream->read_buffer(tmp_string_buffer.data(), string_index.size);

                *out_str = tmp_string_buffer.data();
                return true;
            };

            std::string constant_string;
            for (auto& [constant_offset, string_index] : constant_string_index_for_update)
            {
                wo_assert(preserved_memory[constant_offset].type == wo::value::valuetype::string_type);

                if (!restore_string_from_buffer(string_index, &constant_string))
                    WO_LOAD_BIN_FAILED("Failed to restore string from string buffer.");

                preserved_memory[constant_offset].set_string_nogc(constant_string.c_str());
            }

            for (auto& extern_native_function : extern_native_functions)
            {
                std::string script_path, library_name, function_name;
                if (!restore_string_from_buffer(extern_native_function.script_path_idx, &script_path))
                    WO_LOAD_BIN_FAILED("Failed to restore string from string buffer.");
                if (!restore_string_from_buffer(extern_native_function.library_name_idx, &library_name))
                    WO_LOAD_BIN_FAILED("Failed to restore string from string buffer.");
                if (!restore_string_from_buffer(extern_native_function.function_name_idx, &function_name))
                    WO_LOAD_BIN_FAILED("Failed to restore string from string buffer.");

                wo_native_func func = nullptr;

                if (library_name == "")
                    func = rslib_extern_symbols::get_global_symbol(function_name.c_str());
                else
                    func = result->loaded_libs.try_load_func_from_in(script_path.c_str(), library_name.c_str(), function_name.c_str());

                wo_assert(func != nullptr);

                for (auto constant_offset : extern_native_function.constant_offsets)
                {
                    wo_assert(preserved_memory[constant_offset].type == wo::value::valuetype::handle_type);
                    preserved_memory[constant_offset].set_handle((wo_handle_t)func);
                }
                for (auto ir_code_offset : extern_native_function.ir_command_offsets)
                {
                    // Add 1 for skip `calln` command.
                    uint64_t funcaddr = (uint64_t)func;
                    byte_t* funcaddr_for_write = std::launder(reinterpret_cast<byte_t*>(&funcaddr));

                    code_buf[ir_code_offset + 1 + 0] = funcaddr_for_write[0];
                    code_buf[ir_code_offset + 1 + 1] = funcaddr_for_write[1];
                    code_buf[ir_code_offset + 1 + 2] = funcaddr_for_write[2];
                    code_buf[ir_code_offset + 1 + 3] = funcaddr_for_write[3];
                    code_buf[ir_code_offset + 1 + 4] = funcaddr_for_write[4];
                    code_buf[ir_code_offset + 1 + 5] = funcaddr_for_write[5];
                    code_buf[ir_code_offset + 1 + 6] = funcaddr_for_write[6];
                    code_buf[ir_code_offset + 1 + 7] = funcaddr_for_write[7];
                }
            }

            for (auto& extern_script_function : extern_script_functions)
            {
                std::string function_name;
                if (!restore_string_from_buffer(extern_script_function.function_name, &function_name))
                    WO_LOAD_BIN_FAILED("Failed to restore string from string buffer.");

                wo_assert(result->extern_script_functions.find(function_name) == result->extern_script_functions.end());
                result->extern_script_functions[function_name] = extern_script_function.ir_offset;
            }

            // 6.1 Debug information(TODO)

            return result;
#undef WO_LOAD_BIN_FAILED
        }
        static shared_pointer<runtime_env> load_create_env_from_binary(
            wo_string_t virtual_file, 
            const void* bytestream, 
            size_t streamsz, 
            size_t stack_count, 
            
            wo_string_t* out_reason, 
            bool* out_is_binary)
        {
            std::string buffer_to_store_data_from_file_or_mem;
            if (bytestream == nullptr)
            {
                std::wstring real_read_file;
                wo::read_virtual_source<false>(&buffer_to_store_data_from_file_or_mem, &real_read_file, wo::str_to_wstr(virtual_file), nullptr);
            }
            else
                buffer_to_store_data_from_file_or_mem = std::string((const char*)bytestream, streamsz);

            binary_source_stream buf(buffer_to_store_data_from_file_or_mem.data(), buffer_to_store_data_from_file_or_mem.size());
            return _create_from_stream(&buf, stack_count, out_reason, out_is_binary);
        }
    };

    class ir_compiler
    {
        // IR COMPILER:
        /*
        *
        */

        friend struct vmbase;

    private:

        struct ir_command
        {
            instruct::opcode opcode = instruct::nop;

            opnum::opnumbase* op1 = nullptr;
            opnum::opnumbase* op2 = nullptr;

            int32_t opinteger;

            uint8_t ext_page_id;
            union
            {
                instruct::extern_opcode_page_0 ext_opcode_p0;
                instruct::extern_opcode_page_1 ext_opcode_p1;
                instruct::extern_opcode_page_2 ext_opcode_p2;
                instruct::extern_opcode_page_3 ext_opcode_p3;
            };

#define WO_IS_REG(OPNUM)(dynamic_cast<opnum::reg*>(OPNUM))
            uint8_t dr()
            {
                return (WO_IS_REG(op1) ? 0b00000010 : 0) | (WO_IS_REG(op2) ? 0b00000001 : 0);
            }
#undef WO_IS_REG
        };

        cxx_vec_t<ir_command> ir_command_buffer;
        std::map<size_t, cxx_vec_t<std::string>> tag_irbuffer_offset;

        runtime_env::extern_native_functions_t extern_native_functions;
        runtime_env::extern_function_map_t extern_script_functions;

        struct immless
        {
            bool operator()(const opnum::immbase* lhs, const opnum::immbase* rhs) const
            {
                return (*lhs) < (*rhs);
            }
        };

        cxx_set_t<opnum::immbase*, immless> constant_record_list;
        cxx_vec_t<opnum::global*> global_record_list;

        opnum::opnumbase* _check_and_add_const(opnum::opnumbase* _opnum)
        {
            if (auto* _immbase = dynamic_cast<opnum::immbase*>(_opnum))
            {
                if (auto fnd = constant_record_list.find(_immbase); fnd == constant_record_list.end())
                {
                    _immbase->constant_index = (int32_t)constant_record_list.size();
                    constant_record_list.insert(_immbase);
                }
                else
                {
                    // already have record.
                    _immbase->constant_index = (*fnd)->constant_index;
                }
            }
            else if (auto* _global = dynamic_cast<opnum::global*>(_opnum))
            {
                wo_assert(_global->offset >= 0);
                global_record_list.push_back(_global);
            }

            return _opnum;
        }

        cxx_vec_t<opnum::opnumbase*> _created_opnum_buffer;

        template<typename T>
        T* _created_opnum_item(const T& _opn)
        {
            auto result = new T(_opn);
            _created_opnum_buffer.push_back(result);
            return result;
        }

    public:
        rslib_extern_symbols::extern_lib_set loaded_libs;
        shared_pointer<program_debug_data_info> pdb_info = new program_debug_data_info();

        ~ir_compiler()
        {
            for (auto* created_opnum : _created_opnum_buffer)
                delete created_opnum;
        }

        size_t get_now_ip() const
        {
            return ir_command_buffer.size();
        }

        mutable unsigned int _unique_id = 0;

        std::string get_unique_tag_based_command_ip()const
        {
            return "ip_" + std::to_string(get_now_ip()) + "_" + std::to_string(_unique_id++);
        }

        void revert_code_to(size_t ip)
        {
            ir_command_buffer.resize(ip);
        }

        void record_extern_native_function(intptr_t function, const std::wstring& script_path, const std::wstring& library_name, const std::wstring& function_name)
        {
            if (extern_native_functions.find(function) == extern_native_functions.end())
            {
                auto& native_info = extern_native_functions[function];
                native_info.script_name = wo::wstr_to_str(script_path);
                native_info.library_name = wo::wstr_to_str(library_name);
                native_info.function_name = wo::wstr_to_str(function_name);
            }
        }
        void record_extern_script_function(const std::string& function_name)
        {
            // ISSUE-N221022: Function overload has been removed from woolang.
            wo_assert(extern_script_functions.find(function_name) == extern_script_functions.end());
            extern_script_functions[function_name] = get_now_ip();
        }

#define WO_OPNUM(OPNUM) (_check_and_add_const(\
        (std::is_same<meta::origin_type<decltype(OPNUM)>, opnum::opnumbase>::value)\
        ?\
        const_cast<meta::origin_type<decltype(OPNUM)>*>(&OPNUM)\
        :\
        _created_opnum_item<meta::origin_type<decltype(OPNUM)>>(OPNUM)))

        int32_t update_all_temp_regist_to_stack(size_t begin)
        {
            std::map<uint8_t, int8_t> tr_regist_mapping;

            for (size_t i = begin; i < get_now_ip(); i++)
            {
                // ir_command_buffer problem..
                auto& ircmbuf = ir_command_buffer[i];
                if (ircmbuf.opcode != instruct::calln) // calln will not use opnum but do reptr_cast, dynamic_cast is dangerous
                {
                    if (auto* op1 = dynamic_cast<opnum::reg*>(ircmbuf.op1))
                    {
                        if (op1->is_tmp_regist() && tr_regist_mapping.find(op1->id) == tr_regist_mapping.end())
                        {
                            // is temp reg 
                            size_t stack_idx = tr_regist_mapping.size();
                            tr_regist_mapping[op1->id] = (int8_t)stack_idx;
                        }
                    }
                    if (auto* op2 = dynamic_cast<opnum::reg*>(ircmbuf.op2))
                    {
                        if (op2->is_tmp_regist() && tr_regist_mapping.find(op2->id) == tr_regist_mapping.end())
                        {
                            // is temp reg 
                            size_t stack_idx = tr_regist_mapping.size();
                            tr_regist_mapping[op2->id] = (int8_t)stack_idx;
                        }
                    }
                }
            }

            wo_test(tr_regist_mapping.size() <= 64); // fast bt_offset maxim offset
            // TODO: IF FAIL, FALL BACK
            //          OR REMOVE [BP-XXX]
            int8_t maxim_offset = (int8_t)tr_regist_mapping.size();

            // ATTENTION: DO NOT USE ircmbuf AFTER THIS LINE!!!
            //            WILL INSERT SOME COMMAND BEFORE ir_command_buffer[i],
            //            ircmbuf WILL POINT TO AN INVALID PLACE.

            // analyze_finalize will product same ptr opnum, do not update the same opnum
            // TODO : Very ugly, fix it in future.
            std::set<opnum::opnumbase*> updated_opnum;

            for (size_t i = begin; i < get_now_ip(); i++)
            {
                auto* opnum1 = ir_command_buffer[i].op1;
                auto* opnum2 = ir_command_buffer[i].op2;
                size_t skip_line = 0;
                if (ir_command_buffer[i].opcode != instruct::calln)
                {
                    if (ir_command_buffer[i].opcode == instruct::lds || ir_command_buffer[i].opcode == instruct::sts)
                    {
                        auto* imm_opnum_stx_offset = dynamic_cast<opnum::immbase*>(opnum2);
                        if (imm_opnum_stx_offset)
                        {
                            auto stx_offset = imm_opnum_stx_offset->try_int();
                            if (stx_offset <= 0)
                            {
                                opnum::imm offset_stack_place(stx_offset - maxim_offset);
                                ir_command_buffer[i].op2 = WO_OPNUM(offset_stack_place);
                            }
                        }
                        else
                        {
                            // Here only get arg from stack. so here nothing todo.
                        }
                    }
                    if (auto* op1 = dynamic_cast<opnum::reg*>(opnum1); op1 && updated_opnum.find(opnum1) == updated_opnum.end())
                    {
                        if (op1->is_tmp_regist())
                            op1->id = opnum::reg::bp_offset(-tr_regist_mapping[op1->id]);
                        else if (op1->is_bp_offset() && op1->get_bp_offset() <= 0)
                        {
                            auto offseted_bp_offset = op1->get_bp_offset() - maxim_offset;
                            if (offseted_bp_offset >= -64)
                            {
                                op1->id = opnum::reg::bp_offset(offseted_bp_offset);
                            }
                            else
                            {
                                opnum::reg reg_r0(opnum::reg::r0);
                                opnum::imm imm_offset(offseted_bp_offset);

                                // out of bt_offset range, make lds ldsr
                                ir_command_buffer.insert(ir_command_buffer.begin() + i,
                                    ir_command{ instruct::lds, WO_OPNUM(reg_r0), WO_OPNUM(imm_offset) });         // lds r0, imm(real_offset)
                                op1->id = opnum::reg::r0;
                                i++;

                                if (ir_command_buffer[i].opcode != instruct::call)
                                {
                                    ir_command_buffer.insert(ir_command_buffer.begin() + i + 1,
                                        ir_command{ instruct::sts, WO_OPNUM(reg_r0), WO_OPNUM(imm_offset) });         // sts r0, imm(real_offset)

                                    ++skip_line;
                                }
                            }
                        }

                        updated_opnum.insert(opnum1);
                    }
                    if (auto* op2 = dynamic_cast<opnum::reg*>(opnum2); op2 && updated_opnum.find(opnum2) == updated_opnum.end())
                    {
                        wo_assert(ir_command_buffer[i].opcode != instruct::call);

                        if (op2->is_tmp_regist())
                            op2->id = opnum::reg::bp_offset(-tr_regist_mapping[op2->id]);
                        else if (op2->is_bp_offset() && op2->get_bp_offset() <= 0)
                        {
                            auto offseted_bp_offset = op2->get_bp_offset() - maxim_offset;
                            if (offseted_bp_offset >= -64)
                            {
                                op2->id = opnum::reg::bp_offset(offseted_bp_offset);
                            }
                            else
                            {
                                wo_test(ir_command_buffer[i].opcode != instruct::call);

                                opnum::reg reg_r1(opnum::reg::r1);
                                opnum::imm imm_offset(offseted_bp_offset);

                                // out of bt_offset range, make lds ldsr
                                ir_command_buffer.insert(ir_command_buffer.begin() + i,
                                    ir_command{ instruct::lds, WO_OPNUM(reg_r1), WO_OPNUM(imm_offset) });         // lds r0, imm(real_offset)
                                op2->id = opnum::reg::r1;
                                i++;

                                // No opcode will update opnum2, so here no need for update.
                                //if (ir_command_buffer[i].opcode != instruct::call)
                                //{
                                //    ir_command_buffer.insert(ir_command_buffer.begin() + i + 1,
                                //        ir_command{ instruct::sts, WO_OPNUM(reg_r1), WO_OPNUM(imm_offset) });         // sts r0, imm(real_offset)
                                //    ++skip_line;
                                //}
                            }
                        }

                        updated_opnum.insert(opnum2);
                    }
                }

                i += skip_line;
            }

            return (int32_t)tr_regist_mapping.size();
        }


#define WO_PUT_IR_TO_BUFFER(OPCODE, ...) ir_command_buffer.emplace_back(ir_command{OPCODE, __VA_ARGS__});


        template<typename OP1T, typename OP2T>
        void mov(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not move value to immediate.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::mov, WO_OPNUM(op1), WO_OPNUM(op2));
        }

        template<typename OP1T>
        void psh(const OP1T& op1)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                , "Argument(s) should be opnum.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::psh, WO_OPNUM(op1));
        }

        void pshn(uint16_t op1)
        {
            WO_PUT_IR_TO_BUFFER(instruct::opcode::psh, nullptr, nullptr, (uint16_t)op1);
        }

        size_t reserved_stackvalue()
        {
            WO_PUT_IR_TO_BUFFER(instruct::opcode::psh, nullptr, nullptr, (uint16_t)0);
            return get_now_ip();
        }

        void reserved_stackvalue(size_t ip, uint16_t sz)
        {
            wo_assert(ip);
            ir_command_buffer[ip - 1].opinteger = sz;
        }
        template<typename OP1T>
        void pop(const OP1T& op1)
        {
            if constexpr (std::is_integral<OP1T>::value)
            {
                wo_assert(0 <= op1 && op1 <= UINT16_MAX);
                WO_PUT_IR_TO_BUFFER(instruct::opcode::pop, nullptr, nullptr, (uint16_t)op1);
            }
            else
            {
                static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                    , "Argument(s) should be opnum.");
                static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                    "Can not pop value to immediate.");

                WO_PUT_IR_TO_BUFFER(instruct::opcode::pop, WO_OPNUM(op1));
            }
        }

        template<typename OP1T, typename OP2T>
        void addi(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not add value to immediate.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::addi, WO_OPNUM(op1), WO_OPNUM(op2));
        }

        template<typename OP1T, typename OP2T>
        void subi(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not sub value to immediate.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::subi, WO_OPNUM(op1), WO_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void muli(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not mul value to immediate.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::muli, WO_OPNUM(op1), WO_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void divi(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not div value to immediate.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::divi, WO_OPNUM(op1), WO_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void modi(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not mod value to immediate.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::modi, WO_OPNUM(op1), WO_OPNUM(op2));
        }

        template<typename OP1T, typename OP2T>
        void equr(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not add value to immediate.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::equr, WO_OPNUM(op1), WO_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void nequr(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not sub value to immediate.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::nequr, WO_OPNUM(op1), WO_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void equs(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not mul value to immediate.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::equs, WO_OPNUM(op1), WO_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void nequs(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not div value to immediate.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::nequs, WO_OPNUM(op1), WO_OPNUM(op2));
        }

        template<typename OP1T, typename OP2T>
        void addr(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not add value to immediate.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::addr, WO_OPNUM(op1), WO_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void subr(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not sub value to immediate.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::subr, WO_OPNUM(op1), WO_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void mulr(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not mul value to immediate.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::mulr, WO_OPNUM(op1), WO_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void divr(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not div value to immediate.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::divr, WO_OPNUM(op1), WO_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void modr(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not mod value to immediate.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::modr, WO_OPNUM(op1), WO_OPNUM(op2));
        }

        template<typename OP1T, typename OP2T>
        void addh(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not add value to immediate.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::addh, WO_OPNUM(op1), WO_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void subh(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not sub value to immediate.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::subh, WO_OPNUM(op1), WO_OPNUM(op2));
        }

        template<typename OP1T, typename OP2T>
        void adds(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not add value to immediate.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::adds, WO_OPNUM(op1), WO_OPNUM(op2));
        }

        template<typename OP1T, typename OP2T>
        void equb(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::equb, WO_OPNUM(op1), WO_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void nequb(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::nequb, WO_OPNUM(op1), WO_OPNUM(op2));
        }

        template<typename OP1T, typename OP2T>
        void land(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::land, WO_OPNUM(op1), WO_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void lor(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::lor, WO_OPNUM(op1), WO_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void lmov(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not move value to immediate.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::lmov, WO_OPNUM(op1), WO_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void gti(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::gti, WO_OPNUM(op1), WO_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void lti(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::lti, WO_OPNUM(op1), WO_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void egti(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::egti, WO_OPNUM(op1), WO_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void elti(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::elti, WO_OPNUM(op1), WO_OPNUM(op2));
        }

        template<typename OP1T, typename OP2T>
        void ltr(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::ltr, WO_OPNUM(op1), WO_OPNUM(op2));
        }

        template<typename OP1T, typename OP2T>
        void gtr(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::gtr, WO_OPNUM(op1), WO_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void eltr(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::eltr, WO_OPNUM(op1), WO_OPNUM(op2));
        }

        template<typename OP1T, typename OP2T>
        void egtr(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::egtr, WO_OPNUM(op1), WO_OPNUM(op2));
        }

        template<typename OP1T, typename OP2T>
        void ltx(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::ltx, WO_OPNUM(op1), WO_OPNUM(op2));
        }

        template<typename OP1T, typename OP2T>
        void gtx(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::gtx, WO_OPNUM(op1), WO_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void eltx(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::eltx, WO_OPNUM(op1), WO_OPNUM(op2));
        }

        template<typename OP1T, typename OP2T>
        void egtx(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::egtx, WO_OPNUM(op1), WO_OPNUM(op2));
        }

        template<typename OP1T, typename OP2T>
        void lds(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not load value and save to immediate.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::lds, WO_OPNUM(op1), WO_OPNUM(op2));
        }

        template<typename OP1T, typename OP2T>
        void sts(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::sts, WO_OPNUM(op1), WO_OPNUM(op2));
        }

        template<typename OP1T, typename OP2T>
        void movcast(const OP1T& op1, const OP2T& op2, value::valuetype vtt)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not set value to immediate.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::movcast, WO_OPNUM(op1), WO_OPNUM(op2), (int)vtt);
        }
        template<typename OP1T>
        void typeas(const OP1T& op1, value::valuetype vtt)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value,
                "Argument(s) should be opnum.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::typeas, WO_OPNUM(op1), nullptr, (int)vtt);
        }
        template<typename OP1T>
        void typeis(const OP1T& op1, value::valuetype vtt)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value,
                "Argument(s) should be opnum.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::typeas, WO_OPNUM(op1), nullptr, (int)vtt, 1);
        }

        template<typename OP1T>
        void call(const OP1T& op1)
        {
            if constexpr (std::is_base_of<opnum::opnumbase, OP1T>::value)
            {
                if (dynamic_cast<opnum::tag*>(const_cast<OP1T*>(&op1)))
                {
                    WO_PUT_IR_TO_BUFFER(instruct::opcode::calln, nullptr, WO_OPNUM(op1));
                }
                else if (auto* handle_exp = dynamic_cast<opnum::imm<void*>*>(const_cast<OP1T*>(&op1)))
                {
                    WO_PUT_IR_TO_BUFFER(instruct::opcode::calln, reinterpret_cast<opnum::opnumbase*>(handle_exp->val));
                }
                else
                {
                    WO_PUT_IR_TO_BUFFER(instruct::opcode::call, WO_OPNUM(op1));
                }
            }
            else if constexpr (std::is_pointer<OP1T>::value)
            {
                WO_PUT_IR_TO_BUFFER(instruct::opcode::calln, reinterpret_cast<opnum::opnumbase*>(op1));
            }
            else if constexpr (std::is_integral<OP1T>::value)
            {
                wo_assert(0 <= op1 && op1 <= UINT32_MAX, "Immediate instruct address is to large to call.");

                uint32_t address = (uint32_t)op1;
                WO_PUT_IR_TO_BUFFER(instruct::opcode::calln, nullptr, nullptr, static_cast<int32_t>(op1));
            }
            else
            {
                static_assert(
                    !std::is_base_of<opnum::tag, OP1T>::value
                    && !std::is_base_of<opnum::opnumbase, OP1T>::value
                    && !std::is_pointer<OP1T>::value
                    && !std::is_integral<OP1T>::value
                    , "Argument(s) should be opnum or integer.");
            }
        }

        void jt(const opnum::tag& op1)
        {
            WO_PUT_IR_TO_BUFFER(instruct::opcode::jt, WO_OPNUM(op1));
        }

        void jf(const opnum::tag& op1)
        {
            WO_PUT_IR_TO_BUFFER(instruct::opcode::jf, WO_OPNUM(op1));
        }

        void jmp(const opnum::tag& op1)
        {
            WO_PUT_IR_TO_BUFFER(instruct::opcode::jmp, WO_OPNUM(op1));
        }

        void ret()
        {
            WO_PUT_IR_TO_BUFFER(instruct::opcode::ret);
        }

        void ret(uint16_t popcount)
        {
            WO_PUT_IR_TO_BUFFER(instruct::opcode::ret, nullptr, nullptr, popcount);
        }

        void mkclos(uint16_t capture_count, const opnum::tag& wrapped_func)
        {
            auto& codeb = WO_PUT_IR_TO_BUFFER(instruct::opcode::mkclos, WO_OPNUM(wrapped_func), nullptr, (int32_t)capture_count);
        }

        template<typename OP1T>
        void mkstruct(const OP1T& op1, uint16_t size)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value,
                "Argument(s) should be opnum.");
            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not move value to immediate.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::mkstruct, WO_OPNUM(op1), nullptr, (int32_t)size);
        }
        template<typename OP1T, typename OP2T>
        void idstruct(const OP1T& op1, const OP2T& op2, uint16_t offset)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not move value to immediate.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::idstruct, WO_OPNUM(op1), WO_OPNUM(op2), (int32_t)offset);
        }
        template<typename OP1T>
        void jnequb(const OP1T& op1, const opnum::tag& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value,
                "Argument(s) should be opnum.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::jnequb, WO_OPNUM(op1), WO_OPNUM(op2));
        }

        void nop()
        {
            WO_PUT_IR_TO_BUFFER(instruct::opcode::nop);
        }

        void abrt()
        {
            WO_PUT_IR_TO_BUFFER(instruct::opcode::abrt);
        }

        void end()
        {
            WO_PUT_IR_TO_BUFFER(instruct::opcode::abrt, nullptr, nullptr, 1);
        }

        void tag(const string_t& tagname)
        {
            tag_irbuffer_offset[ir_command_buffer.size()].push_back(tagname);
        }

        template<typename OP1T, typename OP2T>
        void mkunion(const OP1T& op1, const OP2T& op2, uint16_t id)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");
            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not move value to immediate.");

            auto& codeb = WO_PUT_IR_TO_BUFFER(instruct::opcode::mkunion, WO_OPNUM(op1), WO_OPNUM(op2), (int32_t)id);
        }

        template<typename OP1T>
        void ext_panic(const OP1T& op1)
        {
            auto& codeb = WO_PUT_IR_TO_BUFFER(instruct::opcode::ext, WO_OPNUM(op1));
            codeb.ext_page_id = 0;
            codeb.ext_opcode_p0 = instruct::extern_opcode_page_0::panic;
        }

        template<typename OP1T>
        void mkarr(const OP1T& op1, uint16_t size)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not set value to immediate.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::mkarr, WO_OPNUM(op1), nullptr, (int32_t)size);
        }
        template<typename OP1T>
        void mkmap(const OP1T& op1, uint16_t size)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not set value to immediate.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::mkmap, WO_OPNUM(op1), nullptr, (int32_t)size);
        }
        template<typename OP1T, typename OP2T>
        void idarr(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::idarr, WO_OPNUM(op1), WO_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void iddict(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::iddict, WO_OPNUM(op1), WO_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void siddict(const OP1T& op1, const OP2T& op2, const opnum::reg& r)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::siddict, WO_OPNUM(op1), WO_OPNUM(op2), (int32_t)r.id);
        }
        template<typename OP1T, typename OP2T>
        void sidarr(const OP1T& op1, const OP2T& op2, const opnum::reg& r)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::sidarr, WO_OPNUM(op1), WO_OPNUM(op2), (int32_t)r.id);
        }
        template<typename OP1T, typename OP2T>
        void sidstruct(const OP1T& op1, const OP2T& op2, uint16_t offset)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::sidstruct, WO_OPNUM(op1), WO_OPNUM(op2), (int32_t)offset);
        }

        template<typename OP1T, typename OP2T>
        void idstr(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::idstr, WO_OPNUM(op1), WO_OPNUM(op2));
        }

        void ext_funcbegin()
        {
            auto& codeb = WO_PUT_IR_TO_BUFFER(instruct::opcode::ext);
            codeb.ext_page_id = 3;
            codeb.ext_opcode_p3 = instruct::extern_opcode_page_3::funcbegin;
        }
        void ext_funcend()
        {
            auto& codeb = WO_PUT_IR_TO_BUFFER(instruct::opcode::ext);
            codeb.ext_page_id = 3;
            codeb.ext_opcode_p3 = instruct::extern_opcode_page_3::funcend;
        }

        template<typename OP1T, typename OP2T>
        void ext_packargs(const OP1T& op1, const OP2T& op2, uint16_t skipclosure)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value &&
                std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            auto& codeb = WO_PUT_IR_TO_BUFFER(instruct::opcode::ext, WO_OPNUM(op1), WO_OPNUM(op2), skipclosure);
            codeb.ext_page_id = 0;
            codeb.ext_opcode_p0 = instruct::extern_opcode_page_0::packargs;
        }

        template<typename OP1T>
        void ext_unpackargs(const OP1T& op1, wo_integer_t unpack_count)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value,
                "Argument(s) should be opnum.");

            opnum::imm<wo_integer_t> unpacked_count(unpack_count);

            auto& codeb = WO_PUT_IR_TO_BUFFER(instruct::opcode::ext, WO_OPNUM(op1), WO_OPNUM(unpacked_count));
            codeb.ext_page_id = 0;
            codeb.ext_opcode_p0 = instruct::extern_opcode_page_0::unpackargs;
        }

#undef WO_OPNUM
#undef WO_PUT_IR_TO_BUFFER

        shared_pointer<runtime_env> finalize(size_t stacksz)
        {
            // 1. Generate constant & global & register & runtime_stack memory buffer
            size_t constant_value_count = constant_record_list.size();
            size_t global_allign_takeplace_for_avoiding_false_shared =
                config::ENABLE_AVOIDING_FALSE_SHARED ?
                ((size_t)(platform_info::CPU_CACHELINE_SIZE / (double)sizeof(wo::value) + 0.5)) : (1);
            size_t global_value_count = 0;

            for (auto* global_opnum : global_record_list)
            {
                wo_assert(global_opnum->offset + constant_value_count + global_allign_takeplace_for_avoiding_false_shared
                    < INT32_MAX&& global_opnum->offset >= 0);
                global_opnum->real_offset_const_glb = (int32_t)
                    (global_opnum->offset * global_allign_takeplace_for_avoiding_false_shared
                        + constant_value_count + global_allign_takeplace_for_avoiding_false_shared);

                if (((size_t)global_opnum->offset + 1) > global_value_count)
                    global_value_count = (size_t)global_opnum->offset + 1;
            }

            size_t real_register_count = 64;     // t0-t15 r0-r15 (32) special reg (32)
            size_t runtime_stack_count = 1024;  // by default

            size_t preserve_memory_size =
                constant_value_count
                + global_allign_takeplace_for_avoiding_false_shared
                + global_value_count * global_allign_takeplace_for_avoiding_false_shared
                + global_allign_takeplace_for_avoiding_false_shared
                + real_register_count
                + runtime_stack_count;

            value* preserved_memory = (value*)alloc64(preserve_memory_size * sizeof(value));

            cxx_vec_t<byte_t> generated_runtime_code_buf; // It will be put to 16 byte allign mem place.

            std::map<std::string, cxx_vec_t<size_t>> jmp_record_table;
            std::map<std::string, cxx_vec_t<value*>> jmp_record_table_for_immtag;
            std::map<std::string, uint32_t> tag_offset_vector_table;

            wo_assert(preserved_memory, "Alloc memory fail.");

            memset(preserved_memory, 0, preserve_memory_size * sizeof(value));
            //  // Fill constant
            for (auto constant_record : constant_record_list)
            {
                wo_assert((size_t)constant_record->constant_index < constant_value_count,
                    "Constant index out of range.");

                constant_record->apply(&preserved_memory[constant_record->constant_index]);

                if (auto* addr_tagimm_rsfunc = dynamic_cast<opnum::tagimm_rsfunc*>(constant_record))
                {
                    jmp_record_table_for_immtag[addr_tagimm_rsfunc->name].push_back(&preserved_memory[constant_record->constant_index]);
                }
            }

            // 2. Generate code
            shared_pointer<runtime_env> env = new runtime_env();

            for (size_t ip = 0; ip < ir_command_buffer.size(); ip++)
            {

#define WO_IR ir_command_buffer[ip]
#define WO_OPCODE(...) wo_macro_overload(WO_OPCODE,__VA_ARGS__)
#define WO_OPCODE_1(OPCODE) (instruct(instruct::opcode::OPCODE, WO_IR.dr()).opcode_dr)
#define WO_OPCODE_2(OPCODE,DR) (instruct(instruct::opcode::OPCODE, 0b000000##DR).opcode_dr)

#define WO_OPCODE_EXT0(...) wo_macro_overload(WO_OPCODE_EXT0,__VA_ARGS__)
#define WO_OPCODE_EXT0_1(OPCODE) (instruct((instruct::opcode)instruct::extern_opcode_page_0::OPCODE, WO_IR.dr()).opcode_dr)
#define WO_OPCODE_EXT0_2(OPCODE,DR) (instruct((instruct::opcode)instruct::extern_opcode_page_0::OPCODE, 0b000000##DR).opcode_dr)


#define WO_OPCODE_EXT1(...) wo_macro_overload(WO_OPCODE_EXT1,__VA_ARGS__)
#define WO_OPCODE_EXT1_1(OPCODE) (instruct((instruct::opcode)instruct::extern_opcode_page_1::OPCODE, WO_IR.dr()).opcode_dr)
#define WO_OPCODE_EXT1_2(OPCODE,DR) (instruct((instruct::opcode)instruct::extern_opcode_page_1::OPCODE, 0b000000##DR).opcode_dr)

#define WO_OPCODE_EXT2(...) wo_macro_overload(WO_OPCODE_EXT2,__VA_ARGS__)
#define WO_OPCODE_EXT2_1(OPCODE) (instruct((instruct::opcode)instruct::extern_opcode_page_2::OPCODE, WO_IR.dr()).opcode_dr)
#define WO_OPCODE_EXT2_2(OPCODE,DR) (instruct((instruct::opcode)instruct::extern_opcode_page_2::OPCODE, 0b000000##DR).opcode_dr)

#define WO_OPCODE_EXT3(...) wo_macro_overload(WO_OPCODE_EXT3,__VA_ARGS__)
#define WO_OPCODE_EXT3_1(OPCODE) (instruct((instruct::opcode)instruct::extern_opcode_page_3::OPCODE, WO_IR.dr()).opcode_dr)
#define WO_OPCODE_EXT3_2(OPCODE,DR) (instruct((instruct::opcode)instruct::extern_opcode_page_3::OPCODE, 0b000000##DR).opcode_dr)

                cxx_vec_t<byte_t> temp_this_command_code_buf; // one command will be store here tempery for coding allign

                switch (WO_IR.opcode)
                {
                case instruct::opcode::nop:
                    temp_this_command_code_buf.push_back(WO_OPCODE(nop));
                    break;
                case instruct::opcode::mov:
                    temp_this_command_code_buf.push_back(WO_OPCODE(mov));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                    break;
                case instruct::opcode::movcast:
                    temp_this_command_code_buf.push_back(WO_OPCODE(movcast));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                    temp_this_command_code_buf.push_back((byte_t)WO_IR.opinteger);
                    break;
                case instruct::opcode::typeas:

                    if (WO_IR.ext_page_id)
                    {
                        temp_this_command_code_buf.push_back(WO_OPCODE(typeas) | 0b01);
                        WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                        temp_this_command_code_buf.push_back((byte_t)WO_IR.opinteger);
                    }
                    else
                    {
                        temp_this_command_code_buf.push_back(WO_OPCODE(typeas));
                        WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                        temp_this_command_code_buf.push_back((byte_t)WO_IR.opinteger);
                    }
                    break;
                case instruct::opcode::psh:
                    if (nullptr == WO_IR.op1)
                    {
                        if (WO_IR.opinteger == 0)
                            break;

                        temp_this_command_code_buf.push_back(WO_OPCODE(psh, 00));

                        wo_assert(WO_IR.opinteger > 0 && WO_IR.opinteger <= UINT16_MAX
                            , "Invalid count to reserve in stack.");

                        uint16_t opushort = (uint16_t)WO_IR.opinteger;
                        byte_t* readptr = (byte_t*)&opushort;
                        temp_this_command_code_buf.push_back(readptr[0]);
                        temp_this_command_code_buf.push_back(readptr[1]);
                    }
                    else
                    {
                        temp_this_command_code_buf.push_back(WO_OPCODE(psh) | 0b01);
                        WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    }
                    break;
                case instruct::opcode::pop:
                    if (nullptr == WO_IR.op1)
                    {
                        if (WO_IR.opinteger == 0)
                            break;

                        temp_this_command_code_buf.push_back(WO_OPCODE(pop, 00));

                        wo_assert(WO_IR.opinteger > 0 && WO_IR.opinteger <= UINT16_MAX
                            , "Invalid count to pop from stack.");

                        uint16_t opushort = (uint16_t)WO_IR.opinteger;
                        byte_t* readptr = (byte_t*)&opushort;

                        temp_this_command_code_buf.push_back(readptr[0]);
                        temp_this_command_code_buf.push_back(readptr[1]);
                    }
                    else
                    {
                        temp_this_command_code_buf.push_back(WO_OPCODE(pop) | 0b01);
                        WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    }
                    break;
                case instruct::opcode::lds:
                    temp_this_command_code_buf.push_back(WO_OPCODE(lds));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                    break;
                case instruct::opcode::sts:
                    temp_this_command_code_buf.push_back(WO_OPCODE(sts));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                    break;
                case instruct::opcode::addi:
                    temp_this_command_code_buf.push_back(WO_OPCODE(addi));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                    break;
                case instruct::opcode::subi:
                    temp_this_command_code_buf.push_back(WO_OPCODE(subi));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                    break;
                case instruct::opcode::muli:
                    temp_this_command_code_buf.push_back(WO_OPCODE(muli));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                    break;
                case instruct::opcode::divi:
                    temp_this_command_code_buf.push_back(WO_OPCODE(divi));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                    break;
                case instruct::opcode::modi:
                    temp_this_command_code_buf.push_back(WO_OPCODE(modi));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                    break;
                case instruct::opcode::equr:
                    temp_this_command_code_buf.push_back(WO_OPCODE(equr));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);

                    break;
                case instruct::opcode::nequr:
                    temp_this_command_code_buf.push_back(WO_OPCODE(nequr));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);

                    break;
                case instruct::opcode::equs:
                    temp_this_command_code_buf.push_back(WO_OPCODE(equs));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);

                    break;
                case instruct::opcode::nequs:
                    temp_this_command_code_buf.push_back(WO_OPCODE(nequs));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);

                    break;
                case instruct::opcode::addr:
                    temp_this_command_code_buf.push_back(WO_OPCODE(addr));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                    break;
                case instruct::opcode::subr:
                    temp_this_command_code_buf.push_back(WO_OPCODE(subr));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                    break;
                case instruct::opcode::mulr:
                    temp_this_command_code_buf.push_back(WO_OPCODE(mulr));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                    break;
                case instruct::opcode::divr:
                    temp_this_command_code_buf.push_back(WO_OPCODE(divr));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                    break;
                case instruct::opcode::modr:
                    temp_this_command_code_buf.push_back(WO_OPCODE(modr));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                    break;
                case instruct::opcode::addh:
                    temp_this_command_code_buf.push_back(WO_OPCODE(addh));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                    break;
                case instruct::opcode::subh:
                    temp_this_command_code_buf.push_back(WO_OPCODE(subh));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                    break;
                case instruct::opcode::adds:
                    temp_this_command_code_buf.push_back(WO_OPCODE(adds));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                    break;

                case instruct::opcode::equb:
                    temp_this_command_code_buf.push_back(WO_OPCODE(equb));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                    break;
                case instruct::opcode::nequb:
                    temp_this_command_code_buf.push_back(WO_OPCODE(nequb));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                    break;
                case instruct::opcode::land:
                    temp_this_command_code_buf.push_back(WO_OPCODE(land));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                    break;
                case instruct::opcode::lor:
                    temp_this_command_code_buf.push_back(WO_OPCODE(lor));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                    break;
                case instruct::opcode::lmov:
                    temp_this_command_code_buf.push_back(WO_OPCODE(lmov));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                    break;

                case instruct::opcode::gti:
                    temp_this_command_code_buf.push_back(WO_OPCODE(gti));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                    break;
                case instruct::opcode::lti:
                    temp_this_command_code_buf.push_back(WO_OPCODE(lti));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                    break;
                case instruct::opcode::egti:
                    temp_this_command_code_buf.push_back(WO_OPCODE(egti));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                    break;
                case instruct::opcode::elti:
                    temp_this_command_code_buf.push_back(WO_OPCODE(elti));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                    break;

                case instruct::opcode::gtr:
                    temp_this_command_code_buf.push_back(WO_OPCODE(gtr));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                    break;
                case instruct::opcode::ltr:
                    temp_this_command_code_buf.push_back(WO_OPCODE(ltr));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                    break;
                case instruct::opcode::egtr:
                    temp_this_command_code_buf.push_back(WO_OPCODE(egtr));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                    break;
                case instruct::opcode::eltr:
                    temp_this_command_code_buf.push_back(WO_OPCODE(eltr));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                    break;

                case instruct::opcode::gtx:
                    temp_this_command_code_buf.push_back(WO_OPCODE(gtx));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                    break;
                case instruct::opcode::ltx:
                    temp_this_command_code_buf.push_back(WO_OPCODE(ltx));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                    break;
                case instruct::opcode::egtx:
                    temp_this_command_code_buf.push_back(WO_OPCODE(egtx));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                    break;
                case instruct::opcode::eltx:
                    temp_this_command_code_buf.push_back(WO_OPCODE(eltx));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                    break;
                case instruct::opcode::mkstruct:
                {
                    temp_this_command_code_buf.push_back(WO_OPCODE(mkstruct));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);

                    uint16_t size = (uint16_t)(WO_IR.opinteger);
                    byte_t* readptr = (byte_t*)&size;
                    temp_this_command_code_buf.push_back(readptr[0]);
                    temp_this_command_code_buf.push_back(readptr[1]);
                    break;
                }
                case instruct::opcode::idstruct:
                {
                    temp_this_command_code_buf.push_back(WO_OPCODE(idstruct));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);

                    uint16_t size = (uint16_t)(WO_IR.opinteger);
                    byte_t* readptr = (byte_t*)&size;
                    temp_this_command_code_buf.push_back(readptr[0]);
                    temp_this_command_code_buf.push_back(readptr[1]);
                    break;
                }
                case instruct::opcode::mkarr:
                {
                    temp_this_command_code_buf.push_back(WO_OPCODE(mkarr));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);

                    uint16_t size = (uint16_t)(WO_IR.opinteger);
                    byte_t* readptr = (byte_t*)&size;
                    temp_this_command_code_buf.push_back(readptr[0]);
                    temp_this_command_code_buf.push_back(readptr[1]);
                    break;
                }
                case instruct::opcode::mkmap:
                {
                    temp_this_command_code_buf.push_back(WO_OPCODE(mkmap));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);

                    uint16_t size = (uint16_t)(WO_IR.opinteger);
                    byte_t* readptr = (byte_t*)&size;
                    temp_this_command_code_buf.push_back(readptr[0]);
                    temp_this_command_code_buf.push_back(readptr[1]);
                    break;
                }
                case instruct::opcode::idarr:
                    temp_this_command_code_buf.push_back(WO_OPCODE(idarr));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                    break;
                case instruct::opcode::iddict:
                    temp_this_command_code_buf.push_back(WO_OPCODE(iddict));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                    break;
                case instruct::opcode::idstr:
                    temp_this_command_code_buf.push_back(WO_OPCODE(idstr));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                    break;
                case instruct::opcode::siddict:
                    temp_this_command_code_buf.push_back(WO_OPCODE(siddict));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                    opnum::reg((uint8_t)WO_IR.opinteger).generate_opnum_to_buffer(temp_this_command_code_buf);
                    break;
                case instruct::opcode::sidarr:
                    temp_this_command_code_buf.push_back(WO_OPCODE(sidarr));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                    opnum::reg((uint8_t)WO_IR.opinteger).generate_opnum_to_buffer(temp_this_command_code_buf);
                    break;
                case instruct::opcode::sidstruct:
                {
                    temp_this_command_code_buf.push_back(WO_OPCODE(sidstruct));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);

                    uint16_t size = (uint16_t)(WO_IR.opinteger);
                    byte_t* readptr = (byte_t*)&size;
                    temp_this_command_code_buf.push_back(readptr[0]);
                    temp_this_command_code_buf.push_back(readptr[1]);
                    break;
                }
                case instruct::opcode::jt:
                    temp_this_command_code_buf.push_back(WO_OPCODE(jt));

                    wo_assert(dynamic_cast<opnum::tag*>(WO_IR.op1) != nullptr, "Operator num should be a tag.");

                    jmp_record_table[dynamic_cast<opnum::tag*>(WO_IR.op1)->name]
                        .push_back(generated_runtime_code_buf.size() + 1);

                    temp_this_command_code_buf.push_back(0x00);
                    temp_this_command_code_buf.push_back(0x00);
                    temp_this_command_code_buf.push_back(0x00);
                    temp_this_command_code_buf.push_back(0x00);


                    break;
                case instruct::opcode::jf:
                    temp_this_command_code_buf.push_back(WO_OPCODE(jf));

                    wo_assert(dynamic_cast<opnum::tag*>(WO_IR.op1) != nullptr, "Operator num should be a tag.");

                    jmp_record_table[dynamic_cast<opnum::tag*>(WO_IR.op1)->name]
                        .push_back(generated_runtime_code_buf.size() + 1);

                    temp_this_command_code_buf.push_back(0x00);
                    temp_this_command_code_buf.push_back(0x00);
                    temp_this_command_code_buf.push_back(0x00);
                    temp_this_command_code_buf.push_back(0x00);
                    break;

                case instruct::opcode::jmp:
                    temp_this_command_code_buf.push_back(WO_OPCODE(jmp, 00));

                    wo_assert(dynamic_cast<opnum::tag*>(WO_IR.op1) != nullptr, "Operator num should be a tag.");

                    jmp_record_table[dynamic_cast<opnum::tag*>(WO_IR.op1)->name]
                        .push_back(generated_runtime_code_buf.size() + 1);

                    temp_this_command_code_buf.push_back(0x00);
                    temp_this_command_code_buf.push_back(0x00);
                    temp_this_command_code_buf.push_back(0x00);
                    temp_this_command_code_buf.push_back(0x00);
                    break;

                case instruct::opcode::call:
                    temp_this_command_code_buf.push_back(WO_OPCODE(call));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    break;
                case instruct::opcode::calln:
                    if (WO_IR.op2)
                    {
                        if (config::ENABLE_JUST_IN_TIME)
                            env->_calln_opcode_offsets.push_back(generated_runtime_code_buf.size());

                        wo_assert(dynamic_cast<opnum::tag*>(WO_IR.op2) != nullptr, "Operator num should be a tag.");

                        temp_this_command_code_buf.push_back(WO_OPCODE(calln, 00));

                        jmp_record_table[dynamic_cast<opnum::tag*>(WO_IR.op2)->name]
                            .push_back(generated_runtime_code_buf.size() + 1);

                        temp_this_command_code_buf.push_back(0x00);
                        temp_this_command_code_buf.push_back(0x00);
                        temp_this_command_code_buf.push_back(0x00);
                        temp_this_command_code_buf.push_back(0x00);

                        // reserve...
                        temp_this_command_code_buf.push_back(0x00);
                        temp_this_command_code_buf.push_back(0x00);
                        temp_this_command_code_buf.push_back(0x00);
                        temp_this_command_code_buf.push_back(0x00);
                    }
                    else if (WO_IR.op1)
                    {
                        temp_this_command_code_buf.push_back(WO_OPCODE(calln, 01));

                        uint64_t addr = (uint64_t)(WO_IR.op1);

                        wo_assert(!extern_native_functions[(intptr_t)addr].function_name.empty());
                        extern_native_functions[(intptr_t)addr].caller_offset_in_ir
                            .push_back(generated_runtime_code_buf.size());

                        byte_t* readptr = (byte_t*)&addr;
                        temp_this_command_code_buf.push_back(readptr[0]);
                        temp_this_command_code_buf.push_back(readptr[1]);
                        temp_this_command_code_buf.push_back(readptr[2]);
                        temp_this_command_code_buf.push_back(readptr[3]);
                        temp_this_command_code_buf.push_back(readptr[4]);
                        temp_this_command_code_buf.push_back(readptr[5]);
                        temp_this_command_code_buf.push_back(readptr[6]);
                        temp_this_command_code_buf.push_back(readptr[7]);

                    }
                    else
                    {
                        if (config::ENABLE_JUST_IN_TIME)
                            env->_calln_opcode_offsets.push_back(generated_runtime_code_buf.size());

                        temp_this_command_code_buf.push_back(WO_OPCODE(calln, 00));

                        byte_t* readptr = (byte_t*)&WO_IR.opinteger;
                        temp_this_command_code_buf.push_back(readptr[0]);
                        temp_this_command_code_buf.push_back(readptr[1]);
                        temp_this_command_code_buf.push_back(readptr[2]);
                        temp_this_command_code_buf.push_back(readptr[3]);

                        // reserve...
                        temp_this_command_code_buf.push_back(0x00);
                        temp_this_command_code_buf.push_back(0x00);
                        temp_this_command_code_buf.push_back(0x00);
                        temp_this_command_code_buf.push_back(0x00);
                    }
                    break;
                case instruct::opcode::ret:
                    if (WO_IR.opinteger)
                    {
                        // ret pop n
                        temp_this_command_code_buf.push_back(WO_OPCODE(ret, 10));

                        uint16_t pop_count = (uint16_t)WO_IR.opinteger;
                        byte_t* readptr = (byte_t*)&pop_count;
                        temp_this_command_code_buf.push_back(readptr[0]);
                        temp_this_command_code_buf.push_back(readptr[1]);
                    }
                    else
                        temp_this_command_code_buf.push_back(WO_OPCODE(ret, 00));
                    break;
                case instruct::opcode::jnequb:
                {
                    wo_assert(dynamic_cast<opnum::tag*>(WO_IR.op2) != nullptr, "Operator num should be a tag.");

                    temp_this_command_code_buf.push_back(WO_OPCODE(jnequb));
                    size_t opcodelen = WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    opcodelen;

                    // Write jmp

                    jmp_record_table[dynamic_cast<opnum::tag*>(WO_IR.op2)->name]
                        .push_back(generated_runtime_code_buf.size() + 1 + opcodelen);
                    temp_this_command_code_buf.push_back(0x00);
                    temp_this_command_code_buf.push_back(0x00);
                    temp_this_command_code_buf.push_back(0x00);
                    temp_this_command_code_buf.push_back(0x00);

                    break;
                }
                case instruct::mkunion:
                {
                    temp_this_command_code_buf.push_back(WO_OPCODE(mkunion));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);

                    uint16_t id = (uint16_t)WO_IR.opinteger;
                    byte_t* readptr = (byte_t*)&id;
                    temp_this_command_code_buf.push_back(readptr[0]);
                    temp_this_command_code_buf.push_back(readptr[1]);
                    break;
                }
                case instruct::mkclos:
                {
                    temp_this_command_code_buf.push_back(WO_OPCODE(mkclos, 00));

                    uint16_t capture_count = (uint16_t)WO_IR.opinteger;
                    byte_t* readptr = (byte_t*)&capture_count;
                    temp_this_command_code_buf.push_back(readptr[0]);
                    temp_this_command_code_buf.push_back(readptr[1]);

                    jmp_record_table[dynamic_cast<opnum::tag*>(WO_IR.op1)->name]
                        .push_back(generated_runtime_code_buf.size() + 1 + 2);
                    temp_this_command_code_buf.push_back(0x00);
                    temp_this_command_code_buf.push_back(0x00);
                    temp_this_command_code_buf.push_back(0x00);
                    temp_this_command_code_buf.push_back(0x00);

                    // reserve...
                    temp_this_command_code_buf.push_back(0x00);
                    temp_this_command_code_buf.push_back(0x00);
                    temp_this_command_code_buf.push_back(0x00);
                    temp_this_command_code_buf.push_back(0x00);
                    break;
                }
                case instruct::opcode::ext:

                    switch (WO_IR.ext_page_id)
                    {
                    case 0:
                    {
                        temp_this_command_code_buf.push_back(WO_OPCODE(ext, 00));
                        switch (WO_IR.ext_opcode_p0)
                        {
                        case instruct::extern_opcode_page_0::packargs:
                        {
                            temp_this_command_code_buf.push_back(WO_OPCODE_EXT0(packargs));

                            uint16_t skip_count = (uint16_t)WO_IR.opinteger;
                            byte_t* readptr = (byte_t*)&skip_count;
                            temp_this_command_code_buf.push_back(readptr[0]);
                            temp_this_command_code_buf.push_back(readptr[1]);

                            WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                            WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                            break;
                        }
                        case instruct::extern_opcode_page_0::unpackargs:
                            temp_this_command_code_buf.push_back(WO_OPCODE_EXT0(unpackargs));
                            WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                            WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                            break;
                        case instruct::extern_opcode_page_0::panic:
                        {
                            temp_this_command_code_buf.push_back(WO_OPCODE_EXT0(panic));
                            WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                            break;
                        }
                        default:
                            wo_error("Unknown instruct.");
                            break;
                        }
                        break;
                    }
                    //case 1:
                    //{
                    //    temp_this_command_code_buf.push_back(WO_OPCODE(ext, 01));
                    //    switch (WO_IR.ext_opcode_p1)
                    //    {
                    //    default:
                    //        wo_error("Unknown instruct.");
                    //        break;
                    //    }
                    //    break;
                    //}
                    //case 2:
                    //{
                    //    temp_this_command_code_buf.push_back(WO_OPCODE(ext, 10));
                    //    switch (WO_IR.ext_opcode_p2)
                    //    {
                    //    default:
                    //        wo_error("Unknown instruct.");
                    //        break;
                    //    }
                    //    break;
                    //}
                    case 3:
                    {
                        temp_this_command_code_buf.push_back(WO_OPCODE(ext, 11));
                        switch (WO_IR.ext_opcode_p3)
                        {
                        case instruct::extern_opcode_page_3::funcbegin:
                            temp_this_command_code_buf.push_back(WO_OPCODE_EXT3(funcbegin));
                            env->_functions_offsets.push_back(
                                temp_this_command_code_buf.size()
                                + generated_runtime_code_buf.size());
                            break;
                        case instruct::extern_opcode_page_3::funcend:
                            temp_this_command_code_buf.push_back(WO_OPCODE_EXT3(funcend));
                            break;
                        default:
                            wo_error("Unknown instruct.");
                            break;
                        }
                        break;
                    }
                    default:
                        wo_error("Unknown extern-opcode-page.");
                        break;
                    }

                    break;
                case instruct::opcode::abrt:
                    if (WO_IR.opinteger)
                        temp_this_command_code_buf.push_back(WO_OPCODE(abrt, 10));
                    else
                        temp_this_command_code_buf.push_back(WO_OPCODE(abrt, 00));
                    break;

                default:
                    wo_error("Unknown instruct.");
                }

                pdb_info->pdd_rt_code_byte_offset_to_ir[generated_runtime_code_buf.size()] = ip;

                if (auto fnd = tag_irbuffer_offset.find(ip); fnd != tag_irbuffer_offset.end())
                {
                    for (auto& tag_name : fnd->second)
                    {
                        wo_assert(tag_offset_vector_table.find(tag_name) == tag_offset_vector_table.end()
                            , "The tag point to different place.");

                        tag_offset_vector_table[tag_name] = (uint32_t)generated_runtime_code_buf.size();
                    }
                }


                generated_runtime_code_buf.insert(
                    generated_runtime_code_buf.end(),
                    temp_this_command_code_buf.begin(),
                    temp_this_command_code_buf.end());

            }
#undef WO_OPCODE_2
#undef WO_OPCODE_1
#undef WO_OPCODE
#undef WO_IR
            // 3. Generate tag

            for (auto& [tag, offsets] : jmp_record_table)
            {
                uint32_t offset_val = tag_offset_vector_table[tag];
                for (auto offset : offsets)
                {
                    *(uint32_t*)(generated_runtime_code_buf.data() + offset) = offset_val;
                }
            }

            for (auto& [tag, imm_values] : jmp_record_table_for_immtag)
            {
                uint32_t offset_val = tag_offset_vector_table[tag];
                for (auto imm_value : imm_values)
                {
                    wo_assert(imm_value->type == value::valuetype::integer_type);
                    imm_value->integer = (wo_integer_t)offset_val;
                }
            }

            env->constant_global_reg_rtstack = preserved_memory;

            env->constant_value_count = constant_value_count;
            env->constant_and_global_value_takeplace_count =
                constant_value_count
                + global_value_count * global_allign_takeplace_for_avoiding_false_shared
                + 2 * global_allign_takeplace_for_avoiding_false_shared;


            env->reg_begin = env->constant_global_reg_rtstack
                + env->constant_and_global_value_takeplace_count;

            env->stack_begin = env->constant_global_reg_rtstack + (preserve_memory_size - 1);
            env->real_register_count = real_register_count;
            env->runtime_stack_count = runtime_stack_count;
            env->rt_code_len = generated_runtime_code_buf.size();
            byte_t* code_buf = (byte_t*)alloc64(env->rt_code_len * sizeof(byte_t));

            wo_test(reinterpret_cast<size_t>(code_buf) % 8 == 0);
            pdb_info->runtime_codes_length = env->rt_code_len;

            wo_assert(code_buf, "Alloc memory fail.");
            memcpy(code_buf, generated_runtime_code_buf.data(), env->rt_code_len * sizeof(byte_t));

            for (auto& [extern_func_name, extern_func_offset] : extern_script_functions)
            {
                wo_assert(env->extern_script_functions.find(extern_func_name) == env->extern_script_functions.end());
                env->extern_script_functions[extern_func_name] = pdb_info->get_runtime_ip_by_ip(extern_func_offset);
            }
            env->rt_codes = pdb_info->runtime_codes_base = code_buf;

            env->extern_native_functions = extern_native_functions;
            env->loaded_libs = loaded_libs;

            if (wo::config::ENABLE_PDB_INFORMATIONS)
                env->program_debug_info = pdb_info;

            return env;
        }

    };

}

extern const char* wo_stdlib_ir_src_path;
extern const char* wo_stdlib_ir_src_data;