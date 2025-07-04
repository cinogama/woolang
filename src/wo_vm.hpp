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
#include <memory>
#include <optional>

#if WO_BUILD_WITH_MINGW
#   include <mingw.thread.h>
#   include <mingw.mutex.h>
#   include <mingw.shared_mutex.h>
#   include <mingw.condition_variable.h>
#endif

namespace wo
{
    class vmbase;

    class vm_debuggee_bridge_base
    {
        inline static std::mutex _abandon_debuggees_mx;
        inline static std::vector<vm_debuggee_bridge_base*> _abandon_debuggees;

        std::mutex _debug_entry_guard_block_mx;
    public:
        vm_debuggee_bridge_base() = default;
        virtual~vm_debuggee_bridge_base() = default;

        vm_debuggee_bridge_base(const vm_debuggee_bridge_base&) = delete;
        vm_debuggee_bridge_base(vm_debuggee_bridge_base&&) = delete;
        vm_debuggee_bridge_base& operator = (const vm_debuggee_bridge_base&) = delete;
        vm_debuggee_bridge_base& operator = (vm_debuggee_bridge_base&&) = delete;

        virtual void debug_interrupt(vmbase*) = 0;

    public:
        void _vm_invoke_debuggee(vmbase* _vm);
    public:
        void _abandon()
        {
            std::lock_guard _(_abandon_debuggees_mx);
            _abandon_debuggees.push_back(this);
        }
        static void _free_abandons_in_shutdown()
        {
            std::lock_guard _(_abandon_debuggees_mx);
            for (auto* debuggee : _abandon_debuggees)
            {
                delete debuggee;
            }
            _abandon_debuggees.clear();
        }
    };

    class c_debuggee_bridge : public vm_debuggee_bridge_base
    {
        wo_debuggee_callback_func_t m_callback;
        void* m_userdata;

    public:
        c_debuggee_bridge(wo_debuggee_callback_func_t callback, void* userdata)
            : m_callback(callback)
            , m_userdata(userdata)
        {
        }
        ~c_debuggee_bridge()
        {
            m_callback(nullptr, m_userdata);
        }
        virtual void debug_interrupt(vmbase* vm) override
        {
            m_callback(std::launder(reinterpret_cast<wo_vm>(vm)), m_userdata);
        }
    };

    class vmbase
    {
    public:
        enum class vm_type
        {
            INVALID,
            NORMAL,

            // If vm's type is GC_DESTRUCTOR, GC_THREAD will not trying to pause it.
            GC_DESTRUCTOR,
        };
        enum class interrupt_wait_result : uint8_t
        {
            ACCEPT,
            TIMEOUT,
            LEAVED,
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

            ABORT_INTERRUPT = 1 << 10,
            // If virtual machine interrupt with ABORT_INTERRUPT, vm will stop immediately.

            GC_HANGUP_INTERRUPT = 1 << 11,
            // GC_HANGUP_INTERRUPT will be mark in 2 cases:
            // 1. VM received GC_INTERRUPT and start to do self-mark. after self-mark. 
            //      GC_HANGUP_INTERRUPT will be clear after self-mark.
            // 2. VM is leaved or STW-GC, in this case, vm will receive GC_HANGUP_INTERRUPT
            //      to hangup. vm will be mark by gc-worker, and be wake up after mark.
            // 3. Some one trying to read this vm in other thread, to make sure stack shrink
            //      or extern not happend in same time.

            PENDING_INTERRUPT = 1 << 12,
            // VM will be pending finish using and returned to pooled-vm, PENDING_INTERRUPT
            // only setted when vm is not running.

            BR_YIELD_INTERRUPT = 1 << 13,
            // VM will yield & return from running-state while received BR_YIELD_INTERRUPT

            DETACH_DEBUGGEE_INTERRUPT = 1 << 14,
            // VM will handle DETACH_DEBUGGEE_INTERRUPT before DEBUG_INTERRUPT, if vm handled
            // this interrupt, DEBUG_INTERRUPT will be cleared.

            STACK_OVERFLOW_INTERRUPT = 1 << 15,
            // If stack size is not enough for PSH or PUSHN, STACK_OVERFLOW_INTERRUPT will be setted.

            SHRINK_STACK_INTERRUPT = 1 << 16,
            // Advise vm to shrink it's stack usage, raised by GC-Job.

            STACK_MODIFING_INTERRUPT = 1 << 17,
            // While reading/externing/shrinking stack, this interrupt will be set to make sure
            // not read bad stack data in other thread.

            DEBUG_INTERRUPT = 1 << 30,
            // If virtual machine interrupt with DEBUG_INTERRUPT, it will stop at all opcode
            // to check something about debug, such as breakpoint.
            // * DEBUG_INTERRUPT will cause huge performance loss
        };

        struct callstack_info
        {
            std::string m_func_name;
            std::string m_file_path;
            size_t      m_row;
            size_t      m_col;
            bool        m_is_extern;
        };
        struct hangup_lock
        {
            std::mutex mx;
            std::condition_variable cv;
            std::atomic_int8_t flag;

            hangup_lock();
            ~hangup_lock() = default;
            hangup_lock(const hangup_lock&) = delete;
            hangup_lock(hangup_lock&&) = delete;
            hangup_lock& operator = (const hangup_lock&) = delete;
            hangup_lock& operator = (hangup_lock&&) = delete;

            void hangup()noexcept;
            void wakeup()noexcept;
        };
    public:
        inline static constexpr size_t VM_DEFAULT_STACK_SIZE = 32;
        inline static constexpr size_t VM_MAX_STACK_SIZE = 128 * 1024 * 1024;
        inline static constexpr uint8_t VM_SHRINK_STACK_COUNT = 3;
        inline static constexpr uint8_t VM_SHRINK_STACK_MAX_EDGE = 16;
        inline static constexpr uint8_t VM_MAX_JIT_FUNCTION_DEPTH = 32;

        static_assert(VM_SHRINK_STACK_COUNT < VM_SHRINK_STACK_MAX_EDGE);
        static_assert(VM_MAX_STACK_SIZE >= VM_DEFAULT_STACK_SIZE);
        static_assert((VM_DEFAULT_STACK_SIZE& (VM_DEFAULT_STACK_SIZE - 1)) == 0);
        static_assert((VM_MAX_STACK_SIZE& (VM_MAX_STACK_SIZE - 1)) == 0);

    public:
        inline static std::shared_mutex _alive_vm_list_mx;
        inline static cxx_set_t<vmbase*> _alive_vm_list;
        inline static cxx_set_t<vmbase*> _gc_ready_vm_list;

        inline thread_local static vmbase* _this_thread_vm = nullptr;
        inline static std::atomic_uint32_t _alive_vm_count_for_gc_vm_destruct;

    protected:
        inline static vm_debuggee_bridge_base* attaching_debuggee = nullptr;

    public:
        // For performance, interrupt should be first elem.
        union
        {
            std::atomic<uint32_t> vm_interrupt;
            uint32_t fast_ro_vm_interrupt;
        };
        static_assert(sizeof(std::atomic<uint32_t>) == sizeof(uint32_t));
        static_assert(std::atomic<uint32_t>::is_always_lock_free);

        // special register
        value* cr;  // op result trace & function return;
        value* tc;  // arugument count
        value* tp;  // stored argument count

        // stack state.
        value* sp;
        value* bp;
        value* sb;

        value* stack_storage;
        size_t stack_size;

        uint8_t shrink_stack_advise;
        uint8_t shrink_stack_edge;

        // storage to addressing
        value* register_storage;
        value* constant_and_global_storage;

        // next ircode pointer
        const byte_t* ip;
        shared_pointer<runtime_env> env;

        std::optional<std::unique_ptr<lexer>> 
            compile_failed_state;

        uint8_t jit_function_call_depth;
        const vm_type virtual_machine_type;

        hangup_lock hangup_state;

        vmbase* gc_vm;

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
        void inc_destructable_instance_count() noexcept;
        void dec_destructable_instance_count() noexcept;

        static vm_debuggee_bridge_base* attach_debuggee(vm_debuggee_bridge_base* dbg) noexcept;
        static vm_debuggee_bridge_base* current_debuggee() noexcept;
        bool is_aborted() const noexcept;
        bool interrupt(vm_interrupt_type type) noexcept;
        bool clear_interrupt(vm_interrupt_type type)noexcept;
        bool check_interrupt(vm_interrupt_type type)noexcept;
        interrupt_wait_result wait_interrupt(vm_interrupt_type type, bool force_wait)noexcept;
        void block_interrupt(vm_interrupt_type type)noexcept;

        void hangup()noexcept;
        void wakeup()noexcept;

        vmbase(vm_type type) noexcept;
        ~vmbase() noexcept;

        vmbase* get_or_alloc_gcvm() const noexcept;

        bool advise_shrink_stack() noexcept;
        void reset_shrink_stack_count() noexcept;

    public:
        vmbase* create_machine(vm_type type) const noexcept;
        wo_result_t run() noexcept;
    private:
        wo_result_t run_sim() noexcept;
        void _allocate_register_space(size_t regcount) noexcept;
        void _allocate_stack_space(size_t stacksz) noexcept;
        bool _reallocate_stack_space(size_t stacksz) noexcept;
    public:
        void set_runtime(shared_pointer<runtime_env> runtime_environment) noexcept;
        vmbase* make_machine(vm_type type) const noexcept;
        void dump_program_bin(size_t begin = 0, size_t end = SIZE_MAX, std::ostream& os = std::cout) const noexcept;
        void dump_call_stack(size_t max_count = 32, bool need_offset = true, std::ostream& os = std::cout)const noexcept;
        std::vector<callstack_info> dump_call_stack_func_info(size_t max_count, bool need_offset, bool* out_finished)const noexcept;
        size_t callstack_layer() const noexcept;
        bool gc_checkpoint() noexcept;
        bool assure_stack_size(wo_size_t assure_stack_size) noexcept;
        void co_pre_invoke(wo_int_t wo_func_addr, wo_int_t argc) noexcept;
        void co_pre_invoke(wo_handle_t ex_func_addr, wo_int_t argc) noexcept;
        void co_pre_invoke(closure_t* wo_func_addr, wo_int_t argc) noexcept;
        value* invoke(wo_int_t wo_func_addr, wo_int_t argc) noexcept;
        value* invoke(wo_handle_t wo_func_addr, wo_int_t argc) noexcept;
        value* invoke(closure_t* wo_func_closure, wo_int_t argc) noexcept;

    public:
        // Operate support:
        static void ltx_impl(value* result, value* opnum1, value* opnum2) noexcept;
        static void eltx_impl(value* result, value* opnum1, value* opnum2) noexcept;
        static void gtx_impl(value* result, value* opnum1, value* opnum2) noexcept;
        static void egtx_impl(value* result, value* opnum1, value* opnum2) noexcept;
        static value* make_union_impl(value* opnum1, value* opnum2, uint16_t id) noexcept;
        static value* make_closure_fast_impl(value* opnum1, const byte_t* rt_ip, value* rt_sp) noexcept;
        static value* make_closure_safe_impl(value* opnum1, const byte_t* rt_ip, value* rt_sp) noexcept;
        static value* make_array_impl(value* opnum1, uint16_t size, value* rt_sp) noexcept;
        static value* make_map_impl(value* opnum1, uint16_t size, value* rt_sp) noexcept;
        static value* make_struct_impl(value* opnum1, uint16_t size, value* rt_sp) noexcept;
        static void packargs_impl(value* opnum1, uint16_t argcount, const value* tp, value* rt_bp, uint16_t skip_closure_arg_count) noexcept;
        static value* unpackargs_impl(vmbase* vm, value* opnum1, int32_t unpack_argc, value* tc, const byte_t* rt_ip, value* rt_sp, value* rt_bp) noexcept;
        static const char* movcast_impl(value* opnum1, value* opnum2, value::valuetype aim_type) noexcept;
    };
    static_assert(std::is_standard_layout_v<vmbase>);

    class _wo_vm_stack_guard
    {
        vmbase* m_reading_vm;
        _wo_vm_stack_guard(const _wo_vm_stack_guard&) = delete;
        _wo_vm_stack_guard(_wo_vm_stack_guard&&) = delete;
        _wo_vm_stack_guard& operator = (const _wo_vm_stack_guard&) = delete;
        _wo_vm_stack_guard& operator = (_wo_vm_stack_guard&&) = delete;

    public:
        _wo_vm_stack_guard(const vmbase* _reading_vm)
        {
            vmbase* reading_vm = const_cast<vmbase*>(_reading_vm);

            if (reading_vm != vmbase::_this_thread_vm)
            {
                while (!reading_vm->interrupt(
                    vmbase::vm_interrupt_type::STACK_MODIFING_INTERRUPT))
                    gcbase::rw_lock::spin_loop_hint();

                m_reading_vm = reading_vm;
            }
            else
                m_reading_vm = nullptr;
        }
        ~_wo_vm_stack_guard()
        {
            if (m_reading_vm != nullptr)
                wo_assure(m_reading_vm->clear_interrupt(
                    vmbase::vm_interrupt_type::STACK_MODIFING_INTERRUPT));
        }
    };

}

#undef WO_READY_EXCEPTION_HANDLE
