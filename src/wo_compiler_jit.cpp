#include "wo_afx.hpp"

#include "wo_compiler_jit.hpp"

#undef FAILED

/*
ATTENTION: Under no circumstances should you read ip/sp/bp from
        the VM instance; these are write-only for JIT.

Since version 1.14.14, all immediate JIT calls are executed via
callnjit. These calls do not require updating sp/bp/ip, so reading
these values from the VM often yields incorrect results.
*/

#if WO_JIT_SUPPORT_ASMJIT

#ifndef ASMJIT_STATIC
#define ASMJIT_STATIC
#endif
#include "asmjit/asmjit.h"

namespace wo
{
#define WO_SAFE_READ_OFFSET_GET_QWORD (*(uint64_t*)(rt_ip-8))
#define WO_SAFE_READ_OFFSET_GET_DWORD (*(uint32_t*)(rt_ip-4))
#define WO_SAFE_READ_OFFSET_GET_WORD (*(uint16_t*)(rt_ip-2))

    // FOR BigEndian
#define WO_SAFE_READ_OFFSET_PER_BYTE(OFFSET, TYPE) (((TYPE)(*(rt_ip-OFFSET)))<<((sizeof(TYPE)-OFFSET)*8))
#define WO_IS_ODD_IRPTR(ALLIGN) 1 // NOTE: Always odd for safe reading.

#define WO_SAFE_READ_MOVE_2 \
    ((rt_ip+=2),WO_IS_ODD_IRPTR(2)?\
        (WO_SAFE_READ_OFFSET_PER_BYTE(2,uint16_t)|WO_SAFE_READ_OFFSET_PER_BYTE(1,uint16_t)):\
        WO_SAFE_READ_OFFSET_GET_WORD)
#define WO_SAFE_READ_MOVE_4 \
    ((rt_ip+=4),WO_IS_ODD_IRPTR(4)?\
        (WO_SAFE_READ_OFFSET_PER_BYTE(4,uint32_t)|WO_SAFE_READ_OFFSET_PER_BYTE(3,uint32_t)\
        |WO_SAFE_READ_OFFSET_PER_BYTE(2,uint32_t)|WO_SAFE_READ_OFFSET_PER_BYTE(1,uint32_t)):\
        WO_SAFE_READ_OFFSET_GET_DWORD)
#define WO_SAFE_READ_MOVE_8 \
    ((rt_ip+=8),WO_IS_ODD_IRPTR(8)?\
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
#define WO_OFFSETOF(T, M) ((size_t)&reinterpret_cast<char const volatile&>((((T*)0)->M)))

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
            jit_packed_func_t   m_func = nullptr;
            asmjit::FuncNode* m_jitfunc = nullptr;
            bool                m_finished = false;
            asmjit::CodeHolder  m_code_buffer;
            CompileContextT* _m_ctx = nullptr;
        };
    private:
        std::unordered_map<const byte_t*, function_jit_state*>
            m_compiling_functions;

        const byte_t* m_codes;

        struct WooJitErrorHandler :public asmjit::ErrorHandler
        {
            void handleError(asmjit::Error err, const char* message, asmjit::BaseEmitter*) override
            {
                fprintf(stderr, "AsmJit error: %s(%d)\n", message, (int)err);
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
WO_ASMJIT_IR_ITERFACE_DECL(nop);\
WO_ASMJIT_IR_ITERFACE_DECL(mov);\
WO_ASMJIT_IR_ITERFACE_DECL(addi);\
WO_ASMJIT_IR_ITERFACE_DECL(subi);\
WO_ASMJIT_IR_ITERFACE_DECL(muli);\
WO_ASMJIT_IR_ITERFACE_DECL(divi);\
WO_ASMJIT_IR_ITERFACE_DECL(modi);\
WO_ASMJIT_IR_ITERFACE_DECL(addr);\
WO_ASMJIT_IR_ITERFACE_DECL(subr);\
WO_ASMJIT_IR_ITERFACE_DECL(mulr);\
WO_ASMJIT_IR_ITERFACE_DECL(divr);\
WO_ASMJIT_IR_ITERFACE_DECL(negi);\
WO_ASMJIT_IR_ITERFACE_DECL(negr);\
WO_ASMJIT_IR_ITERFACE_DECL(modr);\
WO_ASMJIT_IR_ITERFACE_DECL(adds);\
WO_ASMJIT_IR_ITERFACE_DECL(psh);\
WO_ASMJIT_IR_ITERFACE_DECL(pop);\
WO_ASMJIT_IR_ITERFACE_DECL(sidarr);\
WO_ASMJIT_IR_ITERFACE_DECL(sidstruct);\
WO_ASMJIT_IR_ITERFACE_DECL(lds);\
WO_ASMJIT_IR_ITERFACE_DECL(sts);\
WO_ASMJIT_IR_ITERFACE_DECL(equb);\
WO_ASMJIT_IR_ITERFACE_DECL(nequb);\
WO_ASMJIT_IR_ITERFACE_DECL(lti);\
WO_ASMJIT_IR_ITERFACE_DECL(gti);\
WO_ASMJIT_IR_ITERFACE_DECL(elti);\
WO_ASMJIT_IR_ITERFACE_DECL(egti);\
WO_ASMJIT_IR_ITERFACE_DECL(land);\
WO_ASMJIT_IR_ITERFACE_DECL(lor);\
WO_ASMJIT_IR_ITERFACE_DECL(sidmap);\
WO_ASMJIT_IR_ITERFACE_DECL(lts);\
WO_ASMJIT_IR_ITERFACE_DECL(gts);\
WO_ASMJIT_IR_ITERFACE_DECL(elts);\
WO_ASMJIT_IR_ITERFACE_DECL(egts);\
WO_ASMJIT_IR_ITERFACE_DECL(ltr);\
WO_ASMJIT_IR_ITERFACE_DECL(gtr);\
WO_ASMJIT_IR_ITERFACE_DECL(eltr);\
WO_ASMJIT_IR_ITERFACE_DECL(egtr);\
WO_ASMJIT_IR_ITERFACE_DECL(movicas);\
WO_ASMJIT_IR_ITERFACE_DECL(call);\
WO_ASMJIT_IR_ITERFACE_DECL(calln);\
WO_ASMJIT_IR_ITERFACE_DECL(setip);\
WO_ASMJIT_IR_ITERFACE_DECL(setipgc);\
WO_ASMJIT_IR_ITERFACE_DECL(mkunion);\
WO_ASMJIT_IR_ITERFACE_DECL(movcast);\
WO_ASMJIT_IR_ITERFACE_DECL(movicastr);\
WO_ASMJIT_IR_ITERFACE_DECL(movrcasti);\
WO_ASMJIT_IR_ITERFACE_DECL(mkclos);\
WO_ASMJIT_IR_ITERFACE_DECL(typeas);\
WO_ASMJIT_IR_ITERFACE_DECL(mkstruct);\
WO_ASMJIT_IR_ITERFACE_DECL(endproc);\
WO_ASMJIT_IR_ITERFACE_DECL(idarr);\
WO_ASMJIT_IR_ITERFACE_DECL(iddict);\
WO_ASMJIT_IR_ITERFACE_DECL(mkcontain);\
WO_ASMJIT_IR_ITERFACE_DECL(idstr);\
WO_ASMJIT_IR_ITERFACE_DECL(equr);\
WO_ASMJIT_IR_ITERFACE_DECL(nequr);\
WO_ASMJIT_IR_ITERFACE_DECL(equs);\
WO_ASMJIT_IR_ITERFACE_DECL(nequs);\
WO_ASMJIT_IR_ITERFACE_DECL(siddict);\
WO_ASMJIT_IR_ITERFACE_DECL(jnequb);\
WO_ASMJIT_IR_ITERFACE_DECL(idstruct);\
WO_ASMJIT_IR_ITERFACE_DECL(unpack)

#define WO_ASMJIT_IR_ITERFACE_DECL(IRNAME) \
virtual bool ir_##IRNAME(CompileContextT* ctx, unsigned int dr, const byte_t*& rt_ip) = 0

        IRS;

        virtual bool ir_ext_panic(CompileContextT* ctx, unsigned int dr, const byte_t*& rt_ip) = 0;
        virtual bool ir_ext_packargs(CompileContextT* ctx, unsigned int dr, const byte_t*& rt_ip) = 0;
        virtual bool ir_ext_cdivilr(CompileContextT* ctx, unsigned int dr, const byte_t*& rt_ip) = 0;
        virtual bool ir_ext_cdivil(CompileContextT* ctx, unsigned int dr, const byte_t*& rt_ip) = 0;
        virtual bool ir_ext_cdivirz(CompileContextT* ctx, unsigned int dr, const byte_t*& rt_ip) = 0;
        virtual bool ir_ext_cdivir(CompileContextT* ctx, unsigned int dr, const byte_t*& rt_ip) = 0;
        virtual bool ir_ext_popn(CompileContextT* ctx, unsigned int dr, const byte_t*& rt_ip) = 0;
        virtual bool ir_ext_addh(CompileContextT* ctx, unsigned int dr, const byte_t*& rt_ip) = 0;
        virtual bool ir_ext_subh(CompileContextT* ctx, unsigned int dr, const byte_t*& rt_ip) = 0;
        virtual bool ir_ext_lth(CompileContextT* ctx, unsigned int dr, const byte_t*& rt_ip) = 0;
        virtual bool ir_ext_gth(CompileContextT* ctx, unsigned int dr, const byte_t*& rt_ip) = 0;
        virtual bool ir_ext_elth(CompileContextT* ctx, unsigned int dr, const byte_t*& rt_ip) = 0;
        virtual bool ir_ext_egth(CompileContextT* ctx, unsigned int dr, const byte_t*& rt_ip) = 0;

        virtual void ir_make_fast_vm_resync(CompileContextT* ctx, const byte_t*& rt_ip) = 0;
        virtual void ir_make_checkpoint_fastcheck(CompileContextT* ctx, const byte_t*& rt_ip) = 0;
        virtual void ir_make_checkpoint_normalcheck(
            CompileContextT* ctx, const byte_t*& rt_ip, const std::optional<asmjit::Label>& label) = 0;
        virtual void ir_make_interrupt(CompileContextT* ctx, vmbase::vm_interrupt_type type) = 0;
        virtual void ir_check_jit_invoke_depth(CompileContextT* ctx, const wo::byte_t* rollback_ip) = 0;

#undef WO_ASMJIT_IR_ITERFACE_DECL

        std::map<uint32_t, asmjit::Label> label_table;

        function_jit_state* current_jit_state = nullptr;

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

        function_jit_state* _analyze_function(
            const byte_t* rt_ip,
            function_jit_state* state,
            CompileContextT* ctx,
            asmjit::BaseCompiler* compiler) noexcept
        {
            byte_t              opcode_dr = static_cast<byte_t>(instruct::abrt);
            instruct::opcode    opcode = static_cast<instruct::opcode>(opcode_dr & 0b11111100u);
            unsigned int        dr = opcode_dr & 0b00000011u;

            for (;;)
            {
                uint32_t current_ip_byteoffset = (uint32_t)(rt_ip - m_codes);

                bind_ip(compiler, current_ip_byteoffset);

                opcode_dr = *(rt_ip++);
                opcode = (instruct::opcode)(opcode_dr & 0b11111100u);
                dr = opcode_dr & 0b00000011u;

#define WO_JIT_NOT_SUPPORT do{state->m_state = function_jit_state::state::FAILED; return state; }while(0)

                switch (opcode)
                {
#define WO_ASMJIT_IR_ITERFACE_DECL(IRNAME) \
case instruct::opcode::IRNAME:{if (ir_##IRNAME(ctx, dr, rt_ip)) break; else WO_JIT_NOT_SUPPORT;}
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
                            case instruct::extern_opcode_page_0::pack:
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
                            case instruct::extern_opcode_page_0::popn:
                                if (ir_ext_popn(ctx, dr, rt_ip))
                                    break;
                                else
                                    WO_JIT_NOT_SUPPORT;
                            case instruct::extern_opcode_page_0::addh:
                                if (ir_ext_addh(ctx, dr, rt_ip))
                                    break;
                                else
                                    WO_JIT_NOT_SUPPORT;
                            case instruct::extern_opcode_page_0::subh:
                                if (ir_ext_subh(ctx, dr, rt_ip))
                                    break;
                                else
                                    WO_JIT_NOT_SUPPORT;
                            case instruct::extern_opcode_page_0::lth:
                                if (ir_ext_lth(ctx, dr, rt_ip))
                                    break;
                                else
                                    WO_JIT_NOT_SUPPORT;
                            case instruct::extern_opcode_page_0::gth:
                                if (ir_ext_gth(ctx, dr, rt_ip))
                                    break;
                                else
                                    WO_JIT_NOT_SUPPORT;
                            case instruct::extern_opcode_page_0::elth:
                                if (ir_ext_elth(ctx, dr, rt_ip))
                                    break;
                                else
                                    WO_JIT_NOT_SUPPORT;
                            case instruct::extern_opcode_page_0::egth:
                                if (ir_ext_egth(ctx, dr, rt_ip))
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

                                auto err = get_jit_runtime().add(&state->m_func, &state->m_code_buffer);
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
            WO_UNREACHABLE();
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

            state->m_func = reinterpret_cast<jit_packed_func_t>(
                static_cast<intptr_t>(
                    0x1234567812345678));

            current_jit_state = state;

            state->m_state = function_jit_state::state::COMPILING;
            state->m_func_offset = offset;
            WooJitErrorHandler woo_jit_error_handler;

            state->m_code_buffer.init(get_jit_runtime().environment());
            state->m_code_buffer.setErrorHandler(&woo_jit_error_handler);

            asmjit::BaseCompiler* compiler;
            state->_m_ctx = this->prepare_compiler(
                &compiler, &state->m_code_buffer, state, env);

            compiler->setErrorHandler(&woo_jit_error_handler);

            ir_check_jit_invoke_depth(state->_m_ctx, rt_ip);
            ir_make_checkpoint_fastcheck(state->_m_ctx, rt_ip);

            auto* result = _analyze_function(rt_ip, state, state->_m_ctx, compiler);

            current_jit_state = backup;
            return result;
        }
        void analyze_jit(byte_t* codebuf, runtime_env* env) noexcept
        {
            if (env->jit_code_holder.has_value())
                return;

            m_codes = codebuf;

            auto& update_jit_code_holder = env->jit_code_holder.emplace();

            // This function should not been jit compiled.
            wo_assert(env->jit_code_holder.has_value());

            // 1. for all function, trying to jit compile them:
            for (size_t func_offset : env->meta_data_for_jit._functions_offsets_for_jit)
                analyze_function(codebuf, func_offset, env);

            for (auto& [_, stat] : m_compiling_functions)
            {
                wo_assert(stat->m_state == function_jit_state::FINISHED);
                wo_assert(nullptr != stat->m_func);

                wo_assert(stat->m_finished);
                update_jit_code_holder.insert(std::make_pair(stat->m_func_offset, stat->m_func));

                this->free_compiler(stat->_m_ctx);
            }

            for (auto funtions_constant_offset : env->meta_data_for_jit._functions_constant_idx_for_jit)
            {
                auto* val = &env->constant_and_global_storage[funtions_constant_offset];
                wo_assert(val->m_type == value::valuetype::script_func_type);

                auto& func_state = m_compiling_functions.at(val->m_script_func);
                if (func_state->m_state == function_jit_state::state::FINISHED)
                {
                    wo_assert(func_state->m_func != nullptr
                        && *func_state->m_func != nullptr);
                    val->set_native_func(func_state->m_func);
                }
                else
                    wo_assert(func_state->m_state == function_jit_state::state::FAILED);
            }
            for (auto& [tuple_constant_offset, function_index] :
                env->meta_data_for_jit._functions_constant_in_tuple_idx_for_jit)
            {
                auto* val = &env->constant_and_global_storage[tuple_constant_offset];
                wo_assert(val->m_type == value::valuetype::struct_type
                    && function_index < val->m_structure->m_count);

                auto& func_val = val->m_structure->m_values[function_index];
                wo_assert(func_val.m_type == value::valuetype::script_func_type);

                auto& func_state = m_compiling_functions.at(func_val.m_script_func);
                if (func_state->m_state == function_jit_state::state::FINISHED)
                {
                    wo_assert(func_state->m_func != nullptr);
                    func_val.set_native_func(func_state->m_func);
                }
                else
                    wo_assert(func_state->m_state == function_jit_state::state::FAILED);
            }
            for (auto calln_offset : env->meta_data_for_jit._calln_opcode_offsets_for_jit)
            {
                wo::instruct::opcode* calln = (wo::instruct::opcode*)(codebuf + calln_offset);
                wo_assert(((*calln) & 0b11111100) == wo::instruct::opcode::calln);
                wo_assert(((*calln) & 0b00000011) == 0b00);

                byte_t* rt_ip = codebuf + calln_offset + 1;

                // READ NEXT 8 BYTE
                const byte_t* abs_function_place = codebuf + WO_SAFE_READ_MOVE_4;

                // m_compiling_functions must have this ip
                auto& func_state = m_compiling_functions.at(abs_function_place);
                if (func_state->m_state == function_jit_state::state::FINISHED)
                {
                    wo_assert(func_state->m_func != nullptr);

                    *calln = wo::instruct::opcode::callnjit;
                    const byte_t* jitfunc = reinterpret_cast<const byte_t*>(&func_state->m_func);
                    byte_t* ipbuf = codebuf + calln_offset + 1;

                    for (size_t i = 0; i < 8; ++i)
                        *(ipbuf + i) = *(jitfunc + i);
                }
                else
                    wo_assert(func_state->m_state == function_jit_state::state::FAILED);
            }
            for (auto mkclos_offset : env->meta_data_for_jit._mkclos_opcode_offsets_for_jit)
            {
                wo::instruct::opcode* mkclos = (wo::instruct::opcode*)(codebuf + mkclos_offset);
                wo_assert(((*mkclos) & 0b11111100) == wo::instruct::opcode::mkclos);
                wo_assert(((*mkclos) & 0b00000011) == 0b00);

                byte_t* rt_ip = codebuf + mkclos_offset + 1;

                // SKIP 2 BYTE
                WO_SAFE_READ_MOVE_2;

                // READ NEXT 8 BYTE
                const byte_t* abs_function_place = codebuf + WO_SAFE_READ_MOVE_4;

                // m_compiling_functions must have this ip
                auto& func_state = m_compiling_functions.at(abs_function_place);
                if (func_state->m_state == function_jit_state::state::FINISHED)
                {
                    wo_assert(func_state->m_func != nullptr);

                    *mkclos = (wo::instruct::opcode)(wo::instruct::opcode::mkclos | 0b10);
                    const byte_t* jitfunc = reinterpret_cast<const byte_t*>(&func_state->m_func);
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
            if (env->jit_code_holder.has_value())
            {
                for (const auto& [_, holder_jitfp] : env->jit_code_holder.value())
                    wo_assure(!get_jit_runtime().release(holder_jitfp));
            }
        }
        static void _invoke_vm_interrupt(wo::vmbase* vmm, wo::vmbase::vm_interrupt_type type)
        {
            (void)vmm->interrupt(type);
        }
        static wo_result_t _invoke_vm_checkpoint(
            wo::vmbase* vmm, wo::value* rt_sp, wo::value* rt_bp, const byte_t* rt_ip)
        {
            for (;;)
            {
                const auto interrupt_state = vmm->vm_interrupt.load(std::memory_order_acquire);
                if (interrupt_state == wo::vmbase::vm_interrupt_type::NOTHING)
                    break;

                if (interrupt_state & wo::vmbase::vm_interrupt_type::GC_INTERRUPT)
                {
                    vmm->sp = rt_sp;
                    vmm->gc_checkpoint_self_mark();
                }
                ///////////////////////////////////////////////////////////////////////
                if (interrupt_state & wo::vmbase::vm_interrupt_type::GC_HANGUP_INTERRUPT)
                {
                    vmm->sp = rt_sp;
                    if (vmm->clear_interrupt(wo::vmbase::vm_interrupt_type::GC_HANGUP_INTERRUPT))
                        vmm->hangup();
                }
                else if (interrupt_state & wo::vmbase::vm_interrupt_type::ABORT_INTERRUPT)
                {
                    // ABORTED VM WILL NOT ABLE TO RUN AGAIN, SO DO NOT
                    // CLEAR ABORT_INTERRUPT
                    // store current context, then break out of jit function
                    vmm->ip = rt_ip;
                    vmm->sp = rt_sp;
                    vmm->bp = rt_bp;

                    return wo_result_t::WO_API_SYNC_CHANGED_VM_STATE;
                }
                else if (interrupt_state & wo::vmbase::vm_interrupt_type::BR_YIELD_INTERRUPT)
                {
                    // NOTE: DONOT CLEAR BR_YIELD_INTERRUPT, IT SHOULD BE CLEAR IN VM-RUN
                    // store current context, then break out of jit function
                    vmm->ip = rt_ip;
                    vmm->sp = rt_sp;
                    vmm->bp = rt_bp;

                    return wo_result_t::WO_API_SYNC_CHANGED_VM_STATE; // return 
                }
                else if (interrupt_state & wo::vmbase::vm_interrupt_type::LEAVE_INTERRUPT)
                {
                    // That should not be happend...
                    wo_error("Virtual machine handled a LEAVE_INTERRUPT.");
                }
                else if (interrupt_state & wo::vmbase::vm_interrupt_type::PENDING_INTERRUPT)
                {
                    // That should not be happend...
                    wo_error("Virtual machine handled a PENDING_INTERRUPT.");
                }
                else if (interrupt_state & wo::vmbase::vm_interrupt_type::STACK_OCCUPYING_INTERRUPT)
                {
                    while (vmm->check_interrupt(vmbase::vm_interrupt_type::STACK_OCCUPYING_INTERRUPT))
                        wo::gcbase::_shared_spin::spin_loop_hint();
                }
                else if (interrupt_state & wo::vmbase::vm_interrupt_type::STACK_OVERFLOW_INTERRUPT)
                {
                    vmm->ip = rt_ip;
                    vmm->sp = rt_sp;
                    vmm->bp = rt_bp;
                    return wo_result_t::WO_API_SYNC_CHANGED_VM_STATE;
                }
                else if (interrupt_state & wo::vmbase::vm_interrupt_type::SHRINK_STACK_INTERRUPT)
                {
                    vmm->ip = rt_ip;
                    vmm->sp = rt_sp;
                    vmm->bp = rt_bp;
                    return wo_result_t::WO_API_SYNC_CHANGED_VM_STATE;
                }
                // ATTENTION: it should be last interrupt..
                else if (interrupt_state & wo::vmbase::vm_interrupt_type::DEBUG_INTERRUPT)
                {
                    vmm->ip = rt_ip;
                    vmm->sp = rt_sp;
                    vmm->bp = rt_bp;
                    return wo_result_t::WO_API_SYNC_CHANGED_VM_STATE; // return 
                }
                else
                {
                    // a vm_interrupt is invalid now, just roll back one byte and continue~
                    // so here do nothing
                    wo_assert(interrupt_state == 0
                        || interrupt_state == wo::vmbase::vm_interrupt_type::GC_INTERRUPT);
                }
            }
            return wo_result_t::WO_API_NORMAL;
        }
        static void check_result_is_normal_for_vmfunc(
            asmjit::x86::Compiler& x86compiler,
            asmjit::x86::Gp vm,
            asmjit::x86::Gp& result)
        {
            auto normal = x86compiler.newLabel();
            wo_assure(!x86compiler.cmp(result, asmjit::Imm(wo_result_t::WO_API_NORMAL)));
            wo_assure(!x86compiler.je(normal));
#ifndef NDEBUG
            // Assert for WO_API_RESYNC_JIT_STATE_TO_VM_STATE
            auto not_resync = x86compiler.newLabel();
            wo_assure(!x86compiler.cmp(result, asmjit::Imm(wo_result_t::WO_API_RESYNC_JIT_STATE_TO_VM_STATE)));
            wo_assure(!x86compiler.jne(not_resync));
            wo_assure(!x86compiler.int3());
            wo_assure(!x86compiler.bind(not_resync));
#endif
            wo_assure(!x86compiler.dec(asmjit::x86::byte_ptr(vm, offsetof(vmbase, extern_state_jit_call_depth))));
            wo_assure(!x86compiler.ret(result)); // break this execute!!!

            wo_assure(!x86compiler.bind(normal));
        }
        static void check_result_is_normal_for_nativefunc(
            asmjit::x86::Compiler& x86compiler,
            asmjit::x86::Gp vm,
            asmjit::x86::Gp& result,
            const byte_t* rt_ip,
            asmjit::x86::Gp ret_spoffset_x16,
            asmjit::x86::Gp ret_bpoffset_x16)
        {
            auto normal = x86compiler.newLabel();
            wo_assure(!x86compiler.cmp(result, asmjit::Imm(wo_result_t::WO_API_NORMAL)));
            wo_assure(!x86compiler.je(normal));

            auto not_resync = x86compiler.newLabel();
            wo_assure(!x86compiler.cmp(result, asmjit::Imm(wo_result_t::WO_API_RESYNC_JIT_STATE_TO_VM_STATE)));
            wo_assure(!x86compiler.jne(not_resync));

            /*
            vm->sp = vm->sb - sp_offset;
            vm->bp = vm->sb - bp_offset;
            vm->ip = vm->env->rt_codes + retip;
            */
            auto vm_sb_bp = x86compiler.newInt64();
            auto vm_sp = x86compiler.newInt64();
            auto vm_ip = x86compiler.newInt64();
            wo_assure(!x86compiler.mov(vm_sb_bp, asmjit::x86::qword_ptr(vm, offsetof(vmbase, sb))));
            wo_assure(!x86compiler.mov(vm_sp, vm_sb_bp));

            wo_assure(!x86compiler.sub(vm_sp, ret_spoffset_x16));
            wo_assure(!x86compiler.sub(vm_sb_bp, ret_bpoffset_x16));

            wo_assure(!x86compiler.mov(asmjit::x86::qword_ptr(vm, offsetof(vmbase, sp)), vm_sp));
            wo_assure(!x86compiler.mov(asmjit::x86::qword_ptr(vm, offsetof(vmbase, bp)), vm_sb_bp));

            wo_assure(!x86compiler.mov(vm_ip, asmjit::Imm(rt_ip)));
            wo_assure(!x86compiler.mov(asmjit::x86::qword_ptr(vm, offsetof(vmbase, ip)), vm_ip));

            wo_assure(!x86compiler.dec(asmjit::x86::byte_ptr(vm, offsetof(vmbase, extern_state_jit_call_depth))));
            wo_assure(!x86compiler.mov(result, asmjit::Imm(wo_result_t::WO_API_SYNC_CHANGED_VM_STATE)));

            wo_assure(!x86compiler.bind(not_resync));

            wo_assure(!x86compiler.dec(asmjit::x86::byte_ptr(vm, offsetof(vmbase, extern_state_jit_call_depth))));
            wo_assure(!x86compiler.ret(result)); // break this execute!!!

            wo_assure(!x86compiler.bind(normal));
        }

        WO_FORCE_INLINE static wo_result_t native_do_calln_vmfunc(
            vmbase* vm, wo_extern_native_func_t call_aim_native_func, uint32_t retip, value* rt_sp, value* rt_bp)
        {
            size_t sp_offset = vm->sb - rt_sp;
            size_t bp_offset = vm->sb - rt_bp;

            rt_sp->m_type = value::valuetype::callstack;
            rt_sp->m_vmcallstack.ret_ip = retip;
            rt_sp->m_vmcallstack.bp = (uint32_t)bp_offset;
            rt_bp = --rt_sp;

            vm->bp = vm->sp = rt_sp;

            vm->ip = reinterpret_cast<byte_t*>(call_aim_native_func);

            const wo_result_t func_call_result = call_aim_native_func(
                reinterpret_cast<wo_vm>(vm),
                std::launder(reinterpret_cast<wo_value>(rt_sp + 2)));
            if (func_call_result == WO_API_RESYNC_JIT_STATE_TO_VM_STATE)
            {
                vm->sp = vm->sb - sp_offset;
                vm->bp = vm->sb - bp_offset;
                vm->ip = vm->env->rt_codes + retip;

                return WO_API_SYNC_CHANGED_VM_STATE;
            }
            return func_call_result;
        }
        WO_FORCE_INLINE static wo_result_t native_do_calln_vmfunc_may_far(
            vmbase* vm, wo_extern_native_func_t call_aim_native_func, const byte_t* retip, value* rt_sp, value* rt_bp)
        {
            size_t sp_offset = vm->sb - rt_sp;
            size_t bp_offset = vm->sb - rt_bp;

            rt_sp->m_type = value::valuetype::far_callstack;
            rt_sp->m_farcallstack = retip;
            rt_sp->m_ext_farcallstack_bp = (uint32_t)bp_offset;
            rt_bp = --rt_sp;

            vm->bp = vm->sp = rt_sp;

            vm->ip = reinterpret_cast<byte_t*>(call_aim_native_func);

            const wo_result_t func_call_result = call_aim_native_func(
                reinterpret_cast<wo_vm>(vm),
                std::launder(reinterpret_cast<wo_value>(rt_sp + 2)));
            if (func_call_result == WO_API_RESYNC_JIT_STATE_TO_VM_STATE)
            {
                vm->sp = vm->sb - sp_offset;
                vm->bp = vm->sb - bp_offset;
                vm->ip = retip;

                return WO_API_SYNC_CHANGED_VM_STATE;
            }
            return func_call_result;
        }
        static wo_result_t native_do_call_vmfunc_with_stack_overflow_check(
            vmbase* vm,
            value* target_function,
            const wo::byte_t* rollback_ip,
            const wo::byte_t* retip,
            value* rt_sp,
            value* rt_bp)
        {
            switch (target_function->m_type)
            {
            case value::valuetype::native_func_type:
                if (rt_sp <= vm->stack_storage)
                {
                    vm->interrupt(wo::vmbase::vm_interrupt_type::STACK_OVERFLOW_INTERRUPT);
                    break;
                }
                return native_do_calln_vmfunc_may_far(
                    vm, (wo_extern_native_func_t)(void*)target_function->m_handle, retip, rt_sp, rt_bp);
            case value::valuetype::closure_type:
                if (target_function->m_closure->m_native_call)
                {
                    if (rt_sp - target_function->m_closure->m_closure_args_count - 1 < vm->stack_storage)
                    {
                        vm->interrupt(wo::vmbase::vm_interrupt_type::STACK_OVERFLOW_INTERRUPT);
                        break;
                    }

                    for (uint16_t idx = 0; idx < target_function->m_closure->m_closure_args_count; ++idx)
                        (rt_sp--)->set_val(&target_function->m_closure->m_closure_args[idx]);

                    return native_do_calln_vmfunc_may_far(
                        vm, target_function->m_closure->m_native_func, retip, rt_sp, rt_bp);
                }
                /* fallthrough */
                [[fallthrough]];
            default:
                // Try to invoke a script function address, it cannot be handled in jit,
                // Give a WO_API_SYNC_CHANGED_VM_STATE request and roll back to vm.
                ;
            }
            vm->ip = rollback_ip;
            vm->sp = rt_sp;
            vm->bp = rt_bp;
            return wo_result_t::WO_API_SYNC_CHANGED_VM_STATE;
        }
        static void _vmjitcall_panic(
            vmbase* vm, wo::value* opnum1, const byte_t* rt_ip, value* rt_sp, value* rt_bp)
        {
            vm->ip = rt_ip;
            vm->sp = rt_sp;
            vm->bp = rt_bp;

            wo_fail(WO_FAIL_UNEXPECTED, "%s", wo_cast_string(
                std::launder(reinterpret_cast<wo_value>(opnum1))));
        }
        static void _vmjitcall_adds(wo::value* opnum1, wo::string_t* opnum2)
        {
            opnum1->set_gcunit<wo::value::valuetype::string_type>(
                string_t::gc_new<gcbase::gctype::young>(*opnum1->m_string + *opnum2));
        }
        static void _vmjitcall_write_barrier(wo::value* opnum1)
        {
            wo::value::write_barrier(opnum1);
        }
        static const char* _vmjitcall_idarr(wo::value* cr, wo::array_t* opnum1, wo_integer_t opnum2)
        {
            gcbase::gc_read_guard gwg1(opnum1);

            const size_t index = (size_t)opnum2;
            if (index >= opnum1->size())
                return "Index out of range.";
            else
                cr->set_val(&opnum1->at(index));

            return nullptr;
        }
        static const char* _vmjitcall_iddict(wo::value* cr, wo::dictionary_t* opnum1, wo::value* opnum2)
        {
            gcbase::gc_read_guard gwg1(opnum1);

            auto fnd = opnum1->find(*opnum2);
            if (fnd != opnum1->end())
            {
                auto* result = &fnd->second;
                cr->set_val(result);
            }
            else
                return "No such key in current dict.";
            return nullptr;
        }
        static const char* _vmjitcall_siddict(wo::dictionary_t* opnum1, wo::value* opnum2, wo::value* opnum3)
        {
            gcbase::gc_write_guard gwg1(opnum1);

            auto fnd = opnum1->find(*opnum2);
            if (fnd != opnum1->end())
            {
                auto* result = &fnd->second;
                if (wo::gc::gc_is_marking())
                    wo::value::write_barrier(result);
                result->set_val(opnum3);
            }
            else
                return "No such key in current dict.";
            return nullptr;
        }
        static void _vmjitcall_sidmap(wo::dictionary_t* opnum1, wo::value* opnum2, wo::value* opnum3)
        {
            gcbase::gc_modify_write_guard gwg1(opnum1);

            auto* result = &(*opnum1)[*opnum2];
            if (wo::gc::gc_is_marking())
                wo::value::write_barrier(result);
            result->set_val(opnum3);
        }
        static const char* _vmjitcall_sidarr(wo::array_t* opnum1, wo_integer_t opnum2, wo::value* opnum3)
        {
            gcbase::gc_write_guard gwg1(opnum1);

            size_t index = (size_t)opnum2;
            if (index >= opnum1->size())
                return "Index out of range.";
            else
            {
                auto* result = &opnum1->at(index);
                if (wo::gc::gc_is_marking())
                    wo::value::write_barrier(result);
                result->set_val(opnum3);
            }
            return nullptr;
        }
        static void _vmjitcall_sidstruct(wo::structure_t* opnum1, wo::value* opnum2, uint16_t offset)
        {
            gcbase::gc_write_guard gwg1(opnum1);

            auto* result = opnum1->m_values + offset;
            if (wo::gc::gc_is_marking())
                wo::value::write_barrier(result);
            result->set_val(opnum2);
        }
        static void _vmjitcall_equs(wo::value* opnum1, wo::string_t* opnum2, wo::string_t* opnum3)
        {
            opnum1->set_bool(
                opnum2 == opnum3
                || *opnum2 == *opnum3
            );
        }
        static void _vmjitcall_nequs(wo::value* opnum1, wo::string_t* opnum2, wo::string_t* opnum3)
        {
            opnum1->set_bool(
                opnum2 != opnum3
                && *opnum2 != *opnum3
            );
        }
        static void _vmjitcall_idstr(wo::value* opnum1, wo::string_t* opnum2, wo_integer_t opnum3)
        {
            wo_wchar_t out_str = wo_strn_get_char(
                opnum2->c_str(),
                opnum2->size(),
                (size_t)opnum3);
            opnum1->set_integer((wo_integer_t)out_str);
        }
        static void _vmjitcall_abrt(const char* msg)
        {
            wo_error(msg);
        }
        static void _vmjitcall_fail(
            wo::vmbase* vmm,
            uint32_t id,
            const char* msg,
            const byte_t* rt_ip,
            wo::value* rt_sp,
            wo::value* rt_bp)
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
        asmjit::x86::Gp _vmshead;
        asmjit::x86::Gp _vmreg;
        asmjit::x86::Gp _vmcr;
        asmjit::x86::Gp _vmtc;
        asmjit::x86::Gp _vmtp;

        runtime_env* env;

        X64CompileContext(asmjit::CodeHolder* code, runtime_env* _env)
            : c(code)
            , env(_env)
        {
            _vmbase = c.newUIntPtr();
            _vmsbp = c.newUIntPtr();
            _vmssp = c.newUIntPtr();
            _vmshead = c.newUIntPtr();
            _vmreg = c.newUIntPtr();
            _vmcr = c.newUIntPtr();
            _vmtc = c.newUIntPtr();
            _vmtp = c.newUIntPtr();
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
            bool is_constant_i32() const
            {
                return m_is_constant && m_constant->m_integer >= INT32_MIN && m_constant->m_integer <= INT32_MAX;
            }
            bool is_constant_h32() const
            {
                // NOTE:
                // Using INT32_MAX as the judgment condition here is intentional.
                // This is because some instructions that do not support 64-bit literals
                // may fill according to the sign bit when the operand width is 64 bits;
                // therefore, we need to ensure the sign bit is 0 here.
                return m_is_constant && m_constant->m_handle <= INT32_MAX;
            }
            bool is_constant_real() const
            {
                return m_is_constant;
            }
            bool is_constant_and_not_tag(X64CompileContext* ctx) const
            {
                if (is_constant())
                {
                    auto offset = (size_t)
                        (m_constant - ctx->env->constant_and_global_storage);
                    if (std::find(
                        ctx->env->meta_data_for_jit._functions_constant_idx_for_jit.begin(),
                        ctx->env->meta_data_for_jit._functions_constant_idx_for_jit.end(),
                        offset) == ctx->env->meta_data_for_jit._functions_constant_idx_for_jit.end())
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
                    wo_assure(!x86compiler.lea(
                        result,
                        asmjit::x86::qword_ptr(
                            stack_bp,
                            WO_SIGNED_SHIFT(WO_IPVAL_MOVE_1) * sizeof(wo::value))));
                    return may_constant_x86Gp{ &x86compiler,false,nullptr,result };
                }
                else
                {
                    // from reg
                    auto result = x86compiler.newUIntPtr();
                    wo_assure(!x86compiler.lea(result, asmjit::x86::qword_ptr(reg, WO_IPVAL_MOVE_1 * sizeof(wo::value))));
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
                    return may_constant_x86Gp{ &x86compiler, true, env->constant_and_global_storage + const_global_index };
                }
                else
                {
                    auto result = x86compiler.newUIntPtr();
                    wo_assure(!x86compiler.mov(result, (size_t)(env->constant_and_global_storage + const_global_index)));
                    return may_constant_x86Gp{ &x86compiler,false,nullptr,result };
                }
            }
        }
        virtual void ir_make_fast_vm_resync(X64CompileContext* ctx, const byte_t*& rt_ip)
        {
            auto ipaddr = ctx->c.newIntPtr();
            wo_assure(!ctx->c.mov(ipaddr, asmjit::Imm((intptr_t)rt_ip)));
            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmbase, offsetof(vmbase, ip)), ipaddr));
            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmbase, offsetof(vmbase, sp)), ctx->_vmssp));
            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmbase, offsetof(vmbase, bp)), ctx->_vmsbp));

            auto resync = ctx->c.newInt32();
            wo_assure(!ctx->c.mov(resync, asmjit::Imm(wo_result_t::WO_API_SYNC_CHANGED_VM_STATE)));
            wo_assure(!ctx->c.ret(resync));
        }
        virtual void ir_make_checkpoint_normalcheck(
            X64CompileContext* ctx, const byte_t*& rt_ip, const std::optional<asmjit::Label>& label) override
        {
            auto no_interrupt_label =
                label.has_value() ? label.value() : ctx->c.newLabel();

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
            wo_assure(!ctx->c.dec(asmjit::x86::byte_ptr(ctx->_vmbase, offsetof(vmbase, extern_state_jit_call_depth))));
            wo_assure(!ctx->c.ret(result)); // break this execute!!!
            wo_assure(!ctx->c.bind(no_interrupt_label));
        }
        virtual void ir_make_checkpoint_fastcheck(X64CompileContext* ctx, const byte_t*& rt_ip) override
        {
            auto no_interrupt_label = ctx->c.newLabel();
            static_assert(sizeof(wo::vmbase::vm_interrupt) == 4);
            static_assert(sizeof(wo::vmbase::vm_interrupt) == sizeof(std::atomic<uint32_t>));

            wo_assure(!ctx->c.cmp(asmjit::x86::dword_ptr(ctx->_vmbase, offsetof(vmbase, vm_interrupt)), 0));
            wo_assure(!ctx->c.je(no_interrupt_label));

            ir_make_checkpoint_normalcheck(ctx, rt_ip, no_interrupt_label);
        }
        virtual void ir_make_interrupt(X64CompileContext* ctx, vmbase::vm_interrupt_type type) override
        {
            asmjit::InvokeNode* invoke_node;
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&_invoke_vm_interrupt,
                asmjit::FuncSignatureT<void, vmbase*, vmbase::vm_interrupt_type>()));
            invoke_node->setArg(0, ctx->_vmbase);
            invoke_node->setArg(1, asmjit::Imm(type));
        }
        virtual void ir_check_jit_invoke_depth(X64CompileContext* ctx, const wo::byte_t* rollback_ip) override
        {
            auto check_ok_label = ctx->c.newLabel();
            auto depth_count = ctx->c.newUInt8();
            wo_assure(!ctx->c.mov(depth_count, asmjit::x86::byte_ptr(ctx->_vmbase, offsetof(vmbase, extern_state_jit_call_depth))));
            wo_assure(!ctx->c.cmp(depth_count, asmjit::Imm(wo::vmbase::VM_MAX_JIT_FUNCTION_DEPTH)));
            wo_assure(!ctx->c.jb(check_ok_label));

            auto rollback_ip_addr = ctx->c.newUInt64();
            wo_assure(!ctx->c.mov(rollback_ip_addr, asmjit::Imm((intptr_t)rollback_ip)));
            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmbase, offsetof(vmbase, ip)), rollback_ip_addr));
            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmbase, offsetof(vmbase, sp)), ctx->_vmssp));
            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmbase, offsetof(vmbase, bp)), ctx->_vmsbp));
            wo_assure(!ctx->c.mov(asmjit::x86::byte_ptr(ctx->_vmbase, offsetof(vmbase, extern_state_jit_call_depth)), asmjit::Imm(0)));

            wo_assure(!ctx->c.mov(rollback_ip_addr, asmjit::Imm(wo_result_t::WO_API_SYNC_CHANGED_VM_STATE)));
            wo_assure(!ctx->c.ret(rollback_ip_addr));

            wo_assure(!ctx->c.bind(check_ok_label));
            wo_assure(!ctx->c.inc(depth_count));
            wo_assure(!ctx->c.mov(asmjit::x86::byte_ptr(ctx->_vmbase, offsetof(vmbase, extern_state_jit_call_depth)), depth_count));
        }

        // See ISSUE: 25-08-16 in wo_vm.cpp
        static void write_barrier_for_opnum1_if_write_global(
            X64CompileContext* ctx, may_constant_x86Gp& opnum1, unsigned int dr)
        {
            if ((dr & 0b10) == 0)
            {
                // Is global.
                // if (wo::gc::gc_is_marking())
                auto marking_flag = ctx->c.newUInt8();
                wo_assure(!ctx->c.mov(
                    marking_flag,
                    asmjit::x86::byte_ptr(reinterpret_cast<intptr_t>(&wo::gc::_gc_is_marking))));

                auto not_marking = ctx->c.newLabel();

                wo_assure(!ctx->c.test(marking_flag, asmjit::Imm(0xFF)));
                wo_assure(!ctx->c.jz(not_marking));

                auto op1 = opnum1.gp_value();

                asmjit::InvokeNode* invoke_node;
                wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&_vmjitcall_write_barrier,
                    asmjit::FuncSignatureT<void, wo::value*>()));

                invoke_node->setArg(0, op1);

                wo_assure(!ctx->c.bind(not_marking));
            }
        }

        static asmjit::x86::Gp x86_set_imm(asmjit::x86::Compiler& x86compiler, asmjit::x86::Gp val, const wo::value& instance)
        {
            wo_assure(!x86compiler.mov(asmjit::x86::byte_ptr(val, offsetof(value, m_type)), (uint8_t)instance.m_type));


            if (instance.m_type == wo::value::valuetype::integer_type
                ? instance.m_integer >= INT32_MIN && instance.m_integer <= INT32_MAX
                // NOTE:
               // Using INT32_MAX as the judgment condition here is intentional.
               // This is because some instructions that do not support 64-bit literals
               // may fill according to the sign bit when the operand width is 64 bits;
               // therefore, we need to ensure the sign bit is 0 here.
                : instance.m_value_field <= INT32_MAX)
            {
                wo_assure(!x86compiler.mov(
                    asmjit::x86::qword_ptr(val, offsetof(value, m_value_field)), instance.m_value_field));
            }
            else
            {
                auto data_of_val = x86compiler.newUInt64();
                wo_assure(!x86compiler.mov(data_of_val, instance.m_value_field));
                wo_assure(!x86compiler.mov(asmjit::x86::qword_ptr(val, offsetof(value, m_value_field)), data_of_val));
            }

            return val;
        }
        static asmjit::x86::Gp x86_set_val(asmjit::x86::Compiler& x86compiler, asmjit::x86::Gp val, asmjit::x86::Gp val2)
        {
            auto type_of_val2 = x86compiler.newUInt8();
            wo_assure(!x86compiler.mov(type_of_val2, asmjit::x86::byte_ptr(val2, offsetof(value, m_type))));
            wo_assure(!x86compiler.mov(asmjit::x86::byte_ptr(val, offsetof(value, m_type)), type_of_val2));

            auto data_of_val2 = x86compiler.newUInt64();
            wo_assure(!x86compiler.mov(data_of_val2, asmjit::x86::qword_ptr(val2, offsetof(value, m_value_field))));
            wo_assure(!x86compiler.mov(asmjit::x86::qword_ptr(val, offsetof(value, m_value_field)), data_of_val2));

            return val;
        }
        static void x86_do_calln_native_func(
            asmjit::x86::Compiler& x86compiler,
            asmjit::x86::Gp vm,
            wo_extern_native_func_t call_aim_native_func,
            const byte_t* codes,
            const byte_t* rt_ip,
            asmjit::x86::Gp rt_sp,
            asmjit::x86::Gp rt_bp,
            bool is_native_call)
        {
            auto sp_offset = x86compiler.newInt64();
            auto bp_offset = x86compiler.newInt64();

            wo_assure(!x86compiler.mov(sp_offset, asmjit::x86::qword_ptr(vm, offsetof(vmbase, sb))));
            wo_assure(!x86compiler.mov(bp_offset, sp_offset));
            wo_assure(!x86compiler.sub(sp_offset, rt_sp));
            wo_assure(!x86compiler.sub(bp_offset, rt_bp));

            wo::value callstack;

            if (is_native_call)
            {
                // For debugging purposes, native calls use far callstack to preserve 
                // the complete return address.
                // Since native calls do not use the `ret` instruction to return, there 
                // is no performance penalty
                callstack.m_type = wo::value::valuetype::far_callstack;
                callstack.m_farcallstack = rt_ip;
            }
            else
            {
                callstack.m_type = wo::value::valuetype::callstack;
                callstack.m_vmcallstack.bp = 0;
                callstack.m_vmcallstack.ret_ip = (uint32_t)(rt_ip - codes);
            }
            x86_set_imm(x86compiler, rt_sp, callstack);

            auto shift_bpoffset = x86compiler.newInt64();
            wo_assure(!x86compiler.mov(shift_bpoffset, bp_offset));
            wo_assure(!x86compiler.shr(shift_bpoffset, asmjit::Imm(4)));

            if (is_native_call)
            {
                wo_assure(!x86compiler.mov(
                    asmjit::x86::dword_ptr(
                        rt_sp,
                        offsetof(value, m_ext_farcallstack_bp)),
                    shift_bpoffset.r32()));
            }
            else
            {
                wo_assure(!x86compiler.mov(
                    asmjit::x86::dword_ptr(
                        rt_sp,
                        offsetof(value, m_vmcallstack) + offsetof(value::callstack_t, bp)),
                    shift_bpoffset.r32()));
            }

            auto callargptr = x86compiler.newUIntPtr();
            wo_assure(!x86compiler.lea(
                callargptr, asmjit::x86::qword_ptr(rt_sp, 1 * (int32_t)sizeof(wo::value))));

            // Only store sp/bp/ip for native call, jit call does not need it.
            if (is_native_call)
            {
                // Sync vm's sp/bp
                // vm->bp = vm->sp = --rt_sp;
                // vm->ip = reinterpret_cast<byte_t*>(call_aim_native_func);
                auto new_sp_bp = x86compiler.newUIntPtr();
                wo_assure(!x86compiler.lea(
                    new_sp_bp, asmjit::x86::qword_ptr(rt_sp, -1 * (int32_t)sizeof(wo::value))));
                wo_assure(!x86compiler.mov(
                    asmjit::x86::qword_ptr(vm, offsetof(vmbase, sp)), new_sp_bp));
                wo_assure(!x86compiler.mov(
                    asmjit::x86::qword_ptr(vm, offsetof(vmbase, bp)), new_sp_bp));

                wo_assure(!x86compiler.mov(new_sp_bp, asmjit::Imm(call_aim_native_func)));
                wo_assure(!x86compiler.mov(
                    asmjit::x86::qword_ptr(vm, offsetof(vmbase, ip)), new_sp_bp));
            }

            auto result = x86compiler.newInt32();

            asmjit::InvokeNode* invoke_node;

            wo_assure(!x86compiler.invoke(
                &invoke_node,
                reinterpret_cast<intptr_t>(call_aim_native_func),
                asmjit::FuncSignatureT<wo_result_t, vmbase*, value*>()));
            invoke_node->setArg(0, vm);
            invoke_node->setArg(1, callargptr);

            invoke_node->setRet(0, result);

            check_result_is_normal_for_nativefunc(
                x86compiler, vm, result, rt_ip, sp_offset, bp_offset);
        }

        static void x86_do_calln_vm_func(
            asmjit::x86::Compiler& x86compiler,
            asmjit::x86::Gp vm,
            const void* vm_func,
            const byte_t* codes,
            const byte_t* rt_ip,
            asmjit::x86::Gp rt_sp,
            asmjit::x86::Gp rt_bp)
        {
            // Set calltrace info here!
            wo::value callstack;
            callstack.m_type = wo::value::valuetype::callstack;
            callstack.m_vmcallstack.bp = 0;
            callstack.m_vmcallstack.ret_ip = (uint32_t)(rt_ip - codes);

            x86_set_imm(x86compiler, rt_sp, callstack);
            auto bpoffset = x86compiler.newUInt64();
            wo_assure(!x86compiler.mov(bpoffset, asmjit::x86::qword_ptr(vm, offsetof(vmbase, sb))));
            wo_assure(!x86compiler.sub(bpoffset, rt_bp));
            wo_assure(!x86compiler.shr(bpoffset, asmjit::Imm(4)));
            wo_assure(!x86compiler.mov(
                asmjit::x86::dword_ptr(
                    rt_sp,
                    offsetof(value, m_vmcallstack) + offsetof(value::callstack_t, bp)),
                bpoffset.r32()));

            auto callargptr = x86compiler.newUIntPtr();
            wo_assure(!x86compiler.lea(callargptr, asmjit::x86::qword_ptr(rt_sp, 1 * (int32_t)sizeof(wo::value))));

            auto result = x86compiler.newInt32();

            auto funcaddr = x86compiler.newIntPtr();
            x86compiler.mov(
                funcaddr,
                asmjit::x86::qword_ptr_abs(reinterpret_cast<intptr_t>(vm_func)));

            asmjit::InvokeNode* invoke_node;

            wo_assure(!x86compiler.invoke(&invoke_node, funcaddr,
                asmjit::FuncSignatureT<wo_result_t, vmbase*, value*>()));

            invoke_node->setArg(0, vm);
            invoke_node->setArg(1, callargptr);

            invoke_node->setRet(0, result);

            check_result_is_normal_for_vmfunc(x86compiler, vm, result);
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

            auto sync = ctx->c.newInt32();
            wo_assure(!ctx->c.mov(sync, asmjit::Imm(wo_result_t::WO_API_SYNC_CHANGED_VM_STATE)));
            wo_assure(!ctx->c.ret(sync));
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
            wo_assure(!ctx->c.mov(ctx->_vmshead, asmjit::x86::qword_ptr(ctx->_vmbase, offsetof(vmbase, stack_storage))));
            wo_assure(!ctx->c.mov(ctx->_vmreg, asmjit::x86::qword_ptr(ctx->_vmbase, offsetof(vmbase, register_storage))));
            wo_assure(!ctx->c.mov(ctx->_vmcr, asmjit::x86::qword_ptr(ctx->_vmbase, offsetof(vmbase, cr))));
            wo_assure(!ctx->c.mov(ctx->_vmtc, asmjit::x86::qword_ptr(ctx->_vmbase, offsetof(vmbase, tc))));
            wo_assure(!ctx->c.mov(ctx->_vmtp, asmjit::x86::qword_ptr(ctx->_vmbase, offsetof(vmbase, tp))));

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
            (void)ctx;
            (void)dr;
            (void)rt_ip;

            return true;
        }
        virtual bool ir_mov(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            write_barrier_for_opnum1_if_write_global(ctx, opnum1, dr);

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

            if (opnum2.is_constant_i32())
            {
                const auto integer_op2 = opnum2.const_value()->m_integer;

                if (integer_op2 == 0)
                    return true;
                else if (integer_op2 == 1)
                    wo_assure(!ctx->c.inc(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_integer))));
                else if (integer_op2 == -1)
                    wo_assure(!ctx->c.dec(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_integer))));
                else
                    wo_assure(!ctx->c.add(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_integer)), integer_op2));
            }
            else
            {
                auto int_of_op2 = ctx->c.newInt64();
                wo_assure(!ctx->c.mov(int_of_op2, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, m_integer))));
                wo_assure(!ctx->c.add(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_integer)), int_of_op2));
            }
            return true;
        }
        virtual bool ir_subi(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            if (opnum2.is_constant_i32())
            {
                const auto integer_op2 = opnum2.const_value()->m_integer;

                if (integer_op2 == 0)
                    return true;
                else if (integer_op2 == 1)
                    wo_assure(!ctx->c.dec(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_integer))));
                else if (integer_op2 == -1)
                    wo_assure(!ctx->c.inc(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_integer))));
                else
                    wo_assure(!ctx->c.sub(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_integer)), integer_op2));
            }
            else
            {
                auto int_of_op2 = ctx->c.newInt64();
                wo_assure(!ctx->c.mov(int_of_op2, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, m_integer))));
                wo_assure(!ctx->c.sub(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_integer)), int_of_op2));
            }
            return true;
        }
        virtual bool ir_muli(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            if (opnum2.is_constant_i32())
            {
                auto mul_value = opnum2.const_value()->m_integer;
                if (mul_value == 0)
                {
                    wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_integer)), 0));
                    return true;
                }
                else if (mul_value == 1)
                {
                    return true;
                }
                else if (mul_value == -1)
                {
                    wo_assure(!ctx->c.neg(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_integer))));
                    return true;
                }
                else if ((mul_value & (mul_value - 1)) == 0 && mul_value > 0)
                {
                    int shift_count = 0;
                    auto temp = mul_value;
                    while (temp > 1) { temp >>= 1; shift_count++; }
                    wo_assure(!ctx->c.shl(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_integer)), shift_count));
                    return true;
                }
            }

            auto int_of_op2 = ctx->c.newInt64();
            if (opnum2.is_constant_i32())
                wo_assure(!ctx->c.mov(int_of_op2, opnum2.const_value()->m_integer));
            else
                wo_assure(!ctx->c.mov(int_of_op2, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, m_integer))));

            wo_assure(!ctx->c.imul(int_of_op2, asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_integer))));
            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_integer)), int_of_op2));
            return true;
        }
        virtual bool ir_divi(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            if (opnum2.is_constant_i32())
            {
                auto divisor = opnum2.const_value()->m_integer;

                if (divisor == 1)
                {
                    return true;
                }
                else if (divisor == -1)
                {
                    wo_assure(!ctx->c.neg(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_integer))));
                    return true;
                }
                else if ((divisor & (divisor - 1)) == 0 && divisor > 0)
                {
                    int shift_count = 0;
                    auto temp = divisor;
                    while (temp > 1) { temp >>= 1; shift_count++; }

                    auto non_negative = ctx->c.newLabel();
                    auto end_label = ctx->c.newLabel();
                    auto dividend = ctx->c.newInt64();

                    wo_assure(!ctx->c.mov(dividend, asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_integer))));
                    wo_assure(!ctx->c.test(dividend, dividend));
                    wo_assure(!ctx->c.jns(non_negative));

                    wo_assure(!ctx->c.add(dividend, asmjit::Imm(divisor - 1)));
                    wo_assure(!ctx->c.sar(dividend, asmjit::Imm(shift_count)));
                    wo_assure(!ctx->c.jmp(end_label));

                    wo_assure(!ctx->c.bind(non_negative));
                    wo_assure(!ctx->c.sar(dividend, asmjit::Imm(shift_count)));

                    wo_assure(!ctx->c.bind(end_label));
                    wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_integer)), dividend));
                    return true;
                }
            }

            auto int_of_op1 = ctx->c.newInt64();
            auto int_of_op2 = ctx->c.newInt64();
            auto div_op_num = ctx->c.newInt64();

            wo_assure(!ctx->c.mov(int_of_op1, asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_integer))));
            if (opnum2.is_constant_i32())
                wo_assure(!ctx->c.mov(int_of_op2, opnum2.const_value()->m_integer));
            else
                wo_assure(!ctx->c.mov(int_of_op2, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, m_integer))));

            wo_assure(!ctx->c.xor_(div_op_num, div_op_num));
            wo_assure(!ctx->c.cqo(div_op_num, int_of_op1));
            wo_assure(!ctx->c.idiv(div_op_num, int_of_op1, int_of_op2));

            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_integer)), int_of_op1));
            return true;
        }
        virtual bool ir_modi(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto int_of_op1 = ctx->c.newInt64();
            auto int_of_op2 = ctx->c.newInt64();
            auto div_op_num = ctx->c.newInt64();

            wo_assure(!ctx->c.mov(int_of_op1, asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_integer))));
            if (opnum2.is_constant_i32())
                wo_assure(!ctx->c.mov(int_of_op2, opnum2.const_value()->m_integer));
            else
                wo_assure(!ctx->c.mov(int_of_op2, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, m_integer))));

            wo_assure(!ctx->c.mov(div_op_num, int_of_op1));
            wo_assure(!ctx->c.cqo(int_of_op1, div_op_num));
            wo_assure(!ctx->c.idiv(int_of_op1, div_op_num, int_of_op2));

            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_integer)), int_of_op1));

            return true;
        }
        virtual bool ir_addr(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            if (opnum2.is_constant_real())
            {
                const double real_op2 = opnum2.const_value()->m_real;

                if (real_op2 == 0.0)
                    return true;
            }

            auto real_of_op1 = ctx->c.newXmm();
            wo_assure(!ctx->c.movsd(real_of_op1, asmjit::x86::ptr(opnum1.gp_value(), offsetof(value, m_real))));
            wo_assure(!ctx->c.addsd(real_of_op1, asmjit::x86::ptr(opnum2.gp_value(), offsetof(value, m_real))));
            wo_assure(!ctx->c.movsd(asmjit::x86::ptr(opnum1.gp_value(), offsetof(value, m_real)), real_of_op1));

            return true;
        }
        virtual bool ir_subr(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            if (opnum2.is_constant_real())
            {
                const double real_op2 = opnum2.const_value()->m_real;

                if (real_op2 == 0.0)
                    return true;
            }

            auto real_of_op1 = ctx->c.newXmm();
            wo_assure(!ctx->c.movsd(real_of_op1, asmjit::x86::ptr(opnum1.gp_value(), offsetof(value, m_real))));
            wo_assure(!ctx->c.subsd(real_of_op1, asmjit::x86::ptr(opnum2.gp_value(), offsetof(value, m_real))));
            wo_assure(!ctx->c.movsd(asmjit::x86::ptr(opnum1.gp_value(), offsetof(value, m_real)), real_of_op1));
            return true;
        }
        virtual bool ir_mulr(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            if (opnum2.is_constant_real())
            {
                const double real_op2 = opnum2.const_value()->m_real;

                if (real_op2 == 0.0)
                {
                    auto zero = ctx->c.newXmm();
                    wo_assure(!ctx->c.xorpd(zero, zero));
                    wo_assure(!ctx->c.movsd(asmjit::x86::ptr(opnum1.gp_value(), offsetof(value, m_real)), zero));
                    return true;
                }

                else if (real_op2 == 1.0)
                    return true;

                else if (real_op2 == 2.0)
                {
                    auto real_of_op1 = ctx->c.newXmm();
                    wo_assure(!ctx->c.movsd(real_of_op1, asmjit::x86::ptr(opnum1.gp_value(), offsetof(value, m_real))));
                    wo_assure(!ctx->c.addsd(real_of_op1, real_of_op1));
                    wo_assure(!ctx->c.movsd(asmjit::x86::ptr(opnum1.gp_value(), offsetof(value, m_real)), real_of_op1));
                    return true;
                }
            }

            auto real_of_op1 = ctx->c.newXmm();
            wo_assure(!ctx->c.movsd(real_of_op1, asmjit::x86::ptr(opnum1.gp_value(), offsetof(value, m_real))));
            wo_assure(!ctx->c.mulsd(real_of_op1, asmjit::x86::ptr(opnum2.gp_value(), offsetof(value, m_real))));
            wo_assure(!ctx->c.movsd(asmjit::x86::ptr(opnum1.gp_value(), offsetof(value, m_real)), real_of_op1));
            return true;
        }
        virtual bool ir_divr(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            if (opnum2.is_constant_real())
            {
                const double real_op2 = opnum2.const_value()->m_real;

                if (real_op2 == 1.0)
                    return true;
            }

            auto real_of_op1 = ctx->c.newXmm();
            wo_assure(!ctx->c.movsd(real_of_op1, asmjit::x86::ptr(opnum1.gp_value(), offsetof(value, m_real))));
            wo_assure(!ctx->c.divsd(real_of_op1, asmjit::x86::ptr(opnum2.gp_value(), offsetof(value, m_real))));
            wo_assure(!ctx->c.movsd(asmjit::x86::ptr(opnum1.gp_value(), offsetof(value, m_real)), real_of_op1));
            return true;
        }
        virtual bool ir_modr(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto op1 = opnum1.gp_value();
            auto op2 = opnum2.gp_value();

            auto real_result = ctx->c.newXmm();
            auto real_of_op1 = ctx->c.newXmm();
            auto real_of_op2 = ctx->c.newXmm();

            wo_assure(!ctx->c.movsd(real_of_op1, asmjit::x86::ptr(op1, offsetof(value, m_real))));
            wo_assure(!ctx->c.movsd(real_of_op2, asmjit::x86::ptr(op2, offsetof(value, m_real))));

            asmjit::InvokeNode* invoke_node;
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)(double(*)(double, double)) & fmod,
                asmjit::FuncSignatureT<double, double, double>()));

            invoke_node->setArg(0, real_of_op1);
            invoke_node->setArg(1, real_of_op2);
            invoke_node->setRet(0, real_result);

            wo_assure(!ctx->c.movsd(asmjit::x86::ptr(op1, offsetof(value, m_real)), real_result));
            return true;
        }
        virtual bool ir_adds(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            write_barrier_for_opnum1_if_write_global(ctx, opnum1, dr);

            auto op1 = opnum1.gp_value();
            auto str = ctx->c.newIntPtr();

            if (!opnum2.is_constant())
                wo_assure(!ctx->c.mov(
                    str, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, m_string))));

            asmjit::InvokeNode* invoke_node;
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&_vmjitcall_adds,
                asmjit::FuncSignatureT<void, wo::value*, wo::string_t*>()));

            invoke_node->setArg(0, op1);

            if (opnum2.is_constant())
                invoke_node->setArg(1, asmjit::Imm(opnum2.const_value()->m_string));
            else
                invoke_node->setArg(1, str);

            return true;
        }
        virtual bool ir_psh(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            const byte_t* rollback_ip = rt_ip - 1;

            if (dr & 0b01)
            {
                auto stack_overflow_label = ctx->c.newLabel();
                auto finish_psh_label = ctx->c.newLabel();

                // WO_ADDRESSING_N1_REF;
                // (rt_sp--)->set_val(opnum1);
                wo_assure(!ctx->c.cmp(ctx->_vmssp, ctx->_vmshead));
                wo_assure(!ctx->c.jbe(stack_overflow_label));

                // Ok, stack is enough.
                WO_JIT_ADDRESSING_N1;
                if (opnum1.is_constant_and_not_tag(ctx))
                    x86_set_imm(ctx->c, ctx->_vmssp, *opnum1.const_value());
                else
                    x86_set_val(ctx->c, ctx->_vmssp, opnum1.gp_value());
                wo_assure(!ctx->c.sub(ctx->_vmssp, sizeof(wo::value)));

                wo_assure(!ctx->c.jmp(finish_psh_label));
                wo_assure(!ctx->c.bind(stack_overflow_label));

                ir_make_interrupt(ctx, wo::vmbase::vm_interrupt_type::STACK_OVERFLOW_INTERRUPT);
                ir_make_fast_vm_resync(ctx, rollback_ip);

                wo_assure(!ctx->c.int3()); // Cannot be here.
                wo_assure(!ctx->c.bind(finish_psh_label));
            }
            else
            {
                auto stackenough_label = ctx->c.newLabel();

                uint16_t psh_repeat = WO_IPVAL_MOVE_2;
                if (psh_repeat > 0)
                {
                    wo_assure(!ctx->c.sub(ctx->_vmssp, asmjit::Imm(psh_repeat * sizeof(wo::value))));

                    // NOTE: If there is just enough stack space here, the Stackoverflow interrupt 
                    // will still be triggered, which is planned (purely for performance reasons)
                    // 
                    // SEE: wo_vm.cpp: impl for pshn.
                    wo_assure(!ctx->c.cmp(ctx->_vmssp, ctx->_vmshead));
                    wo_assure(!ctx->c.ja(stackenough_label));

                    wo_assure(!ctx->c.add(ctx->_vmssp, asmjit::Imm(psh_repeat * sizeof(wo::value))));
                    ir_make_interrupt(ctx, wo::vmbase::vm_interrupt_type::STACK_OVERFLOW_INTERRUPT);
                    ir_make_fast_vm_resync(ctx, rollback_ip);

                    wo_assure(!ctx->c.int3()); // Cannot be here.
                    wo_assure(!ctx->c.bind(stackenough_label));
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

                write_barrier_for_opnum1_if_write_global(ctx, opnum1, dr);

                wo_assure(!ctx->c.add(ctx->_vmssp, sizeof(wo::value)));
                x86_set_val(ctx->c, opnum1.gp_value(), ctx->_vmssp);
            }
            else
                wo_assure(!ctx->c.add(ctx->_vmssp, WO_IPVAL_MOVE_2 * sizeof(wo::value)));
            return true;
        }
        virtual bool ir_sidarr(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;
            WO_JIT_ADDRESSING_N3_REG_BPOFF;

            auto arr = ctx->c.newIntPtr();
            auto idx = ctx->c.newInt64();

            wo_assure(!ctx->c.mov(
                arr, asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_array))));

            if (!opnum2.is_constant())
                wo_assure(!ctx->c.mov(
                    idx, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, m_integer))));

            auto op3 = opnum3.gp_value();
            auto err = ctx->c.newIntPtr();

            asmjit::InvokeNode* invoke_node;
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&_vmjitcall_sidarr,
                asmjit::FuncSignatureT<const char*, wo::array_t*, wo_integer_t, wo::value*>()));

            invoke_node->setArg(0, arr);

            if (opnum2.is_constant())
                invoke_node->setArg(1, opnum2.const_value()->m_integer);
            else
                invoke_node->setArg(1, idx);

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

            auto stc = ctx->c.newIntPtr();
            auto op2 = opnum2.gp_value();

            wo_assure(!ctx->c.mov(
                stc, asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_structure))));

            asmjit::InvokeNode* invoke_node;
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&_vmjitcall_sidstruct,
                asmjit::FuncSignatureT<void, wo::structure_t*, wo::value*, uint16_t>()));

            invoke_node->setArg(0, stc);
            invoke_node->setArg(1, op2);
            invoke_node->setArg(2, asmjit::Imm(offset));

            return true;
        }
        virtual bool ir_lds(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            write_barrier_for_opnum1_if_write_global(ctx, opnum1, dr);

            auto bpoffset = ctx->c.newUIntPtr();
            if (opnum2.is_constant())
            {
                wo_assure(!ctx->c.lea(
                    bpoffset,
                    asmjit::x86::qword_ptr(
                        ctx->_vmsbp,
                        (int32_t)(opnum2.const_value()->m_integer * sizeof(wo::value)))));
                x86_set_val(ctx->c, opnum1.gp_value(), bpoffset);
            }
            else
            {
                static_assert(sizeof(wo::value) == 16);

                wo_assure(!ctx->c.mov(bpoffset, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, m_integer))));
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
                wo_assure(!ctx->c.lea(bpoffset, asmjit::x86::qword_ptr(
                    ctx->_vmsbp,
                    (int32_t)(opnum2.const_value()->m_integer * sizeof(wo::value)))));
                x86_set_val(ctx->c, bpoffset, opnum1.gp_value());
            }
            else
            {
                static_assert(sizeof(wo::value) == 16);

                wo_assure(!ctx->c.mov(
                    bpoffset,
                    asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, m_integer))));
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

            auto result_reg = ctx->c.newInt64();
            wo_assure(!ctx->c.xor_(result_reg, result_reg));

            auto int_of_op1 = ctx->c.newInt64();
            wo_assure(!ctx->c.mov(int_of_op1, asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_integer))));
            wo_assure(!ctx->c.cmp(int_of_op1, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, m_integer))));
            wo_assure(!ctx->c.sete(result_reg.r8()));
            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, m_integer)), result_reg));
            wo_assure(!ctx->c.mov(asmjit::x86::byte_ptr(ctx->_vmcr, offsetof(value, m_type)), (uint8_t)value::valuetype::bool_type));

            return true;
        }
        virtual bool ir_nequb(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto result_reg = ctx->c.newInt64();
            wo_assure(!ctx->c.xor_(result_reg, result_reg));

            auto int_of_op1 = ctx->c.newInt64();
            wo_assure(!ctx->c.mov(int_of_op1, asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_integer))));
            wo_assure(!ctx->c.cmp(int_of_op1, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, m_integer))));
            wo_assure(!ctx->c.setne(result_reg.r8()));
            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, m_integer)), result_reg));
            wo_assure(!ctx->c.mov(asmjit::x86::byte_ptr(ctx->_vmcr, offsetof(value, m_type)), (uint8_t)value::valuetype::bool_type));

            return true;
        }
        virtual bool ir_lti(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto result_reg = ctx->c.newInt64();
            wo_assure(!ctx->c.xor_(result_reg, result_reg));

            if (opnum1.is_constant_i32())
            {
                wo_assure(!ctx->c.cmp(asmjit::x86::qword_ptr(
                    opnum2.gp_value(), offsetof(value, m_integer)), opnum1.const_value()->m_integer));
                wo_assure(!ctx->c.setg(result_reg.r8())); // op2 > const => const < op2
            }
            else if (opnum2.is_constant_i32())
            {
                wo_assure(!ctx->c.cmp(asmjit::x86::qword_ptr(
                    opnum1.gp_value(), offsetof(value, m_integer)), opnum2.const_value()->m_integer));
                wo_assure(!ctx->c.setl(result_reg.r8()));
            }
            else
            {
                auto int_of_op1 = ctx->c.newInt64();

                wo_assure(!ctx->c.mov(
                    int_of_op1,
                    asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_integer))));
                wo_assure(!ctx->c.cmp(
                    int_of_op1,
                    asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, m_integer))));
                wo_assure(!ctx->c.setl(result_reg.r8()));
            }

            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, m_integer)), result_reg));
            wo_assure(!ctx->c.mov(asmjit::x86::byte_ptr(ctx->_vmcr, offsetof(value, m_type)), (uint8_t)value::valuetype::bool_type));
            return true;
        }
        virtual bool ir_gti(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto result_reg = ctx->c.newInt64();
            wo_assure(!ctx->c.xor_(result_reg, result_reg));

            if (opnum1.is_constant_i32())
            {
                wo_assure(!ctx->c.cmp(
                    asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, m_integer)),
                    opnum1.const_value()->m_integer));
                wo_assure(!ctx->c.setl(result_reg.r8())); // op2 < const => const > op2
            }
            else if (opnum2.is_constant_i32())
            {
                wo_assure(!ctx->c.cmp(
                    asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_integer)),
                    opnum2.const_value()->m_integer));
                wo_assure(!ctx->c.setg(result_reg.r8()));
            }
            else
            {
                auto int_of_op1 = ctx->c.newInt64();

                wo_assure(!ctx->c.mov(
                    int_of_op1,
                    asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_integer))));
                wo_assure(!ctx->c.cmp(
                    int_of_op1,
                    asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, m_integer))));
                wo_assure(!ctx->c.setg(result_reg.r8()));
            }

            wo_assure(!ctx->c.mov(
                asmjit::x86::qword_ptr(
                    ctx->_vmcr, 
                    offsetof(value, m_integer)), 
                result_reg));
            wo_assure(!ctx->c.mov(
                asmjit::x86::byte_ptr(ctx->_vmcr, offsetof(value, m_type)),
                (uint8_t)value::valuetype::bool_type));

            return true;
        }
        virtual bool ir_elti(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto result_reg = ctx->c.newInt64();
            wo_assure(!ctx->c.xor_(result_reg, result_reg));

            if (opnum1.is_constant_i32())
            {
                wo_assure(!ctx->c.cmp(asmjit::x86::qword_ptr(
                    opnum2.gp_value(), offsetof(value, m_integer)), opnum1.const_value()->m_integer));
                wo_assure(!ctx->c.setge(result_reg.r8())); // op2 >= const => const <= op2
            }
            else if (opnum2.is_constant_i32())
            {
                wo_assure(!ctx->c.cmp(asmjit::x86::qword_ptr(
                    opnum1.gp_value(), offsetof(value, m_integer)), opnum2.const_value()->m_integer));
                wo_assure(!ctx->c.setle(result_reg.r8()));
            }
            else
            {
                auto int_of_op1 = ctx->c.newInt64();

                wo_assure(!ctx->c.mov(
                    int_of_op1,
                    asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_integer))));
                wo_assure(!ctx->c.cmp(
                    int_of_op1,
                    asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, m_integer))));
                wo_assure(!ctx->c.setle(result_reg.r8()));
            }

            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, m_integer)), result_reg));
            wo_assure(!ctx->c.mov(asmjit::x86::byte_ptr(ctx->_vmcr, offsetof(value, m_type)), (uint8_t)value::valuetype::bool_type));
            return true;
        }
        virtual bool ir_egti(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override

        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto result_reg = ctx->c.newInt64();
            wo_assure(!ctx->c.xor_(result_reg, result_reg));

            if (opnum1.is_constant_i32())
            {
                wo_assure(!ctx->c.cmp(
                    asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, m_integer)),
                    opnum1.const_value()->m_integer));
                wo_assure(!ctx->c.setle(result_reg.r8())); // op2 <= const => const >= op2
            }
            else if (opnum2.is_constant_i32())
            {
                wo_assure(!ctx->c.cmp(
                    asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_integer)),
                    opnum2.const_value()->m_integer));
                wo_assure(!ctx->c.setge(result_reg.r8()));
            }
            else
            {
                auto int_of_op1 = ctx->c.newInt64();

                wo_assure(!ctx->c.mov(
                    int_of_op1,
                    asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_integer))));
                wo_assure(!ctx->c.cmp(
                    int_of_op1,
                    asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, m_integer))));
                wo_assure(!ctx->c.setge(result_reg.r8()));
            }

            wo_assure(!ctx->c.mov(
                asmjit::x86::qword_ptr(
                    ctx->_vmcr, 
                    offsetof(value, m_integer)),
                result_reg));
            wo_assure(!ctx->c.mov(
                asmjit::x86::byte_ptr(ctx->_vmcr, offsetof(value, m_type)),
                (uint8_t)value::valuetype::bool_type));

            return true;
        }
        virtual bool ir_land(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            if (opnum1.is_constant() && opnum2.is_constant())
            {
                if (opnum1.const_value()->m_integer && opnum2.const_value()->m_integer)
                    wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, m_integer)), 1));
                else
                    wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, m_integer)), 0));
            }
            else if (opnum1.is_constant())
            {
                if (opnum1.const_value()->m_integer)
                {
                    auto opnum2_val = ctx->c.newInt64();
                    wo_assure(!ctx->c.mov(opnum2_val, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, m_integer))));
                    wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, m_integer)), opnum2_val));
                }
                else
                    wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, m_integer)), 0));
            }
            else if (opnum2.is_constant())
            {
                if (opnum2.const_value()->m_integer)
                {
                    auto opnum1_val = ctx->c.newInt64();
                    wo_assure(!ctx->c.mov(opnum1_val, asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_integer))));
                    wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, m_integer)), opnum1_val));
                }
                else
                    wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, m_integer)), 0));
            }
            else
            {
                auto set_cr_false = ctx->c.newLabel();
                auto land_end = ctx->c.newLabel();

                wo_assure(!ctx->c.cmp(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_integer)), 0));
                wo_assure(!ctx->c.je(set_cr_false));
                wo_assure(!ctx->c.cmp(asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, m_integer)), 0));
                wo_assure(!ctx->c.je(set_cr_false));
                wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, m_integer)), 1));
                wo_assure(!ctx->c.jmp(land_end));
                wo_assure(!ctx->c.bind(set_cr_false));
                wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, m_integer)), 0));
                wo_assure(!ctx->c.bind(land_end));
            }
            ctx->c.mov(asmjit::x86::byte_ptr(ctx->_vmcr, offsetof(value, m_type)), (uint8_t)value::valuetype::bool_type);
            return true;
        }
        virtual bool ir_lor(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            if (opnum1.is_constant() && opnum2.is_constant())
            {
                if (opnum1.const_value()->m_integer || opnum2.const_value()->m_integer)
                    wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, m_integer)), 1));
                else
                    wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, m_integer)), 0));
            }
            else if (opnum1.is_constant())
            {
                if (!opnum1.const_value()->m_integer)
                {
                    auto opnum2_val = ctx->c.newInt64();
                    wo_assure(!ctx->c.mov(opnum2_val, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, m_integer))));
                    wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, m_integer)), opnum2_val));
                }
                else
                    wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, m_integer)), 1));
            }
            else if (opnum2.is_constant())
            {
                if (!opnum2.const_value()->m_integer)
                {
                    auto opnum1_val = ctx->c.newInt64();
                    wo_assure(!ctx->c.mov(opnum1_val, asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_integer))));
                    wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, m_integer)), opnum1_val));
                }
                else
                    wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, m_integer)), 1));
            }
            else
            {
                auto set_cr_true = ctx->c.newLabel();
                auto land_end = ctx->c.newLabel();

                wo_assure(!ctx->c.cmp(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_integer)), 0));
                wo_assure(!ctx->c.jne(set_cr_true));
                wo_assure(!ctx->c.cmp(asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, m_integer)), 0));
                wo_assure(!ctx->c.jne(set_cr_true));
                wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, m_integer)), 0));
                wo_assure(!ctx->c.jmp(land_end));
                wo_assure(!ctx->c.bind(set_cr_true));
                wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, m_integer)), 1));
                wo_assure(!ctx->c.bind(land_end));
            }
            ctx->c.mov(asmjit::x86::byte_ptr(ctx->_vmcr, offsetof(value, m_type)), (uint8_t)value::valuetype::bool_type);
            return true;
        }
        virtual bool ir_sidmap(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;
            WO_JIT_ADDRESSING_N3_REG_BPOFF;

            auto op2 = opnum2.gp_value();
            auto op3 = opnum3.gp_value();

            auto dict = ctx->c.newIntPtr();
            auto err = ctx->c.newIntPtr();

            wo_assure(!ctx->c.mov(
                dict, asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_dictionary))));

            asmjit::InvokeNode* invoke_node;
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&_vmjitcall_sidmap,
                asmjit::FuncSignatureT< void, wo::dictionary_t*, wo::value*, wo::value*>()));

            invoke_node->setArg(0, dict);
            invoke_node->setArg(1, op2);
            invoke_node->setArg(2, op3);

            return true;
        }
        virtual bool ir_negi(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto int_of_op2 = ctx->c.newInt64();
            wo_assure(!ctx->c.mov(
                int_of_op2, 
                asmjit::x86::qword_ptr(
                    opnum2.gp_value(), 
                    offsetof(value, m_integer))));
            wo_assure(!ctx->c.neg(int_of_op2));
            wo_assure(!ctx->c.mov(
                asmjit::x86::qword_ptr(
                    opnum1.gp_value(), 
                    offsetof(value, m_integer)), 
                int_of_op2));
            return true;
        }
        virtual bool ir_negr(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto zero = ctx->c.newXmm();
            wo_assure(!ctx->c.xorpd(zero, zero));

            auto real_of_op2 = ctx->c.newXmm();
            wo_assure(!ctx->c.movsd(real_of_op2, asmjit::x86::ptr(opnum2.gp_value(), offsetof(value, m_real))));
            wo_assure(!ctx->c.subsd(zero, real_of_op2));

            wo_assure(!ctx->c.movsd(asmjit::x86::ptr(opnum1.gp_value(), offsetof(value, m_real)), zero));
            return true;
        }
        virtual bool ir_lts(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto op1 = opnum1.gp_value();
            auto op2 = opnum2.gp_value();

            asmjit::InvokeNode* invoke_node;
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&vmbase::ltx_impl,
                asmjit::FuncSignatureT< void, wo::value*, wo::value*, wo::value*>()));

            invoke_node->setArg(0, ctx->_vmcr);
            invoke_node->setArg(1, op1);
            invoke_node->setArg(2, op2);

            return true;
        }
        virtual bool ir_gts(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto op1 = opnum1.gp_value();
            auto op2 = opnum2.gp_value();

            asmjit::InvokeNode* invoke_node;
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&vmbase::gtx_impl,
                asmjit::FuncSignatureT< void, wo::value*, wo::value*, wo::value*>()));

            invoke_node->setArg(0, ctx->_vmcr);
            invoke_node->setArg(1, op1);
            invoke_node->setArg(2, op2);

            return true;
        }
        virtual bool ir_elts(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto op1 = opnum1.gp_value();
            auto op2 = opnum2.gp_value();

            asmjit::InvokeNode* invoke_node;
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&vmbase::eltx_impl,
                asmjit::FuncSignatureT< void, wo::value*, wo::value*, wo::value*>()));

            invoke_node->setArg(0, ctx->_vmcr);
            invoke_node->setArg(1, op1);
            invoke_node->setArg(2, op2);

            return true;
        }
        virtual bool ir_egts(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto op1 = opnum1.gp_value();
            auto op2 = opnum2.gp_value();

            asmjit::InvokeNode* invoke_node;
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&vmbase::egtx_impl,
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

            auto result_reg = ctx->c.newInt64();
            wo_assure(!ctx->c.xor_(result_reg, result_reg));

            auto real_of_op1 = ctx->c.newXmm();
            wo_assure(!ctx->c.movsd(real_of_op1, asmjit::x86::ptr(opnum1.gp_value(), offsetof(value, m_real))));
            wo_assure(!ctx->c.comisd(real_of_op1, asmjit::x86::ptr(opnum2.gp_value(), offsetof(value, m_real))));
            wo_assure(!ctx->c.setb(result_reg.r8()));
            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, m_integer)), result_reg));
            wo_assure(!ctx->c.mov(asmjit::x86::byte_ptr(ctx->_vmcr, offsetof(value, m_type)), (uint8_t)value::valuetype::bool_type));

            return true;
        }
        virtual bool ir_gtr(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto result_reg = ctx->c.newInt64();
            wo_assure(!ctx->c.xor_(result_reg, result_reg));

            auto real_of_op1 = ctx->c.newXmm();
            wo_assure(!ctx->c.movsd(real_of_op1, asmjit::x86::ptr(opnum1.gp_value(), offsetof(value, m_real))));
            wo_assure(!ctx->c.comisd(real_of_op1, asmjit::x86::ptr(opnum2.gp_value(), offsetof(value, m_real))));
            wo_assure(!ctx->c.seta(result_reg.r8()));
            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, m_integer)), result_reg));
            wo_assure(!ctx->c.mov(asmjit::x86::byte_ptr(ctx->_vmcr, offsetof(value, m_type)), (uint8_t)value::valuetype::bool_type));

            return true;
        }
        virtual bool ir_eltr(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto result_reg = ctx->c.newInt64();
            wo_assure(!ctx->c.xor_(result_reg, result_reg));

            auto real_of_op1 = ctx->c.newXmm();
            wo_assure(!ctx->c.movsd(real_of_op1, asmjit::x86::ptr(opnum1.gp_value(), offsetof(value, m_real))));
            wo_assure(!ctx->c.comisd(real_of_op1, asmjit::x86::ptr(opnum2.gp_value(), offsetof(value, m_real))));
            wo_assure(!ctx->c.setbe(result_reg.r8()));
            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, m_integer)), result_reg));
            wo_assure(!ctx->c.mov(asmjit::x86::byte_ptr(ctx->_vmcr, offsetof(value, m_type)), (uint8_t)value::valuetype::bool_type));

            return true;
        }
        virtual bool ir_egtr(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto result_reg = ctx->c.newInt64();
            wo_assure(!ctx->c.xor_(result_reg, result_reg));

            auto real_of_op1 = ctx->c.newXmm();
            wo_assure(!ctx->c.movsd(real_of_op1, asmjit::x86::ptr(opnum1.gp_value(), offsetof(value, m_real))));
            wo_assure(!ctx->c.comisd(real_of_op1, asmjit::x86::ptr(opnum2.gp_value(), offsetof(value, m_real))));
            wo_assure(!ctx->c.setae(result_reg.r8()));
            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, m_integer)), result_reg));
            wo_assure(!ctx->c.mov(asmjit::x86::byte_ptr(ctx->_vmcr, offsetof(value, m_type)), (uint8_t)value::valuetype::bool_type));

            return true;
        }

        virtual bool ir_movicas(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;
            WO_JIT_ADDRESSING_N3_REG_BPOFF;

            auto expected = ctx->c.newInt64();
            auto desired = ctx->c.newInt64();
            auto op1_ptr = opnum1.gp_value();
            auto op2_val = opnum2.gp_value();
            auto op3_val = opnum3.gp_value();

            // Load expected value (opnum3)
            wo_assure(!ctx->c.mov(expected, asmjit::x86::qword_ptr(op3_val, offsetof(value, m_integer))));

            // Load desired value (opnum2)
            if (opnum2.is_constant())
                wo_assure(!ctx->c.mov(desired, opnum2.const_value()->m_integer));
            else
                wo_assure(!ctx->c.mov(desired, asmjit::x86::qword_ptr(op2_val, offsetof(value, m_integer))));

            // Perform atomic compare and exchange
            auto result = ctx->c.newInt64();
            wo_assure(!ctx->c.mov(result, expected));
            wo_assure(!ctx->c.lock().cmpxchg(
                asmjit::x86::qword_ptr(op1_ptr, offsetof(value, m_integer)),
                desired,
                result));

            // Check if exchange was successful
            auto success_label = ctx->c.newLabel();
            wo_assure(!ctx->c.cmp(result, expected));
            wo_assure(!ctx->c.je(success_label));

            // Exchange failed - update opnum3 with current value
            wo_assure(!ctx->c.mov(
                asmjit::x86::qword_ptr(op3_val, offsetof(value, m_integer)), result));

            wo_assure(!ctx->c.bind(success_label));

            // Set CR to indicate success (1) or failure (0)
            auto cr_val = ctx->c.newInt64();
            wo_assure(!ctx->c.sete(cr_val.r8()));
            wo_assure(!ctx->c.movzx(cr_val, cr_val.r8()));
            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, m_integer)), cr_val));
            wo_assure(!ctx->c.mov(asmjit::x86::byte_ptr(ctx->_vmcr, offsetof(value, m_type)), (uint8_t)value::valuetype::bool_type));

            return true;
        }

        /*
        native_do_call_vmfunc_with_stack_overflow_check
            native_do_calln_vmfunc
        x86_do_calln_native_func_fast
            native_do_calln_vmfunc
        x86_do_calln_native_func
        x86_do_calln_vm_func
            .... vm jit func ....
        */
        virtual bool ir_call(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            auto* rollback_ip = rt_ip - 1;
            WO_JIT_ADDRESSING_N1;

            auto op1 = opnum1.gp_value();

            auto result = ctx->c.newInt32();

            asmjit::InvokeNode* invoke_node;
            wo_assure(!ctx->c.invoke(
                &invoke_node,
                (intptr_t)&native_do_call_vmfunc_with_stack_overflow_check,
                asmjit::FuncSignatureT<wo_result_t, vmbase*, value*, const wo::byte_t*, const wo::byte_t*, value*, value*>()));

            invoke_node->setArg(0, ctx->_vmbase);
            invoke_node->setArg(1, op1);
            invoke_node->setArg(2, asmjit::Imm(reinterpret_cast<intptr_t>(rollback_ip)));
            invoke_node->setArg(3, asmjit::Imm(reinterpret_cast<intptr_t>(rt_ip)));
            invoke_node->setArg(4, ctx->_vmssp);
            invoke_node->setArg(5, ctx->_vmsbp);
            invoke_node->setRet(0, result);

            check_result_is_normal_for_vmfunc(ctx->c, ctx->_vmbase, result);
            return true;
        }

        virtual bool ir_calln(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            const byte_t* rollback_ip = rt_ip - 1;
            auto stackenough_label = ctx->c.newLabel();

            wo_assure(!ctx->c.cmp(ctx->_vmssp, ctx->_vmshead));
            wo_assure(!ctx->c.ja(stackenough_label));

            ir_make_interrupt(ctx, wo::vmbase::vm_interrupt_type::STACK_OVERFLOW_INTERRUPT);
            ir_make_fast_vm_resync(ctx, rollback_ip);

            wo_assure(!ctx->c.int3()); // Cannot be here.
            wo_assure(!ctx->c.bind(stackenough_label));

            if (dr)
            {
                // Call native
                jit_packed_func_t call_aim_native_func = (jit_packed_func_t)(WO_IPVAL_MOVE_8);
                x86_do_calln_native_func(
                    ctx->c,
                    ctx->_vmbase,
                    call_aim_native_func,
                    ctx->env->rt_codes,
                    rt_ip,
                    ctx->_vmssp,
                    ctx->_vmsbp,
                    0 != (dr & 0b10));
            }
            else
            {
                const auto* ip_stores_function_offset_unalligned = rt_ip;
                rt_ip += 8; // skip empty space;

                x86_do_calln_vm_func(
                    ctx->c, ctx->_vmbase,
                    ip_stores_function_offset_unalligned,
                    ctx->env->rt_codes,
                    rt_ip, ctx->_vmssp, ctx->_vmsbp);
            }
            return true;
        }
        virtual bool ir_setip(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            uint32_t jmp_place = WO_IPVAL_MOVE_4;

            switch (dr)
            {
            case 0b00:
                wo_assure(!ctx->c.jmp(jump_ip(&ctx->c, jmp_place)));
                break;
            case 0b10:
                wo_assure(!ctx->c.cmp(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, m_value_field)), 0));
                wo_assure(!ctx->c.je(jump_ip(&ctx->c, jmp_place)));
                break;
            case 0b11:
                wo_assure(!ctx->c.cmp(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, m_value_field)), 0));
                wo_assure(!ctx->c.jne(jump_ip(&ctx->c, jmp_place)));
                break;
            default:
                wo_error("Unknown jmp kind.");
            }

            return true;
        }
        virtual bool ir_setipgc(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            auto check_point_ipaddr = rt_ip - 1;
            uint32_t jmp_place = WO_IPVAL_MOVE_4;

            switch (dr)
            {
            case 0b00:
                ir_make_checkpoint_fastcheck(ctx, check_point_ipaddr);
                wo_assure(!ctx->c.jmp(jump_ip(&ctx->c, jmp_place)));
                break;
            case 0b10:
            {
                wo_assure(!ctx->c.cmp(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, m_value_field)), 0));
                auto label_cond_not_match = ctx->c.newLabel();

                wo_assure(!ctx->c.jne(label_cond_not_match));
                ir_make_checkpoint_fastcheck(ctx, check_point_ipaddr);
                wo_assure(!ctx->c.jmp(jump_ip(&ctx->c, jmp_place)));
                wo_assure(!ctx->c.bind(label_cond_not_match));
                break;
            }
            case 0b11:
            {
                wo_assure(!ctx->c.cmp(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, m_value_field)), 0));
                auto label_cond_not_match = ctx->c.newLabel();

                wo_assure(!ctx->c.je(label_cond_not_match));
                ir_make_checkpoint_fastcheck(ctx, check_point_ipaddr);
                wo_assure(!ctx->c.jmp(jump_ip(&ctx->c, jmp_place)));
                wo_assure(!ctx->c.bind(label_cond_not_match));

                break;
            }
            default:
                wo_error("Unknown jmp kind.");
            }

            return true;
        }
        virtual bool ir_mkunion(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;
            uint16_t id = WO_IPVAL_MOVE_2;

            write_barrier_for_opnum1_if_write_global(ctx, opnum1, dr);

            auto op1 = opnum1.gp_value();
            auto op2 = opnum2.gp_value();

            asmjit::InvokeNode* invoke_node;
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&vmbase::make_union_impl,
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

            write_barrier_for_opnum1_if_write_global(ctx, opnum1, dr);

            auto op1 = opnum1.gp_value();
            auto op2 = opnum2.gp_value();
            auto err = ctx->c.newIntPtr();

            asmjit::InvokeNode* invoke_node;
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&vmbase::movcast_impl,
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
        virtual bool ir_movicastr(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            write_barrier_for_opnum1_if_write_global(ctx, opnum1, dr);

            // Cast opnum2(integer) to real
            auto cast_result = ctx->c.newXmm();

            wo_assure(!ctx->c.cvtsi2sd(
                cast_result,
                asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, m_integer))));
            wo_assure(!ctx->c.movsd(
                asmjit::x86::ptr(opnum1.gp_value(), offsetof(value, m_real)),
                cast_result));

            // Set type
            wo_assure(!ctx->c.mov(
                asmjit::x86::byte_ptr(opnum1.gp_value(), offsetof(value, m_type)),
                (uint8_t)value::valuetype::real_type));

            return true;
        }
        virtual bool ir_movrcasti(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            write_barrier_for_opnum1_if_write_global(ctx, opnum1, dr);

            // Cast opnum2(real) to integer
            auto cast_result = ctx->c.newInt64();
            wo_assure(!ctx->c.cvtsd2si(
                cast_result,
                asmjit::x86::ptr(opnum2.gp_value(), offsetof(value, m_real))));
            wo_assure(!ctx->c.mov(
                asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_integer)),
                cast_result));

            // Set type
            wo_assure(!ctx->c.mov(
                asmjit::x86::byte_ptr(opnum1.gp_value(), offsetof(value, m_type)),
                (uint8_t)value::valuetype::integer_type));

            return true;
        }
        virtual bool ir_mkclos(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            const uint16_t closure_arg_count = WO_IPVAL_MOVE_2;

            // ATTENTION: Always make_closure_fp_impl in jit.
            if (dr != 0)
            {
                // is mkclosfp
                const uint64_t function_pointer = WO_IPVAL_MOVE_8;

                asmjit::InvokeNode* invoke_node;

                // NOTE: in x64, use make_closure_fast_impl.
                wo_assure(!ctx->c.invoke(
                    &invoke_node,
                    (intptr_t)&vmbase::make_closure_fp_impl,
                    asmjit::FuncSignatureT<value*, value*, uint16_t, uint64_t, value*>()));

                invoke_node->setArg(0, ctx->_vmcr);
                invoke_node->setArg(1, asmjit::Imm(closure_arg_count));
                invoke_node->setArg(2, asmjit::Imm(function_pointer));
                invoke_node->setArg(3, ctx->_vmssp);

                invoke_node->setRet(0, ctx->_vmssp);
            }
            else
            {
                // is mkclosfp
                const auto* ip_stores_function_offset_unalligned = rt_ip;
                rt_ip += 8;

                auto absfptr = ctx->c.newIntPtr();
                ctx->c.mov(absfptr, asmjit::x86::qword_ptr_abs(
                    reinterpret_cast<intptr_t>(ip_stores_function_offset_unalligned)));

                asmjit::InvokeNode* invoke_node;

                // NOTE: in x64, use make_closure_fast_impl.
                wo_assure(!ctx->c.invoke(
                    &invoke_node,
                    reinterpret_cast<intptr_t>(&vmbase::make_closure_fp_impl),
                    asmjit::FuncSignatureT<value*, value*, uint16_t, uint64_t, value*>()));

                invoke_node->setArg(0, ctx->_vmcr);
                invoke_node->setArg(1, asmjit::Imm(closure_arg_count));
                invoke_node->setArg(2, absfptr);
                invoke_node->setArg(3, ctx->_vmssp);

                invoke_node->setRet(0, ctx->_vmssp);
            }
            return true;
        }
        virtual bool ir_typeas(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            value::valuetype type = (value::valuetype)(WO_IPVAL_MOVE_1);

            auto typeas_failed = ctx->c.newLabel();
            auto typeas_end = ctx->c.newLabel();
            wo_assure(!ctx->c.cmp(
                asmjit::x86::byte_ptr(opnum1.gp_value(), offsetof(value, m_type)),
                (uint8_t)type));
            wo_assure(!ctx->c.jne(typeas_failed));
            if (dr & 0b01)
            {
                wo_assure(!ctx->c.mov(
                    asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, m_integer)),
                    1));
            }
            wo_assure(!ctx->c.jmp(typeas_end));
            wo_assure(!ctx->c.bind(typeas_failed));
            if (dr & 0b01)
            {
                wo_assure(!ctx->c.mov(
                    asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, m_integer)),
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
                    asmjit::x86::byte_ptr(ctx->_vmcr, offsetof(value, m_type)),
                    (uint8_t)value::valuetype::bool_type));
            }
            return true;
        }
        virtual bool ir_mkstruct(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            uint16_t size = WO_IPVAL_MOVE_2;

            write_barrier_for_opnum1_if_write_global(ctx, opnum1, dr);

            auto op1 = opnum1.gp_value();

            asmjit::InvokeNode* invoke_node;
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&vmbase::make_struct_impl,
                asmjit::FuncSignatureT< wo::value*, wo::value*, uint16_t, wo::value*>()));

            invoke_node->setArg(0, op1);
            invoke_node->setArg(1, asmjit::Imm(size));
            invoke_node->setArg(2, ctx->_vmssp);
            invoke_node->setRet(0, ctx->_vmssp);
            return true;
        }
        virtual bool ir_endproc(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            switch (dr)
            {
            case 0b00:
            {
                asmjit::InvokeNode* invoke_node;
                wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&_vmjitcall_abrt,
                    asmjit::FuncSignatureT< void, const char*>()));
                invoke_node->setArg(0, (intptr_t)"executed 'abrt'.");
                break;
            }
            case 0b10:
                WO_JIT_NOT_SUPPORT;
                break;
            case 0b11:
                (void)WO_IPVAL_MOVE_2;
                /* fall through */
                [[fallthrough]];
            case 0b01:
            {
                auto stat = ctx->c.newInt32();
                static_assert(sizeof(wo_result_t) == sizeof(int32_t));
                wo_assure(!ctx->c.dec(asmjit::x86::byte_ptr(ctx->_vmbase, offsetof(vmbase, extern_state_jit_call_depth))));
                wo_assure(!ctx->c.mov(stat, asmjit::Imm(wo_result_t::WO_API_NORMAL)));
                wo_assure(!ctx->c.ret(stat));
                break;
            }
            default:
                wo_error("Unknown abrt kind.");
            }
            return true;
        }
        virtual bool ir_idarr(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto arr = ctx->c.newIntPtr();
            auto idx = ctx->c.newInt64();
            auto err = ctx->c.newIntPtr();

            wo_assure(!ctx->c.mov(
                arr, asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_array))));

            if (!opnum2.is_constant())
                wo_assure(!ctx->c.mov(
                    idx, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, m_integer))));

            asmjit::InvokeNode* invoke_node;
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&_vmjitcall_idarr,
                asmjit::FuncSignatureT<const char*, wo::value*, wo::array_t*, wo_integer_t>()));

            invoke_node->setArg(0, ctx->_vmcr);
            invoke_node->setArg(1, arr);
            if (opnum2.is_constant())
                invoke_node->setArg(2, asmjit::Imm(opnum2.const_value()->m_integer));
            else
                invoke_node->setArg(2, idx);

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

            auto dict = ctx->c.newIntPtr();
            auto op2 = opnum2.gp_value();
            auto err = ctx->c.newIntPtr();

            wo_assure(!ctx->c.mov(
                dict, asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_dictionary))));

            asmjit::InvokeNode* invoke_node;
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&_vmjitcall_iddict,
                asmjit::FuncSignatureT<const char*, wo::value*, wo::value*, wo::value*>()));

            invoke_node->setArg(0, ctx->_vmcr);
            invoke_node->setArg(1, dict);
            invoke_node->setArg(2, op2);
            invoke_node->setRet(0, err);

            auto noerror_label = ctx->c.newLabel();
            wo_assure(!ctx->c.cmp(err, asmjit::Imm(0)));
            wo_assure(!ctx->c.je(noerror_label));

            x86_do_fail(ctx, WO_FAIL_INDEX_FAIL, err, rt_ip);

            wo_assure(!ctx->c.bind(noerror_label));
            return true;
        }
        virtual bool ir_mkcontain(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            uint16_t size = WO_IPVAL_MOVE_2;

            write_barrier_for_opnum1_if_write_global(ctx, opnum1, dr);

            auto op1 = opnum1.gp_value();

            asmjit::InvokeNode* invoke_node;

            if ((dr & 0b01) == 0)
                // Make array.
                wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&vmbase::make_array_impl,
                    asmjit::FuncSignatureT< wo::value*, wo::value*, uint16_t, wo::value*>()));
            else
                // Make dict.
                wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&vmbase::make_map_impl,
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

            auto str = ctx->c.newIntPtr();
            auto idx = ctx->c.newInt64();

            if (!opnum1.is_constant())
                wo_assure(!ctx->c.mov(
                    str, asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_string))));

            if (!opnum2.is_constant())
                wo_assure(!ctx->c.mov(
                    idx, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, m_integer))));

            asmjit::InvokeNode* invoke_node;
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&_vmjitcall_idstr,
                asmjit::FuncSignatureT< void, wo::value*, wo::string_t*, wo_integer_t>()));

            invoke_node->setArg(0, ctx->_vmcr);

            if (opnum1.is_constant())
                invoke_node->setArg(1, opnum1.const_value()->m_string);
            else
                invoke_node->setArg(1, str);

            if (opnum2.is_constant())
                invoke_node->setArg(2, opnum2.const_value()->m_string);
            else
                invoke_node->setArg(2, idx);
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
            wo_assure(!ctx->c.movsd(real_of_op1, asmjit::x86::ptr(opnum1.gp_value(), offsetof(value, m_real))));
            wo_assure(!ctx->c.comisd(real_of_op1, asmjit::x86::ptr(opnum2.gp_value(), offsetof(value, m_real))));

            wo_assure(!ctx->c.jp(x86_cmp_fail));
            wo_assure(!ctx->c.jne(x86_cmp_fail));

            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, m_integer)), 1));
            wo_assure(!ctx->c.jmp(x86_cmp_end));
            wo_assure(!ctx->c.bind(x86_cmp_fail));
            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, m_integer)), 0));
            wo_assure(!ctx->c.bind(x86_cmp_end));

            ctx->c.mov(asmjit::x86::byte_ptr(ctx->_vmcr, offsetof(value, m_type)), (uint8_t)value::valuetype::bool_type);

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
            wo_assure(!ctx->c.movsd(real_of_op1, asmjit::x86::ptr(opnum1.gp_value(), offsetof(value, m_real))));
            wo_assure(!ctx->c.comisd(real_of_op1, asmjit::x86::ptr(opnum2.gp_value(), offsetof(value, m_real))));

            wo_assure(!ctx->c.jp(x86_cmp_fail));
            wo_assure(!ctx->c.je(x86_cmp_fail));

            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, m_integer)), 1));
            wo_assure(!ctx->c.jmp(x86_cmp_end));
            wo_assure(!ctx->c.bind(x86_cmp_fail));
            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, m_integer)), 0));
            wo_assure(!ctx->c.bind(x86_cmp_end));

            ctx->c.mov(asmjit::x86::byte_ptr(ctx->_vmcr, offsetof(value, m_type)), (uint8_t)value::valuetype::bool_type);

            return true;
        }
        virtual bool ir_equs(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto str1 = ctx->c.newIntPtr();// opnum1.gp_value();
            auto str2 = ctx->c.newIntPtr();// opnum2.gp_value();

            if (!opnum1.is_constant())
                wo_assure(!ctx->c.mov(str1, asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_string))));
            if (!opnum2.is_constant())
                wo_assure(!ctx->c.mov(str2, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, m_string))));

            asmjit::InvokeNode* invoke_node;
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&_vmjitcall_equs,
                asmjit::FuncSignatureT<void, wo::value*, wo::string_t*, wo::string_t*>()));

            invoke_node->setArg(0, ctx->_vmcr);

            if (opnum1.is_constant())
                invoke_node->setArg(1, asmjit::Imm(opnum1.const_value()->m_string));
            else
                invoke_node->setArg(1, str1);

            if (opnum2.is_constant())
                invoke_node->setArg(2, asmjit::Imm(opnum2.const_value()->m_string));
            else
                invoke_node->setArg(2, str2);

            return true;
        }
        virtual bool ir_nequs(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto str1 = ctx->c.newIntPtr();// opnum1.gp_value();
            auto str2 = ctx->c.newIntPtr();// opnum2.gp_value();

            if (!opnum1.is_constant())
                wo_assure(!ctx->c.mov(str1, asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_string))));
            if (!opnum2.is_constant())
                wo_assure(!ctx->c.mov(str2, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, m_string))));

            asmjit::InvokeNode* invoke_node;
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&_vmjitcall_nequs,
                asmjit::FuncSignatureT<void, wo::value*, wo::string_t*, wo::string_t*>()));

            invoke_node->setArg(0, ctx->_vmcr);

            if (opnum1.is_constant())
                invoke_node->setArg(1, asmjit::Imm(opnum1.const_value()->m_string));
            else
                invoke_node->setArg(1, str1);

            if (opnum2.is_constant())
                invoke_node->setArg(2, asmjit::Imm(opnum2.const_value()->m_string));
            else
                invoke_node->setArg(2, str2);

            return true;
        }
        virtual bool ir_siddict(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;
            WO_JIT_ADDRESSING_N3_REG_BPOFF;

            auto op2 = opnum2.gp_value();
            auto op3 = opnum3.gp_value();

            auto dict = ctx->c.newIntPtr();
            auto err = ctx->c.newIntPtr();

            wo_assure(!ctx->c.mov(
                dict, asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_dictionary))));

            asmjit::InvokeNode* invoke_node;
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&_vmjitcall_siddict,
                asmjit::FuncSignatureT<const char*, wo::dictionary_t*, wo::value*, wo::value*>()));

            invoke_node->setArg(0, dict);
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

            if (opnum1.is_constant_i32())
                wo_assure(!ctx->c.cmp(
                    asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, m_integer)), opnum1.const_value()->m_integer));
            else
            {
                auto bvalue = ctx->c.newInt64();
                wo_assure(!ctx->c.mov(bvalue, asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_integer))));
                wo_assure(!ctx->c.cmp(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, m_integer)), bvalue));
            }

            if (dr & 0b01)
            {
                auto label_cond_not_match = ctx->c.newLabel();

                wo_assure(!ctx->c.je(label_cond_not_match));
                ir_make_checkpoint_fastcheck(ctx, check_point_ipaddr);
                wo_assure(!ctx->c.jmp(jump_ip(&ctx->c, jmp_place)));
                wo_assure(!ctx->c.bind(label_cond_not_match));
            }
            else
                wo_assure(!ctx->c.jne(jump_ip(&ctx->c, jmp_place)));

            return true;
        }
        virtual bool ir_idstruct(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;
            uint16_t offset = WO_IPVAL_MOVE_2;

            write_barrier_for_opnum1_if_write_global(ctx, opnum1, dr);

            auto op1 = opnum1.gp_value();
            auto stc = ctx->c.newIntPtr();

            wo_assure(!ctx->c.mov(
                stc, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, m_structure))));

            // Direct JIT implementation instead of function call
            auto struct_values_ptr = ctx->c.newIntPtr();
            wo_assure(!ctx->c.mov(struct_values_ptr, asmjit::x86::qword_ptr(stc, WO_OFFSETOF(structure_t, m_values))));
            wo_assure(!ctx->c.lea(struct_values_ptr, asmjit::x86::qword_ptr(struct_values_ptr, offset * sizeof(wo::value))));
            x86_set_val(ctx->c, op1, struct_values_ptr);

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

            auto sync = ctx->c.newInt32();
            wo_assure(!ctx->c.mov(sync, asmjit::Imm(wo_result_t::WO_API_SYNC_CHANGED_VM_STATE)));
            wo_assure(!ctx->c.ret(sync));
            return true;
        }
        virtual bool ir_ext_packargs(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip) override
        {
            WO_JIT_ADDRESSING_N1;
            uint16_t this_function_arg_count = WO_IPVAL_MOVE_2;
            uint16_t skip_closure_arg_count = WO_IPVAL_MOVE_2;

            write_barrier_for_opnum1_if_write_global(ctx, opnum1, dr);

            auto op1 = opnum1.gp_value();

            asmjit::InvokeNode* invoke_node;
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&vmbase::packargs_impl,
                asmjit::FuncSignatureT<
                void,
                wo::value*,
                uint16_t,
                const wo::value*,
                wo::value*,
                uint16_t>()));

            invoke_node->setArg(0, op1);
            invoke_node->setArg(1, asmjit::Imm(this_function_arg_count));
            invoke_node->setArg(2, ctx->_vmtp);
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
            wo_assure(!ctx->c.mov(divisor_val, asmjit::x86::qword_ptr(op2, offsetof(value, m_integer))));
            wo_assure(!ctx->c.cmp(divisor_val, asmjit::Imm(0)));
            wo_assure(!ctx->c.jne(div_zero_ok));
            x86_do_fail(ctx, WO_FAIL_UNEXPECTED,
                asmjit::Imm((intptr_t)"The divisor cannot be 0."), rt_ip);
            wo_assure(!ctx->c.bind(div_zero_ok));
            auto div_overflow_ok = ctx->c.newLabel();
            wo_assure(!ctx->c.cmp(divisor_val, asmjit::Imm(-1)));
            wo_assure(!ctx->c.jne(div_overflow_ok));
            wo_assure(!ctx->c.mov(divisor_val, asmjit::Imm(INT64_MIN)));
            wo_assure(!ctx->c.cmp(asmjit::x86::qword_ptr(op1, offsetof(value, m_integer)), divisor_val));
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
            auto immv = ctx->c.newInt64();
            wo_assure(!ctx->c.mov(immv, asmjit::Imm(INT64_MIN)));
            wo_assure(!ctx->c.cmp(asmjit::x86::qword_ptr(op1, offsetof(value, m_integer)), immv));
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
            wo_assure(!ctx->c.cmp(asmjit::x86::qword_ptr(op1, offsetof(value, m_integer)), asmjit::Imm(0)));
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
            wo_assure(!ctx->c.mov(divisor_val, asmjit::x86::qword_ptr(op1, offsetof(value, m_integer))));
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
        virtual bool ir_ext_popn(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip) override
        {
            WO_JIT_ADDRESSING_N1;

            auto pop_count = ctx->c.newInt64();
            wo_assure(!ctx->c.mov(
                pop_count,
                asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_integer))));
            wo_assure(!ctx->c.shl(pop_count, asmjit::Imm(4)));
            wo_assure(!ctx->c.lea(ctx->_vmssp, asmjit::x86::qword_ptr(ctx->_vmssp, pop_count)));

            return true;
        }
        virtual bool ir_ext_addh(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip) override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            if (opnum2.is_constant_i32())
                wo_assure(!ctx->c.add(
                    asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_handle)), opnum2.const_value()->m_handle));
            else
            {
                auto handle_of_op2 = ctx->c.newUInt64();
                wo_assure(!ctx->c.mov(handle_of_op2, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, m_handle))));
                wo_assure(!ctx->c.add(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_handle)), handle_of_op2));
            }
            return true;

        }
        virtual bool ir_ext_subh(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip) override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            if (opnum2.is_constant_i32())
                wo_assure(!ctx->c.sub(
                    asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_handle)), opnum2.const_value()->m_handle));
            else
            {
                auto handle_of_op2 = ctx->c.newUInt64();
                wo_assure(!ctx->c.mov(handle_of_op2, asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, m_handle))));
                wo_assure(!ctx->c.sub(asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_handle)), handle_of_op2));
            }
            return true;
        }
        virtual bool ir_ext_lth(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto result_reg = ctx->c.newUInt64();
            wo_assure(!ctx->c.xor_(result_reg, result_reg));

            if (opnum1.is_constant_h32())
            {
                wo_assure(!ctx->c.cmp(asmjit::x86::qword_ptr(
                    opnum2.gp_value(), offsetof(value, m_handle)), opnum1.const_value()->m_handle));
                wo_assure(!ctx->c.seta(result_reg.r8())); // op2 > const => const < op2
            }
            else if (opnum2.is_constant_h32())
            {
                wo_assure(!ctx->c.cmp(asmjit::x86::qword_ptr(
                    opnum1.gp_value(), offsetof(value, m_handle)), opnum2.const_value()->m_handle));
                wo_assure(!ctx->c.setb(result_reg.r8()));
            }
            else
            {
                auto int_of_op1 = ctx->c.newInt64();

                wo_assure(!ctx->c.mov(
                    int_of_op1,
                    asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_handle))));
                wo_assure(!ctx->c.cmp(
                    int_of_op1,
                    asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, m_handle))));
                wo_assure(!ctx->c.setb(result_reg.r8()));
            }

            wo_assure(!ctx->c.mov(
                asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, m_integer)), result_reg));
            wo_assure(!ctx->c.mov(
                asmjit::x86::byte_ptr(ctx->_vmcr, offsetof(value, m_type)), (uint8_t)value::valuetype::bool_type));
            return true;
        }
        virtual bool ir_ext_gth(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto result_reg = ctx->c.newInt64();
            wo_assure(!ctx->c.xor_(result_reg, result_reg));

            if (opnum1.is_constant_i32())
            {
                wo_assure(!ctx->c.cmp(
                    asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, m_handle)),
                    opnum1.const_value()->m_handle));
                wo_assure(!ctx->c.setb(result_reg.r8())); // op2 < const => const > op2
            }
            else if (opnum2.is_constant_i32())
            {
                wo_assure(!ctx->c.cmp(
                    asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_handle)),
                    opnum2.const_value()->m_handle));
                wo_assure(!ctx->c.seta(result_reg.r8()));
            }
            else
            {
                auto int_of_op1 = ctx->c.newInt64();

                wo_assure(!ctx->c.mov(
                    int_of_op1,
                    asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_handle))));
                wo_assure(!ctx->c.cmp(
                    int_of_op1,
                    asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, m_handle))));
                wo_assure(!ctx->c.seta(result_reg.r8()));
            }

            wo_assure(!ctx->c.mov(
                asmjit::x86::qword_ptr(
                    ctx->_vmcr, 
                    offsetof(value, m_integer)), 
                result_reg));
            wo_assure(!ctx->c.mov(
                asmjit::x86::byte_ptr(ctx->_vmcr, offsetof(value, m_type)),
                (uint8_t)value::valuetype::bool_type));

            return true;
        }
        virtual bool ir_ext_elth(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override
        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto result_reg = ctx->c.newInt64();
            wo_assure(!ctx->c.xor_(result_reg, result_reg));

            if (opnum1.is_constant_i32())
            {
                wo_assure(!ctx->c.cmp(asmjit::x86::qword_ptr(
                    opnum2.gp_value(), offsetof(value, m_handle)), opnum1.const_value()->m_handle));
                wo_assure(!ctx->c.setae(result_reg.r8())); // op2 >= const => const <= op2
            }
            else if (opnum2.is_constant_i32())
            {
                wo_assure(!ctx->c.cmp(asmjit::x86::qword_ptr(
                    opnum1.gp_value(), offsetof(value, m_handle)), opnum2.const_value()->m_handle));
                wo_assure(!ctx->c.setbe(result_reg.r8()));
            }
            else
            {
                auto int_of_op1 = ctx->c.newInt64();

                wo_assure(!ctx->c.mov(
                    int_of_op1,
                    asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_handle))));
                wo_assure(!ctx->c.cmp(
                    int_of_op1,
                    asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, m_handle))));
                wo_assure(!ctx->c.setbe(result_reg.r8()));
            }

            wo_assure(!ctx->c.mov(asmjit::x86::qword_ptr(ctx->_vmcr, offsetof(value, m_integer)), result_reg));
            wo_assure(!ctx->c.mov(asmjit::x86::byte_ptr(ctx->_vmcr, offsetof(value, m_type)), (uint8_t)value::valuetype::bool_type));
            return true;
        }
        virtual bool ir_ext_egth(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip)override

        {
            WO_JIT_ADDRESSING_N1;
            WO_JIT_ADDRESSING_N2;

            auto result_reg = ctx->c.newInt64();
            wo_assure(!ctx->c.xor_(result_reg, result_reg));

            if (opnum1.is_constant_i32())
            {
                wo_assure(!ctx->c.cmp(
                    asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, m_handle)),
                    opnum1.const_value()->m_handle));
                wo_assure(!ctx->c.setbe(result_reg.r8())); // op2 <= const => const >= op2
            }
            else if (opnum2.is_constant_i32())
            {
                wo_assure(!ctx->c.cmp(
                    asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_handle)),
                    opnum2.const_value()->m_handle));
                wo_assure(!ctx->c.setae(result_reg.r8()));
            }
            else
            {
                auto int_of_op1 = ctx->c.newInt64();

                wo_assure(!ctx->c.mov(
                    int_of_op1,
                    asmjit::x86::qword_ptr(opnum1.gp_value(), offsetof(value, m_handle))));
                wo_assure(!ctx->c.cmp(
                    int_of_op1,
                    asmjit::x86::qword_ptr(opnum2.gp_value(), offsetof(value, m_handle))));
                wo_assure(!ctx->c.setae(result_reg.r8()));
            }

            wo_assure(!ctx->c.mov(
                asmjit::x86::qword_ptr(
                    ctx->_vmcr, 
                    offsetof(value, m_integer)),
                result_reg));
            wo_assure(!ctx->c.mov(
                asmjit::x86::byte_ptr(ctx->_vmcr, offsetof(value, m_type)),
                (uint8_t)value::valuetype::bool_type));

            return true;
        }
        virtual bool ir_unpack(X64CompileContext* ctx, unsigned int dr, const byte_t*& rt_ip) override
        {
            const byte_t* rollback_ip = rt_ip - 1;

            WO_JIT_ADDRESSING_N1;
            auto unpack_argc_unsigned = WO_IPVAL_MOVE_4;

            auto op1 = opnum1.gp_value();
            auto new_sp = ctx->c.newUIntPtr();

            asmjit::InvokeNode* invoke_node;
            wo_assure(!ctx->c.invoke(&invoke_node, (intptr_t)&vmbase::unpackargs_impl,
                asmjit::FuncSignatureT<wo::value*, vmbase*, int32_t, value*, value*, const byte_t*, value*, value*>()));

            invoke_node->setArg(0, ctx->_vmbase);
            invoke_node->setArg(1, op1);
            invoke_node->setArg(2, asmjit::Imm(reinterpret_cast<int32_t&>(unpack_argc_unsigned)));
            invoke_node->setArg(3, ctx->_vmtc);
            invoke_node->setArg(4, asmjit::Imm((intptr_t)rt_ip));
            invoke_node->setArg(5, ctx->_vmssp);
            invoke_node->setArg(6, ctx->_vmsbp);

            invoke_node->setRet(0, new_sp);

            // Check if new_sp is null
            auto noerror_label = ctx->c.newLabel();
            wo_assure(!ctx->c.cmp(new_sp, 0));
            wo_assure(!ctx->c.jne(noerror_label));

            // It's safe to use rollback_ip, only abort/yieldbr/stackoverflow/debug use it to 
            // rollback to non-jit-execution. and stackoverflow always executed before debug.
            // other interrupt will not run the code.
            ir_make_checkpoint_fastcheck(ctx, rollback_ip);

            wo_assure(!ctx->c.int3());

            wo_assure(!ctx->c.bind(noerror_label));
            wo_assure(!ctx->c.mov(ctx->_vmssp, new_sp));

            return true;
        }

#undef WO_JIT_ADDRESSING_N1
#undef WO_JIT_ADDRESSING_N2
#undef WO_JIT_ADDRESSING_N3_REG_BPOFF
#undef WO_JIT_NOT_SUPPORT
    };

    void analyze_jit(byte_t* codebuf, runtime_env* env)
    {
        switch (platform_info::ARCH_TYPE)
        {
        case platform_info::ArchMask::X86 | platform_info::ArchMask::BIT64:
            asmjit_compiler_x64().analyze_jit(codebuf, env);
            break;
        default:
            // No JIT support do nothing.
            break;
        }
    }
    void free_jit(runtime_env* env)
    {
        switch (platform_info::ARCH_TYPE)
        {
        case platform_info::ArchMask::X86 | platform_info::ArchMask::BIT64:
            asmjit_compiler_x64::free_jit(env);
            break;
        default:
            // No JIT support do nothing.
            break;
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

    void free_jit(runtime_env* min_env)
    {
    }
}
#endif
