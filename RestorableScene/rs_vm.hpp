#pragma once

#include "rs_basic_type.hpp"
#include "rs_ir_compiler.hpp"

#include <csetjmp>
#include <shared_mutex>
#include <thread>

namespace rs
{
    class exception_recovery;

    struct vmbase
    {
        inline static std::shared_mutex _alive_vm_list_mx;
        inline static cxx_set_t<vmbase*> _alive_vm_list;

        vmbase(const vmbase&) = delete;
        vmbase(vmbase&&) = delete;
        vmbase& operator=(const vmbase&) = delete;
        vmbase& operator=(vmbase&&) = delete;

        enum vm_interrupt_type
        {
            NOTHING = 0,
            GC_INTERRUPT = 1,
        };

        std::atomic<int> vm_interrupt = vm_interrupt_type::NOTHING;

        inline bool interrupt(vm_interrupt_type type)
        {
            return !(type & vm_interrupt.fetch_or(type));
        }

        inline bool clear_interrupt(vm_interrupt_type type)
        {
            return type & vm_interrupt.fetch_and(~type);
        }

        inline bool wait_interrupt(vm_interrupt_type type)
        {
            interrupt(type);
            constexpr int MAX_TRY_COUNT = 1000;
            int i = 0;
            for (i = 0; i < MAX_TRY_COUNT && (vm_interrupt & type); i++)
            {
                std::this_thread::yield();
            }
            if (i >= MAX_TRY_COUNT)
                return false;

            return true;
        }

        std::mutex _vm_hang_mx;
        std::condition_variable _vm_hang_cv;
        std::atomic_int8_t _vm_hang_flag = 0;

        inline void hangup()
        {
            std::unique_lock ug1(_vm_hang_mx);

            ++_vm_hang_flag;
            _vm_hang_cv.wait(ug1, [this]() {return _vm_hang_flag > 0; });
        }

        inline void wakeup()
        {
            --_vm_hang_flag;
            _vm_hang_cv.notify_one();
        }

        vmbase()
        {
            std::lock_guard g1(_alive_vm_list_mx);

            rs_assert(_alive_vm_list.find(this) == _alive_vm_list.end(),
                "This vm is already exists in _alive_vm_list, that is illegal.");

            _alive_vm_list.insert(this);
        }
        ~vmbase()
        {
            std::lock_guard g1(_alive_vm_list_mx);

            rs_assert(_alive_vm_list.find(this) != _alive_vm_list.end(),
                "This vm not exists in _alive_vm_list, that is illegal.");

            _alive_vm_list.erase(this);
        }

        inline thread_local static vmbase* _this_thread_vm;

        // vm exception handler
        exception_recovery* veh = nullptr;

        // next ircode pointer
        byte_t* ip = nullptr;

        // special regist
        value* cr = nullptr;  // op result trace & function return;
        value* tc = nullptr;  // arugument count
        value* er = nullptr;  // exception result

        // stack info
        volatile value* sp = nullptr;
        volatile value* bp = nullptr;

        ir_compiler::runtime_env env;

        void set_runtime(ir_compiler::runtime_env _env)
        {
            env = _env;

            ip = env.rt_codes;
            cr = env.reg_begin + opnum::reg::spreg::cr;
            tc = env.reg_begin + opnum::reg::spreg::tc;
            er = env.reg_begin + opnum::reg::spreg::er;
            sp = bp = env.stack_begin;
        }


    };

    class exception_recovery
    {
        exception_recovery(vmbase* _vm, byte_t* _ip, value* _sp, value* _bp)
            : vm(_vm)
            , ip(_ip)
            , sp(_sp)
            , bp(_bp)
            , last(_vm->veh)
        {
            _vm->veh = this;
        }

        vmbase* vm;

    public:
        byte_t* ip;
        value* sp;
        value* bp;

        exception_recovery* last;
        std::jmp_buf native_env;
        inline static void rollback(vmbase* _vm)
        {
            if (_vm->veh)
                std::longjmp(_vm->veh->native_env, 1);
            else
            {
                rs_error("No 'veh' in this vm.");
            }
        }
        inline static void ok(vmbase* _vm)
        {
            auto veh = _vm->veh;
            _vm->veh = veh->last;
            delete veh;
        }
        inline static void _ready(vmbase* _vm, byte_t* _ip, value* _sp, value* _bp)
        {
            exception_recovery* _er = new exception_recovery(_vm, _ip, _sp, _bp);
        }
    };

#define RS_READY_EXCEPTION_HANDLE(VM,IP,ROLLBACKIP,SP,BP)\
    bool _rs_restore_from_exception = false;\
    exception_recovery::_ready((VM),(ROLLBACKIP),(SP),(BP));\
    if (std::setjmp((VM)->veh->native_env))\
    {\
        _rs_restore_from_exception = true;\
        (IP) = (VM)->veh->ip;\
        (SP) = (VM)->veh->sp;\
        (BP) = (VM)->veh->bp;\
        exception_recovery::ok((VM));\
    }if(_rs_restore_from_exception)

    class vm : public vmbase
    {

    public:

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

            ir_compiler::runtime_env* rt_env = &env;
            byte_t* rt_ip;
            value* rt_bp, * rt_sp;
            value* const_global_begin = rt_env->constant_global_reg_rtstack;
            value* reg_begin = rt_env->reg_begin;

            ip_restore_raii _o1((void*&)rt_ip, (void*&)ip);
            ip_restore_raii _o2((void*&)rt_bp, (void*&)sp);
            ip_restore_raii _o3((void*&)rt_sp, (void*&)bp);

            auto* _nullptr = this;
            ip_restore_raii _o4((void*&)_this_thread_vm, (void*&)_nullptr);
            _nullptr = nullptr;

            rs_assert(rt_env->reg_begin == rt_env->constant_global_reg_rtstack
                + rt_env->constant_value_count
                + rt_env->global_value_count);

            rs_assert(rt_env->stack_begin == rt_env->constant_global_reg_rtstack
                + rt_env->constant_value_count
                + rt_env->global_value_count
                + rt_env->real_register_count
                + (rt_env->runtime_stack_count - 1)
            );

            if (!veh)
            {
                byte_t* _uselessip = nullptr;
                value* _uselesssp = nullptr;
                value* _uselessbp = nullptr;

                RS_READY_EXCEPTION_HANDLE(this, _uselessip, 0, _uselesssp, _uselessbp)
                {
                    // unhandled exception happend.
                    std::cerr << "Unexpected exception: " << rs_cast_string((rs_value)er) << std::endl;

                    return;

                }
            }

            // addressing macro
#define RS_IPVAL (*(rt_ip))
#define RS_IPVAL_MOVE_1 (*(rt_ip++))
#define RS_IPVAL_MOVE_2 (*(uint16_t*)((rt_ip+=2)-2))
#define RS_IPVAL_MOVE_4 (*(uint32_t*)((rt_ip+=4)-4))
#define RS_IPVAL_MOVE_8 (*(uint64_t*)((rt_ip+=8)-8))
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

                    (rt_sp--)->set_val(opnum1);

                    rs_assert(rt_sp <= rt_bp);

                    break;
                }
                case instruct::opcode::pshr:
                {
                    RS_ADDRESSING_N1_REF;

                    (rt_sp--)->set_ref(opnum1);

                    rs_assert(rt_sp <= rt_bp);

                    break;
                }
                case instruct::opcode::pop:
                {
                    if (dr & 0b01)
                    {
                        RS_ADDRESSING_N1_REF;
                        opnum1->set_val((++rt_sp));
                    }
                    else
                        rt_sp += RS_IPVAL_MOVE_2;

                    rs_assert(rt_sp <= rt_bp);

                    break;
                }
                case instruct::opcode::popr:
                {
                    RS_ADDRESSING_N1_REF;
                    opnum1->set_ref((++rt_sp)->get());

                    rs_assert(rt_sp <= rt_bp);

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

                    cr->set_ref((string_t::gc_new<gcbase::gctype::eden>(opnum1->string, *opnum1->string + *opnum2->string), opnum1));
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
                case instruct::opcode::ret:
                {
                    value* stored_bp = rt_env->stack_begin + (++rt_bp)->bp;
                    rt_ip = rt_env->rt_codes + rt_bp->ret_ip;
                    rt_sp = rt_bp;
                    rt_bp = stored_bp;
                    break;
                }
                case instruct::opcode::call:
                {
                    RS_ADDRESSING_N1_REF;

                    if (opnum1->type == value::valuetype::handle_type)
                    {
                        // Call native
                        sp = rt_sp;
                        reinterpret_cast<native_func_t>(opnum1->handle)(reinterpret_cast<rs_vm>(this));
                    }
                    else
                    {
                        rs_assert(opnum1->type == value::valuetype::integer_type);

                        byte_t* aimplace = rt_env->rt_codes + opnum1->integer;

                        rt_sp->type = value::valuetype::callstack;
                        rt_sp->ret_ip = (uint32_t)(rt_ip - rt_env->rt_codes);
                        rt_sp->bp = (uint32_t)(rt_env->stack_begin - rt_bp);
                        rt_bp = --rt_sp;

                        rt_ip = aimplace;

                    }
                    break;
                }
                case instruct::opcode::calln:
                {
                    rs_assert((dr & 0b10) == 0);

                    if (dr)
                    {
                        // Call native
                        sp = rt_sp;
                        reinterpret_cast<native_func_t>(RS_IPVAL_MOVE_8)(reinterpret_cast<rs_vm>(this));
                    }
                    else
                    {
                        byte_t* aimplace = rt_env->rt_codes + RS_IPVAL_MOVE_4;

                        rt_sp->type = value::valuetype::callstack;
                        rt_sp->ret_ip = (uint32_t)(rt_ip - rt_env->rt_codes);
                        rt_sp->bp = (uint32_t)(rt_env->stack_begin - rt_bp);
                        rt_bp = --rt_sp;

                        rt_ip = aimplace;

                    }
                    break;
                }
                case instruct::opcode::jmp:
                    rt_ip = rt_env->rt_codes + RS_IPVAL_MOVE_4;
                    break;
                case instruct::opcode::jt:
                {
                    uint32_t aimplace = RS_IPVAL_MOVE_4;
                    if (cr->get()->integer)
                        rt_ip = rt_env->rt_codes + aimplace;
                    break;
                }
                case instruct::opcode::jf:
                {
                    uint32_t aimplace = RS_IPVAL_MOVE_4;
                    if (!cr->get()->integer)
                        rt_ip = rt_env->rt_codes + aimplace;
                    break;
                }
                case instruct::opcode::veh:
                {
                    if (dr & 0b10)
                    {
                        //begin
                        RS_READY_EXCEPTION_HANDLE(this, rt_ip, rt_env->rt_codes + RS_IPVAL_MOVE_4, rt_sp, rt_bp)
                        {}
                    }
                    else if (dr & 0b01)
                    {
                        // throw
                        rs::exception_recovery::rollback(this);
                    }
                    else
                    {
                        // clean
                        rs::exception_recovery::ok(this);
                        rt_ip = rt_env->rt_codes + RS_IPVAL_MOVE_4;
                    }
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
                default:
                    rs_error("Unknown instruct.");
                }

                if (vm_interrupt)
                {
                    if (vm_interrupt & vm_interrupt_type::GC_INTERRUPT)
                    {
                        // write regist(sp) data, then clear interrupt mark.
                        sp = rt_sp;
                        if (clear_interrupt(vm_interrupt_type::GC_INTERRUPT))
                            hangup();   // SLEEP UNTIL WAKE UP
                    }
                }

            }// vm loop end.
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

#undef RS_READY_EXCEPTION_HANDLE