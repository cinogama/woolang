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

        // Base runtime state.
        // NOTE: An environment needs to have at least the following five fields
        //      ready if it is to be started by a VM.
        const byte_t* rt_codes;
        size_t rt_code_len;

        value* constant_and_global_storage;
        size_t constant_and_global_value_takeplace_count;
        size_t constant_value_count;

        // Reference counting for runtime_env instance.
        std::atomic_size_t _running_on_vm_count;
        std::atomic_size_t _created_destructable_instance_count;

        // JIT state
        jit_meta meta_data_for_jit;
        std::optional<jit_code_holder_map_t> jit_code_holder;

        // Extern symbols
        rslib_extern_symbols::extern_lib_set loaded_libraries;
        extern_native_functions_t extern_native_functions;
        extern_function_map_t extern_script_functions;

        // Debug info
        std::optional<shared_pointer<program_debug_data_info>> program_debug_info;

        // Far call context.
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

        static paged_env_mapping _paged_env_mapping_context;
    };
    struct BytecodeGenerateContext;

    struct IRBuilder
    {
        struct Label
        {
            friend struct IRBuilder;

        protected:
            Label() = default;

            struct ApplyAddress
            {
                enum class Formal
                {
                    SHIFT8_U24,
                    U32,
                };
                Formal      m_formal;
                uint32_t    m_ipoffset;
            };
            std::optional<uint32_t> m_bound_ip_offset;
            std::optional<std::vector<ApplyAddress>> m_pending_apply_address;

            void apply_to_address(
                uint32_t* baseaddr,
                uint32_t ipoffset, 
                ApplyAddress::Formal formal) noexcept;
            void bind_at_ip_offset(
                uint32_t* baseaddr, 
                uint32_t ipoffset) noexcept;
        };

        uint32_t* m_code_holder;
        uint32_t m_code_holder_capacity;
        uint32_t m_code_holder_size;

        std::vector<Label*> m_created_labels;
        std::unordered_map<std::string, Label*> m_named_label;

        IRBuilder();
        ~IRBuilder();

        IRBuilder(const IRBuilder&) = delete;
        IRBuilder(IRBuilder&&) = delete;
        IRBuilder& operator = (const IRBuilder&) = delete;
        IRBuilder& operator = (IRBuilder&&) = delete;

        uint32_t emit(uint32_t opcode) noexcept;

        template<typename T, size_t width>
        struct fixed_width
        {
            static_assert(
                std::is_same_v<T, uint32_t>
                || std::is_same_v<T, int32_t>);

            T m_val;
            explicit fixed_width(T val) noexcept
                : m_val(val)
            {
                static_assert(width <= 32);
                if constexpr (width < 32)
                {
                    if (std::is_signed_v<T>)
                        wo_test(
                            m_val >= -(1 << (width - 1))
                            && m_val < (1 << (width - 1)));
                    else
                        wo_test(m_val < (1u << width));
                }
            }

            template<size_t width_b>
            fixed_width(const fixed_width<T, width_b>& b) noexcept
                : m_val(b.m_val)
            {
                static_assert(
                    width_b <= width,
                    "Cannot convert from wider to narrower address sing.");
            }
            template<size_t width_b>
            fixed_width(fixed_width<T, width_b>&& b) noexcept
                : m_val(b.m_val)
            {
                static_assert(
                    width_b <= width,
                    "Cannot convert from wider to narrower address sing.");
            }

            template<size_t width_b>
            fixed_width<T, width>& operator =(const fixed_width<T, width_b>& b) noexcept
            {
                static_assert(
                    width_b <= width,
                    "Cannot convert from wider to narrower address sing.");
                m_val = b.m_val;
                return *this;
            }
            template<size_t width_b>
            fixed_width<T, width>& operator =(fixed_width<T, width_b>&& b) noexcept
            {
                static_assert(
                    width_b <= width,
                    "Cannot convert from wider to narrower address sing.");
                m_adrs = b.m_adrs;
                return *this;
            }
        };

        template<size_t width>
        struct cg_adrsing : fixed_width<int32_t, width>
        {
            explicit cg_adrsing(int32_t val) noexcept
                : fixed_width<int32_t, width>(val)
            {
            }
        };

        struct rs_adrsing8 : fixed_width<int32_t, 8>
        {
            explicit rs_adrsing8(int32_t val) noexcept
                : fixed_width<int32_t, 8>(val)
            {
            }
        };

        template<size_t width>
        struct s_adrsing : fixed_width<int32_t, width>
        {
            explicit s_adrsing(int32_t val) noexcept
                : fixed_width<int32_t, width>(val)
            {
            }
        };

        template<size_t width>
        struct fixed_unsigned : fixed_width<uint32_t, width>
        {
            explicit fixed_unsigned(uint32_t val) noexcept
                : fixed_width<uint32_t, width>(val)
            {
            }
        };

        template<size_t width>
        struct fixed_signed : fixed_width<int32_t, width>
        {
            explicit fixed_signed(int32_t val) noexcept
                : fixed_width<int32_t, width>(val)
            {
            }
        };

        // Code emission
        Label* label() noexcept;
        Label* named_label(const char* name) noexcept;
        void bind(Label* label) noexcept;

        void nop() noexcept;
        void end() noexcept;
        void load(cg_adrsing<32> src_cg32, rs_adrsing8 dst_rs8) noexcept;
        void store(cg_adrsing<32> dst_cg32, rs_adrsing8 src_rs8) noexcept;
        void loadext(s_adrsing<24> dst_s24, cg_adrsing<32> src_cg32) noexcept;
        void storeext(s_adrsing<24> dst_s24, cg_adrsing<32> src_cg32) noexcept;

        void push(fixed_unsigned<24> count_u24) noexcept;
        void push(rs_adrsing8 src_rs8) noexcept;
        void push(cg_adrsing<32> src_cg32) noexcept;

        void pop(fixed_unsigned<24> count_u24) noexcept;
        void pop(rs_adrsing8 dst_rs8) noexcept;
        void pop(cg_adrsing<32> dst_cg32) noexcept;

        void cast(
            rs_adrsing8 dst_rs8,
            rs_adrsing8 src_rs8,
            wo_type_t cast_to) noexcept;
        void castitors(
            rs_adrsing8 dst_rs8,
            rs_adrsing8 src_rs8) noexcept;
        void castrtoi(
            rs_adrsing8 dst_rs8,
            rs_adrsing8 src_rs8) noexcept;

        void typeis(
            rs_adrsing8 dst_rs8,
            rs_adrsing8 src_rs8,
            wo_type_t check_type) noexcept;
        void typeas(
            rs_adrsing8 src_rs8,
            wo_type_t as_type) noexcept;

        void addi(
            rs_adrsing8 dst_rs8,
            rs_adrsing8 src_rs8_a,
            rs_adrsing8 src_rs8_b) noexcept;
        void subi(
            rs_adrsing8 dst_rs8,
            rs_adrsing8 src_rs8_a,
            rs_adrsing8 src_rs8_b) noexcept;
        void muli(
            rs_adrsing8 dst_rs8,
            rs_adrsing8 src_rs8_a,
            rs_adrsing8 src_rs8_b) noexcept;
        void divi(
            rs_adrsing8 dst_rs8,
            rs_adrsing8 src_rs8_a,
            rs_adrsing8 src_rs8_b) noexcept;
        void modi(
            rs_adrsing8 dst_rs8,
            rs_adrsing8 src_rs8_a,
            rs_adrsing8 src_rs8_b) noexcept;
        void negi(
            rs_adrsing8 dst_rs8,
            rs_adrsing8 src_rs8) noexcept;
        void lti(
            rs_adrsing8 dst_rs8,
            rs_adrsing8 src_rs8_a,
            rs_adrsing8 src_rs8_b) noexcept;
        void gti(
            rs_adrsing8 dst_rs8,
            rs_adrsing8 src_rs8_a,
            rs_adrsing8 src_rs8_b) noexcept;
        void elti(
            rs_adrsing8 dst_rs8,
            rs_adrsing8 src_rs8_a,
            rs_adrsing8 src_rs8_b) noexcept;
        void egti(
            rs_adrsing8 dst_rs8,
            rs_adrsing8 src_rs8_a,
            rs_adrsing8 src_rs8_b) noexcept;
        void equb(
            rs_adrsing8 dst_rs8,
            rs_adrsing8 src_rs8_a,
            rs_adrsing8 src_rs8_b) noexcept;
        void nequb(
            rs_adrsing8 dst_rs8,
            rs_adrsing8 src_rs8_a,
            rs_adrsing8 src_rs8_b) noexcept;

        void addr(
            rs_adrsing8 dst_rs8,
            rs_adrsing8 src_rs8_a,
            rs_adrsing8 src_rs8_b) noexcept;
        void subr(
            rs_adrsing8 dst_rs8,
            rs_adrsing8 src_rs8_a,
            rs_adrsing8 src_rs8_b) noexcept;
        void mulr(
            rs_adrsing8 dst_rs8,
            rs_adrsing8 src_rs8_a,
            rs_adrsing8 src_rs8_b) noexcept;
        void divr(
            rs_adrsing8 dst_rs8,
            rs_adrsing8 src_rs8_a,
            rs_adrsing8 src_rs8_b) noexcept;
        void modr(
            rs_adrsing8 dst_rs8,
            rs_adrsing8 src_rs8_a,
            rs_adrsing8 src_rs8_b) noexcept;
        void negr(
            rs_adrsing8 dst_rs8,
            rs_adrsing8 src_rs8) noexcept;
        void ltr(
            rs_adrsing8 dst_rs8,
            rs_adrsing8 src_rs8_a,
            rs_adrsing8 src_rs8_b) noexcept;
        void gtr(
            rs_adrsing8 dst_rs8,
            rs_adrsing8 src_rs8_a,
            rs_adrsing8 src_rs8_b) noexcept;
        void eltr(
            rs_adrsing8 dst_rs8,
            rs_adrsing8 src_rs8_a,
            rs_adrsing8 src_rs8_b) noexcept;
        void egtr(
            rs_adrsing8 dst_rs8,
            rs_adrsing8 src_rs8_a,
            rs_adrsing8 src_rs8_b) noexcept;
        void equr(
            rs_adrsing8 dst_rs8,
            rs_adrsing8 src_rs8_a,
            rs_adrsing8 src_rs8_b) noexcept;
        void nequr(
            rs_adrsing8 dst_rs8,
            rs_adrsing8 src_rs8_a,
            rs_adrsing8 src_rs8_b) noexcept;

        void adds(
            rs_adrsing8 dst_rs8,
            rs_adrsing8 src_rs8_a,
            rs_adrsing8 src_rs8_b) noexcept;
        void lts(
            rs_adrsing8 dst_rs8,
            rs_adrsing8 src_rs8_a,
            rs_adrsing8 src_rs8_b) noexcept;
        void gts(
            rs_adrsing8 dst_rs8,
            rs_adrsing8 src_rs8_a,
            rs_adrsing8 src_rs8_b) noexcept;
        void elts(
            rs_adrsing8 dst_rs8,
            rs_adrsing8 src_rs8_a,
            rs_adrsing8 src_rs8_b) noexcept;
        void egts(
            rs_adrsing8 dst_rs8,
            rs_adrsing8 src_rs8_a,
            rs_adrsing8 src_rs8_b) noexcept;
        void equs(
            rs_adrsing8 dst_rs8,
            rs_adrsing8 src_rs8_a,
            rs_adrsing8 src_rs8_b) noexcept;
        void nequs(
            rs_adrsing8 dst_rs8,
            rs_adrsing8 src_rs8_a,
            rs_adrsing8 src_rs8_b) noexcept;

        void cdivilr(
            rs_adrsing8 src_rs8_a,
            rs_adrsing8 src_rs8_b) noexcept;
        void cdivil(rs_adrsing8 src_rs8) noexcept;
        void cdivir(rs_adrsing8 src_rs8) noexcept;
        void cdivirz(rs_adrsing8 src_rs8) noexcept;

        void land(
            rs_adrsing8 dst_rs8,
            rs_adrsing8 src_rs8_a,
            rs_adrsing8 src_rs8_b) noexcept;
        void lor(
            rs_adrsing8 dst_rs8,
            rs_adrsing8 src_rs8_a,
            rs_adrsing8 src_rs8_b) noexcept;
        void lnot(
            rs_adrsing8 dst_rs8,
            rs_adrsing8 src_rs8) noexcept;

        void idstr(
            rs_adrsing8 dst_rs8,
            rs_adrsing8 cont_rs8,
            rs_adrsing8 idx_rs8) noexcept;
        void idarr(
            rs_adrsing8 dst_rs8,
            rs_adrsing8 cont_rs8,
            rs_adrsing8 idx_rs8) noexcept;
        void iddict(
            rs_adrsing8 dst_rs8,
            rs_adrsing8 cont_rs8,
            rs_adrsing8 idx_rs8) noexcept;
        void idstruct(
            rs_adrsing8 dst_rs8,
            rs_adrsing8 cont_rs8,
            fixed_unsigned<32> idx_u32) noexcept;

        void sidarr(
            rs_adrsing8 src_rs8,
            rs_adrsing8 cont_rs8,
            rs_adrsing8 idx_rs8) noexcept;
        void siddict(
            rs_adrsing8 src_rs8,
            rs_adrsing8 cont_rs8,
            rs_adrsing8 idx_rs8) noexcept;
        void sidmap(
            rs_adrsing8 src_rs8,
            rs_adrsing8 cont_rs8,
            rs_adrsing8 idx_rs8) noexcept;
        void sidstruct(
            rs_adrsing8 src_rs8,
            rs_adrsing8 cont_rs8,
            fixed_unsigned<32> idx_u32) noexcept;

        void jmp(Label* label) noexcept;
        void jmpf(Label* label) noexcept;
        void jmpt(Label* label) noexcept;

        void jmpgc(Label* label) noexcept;
        void jmpgcf(Label* label) noexcept;
        void jmpgct(Label* label) noexcept;

        void ret() noexcept;
        void retn(fixed_unsigned<16> count_u16) noexcept;

        void calln(Label* label) noexcept;
        void callnfp(wo_native_func_t extfunc) noexcept;
        void call(rs_adrsing8 src_rs8) noexcept;

        ////////////////////////////////////////////////////////

        shared_pointer<runtime_env> finish();
    };
}
