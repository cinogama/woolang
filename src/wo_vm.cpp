#include "wo_vm.hpp"

namespace wo
{
    const value value::TAKEPLACE = *value().set_takeplace();

    void vm_debuggee_bridge_base::_vm_invoke_debuggee(vmbase* _vm)
    {
        std::lock_guard _(_debug_entry_guard_block_mx);
        wo_assert(_vm->check_interrupt(vmbase::vm_interrupt_type::LEAVE_INTERRUPT));

        // Just make a block
        debug_interrupt(_vm);
    }

    void vmbase::inc_destructable_instance_count() noexcept
    {
        wo_assert(env != nullptr);
#if WO_ENABLE_RUNTIME_CHECK
        size_t old_count =
#endif
            env->_created_destructable_instance_count.fetch_add(1, std::memory_order::memory_order_relaxed);
        wo_assert(old_count >= 0);
    }
    void vmbase::dec_destructable_instance_count() noexcept
    {
        wo_assert(env != nullptr);
#if WO_ENABLE_RUNTIME_CHECK
        size_t old_count =
#endif
            env->_created_destructable_instance_count.fetch_sub(1, std::memory_order::memory_order_relaxed);
        wo_assert(old_count > 0);
    }

    vm_debuggee_bridge_base* vmbase::attach_debuggee(vm_debuggee_bridge_base* dbg) noexcept
    {
        std::shared_lock g1(_alive_vm_list_mx);

        // Remove old debuggee
        for (auto* vm_instance : _alive_vm_list)
            if (vm_instance->virtual_machine_type != vmbase::vm_type::GC_DESTRUCTOR)
                vm_instance->interrupt(vm_interrupt_type::DETACH_DEBUGGEE_INTERRUPT);
        for (auto* vm_instance : _alive_vm_list)
            if (vm_instance->virtual_machine_type != vmbase::vm_type::GC_DESTRUCTOR)
                vm_instance->wait_interrupt(vm_interrupt_type::DETACH_DEBUGGEE_INTERRUPT, false);

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
    vm_debuggee_bridge_base* vmbase::current_debuggee()noexcept
    {
        return attaching_debuggee;
    }
    bool vmbase::is_aborted() const noexcept
    {
        return vm_interrupt.load(std::memory_order::memory_order_acquire) & vm_interrupt_type::ABORT_INTERRUPT;
    }
    bool vmbase::interrupt(vm_interrupt_type type)noexcept
    {
        return !(type & vm_interrupt.fetch_or(type, std::memory_order::memory_order_acq_rel));
    }
    bool vmbase::clear_interrupt(vm_interrupt_type type)noexcept
    {
        return type & vm_interrupt.fetch_and(~type, std::memory_order::memory_order_acq_rel);
    }

    bool vmbase::check_interrupt(vm_interrupt_type type)noexcept
    {
        return 0 != (vm_interrupt.load(std::memory_order::memory_order_acquire) & type);
    }
    vmbase::interrupt_wait_result vmbase::wait_interrupt(vm_interrupt_type type, bool force_wait)noexcept
    {
        using namespace std;
        size_t retry_count = 0;

        bool warning_raised = false;

        constexpr int MAX_TRY_COUNT = 0;
        int i = 0;
        do
        {
            uint32_t vm_interrupt_mask = vm_interrupt.load(std::memory_order::memory_order_acquire);

            if (0 == (vm_interrupt_mask & type))
                break;

            if (vm_interrupt_mask & vm_interrupt_type::LEAVE_INTERRUPT)
            {
                if (++i > MAX_TRY_COUNT)
                    return interrupt_wait_result::LEAVED;
            }
            else
                i = 0;

            if (force_wait)
            {
                std::this_thread::sleep_for(10ms);
                if (!warning_raised && ++retry_count == config::INTERRUPT_CHECK_TIME_LIMIT)
                {
                    // Wait for too much time.
                    std::string warning_info = "Wait for too much time for waiting interrupt.\n";
                    std::stringstream dump_callstack_info;

                    dump_call_stack(32, false, dump_callstack_info);
                    warning_info += dump_callstack_info.str();
                    wo_warning(warning_info.c_str());

                    warning_raised = true;
                }
            }
            else
                return interrupt_wait_result::TIMEOUT;
        } while (true);

        return interrupt_wait_result::ACCEPT;
    }
    void vmbase::block_interrupt(vm_interrupt_type type)noexcept
    {
        using namespace std;

        while (vm_interrupt.load(std::memory_order::memory_order_acquire) & type)
            std::this_thread::sleep_for(10ms);
    }

    void vmbase::hangup()noexcept
    {
        do
        {
            std::lock_guard g1(_vm_hang_mx);
            _vm_hang_flag.fetch_sub(1);
        } while (0);

        std::unique_lock ug1(_vm_hang_mx);
        _vm_hang_cv.wait(ug1, [this]() {return _vm_hang_flag >= 0; });
    }
    void vmbase::wakeup()noexcept
    {
        do
        {
            std::lock_guard g1(_vm_hang_mx);
            _vm_hang_flag.fetch_add(1);
        } while (0);

        _vm_hang_cv.notify_one();
    }

    vmbase::vmbase(vm_type type) noexcept
        : virtual_machine_type(type)
        , cr(nullptr)
        , tc(nullptr)
        , tp(nullptr)
        , sp(nullptr)
        , bp(nullptr)
        , register_mem_begin(nullptr)
        , const_global_begin(nullptr)
        , stack_mem_begin(nullptr)
        , _self_stack_mem_buf(nullptr)
        , _self_register_mem_buf(nullptr)
        , stack_size(0)
        , gc_vm(nullptr)
        , compile_info(nullptr)
        , ip(nullptr)
        , env(nullptr)
        , _vm_hang_flag(0)
#if WO_ENABLE_RUNTIME_CHECK
        // runtime information
        , attaching_thread_id(std::thread::id{})
#endif        
        , jit_function_call_depth(0)
        , shrink_stack_advise(0)
        , shrink_stack_edge(VM_SHRINK_STACK_COUNT)
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
    vmbase::~vmbase()
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

            free(_self_register_mem_buf);
            free(_self_stack_mem_buf);

        } while (0);

        if (env)
            --env->_running_on_vm_count;

        --_alive_vm_count_for_gc_vm_destruct;
    }

    vmbase* vmbase::get_or_alloc_gcvm() const noexcept
    {
        // TODO: GC will mark globle space when current vm is gc vm, we cannot do it now!
        return gc_vm;
    }
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
        _self_register_mem_buf = std::launder(reinterpret_cast<value*>(calloc(regcount, sizeof(value))));
        register_mem_begin = _self_register_mem_buf;
        cr = register_mem_begin + opnum::reg::spreg::cr;
        tc = register_mem_begin + opnum::reg::spreg::tc;
        tp = register_mem_begin + opnum::reg::spreg::tp;
    }
    void vmbase::_allocate_stack_space(size_t stacksz)noexcept
    {
        stack_size = stacksz;

        _self_stack_mem_buf = std::launder(reinterpret_cast<value*>(calloc(stacksz, sizeof(value))));
        stack_mem_begin = _self_stack_mem_buf + stacksz - 1;
        sp = bp = stack_mem_begin;
    }
    bool vmbase::_reallocate_stack_space(size_t stacksz) noexcept
    {
        wo_assert(stacksz != 0);
        stack_size = stacksz;

        const size_t used_stack_size = stack_mem_begin - sp;
        if (used_stack_size * 2 > stacksz)
            // New stack size is smaller than current stack size
            return false;

        if (stacksz > VM_MAX_STACK_SIZE)
            // Out of limit.
            return false;

        value* new_stack_buf = std::launder(reinterpret_cast<value*>(calloc(stacksz, sizeof(value))));
        if (new_stack_buf == nullptr)
            // Failed to allocate new stack space
            return false;

        value* new_stack_mem_begin = new_stack_buf + stacksz - 1;
        value* new_sp = new_stack_buf + stacksz - 1 - used_stack_size;
        const size_t bp_sp_offset = (size_t)(bp - sp);

        memcpy(new_sp + 1, sp + 1, used_stack_size * sizeof(value));
        free(_self_stack_mem_buf);

        _self_stack_mem_buf = new_stack_buf;
        stack_mem_begin = new_stack_mem_begin;
        sp = new_sp;
        bp = sp + bp_sp_offset;

        return true;
    }
    void vmbase::set_runtime(shared_pointer<runtime_env> runtime_environment)noexcept
    {
        wo_assure(wo_enter_gcguard(std::launder(reinterpret_cast<wo_vm>(this))));

        env = runtime_environment;
        ++env->_running_on_vm_count;

        const_global_begin = env->constant_global;
        ip = env->rt_codes;

        _allocate_stack_space(VM_DEFAULT_STACK_SIZE);
        _allocate_register_space(env->real_register_count);

        do
        {
            std::lock_guard g1(_alive_vm_list_mx);
            _gc_ready_vm_list.insert(this);
        } while (false);

        wo_assure(wo_leave_gcguard(std::launder(reinterpret_cast<wo_vm>(this))));

        // Create a new VM using for GC destruct
        auto* created_subvm_for_gc = make_machine(vm_type::GC_DESTRUCTOR);

        gc_vm = created_subvm_for_gc->gc_vm = created_subvm_for_gc;
    }
    vmbase* vmbase::make_machine(vm_type type) const noexcept
    {
        wo_assert(env != nullptr);

        vmbase* new_vm = create_machine(type);
        new_vm->gc_vm = get_or_alloc_gcvm();

        new_vm->env = env;  // env setted, gc will scan this vm..
        ++env->_running_on_vm_count;

        wo_assure(wo_enter_gcguard(std::launder(reinterpret_cast<wo_vm>(new_vm))));
        new_vm->const_global_begin = const_global_begin;

        new_vm->ip = env->rt_codes;
        new_vm->_allocate_stack_space(VM_DEFAULT_STACK_SIZE);
        new_vm->_allocate_register_space(env->real_register_count);

        do
        {
            std::lock_guard g1(_alive_vm_list_mx);
            _gc_ready_vm_list.insert(new_vm);
        } while (false);

        wo_assure(wo_leave_gcguard(std::launder(reinterpret_cast<wo_vm>(new_vm))));
        return new_vm;
    }
    void vmbase::dump_program_bin(size_t begin, size_t end, std::ostream& os) const noexcept
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
                        << " : " << wo_type_name((wo_type_t)env->constant_global[data_4b].type);
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
                        << " : " << wo_type_name((wo_type_t)env->constant_global[data_4b].type);
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
                tmpos << wo_type_name((wo_type_t) * (this_command_ptr++));

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
                tmpos << wo_type_name((wo_type_t) * (this_command_ptr++));

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

                        auto this_func_argc = *(uint16_t*)((this_command_ptr += 2) - 2);
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
                    case instruct::extern_opcode_page_0::popn:
                        tmpos << "popn\t";  print_opnum1();
                        break;
                    default:
                        tmpos << "??\t";
                        break;
                    }
                    break;
                case 3:
                    tmpos << "flag ";
                    switch (main_command & 0b11111100)
                    {
                    case instruct::extern_opcode_page_3::funcbegin:
                        tmpos << "funcbegin\t";
                        break;
                    case instruct::extern_opcode_page_3::funcend:
                        tmpos << "funcend\t";
                        break;
                    default:
                        tmpos << "??\t";
                        break;
                    }
                    break;
                case 1:
                case 2:
                default:
                    tmpos << "??\t";
                    break;
                }
                break;
            }
            default:
                tmpos << "??\t";
                break;
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

    void vmbase::dump_call_stack(size_t max_count, bool need_offset, std::ostream& os)const noexcept
    {
        bool trace_finished;
        auto callstacks = dump_call_stack_func_info(max_count, need_offset, &trace_finished);
        for (auto idx = callstacks.cbegin(); idx != callstacks.cend(); ++idx)
        {
            os << (idx - callstacks.cbegin()) << ": " << idx->m_func_name << std::endl;

            os << "\t\t-- at " << idx->m_file_path;
            if (!idx->m_is_extern)
                os << " (" << idx->m_row + 1 << ", " << idx->m_col + 1 << ")";
            os << std::endl;
        }
        if (!trace_finished)
            os << callstacks.size() << ": ..." << std::endl;
    }
    std::vector<vmbase::callstack_info> vmbase::dump_call_stack_func_info(
        size_t max_count, bool need_offset, bool* out_finished)const noexcept
    {
        _wo_vm_stack_guard vsg(this);

        // NOTE: When vm running, rt_ip may point to:
        // [ -- COMMAND 6bit --] [ - DR 2bit -] [ ----- OPNUM1 ------] [ ----- OPNUM2 ------]
        //                                     ^1                     ^2                     ^3
        // If rt_ip point to place 3, 'get_current_func_signature_by_runtime_ip' will get next command's debuginfo.
        // So we do a move of 1BYTE here, for getting correct debuginfo.

        if (out_finished != nullptr)
            *out_finished = true;

        if (env == nullptr)
            return {};

        std::vector<callstack_info> result;
        auto generate_callstack_info_with_ip = [this, need_offset](const wo::byte_t* rip, bool is_extern_func)
            {
                const program_debug_data_info::location* src_location_info = nullptr;
                std::string function_signature;
                std::string file_path;
                size_t row_number = 0;
                size_t col_number = 0;

                if (is_extern_func)
                {
                    auto fnd = env->extern_native_functions.find((intptr_t)rip);

                    if (fnd != env->extern_native_functions.end())
                    {
                        function_signature = fnd->second.function_name;
                        file_path = fnd->second.library_name.value_or("<builtin>");
                    }
                    else
                    {
                        char rip_str[sizeof(rip) * 2 + 4];
                        sprintf(rip_str, "0x%p>", rip);

                        function_signature = std::string("<unknown extern function ") + rip_str;
                        file_path = "<unknown library>";
                    }
                }
                else
                {
                    if (env->program_debug_info != nullptr)
                    {
                        src_location_info = &env->program_debug_info
                            ->get_src_location_by_runtime_ip(rip - (need_offset ? 1 : 0));
                        function_signature = env->program_debug_info
                            ->get_current_func_signature_by_runtime_ip(rip - (need_offset ? 1 : 0));

                        file_path = wo::wstrn_to_str(src_location_info->source_file);
                        row_number = src_location_info->begin_row_no;
                        col_number = src_location_info->begin_col_no;
                    }
                    else
                    {
                        char rip_str[sizeof(rip) * 2 + 4];
                        sprintf(rip_str, "0x%p>", rip);

                        function_signature = std::string("<unknown function ") + rip_str;
                        file_path = "<unknown file>";
                    }
                }
                return callstack_info{
                    function_signature,
                    file_path,
                    row_number,
                    col_number,
                    is_extern_func,
                };
            };

        result.push_back(generate_callstack_info_with_ip(ip, ip < env->rt_codes || ip >= env->rt_codes + env->rt_code_len));
        value* base_callstackinfo_ptr = (bp + 1);
        while (base_callstackinfo_ptr <= this->stack_mem_begin)
        {
            if (result.size() >= max_count)
            {
                if (out_finished != nullptr)
                    *out_finished = false;

                break;
            }

            if ((base_callstackinfo_ptr->type & (~1)) == value::valuetype::callstack)
            {
                // NOTE: Tracing call stack might changed in other thread.
                //  Check it to make sure donot reach bad place.
                auto callstack = base_callstackinfo_ptr->vmcallstack;
                auto* next_trace_place = this->stack_mem_begin - callstack.bp;

                result.push_back(
                    generate_callstack_info_with_ip(env->rt_codes + callstack.ret_ip, false));

                if (next_trace_place < base_callstackinfo_ptr || next_trace_place > stack_mem_begin)
                    goto _label_bad_callstack;

                base_callstackinfo_ptr = next_trace_place + 1;
            }
            else if ((base_callstackinfo_ptr->type & (~1)) == value::valuetype::nativecallstack)
            {
                result.push_back(
                    generate_callstack_info_with_ip(base_callstackinfo_ptr->native_function_addr, true));

                goto _label_refind_next_callstack;
            }
            else
            {
            _label_bad_callstack:
                result.push_back(callstack_info{
                        "??",
                        "<bad callstack>",
                        0,
                        0,
                        false,
                    });

            _label_refind_next_callstack:
                for (;;)
                {
                    ++base_callstackinfo_ptr;
                    if (base_callstackinfo_ptr <= stack_mem_begin)
                    {
                        auto ptr_value_type = base_callstackinfo_ptr->type & (~1);

                        if ((ptr_value_type & (~1)) == value::valuetype::nativecallstack
                            || (ptr_value_type & (~1)) == value::valuetype::callstack)
                            break;
                    }
                    else
                        break;
                }
            }
        }
        return result;
    }
    size_t vmbase::callstack_layer() const noexcept
    {
        _wo_vm_stack_guard vsg(this);

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
            if ((base_callstackinfo_ptr->type & (~1)) == value::valuetype::callstack)
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
    bool vmbase::gc_checkpoint() noexcept
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

    bool vmbase::assure_stack_size(wo_size_t assure_stack_size) noexcept
    {
        if (sp - assure_stack_size < _self_stack_mem_buf)
        {
            size_t current_stack_size = stack_size;
            if (current_stack_size <= assure_stack_size)
                current_stack_size <<= 1;

            bool r;
            do
            {
                _wo_vm_stack_guard g(this);
                r = _reallocate_stack_space(current_stack_size << 1);
            } while (0);

            if (!r)
                wo_fail(WO_FAIL_STACKOVERFLOW, "Stack overflow.");
            return true;
        }
        return false;
    }

    void vmbase::co_pre_invoke(wo_int_t wo_func_addr, wo_int_t argc) noexcept
    {
        wo_assert((vm_interrupt & vm_interrupt_type::LEAVE_INTERRUPT) == 0);

        if (!wo_func_addr)
            wo_fail(WO_FAIL_CALL_FAIL, "Cannot call a 'nil' function.");
        else
        {
            assure_stack_size(1);
            auto* return_sp = sp;
            auto return_tc = tc->integer;

            (sp--)->set_native_callstack(ip);
            ip = env->rt_codes + wo_func_addr;
            tc->set_integer(argc);

            bp = sp;

            // Push return place.
            (sp--)->set_callstack(
                (uint32_t)(stack_mem_begin - return_sp),
                (uint32_t)(stack_mem_begin - bp));
            // Push origin tc.
            (sp--)->set_integer(return_tc);
        }
    }
    void vmbase::co_pre_invoke(wo_handle_t ex_func_addr, wo_int_t argc)noexcept
    {
        wo_assert((vm_interrupt & vm_interrupt_type::LEAVE_INTERRUPT) == 0);

        if (!ex_func_addr)
            wo_fail(WO_FAIL_CALL_FAIL, "Cannot call a 'nil' function.");
        else
        {
            assure_stack_size(1);
            auto* return_sp = sp;
            auto return_tc = tc->integer;

            (sp--)->set_native_callstack(ip);
            ip = (const byte_t*)ex_func_addr;
            tc->set_integer(argc);

            bp = sp;

            // Push return place.
            (sp--)->set_callstack(
                (uint32_t)(stack_mem_begin - return_sp),
                (uint32_t)(stack_mem_begin - bp));
            // Push origin tc.
            (sp--)->set_integer(return_tc);
        }
    }
    void vmbase::co_pre_invoke(closure_t* wo_func_addr, wo_int_t argc)noexcept
    {
        wo_assert((vm_interrupt & vm_interrupt_type::LEAVE_INTERRUPT) == 0);

        if (!wo_func_addr)
            wo_fail(WO_FAIL_CALL_FAIL, "Cannot call a 'nil' function.");
        else
        {
            wo::gcbase::gc_read_guard rg1(wo_func_addr);
            assure_stack_size((size_t)wo_func_addr->m_closure_args_count + 1);

            auto* return_sp = sp;
            auto return_tc = tc->integer;

            for (uint16_t idx = 0; idx < wo_func_addr->m_closure_args_count; ++idx)
                (sp--)->set_val(&wo_func_addr->m_closure_args[idx]);

            (sp--)->set_native_callstack(ip);
            ip = wo_func_addr->m_native_call
                ? (const byte_t*)wo_func_addr->m_native_func
                : env->rt_codes + wo_func_addr->m_vm_func;
            tc->set_integer(argc);

            bp = sp;

            // Push return place.
            (sp--)->set_callstack(
                (uint32_t)(stack_mem_begin - return_sp),
                (uint32_t)(stack_mem_begin - bp));
            // Push origin tc.
            (sp--)->set_integer(return_tc);
        }
    }
    value* vmbase::invoke(wo_int_t wo_func_addr, wo_int_t argc)noexcept
    {
        wo_assert((vm_interrupt & vm_interrupt_type::LEAVE_INTERRUPT) == 0);

        if (!wo_func_addr)
            wo_fail(WO_FAIL_CALL_FAIL, "Cannot call a 'nil' function.");
        else
        {
            assure_stack_size(1);
            auto* return_ip = ip;
            auto return_sp_place = stack_mem_begin - sp;
            auto return_bp_place = stack_mem_begin - bp;
            auto return_tc = tc->integer;

            (sp--)->set_native_callstack(ip);
            tc->set_integer(argc);
            ip = env->rt_codes + wo_func_addr;
            bp = sp;

            auto vm_exec_result = run();

            ip = return_ip;
            sp = stack_mem_begin - return_sp_place;
            bp = stack_mem_begin - return_bp_place;
            tc->set_integer(return_tc);

            switch (vm_exec_result)
            {
            case wo_result_t::WO_API_NORMAL:
                return cr;
            case wo_result_t::WO_API_SIM_ABORT:
                break;
            case wo_result_t::WO_API_SIM_YIELD:
                wo_fail(WO_FAIL_CALL_FAIL, "The virtual machine is interrupted by `yield`, but the caller is not `dispatch`.");
                break;
            default:
                wo_fail(WO_FAIL_CALL_FAIL, "Unexpected execution status: %d.", (int)vm_exec_result);
                break;
            }
        }
        return nullptr;
    }
    value* vmbase::invoke(wo_handle_t wo_func_addr, wo_int_t argc)noexcept
    {
        wo_assert((vm_interrupt & vm_interrupt_type::LEAVE_INTERRUPT) == 0);

        if (!wo_func_addr)
            wo_fail(WO_FAIL_CALL_FAIL, "Cannot call a 'nil' function.");
        else
        {
            if (is_aborted())
                return nullptr;

            assure_stack_size(1);
            auto* return_ip = ip;
            auto return_sp_place = stack_mem_begin - sp;
            auto return_bp_place = stack_mem_begin - bp;
            auto return_tc = tc->integer;

            (sp--)->set_native_callstack(ip);
            tc->set_integer(argc);
            ip = env->rt_codes + wo_func_addr;
            bp = sp;

            auto vm_exec_result = ((wo_native_func_t)wo_func_addr)(
                std::launder(reinterpret_cast<wo_vm>(this)),
                std::launder(reinterpret_cast<wo_value>(sp + 2)));

            switch (vm_exec_result)
            {
            case wo_result_t::WO_API_RESYNC:
                // NOTE: WO_API_RESYNC returned by `wo_func_addr`(and it's a extern function)
                //  Only following cases happend:
                //  1) Stack reallocated.
                //  2) Aborted
                //  3) Yield
                //  For case 1) & 2), return immediately; in case 3), just like invoke std::yield,
                //  let interrupt stay at VM, let it handled outside.
                vm_exec_result = wo_result_t::WO_API_NORMAL;
                [[fallthrough]];
            case wo_result_t::WO_API_NORMAL:
                break;
            case wo_result_t::WO_API_SYNC:
                vm_exec_result = run();
                break;
            }

            ip = return_ip;
            sp = stack_mem_begin - return_sp_place;
            bp = stack_mem_begin - return_bp_place;
            tc->set_integer(return_tc);

            switch (vm_exec_result)
            {
            case wo_result_t::WO_API_NORMAL:
                return cr;
            case wo_result_t::WO_API_SIM_ABORT:
                break;
            case wo_result_t::WO_API_SIM_YIELD:
                wo_fail(WO_FAIL_CALL_FAIL, "The virtual machine is interrupted by `yield`, but the caller is not `dispatch`.");
                break;
            default:
                wo_fail(WO_FAIL_CALL_FAIL, "Unexpected execution status: %d.", (int)vm_exec_result);
                break;
            }
        }
        return nullptr;
    }
    value* vmbase::invoke(closure_t* wo_func_closure, wo_int_t argc)noexcept
    {
        wo_assert((vm_interrupt & vm_interrupt_type::LEAVE_INTERRUPT) == 0);

        if (!wo_func_closure)
            wo_fail(WO_FAIL_CALL_FAIL, "Cannot call a 'nil' function.");
        else
        {
            if (is_aborted())
                return nullptr;

            wo::gcbase::gc_read_guard rg1(wo_func_closure);
            if (!wo_func_closure->m_vm_func)
                wo_fail(WO_FAIL_CALL_FAIL, "Cannot call a 'nil' function.");
            else
            {
                assure_stack_size((size_t)wo_func_closure->m_closure_args_count + 1);
                auto* return_ip = ip;

                // NOTE: No need to reduce expand arg count.
                auto return_sp_place = stack_mem_begin - sp;
                auto return_bp_place = stack_mem_begin - bp;
                auto return_tc = tc->integer;

                for (uint16_t idx = 0; idx < wo_func_closure->m_closure_args_count; ++idx)
                    (sp--)->set_val(&wo_func_closure->m_closure_args[idx]);

                (sp--)->set_native_callstack(ip);
                tc->set_integer(argc);
                bp = sp;

                wo_result_t vm_exec_result;

                if (wo_func_closure->m_native_call)
                {
                    vm_exec_result = wo_func_closure->m_native_func(
                        std::launder(reinterpret_cast<wo_vm>(this)),
                        std::launder(reinterpret_cast<wo_value>(sp + 2)));

                    switch (vm_exec_result)
                    {
                    case wo_result_t::WO_API_RESYNC:
                        // NOTE: WO_API_RESYNC returned by `wo_func_addr`(and it's a extern function)
                        //  Only following cases happend:
                        //  1) Stack reallocated.
                        //  2) Aborted
                        //  3) Yield
                        //  For case 1) & 2), return immediately; in case 3), just like invoke std::yield,
                        //  let interrupt stay at VM, let it handled outside.
                        vm_exec_result = wo_result_t::WO_API_NORMAL;
                        [[fallthrough]];
                    case wo_result_t::WO_API_NORMAL:
                        break;
                    case wo_result_t::WO_API_SYNC:
                        vm_exec_result = run();
                        break;
                    }
                }
                else
                {
                    ip = env->rt_codes + wo_func_closure->m_vm_func;
                    vm_exec_result = run();
                }

                ip = return_ip;
                sp = stack_mem_begin - return_sp_place;
                bp = stack_mem_begin - return_bp_place;
                tc->set_integer(return_tc);

                switch (vm_exec_result)
                {
                case wo_result_t::WO_API_NORMAL:
                    return cr;
                case wo_result_t::WO_API_SIM_ABORT:
                    break;
                case wo_result_t::WO_API_SIM_YIELD:
                    wo_fail(WO_FAIL_CALL_FAIL, "The virtual machine is interrupted by `yield`, but the caller is not `dispatch`.");
                    break;
                default:
                    wo_fail(WO_FAIL_CALL_FAIL, "Unexpected execution status: %d.", (int)vm_exec_result);
                    break;
                }
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
                                    (uint16_t)(WO_SAFE_READ_OFFSET_PER_BYTE(2,uint16_t)|WO_SAFE_READ_OFFSET_PER_BYTE(1,uint16_t)):\
                                    WO_SAFE_READ_OFFSET_GET_WORD)
#define WO_SAFE_READ_MOVE_4 (rt_ip+=4,WO_IS_ODD_IRPTR(4)?\
                                    (uint32_t)(WO_SAFE_READ_OFFSET_PER_BYTE(4,uint32_t)|WO_SAFE_READ_OFFSET_PER_BYTE(3,uint32_t)\
                                    |WO_SAFE_READ_OFFSET_PER_BYTE(2,uint32_t)|WO_SAFE_READ_OFFSET_PER_BYTE(1,uint32_t)):\
                                    WO_SAFE_READ_OFFSET_GET_DWORD)
#define WO_SAFE_READ_MOVE_8 (rt_ip+=8,WO_IS_ODD_IRPTR(8)?\
                                    (uint64_t)(WO_SAFE_READ_OFFSET_PER_BYTE(8,uint64_t)|WO_SAFE_READ_OFFSET_PER_BYTE(7,uint64_t)|\
                                    WO_SAFE_READ_OFFSET_PER_BYTE(6,uint64_t)|WO_SAFE_READ_OFFSET_PER_BYTE(5,uint64_t)|\
                                    WO_SAFE_READ_OFFSET_PER_BYTE(4,uint64_t)|WO_SAFE_READ_OFFSET_PER_BYTE(3,uint64_t)|\
                                    WO_SAFE_READ_OFFSET_PER_BYTE(2,uint64_t)|WO_SAFE_READ_OFFSET_PER_BYTE(1,uint64_t)):\
                                    WO_SAFE_READ_OFFSET_GET_QWORD)

// X86 support non-alligned addressing, so just do it!
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
#define WO_SIGNED_SHIFT(VAL) (((signed char)((unsigned char)(((unsigned char)(VAL))<<1)))>>1)

#define WO_ADDRESSING_N1 opnum1 = ((dr >> 1) ?\
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

#define WO_ADDRESSING_N2 opnum2 = ((dr & 0b01) ?\
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
#define WO_ADDRESSING_N3_REG_BPOFF opnum3 = \
                            (WO_IPVAL & (1 << 7)) ?\
                            (bp + WO_SIGNED_SHIFT(WO_IPVAL_MOVE_1))\
                            :\
                            (WO_IPVAL_MOVE_1 + reg_begin)

#define WO_VM_FAIL(ERRNO,ERRINFO) \
    do{ip = rt_ip; wo_fail(ERRNO,ERRINFO); continue;}while(0)
#if WO_ENABLE_RUNTIME_CHECK == 0
#   define WO_VM_ASSERT(EXPR, REASON) wo_assert(EXPR, REASON)
#else
#   define WO_VM_ASSERT(EXPR, REASON) do {\
        if(!(EXPR)){ip = rt_ip; wo_fail(WO_FAIL_UNEXPECTED, REASON); continue;}} while(0)
#endif


    void vmbase::ltx_impl(value* result, value* opnum1, value* opnum2) noexcept
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
    void vmbase::eltx_impl(value* result, value* opnum1, value* opnum2) noexcept
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
    void vmbase::gtx_impl(value* result, value* opnum1, value* opnum2) noexcept
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
    void vmbase::egtx_impl(value* result, value* opnum1, value* opnum2) noexcept
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
    value* vmbase::make_union_impl(value* opnum1, value* opnum2, uint16_t id) noexcept
    {
        opnum1->set_gcunit<value::valuetype::struct_type>(
            struct_t::gc_new<gcbase::gctype::young>(2));

        opnum1->structs->m_values[0].set_integer((wo_integer_t)id);
        opnum1->structs->m_values[1].set_val(opnum2);

        return opnum1;
    }
    value* vmbase::make_closure_fast_impl(value* opnum1, const byte_t* rt_ip, value* rt_sp) noexcept
    {
        const bool make_native_closure = !!(0b0011 & *(rt_ip - 1));
        const uint16_t closure_arg_count = WO_FAST_READ_MOVE_2;

        auto* created_closure = make_native_closure
            ? closure_t::gc_new<gcbase::gctype::young>(
                (wo_native_func_t)WO_FAST_READ_MOVE_8, closure_arg_count)
            : closure_t::gc_new<gcbase::gctype::young>(
                (wo_integer_t)WO_FAST_READ_MOVE_4, closure_arg_count);

        for (size_t i = 0; i < (size_t)closure_arg_count; i++)
        {
            auto* arg_val = ++rt_sp;
            created_closure->m_closure_args[i].set_val(arg_val);
        }
        opnum1->set_gcunit<wo::value::valuetype::closure_type>(created_closure);
        return rt_sp;
    }
    value* vmbase::make_closure_safe_impl(value* opnum1, const byte_t* rt_ip, value* rt_sp) noexcept
    {
        const bool make_native_closure = !!(0b0011 & *(rt_ip - 1));
        const uint16_t closure_arg_count = WO_FAST_READ_MOVE_2;

        auto* created_closure = make_native_closure
            ? closure_t::gc_new<gcbase::gctype::young>(
                (wo_native_func_t)WO_SAFE_READ_MOVE_8, closure_arg_count)
            : closure_t::gc_new<gcbase::gctype::young>(
                (wo_integer_t)WO_SAFE_READ_MOVE_4, closure_arg_count);

        for (size_t i = 0; i < (size_t)closure_arg_count; i++)
        {
            auto* arg_val = ++rt_sp;
            created_closure->m_closure_args[i].set_val(arg_val);
        }
        opnum1->set_gcunit<wo::value::valuetype::closure_type>(created_closure);
        return rt_sp;
    }
    value* vmbase::make_array_impl(value* opnum1, uint16_t size, value* rt_sp) noexcept
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
    value* vmbase::make_map_impl(value* opnum1, uint16_t size, value* rt_sp) noexcept
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
    value* vmbase::make_struct_impl(value* opnum1, uint16_t size, value* rt_sp) noexcept
    {
        opnum1->set_gcunit<value::valuetype::struct_type>(
            struct_t::gc_new<gcbase::gctype::young>(size));

        for (size_t i = 0; i < size; i++)
            opnum1->structs->m_values[size - i - 1].set_val(++rt_sp);

        return rt_sp;
    }
    void vmbase::packargs_impl(
        value* opnum1,
        uint16_t argcount,
        const value* tp,
        value* rt_bp,
        uint16_t skip_closure_arg_count) noexcept
    {
        auto* packed_array = array_t::gc_new<gcbase::gctype::young>();
        packed_array->resize((size_t)tp->integer - (size_t)argcount);
        for (auto argindex = 0 + (size_t)argcount; argindex < (size_t)tp->integer; argindex++)
        {
            (*packed_array)[
                (size_t)argindex - (size_t)argcount].set_val(
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

                    wo_fail(WO_FAIL_INDEX_FAIL,
                        "The number of arguments required for unpack exceeds the number of arguments in the given arguments-package.");
                }
                else
                {
                    if (rt_sp - unpack_argc < vm->_self_stack_mem_buf)
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
                    if (rt_sp - arg_tuple->m_count < vm->_self_stack_mem_buf)
                        goto _wo_unpackargs_stack_overflow;

                    for (uint16_t i = arg_tuple->m_count; i > 0; --i)
                        (rt_sp--)->set_val(&arg_tuple->m_values[i - 1]);

                    tc->integer += (wo_integer_t)arg_tuple->m_count;
                }
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

                    wo_fail(WO_FAIL_INDEX_FAIL,
                        "The number of arguments required for unpack exceeds the number "
                        "of arguments in the given arguments-package.");
                }
                else
                {
                    if (rt_sp - unpack_argc < vm->_self_stack_mem_buf)
                        goto _wo_unpackargs_stack_overflow;

                    for (
                        auto arg_idx = arg_array->rbegin() + (size_t)(
                            (wo_integer_t)arg_array->size() - unpack_argc);
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

                    wo_fail(WO_FAIL_INDEX_FAIL,
                        "The number of arguments required for unpack exceeds the number "
                        "of arguments in the given arguments-package.");
                }
                else
                {
                    size_t arg_array_len = arg_array->size();
                    if (rt_sp - arg_array_len < vm->_self_stack_mem_buf)
                        goto _wo_unpackargs_stack_overflow;

                    for (auto arg_idx = arg_array->rbegin(); arg_idx != arg_array->rend(); arg_idx++)
                        (rt_sp--)->set_val(&*arg_idx);

                    tc->integer += arg_array_len;
                }
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
    _wo_unpackargs_stack_overflow:

        vm->interrupt(vmbase::vm_interrupt_type::STACK_OVERFLOW_INTERRUPT);
        return nullptr;
    }
    const char* vmbase::movcast_impl(value* opnum1, value* opnum2, value::valuetype aim_type) noexcept
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
            case value::valuetype::invalid:
                return "Cannot cast this value to 'nil'.";
                break;
            default:
                return "Unknown type.";
            }
        return nullptr;
    }
    vmbase* vmbase::create_machine(vm_type type) const noexcept
    {
        return new vmbase(type);
    }
    wo_result_t vmbase::run() noexcept
    {
        if (ip >= env->rt_codes && ip < env->rt_codes + env->rt_code_len)
            return run_sim();
        else
            return ((wo_extern_native_func_t)ip)(
                std::launder(reinterpret_cast<wo_vm>(this)),
                std::launder(reinterpret_cast<wo_value>(sp + 2)));
    }
    wo_result_t vmbase::run_sim() noexcept
    {
        // Must not leave when run.
        wo_assert((this->vm_interrupt & vm_interrupt_type::LEAVE_INTERRUPT) == 0);
        wo_assert(_this_thread_vm == this);

        const byte_t* const rt_codes = env.get()->rt_codes;

        value* const rt_cr = cr;
        value* const global_begin = const_global_begin;
        value* const reg_begin = register_mem_begin;

        value* opnum1, * opnum2, * opnum3;

        const byte_t* rt_ip = ip;

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
                do
                {
                    if (dr & 0b01)
                    {
                        if (sp <= _self_stack_mem_buf)
                        {
                            --rt_ip;
                            wo_assure(interrupt(vm_interrupt_type::STACK_OVERFLOW_INTERRUPT));
                            break;
                        }
                        WO_ADDRESSING_N1;
                        (sp--)->set_val(opnum1);
                    }
                    else
                    {
                        uint16_t psh_repeat = WO_IPVAL_MOVE_2;
                        if (sp - psh_repeat < _self_stack_mem_buf)
                        {
                            rt_ip -= 3;
                            wo_assure(interrupt(vm_interrupt_type::STACK_OVERFLOW_INTERRUPT));
                            break;
                        }

                        sp -= psh_repeat;
                    }
                } while (0);
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
                WO_VM_ASSERT(((bp + 1)->type & (~1)) == value::valuetype::callstack
                    || ((bp + 1)->type & (~1)) == value::valuetype::nativecallstack,
                    "Found broken stack in 'ret'.");

                uint16_t pop_count = dr ? WO_IPVAL_MOVE_2 : 0;

                if (((++bp)->type & (~1)) == value::valuetype::nativecallstack)
                {
                    sp = bp;
                    sp += pop_count;

                    // last stack is native_func, just do return; stack balance should be keeped by invoker
                    WO_VM_RETURN(wo_result_t::WO_API_NORMAL);
                }

                value* stored_bp = stack_mem_begin - bp->vmcallstack.bp;
                wo_assert(stored_bp <= stack_mem_begin && stored_bp > _self_stack_mem_buf);

                rt_ip = rt_codes + bp->vmcallstack.ret_ip;
                sp = bp;
                bp = stored_bp;

                // DEBUG

                sp += pop_count;

                // TODO: Panic if rt_ip is outof range.
                break;
            }
            case instruct::opcode::call:
            {
                auto* rollback_rt_ip = rt_ip - 1;

                WO_ADDRESSING_N1;

                WO_VM_ASSERT(0 != opnum1->handle && nullptr != opnum1->closure,
                    "Cannot invoke null function in 'call'.");

                do
                {
                    if (opnum1->type == value::valuetype::closure_type)
                    {
                        // Call closure, unpack closure captured arguments.
                        // 
                        // NOTE: Closure arguments should be poped by closure function it self.
                        //       Can use ret(n) to pop arguments when call.
                        if (sp - opnum1->closure->m_closure_args_count < _self_stack_mem_buf)
                        {
                            rt_ip = rollback_rt_ip;
                            wo_assure(interrupt(vm_interrupt_type::STACK_OVERFLOW_INTERRUPT));
                            break;
                        }

                        for (uint16_t idx = 0; idx < opnum1->closure->m_closure_args_count; ++idx)
                            (sp--)->set_val(&opnum1->closure->m_closure_args[idx]);
                    }
                    else
                    {
                        if (sp <= _self_stack_mem_buf)
                        {
                            rt_ip = rollback_rt_ip;
                            wo_assure(interrupt(vm_interrupt_type::STACK_OVERFLOW_INTERRUPT));
                            break;
                        }
                    }

                    sp->type = value::valuetype::callstack;
                    sp->vmcallstack.ret_ip = (uint32_t)(rt_ip - rt_codes);
                    sp->vmcallstack.bp = (uint32_t)(stack_mem_begin - bp);
                    bp = --sp;
                    auto rt_bp = stack_mem_begin - bp;

                    if (opnum1->type == value::valuetype::handle_type)
                    {
                        // Call native
                        wo_extern_native_func_t call_aim_native_func = (wo_extern_native_func_t)(opnum1->handle);
                        ip = std::launder(reinterpret_cast<byte_t*>(call_aim_native_func));

                        switch (call_aim_native_func(
                            std::launder(reinterpret_cast<wo_vm>(this)),
                            std::launder(reinterpret_cast<wo_value>(sp + 2))))
                        {
                        case wo_result_t::WO_API_RESYNC:
                        case wo_result_t::WO_API_NORMAL:
                        {
                            bp = stack_mem_begin - rt_bp;

                            WO_VM_ASSERT(((bp + 1)->type & (~1)) == value::valuetype::callstack,
                                "Found broken stack in 'call'.");
                            value* stored_bp = stack_mem_begin - (++bp)->vmcallstack.bp;
                            sp = bp;
                            bp = stored_bp;
                            break;
                        }
                        case wo_result_t::WO_API_SYNC:
                        {
                            rt_ip = this->ip;
                            break;
                        }
                        }
                    }
                    else if (opnum1->type == value::valuetype::integer_type)
                    {
                        rt_ip = rt_codes + opnum1->integer;
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
                            case wo_result_t::WO_API_RESYNC:
                            case wo_result_t::WO_API_NORMAL:
                            {
                                bp = stack_mem_begin - rt_bp;

                                WO_VM_ASSERT(((bp + 1)->type & (~1)) == value::valuetype::callstack,
                                    "Found broken stack in 'call'.");
                                value* stored_bp = stack_mem_begin - (++bp)->vmcallstack.bp;
                                // Here to invoke jit closure, jit function cannot pop captured arguments,
                                // So we pop them here.
                                sp = bp + closure->m_closure_args_count;
                                bp = stored_bp;
                                break;
                            }
                            case wo_result_t::WO_API_SYNC:
                            {
                                rt_ip = this->ip;
                                break;
                            }
                            }
                        }
                        else
                            rt_ip = rt_codes + closure->m_vm_func;
                    }
                } while (0);
                break;
            }
            case instruct::opcode::calln:
            {
                WO_VM_ASSERT(dr == 0b11 || dr == 0b01 || dr == 0b00,
                    "Found broken ir-code in 'calln'.");
                do
                {
                    if (sp <= _self_stack_mem_buf)
                    {
                        --rt_ip;
                        wo_assure(interrupt(vm_interrupt_type::STACK_OVERFLOW_INTERRUPT));
                        break;
                    }
                    if (dr)
                    {
                        // Call native
                        wo_extern_native_func_t call_aim_native_func = (wo_extern_native_func_t)(WO_IPVAL_MOVE_8);

                        sp->type = value::valuetype::callstack;
                        sp->vmcallstack.ret_ip = (uint32_t)(rt_ip - rt_codes);
                        sp->vmcallstack.bp = (uint32_t)(stack_mem_begin - bp);
                        bp = --sp;

                        auto rt_bp = stack_mem_begin - bp;
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
                        case wo_result_t::WO_API_RESYNC:
                        case wo_result_t::WO_API_NORMAL:
                        {
                            bp = stack_mem_begin - rt_bp;

                            WO_VM_ASSERT(((bp + 1)->type & (~1)) == value::valuetype::callstack,
                                "Found broken stack in 'calln'.");
                            value* stored_bp = stack_mem_begin - (++bp)->vmcallstack.bp;
                            sp = bp;
                            bp = stored_bp;
                            break;
                        }
                        case wo_result_t::WO_API_SYNC:
                        {
                            rt_ip = this->ip;
                            break;
                        }
                        }
                    }
                    else
                    {
                        const byte_t* aimplace = rt_codes + WO_IPVAL_MOVE_4;
                        rt_ip += 4; // skip reserved place.

                        sp->type = value::valuetype::callstack;
                        sp->vmcallstack.ret_ip = (uint32_t)(rt_ip - rt_codes);
                        sp->vmcallstack.bp = (uint32_t)(stack_mem_begin - bp);
                        bp = --sp;

                        rt_ip = aimplace;

                    }
                } while (0);
                break;
            }
            case instruct::opcode::jmp:
            {
                auto* restore_ip = rt_codes + WO_IPVAL_MOVE_4;
                rt_ip = restore_ip;
                break;
            }
            case instruct::opcode::jt:
            {
                uint32_t aimplace = WO_IPVAL_MOVE_4;
                if (rt_cr->integer)
                    rt_ip = rt_codes + aimplace;
                break;
            }
            case instruct::opcode::jf:
            {
                uint32_t aimplace = WO_IPVAL_MOVE_4;
                if (!rt_cr->integer)
                    rt_ip = rt_codes + aimplace;
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
                    auto* restore_ip = rt_codes + offset;
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
                size_t index = (size_t)opnum2->integer;

                if (index >= opnum1->array->size())
                {
                    rt_cr->set_nil();
                    WO_VM_FAIL(WO_FAIL_INDEX_FAIL, "Index out of range.");
                }
                else
                {
                    rt_cr->set_val(&opnum1->array->at(index));
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

                gcbase::gc_modify_write_guard gwg1(opnum1->gcunit);

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

                size_t index = (size_t)opnum2->integer;
                if (index >= opnum1->array->size())
                {
                    WO_VM_FAIL(WO_FAIL_INDEX_FAIL, "Index out of range.");
                }
                else
                {
                    auto* result = &opnum1->array->at(index);
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
                wchar_t out_str = wo_strn_get_char(
                    opnum1->string->c_str(),
                    opnum1->string->size(),
                    (size_t)opnum2->integer);

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

#ifdef WO_VM_SUPPORT_FAST_NO_ALIGN
                sp = make_closure_fast_impl(rt_cr, rt_ip, sp);
#else
                sp = make_closure_safe_impl(rt_cr, rt_ip, sp);
#endif

                rt_ip += (2 + 8);

                break;
            }
            case instruct::unpackargs:
            {
                auto* rollback_rt_ip = rt_ip - 1;

                WO_ADDRESSING_N1;
                auto unpack_argc_unsigned = WO_IPVAL_MOVE_4;

                auto* new_sp = unpackargs_impl(
                    this, opnum1,
                    reinterpret_cast<int32_t&>(unpack_argc_unsigned),
                    tc, rt_ip, sp, bp);

                if (new_sp != nullptr)
                    sp = new_sp;
                else
                    // STACK_OVERFLOW_INTERRUPT set, rollback and handle the interrupt.
                    rt_ip = rollback_rt_ip;

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

                        packargs_impl(
                            opnum1,
                            this_function_arg_count,
                            tp,
                            bp,
                            skip_closure_arg_count);
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
                    case instruct::extern_opcode_page_0::popn:
                    {
                        WO_ADDRESSING_N1;
                        sp += opnum1->integer;

                        // Check if stack is overflow.
                        wo_assert(sp <= bp);
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
                    // END.
                    WO_VM_RETURN(wo_result_t::WO_API_NORMAL);
                else
                    wo_error("executed 'abrt'.");
            }
            default:
            {
                --rt_ip;    // Move back one command.

                auto interrupt_state = vm_interrupt.load();

                if (interrupt_state & vm_interrupt_type::GC_INTERRUPT)
                {
                    gc_checkpoint();
                }

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
                else if (interrupt_state & vm_interrupt_type::DETACH_DEBUGGEE_INTERRUPT)
                {
                    if (clear_interrupt(vm_interrupt_type::DETACH_DEBUGGEE_INTERRUPT))
                        clear_interrupt(vm_interrupt_type::DEBUG_INTERRUPT);
                }
                else if (interrupt_state & vm_interrupt_type::STACK_OVERFLOW_INTERRUPT)
                {
                    while (!interrupt(vm_interrupt_type::STACK_MODIFING_INTERRUPT))
                        gcbase::rw_lock::spin_loop_hint();

                    shrink_stack_edge = std::min(VM_SHRINK_STACK_MAX_EDGE, (uint8_t)(shrink_stack_edge + 1));
                    // Force realloc stack buffer.
                    bool r = _reallocate_stack_space(stack_size << 1);

                    wo_assure(clear_interrupt((vm_interrupt_type)(
                        vm_interrupt_type::STACK_MODIFING_INTERRUPT
                        | vm_interrupt_type::STACK_OVERFLOW_INTERRUPT)));

                    if (!r)
                        wo_fail(WO_FAIL_STACKOVERFLOW, "Stack overflow.");
                }
                else if (interrupt_state & vm_interrupt_type::SHRINK_STACK_INTERRUPT)
                {
                    while (!interrupt(vm_interrupt_type::STACK_MODIFING_INTERRUPT))
                        gcbase::rw_lock::spin_loop_hint();

                    if (_reallocate_stack_space(stack_size >> 1))
                        shrink_stack_edge = VM_SHRINK_STACK_COUNT;

                    wo_assure(clear_interrupt((vm_interrupt_type)(
                        vm_interrupt_type::STACK_MODIFING_INTERRUPT
                        | vm_interrupt_type::SHRINK_STACK_INTERRUPT)));

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

        WO_VM_RETURN(wo_result_t::WO_API_NORMAL);
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

////////////////////////////////////////////////////////////////////

wo_vm wo_create_vm()
{
    return std::launder(
        reinterpret_cast<wo_vm>(
            new wo::vmbase(wo::vmbase::vm_type::NORMAL)));
}