#pragma once

#include "rs_assert.hpp"
#include "rs_basic_type.hpp"
#include "rs_instruct.hpp"
#include "rs_meta.hpp"
#include "rs_compiler_parser.hpp"
#include "rs_env_locale.hpp"
#include "rs_global_setting.hpp"
#include "rs_lang_extern_symbol_loader.hpp"
#include "rs_shared_ptr.hpp"
#include "rs_memory.hpp"

#include <cstring>
#include <string>

namespace rs
{
    namespace opnum
    {
        struct opnumbase
        {
            virtual ~opnumbase() = default;
            virtual size_t generate_opnum_to_buffer(cxx_vec_t<byte_t>& buffer) const
            {
                rs_error("This type can not generate opnum.");

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
                index_from, ths = index_from,

                last_special_register = 0b00111111,
            };

            reg(uint8_t _id) noexcept
                :id(_id)
            {
            }

            static constexpr uint8_t bp_offset(int8_t offset)
            {
                rs_assert(offset >= -64 && offset <= 63);

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
                rs_assert(is_bp_offset());
#define RS_SIGNED_SHIFT(VAL) (((signed char)((unsigned char)(((unsigned char)(VAL))<<1)))>>1)
                return RS_SIGNED_SHIFT(id);
#undef RS_SIGNED_SHIFT
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
                rs_assert(type() != value::valuetype::invalid, "Invalid immediate.");
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
                    string_t::gc_new<gcbase::gctype::eden>(v->gcunit, val);
                else if constexpr (std::is_pointer<T>::value)
                    v->handle = (uint64_t)val;
                else if constexpr (std::is_integral<T>::value)
                    v->integer = val;
                else if constexpr (std::is_floating_point<T>::value)
                    v->real = val;
                else
                    rs_error("Invalid immediate.");
            }

            virtual int64_t try_int()const override
            {
                if constexpr (std::is_integral<T>::value)
                    return val;

                rs_error("Immediate is not integer.");
                return 0;
            }
            virtual int64_t try_set_int(int64_t _val) override
            {
                if constexpr (std::is_integral<T>::value)
                    return val = (T)_val;

                rs_error("Immediate is not integer.");
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
            std::string source_file = "not_found";
        };
        struct function_symbol_infor
        {
            struct variable_symbol_infor
            {
                std::string name;
                size_t define_place;
                rs_integer_t bp_offset;
            };
            size_t ir_begin;
            size_t ir_end;
            size_t in_stack_reg_count;

            std::map<std::string, std::vector<variable_symbol_infor>> variables;

            void add_variable_define(const std::wstring varname, size_t rowno, rs_integer_t locat)
            {
                variables[wstr_to_str(varname)].push_back(
                    variable_symbol_infor
                    {
                        wstr_to_str(varname),
                        rowno,
                        (rs_integer_t)(locat >= 0 ? locat + in_stack_reg_count : locat)
                    }
                );
            }
        };

        using filename_rowno_colno_ip_info_t = std::map<std::string, std::map<size_t, std::map<size_t, size_t>>>;
        using ip_src_location_info_t = std::map<size_t, location>;
        using runtime_ip_compile_ip_info_t = std::map<size_t, size_t>;
        using function_signature_ip_info_t = std::map<std::string, function_symbol_infor>;

        filename_rowno_colno_ip_info_t  _general_src_data_buf_a;
        ip_src_location_info_t          _general_src_data_buf_b;
        function_signature_ip_info_t    _function_ip_data_buf;
        runtime_ip_compile_ip_info_t    pdd_rt_code_byte_offset_to_ir;
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
        void add_func_variable(ast::ast_value_function_define* funcdef, const std::wstring& varname, size_t rowno, rs_integer_t loc);

        const location& get_src_location_by_runtime_ip(const  byte_t* rt_pos) const;
        size_t get_ip_by_src_location(const std::string& src_name, size_t rowno, bool strict = false)const;
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

        shared_pointer<program_debug_data_info> program_debug_info;

        ~runtime_env()
        {
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

#define RS_IS_REG(OPNUM)(dynamic_cast<opnum::reg*>(OPNUM))
            uint8_t dr()
            {
                return (RS_IS_REG(op1) ? 0b00000010 : 0) | (RS_IS_REG(op2) ? 0b00000001 : 0);
            }
#undef RS_IS_REG
        };

        cxx_vec_t<ir_command> ir_command_buffer;
        std::map<size_t, cxx_vec_t<std::string>> tag_irbuffer_offset;

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
                rs_assert(_global->offset >= 0);
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

#define RS_OPNUM(OPNUM) (_check_and_add_const(\
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

            rs_test(tr_regist_mapping.size() <= 64); // fast bt_offset maxim offset
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
                        if (imm_opnum_stx_offset->try_int() <= 0)
                            imm_opnum_stx_offset->try_set_int(imm_opnum_stx_offset->try_int() - maxim_offset);
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
                                    ir_command{ instruct::ldsr, RS_OPNUM(reg_r0), RS_OPNUM(imm_offset) });         // ldsr r0, imm(real_offset)
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
                                    ir_command{ instruct::ldsr, RS_OPNUM(reg_r1), RS_OPNUM(imm_offset) });         // ldsr r0, imm(real_offset)
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


#define RS_PUT_IR_TO_BUFFER(OPCODE, ...) ir_command_buffer.emplace_back(ir_command{OPCODE, __VA_ARGS__});


        template<typename OP1T, typename OP2T>
        void mov(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not move value to immediate.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::mov, RS_OPNUM(op1), RS_OPNUM(op2));
        }

        template<typename OP1T, typename OP2T>
        void set(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not set value to immediate.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::set, RS_OPNUM(op1), RS_OPNUM(op2));
        }

        template<typename OP1T>
        void psh(const OP1T& op1)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                , "Argument(s) should be opnum.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::psh, RS_OPNUM(op1));
        }

        void pshn(uint16_t op1)
        {
            RS_PUT_IR_TO_BUFFER(instruct::opcode::psh, nullptr, nullptr, (uint16_t)op1);
        }

        size_t reserved_stackvalue()
        {
            RS_PUT_IR_TO_BUFFER(instruct::opcode::psh, nullptr, nullptr, (uint16_t)0);
            return get_now_ip();
        }

        void reserved_stackvalue(size_t ip, uint16_t sz)
        {
            rs_assert(ip);
            ir_command_buffer[ip - 1].opinteger = sz;
        }

        template<typename OP1T>
        void pshr(const OP1T& op1)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                , "Argument(s) should be opnum.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::pshr, RS_OPNUM(op1));
        }
        template<typename OP1T>
        void pop(const OP1T& op1)
        {
            if constexpr (std::is_integral<OP1T>::value)
            {
                rs_assert(0 <= op1 && op1 <= UINT16_MAX);
                RS_PUT_IR_TO_BUFFER(instruct::opcode::pop, nullptr, nullptr, (uint16_t)op1);
            }
            else
            {
                static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                    , "Argument(s) should be opnum.");
                static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                    "Can not pop value to immediate.");

                RS_PUT_IR_TO_BUFFER(instruct::opcode::pop, RS_OPNUM(op1));
            }
        }

        template<typename OP1T>
        void popr(const OP1T& op1)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                , "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not pop value to immediate.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::popr, RS_OPNUM(op1));
        }

        template<typename OP1T, typename OP2T>
        void addi(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not add value to immediate.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::addi, RS_OPNUM(op1), RS_OPNUM(op2));
        }

        template<typename OP1T, typename OP2T>
        void subi(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not sub value to immediate.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::subi, RS_OPNUM(op1), RS_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void muli(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not mul value to immediate.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::muli, RS_OPNUM(op1), RS_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void divi(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not div value to immediate.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::divi, RS_OPNUM(op1), RS_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void modi(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not mod value to immediate.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::modi, RS_OPNUM(op1), RS_OPNUM(op2));
        }

        template<typename OP1T, typename OP2T>
        void movx(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not mov value to immediate.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::movx, RS_OPNUM(op1), RS_OPNUM(op2));
        }

        template<typename OP1T, typename OP2T>
        void addx(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not add value to immediate.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::addx, RS_OPNUM(op1), RS_OPNUM(op2), 1);
        }
        template<typename OP1T, typename OP2T>
        void subx(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not sub value to immediate.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::subx, RS_OPNUM(op1), RS_OPNUM(op2), 1);
        }
        template<typename OP1T, typename OP2T>
        void mulx(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not mul value to immediate.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::mulx, RS_OPNUM(op1), RS_OPNUM(op2), 1);
        }
        template<typename OP1T, typename OP2T>
        void divx(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not div value to immediate.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::divx, RS_OPNUM(op1), RS_OPNUM(op2), 1);
        }
        template<typename OP1T, typename OP2T>
        void modx(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not mod value to immediate.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::modx, RS_OPNUM(op1), RS_OPNUM(op2), 1);
        }

        template<typename OP1T, typename OP2T>
        void addmovx(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not add value to immediate.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::addx, RS_OPNUM(op1), RS_OPNUM(op2), 0);
        }
        template<typename OP1T, typename OP2T>
        void submovx(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not sub value to immediate.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::subx, RS_OPNUM(op1), RS_OPNUM(op2), 0);
        }
        template<typename OP1T, typename OP2T>
        void mulmovx(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not mul value to immediate.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::mulx, RS_OPNUM(op1), RS_OPNUM(op2), 0);
        }
        template<typename OP1T, typename OP2T>
        void divmovx(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not div value to immediate.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::divx, RS_OPNUM(op1), RS_OPNUM(op2), 0);
        }
        template<typename OP1T, typename OP2T>
        void modmovx(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not mod value to immediate.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::modx, RS_OPNUM(op1), RS_OPNUM(op2), 0);
        }

        template<typename OP1T, typename OP2T>
        void addr(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not add value to immediate.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::addr, RS_OPNUM(op1), RS_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void subr(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not sub value to immediate.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::subr, RS_OPNUM(op1), RS_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void mulr(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not mul value to immediate.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::mulr, RS_OPNUM(op1), RS_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void divr(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not div value to immediate.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::divr, RS_OPNUM(op1), RS_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void modr(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not mod value to immediate.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::modr, RS_OPNUM(op1), RS_OPNUM(op2));
        }

        template<typename OP1T, typename OP2T>
        void addh(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not add value to immediate.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::addh, RS_OPNUM(op1), RS_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void subh(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not sub value to immediate.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::subh, RS_OPNUM(op1), RS_OPNUM(op2));
        }

        template<typename OP1T, typename OP2T>
        void adds(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not add value to immediate.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::adds, RS_OPNUM(op1), RS_OPNUM(op2));
        }

        template<typename OP1T, typename OP2T>
        void equb(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::equb, RS_OPNUM(op1), RS_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void nequb(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::nequb, RS_OPNUM(op1), RS_OPNUM(op2));
        }

        template<typename OP1T, typename OP2T>
        void equx(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::equx, RS_OPNUM(op1), RS_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void nequx(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::nequx, RS_OPNUM(op1), RS_OPNUM(op2));
        }

        template<typename OP1T, typename OP2T>
        void land(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::land, RS_OPNUM(op1), RS_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void lor(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::lor, RS_OPNUM(op1), RS_OPNUM(op2));
        }
        template<typename OP1T>
        void lnot(const OP1T& op1)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                , "Argument(s) should be opnum.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::lnot, RS_OPNUM(op1));
        }
        template<typename OP1T, typename OP2T>
        void gti(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::gti, RS_OPNUM(op1), RS_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void lti(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::lti, RS_OPNUM(op1), RS_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void egti(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::egti, RS_OPNUM(op1), RS_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void elti(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::elti, RS_OPNUM(op1), RS_OPNUM(op2));
        }

        template<typename OP1T, typename OP2T>
        void ltr(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::ltr, RS_OPNUM(op1), RS_OPNUM(op2));
        }

        template<typename OP1T, typename OP2T>
        void gtr(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::gtr, RS_OPNUM(op1), RS_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void eltr(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::eltr, RS_OPNUM(op1), RS_OPNUM(op2));
        }

        template<typename OP1T, typename OP2T>
        void egtr(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::egtr, RS_OPNUM(op1), RS_OPNUM(op2));
        }

        template<typename OP1T, typename OP2T>
        void ltx(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::ltx, RS_OPNUM(op1), RS_OPNUM(op2));
        }

        template<typename OP1T, typename OP2T>
        void gtx(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::gtx, RS_OPNUM(op1), RS_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void eltx(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::eltx, RS_OPNUM(op1), RS_OPNUM(op2));
        }

        template<typename OP1T, typename OP2T>
        void egtx(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::egtx, RS_OPNUM(op1), RS_OPNUM(op2));
        }

        template<typename OP1T, typename OP2T>
        void lds(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not load value and save to immediate.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::lds, RS_OPNUM(op1), RS_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void ldsr(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not load value and save to immediate.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::ldsr, RS_OPNUM(op1), RS_OPNUM(op2));
        }

        template<typename OP1T, typename OP2T>
        void movcast(const OP1T& op1, const OP2T& op2, value::valuetype vtt)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not move value to immediate.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::movcast, RS_OPNUM(op1), RS_OPNUM(op2), (int)vtt);
        }
        template<typename OP1T, typename OP2T>
        void setcast(const OP1T& op1, const OP2T& op2, value::valuetype vtt)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not set value to immediate.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::setcast, RS_OPNUM(op1), RS_OPNUM(op2), (int)vtt);
        }
        template<typename OP1T>
        void typeas(const OP1T& op1, value::valuetype vtt)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value,
                "Argument(s) should be opnum.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::typeas, RS_OPNUM(op1), nullptr, (int)vtt);
        }
        template<typename OP1T>
        void typeis(const OP1T& op1, value::valuetype vtt)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value,
                "Argument(s) should be opnum.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::typeas, RS_OPNUM(op1), nullptr, (int)vtt, 1);
        }

        template<typename OP1T>
        void call(const OP1T& op1)
        {
            if constexpr (std::is_base_of<opnum::opnumbase, OP1T>::value)
            {
                if (dynamic_cast<opnum::tag*>(const_cast<OP1T*>(&op1)))
                {
                    RS_PUT_IR_TO_BUFFER(instruct::opcode::calln, nullptr, RS_OPNUM(op1));
                }
                else if (auto* handle_exp = dynamic_cast<opnum::imm<void*>*>(const_cast<OP1T*>(&op1)))
                {
                    RS_PUT_IR_TO_BUFFER(instruct::opcode::calln, reinterpret_cast<opnum::opnumbase*>(handle_exp->val));
                }
                else
                {
                    RS_PUT_IR_TO_BUFFER(instruct::opcode::call, RS_OPNUM(op1));
                }
            }
            else if constexpr (std::is_pointer<OP1T>::value)
            {
                RS_PUT_IR_TO_BUFFER(instruct::opcode::calln, reinterpret_cast<opnum::opnumbase*>(op1));
            }
            else if constexpr (std::is_integral<OP1T>::value)
            {
                rs_assert(0 <= op1 && op1 <= UINT32_MAX, "Immediate instruct address is to large to call.");

                uint32_t address = (uint32_t)op1;
                RS_PUT_IR_TO_BUFFER(instruct::opcode::calln, nullptr, nullptr, static_cast<int32_t>(op1));
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
            RS_PUT_IR_TO_BUFFER(instruct::opcode::jt, RS_OPNUM(op1));
        }

        void jf(const opnum::tag& op1)
        {
            RS_PUT_IR_TO_BUFFER(instruct::opcode::jf, RS_OPNUM(op1));
        }

        void jmp(const opnum::tag& op1)
        {
            RS_PUT_IR_TO_BUFFER(instruct::opcode::jmp, RS_OPNUM(op1));
        }

        void ret()
        {
            RS_PUT_IR_TO_BUFFER(instruct::opcode::ret);
        }
        void retval()
        {
            rs_error("'retval' is not able now...");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::ret, nullptr, nullptr, 1);
        }

        void calljit()
        {
            RS_PUT_IR_TO_BUFFER(instruct::opcode::calljit);
        }

        void nop()
        {
            RS_PUT_IR_TO_BUFFER(instruct::opcode::nop);
        }

        void abrt()
        {
            RS_PUT_IR_TO_BUFFER(instruct::opcode::abrt);
        }

        void end()
        {
            RS_PUT_IR_TO_BUFFER(instruct::opcode::abrt, nullptr, nullptr, 1);
        }

        void tag(const string_t& tagname)
        {
            tag_irbuffer_offset[ir_command_buffer.size()].push_back(tagname);
        }

        void veh_begin(const opnum::tag& op1)
        {
            RS_PUT_IR_TO_BUFFER(instruct::opcode::veh, RS_OPNUM(op1));
        }

        void veh_clean(const opnum::tag& op1)
        {
            RS_PUT_IR_TO_BUFFER(instruct::opcode::veh, nullptr, RS_OPNUM(op1));
        }

        void veh_throw()
        {
            RS_PUT_IR_TO_BUFFER(instruct::opcode::veh);
        }

        template<typename OP1T, typename OP2T>
        void mkarr(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not set value to immediate.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::mkarr, RS_OPNUM(op1), RS_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void mkmap(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not set value to immediate.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::mkmap, RS_OPNUM(op1), RS_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void idx(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::idx, RS_OPNUM(op1), RS_OPNUM(op2));
        }

        template<typename OP1T, typename OP2T>
        void ext_setref(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not set immediate as ref.");

            auto& codeb = RS_PUT_IR_TO_BUFFER(instruct::opcode::ext, RS_OPNUM(op1), RS_OPNUM(op2));
            codeb.ext_page_id = 0;
            codeb.ext_opcode_p0 = instruct::extern_opcode_page_0::setref;
        }

        template<typename OP1T, typename OP2T>
        void ext_movdup(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not mov value to immediate.");

            auto& codeb = RS_PUT_IR_TO_BUFFER(instruct::opcode::ext, RS_OPNUM(op1), RS_OPNUM(op2));
            codeb.ext_page_id = 0;
            codeb.ext_opcode_p0 = instruct::extern_opcode_page_0::movdup;
        }

        void ext_endjit()
        {
            auto& codeb = RS_PUT_IR_TO_BUFFER(instruct::opcode::ext);
            codeb.ext_page_id = 1;
            codeb.ext_opcode_p1 = instruct::extern_opcode_page_1::endjit;
        }

        template<typename OP1T, typename OP2T>
        void ext_packargs(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value &&
                std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            auto& codeb = RS_PUT_IR_TO_BUFFER(instruct::opcode::ext, RS_OPNUM(op1), RS_OPNUM(op2));
            codeb.ext_page_id = 0;
            codeb.ext_opcode_p0 = instruct::extern_opcode_page_0::packargs;
        }

        template<typename OP1T>
        void ext_unpackargs(const OP1T& op1, rs_integer_t unpack_count)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value,
                "Argument(s) should be opnum.");

            opnum::imm<rs_integer_t> unpacked_count(unpack_count);

            auto& codeb = RS_PUT_IR_TO_BUFFER(instruct::opcode::ext, RS_OPNUM(op1), RS_OPNUM(unpacked_count));
            codeb.ext_page_id = 0;
            codeb.ext_opcode_p0 = instruct::extern_opcode_page_0::unpackargs;
        }

#undef RS_OPNUM
#undef RS_PUT_IR_TO_BUFFER

    private:
        shared_pointer<runtime_env> finalize()
        {
            // 0. 
            // 1. Generate constant & global & register & runtime_stack memory buffer
            size_t constant_value_count = constant_record_list.size();
            size_t global_allign_takeplace_for_avoiding_false_shared =
                config::ENABLE_AVOIDING_FALSE_SHARED ?
                ((size_t)(platform_info::CPU_CACHELINE_SIZE / (double)sizeof(rs::value) + 0.5)) : (1);
            size_t global_value_count = 0;

            for (auto* global_opnum : global_record_list)
            {
                rs_assert(global_opnum->offset + constant_value_count + global_allign_takeplace_for_avoiding_false_shared
                    < INT32_MAX&& global_opnum->offset >= 0);
                global_opnum->real_offset_const_glb = (int32_t)
                    (global_opnum->offset * global_allign_takeplace_for_avoiding_false_shared
                        + constant_value_count + global_allign_takeplace_for_avoiding_false_shared);

                if (((size_t)global_opnum->offset + 1) > global_value_count)
                    global_value_count = (size_t)global_opnum->offset + 1;
            }

            size_t real_register_count = 64;     // t0-t15 r0-r15 (32) special reg (32)
            size_t runtime_stack_count = 65536;  // by default

            size_t preserve_memory_size =
                constant_value_count
                + global_allign_takeplace_for_avoiding_false_shared
                + global_value_count * global_allign_takeplace_for_avoiding_false_shared
                + global_allign_takeplace_for_avoiding_false_shared
                + real_register_count
                + runtime_stack_count;

            const auto v = sizeof(value);

            value* preserved_memory = (value*)alloc64(preserve_memory_size * sizeof(value));

            cxx_vec_t<byte_t> generated_runtime_code_buf; // It will be put to 16 byte allign mem place.

            std::map<std::string, cxx_vec_t<size_t>> jmp_record_table;
            std::map<std::string, cxx_vec_t<value*>> jmp_record_table_for_immtag;
            std::map<std::string, uint32_t> tag_offset_vector_table;

            rs_assert(preserved_memory, "Alloc memory fail.");

            memset(preserved_memory, 0, preserve_memory_size * sizeof(value));
            //  // Fill constant
            for (auto constant_record : constant_record_list)
            {
                rs_assert(constant_record->constant_index < constant_value_count,
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

#define RS_IR ir_command_buffer[ip]
#define RS_OPCODE(...) rs_macro_overload(RS_OPCODE,__VA_ARGS__)
#define RS_OPCODE_1(OPCODE) (instruct(instruct::opcode::OPCODE, RS_IR.dr()).opcode_dr)
#define RS_OPCODE_2(OPCODE,DR) (instruct(instruct::opcode::OPCODE, 0b000000##DR).opcode_dr)

#define RS_OPCODE_EXT0(...) rs_macro_overload(RS_OPCODE_EXT0,__VA_ARGS__)
#define RS_OPCODE_EXT0_1(OPCODE) (instruct((instruct::opcode)instruct::extern_opcode_page_0::OPCODE, RS_IR.dr()).opcode_dr)
#define RS_OPCODE_EXT0_2(OPCODE,DR) (instruct((instruct::opcode)instruct::extern_opcode_page_0::OPCODE, 0b000000##DR).opcode_dr)

#define RS_OPCODE_EXT1(...) rs_macro_overload(RS_OPCODE_EXT1,__VA_ARGS__)
#define RS_OPCODE_EXT1_1(OPCODE) (instruct((instruct::opcode)instruct::extern_opcode_page_1::OPCODE, RS_IR.dr()).opcode_dr)
#define RS_OPCODE_EXT1_2(OPCODE,DR) (instruct((instruct::opcode)instruct::extern_opcode_page_1::OPCODE, 0b000000##DR).opcode_dr)

                cxx_vec_t<byte_t> temp_this_command_code_buf; // one command will be store here tempery for coding allign
                size_t already_allign_tmp_sz = 0; // temp store the written sz for allign check, will be reset 0 at each command loop;
                size_t need_fill_count = 0;
                auto auto_check_mem_allign = [&](int offset, size_t write_sz)
                {
                    if (config::ENABLE_IR_CODE_ACTIVE_ALLIGN)
                        if (auto no_allign_sz = (generated_runtime_code_buf.size() + already_allign_tmp_sz + offset) % write_sz)
                        {
                            rs_assert(write_sz && (write_sz % 2 == 0));
                            need_fill_count += write_sz - no_allign_sz;

                            already_allign_tmp_sz += write_sz; // temp add writed sz;
                        }
                };

                switch (RS_IR.opcode)
                {
                case instruct::opcode::nop:
                    temp_this_command_code_buf.push_back(RS_OPCODE(nop));
                    break;
                case instruct::opcode::set:
                    temp_this_command_code_buf.push_back(RS_OPCODE(set));
                    auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    auto_check_mem_allign(1, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));
                    break;
                case instruct::opcode::mov:
                    temp_this_command_code_buf.push_back(RS_OPCODE(mov));
                    auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    auto_check_mem_allign(1, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));
                    break;

                case instruct::opcode::movcast:
                    temp_this_command_code_buf.push_back(RS_OPCODE(movcast));
                    auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    auto_check_mem_allign(1, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));
                    temp_this_command_code_buf.push_back((byte_t)RS_IR.opinteger);
                    break;
                case instruct::opcode::setcast:
                    temp_this_command_code_buf.push_back(RS_OPCODE(setcast));
                    auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    auto_check_mem_allign(1, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));
                    temp_this_command_code_buf.push_back((byte_t)RS_IR.opinteger);
                    break;
                case instruct::opcode::typeas:

                    if (RS_IR.ext_page_id)
                    {
                        temp_this_command_code_buf.push_back(RS_OPCODE(typeas) | 0b01);
                        auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                        temp_this_command_code_buf.push_back((byte_t)RS_IR.opinteger);
                    }
                    else
                    {
                        temp_this_command_code_buf.push_back(RS_OPCODE(typeas));
                        auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                        temp_this_command_code_buf.push_back((byte_t)RS_IR.opinteger);
                    }


                    break;
                case instruct::opcode::psh:
                    if (nullptr == RS_IR.op1)
                    {
                        if (RS_IR.opinteger == 0)
                            break;

                        temp_this_command_code_buf.push_back(RS_OPCODE(psh, 00));

                        rs_assert(RS_IR.opinteger > 0 && RS_IR.opinteger <= UINT16_MAX
                            , "Invalid count to reserve in stack.");

                        uint16_t opushort = (uint16_t)RS_IR.opinteger;
                        byte_t* readptr = (byte_t*)&opushort;
                        temp_this_command_code_buf.push_back(readptr[0]);
                        temp_this_command_code_buf.push_back(readptr[1]);

                        auto_check_mem_allign(1, 2);
                    }
                    else
                    {
                        temp_this_command_code_buf.push_back(RS_OPCODE(psh) | 0b01);
                        auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    }
                    break;
                case instruct::opcode::pshr:
                    temp_this_command_code_buf.push_back(RS_OPCODE(pshr));
                    auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    break;
                case instruct::opcode::pop:
                    if (nullptr == RS_IR.op1)
                    {
                        if (RS_IR.opinteger == 0)
                            break;

                        temp_this_command_code_buf.push_back(RS_OPCODE(pop, 00));

                        rs_assert(RS_IR.opinteger > 0 && RS_IR.opinteger <= UINT16_MAX
                            , "Invalid count to pop from stack.");

                        uint16_t opushort = (uint16_t)RS_IR.opinteger;
                        byte_t* readptr = (byte_t*)&opushort;

                        temp_this_command_code_buf.push_back(readptr[0]);
                        temp_this_command_code_buf.push_back(readptr[1]);

                        auto_check_mem_allign(1, 2);
                    }
                    else
                    {
                        temp_this_command_code_buf.push_back(RS_OPCODE(pop) | 0b01);
                        auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    }
                    break;
                case instruct::opcode::popr:
                    temp_this_command_code_buf.push_back(RS_OPCODE(popr));
                    auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    break;
                case instruct::opcode::lds:
                    temp_this_command_code_buf.push_back(RS_OPCODE(lds));
                    auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    auto_check_mem_allign(1, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));
                    break;
                case instruct::opcode::ldsr:
                    temp_this_command_code_buf.push_back(RS_OPCODE(ldsr));
                    auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    auto_check_mem_allign(1, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));
                    break;
                case instruct::opcode::addi:
                    temp_this_command_code_buf.push_back(RS_OPCODE(addi));
                    auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    auto_check_mem_allign(1, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));
                    break;
                case instruct::opcode::subi:
                    temp_this_command_code_buf.push_back(RS_OPCODE(subi));
                    auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    auto_check_mem_allign(1, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));
                    break;
                case instruct::opcode::muli:
                    temp_this_command_code_buf.push_back(RS_OPCODE(muli));
                    auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    auto_check_mem_allign(1, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));
                    break;
                case instruct::opcode::divi:
                    temp_this_command_code_buf.push_back(RS_OPCODE(divi));
                    auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    auto_check_mem_allign(1, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));
                    break;
                case instruct::opcode::modi:
                    temp_this_command_code_buf.push_back(RS_OPCODE(modi));
                    auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    auto_check_mem_allign(1, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));
                    break;
                case instruct::opcode::movx:
                    temp_this_command_code_buf.push_back(RS_OPCODE(movx));
                    auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    auto_check_mem_allign(1, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));
                    break;
                case instruct::opcode::addx:
                    temp_this_command_code_buf.push_back(RS_OPCODE(addx));
                    temp_this_command_code_buf.push_back((byte_t)RS_IR.opinteger);
                    auto_check_mem_allign(2, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    auto_check_mem_allign(2, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));

                    break;
                case instruct::opcode::subx:
                    temp_this_command_code_buf.push_back(RS_OPCODE(subx));
                    temp_this_command_code_buf.push_back((byte_t)RS_IR.opinteger);
                    auto_check_mem_allign(2, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    auto_check_mem_allign(2, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));

                    break;
                case instruct::opcode::mulx:
                    temp_this_command_code_buf.push_back(RS_OPCODE(mulx));
                    temp_this_command_code_buf.push_back((byte_t)RS_IR.opinteger);
                    auto_check_mem_allign(2, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    auto_check_mem_allign(2, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));

                    break;
                case instruct::opcode::divx:
                    temp_this_command_code_buf.push_back(RS_OPCODE(divx));
                    temp_this_command_code_buf.push_back((byte_t)RS_IR.opinteger);
                    auto_check_mem_allign(2, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    auto_check_mem_allign(2, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));

                    break;
                case instruct::opcode::modx:
                    temp_this_command_code_buf.push_back(RS_OPCODE(modx));
                    temp_this_command_code_buf.push_back((byte_t)RS_IR.opinteger);
                    auto_check_mem_allign(2, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    auto_check_mem_allign(2, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));

                    break;
                case instruct::opcode::addr:
                    temp_this_command_code_buf.push_back(RS_OPCODE(addr));
                    auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    auto_check_mem_allign(1, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));
                    break;
                case instruct::opcode::subr:
                    temp_this_command_code_buf.push_back(RS_OPCODE(subr));
                    auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    auto_check_mem_allign(1, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));
                    break;
                case instruct::opcode::mulr:
                    temp_this_command_code_buf.push_back(RS_OPCODE(mulr));
                    auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    auto_check_mem_allign(1, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));
                    break;
                case instruct::opcode::divr:
                    temp_this_command_code_buf.push_back(RS_OPCODE(divr));
                    auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    auto_check_mem_allign(1, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));
                    break;
                case instruct::opcode::modr:
                    temp_this_command_code_buf.push_back(RS_OPCODE(modr));
                    auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    auto_check_mem_allign(1, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));
                    break;
                case instruct::opcode::addh:
                    temp_this_command_code_buf.push_back(RS_OPCODE(addh));
                    auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    auto_check_mem_allign(1, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));
                    break;
                case instruct::opcode::subh:
                    temp_this_command_code_buf.push_back(RS_OPCODE(subh));
                    auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    auto_check_mem_allign(1, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));
                    break;
                case instruct::opcode::adds:
                    temp_this_command_code_buf.push_back(RS_OPCODE(adds));
                    auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    auto_check_mem_allign(1, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));
                    break;

                case instruct::opcode::equb:
                    temp_this_command_code_buf.push_back(RS_OPCODE(equb));
                    auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    auto_check_mem_allign(1, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));
                    break;
                case instruct::opcode::nequb:
                    temp_this_command_code_buf.push_back(RS_OPCODE(nequb));
                    auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    auto_check_mem_allign(1, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));
                    break;
                case instruct::opcode::equx:
                    temp_this_command_code_buf.push_back(RS_OPCODE(equx));
                    auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    auto_check_mem_allign(1, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));
                    break;
                case instruct::opcode::nequx:
                    temp_this_command_code_buf.push_back(RS_OPCODE(nequx));
                    auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    auto_check_mem_allign(1, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));
                    break;

                case instruct::opcode::land:
                    temp_this_command_code_buf.push_back(RS_OPCODE(land));
                    auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    auto_check_mem_allign(1, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));
                    break;
                case instruct::opcode::lor:
                    temp_this_command_code_buf.push_back(RS_OPCODE(lor));
                    auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    auto_check_mem_allign(1, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));
                    break;
                case instruct::opcode::lnot:
                    temp_this_command_code_buf.push_back(RS_OPCODE(lnot));
                    auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    break;

                case instruct::opcode::gti:
                    temp_this_command_code_buf.push_back(RS_OPCODE(gti));
                    auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    auto_check_mem_allign(1, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));
                    break;
                case instruct::opcode::lti:
                    temp_this_command_code_buf.push_back(RS_OPCODE(lti));
                    auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    auto_check_mem_allign(1, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));
                    break;
                case instruct::opcode::egti:
                    temp_this_command_code_buf.push_back(RS_OPCODE(egti));
                    auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    auto_check_mem_allign(1, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));
                    break;
                case instruct::opcode::elti:
                    temp_this_command_code_buf.push_back(RS_OPCODE(elti));
                    auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    auto_check_mem_allign(1, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));
                    break;

                case instruct::opcode::gtr:
                    temp_this_command_code_buf.push_back(RS_OPCODE(gtr));
                    auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    auto_check_mem_allign(1, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));
                    break;
                case instruct::opcode::ltr:
                    temp_this_command_code_buf.push_back(RS_OPCODE(ltr));
                    auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    auto_check_mem_allign(1, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));
                    break;
                case instruct::opcode::egtr:
                    temp_this_command_code_buf.push_back(RS_OPCODE(egtr));
                    auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    auto_check_mem_allign(1, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));
                    break;
                case instruct::opcode::eltr:
                    temp_this_command_code_buf.push_back(RS_OPCODE(eltr));
                    auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    auto_check_mem_allign(1, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));
                    break;

                case instruct::opcode::gtx:
                    temp_this_command_code_buf.push_back(RS_OPCODE(gtx));
                    auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    auto_check_mem_allign(1, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));
                    break;
                case instruct::opcode::ltx:
                    temp_this_command_code_buf.push_back(RS_OPCODE(ltx));
                    auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    auto_check_mem_allign(1, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));
                    break;
                case instruct::opcode::egtx:
                    temp_this_command_code_buf.push_back(RS_OPCODE(egtx));
                    auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    auto_check_mem_allign(1, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));
                    break;
                case instruct::opcode::eltx:
                    temp_this_command_code_buf.push_back(RS_OPCODE(eltx));
                    auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    auto_check_mem_allign(1, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));
                    break;
                case instruct::opcode::mkarr:
                    temp_this_command_code_buf.push_back(RS_OPCODE(mkarr));
                    auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    auto_check_mem_allign(1, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));
                    break;
                case instruct::opcode::mkmap:
                    temp_this_command_code_buf.push_back(RS_OPCODE(mkmap));
                    auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    auto_check_mem_allign(1, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));
                    break;
                case instruct::opcode::idx:
                    temp_this_command_code_buf.push_back(RS_OPCODE(idx));
                    auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    auto_check_mem_allign(1, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));
                    break;

                case instruct::opcode::jt:
                    temp_this_command_code_buf.push_back(RS_OPCODE(jt));
                    auto_check_mem_allign(1, 4);
                    rs_assert(dynamic_cast<opnum::tag*>(RS_IR.op1) != nullptr, "Operator num should be a tag.");

                    jmp_record_table[dynamic_cast<opnum::tag*>(RS_IR.op1)->name]
                        .push_back(generated_runtime_code_buf.size() + need_fill_count + 1);

                    temp_this_command_code_buf.push_back(0x00);
                    temp_this_command_code_buf.push_back(0x00);
                    temp_this_command_code_buf.push_back(0x00);
                    temp_this_command_code_buf.push_back(0x00);


                    break;
                case instruct::opcode::jf:
                    temp_this_command_code_buf.push_back(RS_OPCODE(jf));
                    auto_check_mem_allign(1, 4);

                    rs_assert(dynamic_cast<opnum::tag*>(RS_IR.op1) != nullptr, "Operator num should be a tag.");

                    jmp_record_table[dynamic_cast<opnum::tag*>(RS_IR.op1)->name]
                        .push_back(generated_runtime_code_buf.size() + need_fill_count + 1);

                    temp_this_command_code_buf.push_back(0x00);
                    temp_this_command_code_buf.push_back(0x00);
                    temp_this_command_code_buf.push_back(0x00);
                    temp_this_command_code_buf.push_back(0x00);
                    break;

                case instruct::opcode::jmp:
                    temp_this_command_code_buf.push_back(RS_OPCODE(jmp, 00));
                    auto_check_mem_allign(1, 4);


                    rs_assert(dynamic_cast<opnum::tag*>(RS_IR.op1) != nullptr, "Operator num should be a tag.");

                    jmp_record_table[dynamic_cast<opnum::tag*>(RS_IR.op1)->name]
                        .push_back(generated_runtime_code_buf.size() + need_fill_count + 1);

                    temp_this_command_code_buf.push_back(0x00);
                    temp_this_command_code_buf.push_back(0x00);
                    temp_this_command_code_buf.push_back(0x00);
                    temp_this_command_code_buf.push_back(0x00);
                    break;

                case instruct::opcode::call:
                    temp_this_command_code_buf.push_back(RS_OPCODE(call));
                    auto_check_mem_allign(1, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                    break;
                case instruct::opcode::calln:
                    if (RS_IR.op2)
                    {
                        rs_assert(dynamic_cast<opnum::tag*>(RS_IR.op2) != nullptr, "Operator num should be a tag.");

                        temp_this_command_code_buf.push_back(RS_OPCODE(calln, 00));
                        auto_check_mem_allign(1, 4);

                        jmp_record_table[dynamic_cast<opnum::tag*>(RS_IR.op2)->name]
                            .push_back(generated_runtime_code_buf.size() + need_fill_count + 1);

                        temp_this_command_code_buf.push_back(0x00);
                        temp_this_command_code_buf.push_back(0x00);
                        temp_this_command_code_buf.push_back(0x00);
                        temp_this_command_code_buf.push_back(0x00);
                    }
                    else if (RS_IR.op1)
                    {
                        temp_this_command_code_buf.push_back(RS_OPCODE(calln, 01));
                        auto_check_mem_allign(1, 8);

                        uint64_t addr = (uint64_t)(RS_IR.op1);

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
                        temp_this_command_code_buf.push_back(RS_OPCODE(calln, 00));
                        auto_check_mem_allign(1, 4);

                        byte_t* readptr = (byte_t*)&RS_IR.opinteger;
                        temp_this_command_code_buf.push_back(readptr[0]);
                        temp_this_command_code_buf.push_back(readptr[1]);
                        temp_this_command_code_buf.push_back(readptr[2]);
                        temp_this_command_code_buf.push_back(readptr[3]);
                    }
                    break;
                case instruct::opcode::calljit:
                    temp_this_command_code_buf.push_back(RS_OPCODE(calljit, 00));
                    temp_this_command_code_buf.push_back(0);
                    auto_check_mem_allign(2, 8);
                    for (size_t i = 0; i < 8; i++)
                        temp_this_command_code_buf.push_back(0);

                    break;
                case instruct::opcode::ret:
                    if (RS_IR.opinteger)
                        temp_this_command_code_buf.push_back(RS_OPCODE(ret, 01));
                    else
                        temp_this_command_code_buf.push_back(RS_OPCODE(ret, 00));
                    break;
                case instruct::opcode::veh:
                    if (RS_IR.op1)
                    {
                        // begin
                        rs_assert(dynamic_cast<opnum::tag*>(RS_IR.op1) != nullptr, "Operator num should be a tag.");
                        auto_check_mem_allign(1, 4);

                        temp_this_command_code_buf.push_back(RS_OPCODE(veh, 10));
                        jmp_record_table[dynamic_cast<opnum::tag*>(RS_IR.op1)->name]
                            .push_back(generated_runtime_code_buf.size() + need_fill_count + 1);
                        temp_this_command_code_buf.push_back(0x00);
                        temp_this_command_code_buf.push_back(0x00);
                        temp_this_command_code_buf.push_back(0x00);
                        temp_this_command_code_buf.push_back(0x00);

                    }
                    else if (RS_IR.op2)
                    {
                        // clean
                        rs_assert(dynamic_cast<opnum::tag*>(RS_IR.op2) != nullptr, "Operator num should be a tag.");
                        auto_check_mem_allign(1, 4);

                        temp_this_command_code_buf.push_back(RS_OPCODE(veh, 00));
                        jmp_record_table[dynamic_cast<opnum::tag*>(RS_IR.op2)->name]
                            .push_back(generated_runtime_code_buf.size() + need_fill_count + 1);

                        temp_this_command_code_buf.push_back(0x00);
                        temp_this_command_code_buf.push_back(0x00);
                        temp_this_command_code_buf.push_back(0x00);
                        temp_this_command_code_buf.push_back(0x00);


                    }
                    else
                    {
                        // throw
                        temp_this_command_code_buf.push_back(RS_OPCODE(veh, 01));
                    }
                    break;
                case instruct::opcode::ext:

                    switch (RS_IR.ext_page_id)
                    {
                    case 0:
                    {
                        temp_this_command_code_buf.push_back(RS_OPCODE(ext, 00));
                        switch (RS_IR.ext_opcode_p0)
                        {
                        case instruct::extern_opcode_page_0::setref:
                            temp_this_command_code_buf.push_back(RS_OPCODE_EXT0(setref));
                            auto_check_mem_allign(2, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                            auto_check_mem_allign(2, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));
                            break;
                            /*case instruct::extern_opcode_page_0::mknilarr:
                                temp_this_command_code_buf.push_back(RS_OPCODE_EXT0(mknilarr));
                                auto_check_mem_allign(2, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                                break;
                            case instruct::extern_opcode_page_0::mknilmap:
                                temp_this_command_code_buf.push_back(RS_OPCODE_EXT0(mknilmap));
                                auto_check_mem_allign(2, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                                break;*/
                        case instruct::extern_opcode_page_0::packargs:
                            temp_this_command_code_buf.push_back(RS_OPCODE_EXT0(packargs));
                            auto_check_mem_allign(2, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                            auto_check_mem_allign(2, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));
                            break;
                        case instruct::extern_opcode_page_0::unpackargs:
                            temp_this_command_code_buf.push_back(RS_OPCODE_EXT0(unpackargs));
                            auto_check_mem_allign(2, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                            auto_check_mem_allign(2, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));
                            break;
                        case instruct::extern_opcode_page_0::movdup:
                            temp_this_command_code_buf.push_back(RS_OPCODE_EXT0(movdup));
                            auto_check_mem_allign(2, RS_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf));
                            auto_check_mem_allign(2, RS_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf));
                            break;
                        default:
                            rs_error("Unknown instruct.");
                            break;
                        }
                        break;
                    }
                    case 1:
                    {
                        temp_this_command_code_buf.push_back(RS_OPCODE(ext, 01));
                        switch (RS_IR.ext_opcode_p1)
                        {
                        case instruct::extern_opcode_page_1::endjit:
                            temp_this_command_code_buf.push_back(RS_OPCODE_EXT1(endjit));
                            break;
                        default:
                            rs_error("Unknown instruct.");
                            break;
                        }
                        break;
                    }
                    case 2:
                    {
                        temp_this_command_code_buf.push_back(RS_OPCODE(ext, 10));
                        break;
                    }
                    case 3:
                    {
                        temp_this_command_code_buf.push_back(RS_OPCODE(ext, 11));
                        break;
                    }
                    default:
                        rs_error("Unknown extern-opcode-page.");
                        break;
                    }

                    break;
                case instruct::opcode::abrt:
                    if (RS_IR.opinteger)
                        temp_this_command_code_buf.push_back(RS_OPCODE(abrt, 10));
                    else
                        temp_this_command_code_buf.push_back(RS_OPCODE(abrt, 00));
                    break;

                default:
                    rs_error("Unknown instruct.");
                }

                // Move temp_this_command_code_buf data to generated_runtime_code_buf


                auto _4byte_tkplace = need_fill_count / 4;
                auto _nbyte_tkplace = need_fill_count % 4;

                for (size_t ci = 0; ci < _4byte_tkplace; ci++)
                {
                    generated_runtime_code_buf.push_back(RS_OPCODE(nop, 11)); // nop(3) will move 4 byte
                    generated_runtime_code_buf.push_back(RS_OPCODE(nop, 10)); // useless mem fill
                    generated_runtime_code_buf.push_back(RS_OPCODE(nop, 01)); // useless mem fill
                    generated_runtime_code_buf.push_back(RS_OPCODE(nop, 00)); // useless mem fill
                }

                switch (_nbyte_tkplace)
                {
                case 3:
                    generated_runtime_code_buf.push_back(RS_OPCODE(nop, 10));
                    // fallthrow
                case 2:
                    generated_runtime_code_buf.push_back(RS_OPCODE(nop, 01));
                    // fallthrow
                case 1:
                    generated_runtime_code_buf.push_back(RS_OPCODE(nop, 00));
                case 0:
                    break; // do nothing
                }

                pdb_info->pdd_rt_code_byte_offset_to_ir[generated_runtime_code_buf.size()] = ip;

                if (auto fnd = tag_irbuffer_offset.find(ip); fnd != tag_irbuffer_offset.end())
                {
                    for (auto& tag_name : fnd->second)
                    {
                        rs_assert(tag_offset_vector_table.find(tag_name) == tag_offset_vector_table.end()
                            , "The tag point to different place.");

                        tag_offset_vector_table[tag_name] = (uint32_t)generated_runtime_code_buf.size();
                    }
                }


                generated_runtime_code_buf.insert(
                    generated_runtime_code_buf.end(),
                    temp_this_command_code_buf.begin(),
                    temp_this_command_code_buf.end());

            }
#undef RS_OPCODE_2
#undef RS_OPCODE_1
#undef RS_OPCODE
#undef RS_IR
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
                    rs_assert(imm_value->type == value::valuetype::integer_type);
                    imm_value->integer = (rs_integer_t)offset_val;
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

            env->rt_codes = pdb_info->runtime_codes_base = (byte_t*)alloc64(env->rt_code_len * sizeof(byte_t));

            rs_test(reinterpret_cast<size_t>(env->rt_codes) % 8 == 0);

            pdb_info->runtime_codes_length = env->rt_code_len;

            rs_assert(env->rt_codes, "Alloc memory fail.");

            memcpy((byte_t*)env->rt_codes, generated_runtime_code_buf.data(), env->rt_code_len * sizeof(byte_t));

            env->program_debug_info = pdb_info;

            return env;
        }
    };

}