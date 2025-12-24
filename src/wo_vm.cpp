#include "wo_afx.hpp"

#define WO_VM_FP_SHIFT(BASE, OFFSET)                                    \
        reinterpret_cast<const irv2::ir*>(                              \
            reinterpret_cast<const uint32_t*>(BASE) + (OFFSET))

#define WO_VM_FP_OFFSET(BASE, FP)                                       \
        static_cast<uint32_t>(                                          \
            reinterpret_cast<const uint32_t*>(FP) - reinterpret_cast<const uint32_t*>(BASE))

#define WO_VM_ADRS_C_G(OFFSET_I)                                        \
        (near_static_global + (OFFSET_I))
#define WO_VM_ADRS_S(OFFSET_I)                                          \
        (bp + (OFFSET_I))
#define WO_VM_ADRS_R_S(OFFSET_I)                                        \
        ((static_cast<int8_t>(OFFSET_I) < 0b0110'0000)                  \
            ? WO_VM_ADRS_S(static_cast<int8_t>(OFFSET_I))               \
            : (rt_reg_fast_lookup + static_cast<int8_t>(OFFSET_I)))

#define WO_VM_BEGIN_CASE_IR6(IRNAME, FORMAT)                            \
            case (WO_##IRNAME << 2) | 0b00:                             \
            case (WO_##IRNAME << 2) | 0b01:                             \
            case (WO_##IRNAME << 2) | 0b10:                             \
            case (WO_##IRNAME << 2) | 0b11:                             \
            {                                                           \
                constexpr uint32_t z__wo_vm_next_ir_offset =            \
                    WO_FORMAL_IR6_##FORMAT##_IRCOUNTOF;                 \
                WO_FORMAL_IR6_##FORMAT(rt_ip);

#define WO_VM_BEGIN_CASE_IR8(IRNAME, MODE, FORMAT)                      \
            case (WO_##IRNAME << 2) | (MODE):                           \
            {                                                           \
                constexpr uint32_t z__wo_vm_next_ir_offset =            \
                    WO_FORMAL_IR8_##FORMAT##_IRCOUNTOF;                 \
                WO_FORMAL_IR8_##FORMAT(rt_ip);                

#define WO_VM_END_CASE()                                                \
                rt_ip = WO_VM_FP_SHIFT(rt_ip, z__wo_vm_next_ir_offset); \
                break;                                                  \
            }

namespace wo
{
    //////////////////////////////////////////////////////////////////////////
    // Static Member Definitions
    //////////////////////////////////////////////////////////////////////////

    std::shared_mutex vm_debuggee_bridge_base::
        _global_debuggee_bridge_mx;
    shared_pointer<vm_debuggee_bridge_base> vm_debuggee_bridge_base::
        _global_debuggee_bridge;

    assure_leave_this_thread_vm_shared_mutex vmbase::
        _alive_vm_list_mx;
    std::set<vmbase*> vmbase::
        _alive_vm_list;
    std::set<vmbase*> vmbase::
        _gc_ready_vm_list;

    thread_local vmbase* vmbase::
        _this_thread_gc_guard_vm = nullptr;
    std::atomic_uint32_t vmbase::
        _alive_vm_count_for_gc_vm_destruct = {};

    //////////////////////////////////////////////////////////////////////////
    // Static Constants
    //////////////////////////////////////////////////////////////////////////

    const value value::TAKEPLACE = *value().set_takeplace();

    //////////////////////////////////////////////////////////////////////////
    // Thread-Safe Mutex Implementation
    // 
    // These classes provide thread-safe locking mechanisms that properly
    // handle GC guard state transitions during lock/unlock operations.
    //////////////////////////////////////////////////////////////////////////

    void assure_leave_this_thread_vm_shared_mutex::leave(
        leave_context* out_context) noexcept
    {
        out_context->m_vm = wo_swap_gcguard(nullptr);
    }

    void assure_leave_this_thread_vm_shared_mutex::enter(
        const leave_context& in_context) noexcept
    {
        wo_swap_gcguard(in_context.m_vm);
    }

    void assure_leave_this_thread_vm_shared_mutex::lock(
        leave_context* out_context) noexcept
    {
        leave(out_context);
        std::shared_mutex::lock();
    }

    void assure_leave_this_thread_vm_shared_mutex::unlock(
        const leave_context& in_context) noexcept
    {
        std::shared_mutex::unlock();
        enter(in_context);
    }

    void assure_leave_this_thread_vm_shared_mutex::lock_shared(
        leave_context* out_context) noexcept
    {
        leave(out_context);
        std::shared_mutex::lock_shared();
    }

    void assure_leave_this_thread_vm_shared_mutex::unlock_shared(
        const leave_context& in_context) noexcept
    {
        std::shared_mutex::unlock_shared();
        enter(in_context);
    }

    bool assure_leave_this_thread_vm_shared_mutex::try_lock(
        leave_context* out_context) noexcept
    {
        leave(out_context);
        if (std::shared_mutex::try_lock())
            return true;

        enter(*out_context);
        return false;
    }

    bool assure_leave_this_thread_vm_shared_mutex::try_lock_shared(
        leave_context* out_context) noexcept
    {
        leave(out_context);
        if (std::shared_mutex::try_lock_shared())
            return true;

        enter(*out_context);
        return false;
    }

    //////////////////////////////////////////////////////////////////////////
    // Lock Guard Implementations
    //////////////////////////////////////////////////////////////////////////

    assure_leave_this_thread_vm_lock_guard::assure_leave_this_thread_vm_lock_guard(
        assure_leave_this_thread_vm_shared_mutex& mx)
        : m_mx(&mx)
    {
        m_mx->lock(&m_context);
    }

    assure_leave_this_thread_vm_lock_guard::~assure_leave_this_thread_vm_lock_guard()
    {
        m_mx->unlock(m_context);
    }

    assure_leave_this_thread_vm_shared_lock::assure_leave_this_thread_vm_shared_lock(
        assure_leave_this_thread_vm_shared_mutex& mx)
        : m_mx(&mx)
    {
        m_mx->lock_shared(&m_context);
    }

    assure_leave_this_thread_vm_shared_lock::~assure_leave_this_thread_vm_shared_lock()
    {
        m_mx->unlock_shared(m_context);
    }

    //////////////////////////////////////////////////////////////////////////
    // Hangup Lock Implementation
    // 
    // Provides a mechanism to temporarily suspend VM execution with
    // condition variable-based waiting.
    //////////////////////////////////////////////////////////////////////////

    vmbase::hangup_lock::hangup_lock()
        : flag(0)
    {
    }

    void vmbase::hangup_lock::hangup() noexcept
    {
        {
            std::lock_guard g1(mx);
            flag.fetch_sub(1);
        }

        std::unique_lock ug1(mx);
        cv.wait(ug1, [this]() { return flag >= 0; });
    }

    void vmbase::hangup_lock::wakeup() noexcept
    {
        {
            std::lock_guard g1(mx);
            flag.fetch_add(1);
        }

        cv.notify_one();
    }

    //////////////////////////////////////////////////////////////////////////
    // Debugger Bridge Implementation
    //////////////////////////////////////////////////////////////////////////
    void vm_debuggee_bridge_base::_vm_invoke_debuggee(vmbase* _vm)
    {
        std::lock_guard _(_debug_entry_guard_block_mx);
        wo_assert(_vm->check_interrupt(vmbase::vm_interrupt_type::LEAVE_INTERRUPT));

        // Just make a block
        debug_interrupt(_vm);
    }

    void vm_debuggee_bridge_base::attach_global_debuggee_bridge(
        const std::optional<shared_pointer<vm_debuggee_bridge_base>>& bridge) noexcept
    {
        std::lock_guard g(_global_debuggee_bridge_mx);

        if (bridge.has_value())
            _global_debuggee_bridge = bridge.value();
        else
            _global_debuggee_bridge.reset();
    }
    std::optional<shared_pointer<vm_debuggee_bridge_base>>
        vm_debuggee_bridge_base::current_global_debuggee_bridge() noexcept
    {
        std::shared_lock sg(_global_debuggee_bridge_mx);

        if (_global_debuggee_bridge != nullptr)
            return shared_pointer<vm_debuggee_bridge_base>(_global_debuggee_bridge);
        return std::nullopt;
    }
    bool vm_debuggee_bridge_base::has_current_global_debuggee_bridge() noexcept
    {
        std::shared_lock sg(_global_debuggee_bridge_mx);
        return _global_debuggee_bridge != nullptr;
    }

    //////////////////////////////////////////////////////////////////////////
    // VM Instance Management
    //////////////////////////////////////////////////////////////////////////

    std::atomic_size_t* vmbase::inc_destructable_instance_count() noexcept
    {
        wo_assert(env != nullptr);
        auto* dec_destructable_instance_countp = &env->_created_destructable_instance_count;

#if WO_ENABLE_RUNTIME_CHECK
        size_t old_count =
#endif
            dec_destructable_instance_countp->fetch_add(
                1, std::memory_order::memory_order_relaxed);
        wo_assert(old_count >= 0);

        return dec_destructable_instance_countp;
    }

    void vmbase::attach_debuggee(
        const std::optional<shared_pointer<vm_debuggee_bridge_base>>& dbg) noexcept
    {
        wo::assure_leave_this_thread_vm_shared_lock g1(_alive_vm_list_mx);

        vm_debuggee_bridge_base::attach_global_debuggee_bridge(dbg);

        // Setup new debuggee for all VMs
        for (auto* vm_instance : _alive_vm_list)
        {
            if (vm_instance->virtual_machine_type == vmbase::vm_type::GC_DESTRUCTOR)
                continue;

            if (dbg.has_value())
                (void)vm_instance->interrupt(vm_interrupt_type::DEBUG_INTERRUPT);
            else
                (void)vm_instance->clear_interrupt(vm_interrupt_type::DEBUG_INTERRUPT);
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // Interrupt Management
    //////////////////////////////////////////////////////////////////////////

    bool vmbase::is_aborted() const noexcept
    {
        return vm_interrupt.load(std::memory_order::memory_order_acquire)
            & vm_interrupt_type::ABORT_INTERRUPT;
    }

    bool vmbase::interrupt(vm_interrupt_type type) noexcept
    {
        return !(type & vm_interrupt.fetch_or(
            type, std::memory_order::memory_order_acq_rel));
    }

    bool vmbase::clear_interrupt(vm_interrupt_type type) noexcept
    {
        return type & vm_interrupt.fetch_and(
            ~type, std::memory_order::memory_order_acq_rel);
    }

    bool vmbase::check_interrupt(vm_interrupt_type type) noexcept
    {
        return 0 != (vm_interrupt.load(std::memory_order_acquire) & type);
    }

    vmbase::interrupt_wait_result vmbase::wait_interrupt(
        vm_interrupt_type type,
        bool force_wait) noexcept
    {
        using namespace std;

        size_t retry_count = 0;
        bool warning_raised = false;
        constexpr int MAX_TRY_COUNT = 0;
        int i = 0;

        do
        {
            uint32_t vm_interrupt_mask = vm_interrupt.load(std::memory_order_relaxed);

            if (0 == (vm_interrupt_mask & type))
                break;

            if (vm_interrupt_mask & vm_interrupt_type::LEAVE_INTERRUPT)
            {
                if (++i > MAX_TRY_COUNT)
                    return interrupt_wait_result::LEAVED;
            }
            else
            {
                i = 0;
            }

            if (force_wait)
            {
                std::this_thread::sleep_for(10ms);

                if (!warning_raised &&
                    ++retry_count == config::INTERRUPT_CHECK_TIME_LIMIT)
                {
                    // Wait for too much time - generate warning
                    std::string warning_info =
                        "Wait for too much time for waiting interrupt.\n";
                    std::stringstream dump_callstack_info;

                    dump_call_stack(32, false, dump_callstack_info);
                    warning_info += dump_callstack_info.str();
                    wo_warning(warning_info.c_str());

                    warning_raised = true;
                }
            }
            else
            {
                return interrupt_wait_result::TIMEOUT;
            }
        } while (true);

        return interrupt_wait_result::ACCEPT;
    }

    void vmbase::block_interrupt(vm_interrupt_type type) noexcept
    {
        using namespace std;

        while (vm_interrupt.load(std::memory_order::memory_order_acquire) & type)
            std::this_thread::sleep_for(10ms);
    }

    //////////////////////////////////////////////////////////////////////////
    // VM State Control (Hangup/Wakeup)
    //////////////////////////////////////////////////////////////////////////

    void vmbase::hangup() noexcept
    {
        hangup_state.hangup();
    }

    void vmbase::wakeup() noexcept
    {
        hangup_state.wakeup();
    }

    //////////////////////////////////////////////////////////////////////////
    // VM Constructor and Destructor
    //////////////////////////////////////////////////////////////////////////

    vmbase::vmbase(vm_type type) noexcept
        : cr(nullptr)
        , tc(nullptr)
        , tp(nullptr)
        , sp(nullptr)
        , bp(nullptr)
        , sb(nullptr)
        , stack_storage(nullptr)
        , stack_size(0)
        , shrink_stack_advise(0)
        , shrink_stack_edge(VM_SHRINK_STACK_COUNT)
        , register_storage(nullptr)
        , runtime_codes_begin(nullptr)
        , runtime_codes_end(nullptr)
        , runtime_static_storage(nullptr)
        , ip(nullptr)
        , env(nullptr)
        , extern_state_stack_update(false)
        , extern_state_jit_call_depth(0)
        , virtual_machine_type(type)
        , compile_failed_state(std::nullopt)
#if WO_ENABLE_RUNTIME_CHECK
        , attaching_thread_id(std::thread::id{})
#endif        
    {
        (void)_alive_vm_count_for_gc_vm_destruct.fetch_add(
            1, std::memory_order_relaxed);

        vm_interrupt.store(vm_interrupt_type::LEAVE_INTERRUPT, std::memory_order_release);
        wo::assure_leave_this_thread_vm_lock_guard g1(_alive_vm_list_mx);

        wo_assert(
            _alive_vm_list.find(this) == _alive_vm_list.end(),
            "This vm is already exists in _alive_vm_list, that is illegal.");

        _alive_vm_list.insert(this);

        if (vm_debuggee_bridge_base::has_current_global_debuggee_bridge())
            wo_assure(this->interrupt(vm_interrupt_type::DEBUG_INTERRUPT));
    }

    vmbase::~vmbase()
    {
        do
        {
            wo::assure_leave_this_thread_vm_lock_guard g1(_alive_vm_list_mx);

            wo_assert(
                _alive_vm_list.find(this) != _alive_vm_list.end(),
                "This vm not exists in _alive_vm_list, that is illegal.");

            _alive_vm_list.erase(this);

            wo_assert(
                _gc_ready_vm_list.find(this) != _gc_ready_vm_list.end() || env == nullptr,
                "This vm not exists in _gc_ready_vm_list, that is illegal.");

            _gc_ready_vm_list.erase(this);

            free(register_storage);
            free(stack_storage);

        } while (0);

        if (env)
            --env->_running_on_vm_count;

        (void)_alive_vm_count_for_gc_vm_destruct.fetch_sub(
            1, std::memory_order_relaxed);
    }

    //////////////////////////////////////////////////////////////////////////
    // Stack Management
    //////////////////////////////////////////////////////////////////////////

    bool vmbase::advise_shrink_stack() noexcept
    {
        return ++shrink_stack_advise >= shrink_stack_edge;
    }

    void vmbase::reset_shrink_stack_count() noexcept
    {
        shrink_stack_advise = 0;
    }

    void vmbase::_allocate_register_space(size_t regcount) noexcept
    {
        register_storage = std::launder(
            reinterpret_cast<value*>(calloc(regcount, sizeof(wo::value))));

        cr = register_storage + WO_REG_CR;
        tc = register_storage + WO_REG_TC;
        tp = register_storage + WO_REG_TP;
    }

    void vmbase::_allocate_stack_space(size_t stacksz) noexcept
    {
        stack_size = stacksz;

        stack_storage = std::launder(
            reinterpret_cast<value*>(calloc(stacksz, sizeof(wo::value))));
        sb = stack_storage + stacksz - 1;
        sp = bp = sb;
    }

    bool vmbase::_reallocate_stack_space(size_t stacksz) noexcept
    {
        wo_assert(stacksz != 0);

        const size_t used_stack_size = sb - sp;

        // New stack size is smaller than current stack size
        if (used_stack_size * 2 > stacksz)
            return false;

        // Out of limit.
        if (stacksz > VM_MAX_STACK_SIZE)
            return false;

        value* new_stack_buf = reinterpret_cast<value*>(
            calloc(stacksz, sizeof(wo::value)));

        // Failed to allocate new stack space
        if (new_stack_buf == nullptr)
            return false;

        value* new_stack_mem_begin = new_stack_buf + stacksz - 1;
        value* new_sp = new_stack_buf + stacksz - 1 - used_stack_size;
        const size_t bp_sp_offset = (size_t)(bp - sp);

        memcpy(new_sp + 1, sp + 1, used_stack_size * sizeof(wo::value));

        // NOTE: stack reallocate must happen in gc-guard range.
        wo_assert(!check_interrupt(vm_interrupt_type::LEAVE_INTERRUPT));

        do
        {
            _wo_vm_stack_occupying_lock_guard g(this);

            free(stack_storage);

            stack_size = stacksz;
            stack_storage = new_stack_buf;
            sb = new_stack_mem_begin;
            sp = new_sp;
            bp = sp + bp_sp_offset;

        } while (0);

        return true;
    }

    //////////////////////////////////////////////////////////////////////////
    // Runtime Environment Setup
    //////////////////////////////////////////////////////////////////////////

    void vmbase::init_main_vm(shared_pointer<runtime_env> runtime_environment) noexcept
    {
        set_runtime(runtime_environment);

        // Create a new VM using for GC destruct
        (void)make_machine(vm_type::GC_DESTRUCTOR);
    }

    void vmbase::set_runtime(shared_pointer<runtime_env> runtime_environment) noexcept
    {
        // NOTE: There is no need to wo_enter_gcguard here, because when `set_runtime`
        //  is called, there is no value need to be marked in the vm, the vm has not 
        //  been run yet.
        env = runtime_environment;
        ++env->_running_on_vm_count;

        runtime_codes_begin = ip = reinterpret_cast<const irv2::ir*>(env->rt_codes);
        runtime_codes_end = runtime_codes_begin + env->rt_code_len;
        runtime_static_storage = env->constant_and_global_storage;

        _allocate_stack_space(VM_DEFAULT_STACK_SIZE);
        _allocate_register_space(VM_REGISTER_COUNT);

        do
        {
            wo::assure_leave_this_thread_vm_lock_guard g1(_alive_vm_list_mx);
            _gc_ready_vm_list.insert(this);

        } while (0);
    }

    vmbase* vmbase::make_machine(vm_type type) const noexcept
    {
        wo_assert(env != nullptr);

        vmbase* new_vm = create_machine(type);
        new_vm->set_runtime(env);

        return new_vm;
    }

    //////////////////////////////////////////////////////////////////////////
    // Program Dump and Debug Functions
    //////////////////////////////////////////////////////////////////////////

    // Bytecode disassembly helper class
    namespace
    {
        // Print hexadecimal representation of bytecode
        void print_hex_bytes(
            std::ostream& os,
            const byte_t* begin,
            const byte_t* end,
            uint32_t offset) noexcept
        {
            constexpr int MAX_BYTE_COUNT = 12;

            char buf[16];
            snprintf(buf, sizeof(buf), "+%04u : ", offset);
            os << buf;

            int count = 0;
            for (const byte_t* p = begin; p < end; ++p, ++count)
            {
                snprintf(buf, sizeof(buf), "%02X ", static_cast<uint32_t>(*p));
                os << buf;
            }

            // Pad with spaces for alignment
            for (int i = count; i < MAX_BYTE_COUNT; ++i)
                os << "   ";
        }
    } // anonymous namespace

    void vmbase::dump_program_bin(
        const runtime_env* codeholder,
        size_t begin,
        size_t end,
        const irv2::ir* focus_runtime_ip,
        std::ostream& os) noexcept
    {
        const auto end_at = std::min(end, codeholder->rt_code_len);
        for (size_t bi = begin; bi + 4 <= end_at; bi += 4)
        {
            // +XXXXX | XX XX XX XX
            char command_4bytes[32];

            const uint32_t command_line = *std::launder(
                reinterpret_cast<const uint32_t*>(
                    &codeholder->rt_codes[bi]));

            if (0 > snprintf(
                command_4bytes,
                sizeof(command_4bytes),
                "+0x%08zX | %02X %02X %02X %02X ",
                bi,
                (command_line & (0xFF << 24)) >> 24,
                (command_line & (0xFF << 16)) >> 16,
                (command_line & (0xFF << 8)) >> 8,
                (command_line & (0xFF << 0)) >> 0))
            {
                // Should not happend.
                wo_error("Failed to format disassemble line.");
            }

            const auto current_ip_offset = static_cast<size_t>(
                reinterpret_cast<const byte_t*>(focus_runtime_ip) - codeholder->rt_codes);

            const bool is_current_command =
                current_ip_offset >= bi && current_ip_offset < bi + 4;

            if (is_current_command)
                os << ANSI_INV;

            os << command_4bytes;

            if (is_current_command)
                os << ANSI_RST;

            os << "| " << std::endl;
        }
    }

    std::string vmbase::disassemble_instruction(bytecode_disassembler& dis) noexcept
    {
        return "";
    }

    std::string vmbase::disassemble_ext_instruction(bytecode_disassembler& dis) noexcept
    {
        return "";
    }

    void vmbase::dump_call_stack(
        size_t max_count,
        bool need_offset,
        std::ostream& os) const noexcept
    {
        bool trace_finished;
        auto callstacks = dump_call_stack_func_info(max_count, need_offset, &trace_finished);

        for (auto idx = callstacks.cbegin(); idx != callstacks.cend(); ++idx)
        {
            os << (idx - callstacks.cbegin()) << ": " << idx->m_func_name << std::endl;

            os << "\t\t-- at " << idx->m_file_path;

            switch (idx->m_call_way)
            {
            case call_way::NEAR:
                os << " (" << idx->m_row + 1 << ", " << idx->m_col + 1 << ")";
                break;
            case call_way::FAR:
                os << " <far> (" << idx->m_row + 1 << ", " << idx->m_col + 1 << ")";
                break;
            default:
                break;
            }
            os << std::endl;
        }

        if (!trace_finished)
            os << callstacks.size() << ": ..." << std::endl;
    }

    std::vector<vmbase::callstack_info> vmbase::dump_call_stack_func_info(
        size_t max_count,
        bool need_offset,
        bool* out_finished_may_null) const noexcept
    {
        // NOTE: When vm running, rt_ip may point to:
        // [ -- COMMAND 6bit --] [ - DR 2bit -] [ ----- OPNUM1 ------] [ ----- OPNUM2 ------]
        //                                     ^1                     ^2                     ^3
        // If rt_ip point to place 3, 'get_current_func_signature_by_runtime_ip' will get 
        // next command's debuginfo. So we do a move of 1BYTE here, for getting correct debuginfo.

        if (out_finished_may_null != nullptr)
            *out_finished_may_null = true;

        if (env == nullptr)
            return {};

        // Internal structure for tracking callstack IP states
        struct callstack_ip_state
        {
            enum class ipaddr_kind
            {
                ABS,
                OFFSET,
                BAD,
            };

            ipaddr_kind m_type;

            union
            {
                const irv2::ir* m_abs_addr;
                uint32_t m_diff_offset;
            };
            value* m_bp;

            callstack_ip_state()
                : m_type(ipaddr_kind::BAD)
                , m_abs_addr(nullptr)
                , m_bp(nullptr)
            {
            }
            callstack_ip_state(const irv2::ir* abs, value* bp)
                : m_type(ipaddr_kind::ABS)
                , m_abs_addr(abs)
                , m_bp(bp)
            {
            }
            callstack_ip_state(uint32_t diff, value* bp)
                : m_type(ipaddr_kind::OFFSET)
                , m_diff_offset(diff)
                , m_bp(bp)
            {
            }
            callstack_ip_state(const callstack_ip_state&) = default;
            callstack_ip_state(callstack_ip_state&&) = default;
            callstack_ip_state& operator = (const callstack_ip_state&) = default;
            callstack_ip_state& operator = (callstack_ip_state&&) = default;
        };
        std::list<callstack_ip_state> callstack_ips;

        size_t callstack_layer_count = 1;
        callstack_ips.push_back(callstack_ip_state(ip, bp));

        do
        {
            _wo_vm_stack_occupying_lock_guard vsg(this);

            value* base_callstackinfo_ptr = (bp + 1);
            while (base_callstackinfo_ptr <= this->sb)
            {
                switch (base_callstackinfo_ptr->m_type)
                {
                case value::valuetype::callstack:
                {
                    auto callstack = base_callstackinfo_ptr->m_vmcallstack;
                    auto* next_trace_place = this->sb - callstack.bp;

                    ++callstack_layer_count;
                    callstack_ips.push_back(
                        callstack_ip_state(
                            callstack.ret_ip, next_trace_place));

                    // NOTE: Tracing call stack might changed in other thread.
                    //  Check it to make sure donot reach bad place.
                    if (next_trace_place < base_callstackinfo_ptr || next_trace_place > sb)
                        goto _label_bad_callstack;

                    base_callstackinfo_ptr = next_trace_place + 1;
                    break;
                }
                case value::valuetype::far_callstack:
                {
                    auto* next_trace_place = this->sb - base_callstackinfo_ptr->m_ext_farcallstack_bp;

                    ++callstack_layer_count;
                    callstack_ips.push_back(
                        callstack_ip_state(
                            base_callstackinfo_ptr->m_farcallstack, next_trace_place));

                    // NOTE: Tracing call stack might changed in other thread.
                    //  Check it to make sure donot reach bad place.
                    if (next_trace_place < base_callstackinfo_ptr || next_trace_place > sb)
                        goto _label_bad_callstack;

                    base_callstackinfo_ptr = next_trace_place + 1;
                    break;
                }
                case value::valuetype::native_callstack:
                {
                    ++callstack_layer_count;
                    callstack_ips.push_back(
                        callstack_ip_state(
                            base_callstackinfo_ptr->m_nativecallstack, nullptr));

                    // Native callstack didn't store bp, we cannot restore next callstack info directly.
                    // _label_refind_next_callstack will try to find next callstack info.
                    goto _label_refind_next_callstack;
                }
                default:
                {
                _label_bad_callstack:
                    ++callstack_layer_count;
                    callstack_ips.push_back(callstack_ip_state());

                _label_refind_next_callstack:
                    for (;;)
                    {
                        ++base_callstackinfo_ptr;
                        if (base_callstackinfo_ptr <= sb)
                        {
                            auto ptr_value_type = base_callstackinfo_ptr->m_type;

                            if (ptr_value_type == value::valuetype::native_callstack
                                || ptr_value_type == value::valuetype::callstack
                                || ptr_value_type == value::valuetype::far_callstack)
                                break;
                        }
                        else
                            break;
                    }
                    wo_assert(!callstack_ips.empty());
                    callstack_ips.back().m_bp = base_callstackinfo_ptr - 1;
                    break;
                }
                }
            }

        } while (0);

        std::vector<callstack_info> result(std::min(callstack_layer_count, max_count));
        auto generate_callstack_info_with_ip =
            [this, need_offset](const wo::byte_t* rip, runtime_env** inout_env)
            {
                const program_debug_data_info::location* src_location_info = nullptr;
                std::string function_signature;
                std::string file_path;
                size_t row_number = 0;
                size_t col_number = 0;

                runtime_env*& callenv = *inout_env;
                call_way callway;

                if (
                    (rip >= callenv->rt_codes
                        && rip < callenv->rt_codes + callenv->rt_code_len)
                    || runtime_env::fetch_far_runtime_env(
                        reinterpret_cast<const irv2::ir*>(rip), &callenv))
                {
                    if (callenv == env)
                        callway = call_way::NEAR;
                    else
                        callway = call_way::FAR;
                }
                else
                    callway = call_way::NATIVE;

                switch (callway)
                {
                case call_way::NEAR:
                case call_way::FAR:
                {
                    if (callenv->program_debug_info.has_value())
                    {
                        auto& pdi = callenv->program_debug_info.value();

                        src_location_info = &pdi->get_src_location_by_runtime_ip(
                            rip - (need_offset ? 1 : 0));
                        function_signature = pdi->get_current_func_signature_by_runtime_ip(
                            rip - (need_offset ? 1 : 0));

                        file_path = src_location_info->source_file;
                        row_number = src_location_info->begin_row_no;
                        col_number = src_location_info->begin_col_no;
                    }
                    else
                    {
                        char rip_str[sizeof(rip) * 2 + 4];
                        int result = snprintf(rip_str, sizeof(rip_str), "0x%p>", rip);

                        (void)result;
                        wo_assert(result > 0 && result < sizeof(rip_str), "snprintf failed or buffer too small");

                        function_signature = std::string("<unknown function ") + rip_str;
                        file_path = "<unknown file>";
                    }
                    break;
                }
                case call_way::NATIVE:
                {
                    // Is extern native function address.
                    auto fnd = env->extern_native_functions.find(
                        reinterpret_cast<wo_native_func_t>(
                            reinterpret_cast<intptr_t>(rip)));

                    if (fnd != env->extern_native_functions.end())
                    {
                        function_signature = fnd->second.function_name;
                        file_path = fnd->second.library_name.value_or("<builtin>");
                    }
                    else
                    {
                        char rip_str[sizeof(rip) * 2 + 4];
                        int result = snprintf(rip_str, sizeof(rip_str), "0x%p>", rip);

                        (void)result;
                        wo_assert(result > 0 && result < sizeof(rip_str), "snprintf failed or buffer too small");

                        function_signature = std::string("<unknown extern function ") + rip_str;
                        file_path = "<unknown library>";
                    }
                    break;
                }
                default:
                    wo_error("Cannot be here.");
                }

                return callstack_info{
                    function_signature,
                    file_path,
                    row_number,
                    col_number,
                    callway,
                    reinterpret_cast<const irv2::ir*>(rip),
                    nullptr, // Will be set outside.
                };
            };

        runtime_env* current_env_pointer = nullptr;

        // Find current env.
        for (auto& callstack_state : callstack_ips)
        {
            /*
            Why can we find the first-level env in this way?

            ip stores the current call location. If it's a Woolang script function call,
            then ip must point to the code segment of some env (even for JIT function calls).
            In this case, we can use fetch_far_runtime_env to locate the corresponding env.

            If it's a native function call, since Woolang VM version 1.14.14, the calling
            convention requires using far callstack to save the call location for native functions.

            Therefore, regardless of the situation, the first valid ABS address encountered
            in callstack_ips points to the currently running env.
            */
            if (callstack_state.m_type == callstack_ip_state::ipaddr_kind::ABS
                && runtime_env::fetch_far_runtime_env(
                    callstack_state.m_abs_addr,
                    &current_env_pointer))
            {
                // Found.
                break;
            }
        }
        if (current_env_pointer == nullptr)
            // Cannot find env from call stack, use default.
            current_env_pointer = env.get();

        // Walk from callstack top to buttom
        size_t callstack_layer_index = 0;
        for (auto& callstack_state : callstack_ips)
        {
            if (callstack_layer_index >= result.size())
            {
                if (out_finished_may_null != nullptr)
                    *out_finished_may_null = false;
                break;
            }

            bool bad = false;
            const byte_t* callstack_ip;
            switch (callstack_state.m_type)
            {
            case callstack_ip_state::ipaddr_kind::BAD:
                bad = true;
                callstack_ip = nullptr;
                break;
            case callstack_ip_state::ipaddr_kind::ABS:
                callstack_ip = reinterpret_cast<const wo::byte_t*>(callstack_state.m_abs_addr);
                break;
            case callstack_ip_state::ipaddr_kind::OFFSET:
                callstack_ip = current_env_pointer->rt_codes + callstack_state.m_diff_offset;
                break;
            default:
                wo_error("Cannot be here.");
            }

            auto& this_callstack_info = result.at(callstack_layer_index++);
            if (bad)
            {
                this_callstack_info = callstack_info{
                    "??",
                    "<bad callstack>",
                    0,
                    0,
                    call_way::BAD,
                    nullptr,
                    nullptr,
                };
            }
            else
            {
                this_callstack_info = generate_callstack_info_with_ip(
                    callstack_ip,
                    &current_env_pointer);
            }
            this_callstack_info.m_bp = callstack_state.m_bp;
        }

        return result;
    }

    size_t vmbase::callstack_layer() const noexcept
    {
        _wo_vm_stack_occupying_lock_guard vsg(this);

        // NOTE: When vm running, rt_ip may point to:
        // [ -- COMMAND 6bit --] [ - DR 2bit -] [ ----- OPNUM1 ------] [ ----- OPNUM2 ------]
        //                                     ^1                     ^2                     ^3
        // If rt_ip point to place 3, 'get_current_func_signature_by_runtime_ip' will get 
        // next command's debuginfo. So we do a move of 1BYTE here, for getting correct debuginfo.

        size_t call_trace_count = 0;

        value* base_callstackinfo_ptr = (bp + 1);
        while (base_callstackinfo_ptr <= this->sb)
        {
            ++call_trace_count;

            switch (base_callstackinfo_ptr->m_type)
            {
            case value::valuetype::callstack:
                base_callstackinfo_ptr = this->sb - base_callstackinfo_ptr->m_vmcallstack.bp;
                base_callstackinfo_ptr++;
                break;
            case value::valuetype::far_callstack:
                base_callstackinfo_ptr = this->sb - base_callstackinfo_ptr->m_ext_farcallstack_bp;
                base_callstackinfo_ptr++;
                break;
            default:
                goto _label_break_trace;
            }
        }

    _label_break_trace:
        return call_trace_count;
    }

    //////////////////////////////////////////////////////////////////////////
    // GC Checkpoint and VM Type Switching
    //////////////////////////////////////////////////////////////////////////

    void vmbase::gc_checkpoint_self_mark() noexcept
    {
        if (interrupt(vm_interrupt_type::GC_HANGUP_INTERRUPT))
        {
            // In stw GC, if current VM in leaving while marking, and receive GC_HANGUP_INTERRUPT
            // at the end of marking stage. vm might step here and failed to receive GC_INTERRUPT.
            // In this case, we still need to clear GC_HANGUP_INTERRUPT.
            //
            // In very small probability that another round of stw GC start in here. 
            // We will make sure GC_HANGUP_INTERRUPT marked repeatedly until successful in gc-work.
            if (clear_interrupt(vm_interrupt_type::GC_INTERRUPT))
                gc::mark_vm(this, nullptr);

            if (!clear_interrupt(vm_interrupt_type::GC_HANGUP_INTERRUPT))
            {
                // `gc_checkpoint_self_mark` might be invoked in gc-work thread, if vm is WEAK_NORMAL.
                wakeup();
            }
        }
        else if (clear_interrupt(wo::vmbase::vm_interrupt_type::GC_HANGUP_INTERRUPT))
        {
            hangup();
        }
    }

    void vmbase::switch_vm_kind(vm_type new_type) noexcept
    {
        // Must be in gc guard.
        wo_vm switch_back = wo_swap_gcguard(reinterpret_cast<wo_vm>(this));

        // Cannot switch to GC_DESTRUCTOR, and cannot switch from GC_DESTRUCTOR.
        wo_assert(
            virtual_machine_type != vm_type::GC_DESTRUCTOR
            && (new_type == vm_type::NORMAL || new_type == vm_type::WEAK_NORMAL));

        virtual_machine_type = new_type;

        wo_swap_gcguard(switch_back);
    }

    bool vmbase::assure_stack_size(wo_size_t assure_stack_size) noexcept
    {
        wo_assert(!check_interrupt(vm_interrupt_type::LEAVE_INTERRUPT));

        if (sp - assure_stack_size < stack_storage)
        {
            size_t current_stack_size = stack_size;

            while (current_stack_size <= assure_stack_size)
                current_stack_size <<= 1;

            if (!_reallocate_stack_space(current_stack_size << 1))
                wo_fail(WO_FAIL_STACKOVERFLOW, "Stack overflow.");

            return true;
        }

        return false;
    }

    //////////////////////////////////////////////////////////////////////////
    // Coroutine Pre-Invoke Functions
    //////////////////////////////////////////////////////////////////////////

    void vmbase::co_pre_invoke_script(const irv2::ir* wo_func_addr, wo_int_t argc) noexcept
    {
        wo_assert(nullptr != wo_func_addr);

        assure_stack_size(1);

        auto* return_sp = sp;
        auto return_tc = tc->m_integer;

        sp->m_type = value::valuetype::native_callstack;
        sp->m_nativecallstack = ip;
        --sp;

        ip = wo_func_addr;
        tc->set_integer(argc);

        bp = sp;

        // Push return place.
        sp->m_type = value::valuetype::yield_checkpoint;
        sp->m_yield_checkpoint.sp = static_cast<uint32_t>(sb - return_sp);
        sp->m_yield_checkpoint.sp = static_cast<uint32_t>(sb - bp);
        --sp;

        // Push origin tc.
        (sp--)->set_integer(return_tc);
    }

    void vmbase::co_pre_invoke_native(wo_native_func_t ex_func_addr, wo_int_t argc) noexcept
    {
        wo_assert(nullptr != ex_func_addr);

        assure_stack_size(1);

        auto* return_sp = sp;
        auto return_tc = tc->m_integer;

        sp->m_type = value::valuetype::native_callstack;
        sp->m_nativecallstack = ip;
        --sp;

        ip = reinterpret_cast<const irv2::ir*>(ex_func_addr);
        tc->set_integer(argc);

        bp = sp;

        // Push return place.
        sp->m_type = value::valuetype::yield_checkpoint;
        sp->m_yield_checkpoint.sp = static_cast<uint32_t>(sb - return_sp);
        sp->m_yield_checkpoint.sp = static_cast<uint32_t>(sb - bp);
        --sp;

        // Push origin tc.
        (sp--)->set_integer(return_tc);
    }

    void vmbase::co_pre_invoke_closure(closure_t* wo_func_addr, wo_int_t argc) noexcept
    {
        wo_assert(nullptr != wo_func_addr);

        wo::gcbase::gc_read_guard rg1(wo_func_addr);
        assure_stack_size((size_t)wo_func_addr->m_closure_args_count + 1);

        auto* return_sp = sp;
        auto return_tc = tc->m_integer;

        for (uint16_t idx = 0; idx < wo_func_addr->m_closure_args_count; ++idx)
            (sp--)->set_val(&wo_func_addr->m_closure_args[idx]);

        sp->m_type = value::valuetype::native_callstack;
        sp->m_nativecallstack = ip;
        --sp;

        ip = wo_func_addr->m_native_call
            ? (const irv2::ir*)wo_func_addr->m_native_func
            : wo_func_addr->m_vm_func;
        tc->set_integer(argc);

        bp = sp;

        // Push return place.
        sp->m_type = value::valuetype::yield_checkpoint;
        sp->m_yield_checkpoint.sp = static_cast<uint32_t>(sb - return_sp);
        sp->m_yield_checkpoint.sp = static_cast<uint32_t>(sb - bp);
        --sp;

        // Push origin tc.
        (sp--)->set_integer(return_tc);
    }

    //////////////////////////////////////////////////////////////////////////
    // Invoke Functions
    //////////////////////////////////////////////////////////////////////////

    value* vmbase::invoke_script(const irv2::ir* wo_func_addr, wo_int_t argc) noexcept
    {
        wo_assert(nullptr != wo_func_addr);

        assure_stack_size(1);

        auto* return_ip = ip;
        auto return_sp_place = sb - sp;
        auto return_bp_place = sb - bp;
        auto return_tc = tc->m_integer;

        sp->m_type = value::valuetype::native_callstack;
        sp->m_nativecallstack = ip;
        --sp;

        tc->set_integer(argc);
        ip = wo_func_addr;
        bp = sp;

        auto vm_exec_result = run();

        ip = return_ip;
        sp = sb - return_sp_place;
        bp = sb - return_bp_place;
        tc->set_integer(return_tc);

        switch (vm_exec_result)
        {
        case wo_result_t::WO_API_NORMAL:
            return cr;
        case wo_result_t::WO_API_SIM_ABORT:
            break;
        case wo_result_t::WO_API_SIM_YIELD:
            wo_fail(WO_FAIL_CALL_FAIL,
                "The virtual machine is interrupted by `yield`, but the caller is not `dispatch`.");
            break;
        default:
            wo_fail(WO_FAIL_CALL_FAIL, "Unexpected execution status: %d.", (int)vm_exec_result);
            break;
        }

        return nullptr;
    }

    value* vmbase::invoke_native(wo_native_func_t wo_func_addr, wo_int_t argc) noexcept
    {
        wo_assert(nullptr != wo_func_addr);

        if (is_aborted())
            return nullptr;

        assure_stack_size(1);

        auto* return_ip = ip;
        auto return_sp_place = sb - sp;
        auto return_bp_place = sb - bp;
        auto return_tc = tc->m_integer;

        sp->m_type = value::valuetype::native_callstack;
        sp->m_nativecallstack = ip;
        --sp;

        tc->set_integer(argc);
        ip = reinterpret_cast<const wo::irv2::ir*>(wo_func_addr);
        bp = sp;

        auto vm_exec_result = ((wo_native_func_t)wo_func_addr)(
            reinterpret_cast<wo_vm>(this),
            std::launder(reinterpret_cast<wo_value>(sp + 2)));

        switch (vm_exec_result)
        {
        case wo_result_t::WO_API_RESYNC_JIT_STATE_TO_VM_STATE:
            // NOTE: WO_API_RESYNC_JIT_STATE_TO_VM_STATE returned by `wo_func_addr`(and it's a extern function)
            //  Only following cases happened:
            //  1) Stack reallocated.
            //  2) Aborted
            //  3) Yield
            //  For case 1) & 2), return immediately; in case 3), just like invoke std::yield,
            //  let interrupt stay at VM, let it handled outside.
            vm_exec_result = wo_result_t::WO_API_NORMAL;
            [[fallthrough]];
        case wo_result_t::WO_API_NORMAL:
            break;
        case wo_result_t::WO_API_SYNC_CHANGED_VM_STATE:
            vm_exec_result = run();
            break;
        default:
#if WO_ENABLE_RUNTIME_CHECK
            wo_fail(WO_FAIL_CALL_FAIL, "Unexpected execution status: %d.", (int)vm_exec_result);
#endif
            break;
        }

        ip = return_ip;
        sp = sb - return_sp_place;
        bp = sb - return_bp_place;
        tc->set_integer(return_tc);

        switch (vm_exec_result)
        {
        case wo_result_t::WO_API_NORMAL:
            return cr;
        case wo_result_t::WO_API_SIM_ABORT:
            break;
        case wo_result_t::WO_API_SIM_YIELD:
            wo_fail(WO_FAIL_CALL_FAIL,
                "The virtual machine is interrupted by `yield`, but the caller is not `dispatch`.");
            break;
        default:
            wo_fail(WO_FAIL_CALL_FAIL, "Unexpected execution status: %d.", (int)vm_exec_result);
            break;
        }

        return nullptr;
    }

    value* vmbase::invoke_closure(closure_t* wo_func_closure, wo_int_t argc) noexcept
    {
        wo_assert(nullptr != wo_func_closure && nullptr != wo_func_closure->m_vm_func);
        wo::gcbase::gc_read_guard rg1(wo_func_closure);

        if (wo_func_closure->m_native_call && is_aborted())
            return nullptr;

        assure_stack_size((size_t)wo_func_closure->m_closure_args_count + 1);

        auto* return_ip = ip;

        // NOTE: No need to reduce expand arg count.
        auto return_sp_place = sb - sp;
        auto return_bp_place = sb - bp;
        auto return_tc = tc->m_integer;

        for (uint16_t idx = 0; idx < wo_func_closure->m_closure_args_count; ++idx)
            (sp--)->set_val(&wo_func_closure->m_closure_args[idx]);

        sp->m_type = value::valuetype::native_callstack;
        sp->m_nativecallstack = ip;
        --sp;

        tc->set_integer(argc);
        bp = sp;

        wo_result_t vm_exec_result;

        if (wo_func_closure->m_native_call)
        {
            vm_exec_result = wo_func_closure->m_native_func(
                reinterpret_cast<wo_vm>(this),
                std::launder(reinterpret_cast<wo_value>(sp + 2)));

            switch (vm_exec_result)
            {
            case wo_result_t::WO_API_RESYNC_JIT_STATE_TO_VM_STATE:
                // NOTE: WO_API_RESYNC_JIT_STATE_TO_VM_STATE returned by `wo_func_addr`(and it's a extern function)
                //  Only following cases happened:
                //  1) Stack reallocated.
                //  2) Aborted
                //  3) Yield
                //  For case 1) & 2), return immediately; in case 3), just like invoke std::yield,
                //  let interrupt stay at VM, let it handled outside.
                vm_exec_result = wo_result_t::WO_API_NORMAL;
                [[fallthrough]];
            case wo_result_t::WO_API_NORMAL:
                break;
            case wo_result_t::WO_API_SYNC_CHANGED_VM_STATE:
                vm_exec_result = run();
                break;
            default:
#if WO_ENABLE_RUNTIME_CHECK
                wo_fail(WO_FAIL_CALL_FAIL, "Unexpected execution status: %d.", (int)vm_exec_result);
#endif
                break;
            }
        }
        else
        {
            ip = wo_func_closure->m_vm_func;
            vm_exec_result = run();
        }

        ip = return_ip;
        sp = sb - return_sp_place;
        bp = sb - return_bp_place;
        tc->set_integer(return_tc);

        switch (vm_exec_result)
        {
        case wo_result_t::WO_API_NORMAL:
            return cr;
        case wo_result_t::WO_API_SIM_ABORT:
            break;
        case wo_result_t::WO_API_SIM_YIELD:
            wo_fail(WO_FAIL_CALL_FAIL,
                "The virtual machine is interrupted by `yield`, but the caller is not `dispatch`.");
            break;
        default:
            wo_fail(WO_FAIL_CALL_FAIL, "Unexpected execution status: %d.", (int)vm_exec_result);
            break;
        }

        return nullptr;
    }

    //////////////////////////////////////////////////////////////////////////
    // VM Instruction Implementation Helpers
    //////////////////////////////////////////////////////////////////////////

    // VM Operate
#define WO_VM_RETURN(V)                                     \
    do{                                                     \
        ip = rt_ip;    \
        return (V);                                         \
    }while(0)

    //////////////////////////////////////////////////////////////////////////
    // Comparison Operation Implementations
    //////////////////////////////////////////////////////////////////////////

    void vmbase::ltx_impl(value* result, value* opnum1, value* opnum2) noexcept
    {
        switch (opnum1->m_type)
        {
        case value::valuetype::integer_type:
            result->set_bool(opnum1->m_integer < opnum2->m_integer);
            break;
        case value::valuetype::handle_type:
            result->set_bool(opnum1->m_handle < opnum2->m_handle);
            break;
        case value::valuetype::real_type:
            result->set_bool(opnum1->m_real < opnum2->m_real);
            break;
        case value::valuetype::string_type:
            result->set_bool(*opnum1->m_string < *opnum2->m_string);
            break;
        default:
            result->set_bool(false);
            wo_fail(WO_FAIL_TYPE_FAIL, "Values of this type cannot be compared.");
            break;
        }
    }

    void vmbase::eltx_impl(value* result, value* opnum1, value* opnum2) noexcept
    {
        switch (opnum1->m_type)
        {
        case value::valuetype::integer_type:
            result->set_bool(opnum1->m_integer <= opnum2->m_integer);
            break;
        case value::valuetype::handle_type:
            result->set_bool(opnum1->m_handle <= opnum2->m_handle);
            break;
        case value::valuetype::real_type:
            result->set_bool(opnum1->m_real <= opnum2->m_real);
            break;
        case value::valuetype::string_type:
            result->set_bool(*opnum1->m_string <= *opnum2->m_string);
            break;
        default:
            result->set_bool(false);
            wo_fail(WO_FAIL_TYPE_FAIL, "Values of this type cannot be compared.");
            break;
        }
    }

    void vmbase::gtx_impl(value* result, value* opnum1, value* opnum2) noexcept
    {
        switch (opnum1->m_type)
        {
        case value::valuetype::integer_type:
            result->set_bool(opnum1->m_integer > opnum2->m_integer);
            break;
        case value::valuetype::handle_type:
            result->set_bool(opnum1->m_handle > opnum2->m_handle);
            break;
        case value::valuetype::real_type:
            result->set_bool(opnum1->m_real > opnum2->m_real);
            break;
        case value::valuetype::string_type:
            result->set_bool(*opnum1->m_string > *opnum2->m_string);
            break;
        default:
            result->set_bool(false);
            wo_fail(WO_FAIL_TYPE_FAIL, "Values of this type cannot be compared.");
            break;
        }
    }

    void vmbase::egtx_impl(value* result, value* opnum1, value* opnum2) noexcept
    {
        switch (opnum1->m_type)
        {
        case value::valuetype::integer_type:
            result->set_bool(opnum1->m_integer >= opnum2->m_integer);
            break;
        case value::valuetype::handle_type:
            result->set_bool(opnum1->m_handle >= opnum2->m_handle);
            break;
        case value::valuetype::real_type:
            result->set_bool(opnum1->m_real >= opnum2->m_real);
            break;
        case value::valuetype::string_type:
            result->set_bool(*opnum1->m_string >= *opnum2->m_string);
            break;
        default:
            result->set_bool(false);
            wo_fail(WO_FAIL_TYPE_FAIL, "Values of this type cannot be compared.");
            break;
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // Value Construction Implementations
    //////////////////////////////////////////////////////////////////////////

    value* vmbase::make_union_impl(value* opnum1, value* opnum2, uint16_t id) noexcept
    {
        auto* union_struct = structure_t::gc_new<gcbase::gctype::young>(
            static_cast<uint16_t>(2));

        union_struct->m_values[0].set_integer((wo_integer_t)id);
        union_struct->m_values[1].set_val(opnum2);

        opnum1->set_gcunit<value::valuetype::struct_type>(union_struct);

        return opnum1;
    }

    value* vmbase::make_closure_wo_impl(
        value* opnum1,
        uint16_t argc,
        const irv2::ir* addr,
        value* rt_sp) noexcept
    {
        auto* created_closure = closure_t::gc_new<gcbase::gctype::young>(addr, argc);

        for (uint16_t i = 0; i < argc; i++)
        {
            auto* arg_val = ++rt_sp;
            created_closure->m_closure_args[i].set_val(arg_val);
        }

        opnum1->set_gcunit<wo::value::valuetype::closure_type>(created_closure);
        return rt_sp;
    }

    value* vmbase::make_closure_fp_impl(
        value* opnum1,
        uint16_t argc,
        wo_native_func_t addr,
        value* rt_sp) noexcept
    {
        auto* created_closure = closure_t::gc_new<gcbase::gctype::young>(addr, argc);

        for (uint16_t i = 0; i < argc; i++)
        {
            auto* arg_val = ++rt_sp;
            created_closure->m_closure_args[i].set_val(arg_val);
        }

        opnum1->set_gcunit<wo::value::valuetype::closure_type>(created_closure);
        return rt_sp;
    }

    value* vmbase::make_array_impl(value* opnum1, uint32_t size, value* rt_sp) noexcept
    {
        auto* maked_array = array_t::gc_new<gcbase::gctype::young>((size_t)size);

        for (uint32_t i = 0; i < size; i++)
        {
            auto* arr_val = ++rt_sp;
            maked_array->at(size - i - 1).set_val(arr_val);
        }

        opnum1->set_gcunit<value::valuetype::array_type>(maked_array);
        return rt_sp;
    }

    value* vmbase::make_map_impl(value* opnum1, uint32_t size, value* rt_sp) noexcept
    {
        auto* maked_dict = dictionary_t::gc_new<gcbase::gctype::young>((size_t)size);

        for (uint32_t i = 0; i < size; i++)
        {
            value* val = ++rt_sp;
            value* key = ++rt_sp;
            maked_dict->insert(std::make_pair(*key, *val));
        }

        opnum1->set_gcunit<value::valuetype::dict_type>(maked_dict);
        return rt_sp;
    }

    value* vmbase::make_struct_impl(value* opnum1, uint32_t size, value* rt_sp) noexcept
    {
        auto* maked_struct = structure_t::gc_new<gcbase::gctype::young>(size);

        for (uint32_t i = 0; i < size; i++)
            maked_struct->m_values[size - i - 1].set_val(++rt_sp);

        opnum1->set_gcunit<value::valuetype::struct_type>(maked_struct);
        return rt_sp;
    }

    //////////////////////////////////////////////////////////////////////////
    // Argument Packing/Unpacking Implementations
    //////////////////////////////////////////////////////////////////////////

    void vmbase::packargs_impl(
        value* opnum1,
        value* rt_bp,
        wo_integer_t    func_argument_count,
        uint16_t        func_named_param,
        uint16_t        closure_captured_count) noexcept
    {
        auto* packed_array = array_t::gc_new<gcbase::gctype::young>(
            func_argument_count - func_named_param);

        for (size_t argindex = 0 + func_named_param;
            argindex < static_cast<size_t>(func_argument_count);
            ++argindex)
        {
            packed_array->at(argindex - func_named_param).set_val(
                rt_bp + 2 + argindex + closure_captured_count);
        }

        opnum1->set_gcunit<wo::value::valuetype::array_type>(packed_array);
    }

    //////////////////////////////////////////////////////////////////////////
    // Type Casting Implementation
    //////////////////////////////////////////////////////////////////////////

    const char* vmbase::movcast_impl(
        value* opnum1,
        value* opnum2,
        value::valuetype aim_type) noexcept
    {
        if (aim_type == opnum2->m_type)
        {
            opnum1->set_val(opnum2);
        }
        else
        {
            switch (aim_type)
            {
            case value::valuetype::integer_type:
                opnum1->set_integer(
                    wo_cast_int(std::launder(reinterpret_cast<wo_value>(opnum2))));
                break;
            case value::valuetype::real_type:
                opnum1->set_real(
                    wo_cast_real(std::launder(reinterpret_cast<wo_value>(opnum2))));
                break;
            case value::valuetype::handle_type:
                opnum1->set_handle(
                    wo_cast_handle(std::launder(reinterpret_cast<wo_value>(opnum2))));
                break;
            case value::valuetype::string_type:
                opnum1->set_string(
                    wo_cast_string(std::launder(reinterpret_cast<wo_value>(opnum2))));
                break;
            case value::valuetype::bool_type:
                opnum1->set_bool(
                    wo_cast_bool(std::launder(reinterpret_cast<wo_value>(opnum2))));
                break;
            case value::valuetype::array_type:
                return "Cannot cast this value to 'array'.";
            case value::valuetype::dict_type:
                return "Cannot cast this value to 'dict'.";
            case value::valuetype::gchandle_type:
                return "Cannot cast this value to 'gchandle'.";
            case value::valuetype::invalid:
                return "Cannot cast this value to 'nil'.";
            default:
                return "Unknown type.";
            }
        }

        return nullptr;
    }

    //////////////////////////////////////////////////////////////////////////
    // VM Creation and Execution Entry Points
    //////////////////////////////////////////////////////////////////////////

    vmbase* vmbase::create_machine(vm_type type) const noexcept
    {
        return new vmbase(type);
    }

    wo_result_t vmbase::run() noexcept
    {
        if (ip >= runtime_codes_begin && ip < runtime_codes_end + env->rt_code_len)
            return run_sim();

        if (runtime_env::fetch_is_far_addr(ip))
        {
            interrupt(vm_interrupt_type::CALL_FAR_RESYNC_VM_STATE_INTERRUPT);
            return run_sim();
        }

        return ((wo_extern_native_func_t)ip)(
            reinterpret_cast<wo_vm>(this),
            std::launder(reinterpret_cast<wo_value>(sp + 2)));
    }

    //////////////////////////////////////////////////////////////////////////
    // VM Simulation Loop
    // 
    // NOTE: The run_sim function should NOT be modified as per user request.
    //////////////////////////////////////////////////////////////////////////

    wo_result_t vmbase::run_sim() noexcept
    {
        // Must not leave when run.
        wo_assert(!check_interrupt(vm_interrupt_type::LEAVE_INTERRUPT));
        wo_assert(_this_thread_gc_guard_vm == this);

        const irv2::ir
            * near_rtcode_begin = reinterpret_cast<const irv2::ir*>(
                runtime_codes_begin),
            * near_rtcode_end = reinterpret_cast<const irv2::ir*>(
                runtime_codes_end);
        value* near_static_global = runtime_static_storage;

        value* const rt_cr = cr;
        value* const rt_reg_fast_lookup = register_storage - 0b01100000;

        const irv2::ir* rt_ip = ip;

#define WO_VM_INTERRUPT_CHECKPOINT          \
        fast_interrupt_state =              \
            vm_interrupt.load(std::memory_order_acquire)

#define WO_VM_FAIL(ERRNO, ...)                          \
    do {                                                \
        ip = rt_ip;\
        wo_fail(ERRNO, __VA_ARGS__);                    \
        WO_VM_INTERRUPT_CHECKPOINT;                     \
        goto re_entry_for_failed_command;               \
    } while(0)

#if WO_ENABLE_RUNTIME_CHECK == 0
#   define WO_VM_ASSERT(EXPR, REASON) wo_assert(EXPR, REASON)
#else
#   define WO_VM_ASSERT(EXPR, ...)                   \
        do {                                            \
            if(!(EXPR))                                 \
                WO_VM_FAIL(WO_FAIL_UNEXPECTED, __VA_ARGS__); \
        } while(0)
#endif

        uint32_t WO_VM_INTERRUPT_CHECKPOINT;
        for (;;)
        {
        re_entry_for_failed_command:
            uint32_t rtopcode = WO_IR_OPBYTE(rt_ip);

            if (fast_interrupt_state)
                rtopcode |= fast_interrupt_state;

        re_entry_for_interrupt:

            switch (rtopcode)
            {
                // NOP
                WO_VM_BEGIN_CASE_IR8(NOP, 0, 24)
                {
                    // Do nothing~
                }
                WO_VM_END_CASE();

                // END
                WO_VM_BEGIN_CASE_IR8(END, 0, 24)
                {
                    WO_VM_RETURN(wo_result_t::WO_API_NORMAL);
                }
                WO_VM_END_CASE();

                // LOAD
                WO_VM_BEGIN_CASE_IR6(LOAD, I18_I8)
                {
                    WO_VM_ADRS_R_S(p2_i8)->set_val(WO_VM_ADRS_C_G(p1_i18));
                }
                WO_VM_END_CASE();

                // STORE
                WO_VM_BEGIN_CASE_IR6(STORE, I18_I8)
                {
                    value* cg = WO_VM_ADRS_C_G(p1_i18);
                    if (gc::gc_is_marking())
                        value::write_barrier(cg);

                    cg->set_val(WO_VM_ADRS_R_S(p2_i8));
                }
                WO_VM_END_CASE();

                // LOADEXT
                WO_VM_BEGIN_CASE_IR8(LOADEXT, 0, I8_16_EI32)
                {
                    WO_VM_ADRS_R_S(p1_i8)->set_val(WO_VM_ADRS_C_G(p2_i32));
                }
                WO_VM_END_CASE();

                // LOADEXT 16
                WO_VM_BEGIN_CASE_IR8(LOADEXT, 1, 8_I16_EI32)
                {
                    WO_VM_ADRS_S(p1_i16)->set_val(WO_VM_ADRS_C_G(p2_i32));
                }
                WO_VM_END_CASE();

                // LOADEXT 24
                WO_VM_BEGIN_CASE_IR8(LOADEXT, 2, I24_EI32)
                {
                    WO_VM_ADRS_S(p1_i24)->set_val(WO_VM_ADRS_C_G(p2_i32));
                }
                WO_VM_END_CASE();

                // STOREEXT
                WO_VM_BEGIN_CASE_IR8(STOREEXT, 0, I8_16_EI32)
                {
                    value* cg = WO_VM_ADRS_C_G(p2_i32);
                    if (gc::gc_is_marking())
                        value::write_barrier(cg);

                    cg->set_val(WO_VM_ADRS_R_S(p1_i8));
                }
                WO_VM_END_CASE();

                // STOREEXT 16
                WO_VM_BEGIN_CASE_IR8(STOREEXT, 1, 8_I16_EI32)
                {
                    value* cg = WO_VM_ADRS_C_G(p2_i32);
                    if (gc::gc_is_marking())
                        value::write_barrier(cg);
                    cg->set_val(WO_VM_ADRS_S(p1_i16));
                }
                WO_VM_END_CASE();

                // STOREEXT 24
                WO_VM_BEGIN_CASE_IR8(STOREEXT, 2, I24_EI32)
                {
                    value* cg = WO_VM_ADRS_C_G(p2_i32);
                    if (gc::gc_is_marking())
                        value::write_barrier(cg);
                    cg->set_val(WO_VM_ADRS_S(p1_i24));
                }
                WO_VM_END_CASE();

                // PUSH RESERVE STACK COUNT
                WO_VM_BEGIN_CASE_IR8(PUSH, 0, U24)
                {
                    value* new_sp = sp - p1_u24;

                    if (new_sp >= stack_storage)
                        sp = new_sp;
                    else
                    {
                        wo_assure(interrupt(vm_interrupt_type::STACK_OVERFLOW_INTERRUPT));
                        WO_VM_INTERRUPT_CHECKPOINT;

                        // Break out, donot `WO_VM_END_CASE`
                        break;
                    }
                }
                WO_VM_END_CASE();

                // PUSH RS
                WO_VM_BEGIN_CASE_IR8(PUSH, 1, I8_16)
                {
                    if (sp > stack_storage)
                        (sp--)->set_val(WO_VM_ADRS_R_S(p1_i8));
                    else
                    {
                        wo_assure(interrupt(vm_interrupt_type::STACK_OVERFLOW_INTERRUPT));
                        WO_VM_INTERRUPT_CHECKPOINT;

                        // Break out, donot `WO_VM_END_CASE`
                        break;
                    }
                }
                WO_VM_END_CASE();

                // PUSH CG
                WO_VM_BEGIN_CASE_IR8(PUSH, 2, I24)
                {
                    if (sp > stack_storage)
                        (sp--)->set_val(WO_VM_ADRS_C_G(p1_i24));
                    else
                    {
                        wo_assure(interrupt(vm_interrupt_type::STACK_OVERFLOW_INTERRUPT));
                        WO_VM_INTERRUPT_CHECKPOINT;

                        // Break out, donot `WO_VM_END_CASE`
                        break;
                    }
                }
                WO_VM_END_CASE();

                // PUSH CG-EXT
                WO_VM_BEGIN_CASE_IR8(PUSH, 3, 24_EI32)
                {
                    if (sp > stack_storage)
                        (sp--)->set_val(WO_VM_ADRS_C_G(p1_i32));
                    else
                    {
                        wo_assure(interrupt(vm_interrupt_type::STACK_OVERFLOW_INTERRUPT));
                        WO_VM_INTERRUPT_CHECKPOINT;

                        // Break out, donot `WO_VM_END_CASE`
                        break;
                    }
                }
                WO_VM_END_CASE();

                // POP TO SHRINK STACK
                WO_VM_BEGIN_CASE_IR8(POP, 0, U24)
                {
                    sp -= p1_u24;
                }
                WO_VM_END_CASE();

                // POP RS
                WO_VM_BEGIN_CASE_IR8(POP, 1, I8_16)
                {
                    WO_VM_ADRS_R_S(p1_i8)->set_val((++sp));
                }
                WO_VM_END_CASE();

                // POP CG
                WO_VM_BEGIN_CASE_IR8(POP, 2, I24)
                {
                    value* cg = WO_VM_ADRS_C_G(p1_i24);
                    if (gc::gc_is_marking())
                        value::write_barrier(cg);

                    cg->set_val((++sp));
                }
                WO_VM_END_CASE();

                // POP CG-EXT
                WO_VM_BEGIN_CASE_IR8(POP, 3, 24_EI32)
                {
                    value* cg = WO_VM_ADRS_C_G(p1_i32);
                    if (gc::gc_is_marking())
                        value::write_barrier(cg);

                    cg->set_val((++sp));
                }
                WO_VM_END_CASE();

                // CASTX
                WO_VM_BEGIN_CASE_IR8(CAST, 0, I8_I8_U8)
                {
                    if (const auto* error_msg = movcast_impl(
                        WO_VM_ADRS_R_S(p1_i8),
                        WO_VM_ADRS_R_S(p2_i8),
                        static_cast<value::valuetype>(p3_u8)))
                    {
                        WO_VM_FAIL(WO_FAIL_TYPE_FAIL, "%s", error_msg);
                    }
                }
                WO_VM_END_CASE();

                // CASTITOR
                WO_VM_BEGIN_CASE_IR8(CAST, 1, I8_I8_8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->set_real(
                        static_cast<wo_real_t>(WO_VM_ADRS_R_S(p2_i8)->m_integer));
                }
                WO_VM_END_CASE();

                // CASTRTOI
                WO_VM_BEGIN_CASE_IR8(CAST, 2, I8_I8_8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->set_integer(
                        static_cast<wo_integer_t>(WO_VM_ADRS_R_S(p2_i8)->m_real));
                }
                WO_VM_END_CASE();

                // TYPEIS
                WO_VM_BEGIN_CASE_IR8(TYPECHK, 0, I8_I8_U8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->set_bool(
                        static_cast<value::valuetype>(
                            p3_u8) == WO_VM_ADRS_R_S(p2_i8)->m_type);
                }
                WO_VM_END_CASE();

                // TYPEAS
                WO_VM_BEGIN_CASE_IR8(TYPECHK, 1, 8_I8_U8)
                {
                    if (static_cast<value::valuetype>(
                        p2_u8) != WO_VM_ADRS_R_S(p1_i8)->m_type)
                    {
                        WO_VM_FAIL(WO_FAIL_TYPE_FAIL,
                            "The given value is not the same as the requested type.");
                    }
                }
                WO_VM_END_CASE();

                // ADDI
                WO_VM_BEGIN_CASE_IR8(OPIA, 0, I8_I8_I8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->set_integer(
                        WO_VM_ADRS_R_S(p2_i8)->m_integer
                        + WO_VM_ADRS_R_S(p3_i8)->m_integer);
                }
                WO_VM_END_CASE();

                // SUBI
                WO_VM_BEGIN_CASE_IR8(OPIA, 1, I8_I8_I8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->set_integer(
                        WO_VM_ADRS_R_S(p2_i8)->m_integer
                        - WO_VM_ADRS_R_S(p3_i8)->m_integer);
                }
                WO_VM_END_CASE();

                // MULI
                WO_VM_BEGIN_CASE_IR8(OPIA, 2, I8_I8_I8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->set_integer(
                        WO_VM_ADRS_R_S(p2_i8)->m_integer
                        * WO_VM_ADRS_R_S(p3_i8)->m_integer);
                }
                WO_VM_END_CASE();

                // DIVI
                WO_VM_BEGIN_CASE_IR8(OPIA, 3, I8_I8_I8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->set_integer(
                        WO_VM_ADRS_R_S(p2_i8)->m_integer
                        / WO_VM_ADRS_R_S(p3_i8)->m_integer);
                }
                WO_VM_END_CASE();

                // MODI
                WO_VM_BEGIN_CASE_IR8(OPIB, 0, I8_I8_I8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->set_integer(
                        WO_VM_ADRS_R_S(p2_i8)->m_integer
                        % WO_VM_ADRS_R_S(p3_i8)->m_integer);
                }
                WO_VM_END_CASE();

                // NEGI
                WO_VM_BEGIN_CASE_IR8(OPIB, 1, I8_I8_8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->set_integer(
                        -WO_VM_ADRS_R_S(p2_i8)->m_integer);
                }
                WO_VM_END_CASE();

                // LTI
                WO_VM_BEGIN_CASE_IR8(OPIB, 2, I8_I8_I8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->set_bool(
                        WO_VM_ADRS_R_S(p2_i8)->m_integer
                        < WO_VM_ADRS_R_S(p3_i8)->m_integer);
                }
                WO_VM_END_CASE();

                // GTI
                WO_VM_BEGIN_CASE_IR8(OPIB, 3, I8_I8_I8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->set_bool(
                        WO_VM_ADRS_R_S(p2_i8)->m_integer
                        > WO_VM_ADRS_R_S(p3_i8)->m_integer);
                }
                WO_VM_END_CASE();

                // ELTI
                WO_VM_BEGIN_CASE_IR8(OPIC, 0, I8_I8_I8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->set_bool(
                        WO_VM_ADRS_R_S(p2_i8)->m_integer
                        <= WO_VM_ADRS_R_S(p3_i8)->m_integer);
                }
                WO_VM_END_CASE();

                // EGTI
                WO_VM_BEGIN_CASE_IR8(OPIC, 1, I8_I8_I8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->set_bool(
                        WO_VM_ADRS_R_S(p2_i8)->m_integer
                        >= WO_VM_ADRS_R_S(p3_i8)->m_integer);
                }
                WO_VM_END_CASE();

                // EQUB
                WO_VM_BEGIN_CASE_IR8(OPIC, 2, I8_I8_I8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->set_bool(
                        WO_VM_ADRS_R_S(p2_i8)->m_integer
                        == WO_VM_ADRS_R_S(p3_i8)->m_integer);
                }
                WO_VM_END_CASE();

                // NEQUB
                WO_VM_BEGIN_CASE_IR8(OPIC, 3, I8_I8_I8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->set_bool(
                        WO_VM_ADRS_R_S(p2_i8)->m_integer
                        != WO_VM_ADRS_R_S(p3_i8)->m_integer);
                }
                WO_VM_END_CASE();

                // ADDR
                WO_VM_BEGIN_CASE_IR8(OPRA, 0, I8_I8_I8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->set_real(
                        WO_VM_ADRS_R_S(p2_i8)->m_real
                        + WO_VM_ADRS_R_S(p3_i8)->m_real);
                }
                WO_VM_END_CASE();

                // SUBR
                WO_VM_BEGIN_CASE_IR8(OPRA, 1, I8_I8_I8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->set_real(
                        WO_VM_ADRS_R_S(p2_i8)->m_real
                        - WO_VM_ADRS_R_S(p3_i8)->m_real);
                }
                WO_VM_END_CASE();

                // MULR
                WO_VM_BEGIN_CASE_IR8(OPRA, 2, I8_I8_I8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->set_real(
                        WO_VM_ADRS_R_S(p2_i8)->m_real
                        * WO_VM_ADRS_R_S(p3_i8)->m_real);
                }
                WO_VM_END_CASE();

                // DIVR
                WO_VM_BEGIN_CASE_IR8(OPRA, 3, I8_I8_I8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->set_real(
                        WO_VM_ADRS_R_S(p2_i8)->m_real
                        / WO_VM_ADRS_R_S(p3_i8)->m_real);
                }
                WO_VM_END_CASE();

                // MODR
                WO_VM_BEGIN_CASE_IR8(OPRB, 0, I8_I8_I8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->set_real(
                        std::fmod(
                            WO_VM_ADRS_R_S(p2_i8)->m_real,
                            WO_VM_ADRS_R_S(p3_i8)->m_real));
                }
                WO_VM_END_CASE();

                // NEGR
                WO_VM_BEGIN_CASE_IR8(OPRB, 1, I8_I8_8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->set_real(
                        -WO_VM_ADRS_R_S(p2_i8)->m_real);
                }
                WO_VM_END_CASE();

                // LTR
                WO_VM_BEGIN_CASE_IR8(OPRB, 2, I8_I8_I8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->set_bool(
                        WO_VM_ADRS_R_S(p2_i8)->m_real
                        < WO_VM_ADRS_R_S(p3_i8)->m_real);
                }
                WO_VM_END_CASE();

                // GTR
                WO_VM_BEGIN_CASE_IR8(OPRB, 3, I8_I8_I8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->set_bool(
                        WO_VM_ADRS_R_S(p2_i8)->m_real
                        > WO_VM_ADRS_R_S(p3_i8)->m_real);
                }
                WO_VM_END_CASE();

                // ELTR
                WO_VM_BEGIN_CASE_IR8(OPRC, 0, I8_I8_I8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->set_bool(
                        WO_VM_ADRS_R_S(p2_i8)->m_real
                        <= WO_VM_ADRS_R_S(p3_i8)->m_real);
                }
                WO_VM_END_CASE();

                // EGTR
                WO_VM_BEGIN_CASE_IR8(OPRC, 1, I8_I8_I8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->set_bool(
                        WO_VM_ADRS_R_S(p2_i8)->m_real
                        >= WO_VM_ADRS_R_S(p3_i8)->m_real);
                }
                WO_VM_END_CASE();

                // EQUR
                WO_VM_BEGIN_CASE_IR8(OPRC, 2, I8_I8_I8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->set_bool(
                        WO_VM_ADRS_R_S(p2_i8)->m_real
                        == WO_VM_ADRS_R_S(p3_i8)->m_real);
                }
                WO_VM_END_CASE();

                // NEQUR
                WO_VM_BEGIN_CASE_IR8(OPRC, 3, I8_I8_I8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->set_bool(
                        WO_VM_ADRS_R_S(p2_i8)->m_real
                        != WO_VM_ADRS_R_S(p3_i8)->m_real);
                }
                WO_VM_END_CASE();

                // ADDS
                WO_VM_BEGIN_CASE_IR8(OPSA, 0, I8_I8_I8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->set_gcunit<wo::value::valuetype::string_type>(
                        string_t::gc_new<gcbase::gctype::young>(
                            *WO_VM_ADRS_R_S(p2_i8)->m_string + *WO_VM_ADRS_R_S(p3_i8)->m_string));
                }
                WO_VM_END_CASE();

                // LTS
                WO_VM_BEGIN_CASE_IR8(OPSA, 1, I8_I8_I8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->set_bool(
                        *WO_VM_ADRS_R_S(p2_i8)->m_string < *WO_VM_ADRS_R_S(p3_i8)->m_string);
                }
                WO_VM_END_CASE();

                // GTS
                WO_VM_BEGIN_CASE_IR8(OPSA, 2, I8_I8_I8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->set_bool(
                        *WO_VM_ADRS_R_S(p2_i8)->m_string > *WO_VM_ADRS_R_S(p3_i8)->m_string);
                }
                WO_VM_END_CASE();

                // ELTS
                WO_VM_BEGIN_CASE_IR8(OPSA, 3, I8_I8_I8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->set_bool(
                        *WO_VM_ADRS_R_S(p2_i8)->m_string <= *WO_VM_ADRS_R_S(p3_i8)->m_string);
                }
                WO_VM_END_CASE();

                // EGTS
                WO_VM_BEGIN_CASE_IR8(OPSB, 0, I8_I8_I8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->set_bool(
                        *WO_VM_ADRS_R_S(p2_i8)->m_string >= *WO_VM_ADRS_R_S(p3_i8)->m_string);
                }
                WO_VM_END_CASE();

                // EQUS
                WO_VM_BEGIN_CASE_IR8(OPSB, 1, I8_I8_I8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->set_bool(
                        *WO_VM_ADRS_R_S(p2_i8)->m_string == *WO_VM_ADRS_R_S(p3_i8)->m_string);
                }
                WO_VM_END_CASE();

                // NEQUS
                WO_VM_BEGIN_CASE_IR8(OPSB, 2, I8_I8_I8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->set_bool(
                        *WO_VM_ADRS_R_S(p2_i8)->m_string != *WO_VM_ADRS_R_S(p3_i8)->m_string);
                }
                WO_VM_END_CASE();

                // CDIVILR
                WO_VM_BEGIN_CASE_IR8(OPCA, 0, I8_I8_8)
                {
                    const wo_integer_t divisor =
                        WO_VM_ADRS_R_S(p2_i8)->m_integer;

                    if (divisor == 0)
                        WO_VM_FAIL(WO_FAIL_UNEXPECTED, "The divisor cannot be 0.");
                    else if (divisor == -1 && WO_VM_ADRS_R_S(p1_i8)->m_integer == INT64_MIN)
                        WO_VM_FAIL(WO_FAIL_UNEXPECTED, "Division overflow.");
                }
                WO_VM_END_CASE();

                // CDIVIL
                WO_VM_BEGIN_CASE_IR8(OPCA, 1, I8_16)
                {
                    if (WO_VM_ADRS_R_S(p1_i8)->m_integer == INT64_MIN)
                        WO_VM_FAIL(WO_FAIL_UNEXPECTED, "Division overflow.");
                }
                WO_VM_END_CASE();

                // CDIVIR
                WO_VM_BEGIN_CASE_IR8(OPCA, 2, I8_16)
                {
                    const wo_integer_t divisor =
                        WO_VM_ADRS_R_S(p1_i8)->m_integer;

                    if (divisor == 0)
                        WO_VM_FAIL(WO_FAIL_UNEXPECTED, "The divisor cannot be 0.");
                    else if (divisor == -1)
                        WO_VM_FAIL(WO_FAIL_UNEXPECTED, "Division overflow.");
                }
                WO_VM_END_CASE();

                // CDIVIRZ
                WO_VM_BEGIN_CASE_IR8(OPCA, 3, I8_16)
                {
                    if (WO_VM_ADRS_R_S(p1_i8)->m_integer == 0)
                        WO_VM_FAIL(WO_FAIL_UNEXPECTED, "The divisor cannot be 0.");
                }
                WO_VM_END_CASE();

                // CUMADDI
                WO_VM_BEGIN_CASE_IR8(OPCB, 0, I8_I8_8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->m_integer +=
                        WO_VM_ADRS_R_S(p2_i8)->m_integer;
                }
                WO_VM_END_CASE();

                // CUMSUBI
                WO_VM_BEGIN_CASE_IR8(OPCB, 1, I8_I8_8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->m_integer -=
                        WO_VM_ADRS_R_S(p2_i8)->m_integer;
                }
                WO_VM_END_CASE();

                // CUMMULI
                WO_VM_BEGIN_CASE_IR8(OPCB, 2, I8_I8_8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->m_integer *=
                        WO_VM_ADRS_R_S(p2_i8)->m_integer;
                }
                WO_VM_END_CASE();

                // CUMDIVI
                WO_VM_BEGIN_CASE_IR8(OPCB, 3, I8_I8_8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->m_integer /=
                        WO_VM_ADRS_R_S(p2_i8)->m_integer;
                }
                WO_VM_END_CASE();

                // CUMADDR
                WO_VM_BEGIN_CASE_IR8(OPCC, 0, I8_I8_8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->m_real +=
                        WO_VM_ADRS_R_S(p2_i8)->m_real;
                }
                WO_VM_END_CASE();

                // CUMSUBR
                WO_VM_BEGIN_CASE_IR8(OPCC, 1, I8_I8_8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->m_real -=
                        WO_VM_ADRS_R_S(p2_i8)->m_real;
                }
                WO_VM_END_CASE();

                // CUMMULR
                WO_VM_BEGIN_CASE_IR8(OPCC, 2, I8_I8_8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->m_real *=
                        WO_VM_ADRS_R_S(p2_i8)->m_real;
                }
                WO_VM_END_CASE();

                // CUMDIVR
                WO_VM_BEGIN_CASE_IR8(OPCC, 3, I8_I8_8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->m_real /=
                        WO_VM_ADRS_R_S(p2_i8)->m_real;
                }
                WO_VM_END_CASE();

                // CUMADDS
                WO_VM_BEGIN_CASE_IR8(OPCD, 0, I8_I8_8)
                {
                    value* const string_val = WO_VM_ADRS_R_S(p1_i8);
                    string_val->m_string =
                        string_t::gc_new<gcbase::gctype::young>(
                            *string_val->m_string
                            + *WO_VM_ADRS_R_S(p2_i8)->m_string);
                }
                WO_VM_END_CASE();

                // CUMADDSF
                WO_VM_BEGIN_CASE_IR8(OPCD, 1, I8_I8_8)
                {
                    value* const string_val = WO_VM_ADRS_R_S(p1_i8);
                    string_val->m_string =
                        string_t::gc_new<gcbase::gctype::young>(
                            *WO_VM_ADRS_R_S(p2_i8)->m_string
                            + *string_val->m_string);
                }
                WO_VM_END_CASE();

                // CUMMODI
                WO_VM_BEGIN_CASE_IR8(OPCD, 2, I8_I8_8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->m_integer %=
                        WO_VM_ADRS_R_S(p2_i8)->m_integer;
                }
                WO_VM_END_CASE();

                // CUMMODR
                WO_VM_BEGIN_CASE_IR8(OPCD, 3, I8_I8_8)
                {
                    value* const real_val = WO_VM_ADRS_R_S(p1_i8);
                    real_val->m_real =
                        std::fmod(
                            real_val->m_real,
                            WO_VM_ADRS_R_S(p2_i8)->m_real);
                }
                WO_VM_END_CASE();

                // CUMLAND
                WO_VM_BEGIN_CASE_IR8(OPCE, 0, I8_I8_8)
                {
                    value* const bool_val = WO_VM_ADRS_R_S(p1_i8);
                    bool_val->m_integer =
                        bool_val->m_integer != 0
                        && WO_VM_ADRS_R_S(p2_i8)->m_integer != 0;
                }
                WO_VM_END_CASE();

                // CUMLOR
                WO_VM_BEGIN_CASE_IR8(OPCE, 1, I8_I8_8)
                {
                    value* const bool_val = WO_VM_ADRS_R_S(p1_i8);
                    bool_val->m_integer =
                        bool_val->m_integer != 0
                        || WO_VM_ADRS_R_S(p2_i8)->m_integer != 0;
                }
                WO_VM_END_CASE();

                // CUMNEGI
                WO_VM_BEGIN_CASE_IR8(OPCE, 2, I8_16)
                {
                    value* const integer_val = WO_VM_ADRS_R_S(p1_i8);
                    integer_val->m_integer = -integer_val->m_integer;
                }
                WO_VM_END_CASE();

                // CUMNEGR
                WO_VM_BEGIN_CASE_IR8(OPCE, 3, I8_16)
                {
                    value* const real_val = WO_VM_ADRS_R_S(p1_i8);
                    real_val->m_real = -real_val->m_real;
                }
                WO_VM_END_CASE();

                // LAND
                WO_VM_BEGIN_CASE_IR8(OPLA, 0, I8_I8_I8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->set_bool(
                        WO_VM_ADRS_R_S(p2_i8)->m_integer
                        && WO_VM_ADRS_R_S(p3_i8)->m_integer);
                }
                WO_VM_END_CASE();

                // LOR
                WO_VM_BEGIN_CASE_IR8(OPLA, 1, I8_I8_I8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->set_bool(
                        WO_VM_ADRS_R_S(p2_i8)->m_integer
                        || WO_VM_ADRS_R_S(p3_i8)->m_integer);
                }
                WO_VM_END_CASE();

                // LNOT
                WO_VM_BEGIN_CASE_IR8(OPLA, 2, I8_I8_8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->set_bool(
                        !WO_VM_ADRS_R_S(p2_i8)->m_integer);
                }
                WO_VM_END_CASE();

                // CUMLNOT
                WO_VM_BEGIN_CASE_IR8(OPLA, 3, I8_16)
                {
                    value* const bool_val = WO_VM_ADRS_R_S(p1_i8);
                    bool_val->m_integer =
                        bool_val->m_integer == 0;
                }
                WO_VM_END_CASE();

                // IDSTR
                WO_VM_BEGIN_CASE_IR8(IDX, 0, I8_I8_I8)
                {
                    const string_t* const indexed_str =
                        WO_VM_ADRS_R_S(p2_i8)->m_string;

                    WO_VM_ADRS_R_S(p1_i8)->set_integer(
                        static_cast<wo_integer_t>(
                            wo_strn_get_char(
                                indexed_str->c_str(),
                                indexed_str->size(),
                                static_cast<size_t>(WO_VM_ADRS_R_S(p3_i8)->m_integer)))
                    );
                }
                WO_VM_END_CASE();

                // IDARR
                WO_VM_BEGIN_CASE_IR8(IDX, 1, I8_I8_I8)
                {
                    const array_t* const indexed_arr =
                        WO_VM_ADRS_R_S(p2_i8)->m_array;
                    gcbase::gc_read_guard rg(indexed_arr);

                    const size_t index = static_cast<size_t>(
                        WO_VM_ADRS_R_S(p3_i8)->m_integer);

                    if (index < indexed_arr->size())
                        WO_VM_ADRS_R_S(p1_i8)->set_val(
                            &indexed_arr->at(index));
                    else
                        WO_VM_FAIL(WO_FAIL_INDEX_FAIL, "Array index out of range.");
                }
                WO_VM_END_CASE();

                // IDDICT
                WO_VM_BEGIN_CASE_IR8(IDX, 2, I8_I8_I8)
                {
                    const dictionary_t* const indexed_dict =
                        WO_VM_ADRS_R_S(p2_i8)->m_dictionary;
                    gcbase::gc_read_guard rg(indexed_dict);

                    const value* const index_key = WO_VM_ADRS_R_S(p3_i8);
                    auto find_iter = indexed_dict->find(*index_key);
                    if (find_iter != indexed_dict->end())
                        WO_VM_ADRS_R_S(p1_i8)->set_val(&find_iter->second);
                    else
                        WO_VM_FAIL(WO_FAIL_INDEX_FAIL, "Dictionary key not found.");
                }
                WO_VM_END_CASE();

                // IDSTRUCT
                WO_VM_BEGIN_CASE_IR8(IDX, 3, I8_I8_U8)
                {
                    const structure_t* const indexed_struct =
                        WO_VM_ADRS_R_S(p2_i8)->m_structure;
                    gcbase::gc_read_guard rg(indexed_struct);

                    WO_VM_ADRS_R_S(p1_i8)->set_val(
                        &indexed_struct->m_values[p3_u8]);
                }
                WO_VM_END_CASE();

                // SIDARR
                WO_VM_BEGIN_CASE_IR8(SIDX, 0, I8_I8_I8)
                {
                    array_t* const indexed_arr =
                        WO_VM_ADRS_R_S(p2_i8)->m_array;
                    gcbase::gc_write_guard wg(indexed_arr);

                    const size_t index = static_cast<size_t>(
                        WO_VM_ADRS_R_S(p3_i8)->m_integer);
                    if (index < indexed_arr->size())
                    {
                        value* arr_val = &indexed_arr->at(index);
                        if (gc::gc_is_marking())
                            value::write_barrier(arr_val);
                        arr_val->set_val(WO_VM_ADRS_R_S(p1_i8));
                    }
                    else
                        WO_VM_FAIL(WO_FAIL_INDEX_FAIL, "Array index out of range.");
                }
                WO_VM_END_CASE();

                // SIDDICT
                WO_VM_BEGIN_CASE_IR8(SIDX, 1, I8_I8_I8)
                {
                    dictionary_t* const indexed_dict =
                        WO_VM_ADRS_R_S(p2_i8)->m_dictionary;
                    gcbase::gc_write_guard wg(indexed_dict);

                    const value* const index_key = WO_VM_ADRS_R_S(p3_i8);
                    auto fnd = indexed_dict->find(*index_key);
                    if (fnd != indexed_dict->end())
                    {
                        value* dict_val = &fnd->second;
                        if (gc::gc_is_marking())
                            value::write_barrier(dict_val);
                        dict_val->set_val(WO_VM_ADRS_R_S(p1_i8));
                    }
                    else
                        WO_VM_FAIL(WO_FAIL_INDEX_FAIL, "Dictionary key not found.");
                }
                WO_VM_END_CASE();

                // SIDMAP
                WO_VM_BEGIN_CASE_IR8(SIDX, 2, I8_I8_I8)
                {
                    dictionary_t* const indexed_dict =
                        WO_VM_ADRS_R_S(p2_i8)->m_dictionary;
                    gcbase::gc_modify_write_guard wg(indexed_dict);

                    const value* const index_key = WO_VM_ADRS_R_S(p3_i8);
                    value* dict_val = &(*indexed_dict)[*index_key];
                    if (gc::gc_is_marking())
                        value::write_barrier(dict_val);
                    dict_val->set_val(WO_VM_ADRS_R_S(p1_i8));
                }
                WO_VM_END_CASE();

                // SIDSTRUCT
                WO_VM_BEGIN_CASE_IR8(SIDX, 3, I8_I8_U8)
                {
                    structure_t* const indexed_struct =
                        WO_VM_ADRS_R_S(p2_i8)->m_structure;
                    gcbase::gc_write_guard wg(indexed_struct);
                    value* struct_val = &indexed_struct->m_values[p3_u8];
                    if (gc::gc_is_marking())
                        value::write_barrier(struct_val);
                    struct_val->set_val(WO_VM_ADRS_R_S(p1_i8));
                }
                WO_VM_END_CASE();

                // IDSTEXT
                WO_VM_BEGIN_CASE_IR8(IDSTEXT, 0, I8_I8_8_EU32)
                {
                    const structure_t* const indexed_struct =
                        WO_VM_ADRS_R_S(p2_i8)->m_structure;
                    gcbase::gc_read_guard rg(indexed_struct);

                    WO_VM_ADRS_R_S(p1_i8)->set_val(
                        &indexed_struct->m_values[p3_u32]);
                }
                WO_VM_END_CASE();

                // SIDSTEXT
                WO_VM_BEGIN_CASE_IR8(SIDSTEXT, 0, I8_I8_8_EU32)
                {
                    structure_t* const indexed_struct =
                        WO_VM_ADRS_R_S(p2_i8)->m_structure;
                    gcbase::gc_write_guard wg(indexed_struct);

                    value* struct_val = &indexed_struct->m_values[p3_u32];
                    if (gc::gc_is_marking())
                        value::write_barrier(struct_val);
                    struct_val->set_val(WO_VM_ADRS_R_S(p1_i8));
                }
                WO_VM_END_CASE();

                // JMP
                WO_VM_BEGIN_CASE_IR8(JMP, 0, U24)
                {
                    rt_ip = WO_VM_FP_SHIFT(near_rtcode_begin, p1_u24);

                    // Break out, donot `WO_VM_END_CASE`
                    break;
                }
                WO_VM_END_CASE();

                // JMPF
                WO_VM_BEGIN_CASE_IR8(JMP, 2, U24)
                {
                    if (!rt_cr->m_integer)
                    {
                        rt_ip = WO_VM_FP_SHIFT(near_rtcode_begin, p1_u24);

                        // Break out, donot `WO_VM_END_CASE`
                        break;
                    }
                }
                WO_VM_END_CASE();

                // JMPT
                WO_VM_BEGIN_CASE_IR8(JMP, 3, U24)
                {
                    if (rt_cr->m_integer)
                    {
                        rt_ip = WO_VM_FP_SHIFT(near_rtcode_begin, p1_u24);

                        // Break out, donot `WO_VM_END_CASE`
                        break;
                    }
                }
                WO_VM_END_CASE();

                // JMPGC
                WO_VM_BEGIN_CASE_IR8(JMPGC, 0, U24)
                {
                    rt_ip = WO_VM_FP_SHIFT(near_rtcode_begin, p1_u24);

                    WO_VM_INTERRUPT_CHECKPOINT;

                    // Break out, donot `WO_VM_END_CASE`
                    break;
                }
                WO_VM_END_CASE();

                // JMPFGC
                WO_VM_BEGIN_CASE_IR8(JMPGC, 2, U24)
                {
                    if (!rt_cr->m_integer)
                    {
                        rt_ip = WO_VM_FP_SHIFT(near_rtcode_begin, p1_u24);

                        WO_VM_INTERRUPT_CHECKPOINT;

                        // Break out, donot `WO_VM_END_CASE`
                        break;
                    }
                }
                WO_VM_END_CASE();

                // JMPTGC
                WO_VM_BEGIN_CASE_IR8(JMPGC, 3, U24)
                {
                    if (rt_cr->m_integer)
                    {
                        rt_ip = WO_VM_FP_SHIFT(near_rtcode_begin, p1_u24);

                        WO_VM_INTERRUPT_CHECKPOINT;

                        // Break out, donot `WO_VM_END_CASE`
                        break;
                    }
                }
                WO_VM_END_CASE();

                // RET
                WO_VM_BEGIN_CASE_IR8(RET, 0, 24)
                {
                    switch ((++bp)->m_type)
                    {
                    case value::valuetype::native_callstack:
                        sp = bp;
                        // last stack is native_func, just do return;
                        // stack balance should be keeped by invoker.
                        WO_VM_RETURN(wo_result_t::WO_API_NORMAL);
                    case value::valuetype::callstack:
                    {
                        value* stored_bp = sb - bp->m_vmcallstack.bp;
                        wo_assert(stored_bp <= sb && stored_bp > stack_storage);

                        rt_ip = WO_VM_FP_SHIFT(near_rtcode_begin, bp->m_vmcallstack.ret_ip);
                        sp = bp;
                        bp = stored_bp;
                        break;
                    }
                    case value::valuetype::far_callstack:
                    {
                        value* stored_bp = sb - bp->m_ext_farcallstack_bp;
                        wo_assert(stored_bp <= sb && stored_bp > stack_storage);

                        rt_ip = bp->m_farcallstack;
                        sp = bp;
                        bp = stored_bp;

                        if (rt_ip < near_rtcode_begin || rt_ip >= near_rtcode_end)
                        {
                            wo_assure(interrupt(vm_interrupt_type::CALL_FAR_RESYNC_VM_STATE_INTERRUPT));
                            WO_VM_INTERRUPT_CHECKPOINT;
                        }
                        break;
                    }
                    default:
#if WO_ENABLE_RUNTIME_CHECK
                        WO_VM_FAIL(WO_FAIL_TYPE_FAIL, "Broken stack in 'retn'.");
#endif
                        break;
                    }

                    // Break out, donot `WO_VM_END_CASE`
                    break;
                }
                WO_VM_END_CASE();

                // RETN 8
                WO_VM_BEGIN_CASE_IR8(RET, 1, U8_16)
                {
                    switch ((++bp)->m_type)
                    {
                    case value::valuetype::native_callstack:
                        sp = bp;
                        sp += p1_u8;
                        // last stack is native_func, just do return;
                        // stack balance should be keeped by invoker.
                        WO_VM_RETURN(wo_result_t::WO_API_NORMAL);
                    case value::valuetype::callstack:
                    {
                        value* stored_bp = sb - bp->m_vmcallstack.bp;
                        wo_assert(stored_bp <= sb && stored_bp > stack_storage);

                        rt_ip = WO_VM_FP_SHIFT(near_rtcode_begin, bp->m_vmcallstack.ret_ip);
                        sp = bp;
                        bp = stored_bp;

                        break;
                    }
                    case value::valuetype::far_callstack:
                    {
                        value* stored_bp = sb - bp->m_ext_farcallstack_bp;
                        wo_assert(stored_bp <= sb && stored_bp > stack_storage);

                        rt_ip = bp->m_farcallstack;
                        sp = bp;
                        bp = stored_bp;

                        if (rt_ip < near_rtcode_begin || rt_ip >= near_rtcode_end)
                        {
                            wo_assure(interrupt(vm_interrupt_type::CALL_FAR_RESYNC_VM_STATE_INTERRUPT));
                            WO_VM_INTERRUPT_CHECKPOINT;
                        }
                        break;
                    }
                    default:
#if WO_ENABLE_RUNTIME_CHECK
                        WO_VM_FAIL(WO_FAIL_TYPE_FAIL, "Broken stack in 'retn'.");
#endif
                        break;
                    }
                    sp += p1_u8;

                    // Break out, donot `WO_VM_END_CASE`
                    break;
                }
                WO_VM_END_CASE();

                // RETN 16
                WO_VM_BEGIN_CASE_IR8(RET, 2, 8_U16)
                {
                    switch ((++bp)->m_type)
                    {
                    case value::valuetype::native_callstack:
                        sp = bp;
                        sp += p1_u16;
                        // last stack is native_func, just do return;
                        // stack balance should be keeped by invoker.
                        WO_VM_RETURN(wo_result_t::WO_API_NORMAL);
                    case value::valuetype::callstack:
                    {
                        value* stored_bp = sb - bp->m_vmcallstack.bp;
                        wo_assert(stored_bp <= sb && stored_bp > stack_storage);

                        rt_ip = WO_VM_FP_SHIFT(near_rtcode_begin, bp->m_vmcallstack.ret_ip);
                        sp = bp;
                        bp = stored_bp;

                        break;
                    }
                    case value::valuetype::far_callstack:
                    {
                        value* stored_bp = sb - bp->m_ext_farcallstack_bp;
                        wo_assert(stored_bp <= sb && stored_bp > stack_storage);

                        rt_ip = bp->m_farcallstack;
                        sp = bp;
                        bp = stored_bp;

                        if (rt_ip < near_rtcode_begin || rt_ip >= near_rtcode_end)
                        {
                            wo_assure(interrupt(vm_interrupt_type::CALL_FAR_RESYNC_VM_STATE_INTERRUPT));
                            WO_VM_INTERRUPT_CHECKPOINT;
                        }
                        break;
                    }
                    default:
#if WO_ENABLE_RUNTIME_CHECK
                        WO_VM_FAIL(WO_FAIL_TYPE_FAIL, "Broken stack in 'retn'.");
#endif
                        break;
                    }
                    sp += p1_u16;

                    // Break out, donot `WO_VM_END_CASE`
                    break;
                }
                WO_VM_END_CASE();

                // CALLNWO
                WO_VM_BEGIN_CASE_IR8(CALLN, 0, U24_E32)
                {
                    if (sp > stack_storage)
                    {
                        sp->m_type = value::valuetype::callstack;
                        sp->m_vmcallstack.bp = static_cast<uint32_t>(sb - bp);
                        sp->m_vmcallstack.ret_ip =
                            WO_VM_FP_OFFSET(near_rtcode_begin, rt_ip) + z__wo_vm_next_ir_offset;

                        bp = --sp;
                        rt_ip = WO_VM_FP_SHIFT(near_rtcode_begin, p1_u24);
                    }
                    else
                        wo_assure(interrupt(vm_interrupt_type::STACK_OVERFLOW_INTERRUPT));

                    WO_VM_INTERRUPT_CHECKPOINT;

                    // Break out, donot `WO_VM_END_CASE`
                    break;
                }
                WO_VM_END_CASE();

                // CALLNJIT
                WO_VM_BEGIN_CASE_IR8(CALLN, 1, EU56)
                {
                    if (sp > stack_storage)
                    {
                        sp->m_type = value::valuetype::callstack;
                        sp->m_vmcallstack.bp = static_cast<uint32_t>(sb - bp);
                        sp->m_vmcallstack.ret_ip =
                            WO_VM_FP_OFFSET(near_rtcode_begin, rt_ip) + z__wo_vm_next_ir_offset;

                        // Donot store or update ip/sp/bp. jit function will not use them.
                        auto* const rt_sp = sp;

                        switch (reinterpret_cast<wo_extern_native_func_t>(
                            static_cast<intptr_t>(p1_u56))(
                                reinterpret_cast<wo_vm>(this),
                                std::launder(reinterpret_cast<wo_value>(sp + 1))))
                        {
                        case wo_result_t::WO_API_NORMAL:
                        {
                            WO_VM_ASSERT(rt_sp > stack_storage && rt_sp <= sb,
                                "Unexpected stack changed in 'callnjit'.");
                            sp = rt_sp;

                            WO_VM_ASSERT(sp->m_type == value::valuetype::callstack,
                                "Found broken stack in 'callnjit'.");
                            bp = sb - sp->m_vmcallstack.bp;
                            break;
                        }
                        case wo_result_t::WO_API_SYNC_CHANGED_VM_STATE:
                            rt_ip = ip;
                            WO_VM_INTERRUPT_CHECKPOINT;
                            break;
                        default:
#if WO_ENABLE_RUNTIME_CHECK
                            WO_VM_FAIL(WO_FAIL_TYPE_FAIL, "Bad native function sync state.");
#endif
                            break;
                        }
                    }
                    else
                    {
                        wo_assure(interrupt(vm_interrupt_type::STACK_OVERFLOW_INTERRUPT));
                        WO_VM_INTERRUPT_CHECKPOINT;

                        // Break out, donot `WO_VM_END_CASE`
                        break;
                    }
                }
                WO_VM_END_CASE();

                // CALLNFP
                WO_VM_BEGIN_CASE_IR8(CALLN, 2, EU56)
                {
                    if (sp > stack_storage)
                    {
                        // For debugging purposes, native calls use far callstack to preserve 
                        // the complete return address.
                        // Since native calls do not use the `ret` instruction to return, there 
                        // is no performance penalty

                        sp->m_type = value::valuetype::far_callstack;
                        sp->m_ext_farcallstack_bp = static_cast<uint32_t>(sb - bp);
                        sp->m_farcallstack = rt_ip;

                        bp = --sp;

                        const auto rt_bp = sb - bp;
                        auto* const target_fp =
                            reinterpret_cast<wo_extern_native_func_t>(static_cast<intptr_t>(p1_u56));
                        ip = reinterpret_cast<irv2::ir*>(target_fp);

                        switch (target_fp(
                            reinterpret_cast<wo_vm>(this),
                            std::launder(reinterpret_cast<wo_value>(sp + 1))))
                        {
                        case wo_result_t::WO_API_RESYNC_JIT_STATE_TO_VM_STATE:
                            WO_VM_INTERRUPT_CHECKPOINT;
                            /* FALLTHROUGH */
                            [[fallthrough]];
                        case wo_result_t::WO_API_NORMAL:
                        {
                            bp = sb - rt_bp;

                            WO_VM_ASSERT((bp + 1)->m_type == value::valuetype::far_callstack,
                                "Found broken stack in 'callnfp'.");
                            value* stored_bp = sb - (++bp)->m_ext_farcallstack_bp;
                            sp = bp;
                            bp = stored_bp;
                            break;
                        }
                        default:
#if WO_ENABLE_RUNTIME_CHECK
                            WO_VM_FAIL(WO_FAIL_TYPE_FAIL, "Bad native function sync state.");
#endif
                            break;
                        }
                    }
                    else
                    {
                        wo_assure(interrupt(vm_interrupt_type::STACK_OVERFLOW_INTERRUPT));
                        WO_VM_INTERRUPT_CHECKPOINT;

                        // Break out, donot `WO_VM_END_CASE`
                        break;
                    }
                }
                WO_VM_END_CASE();

                // CALL
                WO_VM_BEGIN_CASE_IR8(CALL, 0, I8_16)
                {
                    value* const call_target = WO_VM_ADRS_R_S(p1_i8);

                    if (call_target->m_type == value::valuetype::closure_type
                        ? sp - call_target->m_closure->m_closure_args_count > stack_storage
                        : sp > stack_storage)
                    {
                        switch (call_target->m_type)
                        {
                        case value::valuetype::closure_type:
                        {
                            closure_t* const closure = call_target->m_closure;

                            // Unpack captured variables to stack
                            for (uint16_t idx = 0; idx < closure->m_closure_args_count; ++idx)
                                (sp--)->set_val(closure->m_closure_args + idx);

                            if (closure->m_native_call)
                            {
                                // Closure's native function must be callnjit.
                                sp->m_type = value::valuetype::callstack;
                                sp->m_vmcallstack.bp = static_cast<uint32_t>(sb - bp);
                                sp->m_vmcallstack.ret_ip =
                                    WO_VM_FP_OFFSET(near_rtcode_begin, rt_ip) + z__wo_vm_next_ir_offset;

                                // Donot store or update ip/sp/bp. jit function will not use them.
                                auto* const rt_sp = sp;

                                switch (closure->m_native_func(
                                    reinterpret_cast<wo_vm>(this),
                                    std::launder(reinterpret_cast<wo_value>(sp + 1))))
                                {
                                case wo_result_t::WO_API_NORMAL:
                                {
                                    WO_VM_ASSERT(rt_sp > stack_storage && rt_sp <= sb,
                                        "Unexpected stack changed in 'call'.");
                                    sp = rt_sp;

                                    WO_VM_ASSERT(sp->m_type == value::valuetype::callstack,
                                        "Found broken stack in 'call'.");
                                    bp = sb - sp->m_vmcallstack.bp;
                                    break;
                                }
                                case wo_result_t::WO_API_SYNC_CHANGED_VM_STATE:
                                    rt_ip = ip;
                                    WO_VM_INTERRUPT_CHECKPOINT;
                                    break;
                                default:
#if WO_ENABLE_RUNTIME_CHECK
                                    WO_VM_FAIL(WO_FAIL_TYPE_FAIL, "Bad native function sync state.");
#endif
                                    break;
                                }
                            }
                            else
                            {
                                const auto* aim_function_addr = closure->m_vm_func;
                                if (aim_function_addr < near_rtcode_begin || aim_function_addr >= near_rtcode_end)
                                {
                                    sp->m_type = value::valuetype::far_callstack;
                                    sp->m_farcallstack = rt_ip;
                                    sp->m_ext_farcallstack_bp = (uint32_t)(sb - bp);
                                    bp = --sp;
                                    wo_assure(interrupt(vm_interrupt_type::CALL_FAR_RESYNC_VM_STATE_INTERRUPT));
                                }
                                else
                                {
                                    sp->m_type = value::valuetype::callstack;
                                    sp->m_vmcallstack.ret_ip =
                                        WO_VM_FP_OFFSET(near_rtcode_begin, rt_ip);
                                    sp->m_vmcallstack.bp = (uint32_t)(sb - bp);
                                    bp = --sp;
                                }
                                rt_ip = aim_function_addr;

                                goto _label_vm_break_and_continue;
                            }

                            break;
                        }
                        case value::valuetype::native_func_type:
                        {
                            auto* const target_fp = call_target->m_native_func;

                            /* Might be far call jit code. */
                            sp->m_type = value::valuetype::far_callstack;
                            sp->m_farcallstack = rt_ip;
                            sp->m_ext_farcallstack_bp = (uint32_t)(sb - bp);
                            bp = --sp;
                            auto rt_bp = sb - bp;

                            // Call native
                            ip = reinterpret_cast<const irv2::ir*>(target_fp);

                            switch (target_fp(
                                reinterpret_cast<wo_vm>(this),
                                std::launder(reinterpret_cast<wo_value>(sp + 2))))
                            {
                            case wo_result_t::WO_API_RESYNC_JIT_STATE_TO_VM_STATE:
                                WO_VM_INTERRUPT_CHECKPOINT;
                                /* FALLTHROUGH */
                                [[fallthrough]];
                            case wo_result_t::WO_API_NORMAL:
                            {
                                bp = sb - rt_bp;

                                WO_VM_ASSERT((bp + 1)->m_type == value::valuetype::far_callstack,
                                    "Found broken stack in 'call'.");
                                value* stored_bp = sb - (++bp)->m_ext_farcallstack_bp;
                                sp = bp;
                                bp = stored_bp;
                                break;
                            }
                            case wo_result_t::WO_API_SYNC_CHANGED_VM_STATE:
                                rt_ip = ip;
                                if (rt_ip < near_rtcode_begin || rt_ip >= near_rtcode_end)
                                    wo_assure(interrupt(
                                        vm_interrupt_type::CALL_FAR_RESYNC_VM_STATE_INTERRUPT));
                                WO_VM_INTERRUPT_CHECKPOINT;
                                break;
                            default:
#if WO_ENABLE_RUNTIME_CHECK
                                WO_VM_FAIL(WO_FAIL_TYPE_FAIL, "Bad native function sync state.");
#endif
                                break;
                            }
                            break;
                        }
                        case value::valuetype::script_func_type:
                        {
                            rt_ip = call_target->m_script_func;

                            if (rt_ip < near_rtcode_begin || rt_ip >= near_rtcode_end)
                            {
                                sp->m_type = value::valuetype::far_callstack;
                                sp->m_farcallstack = rt_ip;
                                sp->m_ext_farcallstack_bp = (uint32_t)(sb - bp);
                                bp = --sp;
                                wo_assure(interrupt(vm_interrupt_type::CALL_FAR_RESYNC_VM_STATE_INTERRUPT));
                            }
                            else
                            {
                                sp->m_type = value::valuetype::callstack;
                                sp->m_vmcallstack.ret_ip = WO_VM_FP_OFFSET(near_rtcode_begin, rt_ip);
                                sp->m_vmcallstack.bp = (uint32_t)(sb - bp);
                                bp = --sp;
                            }
                            goto _label_vm_break_and_continue;
                        }
                        default:
                            WO_VM_FAIL(WO_FAIL_CALL_FAIL, "Unexpected invoke target type in 'call'.");
                            break;
                        }
                    }
                    else
                    {
                        wo_assure(interrupt(vm_interrupt_type::STACK_OVERFLOW_INTERRUPT));

                    _label_vm_break_and_continue:
                        WO_VM_INTERRUPT_CHECKPOINT;

                        // Break out, donot `WO_VM_END_CASE`
                        break;
                    }
                }
                WO_VM_END_CASE();

                // MKARR
                WO_VM_BEGIN_CASE_IR8(CONS, 0, I8_U16)
                {
                    sp = make_array_impl(
                        WO_VM_ADRS_R_S(p1_i8),
                        p2_u16,
                        sp);
                }
                WO_VM_END_CASE();

                // MKMAP
                WO_VM_BEGIN_CASE_IR8(CONS, 1, I8_U16)
                {
                    sp = make_map_impl(
                        WO_VM_ADRS_R_S(p1_i8),
                        p2_u16,
                        sp);
                }
                WO_VM_END_CASE();

                // MKSTRUCT
                WO_VM_BEGIN_CASE_IR8(CONS, 2, I8_U16)
                {
                    sp = make_struct_impl(
                        WO_VM_ADRS_R_S(p1_i8),
                        p2_u16,
                        sp);
                }
                WO_VM_END_CASE();

                // MKARREXT
                WO_VM_BEGIN_CASE_IR8(CONSEXT, 0, I8_16_EU32)
                {
                    sp = make_struct_impl(
                        WO_VM_ADRS_R_S(p1_i8),
                        p2_u32,
                        sp);
                }
                WO_VM_END_CASE();

                // MKMAPEXT
                WO_VM_BEGIN_CASE_IR8(CONSEXT, 1, I8_16_EU32)
                {
                    sp = make_map_impl(
                        WO_VM_ADRS_R_S(p1_i8),
                        p2_u32,
                        sp);
                }
                WO_VM_END_CASE();

                // MKSTRUCTEXT
                WO_VM_BEGIN_CASE_IR8(CONSEXT, 2, I8_16_EU32)
                {
                    sp = make_struct_impl(
                        WO_VM_ADRS_R_S(p1_i8),
                        p2_u32,
                        sp);
                }
                WO_VM_END_CASE();

                // MKCLOSWO
                WO_VM_BEGIN_CASE_IR8(MKCLOS, 0, I8_U16_EU32_E32)
                {
                    sp = make_closure_wo_impl(
                        WO_VM_ADRS_R_S(p1_i8),
                        p2_u16,
                        near_rtcode_begin + p3_u32,
                        sp);
                }
                WO_VM_END_CASE();

                // MKCLOSJIT
                WO_VM_BEGIN_CASE_IR8(MKCLOS, 1, I8_U16_EU64)
                {
                    sp = make_closure_fp_impl(
                        WO_VM_ADRS_R_S(p1_i8),
                        p2_u16,
                        reinterpret_cast<wo_native_func_t>(
                            static_cast<intptr_t>(p3_u64)),
                        sp);
                }
                WO_VM_END_CASE();

                // UNPACKSN
                WO_VM_BEGIN_CASE_IR8(UNPACK, 0, I8_U16)
                {
                    const structure_t* const unpack_struct =
                        WO_VM_ADRS_R_S(p1_i8)->m_structure;
                    gcbase::gc_read_guard rg(unpack_struct);

                    if (unpack_struct->m_count >= p2_u16)
                    {
                        auto* const new_sp = sp - p2_u16;
                        if (new_sp >= stack_storage)
                        {
                            sp = new_sp;
                            for (uint16_t idx = 0; idx < p2_u16; ++idx)
                            {
                                new_sp[idx + 1].set_val(
                                    &unpack_struct->m_values[idx]);
                            }
                        }
                        else
                        {
                            wo_assure(interrupt(vm_interrupt_type::STACK_OVERFLOW_INTERRUPT));
                            WO_VM_INTERRUPT_CHECKPOINT;

                            // Break out, donot `WO_VM_END_CASE`
                            break;
                        }
                    }
                    else
                    {
                        WO_VM_FAIL(WO_FAIL_INDEX_FAIL,
                            "Structure unpack count exceed.");
                    }
                }
                WO_VM_END_CASE();

                // UNPACKSC
                WO_VM_BEGIN_CASE_IR8(UNPACK, 1, I8_U16)
                {
                    const structure_t* const unpack_struct =
                        WO_VM_ADRS_R_S(p1_i8)->m_structure;
                    gcbase::gc_read_guard rg(unpack_struct);

                    if (unpack_struct->m_count >= p2_u16)
                    {
                        auto* const new_sp = sp - unpack_struct->m_count;
                        if (new_sp >= stack_storage)
                        {
                            sp = new_sp;
                            for (uint32_t idx = 0; idx < unpack_struct->m_count; ++idx)
                            {
                                new_sp[idx + 1].set_val(
                                    &unpack_struct->m_values[idx]);
                            }
                        }
                        else
                        {
                            wo_assure(interrupt(vm_interrupt_type::STACK_OVERFLOW_INTERRUPT));
                            WO_VM_INTERRUPT_CHECKPOINT;

                            // Break out, donot `WO_VM_END_CASE`
                            break;
                        }
                    }
                    else
                    {
                        WO_VM_FAIL(WO_FAIL_INDEX_FAIL,
                            "Structure unpack count exceed.");
                    }
                }
                WO_VM_END_CASE();

                // UNPACKAN
                WO_VM_BEGIN_CASE_IR8(UNPACK, 2, I8_U16)
                {
                    const array_t* const unpack_array =
                        WO_VM_ADRS_R_S(p1_i8)->m_array;
                    gcbase::gc_read_guard rg(unpack_array);

                    if (unpack_array->size() >= p2_u16)
                    {
                        auto* const new_sp = sp - p2_u16;
                        if (new_sp >= stack_storage)
                        {
                            sp = new_sp;

                            auto* data = unpack_array->data();
                            for (uint16_t idx = 0; idx < p2_u16; ++idx)
                            {
                                new_sp[idx + 1].set_val(&data[idx]);
                            }
                        }
                        else
                        {
                            wo_assure(interrupt(vm_interrupt_type::STACK_OVERFLOW_INTERRUPT));
                            WO_VM_INTERRUPT_CHECKPOINT;

                            // Break out, donot `WO_VM_END_CASE`
                            break;
                        }
                    }
                    else
                    {
                        WO_VM_FAIL(WO_FAIL_INDEX_FAIL,
                            "Structure unpack count exceed.");
                    }
                }
                WO_VM_END_CASE();

                // UNPACKAC
                WO_VM_BEGIN_CASE_IR8(UNPACK, 3, I8_U16)
                {
                    const array_t* const unpack_array =
                        WO_VM_ADRS_R_S(p1_i8)->m_array;
                    gcbase::gc_read_guard rg(unpack_array);

                    const auto array_length = unpack_array->size();
                    if (array_length >= p2_u16)
                    {
                        auto* const new_sp = sp - array_length;
                        if (new_sp >= stack_storage)
                        {
                            sp = new_sp;
                            auto* data = unpack_array->data();
                            for (size_t idx = 0; idx < array_length; ++idx)
                            {
                                new_sp[idx + 1].set_val(&data[idx]);
                            }
                        }
                        else
                        {
                            wo_assure(interrupt(vm_interrupt_type::STACK_OVERFLOW_INTERRUPT));
                            WO_VM_INTERRUPT_CHECKPOINT;

                            // Break out, donot `WO_VM_END_CASE`
                            break;
                        }
                    }
                    else
                    {
                        WO_VM_FAIL(WO_FAIL_INDEX_FAIL,
                            "Structure unpack count exceed.");
                    }
                }
                WO_VM_END_CASE();

                // PACK
                WO_VM_BEGIN_CASE_IR8(PACK, 0, I8_U8_U8)
                {
                    packargs_impl(
                        WO_VM_ADRS_R_S(p1_i8),
                        bp,
                        tp->m_integer,
                        static_cast<uint16_t>(p2_u8),
                        static_cast<uint16_t>(p3_u8));
                }
                WO_VM_END_CASE();

                // PACKEXT
                WO_VM_BEGIN_CASE_IR8(PACK, 1, I8_U16_EU32)
                {
                    packargs_impl(
                        WO_VM_ADRS_R_S(p1_i8),
                        bp,
                        tp->m_integer,
                        p2_u16,
                        static_cast<uint16_t>(p3_u32));
                }
                WO_VM_END_CASE();

                // LDSIMM
                WO_VM_BEGIN_CASE_IR8(LDS, 0, I8_I16)
                {
                    WO_VM_ADRS_R_S(p1_i8)->set_val(
                        WO_VM_ADRS_S(p2_i16));
                }
                WO_VM_END_CASE();

                // LDSIMMEXT
                WO_VM_BEGIN_CASE_IR8(LDS, 1, I8_16_EI32)
                {
                    WO_VM_ADRS_R_S(p1_i8)->set_val(
                        WO_VM_ADRS_S(p2_i32));
                }
                WO_VM_END_CASE();

                // LDSIDX
                WO_VM_BEGIN_CASE_IR8(LDS, 2, I8_I8_8)
                {
                    WO_VM_ADRS_R_S(p1_i8)->set_val(
                        WO_VM_ADRS_S(
                            WO_VM_ADRS_R_S(p2_i8)->m_integer));
                }
                WO_VM_END_CASE();

                // LDSIDXEXT
                WO_VM_BEGIN_CASE_IR8(LDS, 3, I8_16_EI32)
                {
                    WO_VM_ADRS_R_S(p1_i8)->set_val(
                        WO_VM_ADRS_S(
                            WO_VM_ADRS_R_S(p2_i32)->m_integer));
                }
                WO_VM_END_CASE();

                // STSIMM
                WO_VM_BEGIN_CASE_IR8(STS, 0, I8_I16)
                {
                    WO_VM_ADRS_S(p2_i16)->set_val(
                        WO_VM_ADRS_R_S(p1_i8));
                }
                WO_VM_END_CASE();

                // STSIMMEXT
                WO_VM_BEGIN_CASE_IR8(STS, 1, I8_16_EI32)
                {
                    WO_VM_ADRS_S(p2_i32)->set_val(
                        WO_VM_ADRS_R_S(p1_i8));
                }
                WO_VM_END_CASE();

                // STSIDX
                WO_VM_BEGIN_CASE_IR8(STS, 2, I8_I8_8)
                {
                    WO_VM_ADRS_S(
                        WO_VM_ADRS_R_S(p2_i8)->m_integer)->set_val(
                            WO_VM_ADRS_R_S(p1_i8));
                }
                WO_VM_END_CASE();

                // STSIDXEXT
                WO_VM_BEGIN_CASE_IR8(STS, 3, I8_16_EI32)
                {
                    WO_VM_ADRS_S(
                        WO_VM_ADRS_R_S(p2_i32)->m_integer)->set_val(
                            WO_VM_ADRS_R_S(p1_i8));
                }
                WO_VM_END_CASE();

                // PANIC
                WO_VM_BEGIN_CASE_IR8(PANIC, 0, I8_16)
                {
                    WO_VM_FAIL(WO_FAIL_UNEXPECTED,
                        "%s",
                        wo_cast_string(
                            reinterpret_cast<wo_value>(
                                WO_VM_ADRS_R_S(p1_i8))));
                }
                WO_VM_END_CASE();

                // PANIC C/G 24
                WO_VM_BEGIN_CASE_IR8(PANIC, 2, I24)
                {
                    WO_VM_FAIL(WO_FAIL_UNEXPECTED,
                        "%s",
                        wo_cast_string(
                            reinterpret_cast<wo_value>(
                                WO_VM_ADRS_C_G(p1_i24))));
                }
                WO_VM_END_CASE();

                // PANIC C/G 32
                WO_VM_BEGIN_CASE_IR8(PANIC, 3, 24_EI32)
                {
                    WO_VM_FAIL(WO_FAIL_UNEXPECTED,
                        "%s",
                        wo_cast_string(
                            reinterpret_cast<wo_value>(
                                WO_VM_ADRS_C_G(p1_i32))));
                }
                WO_VM_END_CASE();


            default:
            {
                static_assert(std::is_same_v<decltype(rtopcode), uint32_t>);
                if ((0xFFFFFF00u & rtopcode) == 0)
                    wo_error("Unknown instruct.");

                const auto interrupt_state = vm_interrupt.load(std::memory_order_acquire);

                if (interrupt_state & vm_interrupt_type::GC_INTERRUPT)
                {
                    gc_checkpoint_self_mark();
                }
                ///////////////////////////////////////////////////////////////////////
                if (interrupt_state & vm_interrupt_type::GC_HANGUP_INTERRUPT)
                {
                    if (clear_interrupt(vm_interrupt_type::GC_HANGUP_INTERRUPT))
                        hangup();
                }
                else if (interrupt_state & vm_interrupt_type::ABORT_INTERRUPT)
                {
                    // ABORTED VM WILL NOT ABLE TO RUN AGAIN, SO DO NOT
                    // CLEAR ABORT_INTERRUPT
                    WO_VM_RETURN(wo_result_t::WO_API_SIM_ABORT);
                }
                else if (interrupt_state & vm_interrupt_type::BR_YIELD_INTERRUPT)
                {
                    wo_assure(clear_interrupt(vm_interrupt_type::BR_YIELD_INTERRUPT));
                    WO_VM_RETURN(wo_result_t::WO_API_SIM_YIELD);
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
                else if (interrupt_state & vm_interrupt_type::STACK_OCCUPYING_INTERRUPT)
                {
                    while (check_interrupt(vm_interrupt_type::STACK_OCCUPYING_INTERRUPT))
                        wo::gcbase::_shared_spin::spin_loop_hint();
                }
                else if (interrupt_state & vm_interrupt_type::STACK_OVERFLOW_INTERRUPT)
                {
                    shrink_stack_edge = std::min(VM_SHRINK_STACK_MAX_EDGE, (uint8_t)(shrink_stack_edge + 1));
                    // Force realloc stack buffer.
                    bool r = _reallocate_stack_space(stack_size << 1);

                    wo_assure(clear_interrupt(vm_interrupt_type::STACK_OVERFLOW_INTERRUPT));

                    if (!r)
                        WO_VM_FAIL(WO_FAIL_STACKOVERFLOW, "Stack overflow.");
                }
                else if (interrupt_state & vm_interrupt_type::SHRINK_STACK_INTERRUPT)
                {
                    if (_reallocate_stack_space(stack_size >> 1))
                        shrink_stack_edge = VM_SHRINK_STACK_COUNT;

                    wo_assure(clear_interrupt(vm_interrupt_type::SHRINK_STACK_INTERRUPT));
                }
                else if (interrupt_state & vm_interrupt_type::CALL_FAR_RESYNC_VM_STATE_INTERRUPT)
                {
                    if (!runtime_env::resync_far_state(
                        rt_ip,
                        &near_rtcode_begin,
                        &near_rtcode_end,
                        &near_static_global))
                        WO_VM_FAIL(WO_FAIL_CALL_FAIL, "Unkown function.");

                    wo_assure(clear_interrupt(vm_interrupt_type::CALL_FAR_RESYNC_VM_STATE_INTERRUPT));
                }
                // ATTENTION: it should be last interrupt..
                else if (interrupt_state & vm_interrupt_type::DEBUG_INTERRUPT)
                {
                    // static_assert(sizeof(instruct::opcode) == 1);

                    ip = rt_ip;

                    auto debug_bridge = vm_debuggee_bridge_base::current_global_debuggee_bridge();
                    if (debug_bridge.has_value())
                    {
                        // check debuggee here
                        wo_assure(wo_leave_gcguard(reinterpret_cast<wo_vm>(this)));
                        debug_bridge.value()->_vm_invoke_debuggee(this);
                        wo_assure(wo_enter_gcguard(reinterpret_cast<wo_vm>(this)));
                    }

                    // Refetch ip from vm, it may modified by debugger.
                    rt_ip = ip;
                    rtopcode = WO_IR_OPBYTE(rt_ip);

                    goto re_entry_for_interrupt;
                }
                else
                {
                    // a vm_interrupt is invalid now, just roll back one byte and continue~
                    // so here do nothing
                    wo_assert(interrupt_state == 0
                        || interrupt_state == vm_interrupt_type::GC_INTERRUPT);
                }

                WO_VM_INTERRUPT_CHECKPOINT;
            }
            }
            }// vm loop end.

            WO_VM_RETURN(wo_result_t::WO_API_NORMAL);

#undef WO_VM_FAIL
#undef WO_VM_ASSERT
#undef WO_VM_INTERRUPT_CHECKPOINT
#undef WO_RSG_ADDRESSING_WRITE_OP1_CASE
#undef WO_WRITE_CHECK_FOR_GLOBAL
#undef WO_RSG_ADDRESSING_CASE
        }

#undef WO_VM_RETURN
#undef WO_VM_FAIL
    }
