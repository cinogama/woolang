#pragma once

#include "rs_basic_type.hpp"
#include "rs_compiler_ir.hpp"
#include "rs_utf8.hpp"
#include "rs_global_setting.hpp"
#include "rs_memory.hpp"
#include "rs_compiler_jit.hpp"

#include <csetjmp>
#include <shared_mutex>
#include <thread>
#include <string>
#include <cmath>
#include <sstream>

namespace rs
{
    struct vmbase;
    class exception_recovery;
    class debuggee_base
    {
        inline static gcbase::rw_lock _global_vm_debug_block_spin;
    public:
        void _vm_invoke_debuggee(vmbase* _vm)
        {
            _global_vm_debug_block_spin.lock();
            _global_vm_debug_block_spin.unlock();
            // Just make a block

            debug_interrupt(_vm);
        }
        virtual void debug_interrupt(vmbase*) = 0;
        virtual~debuggee_base() = default;
    protected:

        void block_other_vm_in_this_debuggee() { _global_vm_debug_block_spin.lock(); }
        void unblock_other_vm_in_this_debuggee() { _global_vm_debug_block_spin.unlock(); }


    };

    class exception_recovery
    {
        exception_recovery(vmbase* _vm, const byte_t* _ip, value* _sp, value* _bp);
        vmbase* vm;
    public:
        const byte_t* ip;
        value* sp;
        value* bp;
        exception_recovery* last;
        std::jmp_buf native_env;
        inline static void rollback(vmbase* _vm);
        inline static void ok(vmbase* _vm);
        inline static void _ready(vmbase* _vm, const byte_t* _ip, value* _sp, value* _bp);
    };

    struct vmbase
    {
        inline static std::shared_mutex _alive_vm_list_mx;
        inline static cxx_set_t<vmbase*> _alive_vm_list;
        inline thread_local static vmbase* _this_thread_vm = nullptr;
        inline static std::atomic_uint32_t _alive_vm_count_for_gc_vm_destruct;

        enum class jit_state : byte_t
        {
            NONE = 0,
            PREPARING = 1,
            READY = 2,
        };

        enum class vm_type
        {
            NORMAL,

            // If vm's type is GC_DESTRUCTOR, GC_THREAD will not trying to pause it.
            GC_DESTRUCTOR,
        };
        vm_type virtual_machine_type = vm_type::NORMAL;

        enum vm_interrupt_type
        {
            NOTHING = 0,
            // There is no interrupt

            GC_INTERRUPT = 1 << 8,
            // GC work will cause this interrupt, if vm received this interrupt,
            // should clean this interrupt flag, if clean-operate is successful,
            // vm should call 'hangup' to wait for GC work. 
            // GC work will cancel GC_INTERRUPT after collect_stage_1. if cancel
            // failed, it means vm already hangned(or trying hangs now), GC work
            // will call 'wakeup' to resume vm.

            LEAVE_INTERRUPT = 1 << 9,
            // When GC work trying GC_INTERRUPT, it will wait for vm cleaning 
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
            //            successful. (We use 'rs_asure' here)'

            DEBUG_INTERRUPT = 1 << 10,
            // If virtual machine interrupt with DEBUG_INTERRUPT, it will stop at all opcode
            // to check something about debug, such as breakpoint.
            // * DEBUG_INTERRUPT will cause huge performance loss

            ABORT_INTERRUPT = 1 << 11,
            // If virtual machine interrupt with ABORT_INTERRUPT, vm will stop immediately.

            YIELD_INTERRUPT = 1 << 12,
            // If virtual machine interrupt with YIELD_INTERRUPT, vm will stop immediately.
            //  * Unlike ABORT_INTERRUPT, VM will clear YIELD_INTERRUPT flag after detective.
            //  * This flag used for rs_coroutine

            PENDING_INTERRUPT = 1 << 13,
            // VM will be pending when roroutine_mgr finish using pooled-vm, PENDING_INTERRUPT
            // only setted when vm is not running.
        };

        vmbase(const vmbase&) = delete;
        vmbase(vmbase&&) = delete;
        vmbase& operator=(const vmbase&) = delete;
        vmbase& operator=(vmbase&&) = delete;

        union
        {
            std::atomic<uint32_t> vm_interrupt;
            uint32_t fast_ro_vm_interrupt;
        };
        static_assert(sizeof(std::atomic<uint32_t>) == sizeof(uint32_t));
        static_assert(std::atomic<uint32_t>::is_always_lock_free);

        void* operator new(size_t sz)
        {
            return alloc64(sz);
        }
        void operator delete(void* ptr)
        {
            return free64(ptr);
        }

    private:
        std::mutex _vm_hang_mx;
        std::condition_variable _vm_hang_cv;
        std::atomic_int8_t _vm_hang_flag = 0;

    protected:
        debuggee_base* attaching_debuggee = nullptr;

    public:
        inline debuggee_base* attach_debuggee(debuggee_base* dbg)
        {
            if (dbg)
                interrupt(vmbase::vm_interrupt_type::DEBUG_INTERRUPT);
            else if (attaching_debuggee)
                clear_interrupt(vmbase::vm_interrupt_type::DEBUG_INTERRUPT);
            auto* old_debuggee = attaching_debuggee;
            attaching_debuggee = dbg;
            return old_debuggee;
        }
        inline debuggee_base* current_debuggee()
        {
            return attaching_debuggee;
        }

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
            constexpr int MAX_TRY_COUNT = 0;
            int i = 0;

            uint32_t vm_interrupt_mask = 0xFFFFFFFF;
            do
            {
                vm_interrupt_mask = vm_interrupt.load();
                if (vm_interrupt_mask & vm_interrupt_type::LEAVE_INTERRUPT)
                {
                    if (++i > MAX_TRY_COUNT)
                        return false;
                }
                else
                    i = 0;

                std::this_thread::yield();

            } while (vm_interrupt_mask & type);

            return true;
        }
        inline void block_interrupt(vm_interrupt_type type)
        {
            while (vm_interrupt & type)
                std::this_thread::yield();
        }

        inline void hangup()
        {
            do
            {
                std::lock_guard g1(_vm_hang_mx);
                _vm_hang_flag.fetch_sub(1);
            } while (0);

            std::unique_lock ug1(_vm_hang_mx);
            _vm_hang_cv.wait(ug1, [this]() {return _vm_hang_flag >= 0; });
        }
        inline void wakeup()
        {
            do
            {
                std::lock_guard g1(_vm_hang_mx);
                _vm_hang_flag.fetch_add(1);
            } while (0);

            _vm_hang_cv.notify_one();
        }

        vmbase()
        {
            ++_alive_vm_count_for_gc_vm_destruct;

            vm_interrupt = vm_interrupt_type::NOTHING;
            interrupt(vm_interrupt_type::LEAVE_INTERRUPT);

            std::lock_guard g1(_alive_vm_list_mx);

            rs_assert(_alive_vm_list.find(this) == _alive_vm_list.end(),
                "This vm is already exists in _alive_vm_list, that is illegal.");

            _alive_vm_list.insert(this);
        }
        virtual ~vmbase()
        {
            std::lock_guard g1(_alive_vm_list_mx);

            if (_self_stack_reg_mem_buf)
                free64(_self_stack_reg_mem_buf);

            rs_assert(_alive_vm_list.find(this) != _alive_vm_list.end(),
                "This vm not exists in _alive_vm_list, that is illegal.");

            _alive_vm_list.erase(this);

            if (veh)
                exception_recovery::ok(this);

            if (compile_info)
                delete compile_info;

            if (env)
                --env->_running_on_vm_count;
        }

        lexer* compile_info = nullptr;

        // vm exception handler
        exception_recovery* veh = nullptr;

        // next ircode pointer
        const byte_t* ip = nullptr;

        // special regist
        value* cr = nullptr;  // op result trace & function return;
        value* tc = nullptr;  // arugument count
        value* er = nullptr;  // exception result
        value* ths = nullptr;  // exception result

        // stack info
        value* sp = nullptr;
        value* bp = nullptr;

        value* stack_mem_begin = nullptr;
        value* register_mem_begin = nullptr;
        value* _self_stack_reg_mem_buf = nullptr;
        size_t stack_size = 0;

        shared_pointer<runtime_env> env;
        void set_runtime(ir_compiler& _compiler)
        {
            // using LEAVE_INTERRUPT to stop GC
            block_interrupt(GC_INTERRUPT);  // must not working when gc
            rs_asure(clear_interrupt(LEAVE_INTERRUPT));

            rs_assert(nullptr == _self_stack_reg_mem_buf);

            env = _compiler.finalize();
            ++env->_running_on_vm_count;

            stack_mem_begin = env->stack_begin;
            register_mem_begin = env->reg_begin;
            stack_size = env->runtime_stack_count;

            ip = env->rt_codes;
            cr = register_mem_begin + opnum::reg::spreg::cr;
            tc = register_mem_begin + opnum::reg::spreg::tc;
            er = register_mem_begin + opnum::reg::spreg::er;
            ths = register_mem_begin + opnum::reg::spreg::ths;
            sp = bp = stack_mem_begin;

            rs_asure(interrupt(LEAVE_INTERRUPT));

            // Create a new VM using for GC destruct
            vmbase* gc_thread = make_machine();
            // gc_thread will be destructed by gc_work..
            gc_thread->virtual_machine_type = vm_type::GC_DESTRUCTOR;

        }
        virtual vmbase* create_machine() const = 0;
        vmbase* make_machine(size_t stack_sz = 0) const
        {
            rs_assert(env != nullptr);

            vmbase* new_vm = create_machine();

            // using LEAVE_INTERRUPT to stop GC
            new_vm->block_interrupt(GC_INTERRUPT);  // must not working when gc'
            rs_asure(new_vm->clear_interrupt(LEAVE_INTERRUPT));

            if (!stack_sz)
                stack_sz = env->runtime_stack_count;
            new_vm->stack_size = stack_sz;

            new_vm->_self_stack_reg_mem_buf = (value*)alloc64(sizeof(value) *
                (env->real_register_count + stack_sz));

            memset(new_vm->_self_stack_reg_mem_buf, 0, sizeof(value) *
                (env->real_register_count + stack_sz));

            new_vm->stack_mem_begin = new_vm->_self_stack_reg_mem_buf
                + (env->real_register_count + stack_sz - 1);
            new_vm->register_mem_begin = new_vm->_self_stack_reg_mem_buf;

            new_vm->ip = env->rt_codes;
            new_vm->cr = new_vm->register_mem_begin + opnum::reg::spreg::cr;
            new_vm->tc = new_vm->register_mem_begin + opnum::reg::spreg::tc;
            new_vm->er = new_vm->register_mem_begin + opnum::reg::spreg::er;
            new_vm->ths = new_vm->register_mem_begin + opnum::reg::spreg::ths;
            new_vm->sp = new_vm->bp = new_vm->stack_mem_begin;

            new_vm->env = env;  // env setted, gc will scan this vm..
            ++env->_running_on_vm_count;

            new_vm->attach_debuggee(this->attaching_debuggee);

            rs_asure(new_vm->interrupt(LEAVE_INTERRUPT));
            return new_vm;
        }
        inline void dump_program_bin(size_t begin = 0, size_t end = SIZE_MAX, std::ostream& os = std::cout) const
        {
            auto* program = env->rt_codes;

            auto* program_ptr = program + begin;
            while (program_ptr < program + std::min(env->rt_code_len, end))
            {
                auto* this_command_ptr = program_ptr;
                auto main_command = *(this_command_ptr++);
                std::stringstream tmpos;

                auto print_byte = [&]() {

                    const int MAX_BYTE_COUNT = 10;
                    printf("+%04d : ", (uint32_t)(program_ptr - program));
                    int displayed_count = 0;
                    for (auto idx = program_ptr; idx < this_command_ptr; idx++)
                    {
                        printf("%02X ", (uint32_t)*idx);
                        displayed_count++;
                    }
                    for (int i = 0; i < MAX_BYTE_COUNT - displayed_count; i++)
                        printf("   ");
                };
#define RS_SIGNED_SHIFT(VAL) (((signed char)((unsigned char)(((unsigned char)(VAL))<<1)))>>1)
                auto print_opnum1 = [&]() {
                    if (main_command & (byte_t)0b00000010)
                    {
                        //is dr 1byte 
                        byte_t data_1b = *(this_command_ptr++);
                        if (data_1b & 1 << 7)
                        {
                            // bp offset
                            auto offset = RS_SIGNED_SHIFT(data_1b);
                            tmpos << "[bp";
                            if (offset < 0)
                                tmpos << offset << "]";
                            else if (offset == 0)
                                tmpos << "-" << offset << "]";
                            else
                                tmpos << "+" << offset << "]";
                        }
                        else
                        {
                            // is reg
                            if (data_1b >= 0 && data_1b <= 15)
                                tmpos << "t" << (uint32_t)data_1b;
                            else if (data_1b >= 16 && data_1b <= 31)
                                tmpos << "r" << (uint32_t)data_1b - 16;
                            else if (data_1b == 32)
                                tmpos << "cr";
                            else if (data_1b == 33)
                                tmpos << "tc";
                            else if (data_1b == 34)
                                tmpos << "er";
                            else if (data_1b == 35)
                                tmpos << "nil";
                            else
                                tmpos << "reg(" << (uint32_t)data_1b << ")";

                        }
                    }
                    else
                    {
                        //const global 4byte
                        uint32_t data_4b = *(uint32_t*)((this_command_ptr += 4) - 4);
                        if (data_4b < env->constant_value_count)
                            tmpos << rs_cast_string((rs_value)&env->constant_global_reg_rtstack[data_4b])
                            << " : " << rs_type_name((rs_value)&env->constant_global_reg_rtstack[data_4b]);
                        else
                            tmpos << "g[" << data_4b - env->constant_value_count << "]";
                    }
                };
                auto print_opnum2 = [&]() {
                    if (main_command & (byte_t)0b00000001)
                    {
                        //is dr 1byte 
                        byte_t data_1b = *(this_command_ptr++);
                        if (data_1b & 1 << 7)
                        {
                            // bp offset
                            auto offset = RS_SIGNED_SHIFT(data_1b);
                            tmpos << "[bp";
                            if (offset < 0)
                                tmpos << offset << "]";
                            else if (offset == 0)
                                tmpos << "-" << offset << "]";
                            else
                                tmpos << "+" << offset << "]";
                        }
                        else
                        {
                            // is reg
                            if (data_1b >= 0 && data_1b <= 15)
                                tmpos << "t" << (uint32_t)data_1b;
                            else if (data_1b >= 16 && data_1b <= 31)
                                tmpos << "r" << (uint32_t)data_1b - 16;
                            else if (data_1b == 32)
                                tmpos << "cr";
                            else if (data_1b == 33)
                                tmpos << "tc";
                            else if (data_1b == 34)
                                tmpos << "er";
                            else if (data_1b == 35)
                                tmpos << "nil";
                            else
                                tmpos << "reg(" << (uint32_t)data_1b << ")";

                        }
                    }
                    else
                    {
                        //const global 4byte
                        uint32_t data_4b = *(uint32_t*)((this_command_ptr += 4) - 4);
                        if (data_4b < env->constant_value_count)
                            tmpos << rs_cast_string((rs_value)&env->constant_global_reg_rtstack[data_4b])
                            << " : " << rs_type_name((rs_value)&env->constant_global_reg_rtstack[data_4b]);
                        else
                            tmpos << "g[" << data_4b - env->constant_value_count << "]";
                    }
                };

#undef RS_SIGNED_SHIFT
                switch (main_command & (byte_t)0b11111100)
                {
                case instruct::nop:
                    tmpos << "nop\t";

                    this_command_ptr += main_command & (byte_t)0b00000011;

                    break;
                case instruct::set:
                    tmpos << "set\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
                case instruct::mov:
                    tmpos << "mov\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;

                case instruct::addi:
                    tmpos << "addi\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
                case instruct::subi:
                    tmpos << "subi\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
                case instruct::muli:
                    tmpos << "muli\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
                case instruct::divi:
                    tmpos << "divi\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
                case instruct::modi:
                    tmpos << "modi\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;

                case instruct::addr:
                    tmpos << "addr\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
                case instruct::subr:
                    tmpos << "subr\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
                case instruct::mulr:
                    tmpos << "mulr\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
                case instruct::divr:
                    tmpos << "divr\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
                case instruct::modr:
                    tmpos << "modr\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;

                case instruct::addh:
                    tmpos << "addh\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
                case instruct::subh:
                    tmpos << "subh\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;

                case instruct::adds:
                    tmpos << "adds\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;

                case instruct::psh:
                    if (main_command & 0b01)
                    {
                        tmpos << "psh\t"; print_opnum1(); break;
                    }
                    else
                    {
                        tmpos << "pshn repeat\t" << *(uint16_t*)((this_command_ptr += 2) - 2); break;
                    }
                case instruct::pop:
                    if (main_command & 0b01)
                    {
                        tmpos << "pop\t"; print_opnum1(); break;
                    }
                    else
                    {
                        tmpos << "pop repeat\t" << *(uint16_t*)((this_command_ptr += 2) - 2); break;
                    }
                case instruct::pshr:
                    tmpos << "pshr\t"; print_opnum1(); break;
                case instruct::popr:
                    tmpos << "popr\t"; print_opnum1(); break;

                case instruct::lds:
                    tmpos << "lds\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
                case instruct::ldsr:
                    tmpos << "ldsr\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;

                case instruct::lti:
                    tmpos << "lti\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
                case instruct::gti:
                    tmpos << "gti\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
                case instruct::elti:
                    tmpos << "elti\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
                case instruct::egti:
                    tmpos << "egti\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;

                case instruct::land:
                    tmpos << "land\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
                case instruct::lor:
                    tmpos << "lor\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
                case instruct::lnot:
                    tmpos << "lnot\t"; print_opnum1(); break;

                case instruct::ltx:
                    tmpos << "ltx\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
                case instruct::gtx:
                    tmpos << "gtx\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
                case instruct::eltx:
                    tmpos << "eltx\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
                case instruct::egtx:
                    tmpos << "egtx\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;

                case instruct::ltr:
                    tmpos << "ltr\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
                case instruct::gtr:
                    tmpos << "gtr\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
                case instruct::eltr:
                    tmpos << "eltr\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
                case instruct::egtr:
                    tmpos << "egtr\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;

                case instruct::call:
                    tmpos << "call\t"; print_opnum1(); break;

                case instruct::calln:
                    tmpos << "calln\t";
                    if (main_command & 0b01)
                        //neg
                        tmpos << *(void**)((this_command_ptr += 8) - 8);
                    else
                        tmpos << "+" << *(uint32_t*)((this_command_ptr += 4) - 4);
                    break;
                case instruct::calljit:
                    tmpos << "calljit\t";
                    this_command_ptr += 9;
                    break;

                case instruct::ret:
                    tmpos << "ret\t"; break;

                case instruct::jt:
                    tmpos << "jt\t";
                    tmpos << "+" << *(uint32_t*)((this_command_ptr += 4) - 4);
                    break;
                case instruct::jf:
                    tmpos << "jf\t";
                    tmpos << "+" << *(uint32_t*)((this_command_ptr += 4) - 4);
                    break;
                case instruct::jmp:
                    tmpos << "jmp\t";
                    tmpos << "+" << *(uint32_t*)((this_command_ptr += 4) - 4);
                    break;

                case instruct::movcast:
                    tmpos << "movcast\t"; print_opnum1(); tmpos << ",\t"; print_opnum2();
                    tmpos << " : ";
                    switch ((value::valuetype) * (this_command_ptr++))
                    {
                    case value::valuetype::integer_type:
                        tmpos << "int"; break;
                    case value::valuetype::real_type:
                        tmpos << "real"; break;
                    case value::valuetype::handle_type:
                        tmpos << "handle"; break;
                    case value::valuetype::string_type:
                        tmpos << "string"; break;
                    case value::valuetype::array_type:
                        tmpos << "array"; break;
                    case value::valuetype::mapping_type:
                        tmpos << "map"; break;
                    case value::valuetype::gchandle_type:
                        tmpos << "gchandle"; break;
                    default:
                        tmpos << "unknown"; break;
                    }

                    break;

                case instruct::setcast:
                    tmpos << "setcast\t"; print_opnum1(); tmpos << ",\t"; print_opnum2();
                    tmpos << " : ";
                    switch ((value::valuetype) * (this_command_ptr++))
                    {
                    case value::valuetype::integer_type:
                        tmpos << "int"; break;
                    case value::valuetype::real_type:
                        tmpos << "real"; break;
                    case value::valuetype::handle_type:
                        tmpos << "handle"; break;
                    case value::valuetype::string_type:
                        tmpos << "string"; break;
                    case value::valuetype::array_type:
                        tmpos << "array"; break;
                    case value::valuetype::mapping_type:
                        tmpos << "map"; break;
                    case value::valuetype::gchandle_type:
                        tmpos << "gchandle"; break;
                    default:
                        tmpos << "unknown"; break;
                    }

                    break;
                case instruct::typeas:
                    if (main_command & 0b01)
                        tmpos << "typeis\t";
                    else
                        tmpos << "typeas\t";
                    print_opnum1();
                    tmpos << " : ";
                    switch ((value::valuetype) * (this_command_ptr++))
                    {
                    case value::valuetype::integer_type:
                        tmpos << "int"; break;
                    case value::valuetype::real_type:
                        tmpos << "real"; break;
                    case value::valuetype::handle_type:
                        tmpos << "handle"; break;
                    case value::valuetype::string_type:
                        tmpos << "string"; break;
                    case value::valuetype::array_type:
                        tmpos << "array"; break;
                    case value::valuetype::mapping_type:
                        tmpos << "map"; break;
                    case value::valuetype::gchandle_type:
                        tmpos << "gchandle"; break;
                    default:
                        tmpos << "unknown"; break;
                    }

                    break;

                case instruct::movx:
                    tmpos << "movx\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
                case instruct::veh:
                    tmpos << "veh ";
                    if (main_command & 0b10)
                    {
                        tmpos << "begin except jmp ";
                        tmpos << "+" << *(uint32_t*)((this_command_ptr += 4) - 4);
                        break;
                    }
                    else if (main_command & 0b01)
                    {
                        tmpos << "throw";
                        break;
                    }
                    else
                    {
                        tmpos << "ok jmp ";
                        tmpos << "+" << *(uint32_t*)((this_command_ptr += 4) - 4);
                        break;
                    }
                case instruct::abrt:
                    if (main_command & 0b10)
                        tmpos << "end\t";
                    else
                        tmpos << "abrt\t";
                    break;

                case instruct::equx:
                    tmpos << "equx\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;

                case instruct::nequx:
                    tmpos << "nequx\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;

                case instruct::equb:
                    tmpos << "equb\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;

                case instruct::nequb:
                    tmpos << "nequb\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;

                case instruct::mkarr:
                    tmpos << "mkarr\t"; print_opnum1(); tmpos << ",\t"; print_opnum2();  break;
                case instruct::mkmap:
                    tmpos << "mkmap\t"; print_opnum1(); tmpos << ",\t"; print_opnum2();  break;
                case instruct::idx:
                    tmpos << "idx\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;

                case instruct::addx:
                    if (*this_command_ptr++)
                    {
                        tmpos << "addx\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
                    }
                    else
                    {
                        tmpos << "addmovx\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
                    }
                case instruct::subx:
                    if (*this_command_ptr++)
                    {
                        tmpos << "subx\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
                    }
                    else
                    {
                        tmpos << "submovx\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
                    }
                case instruct::mulx:
                    if (*this_command_ptr++)
                    {
                        tmpos << "mulx\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
                    }
                    else
                    {
                        tmpos << "mulmovx\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
                    }
                case instruct::divx:
                    if (*this_command_ptr++)
                    {
                        tmpos << "divx\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
                    }
                    else
                    {
                        tmpos << "divmovx\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
                    }
                case instruct::modx:
                    if (*this_command_ptr++)
                    {
                        tmpos << "modx\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
                    }
                    else
                    {
                        tmpos << "modmovx\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
                    }

                case instruct::ext:
                {
                    tmpos << "ext ";
                    int pagecode = main_command & 0b00000011;
                    main_command = *(this_command_ptr++);
                    switch (pagecode)
                    {
                    case 0:
                        switch (main_command & 0b11111100)
                        {
                        case instruct::extern_opcode_page_0::setref:
                            tmpos << "setref\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
                            /*case instruct::extern_opcode_page_0::mknilarr:
                                tmpos << "mknilarr\t"; print_opnum1(); break;
                            case instruct::extern_opcode_page_0::mknilmap:
                                tmpos << "mknilmap\t"; print_opnum1();  break;*/
                        case instruct::extern_opcode_page_0::packargs:
                            tmpos << "packargs\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
                        case instruct::extern_opcode_page_0::unpackargs:
                            tmpos << "unpackargs\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
                        case instruct::extern_opcode_page_0::movdup:
                            tmpos << "movdup\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
                        default:
                            tmpos << "??\t";
                            break;
                        }
                        break;
                    case 1:
                        switch (main_command & 0b11111100)
                        {
                        case instruct::extern_opcode_page_1::endjit:
                            tmpos << "endjit\t"; break;
                        default:
                            tmpos << "??\t";
                            break;
                        }
                        break;
                    default:
                        tmpos << "??\t";
                        break;
                    }
                    break;
                }
                default:
                    tmpos << "??\t"; break;
                }

                if (ip >= program_ptr && ip < this_command_ptr)
                    os << ANSI_INV;

                print_byte();

                if (ip >= program_ptr && ip < this_command_ptr)
                    os << ANSI_RST;

                os << "| " << tmpos.str() << std::endl;



                program_ptr = this_command_ptr;
            }

            os << std::endl;
        }
        inline void dump_call_stack(size_t max_count = 32, bool need_offset = true, std::ostream& os = std::cout)const
        {
            auto* src_location_info = &env->program_debug_info->get_src_location_by_runtime_ip(ip - (need_offset ? 1 : 0));
            // NOTE: When vm running, rt_ip may point to:
            // [ -- COMMAND 6bit --] [ - DR 2bit -] [ ----- OPNUM1 ------] [ ----- OPNUM2 ------]
            //                                     ^1                     ^2                     ^3
            // If rt_ip point to place 3, 'get_current_func_signature_by_runtime_ip' will get next command's debuginfo.
            // So we do a move of 1BYTE here, for getting correct debuginfo.

            int call_trace_count = 0;

            os << call_trace_count << ": " << env->program_debug_info->get_current_func_signature_by_runtime_ip(ip - (need_offset ? 1 : 0)) << std::endl;
            os << "\t--at " << src_location_info->source_file << "(" << src_location_info->row_no << ", " << src_location_info->col_no << ")" << std::endl;

            value* base_callstackinfo_ptr = (bp + 1);
            while (base_callstackinfo_ptr <= this->stack_mem_begin)
            {
                ++call_trace_count;
                if (call_trace_count > max_count)
                {
                    os << call_trace_count << ": ..." << std::endl;
                    break;
                }
                if (base_callstackinfo_ptr->type == value::valuetype::callstack)
                {
                    src_location_info = &env->program_debug_info->get_src_location_by_runtime_ip(env->rt_codes + base_callstackinfo_ptr->ret_ip - (need_offset ? 1 : 0));

                    os << call_trace_count << ": " << env->program_debug_info->get_current_func_signature_by_runtime_ip(
                        env->rt_codes + base_callstackinfo_ptr->ret_ip - (need_offset ? 1 : 0)) << std::endl;
                    os << "\t--at " << src_location_info->source_file << "(" << src_location_info->row_no << ", " << src_location_info->col_no << ")" << std::endl;

                    base_callstackinfo_ptr = this->stack_mem_begin - base_callstackinfo_ptr->bp;
                    base_callstackinfo_ptr++;
                }
                else
                {
                    os << "??" << std::endl;
                    return;
                }
            }
        }
        inline size_t callstack_layer() const
        {
            auto* src_location_info = &env->program_debug_info->get_src_location_by_runtime_ip(ip - 1);
            // NOTE: When vm running, rt_ip may point to:
            // [ -- COMMAND 6bit --] [ - DR 2bit -] [ ----- OPNUM1 ------] [ ----- OPNUM2 ------]
            //                                     ^1                     ^2                     ^3
            // If rt_ip point to place 3, 'get_current_func_signature_by_runtime_ip' will get next command's debuginfo.
            // So we do a move of 1BYTE here, for getting correct debuginfo.

            size_t call_trace_count = 0;


            value* base_callstackinfo_ptr = (bp + 1);
            while (base_callstackinfo_ptr <= this->stack_mem_begin)
            {
                ++call_trace_count;
                if (base_callstackinfo_ptr->type == value::valuetype::callstack)
                {

                    base_callstackinfo_ptr = this->stack_mem_begin - base_callstackinfo_ptr->bp;
                    base_callstackinfo_ptr++;
                }
                else
                {
                    break;
                }
            }

            return call_trace_count;
        }

        virtual void run() = 0;

        value* co_pre_invoke(rs_int_t rs_func_addr, rs_int_t argc)
        {
            rs_assert(vm_interrupt & vm_interrupt_type::LEAVE_INTERRUPT);

            if (!rs_func_addr)
                rs_fail(RS_FAIL_CALL_FAIL, "Cannot call a 'nil' function.");
            else
            {
                auto* return_sp = sp;

                (sp--)->set_native_callstack(ip);
                ip = env->rt_codes + rs_func_addr;
                tc->set_integer(argc);
                bp = sp;

                return return_sp;
            }
            return nullptr;
        }

        value* invoke(rs_int_t rs_func_addr, rs_int_t argc)
        {
            rs_assert(vm_interrupt & vm_interrupt_type::LEAVE_INTERRUPT);

            if (!rs_func_addr)
                rs_fail(RS_FAIL_CALL_FAIL, "Cannot call a 'nil' function.");
            else
            {
                auto* return_ip = ip;
                auto* return_sp = sp + argc;
                auto* return_bp = bp;

                (sp--)->set_native_callstack(ip);
                ip = env->rt_codes + rs_func_addr;
                tc->set_integer(argc);
                bp = sp;

                run();

                ip = return_ip;
                sp = return_sp;
                bp = return_bp;
            }
            return cr;
        }
        value* invoke(rs_handle_t rs_func_addr, rs_int_t argc)
        {
            rs_assert(vm_interrupt & vm_interrupt_type::LEAVE_INTERRUPT);

            if (!rs_func_addr)
                rs_fail(RS_FAIL_CALL_FAIL, "Cannot call a 'nil' function.");
            else
            {
                auto* return_ip = ip;
                auto* return_sp = sp + argc;
                auto* return_bp = bp;

                (sp--)->set_native_callstack(ip);
                ip = env->rt_codes + rs_func_addr;
                tc->set_integer(argc);
                bp = sp;

                reinterpret_cast<rs_native_func>(rs_func_addr)(
                    reinterpret_cast<rs_vm>(this),
                    reinterpret_cast<rs_value>(sp + 2),
                    argc
                    );

                ip = return_ip;
                sp = return_sp;
                bp = return_bp;
            }
            return cr;
        }


#define RS_SAFE_READ_OFFSET_GET_QWORD (*(uint64_t*)(rt_ip-8))
#define RS_SAFE_READ_OFFSET_GET_DWORD (*(uint32_t*)(rt_ip-4))
#define RS_SAFE_READ_OFFSET_GET_WORD (*(uint16_t*)(rt_ip-2))

        // FOR BigEndian
#define RS_SAFE_READ_OFFSET_PER_BYTE(OFFSET, TYPE) (((TYPE)(*(rt_ip-OFFSET)))<<((sizeof(TYPE)-OFFSET)*8))
#define RS_IS_ODD_IRPTR(ALLIGN) 1 //(reinterpret_cast<size_t>(rt_ip)%ALLIGN)

#define RS_SAFE_READ_MOVE_2 (rt_ip+=2,RS_IS_ODD_IRPTR(2)?\
                                    (RS_SAFE_READ_OFFSET_PER_BYTE(2,uint16_t)|RS_SAFE_READ_OFFSET_PER_BYTE(1,uint16_t)):\
                                    RS_SAFE_READ_OFFSET_GET_WORD)
#define RS_SAFE_READ_MOVE_4 (rt_ip+=4,RS_IS_ODD_IRPTR(4)?\
                                    (RS_SAFE_READ_OFFSET_PER_BYTE(4,uint32_t)|RS_SAFE_READ_OFFSET_PER_BYTE(3,uint32_t)\
                                    |RS_SAFE_READ_OFFSET_PER_BYTE(2,uint32_t)|RS_SAFE_READ_OFFSET_PER_BYTE(1,uint32_t)):\
                                    RS_SAFE_READ_OFFSET_GET_DWORD)
#define RS_SAFE_READ_MOVE_8 (rt_ip+=8,RS_IS_ODD_IRPTR(8)?\
                                    (RS_SAFE_READ_OFFSET_PER_BYTE(8,uint64_t)|RS_SAFE_READ_OFFSET_PER_BYTE(7,uint64_t)|\
                                    RS_SAFE_READ_OFFSET_PER_BYTE(6,uint64_t)|RS_SAFE_READ_OFFSET_PER_BYTE(5,uint64_t)|\
                                    RS_SAFE_READ_OFFSET_PER_BYTE(4,uint64_t)|RS_SAFE_READ_OFFSET_PER_BYTE(3,uint64_t)|\
                                    RS_SAFE_READ_OFFSET_PER_BYTE(2,uint64_t)|RS_SAFE_READ_OFFSET_PER_BYTE(1,uint64_t)):\
                                    RS_SAFE_READ_OFFSET_GET_QWORD)
#define RS_IPVAL (*(rt_ip))
#define RS_IPVAL_MOVE_1 (*(rt_ip++))

            // X86 support non-alligned addressing, so just do it!

#define RS_IPVAL_MOVE_2 ((ARCH & platform_info::ArchType::X86)?(*(uint16_t*)((rt_ip += 2) - 2)):((uint16_t)RS_SAFE_READ_MOVE_2))
#define RS_IPVAL_MOVE_4 ((ARCH & platform_info::ArchType::X86)?(*(uint32_t*)((rt_ip += 4) - 4)):((uint32_t)RS_SAFE_READ_MOVE_4))
#define RS_IPVAL_MOVE_8 ((ARCH & platform_info::ArchType::X86)?(*(uint64_t*)((rt_ip += 8) - 8)):((uint64_t)RS_SAFE_READ_MOVE_8))

        template<int/* rs::platform_info::ArchType */ ARCH = rs::platform_info::ARCH_TYPE>
        bool try_invoke_jit_in_vm_run(const byte_t*& rt_ip, value* rt_bp, value* reg_begin, value* rt_cr)
        {
            std::atomic<jit_state>* state_ptr =
                (std::atomic<jit_state>*)
                (const_cast<byte_t*>(rt_ip++));

            if (*state_ptr == jit_state::NONE)
            {
                // IF jit_state::NONE, Try to be jit-generator, or just break.

                if (auto jit_state = state_ptr->exchange(jit_state::PREPARING);
                    jit_state == jit_state::NONE)
                {
                    if (auto jit_native_func = jit_compiler_x86::compile_jit(rt_ip + 8, this))
                    {
                        uint64_t storeb = (uint64_t)jit_native_func;
                        byte_t* storeb_ptr = (byte_t*)&storeb;
                        byte_t* be_store_ptr = const_cast<byte_t*>(rt_ip);
                        for (size_t i = 0; i < sizeof(uint64_t); i++)
                        {
                            static_assert(sizeof(uint64_t) == 8);
                            be_store_ptr[i] = storeb_ptr[i];
                        }

                        state_ptr->store(jit_state::READY);
                    }
                    else
                    {
                        rs_warning("JIT-Compile failed..");
                    }

                }
                else if (jit_state == jit_state::READY)
                {
                    state_ptr->store(jit_state);
                }
            }

            if (*state_ptr == jit_state::READY)
            {
                auto native_func = (jit_compiler_x86::jit_packed_function)RS_IPVAL_MOVE_8;
                native_func(this, rt_bp, reg_begin, rt_cr);
               
                return true;
            }
            else
            {
                rt_ip += 8; // skip function_addr
                return false;
            }
        }
    };

    inline exception_recovery::exception_recovery(vmbase* _vm, const byte_t* _ip, value* _sp, value* _bp)
        : vm(_vm)
        , ip(_ip)
        , sp(_sp)
        , bp(_bp)
        , last(_vm->veh)
    {
        _vm->veh = this;
    }

    inline void  exception_recovery::rollback(vmbase* _vm)
    {
        _vm->clear_interrupt(vmbase::vm_interrupt_type::LEAVE_INTERRUPT);
        if (_vm->veh)
            longjmp(_vm->veh->native_env, 1);
        else
        {
            rs_error("No 'veh' in this vm.");
        }
    }
    inline void  exception_recovery::ok(vmbase* _vm)
    {
        auto veh = _vm->veh;
        _vm->veh = veh->last;
        delete veh;
    }
    inline void exception_recovery::_ready(vmbase* _vm, const byte_t* _ip, value* _sp, value* _bp)
    {
        exception_recovery* _er = new exception_recovery(_vm, _ip, _sp, _bp);
    }


#define RS_READY_EXCEPTION_HANDLE(VM,IP,ROLLBACKIP,SP,BP)\
    bool _rs_restore_from_exception = false;\
    exception_recovery::_ready((VM),(ROLLBACKIP),(SP),(BP));\
    if (setjmp((VM)->veh->native_env))\
    {\
        _rs_restore_from_exception = true;\
        (IP) = (VM)->veh->ip;\
        (SP) = (VM)->veh->sp;\
        (BP) = (VM)->veh->bp;\
        exception_recovery::ok((VM));\
    }if(_rs_restore_from_exception)

    ////////////////////////////////////////////////////////////////////////////////

    class vm : public vmbase
    {
        vmbase* create_machine() const override
        {
            return new vm;
        }

    public:

        template<int/* rs::platform_info::ArchType */ ARCH = rs::platform_info::ARCH_TYPE>
        void run_impl()
        {
            block_interrupt(GC_INTERRUPT);

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

            struct ip_restore_raii_stack
            {
                void*& ot;
                void*& nt;

                ip_restore_raii_stack(void*& _nt, void*& _ot)
                    : ot(_ot)
                    , nt(_nt)
                {
                    _nt = _ot;
                }

                ~ip_restore_raii_stack()
                {
                    nt = ot;
                }
            };

            runtime_env* rt_env = env.get();
            const byte_t* rt_ip;
            value* rt_bp, * rt_sp;
            value* const_global_begin = rt_env->constant_global_reg_rtstack;
            value* reg_begin = register_mem_begin;
            value* const rt_cr = cr;
            value* const rt_ths = ths;

            auto_leave      _o0(this);
            ip_restore_raii _o1((void*&)rt_ip, (void*&)ip);
            ip_restore_raii _o2((void*&)rt_sp, (void*&)sp);
            ip_restore_raii _o3((void*&)rt_bp, (void*&)bp);

            vmbase* last_this_thread_vm = _this_thread_vm;
            vmbase* _nullptr = this;
            ip_restore_raii_stack _o4((void*&)_this_thread_vm, (void*&)_nullptr);
            _nullptr = last_this_thread_vm;

            rs_assert(rt_env->reg_begin == rt_env->constant_global_reg_rtstack
                + rt_env->constant_and_global_value_takeplace_count);

            rs_assert(rt_env->stack_begin == rt_env->constant_global_reg_rtstack
                + rt_env->constant_and_global_value_takeplace_count
                + rt_env->real_register_count
                + (rt_env->runtime_stack_count - 1)
            );

            if (!veh)
            {
                const byte_t* _uselessip = nullptr;
                value* _uselesssp = nullptr;
                value* _uselessbp = nullptr;

                RS_READY_EXCEPTION_HANDLE(this, _uselessip, 0, _uselesssp, _uselessbp)
                {
                    // unhandled exception happend.
                    rs_stderr << ANSI_HIR "Unexpected exception: " ANSI_RST << rs_cast_string((rs_value)er) << rs_endl;
                    dump_call_stack(32, true, std::cerr);
                    return;

                }
            }
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

#define RS_VM_FAIL(ERRNO,ERRINFO) {ip = rt_ip;sp = rt_sp;bp = rt_bp;rs_fail(ERRNO,ERRINFO);continue;}

            byte_t opcode_dr = (byte_t)(instruct::abrt << 2);
            instruct::opcode opcode = (instruct::opcode)(opcode_dr & 0b11111100u);
            unsigned dr = opcode_dr & 0b00000011u;
            try
            {
                for (;;)
                {
                    opcode_dr = *(rt_ip++);
                    opcode = (instruct::opcode)(opcode_dr & 0b11111100u);
                    dr = opcode_dr & 0b00000011u;

                    auto rtopcode = fast_ro_vm_interrupt | opcode;

                re_entry_for_interrupt:

                    switch (rtopcode)
                    {
                    case instruct::opcode::psh:
                    {
                        if (dr & 0b01)
                        {
                            RS_ADDRESSING_N1_REF;
                            (rt_sp--)->set_val(opnum1);
                        }
                        else
                        {
                            uint16_t psh_repeat = RS_IPVAL_MOVE_2;
                            for (uint32_t i = 0; i < psh_repeat; i++)
                                (rt_sp--)->set_nil();
                        }
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
                        RS_ADDRESSING_N1;
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

                        rt_cr->set_ref((opnum1->integer += opnum2->integer, opnum1));
                        break;
                    }
                    case instruct::opcode::subi:
                    {
                        RS_ADDRESSING_N1_REF;
                        RS_ADDRESSING_N2_REF;

                        rs_assert(opnum1->type == opnum2->type
                            && opnum1->type == value::valuetype::integer_type);

                        rt_cr->set_ref((opnum1->integer -= opnum2->integer, opnum1));
                        break;
                    }
                    case instruct::opcode::muli:
                    {
                        RS_ADDRESSING_N1_REF;
                        RS_ADDRESSING_N2_REF;

                        rs_assert(opnum1->type == opnum2->type
                            && opnum1->type == value::valuetype::integer_type);

                        rt_cr->set_ref((opnum1->integer *= opnum2->integer, opnum1));
                        break;
                    }
                    case instruct::opcode::divi:
                    {
                        RS_ADDRESSING_N1_REF;
                        RS_ADDRESSING_N2_REF;

                        rs_assert(opnum1->type == opnum2->type
                            && opnum1->type == value::valuetype::integer_type);

                        rt_cr->set_ref((opnum1->integer /= opnum2->integer, opnum1));
                        break;
                    }
                    case instruct::opcode::modi:
                    {
                        RS_ADDRESSING_N1_REF;
                        RS_ADDRESSING_N2_REF;

                        rs_assert(opnum1->type == opnum2->type
                            && opnum1->type == value::valuetype::integer_type);

                        rt_cr->set_ref((opnum1->integer %= opnum2->integer, opnum1));
                        break;
                    }

                    case instruct::opcode::addr:
                    {
                        RS_ADDRESSING_N1_REF;
                        RS_ADDRESSING_N2_REF;

                        rs_assert(opnum1->type == opnum2->type
                            && opnum1->type == value::valuetype::real_type);

                        rt_cr->set_ref((opnum1->real += opnum2->real, opnum1));
                        break;
                    }
                    case instruct::opcode::subr:
                    {
                        RS_ADDRESSING_N1_REF;
                        RS_ADDRESSING_N2_REF;

                        rs_assert(opnum1->type == opnum2->type
                            && opnum1->type == value::valuetype::real_type);

                        rt_cr->set_ref((opnum1->real -= opnum2->real, opnum1));
                        break;
                    }
                    case instruct::opcode::mulr:
                    {
                        RS_ADDRESSING_N1_REF;
                        RS_ADDRESSING_N2_REF;

                        rs_assert(opnum1->type == opnum2->type
                            && opnum1->type == value::valuetype::real_type);

                        rt_cr->set_ref((opnum1->real *= opnum2->real, opnum1));
                        break;
                    }
                    case instruct::opcode::divr:
                    {
                        RS_ADDRESSING_N1_REF;
                        RS_ADDRESSING_N2_REF;

                        rs_assert(opnum1->type == opnum2->type
                            && opnum1->type == value::valuetype::real_type);

                        rt_cr->set_ref((opnum1->real /= opnum2->real, opnum1));
                        break;
                    }
                    case instruct::opcode::modr:
                    {
                        RS_ADDRESSING_N1_REF;
                        RS_ADDRESSING_N2_REF;

                        rs_assert(opnum1->type == opnum2->type
                            && opnum1->type == value::valuetype::real_type);

                        rt_cr->set_ref((opnum1->real = fmod(opnum1->real, opnum2->real), opnum1));
                        break;
                    }

                    case instruct::opcode::addh:
                    {
                        RS_ADDRESSING_N1_REF;
                        RS_ADDRESSING_N2_REF;

                        rs_assert(opnum1->type == opnum2->type
                            && opnum1->type == value::valuetype::handle_type);

                        rt_cr->set_ref((opnum1->handle += opnum2->handle, opnum1));
                        break;
                    }
                    case instruct::opcode::subh:
                    {
                        RS_ADDRESSING_N1_REF;
                        RS_ADDRESSING_N2_REF;

                        rs_assert(opnum1->type == opnum2->type
                            && opnum1->type == value::valuetype::handle_type);

                        rt_cr->set_ref((opnum1->handle -= opnum2->handle, opnum1));
                        break;
                    }

                    case instruct::opcode::adds:
                    {
                        RS_ADDRESSING_N1_REF;
                        RS_ADDRESSING_N2_REF;

                        rs_assert(opnum1->type == opnum2->type
                            && opnum1->type == value::valuetype::string_type);

                        rt_cr->set_ref((string_t::gc_new<gcbase::gctype::eden>(opnum1->gcunit, *opnum1->string + *opnum2->string), opnum1));
                        break;
                    }

                    case instruct::opcode::addx:
                    {
                        auto change_type_sign = RS_IPVAL_MOVE_1;

                        RS_ADDRESSING_N1_REF;
                        RS_ADDRESSING_N2_REF;

                        value::valuetype max_type = change_type_sign ?
                            std::max(opnum1->type, opnum2->type) : opnum1->type;

                        if (opnum1->type != max_type)
                        {
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
                                    RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Mismatch type for operating."); break;
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
                                    RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Mismatch type for operating."); break;
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
                                    RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Mismatch type for operating."); break;
                                }
                                break;
                            default:
                                RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Mismatch type for operating."); break;
                            }
                        }
                        opnum1->type = max_type;
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
                            case value::valuetype::array_type:
                            {
                                if (opnum1->array)
                                {
                                    // WARNING: MAY CAUSE DEAD LOCK
                                    gcbase::gc_read_guard mx(opnum1->array);
                                    array_t::gc_new<gcbase::gctype::eden>(opnum1->gcunit, *opnum1->array);
                                    if (opnum2->array)
                                    {
                                        gcbase::gc_read_guard mx(opnum2->array);
                                        opnum1->array->insert(opnum1->array->end(),
                                            opnum2->array->begin(),
                                            opnum2->array->end());
                                    }
                                    else
                                        RS_VM_FAIL(RS_FAIL_ACCESS_NIL, "Trying to access is 'nil'."); break;
                                }
                                else
                                    RS_VM_FAIL(RS_FAIL_ACCESS_NIL, "Trying to access is 'nil'."); break;
                                break;
                            }
                            case value::valuetype::mapping_type:
                            {
                                if (opnum1->mapping)
                                {
                                    // WARNING: MAY CAUSE DEAD LOCK
                                    gcbase::gc_read_guard mx(opnum1->mapping);
                                    mapping_t::gc_new<gcbase::gctype::eden>(opnum1->gcunit, *opnum1->mapping);
                                    if (opnum2->mapping)
                                    {
                                        gcbase::gc_read_guard mx(opnum2->mapping);
                                        opnum1->mapping->insert(opnum2->mapping->begin(),
                                            opnum2->mapping->end());
                                    }
                                }
                                break;
                            }
                            default:
                                RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Mismatch type for operating."); break;
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
                                    RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Mismatch type for operating."); break;
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
                                    RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Mismatch type for operating."); break;
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
                                    RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Mismatch type for operating."); break;
                                }
                                break;
                            default:
                                RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Mismatch type for operating."); break;
                            }
                        }

                        rt_cr->set_ref(opnum1);
                        break;
                    }

                    case instruct::opcode::subx:
                    {
                        auto change_type_sign = RS_IPVAL_MOVE_1;

                        RS_ADDRESSING_N1_REF;
                        RS_ADDRESSING_N2_REF;

                        value::valuetype max_type = change_type_sign ?
                            std::max(opnum1->type, opnum2->type) : opnum1->type;

                        if (opnum1->type != max_type)
                        {
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
                                    RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Mismatch type for operating."); break;
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
                                    RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Mismatch type for operating."); break;
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
                                    RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Mismatch type for operating."); break;
                                }
                                break;
                            default:
                                RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Mismatch type for operating."); break;
                            }
                        }
                        opnum1->type = max_type;
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
                                RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Mismatch type for operating."); break;
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
                                    RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Mismatch type for operating."); break;
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
                                    RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Mismatch type for operating."); break;
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
                                    RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Mismatch type for operating."); break;
                                }
                                break;
                            default:
                                RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Mismatch type for operating."); break;
                            }
                        }

                        rt_cr->set_ref(opnum1);
                        break;
                    }

                    case instruct::opcode::mulx:
                    {
                        auto change_type_sign = RS_IPVAL_MOVE_1;

                        RS_ADDRESSING_N1_REF;
                        RS_ADDRESSING_N2_REF;

                        value::valuetype max_type = change_type_sign ?
                            std::max(opnum1->type, opnum2->type) : opnum1->type;

                        if (opnum1->type != max_type)
                        {
                            switch (max_type)
                            {
                            case rs::value::valuetype::integer_type:
                                switch (opnum1->type)
                                {
                                case value::valuetype::real_type:
                                    opnum1->integer = (rs_integer_t)opnum1->real; break;
                                default:
                                    RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Mismatch type for operating."); break;
                                }
                                break;
                            case rs::value::valuetype::real_type:
                                switch (opnum1->type)
                                {
                                case value::valuetype::integer_type:
                                    opnum1->real = (rs_real_t)opnum1->integer; break;
                                default:
                                    RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Mismatch type for operating."); break;
                                }
                                break;
                            default:
                                RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Mismatch type for operating."); break;
                            }
                        }
                        opnum1->type = max_type;
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
                                RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Mismatch type for operating."); break;
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
                                    RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Mismatch type for operating."); break;
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
                                    RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Mismatch type for operating."); break;
                                }
                                break;
                            default:
                                RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Mismatch type for operating."); break;
                            }
                        }

                        rt_cr->set_ref(opnum1);
                        break;
                    }

                    case instruct::opcode::divx:
                    {
                        auto change_type_sign = RS_IPVAL_MOVE_1;

                        RS_ADDRESSING_N1_REF;
                        RS_ADDRESSING_N2_REF;

                        value::valuetype max_type = change_type_sign ?
                            std::max(opnum1->type, opnum2->type) : opnum1->type;

                        if (opnum1->type != max_type)
                        {

                            switch (max_type)
                            {
                            case rs::value::valuetype::integer_type:
                                switch (opnum1->type)
                                {
                                case value::valuetype::real_type:
                                    opnum1->integer = (rs_integer_t)opnum1->real; break;
                                default:
                                    RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Mismatch type for operating."); break;
                                }
                                break;
                            case rs::value::valuetype::real_type:
                                switch (opnum1->type)
                                {
                                case value::valuetype::integer_type:
                                    opnum1->real = (rs_real_t)opnum1->integer; break;
                                default:
                                    RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Mismatch type for operating."); break;
                                }
                                break;
                            default:
                                RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Mismatch type for operating."); break;
                            }
                        }
                        opnum1->type = max_type;
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
                                RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Mismatch type for operating."); break;
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
                                    RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Mismatch type for operating."); break;
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
                                    RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Mismatch type for operating."); break;
                                }
                                break;
                            default:
                                RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Mismatch type for operating."); break;
                            }
                        }

                        rt_cr->set_ref(opnum1);
                        break;
                    }

                    case instruct::opcode::modx:
                    {
                        auto change_type_sign = RS_IPVAL_MOVE_1;

                        RS_ADDRESSING_N1_REF;
                        RS_ADDRESSING_N2_REF;

                        value::valuetype max_type = change_type_sign ?
                            std::max(opnum1->type, opnum2->type) : opnum1->type;

                        if (opnum1->type != max_type)
                        {

                            switch (max_type)
                            {
                            case rs::value::valuetype::integer_type:
                                switch (opnum1->type)
                                {
                                case value::valuetype::real_type:
                                    opnum1->integer = (rs_integer_t)opnum1->real; break;
                                default:
                                    RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Mismatch type for operating."); break;
                                }
                                break;
                            case rs::value::valuetype::real_type:
                                switch (opnum1->type)
                                {
                                case value::valuetype::integer_type:
                                    opnum1->real = (rs_real_t)opnum1->integer; break;
                                default:
                                    RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Mismatch type for operating."); break;
                                }
                                break;
                            default:
                                RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Mismatch type for operating."); break;
                            }
                        }
                        opnum1->type = max_type;
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
                                RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Mismatch type for operating."); break;
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
                                    RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Mismatch type for operating."); break;
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
                                    RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Mismatch type for operating."); break;
                                }
                                break;
                            default:
                                RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Mismatch type for operating."); break;
                            }
                        }

                        rt_cr->set_ref(opnum1);
                        break;
                    }

                    /// OPERATE


                    case instruct::opcode::set:
                    {
                        RS_ADDRESSING_N1;
                        RS_ADDRESSING_N2_REF;

                        rt_cr->set_ref(opnum1->set_val(opnum2));
                        break;
                    }
                    case instruct::opcode::mov:
                    {
                        RS_ADDRESSING_N1_REF;
                        RS_ADDRESSING_N2_REF;

                        rt_cr->set_ref(opnum1->set_val(opnum2));
                        break;
                    }
                    case instruct::opcode::movx:
                    {
                        RS_ADDRESSING_N1_REF;
                        RS_ADDRESSING_N2_REF;

                        if (opnum1->type == opnum2->type)
                            opnum1->handle = opnum2->handle;  // Has same type, just move all data.
                        else
                        {
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
                                    RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Type mismatch between two opnum.");
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
                                    RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Type mismatch between two opnum.");
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
                                    RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Type mismatch between two opnum.");
                                    break;
                                }break;
                            }
                            case value::valuetype::array_type:
                            case value::valuetype::mapping_type:
                            case value::valuetype::gchandle_type:
                                if (opnum2->type == value::valuetype::invalid)
                                {
                                    rs_assert(opnum2->handle == 0);

                                    opnum1->set_nil();
                                    break;
                                }
                                /* fall through~~~ */
                            default:
                                RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Type mismatch between two opnum.");
                                break;
                            }
                        }
                        rt_cr->set_ref(opnum1);
                        break;
                    }
                    case instruct::opcode::movcast:
                    {
                        RS_ADDRESSING_N1_REF;
                        RS_ADDRESSING_N2_REF;

                        value::valuetype aim_type = static_cast<value::valuetype>(RS_IPVAL_MOVE_1);
                        if (aim_type == opnum2->type)
                            rt_cr->set_ref(opnum1->set_val(opnum2));
                        else
                            switch (aim_type)
                            {
                            case value::valuetype::integer_type:
                                switch (opnum2->type)
                                {
                                case value::valuetype::real_type:
                                    rt_cr->set_ref(opnum1->set_integer((rs_integer_t)opnum2->real)); break;
                                case value::valuetype::handle_type:
                                    rt_cr->set_ref(opnum1->set_integer((rs_integer_t)opnum2->handle)); break;
                                case value::valuetype::string_type:
                                    rt_cr->set_ref(opnum1->set_integer((rs_integer_t)std::stoll(*opnum2->string))); break;
                                default:
                                    RS_VM_FAIL(RS_FAIL_TYPE_FAIL, ("Cannot cast '" + opnum2->get_type_name() + "' to 'integer'.").c_str());
                                    break;
                                }
                                break;
                            case value::valuetype::real_type:
                                switch (opnum2->type)
                                {
                                case value::valuetype::integer_type:
                                    rt_cr->set_ref(opnum1->set_real((rs_real_t)opnum2->integer)); break;
                                case value::valuetype::handle_type:
                                    rt_cr->set_ref(opnum1->set_real((rs_real_t)opnum2->handle)); break;
                                case value::valuetype::string_type:
                                    rt_cr->set_ref(opnum1->set_real((rs_real_t)std::stod(*opnum2->string))); break;
                                default:
                                    RS_VM_FAIL(RS_FAIL_TYPE_FAIL, ("Cannot cast '" + opnum2->get_type_name() + "' to 'real'.").c_str());
                                    break;
                                }
                                break;
                            case value::valuetype::handle_type:
                                switch (opnum2->type)
                                {
                                case value::valuetype::integer_type:
                                    rt_cr->set_ref(opnum1->set_handle((rs_handle_t)opnum2->integer)); break;
                                case value::valuetype::real_type:
                                    rt_cr->set_ref(opnum1->set_handle((rs_handle_t)opnum2->real)); break;
                                case value::valuetype::string_type:
                                    rt_cr->set_ref(opnum1->set_handle((rs_handle_t)std::stoull(*opnum2->string))); break;
                                default:
                                    RS_VM_FAIL(RS_FAIL_TYPE_FAIL, ("Cannot cast '" + opnum2->get_type_name() + "' to 'handle'.").c_str());
                                    break;
                                }
                                break;
                            case value::valuetype::string_type:
                                rt_cr->set_ref(opnum1->set_string(rs_cast_string(reinterpret_cast<rs_value>(opnum2)))); break;

                            case value::valuetype::array_type:
                                if (opnum2->is_nil())
                                    rt_cr->set_ref(opnum1->set_nil());
                                else
                                    RS_VM_FAIL(RS_FAIL_TYPE_FAIL, ("Cannot cast '" + opnum2->get_type_name() + "' to 'array'.").c_str());
                                break;
                            case value::valuetype::mapping_type:
                                if (opnum2->is_nil())
                                    rt_cr->set_ref(opnum1->set_nil());
                                else
                                    RS_VM_FAIL(RS_FAIL_TYPE_FAIL, ("Cannot cast '" + opnum2->get_type_name() + "' to 'map'.").c_str());
                                break;
                            case value::valuetype::gchandle_type:
                                if (opnum2->is_nil())
                                    rt_cr->set_ref(opnum1->set_nil());
                                else
                                    RS_VM_FAIL(RS_FAIL_TYPE_FAIL, ("Cannot cast '" + opnum2->get_type_name() + "' to 'gchandle'.").c_str());
                                break;
                            default:
                                rs_error("Unknown type.");
                            }

                        break;
                    }
                    case instruct::opcode::setcast:
                    {
                        RS_ADDRESSING_N1;
                        RS_ADDRESSING_N2_REF;

                        value::valuetype aim_type = static_cast<value::valuetype>(RS_IPVAL_MOVE_1);
                        if (aim_type == opnum2->type)
                            rt_cr->set_ref(opnum1->set_val(opnum2));
                        else
                            switch (aim_type)
                            {
                            case value::valuetype::integer_type:
                                switch (opnum2->type)
                                {
                                case value::valuetype::real_type:
                                    rt_cr->set_ref(opnum1->set_integer((rs_integer_t)opnum2->real)); break;
                                case value::valuetype::handle_type:
                                    rt_cr->set_ref(opnum1->set_integer((rs_integer_t)opnum2->handle)); break;
                                case value::valuetype::string_type:
                                    rt_cr->set_ref(opnum1->set_integer((rs_integer_t)std::stoll(*opnum2->string))); break;
                                default:
                                    RS_VM_FAIL(RS_FAIL_TYPE_FAIL, ("Cannot cast '" + opnum2->get_type_name() + "' to 'integer'.").c_str());
                                    break;
                                }
                                break;
                            case value::valuetype::real_type:
                                switch (opnum2->type)
                                {
                                case value::valuetype::integer_type:
                                    rt_cr->set_ref(opnum1->set_real((rs_real_t)opnum2->integer)); break;
                                case value::valuetype::handle_type:
                                    rt_cr->set_ref(opnum1->set_real((rs_real_t)opnum2->handle)); break;
                                case value::valuetype::string_type:
                                    rt_cr->set_ref(opnum1->set_real((rs_real_t)std::stod(*opnum2->string))); break;
                                default:
                                    RS_VM_FAIL(RS_FAIL_TYPE_FAIL, ("Cannot cast '" + opnum2->get_type_name() + "' to 'real'.").c_str());
                                    break;
                                }
                                break;
                            case value::valuetype::handle_type:
                                switch (opnum2->type)
                                {
                                case value::valuetype::integer_type:
                                    rt_cr->set_ref(opnum1->set_handle((rs_handle_t)opnum2->integer)); break;
                                case value::valuetype::real_type:
                                    rt_cr->set_ref(opnum1->set_handle((rs_handle_t)opnum2->real)); break;
                                case value::valuetype::string_type:
                                    rt_cr->set_ref(opnum1->set_handle((rs_handle_t)std::stoull(*opnum2->string))); break;
                                default:
                                    RS_VM_FAIL(RS_FAIL_TYPE_FAIL, ("Cannot cast '" + opnum2->get_type_name() + "' to 'handle'.").c_str());
                                    break;
                                }
                                break;
                            case value::valuetype::string_type:
                                rt_cr->set_ref(opnum1->set_string(rs_cast_string(reinterpret_cast<rs_value>(opnum2)))); break;

                            case value::valuetype::array_type:
                                if (opnum2->is_nil())
                                    rt_cr->set_ref(opnum1->set_nil());
                                else
                                    RS_VM_FAIL(RS_FAIL_TYPE_FAIL, ("Cannot cast '" + opnum2->get_type_name() + "' to 'array'.").c_str());
                                break;
                            case value::valuetype::mapping_type:
                                if (opnum2->is_nil())
                                    rt_cr->set_ref(opnum1->set_nil());
                                else
                                    RS_VM_FAIL(RS_FAIL_TYPE_FAIL, ("Cannot cast '" + opnum2->get_type_name() + "' to 'map'.").c_str());
                                break;
                            case value::valuetype::gchandle_type:
                                if (opnum2->is_nil())
                                    rt_cr->set_ref(opnum1->set_nil());
                                else
                                    RS_VM_FAIL(RS_FAIL_TYPE_FAIL, ("Cannot cast '" + opnum2->get_type_name() + "' to 'gchandle'.").c_str());
                                break;
                            default:
                                rs_error("Unknown type.");
                            }

                        break;
                    }
                    case instruct::opcode::typeas:
                    {
                        RS_ADDRESSING_N1_REF;
                        if (dr & 0b01)
                        {
                            rt_cr->type = value::valuetype::integer_type;
                            if (opnum1->type != (value::valuetype)(RS_IPVAL_MOVE_1))
                                rt_cr->integer = 0;
                            else
                                rt_cr->integer = 1;
                        }
                        else
                            if (opnum1->type != (value::valuetype)(RS_IPVAL_MOVE_1))
                                RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "The given value is not the same as the requested type.");
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
                        RS_ADDRESSING_N1;
                        RS_ADDRESSING_N2_REF;

                        rs_assert(opnum2->type == value::valuetype::integer_type);
                        opnum1->set_ref((rt_bp + opnum2->integer)->get());
                        break;
                    }
                    case instruct::opcode::equb:
                    {
                        RS_ADDRESSING_N1_REF;
                        RS_ADDRESSING_N2_REF;

                        rt_cr->type = value::valuetype::integer_type;
                        rt_cr->integer = opnum1->integer == opnum2->integer;

                        break;
                    }
                    case instruct::opcode::nequb:
                    {
                        RS_ADDRESSING_N1_REF;
                        RS_ADDRESSING_N2_REF;

                        rt_cr->type = value::valuetype::integer_type;
                        rt_cr->integer = opnum1->integer != opnum2->integer;
                        break;
                    }
                    case instruct::opcode::equx:
                    {
                        RS_ADDRESSING_N1_REF;
                        RS_ADDRESSING_N2_REF;

                        rt_cr->type = value::valuetype::integer_type;
                        if (opnum1->type == opnum2->type)
                        {
                            switch (opnum1->type)
                            {
                            case value::valuetype::integer_type:
                                rt_cr->integer = opnum1->integer == opnum2->integer; break;
                            case value::valuetype::handle_type:
                                rt_cr->integer = opnum1->handle == opnum2->handle; break;
                            case value::valuetype::real_type:
                                rt_cr->integer = opnum1->real == opnum2->real; break;
                            case value::valuetype::string_type:
                                rt_cr->integer = *opnum1->string == *opnum2->string; break;

                            case value::valuetype::mapping_type:
                            case value::valuetype::array_type:
                                rt_cr->integer = opnum1->gcunit == opnum2->gcunit; break;
                            default:
                                RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Values of this type cannot be compared.");
                                rt_cr->integer = 0;
                                break;
                            }
                        }
                        else if (opnum1->type == value::valuetype::integer_type
                            && opnum2->type == value::valuetype::real_type)
                        {
                            rt_cr->integer = (rs_real_t)opnum1->integer == opnum2->real;
                        }
                        else if (opnum1->type == value::valuetype::real_type
                            && opnum2->type == value::valuetype::integer_type)
                        {
                            rt_cr->integer = opnum1->real == (rs_real_t)opnum2->integer;
                        }
                        else
                            rt_cr->integer = opnum1->is_nil() && opnum2->is_nil();
                        break;

                    }
                    case instruct::opcode::nequx:
                    {
                        RS_ADDRESSING_N1_REF;
                        RS_ADDRESSING_N2_REF;

                        rt_cr->type = value::valuetype::integer_type;
                        if (opnum1->type == opnum2->type)
                        {
                            switch (opnum1->type)
                            {
                            case value::valuetype::integer_type:
                                rt_cr->integer = opnum1->integer != opnum2->integer; break;
                            case value::valuetype::handle_type:
                                rt_cr->integer = opnum1->handle != opnum2->handle; break;
                            case value::valuetype::real_type:
                                rt_cr->integer = opnum1->real != opnum2->real; break;
                            case value::valuetype::string_type:
                                rt_cr->integer = *opnum1->string != *opnum2->string; break;

                            case value::valuetype::mapping_type:
                            case value::valuetype::array_type:
                                rt_cr->integer = opnum1->gcunit != opnum2->gcunit; break;
                            default:
                                RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Values of this type cannot be compared.");
                                rt_cr->integer = 1;
                                break;
                            }
                        }
                        else if (opnum1->type == value::valuetype::integer_type
                            && opnum2->type == value::valuetype::real_type)
                        {
                            rt_cr->integer = (rs_real_t)opnum1->integer != opnum2->real;
                        }
                        else if (opnum1->type == value::valuetype::real_type
                            && opnum2->type == value::valuetype::integer_type)
                        {
                            rt_cr->integer = opnum1->real != (rs_real_t)opnum2->integer;
                        }
                        else
                            rt_cr->integer = !(opnum1->is_nil() && opnum2->is_nil());
                        break;
                    }
                    case instruct::opcode::land:
                    {
                        RS_ADDRESSING_N1_REF;
                        RS_ADDRESSING_N2_REF;

                        rt_cr->type = value::valuetype::integer_type;
                        rt_cr->integer = opnum1->integer && opnum2->integer;

                        break;
                    }
                    case instruct::opcode::lor:
                    {
                        RS_ADDRESSING_N1_REF;
                        RS_ADDRESSING_N2_REF;

                        rt_cr->type = value::valuetype::integer_type;
                        rt_cr->integer = opnum1->integer || opnum2->integer;

                        break;
                    }
                    case instruct::opcode::lnot:
                    {
                        RS_ADDRESSING_N1_REF;

                        rt_cr->type = value::valuetype::integer_type;
                        rt_cr->integer = !opnum1->integer;

                        break;
                    }
                    case instruct::opcode::lti:
                    {
                        RS_ADDRESSING_N1_REF;
                        RS_ADDRESSING_N2_REF;

                        rs_assert(opnum1->type == opnum2->type
                            && opnum1->type == value::valuetype::integer_type);

                        rt_cr->type = value::valuetype::integer_type;
                        rt_cr->integer = opnum1->integer < opnum2->integer;


                        break;
                    }
                    case instruct::opcode::gti:
                    {
                        RS_ADDRESSING_N1_REF;
                        RS_ADDRESSING_N2_REF;

                        rs_assert(opnum1->type == opnum2->type
                            && opnum1->type == value::valuetype::integer_type);

                        rt_cr->type = value::valuetype::integer_type;
                        rt_cr->integer = opnum1->integer > opnum2->integer;


                        break;
                    }
                    case instruct::opcode::elti:
                    {
                        RS_ADDRESSING_N1_REF;
                        RS_ADDRESSING_N2_REF;

                        rs_assert(opnum1->type == opnum2->type
                            && opnum1->type == value::valuetype::integer_type);

                        rt_cr->type = value::valuetype::integer_type;
                        rt_cr->integer = opnum1->integer <= opnum2->integer;


                        break;
                    }
                    case instruct::opcode::egti:
                    {
                        RS_ADDRESSING_N1_REF;
                        RS_ADDRESSING_N2_REF;

                        rs_assert(opnum1->type == opnum2->type
                            && opnum1->type == value::valuetype::integer_type);

                        rt_cr->type = value::valuetype::integer_type;
                        rt_cr->integer = opnum1->integer >= opnum2->integer;


                        break;
                    }
                    case instruct::opcode::ltr:
                    {
                        RS_ADDRESSING_N1_REF;
                        RS_ADDRESSING_N2_REF;

                        rs_assert(opnum1->type == opnum2->type
                            && opnum1->type == value::valuetype::real_type);

                        rt_cr->type = value::valuetype::integer_type;
                        rt_cr->integer = opnum1->real < opnum2->real;


                        break;
                    }
                    case instruct::opcode::gtr:
                    {
                        RS_ADDRESSING_N1_REF;
                        RS_ADDRESSING_N2_REF;

                        rs_assert(opnum1->type == opnum2->type
                            && opnum1->type == value::valuetype::real_type);

                        rt_cr->type = value::valuetype::integer_type;
                        rt_cr->integer = opnum1->real > opnum2->real;


                        break;
                    }
                    case instruct::opcode::eltr:
                    {
                        RS_ADDRESSING_N1_REF;
                        RS_ADDRESSING_N2_REF;

                        rs_assert(opnum1->type == opnum2->type
                            && opnum1->type == value::valuetype::real_type);

                        rt_cr->type = value::valuetype::integer_type;
                        rt_cr->integer = opnum1->real <= opnum2->real;


                        break;
                    }
                    case instruct::opcode::egtr:
                    {
                        RS_ADDRESSING_N1_REF;
                        RS_ADDRESSING_N2_REF;

                        rs_assert(opnum1->type == opnum2->type
                            && opnum1->type == value::valuetype::real_type);

                        rt_cr->type = value::valuetype::integer_type;
                        rt_cr->integer = opnum1->real >= opnum2->real;


                        break;
                    }
                    case instruct::opcode::ltx:
                    {
                        RS_ADDRESSING_N1_REF;
                        RS_ADDRESSING_N2_REF;

                        rt_cr->type = value::valuetype::integer_type;
                        if (opnum1->type == opnum2->type)
                        {
                            switch (opnum1->type)
                            {
                            case value::valuetype::integer_type:
                                rt_cr->integer = opnum1->integer < opnum2->integer; break;
                            case value::valuetype::handle_type:
                                rt_cr->integer = opnum1->handle < opnum2->handle; break;
                            case value::valuetype::real_type:
                                rt_cr->integer = opnum1->real < opnum2->real; break;
                            case value::valuetype::string_type:
                                rt_cr->integer = *opnum1->string < *opnum2->string; break;
                            default:
                                RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Values of this type cannot be compared.");
                                rt_cr->integer = 0;
                                break;
                            }
                        }
                        else if (opnum1->type == value::valuetype::integer_type
                            && opnum2->type == value::valuetype::real_type)
                        {
                            rt_cr->integer = (rs_real_t)opnum1->integer < opnum2->real;
                        }
                        else if (opnum1->type == value::valuetype::real_type
                            && opnum2->type == value::valuetype::integer_type)
                        {
                            rt_cr->integer = opnum1->real < (rs_real_t)opnum2->integer;
                        }
                        else
                            rt_cr->integer = opnum1->type < opnum2->type;

                        break;
                    }
                    case instruct::opcode::gtx:
                    {
                        RS_ADDRESSING_N1_REF;
                        RS_ADDRESSING_N2_REF;

                        rt_cr->type = value::valuetype::integer_type;
                        if (opnum1->type == opnum2->type)
                        {
                            switch (opnum1->type)
                            {
                            case value::valuetype::integer_type:
                                rt_cr->integer = opnum1->integer > opnum2->integer; break;
                            case value::valuetype::handle_type:
                                rt_cr->integer = opnum1->handle > opnum2->handle; break;
                            case value::valuetype::real_type:
                                rt_cr->integer = opnum1->real > opnum2->real; break;
                            case value::valuetype::string_type:
                                rt_cr->integer = *opnum1->string > *opnum2->string; break;
                            default:
                                RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Values of this type cannot be compared.");
                                rt_cr->integer = 0;
                                break;
                            }
                        }
                        else if (opnum1->type == value::valuetype::integer_type
                            && opnum2->type == value::valuetype::real_type)
                        {
                            rt_cr->integer = (rs_real_t)opnum1->integer > opnum2->real;
                        }
                        else if (opnum1->type == value::valuetype::real_type
                            && opnum2->type == value::valuetype::integer_type)
                        {
                            rt_cr->integer = opnum1->real > (rs_real_t)opnum2->integer;
                        }
                        else
                            rt_cr->integer = opnum1->type > opnum2->type;
                        break;
                    }
                    case instruct::opcode::eltx:
                    {
                        RS_ADDRESSING_N1_REF;
                        RS_ADDRESSING_N2_REF;

                        rt_cr->type = value::valuetype::integer_type;
                        if (opnum1->type == opnum2->type)
                        {
                            switch (opnum1->type)
                            {
                            case value::valuetype::integer_type:
                                rt_cr->integer = opnum1->integer <= opnum2->integer; break;
                            case value::valuetype::handle_type:
                                rt_cr->integer = opnum1->handle <= opnum2->handle; break;
                            case value::valuetype::real_type:
                                rt_cr->integer = opnum1->real <= opnum2->real; break;
                            case value::valuetype::string_type:
                                rt_cr->integer = *opnum1->string <= *opnum2->string; break;
                            default:
                                RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Values of this type cannot be compared.");
                                rt_cr->integer = 0;
                                break;
                            }
                        }
                        else if (opnum1->type == value::valuetype::integer_type
                            && opnum2->type == value::valuetype::real_type)
                        {
                            rt_cr->integer = (rs_real_t)opnum1->integer <= opnum2->real;
                        }
                        else if (opnum1->type == value::valuetype::real_type
                            && opnum2->type == value::valuetype::integer_type)
                        {
                            rt_cr->integer = opnum1->real <= (rs_real_t)opnum2->integer;
                        }
                        else
                            rt_cr->integer = opnum1->type <= opnum2->type;

                        break;
                    }
                    case instruct::opcode::egtx:
                    {
                        RS_ADDRESSING_N1_REF;
                        RS_ADDRESSING_N2_REF;

                        rt_cr->type = value::valuetype::integer_type;
                        if (opnum1->type == opnum2->type)
                        {
                            switch (opnum1->type)
                            {
                            case value::valuetype::integer_type:
                                rt_cr->integer = opnum1->integer >= opnum2->integer; break;
                            case value::valuetype::handle_type:
                                rt_cr->integer = opnum1->handle >= opnum2->handle; break;
                            case value::valuetype::real_type:
                                rt_cr->integer = opnum1->real >= opnum2->real; break;
                            case value::valuetype::string_type:
                                rt_cr->integer = *opnum1->string >= *opnum2->string; break;
                            default:
                                RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Values of this type cannot be compared.");
                                rt_cr->integer = 0;
                                break;
                            }
                        }
                        else if (opnum1->type == value::valuetype::integer_type
                            && opnum2->type == value::valuetype::real_type)
                        {
                            rt_cr->integer = (rs_real_t)opnum1->integer >= opnum2->real;
                        }
                        else if (opnum1->type == value::valuetype::real_type
                            && opnum2->type == value::valuetype::integer_type)
                        {
                            rt_cr->integer = opnum1->real >= (rs_real_t)opnum2->integer;
                        }
                        else
                            rt_cr->integer = opnum1->type >= opnum2->type;
                        break;
                    }
                    case instruct::opcode::calljit:
                    {
                        if (!try_invoke_jit_in_vm_run(rt_ip, rt_bp, reg_begin, rt_cr))
                            break;

                        // If Call Jit success, just fall throw to ret..
                    }
                    case instruct::opcode::ret:
                    {
                        // NOTE : RET_VAL?
                        /*if (dr)
                            rt_cr->set_val(rt_cr->get());*/

                        rs_assert((rt_bp + 1)->type == value::valuetype::callstack
                            || (rt_bp + 1)->type == value::valuetype::nativecallstack);

                        if ((++rt_bp)->type == value::valuetype::nativecallstack)
                        {
                            rt_sp = rt_bp;
                            return; // last stack is native_func, just do return; stack balance should be keeped by invoker
                        }

                        value* stored_bp = stack_mem_begin - rt_bp->bp;
                        rt_ip = rt_env->rt_codes + rt_bp->ret_ip;
                        rt_sp = rt_bp;
                        rt_bp = stored_bp;

                        // TODO If rt_ip is outof range, return...

                        break;
                    }
                    case instruct::opcode::call:
                    {
                        RS_ADDRESSING_N1_REF;

                        if (!opnum1->handle)
                        {
                            RS_VM_FAIL(RS_FAIL_CALL_FAIL, "Cannot call a 'nil' function.");
                            break;
                        }

                        rt_sp->type = value::valuetype::callstack;
                        rt_sp->ret_ip = (uint32_t)(rt_ip - rt_env->rt_codes);
                        rt_sp->bp = (uint32_t)(stack_mem_begin - rt_bp);
                        rt_bp = --rt_sp;

                        if (opnum1->type == value::valuetype::handle_type)
                        {
                            // Call native
                            bp = sp = rt_sp;
                            rs_extern_native_func_t call_aim_native_func = (rs_extern_native_func_t)(opnum1->handle);
                            ip = reinterpret_cast<byte_t*>(call_aim_native_func);
                            rt_cr->set_nil();

                            rs_asure(interrupt(vm_interrupt_type::LEAVE_INTERRUPT));
                            call_aim_native_func(reinterpret_cast<rs_vm>(this), reinterpret_cast<rs_value>(rt_sp + 2), tc->integer);
                            rs_asure(clear_interrupt(vm_interrupt_type::LEAVE_INTERRUPT));

                            rs_assert((rt_bp + 1)->type == value::valuetype::callstack);
                            value* stored_bp = stack_mem_begin - (++rt_bp)->bp;
                            rt_sp = rt_bp;
                            rt_bp = stored_bp;
                        }
                        else
                        {
                            rs_assert(opnum1->type == value::valuetype::integer_type);
                            rt_ip = rt_env->rt_codes + opnum1->integer;

                        }
                        break;
                    }
                    case instruct::opcode::calln:
                    {
                        rs_assert((dr & 0b10) == 0);

                        if (dr)
                        {
                            // Call native
                            rs_extern_native_func_t call_aim_native_func = (rs_extern_native_func_t)(RS_IPVAL_MOVE_8);

                            rt_sp->type = value::valuetype::callstack;
                            rt_sp->ret_ip = (uint32_t)(rt_ip - rt_env->rt_codes);
                            rt_sp->bp = (uint32_t)(stack_mem_begin - rt_bp);
                            rt_bp = --rt_sp;
                            bp = sp = rt_sp;
                            rt_cr->set_nil();

                            ip = reinterpret_cast<byte_t*>(call_aim_native_func);

                            rs_asure(interrupt(vm_interrupt_type::LEAVE_INTERRUPT));
                            call_aim_native_func(reinterpret_cast<rs_vm>(this), reinterpret_cast<rs_value>(rt_sp + 2), tc->integer);
                            rs_asure(clear_interrupt(vm_interrupt_type::LEAVE_INTERRUPT));

                            rs_assert((rt_bp + 1)->type == value::valuetype::callstack);
                            value* stored_bp = stack_mem_begin - (++rt_bp)->bp;
                            rt_sp = rt_bp;
                            rt_bp = stored_bp;
                        }
                        else
                        {
                            const byte_t* aimplace = rt_env->rt_codes + RS_IPVAL_MOVE_4;

                            rt_sp->type = value::valuetype::callstack;
                            rt_sp->ret_ip = (uint32_t)(rt_ip - rt_env->rt_codes);
                            rt_sp->bp = (uint32_t)(stack_mem_begin - rt_bp);
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
                        if (rt_cr->get()->integer)
                            rt_ip = rt_env->rt_codes + aimplace;
                        break;
                    }
                    case instruct::opcode::jf:
                    {
                        uint32_t aimplace = RS_IPVAL_MOVE_4;
                        if (!rt_cr->get()->integer)
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
                            auto* arr_val = ++rt_sp;
                            if (arr_val->is_ref())
                                (*created_array)[i].set_ref(arr_val->get());
                            else
                                (*created_array)[i].set_val(arr_val->get());
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
                            if (val->is_ref())
                                (*created_map)[*(key->get())].set_ref(val->get());
                            else
                                (*created_map)[*(key->get())].set_val(val->get());
                        }
                        break;
                    }
                    case instruct::opcode::idx:
                    {
                        RS_ADDRESSING_N1_REF;
                        RS_ADDRESSING_N2_REF;

                        rt_ths->set_val(opnum1);

                        if (nullptr == rt_ths->gcunit && rt_ths->is_gcunit())
                        {
                            RS_VM_FAIL(RS_FAIL_ACCESS_NIL, "Trying to access is 'nil'.");
                            rt_cr->set_nil();
                        }
                        else
                        {
                            switch (rt_ths->type)
                            {
                            case value::valuetype::string_type:
                            {
                                gcbase::gc_read_guard gwg1(rt_ths->gcunit);

                                if (opnum2->type == value::valuetype::integer_type || opnum2->type == value::valuetype::handle_type)
                                {
                                    size_t strlength = 0;
                                    rs_string_t out_str = u8substr(rt_ths->string->c_str(), opnum2->integer, 1, &strlength);
                                    rt_cr->set_string(std::string(out_str, strlength).c_str());
                                }
                                else
                                    RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Cannot index string without integer & handle.");
                                break;
                            }
                            case value::valuetype::array_type:
                            {
                                gcbase::gc_read_guard gwg1(rt_ths->gcunit);
                                if (opnum2->type == value::valuetype::integer_type || opnum2->type == value::valuetype::handle_type)
                                {
                                    auto real_idx = opnum2->integer;
                                    if (real_idx < 0)
                                        real_idx = rt_ths->array->size() - (-real_idx);
                                    if ((size_t)real_idx >= rt_ths->array->size())
                                    {
                                        RS_VM_FAIL(RS_FAIL_INDEX_FAIL, "Index out of range.");
                                        rt_cr->set_nil();
                                    }
                                    else
                                        rt_cr->set_ref((*rt_ths->array)[(size_t)real_idx].get());
                                }
                                else
                                    RS_VM_FAIL(RS_FAIL_TYPE_FAIL, "Cannot index array without integer & handle.");
                                break;
                            }
                            case value::valuetype::mapping_type:
                            {
                                {
                                    gcbase::gc_read_guard gwg1(rt_ths->gcunit);
                                    auto fnd = rt_ths->mapping->find(*opnum2);
                                    if (fnd != rt_ths->mapping->end())
                                    {
                                        rt_cr->set_ref(fnd->second.get());
                                        break;
                                    }
                                }
                                gcbase::gc_write_guard gwg1(rt_ths->gcunit);
                                rt_cr->set_ref(&(*rt_ths->mapping)[*opnum2]);
                                break;
                            }
                            default:
                                RS_VM_FAIL(RS_FAIL_INDEX_FAIL, "unknown type to index.");
                                rt_cr->set_nil();
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
                                rt_cr->set_ref(opnum1->set_ref(opnum2));
                                break;
                            }
                            /*case instruct::extern_opcode_page_0::mknilarr:
                            {
                                RS_ADDRESSING_N1_REF;
                                opnum1->set_gcunit_with_barrier(value::valuetype::array_type);
                                rt_cr->set_ref(opnum1);
                                break;
                            }
                            case instruct::extern_opcode_page_0::mknilmap:
                            {
                                RS_ADDRESSING_N1_REF;
                                opnum1->set_gcunit_with_barrier(value::valuetype::mapping_type);
                                rt_cr->set_ref(opnum1);
                                break;
                            }*/
                            case instruct::extern_opcode_page_0::packargs:
                            {
                                RS_ADDRESSING_N1_REF;
                                RS_ADDRESSING_N2_REF;

                                opnum1->set_gcunit_with_barrier(value::valuetype::array_type);
                                auto* packed_array = array_t::gc_new<gcbase::gctype::eden>(opnum1->gcunit);
                                packed_array->resize(tc->integer - opnum2->integer);
                                for (auto argindex = 0 + opnum2->integer; argindex < tc->integer; argindex++)
                                {
                                    (*packed_array)[argindex - opnum2->integer].set_trans((rt_bp + 2 + argindex)->get());
                                }

                                break;
                            }
                            case instruct::extern_opcode_page_0::unpackargs:
                            {
                                RS_ADDRESSING_N1_REF;
                                RS_ADDRESSING_N2_REF;

                                if (opnum1->type != value::valuetype::array_type || opnum1->is_nil())
                                {
                                    RS_VM_FAIL(RS_FAIL_INDEX_FAIL, "Only valid array can used in unpack.");
                                }
                                else if (opnum2->integer > 0)
                                {
                                    auto* arg_array = opnum1->array;
                                    if (opnum2->integer > arg_array->size())
                                    {
                                        RS_VM_FAIL(RS_FAIL_INDEX_FAIL, "The number of arguments required for unpack exceeds the number of arguments in the given arguments-package.");
                                    }
                                    else
                                    {
                                        for (auto arg_idx = arg_array->rbegin() + (arg_array->size() - opnum2->integer);
                                            arg_idx != arg_array->rend();
                                            arg_idx++)
                                            (rt_sp--)->set_trans(&*arg_idx);
                                    }
                                }
                                else
                                {
                                    auto* arg_array = opnum1->array;
                                    if (arg_array->size() < (-opnum2->integer))
                                        RS_VM_FAIL(RS_FAIL_INDEX_FAIL, "The number of arguments required for unpack exceeds the number of arguments in the given arguments-package.");

                                    for (auto arg_idx = arg_array->rbegin(); arg_idx != arg_array->rend(); arg_idx++)
                                        (rt_sp--)->set_trans(&*arg_idx);

                                    tc->integer += arg_array->size();
                                }
                                break;
                            }
                            case instruct::extern_opcode_page_0::movdup:
                            {
                                RS_ADDRESSING_N1_REF;
                                RS_ADDRESSING_N2_REF;

                                rt_cr->set_ref(opnum1->set_dup(opnum2));
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
                            case instruct::extern_opcode_page_1::endjit:
                            {
                                rs_error("Invalid instruct: 'endjit'.");
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
                    case instruct::opcode::nop:
                    {
                        rt_ip += dr; // may need take place, skip them
                        break;
                    }
                    case instruct::opcode::abrt:
                    {
                        if (dr & 0b10)
                            return;
                        else
                            rs_error("executed 'abrt'.");
                    }
                    default:
                    {
                        --rt_ip;    // Move back one command.
                        if (vm_interrupt & vm_interrupt_type::GC_INTERRUPT)
                        {
                            // write regist(sp) data, then clear interrupt mark.
                            sp = rt_sp;
                            if (clear_interrupt(vm_interrupt_type::GC_INTERRUPT))
                                hangup();   // SLEEP UNTIL WAKE UP
                        }
                        else if (vm_interrupt & vm_interrupt_type::ABORT_INTERRUPT)
                        {
                            // ABORTED VM WILL NOT ABLE TO RUN AGAIN, SO DO NOT
                            // CLEAR ABORT_INTERRUPT
                            return;
                        }
                        else if (vm_interrupt & vm_interrupt_type::YIELD_INTERRUPT)
                        {
                            rs_asure(clear_interrupt(vm_interrupt_type::YIELD_INTERRUPT));

                            rs_asure(interrupt(vm_interrupt_type::LEAVE_INTERRUPT));
                            rs_co_yield();
                            rs_asure(clear_interrupt(vm_interrupt_type::LEAVE_INTERRUPT));
                        }
                        else if (vm_interrupt & vm_interrupt_type::LEAVE_INTERRUPT)
                        {
                            // That should not be happend...
                            rs_error("Virtual machine handled a LEAVE_INTERRUPT.");
                        }
                        else if (vm_interrupt & vm_interrupt_type::PENDING_INTERRUPT)
                        {
                            // That should not be happend...
                            rs_error("Virtual machine handled a PENDING_INTERRUPT.");
                        }
                        // it should be last interrupt..
                        else if (vm_interrupt & vm_interrupt_type::DEBUG_INTERRUPT)
                        {
                            rtopcode = opcode;

                            ip = rt_ip;
                            sp = rt_sp;
                            bp = rt_bp;
                            if (attaching_debuggee)
                            {
                                // check debuggee here
                                rs_asure(interrupt(vm_interrupt_type::LEAVE_INTERRUPT));
                                attaching_debuggee->_vm_invoke_debuggee(this);
                                rs_asure(clear_interrupt(vm_interrupt_type::LEAVE_INTERRUPT));
                            }
                            ++rt_ip;
                            goto re_entry_for_interrupt;
                        }
                        else
                        {
                            // a vm_interrupt is invalid now, just roll back one byte and continue~
                            // so here do nothing
                        }
                    }
                    }
                }// vm loop end.
#undef RS_VM_FAIL
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

            }
            catch (const std::exception& any_excep)
            {
                er->set_string(any_excep.what());
                rs::exception_recovery::rollback(this);
            }
        }


        void run()override
        {
            run_impl();
        }
    };


}

#undef RS_READY_EXCEPTION_HANDLE