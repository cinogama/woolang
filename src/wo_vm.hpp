#pragma once

#include "wo_basic_type.hpp"
#include "wo_compiler_ir.hpp"
#include "wo_utf8.hpp"
#include "wo_global_setting.hpp"
#include "wo_memory.hpp"
#include "wo_compiler_jit.hpp"

#include <shared_mutex>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <string>
#include <cmath>
#include <sstream>
#include <ctime>
#include <chrono>

#if WO_BUILD_WITH_MINGW
#   include <mingw.thread.h>
#   include <mingw.mutex.h>
#   include <mingw.shared_mutex.h>
#   include <mingw.condition_variable.h>
#endif

namespace wo
{
    class vmbase;

    class debuggee_base
    {
        inline static std::mutex _abandon_debuggees_mx;
        inline static std::vector<debuggee_base*> _abandon_debuggees;

        std::mutex _debug_block_mx;
    public:
        void _vm_invoke_debuggee(vmbase* _vm)
        {
            std::lock_guard _(_debug_block_mx);

            // Just make a block
            debug_interrupt(_vm);
        }
        virtual void debug_interrupt(vmbase*) = 0;
        virtual~debuggee_base() = default;
    public:
        void abandon()
        {
            std::lock_guard _(_abandon_debuggees_mx);
            _abandon_debuggees.push_back(this);
        }
        static void _free_abandons()
        {
            std::lock_guard _(_abandon_debuggees_mx);
            for (auto* debuggee : _abandon_debuggees)
            {
                delete debuggee;
            }
            _abandon_debuggees.clear();
        }
    };

    class vmbase
    {
    public:
        enum class jit_state : byte_t
        {
            NONE = 0,
            PREPARING = 1,
            READY = 2,
        };
        enum class vm_type
        {
            INVALID,
            NORMAL,

            // If vm's type is GC_DESTRUCTOR, GC_THREAD will not trying to pause it.
            GC_DESTRUCTOR,
        };
        enum vm_interrupt_type
        {
            NOTHING = 0,
            // There is no interrupt

            GC_INTERRUPT = 1 << 8,
            // GC work will cause this interrupt
            // GC main thread will notify GC_INTERRUPT to all vm, if a vm receive it,
            // it will be able to mark it self.
            // Before self marking begin, vm will set GC_HANGUP_INTERRUPT for itself
            // GC_HANGUP_INTERRUPT is a flag here to signing that the vm still in 
            // self marking.
            // after self marking, GC_HANGUP_INTERRUPT will be clear.
            // * If vm leaved and do not received GC_INTERRUPT, gc-work will mark it by 
            // gc-worker-threads, and the GC_HANGUP_INTERRUPT will be setted by gc-work
            // * If vm stucked in vm or jit and donot reach checkpoint to receive GC_INTERRUPT
            // gc-work will wait until timeout(1s), and treat it as self-mark vm. if
            // vm still stuck while gc-work next checking, GC_HANGUP_INTERRUPT will be set
            // and GC_INTERRUPT will clear by gc-work, gc-work will mark the vm. after this
            // mark, GC_HANGUP_INTERRUPT will be clear.


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
            //            successful. (We use 'wo_assure' here)' (Except in case of exception restore)

            DEBUG_INTERRUPT = 1 << 10,
            // If virtual machine interrupt with DEBUG_INTERRUPT, it will stop at all opcode
            // to check something about debug, such as breakpoint.
            // * DEBUG_INTERRUPT will cause huge performance loss

            ABORT_INTERRUPT = 1 << 11,
            // If virtual machine interrupt with ABORT_INTERRUPT, vm will stop immediately.

            GC_HANGUP_INTERRUPT = 1 << 12,
            // GC_HANGUP_INTERRUPT will be mark in 2 cases:
            // 1. VM received GC_INTERRUPT and start to do self-mark. after self-mark. 
            //      GC_HANGUP_INTERRUPT will be clear after self-mark.
            // 2. VM is leaved or STW-GC, in this case, vm will receive GC_HANGUP_INTERRUPT
            //      to hangup. vm will be mark by gc-worker, and be wake up after mark.

            PENDING_INTERRUPT = 1 << 13,
            // VM will be pending finish using and returned to pooled-vm, PENDING_INTERRUPT
            // only setted when vm is not running.

            BR_YIELD_INTERRUPT = 1 << 14,
            // VM will yield & return from running-state while received BR_YIELD_INTERRUPT

            DETACH_DEBUGGEE_INTERRUPT = 1 << 15,
            // VM will handle DETACH_DEBUGGEE_INTERRUPT before DEBUG_INTERRUPT, if vm handled
            // this interrupt, DEBUG_INTERRUPT will be cleared.
        };

        struct callstack_info
        {
            std::string m_func_name;
            size_t      m_row;
        };

    public:
        inline static std::shared_mutex _alive_vm_list_mx;
        inline static cxx_set_t<vmbase*> _alive_vm_list;
        inline static cxx_set_t<vmbase*> _gc_ready_vm_list;

        inline thread_local static vmbase* _this_thread_vm = nullptr;
        inline static std::atomic_uint32_t _alive_vm_count_for_gc_vm_destruct;

    protected:
        inline static debuggee_base* attaching_debuggee = nullptr;

    public:
        const vm_type virtual_machine_type;

        // special regist
        value* cr;  // op result trace & function return;
        value* tc;  // arugument count
        value* er;  // exception result

        // stack info
        value* sp;
        value* bp;

        value* register_mem_begin;
        value* const_global_begin;
        value* stack_mem_begin;

        value* _self_stack_mem_buf;
        value* _self_register_mem_buf;
        size_t stack_size;

        vmbase* gc_vm;

        lexer* compile_info;

        // next ircode pointer
        const byte_t* ip;

        shared_pointer<runtime_env> env;

        std::mutex _vm_hang_mx;
        std::condition_variable _vm_hang_cv;
        std::atomic_int8_t _vm_hang_flag;

        bool _vm_br_yieldable;
        bool _vm_br_yield_flag;

        union
        {
            std::atomic<uint32_t> vm_interrupt;
            uint32_t fast_ro_vm_interrupt;
        };
        static_assert(sizeof(std::atomic<uint32_t>) == sizeof(uint32_t));
        static_assert(std::atomic<uint32_t>::is_always_lock_free);

#if WO_ENABLE_RUNTIME_CHECK
        // runtime information
        std::thread::id attaching_thread_id;
#endif     

    private:
        vmbase(const vmbase&) = delete;
        vmbase(vmbase&&) = delete;
        vmbase& operator=(const vmbase&) = delete;
        vmbase& operator=(vmbase&&) = delete;

    public:
        void inc_destructable_instance_count() noexcept
        {
            wo_assert(env != nullptr);
#if WO_ENABLE_RUNTIME_CHECK
            size_t old_count =
#endif
                env->_created_destructable_instance_count.fetch_add(1, std::memory_order::memory_order_relaxed);
            wo_assert(old_count >= 0);
        }
        void dec_destructable_instance_count() noexcept
        {
            wo_assert(env != nullptr);
#if WO_ENABLE_RUNTIME_CHECK
            size_t old_count =
#endif
                env->_created_destructable_instance_count.fetch_sub(1, std::memory_order::memory_order_relaxed);
            wo_assert(old_count > 0);
        }

        void set_br_yieldable(bool able) noexcept
        {
            _vm_br_yieldable = able;
        }
        bool get_br_yieldable() noexcept
        {
            return _vm_br_yieldable;
        }
        bool get_and_clear_br_yield_flag() noexcept
        {
            bool result = _vm_br_yield_flag;
            _vm_br_yield_flag = false;
            return result;
        }
        void mark_br_yield() noexcept
        {
            _vm_br_yield_flag = true;
        }

        inline static debuggee_base* attach_debuggee(debuggee_base* dbg)
        {
            std::shared_lock g1(_alive_vm_list_mx);

            // Remove old debuggee
            for (auto* vm_instance : _alive_vm_list)
                if (vm_instance->virtual_machine_type != vmbase::vm_type::GC_DESTRUCTOR)
                    vm_instance->interrupt(vm_interrupt_type::DETACH_DEBUGGEE_INTERRUPT);
            for (auto* vm_instance : _alive_vm_list)
                if (vm_instance->virtual_machine_type != vmbase::vm_type::GC_DESTRUCTOR)
                    vm_instance->wait_interrupt(vm_interrupt_type::DETACH_DEBUGGEE_INTERRUPT);

            auto* old_debuggee = attaching_debuggee;
            attaching_debuggee = dbg;


            for (auto* vm_instance : _alive_vm_list)
            {
                if (vm_instance->virtual_machine_type == vmbase::vm_type::GC_DESTRUCTOR)
                    continue;

                bool has_handled = !vm_instance->clear_interrupt(
                    vm_interrupt_type::DETACH_DEBUGGEE_INTERRUPT);

                if (dbg != nullptr &&
                    (old_debuggee == nullptr ||
                        // Failed to clear DETACH_DEBUGGEE_INTERRUPT? it has been handled!
                        // Re-set DEBUG_INTERRUPT
                        has_handled))
                    vm_instance->interrupt(vm_interrupt_type::DEBUG_INTERRUPT);
                else
                    vm_instance->interrupt(vm_interrupt_type::DETACH_DEBUGGEE_INTERRUPT);
            }
            return old_debuggee;
        }
        inline static debuggee_base* current_debuggee()
        {
            return attaching_debuggee;
        }
        inline bool is_aborted() const noexcept
        {
            return vm_interrupt & vm_interrupt_type::ABORT_INTERRUPT;
        }
        inline bool interrupt(vm_interrupt_type type)
        {
            return !(type & vm_interrupt.fetch_or(type));
        }
        inline bool clear_interrupt(vm_interrupt_type type)
        {
            return type & vm_interrupt.fetch_and(~type);
        }

        enum class interrupt_wait_result : uint8_t
        {
            ACCEPT,
            TIMEOUT,
            LEAVED,
        };
        inline bool check_interrupt(vm_interrupt_type type)
        {
            return 0 != (vm_interrupt & type);
        }
        inline interrupt_wait_result wait_interrupt(vm_interrupt_type type)
        {
            using namespace std;
            size_t retry_count = 0;

            constexpr int MAX_TRY_COUNT = 0;
            int i = 0;
            do
            {
                uint32_t vm_interrupt_mask = vm_interrupt.load();

                if (0 == (vm_interrupt_mask & type))
                    break;

                if (vm_interrupt_mask & vm_interrupt_type::LEAVE_INTERRUPT)
                {
                    if (++i > MAX_TRY_COUNT)
                        return interrupt_wait_result::LEAVED;
                }
                else
                    i = 0;

                std::this_thread::sleep_for(10ms);
                if (++retry_count == config::INTERRUPT_CHECK_TIME_LIMIT)
                {
                    // Wait for too much time.
                    std::string warning_info = "Wait for too much time for waiting interrupt.\n";
                    std::stringstream dump_callstack_info;

                    dump_call_stack(32, false, dump_callstack_info);
                    warning_info += dump_callstack_info.str();
                    wo_warning(warning_info.c_str());

                    return interrupt_wait_result::TIMEOUT;
                }

            } while (true);

            return interrupt_wait_result::ACCEPT;
        }
        inline void block_interrupt(vm_interrupt_type type)
        {
            using namespace std;

            while (vm_interrupt & type)
                std::this_thread::sleep_for(10ms);
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

        vmbase(vm_type type)
            : virtual_machine_type(type)
            , cr(nullptr)
            , tc(nullptr)
            , er(nullptr)
            , sp(nullptr)
            , bp(nullptr)
            , register_mem_begin(nullptr)
            , const_global_begin(nullptr)
            , stack_mem_begin(nullptr)
            , _self_register_mem_buf(nullptr)
            , _self_stack_mem_buf(nullptr)
            , stack_size(0)
            , gc_vm(nullptr)
            , compile_info(nullptr)
            , ip(nullptr)
            , env(nullptr)
            , _vm_hang_flag(0)
            , _vm_br_yieldable(false)
            , _vm_br_yield_flag(false)
#if WO_ENABLE_RUNTIME_CHECK
            // runtime information
            , attaching_thread_id(std::thread::id{})
#endif        
        {
            ++_alive_vm_count_for_gc_vm_destruct;

            vm_interrupt = vm_interrupt_type::NOTHING;
            wo_assure(wo_leave_gcguard(reinterpret_cast<wo_vm>(this)));

            std::lock_guard g1(_alive_vm_list_mx);

            wo_assert(_alive_vm_list.find(this) == _alive_vm_list.end(),
                "This vm is already exists in _alive_vm_list, that is illegal.");

            if (current_debuggee() != nullptr)
                wo_assure(this->interrupt(vm_interrupt_type::DEBUG_INTERRUPT));

            _alive_vm_list.insert(this);
        }
        virtual ~vmbase()
        {
            do
            {
                std::lock_guard g1(_alive_vm_list_mx);

                wo_assert(_alive_vm_list.find(this) != _alive_vm_list.end(),
                    "This vm not exists in _alive_vm_list, that is illegal.");

                _alive_vm_list.erase(this);

                wo_assert(_gc_ready_vm_list.find(this) != _gc_ready_vm_list.end() || env == nullptr,
                    "This vm not exists in _gc_ready_vm_list, that is illegal.");

                _gc_ready_vm_list.erase(this);

            } while (0);

            free(_self_register_mem_buf);
            free(_self_stack_mem_buf);

            if (compile_info)
                delete compile_info;

            if (env)
                --env->_running_on_vm_count;

            --_alive_vm_count_for_gc_vm_destruct;
        }

        vmbase* get_or_alloc_gcvm() const
        {
            // TODO: GC will mark globle space when current vm is gc vm, we cannot do it now!

            return gc_vm;
#if 0
            static_assert(std::atomic<vmbase*>::is_always_lock_free);

            std::atomic<vmbase*>* vmbase_atomic = reinterpret_cast<std::atomic<vmbase*>*>(const_cast<vmbase**>(&gc_vm));
            vmbase* const INVALID_VM_PTR = (vmbase*)(intptr_t)-1;

        retry_to_fetch_gcvm:
            if (vmbase_atomic->load())
            {
                vmbase* loaded_gcvm;
                do
                    loaded_gcvm = vmbase_atomic->load();
                while (loaded_gcvm == INVALID_VM_PTR);

                return loaded_gcvm;
            }

            vmbase* excepted = nullptr;
            bool exchange_result = false;
            do
            {
                exchange_result = vmbase_atomic->compare_exchange_weak(excepted, INVALID_VM_PTR);
                if (!exchange_result && excepted)
                {
                    if (excepted == INVALID_VM_PTR)
                        goto retry_to_fetch_gcvm;
                    return excepted;
                }
            } while (!exchange_result);

            wo_assert(vmbase_atomic->load() == INVALID_VM_PTR);
            // Create a new VM using for GC destruct
            auto* created_subvm_for_gc = make_machine(1024);
            // gc_thread will be destructed by gc_work..
            created_subvm_for_gc->virtual_machine_type = vm_type::GC_DESTRUCTOR;

            vmbase_atomic->store(created_subvm_for_gc);
            return created_subvm_for_gc;
#endif
        }

    public:
        virtual vmbase* create_machine(vm_type type) const = 0;
        virtual wo_result_t run() = 0;

    public:
        inline void _allocate_register_space(size_t regcount)
        {
            _self_register_mem_buf = std::launder(reinterpret_cast<value*>(calloc(regcount, sizeof(value))));
            register_mem_begin = _self_register_mem_buf;
            cr = register_mem_begin + opnum::reg::spreg::cr;
            tc = register_mem_begin + opnum::reg::spreg::tc;
            er = register_mem_begin + opnum::reg::spreg::er;
        }
        inline void _allocate_stack_space(size_t stacksz)
        {
            stack_size = stacksz;
            _self_stack_mem_buf = std::launder(reinterpret_cast<value*>(calloc(stack_size + 1, sizeof(value))));
            stack_mem_begin = _self_stack_mem_buf + stack_size;
            sp = bp = stack_mem_begin;
        }
        inline void set_runtime(shared_pointer<runtime_env> runtime_environment)
        {
            wo_assure(wo_enter_gcguard(std::launder(reinterpret_cast<wo_vm>(this))));
            wo_assert(nullptr == _self_stack_reg_mem_buf);

            env = runtime_environment;
            ++env->_running_on_vm_count;

            const_global_begin = env->constant_global;
            ip = env->rt_codes;

            _allocate_stack_space(env->runtime_stack_count);
            _allocate_register_space(env->real_register_count);

            do
            {
                std::lock_guard g1(_alive_vm_list_mx);
                _gc_ready_vm_list.insert(this);
            } while (false);

            wo_assure(wo_leave_gcguard(std::launder(reinterpret_cast<wo_vm>(this))));

            // Create a new VM using for GC destruct
            auto* created_subvm_for_gc = make_machine(stack_size, vm_type::GC_DESTRUCTOR);

            gc_vm = created_subvm_for_gc->gc_vm = created_subvm_for_gc;
        }
        inline vmbase* make_machine(size_t stack_sz, vm_type type) const
        {
            wo_assert(env != nullptr);

            vmbase* new_vm = create_machine(type);
            new_vm->gc_vm = get_or_alloc_gcvm();

            new_vm->env = env;  // env setted, gc will scan this vm..
            ++env->_running_on_vm_count;

            wo_assure(wo_enter_gcguard(std::launder(reinterpret_cast<wo_vm>(new_vm))));
            new_vm->const_global_begin = const_global_begin;

            if (!stack_sz)
                stack_sz = env->runtime_stack_count;
           
            new_vm->ip = env->rt_codes;
            new_vm->_allocate_stack_space(stack_sz);
            new_vm->_allocate_register_space(env->real_register_count);

            do
            {
                std::lock_guard g1(_alive_vm_list_mx);
                _gc_ready_vm_list.insert(new_vm);
            } while (false);

            wo_assure(wo_leave_gcguard(std::launder(reinterpret_cast<wo_vm>(new_vm))));
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

                    const int MAX_BYTE_COUNT = 12;
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
#define WO_SIGNED_SHIFT(VAL) (((signed char)((unsigned char)(((unsigned char)(VAL))<<1)))>>1)
                auto print_reg_bpoffset = [&]() {
                    byte_t data_1b = *(this_command_ptr++);
                    if (data_1b & 1 << 7)
                    {
                        // bp offset
                        auto offset = WO_SIGNED_SHIFT(data_1b);
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
                        else if (data_1b == 36)
                            tmpos << "pm";
                        else if (data_1b == 37)
                            tmpos << "tp";
                        else
                            tmpos << "reg(" << (uint32_t)data_1b << ")";

                    }
                };
                auto print_opnum1 = [&]() {
                    if (main_command & (byte_t)0b00000010)
                    {
                        //is dr 1byte 
                        print_reg_bpoffset();
                    }
                    else
                    {
                        //const global 4byte
                        uint32_t data_4b = *(uint32_t*)((this_command_ptr += 4) - 4);
                        if (data_4b < env->constant_value_count)
                            tmpos << wo_cast_string((wo_value)&env->constant_global[data_4b])
                            << " : " << wo_type_name((wo_type)env->constant_global[data_4b].type);
                        else
                            tmpos << "g[" << data_4b - env->constant_value_count << "]";
                    }
                };
                auto print_opnum2 = [&]() {
                    if (main_command & (byte_t)0b00000001)
                    {
                        //is dr 1byte 
                        print_reg_bpoffset();
                    }
                    else
                    {
                        //const global 4byte
                        uint32_t data_4b = *(uint32_t*)((this_command_ptr += 4) - 4);
                        if (data_4b < env->constant_value_count)
                            tmpos << wo_cast_string((wo_value)&env->constant_global[data_4b])
                            << " : " << wo_type_name((wo_type)env->constant_global[data_4b].type);
                        else
                            tmpos << "g[" << data_4b - env->constant_value_count << "]";
                    }
                };

#undef WO_SIGNED_SHIFT
                switch (main_command & (byte_t)0b11111100)
                {
                case instruct::nop:
                    tmpos << "nop\t";

                    this_command_ptr += main_command & (byte_t)0b00000011;

                    break;
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
                case instruct::lds:
                    tmpos << "lds\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
                case instruct::sts:
                    tmpos << "sts\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
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
                    if (main_command & 0b10)
                        tmpos << "callnfast\t";
                    else
                        tmpos << "calln\t";

                    if (main_command & 0b01 || main_command & 0b10)
                        //neg
                        tmpos << *(void**)((this_command_ptr += 8) - 8);
                    else
                    {
                        tmpos << "+" << *(uint32_t*)((this_command_ptr += 4) - 4);
                        this_command_ptr += 4;
                    }
                    break;
                case instruct::ret:
                    tmpos << "ret\t";
                    if (main_command & 0b10)
                        tmpos << "pop " << *(uint16_t*)((this_command_ptr += 2) - 2);
                    break;

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
                    tmpos << wo_type_name((wo_type) * (this_command_ptr++));

                    break;
                case instruct::mkunion:
                    tmpos << "mkunion\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); tmpos << ",\t id=" << *(uint16_t*)((this_command_ptr += 2) - 2);
                    break;
                case instruct::mkclos:
                    tmpos << "mkclos\t";
                    tmpos << *(uint16_t*)((this_command_ptr += 2) - 2);
                    tmpos << ",\t";
                    if (main_command & 0b10)
                        tmpos << *(void**)((this_command_ptr += 8) - 8);
                    else
                    {
                        tmpos << "+" << *(uint32_t*)((this_command_ptr += 4) - 4);
                        this_command_ptr += 4;
                    }

                    break;
                case instruct::typeas:
                    if (main_command & 0b01)
                        tmpos << "typeis\t";
                    else
                        tmpos << "typeas\t";
                    print_opnum1();
                    tmpos << " : ";
                    tmpos << wo_type_name((wo_type) * (this_command_ptr++));

                    break;
                case instruct::abrt:
                    if (main_command & 0b10)
                        tmpos << "end\t";
                    else
                        tmpos << "abrt\t";
                    break;
                case instruct::equb:
                    tmpos << "equb\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
                case instruct::nequb:
                    tmpos << "nequb\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
                case instruct::mkstruct:
                    tmpos << "mkstruct\t"; print_opnum1(); tmpos << " size=" << *(uint16_t*)((this_command_ptr += 2) - 2); break;
                case instruct::idstruct:
                    tmpos << "idstruct\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); tmpos << " offset=" << *(uint16_t*)((this_command_ptr += 2) - 2); break;
                case instruct::jnequb:
                {
                    tmpos << "jnequb\t"; print_opnum1();
                    tmpos << "\t+" << *(uint32_t*)((this_command_ptr += 4) - 4);
                    break;
                }
                case instruct::mkarr:
                    tmpos << "mkarr\t"; print_opnum1(); tmpos << ",\t size=" << *(uint16_t*)((this_command_ptr += 2) - 2);  break;
                case instruct::mkmap:
                    tmpos << "mkmap\t"; print_opnum1();  tmpos << ",\t size=" << *(uint16_t*)((this_command_ptr += 2) - 2);  break;
                case instruct::idarr:
                    tmpos << "idarr\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
                case instruct::iddict:
                    tmpos << "iddict\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
                case instruct::siddict:
                    tmpos << "siddict\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); tmpos << ",\t"; print_reg_bpoffset(); break;
                case instruct::sidmap:
                    tmpos << "sidmap\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); tmpos << ",\t"; print_reg_bpoffset(); break;
                case instruct::sidarr:
                    tmpos << "sidarr\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); tmpos << ",\t"; print_reg_bpoffset(); break;
                case instruct::sidstruct:
                    tmpos << "sidstruct\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); tmpos << " offset=" << *(uint16_t*)((this_command_ptr += 2) - 2); break;
                case instruct::idstr:
                    tmpos << "idstr\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
                case instruct::equr:
                    tmpos << "equr\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
                case instruct::nequr:
                    tmpos << "nequr\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
                case instruct::equs:
                    tmpos << "equs\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break;
                case instruct::nequs:
                    tmpos << "nequs\t"; print_opnum1(); tmpos << ",\t"; print_opnum2(); break; \
                case instruct::unpackargs:
                    {
                        tmpos << "unpackargs\t"; print_opnum1();
                        int32_t unpack_argc = *(int32_t*)((this_command_ptr += 4) - 4);
                        if (unpack_argc >= 0)
                            tmpos << "\tcount = " << unpack_argc;
                        else
                            tmpos << "\tleast = " << -unpack_argc;
                        break;
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
                        case instruct::extern_opcode_page_0::packargs:
                        {
                            tmpos << "packargs\t"; print_opnum1(); tmpos << ",\t";

                            auto this_func_argc = *(uint32_t*)((this_command_ptr += 4) - 4);
                            auto skip_closure = *(uint16_t*)((this_command_ptr += 2) - 2);

                            tmpos << ": skip " << this_func_argc << "/" << skip_closure;
                            break;
                        }
                        case instruct::extern_opcode_page_0::panic:
                            tmpos << "panic\t"; print_opnum1();
                            break;
                        case instruct::extern_opcode_page_0::cdivilr:
                            tmpos << "cdivilr\t"; print_opnum1(); tmpos << ",\t"; print_opnum2();
                            break;
                        case instruct::extern_opcode_page_0::cdivil:
                            tmpos << "cdivil\t"; print_opnum1();
                            break;
                        case instruct::extern_opcode_page_0::cdivirz:
                            tmpos << "cdivirz\t"; print_opnum1();
                            break;
                        case instruct::extern_opcode_page_0::cdivir:
                            tmpos << "cdivir\t"; print_opnum1();
                            break;
                        default:
                            tmpos << "??\t";
                            break;
                        }
                        break;
                        //case 1:
                        //    switch (main_command & 0b11111100)
                        //    {
                        //    default:
                        //        tmpos << "??\t";
                        //        break;
                        //    }
                        //    break;
                        //case 2:
                        //    switch (main_command & 0b11111100)
                        //    {
                        //    default:
                        //        tmpos << "??\t";
                        //        break;
                        //    }
                        //    break;
                    case 3:
                        tmpos << "flag ";
                        switch (main_command & 0b11111100)
                        {
                        case instruct::extern_opcode_page_3::funcbegin:
                            tmpos << "funcbegin\t"; break;
                        case instruct::extern_opcode_page_3::funcend:
                            tmpos << "funcend\t"; break;
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
            if (env == nullptr)
            {
                os << "<current vm is not ready!>" << std::endl;
                return;
            }

            const program_debug_data_info::location* src_location_info = nullptr;
            if (env->program_debug_info != nullptr)
                src_location_info = &env->program_debug_info->get_src_location_by_runtime_ip(ip - (need_offset ? 1 : 0));
            // NOTE: When vm running, rt_ip may point to:
            // [ -- COMMAND 6bit --] [ - DR 2bit -] [ ----- OPNUM1 ------] [ ----- OPNUM2 ------]
            //                                     ^1                     ^2                     ^3
            // If rt_ip point to place 3, 'get_current_func_signature_by_runtime_ip' will get next command's debuginfo.
            // So we do a move of 1BYTE here, for getting correct debuginfo.

            size_t call_trace_count = 0;

            if (ip >= env->rt_codes && ip < env->rt_codes + env->rt_code_len)
            {
                if (src_location_info)
                {
                    os << call_trace_count << ": " << env->program_debug_info->get_current_func_signature_by_runtime_ip(ip - (need_offset ? 1 : 0)) << std::endl;
                    os << "\t--at " << wstr_to_str(src_location_info->source_file) << "(" << src_location_info->begin_row_no << ", " << src_location_info->begin_col_no << ")" << std::endl;
                }
                else
                    os << call_trace_count << ": " << (void*)ip << std::endl;
            }
            else
            {
                auto fnd = env->extern_native_functions.find((intptr_t)ip);

                if (fnd != env->extern_native_functions.end())
                {
                    os << call_trace_count << ": extern func " << fnd->second.function_name << std::endl;
                    os << "\t--at " << fnd->second.library_name.value_or("woolang") << std::endl;
                }
                else
                {
                    os << call_trace_count << ": extern func __native_function__at_" << (void*)ip << std::endl;
                    os << "\t--at unknown native" << std::endl;
                }
            }

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
                    if (src_location_info)
                    {
                        src_location_info = &env->program_debug_info->get_src_location_by_runtime_ip(env->rt_codes + base_callstackinfo_ptr->vmcallstack.ret_ip - (need_offset ? 1 : 0));

                        os << call_trace_count << ": " << env->program_debug_info->get_current_func_signature_by_runtime_ip(
                            env->rt_codes + base_callstackinfo_ptr->vmcallstack.ret_ip - (need_offset ? 1 : 0)) << std::endl;
                        os << "\t--at " << wstr_to_str(src_location_info->source_file) << "(" << src_location_info->begin_row_no << ", " << src_location_info->begin_col_no << ")" << std::endl;
                    }
                    else
                        os << call_trace_count << ": " << (void*)(env->rt_codes + base_callstackinfo_ptr->vmcallstack.ret_ip) << std::endl;

                    base_callstackinfo_ptr = this->stack_mem_begin - base_callstackinfo_ptr->vmcallstack.bp;
                    base_callstackinfo_ptr++;

                    // When dumping call stack if specified vm is running(during GC), the stack data might be
                    // modified. We will get broken call stack info and we should stop.
                    if (base_callstackinfo_ptr > stack_mem_begin || base_callstackinfo_ptr < stack_mem_begin - (stack_size - 1))
                    {
                        os << call_trace_count + 1 << ": ??" << std::endl;
                        break;
                    }
                }
                else if (base_callstackinfo_ptr->type == value::valuetype::nativecallstack)
                {
                    auto fnd = env->extern_native_functions.find((intptr_t)base_callstackinfo_ptr->native_function_addr);

                    if (fnd != env->extern_native_functions.end())
                    {
                        os << call_trace_count << ": extern func " << fnd->second.function_name << std::endl;
                        os << "\t--at " << fnd->second.library_name.value_or("woolang") << std::endl;
                    }
                    else
                    {
                        os << call_trace_count << ": extern func __native_function__at_" << (void*)base_callstackinfo_ptr->native_function_addr << std::endl;
                        os << "\t--at unknown native" << std::endl;
                    }

                    for (;;)
                    {
                        base_callstackinfo_ptr++;
                        if (base_callstackinfo_ptr <= stack_mem_begin)
                        {
                            if (base_callstackinfo_ptr->type == value::valuetype::nativecallstack
                                || base_callstackinfo_ptr->type == value::valuetype::callstack)
                                break;
                            else
                            {
                                os << "??" << std::endl;
                                return;
                            }
                        }

                    }
                }
                else
                {
                    os << "??" << std::endl;
                    return;
                }
            }
        }
        inline std::vector<callstack_info> dump_call_stack_func_info(bool need_offset = true)const
        {
            // TODO; Dump call stack without pdb
            const program_debug_data_info::location* src_location_info = nullptr;
            if (env->program_debug_info != nullptr)
                src_location_info = &env->program_debug_info->get_src_location_by_runtime_ip(ip - (need_offset ? 1 : 0));
            // NOTE: When vm running, rt_ip may point to:
            // [ -- COMMAND 6bit --] [ - DR 2bit -] [ ----- OPNUM1 ------] [ ----- OPNUM2 ------]
            //                                     ^1                     ^2                     ^3
            // If rt_ip point to place 3, 'get_current_func_signature_by_runtime_ip' will get next command's debuginfo.
            // So we do a move of 1BYTE here, for getting correct debuginfo.

            std::vector<callstack_info> result;

            size_t call_trace_count = 0;

            if (src_location_info)
                result.push_back(
                    callstack_info{
                        env->program_debug_info->get_current_func_signature_by_runtime_ip(ip - (need_offset ? 1 : 0)),
                        src_location_info->begin_row_no,
                    });
            else
                result.push_back(
                    callstack_info{
                        "Extern function",
                        0,
                    });

            value* base_callstackinfo_ptr = (bp + 1);
            while (base_callstackinfo_ptr <= this->stack_mem_begin)
            {
                ++call_trace_count;
                if (base_callstackinfo_ptr->type == value::valuetype::callstack)
                {
                    if (src_location_info)
                        result.push_back(
                            callstack_info{
                                env->program_debug_info->get_current_func_signature_by_runtime_ip(env->rt_codes + base_callstackinfo_ptr->vmcallstack.ret_ip - (need_offset ? 1 : 0)),
                                src_location_info->begin_row_no
                            });
                    else
                        result.push_back(
                            callstack_info{
                                "Extern function",
                                0,
                            });

                    base_callstackinfo_ptr = this->stack_mem_begin - base_callstackinfo_ptr->vmcallstack.bp;
                    base_callstackinfo_ptr++;
                }
                else
                {
                    result.push_back(
                        callstack_info{
                            "Extern function",
                            0,
                        });
                    break;
                }
            }
            return result;
        }
        inline size_t callstack_layer() const
        {
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

                    base_callstackinfo_ptr = this->stack_mem_begin - base_callstackinfo_ptr->vmcallstack.bp;
                    base_callstackinfo_ptr++;
                }
                else
                {
                    break;
                }
            }

            return call_trace_count;
        }
        inline bool gc_checkpoint()
        {
            if (interrupt(vm_interrupt_type::GC_HANGUP_INTERRUPT))
            {
                // In stw GC, if current VM in leaving while marking, and receive GC_HANGUP_INTERRUPT
                // at the end of marking stage. vm might step here and failed to receive GC_INTERRUPT.
                // In this case, we still need to clear GC_HANGUP_INTERRUPT.
                //
                // In very small probability that another round of stw GC start in here. 
                // We will make sure GC_HANGUP_INTERRUPT marked repeatly until successful in gc-work.
                if (clear_interrupt(vm_interrupt_type::GC_INTERRUPT))
                    gc::mark_vm(this, SIZE_MAX);

                wo_assure(clear_interrupt(vm_interrupt_type::GC_HANGUP_INTERRUPT));
                return true;
            }
            return false;
        }
        inline value* co_pre_invoke(wo_int_t wo_func_addr, wo_int_t argc)
        {
            wo_assert((vm_interrupt & vm_interrupt_type::LEAVE_INTERRUPT) == 0);

            if (!wo_func_addr)
                wo_fail(WO_FAIL_CALL_FAIL, "Cannot call a 'nil' function.");
            else
            {
                auto* return_sp = sp;

                (sp--)->set_native_callstack(ip);
                ip = env->rt_codes + wo_func_addr;
                tc->set_integer(argc);
                er->set_integer(argc);
                bp = sp;

                return return_sp;
            }
            return nullptr;
        }
        inline value* co_pre_invoke(wo_handle_t ex_func_addr, wo_int_t argc)
        {
            wo_assert((vm_interrupt & vm_interrupt_type::LEAVE_INTERRUPT) == 0);

            if (!ex_func_addr)
                wo_fail(WO_FAIL_CALL_FAIL, "Cannot call a 'nil' function.");
            else
            {
                auto* return_sp = sp;

                (sp--)->set_native_callstack(ip);
                ip = (const byte_t*)ex_func_addr;
                tc->set_integer(argc);
                er->set_integer(argc);
                bp = sp;

                return return_sp;
            }
            return nullptr;
        }
        inline value* co_pre_invoke(closure_t* wo_func_addr, wo_int_t argc)
        {
            wo_assert((vm_interrupt & vm_interrupt_type::LEAVE_INTERRUPT) == 0);

            if (!wo_func_addr)
                wo_fail(WO_FAIL_CALL_FAIL, "Cannot call a 'nil' function.");
            else
            {
                wo::gcbase::gc_read_guard rg1(wo_func_addr);

                wo_integer_t vm_sfuncaddr = wo_func_addr->m_vm_func;
                if (wo_func_addr->m_native_call)
                {
                    // Current closure stores jit function, find vm func from jit-record;
                    vm_sfuncaddr = (wo_integer_t)env->_jit_functions.at((void*)wo_func_addr->m_native_func);
                }

                if (vm_sfuncaddr == 0)
                    wo_fail(WO_FAIL_CALL_FAIL, "Cannot call a 'nil' function.");
                else
                {
                    auto* return_sp = sp;

                    for (auto idx = wo_func_addr->m_closure_args_count; idx > 0; --idx)
                        (sp--)->set_val(&wo_func_addr->m_closure_args[idx - 1]);

                    (sp--)->set_native_callstack(ip);
                    ip = env->rt_codes + vm_sfuncaddr;
                    tc->set_integer(argc);
                    er->set_integer(argc);
                    bp = sp;

                    return return_sp;
                }
            }
            return nullptr;
        }
        inline value* invoke(wo_int_t wo_func_addr, wo_int_t argc)
        {
            wo_assert((vm_interrupt & vm_interrupt_type::LEAVE_INTERRUPT) == 0);

            if (!wo_func_addr)
                wo_fail(WO_FAIL_CALL_FAIL, "Cannot call a 'nil' function.");
            else
            {
                auto* return_ip = ip;
                auto* return_sp = sp + argc;
                auto* return_bp = bp;

                (sp--)->set_native_callstack(ip);
                ip = env->rt_codes + wo_func_addr;
                tc->set_integer(argc);
                bp = sp;

                run();

                ip = return_ip;
                sp = return_sp;
                bp = return_bp;

                if (is_aborted())
                    return nullptr;

                return cr;
            }
            return nullptr;
        }
        inline value* invoke(wo_handle_t wo_func_addr, wo_int_t argc)
        {
            wo_assert((vm_interrupt & vm_interrupt_type::LEAVE_INTERRUPT) == 0);

            if (!wo_func_addr)
                wo_fail(WO_FAIL_CALL_FAIL, "Cannot call a 'nil' function.");
            else
            {
                auto* return_ip = ip;
                auto* return_sp = sp + argc;
                auto* return_bp = bp;

                (sp--)->set_native_callstack(ip);
                ip = env->rt_codes + wo_func_addr;
                tc->set_integer(argc);
                bp = sp;

                if (!is_aborted())
                {
                    switch (((wo_native_func_t)wo_func_addr)(
                        std::launder(reinterpret_cast<wo_vm>(this)),
                        std::launder(reinterpret_cast<wo_value>(sp + 2))))
                    {
                    case wo_result_t::WO_API_NORMAL:
                        break;
                    case wo_result_t::WO_API_RESYNC:
                        run();
                        break;
                    }
                }

                ip = return_ip;
                sp = return_sp;
                bp = return_bp;

                if (is_aborted())
                    return nullptr;

                return cr;
            }
            return nullptr;
        }
        inline value* invoke(closure_t* wo_func_closure, wo_int_t argc)
        {
            wo_assert((vm_interrupt & vm_interrupt_type::LEAVE_INTERRUPT) == 0);

            if (!wo_func_closure)
                wo_fail(WO_FAIL_CALL_FAIL, "Cannot call a 'nil' function.");
            else
            {
                wo::gcbase::gc_read_guard rg1(wo_func_closure);
                if (!wo_func_closure->m_vm_func)
                    wo_fail(WO_FAIL_CALL_FAIL, "Cannot call a 'nil' function.");
                else
                {
                    auto* return_ip = ip;

                    // NOTE: No need to reduce expand arg count.
                    auto* return_sp = sp + argc;
                    auto* return_bp = bp;

                    for (auto idx = wo_func_closure->m_closure_args_count; idx > 0; --idx)
                        (sp--)->set_val(&wo_func_closure->m_closure_args[idx - 1]);

                    (sp--)->set_native_callstack(ip);
                    bp = sp;

                    if (wo_func_closure->m_native_call)
                    {
                        if (!is_aborted())
                        {
                            switch (wo_func_closure->m_native_func(
                                std::launder(reinterpret_cast<wo_vm>(this)),
                                std::launder(reinterpret_cast<wo_value>(sp + 2))))
                            {
                            case wo_result_t::WO_API_NORMAL:
                                break;
                            case wo_result_t::WO_API_RESYNC:
                                run();
                                break;
                            }
                        }
                    }
                    else
                    {
                        ip = env->rt_codes + wo_func_closure->m_vm_func;
                        tc->set_integer(argc);
                        run();
                    }

                    ip = return_ip;
                    sp = return_sp;
                    bp = return_bp;

                    if (is_aborted())
                        return nullptr;

                    return cr;
                }
            }
            return nullptr;
        }

#define WO_SAFE_READ_OFFSET_GET_QWORD (*(uint64_t*)(rt_ip-8))
#define WO_SAFE_READ_OFFSET_GET_DWORD (*(uint32_t*)(rt_ip-4))
#define WO_SAFE_READ_OFFSET_GET_WORD (*(uint16_t*)(rt_ip-2))

        // FOR BigEndian
#define WO_SAFE_READ_OFFSET_PER_BYTE(OFFSET, TYPE) (((TYPE)(*(rt_ip-OFFSET)))<<((sizeof(TYPE)-OFFSET)*8))
#define WO_IS_ODD_IRPTR(ALLIGN) 1 //(reinterpret_cast<size_t>(rt_ip)%ALLIGN)

#define WO_SAFE_READ_MOVE_2 (rt_ip+=2,WO_IS_ODD_IRPTR(2)?\
                                    (WO_SAFE_READ_OFFSET_PER_BYTE(2,uint16_t)|WO_SAFE_READ_OFFSET_PER_BYTE(1,uint16_t)):\
                                    WO_SAFE_READ_OFFSET_GET_WORD)
#define WO_SAFE_READ_MOVE_4 (rt_ip+=4,WO_IS_ODD_IRPTR(4)?\
                                    (WO_SAFE_READ_OFFSET_PER_BYTE(4,uint32_t)|WO_SAFE_READ_OFFSET_PER_BYTE(3,uint32_t)\
                                    |WO_SAFE_READ_OFFSET_PER_BYTE(2,uint32_t)|WO_SAFE_READ_OFFSET_PER_BYTE(1,uint32_t)):\
                                    WO_SAFE_READ_OFFSET_GET_DWORD)
#define WO_SAFE_READ_MOVE_8 (rt_ip+=8,WO_IS_ODD_IRPTR(8)?\
                                    (WO_SAFE_READ_OFFSET_PER_BYTE(8,uint64_t)|WO_SAFE_READ_OFFSET_PER_BYTE(7,uint64_t)|\
                                    WO_SAFE_READ_OFFSET_PER_BYTE(6,uint64_t)|WO_SAFE_READ_OFFSET_PER_BYTE(5,uint64_t)|\
                                    WO_SAFE_READ_OFFSET_PER_BYTE(4,uint64_t)|WO_SAFE_READ_OFFSET_PER_BYTE(3,uint64_t)|\
                                    WO_SAFE_READ_OFFSET_PER_BYTE(2,uint64_t)|WO_SAFE_READ_OFFSET_PER_BYTE(1,uint64_t)):\
                                    WO_SAFE_READ_OFFSET_GET_QWORD)
#define WO_IPVAL (*(rt_ip))
#define WO_IPVAL_MOVE_1 (*(rt_ip++))

            // X86 support non-alligned addressing, so just do it!
#define WO_FAST_READ_MOVE_2 (*(uint16_t*)((rt_ip += 2) - 2))
#define WO_FAST_READ_MOVE_4 (*(uint32_t*)((rt_ip += 4) - 4))
#define WO_FAST_READ_MOVE_8 (*(uint64_t*)((rt_ip += 8) - 8))


#define WO_IPVAL_MOVE_2 ((ARCH & platform_info::ArchType::X86)?(WO_FAST_READ_MOVE_2):((uint16_t)WO_SAFE_READ_MOVE_2))
#define WO_IPVAL_MOVE_4 ((ARCH & platform_info::ArchType::X86)?(WO_FAST_READ_MOVE_4):((uint32_t)WO_SAFE_READ_MOVE_4))
#define WO_IPVAL_MOVE_8 ((ARCH & platform_info::ArchType::X86)?(WO_FAST_READ_MOVE_8):((uint64_t)WO_SAFE_READ_MOVE_8))

    };

    ////////////////////////////////////////////////////////////////////////////////

    class vm : public vmbase
    {
    public:
        vm(vm_type type)
            : vmbase(type)
        {

        }
        virtual ~vm() override = default;
    public:
        virtual vmbase* create_machine(vm_type type) const override
        {
            return new vm(type);
        }
        virtual wo_result_t run()override
        {
            if (ip >= env->rt_codes && ip < env->rt_codes + env->rt_code_len)
                run_impl();
            else
                return ((wo_extern_native_func_t)ip)(
                    std::launder(reinterpret_cast<wo_vm>(this)),
                    std::launder(reinterpret_cast<wo_value>(sp + 2)));

            return wo_result_t::WO_API_NORMAL;
        }
    public:
        inline static void ltx_impl(value* result, value* opnum1, value* opnum2)
        {
            switch (opnum1->type)
            {
            case value::valuetype::integer_type:
                result->set_bool(opnum1->integer < opnum2->integer); break;
            case value::valuetype::handle_type:
                result->set_bool(opnum1->handle < opnum2->handle); break;
            case value::valuetype::real_type:
                result->set_bool(opnum1->real < opnum2->real); break;
            case value::valuetype::string_type:
                result->set_bool(*opnum1->string < *opnum2->string); break;
            default:
                result->set_bool(false);
                wo_fail(WO_FAIL_TYPE_FAIL, "Values of this type cannot be compared.");
                break;
            }
        }
        inline static void eltx_impl(value* result, value* opnum1, value* opnum2)
        {
            switch (opnum1->type)
            {
            case value::valuetype::integer_type:
                result->set_bool(opnum1->integer <= opnum2->integer); break;
            case value::valuetype::handle_type:
                result->set_bool(opnum1->handle <= opnum2->handle); break;
            case value::valuetype::real_type:
                result->set_bool(opnum1->real <= opnum2->real); break;
            case value::valuetype::string_type:
                result->set_bool(*opnum1->string <= *opnum2->string); break;
            default:
                result->set_bool(false);
                wo_fail(WO_FAIL_TYPE_FAIL, "Values of this type cannot be compared.");
                break;
            }
        }
        inline static void gtx_impl(value* result, value* opnum1, value* opnum2)
        {
            switch (opnum1->type)
            {
            case value::valuetype::integer_type:
                result->set_bool(opnum1->integer > opnum2->integer); break;
            case value::valuetype::handle_type:
                result->set_bool(opnum1->handle > opnum2->handle); break;
            case value::valuetype::real_type:
                result->set_bool(opnum1->real > opnum2->real); break;
            case value::valuetype::string_type:
                result->set_bool(*opnum1->string > *opnum2->string); break;
            default:
                result->set_bool(false);
                wo_fail(WO_FAIL_TYPE_FAIL, "Values of this type cannot be compared.");
                break;
            }
        }
        inline static void egtx_impl(value* result, value* opnum1, value* opnum2)
        {
            switch (opnum1->type)
            {
            case value::valuetype::integer_type:
                result->set_bool(opnum1->integer >= opnum2->integer); break;
            case value::valuetype::handle_type:
                result->set_bool(opnum1->handle >= opnum2->handle); break;
            case value::valuetype::real_type:
                result->set_bool(opnum1->real >= opnum2->real); break;
            case value::valuetype::string_type:
                result->set_bool(*opnum1->string >= *opnum2->string); break;
            default:
                result->set_bool(false);
                wo_fail(WO_FAIL_TYPE_FAIL, "Values of this type cannot be compared.");
                break;
            }
        }
        inline static value* make_union_impl(value* opnum1, value* opnum2, uint16_t id)
        {
            opnum1->set_gcunit<value::valuetype::struct_type>(
                struct_t::gc_new<gcbase::gctype::young>(2));

            opnum1->structs->m_values[0].set_integer((wo_integer_t)id);
            opnum1->structs->m_values[1].set_val(opnum2);

            return opnum1;
        }
        inline static value* make_closure_fast_impl(value* opnum1, const byte_t* rt_ip, value* rt_sp)
        {
            bool is_native_call = !!(0b0011 & *(rt_ip - 1));
            uint16_t closure_arg_count = WO_FAST_READ_MOVE_2;

            auto* created_closure = closure_t::gc_new<gcbase::gctype::young>(closure_arg_count);
            created_closure->m_native_call = is_native_call;

            if (is_native_call)
                created_closure->m_native_func = (wo_native_func_t)WO_FAST_READ_MOVE_8;
            else
                created_closure->m_vm_func = WO_FAST_READ_MOVE_4;

            for (size_t i = 0; i < (size_t)closure_arg_count; i++)
            {
                auto* arg_val = ++rt_sp;
                created_closure->m_closure_args[i].set_val(arg_val);
            }
            opnum1->set_gcunit<wo::value::valuetype::closure_type>(created_closure);
            return rt_sp;
        }
        inline static value* make_closure_safe_impl(value* opnum1, const byte_t* rt_ip, value* rt_sp)
        {
            bool is_native_call = !!(0b0011 & *(rt_ip - 1));
            uint16_t closure_arg_count = WO_SAFE_READ_MOVE_2;

            auto* created_closure = closure_t::gc_new<gcbase::gctype::young>(closure_arg_count);
            created_closure->m_native_call = is_native_call;

            if (is_native_call)
                created_closure->m_native_func = (wo_native_func_t)WO_SAFE_READ_MOVE_8;
            else
                created_closure->m_vm_func = WO_SAFE_READ_MOVE_4;

            for (size_t i = 0; i < (size_t)closure_arg_count; i++)
            {
                auto* arg_val = ++rt_sp;
                created_closure->m_closure_args[i].set_val(arg_val);
            }
            opnum1->set_gcunit<wo::value::valuetype::closure_type>(created_closure);
            return rt_sp;
        }
        inline static value* make_array_impl(value* opnum1, uint16_t size, value* rt_sp)
        {
            opnum1->set_gcunit<value::valuetype::array_type>(
                array_t::gc_new<gcbase::gctype::young>((size_t)size));

            for (size_t i = 0; i < (size_t)size; i++)
            {
                auto* arr_val = ++rt_sp;
                opnum1->array->at(size - i - 1).set_val(arr_val);
            }
            return rt_sp;
        }
        inline static value* make_map_impl(value* opnum1, uint16_t size, value* rt_sp)
        {
            opnum1->set_gcunit<value::valuetype::dict_type>(
                dict_t::gc_new<gcbase::gctype::young>());

            for (size_t i = 0; i < (size_t)size; i++)
            {
                value* val = ++rt_sp;
                value* key = ++rt_sp;
                (*opnum1->dict)[*key].set_val(val);
            }
            return rt_sp;
        }
        inline static value* make_struct_impl(value* opnum1, uint16_t size, value* rt_sp)
        {
            opnum1->set_gcunit<value::valuetype::struct_type>(
                struct_t::gc_new<gcbase::gctype::young>(size));

            for (size_t i = 0; i < size; i++)
                opnum1->structs->m_values[size - i - 1].set_val(rt_sp + 1 + i);

            return rt_sp + size;
        }
        inline static void packargs_impl(value* opnum1, uint16_t argcount, value* tc, value* rt_bp, uint16_t skip_closure_arg_count)
        {
            auto* packed_array = array_t::gc_new<gcbase::gctype::young>();
            packed_array->resize((size_t)tc->integer - (size_t)argcount);
            for (auto argindex = 0 + (size_t)argcount; argindex < (size_t)tc->integer; argindex++)
            {
                (*packed_array)[(size_t)argindex - (size_t)argcount].set_val(rt_bp + 2 + argindex + skip_closure_arg_count);
            }
            opnum1->set_gcunit<wo::value::valuetype::array_type>(packed_array);
        }
        inline static value* unpackargs_impl(vmbase* vm, value* opnum1, int32_t unpack_argc, value* tc, const byte_t* rt_ip, value* rt_sp, value* rt_bp)
        {
            if (opnum1->type == value::valuetype::struct_type)
            {
                auto* arg_tuple = opnum1->structs;
                gcbase::gc_read_guard gwg1(arg_tuple);
                if (unpack_argc > 0)
                {
                    if ((size_t)unpack_argc > (size_t)arg_tuple->m_count)
                    {
                        vm->ip = rt_ip;
                        vm->sp = rt_sp;
                        vm->bp = rt_bp;
                        wo_fail(WO_FAIL_INDEX_FAIL, "The number of arguments required for unpack exceeds the number of arguments in the given arguments-package.");
                    }
                    else
                    {
                        for (uint16_t i = (uint16_t)unpack_argc; i > 0; --i)
                            (rt_sp--)->set_val(&arg_tuple->m_values[i - 1]);
                    }
                }
                else
                {
                    if ((size_t)arg_tuple->m_count < (size_t)(-unpack_argc))
                    {
                        vm->ip = rt_ip;
                        vm->sp = rt_sp;
                        vm->bp = rt_bp;
                        wo_fail(WO_FAIL_INDEX_FAIL, "The number of arguments required for unpack exceeds the number of arguments in the given arguments-package.");
                    }
                    for (uint16_t i = arg_tuple->m_count; i > 0; --i)
                        (rt_sp--)->set_val(&arg_tuple->m_values[i - 1]);

                    tc->integer += (wo_integer_t)arg_tuple->m_count;
                }
            }
            else if (opnum1->type == value::valuetype::array_type)
            {
                if (unpack_argc > 0)
                {
                    auto* arg_array = opnum1->array;
                    gcbase::gc_read_guard gwg1(arg_array);

                    if ((size_t)unpack_argc > arg_array->size())
                    {
                        vm->ip = rt_ip;
                        vm->sp = rt_sp;
                        vm->bp = rt_bp;
                        wo_fail(WO_FAIL_INDEX_FAIL, "The number of arguments required for unpack exceeds the number of arguments in the given arguments-package.");
                    }
                    else
                    {
                        for (auto arg_idx = arg_array->rbegin() + (size_t)((wo_integer_t)arg_array->size() - unpack_argc);
                            arg_idx != arg_array->rend();
                            arg_idx++)
                            (rt_sp--)->set_val(&*arg_idx);
                    }
                }
                else
                {
                    auto* arg_array = opnum1->array;
                    gcbase::gc_read_guard gwg1(arg_array);

                    if (arg_array->size() < (size_t)(-unpack_argc))
                    {
                        vm->ip = rt_ip;
                        vm->sp = rt_sp;
                        vm->bp = rt_bp;
                        wo_fail(WO_FAIL_INDEX_FAIL, "The number of arguments required for unpack exceeds the number of arguments in the given arguments-package.");
                    }
                    for (auto arg_idx = arg_array->rbegin(); arg_idx != arg_array->rend(); arg_idx++)
                        (rt_sp--)->set_val(&*arg_idx);

                    tc->integer += arg_array->size();
                }
            }
            else
            {
                vm->ip = rt_ip;
                vm->sp = rt_sp;
                vm->bp = rt_bp;
                wo_fail(WO_FAIL_INDEX_FAIL, "Only valid array/struct can used in unpack.");
            }
            return rt_sp;
        }
        inline static const char* movcast_impl(value* opnum1, value* opnum2, value::valuetype aim_type)
        {
            if (aim_type == opnum2->type)
                opnum1->set_val(opnum2);
            else
                switch (aim_type)
                {
                case value::valuetype::integer_type:
                    opnum1->set_integer(wo_cast_int(std::launder(reinterpret_cast<wo_value>(opnum2)))); break;
                case value::valuetype::real_type:
                    opnum1->set_real(wo_cast_real(std::launder(reinterpret_cast<wo_value>(opnum2)))); break;
                case value::valuetype::handle_type:
                    opnum1->set_handle(wo_cast_handle(std::launder(reinterpret_cast<wo_value>(opnum2)))); break;
                case value::valuetype::string_type:
                    opnum1->set_string(wo_cast_string(std::launder(reinterpret_cast<wo_value>(opnum2)))); break;
                case value::valuetype::bool_type:
                    opnum1->set_bool(wo_cast_bool(std::launder(reinterpret_cast<wo_value>(opnum2)))); break;
                case value::valuetype::array_type:
                    return "Cannot cast this value to 'array'.";
                    break;
                case value::valuetype::dict_type:
                    return "Cannot cast this value to 'dict'.";
                    break;
                case value::valuetype::gchandle_type:
                    return "Cannot cast this value to 'gchandle'.";
                    break;
                default:
                    return "Unknown type.";
                }
            return nullptr;
        }
        // used for restoring local state
        template<typename T>
        struct _restore_raii
        {
            T& ot;
            T& nt;

            _restore_raii(T& _nt, T& _ot)
                : ot(_ot)
                , nt(_nt)
            {
                _nt = _ot;
            }

            ~_restore_raii()
            {
                ot = nt;
            }
        };

        template<int/* wo::platform_info::ArchType */ ARCH = wo::platform_info::ARCH_TYPE>
        void run_impl()
        {
            // Must not leave when run.
            wo_assert((this->vm_interrupt & vm_interrupt_type::LEAVE_INTERRUPT) == 0);
            wo_assert(_this_thread_vm == this);

            runtime_env* rt_env = env.get();
            const byte_t* rt_ip;
            value* global_begin = const_global_begin;
            value* reg_begin = register_mem_begin;
            value* const rt_cr = cr;

            _restore_raii _o1(rt_ip, ip);

#define WO_SIGNED_SHIFT(VAL) (((signed char)((unsigned char)(((unsigned char)(VAL))<<1)))>>1)

#define WO_ADDRESSING_N1 value * opnum1 = ((dr >> 1) ?\
                        (\
                            (WO_IPVAL & (1 << 7)) ?\
                            (bp + WO_SIGNED_SHIFT(WO_IPVAL_MOVE_1))\
                            :\
                            (WO_IPVAL_MOVE_1 + reg_begin)\
                            )\
                        :\
                        (\
                            WO_IPVAL_MOVE_4 + global_begin\
                        ))

#define WO_ADDRESSING_N2 value * opnum2 = ((dr & 0b01) ?\
                        (\
                            (WO_IPVAL & (1 << 7)) ?\
                            (bp + WO_SIGNED_SHIFT(WO_IPVAL_MOVE_1))\
                            :\
                            (WO_IPVAL_MOVE_1 + reg_begin)\
                            )\
                        :\
                        (\
                            WO_IPVAL_MOVE_4 + global_begin\
                        ))
#define WO_ADDRESSING_N3_REG_BPOFF value * opnum3 = \
                            (WO_IPVAL & (1 << 7)) ?\
                            (bp + WO_SIGNED_SHIFT(WO_IPVAL_MOVE_1))\
                            :\
                            (WO_IPVAL_MOVE_1 + reg_begin)

#define WO_VM_FAIL(ERRNO,ERRINFO) \
    do{ip = rt_ip;wo_fail(ERRNO,ERRINFO);continue;}while(0)
#if WO_ENABLE_RUNTIME_CHECK == 0
#define WO_VM_ASSERT(EXPR, REASON) wo_assert(EXPR, REASON)
#else
#define WO_VM_ASSERT(EXPR, REASON) \
    if(!(EXPR)){ip = rt_ip;wo_fail(WO_FAIL_UNEXPECTED, REASON);continue;}
#endif

            byte_t opcode_dr = (byte_t)(instruct::abrt << (uint8_t)2);
            instruct::opcode opcode = (instruct::opcode)(opcode_dr & 0b11111100u);
            unsigned dr = opcode_dr & 0b00000011u;

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
                        WO_ADDRESSING_N1;
                        (sp--)->set_val(opnum1);
                    }
                    else
                    {
                        uint16_t psh_repeat = WO_IPVAL_MOVE_2;
                        sp -= psh_repeat;
                    }
                    break;
                }
                case instruct::opcode::pop:
                {
                    if (dr & 0b01)
                    {
                        WO_ADDRESSING_N1;
                        opnum1->set_val((++sp));
                    }
                    else
                        sp += WO_IPVAL_MOVE_2;
                    break;
                }
                case instruct::opcode::addi:
                {
                    WO_ADDRESSING_N1;
                    WO_ADDRESSING_N2;

                    WO_VM_ASSERT(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::integer_type, "Operand should be integer in 'addi'.");

                    opnum1->integer += opnum2->integer;
                    break;
                }
                case instruct::opcode::subi:
                {
                    WO_ADDRESSING_N1;
                    WO_ADDRESSING_N2;

                    WO_VM_ASSERT(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::integer_type, "Operand should be integer in 'subi'.");

                    opnum1->integer -= opnum2->integer;
                    break;
                }
                case instruct::opcode::muli:
                {
                    WO_ADDRESSING_N1;
                    WO_ADDRESSING_N2;

                    WO_VM_ASSERT(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::integer_type, "Operand should be integer in 'muli'.");

                    opnum1->integer *= opnum2->integer;
                    break;
                }
                case instruct::opcode::divi:
                {
                    WO_ADDRESSING_N1;
                    WO_ADDRESSING_N2;

                    WO_VM_ASSERT(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::integer_type, "Operand should be integer in 'divi'.");

                    WO_VM_ASSERT(opnum2->integer != 0, "The divisor cannot be 0.");
                    WO_VM_ASSERT(opnum2->integer != -1 || opnum1->integer != INT64_MIN, "Division overflow.");

                    opnum1->integer /= opnum2->integer;
                    break;
                }
                case instruct::opcode::modi:
                {
                    WO_ADDRESSING_N1;
                    WO_ADDRESSING_N2;

                    WO_VM_ASSERT(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::integer_type, "Operand should be integer in 'modi'.");

                    WO_VM_ASSERT(opnum2->integer != 0, "The divisor cannot be 0.");
                    WO_VM_ASSERT(opnum2->integer != -1 || opnum1->integer != INT64_MIN, "Division overflow.");

                    opnum1->integer %= opnum2->integer;
                    break;
                }

                case instruct::opcode::addr:
                {
                    WO_ADDRESSING_N1;
                    WO_ADDRESSING_N2;

                    WO_VM_ASSERT(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::real_type, "Operand should be real in 'addr'.");

                    opnum1->real += opnum2->real;
                    break;
                }
                case instruct::opcode::subr:
                {
                    WO_ADDRESSING_N1;
                    WO_ADDRESSING_N2;

                    WO_VM_ASSERT(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::real_type, "Operand should be real in 'subr'.");

                    opnum1->real -= opnum2->real;
                    break;
                }
                case instruct::opcode::mulr:
                {
                    WO_ADDRESSING_N1;
                    WO_ADDRESSING_N2;

                    WO_VM_ASSERT(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::real_type, "Operand should be real in 'mulr'.");

                    opnum1->real *= opnum2->real;
                    break;
                }
                case instruct::opcode::divr:
                {
                    WO_ADDRESSING_N1;
                    WO_ADDRESSING_N2;

                    WO_VM_ASSERT(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::real_type, "Operand should be real in 'divr'.");

                    opnum1->real /= opnum2->real;
                    break;
                }
                case instruct::opcode::modr:
                {
                    WO_ADDRESSING_N1;
                    WO_ADDRESSING_N2;

                    WO_VM_ASSERT(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::real_type, "Operand should be real in 'modr'.");

                    opnum1->real = fmod(opnum1->real, opnum2->real);
                    break;
                }

                case instruct::opcode::addh:
                {
                    WO_ADDRESSING_N1;
                    WO_ADDRESSING_N2;

                    WO_VM_ASSERT(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::handle_type, "Operand should be handle in 'addh'.");

                    opnum1->handle += opnum2->handle;
                    break;
                }
                case instruct::opcode::subh:
                {
                    WO_ADDRESSING_N1;
                    WO_ADDRESSING_N2;

                    WO_VM_ASSERT(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::handle_type, "Operand should be handle in 'subh'.");

                    opnum1->handle -= opnum2->handle;
                    break;
                }

                case instruct::opcode::adds:
                {
                    WO_ADDRESSING_N1;
                    WO_ADDRESSING_N2;

                    WO_VM_ASSERT(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::string_type, "Operand should be string in 'adds'.");

                    opnum1->set_gcunit<wo::value::valuetype::string_type>(
                        string_t::gc_new<gcbase::gctype::young>(*opnum1->string + *opnum2->string));
                    break;
                }
                /// OPERATE
                case instruct::opcode::mov:
                {
                    WO_ADDRESSING_N1;
                    WO_ADDRESSING_N2;

                    opnum1->set_val(opnum2);
                    break;
                }
                case instruct::opcode::movcast:
                {
                    WO_ADDRESSING_N1;
                    WO_ADDRESSING_N2;
                    value::valuetype aim_type = static_cast<value::valuetype>(WO_IPVAL_MOVE_1);

                    if (auto* err = movcast_impl(opnum1, opnum2, aim_type))
                        WO_VM_FAIL(WO_FAIL_TYPE_FAIL, err);

                    break;
                }
                case instruct::opcode::typeas:
                {
                    WO_ADDRESSING_N1;
                    if (dr & 0b01)
                        rt_cr->set_bool(opnum1->type == (value::valuetype)(WO_IPVAL_MOVE_1));
                    else
                        if (opnum1->type != (value::valuetype)(WO_IPVAL_MOVE_1))
                            WO_VM_FAIL(WO_FAIL_TYPE_FAIL, "The given value is not the same as the requested type.");
                    break;
                }
                case instruct::opcode::lds:
                {
                    WO_ADDRESSING_N1;
                    WO_ADDRESSING_N2;

                    WO_VM_ASSERT(opnum2->type == value::valuetype::integer_type, "Operand 2 should be integer in 'lds'.");
                    opnum1->set_val(bp + opnum2->integer);
                    break;
                }
                case instruct::opcode::sts:
                {
                    WO_ADDRESSING_N1;
                    WO_ADDRESSING_N2;

                    WO_VM_ASSERT(opnum2->type == value::valuetype::integer_type, "Operand 2 should be integer in 'sts'.");
                    (bp + opnum2->integer)->set_val(opnum1);
                    break;
                }
                case instruct::opcode::equb:
                {
                    WO_ADDRESSING_N1;
                    WO_ADDRESSING_N2;

                    rt_cr->set_bool(opnum1->integer == opnum2->integer);

                    break;
                }
                case instruct::opcode::nequb:
                {
                    WO_ADDRESSING_N1;
                    WO_ADDRESSING_N2;

                    rt_cr->set_bool(opnum1->integer != opnum2->integer);
                    break;
                }
                case instruct::opcode::equr:
                {
                    WO_ADDRESSING_N1;
                    WO_ADDRESSING_N2;
                    WO_VM_ASSERT(opnum1->type == opnum2->type && opnum1->type == value::valuetype::real_type,
                        "Operand should be real in 'equr'.");
                    rt_cr->set_bool(opnum1->real == opnum2->real);

                    break;
                }
                case instruct::opcode::nequr:
                {
                    WO_ADDRESSING_N1;
                    WO_ADDRESSING_N2;
                    WO_VM_ASSERT(opnum1->type == opnum2->type && opnum1->type == value::valuetype::real_type,
                        "Operand should be real in 'nequr'.");
                    rt_cr->set_bool(opnum1->real != opnum2->real);
                    break;
                }
                case instruct::opcode::equs:
                {
                    WO_ADDRESSING_N1;
                    WO_ADDRESSING_N2;
                    WO_VM_ASSERT(opnum1->type == opnum2->type && opnum1->type == value::valuetype::string_type,
                        "Operand should be string in 'equs'.");

                    if (opnum1->string == opnum2->string)
                        rt_cr->set_bool(true);
                    else
                        rt_cr->set_bool(*opnum1->string == *opnum2->string);

                    break;
                }
                case instruct::opcode::nequs:
                {
                    WO_ADDRESSING_N1;
                    WO_ADDRESSING_N2;
                    WO_VM_ASSERT(opnum1->type == opnum2->type && opnum1->type == value::valuetype::string_type,
                        "Operand should be string in 'nequs'.");

                    if (opnum1->string == opnum2->string)
                        rt_cr->set_bool(false);
                    else
                        rt_cr->set_bool(*opnum1->string != *opnum2->string);
                    break;
                }
                case instruct::opcode::land:
                {
                    WO_ADDRESSING_N1;
                    WO_ADDRESSING_N2;

                    rt_cr->set_bool(opnum1->integer && opnum2->integer);

                    break;
                }
                case instruct::opcode::lor:
                {
                    WO_ADDRESSING_N1;
                    WO_ADDRESSING_N2;

                    rt_cr->set_bool(opnum1->integer || opnum2->integer);

                    break;
                }
                case instruct::opcode::lti:
                {
                    WO_ADDRESSING_N1;
                    WO_ADDRESSING_N2;

                    WO_VM_ASSERT(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::integer_type,
                        "Operand should be integer in 'lti'.");

                    rt_cr->set_bool(opnum1->integer < opnum2->integer);

                    break;
                }
                case instruct::opcode::gti:
                {
                    WO_ADDRESSING_N1;
                    WO_ADDRESSING_N2;

                    WO_VM_ASSERT(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::integer_type,
                        "Operand should be integer in 'gti'.");

                    rt_cr->set_bool(opnum1->integer > opnum2->integer);

                    break;
                }
                case instruct::opcode::elti:
                {
                    WO_ADDRESSING_N1;
                    WO_ADDRESSING_N2;

                    WO_VM_ASSERT(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::integer_type,
                        "Operand should be integer in 'elti'.");

                    rt_cr->set_bool(opnum1->integer <= opnum2->integer);

                    break;
                }
                case instruct::opcode::egti:
                {
                    WO_ADDRESSING_N1;
                    WO_ADDRESSING_N2;

                    WO_VM_ASSERT(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::integer_type,
                        "Operand should be integer in 'egti'.");

                    rt_cr->set_bool(opnum1->integer >= opnum2->integer);

                    break;
                }
                case instruct::opcode::ltr:
                {
                    WO_ADDRESSING_N1;
                    WO_ADDRESSING_N2;

                    WO_VM_ASSERT(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::real_type,
                        "Operand should be real in 'ltr'.");

                    rt_cr->set_bool(opnum1->real < opnum2->real);

                    break;
                }
                case instruct::opcode::gtr:
                {
                    WO_ADDRESSING_N1;
                    WO_ADDRESSING_N2;

                    WO_VM_ASSERT(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::real_type,
                        "Operand should be real in 'gtr'.");

                    rt_cr->set_bool(opnum1->real > opnum2->real);

                    break;
                }
                case instruct::opcode::eltr:
                {
                    WO_ADDRESSING_N1;
                    WO_ADDRESSING_N2;

                    WO_VM_ASSERT(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::real_type,
                        "Operand should be real in 'eltr'.");

                    rt_cr->set_bool(opnum1->real <= opnum2->real);

                    break;
                }
                case instruct::opcode::egtr:
                {
                    WO_ADDRESSING_N1;
                    WO_ADDRESSING_N2;

                    WO_VM_ASSERT(opnum1->type == opnum2->type
                        && opnum1->type == value::valuetype::real_type,
                        "Operand should be real in 'egtr'.");

                    rt_cr->set_bool(opnum1->real >= opnum2->real);

                    break;
                }
                case instruct::opcode::ltx:
                {
                    WO_ADDRESSING_N1;
                    WO_ADDRESSING_N2;

                    WO_VM_ASSERT(opnum1->type == opnum2->type,
                        "Operand type should be same in 'ltx'.");

                    ltx_impl(rt_cr, opnum1, opnum2);

                    break;
                }
                case instruct::opcode::gtx:
                {
                    WO_ADDRESSING_N1;
                    WO_ADDRESSING_N2;

                    WO_VM_ASSERT(opnum1->type == opnum2->type,
                        "Operand type should be same in 'gtx'.");

                    gtx_impl(rt_cr, opnum1, opnum2);

                    break;
                }
                case instruct::opcode::eltx:
                {
                    WO_ADDRESSING_N1;
                    WO_ADDRESSING_N2;

                    WO_VM_ASSERT(opnum1->type == opnum2->type,
                        "Operand type should be same in 'eltx'.");

                    eltx_impl(rt_cr, opnum1, opnum2);

                    break;
                }
                case instruct::opcode::egtx:
                {
                    WO_ADDRESSING_N1;
                    WO_ADDRESSING_N2;

                    WO_VM_ASSERT(opnum1->type == opnum2->type,
                        "Operand type should be same in 'egtx'.");

                    egtx_impl(rt_cr, opnum1, opnum2);

                    break;
                }
                case instruct::opcode::ret:
                {
                    WO_VM_ASSERT((bp + 1)->type == value::valuetype::callstack
                        || (bp + 1)->type == value::valuetype::nativecallstack,
                        "Found broken stack in 'ret'.");

                    uint16_t pop_count = dr ? WO_IPVAL_MOVE_2 : 0;

                    if ((++bp)->type == value::valuetype::nativecallstack)
                    {
                        sp = bp;
                        sp += pop_count;
                        return; // last stack is native_func, just do return; stack balance should be keeped by invoker
                    }

                    value* stored_bp = stack_mem_begin - bp->vmcallstack.bp;
                    rt_ip = rt_env->rt_codes + bp->vmcallstack.ret_ip;
                    sp = bp;
                    bp = stored_bp;

                    sp += pop_count;
                    // TODO If rt_ip is outof range, return...

                    break;
                }
                case instruct::opcode::call:
                {
                    WO_ADDRESSING_N1;

                    WO_VM_ASSERT(0 != opnum1->handle && nullptr != opnum1->closure,
                        "Cannot invoke null function in 'call'.");

                    if (opnum1->type == value::valuetype::closure_type)
                    {
                        // Call closure, unpack closure captured arguments.
                        // 
                        // NOTE: Closure arguments should be poped by closure function it self.
                        //       Can use ret(n) to pop arguments when call.
                        for (auto idx = opnum1->closure->m_closure_args_count; idx > 0; --idx)
                            (sp--)->set_val(&opnum1->closure->m_closure_args[idx - 1]);
                    }

                    sp->type = value::valuetype::callstack;
                    sp->vmcallstack.ret_ip = (uint32_t)(rt_ip - rt_env->rt_codes);
                    sp->vmcallstack.bp = (uint32_t)(stack_mem_begin - bp);
                    bp = --sp;

                    if (opnum1->type == value::valuetype::handle_type)
                    {
                        // Call native
                        wo_extern_native_func_t call_aim_native_func = (wo_extern_native_func_t)(opnum1->handle);
                        ip = std::launder(reinterpret_cast<byte_t*>(call_aim_native_func));
                        rt_cr->set_nil();

                        switch (call_aim_native_func(
                            std::launder(reinterpret_cast<wo_vm>(this)),
                            std::launder(reinterpret_cast<wo_value>(sp + 2))))
                        {
                        case wo_result_t::WO_API_NORMAL:
                        {
                            WO_VM_ASSERT((bp + 1)->type == value::valuetype::callstack,
                                "Found broken stack in 'call'.");
                            value* stored_bp = stack_mem_begin - (++bp)->vmcallstack.bp;
                            sp = bp;
                            bp = stored_bp;
                            break;
                        }
                        case wo_result_t::WO_API_RESYNC:
                        {
                            rt_ip = this->ip;
                            break;
                        }
                        }
                    }
                    else if (opnum1->type == value::valuetype::integer_type)
                    {
                        rt_ip = rt_env->rt_codes + opnum1->integer;
                    }
                    else
                    {
                        WO_VM_ASSERT(opnum1->type == value::valuetype::closure_type,
                            "Unexpected invoke target type in 'call'.");

                        auto* closure = opnum1->closure;

                        if (closure->m_native_call)
                        {
                            switch (closure->m_native_func(
                                std::launder(reinterpret_cast<wo_vm>(this)),
                                std::launder((reinterpret_cast<wo_value>(sp + 2)))))
                            {
                            case wo_result_t::WO_API_NORMAL:
                            {
                                WO_VM_ASSERT((bp + 1)->type == value::valuetype::callstack,
                                    "Found broken stack in 'call'.");
                                value* stored_bp = stack_mem_begin - (++bp)->vmcallstack.bp;
                                // Here to invoke jit closure, jit function cannot pop captured arguments,
                                // So we pop them here.
                                sp = bp + closure->m_closure_args_count;
                                bp = stored_bp;
                                break;
                            }
                            case wo_result_t::WO_API_RESYNC:
                            {
                                rt_ip = this->ip;
                                break;
                            }
                            }
                        }
                        else
                            rt_ip = rt_env->rt_codes + closure->m_vm_func;
                    }
                    break;
                }
                case instruct::opcode::calln:
                {
                    WO_VM_ASSERT(dr == 0b11 || dr == 0b01 || dr == 0b00,
                        "Found broken ir-code in 'calln'.");

                    if (dr)
                    {
                        // Call native
                        wo_extern_native_func_t call_aim_native_func = (wo_extern_native_func_t)(WO_IPVAL_MOVE_8);

                        sp->type = value::valuetype::callstack;
                        sp->vmcallstack.ret_ip = (uint32_t)(rt_ip - rt_env->rt_codes);
                        sp->vmcallstack.bp = (uint32_t)(stack_mem_begin - bp);
                        bp = --sp;
                        rt_cr->set_nil();

                        ip = std::launder(reinterpret_cast<byte_t*>(call_aim_native_func));

                        wo_result_t api;
                        if (dr & 0b10)
                            api = call_aim_native_func(
                                std::launder(reinterpret_cast<wo_vm>(this)),
                                std::launder(reinterpret_cast<wo_value>(sp + 2)));
                        else
                        {
                            wo_assure(wo_leave_gcguard(std::launder(reinterpret_cast<wo_vm>(this))));
                            api = call_aim_native_func(
                                std::launder(reinterpret_cast<wo_vm>(this)),
                                std::launder(reinterpret_cast<wo_value>(sp + 2)));
                            wo_assure(wo_enter_gcguard(std::launder(reinterpret_cast<wo_vm>(this))));
                        }
                        switch (api)
                        {
                        case wo_result_t::WO_API_NORMAL:
                        {
                            WO_VM_ASSERT((bp + 1)->type == value::valuetype::callstack,
                                "Found broken stack in 'calln'.");
                            value* stored_bp = stack_mem_begin - (++bp)->vmcallstack.bp;
                            sp = bp;
                            bp = stored_bp;
                            break;
                        }
                        case wo_result_t::WO_API_RESYNC:
                        {
                            rt_ip = this->ip;
                            break;
                        }
                        }
                    }
                    else
                    {
                        const byte_t* aimplace = rt_env->rt_codes + WO_IPVAL_MOVE_4;
                        rt_ip += 4; // skip reserved place.

                        sp->type = value::valuetype::callstack;
                        sp->vmcallstack.ret_ip = (uint32_t)(rt_ip - rt_env->rt_codes);
                        sp->vmcallstack.bp = (uint32_t)(stack_mem_begin - bp);
                        bp = --sp;

                        rt_ip = aimplace;

                    }
                    break;
                }
                case instruct::opcode::jmp:
                {
                    auto* restore_ip = rt_env->rt_codes + WO_IPVAL_MOVE_4;
                    rt_ip = restore_ip;
                    break;
                }
                case instruct::opcode::jt:
                {
                    uint32_t aimplace = WO_IPVAL_MOVE_4;
                    if (rt_cr->integer)
                        rt_ip = rt_env->rt_codes + aimplace;
                    break;
                }
                case instruct::opcode::jf:
                {
                    uint32_t aimplace = WO_IPVAL_MOVE_4;
                    if (!rt_cr->integer)
                        rt_ip = rt_env->rt_codes + aimplace;
                    break;
                }
                case instruct::opcode::mkstruct:
                {
                    WO_ADDRESSING_N1; // Aim
                    uint16_t size = WO_IPVAL_MOVE_2;

                    sp = make_struct_impl(opnum1, size, sp);
                    break;
                }
                case instruct::opcode::idstruct:
                {
                    WO_ADDRESSING_N1; // Aim
                    WO_ADDRESSING_N2; // Struct
                    uint16_t offset = WO_IPVAL_MOVE_2;

                    WO_VM_ASSERT(opnum2->type == value::valuetype::struct_type,
                        "Cannot index non-struct value in 'idstruct'.");
                    WO_VM_ASSERT(opnum2->structs != nullptr,
                        "Unable to index null in 'idstruct'.");
                    WO_VM_ASSERT(offset < opnum2->structs->m_count,
                        "Index out of range in 'idstruct'.");

                    gcbase::gc_read_guard gwg1(opnum2->structs);
                    opnum1->set_val(&opnum2->structs->m_values[offset]);

                    break;
                }
                case instruct::opcode::jnequb:
                {
                    WO_ADDRESSING_N1;
                    uint32_t offset = WO_IPVAL_MOVE_4;

                    if (opnum1->integer != rt_cr->integer)
                    {
                        auto* restore_ip = rt_env->rt_codes + offset;
                        rt_ip = restore_ip;
                    }
                    break;
                }
                case instruct::opcode::mkarr:
                {
                    WO_ADDRESSING_N1;
                    uint16_t size = WO_IPVAL_MOVE_2;

                    sp = make_array_impl(opnum1, size, sp);
                    break;
                }
                case instruct::opcode::mkmap:
                {
                    WO_ADDRESSING_N1;
                    uint16_t size = WO_IPVAL_MOVE_2;

                    sp = make_map_impl(opnum1, size, sp);
                    break;
                }
                case instruct::opcode::idarr:
                {
                    WO_ADDRESSING_N1;
                    WO_ADDRESSING_N2;

                    WO_VM_ASSERT(nullptr != opnum1->gcunit,
                        "Unable to index null in 'idarr'.");
                    WO_VM_ASSERT(opnum1->type == value::valuetype::array_type,
                        "Cannot index non-array value in 'idarr'.");
                    WO_VM_ASSERT(opnum2->type == value::valuetype::integer_type,
                        "Cannot index array by non-integer value in 'idarr'.");
                    gcbase::gc_read_guard gwg1(opnum1->gcunit);

                    // ATTENTION: `_vmjitcall_idarr` HAS SAME LOGIC, NEED UPDATE SAME TIME.
                    wo_integer_t index = opnum2->integer;
                    if (opnum2->integer < 0)
                        index = (wo_integer_t)opnum1->array->size() + opnum2->integer;
                    if ((size_t)index >= opnum1->array->size())
                    {
                        rt_cr->set_nil();
                        WO_VM_FAIL(WO_FAIL_INDEX_FAIL, "Index out of range.");
                    }
                    else
                    {
                        rt_cr->set_val(&opnum1->array->at((size_t)index));
                    }
                    break;
                }
                case instruct::opcode::iddict:
                {
                    WO_ADDRESSING_N1;
                    WO_ADDRESSING_N2;

                    WO_VM_ASSERT(nullptr != opnum1->gcunit,
                        "Unable to index null in 'iddict'.");
                    WO_VM_ASSERT(opnum1->type == value::valuetype::dict_type,
                        "Unable to index non-dict value in 'iddict'.");

                    gcbase::gc_read_guard gwg1(opnum1->gcunit);

                    auto fnd = opnum1->dict->find(*opnum2);
                    if (fnd != opnum1->dict->end())
                    {
                        rt_cr->set_val(&fnd->second);
                        break;
                    }
                    else
                        WO_VM_FAIL(WO_FAIL_INDEX_FAIL, "No such key in current dict.");

                    break;
                }
                case instruct::opcode::sidmap:
                {
                    WO_ADDRESSING_N1;
                    WO_ADDRESSING_N2;
                    WO_ADDRESSING_N3_REG_BPOFF;

                    WO_VM_ASSERT(nullptr != opnum1->gcunit,
                        "Unable to index null in 'sidmap'.");
                    WO_VM_ASSERT(opnum1->type == value::valuetype::dict_type,
                        "Unable to index non-map value in 'sidmap'.");

                    gcbase::gc_write_guard gwg1(opnum1->gcunit);

                    auto* result = &(*opnum1->dict)[*opnum2];
                    if (wo::gc::gc_is_marking())
                        wo::gcbase::write_barrier(result);
                    result->set_val(opnum3);

                    break;
                }
                case instruct::opcode::siddict:
                {
                    WO_ADDRESSING_N1;
                    WO_ADDRESSING_N2;
                    WO_ADDRESSING_N3_REG_BPOFF;

                    WO_VM_ASSERT(nullptr != opnum1->gcunit,
                        "Unable to index null in 'siddict'.");
                    WO_VM_ASSERT(opnum1->type == value::valuetype::dict_type,
                        "Unable to index non-dict value in 'siddict'.");

                    gcbase::gc_write_guard gwg1(opnum1->gcunit);

                    auto fnd = opnum1->dict->find(*opnum2);
                    if (fnd != opnum1->dict->end())
                    {
                        auto* result = &fnd->second;
                        if (wo::gc::gc_is_marking())
                            wo::gcbase::write_barrier(result);
                        result->set_val(opnum3);
                        break;
                    }
                    else
                        WO_VM_FAIL(WO_FAIL_INDEX_FAIL, "No such key in current dict.");

                    break;
                }
                case instruct::opcode::sidarr:
                {
                    WO_ADDRESSING_N1;
                    WO_ADDRESSING_N2;
                    WO_ADDRESSING_N3_REG_BPOFF;

                    WO_VM_ASSERT(nullptr != opnum1->gcunit,
                        "Unable to index null in 'sidarr'.");
                    WO_VM_ASSERT(opnum1->type == value::valuetype::array_type,
                        "Unable to index non-array value in 'sidarr'.");
                    WO_VM_ASSERT(opnum2->type == value::valuetype::integer_type,
                        "Unable to index array by non-integer value in 'sidarr'.");

                    gcbase::gc_write_guard gwg1(opnum1->gcunit);

                    wo_integer_t index = opnum2->integer;
                    if (opnum2->integer < 0)
                        index = (wo_integer_t)opnum1->array->size() + opnum2->integer;
                    if ((size_t)index >= opnum1->array->size())
                    {
                        WO_VM_FAIL(WO_FAIL_INDEX_FAIL, "Index out of range.");
                    }
                    else
                    {
                        auto* result = &opnum1->array->at((size_t)index);
                        if (wo::gc::gc_is_marking())
                            wo::gcbase::write_barrier(result);
                        result->set_val(opnum3);
                    }
                    break;
                }
                case instruct::opcode::sidstruct:
                {
                    WO_ADDRESSING_N1;
                    WO_ADDRESSING_N2;
                    uint16_t offset = WO_IPVAL_MOVE_2;

                    WO_VM_ASSERT(nullptr != opnum1->structs,
                        "Unable to index null in 'sidstruct'.");
                    WO_VM_ASSERT(opnum1->type == value::valuetype::struct_type,
                        "Unable to index non-struct value in 'sidstruct'.");
                    WO_VM_ASSERT(offset < opnum1->structs->m_count,
                        "Index out of range in 'sidstruct'.");

                    gcbase::gc_write_guard gwg1(opnum1->gcunit);

                    auto* result = &opnum1->structs->m_values[offset];
                    if (wo::gc::gc_is_marking())
                        wo::gcbase::write_barrier(result);
                    result->set_val(opnum2);

                    break;
                }
                case instruct::opcode::idstr:
                {
                    WO_ADDRESSING_N1;
                    WO_ADDRESSING_N2;

                    WO_VM_ASSERT(nullptr != opnum1->gcunit,
                        "Unable to index null in 'idstr'.");

                    WO_VM_ASSERT(opnum2->type == value::valuetype::integer_type,
                        "Unable to index string by non-integer value in 'idstr'.");
                    wchar_t out_str = wo_strn_get_char(opnum1->string->c_str(), opnum1->string->size(), opnum2->integer);

                    rt_cr->set_integer((wo_integer_t)(wo_handle_t)out_str);
                    break;
                }
                case instruct::opcode::mkunion:
                {
                    WO_ADDRESSING_N1; // aim
                    WO_ADDRESSING_N2; // data
                    uint16_t id = WO_IPVAL_MOVE_2;

                    make_union_impl(opnum1, opnum2, id);

                    break;
                }
                case instruct::opcode::mkclos:
                {
                    WO_VM_ASSERT((dr & 0b01) == 0,
                        "Found broken ir-code in 'mkclos'.");

                    if constexpr (ARCH & platform_info::ArchType::X86)
                        sp = make_closure_fast_impl(rt_cr, rt_ip, sp);
                    else
                        sp = make_closure_safe_impl(rt_cr, rt_ip, sp);

                    rt_ip += (2 + 8);

                    break;
                }
                case instruct::unpackargs:
                {
                    WO_ADDRESSING_N1;
                    auto unpack_argc_unsigned = WO_IPVAL_MOVE_4;

                    sp = unpackargs_impl(
                        this, opnum1, reinterpret_cast<int32_t&>(unpack_argc_unsigned),
                        tc, rt_ip, sp, bp);
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
                        case instruct::extern_opcode_page_0::panic:
                        {
                            WO_ADDRESSING_N1; // data

                            ip = rt_ip;
                            wo_fail(WO_FAIL_UNEXPECTED,
                                "%s", wo_cast_string(std::launder(reinterpret_cast<wo_value>(opnum1))));
                            break;
                        }
                        case instruct::extern_opcode_page_0::packargs:
                        {
                            WO_ADDRESSING_N1;
                            uint16_t this_function_arg_count = WO_IPVAL_MOVE_2;
                            uint16_t skip_closure_arg_count = WO_IPVAL_MOVE_2;

                            packargs_impl(opnum1, this_function_arg_count, tc, bp, skip_closure_arg_count);
                            break;
                        }
                        case instruct::extern_opcode_page_0::cdivilr:
                        {
                            WO_ADDRESSING_N1;
                            WO_ADDRESSING_N2;

                            if (opnum2->integer == 0)
                                WO_VM_FAIL(WO_FAIL_UNEXPECTED, "The divisor cannot be 0.");
                            else if (opnum2->integer == -1 && opnum1->integer == INT64_MIN)
                                WO_VM_FAIL(WO_FAIL_UNEXPECTED, "Division overflow.");

                            break;
                        }
                        case instruct::extern_opcode_page_0::cdivil:
                        {
                            WO_ADDRESSING_N1;

                            if (opnum1->integer == INT64_MIN)
                                WO_VM_FAIL(WO_FAIL_UNEXPECTED, "Division overflow.");

                            break;
                        }
                        case instruct::extern_opcode_page_0::cdivirz:
                        {
                            WO_ADDRESSING_N1;

                            if (opnum1->integer == 0)
                                WO_VM_FAIL(WO_FAIL_UNEXPECTED, "The divisor cannot be 0.");

                            break;
                        }
                        case instruct::extern_opcode_page_0::cdivir:
                        {
                            WO_ADDRESSING_N1;

                            if (opnum1->integer == 0)
                                WO_VM_FAIL(WO_FAIL_UNEXPECTED, "The divisor cannot be 0.");
                            else if (opnum1->integer == -1)
                                WO_VM_FAIL(WO_FAIL_UNEXPECTED, "Division overflow.");

                            break;
                        }
                        default:
                            wo_error("Unknown instruct.");
                            break;
                        }
                        break;
                    case 1:     // extern-opcode-page-1
                        wo_error("Invalid instruct page(empty page 1).");
                        break;
                    case 2:     // extern-opcode-page-2
                        wo_error("Invalid instruct page(empty page 2).");
                        break;
                    case 3:     // extern-opcode-page-3
                        wo_error("Invalid instruct page(flag page).");
                        break;
                    default:
                        wo_error("Unknown extern-opcode-page.");
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
                        wo_error("executed 'abrt'.");
                }
                default:
                {
                    --rt_ip;    // Move back one command.

                    auto interrupt_state = vm_interrupt.load();

                    if (interrupt_state & vm_interrupt_type::GC_INTERRUPT)
                        gc_checkpoint();

                    if (interrupt_state & vm_interrupt_type::GC_HANGUP_INTERRUPT)
                    {
                        if (clear_interrupt(vm_interrupt_type::GC_HANGUP_INTERRUPT))
                            hangup();
                    }
                    else if (interrupt_state & vm_interrupt_type::ABORT_INTERRUPT)
                    {
                        // ABORTED VM WILL NOT ABLE TO RUN AGAIN, SO DO NOT
                        // CLEAR ABORT_INTERRUPT
                        return;
                    }
                    else if (interrupt_state & vm_interrupt_type::BR_YIELD_INTERRUPT)
                    {
                        wo_assure(clear_interrupt(vm_interrupt_type::BR_YIELD_INTERRUPT));
                        if (get_br_yieldable())
                        {
                            mark_br_yield();
                            return;
                        }
                        else
                            wo_fail(WO_FAIL_NOT_SUPPORT, "BR_YIELD_INTERRUPT only work at br_yieldable vm.");
                    }
                    else if (interrupt_state & vm_interrupt_type::LEAVE_INTERRUPT)
                    {
                        // That should not be happend...
                        wo_error("Virtual machine handled a LEAVE_INTERRUPT.");
                    }
                    else if (interrupt_state & vm_interrupt_type::PENDING_INTERRUPT)
                    {
                        // That should not be happend...
                        wo_error("Virtual machine handled a PENDING_INTERRUPT.");
                    }
                    else if (interrupt_state & vm_interrupt_type::DETACH_DEBUGGEE_INTERRUPT)
                    {
                        if (clear_interrupt(vm_interrupt_type::DETACH_DEBUGGEE_INTERRUPT))
                            clear_interrupt(vm_interrupt_type::DEBUG_INTERRUPT);
                    }
                    // ATTENTION: it should be last interrupt..
                    else if (interrupt_state & vm_interrupt_type::DEBUG_INTERRUPT)
                    {
                        rtopcode = opcode;

                        ip = rt_ip;
                        if (attaching_debuggee)
                        {
                            // check debuggee here
                            wo_assure(wo_leave_gcguard(std::launder(reinterpret_cast<wo_vm>(this))));
                            attaching_debuggee->_vm_invoke_debuggee(this);
                            wo_assure(wo_enter_gcguard(std::launder(reinterpret_cast<wo_vm>(this))));
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
#undef WO_VM_FAIL
#undef WO_ADDRESSING_N2
#undef WO_ADDRESSING_N1
#undef WO_ADDRESSING_N2
#undef WO_ADDRESSING_N1
#undef WO_SIGNED_SHIFT
#undef WO_IPVAL_MOVE_8
#undef WO_IPVAL_MOVE_4
#undef WO_IPVAL_MOVE_2
#undef WO_IPVAL_MOVE_1
#undef WO_IPVAL
        }
    };
}

#undef WO_READY_EXCEPTION_HANDLE
