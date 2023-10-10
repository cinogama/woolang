#include "wo_compiler_jit.hpp"
#include "wo_instruct.hpp"
#include "wo_vm.hpp"

#undef FAILED

#include "wo_compiler_jit.hpp"
#include "wo_compiler_ir.hpp"

#if WO_JIT_SUPPORT_ASMJIT

#ifndef ASMJIT_STATIC
#error Woolang should link asmjit statically.
#endif
#include "asmjit/asmjit.h"
#include "asmjit/x86.h"
#include "asmjit/a64.h"

#include <unordered_map>
#include <list>
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


    template<typename CompileContextT>
    class asmjit_backend
    {
    public:
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
            asmjit::FuncNode* m_jitfunc = nullptr;
        };
    private:
        std::unordered_map<const byte_t*, function_jit_state>
            m_compiling_functions;

        const byte_t* m_codes;

        struct WooJitErrorHandler :public asmjit::ErrorHandler
        {
            void handleError(asmjit::Error err, const char* message, asmjit::BaseEmitter* origin) override
            {
                fprintf(stderr, "AsmJit error: %s\n", message);
            }
        };

        static asmjit::JitRuntime& get_jit_runtime()
        {
            static asmjit::JitRuntime jit_runtime;
            return jit_runtime;
        }
    public:
        virtual CompileContextT* prepare_compiler(
            asmjit::BaseCompiler** out_compiler,
            asmjit::CodeHolder* code,
            function_jit_state* state,
            runtime_env* env) = 0;
        virtual void finish_compiler(CompileContextT* context) = 0;
        virtual void free_compiler(CompileContextT* context) = 0;
#define IRS \
WO_ASMJIT_IR_ITERFACE_DECL(nop)\
WO_ASMJIT_IR_ITERFACE_DECL(mov)\
WO_ASMJIT_IR_ITERFACE_DECL(addi)\
WO_ASMJIT_IR_ITERFACE_DECL(subi)\
WO_ASMJIT_IR_ITERFACE_DECL(muli)\
WO_ASMJIT_IR_ITERFACE_DECL(divi)\
WO_ASMJIT_IR_ITERFACE_DECL(modi)\
WO_ASMJIT_IR_ITERFACE_DECL(addr)\
WO_ASMJIT_IR_ITERFACE_DECL(subr)\
WO_ASMJIT_IR_ITERFACE_DECL(mulr)\
WO_ASMJIT_IR_ITERFACE_DECL(divr)\
WO_ASMJIT_IR_ITERFACE_DECL(modr)\
WO_ASMJIT_IR_ITERFACE_DECL(addh)\
WO_ASMJIT_IR_ITERFACE_DECL(subh)\
WO_ASMJIT_IR_ITERFACE_DECL(adds)\
WO_ASMJIT_IR_ITERFACE_DECL(psh)\
WO_ASMJIT_IR_ITERFACE_DECL(pop)\
WO_ASMJIT_IR_ITERFACE_DECL(sidarr)\
WO_ASMJIT_IR_ITERFACE_DECL(sidstruct)\
WO_ASMJIT_IR_ITERFACE_DECL(lds)\
WO_ASMJIT_IR_ITERFACE_DECL(sts)\
WO_ASMJIT_IR_ITERFACE_DECL(equb)\
WO_ASMJIT_IR_ITERFACE_DECL(nequb)\
WO_ASMJIT_IR_ITERFACE_DECL(lti)\
WO_ASMJIT_IR_ITERFACE_DECL(gti)\
WO_ASMJIT_IR_ITERFACE_DECL(elti)\
WO_ASMJIT_IR_ITERFACE_DECL(egti)\
WO_ASMJIT_IR_ITERFACE_DECL(land)\
WO_ASMJIT_IR_ITERFACE_DECL(lor)\
WO_ASMJIT_IR_ITERFACE_DECL(sidmap)\
WO_ASMJIT_IR_ITERFACE_DECL(ltx)\
WO_ASMJIT_IR_ITERFACE_DECL(gtx)\
WO_ASMJIT_IR_ITERFACE_DECL(eltx)\
WO_ASMJIT_IR_ITERFACE_DECL(egtx)\
WO_ASMJIT_IR_ITERFACE_DECL(ltr)\
WO_ASMJIT_IR_ITERFACE_DECL(gtr)\
WO_ASMJIT_IR_ITERFACE_DECL(eltr)\
WO_ASMJIT_IR_ITERFACE_DECL(egtr)\
WO_ASMJIT_IR_ITERFACE_DECL(call)\
WO_ASMJIT_IR_ITERFACE_DECL(calln)\
WO_ASMJIT_IR_ITERFACE_DECL(ret)\
WO_ASMJIT_IR_ITERFACE_DECL(jt)\
WO_ASMJIT_IR_ITERFACE_DECL(jf)\
WO_ASMJIT_IR_ITERFACE_DECL(jmp)\
WO_ASMJIT_IR_ITERFACE_DECL(mkunion)\
WO_ASMJIT_IR_ITERFACE_DECL(movcast)\
WO_ASMJIT_IR_ITERFACE_DECL(mkclos)\
WO_ASMJIT_IR_ITERFACE_DECL(typeas)\
WO_ASMJIT_IR_ITERFACE_DECL(mkstruct)\
WO_ASMJIT_IR_ITERFACE_DECL(abrt)\
WO_ASMJIT_IR_ITERFACE_DECL(idarr)\
WO_ASMJIT_IR_ITERFACE_DECL(iddict)\
WO_ASMJIT_IR_ITERFACE_DECL(mkarr)\
WO_ASMJIT_IR_ITERFACE_DECL(mkmap)\
WO_ASMJIT_IR_ITERFACE_DECL(idstr)\
WO_ASMJIT_IR_ITERFACE_DECL(equr)\
WO_ASMJIT_IR_ITERFACE_DECL(nequr)\
WO_ASMJIT_IR_ITERFACE_DECL(equs)\
WO_ASMJIT_IR_ITERFACE_DECL(nequs)\
WO_ASMJIT_IR_ITERFACE_DECL(siddict)\
WO_ASMJIT_IR_ITERFACE_DECL(jnequb)\
WO_ASMJIT_IR_ITERFACE_DECL(idstruct)
#define WO_ASMJIT_IR_ITERFACE_DECL(IRNAME) virtual bool ir_##IRNAME(CompileContextT* ctx, unsigned int dr, const byte_t*& rt_ip) = 0;
        IRS
            virtual bool ir_ext_panic(CompileContextT* ctx, unsigned int dr, const byte_t*& rt_ip) = 0;
#undef WO_ASMJIT_IR_ITERFACE_DECL

        std::map<uint32_t, asmjit::Label> label_table;
        void bind_ip(asmjit::BaseCompiler* compiler, uint32_t ipoffset)
        {
            if (auto fnd = label_table.find(ipoffset);
                fnd != label_table.end())
            {
                compiler->bind(fnd->second);
            }
            else
            {
                label_table[ipoffset] = compiler->newLabel();
                compiler->bind(label_table[ipoffset]);
            }
        }
        asmjit::Label jump_ip(asmjit::BaseCompiler* compiler, uint32_t ipoffset)
        {
            if (auto fnd = label_table.find(ipoffset);
                fnd != label_table.end())
                return fnd->second;
            else
                return label_table[ipoffset] = compiler->newLabel();
        }

        function_jit_state& _analyze_function(
            const byte_t* rt_ip,
            runtime_env* env,
            function_jit_state& state,
            CompileContextT* ctx,
            asmjit::BaseCompiler* compiler) noexcept
        {
            byte_t              opcode_dr = (byte_t)(instruct::abrt << 2);
            instruct::opcode    opcode = (instruct::opcode)(opcode_dr & 0b11111100u);
            unsigned int        dr = opcode_dr & 0b00000011u;

            for (;;)
            {
                uint32_t current_ip_byteoffset = (uint32_t)(rt_ip - m_codes);

                bind_ip(compiler, current_ip_byteoffset);

                opcode_dr = *(rt_ip++);
                opcode = (instruct::opcode)(opcode_dr & 0b11111100u);
                dr = opcode_dr & 0b00000011u;

#define WO_JIT_NOT_SUPPORT do{state.m_state = function_jit_state::state::FAILED; return state; }while(0)

                switch (opcode)
                {
#define WO_ASMJIT_IR_ITERFACE_DECL(IRNAME) case instruct::opcode::IRNAME:{if (ir_##IRNAME(ctx, dr, rt_ip)) break; else WO_JIT_NOT_SUPPORT;}
                    IRS
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
                                if (ir_ext_panic(ctx, dr, rt_ip))
                                    break;
                                else
                                    WO_JIT_NOT_SUPPORT;
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
                                this->finish_compiler(ctx);
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
#undef WO_ASMJIT_IR_ITERFACE_DECL
#undef WO_JIT_NOT_SUPPORT
                }
            }
            // Should not be here!
            abort();
        }

        function_jit_state& analyze_function(const byte_t* rt_ip, runtime_env* env) noexcept
        {
            using namespace asmjit;

            function_jit_state& state = m_compiling_functions[rt_ip];
            if (state.m_state != function_jit_state::state::BEGIN)
                return state;

            state.m_state = function_jit_state::state::COMPILING;

            WooJitErrorHandler woo_jit_error_handler;

            CodeHolder code_buffer;
            code_buffer.init(get_jit_runtime().environment());
            code_buffer.setErrorHandler(&woo_jit_error_handler);

            asmjit::BaseCompiler* compiler;
            auto* ctx = this->prepare_compiler(&compiler, &code_buffer, &state, env);

            auto& result = _analyze_function(rt_ip, env, state, ctx, compiler);

            if (result.m_state == function_jit_state::state::FINISHED)
                wo_asure(!get_jit_runtime().add(&result.m_func, &code_buffer));

            this->free_compiler(ctx);
            return result;
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

        static void free_jit(runtime_env* env)
        {
            for (auto& [_func, _offset] : env->_jit_functions)
                wo_asure(!get_jit_runtime().release(_func));
        }

        static int32_t _invoke_vm_checkpoint(wo::vmbase* vmm, wo::value* rt_sp, wo::value* rt_bp, const byte_t* rt_ip)
        {
            if (vmm->vm_interrupt & wo::vmbase::vm_interrupt_type::GC_INTERRUPT)
            {
                vmm->sp = rt_sp;
                vmm->gc_checkpoint();
            }

            if (vmm->vm_interrupt & wo::vmbase::vm_interrupt_type::GC_HANGUP_INTERRUPT)
            {
                vmm->sp = rt_sp;
                if (vmm->clear_interrupt(wo::vmbase::vm_interrupt_type::GC_HANGUP_INTERRUPT))
                    vmm->hangup();
            }
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
        static void native_do_calln_nativefunc_fast(vmbase* vm, wo_extern_native_func_t call_aim_native_func, const byte_t* rt_ip, value* rt_sp, value* rt_bp)
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

        static void _vmjitcall_panic(wo::value* opnum1)
        {
            wo_fail(WO_FAIL_DEADLY, wo_cast_string(reinterpret_cast<wo_value>(opnum1)));
        }
        static void _vmjitcall_adds(wo::value* opnum1, wo::value* opnum2)
        {
            wo_assert(opnum1->type == opnum2->type
                && opnum1->type == value::valuetype::string_type);

            opnum1->set_gcunit<wo::value::valuetype::string_type>(
                string_t::gc_new<gcbase::gctype::young>(*opnum1->string + *opnum2->string));
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
                    wo::gcbase::write_barrier(result);
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
                wo::gcbase::write_barrier(result);
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
                    wo::gcbase::write_barrier(result);
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
                wo::gcbase::write_barrier(result);
            result->set_val(opnum2);
        }
    };

    struct X64CompileContext
    {
        asmjit::x86::Compiler c;
        asmjit::x86::Gp _vmbase;
        asmjit::x86::Gp _vmsbp;
        asmjit::x86::Gp _vmssp;
        asmjit::x86::Gp _vmreg;
        asmjit::x86::Gp _vmcr;
        asmjit::x86::Gp _vmtc;

        runtime_env* env;

        X64CompileContext(asmjit::CodeHolder* code, runtime_env* _env)
            : c(code)
            , env(_env)
        {
            _vmbase = c.newUIntPtr();
            _vmsbp = c.newUIntPtr();
            _vmssp = c.newUIntPtr();
            _vmreg = c.newUIntPtr();
            _vmcr = c.newUIntPtr();
            _vmtc = c.newUIntPtr();
        }
        X64CompileContext(const X64CompileContext&) = delete;
        X64CompileContext(X64CompileContext&&) = delete;
        X64CompileContext& operator = (const X64CompileContext&) = delete;
        X64CompileContext& operator = (X64CompileContext&&) = delete;
    };

    struct asmjit_compiler_x64 : public asmjit_backend<X64CompileContext>
    {
        struct may_constant_x86Gp
        {
            asmjit::x86::Compiler* compiler;
            bool                    m_is_constant;
            value* m_constant;
            asmjit::x86::Gp         m_value;

            // ---------------------------------
            bool                    already_valued = false;

            bool is_constant() const
            {
                return m_is_constant;
            }

            asmjit::x86::Gp gp_value()
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
        static asmjit::x86::Mem intptr_ptr(const T& opgreg, int32_t offset = 0)
        {
            return asmjit::x86::qword_ptr(opgreg, offset);
        }

        static may_constant_x86Gp get_opnum_ptr(
            asmjit::x86::Compiler& x86compiler,
            const byte_t*& rt_ip,
            bool dr,
            asmjit::x86::Gp stack_bp,
            asmjit::x86::Gp reg,
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

        static void make_checkpoint(asmjit::x86::Compiler& x86compiler, asmjit::x86::Gp rtvm, asmjit::x86::Gp stack_sp, asmjit::x86::Gp stack_bp, const byte_t* ip)
        {
            // TODO: OPTIMIZE!
            auto no_interrupt_label = x86compiler.newLabel();
            static_assert(sizeof(wo::vmbase::fast_ro_vm_interrupt) == 4);
            wo_asure(!x86compiler.cmp(asmjit::x86::dword_ptr(rtvm, offsetof(wo::vmbase, fast_ro_vm_interrupt)), 0));
            wo_asure(!x86compiler.je(no_interrupt_label));

            auto stackbp = x86compiler.newUIntPtr();
            wo_asure(!x86compiler.mov(stackbp, stack_bp));

            auto interrupt = x86compiler.newInt32();

            asmjit::InvokeNode* invoke_node;
            wo_asure(!x86compiler.invoke(&invoke_node, (size_t)&_invoke_vm_checkpoint,
                asmjit::FuncSignatureT<int32_t, vmbase*, value*, value*, const byte_t*>()));
            invoke_node->setArg(0, rtvm);
            invoke_node->setArg(1, stack_sp);
            invoke_node->setArg(2, stackbp);
            invoke_node->setArg(3, asmjit::Imm((intptr_t)ip));

            invoke_node->setRet(0, interrupt);

            wo_asure(!x86compiler.cmp(interrupt, 0));
            wo_asure(!x86compiler.je(no_interrupt_label));

            wo_asure(!x86compiler.ret()); // break this execute!!!

            wo_asure(!x86compiler.bind(no_interrupt_label));
        }

        static asmjit::x86::Gp x86_set_imm(asmjit::x86::Compiler& x86compiler, asmjit::x86::Gp val, const wo::value& instance)
        {
            // ATTENTION:
            //  Here will no thread safe and mem-branch prevent.
            wo_asure(!x86compiler.mov(asmjit::x86::byte_ptr(val, offsetof(value, type)), (uint8_t)instance.type));

            auto data_of_val = x86compiler.newUInt64();
            wo_asure(!x86compiler.mov(data_of_val, instance.handle));
            wo_asure(!x86compiler.mov(asmjit::x86::dword_ptr(val, offsetof(value, handle)), data_of_val));

            return val;
        }
        static asmjit::x86::Gp x86_set_val(asmjit::x86::Compiler& x86compiler, asmjit::x86::Gp val, asmjit::x86::Gp val2)
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
        static asmjit::x86::Gp x86_set_nil(asmjit::x86::Compiler& x86compiler, asmjit::x86::Gp val)
        {
            wo_asure(!x86compiler.mov(asmjit::x86::byte_ptr(val, offsetof(value, type)), (uint8_t)value::valuetype::invalid));
            wo_asure(!x86compiler.mov(asmjit::x86::qword_ptr(val, offsetof(value, handle)), asmjit::Imm(0)));
            return val;
        }

        static void x86_do_calln_native_func(asmjit::x86::Compiler& x86compiler,
            asmjit::x86::Gp vm,
            wo_extern_native_func_t call_aim_native_func,
            const byte_t* rt_ip,
            asmjit::x86::Gp rt_sp,
            asmjit::x86::Gp rt_bp)
        {
            asmjit::InvokeNode* invoke_node;
            wo_asure(!x86compiler.invoke(&invoke_node, (size_t)&native_do_calln_nativefunc,
                asmjit::FuncSignatureT<void, vmbase*, wo_extern_native_func_t, const byte_t*, value*, value*>()));

            invoke_node->setArg(0, vm);
            invoke_node->setArg(1, asmjit::Imm((size_t)call_aim_native_func));
            invoke_node->setArg(2, asmjit::Imm((size_t)rt_ip));
            invoke_node->setArg(3, rt_sp);
            invoke_node->setArg(4, rt_bp);
        }

        static void x86_do_calln_native_func_fast(asmjit::x86::Compiler& x86compiler,
            asmjit::x86::Gp vm,
            wo_extern_native_func_t call_aim_native_func,
            const byte_t* rt_ip,
            asmjit::x86::Gp rt_sp,
            asmjit::x86::Gp rt_bp)
        {
            asmjit::InvokeNode* invoke_node;
            wo_asure(!x86compiler.invoke(&invoke_node, (size_t)&native_do_calln_nativefunc_fast,
                asmjit::FuncSignatureT<void, vmbase*, wo_extern_native_func_t, const byte_t*, value*, value*>()));

            invoke_node->setArg(0, vm);
            invoke_node->setArg(1, asmjit::Imm((size_t)call_aim_native_func));
            invoke_node->setArg(2, asmjit::Imm((size_t)rt_ip));
            invoke_node->setArg(3, rt_sp);
            invoke_node->setArg(4, rt_bp);
        }

        static void x86_do_calln_vm_func(asmjit::x86::Compiler& x86compiler,
            asmjit::x86::Gp vm,
            function_jit_state& vm_func,
            const byte_t* codes,
            const byte_t* rt_ip,
            asmjit::x86::Gp rt_sp,
            asmjit::x86::Gp rt_bp,
            asmjit::x86::Gp rt_tc)
        {
            if (vm_func.m_state == function_jit_state::state::FINISHED)
            {
                wo_assert(vm_func.m_func);

                asmjit::InvokeNode* invoke_node;
                wo_asure(!x86compiler.invoke(&invoke_node, (size_t)&native_do_calln_vmfunc,
                    asmjit::FuncSignatureT<void, vmbase*, wo_extern_native_func_t, const byte_t*, value*, value*>()));

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

                asmjit::InvokeNode* invoke_node;
                wo_asure(!x86compiler.invoke(&invoke_node, vm_func.m_jitfunc->label(),
                    asmjit::FuncSignatureT<wo_result_t, vmbase*, value*, size_t>()));

                invoke_node->setArg(0, vm);
                invoke_node->setArg(1, callargptr);
                invoke_node->setArg(2, targc);
            }
        }

        virtual X64CompileContext* prepare_compiler(
            asmjit::BaseCompiler** out_compiler,
            asmjit::CodeHolder* code,
            function_jit_state* state,
            runtime_env* env)override
        {
            X64CompileContext* ctx = new X64CompileContext(code, env);
            *out_compiler = &ctx->c;

            state->m_jitfunc = ctx->c.addFunc(
                asmjit::FuncSignatureT<wo_result_t, vmbase*, value*, size_t>());
            // void _jit_(vmbase*  vm , value* bp, value* reg, value* const_global);

            // 0. Get vmptr reg stack base global ptr.
            state->m_jitfunc->setArg(0, ctx->_vmbase);
            state->m_jitfunc->setArg(1, ctx->_vmsbp);
            wo_asure(!ctx->c.sub(ctx->_vmsbp, 2 * sizeof(wo::value)));
            wo_asure(!ctx->c.mov(ctx->_vmssp, ctx->_vmsbp));                    // let sp = bp;
            wo_asure(!ctx->c.mov(ctx->_vmreg, intptr_ptr(ctx->_vmbase, offsetof(vmbase, register_mem_begin))));
            wo_asure(!ctx->c.mov(ctx->_vmcr, intptr_ptr(ctx->_vmbase, offsetof(vmbase, cr))));
            wo_asure(!ctx->c.mov(ctx->_vmtc, intptr_ptr(ctx->_vmbase, offsetof(vmbase, tc))));

            return ctx;
        }
        virtual void finish_compiler(X64CompileContext* context)override
        {
            wo_asure(!context->c.endFunc());
            wo_asure(!context->c.finalize());
        }
        virtual void free_compiler(X64CompileContext* context) override
        {
            delete context;
        }

#define WO_JIT_ADDRESSING_N1 auto opnum1 = get_opnum_ptr(ctx->c, rt_ip, (dr & 0b10), ctx->_vmsbp, ctx->_vmreg, ctx->env)
#define WO_JIT_ADDRESSING_N2 auto opnum2 = get_opnum_ptr(ctx->c, rt_ip, (dr & 0b01), ctx->_vmsbp, ctx->_vmreg, ctx->env)
#define WO_JIT_ADDRESSING_N3_REG_BPOFF auto opnum3 = get_opnum_ptr(ctx->c, rt_ip, true, ctx->_vmsbp, ctx->_vmreg, ctx->env)
#define WO_JIT_NOT_SUPPORT do{ return false; }while(0)

        virtual bool ir_nop(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return true;
        }
        virtual bool ir_mov(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            if (opnum2.is_constant())
                x86_set_imm(ctx->c, opnum1.gp_value(), *opnum2.const_value());
            else
                x86_set_val(ctx->c, opnum1.gp_value(), opnum2.gp_value());
            return true;
        }
        virtual bool ir_addi(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            if (opnum2.is_constant())
            {
                wo_asure(!ctx->c.add(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer)), opnum2.const_value()->integer));
            }
            else
            {
                auto int_of_op2 = ctx->c.newInt64();
                wo_asure(!ctx->c.mov(int_of_op2, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer))));
                wo_asure(!ctx->c.add(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer)), int_of_op2));
            }
            return true;
        }
        virtual bool ir_subi(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            if (opnum2.is_constant())
            {
                wo_asure(!ctx->c.sub(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer)), opnum2.const_value()->integer));
            }
            else
            {
                auto int_of_op2 = ctx->c.newInt64();
                wo_asure(!ctx->c.mov(int_of_op2, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer))));
                wo_asure(!ctx->c.sub(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer)), int_of_op2));
            }
            return true;
        }
        virtual bool ir_muli(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto int_of_op2 = ctx->c.newInt64();
            if (opnum2.is_constant())
                wo_asure(!ctx->c.mov(int_of_op2, opnum2.const_value()->integer));
            else
                wo_asure(!ctx->c.mov(int_of_op2, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer))));

            wo_asure(!ctx->c.imul(int_of_op2, asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer))));
            wo_asure(!ctx->c.mov(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer)), int_of_op2));
            return true;
        }
        virtual bool ir_divi(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto int_of_op1 = ctx->c.newInt64();
            auto int_of_op2 = ctx->c.newInt64();
            auto div_op_num = ctx->c.newInt64();

            wo_asure(!ctx->c.mov(int_of_op1, asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer))));
            if (opnum2.is_constant())
                wo_asure(!ctx->c.mov(int_of_op2, opnum2.const_value()->integer));
            else
                wo_asure(!ctx->c.mov(int_of_op2, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer))));

            wo_asure(!ctx->c.xor_(div_op_num, div_op_num));
            wo_asure(!ctx->c.cqo(div_op_num, int_of_op1));
            wo_asure(!ctx->c.idiv(div_op_num, int_of_op1, int_of_op2));

            wo_asure(!ctx->c.mov(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer)), int_of_op1));
            return true;
        }
        virtual bool ir_modi(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto int_of_op1 = ctx->c.newInt64();
            auto int_of_op2 = ctx->c.newInt64();
            auto div_op_num = ctx->c.newInt64();

            wo_asure(!ctx->c.mov(int_of_op1, asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer))));
            if (opnum2.is_constant())
                wo_asure(!ctx->c.mov(int_of_op2, opnum2.const_value()->integer));
            else
                wo_asure(!ctx->c.mov(int_of_op2, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer))));

            wo_asure(!ctx->c.mov(div_op_num, int_of_op1));
            wo_asure(!ctx->c.cqo(int_of_op1, div_op_num));
            wo_asure(!ctx->c.idiv(int_of_op1, div_op_num, int_of_op2));

            wo_asure(!ctx->c.mov(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer)), int_of_op1));

            return true;
        }
        virtual bool ir_addr(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto real_of_op1 = ctx->c.newXmm();
            wo_asure(!ctx->c.movsd(real_of_op1, asmjit::x86::ptr(opnum1.gp_value(), offsetof(value, real))));
            wo_asure(!ctx->c.addsd(real_of_op1, asmjit::x86::ptr(opnum2.gp_value(), offsetof(value, real))));
            wo_asure(!ctx->c.movsd(asmjit::x86::ptr(opnum1.gp_value(), offsetof(value, real)), real_of_op1));

            return true;
        }
        virtual bool ir_subr(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto real_of_op1 = ctx->c.newXmm();
            wo_asure(!ctx->c.movsd(real_of_op1, asmjit::x86::ptr(opnum1.gp_value(), offsetof(value, real))));
            wo_asure(!ctx->c.subsd(real_of_op1, asmjit::x86::ptr(opnum2.gp_value(), offsetof(value, real))));
            wo_asure(!ctx->c.movsd(asmjit::x86::ptr(opnum1.gp_value(), offsetof(value, real)), real_of_op1));
            return true;
        }
        virtual bool ir_mulr(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto real_of_op1 = ctx->c.newXmm();
            wo_asure(!ctx->c.movsd(real_of_op1, asmjit::x86::ptr(opnum1.gp_value(), offsetof(value, real))));
            wo_asure(!ctx->c.mulsd(real_of_op1, asmjit::x86::ptr(opnum2.gp_value(), offsetof(value, real))));
            wo_asure(!ctx->c.movsd(asmjit::x86::ptr(opnum1.gp_value(), offsetof(value, real)), real_of_op1));
            return true;
        }
        virtual bool ir_divr(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto real_of_op1 = ctx->c.newXmm();
            wo_asure(!ctx->c.movsd(real_of_op1, asmjit::x86::ptr(opnum1.gp_value(), offsetof(value, real))));
            wo_asure(!ctx->c.divsd(real_of_op1, asmjit::x86::ptr(opnum2.gp_value(), offsetof(value, real))));
            wo_asure(!ctx->c.movsd(asmjit::x86::ptr(opnum1.gp_value(), offsetof(value, real)), real_of_op1));
            return true;
        }
        virtual bool ir_modr(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto op1 = opnum1.gp_value();
            auto op2 = opnum2.gp_value();

            asmjit::InvokeNode* invoke_node;
            wo_asure(!ctx->c.invoke(&invoke_node, (size_t)&_vmjitcall_modr,
                asmjit::FuncSignatureT<void, wo::value*, wo::value*>()));

            invoke_node->setArg(0, op1);
            invoke_node->setArg(1, op2);
            return true;
        }
        virtual bool ir_addh(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            if (opnum2.is_constant())
                wo_asure(!ctx->c.add(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, handle)), opnum2.const_value()->handle));
            else
            {
                auto handle_of_op2 = ctx->c.newUInt64();
                wo_asure(!ctx->c.mov(handle_of_op2, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, handle))));
                wo_asure(!ctx->c.add(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, handle)), handle_of_op2));
            }
            return true;
        }
        virtual bool ir_subh(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            if (opnum2.is_constant())
                wo_asure(!ctx->c.sub(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, handle)), opnum2.const_value()->handle));
            else
            {
                auto handle_of_op2 = ctx->c.newUInt64();
                wo_asure(!ctx->c.mov(handle_of_op2, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, handle))));
                wo_asure(!ctx->c.sub(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, handle)), handle_of_op2));
            }
            return true;
        }
        virtual bool ir_adds(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto op1 = opnum1.gp_value();
            auto op2 = opnum2.gp_value();

            asmjit::InvokeNode* invoke_node;
            wo_asure(!ctx->c.invoke(&invoke_node, (size_t)&_vmjitcall_adds,
                asmjit::FuncSignatureT<void, wo::value*, wo::value*>()));

            invoke_node->setArg(0, op1);
            invoke_node->setArg(1, op2);
            return true;
        }
        virtual bool ir_psh(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            if (dr & 0b01)
            {
                // WO_ADDRESSING_N1_REF;
                // (rt_sp--)->set_val(opnum1);

                WO_JIT_ADDRESSING_N1;
                if (opnum1.is_constant())
                    x86_set_imm(ctx->c, ctx->_vmssp, *opnum1.const_value());
                else
                    x86_set_val(ctx->c, ctx->_vmssp, opnum1.gp_value());
                wo_asure(!ctx->c.sub(ctx->_vmssp, sizeof(value)));
            }
            else
            {
                // uint16_t psh_repeat = WO_IPVAL_MOVE_2;
                // for (uint32_t i = 0; i < psh_repeat; i++)
                //      (rt_sp--)->set_nil();

                uint16_t psh_repeat = WO_IPVAL_MOVE_2;
                for (uint32_t i = 0; i < psh_repeat; i++)
                {
                    x86_set_nil(ctx->c, ctx->_vmssp);
                    wo_asure(!ctx->c.sub(ctx->_vmssp, sizeof(value)));
                }
            }
            return true;
        }
        virtual bool ir_pop(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            if (dr & 0b01)
            {
                // WO_ADDRESSING_N1_REF;
                // opnum1->set_val((++rt_sp));
                WO_JIT_ADDRESSING_N1;

                wo_asure(!ctx->c.add(ctx->_vmssp, sizeof(value)));
                x86_set_val(ctx->c, opnum1.gp_value(), ctx->_vmssp);
            }
            else
                wo_asure(!ctx->c.add(ctx->_vmssp, WO_IPVAL_MOVE_2 * sizeof(value)));
            return true;
        }
        virtual bool ir_sidarr(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;
            WO_JIT_ADDRESSING_N3_REG_BPOFF;

            auto op1 = opnum1.gp_value();
            auto op2 = opnum2.gp_value();
            auto op3 = opnum3.gp_value();

            asmjit::InvokeNode* invoke_node;
            wo_asure(!ctx->c.invoke(&invoke_node, (size_t)&_vmjitcall_sidarr,
                asmjit::FuncSignatureT< void, wo::value*, wo::value*, wo::value*>()));

            invoke_node->setArg(0, op1);
            invoke_node->setArg(1, op2);
            invoke_node->setArg(2, op3);
            return true;
        }
        virtual bool ir_sidstruct(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;
            uint16_t offset = WO_IPVAL_MOVE_2;

            auto op1 = opnum1.gp_value();
            auto op2 = opnum2.gp_value();

            asmjit::InvokeNode* invoke_node;
            wo_asure(!ctx->c.invoke(&invoke_node, (size_t)&_vmjitcall_sidstruct,
                asmjit::FuncSignatureT< void, wo::value*, wo::value*, uint16_t>()));

            invoke_node->setArg(0, op1);
            invoke_node->setArg(1, op2);
            invoke_node->setArg(2, asmjit::Imm(offset));
            return true;
        }
        virtual bool ir_lds(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            if (opnum2.is_constant())
            {
                auto bpoffset = ctx->c.newUIntPtr();
                wo_asure(!ctx->c.lea(bpoffset, asmjit::x86::qword_ptr(ctx->_vmsbp, (uint32_t)(opnum2.const_value()->integer * sizeof(value)))));
                x86_set_val(ctx->c, opnum1.gp_value(), bpoffset);
            }
            else
            {
                auto bpoffset = ctx->c.newUIntPtr();
                wo_asure(!ctx->c.lea(bpoffset, asmjit::x86::qword_ptr(ctx->_vmsbp, opnum2.gp_value(), sizeof(value))));
                x86_set_val(ctx->c, opnum1.gp_value(), bpoffset);
            }
            return true;
        }
        virtual bool ir_sts(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            if (opnum2.is_constant())
            {
                auto bpoffset = ctx->c.newUIntPtr();
                wo_asure(!ctx->c.lea(bpoffset, asmjit::x86::qword_ptr(ctx->_vmsbp, (uint32_t)(opnum2.const_value()->integer * sizeof(value)))));
                x86_set_val(ctx->c, bpoffset, opnum1.gp_value());
            }
            else
            {
                auto bpoffset = ctx->c.newUIntPtr();
                wo_asure(!ctx->c.lea(bpoffset, asmjit::x86::qword_ptr(ctx->_vmsbp, opnum2.gp_value(), sizeof(value))));
                x86_set_val(ctx->c, bpoffset, opnum1.gp_value());
            }
            return true;
        }
        virtual bool ir_equb(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            // <=

            auto x86_equ_jmp_label = ctx->c.newLabel();
            auto x86_nequ_jmp_label = ctx->c.newLabel();

            wo_asure(!ctx->c.mov(asmjit::x86::byte_ptr(ctx->_vmcr, offsetof(value, type)), (uint8_t)value::valuetype::bool_type));

            auto int_of_op1 = ctx->c.newInt64();
            wo_asure(!ctx->c.mov(int_of_op1, asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer))));
            wo_asure(!ctx->c.cmp(int_of_op1, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer))));
            wo_asure(!ctx->c.jne(x86_nequ_jmp_label));

            wo_asure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 1));
            wo_asure(!ctx->c.jmp(x86_equ_jmp_label));
            wo_asure(!ctx->c.bind(x86_nequ_jmp_label));
            wo_asure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 0));
            wo_asure(!ctx->c.bind(x86_equ_jmp_label));

            return true;
        }
        virtual bool ir_nequb(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            // <=

            auto x86_equ_jmp_label = ctx->c.newLabel();
            auto x86_nequ_jmp_label = ctx->c.newLabel();

            wo_asure(!ctx->c.mov(asmjit::x86::byte_ptr(ctx->_vmcr, offsetof(value, type)), (uint8_t)value::valuetype::bool_type));

            auto int_of_op1 = ctx->c.newInt64();
            wo_asure(!ctx->c.mov(int_of_op1, asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer))));
            wo_asure(!ctx->c.cmp(int_of_op1, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer))));
            wo_asure(!ctx->c.jne(x86_nequ_jmp_label));

            wo_asure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 0));
            wo_asure(!ctx->c.jmp(x86_equ_jmp_label));
            wo_asure(!ctx->c.bind(x86_nequ_jmp_label));
            wo_asure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 1));
            wo_asure(!ctx->c.bind(x86_equ_jmp_label));

            return true;
        }
        virtual bool ir_lti(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            // <

            auto x86_cmp_fail = ctx->c.newLabel();
            auto x86_cmp_end = ctx->c.newLabel();

            ctx->c.mov(asmjit::x86::byte_ptr(ctx->_vmcr, offsetof(value, type)), (uint8_t)value::valuetype::bool_type);

            if (opnum1.is_constant())
            {
                wo_asure(!ctx->c.cmp(asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer)), opnum1.m_constant->integer));
                wo_asure(!ctx->c.jl(x86_cmp_fail));
            }
            else if (opnum2.is_constant())
            {
                wo_asure(!ctx->c.cmp(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer)), opnum2.m_constant->integer));
                wo_asure(!ctx->c.jge(x86_cmp_fail));
            }
            else
            {
                auto int_of_op1 = ctx->c.newInt64();
                wo_asure(!ctx->c.mov(int_of_op1, asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer))));
                wo_asure(!ctx->c.cmp(int_of_op1, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer))));
                wo_asure(!ctx->c.jge(x86_cmp_fail));
            }

            wo_asure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 1));
            wo_asure(!ctx->c.jmp(x86_cmp_end));
            wo_asure(!ctx->c.bind(x86_cmp_fail));
            wo_asure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 0));
            wo_asure(!ctx->c.bind(x86_cmp_end));

            return true;
        }
        virtual bool ir_gti(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            // >

            auto x86_cmp_fail = ctx->c.newLabel();
            auto x86_cmp_end = ctx->c.newLabel();

            ctx->c.mov(asmjit::x86::byte_ptr(ctx->_vmcr, offsetof(value, type)), (uint8_t)value::valuetype::bool_type);

            if (opnum1.is_constant())
            {
                wo_asure(!ctx->c.cmp(asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer)), opnum1.m_constant->integer));
                ctx->c.jg(x86_cmp_fail);
            }
            else if (opnum2.is_constant())
            {
                wo_asure(!ctx->c.cmp(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer)), opnum2.m_constant->integer));
                ctx->c.jle(x86_cmp_fail);
            }
            else
            {
                auto int_of_op1 = ctx->c.newInt64();
                wo_asure(!ctx->c.mov(int_of_op1, asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer))));
                wo_asure(!ctx->c.cmp(int_of_op1, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer))));
                ctx->c.jle(x86_cmp_fail);
            }

            wo_asure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 1));
            ctx->c.jmp(x86_cmp_end);
            ctx->c.bind(x86_cmp_fail);
            wo_asure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 0));
            ctx->c.bind(x86_cmp_end);

            return true;
        }
        virtual bool ir_elti(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            // <=

            auto x86_cmp_fail = ctx->c.newLabel();
            auto x86_cmp_end = ctx->c.newLabel();

            ctx->c.mov(asmjit::x86::byte_ptr(ctx->_vmcr, offsetof(value, type)), (uint8_t)value::valuetype::bool_type);

            if (opnum1.is_constant())
            {
                wo_asure(!ctx->c.cmp(asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer)), opnum1.m_constant->integer));
                ctx->c.jle(x86_cmp_fail);
            }
            else if (opnum2.is_constant())
            {
                wo_asure(!ctx->c.cmp(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer)), opnum2.m_constant->integer));
                ctx->c.jg(x86_cmp_fail);
            }
            else
            {
                auto int_of_op1 = ctx->c.newInt64();
                wo_asure(!ctx->c.mov(int_of_op1, asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer))));
                wo_asure(!ctx->c.cmp(int_of_op1, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer))));
                ctx->c.jg(x86_cmp_fail);
            }

            wo_asure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 1));
            ctx->c.jmp(x86_cmp_end);
            ctx->c.bind(x86_cmp_fail);
            wo_asure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 0));
            ctx->c.bind(x86_cmp_end);

            return true;
        }
        virtual bool ir_egti(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            // >=

            auto x86_cmp_fail = ctx->c.newLabel();
            auto x86_cmp_end = ctx->c.newLabel();

            ctx->c.mov(asmjit::x86::byte_ptr(ctx->_vmcr, offsetof(value, type)), (uint8_t)value::valuetype::bool_type);

            if (opnum1.is_constant())
            {
                wo_asure(!ctx->c.cmp(asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer)), opnum1.m_constant->integer));
                ctx->c.jge(x86_cmp_fail);
            }
            else if (opnum2.is_constant())
            {
                wo_asure(!ctx->c.cmp(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer)), opnum2.m_constant->integer));
                ctx->c.jl(x86_cmp_fail);
            }
            else
            {
                auto int_of_op1 = ctx->c.newInt64();
                wo_asure(!ctx->c.mov(int_of_op1, asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer))));
                wo_asure(!ctx->c.cmp(int_of_op1, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer))));
                ctx->c.jl(x86_cmp_fail);
            }

            wo_asure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 1));
            ctx->c.jmp(x86_cmp_end);
            ctx->c.bind(x86_cmp_fail);
            wo_asure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 0));
            ctx->c.bind(x86_cmp_end);

            return true;
        }
        virtual bool ir_land(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            ctx->c.mov(asmjit::x86::byte_ptr(ctx->_vmcr, offsetof(value, type)), (uint8_t)value::valuetype::bool_type);
            if (opnum1.is_constant() && opnum2.is_constant())
            {
                if (opnum1.const_value()->integer && opnum2.const_value()->integer)
                    wo_asure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 1));
                else
                    wo_asure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 0));
            }
            else if (opnum1.is_constant())
            {
                if (opnum1.const_value()->integer)
                {
                    auto opnum2_val = ctx->c.newInt64();
                    wo_asure(!ctx->c.mov(opnum2_val, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer))));
                    wo_asure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), opnum2_val));
                }
                else
                    wo_asure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 0));
            }
            else if (opnum2.is_constant())
            {
                if (opnum2.const_value()->integer)
                {
                    auto opnum1_val = ctx->c.newInt64();
                    wo_asure(!ctx->c.mov(opnum1_val, asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer))));
                    wo_asure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), opnum1_val));
                }
                else
                    wo_asure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 0));
            }
            else
            {
                auto set_cr_false = ctx->c.newLabel();
                auto land_end = ctx->c.newLabel();

                wo_asure(!ctx->c.cmp(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer)), 0));
                wo_asure(!ctx->c.je(set_cr_false));
                wo_asure(!ctx->c.cmp(asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer)), 0));
                wo_asure(!ctx->c.je(set_cr_false));
                wo_asure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 1));
                wo_asure(!ctx->c.jmp(land_end));
                wo_asure(!ctx->c.bind(set_cr_false));
                wo_asure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 0));
                wo_asure(!ctx->c.bind(land_end));
            }
            return true;
        }
        virtual bool ir_lor(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            ctx->c.mov(asmjit::x86::byte_ptr(ctx->_vmcr, offsetof(value, type)), (uint8_t)value::valuetype::bool_type);
            if (opnum1.is_constant() && opnum2.is_constant())
            {
                if (opnum1.const_value()->integer || opnum2.const_value()->integer)
                    wo_asure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 1));
                else
                    wo_asure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 0));
            }
            else if (opnum1.is_constant())
            {
                if (!opnum1.const_value()->integer)
                {
                    auto opnum2_val = ctx->c.newInt64();
                    wo_asure(!ctx->c.mov(opnum2_val, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer))));
                    wo_asure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), opnum2_val));
                }
                else
                    wo_asure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 1));
            }
            else if (opnum2.is_constant())
            {
                if (!opnum2.const_value()->integer)
                {
                    auto opnum1_val = ctx->c.newInt64();
                    wo_asure(!ctx->c.mov(opnum1_val, asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer))));
                    wo_asure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), opnum1_val));
                }
                else
                    wo_asure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 1));
            }
            else
            {
                auto set_cr_true = ctx->c.newLabel();
                auto land_end = ctx->c.newLabel();

                wo_asure(!ctx->c.cmp(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer)), 0));
                wo_asure(!ctx->c.jne(set_cr_true));
                wo_asure(!ctx->c.cmp(asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer)), 0));
                wo_asure(!ctx->c.jne(set_cr_true));
                wo_asure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 0));
                wo_asure(!ctx->c.jmp(land_end));
                wo_asure(!ctx->c.bind(set_cr_true));
                wo_asure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 1));
                wo_asure(!ctx->c.bind(land_end));
            }
            return true;
        }
        virtual bool ir_sidmap(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;
            WO_JIT_ADDRESSING_N3_REG_BPOFF;

            auto op1 = opnum1.gp_value();
            auto op2 = opnum2.gp_value();
            auto op3 = opnum3.gp_value();

            asmjit::InvokeNode* invoke_node;
            wo_asure(!ctx->c.invoke(&invoke_node, (size_t)&_vmjitcall_sidmap,
                asmjit::FuncSignatureT< void, wo::value*, wo::value*, wo::value*>()));

            invoke_node->setArg(0, op1);
            invoke_node->setArg(1, op2);
            invoke_node->setArg(2, op3);

            return true;
        }

        virtual bool ir_ltx(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_NOT_SUPPORT;
        }
        virtual bool ir_gtx(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_NOT_SUPPORT;
        }
        virtual bool ir_eltx(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_NOT_SUPPORT;
        }
        virtual bool ir_egtx(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_NOT_SUPPORT;
        }
        virtual bool ir_ltr(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_NOT_SUPPORT;
        }
        virtual bool ir_gtr(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_NOT_SUPPORT;
        }
        virtual bool ir_eltr(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_NOT_SUPPORT;
        }
        virtual bool ir_egtr(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_NOT_SUPPORT;
        }

        virtual bool ir_call(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            // Cannot invoke vm function by call.
            // 1. If calling function is vm-func and it was not been jit-compiled. it will make huge stack-space cost.
            // 2. 'call' cannot tail jit-compiled vm-func or native-func. It means vm cannot set LEAVE_INTERRUPT correctly.

            WO_JIT_NOT_SUPPORT;
        }
        virtual bool ir_calln(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            if (dr)
            {
                // Call native
                jit_packed_func_t call_aim_native_func = (jit_packed_func_t)(WO_IPVAL_MOVE_8);

                if (dr & 0b10)
                    x86_do_calln_native_func_fast(ctx->c, ctx->_vmbase, call_aim_native_func, rt_ip, ctx->_vmssp, ctx->_vmsbp);
                else
                    x86_do_calln_native_func(ctx->c, ctx->_vmbase, call_aim_native_func, rt_ip, ctx->_vmssp, ctx->_vmsbp);
            }
            else
            {
                uint32_t call_aim_vm_func = WO_IPVAL_MOVE_4;
                rt_ip += 4; // skip empty space;

                // Try compile this func
                auto& compiled_funcstat = analyze_function(ctx->env->rt_codes + call_aim_vm_func, ctx->env);
                if (compiled_funcstat.m_state == function_jit_state::state::FAILED)
                {
                    WO_JIT_NOT_SUPPORT;
                }
                x86_do_calln_vm_func(
                    ctx->c, ctx->_vmbase,
                    compiled_funcstat,
                    ctx->env->rt_codes,
                    rt_ip, ctx->_vmssp, ctx->_vmsbp, ctx->_vmtc);
            }

            // ATTENTION: AFTER CALLING VM FUNCTION, DONOT MODIFY SP/BP/IP CONTEXT, HERE MAY HAPPEND/PASS BREAK INFO!!!
            make_checkpoint(ctx->c, ctx->_vmbase, ctx->_vmssp, ctx->_vmsbp, rt_ip);
            return true;
        }
        virtual bool ir_ret(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            if (dr != 0)
                (void)WO_IPVAL_MOVE_2;

            wo_asure(!ctx->c.ret());
            return true;
        }
        virtual bool ir_jt(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            auto check_point_ipaddr = rt_ip - 1;
            uint32_t jmp_place = WO_IPVAL_MOVE_4;

            if (jmp_place < rt_ip - ctx->env->rt_codes)
                make_checkpoint(ctx->c, ctx->_vmbase, ctx->_vmssp, ctx->_vmsbp, check_point_ipaddr);

            wo_asure(!ctx->c.cmp(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, handle)), 0));
            wo_asure(!ctx->c.jne(jump_ip(&ctx->c, jmp_place)));
            return true;
        }
        virtual bool ir_jf(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            auto check_point_ipaddr = rt_ip - 1;
            uint32_t jmp_place = WO_IPVAL_MOVE_4;

            if (jmp_place < rt_ip - ctx->env->rt_codes)
                make_checkpoint(ctx->c, ctx->_vmbase, ctx->_vmssp, ctx->_vmsbp, check_point_ipaddr);

            wo_asure(!ctx->c.cmp(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, handle)), 0));
            wo_asure(!ctx->c.je(jump_ip(&ctx->c, jmp_place)));
            return true;
        }
        virtual bool ir_jmp(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            auto check_point_ipaddr = rt_ip - 1;
            uint32_t jmp_place = WO_IPVAL_MOVE_4;

            if (jmp_place < rt_ip - ctx->env->rt_codes)
                make_checkpoint(ctx->c, ctx->_vmbase, ctx->_vmssp, ctx->_vmsbp, check_point_ipaddr);

            wo_asure(!ctx->c.jmp(jump_ip(&ctx->c, jmp_place)));
            return true;
        }
        virtual bool ir_mkunion(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;
            uint16_t id = WO_IPVAL_MOVE_2;

            auto op1 = opnum1.gp_value();
            auto op2 = opnum2.gp_value();

            asmjit::InvokeNode* invoke_node;
            wo_asure(!ctx->c.invoke(&invoke_node, (size_t)&vm::make_union_impl,
                asmjit::FuncSignatureT<wo::value*, wo::value*, wo::value*, uint16_t>()));

            invoke_node->setArg(0, op1);
            invoke_node->setArg(1, op2);
            invoke_node->setArg(2, asmjit::Imm(id));
            invoke_node->setRet(0, ctx->_vmssp);
            return true;
        }
        virtual bool ir_movcast(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_NOT_SUPPORT;
        }
        virtual bool ir_mkclos(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_NOT_SUPPORT;
        }
        virtual bool ir_typeas(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_NOT_SUPPORT;
        }
        virtual bool ir_mkstruct(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            uint16_t size = WO_IPVAL_MOVE_2;

            auto op1 = opnum1.gp_value();

            asmjit::InvokeNode* invoke_node;
            wo_asure(!ctx->c.invoke(&invoke_node, (size_t)&vm::make_struct_impl,
                asmjit::FuncSignatureT< wo::value*, wo::value*, uint16_t, wo::value*>()));

            invoke_node->setArg(0, op1);
            invoke_node->setArg(1, asmjit::Imm(size));
            invoke_node->setArg(2, ctx->_vmssp);
            invoke_node->setRet(0, ctx->_vmssp);
            return true;
        }
        virtual bool ir_abrt(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_NOT_SUPPORT;
        }
        virtual bool ir_idarr(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto op1 = opnum1.gp_value();
            auto op2 = opnum2.gp_value();

            asmjit::InvokeNode* invoke_node;
            wo_asure(!ctx->c.invoke(&invoke_node, (size_t)&_vmjitcall_idarr,
                asmjit::FuncSignatureT< void, wo::value*, wo::value*, wo::value*>()));

            invoke_node->setArg(0, ctx->_vmcr);
            invoke_node->setArg(1, op1);
            invoke_node->setArg(2, op2);
            return true;
        }
        virtual bool ir_iddict(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto op1 = opnum1.gp_value();
            auto op2 = opnum2.gp_value();

            asmjit::InvokeNode* invoke_node;
            wo_asure(!ctx->c.invoke(&invoke_node, (size_t)&_vmjitcall_iddict,
                asmjit::FuncSignatureT< void, wo::value*, wo::value*, wo::value*>()));

            invoke_node->setArg(0, ctx->_vmcr);
            invoke_node->setArg(1, op1);
            invoke_node->setArg(2, op2);
            return true;
        }
        virtual bool ir_mkarr(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            uint16_t size = WO_IPVAL_MOVE_2;

            auto op1 = opnum1.gp_value();

            asmjit::InvokeNode* invoke_node;
            wo_asure(!ctx->c.invoke(&invoke_node, (size_t)&vm::make_array_impl,
                asmjit::FuncSignatureT< wo::value*, wo::value*, uint16_t, wo::value*>()));

            invoke_node->setArg(0, op1);
            invoke_node->setArg(1, asmjit::Imm(size));
            invoke_node->setArg(2, ctx->_vmssp);
            invoke_node->setRet(0, ctx->_vmssp);
            return true;
        }
        virtual bool ir_mkmap(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            uint16_t size = WO_IPVAL_MOVE_2;

            auto op1 = opnum1.gp_value();

            asmjit::InvokeNode* invoke_node;
            wo_asure(!ctx->c.invoke(&invoke_node, (size_t)&vm::make_map_impl,
                asmjit::FuncSignatureT< wo::value*, wo::value*, uint16_t, wo::value*>()));

            invoke_node->setArg(0, op1);
            invoke_node->setArg(1, asmjit::Imm(size));
            invoke_node->setArg(2, ctx->_vmssp);
            invoke_node->setRet(0, ctx->_vmssp);
            return true;
        }
        virtual bool ir_idstr(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_NOT_SUPPORT;
        }
        virtual bool ir_equr(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_NOT_SUPPORT;
        }
        virtual bool ir_nequr(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_NOT_SUPPORT;
        }
        virtual bool ir_equs(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_NOT_SUPPORT;
        }
        virtual bool ir_nequs(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_NOT_SUPPORT;
        }
        virtual bool ir_siddict(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;
            WO_JIT_ADDRESSING_N3_REG_BPOFF;

            auto op1 = opnum1.gp_value();
            auto op2 = opnum2.gp_value();
            auto op3 = opnum3.gp_value();

            asmjit::InvokeNode* invoke_node;
            wo_asure(!ctx->c.invoke(&invoke_node, (size_t)&_vmjitcall_siddict,
                asmjit::FuncSignatureT< void, wo::value*, wo::value*, wo::value*>()));

            invoke_node->setArg(0, op1);
            invoke_node->setArg(1, op2);
            invoke_node->setArg(2, op3);
            return true;
        }
        virtual bool ir_jnequb(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            auto check_point_ipaddr = rt_ip - 1;

            WO_JIT_ADDRESSING_N1;
            uint32_t jmp_place = WO_IPVAL_MOVE_4;

            if (jmp_place < rt_ip - ctx->env->rt_codes)
                make_checkpoint(ctx->c, ctx->_vmbase, ctx->_vmssp, ctx->_vmsbp, check_point_ipaddr);

            if (opnum1.is_constant())
                wo_asure(!ctx->c.cmp(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), opnum1.const_value()->integer));
            else
            {
                auto bvalue = ctx->c.newInt64();
                wo_asure(!ctx->c.mov(bvalue, asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer))));
                wo_asure(!ctx->c.cmp(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), bvalue));
            }

            wo_asure(!ctx->c.jne(jump_ip(&ctx->c, jmp_place)));
            return true;
        }
        virtual bool ir_idstruct(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;
            uint16_t offset = WO_IPVAL_MOVE_2;

            auto op1 = opnum1.gp_value();
            auto op2 = opnum2.gp_value();

            asmjit::InvokeNode* invoke_node;
            wo_asure(!ctx->c.invoke(&invoke_node, (size_t)&_vmjitcall_idstruct,
                asmjit::FuncSignatureT< void, wo::value*, wo::value*, uint16_t>()));

            invoke_node->setArg(0, op1);
            invoke_node->setArg(1, op2);
            invoke_node->setArg(2, asmjit::Imm(offset));
            return true;
        }
        virtual bool ir_ext_panic(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            auto op1 = opnum1.gp_value();

            asmjit::InvokeNode* invoke_node;
            wo_asure(!ctx->c.invoke(&invoke_node, (size_t)&_vmjitcall_panic,
                asmjit::FuncSignatureT<void, wo::value*>()));

            invoke_node->setArg(0, op1);
            return true;
        }

#undef WO_JIT_ADDRESSING_N1
#undef WO_JIT_ADDRESSING_N2
#undef WO_JIT_ADDRESSING_N3_REG_BPOFF
#undef WO_JIT_NOT_SUPPORT
    };

    struct AArch64CompileContext
    {
        asmjit::a64::Compiler c;
        asmjit::a64::Gp _vmbase;
        asmjit::a64::Gp _vmsbp;
        asmjit::a64::Gp _vmssp;
        asmjit::a64::Gp _vmreg;
        asmjit::a64::Gp _vmglb;
        asmjit::a64::Gp _vmcr;
        asmjit::a64::Gp _vmtc;

        runtime_env* env;

        std::unordered_map<wo_integer_t, asmjit::a64::Gp> m_int_constant_pool;
        std::unordered_map<wo_real_t, asmjit::a64::Vec> m_f64_constant_pool;
        std::list<std::function<void(void)>> m_generate_list;
        asmjit::a64::Gp load_int64(wo_integer_t val)
        {
            auto fnd = m_int_constant_pool.find(val);
            if (fnd != m_int_constant_pool.end())
                return fnd->second;

            auto gp = c.newInt64();
            c.mov(gp, val);
            return m_int_constant_pool[val] = gp;
        }
        asmjit::a64::Vec load_float64(wo_real_t val)
        {
            auto fnd = m_f64_constant_pool.find(val);
            if (fnd != m_f64_constant_pool.end())
                return fnd->second;

            auto dvec = c.newVecD();
            c.fmov(dvec, val);
            return m_f64_constant_pool[val] = dvec;
        }
        void generate(const std::function<void(void)>& opt)
        {
            m_generate_list.push_back(opt);
        }

        AArch64CompileContext(asmjit::CodeHolder* code, runtime_env* _env)
            : c(code)
            , env(_env)
        {
            _vmbase = c.newUIntPtr();
            _vmsbp = c.newUIntPtr();
            _vmssp = c.newUIntPtr();
            _vmreg = c.newUIntPtr();
            _vmcr = c.newUIntPtr();
            _vmtc = c.newUIntPtr();
        }
        AArch64CompileContext(const AArch64CompileContext&) = delete;
        AArch64CompileContext(AArch64CompileContext&&) = delete;
        AArch64CompileContext& operator = (const AArch64CompileContext&) = delete;
        AArch64CompileContext& operator = (AArch64CompileContext&&) = delete;
    };

    struct asmjit_compiler_aarch64 : public asmjit_backend<AArch64CompileContext>
    {
        struct aarch64_addresing
        {
            enum class addressing_type
            {
                BPOFFSET,
                REGISTER,
                GLOBAL,
            };
            addressing_type m_type;
            ptrdiff_t m_offset;
            const value* m_constant;

            aarch64_addresing(AArch64CompileContext* context, const byte_t*& rt_ip, bool dr, runtime_env* env)
                :m_constant(nullptr)
            {
                if (dr)
                {
                    // opnum from bp-offset or regist
                    auto offset_1b = WO_IPVAL_MOVE_1;
                    if (offset_1b & (1 << 7))
                    {
                        // from bp-offset
                        m_type = addressing_type::BPOFFSET;
                        m_offset = WO_SIGNED_SHIFT(offset_1b) * sizeof(value);
                    }
                    else
                    {
                        // from reg
                        m_type = addressing_type::REGISTER;
                        m_offset = offset_1b * sizeof(value);
                    }
                }
                else
                {
                    // opnum from global_const
                    m_type = addressing_type::GLOBAL;
                    size_t offset = (size_t)WO_SAFE_READ_MOVE_4;

                    if (offset < env->constant_value_count)
                    {
                        m_constant = env->constant_global_reg_rtstack + offset;
                        if (m_constant->type != value::valuetype::real_type)
                            context->load_int64(m_constant->integer);
                        else
                            context->load_float64(m_constant->real);
                    }
                    m_offset = offset * sizeof(value);
                }
            }
            asmjit::a64::Gp get_addr(AArch64CompileContext* context)const
            {
                auto addr = context->c.newIntPtr();
                if (m_type == addressing_type::BPOFFSET)
                {
                    wo_asure(!context->c.mov(addr, m_offset));
                    wo_asure(!context->c.add(addr, addr, context->_vmsbp));
                }
                else if (m_type == addressing_type::REGISTER)
                {
                    wo_asure(!context->c.mov(addr, m_offset));
                    wo_asure(!context->c.add(addr, addr, context->_vmreg));
                }
                else
                {
                    wo_asure(!context->c.mov(addr, m_offset));
                    wo_asure(!context->c.add(addr, addr, context->_vmglb));
                }
                return addr;
            }
            asmjit::a64::Mem get_value(AArch64CompileContext* context)const
            {
                if (m_type == addressing_type::BPOFFSET)
                {
                    return asmjit::a64::Mem(context->_vmsbp, (int32_t)(m_offset + offsetof(value, integer)));
                }
                else if (m_type == addressing_type::REGISTER)
                {
                    return asmjit::a64::Mem(context->_vmreg, (int32_t)(m_offset + offsetof(value, integer)));
                }
                else
                {
                    return asmjit::a64::Mem(context->_vmglb, (int32_t)(m_offset + offsetof(value, integer)));
                }
            }
            asmjit::a64::Mem get_type(AArch64CompileContext* context) const
            {
                if (m_type == addressing_type::BPOFFSET)
                {
                    return asmjit::a64::Mem(context->_vmsbp, (int32_t)(m_offset + offsetof(value, type)));
                }
                else if (m_type == addressing_type::REGISTER)
                {
                    return asmjit::a64::Mem(context->_vmreg, (int32_t)(m_offset + offsetof(value, type)));
                }
                else
                {
                    return asmjit::a64::Mem(context->_vmglb, (int32_t)(m_offset + offsetof(value, type)));
                }
            }
        };

        template<typename T>
        static asmjit::a64::Mem intptr_ptr(const T& opgreg, int32_t offset = 0)
        {
            return asmjit::a64::Mem(opgreg, offset);
        }

        virtual AArch64CompileContext* prepare_compiler(
            asmjit::BaseCompiler** out_compiler,
            asmjit::CodeHolder* code,
            function_jit_state* state,
            runtime_env* env)override
        {
            AArch64CompileContext* ctx = new AArch64CompileContext(code, env);
            *out_compiler = &ctx->c;

            state->m_jitfunc = ctx->c.addFunc(
                asmjit::FuncSignatureT<wo_result_t, vmbase*, value*, size_t>());
            // void _jit_(vmbase*  vm , value* bp, value* reg, value* const_global);

            // 0. Get vmptr reg stack base global ptr.
            state->m_jitfunc->setArg(0, ctx->_vmbase);
            state->m_jitfunc->setArg(1, ctx->_vmsbp);
            wo_asure(!ctx->c.sub(ctx->_vmsbp, ctx->_vmsbp, 2 * sizeof(wo::value)));
            wo_asure(!ctx->c.mov(ctx->_vmssp, ctx->_vmsbp));                    // let sp = bp;
            wo_asure(!ctx->c.ldr(ctx->_vmglb, intptr_ptr(ctx->_vmbase, offsetof(vmbase, const_global_begin))));
            wo_asure(!ctx->c.ldr(ctx->_vmreg, intptr_ptr(ctx->_vmbase, offsetof(vmbase, register_mem_begin))));
            wo_asure(!ctx->c.ldr(ctx->_vmcr, intptr_ptr(ctx->_vmbase, offsetof(vmbase, cr))));
            wo_asure(!ctx->c.ldr(ctx->_vmtc, intptr_ptr(ctx->_vmbase, offsetof(vmbase, tc))));

            return ctx;
        }

        virtual void finish_compiler(AArch64CompileContext* context)override
        {
            for (auto& f : context->m_generate_list)
                f();

            wo_asure(!context->c.endFunc());
            wo_asure(!context->c.finalize());
        }
        virtual void free_compiler(AArch64CompileContext* context) override
        {
            delete context;
        }

        inline static const wo::value NIL_VALUE = {};

        void a64_set_imm(AArch64CompileContext* ctx, const aarch64_addresing& target, const wo::value* immval)
        {
            wo_asure(!ctx->c.str(ctx->load_int64(immval->integer), target.get_value(ctx)));
            auto tmp = ctx->c.newGpw();
            wo_asure(!ctx->c.mov(tmp, immval->type));
            wo_asure(!ctx->c.strb(tmp, target.get_type(ctx)));
        }
        void a64_set_val(AArch64CompileContext* ctx, const aarch64_addresing& target, const aarch64_addresing& val)
        {
            auto tmp = ctx->c.newInt64();
            wo_asure(!ctx->c.ldr(tmp, val.get_value(ctx)));
            wo_asure(!ctx->c.str(tmp, target.get_value(ctx)));
            tmp = ctx->c.newGpw();
            wo_asure(!ctx->c.ldrb(tmp, val.get_type(ctx)));
            wo_asure(!ctx->c.strb(tmp, target.get_type(ctx)));
        }
        void a64_set_imm_to_addr(AArch64CompileContext* ctx, asmjit::a64::Gp& addr, const wo::value* immval)
        {
            wo_asure(!ctx->c.str(ctx->load_int64(immval->integer), asmjit::a64::Mem(addr, offsetof(value, handle))));
            auto tmp = ctx->c.newGpw();
            wo_asure(!ctx->c.mov(tmp, immval->type));
            wo_asure(!ctx->c.strb(tmp, asmjit::a64::Mem(addr, offsetof(value, type))));
        }
        void a64_set_val_to_addr(AArch64CompileContext* ctx, asmjit::a64::Gp& addr, const aarch64_addresing& val)
        {
            auto tmp = ctx->c.newInt64();
            wo_asure(!ctx->c.ldr(tmp, val.get_value(ctx)));
            wo_asure(!ctx->c.str(tmp, asmjit::a64::Mem(addr, offsetof(value, handle))));
            tmp = ctx->c.newGpw();
            wo_asure(!ctx->c.ldrb(tmp, val.get_type(ctx)));
            wo_asure(!ctx->c.strb(tmp, asmjit::a64::Mem(addr, offsetof(value, type))));
        }

        static void make_checkpoint(asmjit::a64::Compiler& x86compiler, asmjit::a64::Gp rtvm, asmjit::a64::Gp stack_sp, asmjit::a64::Gp stack_bp, const byte_t* ip)
        {
            // todo;
        }

#define WO_JIT_ADDRESSING_N1 aarch64_addresing opnum1(ctx, rt_ip, (dr & 0b10),  ctx->env)
#define WO_JIT_ADDRESSING_N2 aarch64_addresing opnum2(ctx, rt_ip, (dr & 0b01), ctx->env)
#define WO_JIT_ADDRESSING_N3_REG_BPOFF aarch64_addresing opnum3(ctx, rt_ip, true, ctx->env)
#define WO_JIT_NOT_SUPPORT do{ return false; }while(0)

        virtual bool ir_nop(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return true;
        }
        virtual bool ir_mov(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            ctx->generate([=]()
                {
                    if (opnum2.m_constant != nullptr)
                        a64_set_imm(ctx, opnum1, opnum2.m_constant);
                    else
                        a64_set_val(ctx, opnum1, opnum2);
                });

            return true;
        }
        virtual bool ir_addi(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto tmp = ctx->c.newInt64();
            auto tmp2 = opnum2.m_constant != nullptr
                ? ctx->load_int64(opnum2.m_constant->integer)
                : ctx->c.newInt64();
            ctx->generate([=]()
                {
                    wo_asure(!ctx->c.ldr(tmp, opnum1.get_value(ctx)));

                    if (opnum2.m_constant == nullptr)
                        wo_asure(!ctx->c.ldr(tmp2, opnum2.get_value(ctx)));

                    wo_asure(!ctx->c.add(tmp, tmp, tmp2));
                    wo_asure(!ctx->c.str(tmp, opnum1.get_value(ctx)));
                });
            return true;
        }
        virtual bool ir_subi(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_muli(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_divi(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_modi(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_addr(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_subr(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_mulr(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_divr(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_modr(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_addh(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_subh(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_adds(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_psh(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            if (dr & 0b01)
            {
                // WO_ADDRESSING_N1_REF;
                // (rt_sp--)->set_val(opnum1);

                WO_JIT_ADDRESSING_N1;

                ctx->generate([=]()
                    {
                        if (opnum1.m_constant != nullptr)
                            a64_set_imm_to_addr(ctx, ctx->_vmssp, opnum1.m_constant);
                        else
                            a64_set_val_to_addr(ctx, ctx->_vmssp, opnum1);
                        wo_asure(!ctx->c.sub(ctx->_vmssp, ctx->_vmssp, sizeof(value)));
                    });
            }
            else
            {
                // uint16_t psh_repeat = WO_IPVAL_MOVE_2;
                // for (uint32_t i = 0; i < psh_repeat; i++)
                //      (rt_sp--)->set_nil();

                uint16_t psh_repeat = WO_IPVAL_MOVE_2;
                ctx->generate([=]()
                    {
                        for (uint32_t i = 0; i < psh_repeat; i++)
                        {
                            a64_set_imm_to_addr(ctx, ctx->_vmssp, &NIL_VALUE);
                            wo_asure(!ctx->c.sub(ctx->_vmssp, ctx->_vmssp, sizeof(value)));
                        }});
            }
            return true;
        }
        virtual bool ir_pop(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_sidarr(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_sidstruct(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_lds(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_sts(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_equb(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_nequb(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_lti(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            // <

            auto tmp = opnum1.m_constant != nullptr
                ? ctx->load_int64(opnum1.m_constant->integer)
                : ctx->c.newInt64();
            auto tmp2 = opnum2.m_constant != nullptr
                ? ctx->load_int64(opnum2.m_constant->integer)
                : ctx->c.newInt64();

            auto x86_cmp_fail = ctx->c.newLabel();
            auto x86_cmp_end = ctx->c.newLabel();

            auto type_tmp = ctx->c.newGpw();

            ctx->generate([=]() {
                wo_asure(!ctx->c.mov(type_tmp, (uint8_t)value::valuetype::bool_type));
                wo_asure(!ctx->c.strb(type_tmp, asmjit::a64::Mem(ctx->_vmcr, offsetof(value, type))));

                if (opnum1.m_constant != nullptr)
                {
                    wo_asure(!ctx->c.ldr(tmp2, opnum2.get_value(ctx)));
                }
                else if (opnum2.m_constant != nullptr)
                {
                    wo_asure(!ctx->c.ldr(tmp, opnum1.get_value(ctx)));
                }
                else
                {
                    wo_asure(!ctx->c.ldr(tmp, opnum1.get_value(ctx)));
                    wo_asure(!ctx->c.ldr(tmp2, opnum2.get_value(ctx)));
                }

                wo_asure(!ctx->c.cmp(tmp, tmp2));
                wo_asure(!ctx->c.b_ge(x86_cmp_fail));

                wo_asure(!ctx->c.mov(type_tmp, 1));
                wo_asure(!ctx->c.b(x86_cmp_end));
                wo_asure(!ctx->c.bind(x86_cmp_fail));
                wo_asure(!ctx->c.mov(type_tmp, 0));
                wo_asure(!ctx->c.bind(x86_cmp_end));

                wo_asure(!ctx->c.strb(type_tmp, asmjit::a64::Mem(ctx->_vmcr, offsetof(value, integer))));
                });
            return true;
        }
        virtual bool ir_gti(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_elti(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_egti(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_land(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_lor(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_sidmap(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_ltx(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_gtx(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_eltx(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_egtx(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_ltr(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_gtr(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_eltr(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_egtr(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_call(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_calln(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_ret(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            if (dr != 0)
                (void)WO_IPVAL_MOVE_2;

            ctx->generate([=]() {
                wo_asure(!ctx->c.ret());
                });
            return true;
        }
        virtual bool ir_jt(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_jf(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            auto check_point_ipaddr = rt_ip - 1;
            uint32_t jmp_place = WO_IPVAL_MOVE_4;

            ctx->generate([=]() {
                if (jmp_place < rt_ip - ctx->env->rt_codes)
                    make_checkpoint(ctx->c, ctx->_vmbase, ctx->_vmssp, ctx->_vmsbp, check_point_ipaddr);

                auto tmp = ctx->c.newInt64();
                wo_asure(!ctx->c.ldr(tmp, asmjit::a64::Mem(ctx->_vmcr, offsetof(value, handle))));
                wo_asure(!ctx->c.cmp(tmp, 0));
                wo_asure(!ctx->c.b_eq(jump_ip(&ctx->c, jmp_place)));
                });
            return true;
        }
        virtual bool ir_jmp(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            auto check_point_ipaddr = rt_ip - 1;
            uint32_t jmp_place = WO_IPVAL_MOVE_4;

            ctx->generate([=]() {
                if (jmp_place < rt_ip - ctx->env->rt_codes)
                    make_checkpoint(ctx->c, ctx->_vmbase, ctx->_vmssp, ctx->_vmsbp, check_point_ipaddr);

                wo_asure(!ctx->c.b(jump_ip(&ctx->c, jmp_place)));
                });
            return true;
        }
        virtual bool ir_mkunion(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_movcast(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_mkclos(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_typeas(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_mkstruct(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_abrt(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_idarr(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_iddict(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_mkarr(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_mkmap(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_idstr(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_equr(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_nequr(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_equs(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_nequs(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_siddict(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_jnequb(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_idstruct(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            return false;
        }
        virtual bool ir_ext_panic(AArch64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;

            ctx->generate([=]() {
                asmjit::InvokeNode* invoke_node;
                wo_asure(!ctx->c.invoke(&invoke_node, (size_t)&_vmjitcall_panic,
                    asmjit::FuncSignatureT<void, wo::value*>()));

                invoke_node->setArg(0, opnum1.get_addr(ctx));
                });
            return true;
        }

#undef WO_JIT_ADDRESSING_N1
#undef WO_JIT_ADDRESSING_N2
#undef WO_JIT_ADDRESSING_N3_REG_BPOFF
#undef WO_JIT_NOT_SUPPORT
    };

    void analyze_jit(byte_t* codebuf, runtime_env* env)
    {
        if constexpr (platform_info::ARCH_TYPE == (platform_info::ArchType::X86 | platform_info::ArchType::BIT64))
        {
            asmjit_compiler_x64 compiler;
            compiler.analyze_jit(codebuf, env);
        }
        else if constexpr (platform_info::ARCH_TYPE == (platform_info::ArchType::ARM | platform_info::ArchType::BIT64))
        {
            asmjit_compiler_aarch64 compiler;
            compiler.analyze_jit(codebuf, env);
        }
        else
        {
            wo_error("No jit function support.");
        }
    }
    void free_jit(runtime_env* env)
    {
        if constexpr (platform_info::ARCH_TYPE == (platform_info::ArchType::X86 | platform_info::ArchType::BIT64))
        {
            asmjit_compiler_x64::free_jit(env);
        }
        else if constexpr (platform_info::ARCH_TYPE == (platform_info::ArchType::ARM | platform_info::ArchType::BIT64))
        {
            asmjit_compiler_aarch64::free_jit(env);
        }
        else
        {
            wo_error("No jit function support.");
        }

    }
}
#else

namespace wo
{
    struct runtime_env;
    void analyze_jit(byte_t* codebuf, runtime_env* env)
    {
    }

    void free_jit(runtime_env* env)
    {
    }


#endif
