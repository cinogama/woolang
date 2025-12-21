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

        static bool fetch_is_far_addr(const byte_t* ip) noexcept;
        static bool fetch_far_runtime_env(
            const byte_t* ip,
            runtime_env** out_env) noexcept;
        static bool resync_far_state(
            const byte_t* ip,
            const byte_t** out_runtime_code_begin,
            const byte_t** out_runtime_code_end,
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

    class ir_compiler
    {
        friend class vmbase;

        ///////////////////////////////////////////////////////////////////////
        // Nested types
        ///////////////////////////////////////////////////////////////////////
    private:
        struct ir_param
        {
            enum type
            {
                OPCODE,
                OPNUM,
                IMM_TAG,
                IMM_U8,
                IMM_U16,
                IMM_U32,
                IMM_U64,
            };

            type m_type;
            union
            {
                struct
                {
                    instruct::opcode m_instruct;
                    uint8_t m_dr;
                };
                opnum::opnumbase* m_opnum;
                uint8_t m_immu8;
                uint16_t m_immu16;
                uint32_t m_immu32;
                uint64_t m_immu64;
            };
        };
        struct ir_command
        {
            instruct::opcode opcode;

            const opnum::opnumbase* op1 = nullptr;
            const opnum::opnumbase* op2 = nullptr;

            int32_t opinteger1;
            int32_t opinteger2;

            uint8_t ext_page_id;
            union
            {
                instruct::extern_opcode_page_0 ext_opcode_p0;
                instruct::extern_opcode_page_1 ext_opcode_p1;
                instruct::extern_opcode_page_2 ext_opcode_p2;
                instruct::extern_opcode_page_3 ext_opcode_p3;
            };

            std::optional<ir_param> param;

            ir_command()
                : opcode(instruct::opcode::nop)
                , op1(nullptr)
                , op2(nullptr)
                , opinteger1(0)
                , opinteger2(0)
                , ext_page_id(0)
                , param(std::nullopt)
            {
            }

            ir_command(
                instruct::opcode _opcode,
                opnum::opnumbase* _op1 = nullptr,
                opnum::opnumbase* _op2 = nullptr,
                int32_t _opinteger1 = 0,
                int32_t _opinteger2 = 0,
                uint8_t _extpage = 0
            )
                : opcode(_opcode)
                , op1(_op1)
                , op2(_op2)
                , opinteger1(_opinteger1)
                , opinteger2(_opinteger2)
                , ext_page_id(_extpage)
                , param(std::nullopt)
            {
            }

            ir_command(const ir_command&) = default;
            ir_command(ir_command&&) = default;
            ir_command& operator = (const ir_command&) = default;
            ir_command& operator = (ir_command&&) = default;

#define WO_IS_REG(OPNUM)(dynamic_cast<const opnum::reg*>(OPNUM))
            uint8_t dr()
            {
                return (WO_IS_REG(op1) ? (uint8_t)0b00000010 : (uint8_t)0)
                    | (WO_IS_REG(op2) ? (uint8_t)0b00000001 : (uint8_t)0);
            }
#undef WO_IS_REG
        };

        struct TagOffsetInConstantOffset
        {
            uint32_t m_offset_in_constant;
            std::vector<std::pair<uint32_t, uint16_t>> m_offset_in_tuple;
        };
        using TagOffsetLocatedInConstantTableOffsetRecordT =
            std::map<wo_pstring_t, TagOffsetInConstantOffset>;

        // Member variables
    private:
        mutable unsigned int                        _unique_id;

        // TODO: Use ir_param_buffer instead of ir_command_buffer
        std::vector<ir_command>                     ir_command_buffer;
        std::map<size_t, std::vector<wo_pstring_t>> tag_irbuffer_offset;

        runtime_env::extern_native_functions_t      extern_native_functions;
        runtime_env::extern_function_map_t          extern_script_functions;

        std::map<ast::ConstantValue, uint32_t>      constant_record_to_index_mapping;
        std::vector<const ast::ConstantValue*>      ordered_constant_record_list;
        std::vector<opnum::global*>                 global_record_list;
        std::vector<opnum::opnumbase*>              created_opnum_buffer;

    public:
        rslib_extern_symbols::extern_lib_set        loaded_libraries;
        shared_pointer<program_debug_data_info>     pdb_info;

        // Private methods
    private:
        template<typename T>
        T* _created_opnum_item(const T& _opn) noexcept
        {
            auto result = new T(_opn);
            created_opnum_buffer.push_back(result);
            return result;
        }

        uint32_t _check_constant_and_give_storage_idx(
            const ast::ConstantValue& constant) noexcept;

        void apply_value_to_constant_instance(
            value* constant_value_pool,
            const ast::ConstantValue& constant_value,
            uint32_t this_constant_index,
            TagOffsetLocatedInConstantTableOffsetRecordT& out_record) noexcept;

        opnum::opnumbase* _check_and_add_const(opnum::opnumbase* _opnum) noexcept;

        // Public methods
    public:
        ir_compiler()
            : _unique_id(0)
            , pdb_info(new program_debug_data_info())
        {
        }

        ~ir_compiler()
        {
            for (auto* created_opnum : created_opnum_buffer)
                delete created_opnum;
        }

        size_t get_now_ip() const
        {
            return ir_command_buffer.size();
        }
        std::string get_unique_tag_based_command_ip()const
        {
            return "ip_" + std::to_string(get_now_ip()) + "_" + std::to_string(_unique_id++);
        }
        void revert_code_to(size_t ip)
        {
            ir_command_buffer.resize(ip);
        }
        void record_extern_native_function(
            wo_native_func_t function,
            const std::string& script_path,
            const std::optional<std::string>& library_name,
            const std::string& function_name)
        {
            if (extern_native_functions.find(function) == extern_native_functions.end())
            {
                auto& native_info = extern_native_functions[function];
                native_info.script_name = script_path;
                native_info.library_name = std::nullopt;

                if (library_name.has_value())
                    native_info.library_name = library_name.value();
                native_info.function_name = function_name;
            }
        }
        void record_extern_script_function(const std::string& function_name)
        {
            // ISSUE-N221022: Function overload has been removed from woolang.
            wo_assert(extern_script_functions.find(function_name) == extern_script_functions.end());
            extern_script_functions[function_name] = get_now_ip();
        }

#ifndef WO_DISABLE_COMPILER
        int32_t update_all_temp_regist_to_stack(BytecodeGenerateContext* ctx, size_t begin) noexcept;
#endif

#define WO_OPNUM(OPNUM) (_check_and_add_const(\
        (std::is_same<meta::origin_type<decltype(OPNUM)>, opnum::opnumbase>::value)\
        ? const_cast<meta::origin_type<decltype(OPNUM)>*>(&OPNUM)\
        : _created_opnum_item<meta::origin_type<decltype(OPNUM)>>(OPNUM)))

#define WO_PUT_IR_TO_BUFFER(OPCODE, ...)\
    ir_command_buffer.emplace_back(ir_command{OPCODE, __VA_ARGS__})

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
            ir_command_buffer[ip - 1].opinteger1 = sz;
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
        template<typename OP1T, typename OP2T>
        void movrcasti(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not set value to immediate.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::movrcasti, WO_OPNUM(op1), WO_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void movicastr(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not set value to immediate.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::movicastr, WO_OPNUM(op1), WO_OPNUM(op2));
        }
        template<typename OP1T>
        void typeas(const OP1T& op1, value::valuetype vtt)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value,
                "Argument(s) should be opnum.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::typeas, WO_OPNUM(op1), nullptr, (int)vtt, 0);
        }
        template<typename OP1T>
        void typeis(const OP1T& op1, value::valuetype vtt)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value,
                "Argument(s) should be opnum.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::typeas, WO_OPNUM(op1), nullptr, (int)vtt, 1);
        }
        template<typename OP1T, typename OP2T, typename OP3T>
        void movicas(const OP1T& op1, const OP2T& op2, const OP3T& r)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(std::is_same<OP3T, opnum::reg>::value
                || std::is_same<OP3T, opnum::temporary>::value,
                "Argument r should be reg or temporary.");

            if constexpr (std::is_same<OP3T, opnum::reg>::value)
                WO_PUT_IR_TO_BUFFER(instruct::opcode::movicas, WO_OPNUM(op1), WO_OPNUM(op2), (int32_t)r.id);
            else
                WO_PUT_IR_TO_BUFFER(instruct::opcode::movicas, WO_OPNUM(op1), WO_OPNUM(op2), -(int32_t)(r.m_id + 1));
        }
        template<typename OP1T>
        void call(const OP1T& op1)
        {
            if constexpr (std::is_base_of<opnum::opnumbase, OP1T>::value)
            {
                if (nullptr != dynamic_cast<const opnum::tag*>(&op1))
                {
                    WO_PUT_IR_TO_BUFFER(instruct::opcode::calln, nullptr, WO_OPNUM(op1));
                    return;
                }
                else if (auto* immop = dynamic_cast<const opnum::immbase*>(&op1))
                {
                    switch (immop->type())
                    {
                    case ast::ConstantValue::Type::FUNCTION:
                    {
#ifndef WO_DISABLE_COMPILER
                        auto* function = immop->constant_value.value_function();
                        auto* extern_function =
                            function->m_IR_extern_information.value()->m_IR_externed_function.value();

                        WO_PUT_IR_TO_BUFFER(
                            instruct::opcode::calln,
                            reinterpret_cast<opnum::opnumbase*>(extern_function));
#else
                        wo_error("Should never be function if compiler disabled.");
#endif
                        return;
                    }
                    default:
                        // Treate it as normal call.
                        // Do nothing.
                        break;
                    }
                }

                WO_PUT_IR_TO_BUFFER(instruct::opcode::call, WO_OPNUM(op1));
                return;
            }
            else
            {
                static_assert(
                    !std::is_base_of<opnum::tag, OP1T>::value
                    && !std::is_base_of<opnum::opnumbase, OP1T>::value
                    && !std::is_pointer<OP1T>::value
                    && !std::is_integral<OP1T>::value
                    , "Argument(s) should be opnum.");
            }
        }

        void jmp(const opnum::tag& op1)
        {
            WO_PUT_IR_TO_BUFFER(instruct::opcode::setip, WO_OPNUM(op1), nullptr, 0);
        }
        void jf(const opnum::tag& op1)
        {
            WO_PUT_IR_TO_BUFFER(instruct::opcode::setip, WO_OPNUM(op1), nullptr, 1);
        }
        void jt(const opnum::tag& op1)
        {
            WO_PUT_IR_TO_BUFFER(instruct::opcode::setip, WO_OPNUM(op1), nullptr, 2);
        }

        void mkclos(uint16_t capture_count, const opnum::tag& wrapped_func)
        {
            WO_PUT_IR_TO_BUFFER(instruct::opcode::mkclos, WO_OPNUM(wrapped_func), nullptr, (int32_t)capture_count);
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
            WO_PUT_IR_TO_BUFFER(
                instruct::opcode::endproc, nullptr, nullptr, 0);
        }
        void end()
        {
            WO_PUT_IR_TO_BUFFER(
                instruct::opcode::endproc, nullptr, nullptr, 1);
        }
        void ret()
        {
            WO_PUT_IR_TO_BUFFER(
                instruct::opcode::endproc, nullptr, nullptr, 2);
        }
        void ret(uint16_t popcount)
        {
            WO_PUT_IR_TO_BUFFER(
                instruct::opcode::endproc, nullptr, nullptr, 3, (int32_t)popcount);
        }
        void tag(wo_pstring_t tagname)
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

            WO_PUT_IR_TO_BUFFER(instruct::opcode::mkunion, WO_OPNUM(op1), WO_OPNUM(op2), (int32_t)id);
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

            WO_PUT_IR_TO_BUFFER(
                instruct::opcode::mkcontain, 
                WO_OPNUM(op1), 
                nullptr, 
                (int32_t)size,
                (int32_t)0);
        }
        template<typename OP1T>
        void mkmap(const OP1T& op1, uint16_t size)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not set value to immediate.");

            WO_PUT_IR_TO_BUFFER(
                instruct::opcode::mkcontain, 
                WO_OPNUM(op1), 
                nullptr, 
                (int32_t)size,
                (int32_t)1);
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
        template<typename OP1T, typename OP2T, typename OP3T>
        void siddict(const OP1T& op1, const OP2T& op2, const OP3T& r)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(std::is_same<OP3T, opnum::reg>::value
                || std::is_same<OP3T, opnum::temporary>::value,
                "Argument r should be reg or temporary.");

            if constexpr (std::is_same<OP3T, opnum::reg>::value)
                WO_PUT_IR_TO_BUFFER(instruct::opcode::siddict, WO_OPNUM(op1), WO_OPNUM(op2), (int32_t)r.id);
            else
                WO_PUT_IR_TO_BUFFER(instruct::opcode::siddict, WO_OPNUM(op1), WO_OPNUM(op2), -(int32_t)(r.m_id + 1));
        }
        template<typename OP1T, typename OP2T, typename OP3T>
        void sidmap(const OP1T& op1, const OP2T& op2, const OP3T& r)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(std::is_same<OP3T, opnum::reg>::value
                || std::is_same<OP3T, opnum::temporary>::value,
                "Argument r should be reg or temporary.");

            if constexpr (std::is_same<OP3T, opnum::reg>::value)
                WO_PUT_IR_TO_BUFFER(instruct::opcode::sidmap, WO_OPNUM(op1), WO_OPNUM(op2), (int32_t)r.id);
            else
                WO_PUT_IR_TO_BUFFER(instruct::opcode::sidmap, WO_OPNUM(op1), WO_OPNUM(op2), -(int32_t)(r.m_id + 1));
        }
        template<typename OP1T, typename OP2T, typename OP3T>
        void sidarr(const OP1T& op1, const OP2T& op2, const OP3T& r)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(std::is_same<OP3T, opnum::reg>::value
                || std::is_same<OP3T, opnum::temporary>::value,
                "Argument r should be reg or temporary.");

            if constexpr (std::is_same<OP3T, opnum::reg>::value)
                WO_PUT_IR_TO_BUFFER(instruct::opcode::sidarr, WO_OPNUM(op1), WO_OPNUM(op2), (int32_t)r.id);
            else
                WO_PUT_IR_TO_BUFFER(instruct::opcode::sidarr, WO_OPNUM(op1), WO_OPNUM(op2), -(int32_t)(r.m_id + 1));
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

        template<typename OP1T>
        void ext_packargs(const OP1T& op1, uint16_t thisfuncargc, uint16_t skipclosure)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value,
                "Argument(s) should be opnum.");

            auto& codeb = WO_PUT_IR_TO_BUFFER(instruct::opcode::ext,
                WO_OPNUM(op1), nullptr, (int32_t)thisfuncargc, (int32_t)skipclosure);

            codeb.ext_page_id = 0;
            codeb.ext_opcode_p0 = instruct::extern_opcode_page_0::pack;
        }

        template<typename OP1T>
        void unpack(const OP1T& op1, int32_t unpack_count)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value,
                "Argument(s) should be opnum.");

            WO_PUT_IR_TO_BUFFER(
                instruct::opcode::unpack, WO_OPNUM(op1), nullptr, unpack_count);
        }

        template<typename OP1T, typename OP2T>
        void ext_cdivilr(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value &&
                std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            auto& codeb = WO_PUT_IR_TO_BUFFER(instruct::opcode::ext, WO_OPNUM(op1), WO_OPNUM(op2));
            codeb.ext_page_id = 0;
            codeb.ext_opcode_p0 = instruct::extern_opcode_page_0::cdivilr;
        }
        template<typename OP1T>
        void ext_cdivil(const OP1T& op1)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value,
                "Argument(s) should be opnum.");

            auto& codeb = WO_PUT_IR_TO_BUFFER(instruct::opcode::ext, WO_OPNUM(op1));
            codeb.ext_page_id = 0;
            codeb.ext_opcode_p0 = instruct::extern_opcode_page_0::cdivil;
        }
        template<typename OP1T>
        void ext_cdivirz(const OP1T& op1)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value,
                "Argument(s) should be opnum.");

            auto& codeb = WO_PUT_IR_TO_BUFFER(instruct::opcode::ext, WO_OPNUM(op1));
            codeb.ext_page_id = 0;
            codeb.ext_opcode_p0 = instruct::extern_opcode_page_0::cdivirz;
        }

        template<typename OP1T>
        void ext_cdivir(const OP1T& op1)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value,
                "Argument(s) should be opnum.");

            auto& codeb = WO_PUT_IR_TO_BUFFER(instruct::opcode::ext, WO_OPNUM(op1));
            codeb.ext_page_id = 0;
            codeb.ext_opcode_p0 = instruct::extern_opcode_page_0::cdivir;
        }

        template<typename OP1T>
        void ext_popn(const OP1T& op1)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value,
                "Argument(s) should be opnum.");

            auto& codeb = WO_PUT_IR_TO_BUFFER(instruct::opcode::ext, WO_OPNUM(op1));
            codeb.ext_page_id = 0;
            codeb.ext_opcode_p0 = instruct::extern_opcode_page_0::popn;
        }

        ir_param& _new_ir_param()
        {
            auto& codeb = ir_command_buffer.emplace_back();
            codeb.param = std::make_optional<ir_param>();

            return codeb.param.value();
        }

        void ir_opcode(instruct::opcode code, uint8_t dr)
        {
            auto& codeb = _new_ir_param();

            codeb.m_type = ir_param::type::OPCODE;
            codeb.m_instruct = code;
            codeb.m_dr = dr;
        }

        template<typename OP1T>
        void ir_opnum(const OP1T& op1)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value,
                "Argument(s) should be opnum.");

            auto& codeb = _new_ir_param();

            codeb.m_type = ir_param::type::OPNUM;
            codeb.m_opnum = WO_OPNUM(op1);
        }

        void ir_imm_tag(opnum::tag tag)
        {
            auto& codeb = _new_ir_param();

            codeb.m_type = ir_param::type::IMM_TAG;
            codeb.m_opnum = WO_OPNUM(tag);
        }

        void ir_imm_u8(uint8_t val)
        {
            auto& codeb = _new_ir_param();

            codeb.m_type = ir_param::type::IMM_U8;
            codeb.m_immu8 = val;
        }
        void ir_imm_u16(uint16_t val)
        {
            auto& codeb = _new_ir_param();

            codeb.m_type = ir_param::type::IMM_U16;
            codeb.m_immu16 = val;
        }
        void ir_imm_u32(uint32_t val)
        {
            auto& codeb = _new_ir_param();

            codeb.m_type = ir_param::type::IMM_U32;
            codeb.m_immu32 = val;
        }
        void ir_imm_u64(uint64_t val)
        {
            auto& codeb = _new_ir_param();

            codeb.m_type = ir_param::type::IMM_U64;
            codeb.m_immu64 = val;
        }
#undef WO_OPNUM
#undef WO_PUT_IR_TO_BUFFER
        shared_pointer<runtime_env> finalize();

    };
}
