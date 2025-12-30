#include "wo_afx.hpp"

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

        cr = &register_storage[WO_REG_CR];
        tc = &register_storage[WO_REG_TC];
        tp = &register_storage[WO_REG_TP];
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

        runtime_codes_begin = ip = env->rt_codes;
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
    class bytecode_disassembler
    {
    public:
        bytecode_disassembler(
            const byte_t* code_ptr,
            const runtime_env* env) noexcept
            : m_code_ptr(code_ptr)
            , m_env(env)
            , m_static_storage(env->constant_and_global_storage)
            , m_main_command(0)
        {
        }

        // Read main command byte
        byte_t read_command() noexcept
        {
            m_main_command = *m_code_ptr++;
            return m_main_command;
        }

        // Get current code pointer
        const byte_t* current() const noexcept { return m_code_ptr; }

        // Get main command
        byte_t command() const noexcept { return m_main_command; }

        // Get opcode (upper 6 bits)
        byte_t opcode() const noexcept { return m_main_command & 0b11111100; }

        // Get DR bits (lower 2 bits)
        byte_t dr() const noexcept { return m_main_command & 0b00000011; }

        // Read fixed-size data
        template<typename T>
        T read() noexcept
        {
            T value = *reinterpret_cast<const T*>(m_code_ptr);
            m_code_ptr += sizeof(T);
            return value;
        }

        // Format register or BP offset
        std::string format_reg_or_bpoffset() noexcept
        {
            byte_t data = read<byte_t>();
            if (data & (1 << 7))
                return format_bp_offset(data);
            return format_register(data);
        }

        // Format global/static value
        std::string format_global_static() noexcept
        {
            uint32_t index = read<uint32_t>();
            if (index < m_env->constant_value_count)
                return format_constant_value(index);
            return "g[" + std::to_string(index - m_env->constant_value_count) + "]";
        }

        // Format first operand based on DR bits
        std::string format_opnum1() noexcept
        {
            return (m_main_command & 0b00000010)
                ? format_reg_or_bpoffset()
                : format_global_static();
        }

        // Format second operand based on DR bits
        std::string format_opnum2() noexcept
        {
            return (m_main_command & 0b00000001)
                ? format_reg_or_bpoffset()
                : format_global_static();
        }

    private:
        static constexpr int SIGNED_SHIFT(byte_t val) noexcept
        {
            return static_cast<signed char>(
                static_cast<unsigned char>(
                    static_cast<unsigned char>(val) << 1)) >> 1;
        }

        std::string format_bp_offset(byte_t data) const noexcept
        {
            int offset = SIGNED_SHIFT(data);
            std::string result = "[bp";
            if (offset < 0)
                result += std::to_string(offset);
            else if (offset == 0)
                result += "-0";
            else
                result += "+" + std::to_string(offset);
            return result + "]";
        }

        std::string format_register(byte_t reg) const noexcept
        {
            static_assert(
                WO_REG_T0 == 0 
                && WO_REG_R0 == 16 
                && WO_REG_CR == 32);

            if (reg <= WO_REG_T0)
                return "t" + std::to_string(reg - WO_REG_T0);
            if (reg <= WO_REG_R0)
                return "r" + std::to_string(reg - WO_REG_R0);

            switch (reg)
            {
            case WO_REG_CR: return "cr";
            case WO_REG_TC: return "tc";
            case WO_REG_ER: return "er";
            case WO_REG_NI: return "nil";
            case WO_REG_PM: return "pm";
            case WO_REG_TP: return "tp";
            default: return "reg(" + std::to_string(reg) + ")";
            }
        }

        std::string format_constant_value(uint32_t index) const noexcept
        {
            const auto& val = m_static_storage[index];
            std::string result;

            if (val.m_type == value::valuetype::string_type)
                result = u8enstring(val.m_string->data(), val.m_string->size(), false);
            else
                result = wo_cast_string(
                    std::launder(
                        reinterpret_cast<wo_value>(
                            const_cast<value*>(&val))));

            result += ": ";
            result += wo_type_name(static_cast<wo_type_t>(val.m_type));
            return result;
        }

        const byte_t* m_code_ptr;
        const runtime_env* m_env;
        const value* m_static_storage;
        byte_t m_main_command;
    };

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
        const wo::byte_t* focus_runtime_ip,
        std::ostream& os) noexcept
    {
        const byte_t* const program = codeholder->rt_codes;
        const byte_t* code_ptr = program + begin;
        const byte_t* const code_end = program + std::min(codeholder->rt_code_len, end);

        while (code_ptr < code_end)
        {
            const byte_t* const instr_begin = code_ptr;
            bytecode_disassembler dis(code_ptr, codeholder);

            std::string mnemonic = disassemble_instruction(dis);
            code_ptr = dis.current();

            // Highlight current instruction pointer position
            const bool is_current_ip =
                (focus_runtime_ip >= instr_begin && focus_runtime_ip < code_ptr);

            if (is_current_ip)
                os << ANSI_INV;

            print_hex_bytes(os, instr_begin, code_ptr,
                static_cast<uint32_t>(instr_begin - program));

            if (is_current_ip)
                os << ANSI_RST;

            os << "| " << mnemonic << std::endl;
        }

        os << std::endl;
    }

    std::string vmbase::disassemble_instruction(bytecode_disassembler& dis) noexcept
    {
        dis.read_command();
        std::string result;

        // Helper macros to simplify binary operation instructions
#define BINARY_OP(name) \
            result = name "\t"; \
            result += dis.format_opnum1(); \
            result += ",\t"; \
            result += dis.format_opnum2(); \
            break

#define UNARY_OP(name) \
            result = name "\t"; \
            result += dis.format_opnum1(); \
            break

#define TERNARY_OP(name) \
            result = name "\t"; \
            result += dis.format_opnum1(); \
            result += ",\t"; \
            result += dis.format_opnum2(); \
            result += ",\t"; \
            result += dis.format_reg_or_bpoffset(); \
            break

        switch (dis.opcode())
        {
        case instruct::nop:
            result = "nop\t";
            // Skip NOP padding bytes
            for (int i = 0; i < dis.dr(); ++i)
                dis.read<byte_t>();
            break;

        case instruct::mov:   BINARY_OP("mov");
        case instruct::movicastr:   BINARY_OP("movicastr");
        case instruct::movrcasti:   BINARY_OP("movrcasti");
        case instruct::negi:  BINARY_OP("negi");
        case instruct::negr:  BINARY_OP("negr");
        case instruct::addi:  BINARY_OP("addi");
        case instruct::subi:  BINARY_OP("subi");
        case instruct::muli:  BINARY_OP("muli");
        case instruct::divi:  BINARY_OP("divi");
        case instruct::modi:  BINARY_OP("modi");
        case instruct::addr:  BINARY_OP("addr");
        case instruct::subr:  BINARY_OP("subr");
        case instruct::mulr:  BINARY_OP("mulr");
        case instruct::divr:  BINARY_OP("divr");
        case instruct::modr:  BINARY_OP("modr");
        case instruct::adds:  BINARY_OP("adds");
        case instruct::lds:   BINARY_OP("lds");
        case instruct::sts:   BINARY_OP("sts");
        case instruct::lti:   BINARY_OP("lti");
        case instruct::gti:   BINARY_OP("gti");
        case instruct::elti:  BINARY_OP("elti");
        case instruct::egti:  BINARY_OP("egti");
        case instruct::land:  BINARY_OP("land");
        case instruct::lor:   BINARY_OP("lor");
        case instruct::lts:   BINARY_OP("lts");
        case instruct::gts:   BINARY_OP("gts");
        case instruct::elts:  BINARY_OP("elts");
        case instruct::egts:  BINARY_OP("egts");
        case instruct::ltr:   BINARY_OP("ltr");
        case instruct::gtr:   BINARY_OP("gtr");
        case instruct::eltr:  BINARY_OP("eltr");
        case instruct::egtr:  BINARY_OP("egtr");
        case instruct::equb:  BINARY_OP("equb");
        case instruct::nequb: BINARY_OP("nequb");
        case instruct::idarr: BINARY_OP("idarr");
        case instruct::iddict: BINARY_OP("iddict");
        case instruct::idstr: BINARY_OP("idstr");
        case instruct::equr:  BINARY_OP("equr");
        case instruct::nequr: BINARY_OP("nequr");
        case instruct::equs:  BINARY_OP("equs");
        case instruct::nequs: BINARY_OP("nequs");
        case instruct::siddict: TERNARY_OP("siddict");
        case instruct::sidmap:  TERNARY_OP("sidmap");
        case instruct::sidarr:  TERNARY_OP("sidarr");

        case instruct::psh:
            if (dis.dr() & 0b01)
            {
                UNARY_OP("psh");
            }
            else
            {
                result = "pshn repeat\t";
                result += std::to_string(dis.read<uint16_t>());
            }
            break;

        case instruct::pop:
            if (dis.dr() & 0b01)
            {
                UNARY_OP("pop");
            }
            else
            {
                result = "pop repeat\t";
                result += std::to_string(dis.read<uint16_t>());
            }
            break;

        case instruct::call:
            UNARY_OP("call");

        case instruct::calln:
        {
            const auto dr = dis.dr();

            if (dr == 0)
                result = "callnwo\t";
            else
                result = (dr & 0b10) ? "callnfp\t" : "callnjit\t";

            if (dis.dr() & 0b01 || dis.dr() & 0b10)
            {
                char buf[32];
                snprintf(buf, sizeof(buf), "%p", dis.read<void*>());
                result += buf;
            }
            else
            {
                result += "+" + std::to_string(dis.read<uint32_t>());
                dis.read<uint32_t>(); // Skip padding
            }
            break;
        }
        case instruct::setip:
        case instruct::setipgc:
        {
            static const char* jmp_names[] = { "jmp", "??", "jmpf", "jmpt" };
            result = jmp_names[dis.dr()];
            result += "\t+";
            result += std::to_string(dis.read<uint32_t>());
            break;
        }

        case instruct::movcast:
            result = "movcast\t";
            result += dis.format_opnum1();
            result += ",\t";
            result += dis.format_opnum2();
            result += " : ";
            result += wo_type_name(static_cast<wo_type_t>(dis.read<byte_t>()));
            break;

        case instruct::movicas:
            result = "movicas\t";
            result += dis.format_opnum1();
            result += ",\t";
            result += dis.format_opnum2();
            result += " if match ";
            result += dis.format_reg_or_bpoffset();
            break;

        case instruct::mkunion:
            result = "mkunion\t";
            result += dis.format_opnum1();
            result += ",\t";
            result += dis.format_opnum2();
            result += ",\t id=";
            result += std::to_string(dis.read<uint16_t>());
            break;

        case instruct::mkclos:
        {
            result = "mkclos\t";
            result += std::to_string(dis.read<uint16_t>());
            result += ",\t";
            if (dis.dr() & 0b10)
            {
                char buf[32];
                snprintf(buf, sizeof(buf), "%p", dis.read<void*>());
                result += buf;
            }
            else
            {
                result += "+" + std::to_string(dis.read<uint32_t>());
                dis.read<uint32_t>(); // Skip padding
            }
            break;
        }

        case instruct::typeas:
            result = (dis.dr() & 0b01) ? "typeis\t" : "typeas\t";
            result += dis.format_opnum1();
            result += " : ";
            result += wo_type_name(static_cast<wo_type_t>(dis.read<byte_t>()));
            break;

        case instruct::endproc:
            switch (dis.dr())
            {
            case 0b00: result = "abrt\t"; break;
            case 0b10: result = "end\t"; break;
            case 0b01: result = "ret\t"; break;
            case 0b11:
                result = "ret pop\t";
                result += std::to_string(dis.read<uint16_t>());
                break;
            }
            break;

        case instruct::mkstruct:
            result = "mkstruct\t";
            result += dis.format_opnum1();
            result += " size=";
            result += std::to_string(dis.read<uint16_t>());
            break;

        case instruct::idstruct:
            result = "idstruct\t";
            result += dis.format_opnum1();
            result += ",\t";
            result += dis.format_opnum2();
            result += " offset=";
            result += std::to_string(dis.read<uint16_t>());
            break;

        case instruct::sidstruct:
            result = "sidstruct\t";
            result += dis.format_opnum1();
            result += ",\t";
            result += dis.format_opnum2();
            result += " offset=";
            result += std::to_string(dis.read<uint16_t>());
            break;

        case instruct::jnequb:
            result = "jnequb\t";
            result += dis.format_opnum1();
            result += "\t+";
            result += std::to_string(dis.read<uint32_t>());
            break;

        case instruct::mkcontain:
            result = (dis.dr() & 0b01) ? "mkmap\t" : "mkarr\t";
            result += dis.format_opnum1();
            result += ",\t size=";
            result += std::to_string(dis.read<uint16_t>());
            break;

        case instruct::unpack:
        {
            result = "unpack\t";
            result += dis.format_opnum1();
            int32_t argc = dis.read<int32_t>();
            if (argc >= 0)
                result += "\tcount = " + std::to_string(argc);
            else
                result += "\tleast = " + std::to_string(-argc);
            break;
        }

        case instruct::ext:
            result = disassemble_ext_instruction(dis);
            break;

        default:
            result = "??\t";
            break;
        }

#undef BINARY_OP
#undef UNARY_OP
#undef TERNARY_OP

        return result;
    }

    std::string vmbase::disassemble_ext_instruction(bytecode_disassembler& dis) noexcept
    {
        std::string result = "ext ";
        int page = dis.dr();
        dis.read_command(); // Read extended opcode

        switch (page)
        {
        case 0:
            switch (dis.opcode())
            {
            case instruct::extern_opcode_page_0::pack:
            {
                result += "pack\t";
                result += dis.format_opnum1();
                result += ",\t";
                uint16_t argc = dis.read<uint16_t>();
                uint16_t skip = dis.read<uint16_t>();
                result += ": skip " + std::to_string(argc) + "/" + std::to_string(skip);
                break;
            }
            case instruct::extern_opcode_page_0::panic:
                result += "panic\t";
                result += dis.format_opnum1();
                break;
            case instruct::extern_opcode_page_0::cdivilr:
                result += "cdivilr\t";
                result += dis.format_opnum1();
                result += ",\t";
                result += dis.format_opnum2();
                break;
            case instruct::extern_opcode_page_0::cdivil:
                result += "cdivil\t";
                result += dis.format_opnum1();
                break;
            case instruct::extern_opcode_page_0::cdivirz:
                result += "cdivirz\t";
                result += dis.format_opnum1();
                break;
            case instruct::extern_opcode_page_0::cdivir:
                result += "cdivir\t";
                result += dis.format_opnum1();
                break;
            case instruct::extern_opcode_page_0::popn:
                result += "popn\t";
                result += dis.format_opnum1();
                break;
            case instruct::extern_opcode_page_0::addh:
                result += "addh\t";
                result += dis.format_opnum1();
                result += ",\t";
                result += dis.format_opnum2();
                break;
            case instruct::extern_opcode_page_0::subh:
                result += "subh\t";
                result += dis.format_opnum1();
                result += ",\t";
                result += dis.format_opnum2();
                break;
            case instruct::extern_opcode_page_0::lth:
                result += "lth\t";
                result += dis.format_opnum1();
                result += ",\t";
                result += dis.format_opnum2();
                break;
            case instruct::extern_opcode_page_0::gth:
                result += "gth\t";
                result += dis.format_opnum1();
                result += ",\t";
                result += dis.format_opnum2();
                break;
            case instruct::extern_opcode_page_0::elth:
                result += "elth\t";
                result += dis.format_opnum1();
                result += ",\t";
                result += dis.format_opnum2();
                break;
            case instruct::extern_opcode_page_0::egth:
                result += "egth\t";
                result += dis.format_opnum1();
                result += ",\t";
                result += dis.format_opnum2();
                break;
            default:
                result += "??\t";
                break;
            }
            break;

        case 3:
            result += "flag ";
            switch (dis.opcode())
            {
            case instruct::extern_opcode_page_3::funcbegin:
                result += "funcbegin\t";
                break;
            case instruct::extern_opcode_page_3::funcend:
                result += "funcend\t";
                break;
            default:
                result += "??\t";
                break;
            }
            break;

        default:
            result += "??\t";
            break;
        }

        return result;
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
                const byte_t* m_abs_addr;
                uint32_t m_diff_offset;
            };
            value* m_bp;

            callstack_ip_state()
                : m_type(ipaddr_kind::BAD)
                , m_abs_addr(nullptr)
                , m_bp(nullptr)
            {
            }
            callstack_ip_state(const byte_t* abs, value* bp)
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
                    || runtime_env::fetch_far_runtime_env(rip, &callenv))
                {
                    if (callenv == env.get())
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
                    rip,
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
                    callstack_state.m_abs_addr, &current_env_pointer))
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
                callstack_ip = callstack_state.m_abs_addr;
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

    void vmbase::co_pre_invoke_script(const byte_t* wo_func_addr, wo_int_t argc) noexcept
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

        ip = reinterpret_cast<const byte_t*>(ex_func_addr);
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
            ? (const byte_t*)wo_func_addr->m_native_func
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

    value* vmbase::invoke_script(const byte_t* wo_func_addr, wo_int_t argc) noexcept
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
        ip = reinterpret_cast<const wo::byte_t*>(wo_func_addr);
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

#define WO_SAFE_READ_OFFSET_GET_QWORD (*(uint64_t*)(rt_ip-8))
#define WO_SAFE_READ_OFFSET_GET_DWORD (*(uint32_t*)(rt_ip-4))
#define WO_SAFE_READ_OFFSET_GET_WORD (*(uint16_t*)(rt_ip-2))

    // FOR BigEndian
#define WO_SAFE_READ_OFFSET_PER_BYTE(OFFSET, TYPE) (((TYPE)(*(rt_ip-OFFSET)))<<((sizeof(TYPE)-OFFSET)*8))
#define WO_IS_ODD_IRPTR(ALLIGN) 1 //(reinterpret_cast<size_t>(rt_ip)%ALLIGN)

#define WO_SAFE_READ_MOVE_2 \
    (rt_ip+=2,WO_IS_ODD_IRPTR(2)?\
        (uint16_t)(WO_SAFE_READ_OFFSET_PER_BYTE(2,uint16_t)|WO_SAFE_READ_OFFSET_PER_BYTE(1,uint16_t)):\
        WO_SAFE_READ_OFFSET_GET_WORD)
#define WO_SAFE_READ_MOVE_4 \
    (rt_ip+=4,WO_IS_ODD_IRPTR(4)?\
        (uint32_t)(WO_SAFE_READ_OFFSET_PER_BYTE(4,uint32_t)|WO_SAFE_READ_OFFSET_PER_BYTE(3,uint32_t)\
        |WO_SAFE_READ_OFFSET_PER_BYTE(2,uint32_t)|WO_SAFE_READ_OFFSET_PER_BYTE(1,uint32_t)):\
        WO_SAFE_READ_OFFSET_GET_DWORD)
#define WO_SAFE_READ_MOVE_8 \
    (rt_ip+=8,WO_IS_ODD_IRPTR(8)?\
        (uint64_t)(WO_SAFE_READ_OFFSET_PER_BYTE(8,uint64_t)|WO_SAFE_READ_OFFSET_PER_BYTE(7,uint64_t)|\
        WO_SAFE_READ_OFFSET_PER_BYTE(6,uint64_t)|WO_SAFE_READ_OFFSET_PER_BYTE(5,uint64_t)|\
        WO_SAFE_READ_OFFSET_PER_BYTE(4,uint64_t)|WO_SAFE_READ_OFFSET_PER_BYTE(3,uint64_t)|\
        WO_SAFE_READ_OFFSET_PER_BYTE(2,uint64_t)|WO_SAFE_READ_OFFSET_PER_BYTE(1,uint64_t)):\
        WO_SAFE_READ_OFFSET_GET_QWORD)

    // X86 support non-aligned addressing, so just do it!
#define WO_FAST_READ_MOVE_2 (*(uint16_t*)((rt_ip += 2) - 2))
#define WO_FAST_READ_MOVE_4 (*(uint32_t*)((rt_ip += 4) - 4))
#define WO_FAST_READ_MOVE_8 (*(uint64_t*)((rt_ip += 8) - 8))

#define WO_IPVAL (*(rt_ip))
#define WO_IPVAL_MOVE_1 (*(rt_ip++))

#ifdef WO_VM_SUPPORT_FAST_NO_ALIGN
#   define WO_IPVAL_MOVE_2 WO_FAST_READ_MOVE_2
#   define WO_IPVAL_MOVE_4 WO_FAST_READ_MOVE_4
#   define WO_IPVAL_MOVE_8 WO_FAST_READ_MOVE_8
#else
#   define WO_IPVAL_MOVE_2 WO_SAFE_READ_MOVE_2
#   define WO_IPVAL_MOVE_4 WO_SAFE_READ_MOVE_4
#   define WO_IPVAL_MOVE_8 WO_SAFE_READ_MOVE_8
#endif

    // VM Operate
#define WO_VM_RETURN(V) do{ ip = rt_ip; return (V); }while(0)
#define WO_SIGNED_SHIFT(VAL) (static_cast<int8_t>(((VAL) << 1)) >> 1)

#define WO_ADDRESSING_RS \
    ((static_cast<int8_t>(WO_IPVAL) < 0 ? bp : reg_begin) + WO_SIGNED_SHIFT(WO_IPVAL))
#define WO_ADDRESSING_G_AND_MOVE4 \
    (WO_IPVAL_MOVE_4 + near_static_global)

#define WO_ADDRESSING_RS1 \
        opnum1 = WO_ADDRESSING_RS; ++rt_ip
#define WO_ADDRESSING_G1 \
        opnum1 = WO_ADDRESSING_G_AND_MOVE4
#define WO_ADDRESSING_RS2 \
        opnum2 = WO_ADDRESSING_RS; ++rt_ip
#define WO_ADDRESSING_G2 \
        opnum2 = WO_ADDRESSING_G_AND_MOVE4
#define WO_ADDRESSING_RS3 \
        opnum3 = WO_ADDRESSING_RS; ++rt_ip

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
        const wo::byte_t* addr,
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

    value* vmbase::make_array_impl(value* opnum1, uint16_t size, value* rt_sp) noexcept
    {
        auto* maked_array = array_t::gc_new<gcbase::gctype::young>((size_t)size);

        for (size_t i = 0; i < (size_t)size; i++)
        {
            auto* arr_val = ++rt_sp;
            maked_array->at(size - i - 1).set_val(arr_val);
        }

        opnum1->set_gcunit<value::valuetype::array_type>(maked_array);
        return rt_sp;
    }

    value* vmbase::make_map_impl(value* opnum1, uint16_t size, value* rt_sp) noexcept
    {
        auto* maked_dict = dictionary_t::gc_new<gcbase::gctype::young>((size_t)size);

        for (size_t i = 0; i < (size_t)size; i++)
        {
            value* val = ++rt_sp;
            value* key = ++rt_sp;
            maked_dict->insert(std::make_pair(*key, *val));
        }

        opnum1->set_gcunit<value::valuetype::dict_type>(maked_dict);
        return rt_sp;
    }

    value* vmbase::make_struct_impl(value* opnum1, uint16_t size, value* rt_sp) noexcept
    {
        auto* maked_struct = structure_t::gc_new<gcbase::gctype::young>(size);

        for (size_t i = 0; i < size; i++)
            maked_struct->m_values[size - i - 1].set_val(++rt_sp);

        opnum1->set_gcunit<value::valuetype::struct_type>(maked_struct);
        return rt_sp;
    }

    //////////////////////////////////////////////////////////////////////////
    // Argument Packing/Unpacking Implementations
    //////////////////////////////////////////////////////////////////////////

    void vmbase::packargs_impl(
        value* opnum1,
        uint16_t argcount,
        const value* tp,
        value* rt_bp,
        uint16_t skip_closure_arg_count) noexcept
    {
        auto* packed_array = array_t::gc_new<gcbase::gctype::young>();
        packed_array->resize((size_t)tp->m_integer - (size_t)argcount);

        for (auto argindex = 0 + (size_t)argcount;
            argindex < (size_t)tp->m_integer;
            argindex++)
        {
            (*packed_array)[(size_t)argindex - (size_t)argcount].set_val(
                rt_bp + 2 + argindex + skip_closure_arg_count);
        }

        opnum1->set_gcunit<wo::value::valuetype::array_type>(packed_array);
    }

    value* vmbase::unpackargs_impl(
        vmbase* vm,
        value* opnum1,
        int32_t unpack_argc,
        value* tc,
        const byte_t* rt_ip,
        value* rt_sp,
        value* rt_bp) noexcept
    {
        if (opnum1->m_type == value::valuetype::struct_type)
        {
            auto* arg_tuple = opnum1->m_structure;
            gcbase::gc_read_guard gwg1(arg_tuple);

            if (unpack_argc > 0)
            {
                if ((size_t)unpack_argc > (size_t)arg_tuple->m_count)
                {
                    vm->ip = rt_ip;
                    vm->sp = rt_sp;
                    vm->bp = rt_bp;

                    wo_fail(WO_FAIL_INDEX_FAIL,
                        "The number of arguments required for unpack exceeds the number "
                        "of arguments in the given arguments-package.");
                }
                else
                {
                    if (rt_sp - unpack_argc < vm->stack_storage)
                        goto _wo_unpackargs_stack_overflow;

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

                    wo_fail(WO_FAIL_INDEX_FAIL,
                        "The number of arguments required for unpack exceeds the number "
                        "of arguments in the given arguments-package.");
                }
                else
                {
                    if (rt_sp - arg_tuple->m_count < vm->stack_storage)
                        goto _wo_unpackargs_stack_overflow;

                    for (uint16_t i = arg_tuple->m_count; i > 0; --i)
                        (rt_sp--)->set_val(&arg_tuple->m_values[i - 1]);

                    tc->m_integer += (wo_integer_t)arg_tuple->m_count;
                }
            }
        }
        else if (opnum1->m_type == value::valuetype::array_type)
        {
            if (unpack_argc > 0)
            {
                auto* arg_array = opnum1->m_array;
                gcbase::gc_read_guard gwg1(arg_array);

                if ((size_t)unpack_argc > arg_array->size())
                {
                    vm->ip = rt_ip;
                    vm->sp = rt_sp;
                    vm->bp = rt_bp;

                    wo_fail(WO_FAIL_INDEX_FAIL,
                        "The number of arguments required for unpack exceeds the number "
                        "of arguments in the given arguments-package.");
                }
                else
                {
                    if (rt_sp - unpack_argc < vm->stack_storage)
                        goto _wo_unpackargs_stack_overflow;

                    const auto args_rend = arg_array->rend();
                    auto arg_idx = arg_array->rbegin();

                    std::advance(
                        arg_idx,
                        static_cast<ptrdiff_t>(
                            (wo_integer_t)arg_array->size() - unpack_argc));

                    for (; arg_idx != args_rend; arg_idx++)
                        (rt_sp--)->set_val(&*arg_idx);
                }
            }
            else
            {
                auto* arg_array = opnum1->m_array;
                gcbase::gc_read_guard gwg1(arg_array);

                if (arg_array->size() < (size_t)(-unpack_argc))
                {
                    vm->ip = rt_ip;
                    vm->sp = rt_sp;
                    vm->bp = rt_bp;

                    wo_fail(WO_FAIL_INDEX_FAIL,
                        "The number of arguments required for unpack exceeds the number "
                        "of arguments in the given arguments-package.");
                }
                else
                {
                    size_t arg_array_len = arg_array->size();

                    if (rt_sp - arg_array_len < vm->stack_storage)
                        goto _wo_unpackargs_stack_overflow;

                    const auto args_rend = arg_array->rend();
                    for (auto arg_idx = arg_array->rbegin(); arg_idx != args_rend; arg_idx++)
                        (rt_sp--)->set_val(&*arg_idx);

                    tc->m_integer += arg_array_len;
                }
            }
        }
        else
        {
            vm->ip = rt_ip;
            vm->sp = rt_sp;
            vm->bp = rt_bp;
            wo_fail(WO_FAIL_INDEX_FAIL, "Only valid array/struct can be used in unpack.");
        }

        return rt_sp;

    _wo_unpackargs_stack_overflow:
        vm->interrupt(vmbase::vm_interrupt_type::STACK_OVERFLOW_INTERRUPT);
        return nullptr;
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

        const byte_t* near_rtcode_begin = runtime_codes_begin;
        const byte_t* near_rtcode_end = runtime_codes_end;
        value* near_static_global = runtime_static_storage;

        value* const rt_cr = cr;
        value* const reg_begin = register_storage;

        value* opnum1, * opnum2, * opnum3;

        const byte_t* rt_ip = ip;
        const byte_t* ip_for_rollback;

#define WO_RSG_ADDRESSING_CASE(CODE)        \
        instruct::opcode::CODE##gg:         \
            WO_ADDRESSING_G1;               \
            WO_ADDRESSING_G2;               \
            goto _label_##CODE##_impl;      \
        case instruct::opcode::CODE##gs:    \
            WO_ADDRESSING_G1;               \
            WO_ADDRESSING_RS2;              \
            goto _label_##CODE##_impl;      \
        case instruct::opcode::CODE##sg:    \
            WO_ADDRESSING_RS1;              \
            WO_ADDRESSING_G2;               \
            goto _label_##CODE##_impl;      \
        case instruct::opcode::CODE##ss:    \
            WO_ADDRESSING_RS1;              \
            WO_ADDRESSING_RS2;              \
        _label_##CODE##_impl
        /*
        // ISSUE: 25-08-16:
        The global storage area functions like a container, and as such, elements within
        it may escape, just like in a container. Therefore, write barrier checks are
        required for all potential write operations to the global storage area. Below
        is a list of instructions that may trigger such write barriers:

        pop,
        adds,
        mov,
        movcast,
        lds,
        mkstruct,
        idstruct,
        mkcontain,
        mkunion,
        pack,

        Note that instructions like addi and addr do not require write barrier checks
        because they explicitly specify that the operand's type is a primitive type.
        */
#define WO_WRITE_CHECK_FOR_GLOBAL(VAL)                          \
        if (wo::gc::gc_is_marking())                            \
            wo::value::write_barrier(VAL)

#define WO_RSG_ADDRESSING_WRITE_OP1_CASE(CODE)                  \
        instruct::opcode::CODE##gg:                             \
            WO_ADDRESSING_G1;                                   \
            WO_WRITE_CHECK_FOR_GLOBAL(opnum1);                  \
            WO_ADDRESSING_G2;                                   \
            goto _label_##CODE##_impl;                          \
        case instruct::opcode::CODE##gs:                        \
            WO_ADDRESSING_G1;                                   \
            WO_WRITE_CHECK_FOR_GLOBAL(opnum1);                  \
            WO_ADDRESSING_RS2;                                  \
            goto _label_##CODE##_impl;                          \
        case instruct::opcode::CODE##sg:                        \
            WO_ADDRESSING_RS1;                                  \
            WO_ADDRESSING_G2;                                   \
            goto _label_##CODE##_impl;                          \
        case instruct::opcode::CODE##ss:                        \
            WO_ADDRESSING_RS1;                                  \
            WO_ADDRESSING_RS2;                                  \
        _label_##CODE##_impl

#define WO_RSG_ADDRESSING_EXT_CASE(CODE)                        \
        instruct::extern_opcode_page_0::CODE##gg:               \
            WO_ADDRESSING_G1;                                   \
            WO_ADDRESSING_G2;                                   \
            goto _label_ext0_##CODE##_impl;                     \
        case instruct::extern_opcode_page_0::CODE##gs:          \
            WO_ADDRESSING_G1;                                   \
            WO_ADDRESSING_RS2;                                  \
            goto _label_ext0_##CODE##_impl;                     \
        case instruct::extern_opcode_page_0::CODE##sg:          \
            WO_ADDRESSING_RS1;                                  \
            WO_ADDRESSING_G2;                                   \
            goto _label_ext0_##CODE##_impl;                     \
        case instruct::extern_opcode_page_0::CODE##ss:          \
            WO_ADDRESSING_RS1;                                  \
            WO_ADDRESSING_RS2;                                  \
        _label_ext0_##CODE##_impl

#define WO_VM_GOTO_HANDLE_INTERRUPT                             \
    goto _label_vm_handle_interrupt

#define WO_VM_CHECK_INTERRUPT                                   \
    (vm_interrupt_type::NOTHING != vm_interrupt.load(std::memory_order_acquire))

#define WO_VM_INTERRUPT_CHECKPOINT_AND_GOTO_HANDLE_INTERRUPT    \
    do {                                                        \
        if (WO_VM_CHECK_INTERRUPT) [[unlikely]]                 \
            WO_VM_GOTO_HANDLE_INTERRUPT;                        \
    } while (0)

#define WO_VM_FAIL(ERRNO, ...)                                  \
    do {                                                        \
        ip = rt_ip;                                             \
        wo_fail(ERRNO, __VA_ARGS__);                            \
        WO_VM_GOTO_HANDLE_INTERRUPT;                            \
    } while(0)

#if WO_ENABLE_RUNTIME_CHECK == 0
#   define WO_VM_ASSERT(EXPR, REASON) wo_assert(EXPR, REASON)
#else
#   define WO_VM_ASSERT(EXPR, ...)                              \
        do {                                                    \
            if(!(EXPR))                                         \
                WO_VM_FAIL(WO_FAIL_UNEXPECTED, __VA_ARGS__);    \
        } while(0)
#endif

        bool debuggee_attached = WO_VM_CHECK_INTERRUPT;
        for (;;)
        {
            if (debuggee_attached) [[unlikely]]
                goto _label_vm_handle_interrupt;

        _label_vm_re_entry:
            switch (WO_IPVAL_MOVE_1)
            {
            case instruct::opcode::pshr:
            {
                uint16_t psh_repeat = WO_IPVAL_MOVE_2;
                value* new_sp = sp - psh_repeat;
                if (new_sp < stack_storage) [[unlikely]]
                {
                    rt_ip -= 3;
                    wo_assure(interrupt(vm_interrupt_type::STACK_OVERFLOW_INTERRUPT));
                    WO_VM_GOTO_HANDLE_INTERRUPT;
                }
                else
                    sp = new_sp;
                break;
            }
            case instruct::opcode::pshg:
                if (sp <= stack_storage) [[unlikely]]
                {
                    --rt_ip;
                    wo_assure(interrupt(vm_interrupt_type::STACK_OVERFLOW_INTERRUPT));
                    WO_VM_GOTO_HANDLE_INTERRUPT;
                    break;
                }
                WO_ADDRESSING_G1;
                goto _label_psh_impl;
            case instruct::opcode::pshs:
                if (sp <= stack_storage) [[unlikely]]
                {
                    --rt_ip;
                    wo_assure(interrupt(vm_interrupt_type::STACK_OVERFLOW_INTERRUPT));
                    WO_VM_GOTO_HANDLE_INTERRUPT;
                    break;
                }
                WO_ADDRESSING_RS1;
            _label_psh_impl:
                (sp--)->set_val(opnum1);
                break;
            case instruct::opcode::popr:
                sp += WO_IPVAL_MOVE_2;
                break;
            case instruct::opcode::popg:
                WO_ADDRESSING_G1;
                WO_WRITE_CHECK_FOR_GLOBAL(opnum1);
                goto _label_pop_impl;
            case instruct::opcode::pops:
                WO_ADDRESSING_RS1;
            _label_pop_impl:
                opnum1->set_val((++sp));
                break;
            case WO_RSG_ADDRESSING_CASE(addi):
                WO_VM_ASSERT(opnum1->m_type == opnum2->m_type
                    && opnum1->m_type == value::valuetype::integer_type,
                    "Operand should be integer in 'addi'.");

                opnum1->m_integer += opnum2->m_integer;
                break;
            case WO_RSG_ADDRESSING_CASE(subi):
                WO_VM_ASSERT(opnum1->m_type == opnum2->m_type
                    && opnum1->m_type == value::valuetype::integer_type,
                    "Operand should be integer in 'subi'.");

                opnum1->m_integer -= opnum2->m_integer;
                break;
            case WO_RSG_ADDRESSING_CASE(muli):
                WO_VM_ASSERT(opnum1->m_type == opnum2->m_type
                    && opnum1->m_type == value::valuetype::integer_type,
                    "Operand should be integer in 'muli'.");

                opnum1->m_integer *= opnum2->m_integer;
                break;
            case WO_RSG_ADDRESSING_CASE(divi):
                WO_VM_ASSERT(opnum1->m_type == opnum2->m_type
                    && opnum1->m_type == value::valuetype::integer_type,
                    "Operand should be integer in 'divi'.");

                WO_VM_ASSERT(opnum2->m_integer != 0,
                    "The divisor cannot be 0.");
                WO_VM_ASSERT(opnum2->m_integer != -1 || opnum1->m_integer != INT64_MIN,
                    "Division overflow.");

                opnum1->m_integer /= opnum2->m_integer;
                break;
            case WO_RSG_ADDRESSING_CASE(modi):
                WO_VM_ASSERT(opnum1->m_type == opnum2->m_type
                    && opnum1->m_type == value::valuetype::integer_type,
                    "Operand should be integer in 'modi'.");

                WO_VM_ASSERT(opnum2->m_integer != 0,
                    "The divisor cannot be 0.");
                WO_VM_ASSERT(opnum2->m_integer != -1 || opnum1->m_integer != INT64_MIN,
                    "Division overflow.");

                opnum1->m_integer %= opnum2->m_integer;
                break;
            case WO_RSG_ADDRESSING_CASE(addr):
                WO_VM_ASSERT(opnum1->m_type == opnum2->m_type
                    && opnum1->m_type == value::valuetype::real_type,
                    "Operand should be real in 'addr'.");

                opnum1->m_real += opnum2->m_real;
                break;
            case WO_RSG_ADDRESSING_CASE(subr):
                WO_VM_ASSERT(opnum1->m_type == opnum2->m_type
                    && opnum1->m_type == value::valuetype::real_type,
                    "Operand should be real in 'subr'.");

                opnum1->m_real -= opnum2->m_real;
                break;
            case WO_RSG_ADDRESSING_CASE(mulr):
                WO_VM_ASSERT(opnum1->m_type == opnum2->m_type
                    && opnum1->m_type == value::valuetype::real_type,
                    "Operand should be real in 'mulr'.");

                opnum1->m_real *= opnum2->m_real;
                break;
            case WO_RSG_ADDRESSING_CASE(divr):
                WO_VM_ASSERT(opnum1->m_type == opnum2->m_type
                    && opnum1->m_type == value::valuetype::real_type,
                    "Operand should be real in 'divr'.");

                opnum1->m_real /= opnum2->m_real;
                break;
            case WO_RSG_ADDRESSING_CASE(modr):
                WO_VM_ASSERT(opnum1->m_type == opnum2->m_type
                    && opnum1->m_type == value::valuetype::real_type,
                    "Operand should be real in 'modr'.");

                opnum1->m_real = fmod(opnum1->m_real, opnum2->m_real);
                break;
            case WO_RSG_ADDRESSING_CASE(negi):
                WO_VM_ASSERT(opnum2->m_type == value::valuetype::integer_type,
                    "Operand should be integer in 'negi'.");
                opnum1->set_integer(-opnum2->m_integer);
                break;
            case WO_RSG_ADDRESSING_CASE(negr):
                WO_VM_ASSERT(opnum2->m_type == value::valuetype::real_type,
                    "Operand should be real in 'negr'.");
                opnum1->set_real(-opnum2->m_real);
                break;
            case WO_RSG_ADDRESSING_WRITE_OP1_CASE(adds):
                WO_VM_ASSERT(opnum1->m_type == opnum2->m_type
                    && opnum1->m_type == value::valuetype::string_type,
                    "Operand should be string in 'adds'.");

                opnum1->set_gcunit<wo::value::valuetype::string_type>(
                    string_t::gc_new<gcbase::gctype::young>(
                        *opnum1->m_string + *opnum2->m_string));
                break;
            case WO_RSG_ADDRESSING_WRITE_OP1_CASE(mov):
                opnum1->set_val(opnum2);
                break;
            case WO_RSG_ADDRESSING_WRITE_OP1_CASE(movcast):
                if (auto* err = movcast_impl(
                    opnum1,
                    opnum2,
                    static_cast<value::valuetype>(WO_IPVAL_MOVE_1)))
                    WO_VM_FAIL(WO_FAIL_TYPE_FAIL, err);
                break;
            case WO_RSG_ADDRESSING_WRITE_OP1_CASE(movicastr):
                WO_VM_ASSERT(opnum2->m_type == value::valuetype::integer_type,
                    "Operand 2 should be integer in 'movicastr'.");
                opnum1->set_real(static_cast<wo_real_t>(opnum2->m_integer));
                break;
            case WO_RSG_ADDRESSING_WRITE_OP1_CASE(movrcasti):
                WO_VM_ASSERT(opnum2->m_type == value::valuetype::real_type,
                    "Operand 2 should be real in 'movrcasti'.");
                opnum1->set_integer(static_cast<wo_integer_t>(opnum2->m_real));
                break;
            case instruct::opcode::typeasg:
                WO_ADDRESSING_G1;
                goto _label_typeas_impl;
            case instruct::opcode::typeass:
                WO_ADDRESSING_RS1;
            _label_typeas_impl:
                if (opnum1->m_type != static_cast<value::valuetype>(WO_IPVAL_MOVE_1))
                    WO_VM_FAIL(WO_FAIL_TYPE_FAIL,
                        "The given value is not the same as the requested type.");
                break;
            case instruct::opcode::typeisg:
                WO_ADDRESSING_G1;
                goto _label_typeis_impl;
            case instruct::opcode::typeiss:
                WO_ADDRESSING_RS1;
            _label_typeis_impl:
                rt_cr->set_bool(
                    opnum1->m_type == static_cast<value::valuetype>(
                        WO_IPVAL_MOVE_1));
                break;
            case WO_RSG_ADDRESSING_WRITE_OP1_CASE(lds):
                WO_VM_ASSERT(opnum2->m_type == value::valuetype::integer_type,
                    "Operand 2 should be integer in 'lds'.");
                opnum1->set_val(bp + opnum2->m_integer);
                break;
            case WO_RSG_ADDRESSING_CASE(sts):
                WO_VM_ASSERT(opnum2->m_type == value::valuetype::integer_type,
                    "Operand 2 should be integer in 'sts'.");
                (bp + opnum2->m_integer)->set_val(opnum1);
                break;
            case WO_RSG_ADDRESSING_CASE(equb):
                rt_cr->set_bool(opnum1->m_integer == opnum2->m_integer);
                break;
            case WO_RSG_ADDRESSING_CASE(nequb):
                rt_cr->set_bool(opnum1->m_integer != opnum2->m_integer);
                break;
            case WO_RSG_ADDRESSING_CASE(equr):
                WO_VM_ASSERT(opnum1->m_type == opnum2->m_type && opnum1->m_type == value::valuetype::real_type,
                    "Operand should be real in 'equr'.");
                rt_cr->set_bool(opnum1->m_real == opnum2->m_real);
                break;
            case WO_RSG_ADDRESSING_CASE(nequr):
                WO_VM_ASSERT(opnum1->m_type == opnum2->m_type && opnum1->m_type == value::valuetype::real_type,
                    "Operand should be real in 'nequr'.");
                rt_cr->set_bool(opnum1->m_real != opnum2->m_real);
                break;
            case WO_RSG_ADDRESSING_CASE(equs):
                WO_VM_ASSERT(opnum1->m_type == opnum2->m_type
                    && opnum1->m_type == value::valuetype::string_type,
                    "Operand should be string in 'equs'.");

                rt_cr->set_bool(
                    opnum1->m_string == opnum2->m_string
                    || *opnum1->m_string == *opnum2->m_string);

                break;
            case WO_RSG_ADDRESSING_CASE(nequs):
                WO_VM_ASSERT(opnum1->m_type == opnum2->m_type
                    && opnum1->m_type == value::valuetype::string_type,
                    "Operand should be string in 'nequs'.");

                rt_cr->set_bool(
                    opnum1->m_string != opnum2->m_string
                    && *opnum1->m_string != *opnum2->m_string);

                break;
            case WO_RSG_ADDRESSING_CASE(land):
                rt_cr->set_bool(opnum1->m_integer && opnum2->m_integer);
                break;
            case WO_RSG_ADDRESSING_CASE(lor):
                rt_cr->set_bool(opnum1->m_integer || opnum2->m_integer);
                break;
            case WO_RSG_ADDRESSING_CASE(lti):
                WO_VM_ASSERT(opnum1->m_type == opnum2->m_type
                    && opnum1->m_type == value::valuetype::integer_type,
                    "Operand should be integer in 'lti'.");

                rt_cr->set_bool(opnum1->m_integer < opnum2->m_integer);
                break;
            case WO_RSG_ADDRESSING_CASE(gti):
                WO_VM_ASSERT(opnum1->m_type == opnum2->m_type
                    && opnum1->m_type == value::valuetype::integer_type,
                    "Operand should be integer in 'gti'.");

                rt_cr->set_bool(opnum1->m_integer > opnum2->m_integer);
                break;
            case WO_RSG_ADDRESSING_CASE(elti):
                WO_VM_ASSERT(opnum1->m_type == opnum2->m_type
                    && opnum1->m_type == value::valuetype::integer_type,
                    "Operand should be integer in 'elti'.");

                rt_cr->set_bool(opnum1->m_integer <= opnum2->m_integer);
                break;
            case WO_RSG_ADDRESSING_CASE(egti):
                WO_VM_ASSERT(opnum1->m_type == opnum2->m_type
                    && opnum1->m_type == value::valuetype::integer_type,
                    "Operand should be integer in 'egti'.");

                rt_cr->set_bool(opnum1->m_integer >= opnum2->m_integer);
                break;
            case WO_RSG_ADDRESSING_CASE(ltr):
                WO_VM_ASSERT(opnum1->m_type == opnum2->m_type
                    && opnum1->m_type == value::valuetype::real_type,
                    "Operand should be real in 'ltr'.");

                rt_cr->set_bool(opnum1->m_real < opnum2->m_real);
                break;
            case WO_RSG_ADDRESSING_CASE(gtr):
                WO_VM_ASSERT(opnum1->m_type == opnum2->m_type
                    && opnum1->m_type == value::valuetype::real_type,
                    "Operand should be real in 'gtr'.");

                rt_cr->set_bool(opnum1->m_real > opnum2->m_real);
                break;
            case WO_RSG_ADDRESSING_CASE(eltr):
                WO_VM_ASSERT(opnum1->m_type == opnum2->m_type
                    && opnum1->m_type == value::valuetype::real_type,
                    "Operand should be real in 'eltr'.");

                rt_cr->set_bool(opnum1->m_real <= opnum2->m_real);
                break;
            case WO_RSG_ADDRESSING_CASE(egtr):
                WO_VM_ASSERT(opnum1->m_type == opnum2->m_type
                    && opnum1->m_type == value::valuetype::real_type,
                    "Operand should be real in 'egtr'.");

                rt_cr->set_bool(opnum1->m_real >= opnum2->m_real);
                break;
            case WO_RSG_ADDRESSING_CASE(lts):
                WO_VM_ASSERT(opnum1->m_type == opnum2->m_type
                    && opnum1->m_type == value::valuetype::string_type,
                    "Operand should be string in 'lts'.");

                rt_cr->set_bool(*opnum1->m_string < *opnum2->m_string);
                break;
            case WO_RSG_ADDRESSING_CASE(gts):
                WO_VM_ASSERT(opnum1->m_type == opnum2->m_type,
                    "Operand should be string in 'gtx'.");

                rt_cr->set_bool(*opnum1->m_string > *opnum2->m_string);
                break;
            case WO_RSG_ADDRESSING_CASE(elts):
                WO_VM_ASSERT(opnum1->m_type == opnum2->m_type,
                    "Operand should be string in 'eltx'.");

                rt_cr->set_bool(*opnum1->m_string <= *opnum2->m_string);
                break;
            case WO_RSG_ADDRESSING_CASE(egts):
                WO_VM_ASSERT(opnum1->m_type == opnum2->m_type,
                    "Operand should be string in 'egtx'.");

                rt_cr->set_bool(*opnum1->m_string >= *opnum2->m_string);
                break;
            case instruct::opcode::retn:
            {
                const uint16_t pop_count = WO_IPVAL_MOVE_2;

                switch ((++bp)->m_type)
                {
                case value::valuetype::native_callstack:
                    sp = bp;
                    sp += pop_count;
                    // last stack is native_func, just do return;
                    // stack balance should be keeped by invoker.
                    WO_VM_RETURN(wo_result_t::WO_API_NORMAL);
                case value::valuetype::callstack:
                {
                    value* stored_bp = sb - bp->m_vmcallstack.bp;
                    wo_assert(stored_bp <= sb && stored_bp > stack_storage);

                    rt_ip = near_rtcode_begin + bp->m_vmcallstack.ret_ip;
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

                    if (rt_ip < near_rtcode_begin || rt_ip >= near_rtcode_end) [[unlikely]]
                    {
                        wo_assure(interrupt(vm_interrupt_type::CALL_FAR_RESYNC_VM_STATE_INTERRUPT));
                        WO_VM_GOTO_HANDLE_INTERRUPT;
                    }
                    break;
                }
                default:
#if WO_ENABLE_RUNTIME_CHECK
                    WO_VM_FAIL(WO_FAIL_TYPE_FAIL, "Broken stack in 'retn'.");
#endif
                    break;
                }
                sp += pop_count;
                break;
            }
            case instruct::opcode::ret:
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

                    rt_ip = near_rtcode_begin + bp->m_vmcallstack.ret_ip;
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

                    if (rt_ip < near_rtcode_begin || rt_ip >= near_rtcode_end) [[unlikely]]
                    {
                        wo_assure(interrupt(vm_interrupt_type::CALL_FAR_RESYNC_VM_STATE_INTERRUPT));
                        WO_VM_GOTO_HANDLE_INTERRUPT;
                    }
                    break;
                }
                default:
#if WO_ENABLE_RUNTIME_CHECK
                    WO_VM_FAIL(WO_FAIL_TYPE_FAIL, "Broken stack in 'retn'.");
#endif
                    break;
                }
                break;
            }
            case instruct::opcode::callg:
                ip_for_rollback = rt_ip - 1;
                WO_ADDRESSING_G1;
                goto _label_call_impl;
            case instruct::opcode::calls:
                ip_for_rollback = rt_ip - 1;
                WO_ADDRESSING_RS1;
            _label_call_impl:
                WO_VM_ASSERT(0 != opnum1->m_handle && nullptr != opnum1->m_closure,
                    "Cannot invoke null function in 'call'.");
                do
                {
                    if (opnum1->m_type == value::valuetype::closure_type)
                    {
                        // Call closure, unpack closure captured arguments.
                        // 
                        // NOTE: Closure arguments should be poped by closure function it self.
                        //       Can use ret(n) to pop arguments when call.
                        if (sp - opnum1->m_closure->m_closure_args_count < stack_storage) [[unlikely]]
                        {
                            rt_ip = ip_for_rollback;
                            wo_assure(interrupt(vm_interrupt_type::STACK_OVERFLOW_INTERRUPT));
                            WO_VM_GOTO_HANDLE_INTERRUPT;
                            break;
                        }

                        for (uint16_t idx = 0; idx < opnum1->m_closure->m_closure_args_count; ++idx)
                            (sp--)->set_val(&opnum1->m_closure->m_closure_args[idx]);
                    }
                    else
                    {
                        if (sp <= stack_storage)
                        {
                            rt_ip = ip_for_rollback;
                            wo_assure(interrupt(vm_interrupt_type::STACK_OVERFLOW_INTERRUPT));
                            WO_VM_GOTO_HANDLE_INTERRUPT;
                            break;
                        }
                    }

                    switch (opnum1->m_type)
                    {
                    case value::valuetype::native_func_type:
                    {
                        /* Might be far call jit code. */
                        sp->m_type = value::valuetype::far_callstack;
                        sp->m_farcallstack = rt_ip;
                        sp->m_ext_farcallstack_bp = (uint32_t)(sb - bp);
                        bp = --sp;
                        auto rt_bp = sb - bp;

                        // Call native
                        const wo_extern_native_func_t call_aim_native_func = opnum1->m_native_func;
                        ip = reinterpret_cast<byte_t*>(call_aim_native_func);

                        switch (call_aim_native_func(
                            reinterpret_cast<wo_vm>(this),
                            std::launder(reinterpret_cast<wo_value>(sp + 2))))
                        {
                        case wo_result_t::WO_API_RESYNC_JIT_STATE_TO_VM_STATE:
                        {
                            bp = sb - rt_bp;

                            WO_VM_ASSERT((bp + 1)->m_type == value::valuetype::far_callstack,
                                "Found broken stack in 'call'.");

                            value* const stored_bp =
                                sb - (++bp)->m_ext_farcallstack_bp;

                            sp = bp;
                            bp = stored_bp;

                            WO_VM_INTERRUPT_CHECKPOINT_AND_GOTO_HANDLE_INTERRUPT;
                            break;
                        }
                        case wo_result_t::WO_API_NORMAL:
                        {
                            bp = sb - rt_bp;

                            WO_VM_ASSERT((bp + 1)->m_type == value::valuetype::far_callstack,
                                "Found broken stack in 'call'.");

                            value* const stored_bp =
                                sb - (++bp)->m_ext_farcallstack_bp;

                            sp = bp;
                            bp = stored_bp;
                            break;
                        }
                        case wo_result_t::WO_API_SYNC_CHANGED_VM_STATE:
                            rt_ip = this->ip;
                            if (rt_ip < near_rtcode_begin || rt_ip >= near_rtcode_end) [[unlikely]]
                            {
                                wo_assure(interrupt(vm_interrupt_type::CALL_FAR_RESYNC_VM_STATE_INTERRUPT));
                                WO_VM_GOTO_HANDLE_INTERRUPT;
                            }
                            else
                                WO_VM_INTERRUPT_CHECKPOINT_AND_GOTO_HANDLE_INTERRUPT;
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
                        const auto* aim_function_addr = opnum1->m_script_func;
                        if (aim_function_addr < near_rtcode_begin || aim_function_addr >= near_rtcode_end) [[unlikely]]
                        {
                            sp->m_type = value::valuetype::far_callstack;
                            sp->m_farcallstack = rt_ip;
                            sp->m_ext_farcallstack_bp = (uint32_t)(sb - bp);
                            bp = --sp;
                            wo_assure(interrupt(vm_interrupt_type::CALL_FAR_RESYNC_VM_STATE_INTERRUPT));

                            rt_ip = aim_function_addr;
                            WO_VM_GOTO_HANDLE_INTERRUPT;
                        }
                        else
                        {
                            sp->m_type = value::valuetype::callstack;
                            sp->m_vmcallstack.ret_ip = (uint32_t)(rt_ip - near_rtcode_begin);
                            sp->m_vmcallstack.bp = (uint32_t)(sb - bp);
                            bp = --sp;

                            rt_ip = aim_function_addr;
                            WO_VM_INTERRUPT_CHECKPOINT_AND_GOTO_HANDLE_INTERRUPT;
                        }
                        break;
                    }
                    case value::valuetype::closure_type:
                    {
                        auto* closure = opnum1->m_closure;

                        if (closure->m_native_call)
                        {
                            sp->m_type = value::valuetype::far_callstack;
                            sp->m_farcallstack = rt_ip;
                            sp->m_ext_farcallstack_bp = (uint32_t)(sb - bp);
                            bp = --sp;
                            auto rt_bp = sb - bp;

                            switch (closure->m_native_func(
                                reinterpret_cast<wo_vm>(this),
                                std::launder((reinterpret_cast<wo_value>(sp + 2)))))
                            {
                            case wo_result_t::WO_API_RESYNC_JIT_STATE_TO_VM_STATE:
                            {
                                bp = sb - rt_bp;

                                WO_VM_ASSERT((bp + 1)->m_type == value::valuetype::far_callstack,
                                    "Found broken stack in 'call'.");

                                value* const stored_bp = sb - (++bp)->m_ext_farcallstack_bp;
                                // Here to invoke jit closure, jit function cannot pop captured arguments,
                                // So we pop them here.
                                sp = bp + closure->m_closure_args_count;
                                bp = stored_bp;

                                WO_VM_INTERRUPT_CHECKPOINT_AND_GOTO_HANDLE_INTERRUPT;
                                break;
                            }
                            case wo_result_t::WO_API_NORMAL:
                            {
                                bp = sb - rt_bp;

                                WO_VM_ASSERT((bp + 1)->m_type == value::valuetype::far_callstack,
                                    "Found broken stack in 'call'.");

                                value* const stored_bp = sb - (++bp)->m_ext_farcallstack_bp;
                                // Here to invoke jit closure, jit function cannot pop captured arguments,
                                // So we pop them here.
                                sp = bp + closure->m_closure_args_count;
                                bp = stored_bp;
                                break;
                            }
                            case wo_result_t::WO_API_SYNC_CHANGED_VM_STATE:
                            {
                                rt_ip = this->ip;
                                if (rt_ip < near_rtcode_begin || rt_ip >= near_rtcode_end) [[unlikely]]
                                {
                                    wo_assure(interrupt(vm_interrupt_type::CALL_FAR_RESYNC_VM_STATE_INTERRUPT));
                                    WO_VM_GOTO_HANDLE_INTERRUPT;
                                }
                                else
                                    WO_VM_INTERRUPT_CHECKPOINT_AND_GOTO_HANDLE_INTERRUPT;
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
                            const auto* aim_function_addr = closure->m_vm_func;
                            if (aim_function_addr < near_rtcode_begin || aim_function_addr >= near_rtcode_end) [[unlikely]]
                            {
                                sp->m_type = value::valuetype::far_callstack;
                                sp->m_farcallstack = rt_ip;
                                sp->m_ext_farcallstack_bp = (uint32_t)(sb - bp);
                                bp = --sp;
                                wo_assure(interrupt(vm_interrupt_type::CALL_FAR_RESYNC_VM_STATE_INTERRUPT));

                                rt_ip = aim_function_addr;
                                WO_VM_GOTO_HANDLE_INTERRUPT;
                            }
                            else
                            {
                                sp->m_type = value::valuetype::callstack;
                                sp->m_vmcallstack.ret_ip = (uint32_t)(rt_ip - near_rtcode_begin);
                                sp->m_vmcallstack.bp = (uint32_t)(sb - bp);
                                bp = --sp;

                                rt_ip = aim_function_addr;
                                WO_VM_INTERRUPT_CHECKPOINT_AND_GOTO_HANDLE_INTERRUPT;
                            }
                        }
                        break;
                    }
                    default:
                        WO_VM_FAIL(WO_FAIL_CALL_FAIL, "Unexpected invoke target type in 'call'.");
                    }
                } while (0);
                break;
            case instruct::opcode::callnwo:
                if (sp <= stack_storage) [[unlikely]]
                {
                    --rt_ip;
                    wo_assure(interrupt(vm_interrupt_type::STACK_OVERFLOW_INTERRUPT));
                    WO_VM_GOTO_HANDLE_INTERRUPT;
                }
                else
                {
                    const byte_t* aimplace = near_rtcode_begin + WO_IPVAL_MOVE_4;

                    sp->m_type = value::valuetype::callstack;
                    sp->m_vmcallstack.ret_ip = static_cast<uint32_t>(rt_ip - near_rtcode_begin) + 4 /* skip 4 byte gap. */;
                    sp->m_vmcallstack.bp = static_cast<uint32_t>(sb - bp);
                    bp = --sp;

                    rt_ip = aimplace;
                    WO_VM_INTERRUPT_CHECKPOINT_AND_GOTO_HANDLE_INTERRUPT;
                }
                break;
            case instruct::opcode::callnjit:
                if (sp <= stack_storage)
                {
                    --rt_ip;
                    wo_assure(interrupt(vm_interrupt_type::STACK_OVERFLOW_INTERRUPT));
                    WO_VM_INTERRUPT_CHECKPOINT_AND_GOTO_HANDLE_INTERRUPT;
                }
                else
                {
                    wo_extern_native_func_t call_aim_native_func =
                        reinterpret_cast<wo_extern_native_func_t>(WO_IPVAL_MOVE_8);

                    sp->m_type = value::valuetype::callstack;
                    sp->m_vmcallstack.ret_ip = (uint32_t)(rt_ip - near_rtcode_begin);
                    sp->m_vmcallstack.bp = (uint32_t)(sb - bp);

                    // Donot store or update ip/sp/bp. jit function will not use them.
                    auto* const rt_sp = sp;

                    switch (call_aim_native_func(
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
                        rt_ip = this->ip;
                        WO_VM_INTERRUPT_CHECKPOINT_AND_GOTO_HANDLE_INTERRUPT;
                        break;
                    default:
#if WO_ENABLE_RUNTIME_CHECK
                        WO_VM_FAIL(WO_FAIL_TYPE_FAIL, "Bad native function sync state.");
#endif
                        break;
                    }
                }
                break;
            case instruct::opcode::callnfp:
                if (sp <= stack_storage) [[unlikely]]
                {
                    --rt_ip;
                    wo_assure(interrupt(vm_interrupt_type::STACK_OVERFLOW_INTERRUPT));
                    WO_VM_GOTO_HANDLE_INTERRUPT;
                }
                else
                {
                    wo_extern_native_func_t call_aim_native_func =
                        reinterpret_cast<wo_extern_native_func_t>(WO_IPVAL_MOVE_8);

                    // For debugging purposes, native calls use far callstack to preserve 
                    // the complete return address.
                    // Since native calls do not use the `ret` instruction to return, there 
                    // is no performance penalty
                    sp->m_type = value::valuetype::far_callstack;
                    sp->m_farcallstack = rt_ip;
                    sp->m_ext_farcallstack_bp = (uint32_t)(sb - bp);
                    bp = --sp;

                    auto rt_bp = sb - bp;
                    ip = reinterpret_cast<const byte_t*>(call_aim_native_func);

                    const auto extern_func_result =
                        call_aim_native_func(
                            reinterpret_cast<wo_vm>(this),
                            std::launder(reinterpret_cast<wo_value>(sp + 2)));

                    bp = sb - rt_bp;

                    WO_VM_ASSERT((bp + 1)->m_type == value::valuetype::far_callstack,
                        "Found broken stack in 'callnfp'.");
                    value* stored_bp = sb - (++bp)->m_ext_farcallstack_bp;
                    sp = bp;
                    bp = stored_bp;

                    if (extern_func_result == wo_result_t::WO_API_RESYNC_JIT_STATE_TO_VM_STATE)
                        WO_VM_INTERRUPT_CHECKPOINT_AND_GOTO_HANDLE_INTERRUPT;
#if WO_ENABLE_RUNTIME_CHECK
                    else if (extern_func_result != wo_result_t::WO_API_NORMAL)
                        WO_VM_FAIL(WO_FAIL_TYPE_FAIL, "Bad native function sync state.");
#endif
                }
                break;
            case instruct::opcode::jmp:
                rt_ip = near_rtcode_begin + WO_IPVAL_MOVE_4;
                break;
            case instruct::opcode::jmpt:
            {
                const uint32_t aimplace = WO_IPVAL_MOVE_4;
                if (rt_cr->m_value_field)
                    rt_ip = near_rtcode_begin + aimplace;

                break;
            }
            case instruct::opcode::jmpf:
            {
                const uint32_t aimplace = WO_IPVAL_MOVE_4;
                if (!rt_cr->m_value_field)
                    rt_ip = near_rtcode_begin + aimplace;

                break;
            }
            case instruct::opcode::jmpgc:
                rt_ip = near_rtcode_begin + WO_IPVAL_MOVE_4;
                WO_VM_INTERRUPT_CHECKPOINT_AND_GOTO_HANDLE_INTERRUPT;
                break;
            case instruct::opcode::jmpgct:
            {
                const uint32_t aimplace = WO_IPVAL_MOVE_4;
                if (rt_cr->m_value_field)
                {
                    rt_ip = near_rtcode_begin + aimplace;
                    WO_VM_INTERRUPT_CHECKPOINT_AND_GOTO_HANDLE_INTERRUPT;
                }
                break;
            }
            case instruct::opcode::jmpgcf:
            {
                const uint32_t aimplace = WO_IPVAL_MOVE_4;
                if (!rt_cr->m_value_field)
                {
                    rt_ip = near_rtcode_begin + aimplace;
                    WO_VM_INTERRUPT_CHECKPOINT_AND_GOTO_HANDLE_INTERRUPT;
                }
                break;
            }
            case instruct::opcode::mkstructg:
                WO_ADDRESSING_G1;
                WO_WRITE_CHECK_FOR_GLOBAL(opnum1);
                goto _label_mkstruct_impl;
            case instruct::opcode::mkstructs:
                WO_ADDRESSING_RS1; // Aim
            _label_mkstruct_impl:
                sp = make_struct_impl(opnum1, WO_IPVAL_MOVE_2, sp);
                break;
            case WO_RSG_ADDRESSING_WRITE_OP1_CASE(idstruct):
            {
                const uint16_t offset = WO_IPVAL_MOVE_2;

                WO_VM_ASSERT(opnum2->m_type == value::valuetype::struct_type,
                    "Cannot index non-struct value in 'idstruct'.");
                WO_VM_ASSERT(opnum2->m_structure != nullptr,
                    "Unable to index null in 'idstruct'.");
                WO_VM_ASSERT(offset < opnum2->m_structure->m_count,
                    "Index out of range in 'idstruct'.");

                gcbase::gc_read_guard gwg1(opnum2->m_structure);
                opnum1->set_val(&opnum2->m_structure->m_values[offset]);

                break;
            }
            case instruct::opcode::jnequbg:
                WO_ADDRESSING_G1;
                goto _label_jnequb_impl;
            case instruct::opcode::jnequbs:
                WO_ADDRESSING_RS1;
            _label_jnequb_impl:
                {
                    const uint32_t offset = WO_IPVAL_MOVE_4;
                    if (opnum1->m_integer != rt_cr->m_integer)
                        rt_ip = near_rtcode_begin + offset;
                    break;
                }
            case instruct::opcode::jnequbgcg:
                WO_ADDRESSING_G1;
                goto _label_jnequbgc_impl;
            case instruct::opcode::jnequbgcs:
                WO_ADDRESSING_RS1;
            _label_jnequbgc_impl:
                {
                    const uint32_t offset = WO_IPVAL_MOVE_4;
                    if (opnum1->m_integer != rt_cr->m_integer)
                    {
                        rt_ip = near_rtcode_begin + offset;
                        WO_VM_INTERRUPT_CHECKPOINT_AND_GOTO_HANDLE_INTERRUPT;
                    }
                    break;
                }
            case instruct::opcode::mkarrg:
                WO_ADDRESSING_G1;
                WO_WRITE_CHECK_FOR_GLOBAL(opnum1);
                goto _label_mkarr_impl;
            case instruct::opcode::mkarrs:
                WO_ADDRESSING_RS1;
            _label_mkarr_impl:
                {
                    const uint16_t size = WO_IPVAL_MOVE_2;

                    sp = make_array_impl(opnum1, size, sp);
                    break;
                }
            case instruct::opcode::mkmapg:
                WO_ADDRESSING_G1;
                WO_WRITE_CHECK_FOR_GLOBAL(opnum1);
                goto _label_mkmap_impl;
            case instruct::opcode::mkmaps:
                WO_ADDRESSING_RS1;
            _label_mkmap_impl:
                {
                    const uint16_t size = WO_IPVAL_MOVE_2;

                    sp = make_map_impl(opnum1, size, sp);
                    break;
                }
            case WO_RSG_ADDRESSING_CASE(idarr):
            {
                WO_VM_ASSERT(nullptr != opnum1->m_gcunit,
                    "Unable to index null in 'idarr'.");
                WO_VM_ASSERT(opnum1->m_type == value::valuetype::array_type,
                    "Cannot index non-array value in 'idarr'.");
                WO_VM_ASSERT(opnum2->m_type == value::valuetype::integer_type,
                    "Cannot index array by non-integer value in 'idarr'.");
                gcbase::gc_read_guard gwg1(opnum1->m_gcunit);

                // ATTENTION: `_vmjitcall_idarr` HAS SAME LOGIC, NEED UPDATE SAME TIME.
                const size_t index = static_cast<size_t>(opnum2->m_integer);

                if (index >= opnum1->m_array->size())
                    WO_VM_FAIL(WO_FAIL_INDEX_FAIL, "Index out of range.");
                else
                    rt_cr->set_val(&opnum1->m_array->at(index));

                break;
            }
            case WO_RSG_ADDRESSING_CASE(iddict):
            {
                WO_VM_ASSERT(nullptr != opnum1->m_gcunit,
                    "Unable to index null in 'iddict'.");
                WO_VM_ASSERT(opnum1->m_type == value::valuetype::dict_type,
                    "Unable to index non-dict value in 'iddict'.");

                gcbase::gc_read_guard gwg1(opnum1->m_gcunit);

                auto fnd = opnum1->m_dictionary->find(*opnum2);
                if (fnd != opnum1->m_dictionary->end())
                    rt_cr->set_val(&fnd->second);
                else
                    WO_VM_FAIL(WO_FAIL_INDEX_FAIL,
                        "No such key in current dict.");

                break;
            }
            case WO_RSG_ADDRESSING_CASE(sidmap):
                WO_ADDRESSING_RS3;
                {
                    WO_VM_ASSERT(nullptr != opnum1->m_gcunit,
                        "Unable to index null in 'sidmap'.");
                    WO_VM_ASSERT(opnum1->m_type == value::valuetype::dict_type,
                        "Unable to index non-map value in 'sidmap'.");

                    gcbase::gc_modify_write_guard gwg1(opnum1->m_gcunit);

                    auto* result = &(*opnum1->m_dictionary)[*opnum2];
                    if (wo::gc::gc_is_marking())
                        wo::value::write_barrier(result);
                    result->set_val(opnum3);

                    break;
                }
            case WO_RSG_ADDRESSING_CASE(siddict):
                WO_ADDRESSING_RS3;
                {
                    WO_VM_ASSERT(nullptr != opnum1->m_gcunit,
                        "Unable to index null in 'siddict'.");
                    WO_VM_ASSERT(opnum1->m_type == value::valuetype::dict_type,
                        "Unable to index non-dict value in 'siddict'.");

                    gcbase::gc_write_guard gwg1(opnum1->m_gcunit);

                    auto fnd = opnum1->m_dictionary->find(*opnum2);
                    if (fnd != opnum1->m_dictionary->end())
                    {
                        auto* result = &fnd->second;
                        if (wo::gc::gc_is_marking())
                            wo::value::write_barrier(result);
                        result->set_val(opnum3);
                        break;
                    }
                    else
                        WO_VM_FAIL(WO_FAIL_INDEX_FAIL, "No such key in current dict.");

                    break;
                }
            case WO_RSG_ADDRESSING_CASE(sidarr):
                WO_ADDRESSING_RS3;
                {
                    WO_VM_ASSERT(nullptr != opnum1->m_gcunit,
                        "Unable to index null in 'sidarr'.");
                    WO_VM_ASSERT(opnum1->m_type == value::valuetype::array_type,
                        "Unable to index non-array value in 'sidarr'.");
                    WO_VM_ASSERT(opnum2->m_type == value::valuetype::integer_type,
                        "Unable to index array by non-integer value in 'sidarr'.");

                    gcbase::gc_write_guard gwg1(opnum1->m_gcunit);

                    size_t index = (size_t)opnum2->m_integer;
                    if (index >= opnum1->m_array->size())
                    {
                        WO_VM_FAIL(WO_FAIL_INDEX_FAIL, "Index out of range.");
                    }
                    else
                    {
                        auto* result = &opnum1->m_array->at(index);
                        if (wo::gc::gc_is_marking())
                            wo::value::write_barrier(result);
                        result->set_val(opnum3);
                    }
                    break;
                }
            case WO_RSG_ADDRESSING_CASE(sidstruct):
            {
                const uint16_t offset = WO_IPVAL_MOVE_2;

                WO_VM_ASSERT(nullptr != opnum1->m_structure,
                    "Unable to index null in 'sidstruct'.");
                WO_VM_ASSERT(opnum1->m_type == value::valuetype::struct_type,
                    "Unable to index non-struct value in 'sidstruct'.");
                WO_VM_ASSERT(offset < opnum1->m_structure->m_count,
                    "Index out of range in 'sidstruct'.");

                gcbase::gc_write_guard gwg1(opnum1->m_gcunit);

                auto* result = &opnum1->m_structure->m_values[offset];
                if (wo::gc::gc_is_marking())
                    wo::value::write_barrier(result);
                result->set_val(opnum2);

                break;
            }
            case WO_RSG_ADDRESSING_CASE(idstr):
                WO_VM_ASSERT(nullptr != opnum1->m_gcunit,
                    "Unable to index null in 'idstr'.");

                WO_VM_ASSERT(opnum2->m_type == value::valuetype::integer_type,
                    "Unable to index string by non-integer value in 'idstr'.");

                rt_cr->set_integer(
                    static_cast<wo_integer_t>(
                        wo_strn_get_char(
                            opnum1->m_string->c_str(),
                            opnum1->m_string->size(),
                            static_cast<size_t>(opnum2->m_integer))));
                break;
            case WO_RSG_ADDRESSING_WRITE_OP1_CASE(mkunion):
                make_union_impl(opnum1, opnum2, WO_IPVAL_MOVE_2);
                break;
            case instruct::opcode::mkcloswo:
            {
                const uint16_t closure_arg_count = WO_IPVAL_MOVE_2;
                const byte_t* script_function = near_rtcode_begin + WO_IPVAL_MOVE_4;
                rt_ip += 4; /* skip 4 byte gap */

                sp = make_closure_wo_impl(
                    rt_cr, closure_arg_count, script_function, sp);

                break;
            }
            case instruct::opcode::mkclosfp:
            {
                const uint16_t closure_arg_count = WO_IPVAL_MOVE_2;
                wo_native_func_t native_function =
                    reinterpret_cast<wo_native_func_t>(
                        static_cast<intptr_t>(WO_IPVAL_MOVE_8));

                sp = make_closure_fp_impl(
                    rt_cr, closure_arg_count, native_function, sp);

                break;
            }
            case WO_RSG_ADDRESSING_CASE(movicas):
            {
                WO_ADDRESSING_RS3;

                if constexpr (std::atomic<wo_integer_t>::is_always_lock_free
                    && sizeof(std::atomic<wo_integer_t>) == sizeof(wo_integer_t))
                {
                    std::atomic<wo_integer_t>* target_atomic =
                        std::launder(
                            reinterpret_cast<std::atomic<wo_integer_t>*>(
                                &opnum1->m_integer));

                    cr->set_bool(
                        target_atomic->compare_exchange_strong(
                            opnum3->m_integer,
                            opnum2->m_integer));
                }
                else
                {
                    static gcbase::_shared_spin _movicas_lock;

                    std::lock_guard g(_movicas_lock);
                    if (opnum1->m_integer == opnum3->m_integer)
                    {
                        opnum1->m_integer = opnum2->m_integer;
                        cr->set_bool(true);
                    }
                    else
                    {
                        opnum3->m_integer = opnum1->m_integer;
                        cr->set_bool(false);
                    }
                }
                break;
            }
            case instruct::unpackg:
                ip_for_rollback = rt_ip - 1;
                WO_ADDRESSING_G1;
                goto _label_unpack_impl;
            case instruct::unpacks:
                ip_for_rollback = rt_ip - 1;
                WO_ADDRESSING_RS1;
            _label_unpack_impl:
                {
                    const uint32_t unpack_argc_unsigned = WO_IPVAL_MOVE_4;

                    auto* new_sp = unpackargs_impl(
                        this, opnum1,
                        *reinterpret_cast<const int32_t*>(&unpack_argc_unsigned),
                        tc, rt_ip, sp, bp);

                    if (new_sp == nullptr) [[unlikely]]
                    {
                        // STACK_OVERFLOW_INTERRUPT set, rollback and handle the interrupt.
                        rt_ip = ip_for_rollback;
                        WO_VM_GOTO_HANDLE_INTERRUPT;
                    }
                    else
                        sp = new_sp;

                    break;
                }
            case instruct::opcode::ext0:
                switch (*(rt_ip++))
                {
                case instruct::extern_opcode_page_0::panicg:
                    WO_ADDRESSING_G1;
                    goto _label_ext0_panic_impl;
                case instruct::extern_opcode_page_0::panics:
                    WO_ADDRESSING_RS1; // data
                _label_ext0_panic_impl:
                    ip = rt_ip;
                    WO_VM_FAIL(WO_FAIL_UNEXPECTED,
                        "%s", wo_cast_string(std::launder(reinterpret_cast<wo_value>(opnum1))));
                    break;
                case instruct::extern_opcode_page_0::packg:
                    WO_ADDRESSING_G1;
                    WO_WRITE_CHECK_FOR_GLOBAL(opnum1);
                    goto _label_ext0_pack_impl;
                case instruct::extern_opcode_page_0::packs:
                    WO_ADDRESSING_RS1;
                _label_ext0_pack_impl:
                    {
                        const uint16_t this_function_arg_count = WO_IPVAL_MOVE_2;
                        const uint16_t skip_closure_arg_count = WO_IPVAL_MOVE_2;

                        packargs_impl(
                            opnum1,
                            this_function_arg_count,
                            tp,
                            bp,
                            skip_closure_arg_count);
                        break;
                    }
                case WO_RSG_ADDRESSING_EXT_CASE(cdivilr):
                    if (opnum2->m_integer == 0)
                        WO_VM_FAIL(WO_FAIL_UNEXPECTED, "The divisor cannot be 0.");
                    else if (opnum2->m_integer == -1 && opnum1->m_integer == INT64_MIN)
                        WO_VM_FAIL(WO_FAIL_UNEXPECTED, "Division overflow.");
                    break;
                case instruct::extern_opcode_page_0::cdivilg:
                    WO_ADDRESSING_G1;
                    goto _label_ext0_cdivil_impl;
                case instruct::extern_opcode_page_0::cdivils:
                    WO_ADDRESSING_RS1;
                _label_ext0_cdivil_impl:
                    if (opnum1->m_integer == INT64_MIN)
                        WO_VM_FAIL(WO_FAIL_UNEXPECTED, "Division overflow.");
                    break;
                case instruct::extern_opcode_page_0::cdivirzg:
                    WO_ADDRESSING_G1;
                    goto _label_ext0_cdivirz_impl;
                case instruct::extern_opcode_page_0::cdivirzs:
                    WO_ADDRESSING_RS1;
                _label_ext0_cdivirz_impl:
                    if (opnum1->m_integer == 0)
                        WO_VM_FAIL(WO_FAIL_UNEXPECTED, "The divisor cannot be 0.");
                    break;
                case instruct::extern_opcode_page_0::cdivirg:
                    WO_ADDRESSING_G1;
                    goto _label_ext0_cdivir_impl;
                case instruct::extern_opcode_page_0::cdivirs:
                    WO_ADDRESSING_RS1;
                _label_ext0_cdivir_impl:
                    if (opnum1->m_integer == 0)
                        WO_VM_FAIL(WO_FAIL_UNEXPECTED, "The divisor cannot be 0.");
                    else if (opnum1->m_integer == -1)
                        WO_VM_FAIL(WO_FAIL_UNEXPECTED, "Division overflow.");
                    break;
                case instruct::extern_opcode_page_0::popng:
                    WO_ADDRESSING_G1;
                    goto _label_ext0_popn_impl;
                case instruct::extern_opcode_page_0::popns:
                    WO_ADDRESSING_RS1;
                _label_ext0_popn_impl:
                    sp += opnum1->m_integer;

                    // Check if stack is overflow.
                    wo_assert(sp <= bp);
                    break;
                case WO_RSG_ADDRESSING_EXT_CASE(addh):
                    WO_VM_ASSERT(opnum1->m_type == opnum2->m_type
                        && opnum1->m_type == value::valuetype::handle_type,
                        "Operand should be handle in 'addh'.");

                    opnum1->m_handle += opnum2->m_handle;
                    break;
                case WO_RSG_ADDRESSING_EXT_CASE(subh):
                    WO_VM_ASSERT(opnum1->m_type == opnum2->m_type
                        && opnum1->m_type == value::valuetype::handle_type,
                        "Operand should be handle in 'subh'.");

                    opnum1->m_handle -= opnum2->m_handle;
                    break;
                case WO_RSG_ADDRESSING_EXT_CASE(lth):
                    WO_VM_ASSERT(opnum1->m_type == value::valuetype::handle_type
                        && opnum2->m_type == value::valuetype::handle_type,
                        "Operand should be handle in 'lth'.");
                    rt_cr->set_bool(opnum1->m_handle < opnum2->m_handle);
                    break;
                case WO_RSG_ADDRESSING_EXT_CASE(gth):
                    WO_VM_ASSERT(opnum1->m_type == value::valuetype::handle_type
                        && opnum2->m_type == value::valuetype::handle_type,
                        "Operand should be handle in 'gth'.");
                    rt_cr->set_bool(opnum1->m_handle > opnum2->m_handle);
                    break;
                case WO_RSG_ADDRESSING_EXT_CASE(elth):
                    WO_VM_ASSERT(opnum1->m_type == value::valuetype::handle_type
                        && opnum2->m_type == value::valuetype::handle_type,
                        "Operand should be handle in 'elth'.");
                    rt_cr->set_bool(opnum1->m_handle <= opnum2->m_handle);
                    break;
                case WO_RSG_ADDRESSING_EXT_CASE(egth):
                    WO_VM_ASSERT(opnum1->m_type == value::valuetype::handle_type
                        && opnum2->m_type == value::valuetype::handle_type,
                        "Operand should be handle in 'egth'.");
                    rt_cr->set_bool(opnum1->m_handle >= opnum2->m_handle);
                    break;
                default:
                    wo_unreachable("Bad ir.");
                    break;
                }
                break;
            case instruct::opcode::ext1:
            case instruct::opcode::ext2:
            case instruct::opcode::ext3:
                wo_unreachable("Bad ir.");
                break;
            case instruct::opcode::nop3:
                ++rt_ip;
                [[fallthrough]];
            case instruct::opcode::nop2:
                ++rt_ip;
                [[fallthrough]];
            case instruct::opcode::nop1:
                ++rt_ip;
                [[fallthrough]];
            case instruct::opcode::nop0:
                break;
            case instruct::opcode::end:
                // END.
                WO_VM_RETURN(wo_result_t::WO_API_NORMAL);
            case instruct::opcode::abrt:
                wo_error("executed 'abrt'.");
                break;
            default:
                wo_unreachable("Bad ir.");
            }
            if (0)
            {
            _label_vm_handle_interrupt:
                const auto interrupt_state =
                    vm_interrupt.load(std::memory_order_acquire);

                if (vm_interrupt_type::NOTHING == interrupt_state)
                {
                    // Debugee datached.
                    debuggee_attached = false;
                    goto _label_vm_re_entry;
                }

                ///////////////////////////////////////////////////////////////////////
                if (interrupt_state & vm_interrupt_type::GC_INTERRUPT)
                {
                    gc_checkpoint_self_mark();
                }
                else if (interrupt_state & vm_interrupt_type::GC_HANGUP_INTERRUPT)
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
                    debuggee_attached = true;
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

                    goto _label_vm_re_entry;
                }
                else
                {
                    wo_unreachable("Unknown interrupt.");
                }

                WO_VM_INTERRUPT_CHECKPOINT_AND_GOTO_HANDLE_INTERRUPT;
            }
        }// vm loop end.

        WO_VM_RETURN(wo_result_t::WO_API_NORMAL);

#undef WO_VM_FAIL
#undef WO_VM_ASSERT
#undef WO_VM_INTERRUPT_CHECKPOINT_AND_GOTO_HANDLE_INTERRUPT
#undef WO_VM_GOTO_HANDLE_INTERRUPT
#undef WO_RSG_ADDRESSING_WRITE_OP1_CASE
#undef WO_WRITE_CHECK_FOR_GLOBAL
#undef WO_RSG_ADDRESSING_CASE
    }

#undef WO_VM_RETURN
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

#undef WO_SAFE_READ_OFFSET_GET_QWORD
#undef WO_SAFE_READ_OFFSET_GET_DWORD
#undef WO_SAFE_READ_OFFSET_GET_WORD

#undef WO_SAFE_READ_OFFSET_PER_BYTE
#undef WO_IS_ODD_IRPTR

#undef WO_SAFE_READ_MOVE_2
#undef WO_SAFE_READ_MOVE_4
#undef WO_SAFE_READ_MOVE_8

#undef WO_FAST_READ_MOVE_2
#undef WO_FAST_READ_MOVE_4
#undef WO_FAST_READ_MOVE_8
}
