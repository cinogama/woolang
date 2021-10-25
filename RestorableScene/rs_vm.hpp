#pragma once

#include "rs_basic_type.hpp"
#include "rs_ir_compiler.hpp"

namespace rs
{

    struct vmbase
    {

    };
    class vm
    {
        ir_compiler::runtime_env rt_env;
        byte_t* ip;
    public:

        value* cr;
        value* stacktop;
        value* stackbuttom;

        void set_runtime(ir_compiler::runtime_env _rt_env)
        {
            rt_env = _rt_env;
            ip = rt_env.rt_codes;

            cr = rt_env.reg_begin + opnum::reg::spreg::cr;
            stacktop = stackbuttom = rt_env.stack_begin;
        }

        void run()
        {
            // used for restoring IP
            struct ip_restore_raii
            {
                void*& ot;
                void*& nt;

                ip_restore_raii(void*& _nt, void*& _ot)
                    : ot(_ot)
                    , nt(_nt)
                {
                    _nt = _ot;
                }

                ~ip_restore_raii()
                {
                    ot = nt;
                }
            };

            byte_t* rt_ip;
            value* rt_bp, * rt_sp;
            value* const_global_begin = rt_env.constant_global_reg_rtstack;
            value* reg_begin = rt_env.reg_begin;

            ip_restore_raii _o1((void*&)rt_ip, (void*&)ip);
            ip_restore_raii _o2((void*&)rt_bp, (void*&)stackbuttom);
            ip_restore_raii _o3((void*&)rt_sp, (void*&)stacktop);

            rs_assert(rt_env.reg_begin == rt_env.constant_global_reg_rtstack
                + rt_env.constant_value_count
                + rt_env.global_value_count);

            rs_assert(rt_env.stack_begin == rt_env.constant_global_reg_rtstack
                + rt_env.constant_value_count
                + rt_env.global_value_count
                + rt_env.real_register_count);

            // addressing macro
#define RS_IPVAL (*(rt_ip))
#define RS_IPVAL_MOVE_1 (*(rt_ip++))
#define RS_IPVAL_MOVE_2 (*(uint16_t*)((rt_ip+=2)-2))
#define RS_IPVAL_MOVE_4 (*(uint32_t*)((rt_ip+=4)-4))
#define RS_SIGNED_SHIFT(VAL) (((signed char)((unsigned char)(((unsigned char)(VAL))<<1)))>>1)

#define RS_ADDRESSING_N1 value * opnum1 = ((dr >> 1) ?\
                        (\
                            (RS_IPVAL & (1 << 7)) ?\
                            (rt_bp + RS_SIGNED_SHIFT(RS_IPVAL_MOVE_1))\
                            :\
                            (RS_IPVAL_MOVE_1 + reg_begin)\
                            )\
                        :\
                        (\
                            RS_IPVAL_MOVE_4 + const_global_begin\
                        ))

#define RS_ADDRESSING_N2 value * opnum2 = ((dr & 0b01) ?\
                        (\
                            (RS_IPVAL & (1 << 7)) ?\
                            (rt_bp + RS_SIGNED_SHIFT(RS_IPVAL_MOVE_1))\
                            :\
                            (RS_IPVAL_MOVE_1 + reg_begin)\
                            )\
                        :\
                        (\
                            RS_IPVAL_MOVE_4 + const_global_begin\
                        ))

#define RS_ADDRESSING_N1_REF RS_ADDRESSING_N1 -> get()
#define RS_ADDRESSING_N2_REF RS_ADDRESSING_N2 -> get()

            for (;;)
            {
                byte_t opcode_dr = *(rt_ip++);
                instruct::opcode opcode = (instruct::opcode)(opcode_dr & 0b11111100u);
                unsigned dr = opcode_dr & 0b00000011u;

                switch (opcode)
                {
                case instruct::opcode::psh:
                {
                    RS_ADDRESSING_N1_REF;

                    (rt_sp++)->set_val(opnum1);
                    break;
                }
                case instruct::opcode::pshr:
                {
                    RS_ADDRESSING_N1_REF;

                    (rt_sp++)->set_ref(opnum1);
                    break;
                }
                case instruct::opcode::pop:
                {
                    if (dr & 0b01)
                    {
                        RS_ADDRESSING_N1_REF;
                        opnum1->set_val((--rt_sp));
                    }
                    else
                        rt_sp -= RS_IPVAL_MOVE_2;

                    break;
                }
                case instruct::opcode::popr:
                {
                    RS_ADDRESSING_N1_REF;
                    opnum1->set_ref((--rt_sp)->get());

                    break;
                }

                /// OPERATE
                case instruct::opcode::addi:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    rs_assert(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::integer_type);

                    cr->set_ref((opnum1->integer += opnum2->integer, opnum1));
                    break;
                }
                case instruct::opcode::subi:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    rs_assert(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::integer_type);

                    cr->set_ref((opnum1->integer -= opnum2->integer, opnum1));
                    break;
                }
                case instruct::opcode::muli:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    rs_assert(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::integer_type);

                    cr->set_ref((opnum1->integer *= opnum2->integer, opnum1));
                    break;
                }
                case instruct::opcode::divi:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    rs_assert(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::integer_type);

                    cr->set_ref((opnum1->integer /= opnum2->integer, opnum1));
                    break;
                }
                case instruct::opcode::modi:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    rs_assert(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::integer_type);

                    cr->set_ref((opnum1->integer %= opnum2->integer, opnum1));
                    break;
                }

                case instruct::opcode::addr:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    rs_assert(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::real_type);

                    cr->set_ref((opnum1->real += opnum2->real, opnum1));
                    break;
                }
                case instruct::opcode::subr:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    rs_assert(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::real_type);

                    cr->set_ref((opnum1->real -= opnum2->real, opnum1));
                    break;
                }
                case instruct::opcode::mulr:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    rs_assert(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::real_type);

                    cr->set_ref((opnum1->real *= opnum2->real, opnum1));
                    break;
                }
                case instruct::opcode::divr:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    rs_assert(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::real_type);

                    cr->set_ref((opnum1->real /= opnum2->real, opnum1));
                    break;
                }
                case instruct::opcode::modr:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    rs_assert(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::real_type);

                    cr->set_ref((opnum1->real = fmod(opnum1->real, opnum2->real), opnum1));
                    break;
                }

                case instruct::opcode::addh:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    rs_assert(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::handle_type);

                    cr->set_ref((opnum1->handle += opnum2->handle, opnum1));
                    break;
                }
                case instruct::opcode::subh:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    rs_assert(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::handle_type);

                    cr->set_ref((opnum1->handle -= opnum2->handle, opnum1));
                    break;
                }

                case instruct::opcode::adds:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    rs_assert(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::string_type);

                    cr->set_ref((opnum1->string = string_t::gc_new(*opnum1->string + *opnum2->string), opnum1));
                    break;
                }
                /// OPERATE


                case instruct::opcode::set:
                {
                    RS_ADDRESSING_N1;
                    RS_ADDRESSING_N2_REF;

                    cr->set_ref(opnum1->set_val(opnum2));
                    break;
                }
                case instruct::opcode::mov:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    cr->set_ref(opnum1->set_val(opnum2));
                    break;
                }
                case instruct::opcode::movi2r:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    rs_assert(opnum2->type == value::valuetype::integer_type);

                    opnum1->real = (real_t)opnum2->integer;
                    opnum1->type = value::valuetype::real_type;
                    cr->set_ref(opnum1);

                    break;
                }
                case instruct::opcode::movr2i:
                {

                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    rs_assert(opnum2->type == value::valuetype::real_type);

                    opnum1->integer = (int64_t)opnum2->real;
                    opnum1->type = value::valuetype::integer_type;
                    cr->set_ref(opnum1);

                    break;
                }
                case instruct::opcode::seti2r:
                {
                    RS_ADDRESSING_N1;
                    RS_ADDRESSING_N2_REF;

                    rs_assert(opnum2->type == value::valuetype::integer_type);

                    opnum1->real = (real_t)opnum2->integer;
                    opnum1->type = value::valuetype::real_type;
                    cr->set_ref(opnum1);

                    break;
                }
                case instruct::opcode::setr2i:
                {

                    RS_ADDRESSING_N1;
                    RS_ADDRESSING_N2_REF;

                    rs_assert(opnum2->type == value::valuetype::real_type);

                    opnum1->integer = (int64_t)opnum2->real;
                    opnum1->type = value::valuetype::integer_type;
                    cr->set_ref(opnum1);

                    break;
                }
                case instruct::opcode::lds:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    rs_assert(opnum2->type == value::valuetype::integer_type);
                    opnum1->set_val((rt_bp + opnum2->integer)->get());
                    break;
                }
                case instruct::opcode::ldsr:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    rs_assert(opnum2->type == value::valuetype::integer_type);
                    opnum1->set_ref((rt_bp + opnum2->integer)->get());
                    break;
                }
                case instruct::opcode::equ:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    cr->integer = opnum1->integer == opnum2->integer;
                    cr->type = value::valuetype::integer_type;

                    break;
                }
                case instruct::opcode::nequ:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    cr->integer = opnum1->integer != opnum2->integer;
                    cr->type = value::valuetype::integer_type;

                    break;
                }
                case instruct::opcode::equs:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    cr->integer =
                        opnum1->type == value::valuetype::string_type
                        && opnum1->type == opnum2->type
                        && *opnum1->string == *opnum2->string;

                    cr->type = value::valuetype::integer_type;

                    break;
                }
                case instruct::opcode::nequs:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    cr->integer =
                        opnum1->type != value::valuetype::string_type
                        || opnum1->type != opnum2->type
                        || *opnum1->string != *opnum2->string;

                    cr->type = value::valuetype::integer_type;

                    break;
                }

                case instruct::opcode::land:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    cr->integer = opnum1->integer && opnum2->integer;
                    cr->type = value::valuetype::integer_type;

                    break;
                }
                case instruct::opcode::lor:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    cr->integer = opnum1->integer || opnum2->integer;
                    cr->type = value::valuetype::integer_type;

                    break;
                }
                case instruct::opcode::lnot:
                {
                    RS_ADDRESSING_N1_REF;

                    cr->integer = !opnum1->integer;
                    cr->type = value::valuetype::integer_type;

                    break;
                }

                case instruct::opcode::lti:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    rs_assert(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::integer_type);

                    cr->integer = opnum1->integer < opnum2->integer;
                    cr->type = value::valuetype::integer_type;

                    break;
                }
                case instruct::opcode::gti:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    rs_assert(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::integer_type);

                    cr->integer = opnum1->integer > opnum2->integer;
                    cr->type = value::valuetype::integer_type;

                    break;
                }
                case instruct::opcode::elti:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    rs_assert(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::integer_type);

                    cr->integer = opnum1->integer <= opnum2->integer;
                    cr->type = value::valuetype::integer_type;

                    break;
                }
                case instruct::opcode::egti:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    rs_assert(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::integer_type);

                    cr->integer = opnum1->integer >= opnum2->integer;
                    cr->type = value::valuetype::integer_type;

                    break;
                }

                case instruct::opcode::ltr:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    rs_assert(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::real_type);

                    cr->integer = opnum1->real < opnum2->real;
                    cr->type = value::valuetype::integer_type;

                    break;
                }
                case instruct::opcode::gtr:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    rs_assert(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::real_type);

                    cr->integer = opnum1->real > opnum2->real;
                    cr->type = value::valuetype::integer_type;

                    break;
                }
                case instruct::opcode::eltr:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    rs_assert(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::real_type);

                    cr->integer = opnum1->real <= opnum2->real;
                    cr->type = value::valuetype::integer_type;

                    break;
                }
                case instruct::opcode::egtr:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    rs_assert(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::real_type);

                    cr->integer = opnum1->real >= opnum2->real;
                    cr->type = value::valuetype::integer_type;

                    break;
                }

                case instruct::opcode::lth:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    rs_assert(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::handle_type);

                    cr->integer = opnum1->handle < opnum2->handle;
                    cr->type = value::valuetype::integer_type;

                    break;
                }
                case instruct::opcode::gth:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    rs_assert(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::handle_type);

                    cr->integer = opnum1->handle > opnum2->handle;
                    cr->type = value::valuetype::integer_type;

                    break;
                }
                case instruct::opcode::elth:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    rs_assert(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::handle_type);

                    cr->integer = opnum1->handle <= opnum2->handle;
                    cr->type = value::valuetype::integer_type;

                    break;
                }
                case instruct::opcode::egth:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    rs_assert(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::handle_type);

                    cr->integer = opnum1->handle >= opnum2->handle;
                    cr->type = value::valuetype::integer_type;

                    break;
                }

                case instruct::opcode::jmp:
                    rt_ip = rt_env.rt_codes + RS_IPVAL_MOVE_4;
                    break;
                case instruct::opcode::jt:
                {
                    uint32_t aimplace = RS_IPVAL_MOVE_4;
                    if (cr->get()->integer)
                        rt_ip = rt_env.rt_codes + aimplace;
                    break;
                }
                case instruct::opcode::jf:
                {
                    uint32_t aimplace = RS_IPVAL_MOVE_4;
                    if (!cr->get()->integer)
                        rt_ip = rt_env.rt_codes + aimplace;
                    break;
                }

                case instruct::opcode::end:
                {
                    return;
                }
                case instruct::opcode::nop:
                {
                    break;
                }
                case instruct::opcode::abrt:
                    rs_error("executed 'abrt'.");
                    break;
                default:
                    rs_error("Unknown instruct.");

                }
            }
#undef RS_ADDRESSING_N2_REF
#undef RS_ADDRESSING_N1_REF
#undef RS_ADDRESSING_N2
#undef RS_ADDRESSING_N1
#undef RS_SIGNED_SHIFT
#undef RS_IPVAL_MOVE_4
#undef RS_IPVAL_MOVE_1
#undef RS_IPVAL

        }
    };
}