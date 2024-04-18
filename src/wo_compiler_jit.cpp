#include "wo_compiler_jit.hpp"
#include "wo_instruct.hpp"
#include "wo_vm.hpp"

#undef FAILED

#include "wo_compiler_jit.hpp"
#include "wo_compiler_ir.hpp"

#if WO_JIT_SUPPORT_ASMJIT

#ifndef ASMJIT_STATIC
#define ASMJIT_STATIC
#endif
#include "asmjit/asmjit.h"

#include <unordered_map>
#include <unordered_set>
#include <list>
namespace wo
{
#define WO_SAFE_READ_OFFSET_GET_QWORD (*(uint64_t*)(rt_ip-8))
#define WO_SAFE_READ_OFFSET_GET_DWORD (*(uint32_t*)(rt_ip-4))
#define WO_SAFE_READ_OFFSET_GET_WORD (*(uint16_t*)(rt_ip-2))

    // FOR BigEndian
#define WO_SAFE_READ_OFFSET_PER_BYTE(OFFSET, TYPE) (((TYPE)(*(rt_ip-OFFSET)))<<((sizeof(TYPE)-OFFSET)*8))
#define WO_IS_ODD_IRPTR(ALLIGN) 1 // NOTE: Always odd for safe reading.

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

#define WO_IPVAL_MOVE_2 WO_SAFE_READ_MOVE_2
#define WO_IPVAL_MOVE_4 WO_SAFE_READ_MOVE_4
#define WO_IPVAL_MOVE_8 WO_SAFE_READ_MOVE_8

#define WO_SIGNED_SHIFT(VAL) (((signed char)((unsigned char)(((unsigned char)(VAL))<<1)))>>1)

#define WO_VM_FAIL(ERRNO,ERRINFO) {ip = rt_ip;sp = rt_sp;bp = rt_bp;wo_fail(ERRNO,ERRINFO);continue;}

    template<typename CompileContextT>
    class asmjit_backend
    {
    public:
        using jit_packed_func_t = wo_native_func_t; // x(vm, bp + 2, argc/* useless */)
        struct function_jit_state
        {
            enum state
            {
                BEGIN,
                COMPILING,
                FINISHED,
                FAILED,
            };
            size_t              m_func_offset = 0;
            state               m_state = state::BEGIN;
            jit_packed_func_t*  m_func = nullptr;
            asmjit::FuncNode*   m_jitfunc = nullptr;
            bool                m_finished = false;
            asmjit::CodeHolder  m_code_buffer;
            CompileContextT*    _m_ctx = nullptr;
        };
    private:
        std::unordered_map<const byte_t*, function_jit_state*>
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
WO_ASMJIT_IR_ITERFACE_DECL(idstruct)\
WO_ASMJIT_IR_ITERFACE_DECL(unpackargs)

#define WO_ASMJIT_IR_ITERFACE_DECL(IRNAME) virtual bool ir_##IRNAME(CompileContextT* ctx, unsigned int dr, const byte_t*& rt_ip) = 0;
        IRS

            virtual bool ir_ext_panic(CompileContextT* ctx, unsigned int dr, const byte_t*& rt_ip) = 0;
        virtual bool ir_ext_packargs(CompileContextT* ctx, unsigned int dr, const byte_t*& rt_ip) = 0;
        virtual bool ir_ext_cdivilr(CompileContextT* ctx, unsigned int dr, const byte_t*& rt_ip) = 0;
        virtual bool ir_ext_cdivil(CompileContextT* ctx, unsigned int dr, const byte_t*& rt_ip) = 0;
        virtual bool ir_ext_cdivirz(CompileContextT* ctx, unsigned int dr, const byte_t*& rt_ip) = 0;
        virtual bool ir_ext_cdivir(CompileContextT* ctx, unsigned int dr, const byte_t*& rt_ip) = 0;

        virtual void ir_make_checkpoint(CompileContextT* ctx, const byte_t*& rt_ip) = 0;
#undef WO_ASMJIT_IR_ITERFACE_DECL

        std::map<uint32_t, asmjit::Label> label_table;
        std::unordered_map<function_jit_state*, std::unordered_set<function_jit_state*>> _dependence_jit_function;
        function_jit_state* current_jit_state = nullptr;

        void register_dependence_function(function_jit_state* depend)
        {
            _dependence_jit_function[depend].insert(current_jit_state);
        }

        virtual void bind_ip(CompileContextT* ctx, asmjit::BaseCompiler* compiler, uint32_t ipoffset)
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

        function_jit_state* _analyze_function(
            const byte_t* rt_ip,
            runtime_env* env,
            function_jit_state* state,
            CompileContextT* ctx,
            asmjit::BaseCompiler* compiler) noexcept
        {
            byte_t              opcode_dr = (byte_t)(instruct::abrt << 2);
            instruct::opcode    opcode = (instruct::opcode)(opcode_dr & 0b11111100u);
            unsigned int        dr = opcode_dr & 0b00000011u;

            ir_make_checkpoint(ctx, rt_ip);

            for (;;)
            {
                uint32_t current_ip_byteoffset = (uint32_t)(rt_ip - m_codes);

                bind_ip(ctx, compiler, current_ip_byteoffset);

                opcode_dr = *(rt_ip++);
                opcode = (instruct::opcode)(opcode_dr & 0b11111100u);
                dr = opcode_dr & 0b00000011u;

#define WO_JIT_NOT_SUPPORT do{state->m_state = function_jit_state::state::FAILED; return state; }while(0)

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
                                if (ir_ext_packargs(ctx, dr, rt_ip))
                                    break;
                                else
                                    WO_JIT_NOT_SUPPORT;
                            case instruct::extern_opcode_page_0::panic:
                                if (ir_ext_panic(ctx, dr, rt_ip))
                                    break;
                                else
                                    WO_JIT_NOT_SUPPORT;
                            case instruct::extern_opcode_page_0::cdivilr:
                                if (ir_ext_cdivilr(ctx, dr, rt_ip))
                                    break;
                                else
                                    WO_JIT_NOT_SUPPORT;
                            case instruct::extern_opcode_page_0::cdivil:
                                if (ir_ext_cdivil(ctx, dr, rt_ip))
                                    break;
                                else
                                    WO_JIT_NOT_SUPPORT;
                            case instruct::extern_opcode_page_0::cdivirz:
                                if (ir_ext_cdivirz(ctx, dr, rt_ip))
                                    break;
                                else
                                    WO_JIT_NOT_SUPPORT;
                            case instruct::extern_opcode_page_0::cdivir:
                                if (ir_ext_cdivir(ctx, dr, rt_ip))
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
                                state->m_state = function_jit_state::state::FINISHED;

                                wo_assert(state->m_finished == false);

                                wo_assert(state->_m_ctx == ctx);
                                this->finish_compiler(ctx);
                                auto err = get_jit_runtime().add(state->m_func, &state->m_code_buffer);
                                if (err != 0)
                                {
                                    static_assert(std::is_same<decltype(err), uint32_t>::value);
                                    fprintf(stderr, "Failed to create jit-function: (%u)\n", err);
                                    abort();
                                }

                                state->m_finished = true;

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

        function_jit_state* analyze_function(const byte_t* code, size_t offset, runtime_env* env) noexcept
        {
            using namespace asmjit;

            auto* backup = current_jit_state;
            auto* rt_ip = code + offset;

            auto fnd = m_compiling_functions.find(rt_ip);
            if (fnd != m_compiling_functions.end())
                return fnd->second;

            function_jit_state*& state = m_compiling_functions[rt_ip];
            state = new function_jit_state;

            state->m_func = new jit_packed_func_t{};
            *state->m_func = (jit_packed_func_t)(void*)(intptr_t)0x12345678;
            current_jit_state = state;

            state->m_state = function_jit_state::state::COMPILING;
            state->m_func_offset = offset;
            WooJitErrorHandler woo_jit_error_handler;

            state->m_code_buffer.init(get_jit_runtime().environment());
            state->m_code_buffer.setErrorHandler(&woo_jit_error_handler);

            asmjit::BaseCompiler* compiler;
            state->_m_ctx = this->prepare_compiler(&compiler, &state->m_code_buffer, state, env);
            compiler->setErrorHandler(&woo_jit_error_handler);

            auto* result = _analyze_function(rt_ip, env, state, state->_m_ctx, compiler);

            current_jit_state = backup;
            return result;
        }

        void analyze_jit(byte_t* codebuf, runtime_env* env) noexcept
        {
            m_codes = codebuf;

            // 1. for all function, trying to jit compile them:
            for (size_t func_offset : env->_functions_offsets_for_jit)
                analyze_function(codebuf, func_offset, env);

            for (auto& [_, stat] : m_compiling_functions)
            {
                if (stat->m_state != function_jit_state::FINISHED)
                {
                    wo_assert(stat->m_state == function_jit_state::FAILED);
                    for (auto& dependence : _dependence_jit_function[stat])
                    {
                        dependence->m_state = function_jit_state::FAILED;
                    }
                }
            }
            for (auto& [_, stat] : m_compiling_functions)
            {
                wo_assert(nullptr != stat->m_func);

                if (stat->m_state == function_jit_state::FINISHED)
                {
                    wo_assert(stat->m_finished);

                    wo_assert(nullptr != *stat->m_func);
                    env->_jit_functions[(void*)*stat->m_func] = stat->m_func_offset;
                    env->_jit_code_holder[stat->m_func_offset] = stat->m_func;
                }
                else
                    delete stat->m_func;

                this->free_compiler(stat->_m_ctx);
            }
            _dependence_jit_function.clear();

            for (size_t funtions_constant_offset : env->_functions_def_constant_idx_for_jit)
            {
                auto* val = &env->constant_global[funtions_constant_offset];
                wo_assert(val->type == value::valuetype::integer_type);

                auto holder = env->_jit_code_holder[(size_t)val->integer];
                wo_assert(holder != nullptr && *holder != nullptr);

                val->type = value::valuetype::handle_type;
                val->handle = (wo_handle_t)(void*)*holder;
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
                if (func_state->m_state == function_jit_state::state::FINISHED)
                {
                    wo_assert(func_state->m_func != nullptr
                        && *func_state->m_func != nullptr);

                    *calln = (wo::instruct::opcode)(wo::instruct::opcode::calln | 0b11);
                    byte_t* jitfunc = (byte_t*)func_state->m_func;
                    byte_t* ipbuf = codebuf + calln_offset + 1;

                    for (size_t i = 0; i < 8; ++i)
                        *(ipbuf + i) = *(jitfunc + i);
                }
                else
                    wo_assert(func_state->m_state == function_jit_state::state::FAILED);
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
                if (func_state->m_state == function_jit_state::state::FINISHED)
                {
                    wo_assert(func_state->m_func != nullptr
                        && *func_state->m_func != nullptr);

                    *mkclos = (wo::instruct::opcode)(wo::instruct::opcode::mkclos | 0b10);
                    byte_t* jitfunc = (byte_t*)func_state->m_func;
                    byte_t* ipbuf = codebuf + mkclos_offset + 1 + 2;

                    for (size_t i = 0; i < 8; ++i)
                        *(ipbuf + i) = *(jitfunc + i);
                }
                else
                    wo_assert(func_state->m_state == function_jit_state::state::FAILED);
            }

            for (auto& [_, stat] : m_compiling_functions)
                delete stat;
            m_compiling_functions.clear();
        }

        static void free_jit(runtime_env* env)
        {
            for (auto& holder : env->_jit_code_holder)
            {
                wo_assure(!get_jit_runtime().release(*holder.second));
                delete holder.second;
            }
        }

        static wo_result_t _invoke_vm_checkpoint(wo::vmbase* vmm, wo::value* rt_sp, wo::value* rt_bp, const byte_t* rt_ip)
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
                // store current context, then break out of jit function
                vmm->ip = rt_ip;
                vmm->sp = rt_sp;
                vmm->bp = rt_bp;

                return wo_result_t::WO_API_RESYNC;
            }
            else if (vmm->vm_interrupt & wo::vmbase::vm_interrupt_type::BR_YIELD_INTERRUPT)
            {
                // wo_assure(vmm->clear_interrupt(wo::vmbase::vm_interrupt_type::BR_YIELD_INTERRUPT));
                if (vmm->get_br_yieldable())
                {
                    // store current context, then break out of jit function
                    vmm->ip = rt_ip;
                    vmm->sp = rt_sp;
                    vmm->bp = rt_bp;
                    // NOTE: DONOT CLEAR BR_YIELD_INTERRUPT, IT SHOULD BE CLEAR IN VM-RUN

                    vmm->mark_br_yield();
                    return wo_result_t::WO_API_RESYNC; // return 
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
            else if (vmm->vm_interrupt & wo::vmbase::vm_interrupt_type::DETACH_DEBUGGEE_INTERRUPT)
            {
                if (vmm->clear_interrupt(wo::vmbase::vm_interrupt_type::DETACH_DEBUGGEE_INTERRUPT))
                    vmm->clear_interrupt(wo::vmbase::vm_interrupt_type::DEBUG_INTERRUPT);
            }
            // ATTENTION: it should be last interrupt..
            else if (vmm->vm_interrupt & wo::vmbase::vm_interrupt_type::DEBUG_INTERRUPT)
            {
                vmm->ip = rt_ip;
                vmm->sp = rt_sp;
                vmm->bp = rt_bp;
                return wo_result_t::WO_API_RESYNC; // return 
            }
            else
            {
                // a vm_interrupt is invalid now, just roll back one byte and continue~
                // so here do nothing
            }
            return wo_result_t::WO_API_NORMAL;
        }
        static wo_result_t native_do_calln_nativefunc(vmbase* vm, wo_extern_native_func_t call_aim_native_func, uint32_t retip, value* rt_sp, value* rt_bp)
        {
            rt_sp->type = value::valuetype::callstack;
            rt_sp->vmcallstack.ret_ip = retip;
            rt_sp->vmcallstack.bp = (uint32_t)(vm->stack_mem_begin - rt_bp);
            rt_bp = --rt_sp;
            vm->bp = vm->sp = rt_sp;

            // May be useless?
            vm->cr->set_nil();

            vm->ip = reinterpret_cast<byte_t*>(call_aim_native_func);

            wo_assure(wo_leave_gcguard(reinterpret_cast<wo_vm>(vm)));
            wo_result_t result = call_aim_native_func(reinterpret_cast<wo_vm>(vm), reinterpret_cast<wo_value>(rt_sp + 2));
            wo_assure(wo_enter_gcguard(reinterpret_cast<wo_vm>(vm)));

            return result;
        }
        static wo_result_t native_do_calln_vmfunc(vmbase* vm, wo_extern_native_func_t call_aim_native_func, uint32_t retip, value* rt_sp, value* rt_bp)
        {
            rt_sp->type = value::valuetype::callstack;
            rt_sp->vmcallstack.ret_ip = retip;
            rt_sp->vmcallstack.bp = (uint32_t)(vm->stack_mem_begin - rt_bp);
            rt_bp = --rt_sp;
            vm->bp = vm->sp = rt_sp;

            // May be useless?
            vm->cr->set_nil();

            vm->ip = reinterpret_cast<byte_t*>(call_aim_native_func);

            return call_aim_native_func(reinterpret_cast<wo_vm>(vm), reinterpret_cast<wo_value>(rt_sp + 2));
        }
        static wo_result_t native_do_call_vmfunc(vmbase* vm, value* target_function, uint32_t retip, value* rt_sp, value* rt_bp)
        {
            switch (target_function->type)
            {
            case value::valuetype::handle_type:
                return native_do_calln_vmfunc(vm, (wo_extern_native_func_t)(void*)target_function->handle, retip, rt_sp, rt_bp);
            case value::valuetype::closure_type:
            {
                wo_assert(target_function->closure->m_native_call);
                for (auto idx = target_function->closure->m_closure_args_count; idx > 0; --idx)
                    (rt_sp--)->set_val(&target_function->closure->m_closure_args[idx - 1]);
                return native_do_calln_vmfunc(vm, target_function->closure->m_native_func, retip, rt_sp, rt_bp);
            }
            default:
                wo_fail(WO_FAIL_CALL_FAIL, "Unexpected function type when invoked in jit.");
            }
            return wo_result_t::WO_API_NORMAL;
        }
        static void _vmjitcall_panic(vmbase* vm, wo::value* opnum1, const byte_t* rt_ip, value* rt_sp, value* rt_bp)
        {
            vm->ip = rt_ip;
            vm->sp = rt_sp;
            vm->bp = rt_bp;

            wo_fail(WO_FAIL_UNEXPECTED, "%s", wo_cast_string(reinterpret_cast<wo_value>(opnum1)));
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
        static const char* _vmjitcall_idarr(wo::value* cr, wo::value* opnum1, wo::value* opnum2)
        {
            wo_assert(opnum1->type == value::valuetype::array_type && opnum1->array != nullptr);
            wo_assert(opnum2->type == value::valuetype::integer_type);

            gcbase::gc_read_guard gwg1(opnum1->gcunit);

            size_t index = opnum2->integer;
            if (opnum2->integer < 0)
                index = opnum1->array->size() + opnum2->integer;
            if (index >= opnum1->array->size())
            {
                return "Index out of range.";
            }
            else
            {
                auto* result = &opnum1->array->at(index);
                cr->set_val(result);
            }
            return nullptr;
        }
        static const char* _vmjitcall_iddict(wo::value* cr, wo::value* opnum1, wo::value* opnum2)
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
                return "No such key in current dict.";
            return nullptr;
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
        static const char* _vmjitcall_siddict(wo::value* opnum1, wo::value* opnum2, wo::value* opnum3)
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
                return "No such key in current dict.";
            return nullptr;
        }
        static void _vmjitcall_sidmap(wo::value* opnum1, wo::value* opnum2, wo::value* opnum3)
        {
            wo_assert(opnum1->type == value::valuetype::dict_type && opnum1->dict != nullptr);

            gcbase::gc_modify_write_guard gwg1(opnum1->gcunit);

            auto* result = &(*opnum1->dict)[*opnum2];
            if (wo::gc::gc_is_marking())
                wo::gcbase::write_barrier(result);
            result->set_val(opnum3);
        }
        static const char* _vmjitcall_sidarr(wo::value* opnum1, wo::value* opnum2, wo::value* opnum3)
        {
            wo_assert(opnum1->type == value::valuetype::array_type && opnum1->array != nullptr);
            wo_assert(opnum2->type == value::valuetype::integer_type);

            gcbase::gc_write_guard gwg1(opnum1->gcunit);

            size_t index = opnum2->integer;
            if (opnum2->integer < 0)
                index = opnum1->array->size() + opnum2->integer;
            if (index >= opnum1->array->size())
            {
                return "Index out of range.";
            }
            else
            {
                auto* result = &opnum1->array->at(index);
                if (wo::gc::gc_is_marking())
                    wo::gcbase::write_barrier(result);
                result->set_val(opnum3);
            }
            return nullptr;
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
        static void _vmjitcall_equs(wo::value* opnum1, wo::value* opnum2, wo::value* opnum3)
        {
            wo_assert(opnum2->type == value::valuetype::string_type);
            wo_assert(opnum3->type == value::valuetype::string_type);

            opnum1->set_bool(
                opnum2->string == opnum3->string
                || *opnum2->string == *opnum3->string
            );
        }
        static void _vmjitcall_nequs(wo::value* opnum1, wo::value* opnum2, wo::value* opnum3)
        {
            wo_assert(opnum2->type == value::valuetype::string_type);
            wo_assert(opnum3->type == value::valuetype::string_type);

            opnum1->set_bool(
                opnum2->string != opnum3->string
                && *opnum2->string != *opnum3->string
            );
        }
        static void _vmjitcall_idstr(wo::value* opnum1, wo::value* opnum2, wo::value* opnum3)
        {
            wo_assert(opnum2->type == value::valuetype::string_type);
            wo_assert(opnum3->type == value::valuetype::integer_type);

            wchar_t out_str = wo_strn_get_char(opnum2->string->c_str(), opnum2->string->size(), opnum3->integer);
            opnum1->set_integer((wo_integer_t)(wo_handle_t)out_str);
        }
        static void _vmjitcall_abrt(const char* msg)
        {
            wo_error(msg);
        }
        static void _vmjitcall_fail(wo::vmbase* vmm, uint32_t id, const char* msg, const byte_t* rt_ip, wo::value* rt_sp, wo::value* rt_bp)
        {
            vmm->ip = rt_ip;
            vmm->sp = rt_sp;
            vmm->bp = rt_bp;
            wo_fail(id, msg);
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
            bool is_constant_and_not_tag(X64CompileContext* ctx) const
            {
                if (is_constant())
                {
                    auto offset = (size_t)
                        (m_constant - ctx->env->constant_global);
                    if (std::find(
                        ctx->env->_functions_def_constant_idx_for_jit.begin(),
                        ctx->env->_functions_def_constant_idx_for_jit.end(),
                        offset) == ctx->env->_functions_def_constant_idx_for_jit.end())
                        return true;
                };
                return false;
            }

            asmjit::x86::Gp gp_value()
            {
                if (m_is_constant && !already_valued)
                {
                    already_valued = true;

                    m_value = compiler->newUIntPtr();
                    wo_assure(!compiler->mov(m_value, (size_t)m_constant));
                }
                return m_value;
            }

            value* const_value() const
            {
                wo_assert(m_is_constant);
                return m_constant;
            }
        };

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
                    wo_assure(!x86compiler.lea(result, asmjit::x86::qword_ptr(stack_bp, WO_SIGNED_SHIFT(WO_IPVAL_MOVE_1) * sizeof(value))));
                    return may_constant_x86Gp{ &x86compiler,false,nullptr,result };
                }
                else
                {
                    // from reg
                    auto result = x86compiler.newUIntPtr();
                    wo_assure(!x86compiler.lea(result, asmjit::x86::qword_ptr(reg, WO_IPVAL_MOVE_1 * sizeof(value))));
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
                    return may_constant_x86Gp{ &x86compiler, true, env->constant_global + const_global_index };
                }
                else
                {
                    auto result = x86compiler.newUIntPtr();
                    wo_assure(!x86compiler.mov(result, (size_t)(env->constant_global + const_global_index)));
                    return may_constant_x86Gp{ &x86compiler,false,nullptr,result };
                }
            }
        }
        virtual void ir_make_checkpoint(X64CompileContext* ctx, const byte_t*& rt_ip) override
        {
            auto no_interrupt_label = ctx->c.newLabel();
            static_assert(sizeof(wo::vmbase::fast_ro_vm_interrupt) == 4);
            wo_assure(!ctx->c.cmp(asmjit::x86::dword_ptr(ctx->_vmbase, offsetof(wo::vmbase, fast_ro_vm_interrupt)), 0));
            wo_assure(!ctx->c.je(no_interrupt_label));

            auto result = ctx->c.newInt32();

            asmjit::InvokeNode* invoke_node;
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&_invoke_vm_checkpoint,
                asmjit::FuncSignatureT<wo_result_t, vmbase*, value*, value*, const byte_t*>()));
            invoke_node->setArg(0, ctx->_vmbase);
            invoke_node->setArg(1, ctx->_vmssp);
            invoke_node->setArg(2, ctx->_vmsbp);
            invoke_node->setArg(3, asmjit::Imm((intptr_t)rt_ip));

            invoke_node->setRet(0, result);

            wo_assure(!ctx->c.cmp(result, asmjit::Imm(wo_result_t::WO_API_NORMAL)));
            wo_assure(!ctx->c.je(no_interrupt_label));
            wo_assure(!ctx->c.ret(result)); // break this execute!!!
            wo_assure(!ctx->c.bind(no_interrupt_label));
        }

        static asmjit::x86::Gp x86_set_imm(asmjit::x86::Compiler& x86compiler, asmjit::x86::Gp val, const wo::value& instance)
        {
            wo_assure(!x86compiler.mov(asmjit::x86::byte_ptr(val, offsetof(value, type)), (uint8_t)instance.type));

            auto data_of_val = x86compiler.newUInt64();
            wo_assure(!x86compiler.mov(data_of_val, instance.handle));
            wo_assure(!x86compiler.mov(asmjit::x86::dword_ptr(val, offsetof(value, handle)), data_of_val));

            return val;
        }
        static asmjit::x86::Gp x86_set_val(asmjit::x86::Compiler& x86compiler, asmjit::x86::Gp val, asmjit::x86::Gp val2)
        {
            auto type_of_val2 = x86compiler.newUInt8();
            wo_assure(!x86compiler.mov(type_of_val2, asmjit::x86::byte_ptr(val2, offsetof(value, type))));
            wo_assure(!x86compiler.mov(asmjit::x86::byte_ptr(val, offsetof(value, type)), type_of_val2));

            auto data_of_val2 = x86compiler.newUInt64();
            wo_assure(!x86compiler.mov(data_of_val2, asmjit::x86::qword_ptr(val2, offsetof(value, handle))));
            wo_assure(!x86compiler.mov(asmjit::x86::qword_ptr(val, offsetof(value, handle)), data_of_val2));

            return val;
        }
        static asmjit::x86::Gp x86_set_nil(asmjit::x86::Compiler& x86compiler, asmjit::x86::Gp val)
        {
            wo_assure(!x86compiler.mov(asmjit::x86::byte_ptr(val, offsetof(value, type)), (uint8_t)value::valuetype::invalid));
            wo_assure(!x86compiler.mov(asmjit::x86::qword_ptr(val, offsetof(value, handle)), asmjit::Imm(0)));
            return val;
        }

        static void x86_do_calln_native_func(asmjit::x86::Compiler& x86compiler,
            asmjit::x86::Gp vm,
            wo_extern_native_func_t call_aim_native_func,
            const byte_t* codes,
            const byte_t* rt_ip,
            asmjit::x86::Gp rt_sp,
            asmjit::x86::Gp rt_bp)
        {
            auto result = x86compiler.newInt32();
            asmjit::InvokeNode* invoke_node;
            wo_assure(!x86compiler.invoke(&invoke_node, (intptr_t)&native_do_calln_nativefunc,
                asmjit::FuncSignatureT<wo_result_t, vmbase*, wo_extern_native_func_t, uint32_t, value*, value*>()));

            invoke_node->setArg(0, vm);
            invoke_node->setArg(1, asmjit::Imm((intptr_t)call_aim_native_func));
            invoke_node->setArg(2, asmjit::Imm((uint32_t)(rt_ip - codes)));
            invoke_node->setArg(3, rt_sp);
            invoke_node->setArg(4, rt_bp);
            invoke_node->setRet(0, result);

            auto normal = x86compiler.newLabel();
            wo_assure(!x86compiler.cmp(result, asmjit::Imm(wo_result_t::WO_API_NORMAL)));
            wo_assure(!x86compiler.je(normal));
            wo_assure(!x86compiler.ret(result)); // break this execute!!!
            wo_assure(!x86compiler.bind(normal));
        }

        static void x86_do_calln_native_func_fast(asmjit::x86::Compiler& x86compiler,
            asmjit::x86::Gp vm,
            wo_extern_native_func_t call_aim_native_func,
            const byte_t* codes,
            const byte_t* rt_ip,
            asmjit::x86::Gp rt_sp,
            asmjit::x86::Gp rt_bp)
        {
            auto result = x86compiler.newInt32();
            asmjit::InvokeNode* invoke_node;
            wo_assure(!x86compiler.invoke(&invoke_node, (intptr_t)&native_do_calln_vmfunc,
                asmjit::FuncSignatureT<wo_result_t, vmbase*, wo_extern_native_func_t, uint32_t, value*, value*>()));

            invoke_node->setArg(0, vm);
            invoke_node->setArg(1, asmjit::Imm((intptr_t)call_aim_native_func));
            invoke_node->setArg(2, asmjit::Imm((uint32_t)(rt_ip - codes)));
            invoke_node->setArg(3, rt_sp);
            invoke_node->setArg(4, rt_bp);
            invoke_node->setRet(0, result);

            auto normal = x86compiler.newLabel();
            wo_assure(!x86compiler.cmp(result, asmjit::Imm(wo_result_t::WO_API_NORMAL)));
            wo_assure(!x86compiler.je(normal));
            wo_assure(!x86compiler.ret(result)); // break this execute!!!
            wo_assure(!x86compiler.bind(normal));
        }

        static void x86_do_calln_vm_func(
            asmjit::x86::Compiler& x86compiler,
            asmjit::x86::Gp vm,
            function_jit_state* vm_func,
            const byte_t* codes,
            const byte_t* rt_ip,
            asmjit::x86::Gp rt_sp,
            asmjit::x86::Gp rt_bp)
        {
            // Set calltrace info here!
            wo::value callstack;
            callstack.type = wo::value::valuetype::callstack;
            callstack.vmcallstack.bp = 0;
            callstack.vmcallstack.ret_ip = (uint32_t)(rt_ip - codes);

            x86_set_imm(x86compiler, rt_sp, callstack);
            auto bpoffset = x86compiler.newUInt64();
            wo_assure(!x86compiler.mov(bpoffset, asmjit::x86::qword_ptr(vm, offsetof(vmbase, stack_mem_begin))));
            wo_assure(!x86compiler.sub(bpoffset, rt_bp));
            wo_assure(!x86compiler.shr(bpoffset, asmjit::Imm(4)));
            wo_assure(!x86compiler.mov(asmjit::x86::dword_ptr(rt_sp, offsetof(value, vmcallstack) + offsetof(value::callstack, bp)), bpoffset.r32()));

            auto callargptr = x86compiler.newUIntPtr();
            wo_assure(!x86compiler.lea(callargptr, asmjit::x86::qword_ptr(rt_sp, 1 * (int32_t)sizeof(value))));

            auto result = x86compiler.newInt32();

            asmjit::InvokeNode* invoke_node;

            if (vm_func->m_finished)
            {
                wo_assert(vm_func->m_func != nullptr
                    && *vm_func->m_func != nullptr);

                wo_assure(!x86compiler.invoke(&invoke_node, *vm_func->m_func,
                    asmjit::FuncSignatureT<wo_result_t, vmbase*, value*>()));
            }
            else
            {
                wo_assert(vm_func->m_state == function_jit_state::state::COMPILING
                    || vm_func->m_state == function_jit_state::state::FINISHED);
                wo_assert(vm_func->m_jitfunc);


                auto funcaddr = x86compiler.newIntPtr();
                x86compiler.mov(funcaddr, asmjit::x86::qword_ptr((intptr_t)vm_func->m_func));

                wo_assure(!x86compiler.invoke(&invoke_node, funcaddr,
                    asmjit::FuncSignatureT<wo_result_t, vmbase*, value*>()));
            }
            invoke_node->setArg(0, vm);
            invoke_node->setArg(1, callargptr);

            invoke_node->setRet(0, result);

            auto normal = x86compiler.newLabel();
            wo_assure(!x86compiler.cmp(result, asmjit::Imm(wo_result_t::WO_API_NORMAL)));
            wo_assure(!x86compiler.je(normal));
            wo_assure(!x86compiler.ret(result)); // break this execute!!!
            wo_assure(!x86compiler.bind(normal));
        }

        template <typename T>
        void x86_do_fail(
            X64CompileContext* ctx,
            uint32_t failcode,
            T failreason,
            const byte_t* rt_ip)
        {
            asmjit::InvokeNode* invoke_node;
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&_vmjitcall_fail,
                asmjit::FuncSignatureT<void, wo::vmbase*, uint32_t, const char*, const byte_t*, wo::value*, wo::value*>()));

            invoke_node->setArg(0, ctx->_vmbase);
            invoke_node->setArg(1, asmjit::Imm(failcode));
            invoke_node->setArg(2, failreason);
            invoke_node->setArg(3, asmjit::Imm((intptr_t)rt_ip));
            invoke_node->setArg(4, ctx->_vmssp);
            invoke_node->setArg(5, ctx->_vmsbp);

            ir_make_checkpoint(ctx, rt_ip);
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
                asmjit::FuncSignatureT<wo_result_t, vmbase*, value*>());
            // void _jit_(vmbase*  vm , value* bp, value* reg, value* const_global);

            // 0. Get vmptr reg stack base global ptr.
            state->m_jitfunc->setArg(0, ctx->_vmbase);
            state->m_jitfunc->setArg(1, ctx->_vmsbp);
            wo_assure(!ctx->c.sub(ctx->_vmsbp, 2 * sizeof(wo::value)));
            wo_assure(!ctx->c.mov(ctx->_vmssp, ctx->_vmsbp));                    // let sp = bp;
            wo_assure(!ctx->c.mov(ctx->_vmreg, asmjit::x86::qword_ptr(ctx->_vmbase, offsetof(vmbase, register_mem_begin))));
            wo_assure(!ctx->c.mov(ctx->_vmcr, asmjit::x86::qword_ptr(ctx->_vmbase, offsetof(vmbase, cr))));
            wo_assure(!ctx->c.mov(ctx->_vmtc, asmjit::x86::qword_ptr(ctx->_vmbase, offsetof(vmbase, tc))));

            return ctx;
        }

        virtual void finish_compiler(X64CompileContext* context)override
        {
            wo_assure(!context->c.endFunc());
            wo_assure(!context->c.finalize());
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

            if (opnum2.is_constant_and_not_tag(ctx))
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
                wo_assure(!ctx->c.add(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer)), opnum2.const_value()->integer));
            else
            {
                auto int_of_op2 = ctx->c.newInt64();
                wo_assure(!ctx->c.mov(int_of_op2, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer))));
                wo_assure(!ctx->c.add(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer)), int_of_op2));
            }
            return true;
        }
        virtual bool ir_subi(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            if (opnum2.is_constant())
            {
                wo_assure(!ctx->c.sub(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer)), opnum2.const_value()->integer));
            }
            else
            {
                auto int_of_op2 = ctx->c.newInt64();
                wo_assure(!ctx->c.mov(int_of_op2, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer))));
                wo_assure(!ctx->c.sub(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer)), int_of_op2));
            }
            return true;
        }
        virtual bool ir_muli(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto int_of_op2 = ctx->c.newInt64();
            if (opnum2.is_constant())
                wo_assure(!ctx->c.mov(int_of_op2, opnum2.const_value()->integer));
            else
                wo_assure(!ctx->c.mov(int_of_op2, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer))));

            wo_assure(!ctx->c.imul(int_of_op2, asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer))));
            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer)), int_of_op2));
            return true;
        }
        virtual bool ir_divi(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto int_of_op1 = ctx->c.newInt64();
            auto int_of_op2 = ctx->c.newInt64();
            auto div_op_num = ctx->c.newInt64();

            wo_assure(!ctx->c.mov(int_of_op1, asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer))));
            if (opnum2.is_constant())
                wo_assure(!ctx->c.mov(int_of_op2, opnum2.const_value()->integer));
            else
                wo_assure(!ctx->c.mov(int_of_op2, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer))));

            wo_assure(!ctx->c.xor_(div_op_num, div_op_num));
            wo_assure(!ctx->c.cqo(div_op_num, int_of_op1));
            wo_assure(!ctx->c.idiv(div_op_num, int_of_op1, int_of_op2));

            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer)), int_of_op1));
            return true;
        }
        virtual bool ir_modi(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto int_of_op1 = ctx->c.newInt64();
            auto int_of_op2 = ctx->c.newInt64();
            auto div_op_num = ctx->c.newInt64();

            wo_assure(!ctx->c.mov(int_of_op1, asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer))));
            if (opnum2.is_constant())
                wo_assure(!ctx->c.mov(int_of_op2, opnum2.const_value()->integer));
            else
                wo_assure(!ctx->c.mov(int_of_op2, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer))));

            wo_assure(!ctx->c.mov(div_op_num, int_of_op1));
            wo_assure(!ctx->c.cqo(int_of_op1, div_op_num));
            wo_assure(!ctx->c.idiv(int_of_op1, div_op_num, int_of_op2));

            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer)), int_of_op1));

            return true;
        }
        virtual bool ir_addr(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto real_of_op1 = ctx->c.newXmm();
            wo_assure(!ctx->c.movsd(real_of_op1, asmjit::x86::ptr(opnum1.gp_value(), offsetof(value, real))));
            wo_assure(!ctx->c.addsd(real_of_op1, asmjit::x86::ptr(opnum2.gp_value(), offsetof(value, real))));
            wo_assure(!ctx->c.movsd(asmjit::x86::ptr(opnum1.gp_value(), offsetof(value, real)), real_of_op1));

            return true;
        }
        virtual bool ir_subr(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto real_of_op1 = ctx->c.newXmm();
            wo_assure(!ctx->c.movsd(real_of_op1, asmjit::x86::ptr(opnum1.gp_value(), offsetof(value, real))));
            wo_assure(!ctx->c.subsd(real_of_op1, asmjit::x86::ptr(opnum2.gp_value(), offsetof(value, real))));
            wo_assure(!ctx->c.movsd(asmjit::x86::ptr(opnum1.gp_value(), offsetof(value, real)), real_of_op1));
            return true;
        }
        virtual bool ir_mulr(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto real_of_op1 = ctx->c.newXmm();
            wo_assure(!ctx->c.movsd(real_of_op1, asmjit::x86::ptr(opnum1.gp_value(), offsetof(value, real))));
            wo_assure(!ctx->c.mulsd(real_of_op1, asmjit::x86::ptr(opnum2.gp_value(), offsetof(value, real))));
            wo_assure(!ctx->c.movsd(asmjit::x86::ptr(opnum1.gp_value(), offsetof(value, real)), real_of_op1));
            return true;
        }
        virtual bool ir_divr(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto real_of_op1 = ctx->c.newXmm();
            wo_assure(!ctx->c.movsd(real_of_op1, asmjit::x86::ptr(opnum1.gp_value(), offsetof(value, real))));
            wo_assure(!ctx->c.divsd(real_of_op1, asmjit::x86::ptr(opnum2.gp_value(), offsetof(value, real))));
            wo_assure(!ctx->c.movsd(asmjit::x86::ptr(opnum1.gp_value(), offsetof(value, real)), real_of_op1));
            return true;
        }
        virtual bool ir_modr(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto op1 = opnum1.gp_value();
            auto op2 = opnum2.gp_value();

            asmjit::InvokeNode* invoke_node;
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&_vmjitcall_modr,
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
                wo_assure(!ctx->c.add(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, handle)), opnum2.const_value()->handle));
            else
            {
                auto handle_of_op2 = ctx->c.newUInt64();
                wo_assure(!ctx->c.mov(handle_of_op2, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, handle))));
                wo_assure(!ctx->c.add(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, handle)), handle_of_op2));
            }
            return true;
        }
        virtual bool ir_subh(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            if (opnum2.is_constant())
                wo_assure(!ctx->c.sub(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, handle)), opnum2.const_value()->handle));
            else
            {
                auto handle_of_op2 = ctx->c.newUInt64();
                wo_assure(!ctx->c.mov(handle_of_op2, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, handle))));
                wo_assure(!ctx->c.sub(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, handle)), handle_of_op2));
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
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&_vmjitcall_adds,
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
                if (opnum1.is_constant_and_not_tag(ctx))
                    x86_set_imm(ctx->c, ctx->_vmssp, *opnum1.const_value());
                else
                    x86_set_val(ctx->c, ctx->_vmssp, opnum1.gp_value());
                wo_assure(!ctx->c.sub(ctx->_vmssp, sizeof(value)));
            }
            else
            {
                uint16_t psh_repeat = WO_IPVAL_MOVE_2;

                if (psh_repeat > 0)
                    wo_assure(!ctx->c.sub(ctx->_vmssp, psh_repeat * sizeof(value)));
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

                wo_assure(!ctx->c.add(ctx->_vmssp, sizeof(value)));
                x86_set_val(ctx->c, opnum1.gp_value(), ctx->_vmssp);
            }
            else
                wo_assure(!ctx->c.add(ctx->_vmssp, WO_IPVAL_MOVE_2 * sizeof(value)));
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
            auto err = ctx->c.newIntPtr();

            asmjit::InvokeNode* invoke_node;
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&_vmjitcall_sidarr,
                asmjit::FuncSignatureT<const char*, wo::value*, wo::value*, wo::value*>()));

            invoke_node->setArg(0, op1);
            invoke_node->setArg(1, op2);
            invoke_node->setArg(2, op3);
            invoke_node->setRet(0, err);

            auto noerror_label = ctx->c.newLabel();
            wo_assure(!ctx->c.cmp(err, asmjit::Imm(0)));
            wo_assure(!ctx->c.je(noerror_label));

            x86_do_fail(ctx, WO_FAIL_INDEX_FAIL, err, rt_ip);

            wo_assure(!ctx->c.bind(noerror_label));
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
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&_vmjitcall_sidstruct,
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

            auto bpoffset = ctx->c.newUIntPtr();
            if (opnum2.is_constant())
            {
                wo_assure(!ctx->c.lea(bpoffset, asmjit::x86::qword_ptr(ctx->_vmsbp, (int32_t)(opnum2.const_value()->integer * sizeof(value)))));
                x86_set_val(ctx->c, opnum1.gp_value(), bpoffset);
            }
            else
            {
                static_assert(sizeof(wo::value) == 16);

                wo_assure(!ctx->c.mov(bpoffset, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer))));
                wo_assure(!ctx->c.shl(bpoffset, asmjit::Imm(4)));
                wo_assure(!ctx->c.lea(bpoffset, asmjit::x86::qword_ptr(ctx->_vmsbp, bpoffset)));
                x86_set_val(ctx->c, opnum1.gp_value(), bpoffset);
            }
            return true;
        }
        virtual bool ir_sts(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto bpoffset = ctx->c.newUIntPtr();
            if (opnum2.is_constant())
            {
                wo_assure(!ctx->c.lea(bpoffset, asmjit::x86::qword_ptr(ctx->_vmsbp, (int32_t)(opnum2.const_value()->integer * sizeof(value)))));
                x86_set_val(ctx->c, bpoffset, opnum1.gp_value());
            }
            else
            {
                static_assert(sizeof(wo::value) == 16);

                wo_assure(!ctx->c.mov(bpoffset, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer))));
                wo_assure(!ctx->c.shl(bpoffset, asmjit::Imm(4)));
                wo_assure(!ctx->c.lea(bpoffset, asmjit::x86::qword_ptr(ctx->_vmsbp, bpoffset)));
                x86_set_val(ctx->c, bpoffset, opnum1.gp_value());
            }
            return true;
        }
        virtual bool ir_equb(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            // ==

            auto x86_equ_jmp_label = ctx->c.newLabel();
            auto x86_nequ_jmp_label = ctx->c.newLabel();


            auto int_of_op1 = ctx->c.newInt64();
            wo_assure(!ctx->c.mov(int_of_op1, asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer))));
            wo_assure(!ctx->c.cmp(int_of_op1, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer))));
            wo_assure(!ctx->c.jne(x86_nequ_jmp_label));

            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 1));
            wo_assure(!ctx->c.jmp(x86_equ_jmp_label));
            wo_assure(!ctx->c.bind(x86_nequ_jmp_label));
            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 0));
            wo_assure(!ctx->c.bind(x86_equ_jmp_label));

            wo_assure(!ctx->c.mov(asmjit::x86::byte_ptr(ctx->_vmcr, offsetof(value, type)), (uint8_t)value::valuetype::bool_type));

            return true;
        }
        virtual bool ir_nequb(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            // !=

            auto x86_equ_jmp_label = ctx->c.newLabel();
            auto x86_nequ_jmp_label = ctx->c.newLabel();

            auto int_of_op1 = ctx->c.newInt64();
            wo_assure(!ctx->c.mov(int_of_op1, asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer))));
            wo_assure(!ctx->c.cmp(int_of_op1, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer))));
            wo_assure(!ctx->c.jne(x86_nequ_jmp_label));

            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 0));
            wo_assure(!ctx->c.jmp(x86_equ_jmp_label));
            wo_assure(!ctx->c.bind(x86_nequ_jmp_label));
            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 1));
            wo_assure(!ctx->c.bind(x86_equ_jmp_label));

            wo_assure(!ctx->c.mov(asmjit::x86::byte_ptr(ctx->_vmcr, offsetof(value, type)), (uint8_t)value::valuetype::bool_type));

            return true;
        }
        virtual bool ir_lti(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            // <

            auto x86_cmp_fail = ctx->c.newLabel();
            auto x86_cmp_end = ctx->c.newLabel();

            if (opnum1.is_constant())
            {
                wo_assure(!ctx->c.cmp(asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer)), opnum1.m_constant->integer));
                wo_assure(!ctx->c.jl(x86_cmp_fail));
            }
            else if (opnum2.is_constant())
            {
                wo_assure(!ctx->c.cmp(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer)), opnum2.m_constant->integer));
                wo_assure(!ctx->c.jge(x86_cmp_fail));
            }
            else
            {
                auto int_of_op1 = ctx->c.newInt64();
                wo_assure(!ctx->c.mov(int_of_op1, asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer))));
                wo_assure(!ctx->c.cmp(int_of_op1, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer))));
                wo_assure(!ctx->c.jge(x86_cmp_fail));
            }

            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 1));
            wo_assure(!ctx->c.jmp(x86_cmp_end));
            wo_assure(!ctx->c.bind(x86_cmp_fail));
            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 0));
            wo_assure(!ctx->c.bind(x86_cmp_end));
            ctx->c.mov(asmjit::x86::byte_ptr(ctx->_vmcr, offsetof(value, type)), (uint8_t)value::valuetype::bool_type);
            return true;
        }
        virtual bool ir_gti(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            // >

            auto x86_cmp_fail = ctx->c.newLabel();
            auto x86_cmp_end = ctx->c.newLabel();

            if (opnum1.is_constant())
            {
                wo_assure(!ctx->c.cmp(asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer)), opnum1.m_constant->integer));
                ctx->c.jg(x86_cmp_fail);
            }
            else if (opnum2.is_constant())
            {
                wo_assure(!ctx->c.cmp(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer)), opnum2.m_constant->integer));
                ctx->c.jle(x86_cmp_fail);
            }
            else
            {
                auto int_of_op1 = ctx->c.newInt64();
                wo_assure(!ctx->c.mov(int_of_op1, asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer))));
                wo_assure(!ctx->c.cmp(int_of_op1, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer))));
                ctx->c.jle(x86_cmp_fail);
            }

            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 1));
            ctx->c.jmp(x86_cmp_end);
            ctx->c.bind(x86_cmp_fail);
            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 0));
            ctx->c.bind(x86_cmp_end);
            ctx->c.mov(asmjit::x86::byte_ptr(ctx->_vmcr, offsetof(value, type)), (uint8_t)value::valuetype::bool_type);

            return true;
        }
        virtual bool ir_elti(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            // <=

            auto x86_cmp_fail = ctx->c.newLabel();
            auto x86_cmp_end = ctx->c.newLabel();

            if (opnum1.is_constant())
            {
                wo_assure(!ctx->c.cmp(asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer)), opnum1.m_constant->integer));
                ctx->c.jle(x86_cmp_fail);
            }
            else if (opnum2.is_constant())
            {
                wo_assure(!ctx->c.cmp(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer)), opnum2.m_constant->integer));
                ctx->c.jg(x86_cmp_fail);
            }
            else
            {
                auto int_of_op1 = ctx->c.newInt64();
                wo_assure(!ctx->c.mov(int_of_op1, asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer))));
                wo_assure(!ctx->c.cmp(int_of_op1, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer))));
                ctx->c.jg(x86_cmp_fail);
            }

            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 1));
            ctx->c.jmp(x86_cmp_end);
            ctx->c.bind(x86_cmp_fail);
            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 0));
            ctx->c.bind(x86_cmp_end);
            ctx->c.mov(asmjit::x86::byte_ptr(ctx->_vmcr, offsetof(value, type)), (uint8_t)value::valuetype::bool_type);
            return true;
        }
        virtual bool ir_egti(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            // >=

            auto x86_cmp_fail = ctx->c.newLabel();
            auto x86_cmp_end = ctx->c.newLabel();

            if (opnum1.is_constant())
            {
                wo_assure(!ctx->c.cmp(asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer)), opnum1.m_constant->integer));
                ctx->c.jge(x86_cmp_fail);
            }
            else if (opnum2.is_constant())
            {
                wo_assure(!ctx->c.cmp(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer)), opnum2.m_constant->integer));
                ctx->c.jl(x86_cmp_fail);
            }
            else
            {
                auto int_of_op1 = ctx->c.newInt64();
                wo_assure(!ctx->c.mov(int_of_op1, asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer))));
                wo_assure(!ctx->c.cmp(int_of_op1, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer))));
                ctx->c.jl(x86_cmp_fail);
            }

            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 1));
            ctx->c.jmp(x86_cmp_end);
            ctx->c.bind(x86_cmp_fail);
            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 0));
            ctx->c.bind(x86_cmp_end);
            ctx->c.mov(asmjit::x86::byte_ptr(ctx->_vmcr, offsetof(value, type)), (uint8_t)value::valuetype::bool_type);

            return true;
        }
        virtual bool ir_land(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            if (opnum1.is_constant() && opnum2.is_constant())
            {
                if (opnum1.const_value()->integer && opnum2.const_value()->integer)
                    wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 1));
                else
                    wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 0));
            }
            else if (opnum1.is_constant())
            {
                if (opnum1.const_value()->integer)
                {
                    auto opnum2_val = ctx->c.newInt64();
                    wo_assure(!ctx->c.mov(opnum2_val, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer))));
                    wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), opnum2_val));
                }
                else
                    wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 0));
            }
            else if (opnum2.is_constant())
            {
                if (opnum2.const_value()->integer)
                {
                    auto opnum1_val = ctx->c.newInt64();
                    wo_assure(!ctx->c.mov(opnum1_val, asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer))));
                    wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), opnum1_val));
                }
                else
                    wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 0));
            }
            else
            {
                auto set_cr_false = ctx->c.newLabel();
                auto land_end = ctx->c.newLabel();

                wo_assure(!ctx->c.cmp(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer)), 0));
                wo_assure(!ctx->c.je(set_cr_false));
                wo_assure(!ctx->c.cmp(asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer)), 0));
                wo_assure(!ctx->c.je(set_cr_false));
                wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 1));
                wo_assure(!ctx->c.jmp(land_end));
                wo_assure(!ctx->c.bind(set_cr_false));
                wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 0));
                wo_assure(!ctx->c.bind(land_end));
            }
            ctx->c.mov(asmjit::x86::byte_ptr(ctx->_vmcr, offsetof(value, type)), (uint8_t)value::valuetype::bool_type);
            return true;
        }
        virtual bool ir_lor(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            if (opnum1.is_constant() && opnum2.is_constant())
            {
                if (opnum1.const_value()->integer || opnum2.const_value()->integer)
                    wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 1));
                else
                    wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 0));
            }
            else if (opnum1.is_constant())
            {
                if (!opnum1.const_value()->integer)
                {
                    auto opnum2_val = ctx->c.newInt64();
                    wo_assure(!ctx->c.mov(opnum2_val, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer))));
                    wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), opnum2_val));
                }
                else
                    wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 1));
            }
            else if (opnum2.is_constant())
            {
                if (!opnum2.const_value()->integer)
                {
                    auto opnum1_val = ctx->c.newInt64();
                    wo_assure(!ctx->c.mov(opnum1_val, asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer))));
                    wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), opnum1_val));
                }
                else
                    wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 1));
            }
            else
            {
                auto set_cr_true = ctx->c.newLabel();
                auto land_end = ctx->c.newLabel();

                wo_assure(!ctx->c.cmp(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer)), 0));
                wo_assure(!ctx->c.jne(set_cr_true));
                wo_assure(!ctx->c.cmp(asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer)), 0));
                wo_assure(!ctx->c.jne(set_cr_true));
                wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 0));
                wo_assure(!ctx->c.jmp(land_end));
                wo_assure(!ctx->c.bind(set_cr_true));
                wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 1));
                wo_assure(!ctx->c.bind(land_end));
            }
            ctx->c.mov(asmjit::x86::byte_ptr(ctx->_vmcr, offsetof(value, type)), (uint8_t)value::valuetype::bool_type);
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
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&_vmjitcall_sidmap,
                asmjit::FuncSignatureT< void, wo::value*, wo::value*, wo::value*>()));

            invoke_node->setArg(0, op1);
            invoke_node->setArg(1, op2);
            invoke_node->setArg(2, op3);

            return true;
        }

        virtual bool ir_ltx(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto op1 = opnum1.gp_value();
            auto op2 = opnum2.gp_value();

            asmjit::InvokeNode* invoke_node;
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&vm::ltx_impl,
                asmjit::FuncSignatureT< void, wo::value*, wo::value*, wo::value*>()));

            invoke_node->setArg(0, ctx->_vmcr);
            invoke_node->setArg(1, op1);
            invoke_node->setArg(2, op2);

            return true;
        }
        virtual bool ir_gtx(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto op1 = opnum1.gp_value();
            auto op2 = opnum2.gp_value();

            asmjit::InvokeNode* invoke_node;
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&vm::gtx_impl,
                asmjit::FuncSignatureT< void, wo::value*, wo::value*, wo::value*>()));

            invoke_node->setArg(0, ctx->_vmcr);
            invoke_node->setArg(1, op1);
            invoke_node->setArg(2, op2);

            return true;
        }
        virtual bool ir_eltx(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto op1 = opnum1.gp_value();
            auto op2 = opnum2.gp_value();

            asmjit::InvokeNode* invoke_node;
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&vm::eltx_impl,
                asmjit::FuncSignatureT< void, wo::value*, wo::value*, wo::value*>()));

            invoke_node->setArg(0, ctx->_vmcr);
            invoke_node->setArg(1, op1);
            invoke_node->setArg(2, op2);

            return true;
        }
        virtual bool ir_egtx(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto op1 = opnum1.gp_value();
            auto op2 = opnum2.gp_value();

            asmjit::InvokeNode* invoke_node;
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&vm::egtx_impl,
                asmjit::FuncSignatureT< void, wo::value*, wo::value*, wo::value*>()));

            invoke_node->setArg(0, ctx->_vmcr);
            invoke_node->setArg(1, op1);
            invoke_node->setArg(2, op2);

            return true;
        }
        virtual bool ir_ltr(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            // <

            auto x86_cmp_fail = ctx->c.newLabel();
            auto x86_cmp_end = ctx->c.newLabel();

            auto real_of_op1 = ctx->c.newXmm();
            wo_assure(!ctx->c.movsd(real_of_op1, asmjit::x86::ptr(opnum1.gp_value(), offsetof(value, real))));
            wo_assure(!ctx->c.comisd(real_of_op1, asmjit::x86::ptr(opnum2.gp_value(), offsetof(value, real))));

            wo_assure(!ctx->c.jae(x86_cmp_fail));

            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 1));
            wo_assure(!ctx->c.jmp(x86_cmp_end));
            wo_assure(!ctx->c.bind(x86_cmp_fail));
            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 0));
            wo_assure(!ctx->c.bind(x86_cmp_end));
            ctx->c.mov(asmjit::x86::byte_ptr(ctx->_vmcr, offsetof(value, type)), (uint8_t)value::valuetype::bool_type);

            return true;
        }
        virtual bool ir_gtr(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            // >

            auto x86_cmp_fail = ctx->c.newLabel();
            auto x86_cmp_end = ctx->c.newLabel();

            auto real_of_op1 = ctx->c.newXmm();
            wo_assure(!ctx->c.movsd(real_of_op1, asmjit::x86::ptr(opnum1.gp_value(), offsetof(value, real))));
            wo_assure(!ctx->c.comisd(real_of_op1, asmjit::x86::ptr(opnum2.gp_value(), offsetof(value, real))));

            wo_assure(!ctx->c.jbe(x86_cmp_fail));

            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 1));
            wo_assure(!ctx->c.jmp(x86_cmp_end));
            wo_assure(!ctx->c.bind(x86_cmp_fail));

            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 0));
            wo_assure(!ctx->c.bind(x86_cmp_end));
            ctx->c.mov(asmjit::x86::byte_ptr(ctx->_vmcr, offsetof(value, type)), (uint8_t)value::valuetype::bool_type);

            return true;
        }
        virtual bool ir_eltr(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            // <=

            auto x86_cmp_fail = ctx->c.newLabel();
            auto x86_cmp_end = ctx->c.newLabel();

            auto real_of_op1 = ctx->c.newXmm();
            wo_assure(!ctx->c.movsd(real_of_op1, asmjit::x86::ptr(opnum1.gp_value(), offsetof(value, real))));
            wo_assure(!ctx->c.comisd(real_of_op1, asmjit::x86::ptr(opnum2.gp_value(), offsetof(value, real))));

            wo_assure(!ctx->c.ja(x86_cmp_fail));

            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 1));
            wo_assure(!ctx->c.jmp(x86_cmp_end));
            wo_assure(!ctx->c.bind(x86_cmp_fail));
            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 0));
            wo_assure(!ctx->c.bind(x86_cmp_end));
            ctx->c.mov(asmjit::x86::byte_ptr(ctx->_vmcr, offsetof(value, type)), (uint8_t)value::valuetype::bool_type);

            return true;
        }
        virtual bool ir_egtr(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            // <=

            auto x86_cmp_fail = ctx->c.newLabel();
            auto x86_cmp_end = ctx->c.newLabel();

            auto real_of_op1 = ctx->c.newXmm();
            wo_assure(!ctx->c.movsd(real_of_op1, asmjit::x86::ptr(opnum1.gp_value(), offsetof(value, real))));
            wo_assure(!ctx->c.comisd(real_of_op1, asmjit::x86::ptr(opnum2.gp_value(), offsetof(value, real))));

            wo_assure(!ctx->c.jb(x86_cmp_fail));

            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 1));
            wo_assure(!ctx->c.jmp(x86_cmp_end));
            wo_assure(!ctx->c.bind(x86_cmp_fail));
            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 0));
            wo_assure(!ctx->c.bind(x86_cmp_end));
            ctx->c.mov(asmjit::x86::byte_ptr(ctx->_vmcr, offsetof(value, type)), (uint8_t)value::valuetype::bool_type);

            return true;
        }

        virtual bool ir_call(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;

            auto op1 = opnum1.gp_value();

            auto result = ctx->c.newInt32();

            asmjit::InvokeNode* invoke_node;
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&native_do_call_vmfunc,
                asmjit::FuncSignatureT<wo_result_t, vmbase*, value*, uint32_t, value*, value*>()));

            invoke_node->setArg(0, ctx->_vmbase);
            invoke_node->setArg(1, op1);
            invoke_node->setArg(2, asmjit::Imm((uint32_t)(rt_ip - ctx->env->rt_codes)));
            invoke_node->setArg(3, ctx->_vmssp);
            invoke_node->setArg(4, ctx->_vmsbp);
            invoke_node->setRet(0, result);

            auto normal = ctx->c.newLabel();
            wo_assure(!ctx->c.cmp(result, asmjit::Imm(wo_result_t::WO_API_NORMAL)));
            wo_assure(!ctx->c.je(normal));
            wo_assure(!ctx->c.ret(result)); // break this execute!!!
            wo_assure(!ctx->c.bind(normal));

            return true;
        }
        virtual bool ir_calln(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            if (dr)
            {
                // Call native
                jit_packed_func_t call_aim_native_func = (jit_packed_func_t)(WO_IPVAL_MOVE_8);

                if (dr & 0b10)
                    x86_do_calln_native_func_fast(ctx->c, ctx->_vmbase, call_aim_native_func, ctx->env->rt_codes, rt_ip, ctx->_vmssp, ctx->_vmsbp);
                else
                    x86_do_calln_native_func(ctx->c, ctx->_vmbase, call_aim_native_func, ctx->env->rt_codes, rt_ip, ctx->_vmssp, ctx->_vmsbp);

                // ISSUE: 240219 1.13.1.1
                // We need to check if extern-function returned with panic/halt.
                // Check and make it work!
                ir_make_checkpoint(ctx, rt_ip);
            }
            else
            {
                uint32_t call_aim_vm_func = WO_IPVAL_MOVE_4;
                rt_ip += 4; // skip empty space;

                // Try compile this func
                auto* compiled_funcstat = analyze_function(ctx->env->rt_codes, call_aim_vm_func, ctx->env);
                if (compiled_funcstat->m_state == function_jit_state::state::FAILED)
                {
                    WO_JIT_NOT_SUPPORT;
                }

                register_dependence_function(compiled_funcstat);

                x86_do_calln_vm_func(
                    ctx->c, ctx->_vmbase,
                    compiled_funcstat,
                    ctx->env->rt_codes,
                    rt_ip, ctx->_vmssp, ctx->_vmsbp);
            }

            return true;
        }
        virtual bool ir_ret(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            if (dr != 0)
                (void)WO_IPVAL_MOVE_2;

            auto stat = ctx->c.newInt32();
            static_assert(sizeof(wo_result_t) == sizeof(int32_t));
            ctx->c.mov(stat, asmjit::Imm(wo_result_t::WO_API_NORMAL));
            wo_assure(!ctx->c.ret(stat));
            return true;
        }
        virtual bool ir_jt(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            auto check_point_ipaddr = rt_ip - 1;
            uint32_t jmp_place = WO_IPVAL_MOVE_4;

            if (jmp_place < rt_ip - ctx->env->rt_codes)
                ir_make_checkpoint(ctx, check_point_ipaddr);

            wo_assure(!ctx->c.cmp(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, handle)), 0));
            wo_assure(!ctx->c.jne(jump_ip(&ctx->c, jmp_place)));
            return true;
        }
        virtual bool ir_jf(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            auto check_point_ipaddr = rt_ip - 1;
            uint32_t jmp_place = WO_IPVAL_MOVE_4;

            if (jmp_place < rt_ip - ctx->env->rt_codes)
                ir_make_checkpoint(ctx, check_point_ipaddr);

            wo_assure(!ctx->c.cmp(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, handle)), 0));
            wo_assure(!ctx->c.je(jump_ip(&ctx->c, jmp_place)));
            return true;
        }
        virtual bool ir_jmp(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            auto check_point_ipaddr = rt_ip - 1;
            uint32_t jmp_place = WO_IPVAL_MOVE_4;

            if (jmp_place < rt_ip - ctx->env->rt_codes)
                ir_make_checkpoint(ctx, check_point_ipaddr);

            wo_assure(!ctx->c.jmp(jump_ip(&ctx->c, jmp_place)));
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
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&vm::make_union_impl,
                asmjit::FuncSignatureT<wo::value*, wo::value*, wo::value*, uint16_t>()));

            invoke_node->setArg(0, op1);
            invoke_node->setArg(1, op2);
            invoke_node->setArg(2, asmjit::Imm(id));
            invoke_node->setRet(0, ctx->_vmssp);
            return true;
        }
        virtual bool ir_movcast(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;
            value::valuetype aim_type = static_cast<value::valuetype>(WO_IPVAL_MOVE_1);

            auto op1 = opnum1.gp_value();
            auto op2 = opnum2.gp_value();
            auto err = ctx->c.newIntPtr();

            asmjit::InvokeNode* invoke_node;
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&vm::movcast_impl,
                asmjit::FuncSignatureT<const char*, wo::value*, wo::value*, uint8_t>()));

            static_assert(sizeof(uint8_t) == sizeof(value::valuetype));

            invoke_node->setArg(0, op1);
            invoke_node->setArg(1, op2);
            invoke_node->setArg(2, asmjit::Imm((uint8_t)aim_type));
            invoke_node->setRet(0, err);

            auto noerror_label = ctx->c.newLabel();
            wo_assure(!ctx->c.cmp(err, asmjit::Imm(0)));
            wo_assure(!ctx->c.je(noerror_label));

            x86_do_fail(ctx, WO_FAIL_TYPE_FAIL, err, rt_ip);

            wo_assure(!ctx->c.bind(noerror_label));
            return true;
        }
        virtual bool ir_mkclos(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            asmjit::InvokeNode* invoke_node;

            // NOTE: in x64, use make_closure_fast_impl.
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&vm::make_closure_fast_impl,
                asmjit::FuncSignatureT<value*, value*, const byte_t*, value*>()));

            invoke_node->setArg(0, ctx->_vmcr);
            invoke_node->setArg(1, asmjit::Imm((intptr_t)rt_ip));
            invoke_node->setArg(2, ctx->_vmssp);

            invoke_node->setRet(0, ctx->_vmssp);
            rt_ip += (2 + 8);
            return true;
        }
        virtual bool ir_typeas(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            value::valuetype type = (value::valuetype)(WO_IPVAL_MOVE_1);

            auto typeas_failed = ctx->c.newLabel();
            auto typeas_end = ctx->c.newLabel();
            wo_assure(!ctx->c.cmp(
                asmjit::x86::byte_ptr(opnum1.gp_value(), offsetof(value, type)),
                (uint8_t)type));
            wo_assure(!ctx->c.jne(typeas_failed));
            if (dr & 0b01)
            {
                wo_assure(!ctx->c.mov(
                    asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)),
                    1));
            }
            wo_assure(!ctx->c.jmp(typeas_end));
            wo_assure(!ctx->c.bind(typeas_failed));
            if (dr & 0b01)
            {
                wo_assure(!ctx->c.mov(
                    asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)),
                    0));
            }
            else
            {
                x86_do_fail(ctx, WO_FAIL_TYPE_FAIL,
                    asmjit::Imm((intptr_t)"The given value is not the same as the requested type."), rt_ip);
            }
            wo_assure(!ctx->c.bind(typeas_end));
            if (dr & 0b01)
            {
                wo_assure(!ctx->c.mov(
                    asmjit::x86::byte_ptr(ctx->_vmcr, offsetof(value, type)),
                    (uint8_t)value::valuetype::bool_type));
            }
            return true;
        }
        virtual bool ir_mkstruct(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            uint16_t size = WO_IPVAL_MOVE_2;

            auto op1 = opnum1.gp_value();

            asmjit::InvokeNode* invoke_node;
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&vm::make_struct_impl,
                asmjit::FuncSignatureT< wo::value*, wo::value*, uint16_t, wo::value*>()));

            invoke_node->setArg(0, op1);
            invoke_node->setArg(1, asmjit::Imm(size));
            invoke_node->setArg(2, ctx->_vmssp);
            invoke_node->setRet(0, ctx->_vmssp);
            return true;
        }
        virtual bool ir_abrt(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            if (dr & 0b10)
                WO_JIT_NOT_SUPPORT;
            else
            {
                asmjit::InvokeNode* invoke_node;
                wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&_vmjitcall_abrt,
                    asmjit::FuncSignatureT< void, const char*>()));
                invoke_node->setArg(0, (intptr_t)"executed 'abrt'.");
            }
            return true;
        }
        virtual bool ir_idarr(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto op1 = opnum1.gp_value();
            auto op2 = opnum2.gp_value();
            auto err = ctx->c.newIntPtr();

            asmjit::InvokeNode* invoke_node;
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&_vmjitcall_idarr,
                asmjit::FuncSignatureT<const char*, wo::value*, wo::value*, wo::value*>()));

            invoke_node->setArg(0, ctx->_vmcr);
            invoke_node->setArg(1, op1);
            invoke_node->setArg(2, op2);
            invoke_node->setRet(0, err);

            auto noerror_label = ctx->c.newLabel();
            wo_assure(!ctx->c.cmp(err, asmjit::Imm(0)));
            wo_assure(!ctx->c.je(noerror_label));

            x86_do_fail(ctx, WO_FAIL_INDEX_FAIL, err, rt_ip);

            wo_assure(!ctx->c.bind(noerror_label));
            return true;
        }
        virtual bool ir_iddict(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto op1 = opnum1.gp_value();
            auto op2 = opnum2.gp_value();
            auto err = ctx->c.newIntPtr();

            asmjit::InvokeNode* invoke_node;
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&_vmjitcall_iddict,
                asmjit::FuncSignatureT<const char*, wo::value*, wo::value*, wo::value*>()));

            invoke_node->setArg(0, ctx->_vmcr);
            invoke_node->setArg(1, op1);
            invoke_node->setArg(2, op2);
            invoke_node->setRet(0, err);

            auto noerror_label = ctx->c.newLabel();
            wo_assure(!ctx->c.cmp(err, asmjit::Imm(0)));
            wo_assure(!ctx->c.je(noerror_label));

            x86_do_fail(ctx, WO_FAIL_INDEX_FAIL, err, rt_ip);

            wo_assure(!ctx->c.bind(noerror_label));
            return true;
        }
        virtual bool ir_mkarr(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            uint16_t size = WO_IPVAL_MOVE_2;

            auto op1 = opnum1.gp_value();

            asmjit::InvokeNode* invoke_node;
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&vm::make_array_impl,
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
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&vm::make_map_impl,
                asmjit::FuncSignatureT< wo::value*, wo::value*, uint16_t, wo::value*>()));

            invoke_node->setArg(0, op1);
            invoke_node->setArg(1, asmjit::Imm(size));
            invoke_node->setArg(2, ctx->_vmssp);
            invoke_node->setRet(0, ctx->_vmssp);
            return true;
        }
        virtual bool ir_idstr(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto op1 = opnum1.gp_value();
            auto op2 = opnum2.gp_value();

            asmjit::InvokeNode* invoke_node;
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&_vmjitcall_idstr,
                asmjit::FuncSignatureT< void, wo::value*, wo::value*, wo::value*>()));

            invoke_node->setArg(0, ctx->_vmcr);
            invoke_node->setArg(1, op1);
            invoke_node->setArg(2, op2);
            return true;
        }
        virtual bool ir_equr(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            // ==

            auto x86_cmp_fail = ctx->c.newLabel();
            auto x86_cmp_end = ctx->c.newLabel();

            auto real_of_op1 = ctx->c.newXmm();
            wo_assure(!ctx->c.movsd(real_of_op1, asmjit::x86::ptr(opnum1.gp_value(), offsetof(value, real))));
            wo_assure(!ctx->c.comisd(real_of_op1, asmjit::x86::ptr(opnum2.gp_value(), offsetof(value, real))));

            wo_assure(!ctx->c.jp(x86_cmp_fail));
            wo_assure(!ctx->c.jne(x86_cmp_fail));

            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 1));
            wo_assure(!ctx->c.jmp(x86_cmp_end));
            wo_assure(!ctx->c.bind(x86_cmp_fail));
            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 0));
            wo_assure(!ctx->c.bind(x86_cmp_end));

            ctx->c.mov(asmjit::x86::byte_ptr(ctx->_vmcr, offsetof(value, type)), (uint8_t)value::valuetype::bool_type);

            return true;
        }
        virtual bool ir_nequr(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            // !=

            auto x86_cmp_fail = ctx->c.newLabel();
            auto x86_cmp_end = ctx->c.newLabel();

            auto real_of_op1 = ctx->c.newXmm();
            wo_assure(!ctx->c.movsd(real_of_op1, asmjit::x86::ptr(opnum1.gp_value(), offsetof(value, real))));
            wo_assure(!ctx->c.comisd(real_of_op1, asmjit::x86::ptr(opnum2.gp_value(), offsetof(value, real))));

            wo_assure(!ctx->c.jp(x86_cmp_fail));
            wo_assure(!ctx->c.je(x86_cmp_fail));

            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 1));
            wo_assure(!ctx->c.jmp(x86_cmp_end));
            wo_assure(!ctx->c.bind(x86_cmp_fail));
            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), 0));
            wo_assure(!ctx->c.bind(x86_cmp_end));

            ctx->c.mov(asmjit::x86::byte_ptr(ctx->_vmcr, offsetof(value, type)), (uint8_t)value::valuetype::bool_type);

            return true;
        }
        virtual bool ir_equs(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto op1 = opnum1.gp_value();
            auto op2 = opnum2.gp_value();

            asmjit::InvokeNode* invoke_node;
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&_vmjitcall_equs,
                asmjit::FuncSignatureT<void, wo::value*, wo::value*, wo::value*>()));

            invoke_node->setArg(0, ctx->_vmcr);
            invoke_node->setArg(1, op1);
            invoke_node->setArg(2, op2);
            return true;
        }
        virtual bool ir_nequs(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto op1 = opnum1.gp_value();
            auto op2 = opnum2.gp_value();

            asmjit::InvokeNode* invoke_node;
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&_vmjitcall_nequs,
                asmjit::FuncSignatureT<void, wo::value*, wo::value*, wo::value*>()));

            invoke_node->setArg(0, ctx->_vmcr);
            invoke_node->setArg(1, op1);
            invoke_node->setArg(2, op2);
            return true;
        }
        virtual bool ir_siddict(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;
            WO_JIT_ADDRESSING_N3_REG_BPOFF;

            auto op1 = opnum1.gp_value();
            auto op2 = opnum2.gp_value();
            auto op3 = opnum3.gp_value();
            auto err = ctx->c.newIntPtr();

            asmjit::InvokeNode* invoke_node;
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&_vmjitcall_siddict,
                asmjit::FuncSignatureT<const char*, wo::value*, wo::value*, wo::value*>()));

            invoke_node->setArg(0, op1);
            invoke_node->setArg(1, op2);
            invoke_node->setArg(2, op3);
            invoke_node->setRet(0, err);

            auto noerror_label = ctx->c.newLabel();
            wo_assure(!ctx->c.cmp(err, asmjit::Imm(0)));
            wo_assure(!ctx->c.je(noerror_label));

            x86_do_fail(ctx, WO_FAIL_INDEX_FAIL, err, rt_ip);

            wo_assure(!ctx->c.bind(noerror_label));
            return true;
        }
        virtual bool ir_jnequb(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            auto check_point_ipaddr = rt_ip - 1;

            WO_JIT_ADDRESSING_N1;
            uint32_t jmp_place = WO_IPVAL_MOVE_4;

            if (jmp_place < rt_ip - ctx->env->rt_codes)
                ir_make_checkpoint(ctx, check_point_ipaddr);

            if (opnum1.is_constant())
                wo_assure(!ctx->c.cmp(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), opnum1.const_value()->integer));
            else
            {
                auto bvalue = ctx->c.newInt64();
                wo_assure(!ctx->c.mov(bvalue, asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer))));
                wo_assure(!ctx->c.cmp(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, integer)), bvalue));
            }

            wo_assure(!ctx->c.jne(jump_ip(&ctx->c, jmp_place)));
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
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&_vmjitcall_idstruct,
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
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&_vmjitcall_panic,
                asmjit::FuncSignatureT<void, wo::vmbase*, wo::value*, const byte_t*, value*, value*>()));

            invoke_node->setArg(0, ctx->_vmbase);
            invoke_node->setArg(1, op1);
            invoke_node->setArg(2, asmjit::Imm((intptr_t)rt_ip));
            invoke_node->setArg(3, ctx->_vmssp);
            invoke_node->setArg(4, ctx->_vmsbp);
            ir_make_checkpoint(ctx, rt_ip);
            return true;
        }
        virtual bool ir_ext_packargs(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip) override
        {
            WO_JIT_ADDRESSING_N1;
            uint16_t this_function_arg_count = WO_IPVAL_MOVE_2;
            uint16_t skip_closure_arg_count = WO_IPVAL_MOVE_2;

            auto op1 = opnum1.gp_value();

            asmjit::InvokeNode* invoke_node;
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&vm::packargs_impl,
                asmjit::FuncSignatureT<void, wo::value*, uint16_t, wo::value*, wo::value*, uint16_t>()));

            invoke_node->setArg(0, op1);
            invoke_node->setArg(1, asmjit::Imm(this_function_arg_count));
            invoke_node->setArg(2, ctx->_vmtc);
            invoke_node->setArg(3, ctx->_vmsbp);
            invoke_node->setArg(4, asmjit::Imm(skip_closure_arg_count));

            return true;
        }
        virtual bool ir_ext_cdivilr(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip) override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto op1 = opnum1.gp_value();
            auto op2 = opnum2.gp_value();

            auto divisor_val = ctx->c.newInt64();
            auto div_zero_ok = ctx->c.newLabel();
            wo_assure(!ctx->c.mov(divisor_val, asmjit::x86::qword_ptr(op2, offsetof(value, integer))));
            wo_assure(!ctx->c.cmp(divisor_val, asmjit::Imm(0)));
            wo_assure(!ctx->c.jne(div_zero_ok));
            x86_do_fail(ctx, WO_FAIL_UNEXPECTED,
                asmjit::Imm((intptr_t)"The divisor cannot be 0."), rt_ip);
            wo_assure(!ctx->c.bind(div_zero_ok));
            auto div_overflow_ok = ctx->c.newLabel();
            wo_assure(!ctx->c.cmp(divisor_val, asmjit::Imm(-1)));
            wo_assure(!ctx->c.jne(div_overflow_ok));
            wo_assure(!ctx->c.cmp(asmjit::x86::qword_ptr(op1, offsetof(value, integer)), asmjit::Imm(INT64_MIN)));
            wo_assure(!ctx->c.jne(div_overflow_ok));
            x86_do_fail(ctx, WO_FAIL_UNEXPECTED,
                asmjit::Imm((intptr_t)"Division overflow."), rt_ip);
            wo_assure(!ctx->c.bind(div_overflow_ok));

            return true;
        }
        virtual bool ir_ext_cdivil(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip) override
        {
            WO_JIT_ADDRESSING_N1;

            auto op1 = opnum1.gp_value();

            auto div_overflow_ok = ctx->c.newLabel();
            wo_assure(!ctx->c.cmp(asmjit::x86::qword_ptr(op1, offsetof(value, integer)), asmjit::Imm(INT64_MIN)));
            wo_assure(!ctx->c.jne(div_overflow_ok));
            x86_do_fail(ctx, WO_FAIL_UNEXPECTED,
                asmjit::Imm((intptr_t)"Division overflow."), rt_ip);
            wo_assure(!ctx->c.bind(div_overflow_ok));

            return true;
        }
        virtual bool ir_ext_cdivirz(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip) override
        {
            WO_JIT_ADDRESSING_N1;

            auto op1 = opnum1.gp_value();

            auto div_zero_ok = ctx->c.newLabel();
            wo_assure(!ctx->c.cmp(asmjit::x86::qword_ptr(op1, offsetof(value, integer)), asmjit::Imm(0)));
            wo_assure(!ctx->c.jne(div_zero_ok));
            x86_do_fail(ctx, WO_FAIL_UNEXPECTED,
                asmjit::Imm((intptr_t)"The divisor cannot be 0."), rt_ip);
            wo_assure(!ctx->c.bind(div_zero_ok));

            return true;
        }
        virtual bool ir_ext_cdivir(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip) override
        {
            WO_JIT_ADDRESSING_N1;

            auto op1 = opnum1.gp_value();

            auto divisor_val = ctx->c.newInt64();
            auto div_zero_ok = ctx->c.newLabel();
            wo_assure(!ctx->c.mov(divisor_val, asmjit::x86::qword_ptr(op1, offsetof(value, integer))));
            wo_assure(!ctx->c.cmp(divisor_val, asmjit::Imm(0)));
            wo_assure(!ctx->c.jne(div_zero_ok));
            x86_do_fail(ctx, WO_FAIL_UNEXPECTED,
                asmjit::Imm((intptr_t)"The divisor cannot be 0."), rt_ip);
            wo_assure(!ctx->c.bind(div_zero_ok));
            auto div_overflow_ok = ctx->c.newLabel();
            wo_assure(!ctx->c.cmp(divisor_val, asmjit::Imm(-1)));
            wo_assure(!ctx->c.jne(div_overflow_ok));
            x86_do_fail(ctx, WO_FAIL_UNEXPECTED,
                asmjit::Imm((intptr_t)"Division overflow."), rt_ip);
            wo_assure(!ctx->c.bind(div_overflow_ok));

            return true;
        }
        virtual bool ir_unpackargs(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip) override
        {
            WO_JIT_ADDRESSING_N1;
            auto unpack_argc_unsigned = WO_IPVAL_MOVE_4;

            auto op1 = opnum1.gp_value();

            asmjit::InvokeNode* invoke_node;
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&vm::unpackargs_impl,
                asmjit::FuncSignatureT<wo::value*, vmbase*, int32_t, value*, value*, const byte_t*, value*, value*>()));

            invoke_node->setArg(0, ctx->_vmbase);
            invoke_node->setArg(1, op1);
            invoke_node->setArg(2, asmjit::Imm(reinterpret_cast<int32_t&>(unpack_argc_unsigned)));
            invoke_node->setArg(3, ctx->_vmtc);
            invoke_node->setArg(4, asmjit::Imm((intptr_t)rt_ip));
            invoke_node->setArg(5, ctx->_vmssp);
            invoke_node->setArg(6, ctx->_vmsbp);
            invoke_node->setRet(0, ctx->_vmssp);

            ir_make_checkpoint(ctx, rt_ip);
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
            // TODO: Support jit for aarch64
        }
        else
        {
            wo_error("Unknown platform.");
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
            // TODO: Support jit for aarch64
        }
        else
        {
            wo_error("Unknown platform.");
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
        }
#endif
