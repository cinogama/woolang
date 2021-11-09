#pragma once

#include "rs_assert.hpp"
#include "rs_basic_type.hpp"
#include "rs_instruct.hpp"
#include "rs_meta.hpp"

#include <cstring>

namespace rs
{
    namespace opnum
    {
        struct opnumbase
        {
            virtual ~opnumbase() = default;
            virtual void generate_opnum_to_buffer(cxx_vec_t<byte_t>& buffer) const
            {
                rs_error("This type can not generate opnum.");
            }
        };

        struct stack : opnumbase
        {
            int16_t offset;

            stack(int16_t _offset) noexcept
                :offset(_offset)
            {

            }
        };

        struct global : opnumbase
        {
            int32_t offset;

            global(int32_t _offset) noexcept
                :offset(_offset)
            {

            }
        };

        struct reg :opnumbase
        {
            // REGID
            // 0B 1 1 000000
            //  isreg? stackbp? offset

            uint8_t id;

            enum spreg : uint8_t
            {
                // normal regist
                t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14, t15,
                r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, r13, r14, r15,

                // special regist
                op_trace_result = 0b00100000, cr = op_trace_result,
                argument_count, tc = argument_count,
                exception_result, er = exception_result,

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

            void generate_opnum_to_buffer(cxx_vec_t<byte_t>& buffer) const override
            {
                buffer.push_back(id);
            }
        };

        struct immbase : opnumbase
        {
            int32_t constant_index = 0;

            virtual value::valuetype type()const = 0;
            virtual bool operator < (const immbase&) const = 0;
            virtual void apply(value*) const = 0;

            void generate_opnum_to_buffer(cxx_vec_t<byte_t>& buffer) const override
            {
                byte_t* buf = (byte_t*)&constant_index;

                buffer.push_back(buf[0]);
                buffer.push_back(buf[1]);
                buffer.push_back(buf[2]);
                buffer.push_back(buf[3]);
            }

            virtual int64_t try_int()const = 0;
        };

        template<typename T>
        struct imm : immbase
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
                auto t = type();
                auto t2 = _another.type();
                if (t == t2)
                {
                    return val < ((const imm&)_another).val;
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
            }
        };

        struct tag : opnumbase
        {
            string_t name;

            tag(const string_t& _name)
                : name(_name)
            {

            }
        };
    } // namespace opnum;

    struct vmbase;

    class ir_compiler
    {
        // IR COMPILER:
        /*
        *
        */

        friend struct vmbase;

        struct ir_command
        {
            instruct::opcode opcode;

            opnum::opnumbase* op1;
            opnum::opnumbase* op2;

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
        std::map<size_t, cxx_vec_t<string_t>> tag_irbuffer_offset;

        struct immless
        {
            bool operator()(const opnum::immbase* lhs, const opnum::immbase* rhs) const
            {
                return (*lhs) < (*rhs);
            }
        };
        cxx_set_t<opnum::immbase*, immless>constant_record_list;

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

            return _opnum;
        }

    public:

#define RS_PUT_IR_TO_BUFFER(OPCODE, ...) ir_command_buffer.emplace_back(ir_command{OPCODE, __VA_ARGS__});
#define RS_OPNUM(OPNUM) (_check_and_add_const(new meta::origin_type<decltype(OPNUM)>(OPNUM)))

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
                rs_assert(0 < op1 && op1 <= UINT16_MAX);
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

            RS_PUT_IR_TO_BUFFER(instruct::opcode::addx, RS_OPNUM(op1), RS_OPNUM(op2));
        }

        template<typename OP1T, typename OP2T>
        void subx(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not sub value to immediate.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::subx, RS_OPNUM(op1), RS_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void mulx(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not mul value to immediate.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::mulx, RS_OPNUM(op1), RS_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void divx(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not div value to immediate.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::divx, RS_OPNUM(op1), RS_OPNUM(op2));
        }
        template<typename OP1T, typename OP2T>
        void modx(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not mod value to immediate.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::modx, RS_OPNUM(op1), RS_OPNUM(op2));
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
        void movr2i(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not move value to immediate.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::movr2i, RS_OPNUM(op1), RS_OPNUM(op2));
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
        void movi2r(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not move value to immediate.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::movi2r, RS_OPNUM(op1), RS_OPNUM(op2));
        }

        template<typename OP1T, typename OP2T>
        void setr2i(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not set value to immediate.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::setr2i, RS_OPNUM(op1), RS_OPNUM(op2));
        }

        template<typename OP1T, typename OP2T>
        void seti2r(const OP1T& op1, const OP2T& op2)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value
                && std::is_base_of<opnum::opnumbase, OP2T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not set value to immediate.");

            RS_PUT_IR_TO_BUFFER(instruct::opcode::seti2r, RS_OPNUM(op1), RS_OPNUM(op2));
        }

        template<typename OP1T>
        void call(const OP1T& op1)
        {
            if constexpr (std::is_base_of<opnum::tag, OP1T>::value)
            {
                RS_PUT_IR_TO_BUFFER(instruct::opcode::calln, nullptr, RS_OPNUM(op1));
            }
            else if constexpr (std::is_base_of<opnum::opnumbase, OP1T>::value)
            {
                RS_PUT_IR_TO_BUFFER(instruct::opcode::call, RS_OPNUM(op1));
            }
            else if constexpr (std::is_pointer<OP1T>::value)
            {
                RS_PUT_IR_TO_BUFFER(instruct::opcode::calln, reinterpret_cast<opnum::opnumbase*>(op1));
            }
            else if constexpr (std::is_integral<OP1T>::value)
            {
                rs_assert(0 <= op1 && op1 <= UINT32_MAX, "Immediate instruct address is to large to call.");

                uint32_t address = (uint32_t)op1;
                RS_PUT_IR_TO_BUFFER(instruct::opcode::calln, nullptr, nullptr, reinterpret_cast<int32_t>(op1));
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
            RS_PUT_IR_TO_BUFFER(instruct::opcode::end);
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

        template<typename OP1T>
        void ext_mknilmap(const OP1T& op1)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not set immediate as ref.");

            auto& codeb = RS_PUT_IR_TO_BUFFER(instruct::opcode::ext, RS_OPNUM(op1));
            codeb.ext_page_id = 0;
            codeb.ext_opcode_p0 = instruct::extern_opcode_page_0::mknilmap;
        }

        template<typename OP1T>
        void ext_mknilarr(const OP1T& op1)
        {
            static_assert(std::is_base_of<opnum::opnumbase, OP1T>::value,
                "Argument(s) should be opnum.");

            static_assert(!std::is_base_of<opnum::immbase, OP1T>::value,
                "Can not set immediate as ref.");

            auto& codeb = RS_PUT_IR_TO_BUFFER(instruct::opcode::ext, RS_OPNUM(op1));
            codeb.ext_page_id = 0;
            codeb.ext_opcode_p0 = instruct::extern_opcode_page_0::mknilarr;
        }

#undef RS_OPNUM
#undef RS_PUT_IR_TO_BUFFER

        struct runtime_env
        {
            value* constant_global_reg_rtstack = nullptr;
            value* reg_begin = nullptr;
            value* stack_begin = nullptr;

            size_t constant_value_count = 0;
            size_t global_value_count = 0;
            size_t real_register_count = 0;
            size_t cgr_global_value_count = 0;

            size_t runtime_stack_count = 0;

            byte_t* rt_codes = nullptr;

            ~runtime_env()
            {
                if (constant_global_reg_rtstack)
                    free(constant_global_reg_rtstack);

                if (rt_codes)
                    free(rt_codes);
            }
        };

    private:
        std::unique_ptr<runtime_env> finalize()
        {
            // 1. Generate constant & global & register & runtime_stack memory buffer
            size_t constant_value_count = constant_record_list.size();
            size_t global_value_count = 0;
            size_t real_register_count = 64;     // t0-t15 r0-r15 (32) special reg (32)
            size_t runtime_stack_count = 65536;  // by default

            size_t preserve_memory_size =
                constant_value_count
                + global_value_count
                + real_register_count
                + runtime_stack_count;

            const auto v = sizeof(value);

            value* preserved_memory = (value*)malloc(preserve_memory_size * sizeof(value));

            rs_assert(preserved_memory, "Alloc memory fail.");

            memset(preserved_memory, 0, preserve_memory_size * sizeof(value));

            //  // Fill constant
            for (auto& constant_record : constant_record_list)
            {
                rs_assert(constant_record->constant_index < constant_value_count,
                    "Constant index out of range.");

                constant_record->apply(&preserved_memory[constant_record->constant_index]);
            }
            // 2. Generate code
            cxx_vec_t<byte_t> runtime_command_buffer;
            std::map<string_t, cxx_vec_t<size_t>> jmp_record_table;
            std::map<string_t, uint32_t> tag_offset_vector_table;

            for (size_t ip = 0; ip < ir_command_buffer.size(); ip++)
            {
                if (auto fnd = tag_irbuffer_offset.find(ip); fnd != tag_irbuffer_offset.end())
                {
                    for (auto& tag_name : fnd->second)
                    {
                        rs_assert(tag_offset_vector_table.find(tag_name) == tag_offset_vector_table.end()
                            , "The tag point to different place.");

                        tag_offset_vector_table[tag_name] = (uint32_t)runtime_command_buffer.size();
                    }
                }

#define RS_IR ir_command_buffer[ip]
#define RS_OPCODE(...) rs_macro_overload(RS_OPCODE,__VA_ARGS__)
#define RS_OPCODE_1(OPCODE) (instruct(instruct::opcode::OPCODE, RS_IR.dr()).opcode_dr)
#define RS_OPCODE_2(OPCODE,DR) (instruct(instruct::opcode::OPCODE, 0b000000##DR).opcode_dr)

#define RS_OPCODE_EXT0(...) rs_macro_overload(RS_OPCODE_EXT0,__VA_ARGS__)
#define RS_OPCODE_EXT0_1(OPCODE) (instruct((instruct::opcode)instruct::extern_opcode_page_0::OPCODE, RS_IR.dr()).opcode_dr)
#define RS_OPCODE_EXT0_2(OPCODE,DR) (instruct((instruct::opcode)instruct::extern_opcode_page_0::OPCODE, 0b000000##DR).opcode_dr)

                switch (RS_IR.opcode)
                {
                case instruct::opcode::nop:
                    runtime_command_buffer.push_back(RS_OPCODE(nop));
                    break;
                case instruct::opcode::set:
                    runtime_command_buffer.push_back(RS_OPCODE(set));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::mov:
                    runtime_command_buffer.push_back(RS_OPCODE(mov));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;

                case instruct::opcode::movi2r:
                    runtime_command_buffer.push_back(RS_OPCODE(movi2r));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::movr2i:
                    runtime_command_buffer.push_back(RS_OPCODE(movr2i));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::seti2r:
                    runtime_command_buffer.push_back(RS_OPCODE(seti2r));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::setr2i:
                    runtime_command_buffer.push_back(RS_OPCODE(setr2i));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::psh:
                    runtime_command_buffer.push_back(RS_OPCODE(psh));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::pshr:
                    runtime_command_buffer.push_back(RS_OPCODE(pshr));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::pop:
                    if (nullptr == RS_IR.op1)
                    {
                        runtime_command_buffer.push_back(RS_OPCODE(pop, 00));

                        rs_assert(RS_IR.opinteger > 0 && RS_IR.opinteger <= UINT16_MAX
                            , "Invalid count to pop from stack.");

                        uint16_t opushort = (uint16_t)RS_IR.opinteger;
                        byte_t* readptr = (byte_t*)&opushort;

                        runtime_command_buffer.push_back(readptr[0]);
                        runtime_command_buffer.push_back(readptr[1]);
                    }
                    else
                    {
                        runtime_command_buffer.push_back(RS_OPCODE(pop) | 0b01);
                        RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    }
                    break;
                case instruct::opcode::popr:
                    runtime_command_buffer.push_back(RS_OPCODE(popr));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::lds:
                    runtime_command_buffer.push_back(RS_OPCODE(lds));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::ldsr:
                    runtime_command_buffer.push_back(RS_OPCODE(ldsr));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::addi:
                    runtime_command_buffer.push_back(RS_OPCODE(addi));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::subi:
                    runtime_command_buffer.push_back(RS_OPCODE(subi));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::muli:
                    runtime_command_buffer.push_back(RS_OPCODE(muli));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::divi:
                    runtime_command_buffer.push_back(RS_OPCODE(divi));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::modi:
                    runtime_command_buffer.push_back(RS_OPCODE(modi));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::movx:
                    runtime_command_buffer.push_back(RS_OPCODE(movx));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::addx:
                    runtime_command_buffer.push_back(RS_OPCODE(addx));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::subx:
                    runtime_command_buffer.push_back(RS_OPCODE(subx));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::mulx:
                    runtime_command_buffer.push_back(RS_OPCODE(mulx));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::divx:
                    runtime_command_buffer.push_back(RS_OPCODE(divx));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::modx:
                    runtime_command_buffer.push_back(RS_OPCODE(modx));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::addr:
                    runtime_command_buffer.push_back(RS_OPCODE(addr));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::subr:
                    runtime_command_buffer.push_back(RS_OPCODE(subr));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::mulr:
                    runtime_command_buffer.push_back(RS_OPCODE(mulr));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::divr:
                    runtime_command_buffer.push_back(RS_OPCODE(divr));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::modr:
                    runtime_command_buffer.push_back(RS_OPCODE(modr));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::addh:
                    runtime_command_buffer.push_back(RS_OPCODE(addh));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::subh:
                    runtime_command_buffer.push_back(RS_OPCODE(subh));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::adds:
                    runtime_command_buffer.push_back(RS_OPCODE(adds));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;

                case instruct::opcode::equb:
                    runtime_command_buffer.push_back(RS_OPCODE(equb));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::nequb:
                    runtime_command_buffer.push_back(RS_OPCODE(nequb));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::equx:
                    runtime_command_buffer.push_back(RS_OPCODE(equx));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::nequx:
                    runtime_command_buffer.push_back(RS_OPCODE(nequx));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;

                case instruct::opcode::land:
                    runtime_command_buffer.push_back(RS_OPCODE(land));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::lor:
                    runtime_command_buffer.push_back(RS_OPCODE(lor));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::lnot:
                    runtime_command_buffer.push_back(RS_OPCODE(lnot));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    break;

                case instruct::opcode::gti:
                    runtime_command_buffer.push_back(RS_OPCODE(gti));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::lti:
                    runtime_command_buffer.push_back(RS_OPCODE(lti));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::egti:
                    runtime_command_buffer.push_back(RS_OPCODE(egti));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::elti:
                    runtime_command_buffer.push_back(RS_OPCODE(elti));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;

                case instruct::opcode::gtr:
                    runtime_command_buffer.push_back(RS_OPCODE(gtr));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::ltr:
                    runtime_command_buffer.push_back(RS_OPCODE(ltr));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::egtr:
                    runtime_command_buffer.push_back(RS_OPCODE(egtr));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::eltr:
                    runtime_command_buffer.push_back(RS_OPCODE(eltr));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;

                case instruct::opcode::gtx:
                    runtime_command_buffer.push_back(RS_OPCODE(gtx));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::ltx:
                    runtime_command_buffer.push_back(RS_OPCODE(ltx));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::egtx:
                    runtime_command_buffer.push_back(RS_OPCODE(egtx));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::eltx:
                    runtime_command_buffer.push_back(RS_OPCODE(eltx));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::mkarr:
                    runtime_command_buffer.push_back(RS_OPCODE(mkarr));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::mkmap:
                    runtime_command_buffer.push_back(RS_OPCODE(mkmap));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::idx:
                    runtime_command_buffer.push_back(RS_OPCODE(idx));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                    break;

                case instruct::opcode::jt:
                    runtime_command_buffer.push_back(RS_OPCODE(jt));

                    rs_assert(dynamic_cast<opnum::tag*>(RS_IR.op1) != nullptr, "Operator num should be a tag.");

                    jmp_record_table[dynamic_cast<opnum::tag*>(RS_IR.op1)->name]
                        .push_back(runtime_command_buffer.size());

                    runtime_command_buffer.push_back(0x00);
                    runtime_command_buffer.push_back(0x00);
                    runtime_command_buffer.push_back(0x00);
                    runtime_command_buffer.push_back(0x00);
                    break;
                case instruct::opcode::jf:
                    runtime_command_buffer.push_back(RS_OPCODE(jf));

                    rs_assert(dynamic_cast<opnum::tag*>(RS_IR.op1) != nullptr, "Operator num should be a tag.");

                    jmp_record_table[dynamic_cast<opnum::tag*>(RS_IR.op1)->name]
                        .push_back(runtime_command_buffer.size());

                    runtime_command_buffer.push_back(0x00);
                    runtime_command_buffer.push_back(0x00);
                    runtime_command_buffer.push_back(0x00);
                    runtime_command_buffer.push_back(0x00);
                    break;

                case instruct::opcode::jmp:
                    runtime_command_buffer.push_back(RS_OPCODE(jmp, 00));

                    rs_assert(dynamic_cast<opnum::tag*>(RS_IR.op1) != nullptr, "Operator num should be a tag.");

                    jmp_record_table[dynamic_cast<opnum::tag*>(RS_IR.op1)->name]
                        .push_back(runtime_command_buffer.size());

                    runtime_command_buffer.push_back(0x00);
                    runtime_command_buffer.push_back(0x00);
                    runtime_command_buffer.push_back(0x00);
                    runtime_command_buffer.push_back(0x00);
                    break;

                case instruct::opcode::call:
                    runtime_command_buffer.push_back(RS_OPCODE(call));
                    RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                    break;
                case instruct::opcode::calln:
                    if (RS_IR.op2)
                    {
                        rs_assert(dynamic_cast<opnum::tag*>(RS_IR.op2) != nullptr, "Operator num should be a tag.");

                        runtime_command_buffer.push_back(RS_OPCODE(calln, 00));

                        jmp_record_table[dynamic_cast<opnum::tag*>(RS_IR.op2)->name]
                            .push_back(runtime_command_buffer.size());

                        runtime_command_buffer.push_back(0x00);
                        runtime_command_buffer.push_back(0x00);
                        runtime_command_buffer.push_back(0x00);
                        runtime_command_buffer.push_back(0x00);
                    }
                    else if (RS_IR.op1)
                    {
                        runtime_command_buffer.push_back(RS_OPCODE(calln, 01));

                        uint64_t addr = (uint64_t)(RS_IR.op1);

                        byte_t* readptr = (byte_t*)&addr;
                        runtime_command_buffer.push_back(readptr[0]);
                        runtime_command_buffer.push_back(readptr[1]);
                        runtime_command_buffer.push_back(readptr[2]);
                        runtime_command_buffer.push_back(readptr[3]);
                        runtime_command_buffer.push_back(readptr[4]);
                        runtime_command_buffer.push_back(readptr[5]);
                        runtime_command_buffer.push_back(readptr[6]);
                        runtime_command_buffer.push_back(readptr[7]);
                    }
                    else
                    {
                        runtime_command_buffer.push_back(RS_OPCODE(calln, 00));
                        byte_t* readptr = (byte_t*)&RS_IR.opinteger;
                        runtime_command_buffer.push_back(readptr[0]);
                        runtime_command_buffer.push_back(readptr[1]);
                        runtime_command_buffer.push_back(readptr[2]);
                        runtime_command_buffer.push_back(readptr[3]);
                    }
                    break;
                case instruct::opcode::ret:
                    runtime_command_buffer.push_back(RS_OPCODE(ret, 00));
                    break;
                case instruct::opcode::veh:
                    if (RS_IR.op1)
                    {
                        // begin
                        rs_assert(dynamic_cast<opnum::tag*>(RS_IR.op1) != nullptr, "Operator num should be a tag.");

                        runtime_command_buffer.push_back(RS_OPCODE(veh, 10));
                        jmp_record_table[dynamic_cast<opnum::tag*>(RS_IR.op1)->name]
                            .push_back(runtime_command_buffer.size());
                        runtime_command_buffer.push_back(0x00);
                        runtime_command_buffer.push_back(0x00);
                        runtime_command_buffer.push_back(0x00);
                        runtime_command_buffer.push_back(0x00);
                    }
                    else if (RS_IR.op2)
                    {
                        // clean
                        rs_assert(dynamic_cast<opnum::tag*>(RS_IR.op2) != nullptr, "Operator num should be a tag.");

                        runtime_command_buffer.push_back(RS_OPCODE(veh, 00));
                        jmp_record_table[dynamic_cast<opnum::tag*>(RS_IR.op2)->name]
                            .push_back(runtime_command_buffer.size());
                        runtime_command_buffer.push_back(0x00);
                        runtime_command_buffer.push_back(0x00);
                        runtime_command_buffer.push_back(0x00);
                        runtime_command_buffer.push_back(0x00);
                    }
                    else
                    {
                        // throw
                        runtime_command_buffer.push_back(RS_OPCODE(veh, 01));
                    }
                    break;
                case instruct::opcode::ext:

                    switch (RS_IR.ext_page_id)
                    {
                    case 0:
                    {
                        runtime_command_buffer.push_back(RS_OPCODE(ext, 00));
                        switch (RS_IR.ext_opcode_p0)
                        {
                        case instruct::extern_opcode_page_0::setref:
                            runtime_command_buffer.push_back(RS_OPCODE_EXT0(setref));
                            RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                            RS_IR.op2->generate_opnum_to_buffer(runtime_command_buffer);
                            break;
                        case instruct::extern_opcode_page_0::mknilarr:
                            runtime_command_buffer.push_back(RS_OPCODE_EXT0(mknilarr));
                            RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                            break;
                        case instruct::extern_opcode_page_0::mknilmap:
                            runtime_command_buffer.push_back(RS_OPCODE_EXT0(mknilmap));
                            RS_IR.op1->generate_opnum_to_buffer(runtime_command_buffer);
                            break;
                        default:
                            rs_error("Unknown instruct.");
                            break;
                        }
                        break;
                    }
                    case 1:
                    {
                        runtime_command_buffer.push_back(RS_OPCODE(ext, 01));
                        break;
                    }
                    case 2:
                    {
                        runtime_command_buffer.push_back(RS_OPCODE(ext, 10));
                        break;
                    }
                    case 3:
                    {
                        runtime_command_buffer.push_back(RS_OPCODE(ext, 11));
                        break;
                    }
                    default:
                        rs_error("Unknown extern-opcode-page.");
                        break;
                    }

                    break;
                case instruct::opcode::abrt:
                    runtime_command_buffer.push_back(RS_OPCODE(abrt, 00));
                    break;

                case instruct::opcode::end:
                    runtime_command_buffer.push_back(RS_OPCODE(end, 00));
                    break;

                default:
                    rs_error("Unknown instruct.");
                }
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
                    *(uint32_t*)(runtime_command_buffer.data() + offset) = offset_val;
                }
            }

            std::unique_ptr<runtime_env> env = std::make_unique<runtime_env>();

            env->constant_global_reg_rtstack = preserved_memory;
            env->reg_begin = env->constant_global_reg_rtstack + constant_value_count + global_value_count;
            env->stack_begin = env->constant_global_reg_rtstack + (preserve_memory_size - 1);

            env->constant_value_count = constant_value_count;
            env->global_value_count = global_value_count;
            env->real_register_count = real_register_count;
            env->cgr_global_value_count = env->constant_value_count
                + env->global_value_count
                + env->real_register_count;

            env->runtime_stack_count = runtime_stack_count;

            env->rt_codes = (byte_t*)malloc(runtime_command_buffer.size() * sizeof(byte_t));
            rs_assert(env->rt_codes, "Alloc memory fail.");

            memcpy(env->rt_codes, runtime_command_buffer.data(), runtime_command_buffer.size() * sizeof(byte_t));

            return env;
        }
    };
}