#define _CRT_SECURE_NO_WARNINGS

#include "wo_compiler_ir.hpp"
#include "wo_lang_ast_builder.hpp"

namespace wo
{
    void program_debug_data_info::generate_debug_info_at_astnode(grammar::ast_base* ast_node, ir_compiler* compiler)
    {
        // funcdef should not genrate val..
        if (ast_node->source_file)
        {
            auto& location_list_of_file = _general_src_data_buf_a[*ast_node->source_file];

            location loc = {
                compiler->get_now_ip(),
                ast_node->row_begin_no,
                ast_node->col_begin_no,
                ast_node->row_end_no,
                ast_node->col_end_no,
                *ast_node->source_file,
            };

            location_list_of_file.push_back(loc);
            _general_src_data_buf_b[compiler->get_now_ip()] = loc;
        }
    }
    void program_debug_data_info::finalize_generate_debug_info()
    {
    }
    const program_debug_data_info::location program_debug_data_info::FAIL_LOC;
    const program_debug_data_info::location& program_debug_data_info::get_src_location_by_runtime_ip(const byte_t* rt_pos) const
    {
        const size_t FAIL_INDEX = SIZE_MAX;

        if (rt_pos == nullptr)
            return FAIL_LOC;

        size_t result = FAIL_INDEX;
        auto byte_offset = (rt_pos - runtime_codes_base) + 1;
        if (rt_pos < runtime_codes_base || rt_pos >= runtime_codes_base + runtime_codes_length)
            return FAIL_LOC;

        do
        {
            --byte_offset;
            if (auto fnd = pdd_rt_code_byte_offset_to_ir.find(byte_offset);
                fnd != pdd_rt_code_byte_offset_to_ir.end())
            {
                result = fnd->second;
                break;
            }

        } while (byte_offset > 0);

        if (result == FAIL_INDEX)
            return FAIL_LOC;

        while (_general_src_data_buf_b.find(result) == _general_src_data_buf_b.end())
        {
            if (!result)
            {
                return FAIL_LOC;
            }
            result--;
        }

        return _general_src_data_buf_b.at(result);
    }
    size_t program_debug_data_info::get_ip_by_src_location(const std::wstring& src_name, size_t rowno, bool strict)const
    {
        const size_t FAIL_INDEX = SIZE_MAX;

        auto fnd = _general_src_data_buf_a.find(src_name);
        if (fnd == _general_src_data_buf_a.end())
            return FAIL_INDEX;

        size_t result = FAIL_INDEX;
        for (auto& locinfo : fnd->second)
        {
            if (strict)
            {
                if (locinfo.begin_row_no == rowno)
                {
                    if (locinfo.ip < result)
                        result = locinfo.ip;
                }
            }
            else if (locinfo.begin_row_no >= rowno)
            {
                if (locinfo.ip < result)
                    result = locinfo.ip;
            }
        }
        return result;
    }
    size_t program_debug_data_info::get_ip_by_runtime_ip(const byte_t* rt_pos) const
    {
        const size_t FAIL_INDEX = SIZE_MAX;
        static location     FAIL_LOC;

        size_t result = FAIL_INDEX;
        auto byte_offset = (rt_pos - runtime_codes_base) + 1;
        if (rt_pos < runtime_codes_base || rt_pos >= runtime_codes_base + runtime_codes_length)
            return FAIL_INDEX;
        do
        {
            --byte_offset;
            if (auto fnd = pdd_rt_code_byte_offset_to_ir.find(byte_offset);
                fnd != pdd_rt_code_byte_offset_to_ir.end())
            {
                result = fnd->second;
                break;
            }
        } while (byte_offset > 0);

        return result;
    }
    size_t program_debug_data_info::get_runtime_ip_by_ip(size_t ip) const
    {
        for (auto& [rtip, cpip] : pdd_rt_code_byte_offset_to_ir)
        {
            if (cpip >= ip)
                return rtip;
        }

        return SIZE_MAX;
    }

    void program_debug_data_info::generate_func_begin(ast::ast_value_function_define* funcdef, ir_compiler* compiler)
    {
        _function_ip_data_buf[funcdef->get_ir_func_signature_tag()].ir_begin = compiler->get_now_ip();
        generate_debug_info_at_astnode(funcdef, compiler);
    }
    void program_debug_data_info::generate_func_end(ast::ast_value_function_define* funcdef, size_t tmpreg_count, ir_compiler* compiler)
    {
        _function_ip_data_buf[funcdef->get_ir_func_signature_tag()].ir_end = compiler->get_now_ip();
        _function_ip_data_buf[funcdef->get_ir_func_signature_tag()].in_stack_reg_count = tmpreg_count;
    }
    void program_debug_data_info::add_func_variable(ast::ast_value_function_define* funcdef, const std::wstring& varname, size_t rowno, wo_integer_t loc)
    {
        _function_ip_data_buf[funcdef->get_ir_func_signature_tag()].add_variable_define(varname, rowno, loc);
    }

    std::string program_debug_data_info::get_current_func_signature_by_runtime_ip(const byte_t* rt_pos) const
    {
        auto compile_ip = get_ip_by_runtime_ip(rt_pos);
        for (auto& [func_signature, iplocs] : _function_ip_data_buf)
        {
            if (iplocs.ir_begin <= compile_ip && compile_ip <= iplocs.ir_end)
                return func_signature;
        }
        return "__unknown_func__at_" +
            [rt_pos]()->std::string {
            char ptrr[20] = {};
            sprintf(ptrr, "0x%p", rt_pos);
            return ptrr;
        }();
    }
}

WO_API wo_api rslib_std_ir_create_compiler(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_gchandle(vm, new wo::ir_compiler, nullptr, [](void* ptr) {
        delete (wo::ir_compiler*)ptr;
        });
}

WO_API wo_api rslib_std_ir_command_nop(wo_vm vm, wo_value args, size_t argc)
{
    wo::ir_compiler* ircompiler = (wo::ir_compiler*)wo_pointer(args + 0);
    ircompiler->nop();

    return wo_ret_void(vm);
}

constexpr size_t OPNUM_BYTE_SIZE = 128;

template<typename OpnumT, typename ... ArgTs>
const wo::opnum::opnumbase* _construst(char* dat, ArgTs&& ... args)
{
    static_assert(sizeof(OpnumT) <= OPNUM_BYTE_SIZE);
    return new(dat)OpnumT(args...);
}

template<size_t opnumid, size_t except_type = 0>
const wo::opnum::opnumbase& _get_opnum(wo_value val)
{
    static thread_local char __opnum_buffer[OPNUM_BYTE_SIZE];
    static thread_local const wo::opnum::opnumbase* __opnum_instance = nullptr;

    if (__opnum_instance)
    {
        __opnum_instance->~opnumbase();
        __opnum_instance = nullptr;
    }

    if (wo_valuetype(val) != WO_STRUCT_TYPE)
    {
        wo_fail(WO_FAIL_TYPE_FAIL, "Opnum should be a struct.");
        return *(__opnum_instance = _construst<wo::opnum::reg>(__opnum_buffer, wo::opnum::reg::ni));
    }
    else
    {
        using namespace wo::opnum;
        if (except_type != 0 && except_type != wo_int(wo_struct_get(val, 0)))
        {
            wo_fail(WO_FAIL_TYPE_FAIL, "Unexcepted opnum type.");
            return *(__opnum_instance = _construst<wo::opnum::reg>(__opnum_buffer, wo::opnum::reg::ni));
        }

        switch (wo_int(wo_struct_get(val, 0)))
        {
        case 1: // register(int),
            return *(__opnum_instance = _construst<reg>(__opnum_buffer, (uint8_t)wo_int(wo_struct_get(val, 1))));
        case 2: // stackoffset(int),
            return *(__opnum_instance = _construst<reg>(__opnum_buffer, reg::bp_offset((uint8_t)wo_int(wo_struct_get(val, 1)))));
        case 3: // global(int),
            return *(__opnum_instance = _construst<global>(__opnum_buffer, (int32_t)wo_int(wo_struct_get(val, 1))));
        case 4: // constant_int(int),
            return *(__opnum_instance = _construst<imm<wo_integer_t>>(__opnum_buffer, wo_int(wo_struct_get(val, 1))));
        case 5: // constant_handle(handle),
            return *(__opnum_instance = _construst<imm<void*>>(__opnum_buffer, (void*)wo_handle(wo_struct_get(val, 1))));
        case 6: // constant_string(string),
            return *(__opnum_instance = _construst<imm<std::string>>(__opnum_buffer, wo_string(wo_struct_get(val, 1))));
        case 7: // label(string),
            return *(__opnum_instance = _construst<tag>(__opnum_buffer, wo_string(wo_struct_get(val, 1))));
        default:
            wo_fail(WO_FAIL_TYPE_FAIL, "Unknown opnum type.");
            return *(__opnum_instance = _construst<wo::opnum::reg>(__opnum_buffer, wo::opnum::reg::ni));
        }
    }
}

template<size_t opnumid>
const wo::opnum::tag& _get_label(wo_value val)
{
    const static wo::opnum::tag __unknown_label("error_unknown_label");
    auto* opnum_tag = dynamic_cast<const wo::opnum::tag*>(&_get_opnum<opnumid, 7>(val));
    if (opnum_tag == nullptr)
        return __unknown_label;
    return *opnum_tag;
}

#define WO_COMMAND_WITH_2_OPNUM(COMMAND) \
WO_API wo_api rslib_std_ir_command_##COMMAND(wo_vm vm, wo_value args, size_t argc)\
{\
    wo::ir_compiler* ircompiler = (wo::ir_compiler*)wo_pointer(args + 0);\
    ircompiler->COMMAND(_get_opnum<1>(args + 1), _get_opnum<2>(args + 2));\
    return wo_ret_void(vm);\
}

#define WO_COMMAND_WITH_1_OPNUM(COMMAND) \
WO_API wo_api rslib_std_ir_command_##COMMAND(wo_vm vm, wo_value args, size_t argc)\
{\
    wo::ir_compiler* ircompiler = (wo::ir_compiler*)wo_pointer(args + 0);\
    ircompiler->COMMAND(_get_opnum<1>(args + 1));\
    return wo_ret_void(vm);\
}

#define WO_COMMAND_WITH_0_OPNUM(COMMAND) \
WO_API wo_api rslib_std_ir_command_##COMMAND(wo_vm vm, wo_value args, size_t argc)\
{\
    wo::ir_compiler* ircompiler = (wo::ir_compiler*)wo_pointer(args + 0);\
    ircompiler->COMMAND();\
    return wo_ret_void(vm);\
}

WO_COMMAND_WITH_2_OPNUM(mov);

WO_COMMAND_WITH_2_OPNUM(addi);
WO_COMMAND_WITH_2_OPNUM(subi);
WO_COMMAND_WITH_2_OPNUM(muli);
WO_COMMAND_WITH_2_OPNUM(divi);
WO_COMMAND_WITH_2_OPNUM(modi);

WO_COMMAND_WITH_2_OPNUM(addr);
WO_COMMAND_WITH_2_OPNUM(subr);
WO_COMMAND_WITH_2_OPNUM(mulr);
WO_COMMAND_WITH_2_OPNUM(divr);
WO_COMMAND_WITH_2_OPNUM(modr);

WO_COMMAND_WITH_2_OPNUM(addh);
WO_COMMAND_WITH_2_OPNUM(subh);

WO_COMMAND_WITH_2_OPNUM(adds);

WO_COMMAND_WITH_1_OPNUM(psh);
WO_API wo_api rslib_std_ir_command_pshn(wo_vm vm, wo_value args, size_t argc)
{
    wo::ir_compiler* ircompiler = (wo::ir_compiler*)wo_pointer(args + 0);
    ircompiler->pshn((uint16_t)wo_int(args + 1));
    return wo_ret_void(vm);
}

WO_COMMAND_WITH_1_OPNUM(pop);
WO_API wo_api rslib_std_ir_command_popn(wo_vm vm, wo_value args, size_t argc)
{
    wo::ir_compiler* ircompiler = (wo::ir_compiler*)wo_pointer(args + 0);
    ircompiler->pop((uint16_t)wo_int(args + 1));
    return wo_ret_void(vm);
}

WO_COMMAND_WITH_2_OPNUM(lds);

WO_COMMAND_WITH_2_OPNUM(equb);
WO_COMMAND_WITH_2_OPNUM(nequb);

WO_COMMAND_WITH_2_OPNUM(lti);
WO_COMMAND_WITH_2_OPNUM(gti);
WO_COMMAND_WITH_2_OPNUM(elti);
WO_COMMAND_WITH_2_OPNUM(egti);

WO_COMMAND_WITH_2_OPNUM(land);
WO_COMMAND_WITH_2_OPNUM(lor);
WO_COMMAND_WITH_2_OPNUM(lmov);

WO_COMMAND_WITH_2_OPNUM(ltx);
WO_COMMAND_WITH_2_OPNUM(gtx);
WO_COMMAND_WITH_2_OPNUM(eltx);
WO_COMMAND_WITH_2_OPNUM(egtx);

WO_COMMAND_WITH_2_OPNUM(ltr);
WO_COMMAND_WITH_2_OPNUM(gtr);
WO_COMMAND_WITH_2_OPNUM(eltr);
WO_COMMAND_WITH_2_OPNUM(egtr);

WO_COMMAND_WITH_1_OPNUM(call);

WO_COMMAND_WITH_0_OPNUM(ret);
WO_API wo_api rslib_std_ir_command_retn(wo_vm vm, wo_value args, size_t argc)
{
    wo::ir_compiler* ircompiler = (wo::ir_compiler*)wo_pointer(args + 0);
    ircompiler->ret((uint16_t)wo_int(args + 1));
    return wo_ret_void(vm);
}

WO_API wo_api rslib_std_ir_command_jt(wo_vm vm, wo_value args, size_t argc)
{
    wo::ir_compiler* ircompiler = (wo::ir_compiler*)wo_pointer(args + 0);
    ircompiler->jt(_get_label<1>(args + 1));
    return wo_ret_void(vm);
}
WO_API wo_api rslib_std_ir_command_jf(wo_vm vm, wo_value args, size_t argc)
{
    wo::ir_compiler* ircompiler = (wo::ir_compiler*)wo_pointer(args + 0);
    ircompiler->jf(_get_label<1>(args + 1));
    return wo_ret_void(vm);
}
WO_API wo_api rslib_std_ir_command_jmp(wo_vm vm, wo_value args, size_t argc)
{
    wo::ir_compiler* ircompiler = (wo::ir_compiler*)wo_pointer(args + 0);
    ircompiler->jmp(_get_label<1>(args + 1));
    return wo_ret_void(vm);
}

const char* wo_stdlib_ir_src_path = u8"woo/ir.wo";
const char* wo_stdlib_ir_src_data = R"(
namespace std
{
    public using ircompiler = gchandle
    {
        alias irc = ircompiler;

        public union opnum {
            register(int),
            stackoffset(int),
            global(int),
            constant_int(int),
            constant_handle(handle),
            constant_string(string),
            label(string),
        }

        using label = struct {
            padding: int, // must be 7.
            id: string,
        }
        {
            public func create_with_id(id: string)
            {
                return label{padding = 7, id = id};
            }
            public func to_opnum(self: label)
            {
                return opnum::label(self.id);
            }
        }

        extern("rslib_std_ir_create_compiler")
        public func create()=> irc;

        extern("rslib_std_ir_command_nop")
        public func nop(self: irc)=> void;

        extern("rslib_std_ir_command_mov")
        public func mov(self: irc, dst: opnum, src: opnum)=> void;

        extern("rslib_std_ir_command_addi")
        public func addi(self: irc, dst: opnum, src: opnum)=> void;
        extern("rslib_std_ir_command_subi")
        public func subi(self: irc, dst: opnum, src: opnum)=> void;
        extern("rslib_std_ir_command_muli")
        public func muli(self: irc, dst: opnum, src: opnum)=> void;
        extern("rslib_std_ir_command_divi")
        public func divi(self: irc, dst: opnum, src: opnum)=> void;
        extern("rslib_std_ir_command_modi")
        public func modi(self: irc, dst: opnum, src: opnum)=> void;

        extern("rslib_std_ir_command_addr")
        public func addr(self: irc, dst: opnum, src: opnum)=> void;
        extern("rslib_std_ir_command_subr")
        public func subr(self: irc, dst: opnum, src: opnum)=> void;
        extern("rslib_std_ir_command_mulr")
        public func mulr(self: irc, dst: opnum, src: opnum)=> void;
        extern("rslib_std_ir_command_divr")
        public func divr(self: irc, dst: opnum, src: opnum)=> void;
        extern("rslib_std_ir_command_modr")
        public func modr(self: irc, dst: opnum, src: opnum)=> void;

        extern("rslib_std_ir_command_addh")
        public func addh(self: irc, dst: opnum, src: opnum)=> void;
        extern("rslib_std_ir_command_subh")
        public func subh(self: irc, dst: opnum, src: opnum)=> void;
    
        extern("rslib_std_ir_command_adds")
        public func adds(self: irc, dst: opnum, src: opnum)=> void;

        extern("rslib_std_ir_command_psh")
        public func psh(self: irc, op: opnum)=> void;
        extern("rslib_std_ir_command_pshn")
        public func pshn(self: irc, count: int)=> void;

        extern("rslib_std_ir_command_pop")
        public func pop(self: irc, op: opnum)=> void;
        extern("rslib_std_ir_command_popn")
        public func popn(self: irc, count: int)=> void;

        extern("rslib_std_ir_command_lds")
        public func lds(self: irc, dst: opnum, src: opnum)=> void;

        extern("rslib_std_ir_command_equb")
        public func equb(self: irc, a: opnum, b: opnum)=> void;
        extern("rslib_std_ir_command_nequb")
        public func nequb(self: irc, a: opnum, b: opnum)=> void;

        extern("rslib_std_ir_command_lti")
        public func lti(self: irc, a: opnum, b: opnum)=> void;
        extern("rslib_std_ir_command_gti")
        public func gti(self: irc, a: opnum, b: opnum)=> void;
        extern("rslib_std_ir_command_elti")
        public func elti(self: irc, a: opnum, b: opnum)=> void;
        extern("rslib_std_ir_command_egti")
        public func egti(self: irc, a: opnum, b: opnum)=> void;

        extern("rslib_std_ir_command_land")
        public func land(self: irc, a: opnum, b: opnum)=> void;
        extern("rslib_std_ir_command_lor")
        public func lor(self: irc, a: opnum, b: opnum)=> void;
        extern("rslib_std_ir_command_lmov")
        public func lmov(self: irc, a: opnum, b: opnum)=> void;

        extern("rslib_std_ir_command_ltx")
        public func ltx(self: irc, a: opnum, b: opnum)=> void;
        extern("rslib_std_ir_command_gtx")
        public func gtx(self: irc, a: opnum, b: opnum)=> void;
        extern("rslib_std_ir_command_eltx")
        public func eltx(self: irc, a: opnum, b: opnum)=> void;
        extern("rslib_std_ir_command_egtx")
        public func egtx(self: irc, a: opnum, b: opnum)=> void;

        extern("rslib_std_ir_command_ltr")
        public func ltr(self: irc, a: opnum, b: opnum)=> void;
        extern("rslib_std_ir_command_gtr")
        public func gtr(self: irc, a: opnum, b: opnum)=> void;
        extern("rslib_std_ir_command_eltr")
        public func eltr(self: irc, a: opnum, b: opnum)=> void;
        extern("rslib_std_ir_command_egtr")
        public func egtr(self: irc, a: opnum, b: opnum)=> void;

        extern("rslib_std_ir_command_call")
        public func call(self: irc, aim)=> void
            where aim is opnum || aim is label;
        extern("rslib_std_ir_command_ret")
        public func ret(self: irc)=> void;
        extern("rslib_std_ir_command_retn")
        public func retn(self: irc, popcount: int)=> void;

        extern("rslib_std_ir_command_jt")
        public func jt(self: irc, aim: label)=> void;
        extern("rslib_std_ir_command_jf")
        public func jf(self: irc, aim: label)=> void;
        extern("rslib_std_ir_command_jmp")
        public func jmp(self: irc, aim: label)=> void;
    }
}
)";