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
            NOTHING = 0,                // There is no interrupt

            GC_INTERRUPT = 1,           // GC work will cause this interrupt, if vm received this interrupt,
                                        // should clean this interrupt flag, if clean-operate is successful,
                                        // vm should call 'hangup' to wait for GC work. 
                                        // GC work will cancel GC_INTERRUPT after collect_stage_1. if cancel
                                        // failed, it means vm already hangned(or trying hangs now), GC work
                                        // will call 'wakeup' to resume vm.

                                        LEAVE_INTERRUPT = 1 << 1,   // When GC work trying GC_INTERRUPT, it will wait for vm cleaning 
                                                                    // GC_INTERRUPT flag(and hangs), and the wait will be endless, besides:
                                                                    // If LEAVE_INTERRUPT was setted, 'wait_interrupt' will try to wait in
                                                                    // a limitted time.
                                                                    // VM will set LEAVE_INTERRUPT when:
                                                                    // 1) calling native function
                                                                    // 2) leaving vm run()
                                                                    // 3) vm was created.
                                                                    // VM will clean LEAVE_INTERRUPT when:
                                                                    // 1) the native function calling was end.
                                                                    // 2) enter vm run()
                                                                    // 3) vm destructed.
                                                                    // ATTENTION: Each operate of setting or cleaning LEAVE_INTERRUPT must be
                                                                    //            successful. (We use 'rs_asure' here)
        };

        std::atomic<int> vm_interrupt = vm_interrupt_type::NOTHING;

        inline bool interrupt(vm_interrupt_type type)
        {
            bool result;
            rs_test(result = !(type & vm_interrupt.fetch_or(type)));

            return result;
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
            while (vm_interrupt & type)
            {
                if (vm_interrupt & vm_interrupt_type::LEAVE_INTERRUPT)
                {
                    if (++i > MAX_TRY_COUNT)
                        return false;
                }
                else
                    i = 0;

                std::this_thread::yield();
            }
            return true;
        }

        std::mutex _vm_hang_mx;
        std::condition_variable _vm_hang_cv;
        std::atomic_int8_t _vm_hang_flag = 0;

        inline void hangup()
        {
            do
            {
                std::lock_guard g1(_vm_hang_mx);
                --_vm_hang_flag;
            } while (0);

            std::unique_lock ug1(_vm_hang_mx);
            _vm_hang_cv.wait(ug1, [this]() {return _vm_hang_flag >= 0; });
        }

        inline void wakeup()
        {
            do
            {
                std::lock_guard g1(_vm_hang_mx);
                ++_vm_hang_flag;
            } while (0);

            _vm_hang_cv.notify_one();
        }

        vmbase()
        {
            interrupt(vm_interrupt_type::LEAVE_INTERRUPT);

            std::lock_guard g1(_alive_vm_list_mx);

            rs_assert(_alive_vm_list.find(this) == _alive_vm_list.end(),
                "This vm is already exists in _alive_vm_list, that is illegal.");

            _alive_vm_list.insert(this);
        }
        ~vmbase()
        {
            rs_test(clear_interrupt(vm_interrupt_type::LEAVE_INTERRUPT));

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

        std::unique_ptr<ir_compiler::runtime_env> env;

        void set_runtime(ir_compiler& _compiler)
        {
            // using LEAVE_INTERRUPT to stop GC
            rs_asure(clear_interrupt(LEAVE_INTERRUPT));

            env = _compiler.finalize();

            ip = env->rt_codes;
            cr = env->reg_begin + opnum::reg::spreg::cr;
            tc = env->reg_begin + opnum::reg::spreg::tc;
            er = env->reg_begin + opnum::reg::spreg::er;
            sp = bp = env->stack_begin;

            rs_asure(interrupt(LEAVE_INTERRUPT));
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

        void run() try
        {
            struct auto_leave
            {
                vmbase* vm;
                auto_leave(vmbase* _vm)
                    :vm(_vm)
                {
                    rs_asure(vm->clear_interrupt(vm_interrupt_type::LEAVE_INTERRUPT));
                }
                ~auto_leave()
                {
                    rs_asure(vm->interrupt(vm_interrupt_type::LEAVE_INTERRUPT));
                }
            };
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

            ir_compiler::runtime_env* rt_env = env.get();
            byte_t* rt_ip;
            value* rt_bp, * rt_sp;
            value* const_global_begin = rt_env->constant_global_reg_rtstack;
            value* reg_begin = rt_env->reg_begin;

            auto_leave      _o0(this);
            ip_restore_raii _o1((void*&)rt_ip, (void*&)ip);
            ip_restore_raii _o2((void*&)rt_bp, (void*&)sp);
            ip_restore_raii _o3((void*&)rt_sp, (void*&)bp);

            vmbase* last_this_thread_vm = _this_thread_vm;
            vmbase* _nullptr = this;
            ip_restore_raii _o4((void*&)_this_thread_vm, (void*&)_nullptr);
            _nullptr = last_this_thread_vm;

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

            byte_t opcode_dr = (byte_t)(instruct::abrt << 2);
            instruct::opcode opcode = (instruct::opcode)(opcode_dr & 0b11111100u);
            unsigned dr = opcode_dr & 0b00000011u;

            goto _vm_check_interrupt_first;

            for (;;)
            {
                opcode_dr = *(rt_ip++);
                opcode = (instruct::opcode)(opcode_dr & 0b11111100u);
                dr = opcode_dr & 0b00000011u;

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

                    cr->set_ref((string_t::gc_new<gcbase::gctype::eden>(opnum1->gcunit, *opnum1->string + *opnum2->string), opnum1));
                    break;
                }

                case instruct::opcode::addx:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    value::valuetype max_type = std::max(opnum1->type, opnum2->type);

                    if (opnum1->type != max_type)
                    {
                        opnum1->type = max_type;
                        switch (max_type)
                        {
                        case rs::value::valuetype::integer_type:
                            switch (opnum1->type)
                            {
                            case value::valuetype::real_type:
                                opnum1->integer = (rs_integer_t)opnum1->real; break;
                            case value::valuetype::handle_type:
                                opnum1->integer = (rs_integer_t)opnum1->handle; break;
                            default:
                                rs_fail("Mismatch type for operating."); break;
                            }
                            break;
                        case rs::value::valuetype::real_type:
                            switch (opnum1->type)
                            {
                            case value::valuetype::integer_type:
                                opnum1->real = (rs_real_t)opnum1->integer; break;
                            case value::valuetype::handle_type:
                                opnum1->real = (rs_real_t)opnum1->handle; break;
                            default:
                                rs_fail("Mismatch type for operating."); break;
                            }
                            break;
                        case rs::value::valuetype::handle_type:
                            switch (opnum1->type)
                            {
                            case value::valuetype::integer_type:
                                opnum1->handle = (rs_handle_t)opnum1->integer; break;
                            case value::valuetype::real_type:
                                opnum1->handle = (rs_handle_t)opnum1->real; break;
                            default:
                                rs_fail("Mismatch type for operating."); break;
                            }
                            break;
                        default:
                            rs_fail("Mismatch type for operating."); break;
                        }
                    }

                    ///////////////////////////////////////////////

                    if (opnum2->type == max_type)
                    {
                        switch (opnum2->type)
                        {
                        case value::valuetype::integer_type:
                            opnum1->integer += opnum2->integer; break;
                        case value::valuetype::real_type:
                            opnum1->real += opnum2->real; break;
                        case value::valuetype::handle_type:
                            opnum1->handle += opnum2->handle; break;
                        case value::valuetype::string_type:
                            string_t::gc_new<gcbase::gctype::eden>(opnum1->gcunit, *opnum1->string + *opnum2->string); break;
                        default:
                            rs_fail("Mismatch type for operating."); break;
                        }
                    }
                    else
                    {
                        switch (max_type)
                        {
                        case rs::value::valuetype::integer_type:
                            switch (opnum2->type)
                            {
                            case value::valuetype::integer_type:
                                opnum1->integer += (rs_integer_t)opnum2->integer; break;
                            case value::valuetype::real_type:
                                opnum1->integer += (rs_integer_t)opnum2->real; break;
                            case value::valuetype::handle_type:
                                opnum1->integer += (rs_integer_t)opnum2->handle; break;
                            default:
                                rs_fail("Mismatch type for operating."); break;
                            }
                            break;
                        case rs::value::valuetype::real_type:
                            switch (opnum2->type)
                            {
                            case value::valuetype::integer_type:
                                opnum1->real += (rs_real_t)opnum2->integer; break;
                            case value::valuetype::real_type:
                                opnum1->real += (rs_real_t)opnum2->real; break;
                            case value::valuetype::handle_type:
                                opnum1->real += (rs_real_t)opnum2->handle; break;
                            default:
                                rs_fail("Mismatch type for operating."); break;
                            }
                            break;
                        case rs::value::valuetype::handle_type:
                            switch (opnum2->type)
                            {
                            case value::valuetype::integer_type:
                                opnum1->handle += (rs_handle_t)opnum2->integer; break;
                            case value::valuetype::real_type:
                                opnum1->handle += (rs_handle_t)opnum2->real; break;
                            case value::valuetype::handle_type:
                                opnum1->handle += (rs_handle_t)opnum2->handle; break;
                            default:
                                rs_fail("Mismatch type for operating."); break;
                            }
                            break;
                        default:
                            rs_fail("Mismatch type for operating."); break;
                        }
                    }

                    cr->set_ref(opnum1);
                    break;
                }

                case instruct::opcode::subx:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    value::valuetype max_type = std::max(opnum1->type, opnum2->type);

                    if (opnum1->type != max_type)
                    {
                        opnum1->type = max_type;
                        switch (max_type)
                        {
                        case rs::value::valuetype::integer_type:
                            switch (opnum1->type)
                            {
                            case value::valuetype::real_type:
                                opnum1->integer = (rs_integer_t)opnum1->real; break;
                            case value::valuetype::handle_type:
                                opnum1->integer = (rs_integer_t)opnum1->handle; break;
                            default:
                                rs_fail("Mismatch type for operating."); break;
                            }
                            break;
                        case rs::value::valuetype::real_type:
                            switch (opnum1->type)
                            {
                            case value::valuetype::integer_type:
                                opnum1->real = (rs_real_t)opnum1->integer; break;
                            case value::valuetype::handle_type:
                                opnum1->real = (rs_real_t)opnum1->handle; break;
                            default:
                                rs_fail("Mismatch type for operating."); break;
                            }
                            break;
                        case rs::value::valuetype::handle_type:
                            switch (opnum1->type)
                            {
                            case value::valuetype::integer_type:
                                opnum1->handle = (rs_handle_t)opnum1->integer; break;
                            case value::valuetype::real_type:
                                opnum1->handle = (rs_handle_t)opnum1->real; break;
                            default:
                                rs_fail("Mismatch type for operating."); break;
                            }
                            break;
                        default:
                            rs_fail("Mismatch type for operating."); break;
                        }
                    }

                    ///////////////////////////////////////////////

                    if (opnum2->type == max_type)
                    {
                        switch (opnum2->type)
                        {
                        case value::valuetype::integer_type:
                            opnum1->integer -= opnum2->integer; break;
                        case value::valuetype::real_type:
                            opnum1->real -= opnum2->real; break;
                        case value::valuetype::handle_type:
                            opnum1->handle -= opnum2->handle; break;
                        default:
                            rs_fail("Mismatch type for operating."); break;
                        }
                    }
                    else
                    {
                        switch (max_type)
                        {
                        case rs::value::valuetype::integer_type:
                            switch (opnum2->type)
                            {
                            case value::valuetype::integer_type:
                                opnum1->integer -= (rs_integer_t)opnum2->integer; break;
                            case value::valuetype::real_type:
                                opnum1->integer -= (rs_integer_t)opnum2->real; break;
                            case value::valuetype::handle_type:
                                opnum1->integer -= (rs_integer_t)opnum2->handle; break;
                            default:
                                rs_fail("Mismatch type for operating."); break;
                            }
                            break;
                        case rs::value::valuetype::real_type:
                            switch (opnum2->type)
                            {
                            case value::valuetype::integer_type:
                                opnum1->real -= (rs_real_t)opnum2->integer; break;
                            case value::valuetype::real_type:
                                opnum1->real -= (rs_real_t)opnum2->real; break;
                            case value::valuetype::handle_type:
                                opnum1->real -= (rs_real_t)opnum2->handle; break;
                            default:
                                rs_fail("Mismatch type for operating."); break;
                            }
                            break;
                        case rs::value::valuetype::handle_type:
                            switch (opnum2->type)
                            {
                            case value::valuetype::integer_type:
                                opnum1->handle -= (rs_handle_t)opnum2->integer; break;
                            case value::valuetype::real_type:
                                opnum1->handle -= (rs_handle_t)opnum2->real; break;
                            case value::valuetype::handle_type:
                                opnum1->handle -= (rs_handle_t)opnum2->handle; break;
                            default:
                                rs_fail("Mismatch type for operating."); break;
                            }
                            break;
                        default:
                            rs_fail("Mismatch type for operating."); break;
                        }
                    }

                    cr->set_ref(opnum1);
                    break;
                }

                case instruct::opcode::mulx:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    value::valuetype max_type = std::max(opnum1->type, opnum2->type);

                    if (opnum1->type != max_type)
                    {
                        opnum1->type = max_type;
                        switch (max_type)
                        {
                        case rs::value::valuetype::integer_type:
                            switch (opnum1->type)
                            {
                            case value::valuetype::real_type:
                                opnum1->integer = (rs_integer_t)opnum1->real; break;
                            default:
                                rs_fail("Mismatch type for operating."); break;
                            }
                            break;
                        case rs::value::valuetype::real_type:
                            switch (opnum1->type)
                            {
                            case value::valuetype::integer_type:
                                opnum1->real = (rs_real_t)opnum1->integer; break;
                            default:
                                rs_fail("Mismatch type for operating."); break;
                            }
                            break;
                        default:
                            rs_fail("Mismatch type for operating."); break;
                        }
                    }

                    ///////////////////////////////////////////////

                    if (opnum2->type == max_type)
                    {
                        switch (opnum2->type)
                        {
                        case value::valuetype::integer_type:
                            opnum1->integer *= opnum2->integer; break;
                        case value::valuetype::real_type:
                            opnum1->real *= opnum2->real; break;
                        default:
                            rs_fail("Mismatch type for operating."); break;
                        }
                    }
                    else
                    {
                        switch (max_type)
                        {
                        case rs::value::valuetype::integer_type:
                            switch (opnum2->type)
                            {
                            case value::valuetype::integer_type:
                                opnum1->integer *= (rs_integer_t)opnum2->integer; break;
                            case value::valuetype::real_type:
                                opnum1->integer *= (rs_integer_t)opnum2->real; break;
                            default:
                                rs_fail("Mismatch type for operating."); break;
                            }
                            break;
                        case rs::value::valuetype::real_type:
                            switch (opnum2->type)
                            {
                            case value::valuetype::integer_type:
                                opnum1->real *= (rs_real_t)opnum2->integer; break;
                            case value::valuetype::real_type:
                                opnum1->real *= (rs_real_t)opnum2->real; break;
                            default:
                                rs_fail("Mismatch type for operating."); break;
                            }
                            break;
                        default:
                            rs_fail("Mismatch type for operating."); break;
                        }
                    }

                    cr->set_ref(opnum1);
                    break;
                }

                case instruct::opcode::divx:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    value::valuetype max_type = std::max(opnum1->type, opnum2->type);

                    if (opnum1->type != max_type)
                    {
                        opnum1->type = max_type;
                        switch (max_type)
                        {
                        case rs::value::valuetype::integer_type:
                            switch (opnum1->type)
                            {
                            case value::valuetype::real_type:
                                opnum1->integer = (rs_integer_t)opnum1->real; break;
                            default:
                                rs_fail("Mismatch type for operating."); break;
                            }
                            break;
                        case rs::value::valuetype::real_type:
                            switch (opnum1->type)
                            {
                            case value::valuetype::integer_type:
                                opnum1->real = (rs_real_t)opnum1->integer; break;
                            default:
                                rs_fail("Mismatch type for operating."); break;
                            }
                            break;
                        default:
                            rs_fail("Mismatch type for operating."); break;
                        }
                    }

                    ///////////////////////////////////////////////

                    if (opnum2->type == max_type)
                    {
                        switch (opnum2->type)
                        {
                        case value::valuetype::integer_type:
                            opnum1->integer /= opnum2->integer; break;
                        case value::valuetype::real_type:
                            opnum1->real /= opnum2->real; break;
                        default:
                            rs_fail("Mismatch type for operating."); break;
                        }
                    }
                    else
                    {
                        switch (max_type)
                        {
                        case rs::value::valuetype::integer_type:
                            switch (opnum2->type)
                            {
                            case value::valuetype::integer_type:
                                opnum1->integer /= (rs_integer_t)opnum2->integer; break;
                            case value::valuetype::real_type:
                                opnum1->integer /= (rs_integer_t)opnum2->real; break;
                            default:
                                rs_fail("Mismatch type for operating."); break;
                            }
                            break;
                        case rs::value::valuetype::real_type:
                            switch (opnum2->type)
                            {
                            case value::valuetype::integer_type:
                                opnum1->real /= (rs_real_t)opnum2->integer; break;
                            case value::valuetype::real_type:
                                opnum1->real /= (rs_real_t)opnum2->real; break;
                            default:
                                rs_fail("Mismatch type for operating."); break;
                            }
                            break;
                        default:
                            rs_fail("Mismatch type for operating."); break;
                        }
                    }

                    cr->set_ref(opnum1);
                    break;
                }

                case instruct::opcode::modx:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    value::valuetype max_type = std::max(opnum1->type, opnum2->type);

                    if (opnum1->type != max_type)
                    {
                        opnum1->type = max_type;
                        switch (max_type)
                        {
                        case rs::value::valuetype::integer_type:
                            switch (opnum1->type)
                            {
                            case value::valuetype::real_type:
                                opnum1->integer = (rs_integer_t)opnum1->real; break;
                            default:
                                rs_fail("Mismatch type for operating."); break;
                            }
                            break;
                        case rs::value::valuetype::real_type:
                            switch (opnum1->type)
                            {
                            case value::valuetype::integer_type:
                                opnum1->real = (rs_real_t)opnum1->integer; break;
                            default:
                                rs_fail("Mismatch type for operating."); break;
                            }
                            break;
                        default:
                            rs_fail("Mismatch type for operating."); break;
                        }
                    }

                    ///////////////////////////////////////////////

                    if (opnum2->type == max_type)
                    {
                        switch (opnum2->type)
                        {
                        case value::valuetype::integer_type:
                            opnum1->integer %= opnum2->integer; break;
                        case value::valuetype::real_type:
                            opnum1->real = fmod(opnum1->real, opnum2->real); break;
                        default:
                            rs_fail("Mismatch type for operating."); break;
                        }
                    }
                    else
                    {
                        switch (max_type)
                        {
                        case rs::value::valuetype::integer_type:
                            switch (opnum2->type)
                            {
                            case value::valuetype::integer_type:
                                opnum1->integer %= (rs_integer_t)opnum2->integer; break;
                            case value::valuetype::real_type:
                                opnum1->integer %= (rs_integer_t)opnum2->real; break;
                            default:
                                rs_fail("Mismatch type for operating."); break;
                            }
                            break;
                        case rs::value::valuetype::real_type:
                            switch (opnum2->type)
                            {
                            case value::valuetype::integer_type:
                                opnum1->real = fmod(opnum1->real, (rs_real_t)opnum2->integer); break;
                            case value::valuetype::real_type:
                                opnum1->real = fmod(opnum1->real, (rs_real_t)opnum2->real); break;
                            default:
                                rs_fail("Mismatch type for operating."); break;
                            }
                            break;
                        default:
                            rs_fail("Mismatch type for operating."); break;
                        }
                    }

                    cr->set_ref(opnum1);
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
                case instruct::opcode::movx:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    if (opnum1->type == opnum2->type)
                        opnum1->handle = opnum2->handle;  // Has same type, just move all data.

                    switch (opnum1->type)
                    {
                    case value::valuetype::integer_type:
                    {
                        switch (opnum2->type)
                        {
                        case value::valuetype::real_type:
                            opnum1->integer = (rs_integer_t)opnum2->real; break;
                        case value::valuetype::handle_type:
                            opnum1->integer = (rs_integer_t)opnum2->handle; break;
                        default:
                            rs_fail("Type mismatch between two opnum.");
                            break;
                        }break;
                    }
                    case value::valuetype::real_type:
                    {
                        switch (opnum2->type)
                        {
                        case value::valuetype::integer_type:
                            opnum1->real = (rs_real_t)opnum2->integer; break;
                        case value::valuetype::handle_type:
                            opnum1->real = (rs_real_t)opnum2->handle; break;
                        default:
                            rs_fail("Type mismatch between two opnum.");
                            break;
                        }break;
                    }
                    case value::valuetype::handle_type:
                    {
                        switch (opnum2->type)
                        {
                        case value::valuetype::integer_type:
                            opnum1->handle = (rs_handle_t)opnum2->integer; break;
                        case value::valuetype::real_type:
                            opnum1->handle = (rs_handle_t)opnum2->real; break;
                        default:
                            rs_fail("Type mismatch between two opnum.");
                            break;
                        }break;
                    }
                    default:
                        rs_fail("Type mismatch between two opnum.");
                        break;
                    }

                    cr->set_ref(opnum1);
                    break;
                }
                case instruct::opcode::movi2r:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    rs_assert(opnum2->type == value::valuetype::integer_type);

                    opnum1->type = value::valuetype::real_type;
                    opnum1->real = (real_t)opnum2->integer;

                    cr->set_ref(opnum1);

                    break;
                }
                case instruct::opcode::movr2i:
                {

                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    rs_assert(opnum2->type == value::valuetype::real_type);

                    opnum1->type = value::valuetype::integer_type;
                    opnum1->integer = (int64_t)opnum2->real;

                    cr->set_ref(opnum1);

                    break;
                }
                case instruct::opcode::seti2r:
                {
                    RS_ADDRESSING_N1;
                    RS_ADDRESSING_N2_REF;

                    rs_assert(opnum2->type == value::valuetype::integer_type);

                    opnum1->type = value::valuetype::real_type;
                    opnum1->real = (real_t)opnum2->integer;
                    cr->set_ref(opnum1);

                    break;
                }
                case instruct::opcode::setr2i:
                {

                    RS_ADDRESSING_N1;
                    RS_ADDRESSING_N2_REF;

                    rs_assert(opnum2->type == value::valuetype::real_type);

                    opnum1->type = value::valuetype::integer_type;
                    opnum1->integer = (int64_t)opnum2->real;
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
                case instruct::opcode::equb:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    cr->type = value::valuetype::integer_type;
                    cr->integer = opnum1->integer == opnum2->integer;

                    break;
                }
                case instruct::opcode::nequb:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    cr->type = value::valuetype::integer_type;
                    cr->integer = opnum1->integer != opnum2->integer;
                    break;
                }
                case instruct::opcode::equx:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    cr->type = value::valuetype::integer_type;
                    if (opnum1->type == opnum2->type)
                    {
                        switch (opnum1->type)
                        {
                        case value::valuetype::integer_type:
                            cr->integer = opnum1->integer == opnum2->integer; break;
                        case value::valuetype::handle_type:
                            cr->integer = opnum1->handle == opnum2->handle; break;
                        case value::valuetype::real_type:
                            cr->integer = opnum1->real == opnum2->real; break;
                        case value::valuetype::string_type:
                            cr->integer = *opnum1->string == *opnum2->string; break;

                        case value::valuetype::mapping_type:
                        case value::valuetype::array_type:
                            cr->integer = opnum1->gcunit == opnum2->gcunit; break;
                        default:
                            rs_fail("Values of this type cannot be compared.");
                            cr->integer = 0;
                            break;
                        }
                    }
                    else
                        cr->integer = 1;
                    break;

                }
                case instruct::opcode::nequx:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    cr->type = value::valuetype::integer_type;
                    if (opnum1->type == opnum2->type)
                    {
                        switch (opnum1->type)
                        {
                        case value::valuetype::integer_type:
                            cr->integer = opnum1->integer != opnum2->integer; break;
                        case value::valuetype::handle_type:
                            cr->integer = opnum1->handle != opnum2->handle; break;
                        case value::valuetype::real_type:
                            cr->integer = opnum1->real != opnum2->real; break;
                        case value::valuetype::string_type:
                            cr->integer = *opnum1->string != *opnum2->string; break;

                        case value::valuetype::mapping_type:
                        case value::valuetype::array_type:
                            cr->integer = opnum1->gcunit != opnum2->gcunit; break;
                        default:
                            rs_fail("Values of this type cannot be compared.");
                            cr->integer = 0;
                            break;
                        }
                    }
                    else
                        cr->integer = 0;
                    break;
                }

                case instruct::opcode::land:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    cr->type = value::valuetype::integer_type;
                    cr->integer = opnum1->integer && opnum2->integer;

                    break;
                }
                case instruct::opcode::lor:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    cr->type = value::valuetype::integer_type;
                    cr->integer = opnum1->integer || opnum2->integer;

                    break;
                }
                case instruct::opcode::lnot:
                {
                    RS_ADDRESSING_N1_REF;

                    cr->type = value::valuetype::integer_type;
                    cr->integer = !opnum1->integer;

                    break;
                }

                case instruct::opcode::lti:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    rs_assert(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::integer_type);

                    cr->type = value::valuetype::integer_type;
                    cr->integer = opnum1->integer < opnum2->integer;


                    break;
                }
                case instruct::opcode::gti:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    rs_assert(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::integer_type);

                    cr->type = value::valuetype::integer_type;
                    cr->integer = opnum1->integer > opnum2->integer;


                    break;
                }
                case instruct::opcode::elti:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    rs_assert(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::integer_type);

                    cr->type = value::valuetype::integer_type;
                    cr->integer = opnum1->integer <= opnum2->integer;


                    break;
                }
                case instruct::opcode::egti:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    rs_assert(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::integer_type);

                    cr->type = value::valuetype::integer_type;
                    cr->integer = opnum1->integer >= opnum2->integer;


                    break;
                }

                case instruct::opcode::ltr:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    rs_assert(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::real_type);

                    cr->type = value::valuetype::integer_type;
                    cr->integer = opnum1->real < opnum2->real;


                    break;
                }
                case instruct::opcode::gtr:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    rs_assert(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::real_type);

                    cr->type = value::valuetype::integer_type;
                    cr->integer = opnum1->real > opnum2->real;


                    break;
                }
                case instruct::opcode::eltr:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    rs_assert(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::real_type);

                    cr->type = value::valuetype::integer_type;
                    cr->integer = opnum1->real <= opnum2->real;


                    break;
                }
                case instruct::opcode::egtr:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    rs_assert(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::real_type);

                    cr->type = value::valuetype::integer_type;
                    cr->integer = opnum1->real >= opnum2->real;


                    break;
                }

                case instruct::opcode::ltx:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    cr->type = value::valuetype::integer_type;
                    if (opnum1->type == opnum2->type)
                    {
                        switch (opnum1->type)
                        {
                        case value::valuetype::integer_type:
                            cr->integer = opnum1->integer < opnum2->integer; break;
                        case value::valuetype::handle_type:
                            cr->integer = opnum1->handle < opnum2->handle; break;
                        case value::valuetype::real_type:
                            cr->integer = opnum1->real < opnum2->real; break;
                        case value::valuetype::string_type:
                            cr->integer = *opnum1->string < *opnum2->string; break;
                        default:
                            rs_fail("Values of this type cannot be compared.");
                            cr->integer = 0;
                            break;
                        }
                    }
                    else
                        cr->integer = opnum1->type < opnum2->type;

                    break;
                }
                case instruct::opcode::gtx:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    cr->type = value::valuetype::integer_type;
                    if (opnum1->type == opnum2->type)
                    {
                        switch (opnum1->type)
                        {
                        case value::valuetype::integer_type:
                            cr->integer = opnum1->integer > opnum2->integer; break;
                        case value::valuetype::handle_type:
                            cr->integer = opnum1->handle > opnum2->handle; break;
                        case value::valuetype::real_type:
                            cr->integer = opnum1->real > opnum2->real; break;
                        case value::valuetype::string_type:
                            cr->integer = *opnum1->string > *opnum2->string; break;
                        default:
                            rs_fail("Values of this type cannot be compared.");
                            cr->integer = 0;
                            break;
                        }
                    }
                    else
                        cr->integer = opnum1->type > opnum2->type;
                    break;
                }
                case instruct::opcode::eltx:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    cr->type = value::valuetype::integer_type;
                    if (opnum1->type == opnum2->type)
                    {
                        switch (opnum1->type)
                        {
                        case value::valuetype::integer_type:
                            cr->integer = opnum1->integer <= opnum2->integer; break;
                        case value::valuetype::handle_type:
                            cr->integer = opnum1->handle <= opnum2->handle; break;
                        case value::valuetype::real_type:
                            cr->integer = opnum1->real <= opnum2->real; break;
                        case value::valuetype::string_type:
                            cr->integer = *opnum1->string <= *opnum2->string; break;
                        default:
                            rs_fail("Values of this type cannot be compared.");
                            cr->integer = 0;
                            break;
                        }
                    }
                    else
                        cr->integer = opnum1->type <= opnum2->type;

                    break;
                }
                case instruct::opcode::egtx:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    cr->type = value::valuetype::integer_type;
                    if (opnum1->type == opnum2->type)
                    {
                        switch (opnum1->type)
                        {
                        case value::valuetype::integer_type:
                            cr->integer = opnum1->integer >= opnum2->integer; break;
                        case value::valuetype::handle_type:
                            cr->integer = opnum1->handle >= opnum2->handle; break;
                        case value::valuetype::real_type:
                            cr->integer = opnum1->real >= opnum2->real; break;
                        case value::valuetype::string_type:
                            cr->integer = *opnum1->string >= *opnum2->string; break;
                        default:
                            rs_fail("Values of this type cannot be compared.");
                            cr->integer = 0;
                            break;
                        }
                    }
                    else
                        cr->integer = opnum1->type >= opnum2->type;
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
                        bp = sp = rt_sp;
                        rs_asure(interrupt(vm_interrupt_type::LEAVE_INTERRUPT));
                        reinterpret_cast<native_func_t>(opnum1->handle)(reinterpret_cast<rs_vm>(this));
                        rs_asure(clear_interrupt(vm_interrupt_type::LEAVE_INTERRUPT));
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
                        bp = sp = rt_sp;
                        rs_asure(interrupt(vm_interrupt_type::LEAVE_INTERRUPT));
                        reinterpret_cast<native_func_t>(RS_IPVAL_MOVE_8)(reinterpret_cast<rs_vm>(this));
                        rs_asure(clear_interrupt(vm_interrupt_type::LEAVE_INTERRUPT));
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
                        {
                            // Maybe need something to solve exception?
                        }
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
                case instruct::opcode::mkarr:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    opnum1->set_gcunit_with_barrier(value::valuetype::array_type);
                    auto* created_array = array_t::gc_new<gcbase::gctype::eden>(opnum1->gcunit);

                    rs_assert(opnum2->type == value::valuetype::integer_type);
                    // Well, both integer_type and handle_type will work well, but here just allowed integer_type.

                    gcbase::gc_write_guard gwg1(created_array);

                    created_array->resize((size_t)opnum2->integer);
                    for (size_t i = 0; i < (size_t)opnum2->integer; i++)
                    {
                        (*created_array)[i].set_val((++rt_sp)->get());
                    }
                    break;
                }
                case instruct::opcode::mkmap:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    opnum1->set_gcunit_with_barrier(value::valuetype::mapping_type);
                    auto* created_map = mapping_t::gc_new<gcbase::gctype::eden>(opnum1->gcunit);

                    rs_assert(opnum2->type == value::valuetype::integer_type);
                    // Well, both integer_type and handle_type will work well, but here just allowed integer_type.

                    gcbase::gc_write_guard gwg1(created_map);

                    for (size_t i = 0; i < (size_t)opnum2->integer; i++)
                    {
                        value* val = ++rt_sp;
                        value* key = ++rt_sp;
                        (*created_map)[*(key->get())].set_val(val->get());
                    }
                    break;
                }
                case instruct::opcode::idx:
                {
                    RS_ADDRESSING_N1_REF;
                    RS_ADDRESSING_N2_REF;

                    if (nullptr == opnum1->gcunit)
                        rs_fail("the unit trying to access is 'nil'.");
                    else
                    {
                        gcbase::gc_read_guard gwg1(opnum1->gcunit);

                        switch (opnum1->type)
                        {
                        case value::valuetype::string_type:
                            cr->type = value::valuetype::integer_type;
                            cr->integer = (unsigned char)(*opnum1->string)[(size_t)opnum2->integer];
                            break;
                        case value::valuetype::array_type:
                            cr->set_ref(&(*opnum1->array)[(size_t)opnum2->integer]);
                            break;
                        case value::valuetype::mapping_type:
                            cr->set_ref(&(*opnum1->mapping)[*opnum2]);
                            break;
                        default:
                            rs_fail("unknown type to index.");
                            break;
                        }
                    }
                    break;
                }
                case instruct::opcode::ext:
                {
                    // extern code page:
                    int page_index = dr;

                    opcode_dr = *(rt_ip++);
                    opcode = (instruct::opcode)(opcode_dr & 0b11111100u);
                    dr = opcode_dr & 0b00000011u;

                    switch (page_index)
                    {
                    case 0:     // extern-opcode-page-0
                        switch ((instruct::extern_opcode_page_0)(opcode))
                        {
                        case instruct::extern_opcode_page_0::setref:
                        {
                            RS_ADDRESSING_N1;
                            RS_ADDRESSING_N2_REF;
                            cr->set_ref(opnum1->set_ref(opnum2));
                            break;
                        }
                        default:
                            rs_error("Unknown instruct.");
                            break;
                        }
                        break;
                    case 1:     // extern-opcode-page-1
                        switch ((instruct::extern_opcode_page_1)(opcode))
                        {
                        case instruct::extern_opcode_page_1::prnt:
                        {
                            RS_ADDRESSING_N1_REF;
                            rs_error("This command not impl now");
                            break;
                        }
                        case instruct::extern_opcode_page_1::detail:
                        {
                            RS_ADDRESSING_N1;
                            rs_error("This command not impl now");
                            break;
                        }
                        default:
                            rs_error("Unknown instruct.");
                            break;
                        }
                        break;
                    default:
                        rs_error("Unknown extern-opcode-page.");
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

            _vm_check_interrupt_first:

                if (vm_interrupt)
                {
                    if (vm_interrupt & vm_interrupt_type::GC_INTERRUPT)
                    {
                        // write regist(sp) data, then clear interrupt mark.
                        sp = rt_sp;
                        if (clear_interrupt(vm_interrupt_type::GC_INTERRUPT))
                            hangup();   // SLEEP UNTIL WAKE UP
                    }
                    else if (vm_interrupt & vm_interrupt_type::LEAVE_INTERRUPT)
                    {
                        // That should not be happend...

                        rs_error("Virtual machine handled a LEAVE_INTERRUPT.");
                    }
                }

            }// vm loop end.
#undef RS_ADDRESSING_N2_REF
#undef RS_ADDRESSING_N1_REF
#undef RS_ADDRESSING_N2
#undef RS_ADDRESSING_N1
#undef RS_SIGNED_SHIFT
#undef RS_IPVAL_MOVE_8
#undef RS_IPVAL_MOVE_4
#undef RS_IPVAL_MOVE_2
#undef RS_IPVAL_MOVE_1
#undef RS_IPVAL

        }catch (const std::exception& any_excep)
        {
            rs::exception_recovery::rollback(this);
        }
    };
}

#undef RS_READY_EXCEPTION_HANDLE