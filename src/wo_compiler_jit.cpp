#include "wo_compiler_jit.hpp"
#include "wo_instruct.hpp"
#include "wo_vm.hpp"



#undef FAILED

#include "wo_compiler_jit.hpp"
#include "wo_compiler_ir.hpp"

#if WO_JIT_SUPPORT_ASMJIT

#define ASMJIT_STATIC
#include "asmjit/asmjit.h"

#include <unordered_map>

namespace wo
{
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

#define WO_IPVAL_MOVE_2 WO_SAFE_READ_MOVE_2
#define WO_IPVAL_MOVE_4 WO_SAFE_READ_MOVE_4
#define WO_IPVAL_MOVE_8 WO_SAFE_READ_MOVE_8

#define WO_SIGNED_SHIFT(VAL) (((signed char)((unsigned char)(((unsigned char)(VAL))<<1)))>>1)

#define WO_VM_FAIL(ERRNO,ERRINFO) {ip = rt_ip;sp = rt_sp;bp = rt_bp;wo_fail(ERRNO,ERRINFO);continue;}

    struct asmjit_compiler_x64
    {
        using jit_packed_func_t = wo_native_func; // x(vm, bp + 2, argc/* useless */)

        struct function_jit_state
        {
            enum state
            {
                BEGIN,
                COMPILING,
                FINISHED,
                FAILED,
            };

            state m_state = state::BEGIN;
            jit_packed_func_t m_func = nullptr;
            asmjit::CCFunc* m_jitfunc = nullptr;

            /*struct value_in_stack
            {
                asmjit::X86Gp m_value;
                asmjit::X86Gp m_type;
            };
            std::vector<value_in_stack> m_in_stack_values;*/
        };

        std::unordered_map<const byte_t*, function_jit_state>
            m_compiling_functions;
        const byte_t* m_codes;

        struct may_constant_x86Gp
        {
            asmjit::X86Compiler* compiler;
            bool            m_is_constant;
            value* m_constant;
            asmjit::X86Gp   m_value;

            // ---------------------------------
            bool already_valued = false;

            bool is_constant() const
            {
                return m_is_constant;
            }

            asmjit::X86Gp gp_value()
            {
                if (m_is_constant && !already_valued)
                {
                    already_valued = true;

                    m_value = compiler->newUIntPtr();
                    wo_asure(!compiler->mov(m_value, (size_t)m_constant));
                }
                return m_value;
            }

            value* const_value() const
            {
                wo_assert(m_is_constant);
                return m_constant;
            }
        };

        template<typename T>
        static asmjit::X86Mem intptr_ptr(const T& opgreg, int32_t offset = 0)
        {
#ifdef WO_PLATFORM_64
            return asmjit::x86::qword_ptr(opgreg, offset);
#else
            return asmjit::x86::dword_ptr(opgreg, offset);
#endif
        }

        static may_constant_x86Gp get_opnum_ptr(
            asmjit::X86Compiler& x86compiler,
            const byte_t*& rt_ip,
            bool dr,
            asmjit::X86Gp stack_bp,
            asmjit::X86Gp reg,
            runtime_env* env)
        {
            if (dr)
            {
                // opnum from bp-offset or regist
                if ((*rt_ip) & (1 << 7))
                {
                    // from bp-offset
                    auto result = x86compiler.newUIntPtr();
                    wo_asure(!x86compiler.lea(result, intptr_ptr(stack_bp, WO_SIGNED_SHIFT(WO_IPVAL_MOVE_1) * sizeof(value))));
                    return may_constant_x86Gp{ &x86compiler,false,nullptr,result };
                }
                else
                {
                    // from reg
                    auto result = x86compiler.newUIntPtr();
                    wo_asure(!x86compiler.lea(result, intptr_ptr(reg, WO_IPVAL_MOVE_1 * sizeof(value))));
                    return may_constant_x86Gp{ &x86compiler,false,nullptr,result };
                }
            }
            else
            {
                // opnum from global_const

                auto const_global_index = WO_SAFE_READ_MOVE_4;

                if (const_global_index < env->constant_value_count)
                {
                    //Is constant
                    return may_constant_x86Gp{ &x86compiler, true, env->constant_global_reg_rtstack + const_global_index };
                }
                else
                {
                    auto result = x86compiler.newUIntPtr();
                    wo_asure(!x86compiler.mov(result, (size_t)(env->constant_global_reg_rtstack + const_global_index)));
                    return may_constant_x86Gp{ &x86compiler,false,nullptr,result };
                }
            }


        }

        static int _invoke_vm_checkpoint(wo::vmbase* vmm, wo::value* rt_sp, wo::value* rt_bp, const byte_t* rt_ip)
        {
            if (vmm->vm_interrupt & wo::vmbase::GC_INTERRUPT)
                vmm->gc_checkpoint(rt_sp);
            else if (vmm->vm_interrupt & wo::vmbase::vm_interrupt_type::ABORT_INTERRUPT)
            {
                // ABORTED VM WILL NOT ABLE TO RUN AGAIN, SO DO NOT
                // CLEAR ABORT_INTERRUPT
                return 1;
            }
            else if (vmm->vm_interrupt & wo::vmbase::vm_interrupt_type::BR_YIELD_INTERRUPT)
            {
                // wo_asure(vmm->clear_interrupt(wo::vmbase::vm_interrupt_type::BR_YIELD_INTERRUPT));
                if (vmm->get_br_yieldable())
                {
                    vmm->ip = rt_ip;
                    vmm->sp = rt_sp;
                    vmm->bp = rt_bp; // store current context, then break out of jit function
                    // NOTE: DONOT CLEAR BR_YIELD_INTERRUPT, IT SHOULD BE CLEAR IN VM-RUN
                    return 1; // return 
                }
                else
                    wo_fail(WO_FAIL_NOT_SUPPORT, "BR_YIELD_INTERRUPT only work at br_yieldable vm.");
            }
            else if (vmm->vm_interrupt & wo::vmbase::vm_interrupt_type::LEAVE_INTERRUPT)
            {
                // That should not be happend...
                wo_error("Virtual machine handled a LEAVE_INTERRUPT.");
            }
            else if (vmm->vm_interrupt & wo::vmbase::vm_interrupt_type::PENDING_INTERRUPT)
            {
                // That should not be happend...
                wo_error("Virtual machine handled a PENDING_INTERRUPT.");
            }
            // it should be last interrupt..
            else if (vmm->vm_interrupt & wo::vmbase::vm_interrupt_type::DEBUG_INTERRUPT)
            {
                vmm->ip = rt_ip;
                vmm->sp = rt_sp;
                vmm->bp = rt_bp;
                if (auto* debuggee = vmm->current_debuggee())
                {
                    // check debuggee here
                    wo_asure(wo_leave_gcguard(reinterpret_cast<wo_vm>(vmm)));
                    debuggee->_vm_invoke_debuggee(vmm);
                    wo_asure(wo_enter_gcguard(reinterpret_cast<wo_vm>(vmm)));
                }
            }
            else
            {
                // a vm_interrupt is invalid now, just roll back one byte and continue~
                // so here do nothing
            }
            return 0;
        }
        static void make_checkpoint(asmjit::X86Compiler& x86compiler, asmjit::X86Gp rtvm, asmjit::X86Gp stack_sp, asmjit::X86Gp stack_bp, const byte_t* ip)
        {
            // TODO: OPTIMIZE!
            auto no_interrupt_label = x86compiler.newLabel();

            wo_asure(!x86compiler.cmp(asmjit::x86::qword_ptr(rtvm, offsetof(wo::vmbase, fast_ro_vm_interrupt)), 0));
            wo_asure(!x86compiler.je(no_interrupt_label));

            auto stackbp = x86compiler.newUIntPtr();
            wo_asure(!x86compiler.mov(stackbp, stack_bp));

            auto interrupt = x86compiler.newInt32();

            auto invoke_node =
                x86compiler.call((size_t)&_invoke_vm_checkpoint,
                    asmjit::FuncSignatureT<void, vmbase*, value*, value*, const byte_t*>());
            wo_asure(invoke_node->setArg(0, rtvm));
            wo_asure(invoke_node->setArg(1, stack_sp));
            wo_asure(invoke_node->setArg(2, stackbp));
            wo_asure(invoke_node->setArg(3, asmjit::Imm((intptr_t)ip)));

            invoke_node->setRet(0, interrupt);

            // x86compiler.cmp(interrupt, wo::vmbase::vm_interrupt_type::GC_INTERRUPT);
            wo_asure(!x86compiler.cmp(interrupt, 0));
            wo_asure(!x86compiler.je(no_interrupt_label));

            wo_asure(x86compiler.ret()); // break this execute!!!

            wo_asure(!x86compiler.bind(no_interrupt_label));
        }

        static asmjit::X86Gp x86_set_imm(asmjit::X86Compiler& x86compiler, asmjit::X86Gp val, const wo::value& instance)
        {
            // ATTENTION:
            //  Here will no thread safe and mem-branch prevent.
            wo_asure(!x86compiler.mov(asmjit::x86::byte_ptr(val, offsetof(value, type)), (uint8_t)instance.type));

            auto data_of_val = x86compiler.newUInt64();
            wo_asure(!x86compiler.mov(data_of_val, instance.handle));
            wo_asure(!x86compiler.mov(asmjit::x86::dword_ptr(val, offsetof(value, handle)), data_of_val));

            return val;
        }
        static asmjit::X86Gp x86_set_val(asmjit::X86Compiler& x86compiler, asmjit::X86Gp val, asmjit::X86Gp val2)
        {
            // ATTENTION:
            //  Here will no thread safe and mem-branch prevent.
            auto type_of_val2 = x86compiler.newUInt8();
            wo_asure(!x86compiler.mov(type_of_val2, asmjit::x86::byte_ptr(val2, offsetof(value, type))));
            wo_asure(!x86compiler.mov(asmjit::x86::byte_ptr(val, offsetof(value, type)), type_of_val2));

            auto data_of_val2 = x86compiler.newUInt64();
            wo_asure(!x86compiler.mov(data_of_val2, asmjit::x86::qword_ptr(val2, offsetof(value, handle))));
            wo_asure(!x86compiler.mov(asmjit::x86::qword_ptr(val, offsetof(value, handle)), data_of_val2));

            return val;
        }

        static asmjit::X86Gp x86_set_nil(asmjit::X86Compiler& x86compiler, asmjit::X86Gp val)
        {
            wo_asure(!x86compiler.mov(asmjit::x86::byte_ptr(val, offsetof(value, type)), (uint8_t)value::valuetype::invalid));
            wo_asure(!x86compiler.mov(asmjit::x86::qword_ptr(val, offsetof(value, handle)), asmjit::Imm(0)));
            return val;
        }

        static asmjit::JitRuntime& get_jit_runtime()
        {
            static asmjit::JitRuntime jit_runtime;
            return jit_runtime;
        }

        static void native_do_calln_nativefunc(vmbase* vm, wo_extern_native_func_t call_aim_native_func, const byte_t* rt_ip, value* rt_sp, value* rt_bp)
        {
            rt_sp->type = value::valuetype::callstack;
            rt_sp->vmcallstack.ret_ip = (uint32_t)(rt_ip - vm->env->rt_codes);
            rt_sp->vmcallstack.bp = (uint32_t)(vm->stack_mem_begin - rt_bp);
            rt_bp = --rt_sp;
            vm->bp = vm->sp = rt_sp;

            // May be useless?
            vm->cr->set_nil();

            vm->ip = reinterpret_cast<byte_t*>(call_aim_native_func);

            wo_asure(wo_leave_gcguard(reinterpret_cast<wo_vm>(vm)));
            call_aim_native_func(reinterpret_cast<wo_vm>(vm), reinterpret_cast<wo_value>(rt_sp + 2), vm->tc->integer);
            wo_asure(wo_enter_gcguard(reinterpret_cast<wo_vm>(vm)));

            wo_assert((rt_bp + 1)->type == value::valuetype::callstack);
            //value* stored_bp = vm->stack_mem_begin - (++rt_bp)->bp;
            //rt_sp = rt_bp;
            //rt_bp = stored_bp;
        }

        static void native_do_calln_vmfunc(vmbase* vm, wo_extern_native_func_t call_aim_native_func, const byte_t* rt_ip, value* rt_sp, value* rt_bp)
        {
            rt_sp->type = value::valuetype::callstack;
            rt_sp->vmcallstack.ret_ip = (uint32_t)(rt_ip - vm->env->rt_codes);
            rt_sp->vmcallstack.bp = (uint32_t)(vm->stack_mem_begin - rt_bp);
            rt_bp = --rt_sp;
            vm->bp = vm->sp = rt_sp;

            // May be useless?
            vm->cr->set_nil();

            vm->ip = reinterpret_cast<byte_t*>(call_aim_native_func);

            call_aim_native_func(reinterpret_cast<wo_vm>(vm), reinterpret_cast<wo_value>(rt_sp + 2), vm->tc->integer);

            wo_assert((rt_bp + 1)->type == value::valuetype::callstack);
            //value* stored_bp = vm->stack_mem_begin - (++rt_bp)->bp;
            //rt_sp = rt_bp;
            //rt_bp = stored_bp;
        }

        static void x86_do_calln_native_func(asmjit::X86Compiler& x86compiler,
            asmjit::X86Gp vm,
            wo_extern_native_func_t call_aim_native_func,
            const byte_t* rt_ip,
            asmjit::X86Gp rt_sp,
            asmjit::X86Gp rt_bp)
        {
            auto invoke_node =
                x86compiler.call((size_t)&native_do_calln_nativefunc,
                    asmjit::FuncSignatureT<void, vmbase*, wo_extern_native_func_t, const byte_t*, value*, value*>());

            invoke_node->setArg(0, vm);
            invoke_node->setArg(1, asmjit::Imm((size_t)call_aim_native_func));
            invoke_node->setArg(2, asmjit::Imm((size_t)rt_ip));
            invoke_node->setArg(3, rt_sp);
            invoke_node->setArg(4, rt_bp);
        }

        static void x86_do_calln_vm_func(asmjit::X86Compiler& x86compiler,
            asmjit::X86Gp vm,
            function_jit_state& vm_func,
            const byte_t* codes,
            const byte_t* rt_ip,
            asmjit::X86Gp rt_sp,
            asmjit::X86Gp rt_bp,
            asmjit::X86Gp rt_tc)
        {
            if (vm_func.m_state == function_jit_state::state::FINISHED)
            {
                wo_assert(vm_func.m_func);

                auto invoke_node =
                    x86compiler.call((size_t)&native_do_calln_vmfunc,
                        asmjit::FuncSignatureT<void, vmbase*, wo_extern_native_func_t, const byte_t*, value*, value*>());

                invoke_node->setArg(0, vm);
                invoke_node->setArg(1, asmjit::Imm((size_t)vm_func.m_func));
                invoke_node->setArg(2, asmjit::Imm((size_t)rt_ip));
                invoke_node->setArg(3, rt_sp);
                invoke_node->setArg(4, rt_bp);
            }
            else
            {
                wo_assert(vm_func.m_state == function_jit_state::state::COMPILING);
                wo_assert(vm_func.m_jitfunc);

                // Set calltrace info here!

                wo::value callstack;
                callstack.type = wo::value::valuetype::callstack;
                callstack.vmcallstack.bp = 0;
                callstack.vmcallstack.ret_ip = (uint32_t)(rt_ip - codes);

                x86_set_imm(x86compiler, rt_sp, callstack);
                auto bpoffset = x86compiler.newUInt64();
                wo_asure(!x86compiler.mov(bpoffset, intptr_ptr(vm, offsetof(value, vmcallstack) + offsetof(value::callstack, bp))));
                wo_asure(!x86compiler.sub(bpoffset, rt_bp));
                wo_asure(!x86compiler.mov(asmjit::x86::dword_ptr(rt_sp, offsetof(value, vmcallstack) + offsetof(value::callstack, bp)), bpoffset));

                auto callargptr = x86compiler.newUIntPtr();
                auto targc = x86compiler.newInt64();
                wo_asure(!x86compiler.lea(callargptr, asmjit::x86::qword_ptr(rt_sp, 1 * sizeof(value))));
                wo_asure(!x86compiler.lea(targc, asmjit::x86::qword_ptr(rt_bp, offsetof(value, integer))));

                auto invoke_node =
                    x86compiler.call(vm_func.m_jitfunc->getLabel(),
                        asmjit::FuncSignatureT<wo_result_t, vmbase*, value*, size_t>());

                invoke_node->setArg(0, vm);
                invoke_node->setArg(1, callargptr);
                invoke_node->setArg(2, targc);
            }
        }
        static void _vmjitcall_panic(wo::value* opnum1)
        {
            wo_fail(WO_FAIL_DEADLY, wo_cast_string(reinterpret_cast<wo_value>(opnum1)));
        }
        static void _vmjitcall_adds(wo::value* opnum1, wo::value* opnum2)
        {
            wo_assert(opnum1->type == opnum2->type
                && opnum1->type == value::valuetype::string_type);

            string_t::gc_new<gcbase::gctype::eden>(opnum1->gcunit, *opnum1->string + *opnum2->string);
        }
        static void _vmjitcall_modr(wo::value* opnum1, wo::value* opnum2)
        {
            wo_assert(opnum1->type == opnum2->type
                && opnum1->type == value::valuetype::real_type);

            opnum1->real = fmod(opnum1->real, opnum2->real);
        }
        static void _vmjitcall_idarr(wo::value* cr, wo::value* opnum1, wo::value* opnum2)
        {
            wo_assert(opnum1->type == value::valuetype::array_type && opnum1->array != nullptr);
            wo_assert(opnum2->type == value::valuetype::integer_type);

            gcbase::gc_read_guard gwg1(opnum1->gcunit);

            size_t index = opnum2->integer;
            if (opnum2->integer < 0)
                index = opnum1->array->size() + opnum2->integer;
            if (index >= opnum1->array->size())
            {
                wo_fail(WO_FAIL_INDEX_FAIL, "Index out of range.");
                cr->set_nil();
            }
            else
            {
                auto* result = &opnum1->array->at(index);
                if (wo::gc::gc_is_marking())
                    opnum1->array->add_memo(result);
                cr->set_val(result);
            }
        }
        static void _vmjitcall_iddict(wo::value* cr, wo::value* opnum1, wo::value* opnum2)
        {
            wo_assert(opnum1->type == value::valuetype::dict_type && opnum1->dict != nullptr);

            gcbase::gc_read_guard gwg1(opnum1->gcunit);

            auto fnd = opnum1->dict->find(*opnum2);
            if (fnd != opnum1->dict->end())
            {
                auto* result = &fnd->second;
                if (wo::gc::gc_is_marking())
                    opnum1->dict->add_memo(result);
                cr->set_val(result);
            }
            else
                wo_fail(WO_FAIL_INDEX_FAIL, "No such key in current dict.");
        }
        static void _vmjitcall_idstruct(wo::value* opnum1, wo::value* opnum2, uint16_t offset)
        {
            wo_assert(opnum2->type == value::valuetype::struct_type,
                "Cannot index non-struct value in 'idstruct'.");
            wo_assert(opnum2->structs != nullptr,
                "Unable to index null in 'idstruct'.");
            wo_assert(offset < opnum2->structs->m_count,
                "Index out of range in 'idstruct'.");

            gcbase::gc_read_guard gwg1(opnum2->structs);

            auto* result = &opnum2->structs->m_values[offset];
            if (wo::gc::gc_is_marking())
                opnum2->structs->add_memo(result);
            opnum1->set_val(result);
        }
        static void _vmjitcall_siddict(wo::value* opnum1, wo::value* opnum2, wo::value* opnum3)
        {
            wo_assert(opnum1->type == value::valuetype::dict_type && opnum1->dict != nullptr);
            
            gcbase::gc_write_guard gwg1(opnum1->gcunit);

            auto fnd = opnum1->dict->find(*opnum2);
            if (fnd != opnum1->dict->end())
            {
                auto* result = &fnd->second;
                if (wo::gc::gc_is_marking())
                    opnum1->dict->add_memo(result);
                result->set_val(opnum3);
            }
            else
                wo_fail(WO_FAIL_INDEX_FAIL, "No such key in current dict.");
        }
        static void _vmjitcall_sidmap(wo::value* opnum1, wo::value* opnum2, wo::value* opnum3)
        {
            wo_assert(opnum1->type == value::valuetype::dict_type && opnum1->dict != nullptr);

            gcbase::gc_write_guard gwg1(opnum1->gcunit);

            auto* result = &(*opnum1->dict)[*opnum2];
            if (wo::gc::gc_is_marking())
                opnum1->dict->add_memo(result);
            result->set_val(opnum3);
        }
        static void _vmjitcall_sidarr(wo::value* opnum1, wo::value* opnum2, wo::value* opnum3)
        {
            wo_assert(opnum1->type == value::valuetype::array_type && opnum1->array != nullptr);
            wo_assert(opnum2->type == value::valuetype::integer_type);

            gcbase::gc_write_guard gwg1(opnum1->gcunit);

            size_t index = opnum2->integer;
            if (opnum2->integer < 0)
                index = opnum1->array->size() + opnum2->integer;
            if (index >= opnum1->array->size())
            {
                wo_fail(WO_FAIL_INDEX_FAIL, "Index out of range.");
            }
            else
            {
                auto* result = &opnum1->array->at(index);
                if (wo::gc::gc_is_marking())
                    opnum1->array->add_memo(result);
                result->set_val(opnum3);
            }
        }
        static void _vmjitcall_sidstruct(wo::value* opnum1, wo::value* opnum2, uint16_t offset)
        {
            wo_assert(nullptr != opnum1->structs,
                "Unable to index null in 'sidstruct'.");
            wo_assert(opnum1->type == value::valuetype::struct_type,
                "Unable to index non-struct value in 'sidstruct'.");
            wo_assert(offset < opnum1->structs->m_count,
                "Index out of range in 'sidstruct'.");

            gcbase::gc_write_guard gwg1(opnum1->gcunit);

            auto* result = &opnum1->structs->m_values[offset];
            if (wo::gc::gc_is_marking())
                opnum1->structs->add_memo(result);
            result->set_val(opnum2);
        }
        struct WooJitErrorHandler :public asmjit::ErrorHandler
        {
            bool handleError(asmjit::Error err, const char* message, asmjit::CodeEmitter* origin) override
            {
                printf("AsmJit error: %s\n", message);
                return true;
            }
        };

        function_jit_state& analyze_function(const byte_t* rt_ip, runtime_env* env) noexcept
        {
            function_jit_state& state = m_compiling_functions[rt_ip];
            if (state.m_state != function_jit_state::state::BEGIN)
                return state;

            state.m_state = function_jit_state::state::COMPILING;

            using namespace asmjit;

            WooJitErrorHandler woo_jit_error_handler;

            CodeHolder code_buffer;
            code_buffer.init(get_jit_runtime().getCodeInfo());
            code_buffer.setErrorHandler(&woo_jit_error_handler);

            X86Compiler x86compiler(&code_buffer);

            // Generate function declear
            state.m_jitfunc = x86compiler.addFunc(FuncSignatureT<wo_result_t, vmbase*, value*, size_t>());
            // void _jit_(vmbase*  vm , value* bp, value* reg, value* const_global);

            // 0. Get vmptr reg stack base global ptr.
            auto _vmbase = x86compiler.newUIntPtr();
            auto _vmsbp = x86compiler.newUIntPtr();
            auto _vmssp = x86compiler.newUIntPtr();
            auto _vmreg = x86compiler.newUIntPtr();
            auto _vmcr = x86compiler.newUIntPtr();
            auto _vmtc = x86compiler.newUIntPtr();


            x86compiler.setArg(0, _vmbase);
            x86compiler.setArg(1, _vmsbp);
            wo_asure(!x86compiler.sub(_vmsbp, 2 * sizeof(wo::value)));
            wo_asure(!x86compiler.mov(_vmssp, _vmsbp));                    // let sp = bp;
            wo_asure(!x86compiler.mov(_vmreg, intptr_ptr(_vmbase, offsetof(vmbase, register_mem_begin))));
            wo_asure(!x86compiler.mov(_vmcr, intptr_ptr(_vmbase, offsetof(vmbase, cr))));
            wo_asure(!x86compiler.mov(_vmtc, intptr_ptr(_vmbase, offsetof(vmbase, tc))));

            byte_t              opcode_dr = (byte_t)(instruct::abrt << 2);
            instruct::opcode    opcode = (instruct::opcode)(opcode_dr & 0b11111100u);
            unsigned            dr = opcode_dr & 0b00000011u;
            std::map<uint32_t, asmjit::Label> x86_label_table;
            for (;;)
            {
                uint32_t current_ip_byteoffset = (uint32_t)(rt_ip - m_codes);

                if (auto fnd = x86_label_table.find(current_ip_byteoffset);
                    fnd != x86_label_table.end())
                {
                    x86compiler.bind(fnd->second);
                }
                else
                {
                    x86_label_table[current_ip_byteoffset] = x86compiler.newLabel();
                    x86compiler.bind(x86_label_table[current_ip_byteoffset]);
                }

                opcode_dr = *(rt_ip++);
                opcode = (instruct::opcode)(opcode_dr & 0b11111100u);
                dr = opcode_dr & 0b00000011u;

#define WO_JIT_ADDRESSING_N1 auto opnum1 = get_opnum_ptr(x86compiler, rt_ip, (dr & 0b10), _vmsbp, _vmreg, env)
#define WO_JIT_ADDRESSING_N2 auto opnum2 = get_opnum_ptr(x86compiler, rt_ip, (dr & 0b01), _vmsbp, _vmreg, env)
#define WO_JIT_ADDRESSING_N3_REG_BPOFF auto opnum3 = get_opnum_ptr(x86compiler, rt_ip, true, _vmsbp, _vmreg, env)

#define WO_JIT_NOT_SUPPORT do{state.m_state = function_jit_state::state::FAILED; return state; }while(0)

                switch (opcode)
                {
                case instruct::opcode::nop:
                    // do nothing.
                    break;
                case instruct::opcode::psh:
                {
                    if (dr & 0b01)
                    {
                        // WO_ADDRESSING_N1_REF;
                        // (rt_sp--)->set_val(opnum1);

                        WO_JIT_ADDRESSING_N1;

                        x86_set_val(x86compiler, _vmssp, opnum1.gp_value());
                        wo_asure(!x86compiler.sub(_vmssp, sizeof(value)));
                    }
                    else
                    {
                        // uint16_t psh_repeat = WO_IPVAL_MOVE_2;
                        // for (uint32_t i = 0; i < psh_repeat; i++)
                        //      (rt_sp--)->set_nil();

                        uint16_t psh_repeat = WO_IPVAL_MOVE_2;
                        for (uint32_t i = 0; i < psh_repeat; i++)
                        {
                            x86_set_nil(x86compiler, _vmssp);
                            wo_asure(!x86compiler.sub(_vmssp, sizeof(value)));
                        }
                    }
                    break;
                }
                case instruct::opcode::pop:
                {
                    if (dr & 0b01)
                    {
                        // WO_ADDRESSING_N1_REF;
                        // opnum1->set_val((++rt_sp));
                        WO_JIT_ADDRESSING_N1;

                        wo_asure(!x86compiler.add(_vmssp, sizeof(value)));
                        x86_set_val(x86compiler, opnum1.gp_value(), _vmssp);
                    }
                    else
                        wo_asure(!x86compiler.add(_vmssp, WO_IPVAL_MOVE_2 * sizeof(value)));
                    break;
                }
                case instruct::mov:
                {
                    WO_JIT_ADDRESSING_N1;
                    WO_JIT_ADDRESSING_N2;

                    if (opnum2.is_constant())
                        x86_set_imm(x86compiler, opnum1.gp_value(), *opnum2.const_value());
                    else
                        x86_set_val(x86compiler, opnum1.gp_value(), opnum2.gp_value());

                    break;
                }
                case instruct::addi:
                {
                    WO_JIT_ADDRESSING_N1;
                    WO_JIT_ADDRESSING_N2;

                    if (opnum2.is_constant())
                    {
                        wo_asure(!x86compiler.add(x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer)), opnum2.const_value()->integer));
                    }
                    else
                    {
                        auto int_of_op2 = x86compiler.newInt64();
                        wo_asure(!x86compiler.mov(int_of_op2, x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer))));
                        wo_asure(!x86compiler.add(x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer)), int_of_op2));
                    }
                    break;
                }
                case instruct::subi:
                {
                    WO_JIT_ADDRESSING_N1;
                    WO_JIT_ADDRESSING_N2;

                    if (opnum2.is_constant())
                    {
                        wo_asure(!x86compiler.sub(x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer)), opnum2.const_value()->integer));
                    }
                    else
                    {
                        auto int_of_op2 = x86compiler.newInt64();
                        wo_asure(!x86compiler.mov(int_of_op2, x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer))));
                        wo_asure(!x86compiler.sub(x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer)), int_of_op2));
                    }

                    break;
                }
                case instruct::muli:
                {
                    WO_JIT_ADDRESSING_N1;
                    WO_JIT_ADDRESSING_N2;

                    auto int_of_op2 = x86compiler.newInt64();
                    if (opnum2.is_constant())
                        wo_asure(!x86compiler.mov(int_of_op2, opnum2.const_value()->integer));
                    else
                        wo_asure(!x86compiler.mov(int_of_op2, x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer))));

                    wo_asure(!x86compiler.imul(int_of_op2, x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer))));
                    wo_asure(!x86compiler.mov(x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer)), int_of_op2));

                    break;
                }
                case instruct::divi:
                {
                    WO_JIT_ADDRESSING_N1;
                    WO_JIT_ADDRESSING_N2;

                    auto int_of_op1 = x86compiler.newInt64();
                    auto int_of_op2 = x86compiler.newInt64();
                    auto div_op_num = x86compiler.newInt64();

                    wo_asure(!x86compiler.mov(int_of_op1, x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer))));
                    if (opnum2.is_constant())
                        wo_asure(!x86compiler.mov(int_of_op2, opnum2.const_value()->integer));
                    else
                        wo_asure(!x86compiler.mov(int_of_op2, x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer))));

                    wo_asure(!x86compiler.xor_(div_op_num, div_op_num));
                    wo_asure(!x86compiler.cqo(div_op_num, int_of_op1));
                    wo_asure(!x86compiler.idiv(div_op_num, int_of_op1, int_of_op2));

                    wo_asure(!x86compiler.mov(x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer)), int_of_op1));

                    break;
                }
                case instruct::modi:
                {
                    WO_JIT_ADDRESSING_N1;
                    WO_JIT_ADDRESSING_N2;

                    auto int_of_op1 = x86compiler.newInt64();
                    auto int_of_op2 = x86compiler.newInt64();
                    auto div_op_num = x86compiler.newInt64();

                    wo_asure(!x86compiler.mov(int_of_op1, x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer))));
                    if (opnum2.is_constant())
                        wo_asure(!x86compiler.mov(int_of_op2, opnum2.const_value()->integer));
                    else
                        wo_asure(!x86compiler.mov(int_of_op2, x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer))));

                    wo_asure(!x86compiler.mov(div_op_num, int_of_op1));
                    wo_asure(!x86compiler.cqo(int_of_op1, div_op_num));
                    wo_asure(!x86compiler.idiv(int_of_op1, div_op_num, int_of_op2));

                    wo_asure(!x86compiler.mov(x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer)), int_of_op1));

                    break;
                }
                case instruct::opcode::lds:
                {
                    WO_JIT_ADDRESSING_N1;
                    WO_JIT_ADDRESSING_N2;

                    if (opnum2.is_constant())
                    {
                        auto bpoffset = x86compiler.newUIntPtr();
                        wo_asure(!x86compiler.lea(bpoffset, x86::qword_ptr(_vmsbp, (uint32_t)(opnum2.const_value()->integer * sizeof(value)))));
                        x86_set_val(x86compiler, opnum1.gp_value(), bpoffset);
                    }
                    else
                    {
                        auto bpoffset = x86compiler.newUIntPtr();
                        wo_asure(!x86compiler.lea(bpoffset, x86::qword_ptr(_vmsbp, opnum2.gp_value(), sizeof(value))));
                        x86_set_val(x86compiler, opnum1.gp_value(), bpoffset);
                    }
                    break;
                }
                case instruct::opcode::sts:
                {
                    WO_JIT_ADDRESSING_N1;
                    WO_JIT_ADDRESSING_N2;

                    if (opnum2.is_constant())
                    {
                        auto bpoffset = x86compiler.newUIntPtr();
                        wo_asure(!x86compiler.lea(bpoffset, x86::qword_ptr(_vmsbp, (uint32_t)(opnum2.const_value()->integer * sizeof(value)))));
                        x86_set_val(x86compiler, bpoffset, opnum1.gp_value());
                    }
                    else
                    {
                        auto bpoffset = x86compiler.newUIntPtr();
                        wo_asure(!x86compiler.lea(bpoffset, x86::qword_ptr(_vmsbp, opnum2.gp_value(), sizeof(value))));
                        x86_set_val(x86compiler, bpoffset, opnum1.gp_value());
                    }
                    break;
                }
                case instruct::equb:
                {
                    WO_JIT_ADDRESSING_N1;
                    WO_JIT_ADDRESSING_N2;

                    // <=

                    auto x86_equ_jmp_label = x86compiler.newLabel();
                    auto x86_nequ_jmp_label = x86compiler.newLabel();

                    wo_asure(!x86compiler.mov(asmjit::x86::byte_ptr(_vmcr, offsetof(value, type)), (uint8_t)value::valuetype::bool_type));

                    auto int_of_op1 = x86compiler.newInt64();
                    wo_asure(!x86compiler.mov(int_of_op1, x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer))));
                    wo_asure(!x86compiler.cmp(int_of_op1, x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer))));
                    wo_asure(!x86compiler.jne(x86_nequ_jmp_label));

                    wo_asure(!x86compiler.mov(x86::qword_ptr(_vmcr, offsetof(value, integer)), 1));
                    wo_asure(!x86compiler.jmp(x86_equ_jmp_label));
                    wo_asure(!x86compiler.bind(x86_nequ_jmp_label));
                    wo_asure(!x86compiler.mov(x86::qword_ptr(_vmcr, offsetof(value, integer)), 0));
                    wo_asure(!x86compiler.bind(x86_equ_jmp_label));

                    break;
                }
                case instruct::nequb:
                {
                    WO_JIT_ADDRESSING_N1;
                    WO_JIT_ADDRESSING_N2;

                    // <=

                    auto x86_equ_jmp_label = x86compiler.newLabel();
                    auto x86_nequ_jmp_label = x86compiler.newLabel();

                    wo_asure(!x86compiler.mov(asmjit::x86::byte_ptr(_vmcr, offsetof(value, type)), (uint8_t)value::valuetype::bool_type));

                    auto int_of_op1 = x86compiler.newInt64();
                    wo_asure(!x86compiler.mov(int_of_op1, x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer))));
                    wo_asure(!x86compiler.cmp(int_of_op1, x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer))));
                    wo_asure(!x86compiler.jne(x86_nequ_jmp_label));

                    wo_asure(!x86compiler.mov(x86::qword_ptr(_vmcr, offsetof(value, integer)), 0));
                    wo_asure(!x86compiler.jmp(x86_equ_jmp_label));
                    wo_asure(!x86compiler.bind(x86_nequ_jmp_label));
                    wo_asure(!x86compiler.mov(x86::qword_ptr(_vmcr, offsetof(value, integer)), 1));
                    wo_asure(!x86compiler.bind(x86_equ_jmp_label));

                    break;
                }
                case instruct::lti:
                {
                    WO_JIT_ADDRESSING_N1;
                    WO_JIT_ADDRESSING_N2;

                    // <

                    auto x86_cmp_fail = x86compiler.newLabel();
                    auto x86_cmp_end = x86compiler.newLabel();

                    x86compiler.mov(asmjit::x86::byte_ptr(_vmcr, offsetof(value, type)), (uint8_t)value::valuetype::bool_type);

                    if (opnum1.is_constant())
                    {
                        auto int_of_op1 = x86compiler.newInt64();
                        wo_asure(!x86compiler.cmp(x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer)), opnum1.m_constant->integer));
                        wo_asure(!x86compiler.jl(x86_cmp_fail));
                    }
                    else if (opnum2.is_constant())
                    {
                        auto int_of_op1 = x86compiler.newInt64();
                        wo_asure(!x86compiler.cmp(x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer)), opnum2.m_constant->integer));
                        wo_asure(!x86compiler.jge(x86_cmp_fail));
                    }
                    else
                    {
                        auto int_of_op1 = x86compiler.newInt64();
                        wo_asure(!x86compiler.mov(int_of_op1, x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer))));
                        wo_asure(!x86compiler.cmp(int_of_op1, x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer))));
                        wo_asure(!x86compiler.jge(x86_cmp_fail));
                    }

                    wo_asure(!x86compiler.mov(x86::qword_ptr(_vmcr, offsetof(value, integer)), 1));
                    wo_asure(!x86compiler.jmp(x86_cmp_end));
                    wo_asure(!x86compiler.bind(x86_cmp_fail));
                    wo_asure(!x86compiler.mov(x86::qword_ptr(_vmcr, offsetof(value, integer)), 0));
                    wo_asure(!x86compiler.bind(x86_cmp_end));

                    break;
                }
                case instruct::elti:
                {
                    WO_JIT_ADDRESSING_N1;
                    WO_JIT_ADDRESSING_N2;

                    // <=

                    auto x86_cmp_fail = x86compiler.newLabel();
                    auto x86_cmp_end = x86compiler.newLabel();

                    x86compiler.mov(asmjit::x86::byte_ptr(_vmcr, offsetof(value, type)), (uint8_t)value::valuetype::bool_type);

                    if (opnum1.is_constant())
                    {
                        auto int_of_op1 = x86compiler.newInt64();
                        wo_asure(!x86compiler.cmp(x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer)), opnum1.m_constant->integer));
                        x86compiler.jle(x86_cmp_fail);
                    }
                    else if (opnum2.is_constant())
                    {
                        auto int_of_op1 = x86compiler.newInt64();
                        wo_asure(!x86compiler.cmp(x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer)), opnum2.m_constant->integer));
                        x86compiler.jg(x86_cmp_fail);
                    }
                    else
                    {
                        auto int_of_op1 = x86compiler.newInt64();
                        wo_asure(!x86compiler.mov(int_of_op1, x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer))));
                        wo_asure(!x86compiler.cmp(int_of_op1, x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer))));
                        x86compiler.jg(x86_cmp_fail);
                    }

                    wo_asure(!x86compiler.mov(x86::qword_ptr(_vmcr, offsetof(value, integer)), 1));
                    x86compiler.jmp(x86_cmp_end);
                    x86compiler.bind(x86_cmp_fail);
                    wo_asure(!x86compiler.mov(x86::qword_ptr(_vmcr, offsetof(value, integer)), 0));
                    x86compiler.bind(x86_cmp_end);

                    break;
                }
                case instruct::gti:
                {
                    WO_JIT_ADDRESSING_N1;
                    WO_JIT_ADDRESSING_N2;

                    // >

                    auto x86_cmp_fail = x86compiler.newLabel();
                    auto x86_cmp_end = x86compiler.newLabel();

                    x86compiler.mov(asmjit::x86::byte_ptr(_vmcr, offsetof(value, type)), (uint8_t)value::valuetype::bool_type);

                    if (opnum1.is_constant())
                    {
                        auto int_of_op1 = x86compiler.newInt64();
                        wo_asure(!x86compiler.cmp(x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer)), opnum1.m_constant->integer));
                        x86compiler.jg(x86_cmp_fail);
                    }
                    else if (opnum2.is_constant())
                    {
                        auto int_of_op1 = x86compiler.newInt64();
                        wo_asure(!x86compiler.cmp(x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer)), opnum2.m_constant->integer));
                        x86compiler.jle(x86_cmp_fail);
                    }
                    else
                    {
                        auto int_of_op1 = x86compiler.newInt64();
                        wo_asure(!x86compiler.mov(int_of_op1, x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer))));
                        wo_asure(!x86compiler.cmp(int_of_op1, x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer))));
                        x86compiler.jle(x86_cmp_fail);
                    }

                    wo_asure(!x86compiler.mov(x86::qword_ptr(_vmcr, offsetof(value, integer)), 1));
                    x86compiler.jmp(x86_cmp_end);
                    x86compiler.bind(x86_cmp_fail);
                    wo_asure(!x86compiler.mov(x86::qword_ptr(_vmcr, offsetof(value, integer)), 0));
                    x86compiler.bind(x86_cmp_end);

                    break;
                }
                case instruct::egti:
                {
                    WO_JIT_ADDRESSING_N1;
                    WO_JIT_ADDRESSING_N2;

                    // >=

                    auto x86_cmp_fail = x86compiler.newLabel();
                    auto x86_cmp_end = x86compiler.newLabel();

                    x86compiler.mov(asmjit::x86::byte_ptr(_vmcr, offsetof(value, type)), (uint8_t)value::valuetype::bool_type);

                    if (opnum1.is_constant())
                    {
                        auto int_of_op1 = x86compiler.newInt64();
                        wo_asure(!x86compiler.cmp(x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer)), opnum1.m_constant->integer));
                        x86compiler.jge(x86_cmp_fail);
                    }
                    else if (opnum2.is_constant())
                    {
                        auto int_of_op1 = x86compiler.newInt64();
                        wo_asure(!x86compiler.cmp(x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer)), opnum2.m_constant->integer));
                        x86compiler.jl(x86_cmp_fail);
                    }
                    else
                    {
                        auto int_of_op1 = x86compiler.newInt64();
                        wo_asure(!x86compiler.mov(int_of_op1, x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer))));
                        wo_asure(!x86compiler.cmp(int_of_op1, x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer))));
                        x86compiler.jl(x86_cmp_fail);
                    }

                    wo_asure(!x86compiler.mov(x86::qword_ptr(_vmcr, offsetof(value, integer)), 1));
                    x86compiler.jmp(x86_cmp_end);
                    x86compiler.bind(x86_cmp_fail);
                    wo_asure(!x86compiler.mov(x86::qword_ptr(_vmcr, offsetof(value, integer)), 0));
                    x86compiler.bind(x86_cmp_end);

                    break;
                }
                case instruct::ret:
                {
                    if (dr != 0)
                        (void)WO_IPVAL_MOVE_2;

                    wo_asure(x86compiler.ret());
                    break;
                }
                case instruct::jmp:
                {
                    auto check_point_ipaddr = rt_ip - 1;
                    uint32_t jmp_place = WO_IPVAL_MOVE_4;

                    if (jmp_place < current_ip_byteoffset)
                        make_checkpoint(x86compiler, _vmbase, _vmssp, _vmsbp, check_point_ipaddr);

                    if (auto fnd = x86_label_table.find(jmp_place);
                        fnd != x86_label_table.end())
                        wo_asure(!x86compiler.jmp(fnd->second));
                    else
                    {
                        x86_label_table[jmp_place] = x86compiler.newLabel();
                        wo_asure(!x86compiler.jmp(x86_label_table[jmp_place]));
                    }

                    break;
                }
                case instruct::jf:
                {
                    auto check_point_ipaddr = rt_ip - 1;
                    uint32_t jmp_place = WO_IPVAL_MOVE_4;

                    if (jmp_place < current_ip_byteoffset)
                        make_checkpoint(x86compiler, _vmbase, _vmssp, _vmsbp, check_point_ipaddr);

                    wo_asure(!x86compiler.cmp(x86::qword_ptr(_vmcr, offsetof(value, handle)), 0));

                    if (auto fnd = x86_label_table.find(jmp_place);
                        fnd != x86_label_table.end())
                        wo_asure(!x86compiler.je(fnd->second));
                    else
                    {
                        x86_label_table[jmp_place] = x86compiler.newLabel();
                        wo_asure(!x86compiler.je(x86_label_table[jmp_place]));
                    }

                    break;
                }
                case instruct::jt:
                {
                    auto check_point_ipaddr = rt_ip - 1;
                    uint32_t jmp_place = WO_IPVAL_MOVE_4;

                    if (jmp_place < current_ip_byteoffset)
                        make_checkpoint(x86compiler, _vmbase, _vmssp, _vmsbp, check_point_ipaddr);

                    wo_asure(!x86compiler.cmp(x86::qword_ptr(_vmcr, offsetof(value, handle)), 0));

                    if (auto fnd = x86_label_table.find(jmp_place);
                        fnd != x86_label_table.end())
                        wo_asure(!x86compiler.jne(fnd->second));
                    else
                    {
                        x86_label_table[jmp_place] = x86compiler.newLabel();
                        wo_asure(!x86compiler.jne(x86_label_table[jmp_place]));
                    }

                    break;
                }
                case instruct::call:
                {
                    // Cannot invoke vm function by call.
                    // 1. If calling function is vm-func and it was not been jit-compiled. it will make huge stack-space cost.
                    // 2. 'call' cannot tail jit-compiled vm-func or native-func. It means vm cannot set LEAVE_INTERRUPT correctly.

                    WO_JIT_NOT_SUPPORT;
                }
                case instruct::calln:
                {
                    if (dr)
                    {
                        // Call native
                        jit_packed_func_t call_aim_native_func = (jit_packed_func_t)(WO_IPVAL_MOVE_8);
                        x86_do_calln_native_func(x86compiler, _vmbase, call_aim_native_func, rt_ip, _vmssp, _vmsbp);
                    }
                    else
                    {
                        auto check_point_ipaddr = rt_ip - 1;

                        uint32_t call_aim_vm_func = WO_IPVAL_MOVE_4;
                        rt_ip += 4; // skip empty space;

                        make_checkpoint(x86compiler, _vmbase, _vmssp, _vmsbp, check_point_ipaddr);

                        // Try compile this func
                        auto& compiled_funcstat = analyze_function(m_codes + call_aim_vm_func, env);
                        if (compiled_funcstat.m_state == function_jit_state::state::FAILED)
                        {
                            state.m_state = function_jit_state::state::FAILED;
                            return state;
                        }
                        x86_do_calln_vm_func(x86compiler, _vmbase, compiled_funcstat, m_codes, rt_ip, _vmssp, _vmsbp, _vmtc);
                    }

                    // ATTENTION: AFTER CALLING VM FUNCTION, DONOT MODIFY SP/BP/IP CONTEXT, HERE MAY HAPPEND/PASS BREAK INFO!!!
                    make_checkpoint(x86compiler, _vmbase, _vmssp, _vmsbp, rt_ip);
                    break;
                }
                case instruct::addr:
                {
                    WO_JIT_ADDRESSING_N1;
                    WO_JIT_ADDRESSING_N2;

                    auto real_of_op1 = x86compiler.newXmm();
                    wo_asure(!x86compiler.movsd(real_of_op1, x86::ptr(opnum1.gp_value(), offsetof(value, real))));
                    wo_asure(!x86compiler.addsd(real_of_op1, x86::ptr(opnum2.gp_value(), offsetof(value, real))));
                    wo_asure(!x86compiler.movsd(x86::ptr(opnum1.gp_value(), offsetof(value, real)), real_of_op1));
                    break;
                }
                case instruct::subr:
                {
                    WO_JIT_ADDRESSING_N1;
                    WO_JIT_ADDRESSING_N2;

                    auto real_of_op1 = x86compiler.newXmm();
                    wo_asure(!x86compiler.movsd(real_of_op1, x86::ptr(opnum1.gp_value(), offsetof(value, real))));
                    wo_asure(!x86compiler.subsd(real_of_op1, x86::ptr(opnum2.gp_value(), offsetof(value, real))));
                    wo_asure(!x86compiler.movsd(x86::ptr(opnum1.gp_value(), offsetof(value, real)), real_of_op1));
                    break;
                }
                case instruct::mulr:
                {
                    WO_JIT_ADDRESSING_N1;
                    WO_JIT_ADDRESSING_N2;

                    auto real_of_op1 = x86compiler.newXmm();
                    wo_asure(!x86compiler.movsd(real_of_op1, x86::ptr(opnum1.gp_value(), offsetof(value, real))));
                    wo_asure(!x86compiler.mulsd(real_of_op1, x86::ptr(opnum2.gp_value(), offsetof(value, real))));
                    wo_asure(!x86compiler.movsd(x86::ptr(opnum1.gp_value(), offsetof(value, real)), real_of_op1));
                    break;
                }
                case instruct::divr:
                {
                    WO_JIT_ADDRESSING_N1;
                    WO_JIT_ADDRESSING_N2;

                    auto real_of_op1 = x86compiler.newXmm();
                    wo_asure(!x86compiler.movsd(real_of_op1, x86::ptr(opnum1.gp_value(), offsetof(value, real))));
                    wo_asure(!x86compiler.divsd(real_of_op1, x86::ptr(opnum2.gp_value(), offsetof(value, real))));
                    wo_asure(!x86compiler.movsd(x86::ptr(opnum1.gp_value(), offsetof(value, real)), real_of_op1));
                    break;
                }
                case instruct::modr:
                {
                    WO_JIT_ADDRESSING_N1;
                    WO_JIT_ADDRESSING_N2;

                    auto op1 = opnum1.gp_value();
                    auto op2 = opnum2.gp_value();

                    auto invoke_node =
                        x86compiler.call((size_t)&_vmjitcall_modr,
                            asmjit::FuncSignatureT<void, wo::value*, wo::value*>());

                    invoke_node->setArg(0, op1);
                    invoke_node->setArg(1, op2);
                }
                case instruct::addh:
                {
                    WO_JIT_ADDRESSING_N1;
                    WO_JIT_ADDRESSING_N2;

                    if (opnum2.is_constant())
                        wo_asure(!x86compiler.add(x86::qword_ptr(opnum1.gp_value(), offsetof(value, handle)), opnum2.const_value()->handle));
                    else
                    {
                        auto handle_of_op2 = x86compiler.newUInt64();
                        wo_asure(!x86compiler.mov(handle_of_op2, x86::qword_ptr(opnum2.gp_value(), offsetof(value, handle))));
                        wo_asure(!x86compiler.add(x86::qword_ptr(opnum1.gp_value(), offsetof(value, handle)), handle_of_op2));
                    }
                    break;
                }
                case instruct::subh:
                {
                    WO_JIT_ADDRESSING_N1;
                    WO_JIT_ADDRESSING_N2;

                    if (opnum2.is_constant())
                        wo_asure(!x86compiler.sub(x86::qword_ptr(opnum1.gp_value(), offsetof(value, handle)), opnum2.const_value()->handle));
                    else
                    {
                        auto handle_of_op2 = x86compiler.newUInt64();
                        wo_asure(!x86compiler.mov(handle_of_op2, x86::qword_ptr(opnum2.gp_value(), offsetof(value, handle))));
                        wo_asure(!x86compiler.sub(x86::qword_ptr(opnum1.gp_value(), offsetof(value, handle)), handle_of_op2));
                    }
                    break;
                }
                case instruct::adds:
                {
                    WO_JIT_ADDRESSING_N1;
                    WO_JIT_ADDRESSING_N2;

                    auto op1 = opnum1.gp_value();
                    auto op2 = opnum2.gp_value();

                    auto invoke_node =
                        x86compiler.call((size_t)&_vmjitcall_adds,
                            asmjit::FuncSignatureT<void, wo::value*, wo::value*>());

                    invoke_node->setArg(0, op1);
                    invoke_node->setArg(1, op2);

                    break;
                }
                case instruct::land:
                {
                    WO_JIT_ADDRESSING_N1;
                    WO_JIT_ADDRESSING_N2;

                    x86compiler.mov(asmjit::x86::byte_ptr(_vmcr, offsetof(value, type)), (uint8_t)value::valuetype::bool_type);
                    if (opnum1.is_constant() && opnum2.is_constant())
                    {
                        if (opnum1.const_value()->integer && opnum2.const_value()->integer)
                            wo_asure(!x86compiler.mov(asmjit::x86::qword_ptr(_vmcr, offsetof(value, integer)), 1));
                        else
                            wo_asure(!x86compiler.mov(asmjit::x86::qword_ptr(_vmcr, offsetof(value, integer)), 0));
                    }
                    else if (opnum1.is_constant())
                    {
                        if (opnum1.const_value()->integer)
                        {
                            auto opnum2_val = x86compiler.newInt64();
                            wo_asure(!x86compiler.mov(opnum2_val, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer))));
                            wo_asure(!x86compiler.mov(asmjit::x86::qword_ptr(_vmcr, offsetof(value, integer)), opnum2_val));
                        }
                        else
                            wo_asure(!x86compiler.mov(asmjit::x86::qword_ptr(_vmcr, offsetof(value, integer)), 0));
                    }
                    else if (opnum2.is_constant())
                    {
                        if (opnum2.const_value()->integer)
                        {
                            auto opnum1_val = x86compiler.newInt64();
                            wo_asure(!x86compiler.mov(opnum1_val, asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer))));
                            wo_asure(!x86compiler.mov(asmjit::x86::qword_ptr(_vmcr, offsetof(value, integer)), opnum1_val));
                        }
                        else
                            wo_asure(!x86compiler.mov(asmjit::x86::qword_ptr(_vmcr, offsetof(value, integer)), 0));
                    }
                    else
                    {
                        auto set_cr_false = x86compiler.newLabel();
                        auto land_end = x86compiler.newLabel();

                        wo_asure(!x86compiler.cmp(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer)), 0));
                        wo_asure(!x86compiler.je(set_cr_false));
                        wo_asure(!x86compiler.cmp(asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer)), 0));
                        wo_asure(!x86compiler.je(set_cr_false));
                        wo_asure(!x86compiler.mov(asmjit::x86::qword_ptr(_vmcr, offsetof(value, integer)), 1));
                        wo_asure(!x86compiler.jmp(land_end));
                        wo_asure(!x86compiler.bind(set_cr_false));
                        wo_asure(!x86compiler.mov(asmjit::x86::qword_ptr(_vmcr, offsetof(value, integer)), 0));
                        wo_asure(!x86compiler.bind(land_end));
                    }
                    break;
                }
                case instruct::lor:
                {
                    WO_JIT_ADDRESSING_N1;
                    WO_JIT_ADDRESSING_N2;

                    x86compiler.mov(asmjit::x86::byte_ptr(_vmcr, offsetof(value, type)), (uint8_t)value::valuetype::bool_type);
                    if (opnum1.is_constant() && opnum2.is_constant())
                    {
                        if (opnum1.const_value()->integer || opnum2.const_value()->integer)
                            wo_asure(!x86compiler.mov(asmjit::x86::qword_ptr(_vmcr, offsetof(value, integer)), 1));
                        else
                            wo_asure(!x86compiler.mov(asmjit::x86::qword_ptr(_vmcr, offsetof(value, integer)), 0));
                    }
                    else if (opnum1.is_constant())
                    {
                        if (!opnum1.const_value()->integer)
                        {
                            auto opnum2_val = x86compiler.newInt64();
                            wo_asure(!x86compiler.mov(opnum2_val, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer))));
                            wo_asure(!x86compiler.mov(asmjit::x86::qword_ptr(_vmcr, offsetof(value, integer)), opnum2_val));
                        }
                        else
                            wo_asure(!x86compiler.mov(asmjit::x86::qword_ptr(_vmcr, offsetof(value, integer)), 1));
                    }
                    else if (opnum2.is_constant())
                    {
                        if (!opnum2.const_value()->integer)
                        {
                            auto opnum1_val = x86compiler.newInt64();
                            wo_asure(!x86compiler.mov(opnum1_val, asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer))));
                            wo_asure(!x86compiler.mov(asmjit::x86::qword_ptr(_vmcr, offsetof(value, integer)), opnum1_val));
                        }
                        else
                            wo_asure(!x86compiler.mov(asmjit::x86::qword_ptr(_vmcr, offsetof(value, integer)), 1));
                    }
                    else
                    {
                        auto set_cr_true = x86compiler.newLabel();
                        auto land_end = x86compiler.newLabel();

                        wo_asure(!x86compiler.cmp(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer)), 0));
                        wo_asure(!x86compiler.jne(set_cr_true));
                        wo_asure(!x86compiler.cmp(asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer)), 0));
                        wo_asure(!x86compiler.jne(set_cr_true));
                        wo_asure(!x86compiler.mov(asmjit::x86::qword_ptr(_vmcr, offsetof(value, integer)), 0));
                        wo_asure(!x86compiler.jmp(land_end));
                        wo_asure(!x86compiler.bind(set_cr_true));
                        wo_asure(!x86compiler.mov(asmjit::x86::qword_ptr(_vmcr, offsetof(value, integer)), 1));
                        wo_asure(!x86compiler.bind(land_end));
                    }
                    break;
                }
                case instruct::ltx:
                    WO_JIT_NOT_SUPPORT;
                case instruct::gtx:
                    WO_JIT_NOT_SUPPORT;
                case instruct::eltx:
                    WO_JIT_NOT_SUPPORT;
                case instruct::egtx:
                    WO_JIT_NOT_SUPPORT;
                case instruct::ltr:
                    WO_JIT_NOT_SUPPORT;
                case instruct::gtr:
                    WO_JIT_NOT_SUPPORT;
                case instruct::eltr:
                    WO_JIT_NOT_SUPPORT;
                case instruct::egtr:
                    WO_JIT_NOT_SUPPORT;
                case instruct::movcast:
                    WO_JIT_NOT_SUPPORT;
                case instruct::mkunion:
                {
                    WO_JIT_ADDRESSING_N1;
                    WO_JIT_ADDRESSING_N2;
                    uint16_t id = WO_IPVAL_MOVE_2;

                    auto op1 = opnum1.gp_value();
                    auto op2 = opnum2.gp_value();

                    auto invoke_node =
                        x86compiler.call((size_t)&vm::make_union_impl,
                            asmjit::FuncSignatureT<wo::value*, wo::value*, wo::value*, uint16_t>());

                    invoke_node->setArg(0, op1);
                    invoke_node->setArg(1, op2);
                    invoke_node->setArg(2, asmjit::Imm(id));
                    invoke_node->setRet(0, _vmssp);
                    break;
                }
                case instruct::mkclos:
                    WO_JIT_NOT_SUPPORT;
                case instruct::mkstruct:
                {
                    WO_JIT_ADDRESSING_N1;
                    uint16_t size = WO_IPVAL_MOVE_2;

                    auto op1 = opnum1.gp_value();

                    auto invoke_node =
                        x86compiler.call((size_t)&vm::make_struct_impl,
                            asmjit::FuncSignatureT< wo::value*, wo::value*, uint16_t, wo::value*>());

                    invoke_node->setArg(0, op1);
                    invoke_node->setArg(1, asmjit::Imm(size));
                    invoke_node->setArg(2, _vmssp);
                    invoke_node->setRet(0, _vmssp);
                    break;
                }
                case instruct::abrt:
                    WO_JIT_NOT_SUPPORT;
                case instruct::idarr:
                {
                    WO_JIT_ADDRESSING_N1;
                    WO_JIT_ADDRESSING_N2;

                    auto op1 = opnum1.gp_value();
                    auto op2 = opnum2.gp_value();

                    auto invoke_node =
                        x86compiler.call((size_t)&_vmjitcall_idarr,
                            asmjit::FuncSignatureT< void, wo::value*, wo::value*, wo::value*>());

                    invoke_node->setArg(0, _vmcr);
                    invoke_node->setArg(1, op1);
                    invoke_node->setArg(2, op2);

                    break;
                }
                case instruct::iddict:
                {
                    WO_JIT_ADDRESSING_N1;
                    WO_JIT_ADDRESSING_N2;

                    auto op1 = opnum1.gp_value();
                    auto op2 = opnum2.gp_value();

                    auto invoke_node =
                        x86compiler.call((size_t)&_vmjitcall_iddict,
                            asmjit::FuncSignatureT< void, wo::value*, wo::value*, wo::value*>());

                    invoke_node->setArg(0, _vmcr);
                    invoke_node->setArg(1, op1);
                    invoke_node->setArg(2, op2);

                    break;
                }
                case instruct::mkarr:
                {
                    WO_JIT_ADDRESSING_N1;
                    uint16_t size = WO_IPVAL_MOVE_2;

                    auto op1 = opnum1.gp_value();

                    auto invoke_node =
                        x86compiler.call((size_t)&vm::make_array_impl,
                            asmjit::FuncSignatureT< wo::value*, wo::value*, uint16_t, wo::value*>());

                    invoke_node->setArg(0, op1);
                    invoke_node->setArg(1, asmjit::Imm(size));
                    invoke_node->setArg(2, _vmssp);
                    invoke_node->setRet(0, _vmssp);
                    break;
                }
                case instruct::mkmap:
                {
                    WO_JIT_ADDRESSING_N1;
                    uint16_t size = WO_IPVAL_MOVE_2;

                    auto op1 = opnum1.gp_value();

                    auto invoke_node =
                        x86compiler.call((size_t)&vm::make_map_impl,
                            asmjit::FuncSignatureT< wo::value*, wo::value*, uint16_t, wo::value*>());

                    invoke_node->setArg(0, op1);
                    invoke_node->setArg(1, asmjit::Imm(size));
                    invoke_node->setArg(2, _vmssp);
                    invoke_node->setRet(0, _vmssp);
                    break;
                }
                case instruct::idstr:
                    WO_JIT_NOT_SUPPORT;
                case instruct::equr:
                    WO_JIT_NOT_SUPPORT;
                case instruct::nequr:
                    WO_JIT_NOT_SUPPORT;
                case instruct::equs:
                    WO_JIT_NOT_SUPPORT;
                case instruct::nequs:
                    WO_JIT_NOT_SUPPORT;
                case instruct::siddict:
                {
                    WO_JIT_ADDRESSING_N1;
                    WO_JIT_ADDRESSING_N2;
                    WO_JIT_ADDRESSING_N3_REG_BPOFF;

                    auto op1 = opnum1.gp_value();
                    auto op2 = opnum2.gp_value();
                    auto op3 = opnum3.gp_value();

                    auto invoke_node =
                        x86compiler.call((size_t)&_vmjitcall_siddict,
                            asmjit::FuncSignatureT< void, wo::value*, wo::value*, wo::value*>());

                    invoke_node->setArg(0, op1);
                    invoke_node->setArg(1, op2);
                    invoke_node->setArg(2, op3);

                    break;
                }
                case instruct::sidmap:
                {
                    WO_JIT_ADDRESSING_N1;
                    WO_JIT_ADDRESSING_N2;
                    WO_JIT_ADDRESSING_N3_REG_BPOFF;

                    auto op1 = opnum1.gp_value();
                    auto op2 = opnum2.gp_value();
                    auto op3 = opnum3.gp_value();

                    auto invoke_node =
                        x86compiler.call((size_t)&_vmjitcall_sidmap,
                            asmjit::FuncSignatureT< void, wo::value*, wo::value*, wo::value*>());

                    invoke_node->setArg(0, op1);
                    invoke_node->setArg(1, op2);
                    invoke_node->setArg(2, op3);

                    break;
                }
                case instruct::sidarr:
                {
                    WO_JIT_ADDRESSING_N1;
                    WO_JIT_ADDRESSING_N2;
                    WO_JIT_ADDRESSING_N3_REG_BPOFF;

                    auto op1 = opnum1.gp_value();
                    auto op2 = opnum2.gp_value();
                    auto op3 = opnum3.gp_value();

                    auto invoke_node =
                        x86compiler.call((size_t)&_vmjitcall_sidarr,
                            asmjit::FuncSignatureT< void, wo::value*, wo::value*, wo::value*>());

                    invoke_node->setArg(0, op1);
                    invoke_node->setArg(1, op2);
                    invoke_node->setArg(2, op3);

                    break;
                }
                case instruct::sidstruct:
                {
                    WO_JIT_ADDRESSING_N1;
                    WO_JIT_ADDRESSING_N2;
                    uint16_t offset = WO_IPVAL_MOVE_2;

                    auto op1 = opnum1.gp_value();
                    auto op2 = opnum2.gp_value();

                    auto invoke_node =
                        x86compiler.call((size_t)&_vmjitcall_sidstruct,
                            asmjit::FuncSignatureT< void, wo::value*, wo::value*, uint16_t>());

                    invoke_node->setArg(0, op1);
                    invoke_node->setArg(1, op2);
                    invoke_node->setArg(2, Imm(offset));
                    break;
                }
                case instruct::jnequb:
                {
                    auto check_point_ipaddr = rt_ip - 1;

                    WO_JIT_ADDRESSING_N1;
                    uint32_t jmp_place = WO_IPVAL_MOVE_4;

                    if (jmp_place < current_ip_byteoffset)
                        make_checkpoint(x86compiler, _vmbase, _vmssp, _vmsbp, check_point_ipaddr);

                    if (opnum1.is_constant())
                        wo_asure(!x86compiler.cmp(x86::qword_ptr(_vmcr, offsetof(value, integer)), opnum1.const_value()->integer));
                    else
                    {
                        auto bvalue = x86compiler.newInt64();
                        wo_asure(!x86compiler.mov(bvalue, x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer))));
                        wo_asure(!x86compiler.cmp(x86::qword_ptr(_vmcr, offsetof(value, integer)), bvalue));
                    }

                    if (auto fnd = x86_label_table.find(jmp_place);
                        fnd != x86_label_table.end())
                        wo_asure(!x86compiler.jne(fnd->second));
                    else
                    {
                        x86_label_table[jmp_place] = x86compiler.newLabel();
                        wo_asure(!x86compiler.jne(x86_label_table[jmp_place]));
                    }

                    break;
                }
                case instruct::idstruct:
                {
                    WO_JIT_ADDRESSING_N1;
                    WO_JIT_ADDRESSING_N2;
                    uint16_t offset = WO_IPVAL_MOVE_2;

                    auto op1 = opnum1.gp_value();
                    auto op2 = opnum2.gp_value();

                    auto invoke_node =
                        x86compiler.call((size_t)&_vmjitcall_idstruct,
                            asmjit::FuncSignatureT< void, wo::value*, wo::value*, uint16_t>());

                    invoke_node->setArg(0, op1);
                    invoke_node->setArg(1, op2);
                    invoke_node->setArg(2, Imm(offset));
                    break;
                }
                case instruct::ext:
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
                        case instruct::extern_opcode_page_0::packargs:
                            WO_JIT_NOT_SUPPORT;
                        case instruct::extern_opcode_page_0::unpackargs:
                            WO_JIT_NOT_SUPPORT;
                        case instruct::extern_opcode_page_0::panic:
                        {
                            WO_JIT_ADDRESSING_N1;
                            auto op1 = opnum1.gp_value();

                            auto invoke_node =
                                x86compiler.call((size_t)&_vmjitcall_panic,
                                    asmjit::FuncSignatureT<void, wo::value*>());

                            invoke_node->setArg(0, op1);
                            break;
                        }
                        default:
                            WO_JIT_NOT_SUPPORT;
                        }
                        break;
                    case 3:     // extern-opcode-page-3
                        switch ((instruct::extern_opcode_page_3)(opcode))
                        {
                        case instruct::extern_opcode_page_3::funcbegin:
                            WO_JIT_NOT_SUPPORT;
                        case instruct::extern_opcode_page_3::funcend:
                        {
                            // This function work end!
                            wo_asure(x86compiler.ret());
                            wo_asure(x86compiler.endFunc());
                            wo_asure(!x86compiler.finalize());

                            wo_asure(!get_jit_runtime().add(&state.m_func, &code_buffer));
                            state.m_state = function_jit_state::state::FINISHED;
                            return state;
                        }
                        default:
                            WO_JIT_NOT_SUPPORT;
                        }
                        break;
                    default:
                        WO_JIT_NOT_SUPPORT;
                    }

                    break;
                }
                default:
                    // Unsupport opcode here. stop compile...
                    WO_JIT_NOT_SUPPORT;
                }
            }
            // Should not be here!
            wo_assert(false);
            WO_JIT_NOT_SUPPORT;
        }
        void analyze_jit(byte_t* codebuf, runtime_env* env) noexcept
        {
            m_codes = codebuf;

            // 1. for all function, trying to jit compile them:
            for (size_t func_offset : env->_functions_offsets_for_jit)
                if (auto& stat = analyze_function(codebuf + func_offset, env); stat.m_state == function_jit_state::FINISHED)
                {
                    wo_assert(nullptr != stat.m_func);
                    env->_jit_functions[(void*)stat.m_func] = func_offset;
                }

            for (size_t calln_offset : env->_calln_opcode_offsets_for_jit)
            {
                wo::instruct::opcode* calln = (wo::instruct::opcode*)(codebuf + calln_offset);
                wo_assert(((*calln) & 0b11111100) == wo::instruct::opcode::calln);
                wo_assert(((*calln) & 0b00000011) == 0b00);

                byte_t* rt_ip = codebuf + calln_offset + 1;

                // READ NEXT 4 BYTE
                size_t offset = (size_t)WO_SAFE_READ_MOVE_4;

                // m_compiling_functions must have this ip
                auto& func_state = m_compiling_functions.at(codebuf + offset);
                if (func_state.m_state == function_jit_state::state::FINISHED)
                {
                    wo_assert(func_state.m_func != nullptr);

                    *calln = (wo::instruct::opcode)(wo::instruct::opcode::calln | 0b11);
                    byte_t* jitfunc = (byte_t*)&func_state.m_func;
                    byte_t* ipbuf = codebuf + calln_offset + 1;

                    for (size_t i = 0; i < 8; ++i)
                        *(ipbuf + i) = *(jitfunc + i);
                }
                else
                    wo_assert(func_state.m_state == function_jit_state::state::FAILED);
            }
            for (size_t mkclos_offset : env->_mkclos_opcode_offsets_for_jit)
            {
                wo::instruct::opcode* mkclos = (wo::instruct::opcode*)(codebuf + mkclos_offset);
                wo_assert(((*mkclos) & 0b11111100) == wo::instruct::opcode::mkclos);
                wo_assert(((*mkclos) & 0b00000011) == 0b00);

                byte_t* rt_ip = codebuf + mkclos_offset + 1;

                // SKIP 2 BYTE
                WO_SAFE_READ_MOVE_2;

                // READ NEXT 4 BYTE
                size_t offset = (size_t)WO_SAFE_READ_MOVE_4;

                // m_compiling_functions must have this ip
                auto& func_state = m_compiling_functions.at(codebuf + offset);
                if (func_state.m_state == function_jit_state::state::FINISHED)
                {
                    wo_assert(func_state.m_func != nullptr);

                    *mkclos = (wo::instruct::opcode)(wo::instruct::opcode::mkclos | 0b10);
                    byte_t* jitfunc = (byte_t*)&func_state.m_func;
                    byte_t* ipbuf = codebuf + mkclos_offset + 1 + 2;

                    for (size_t i = 0; i < 8; ++i)
                        *(ipbuf + i) = *(jitfunc + i);
                }
                else
                    wo_assert(func_state.m_state == function_jit_state::state::FAILED);
            }
        }
    };

    void analyze_jit(byte_t* codebuf, runtime_env* env)
    {
        asmjit_compiler_x64 compiler;
        compiler.analyze_jit(codebuf, env);
    }

    void free_jit(runtime_env* env)
    {
        for (auto& [_func, _offset] : env->_jit_functions)
            wo_asure(!asmjit_compiler_x64::get_jit_runtime().release(_func));
    }
}
#else

namespace wo
{
    struct runtime_env;
    void analyze_jit(byte_t* codebuf, runtime_env* env)
    {
        wo_error("No jit function support.");
    }

    void free_jit(runtime_env* env)
    {
        // Do nothing...
    }
}


#endif
