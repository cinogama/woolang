#pragma once

#include "wo_assert.hpp"
#include "wo_basic_type.hpp"
#include "wo_instruct.hpp"
#include "wo_meta.hpp"
#include "wo_compiler_parser.hpp"
#include "wo_env_locale.hpp"
#include "wo_global_setting.hpp"
#include "wo_lang_extern_symbol_loader.hpp"
#include "wo_lang_ast.hpp"
#include "wo_shared_ptr.hpp"
#include "wo_memory.hpp"
#include "wo_compiler_jit.hpp"
#include "wo_utf8.hpp"

#include <cstring>
#include <string>
#include <optional>
#include <vector>
#include <map>

namespace wo
{
    namespace opnum
    {
        struct opnumbase
        {
            virtual ~opnumbase() = default;
            virtual size_t generate_opnum_to_buffer(std::vector<byte_t>&) const
            {
                wo_error("This type can not generate opnum.");
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

            size_t generate_opnum_to_buffer(std::vector<byte_t>& buffer) const override
            {
                byte_t* buf = (byte_t*)&real_offset_const_glb;

                buffer.push_back(buf[0]);
                buffer.push_back(buf[1]);
                buffer.push_back(buf[2]);
                buffer.push_back(buf[3]);

                return 4;
            }
        };
        struct temporary : virtual opnumbase
        {
            uint32_t m_id;
            temporary(uint32_t id) noexcept
                : m_id(id)
            {
            }
        };
        struct reg :virtual opnumbase
        {
            // REGID
            // 0B | 1    |         1           |000000
            //    |isreg?|stacksign(if not reg)|offset

            uint8_t id;

            static constexpr uint32_t T_REGISTER_COUNT = 16;
            static constexpr uint32_t R_REGISTER_COUNT = 16;
            static constexpr uint32_t ALL_REGISTER_COUNT = 64;

            enum spreg : uint8_t
            {
                // normal regist
                t0 = _wo_reg::WO_REG_T0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14, t15,
                r0 = _wo_reg::WO_REG_R0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, r13, r14, r15,

                // special regist
                op_trace_result = _wo_reg::WO_REG_CR,
                cr = op_trace_result,

                argument_count,
                tc = argument_count,

                exception_inform,
                er = exception_inform,

                nil_constant,
                ni = nil_constant,

                pattern_match,
                pm = pattern_match,

                temporary,
                tp = temporary,

                last_special_register = 0b00111111,
            };

            reg(uint8_t _id) noexcept
                :id(_id)
            {
            }

            static constexpr uint8_t bp_offset(int8_t offset)
            {
                wo_assert(offset >= -64 && offset <= 63);
                return static_cast<uint8_t>(
                    (uint8_t)0b10000000 | static_cast<uint8_t>(offset));
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
            size_t generate_opnum_to_buffer(std::vector<byte_t>& buffer) const override
            {
                buffer.push_back(id);
                return 1;
            }
        };
        struct tag :virtual opnumbase
        {
            wo_pstring_t name;

            tag(wo_pstring_t _name)
                : name(_name)
            {
            }
        };
        struct immbase :virtual opnumbase
        {
        protected:
            explicit immbase(const bool* val)
                : constant_index(std::nullopt)
                , constant_value(*val)
            {
            }
            explicit immbase(const wo_integer_t* val)
                : constant_index(std::nullopt)
                , constant_value(*val)
            {
            }
            explicit immbase(const wo_real_t* val)
                : constant_index(std::nullopt)
                , constant_value(*val)
            {
            }
            explicit immbase(const wo_handle_t* val)
                : constant_index(std::nullopt)
                , constant_value(*val)
            {
            }
            explicit immbase(void* const* val)
                : constant_index(std::nullopt)
                , constant_value(static_cast<wo_handle_t>(
                    reinterpret_cast<intptr_t>(
                        *val)))
            {
            }
            explicit immbase(ast::AstValueFunction* const* val)
                : constant_index(std::nullopt)
                , constant_value(*val)
            {
            }
            explicit immbase(wo_pstring_t val)
                : constant_index(std::nullopt)
                , constant_value(val)
            {
            }
            explicit immbase(const std::string& val)
                : constant_index(std::nullopt)
                , constant_value(val)
            {
            }
        public:
            std::optional<uint32_t> constant_index;
            ast::ConstantValue constant_value;

            explicit immbase(const ast::ConstantValue& val)
                : constant_index(std::nullopt)
                , constant_value(val)
            {
            }

            bool operator < (const immbase& another) const
            {
                return constant_value < another.constant_value;
            }
            ast::ConstantValue::Type type()const
            {
                return constant_value.m_type;
            }
            size_t generate_opnum_to_buffer(std::vector<byte_t>& buffer) const override
            {
                const byte_t* buf =
                    reinterpret_cast<const byte_t*>(
                        &constant_index.value());

                buffer.push_back(buf[0]);
                buffer.push_back(buf[1]);
                buffer.push_back(buf[2]);
                buffer.push_back(buf[3]);

                return 4;
            }
        };

        struct imm_bool : virtual immbase
        {
            explicit imm_bool(bool val)
                : immbase(&val)
            {
            }
        };
        struct imm_int : virtual immbase
        {
            explicit imm_int(wo_integer_t val)
                : immbase(&val)
            {
            }
        };
        struct imm_real : virtual immbase
        {
            explicit imm_real(wo_real_t val)
                : immbase(&val)
            {
            }
        };
        struct imm_handle : virtual immbase
        {
            explicit imm_handle(wo_handle_t val)
                : immbase(&val)
            {
            }
            explicit imm_handle(void* val)
                : immbase(&val)
            {
            }
        };
        struct imm_string : virtual immbase
        {
            explicit imm_string(wo_pstring_t val)
                : immbase(val)
            {
            }
            explicit imm_string(const std::string& val)
                : immbase(val)
            {
            }
        };
#ifndef WO_DISABLE_COMPILER
        struct imm_extfunc : virtual immbase
        {
            imm_extfunc(ast::AstValueFunction* func);
        };
        struct tagimm_rsfunc :virtual tag, virtual immbase
        {
            tagimm_rsfunc(ast::AstValueFunction* func);
            size_t generate_opnum_to_buffer(std::vector<byte_t>& buffer) const override
            {
                // Avoid warning c4250.
                return immbase::generate_opnum_to_buffer(buffer);
            }
        };
#endif
    } // namespace opnum;

    class vmbase;
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
            std::string source_file = "not_found";

            bool        unbreakable; // Can set breakpoint?
        };
        struct function_symbol_infor
        {
            struct variable_symbol_infor
            {
                std::string     name;
                size_t          define_place;
                wo_integer_t    bp_offset;
            };
            size_t ir_begin;
            size_t ir_end;

            std::unordered_map<std::string, std::vector<variable_symbol_infor>> variables;

            void add_variable_define(const std::string varname, size_t rowno, wo_integer_t locat)
            {
                variables[varname].push_back(
                    variable_symbol_infor
                    {
                        varname,
                        rowno,
                        locat,
                    }
                    );
            }
        };

        // Attention: Do not use unordered_map for _general_src_data_buf_b & pdd_rt_code_byte_offset_to_ir
        //            They should keep order.
        using filename_rowno_colno_ip_info_t = std::unordered_map<std::string, std::vector<location>>;
        using ip_src_location_info_t = std::map<size_t, location>;
        using runtime_ip_compile_ip_info_t = std::map<size_t, size_t>;
        using function_signature_ip_info_t = std::unordered_map<std::string, function_symbol_infor>;

        filename_rowno_colno_ip_info_t  _general_src_data_buf_a;
        ip_src_location_info_t          _general_src_data_buf_b;
        function_signature_ip_info_t    _function_ip_data_buf;
        runtime_ip_compile_ip_info_t    pdd_rt_code_byte_offset_to_ir;

        const byte_t* runtime_codes_base;
        size_t runtime_codes_length;

        static const location FAIL_LOC;

        // for lang
#ifndef WO_DISABLE_COMPILER
        void generate_debug_info_at_astnode(ast::AstBase* ast_node, ir_compiler* compiler);
        void finalize_generate_debug_info();

        void generate_func_begin(const std::string& function_name, ast::AstBase* ast_node, ir_compiler* compiler);
        void generate_func_end(const std::string& function_name, ir_compiler* compiler);
        void add_func_variable(const std::string& function_name, const std::string& varname, size_t rowno, wo_integer_t loc);
        void update_func_variable(const std::string& function_name, wo_integer_t offset);
#endif
        const location& get_src_location_by_runtime_ip(const  byte_t* rt_pos) const;
        std::vector<size_t> get_ip_by_src_location(const std::string& src_name, size_t rowno, bool strict, bool ignore_unbreakable)const;
        size_t get_ip_by_runtime_ip(const  byte_t* rt_pos) const;
        size_t get_runtime_ip_by_ip(size_t ip) const;
        std::string get_current_func_signature_by_runtime_ip(const byte_t* rt_pos) const;
    };

    struct paged_env_min_context
    {
        runtime_env* m_runtime_env;

        const byte_t* m_runtime_code_begin;
        const byte_t* m_runtime_code_end;
        value* m_static_storage_edge;
    };
    struct paged_env_mapping
    {
        std::shared_mutex m_paged_envs_mx;
        std::map<intptr_t, paged_env_min_context> m_paged_envs;
    };

    struct runtime_env
    {
        struct jit_meta
        {
            std::vector<uint32_t /* rtcode offset */>
                _functions_offsets_for_jit;
            std::vector<uint32_t /* constant offset */>
                _functions_constant_idx_for_jit;
            std::vector<std::pair<uint32_t /* constant offset */, uint16_t /* tuple offset */>>
                _functions_constant_in_tuple_idx_for_jit;
            std::vector<uint32_t /* rtcode offset */>
                _calln_opcode_offsets_for_jit;
            std::vector<uint32_t /* rtcode offset */>
                _mkclos_opcode_offsets_for_jit;

            jit_meta() = default;
            ~jit_meta() = default;
            jit_meta(const jit_meta&) = delete;
            jit_meta(jit_meta&&) = delete;
            jit_meta& operator = (const jit_meta&) = delete;
            jit_meta& operator = (jit_meta&&) = delete;
        };
        struct extern_native_function_location
        {
            std::string script_name;
            std::optional<std::string> library_name;
            std::string function_name;

            // Will be fill in finalize of env.
            std::vector<uint32_t> caller_offset_in_ir;
            std::vector<uint32_t> offset_in_constant;
            std::vector<std::pair<uint32_t, uint16_t>>
                offset_and_tuple_index_in_constant;
        };
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

        using extern_native_functions_t =
            std::unordered_map<wo_native_func_t, extern_native_function_location>;
        using extern_function_map_t =
            std::unordered_map<std::string, size_t>;
        using jit_code_holder_map_t =
            std::unordered_map<size_t, wo_native_func_t>;

        runtime_env();
        ~runtime_env();

        runtime_env(const runtime_env&) = delete;
        runtime_env(runtime_env&&) = delete;
        runtime_env& operator = (const runtime_env&) = delete;
        runtime_env& operator = (runtime_env&&) = delete;

        std::tuple<void*, size_t> create_env_binary(bool savepdi) noexcept;
        static std::optional<shared_pointer<runtime_env>> _create_from_stream(
            binary_source_stream* stream,
            wo_string_t* out_reason,
            bool* out_is_binary);
        static std::optional<shared_pointer<runtime_env>> load_create_env_from_binary(
            wo_string_t virtual_file,
            const void* bytestream,
            size_t streamsz,
            wo_string_t* out_reason,
            bool* out_is_binary);

        bool try_find_script_func(const std::string& name, const byte_t** out_script_func);
        bool try_find_jit_func(const byte_t* script_func, wo_native_func_t* out_jit_func);

        static paged_env_mapping _paged_env_mapping_context;

        static void register_envs(runtime_env* env) noexcept;
        static void unregister_envs(const runtime_env* env) noexcept;

        static bool fetch_is_far_addr(const irv2::ir* ip) noexcept;
        static bool fetch_far_runtime_env(
            const irv2::ir* ip,
            runtime_env** out_env) noexcept;
        static bool resync_far_state(
            const irv2::ir* ip,
            const irv2::ir** out_runtime_code_begin,
            const irv2::ir** out_runtime_code_end,
            value** out_static_storage_edge) noexcept;

        value* constant_and_global_storage = nullptr;
        size_t constant_and_global_value_takeplace_count = 0;
        size_t constant_value_count = 0;
        size_t real_register_count = 0;

        const byte_t* rt_codes = nullptr;
        size_t rt_code_len = 0;

        std::atomic_size_t _running_on_vm_count = 0;
        std::atomic_size_t _created_destructable_instance_count = 0;

        jit_meta meta_data_for_jit;
        std::optional<jit_code_holder_map_t> jit_code_holder;
        std::optional<shared_pointer<program_debug_data_info>> program_debug_info;
        rslib_extern_symbols::extern_lib_set loaded_libraries;
        extern_native_functions_t extern_native_functions;
        extern_function_map_t extern_script_functions;
    };
    struct BytecodeGenerateContext;
}
