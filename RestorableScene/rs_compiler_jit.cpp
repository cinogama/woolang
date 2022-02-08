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
        vmbase* vmptr)
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
                rs_asure(!x86compiler.lea(result, asmjit::x86::byte_ptr(stack_bp, RS_SIGNED_SHIFT(RS_IPVAL_MOVE_1) * sizeof(value))));
                return result;
            }
            else
            {
                // from reg
                auto result = x86compiler.newUIntPtr();
                rs_asure(!x86compiler.lea(result, asmjit::x86::byte_ptr(reg, RS_IPVAL_MOVE_1 * sizeof(value))));
                return result;
            }
        }
        else
        {
            // opnum from global_const
            auto result = x86compiler.newUIntPtr();
            rs_asure(!x86compiler.mov(result, vmptr->env->constant_global_reg_rtstack + RS_SAFE_READ_MOVE_4));
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
    void native_do_calln_nativefunc(vmbase* vm, rs_extern_native_func_t call_aim_native_func, const byte_t* rt_ip, value* rt_sp, value* rt_bp)
    {
        rt_sp->type = value::valuetype::callstack;
        rt_sp->ret_ip = (uint32_t)(rt_ip - vm->env->rt_codes);
        rt_sp->bp = (uint32_t)(vm->stack_mem_begin - rt_bp);
        rt_bp = --rt_sp;
        vm->bp = vm->sp = rt_sp;

        // May be useless?
        vm->cr->set_nil();

        vm->ip = reinterpret_cast<byte_t*>(call_aim_native_func);

        rs_asure(vm->interrupt(vmbase::vm_interrupt_type::LEAVE_INTERRUPT));
        call_aim_native_func(reinterpret_cast<rs_vm>(vm), reinterpret_cast<rs_value>(rt_sp + 2), vm->tc->integer);
        rs_asure(vm->clear_interrupt(vmbase::vm_interrupt_type::LEAVE_INTERRUPT));

        rs_assert((rt_bp + 1)->type == value::valuetype::callstack);
        //value* stored_bp = vm->stack_mem_begin - (++rt_bp)->bp;
        //rt_sp = rt_bp;
        //rt_bp = stored_bp;
    }
    void native_do_calln_vmfunc(vmbase* vm, uint32_t call_aim_vm_func, const byte_t* rt_ip, value* rt_sp, value* rt_bp)
    {
        rt_sp->type = value::valuetype::callstack;
        rt_sp->ret_ip = (uint32_t)(rt_ip - vm->env->rt_codes);
        rt_sp->bp = (uint32_t)(vm->stack_mem_begin - rt_bp);
        rt_bp = --rt_sp;
        vm->bp = vm->sp = rt_sp;

        vm->co_pre_invoke((rs_integer_t)call_aim_vm_func, vm->tc->integer);
        vm->run();
    }
   
    asmjit::x86::Gp get_opnum_ptr_ref(
        asmjit::x86::Compiler& x86compiler,
        const byte_t*& rt_ip,
        bool dr,
        asmjit::x86::Gp stack_bp,
        asmjit::x86::Gp reg,
        vmbase* vmptr)
    {
        auto opnum = get_opnum_ptr(x86compiler, rt_ip, dr, stack_bp, reg, vmptr);
        // if opnum->type is ref, get it's ref

        asmjit::InvokeNode* inode = nullptr;
        rs_asure(!x86compiler.invoke(&inode, (uint64_t)&native_abi_get_ref,
            asmjit::FuncSignatureT<value*, value*>()
        ));
        inode->setArg(0, opnum);

        return inode->ret().as<asmjit::x86::Gp>();
    }

    asmjit::x86::Gp x86_set_val(asmjit::x86::Compiler& x86compiler, asmjit::x86::Gp val, asmjit::x86::Gp val2)
    {
        asmjit::InvokeNode* inode = nullptr;
        rs_asure(!x86compiler.invoke(&inode, (uint64_t)&native_abi_set_val,
            asmjit::FuncSignatureT<value*, value*, value*>()
        ));
        inode->setArg(0, val);
        inode->setArg(1, val2);

        return inode->ret().as<asmjit::x86::Gp>();
    }
    asmjit::x86::Gp x86_set_ref(asmjit::x86::Compiler& x86compiler, asmjit::x86::Gp val, asmjit::x86::Gp val2)
    {
        asmjit::InvokeNode* inode = nullptr;
        rs_asure(!x86compiler.invoke(&inode, (uint64_t)&native_abi_set_ref,
            asmjit::FuncSignatureT<value*, value*, value*>()
        ));
        inode->setArg(0, val);
        inode->setArg(1, val2);

        return inode->ret().as<asmjit::x86::Gp>();
    }

    asmjit::x86::Gp x86_set_nil(asmjit::x86::Compiler& x86compiler, asmjit::x86::Gp val)
    {
        asmjit::InvokeNode* inode = nullptr;
        rs_asure(!x86compiler.invoke(&inode, (uint64_t)&native_abi_set_nil,
            asmjit::FuncSignatureT<value*, value*>()
        ));
        inode->setArg(0, val);

        return inode->ret().as<asmjit::x86::Gp>();
    }

    void x86_do_calln_native_func(asmjit::x86::Compiler& x86compiler,
        asmjit::x86::Gp vm,
        rs_extern_native_func_t call_aim_native_func,
        const byte_t* rt_ip,
        asmjit::x86::Gp rt_sp,
        asmjit::x86::Gp rt_bp)
    {
        asmjit::InvokeNode* inode = nullptr;
        rs_asure(!x86compiler.invoke(&inode, (uint64_t)&native_do_calln_nativefunc,
            asmjit::FuncSignatureT<void, vmbase*, rs_extern_native_func_t, const byte_t*, value*, value*>()
        ));
        inode->setArg(0, vm);
        inode->setArg(1, call_aim_native_func);
        inode->setArg(2, rt_ip);
        inode->setArg(3, rt_sp);
        inode->setArg(4, rt_bp);
    }

    void x86_do_calln_vm_func(asmjit::x86::Compiler& x86compiler,
        asmjit::x86::Gp vm,
        uint32_t call_aim_vm_func,
        const byte_t* rt_ip,
        asmjit::x86::Gp rt_sp,
        asmjit::x86::Gp rt_bp)
    {
        asmjit::InvokeNode* inode = nullptr;
        rs_asure(!x86compiler.invoke(&inode, (uint64_t)&native_do_calln_vmfunc,
            asmjit::FuncSignatureT<void, vmbase*, uint32_t, const byte_t*, value*, value*>()
        ));
        inode->setArg(0, vm);
        inode->setArg(1, call_aim_vm_func);
        inode->setArg(2, rt_ip);
        inode->setArg(3, rt_sp);
        inode->setArg(4, rt_bp);
    }

    jit_compiler_x86::jit_packed_function jit_compiler_x86::compile_jit(const byte_t* rt_ip, vmbase* compile_vmptr)
    {
        // Prepare asmjit;
        using namespace asmjit;

        CodeHolder code_buffer;
        code_buffer.init(get_jit_runtime().environment());
        x86::Compiler x86compiler(&code_buffer);

        // Generate function declear
        auto jit_func_node = x86compiler.addFunc(FuncSignatureT<void, vmbase*, value*, value*, value*>());
        // void _jit_(vmbase*  vm , value* bp, value* reg, value* const_global);

        // 0. Get vmptr reg stack base global ptr.
        auto jit_vm_ptr = x86compiler.newUIntPtr();
        auto jit_stack_bp_ptr = x86compiler.newUIntPtr();
        auto jit_reg_ptr = x86compiler.newUIntPtr();
        jit_func_node->setArg(0, jit_vm_ptr);
        jit_func_node->setArg(1, jit_stack_bp_ptr);
        jit_func_node->setArg(2, jit_reg_ptr);
        auto jit_stack_sp_ptr = x86compiler.newUIntPtr();

        rs_asure(!x86compiler.mov(jit_stack_sp_ptr, jit_stack_bp_ptr));                    // let sp = bp;

        byte_t opcode_dr = (byte_t)(instruct::abrt << 2);
        instruct::opcode opcode = (instruct::opcode)(opcode_dr & 0b11111100u);
        unsigned dr = opcode_dr & 0b00000011u;

        for (;;)
        {
            opcode_dr = *(rt_ip++);
            opcode = (instruct::opcode)(opcode_dr & 0b11111100u);
            dr = opcode_dr & 0b00000011u;

#define RS_JIT_ADDRESSING_N1 auto opnum1 = get_opnum_ptr(x86compiler, rt_ip, dr >> 1, jit_stack_bp_ptr, jit_reg_ptr, compile_vmptr)
#define RS_JIT_ADDRESSING_N2 auto opnum2 = get_opnum_ptr(x86compiler, rt_ip, dr &0b01, jit_stack_bp_ptr, jit_reg_ptr, compile_vmptr)
#define RS_JIT_ADDRESSING_N1_REF auto opnum1 = get_opnum_ptr_ref(x86compiler, rt_ip, dr >> 1, jit_stack_bp_ptr, jit_reg_ptr, compile_vmptr)
#define RS_JIT_ADDRESSING_N2_REF auto opnum2 = get_opnum_ptr_ref(x86compiler, rt_ip, dr &0b01, jit_stack_bp_ptr, jit_reg_ptr, compile_vmptr)

            switch (opcode)
            {
            case instruct::psh:
            {
                if (dr & 0b01)
                {
                    // RS_ADDRESSING_N1_REF;
                    // (rt_sp--)->set_val(opnum1);

                    RS_JIT_ADDRESSING_N1_REF;

                    x86_set_val(x86compiler, jit_stack_sp_ptr, opnum1);
                    rs_asure(!x86compiler.sub(jit_stack_sp_ptr, sizeof(value)));
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
                        rs_asure(!x86compiler.sub(jit_stack_sp_ptr, sizeof(value)));
                    }
                }
                break;
            }
            case instruct::set:
            {
                RS_JIT_ADDRESSING_N1;
                RS_JIT_ADDRESSING_N2_REF;

                x86_set_val(x86compiler, opnum1, opnum2);

                break;
            }
            case instruct::ret:
            {
                rs_asure(!x86compiler.ret());
                break;
            }
            case instruct::calln:
            {
                if (dr)
                {
                    // Call native
                    rs_extern_native_func_t call_aim_native_func = (rs_extern_native_func_t)(RS_IPVAL_MOVE_8);
                    x86_do_calln_native_func(x86compiler, jit_vm_ptr, call_aim_native_func, rt_ip, jit_stack_sp_ptr, jit_stack_bp_ptr);
                }
                else
                {
                    uint32_t call_aim_vm_func = RS_IPVAL_MOVE_4;

                    x86_do_calln_vm_func(x86compiler, jit_vm_ptr, call_aim_vm_func, rt_ip, jit_stack_sp_ptr, jit_stack_bp_ptr);

                }
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
                    default:
                        rs_warning("Unknown ext 0 instruct.");
                        return nullptr;
                    }
                    break;
                case 1:     // extern-opcode-page-1
                    switch ((instruct::extern_opcode_page_1)(opcode))
                    {
                    case instruct::extern_opcode_page_1::endjit:
                    {
                        // This function work end!
                        rs_asure(!x86compiler.ret());
                        rs_asure(!x86compiler.endFunc());
                        rs_asure(!x86compiler.finalize());

                        jit_packed_function result;

                        rs_asure(!get_jit_runtime().add(&result, &code_buffer));
                        return result;
                    }
                    default:
                        rs_warning("Unknown ext 1 instruct.");
                        return nullptr;
                    }
                    break;
                default:
                    rs_warning("Unknown extern-opcode-page.");
                    return nullptr;
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