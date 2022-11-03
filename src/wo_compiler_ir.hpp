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
            size_t      row_no;
            size_t      col_no;
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

        using filename_rowno_colno_ip_info_t = std::map<std::wstring, std::map<size_t, std::map<size_t, size_t>>>;
        using ip_src_location_info_t = std::map<size_t, location>;
        using runtime_ip_compile_ip_info_t = std::map<size_t, size_t>;
        using function_signature_ip_info_t = std::map<std::string, function_symbol_infor>;
        using extern_function_map_t = std::map<std::string, size_t>;

        filename_rowno_colno_ip_info_t  _general_src_data_buf_a;
        ip_src_location_info_t          _general_src_data_buf_b;
        function_signature_ip_info_t    _function_ip_data_buf;
        runtime_ip_compile_ip_info_t    pdd_rt_code_byte_offset_to_ir;
        extern_function_map_t           extern_function_map;
        const byte_t* runtime_codes_base;
        size_t runtime_codes_length;

        rslib_extern_symbols::extern_lib_set loaded_libs;

        // for lang
        void generate_debug_info_at_funcbegin(ast::ast_value_function_define* ast_func, ir_compiler* compiler);
        void generate_debug_info_at_funcend(ast::ast_value_function_define* ast_func, ir_compiler* compiler);
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
        std::vector<size_t> _mkclos_opcode_offsets;
        std::vector<void*> _jit_functions;

        shared_pointer<program_debug_data_info> program_debug_info;

        ~runtime_env()
        {
            free_jit(this);

            for (size_t ci = 0; ci<constant_value_count;++ci)
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
    };

    class ir_compiler
    {
        // IR COMPILER:
        /*
        *
        */

        friend struct vmbase;

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

        struct extern_native_function_location
        {
            std::string library_name;
            std::string function_name;
        };
        std::map<intptr_t, extern_native_function_location> extern_native_functions;

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

        void record_extern_native_function(intptr_t function, const std::wstring& library_name, const std::wstring& function_name)
        {
            if (extern_native_functions.find(function) == extern_native_functions.end())
            {
                auto& native_info = extern_native_functions[function];
                native_info.library_name = wo::wstr_to_str(library_name);
                native_info.function_name = wo::wstr_to_str(function_name);
            }
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

                if (ir_command_buffer[i].opcode != instruct::calln)
                {
                    if (ir_command_buffer[i].opcode == instruct::lds || ir_command_buffer[i].opcode == instruct::ldsr)
                    {
                        auto* imm_opnum_stx_offset = dynamic_cast<opnum::immbase*>(opnum2);
                        if (imm_opnum_stx_offset)
                        {
                            if (imm_opnum_stx_offset->try_int() <= 0)
                                imm_opnum_stx_offset->try_set_int(imm_opnum_stx_offset->try_int() - maxim_offset);
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
                                    ir_command{ instruct::ldsr, WO_OPNUM(reg_r0), WO_OPNUM(imm_offset) });         // ldsr r0, imm(real_offset)
                                op1->id = opnum::reg::r0;
                                i++;
                            }
                        }

                        updated_opnum.insert(opnum1);
                    }
                    if (auto* op2 = dynamic_cast<opnum::reg*>(opnum2); op2 && updated_opnum.find(opnum2) == updated_opnum.end())
                    {
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
                                opnum::reg reg_r1(opnum::reg::r1);
                                opnum::imm imm_offset(offseted_bp_offset);

                                // out of bt_offset range, make lds ldsr
                                ir_command_buffer.insert(ir_command_buffer.begin() + i,
                                    ir_command{ instruct::ldsr, WO_OPNUM(reg_r1), WO_OPNUM(imm_offset) });         // ldsr r0, imm(real_offset)
                                op2->id = opnum::reg::r1;
                                i++;
                            }
                        }

                        updated_opnum.insert(opnum2);
                    }
                }
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

        template<typename OP1T, typename OP2T>
        void set(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not set value to immediate.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::set, WO_OPNUM(op1), WO_OPNUM(op2));
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
        void pshr(const OP1T& op1)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                , "Argument(s) should be opnum.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::pshr, WO_OPNUM(op1));
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
        void ldsr(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not load value and save to immediate.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::ldsr, WO_OPNUM(op1), WO_OPNUM(op2));
        }

        template<typename OP1T, typename OP2T>
        void movcast(const OP1T& op1, const OP2T& op2, value::valuetype vtt)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not move value to immediate.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::movcast, WO_OPNUM(op1), WO_OPNUM(op2), (int)vtt);
        }
        template<typename OP1T, typename OP2T>
        void setcast(const OP1T& op1, const OP2T& op2, value::valuetype vtt)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not set value to immediate.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::setcast, WO_OPNUM(op1), WO_OPNUM(op2), (int)vtt);
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

        void ext_veh_begin(const opnum::tag& op1)
        {
            auto& codeb = WO_PUT_IR_TO_BUFFER(instruct::opcode::ext, WO_OPNUM(op1));
            codeb.ext_page_id = 0;
            codeb.ext_opcode_p0 = instruct::extern_opcode_page_0::veh;
        }

        void ext_veh_clean(const opnum::tag& op1)
        {
            auto& codeb = WO_PUT_IR_TO_BUFFER(instruct::opcode::ext, nullptr, WO_OPNUM(op1));
            codeb.ext_page_id = 0;
            codeb.ext_opcode_p0 = instruct::extern_opcode_page_0::veh;
        }

        void ext_veh_throw()
        {
            auto& codeb = WO_PUT_IR_TO_BUFFER(instruct::opcode::ext);
            codeb.ext_page_id = 0;
            codeb.ext_opcode_p0 = instruct::extern_opcode_page_0::veh;
        }

        template<typename OP1T>
        void ext_mkunion(const OP1T& op1, uint16_t id)
        {
            auto& codeb = WO_PUT_IR_TO_BUFFER(instruct::opcode::ext, WO_OPNUM(op1), nullptr, (int32_t)id);
            codeb.ext_page_id = 0;
            codeb.ext_opcode_p0 = instruct::extern_opcode_page_0::mkunion;
        }

        template<typename OP1T>
        void ext_panic(const OP1T& op1)
        {
            auto& codeb = WO_PUT_IR_TO_BUFFER(instruct::opcode::ext, WO_OPNUM(op1));
            codeb.ext_page_id = 0;
            codeb.ext_opcode_p0 = instruct::extern_opcode_page_0::panic;
        }

        template<typename OP1T, typename OP2T>
        void mkarr(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not set value to immediate.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::mkarr, WO_OPNUM(op1), WO_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void mkmap(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not set value to immediate.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::mkmap, WO_OPNUM(op1), WO_OPNUM(op2));
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
        void sidmap(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::sidmap, WO_OPNUM(op1), WO_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void idstr(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            WO_PUT_IR_TO_BUFFER(instruct::opcode::idstr, WO_OPNUM(op1), WO_OPNUM(op2));
        }

        template<typename OP1T, typename OP2T>
        void ext_setref(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not set immediate as ref.");

            auto& codeb = WO_PUT_IR_TO_BUFFER(instruct::opcode::ext, WO_OPNUM(op1), WO_OPNUM(op2));
            codeb.ext_page_id = 0;
            codeb.ext_opcode_p0 = instruct::extern_opcode_page_0::setref;
        }

        template<typename OP1T, typename OP2T>
        void ext_trans(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not set immediate as ref.");

            auto& codeb = WO_PUT_IR_TO_BUFFER(instruct::opcode::ext, WO_OPNUM(op1), WO_OPNUM(op2));
            codeb.ext_page_id = 0;
            codeb.ext_opcode_p0 = instruct::extern_opcode_page_0::trans;
        }

        template<typename OP1T, typename OP2T>
        void ext_movdup(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not mov value to immediate.");

            auto& codeb = WO_PUT_IR_TO_BUFFER(instruct::opcode::ext, WO_OPNUM(op1), WO_OPNUM(op2));
            codeb.ext_page_id = 0;
            codeb.ext_opcode_p0 = instruct::extern_opcode_page_0::movdup;
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

        shared_pointer<runtime_env> finalize(size_t stacksz = 0)
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
            size_t runtime_stack_count = stacksz ? stacksz : 1024;  // by default

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
                case instruct::opcode::set:
                    temp_this_command_code_buf.push_back(WO_OPCODE(set));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
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
                case instruct::opcode::setcast:
                    temp_this_command_code_buf.push_back(WO_OPCODE(setcast));
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
                case instruct::opcode::pshr:
                    temp_this_command_code_buf.push_back(WO_OPCODE(pshr));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
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
                case instruct::opcode::ldsr:
                    temp_this_command_code_buf.push_back(WO_OPCODE(ldsr));
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
                    temp_this_command_code_buf.push_back(WO_OPCODE(mkarr));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                    break;
                case instruct::opcode::mkmap:
                    temp_this_command_code_buf.push_back(WO_OPCODE(mkmap));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                    break;
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
                case instruct::opcode::sidmap:
                    temp_this_command_code_buf.push_back(WO_OPCODE(sidmap));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                    break;
                case instruct::opcode::idstr:
                    temp_this_command_code_buf.push_back(WO_OPCODE(idstr));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                    break;

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
                case instruct::mkclos:
                {
                    if (config::ENABLE_JUST_IN_TIME)
                        env->_mkclos_opcode_offsets.push_back(generated_runtime_code_buf.size());

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
                        case instruct::extern_opcode_page_0::setref:
                            temp_this_command_code_buf.push_back(WO_OPCODE_EXT0(setref));
                            WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                            WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                            break;
                        case instruct::extern_opcode_page_0::trans:
                            temp_this_command_code_buf.push_back(WO_OPCODE_EXT0(trans));
                            WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                            WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                            break;
                            /*case instruct::extern_opcode_page_0::mknilmap:
                                temp_this_command_code_buf.push_back(WO_OPCODE_EXT0(mknilmap));
                                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                                break;*/
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
                        case instruct::extern_opcode_page_0::movdup:
                            temp_this_command_code_buf.push_back(WO_OPCODE_EXT0(movdup));
                            WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                            WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                            break;
                        case instruct::extern_opcode_page_0::veh:
                            if (WO_IR.op1)
                            {
                                // begin
                                wo_assert(dynamic_cast<opnum::tag*>(WO_IR.op1) != nullptr, "Operator num should be a tag.");

                                temp_this_command_code_buf.push_back(WO_OPCODE_EXT0(veh, 10));

                                jmp_record_table[dynamic_cast<opnum::tag*>(WO_IR.op1)->name]
                                    .push_back(generated_runtime_code_buf.size() + 1 + 1);
                                temp_this_command_code_buf.push_back(0x00);
                                temp_this_command_code_buf.push_back(0x00);
                                temp_this_command_code_buf.push_back(0x00);
                                temp_this_command_code_buf.push_back(0x00);

                            }
                            else if (WO_IR.op2)
                            {
                                // clean
                                wo_assert(dynamic_cast<opnum::tag*>(WO_IR.op2) != nullptr, "Operator num should be a tag.");
                                temp_this_command_code_buf.push_back(WO_OPCODE_EXT0(veh, 00));

                                jmp_record_table[dynamic_cast<opnum::tag*>(WO_IR.op2)->name]
                                    .push_back(generated_runtime_code_buf.size() + 1 + 1);

                                temp_this_command_code_buf.push_back(0x00);
                                temp_this_command_code_buf.push_back(0x00);
                                temp_this_command_code_buf.push_back(0x00);
                                temp_this_command_code_buf.push_back(0x00);
                            }
                            else
                            {
                                // throw
                                temp_this_command_code_buf.push_back(WO_OPCODE_EXT0(veh, 01));
                            }
                            break;
                        case instruct::extern_opcode_page_0::mkunion:
                        {
                            temp_this_command_code_buf.push_back(WO_OPCODE_EXT0(mkunion));
                            WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);

                            uint16_t id = (uint16_t)WO_IR.opinteger;
                            byte_t* readptr = (byte_t*)&id;
                            temp_this_command_code_buf.push_back(readptr[0]);
                            temp_this_command_code_buf.push_back(readptr[1]);
                            break;
                        }
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
            env->program_debug_info = pdb_info;

            for (auto& extern_func_info : pdb_info->extern_function_map)
            {
                extern_func_info.second = pdb_info->get_runtime_ip_by_ip(extern_func_info.second);
            }
            env->rt_codes = pdb_info->runtime_codes_base = code_buf;

            return env;
        }

    };

}