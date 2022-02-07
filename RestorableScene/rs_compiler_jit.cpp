#define _CRT_SECURE_NO_WARNINGS

#include "rs_compiler_jit.hpp"
#include "rs_instruct.hpp"
#include "rs_vm.hpp"

#include "asmjit/asmjit.h"


#define RS_SAFE_READ_OFFSET_GET_QWORD (*(uint64_t*)(rt_ip-8))
#define RS_SAFE_READ_OFFSET_GET_DWORD (*(uint32_t*)(rt_ip-4))
#define RS_SAFE_READ_OFFSET_GET_WORD (*(uint16_t*)(rt_ip-2))

// FOR BigEndian
#define RS_SAFE_READ_OFFSET_PER_BYTE(OFFSET, TYPE) (((TYPE)(*(rt_ip-OFFSET)))<<((sizeof(TYPE)-OFFSET)*8))
#define RS_IS_ODD_IRPTR(ALLIGN) 1 //(reinterpret_cast<size_t>(rt_ip)%ALLIGN)

#define RS_SAFE_READ_MOVE_2 (rt_ip+=2,RS_IS_ODD_IRPTR(2)?\
                                    (RS_SAFE_READ_OFFSET_PER_BYTE(2,uint16_t)|RS_SAFE_READ_OFFSET_PER_BYTE(1,uint16_t)):\
                                    RS_SAFE_READ_OFFSET_GET_WORD)
#define RS_SAFE_READ_MOVE_4 (rt_ip+=4,RS_IS_ODD_IRPTR(4)?\
                                    (RS_SAFE_READ_OFFSET_PER_BYTE(4,uint32_t)|RS_SAFE_READ_OFFSET_PER_BYTE(3,uint32_t)\
                                    |RS_SAFE_READ_OFFSET_PER_BYTE(2,uint32_t)|RS_SAFE_READ_OFFSET_PER_BYTE(1,uint32_t)):\
                                    RS_SAFE_READ_OFFSET_GET_DWORD)
#define RS_SAFE_READ_MOVE_8 (rt_ip+=8,RS_IS_ODD_IRPTR(8)?\
                                    (RS_SAFE_READ_OFFSET_PER_BYTE(8,uint64_t)|RS_SAFE_READ_OFFSET_PER_BYTE(7,uint64_t)|\
                                    RS_SAFE_READ_OFFSET_PER_BYTE(6,uint64_t)|RS_SAFE_READ_OFFSET_PER_BYTE(5,uint64_t)|\
                                    RS_SAFE_READ_OFFSET_PER_BYTE(4,uint64_t)|RS_SAFE_READ_OFFSET_PER_BYTE(3,uint64_t)|\
                                    RS_SAFE_READ_OFFSET_PER_BYTE(2,uint64_t)|RS_SAFE_READ_OFFSET_PER_BYTE(1,uint64_t)):\
                                    RS_SAFE_READ_OFFSET_GET_QWORD)
#define RS_IPVAL (*(rt_ip))
#define RS_IPVAL_MOVE_1 (*(rt_ip++))

            // X86 support non-alligned addressing, so just do it!

#define RS_IPVAL_MOVE_2 RS_SAFE_READ_MOVE_2
#define RS_IPVAL_MOVE_4 RS_SAFE_READ_MOVE_4
#define RS_IPVAL_MOVE_8 RS_SAFE_READ_MOVE_8

#define RS_SIGNED_SHIFT(VAL) (((signed char)((unsigned char)(((unsigned char)(VAL))<<1)))>>1)

#define RS_ADDRESSING_N1 value * opnum1 = ((dr >> 1) ?\
                        (\
                            (RS_IPVAL & (1 << 7)) ?\
                            (rt_bp + RS_SIGNED_SHIFT(RS_IPVAL_MOVE_1))\
                            :\
                            (RS_IPVAL_MOVE_1 + reg_begin)\
                            )\
                        :\
                        (\
                            RS_IPVAL_MOVE_4 + const_global_begin\
                        ))

#define RS_ADDRESSING_N2 value * opnum2 = ((dr & 0b01) ?\
                        (\
                            (RS_IPVAL & (1 << 7)) ?\
                            (rt_bp + RS_SIGNED_SHIFT(RS_IPVAL_MOVE_1))\
                            :\
                            (RS_IPVAL_MOVE_1 + reg_begin)\
                            )\
                        :\
                        (\
                            RS_IPVAL_MOVE_4 + const_global_begin\
                        ))

#define RS_ADDRESSING_N1_REF RS_ADDRESSING_N1 -> get()
#define RS_ADDRESSING_N2_REF RS_ADDRESSING_N2 -> get()

#define RS_VM_FAIL(ERRNO,ERRINFO) {ip = rt_ip;sp = rt_sp;bp = rt_bp;rs_fail(ERRNO,ERRINFO);continue;}

namespace rs
{
    asmjit::JitRuntime& get_jit_runtime()
    {
        static asmjit::JitRuntime jit_runtime;
        return jit_runtime;
    }


    asmjit::x86::Gp get_opnum_ptr(
        asmjit::x86::Compiler& x86compiler,
        const byte_t*& rt_ip,
        bool dr,
        asmjit::x86::Gp stack_bp,
        asmjit::x86::Gp reg,
        asmjit::x86::Gp const_global)
    {
        /*
        #define RS_ADDRESSING_N1 value * opnum1 = ((dr >> 1) ?\
                        (\
                            (RS_IPVAL & (1 << 7)) ?\
                            (rt_bp + RS_SIGNED_SHIFT(RS_IPVAL_MOVE_1))\
                            :\
                            (RS_IPVAL_MOVE_1 + reg_begin)\
                            )\
                        :\
                        (\
                            RS_IPVAL_MOVE_4 + const_global_begin\
                        ))
        */
        if (dr)
        {
            // opnum from bp-offset or regist
            if ((*rt_ip) & (1 << 7))
            {
                // from bp-offset
                auto result = x86compiler.newUIntPtr();
                x86compiler.mov(result, asmjit::x86::byte_ptr(stack_bp, RS_SIGNED_SHIFT(RS_IPVAL_MOVE_1) * sizeof(value)));
                return result;
            }
            else
            {
                // from reg
                auto result = x86compiler.newUIntPtr();
                x86compiler.mov(result, asmjit::x86::byte_ptr(reg, RS_IPVAL_MOVE_1 * sizeof(value)));
                return result;
            }
        }
        else
        {
            // opnum from global_const
            auto result = x86compiler.newUIntPtr();
            x86compiler.mov(result, asmjit::x86::byte_ptr(const_global, RS_SAFE_READ_MOVE_4 * sizeof(value)));
            return result;
        }


    }

    value* native_abi_get_ref(value* origin_val)
    {
        return origin_val->get();
    }
    value* native_abi_set_val(value* origin_val, value* set_val)
    {
        return origin_val->set_val(set_val);
    }
    value* native_abi_set_ref(value* origin_val, value* set_val)
    {
        return origin_val->set_ref(set_val);
    }
    value* native_abi_set_nil(value* origin_val)
    {
        return origin_val->set_nil();
    }

    asmjit::x86::Gp get_opnum_ptr_ref(
        asmjit::x86::Compiler& x86compiler,
        const byte_t*& rt_ip,
        bool dr,
        asmjit::x86::Gp stack_bp,
        asmjit::x86::Gp reg,
        asmjit::x86::Gp const_global)
    {
        auto opnum = get_opnum_ptr(x86compiler, rt_ip, dr, stack_bp, reg, const_global);
        // if opnum->type is ref, get it's ref

        asmjit::InvokeNode* inode = nullptr;
        auto fc = x86compiler.invoke(&inode, (uint64_t)&native_abi_get_ref,
            asmjit::FuncSignatureT<value*, value*>()
        );
        inode->setArg(0, opnum);

        return inode->ret().as<asmjit::x86::Gp>();
    }

    asmjit::x86::Gp x86_set_val(asmjit::x86::Compiler& x86compiler, asmjit::x86::Gp val, asmjit::x86::Gp val2)
    {
        asmjit::InvokeNode* inode = nullptr;
        auto fc = x86compiler.invoke(&inode, (uint64_t)&native_abi_set_val,
            asmjit::FuncSignatureT<value*, value*, value*>()
        );
        inode->setArg(0, val);
        inode->setArg(1, val2);

        return inode->ret().as<asmjit::x86::Gp>();
    }
    asmjit::x86::Gp x86_set_ref(asmjit::x86::Compiler& x86compiler, asmjit::x86::Gp val, asmjit::x86::Gp val2)
    {
        asmjit::InvokeNode* inode = nullptr;
        auto fc = x86compiler.invoke(&inode, (uint64_t)&native_abi_set_ref,
            asmjit::FuncSignatureT<value*, value*, value*>()
        );
        inode->setArg(0, val);
        inode->setArg(1, val2);

        return inode->ret().as<asmjit::x86::Gp>();
    }

    asmjit::x86::Gp x86_set_nil(asmjit::x86::Compiler& x86compiler, asmjit::x86::Gp val)
    {
        asmjit::InvokeNode* inode = nullptr;
        auto fc = x86compiler.invoke(&inode, (uint64_t)&native_abi_set_nil,
            asmjit::FuncSignatureT<value*, value*>()
        );
        inode->setArg(0, val);

        return inode->ret().as<asmjit::x86::Gp>();
    }

    jit_compiler_x86::jit_packed_function jit_compiler_x86::compile_jit(const byte_t* rt_ip)
    {
        // Prepare asmjit;
        using namespace asmjit;

        CodeHolder code_buffer;
        code_buffer.init(get_jit_runtime().environment());
        x86::Compiler x86compiler(&code_buffer);

        // Generate function declear
        auto jit_func_node = x86compiler.newFunc(FuncSignatureT<void, vmbase*, value*, value*, value*>());
        // void _jit_(vmbase*  vm , value* bp, value* reg, value* const_global);

        // 0. Get vmptr reg stack base global ptr.
        auto jit_vm_ptr = x86compiler.newUIntPtr();
        auto jit_stack_bp_ptr = x86compiler.newUIntPtr();
        auto jit_reg_ptr = x86compiler.newUIntPtr();
        auto jit_const_global_ptr = x86compiler.newUIntPtr();
        jit_func_node->setArg(0, jit_vm_ptr);
        jit_func_node->setArg(1, jit_stack_bp_ptr);
        jit_func_node->setArg(2, jit_reg_ptr);
        jit_func_node->setArg(3, jit_const_global_ptr);
        auto jit_stack_sp_ptr = x86compiler.newUIntPtr();

        x86compiler.mov(jit_stack_sp_ptr, jit_stack_bp_ptr);                    // let sp = bp;

        byte_t opcode_dr = (byte_t)(instruct::abrt << 2);
        instruct::opcode opcode = (instruct::opcode)(opcode_dr & 0b11111100u);
        unsigned dr = opcode_dr & 0b00000011u;

        for (;;)
        {
            opcode_dr = *(rt_ip++);
            opcode = (instruct::opcode)(opcode_dr & 0b11111100u);
            dr = opcode_dr & 0b00000011u;

#define RS_JIT_ADDRESSING_N1 get_opnum_ptr(x86compiler, rt_ip, dr >> 1, jit_stack_bp_ptr, jit_reg_ptr, jit_const_global_ptr)
#define RS_JIT_ADDRESSING_N2 get_opnum_ptr(x86compiler, rt_ip, dr &0b01, jit_stack_bp_ptr, jit_reg_ptr, jit_const_global_ptr)
#define RS_JIT_ADDRESSING_N1_REF get_opnum_ptr_ref(x86compiler, rt_ip, dr >> 1, jit_stack_bp_ptr, jit_reg_ptr, jit_const_global_ptr)
#define RS_JIT_ADDRESSING_N2_REF get_opnum_ptr_ref(x86compiler, rt_ip, dr &0b01, jit_stack_bp_ptr, jit_reg_ptr, jit_const_global_ptr)

            switch (opcode)
            {
            case instruct::psh:
            {
                if (dr & 0b01)
                {
                    // RS_ADDRESSING_N1_REF;
                    // (rt_sp--)->set_val(opnum1);
                    x86_set_val(
                        x86compiler,
                        jit_stack_sp_ptr, RS_JIT_ADDRESSING_N1_REF);
                    x86compiler.sub(jit_stack_sp_ptr, sizeof(value));
                }
                else
                {
                    // uint16_t psh_repeat = RS_IPVAL_MOVE_2;
                    // for (uint32_t i = 0; i < psh_repeat; i++)
                    //      (rt_sp--)->set_nil();

                    uint16_t psh_repeat = RS_IPVAL_MOVE_2;
                    for (uint32_t i = 0; i < psh_repeat; i++)
                    {
                        x86_set_nil(x86compiler,
                            jit_stack_sp_ptr);
                        x86compiler.sub(jit_stack_sp_ptr, sizeof(value));
                    }
                }
                break;
            }
            case instruct::calln:
            {
                if (dr)
                {
                    // Call native
                    rs_extern_native_func_t call_aim_native_func = (rs_extern_native_func_t)(RS_IPVAL_MOVE_8);

                    rt_sp->type = value::valuetype::callstack;
                    rt_sp->ret_ip = (uint32_t)(rt_ip - rt_env->rt_codes);
                    rt_sp->bp = (uint32_t)(stack_mem_begin - rt_bp);
                    rt_bp = --rt_sp;
                    bp = sp = rt_sp;
                    rt_cr->set_nil();

                    ip = reinterpret_cast<byte_t*>(call_aim_native_func);

                    rs_asure(interrupt(vm_interrupt_type::LEAVE_INTERRUPT));
                    call_aim_native_func(reinterpret_cast<rs_vm>(this), reinterpret_cast<rs_value>(rt_sp + 2), tc->integer);
                    rs_asure(clear_interrupt(vm_interrupt_type::LEAVE_INTERRUPT));

                    rs_assert((rt_bp + 1)->type == value::valuetype::callstack);
                    value* stored_bp = stack_mem_begin - (++rt_bp)->bp;
                    rt_sp = rt_bp;
                    rt_bp = stored_bp;
                }
                else
                {
                    const byte_t* aimplace = rt_env->rt_codes + RS_IPVAL_MOVE_4;

                    rt_sp->type = value::valuetype::callstack;
                    rt_sp->ret_ip = (uint32_t)(rt_ip - rt_env->rt_codes);
                    rt_sp->bp = (uint32_t)(stack_mem_begin - rt_bp);
                    rt_bp = --rt_sp;

                    rt_ip = aimplace;

                }
                break;
            }
            default:
                // Unknown opcode, return nullptr.
                rs_warning("Unknown instruct in jit-compiling.");
                return nullptr;
            }
        }

        /////////////////

         // There is something wrong happend.
        return nullptr;
    }
}