#define _CRT_SECURE_NO_WARNINGS

#if WO_ENABLE_ASMJIT

#include "wo_compiler_jit.hpp"
#include "wo_instruct.hpp"
#include "wo_vm.hpp"

#include "asmjit/asmjit.h"


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

#define WO_ADDRESSING_N1 value * opnum1 = ((dr >> 1) ?\
                        (\
                            (WO_IPVAL & (1 << 7)) ?\
                            (rt_bp + WO_SIGNED_SHIFT(WO_IPVAL_MOVE_1))\
                            :\
                            (WO_IPVAL_MOVE_1 + reg_begin)\
                            )\
                        :\
                        (\
                            WO_IPVAL_MOVE_4 + const_global_begin\
                        ))

#define WO_ADDRESSING_N2 value * opnum2 = ((dr & 0b01) ?\
                        (\
                            (WO_IPVAL & (1 << 7)) ?\
                            (rt_bp + WO_SIGNED_SHIFT(WO_IPVAL_MOVE_1))\
                            :\
                            (WO_IPVAL_MOVE_1 + reg_begin)\
                            )\
                        :\
                        (\
                            WO_IPVAL_MOVE_4 + const_global_begin\
                        ))

#define WO_ADDRESSING_N1_REF WO_ADDRESSING_N1 -> get()
#define WO_ADDRESSING_N2_REF WO_ADDRESSING_N2 -> get()

#define WO_VM_FAIL(ERRNO,ERRINFO) {ip = rt_ip;sp = rt_sp;bp = rt_bp;wo_fail(ERRNO,ERRINFO);continue;}

namespace wo
{
    asmjit::JitRuntime& get_jit_runtime()
    {
        static asmjit::JitRuntime jit_runtime;
        return jit_runtime;
    }

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
    asmjit::X86Mem intptr_ptr(const T& opgreg, int32_t offset = 0)
    {
#ifdef WO_PLATFORM_64
        return asmjit::x86::qword_ptr(opgreg, offset);
#else
        return asmjit::x86::dword_ptr(opgreg, offset);
#endif
    }

    may_constant_x86Gp get_opnum_ptr(
        asmjit::X86Compiler& x86compiler,
        const byte_t*& rt_ip,
        bool dr,
        asmjit::X86Gp stack_bp,
        asmjit::X86Gp reg,
        vmbase* vmptr)
    {
        /*
        #define WO_ADDRESSING_N1 value * opnum1 = ((dr >> 1) ?\
                        (\
                            (WO_IPVAL & (1 << 7)) ?\
                            (rt_bp + WO_SIGNED_SHIFT(WO_IPVAL_MOVE_1))\
                            :\
                            (WO_IPVAL_MOVE_1 + reg_begin)\
                            )\
                        :\
                        (\
                            WO_IPVAL_MOVE_4 + const_global_begin\
                        ))
        */
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

            if (const_global_index < vmptr->env->constant_value_count)
            {
                //Is constant
                return may_constant_x86Gp{ &x86compiler, true,vmptr->env->constant_global_reg_rtstack + const_global_index };
            }
            else
            {
                auto result = x86compiler.newUIntPtr();
                wo_asure(!x86compiler.mov(result, (size_t)(vmptr->env->constant_global_reg_rtstack + const_global_index)));
                return may_constant_x86Gp{ &x86compiler,false,nullptr,result };
            }
        }


    }

    //value* native_abi_set_ref(value* origin_val, value* set_val)
    //{
    //    return origin_val->set_ref(set_val);
    //}

    void native_do_calln_nativefunc(vmbase* vm, wo_extern_native_func_t call_aim_native_func, const byte_t* rt_ip, value* rt_sp, value* rt_bp)
    {
        rt_sp->type = value::valuetype::callstack;
        rt_sp->ret_ip = (uint32_t)(rt_ip - vm->env->rt_codes);
        rt_sp->bp = (uint32_t)(vm->stack_mem_begin - rt_bp);
        rt_bp = --rt_sp;
        vm->bp = vm->sp = rt_sp;

        // May be useless?
        vm->cr->set_nil();

        vm->ip = reinterpret_cast<byte_t*>(call_aim_native_func);

        wo_asure(vm->interrupt(vmbase::vm_interrupt_type::LEAVE_INTERRUPT));
        call_aim_native_func(reinterpret_cast<wo_vm>(vm), reinterpret_cast<wo_value>(rt_sp + 2), vm->tc->integer);
        wo_asure(vm->clear_interrupt(vmbase::vm_interrupt_type::LEAVE_INTERRUPT));

        wo_assert((rt_bp + 1)->type == value::valuetype::callstack);
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

        vm->ip = vm->env->rt_codes + call_aim_vm_func;

        ++vm->ip;
        if (!vm->try_invoke_jit_in_vm_run(vm->ip, rt_bp, vm->register_mem_begin, vm->cr))
        {
            wo_asure(vm->interrupt(vmbase::vm_interrupt_type::LEAVE_INTERRUPT));

            vm->run();
            wo_asure(vm->clear_interrupt(vmbase::vm_interrupt_type::LEAVE_INTERRUPT));
        }
    }

    may_constant_x86Gp get_opnum_ptr_ref(
        asmjit::X86Compiler& x86compiler,
        const byte_t*& rt_ip,
        bool dr,
        asmjit::X86Gp stack_bp,
        asmjit::X86Gp reg,
        vmbase* vmptr)
    {
        auto opnum = get_opnum_ptr(x86compiler, rt_ip, dr, stack_bp, reg, vmptr);
        // if opnum->type is ref, get it's ref

        if (!opnum.is_constant())
        {
            auto is_no_ref_label = x86compiler.newLabel();

            wo_asure(!x86compiler.cmp(asmjit::x86::byte_ptr(opnum.gp_value(), offsetof(value, type)), (uint8_t)value::valuetype::is_ref));
            wo_asure(!x86compiler.jne(is_no_ref_label));

            wo_asure(!x86compiler.mov(opnum.gp_value(), intptr_ptr(opnum.gp_value(), offsetof(value, ref))));

            wo_asure(!x86compiler.bind(is_no_ref_label));

        }
        return opnum;
    }

    asmjit::X86Gp x86_set_val(asmjit::X86Compiler& x86compiler, asmjit::X86Gp val, asmjit::X86Gp val2)
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
    asmjit::X86Gp x86_set_ref(asmjit::X86Compiler& x86compiler, asmjit::X86Gp val, asmjit::X86Gp val2)
    {
        auto skip_self_ref_label = x86compiler.newLabel();

        wo_asure(!x86compiler.cmp(val, val2));
        wo_asure(!x86compiler.je(skip_self_ref_label));
        wo_asure(!x86compiler.mov(asmjit::x86::byte_ptr(val, offsetof(value, type)), (uint8_t)value::valuetype::is_ref));
        wo_asure(!x86compiler.mov(intptr_ptr(val, offsetof(value, ref)), val2));

        wo_asure(!x86compiler.bind(skip_self_ref_label));
        return val2;
    }

    asmjit::X86Gp x86_set_nil(asmjit::X86Compiler& x86compiler, asmjit::X86Gp val)
    {
        wo_asure(!x86compiler.mov(asmjit::x86::byte_ptr(val, offsetof(value, type)), (uint8_t)value::valuetype::invalid));
        wo_asure(!x86compiler.mov(asmjit::x86::qword_ptr(val, offsetof(value, handle)), 0));
        return val;
    }

    void x86_do_calln_native_func(asmjit::X86Compiler& x86compiler,
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

    void x86_do_calln_vm_func(asmjit::X86Compiler& x86compiler,
        asmjit::X86Gp vm,
        uint32_t call_aim_vm_func,
        const byte_t* rt_ip,
        asmjit::X86Gp rt_sp,
        asmjit::X86Gp rt_bp)
    {

        auto invoke_node =
            x86compiler.call((size_t)&native_do_calln_vmfunc,
                asmjit::FuncSignatureT<void, vmbase*, uint32_t, const byte_t*, value*, value*>());

        invoke_node->setArg(0, vm);
        invoke_node->setArg(1, asmjit::Imm((size_t)call_aim_vm_func));
        invoke_node->setArg(2, asmjit::Imm((size_t)rt_ip));
        invoke_node->setArg(3, rt_sp);
        invoke_node->setArg(4, rt_bp);
    }

    jit_compiler_x86::jit_packed_function jit_compiler_x86::compile_jit(const byte_t* rt_ip, vmbase* compile_vmptr)
    {
        // Prepare asmjit;
        using namespace asmjit;

        CodeHolder code_buffer;
        code_buffer.init(get_jit_runtime().getCodeInfo());
        X86Compiler x86compiler(&code_buffer);

        // Generate function declear
        auto jit_func_node = x86compiler.addFunc(FuncSignatureT<void, vmbase*, value*, value*, value*, value*>());
        // void _jit_(vmbase*  vm , value* bp, value* reg, value* const_global);

        // 0. Get vmptr reg stack base global ptr.
        auto jit_vm_ptr = x86compiler.newUIntPtr();
        auto jit_stack_bp_ptr = x86compiler.newUIntPtr();
        auto jit_reg_ptr = x86compiler.newUIntPtr();
        auto jit_cr_ptr = x86compiler.newUIntPtr();

        x86compiler.setArg(0, jit_vm_ptr);
        x86compiler.setArg(1, jit_stack_bp_ptr);
        x86compiler.setArg(2, jit_reg_ptr);
        x86compiler.setArg(3, jit_cr_ptr);

        auto jit_stack_sp_ptr = x86compiler.newUIntPtr();

        wo_asure(!x86compiler.mov(jit_stack_sp_ptr, jit_stack_bp_ptr));                    // let sp = bp;

        byte_t opcode_dr = (byte_t)(instruct::abrt << 2);
        instruct::opcode opcode = (instruct::opcode)(opcode_dr & 0b11111100u);
        unsigned dr = opcode_dr & 0b00000011u;

        std::map<uint32_t, asmjit::Label> x86_label_table;

        for (;;)
        {
            uint32_t current_ip_byteoffset = (uint32_t)(rt_ip - compile_vmptr->env->rt_codes);

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

#define WO_JIT_ADDRESSING_N1 auto opnum1 = get_opnum_ptr(x86compiler, rt_ip, dr >> 1, jit_stack_bp_ptr, jit_reg_ptr, compile_vmptr)
#define WO_JIT_ADDRESSING_N2 auto opnum2 = get_opnum_ptr(x86compiler, rt_ip, dr &0b01, jit_stack_bp_ptr, jit_reg_ptr, compile_vmptr)
#define WO_JIT_ADDRESSING_N1_REF auto opnum1 = get_opnum_ptr_ref(x86compiler, rt_ip, dr >> 1, jit_stack_bp_ptr, jit_reg_ptr, compile_vmptr)
#define WO_JIT_ADDRESSING_N2_REF auto opnum2 = get_opnum_ptr_ref(x86compiler, rt_ip, dr &0b01, jit_stack_bp_ptr, jit_reg_ptr, compile_vmptr)

            switch (opcode)
            {
            case instruct::psh:
            {
                if (dr & 0b01)
                {
                    // WO_ADDRESSING_N1_REF;
                    // (rt_sp--)->set_val(opnum1);

                    WO_JIT_ADDRESSING_N1_REF;

                    x86_set_val(x86compiler, jit_stack_sp_ptr, opnum1.gp_value());
                    wo_asure(!x86compiler.sub(jit_stack_sp_ptr, sizeof(value)));
                }
                else
                {
                    // uint16_t psh_repeat = WO_IPVAL_MOVE_2;
                    // for (uint32_t i = 0; i < psh_repeat; i++)
                    //      (rt_sp--)->set_nil();

                    uint16_t psh_repeat = WO_IPVAL_MOVE_2;
                    for (uint32_t i = 0; i < psh_repeat; i++)
                    {
                        x86_set_nil(x86compiler,
                            jit_stack_sp_ptr);
                        wo_asure(!x86compiler.sub(jit_stack_sp_ptr, sizeof(value)));
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
                    WO_JIT_ADDRESSING_N1_REF;

                    wo_asure(!x86compiler.add(jit_stack_sp_ptr, sizeof(value)));
                    x86_set_val(x86compiler, opnum1.gp_value(), jit_stack_sp_ptr);
                }
                else
                    wo_asure(!x86compiler.add(jit_stack_sp_ptr, WO_IPVAL_MOVE_2 * sizeof(value)));
                break;
            }

            case instruct::set:
            {
                WO_JIT_ADDRESSING_N1;
                WO_JIT_ADDRESSING_N2_REF;

                x86_set_val(x86compiler, opnum1.gp_value(), opnum2.gp_value());

                break;
            }
            case instruct::mov:
            {
                WO_JIT_ADDRESSING_N1_REF;
                WO_JIT_ADDRESSING_N2_REF;

                x86_set_val(x86compiler, opnum1.gp_value(), opnum2.gp_value());

                break;
            }
            case instruct::addi:
            {
                WO_JIT_ADDRESSING_N1_REF;
                WO_JIT_ADDRESSING_N2_REF;

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
                WO_JIT_ADDRESSING_N1_REF;
                WO_JIT_ADDRESSING_N2_REF;

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
            case instruct::elti:
            {
                WO_JIT_ADDRESSING_N1_REF;
                WO_JIT_ADDRESSING_N2_REF;

                // <=

                auto x86_greater_jmp_label = x86compiler.newLabel();
                auto x86_lesseql_jmp_label = x86compiler.newLabel();

                x86compiler.mov(asmjit::x86::byte_ptr(jit_cr_ptr, offsetof(value, type)), (uint8_t)value::valuetype::integer_type);


                auto int_of_op1 = x86compiler.newInt64();
                wo_asure(!x86compiler.mov(int_of_op1, x86::qword_ptr(opnum1.gp_value(), offsetof(value, integer))));
                wo_asure(!x86compiler.cmp(int_of_op1, x86::qword_ptr(opnum2.gp_value(), offsetof(value, integer))));
                x86compiler.jg(x86_greater_jmp_label);

                wo_asure(!x86compiler.mov(x86::qword_ptr(jit_cr_ptr, offsetof(value, integer)), 1));
                x86compiler.jmp(x86_lesseql_jmp_label);
                x86compiler.bind(x86_greater_jmp_label);
                wo_asure(!x86compiler.mov(x86::qword_ptr(jit_cr_ptr, offsetof(value, integer)), 0));
                x86compiler.bind(x86_lesseql_jmp_label);

                break;
            }
            case instruct::ret:
            {
                wo_asure(x86compiler.ret());
                break;
            }
            case instruct::jmp:
            {
                uint32_t jmp_place = WO_IPVAL_MOVE_4;

                if (auto fnd = x86_label_table.find(jmp_place);
                    fnd != x86_label_table.end())
                {
                    wo_asure(!x86compiler.jmp(fnd->second));
                }
                else
                {
                    x86_label_table[jmp_place] = x86compiler.newLabel();
                    wo_asure(!x86compiler.jmp(x86_label_table[jmp_place]));
                }

                break;
            }
            case instruct::jf:
            {
                wo_asure(!x86compiler.cmp(x86::qword_ptr(jit_cr_ptr, offsetof(value, handle)), 0));

                uint32_t jmp_place = WO_IPVAL_MOVE_4;

                if (auto fnd = x86_label_table.find(jmp_place);
                    fnd != x86_label_table.end())
                {
                    wo_asure(!x86compiler.je(fnd->second));
                }
                else
                {
                    x86_label_table[jmp_place] = x86compiler.newLabel();
                    wo_asure(!x86compiler.je(x86_label_table[jmp_place]));
                }

                break;
            }
            case instruct::calln:
            {
                if (dr)
                {
                    // Call native
                    wo_extern_native_func_t call_aim_native_func = (wo_extern_native_func_t)(WO_IPVAL_MOVE_8);
                    x86_do_calln_native_func(x86compiler, jit_vm_ptr, call_aim_native_func, rt_ip, jit_stack_sp_ptr, jit_stack_bp_ptr);
                }
                else
                {
                    uint32_t call_aim_vm_func = WO_IPVAL_MOVE_4;
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
                        wo_warning("Unknown ext 0 instruct.");
                        return nullptr;
                    }
                    break;
                case 1:     // extern-opcode-page-1
                    switch ((instruct::extern_opcode_page_1)(opcode))
                    {
                    case instruct::extern_opcode_page_1::endjit:
                    {
                        // This function work end!
                        wo_asure(x86compiler.ret());
                        wo_asure(x86compiler.endFunc());
                        wo_asure(!x86compiler.finalize());

                        jit_packed_function result;

                        wo_asure(!get_jit_runtime().add(&result, &code_buffer));
                        return result;
                    }
                    default:
                        wo_warning("Unknown ext 1 instruct.");
                        return nullptr;
                    }
                    break;
                default:
                    wo_warning("Unknown extern-opcode-page.");
                    return nullptr;
                }

                break;
            }

            default:
                // Unknown opcode, return nullptr.
                wo_warning("Unknown instruct in jit-compiling.");
                return nullptr;
            }
        }

        /////////////////

         // There is something wrong happend.
        return nullptr;
    }
}

#endif

#include "wo_compiler_jit.hpp"
#include "wo_compiler_ir.hpp"

namespace wo
{

    struct asmjit_jit_context
    {
#if WO_JIT_SUPPORT_ASMJIT

#else

#endif
    };

    jit_compiler_x64::jit_compiler_x64() noexcept
        : m_asmjit_context(new asmjit_jit_context)
    {

    }
    jit_compiler_x64::~jit_compiler_x64()
    {
        if (m_asmjit_context)
            delete m_asmjit_context;
    }

    void jit_compiler_x64::analyze_function(const byte_t* codebuf, runtime_env* env) noexcept
    {

    }
    void jit_compiler_x64::analyze_jit(byte_t* codebuf, runtime_env* env) noexcept
    {

    }
}