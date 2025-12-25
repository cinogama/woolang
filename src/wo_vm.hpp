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
#include <set>

#if WO_BUILD_WITH_MINGW
#include <mingw.thread.h>
#include <mingw.mutex.h>
#include <mingw.shared_mutex.h>
#include <mingw.condition_variable.h>
#endif

namespace wo
{
    class vmbase;
    class bytecode_disassembler;

    class vm_debuggee_bridge_base
    {
        static std::shared_mutex _global_debuggee_bridge_mx;
        static shared_pointer<vm_debuggee_bridge_base> _global_debuggee_bridge;

        std::mutex _debug_entry_guard_block_mx;

    public:
        vm_debuggee_bridge_base() = default;
        virtual ~vm_debuggee_bridge_base() = default;

        vm_debuggee_bridge_base(const vm_debuggee_bridge_base&) = delete;
        vm_debuggee_bridge_base(vm_debuggee_bridge_base&&) = delete;
        vm_debuggee_bridge_base& operator=(const vm_debuggee_bridge_base&) = delete;
        vm_debuggee_bridge_base& operator=(vm_debuggee_bridge_base&&) = delete;

        virtual void debug_interrupt(vmbase*) = 0;

    public:
        void _vm_invoke_debuggee(vmbase* _vm);

    public:
        static void attach_global_debuggee_bridge(
            const std::optional<shared_pointer<vm_debuggee_bridge_base>>& bridge) noexcept;
        static std::optional<shared_pointer<vm_debuggee_bridge_base>>
            current_global_debuggee_bridge() noexcept;
        static bool has_current_global_debuggee_bridge() noexcept;
    };
    class c_debuggee_bridge : public vm_debuggee_bridge_base
    {
        wo_debuggee_callback_func_t m_callback;
        void* m_userdata;

    public:
        c_debuggee_bridge(wo_debuggee_callback_func_t callback, void* userdata)
            : m_callback(callback), m_userdata(userdata)
        {
        }
        ~c_debuggee_bridge()
        {
            m_callback(nullptr, m_userdata);
        }
        virtual void debug_interrupt(vmbase* vm) override
        {
            m_callback(reinterpret_cast<wo_vm>(vm), m_userdata);
        }
    };

    class assure_leave_this_thread_vm_shared_mutex : private std::shared_mutex
    {
        // ATTENTION: GC Thread will check if vm is received GC_INTERRUPT while locking
        //  this mutex, so, all vm in this thread must be LEAVED before lock this mutex.
    public:
        struct leave_context
        {
            wo_vm m_vm;
        };

    private:
        void leave(leave_context* out_context) noexcept;
        void enter(const leave_context& in_context) noexcept;

    public:
        assure_leave_this_thread_vm_shared_mutex() = default;
        assure_leave_this_thread_vm_shared_mutex(const assure_leave_this_thread_vm_shared_mutex&) = delete;
        assure_leave_this_thread_vm_shared_mutex(assure_leave_this_thread_vm_shared_mutex&&) = delete;
        assure_leave_this_thread_vm_shared_mutex& operator=(const assure_leave_this_thread_vm_shared_mutex&) = delete;
        assure_leave_this_thread_vm_shared_mutex& operator=(assure_leave_this_thread_vm_shared_mutex&&) = delete;

        void lock(leave_context* out_context) noexcept;
        void unlock(const leave_context& in_context) noexcept;
        void lock_shared(leave_context* out_context) noexcept;
        void unlock_shared(const leave_context& in_context) noexcept;
        bool try_lock(leave_context* out_context) noexcept;
        bool try_lock_shared(leave_context* out_context) noexcept;
    };
    class assure_leave_this_thread_vm_lock_guard
    {
        assure_leave_this_thread_vm_shared_mutex* m_mx;
        assure_leave_this_thread_vm_shared_mutex::leave_context m_context;

    public:
        assure_leave_this_thread_vm_lock_guard(assure_leave_this_thread_vm_shared_mutex& mx);
        ~assure_leave_this_thread_vm_lock_guard();
        assure_leave_this_thread_vm_lock_guard(const assure_leave_this_thread_vm_lock_guard&) = delete;
        assure_leave_this_thread_vm_lock_guard(assure_leave_this_thread_vm_lock_guard&&) = delete;
        assure_leave_this_thread_vm_lock_guard& operator=(const assure_leave_this_thread_vm_lock_guard&) = delete;
        assure_leave_this_thread_vm_lock_guard& operator=(assure_leave_this_thread_vm_lock_guard&&) = delete;
    };
    class assure_leave_this_thread_vm_shared_lock
    {
        assure_leave_this_thread_vm_shared_mutex* m_mx;
        assure_leave_this_thread_vm_shared_mutex::leave_context m_context;

    public:
        assure_leave_this_thread_vm_shared_lock(assure_leave_this_thread_vm_shared_mutex& mx);
        ~assure_leave_this_thread_vm_shared_lock();
        assure_leave_this_thread_vm_shared_lock(const assure_leave_this_thread_vm_shared_lock&) = delete;
        assure_leave_this_thread_vm_shared_lock(assure_leave_this_thread_vm_shared_lock&&) = delete;
        assure_leave_this_thread_vm_shared_lock& operator=(const assure_leave_this_thread_vm_shared_lock&) = delete;
        assure_leave_this_thread_vm_shared_lock& operator=(assure_leave_this_thread_vm_shared_lock&&) = delete;
    };

    class vmbase
    {
    public:
        enum class vm_type : uint8_t
        {
            INVALID,

            // Normal virtual machine, used for executing code. GC treats it as a root object 
            // and directly marks its internal registers and stack space.
            NORMAL,

            // Just like NORMAL, but if WEAK_NORMAL leaved, it should be marked by manually.
            WEAK_NORMAL,

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
            // Virtual machines that receive GC_INTERRUPT will immediately call `gc_checkpoint_self_mark`
            // to begin self-marking.

            GC_HANGUP_INTERRUPT = 1 << 9,
            // When the virtual machine confirms receipt of the GC_HANGUP_INTERRUPT signal, it should
            // immediately suspend until notified by the GC worker thread to resume; GC_HANGUP_INTERRUPT
            // is a coordination signal between the GC worker thread and the virtual machine, used to
            // ensure that during GC work, the virtual machine does not arbitrarily execute operations
            // that could corrupt the VM state under specific circumstances.
            //
            // The following situations require the virtual machine to suspend:
            // 1) The GC worker thread initiated GC_INTERRUPT, but the current virtual machine
            //      has left the GC scope.
            // 2) Before an STW-GC begins, the GC worker thread uses this signal to ensure all virtual
            //      machines are suspended until the STW-GC ends.
            //
            // In addition to being used for suspension, GC_HANGUP_INTERRUPT also has a special purpose:
            // 1) When the virtual machine receives GC_INTERRUPT, it sets GC_HANGUP_INTERRUPT to notify 
            //      the GC worker thread that the current virtual machine has received the interrupt request.

            GC_WEAK_PENDING_INTERRUPT = 1 << 10,
            // 

            LEAVE_INTERRUPT = 1 << 11,
            // When GC work trying GC_INTERRUPT, it will wait for vm cleaning GC_INTERRUPT flag(and hangs), 
            // and the wait will be endless, besides: If LEAVE_INTERRUPT was setted, 'wait_interrupt' will 
            // try to wait in unlimitted time.
            //
            // VM will set LEAVE_INTERRUPT when:
            // 1) calling slow native function
            // 2) leaving vm run()
            // 3) vm was created.
            //
            // VM will clean LEAVE_INTERRUPT when:
            // 1) the slow native function calling was end.
            // 2) enter vm run()
            // 3) vm destructed.
            //
            // ATTENTION: Each operate of setting or cleaning LEAVE_INTERRUPT must be
            //            successful. (We use 'wo_assure' here)' (Except in case of exception restore)

            ABORT_INTERRUPT = 1 << 12,
            // If virtual machine interrupt with ABORT_INTERRUPT, vm will stop immediately.

            PENDING_INTERRUPT = 1 << 13,
            // VM will be pending finish using and returned to pooled-vm, PENDING_INTERRUPT
            // only setted when vm is not running.

            BR_YIELD_INTERRUPT = 1 << 14,
            // VM will yield & return from running-state while received BR_YIELD_INTERRUPT

            STACK_OVERFLOW_INTERRUPT = 1 << 16,
            // If stack size is not enough for PSH or PUSHN, STACK_OVERFLOW_INTERRUPT will be setted.

            SHRINK_STACK_INTERRUPT = 1 << 17,
            // Advise vm to shrink it's stack usage, raised by GC-Job.

            STACK_OCCUPYING_INTERRUPT = 1 << 18,
            // When reallocating (expanding or shrinking) stack space, stack read operations other
            // than runtime operations (since reallocation only occurs during runtime or when the VM
            // is inactive) include:
            //  1. Stack unwinding.
            //  2. Direct stack value access.
            // Therefore, this interrupt is triggered both during stack reallocation and when the above
            // situations occur, functioning as a spinlock-like mechanism;
            // Upon receiving this interrupt, the VM should halt.
            //
            // NOTE: GC-in-gc-thread will scan the stack in other thread, too, but we use gc_guard to make
            //  sure stack-reallocate never happend during GC scan.

            CALL_FAR_RESYNC_VM_STATE_INTERRUPT = 1 << 19,
            // [Only interrupt in VM] When a virtual machine attempts to call a function outside
            // its near code area, this is referred to as a "far call" (call far).
            // Since different code areas use different static constant tables, to ensure the virtual
            // machine can correctly handle various relative quantities
            // (static/constant access, relative address jumps, and immediate calls) after a far call
            // occurs, the virtual machine will request this interrupt to update the near code area
            // and static constant table either after a far call or when returning from one.
            //  * This interrupt carries relatively high overhead, so avoid using far calls unnecessarily
            // unless absolutely required.

            DEBUG_INTERRUPT = 1 << 30,
            // If virtual machine interrupt with DEBUG_INTERRUPT, it will stop at all opcode
            // to check something about debug, such as breakpoint.
            // * DEBUG_INTERRUPT will cause huge performance loss
        };
        enum class call_way
        {
            BAD,
            NEAR,
            FAR,
            NATIVE,
        };
        struct callstack_info
        {
            std::string m_func_name;
            std::string m_file_path;

            size_t m_row;
            size_t m_col;
            call_way m_call_way;

            const irv2::ir* m_address;
            wo::value* m_bp;
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
            hangup_lock& operator=(const hangup_lock&) = delete;
            hangup_lock& operator=(hangup_lock&&) = delete;

            void hangup() noexcept;
            void wakeup() noexcept;
        };

    public:
        inline static constexpr size_t VM_DEFAULT_STACK_SIZE = 32;
        inline static constexpr size_t VM_REGISTER_COUNT = _WO_TOTAL_REG_COUNT;
        inline static constexpr size_t VM_MAX_STACK_SIZE = 128 * 1024 * 1024;
        inline static constexpr uint8_t VM_SHRINK_STACK_COUNT = 3;
        inline static constexpr uint8_t VM_SHRINK_STACK_MAX_EDGE = 16;
        inline static constexpr uint8_t VM_MAX_JIT_FUNCTION_DEPTH = 32;

        static_assert(VM_SHRINK_STACK_COUNT < VM_SHRINK_STACK_MAX_EDGE);
        static_assert(VM_MAX_STACK_SIZE >= VM_DEFAULT_STACK_SIZE);
        static_assert((VM_DEFAULT_STACK_SIZE& (VM_DEFAULT_STACK_SIZE - 1)) == 0);
        static_assert((VM_MAX_STACK_SIZE& (VM_MAX_STACK_SIZE - 1)) == 0);

    public:
        static assure_leave_this_thread_vm_shared_mutex _alive_vm_list_mx;
        static std::set<vmbase*> _alive_vm_list;
        static std::set<vmbase*> _gc_ready_vm_list;

        thread_local static vmbase* _this_thread_gc_guard_vm;
        static std::atomic_uint32_t _alive_vm_count_for_gc_vm_destruct;
    public:
        // For performance, interrupt should be first elem.
        std::atomic<uint32_t> vm_interrupt;

        // special register
        value* cr; // op result trace & function return;
        value* tc; // arugument count
        value* tp; // stored argument count

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

        // Runtime min-env.
        const irv2::ir* runtime_codes_begin;
        const irv2::ir* runtime_codes_end;
        value* runtime_static_storage;

        // next ircode pointer
        const irv2::ir* ip;
        shared_pointer<runtime_env> env;

        hangup_lock hangup_state;

        // Flag used to check and reset when returning from an external function.
        // If the flag is true, it indicates that during the external function call,
        // the stack's starting position and space have changed.
        // Therefore, the caller of the external function - if it's the JIT runtime -
        // needs to propagate a synchronization request upward to the VM runtime for handling.
        bool extern_state_stack_update;

        // JIT call depth counter
        // In most cases, the native stack will be exhausted before the virtual machine stack.
        // Under JIT runtime, nested calls consume native stack space.
        // To minimize waste of native stack space, this counter is used to track JIT call nesting depth.
        // When the depth reaches a certain threshold, all execution falls back to the VM runtime.
        uint8_t extern_state_jit_call_depth;

        // Only changed by `switch_vm_kind`
        vm_type virtual_machine_type;

        // Filled if this VM failed to compile, the lexer contains the error information.
        std::optional<std::unique_ptr<lexer>> compile_failed_state;

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
        std::atomic_size_t* inc_destructable_instance_count() noexcept;
        static void attach_debuggee(
            const std::optional<shared_pointer<vm_debuggee_bridge_base>>& dbg) noexcept;
        bool is_aborted() const noexcept;
        bool interrupt(vm_interrupt_type type) noexcept;
        bool clear_interrupt(vm_interrupt_type type) noexcept;
        bool check_interrupt(vm_interrupt_type type) noexcept;
        interrupt_wait_result wait_interrupt(vm_interrupt_type type, bool force_wait) noexcept;
        void block_interrupt(vm_interrupt_type type) noexcept;

        void hangup() noexcept;
        void wakeup() noexcept;

        vmbase(vm_type type) noexcept;
        ~vmbase() noexcept;

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
        void set_runtime(shared_pointer<runtime_env> runtime_environment) noexcept;

    public:
        void init_main_vm(shared_pointer<runtime_env> runtime_environment) noexcept;
        vmbase* make_machine(vm_type type) const noexcept;
        static void dump_program_bin(
            const runtime_env* codeholder,
            size_t begin,
            size_t end,
            const irv2::ir* focus_runtime_ip,
            std::ostream& os) noexcept;
        void dump_call_stack(
            size_t max_count,
            bool need_offset,
            std::ostream& os) const noexcept;
        std::vector<callstack_info> dump_call_stack_func_info(
            size_t max_count,
            bool need_offset,
            bool* out_finished_may_null) const noexcept;
        size_t callstack_layer() const noexcept;

    private:
        // Disassemble helper
        static std::string disassemble_instruction(const irv2::ir** in_out_iraddr) noexcept;

    public:
        void gc_checkpoint_self_mark() noexcept;
        bool assure_stack_size(wo_size_t assure_stack_size) noexcept;
        void co_pre_invoke_script(const irv2::ir* wo_func_addr, wo_int_t argc) noexcept;
        void co_pre_invoke_native(wo_native_func_t ex_func_addr, wo_int_t argc) noexcept;
        void co_pre_invoke_closure(closure_t* wo_func_addr, wo_int_t argc) noexcept;
        value* invoke_script(const irv2::ir* wo_func_addr, wo_int_t argc) noexcept;
        value* invoke_native(wo_native_func_t wo_func_addr, wo_int_t argc) noexcept;
        value* invoke_closure(closure_t* wo_func_closure, wo_int_t argc) noexcept;
        void switch_vm_kind(vm_type new_type) noexcept;
    public:
        // Operate support:
        static void ltx_impl(value* result, value* opnum1, value* opnum2) noexcept;
        static void eltx_impl(value* result, value* opnum1, value* opnum2) noexcept;
        static void gtx_impl(value* result, value* opnum1, value* opnum2) noexcept;
        static void egtx_impl(value* result, value* opnum1, value* opnum2) noexcept;
        static value* make_union_impl(value* opnum1, value* opnum2, uint16_t id) noexcept;
        static value* make_closure_wo_impl(value* opnum1, uint16_t argc, const wo::irv2::ir* addr, value* rt_sp) noexcept;
        static value* make_closure_fp_impl(value* opnum1, uint16_t argc, wo_native_func_t addr, value* rt_sp) noexcept;
        static value* make_array_impl(value* opnum1, uint32_t size, value* rt_sp) noexcept;
        static value* make_map_impl(value* opnum1, uint32_t size, value* rt_sp) noexcept;
        static value* make_struct_impl(value* opnum1, uint32_t size, value* rt_sp) noexcept;
        static void packargs_impl(
            value* opnum1,
            value* rt_bp,
            wo_integer_t func_argument_count,
            uint16_t func_named_param,
            uint16_t closure_captured_count) noexcept;
        static const char* movcast_impl(value* opnum1, value* opnum2, value::valuetype aim_type) noexcept;
    };
    static_assert(std::is_standard_layout_v<vmbase>);

    class _wo_vm_stack_occupying_lock_guard
    {
        vmbase* m_reading_vm;
        _wo_vm_stack_occupying_lock_guard(const _wo_vm_stack_occupying_lock_guard&) = delete;
        _wo_vm_stack_occupying_lock_guard(_wo_vm_stack_occupying_lock_guard&&) = delete;
        _wo_vm_stack_occupying_lock_guard& operator=(const _wo_vm_stack_occupying_lock_guard&) = delete;
        _wo_vm_stack_occupying_lock_guard& operator=(_wo_vm_stack_occupying_lock_guard&&) = delete;

    public:
        _wo_vm_stack_occupying_lock_guard(const vmbase* _reading_vm)
        {
            vmbase* reading_vm = const_cast<vmbase*>(_reading_vm);

            while (!reading_vm->interrupt(
                vmbase::vm_interrupt_type::STACK_OCCUPYING_INTERRUPT))
                gcbase::rw_lock::spin_loop_hint();

            m_reading_vm = reading_vm;
        }
        ~_wo_vm_stack_occupying_lock_guard()
        {
            wo_assure(m_reading_vm->clear_interrupt(
                vmbase::vm_interrupt_type::STACK_OCCUPYING_INTERRUPT));
        }
    };

}

#undef WO_READY_EXCEPTION_HANDLE
