#include "wo_afx.hpp"

namespace wo
{
#ifndef WO_DISABLE_COMPILER
    void program_debug_data_info::generate_debug_info_at_astnode(ast::AstBase* ast_node, ir_compiler* compiler)
    {
        // funcdef should not genrate val..
        if (ast_node->source_location.source_file)
        {
            bool unbreakable = false;
            switch (ast_node->node_type)
            {
            case ast::AstBase::AST_VALUE_FUNCTION:
            case ast::AstBase::AST_LIST:
            case ast::AstBase::AST_NAMESPACE:
            case ast::AstBase::AST_SCOPE:
            case ast::AstBase::AST_IF:
            case ast::AstBase::AST_WHILE:
            case ast::AstBase::AST_FOR:
            case ast::AstBase::AST_FOREACH:
            case ast::AstBase::AST_MATCH:
                unbreakable = true;
                break;
            default:
                break;
            }

            auto& location_list_of_file = _general_src_data_buf_a[*ast_node->source_location.source_file];

            location loc = {
                compiler->get_now_ip(),
                ast_node->source_location.begin_at.row,
                ast_node->source_location.begin_at.column,
                ast_node->source_location.end_at.row,
                ast_node->source_location.end_at.column,
                *ast_node->source_location.source_file,
                unbreakable
            };

            location_list_of_file.push_back(loc);
            _general_src_data_buf_b[compiler->get_now_ip()] = loc;
        }
    }
    void program_debug_data_info::finalize_generate_debug_info()
    {
    }
    void program_debug_data_info::generate_func_begin(const std::string& function_name, ast::AstBase* ast_node, ir_compiler* compiler)
    {
        _function_ip_data_buf[function_name].ir_begin = compiler->get_now_ip();
        generate_debug_info_at_astnode(ast_node, compiler);
    }
    void program_debug_data_info::generate_func_end(const std::string& function_name, size_t tmpreg_count, ir_compiler* compiler)
    {
        _function_ip_data_buf[function_name].ir_end = compiler->get_now_ip();
    }
    void program_debug_data_info::add_func_variable(const std::string& function_name, const std::wstring& varname, size_t rowno, wo_integer_t loc)
    {
        _function_ip_data_buf[function_name].add_variable_define(varname, rowno, loc);
    }
    void program_debug_data_info::update_func_variable(const std::string& function_name, wo_integer_t offset)
    {
        for (auto& [_useless, infors] : _function_ip_data_buf[function_name].variables)
        {
            for (auto& info : infors)
            {
                if (info.bp_offset <= 0)
                    info.bp_offset += offset;
            }
        }
    }


    int32_t ir_compiler::update_all_temp_regist_to_stack(
        BytecodeGenerateContext* ctx, size_t begin) noexcept
    {
        std::map<uint32_t, int32_t> tr_regist_mapping;

        for (size_t i = begin; i < get_now_ip(); i++)
        {
            // ir_command_buffer problem..
            auto& ircmbuf = ir_command_buffer[i];
            if (ircmbuf.opcode != instruct::calln) // calln will not use opnum but do reptr_cast, dynamic_cast is dangerous
            {
                if (auto* op1 = dynamic_cast<const opnum::temporary*>(ircmbuf.op1))
                {
                    if (tr_regist_mapping.find(op1->m_id) == tr_regist_mapping.end())
                    {
                        // is temp reg 
                        size_t stack_idx = tr_regist_mapping.size();
                        tr_regist_mapping[op1->m_id] = (int32_t)stack_idx;
                    }
                }
                if (auto* op2 = dynamic_cast<const opnum::temporary*>(ircmbuf.op2))
                {
                    if (tr_regist_mapping.find(op2->m_id) == tr_regist_mapping.end())
                    {
                        // is temp reg 
                        size_t stack_idx = tr_regist_mapping.size();
                        tr_regist_mapping[op2->m_id] = (int32_t)stack_idx;
                    }
                }
                switch (ircmbuf.opcode & 0b11111100)
                {
                case instruct::opcode::sidarr:
                case instruct::opcode::sidmap:
                case instruct::opcode::siddict:
                    if (ircmbuf.opinteger1 < 0
                        && tr_regist_mapping.find(
                            (uint32_t)(-(ircmbuf.opinteger1 + 1))) == tr_regist_mapping.end())
                    {
                        // is temp reg 
                        size_t stack_idx = tr_regist_mapping.size();
                        tr_regist_mapping[(uint32_t)(-(ircmbuf.opinteger1 + 1))] = (int32_t)stack_idx;
                    }
                    break;
                default:
                    break;
                }
            }
        }
        int32_t maxim_offset = (int32_t)tr_regist_mapping.size();

        // ATTENTION: DO NOT USE ircmbuf AFTER THIS LINE!!!
        //            WILL INSERT SOME COMMAND BEFORE ir_command_buffer[i],
        //            ircmbuf WILL POINT TO AN INVALID PLACE.

        for (size_t i = begin; i < get_now_ip(); i++)
        {
            auto* opnum1 = ir_command_buffer[i].op1;
            auto* opnum2 = ir_command_buffer[i].op2;

            size_t skip_line = 0;
            if (ir_command_buffer[i].opcode == instruct::lds
                || ir_command_buffer[i].opcode == instruct::sts)
            {
                auto* imm_opnum_stx_offset = dynamic_cast<const opnum::immbase*>(opnum2);
                if (imm_opnum_stx_offset)
                {
                    wo_assert(imm_opnum_stx_offset->type() == wo::value::valuetype::integer_type);
                    auto stx_offset = imm_opnum_stx_offset->constant_value.integer;
                    if (stx_offset <= 0)
                    {
                        ir_command_buffer[i].op2 =
                            ctx->opnum_imm_int(stx_offset - maxim_offset);
                    }
                }
                else
                {
                    // Here only get arg from stack. so here nothing todo.
                }
            }
            if (ir_command_buffer[i].opcode != instruct::calln)
            {
                std::optional<int32_t> stack_offset = std::nullopt;
                if (auto* op1 = dynamic_cast<const opnum::temporary*>(opnum1))
                {
                    stack_offset = -(int32_t)tr_regist_mapping[op1->m_id];
                }
                else if (auto* op1 = dynamic_cast<const opnum::reg*>(opnum1);
                    op1 != nullptr && op1->is_bp_offset() && op1->get_bp_offset() <= 0)
                {
                    stack_offset = (int32_t)op1->get_bp_offset() - (int32_t)maxim_offset;
                }

                if (stack_offset.has_value())
                {
                    auto stack_offset_val = stack_offset.value();
                    if (stack_offset_val >= -64)
                        opnum1 = ctx->opnum_stack_offset(stack_offset_val);
                    else
                    {
                        auto* reg_r0 = ctx->opnum_spreg(opnum::reg::r0);
                        auto* imm_offset = _check_and_add_const(ctx->opnum_imm_int(stack_offset_val));

                        // out of bt_offset range, make lds ldsr
                        ir_command_buffer.insert(ir_command_buffer.begin() + i,
                            ir_command{ instruct::lds, reg_r0, imm_offset });         // lds r0, imm(real_offset)
                        opnum1 = reg_r0;
                        ++i;

                        if (ir_command_buffer[i].opcode != instruct::call)
                        {
                            ir_command_buffer.insert(ir_command_buffer.begin() + i + 1,
                                ir_command{ instruct::sts, reg_r0, imm_offset });         // sts r0, imm(real_offset)

                            ++skip_line;
                        }
                    }
                    ir_command_buffer[i].op1 = opnum1;
                }
            }
            /////////////////////////////////////////////////////////////////////////////////////////////////
            do
            {
                std::optional<int32_t> stack_offset = std::nullopt;

                if (auto* op2 = dynamic_cast<const opnum::temporary*>(opnum2))
                {
                    stack_offset = -(int32_t)tr_regist_mapping[op2->m_id];
                }
                else if (auto* op2 = dynamic_cast<const opnum::reg*>(opnum2);
                    op2 != nullptr && op2->is_bp_offset() && op2->get_bp_offset() <= 0)
                {
                    stack_offset = (int32_t)op2->get_bp_offset() - (int32_t)maxim_offset;
                }

                if (stack_offset.has_value())
                {
                    auto stack_offset_val = stack_offset.value();
                    if (stack_offset_val >= -64)
                        opnum2 = ctx->opnum_stack_offset(stack_offset_val);
                    else
                    {
                        wo_assert(ir_command_buffer[i].opcode != instruct::call);

                        auto* reg_r1 = ctx->opnum_spreg(opnum::reg::r1);
                        auto* imm_offset = _check_and_add_const(ctx->opnum_imm_int(stack_offset_val));

                        // out of bt_offset range, make lds ldsr
                        ir_command_buffer.insert(ir_command_buffer.begin() + i,
                            ir_command{ instruct::lds, reg_r1, imm_offset });         // lds r1, imm(real_offset)
                        opnum2 = reg_r1;
                        ++i;

                        // No opcode will update opnum2, so here no need for update.
                    }
                    ir_command_buffer[i].op2 = opnum2;
                }
            } while (0);

            switch (ir_command_buffer[i].opcode & 0b11111100)
            {
            case instruct::opcode::sidarr:
            case instruct::opcode::sidmap:
            case instruct::opcode::siddict:
            {
                std::optional<int32_t> stack_offset = std::nullopt;
                if (ir_command_buffer[i].opinteger1 < 0)
                {
                    stack_offset = -(int32_t)tr_regist_mapping[(uint32_t)(-(ir_command_buffer[i].opinteger1 + 1))];
                }
                else
                {
                    opnum::reg op3((uint8_t)ir_command_buffer[i].opinteger1);
                    if (op3.is_bp_offset() && op3.get_bp_offset() <= 0)
                    {
                        stack_offset = (int32_t)op3.get_bp_offset() - (int32_t)maxim_offset;
                    }
                }

                if (stack_offset.has_value())
                {
                    auto stack_offset_val = stack_offset.value();
                    if (stack_offset_val >= -64)
                    {
                        opnum::reg op3(opnum::reg::bp_offset(stack_offset_val));
                        ir_command_buffer[i].opinteger1 = (int32_t)op3.id;
                    }
                    else
                    {
                        auto* reg_r2 = ctx->opnum_spreg(opnum::reg::r2);
                        auto* imm_offset = _check_and_add_const(ctx->opnum_imm_int(stack_offset_val));

                        // out of bt_offset range, make lds ldsr
                        ir_command_buffer.insert(ir_command_buffer.begin() + i,
                            ir_command{ instruct::lds, reg_r2, imm_offset });         // lds r2, imm(real_offset)

                        opnum::reg op3(opnum::reg::r2);
                        ++i;

                        // No opcode will update opnum3, so here no need for update.
                        ir_command_buffer[i].opinteger1 = (int32_t)op3.id;
                    }
                }


                break;
            }
            default:
                break;
            }

            i += skip_line;
        }
        return (int32_t)tr_regist_mapping.size();
    }
#endif
    const program_debug_data_info::location program_debug_data_info::FAIL_LOC = {};
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
    std::vector<size_t> program_debug_data_info::get_ip_by_src_location(const std::wstring& src_name, size_t rowno, bool strict, bool ignore_unbreakable)const
    {
        auto fnd = _general_src_data_buf_a.find(src_name);
        if (fnd == _general_src_data_buf_a.end())
            return {};

        std::vector<size_t> result = {};
        for (auto& locinfo : fnd->second)
        {
            if (ignore_unbreakable || locinfo.unbreakable == false)
            {
                if (strict)
                {
                    if (locinfo.begin_row_no == rowno)
                    {
                        result.push_back(locinfo.ip);
                    }
                }
                else if (locinfo.begin_row_no <= rowno && locinfo.end_row_no >= rowno)
                {
                    result.push_back(locinfo.ip);
                }
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

    std::string program_debug_data_info::get_current_func_signature_by_runtime_ip(const byte_t* rt_pos) const
    {
        auto compile_ip = get_ip_by_runtime_ip(rt_pos);
        for (auto& [func_signature, iplocs] : _function_ip_data_buf)
        {
            if (iplocs.ir_begin <= compile_ip && compile_ip <= iplocs.ir_end)
                return func_signature;
        }
        return "<unknown function " +
            [rt_pos]()->std::string {

            char ptrr[sizeof(rt_pos) * 2 + 4] = {};
            sprintf(ptrr, "0x%p>", rt_pos);
            return ptrr;

            }();
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    runtime_env::runtime_env()
        : constant_and_global_storage(nullptr)
        , constant_and_global_value_takeplace_count(0)
        , constant_value_count(0)
        , real_register_count(0)
        , rt_code_len(0)
        , rt_codes(nullptr)
        , _running_on_vm_count(0)
        , _created_destructable_instance_count(0)
    {
    }
    static void cancel_nogc_mark_for_value(const value& val)
    {
        switch (val.type)
        {
        case value::valuetype::struct_type:
            if (val.structs != nullptr)
            {
                // Un-completed struct might be created when loading from binary.
                // unit might be nullptr.

                for (uint16_t idx = 0; idx < val.structs->m_count; ++idx)
                    cancel_nogc_mark_for_value(val.structs->m_values[idx]);
            }
            [[fallthrough]];
        case value::valuetype::string_type:
        {
            gcbase::unit_attrib cancel_nogc;
            cancel_nogc.m_gc_age = 0;
            cancel_nogc.m_marked = (uint8_t)gcbase::gcmarkcolor::full_mark;
            cancel_nogc.m_alloc_mask = 0;
            cancel_nogc.m_nogc = 0;

            gcbase::unit_attrib* attrib;

            // NOTE: Constant gcunit is nogc-unit, its safe here to `get_gcunit_and_attrib_ref`.
            auto* unit = val.get_gcunit_and_attrib_ref(&attrib);
            if (unit != nullptr)
            {
                // Un-completed string might be created when loading from binary.
                // unit might be nullptr.
                wo_assert(attrib->m_nogc != 0);
                attrib->m_attr = cancel_nogc.m_attr;
            }
            break;
        }
        default:
            wo_assert(!val.is_gcunit());
            break;
        }
    }

    runtime_env::~runtime_env()
    {
        if (wo::config::ENABLE_JUST_IN_TIME)
            free_jit(this);

        for (size_t ci = 0; ci < constant_value_count; ++ci)
            cancel_nogc_mark_for_value(constant_and_global_storage[ci]);

        if (constant_and_global_storage)
            free(constant_and_global_storage);

        if (rt_codes)
            free((byte_t*)rt_codes);
    }

    std::tuple<void*, size_t> runtime_env::create_env_binary(bool savepdi) noexcept
    {
        std::vector<wo::byte_t> binary_buffer;
        auto write_buffer_to_buffer =
            [&binary_buffer](const void* written_data, size_t written_length, size_t allign)
            {
                const size_t write_begin_place = binary_buffer.size();

                wo_assert(write_begin_place % allign == 0);

                binary_buffer.resize(write_begin_place + written_length);

                memcpy(binary_buffer.data() + write_begin_place, written_data, written_length);
                return binary_buffer.size();
            };

        auto write_binary_to_buffer =
            [&write_buffer_to_buffer](const auto& d, size_t size_for_assert)
            {
                const wo::byte_t* written_data = std::launder(reinterpret_cast<const wo::byte_t*>(&d));
                const size_t written_length = sizeof(d);

                wo_assert(written_length == size_for_assert);

                return write_buffer_to_buffer(written_data, written_length, sizeof(d) > 8 ? 8 : sizeof(d));
            };

        class _string_pool_t
        {
            std::vector<char> _string_pool_buffer;
            std::unordered_map<std::string, size_t> _string_pool_map;
        public:
            size_t insert(const char* str, size_t len)
            {
                std::string strkey(str, len);
                auto fnd = _string_pool_map.find(strkey);
                if (fnd != _string_pool_map.end())
                    return fnd->second;

                size_t insert_place = _string_pool_buffer.size();
                _string_pool_buffer.insert(_string_pool_buffer.end(), str, str + len);
                _string_pool_map[strkey] = insert_place;
                return insert_place;
            }
            const std::vector<char>& get_pool() const
            {
                return _string_pool_buffer;
            }
        };
        _string_pool_t constant_string_pool;

        std::function<void(const wo::value&)> write_value_to_buffer;
        auto write_constant_str_to_buffer =
            [&write_binary_to_buffer, &constant_string_pool](const char* str, size_t len)
            {
                write_binary_to_buffer((uint32_t)constant_string_pool.insert(str, len), 4);
                write_binary_to_buffer((uint32_t)len, 4);
            };
        auto write_constant_struct_to_buffer =
            [&write_binary_to_buffer, &write_value_to_buffer](const struct_t* struct_instance)
            {
                write_binary_to_buffer((uint64_t)struct_instance->m_count, 8);
                for (uint16_t idx = 0; idx < struct_instance->m_count; ++idx)
                {
                    write_value_to_buffer(struct_instance->m_values[idx]);
                }
            };
        write_value_to_buffer =
            [this,
            &write_binary_to_buffer,
            &write_constant_str_to_buffer,
            &write_constant_struct_to_buffer](const wo::value& val)
            {
                write_binary_to_buffer((uint64_t)val.type_space, 8);

                switch (val.type)
                {
                case wo::value::valuetype::string_type:
                    // Record for string
                    write_constant_str_to_buffer(
                        val.string->c_str(), val.string->size());
                    break;
                case wo::value::valuetype::struct_type:
                    // Record for struct
                    write_constant_struct_to_buffer(
                        val.structs);
                    break;
                default:
                    // Record for value
                    wo_assert(val.type == wo::value::valuetype::integer_type
                        || val.type == wo::value::valuetype::real_type
                        || val.type == wo::value::valuetype::handle_type
                        || val.type == wo::value::valuetype::bool_type
                        || val.type == wo::value::valuetype::invalid);
                    write_binary_to_buffer((uint64_t)val.value_space, 8);
                    break;
                }
            };

        // 1.1 (+0) Magic number(0x3001A26B look like WOOLANG B)
        write_binary_to_buffer((uint32_t)0x3001A26B, 4);
        write_binary_to_buffer((uint32_t)0x00000000, 4);
        // 1.2 (+8) Version info
        write_binary_to_buffer((uint64_t)wo_version_int(), 8);
        // 1.3 (+16) Source CRC64(TODO)

        // 2.1 (+16 + 2N * 8) Global space size * sizeof(Value)
        write_binary_to_buffer(
            (uint64_t)(this->constant_and_global_value_takeplace_count
                - this->constant_value_count), 8);

        // 2.2 Default register size
        write_binary_to_buffer(
            (uint64_t)this->real_register_count, 8);

        // 2.3 Constant data
        write_binary_to_buffer(
            (uint64_t)this->constant_value_count, 8);
        for (size_t ci = 0; ci < this->constant_value_count; ++ci)
        {
            auto& constant_value = this->constant_and_global_storage[ci];

            write_value_to_buffer(constant_value);

            if (constant_value.type == wo::value::valuetype::handle_type)
            {
                // Check if val is function address from native?
                auto fnd = this->extern_native_functions.find((intptr_t)constant_value.handle);
                if (fnd != this->extern_native_functions.end())
                {
                    wo_assert(!fnd->second.function_name.empty());
                    fnd->second.constant_offset_in_binary.push_back(ci);
                }
            }
        }

        // 3.1 Code data
        //  3.1.1 Code data length
        size_t padding_length_for_rt_coding = (8ull - (this->rt_code_len % 8ull)) % 8ull;
        write_binary_to_buffer((uint64_t)(this->rt_code_len + padding_length_for_rt_coding), 8);
        write_buffer_to_buffer(this->rt_codes, this->rt_code_len, 1);
        write_buffer_to_buffer("\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC", padding_length_for_rt_coding, 1);
        static_assert((0xCC >> 2) == WO_ABRT);

        // 4.1 Extern native function information
        //  4.1.1 Extern function libname & symbname & used constant offset / code offset
        write_binary_to_buffer((uint64_t)this->extern_native_functions.size(), 8);
        for (auto& [funcptr, extfuncloc] : this->extern_native_functions)
        {
            // 4.1.1.1 extern script name
            write_constant_str_to_buffer(extfuncloc.script_name.c_str(), extfuncloc.script_name.size());

            // 4.1.1.2 extern library name
            auto library_name = extfuncloc.library_name.value_or("");
            write_constant_str_to_buffer(library_name.c_str(), library_name.size());

            // 4.1.1.3 extern function name
            write_constant_str_to_buffer(extfuncloc.function_name.c_str(), extfuncloc.function_name.size());

            // 4.1.1.4 used function in constant index
            write_binary_to_buffer((uint64_t)extfuncloc.constant_offset_in_binary.size(), 8);
            for (auto constant_index : extfuncloc.constant_offset_in_binary)
                write_binary_to_buffer((uint64_t)constant_index, 8);

            // 4.1.1.5 used function in ir binary code
            write_binary_to_buffer((uint64_t)extfuncloc.caller_offset_in_ir.size(), 8);
            for (auto constant_index : extfuncloc.caller_offset_in_ir)
                write_binary_to_buffer((uint64_t)constant_index, 8);
        }

        // 4.2 Extern woolang function define & offset
        //  4.2.1 Extern woolang function symbol name & offset
        write_binary_to_buffer((uint64_t)this->extern_script_functions.size(), 8);
        for (auto& [funcname, offset] : this->extern_script_functions)
        {
            // 4.2.1.1 extern function name & offset
            write_constant_str_to_buffer(funcname.c_str(), funcname.size());
            write_binary_to_buffer((uint64_t)offset, 8);
        }

        // 5.1 JIT Informations
        write_binary_to_buffer((uint64_t)meta_data_for_jit._functions_offsets_for_jit.size(), 8);
        for (size_t function_offset : meta_data_for_jit._functions_offsets_for_jit)
        {
            write_binary_to_buffer((uint64_t)function_offset, 8);
        }

        write_binary_to_buffer((uint64_t)meta_data_for_jit._functions_def_constant_idx_for_jit.size(), 8);
        for (size_t function_constant_offset : meta_data_for_jit._functions_def_constant_idx_for_jit)
        {
            write_binary_to_buffer((uint64_t)function_constant_offset, 8);
        }

        write_binary_to_buffer((uint64_t)meta_data_for_jit._calln_opcode_offsets_for_jit.size(), 8);
        for (size_t calln_offset : meta_data_for_jit._calln_opcode_offsets_for_jit)
        {
            write_binary_to_buffer((uint64_t)calln_offset, 8);
        }

        write_binary_to_buffer((uint64_t)meta_data_for_jit._mkclos_opcode_offsets_for_jit.size(), 8);
        for (size_t mkclos_offset : meta_data_for_jit._mkclos_opcode_offsets_for_jit)
        {
            write_binary_to_buffer((uint64_t)mkclos_offset, 8);
        }

        // 6.1 Constant string buffer
        auto& constant_string_buffer = constant_string_pool.get_pool();
        size_t padding_length_for_constant_string_buf = (8ull - (constant_string_buffer.size() % 8ull)) % 8ull;

        write_binary_to_buffer(
            (uint64_t)(constant_string_buffer.size() + padding_length_for_constant_string_buf), 8);
        write_buffer_to_buffer(constant_string_buffer.data(), constant_string_buffer.size(), 1);
        write_buffer_to_buffer("_padding", padding_length_for_constant_string_buf, 1);

        // 7.1 Debug information
        if (!savepdi || this->program_debug_info == nullptr)
            write_buffer_to_buffer("nopdisup", 8, 1);
        else
        {
            write_buffer_to_buffer("pdisuped", 8, 1);
            // 7.1.1 Saving pdi's A data(_general_src_data_buf_a), it stores srcs' location informations.
            // And used for getting ip(not runtime ip) by src location informs.
            write_binary_to_buffer((uint32_t)this->program_debug_info->_general_src_data_buf_a.size(), 4);
            for (auto& [src_file_path, locations] : this->program_debug_info->_general_src_data_buf_a)
            {
                auto&& src_file = wstr_to_str(src_file_path);
                write_binary_to_buffer((uint32_t)src_file.size(), 4);
                write_buffer_to_buffer(src_file.c_str(), src_file.size(), 1);
                write_buffer_to_buffer("_pad", (4ull - (src_file.size() % 4ull)) % 4ull, 1);

                write_binary_to_buffer((uint32_t)locations.size(), 4);
                for (auto& loc : locations)
                {
                    // Note: No need for storage `source_file`, this field has been restored.
                    write_binary_to_buffer((uint32_t)loc.ip, 4);
                    write_binary_to_buffer((uint32_t)loc.begin_row_no, 4);
                    write_binary_to_buffer((uint32_t)loc.begin_col_no, 4);
                    write_binary_to_buffer((uint32_t)loc.end_row_no, 4);
                    write_binary_to_buffer((uint32_t)loc.end_col_no, 4);
                    write_binary_to_buffer((uint32_t)(loc.unbreakable ? 1 : 0), 4);
                }
            }

            // 7.1.2 Saving pdi's B data(_general_src_data_buf_b), it stores srcs' location informations.
            // It used for getting src location informs by ip(not runtime ip).
            write_binary_to_buffer((uint32_t)this->program_debug_info->_general_src_data_buf_b.size(), 4);
            for (auto& [ip, loc] : this->program_debug_info->_general_src_data_buf_b)
            {
                write_binary_to_buffer((uint32_t)ip, 4);

                auto&& src_file = wstr_to_str(loc.source_file);
                write_binary_to_buffer((uint32_t)src_file.size(), 4);
                write_buffer_to_buffer(src_file.c_str(), src_file.size(), 1);
                write_buffer_to_buffer("_pad", (4ull - (src_file.size() % 4ull)) % 4ull, 1);

                write_binary_to_buffer((uint32_t)loc.ip, 4);
                write_binary_to_buffer((uint32_t)loc.begin_row_no, 4);
                write_binary_to_buffer((uint32_t)loc.begin_col_no, 4);
                write_binary_to_buffer((uint32_t)loc.end_row_no, 4);
                write_binary_to_buffer((uint32_t)loc.end_col_no, 4);
                write_binary_to_buffer((uint32_t)(loc.unbreakable ? 1 : 0), 4);
            }

            // 7.1.3 Saving pdi's C data(_function_ip_data_buf), it stores functions' location and variables informs.
            write_binary_to_buffer((uint32_t)this->program_debug_info->_function_ip_data_buf.size(), 4);
            for (auto& [function_name, func_symb_info] : this->program_debug_info->_function_ip_data_buf)
            {
                write_binary_to_buffer((uint32_t)function_name.size(), 4);
                write_buffer_to_buffer(function_name.c_str(), function_name.size(), 1);
                write_buffer_to_buffer("_pad", (4ull - (function_name.size() % 4ull)) % 4ull, 1);

                write_binary_to_buffer((uint32_t)func_symb_info.ir_begin, 4);
                write_binary_to_buffer((uint32_t)func_symb_info.ir_end, 4);

                write_binary_to_buffer((uint32_t)func_symb_info.variables.size(), 4);
                for (auto& [varname, varinfolist] : func_symb_info.variables)
                {
                    write_binary_to_buffer((uint32_t)varname.size(), 4);
                    write_buffer_to_buffer(varname.c_str(), varname.size(), 1);
                    write_buffer_to_buffer("_pad", (4ull - (varname.size() % 4ull)) % 4ull, 1);

                    write_binary_to_buffer((uint32_t)varinfolist.size(), 4);
                    for (auto& varinfo : varinfolist)
                    {
                        // Note: No need for storing varname of varinfo, it has been stored.
                        write_binary_to_buffer((uint32_t)varinfo.define_place, 4);
                        write_binary_to_buffer((int32_t)varinfo.bp_offset, 4);
                    }
                }
            }

            // 7.1.4 Saving pdi's D data(pdd_rt_code_byte_offset_to_ir), it stores the relationship between
            // ip and runtime ip.
            write_binary_to_buffer((uint32_t)this->program_debug_info->pdd_rt_code_byte_offset_to_ir.size(), 4);
            for (auto& [rtir, ir] : this->program_debug_info->pdd_rt_code_byte_offset_to_ir)
            {
                write_binary_to_buffer((uint32_t)rtir, 4);
                write_binary_to_buffer((uint32_t)ir, 4);
            }
        }

        auto* finalbinary = malloc(binary_buffer.size());
        memcpy(finalbinary, binary_buffer.data(), binary_buffer.size());

        return std::make_tuple(finalbinary, binary_buffer.size());
    }

    std::optional<shared_pointer<runtime_env>> runtime_env::_create_from_stream(
        binary_source_stream* stream, wo_string_t* out_reason, bool* out_is_binary)
    {
        *out_is_binary = true;

#define WO_LOAD_BIN_FAILED(reason) do{*out_reason = reason; return std::nullopt;}while(0)
        // 1.1 (+0) Magic number(0x3001A26B look like WOOLANG B)
        uint32_t magic_number;
        if (!stream->read_elem(&magic_number) || magic_number != (uint32_t)0x3001A26B)
        {
            *out_is_binary = false;
            WO_LOAD_BIN_FAILED("Magic number failed.");
        }

        stream->read_elem(&magic_number); // padding 4 byte.

        // 1.2 (+8) Version info
        uint64_t version_infrom;
        if (!stream->read_elem(&version_infrom) || version_infrom != (uint64_t)wo_version_int())
            WO_LOAD_BIN_FAILED("Woolang version missmatch.");

        // 1.3 (+16) Source CRC64(TODO)

        // 2.1 (+16 + 2N * 8) Global space size * sizeof(Value)
        uint64_t global_value_count;
        if (!stream->read_elem(&global_value_count))
            WO_LOAD_BIN_FAILED("Failed to restore global value count.");

        // 2.2 Default register size
        uint64_t register_count;
        if (!stream->read_elem(&register_count))
            WO_LOAD_BIN_FAILED("Failed to restore register count.");

        // 2.3 Constant data
        uint64_t constant_value_count;
        if (!stream->read_elem(&constant_value_count))
            WO_LOAD_BIN_FAILED("Failed to restore constant count.");

        shared_pointer<runtime_env> result = new runtime_env;
        result->real_register_count = (size_t)register_count;
        result->constant_and_global_value_takeplace_count =
            (size_t)(constant_value_count + 1 + global_value_count + 1);
        result->constant_value_count = (size_t)constant_value_count;

        size_t preserve_memory_size =
            result->constant_and_global_value_takeplace_count;

        value* preserved_memory = (value*)malloc(preserve_memory_size * sizeof(wo::value));
        memset(preserved_memory, 0, preserve_memory_size * sizeof(wo::value));

        result->constant_and_global_storage = preserved_memory;

        struct string_buffer_index
        {
            uint32_t index;
            uint32_t size;
        };
        std::map<value*, string_buffer_index> constant_string_index_for_update;

        std::function<bool(value*)> read_value_from_buffer =
            [
                &stream,
                out_reason,
                &constant_string_index_for_update,
                &read_value_from_buffer](value* out_val)
            {
                uint64_t constant_type_scope, constant_value_scope;
                if (!stream->read_elem(&constant_type_scope))
                    return false;

                out_val->type_space = constant_type_scope;
                switch (out_val->type)
                {
                case wo::value::valuetype::struct_type:
                {
                    uint64_t constant_struct_size;
                    if (!stream->read_elem(&constant_struct_size))
                        return false;

                    out_val->set_struct_nogc(static_cast<uint16_t>(constant_struct_size));
                    for (uint16_t idx = 0; idx < constant_struct_size; ++idx)
                    {
                        if (!read_value_from_buffer(&out_val->structs->m_values[idx]))
                            return false;
                    }
                    break;
                }
                case wo::value::valuetype::string_type:
                {
                    uint32_t constant_string_pool_loc, constant_string_pool_size;
                    if (!stream->read_elem(&constant_string_pool_loc)
                        || !stream->read_elem(&constant_string_pool_size))
                        return false;

                    auto& loc = constant_string_index_for_update[out_val];
                    loc.index = constant_string_pool_loc;
                    loc.size = constant_string_pool_size;
                    break;
                }
                case wo::value::valuetype::integer_type:
                case wo::value::valuetype::real_type:
                case wo::value::valuetype::handle_type:
                case wo::value::valuetype::bool_type:
                case wo::value::valuetype::invalid:
                    if (!stream->read_elem(&constant_value_scope))
                        return false;
                    out_val->value_space = constant_value_scope;
                    break;
                default:
                    return false;
                }

                return true;
            };

        for (uint64_t ci = 0; ci < constant_value_count; ++ci)
        {
            if (!read_value_from_buffer(&preserved_memory[ci]))
                WO_LOAD_BIN_FAILED("Failed to restore constant value.");
        }

        // 3.1 Code data
        //  3.1.1 Code data length
        uint64_t rt_code_with_padding_length;
        if (!stream->read_elem(&rt_code_with_padding_length))
            WO_LOAD_BIN_FAILED("Failed to restore code length.");

        byte_t* code_buf = (byte_t*)malloc((size_t)rt_code_with_padding_length * sizeof(byte_t));
        result->rt_codes = code_buf;
        result->rt_code_len = (size_t)rt_code_with_padding_length * sizeof(byte_t);

        wo_assert(code_buf != nullptr);

        if (!stream->read_buffer(code_buf, (size_t)rt_code_with_padding_length * sizeof(byte_t)))
            WO_LOAD_BIN_FAILED("Failed to restore code.");

        struct extern_native_function
        {
            string_buffer_index script_path_idx;
            string_buffer_index library_name_idx;
            string_buffer_index function_name_idx;

            std::vector<size_t> constant_offsets;
            std::vector<size_t> ir_command_offsets;
        };
        std::vector<extern_native_function> extern_native_functions;

        // 4.1 Extern native function information
        //  4.1.1 Extern function libname & symbname & used constant offset / code offset
        uint64_t extern_native_function_count;
        if (!stream->read_elem(&extern_native_function_count))
            WO_LOAD_BIN_FAILED("Failed to restore extern native function count.");

        for (uint64_t i = 0; i < extern_native_function_count; ++i)
        {
            extern_native_function loading_function;

            // 4.1.1.1 extern script name
            if (!stream->read_elem(&loading_function.script_path_idx.index)
                || !stream->read_elem(&loading_function.script_path_idx.size))
                WO_LOAD_BIN_FAILED("Failed to restore extern native function symbol loader script.");

            // 4.1.1.2 extern library name
            if (!stream->read_elem(&loading_function.library_name_idx.index)
                || !stream->read_elem(&loading_function.library_name_idx.size))
                WO_LOAD_BIN_FAILED("Failed to restore extern native function library name.");

            // 4.1.1.3 extern function name
            if (!stream->read_elem(&loading_function.function_name_idx.index)
                || !stream->read_elem(&loading_function.function_name_idx.size))
                WO_LOAD_BIN_FAILED("Failed to restore extern native function symbol name.");

            // 4.1.1.4 used function in constant index
            uint64_t used_constant_offset_count;
            if (!stream->read_elem(&used_constant_offset_count))
                WO_LOAD_BIN_FAILED("Failed to restore extern native function constant count.");

            for (uint64_t i = 0; i < used_constant_offset_count; ++i)
            {
                uint64_t constant_index;
                if (!stream->read_elem(&constant_index))
                    WO_LOAD_BIN_FAILED("Failed to restore extern native function constant index.");

                loading_function.constant_offsets.push_back((size_t)constant_index);
            }

            // 4.1.1.5 used function in ir binary code
            uint64_t used_ir_offset_count;
            if (!stream->read_elem(&used_ir_offset_count))
                WO_LOAD_BIN_FAILED("Failed to restore extern native function ir-offset count.");

            for (uint64_t i = 0; i < used_ir_offset_count; ++i)
            {
                uint64_t ir_code_offset;
                if (!stream->read_elem(&ir_code_offset))
                    WO_LOAD_BIN_FAILED("Failed to restore extern native function ir-offset.");

                loading_function.ir_command_offsets.push_back((size_t)ir_code_offset);
            }
            extern_native_functions.emplace_back(std::move(loading_function));
        }

        // 4.2 Extern woolang function define & offset
        //  4.2.1 Extern woolang function symbol name & offset
        struct extern_script_function
        {
            string_buffer_index function_name;
            size_t ir_offset;
        };
        std::vector<extern_script_function> extern_script_functions;

        uint64_t extern_script_function_count;
        if (!stream->read_elem(&extern_script_function_count))
            WO_LOAD_BIN_FAILED("Failed to restore extern script function count.");

        for (uint64_t i = 0; i < extern_script_function_count; ++i)
        {
            extern_script_function loading_function;

            if (!stream->read_elem(&loading_function.function_name.index)
                || !stream->read_elem(&loading_function.function_name.size))
                WO_LOAD_BIN_FAILED("Failed to restore extern script function count.");

            uint64_t iroffset;
            if (!stream->read_elem(&iroffset))
                WO_LOAD_BIN_FAILED("Failed to restore extern script function ir-offset.");

            loading_function.ir_offset = (size_t)iroffset;

            extern_script_functions.emplace_back(std::move(loading_function));
        }

        // 5.1 JIT Informations
        uint64_t _functions_offsets_count = 0;
        if (!stream->read_elem(&_functions_offsets_count))
            WO_LOAD_BIN_FAILED("Failed to restore functions offset count.");
        for (uint64_t i = 0; i < _functions_offsets_count; ++i)
        {
            uint64_t offset = 0;
            if (!stream->read_elem(&offset))
                WO_LOAD_BIN_FAILED("Failed to restore functions offset.");
            result->meta_data_for_jit._functions_offsets_for_jit.push_back((size_t)offset);
        }

        uint64_t _functions_constant_offsets_count = 0;
        if (!stream->read_elem(&_functions_constant_offsets_count))
            WO_LOAD_BIN_FAILED("Failed to restore functions constant offset count.");
        for (uint64_t i = 0; i < _functions_constant_offsets_count; ++i)
        {
            uint64_t offset = 0;
            if (!stream->read_elem(&offset))
                WO_LOAD_BIN_FAILED("Failed to restore functions offset.");
            result->meta_data_for_jit._functions_def_constant_idx_for_jit.push_back((size_t)offset);
        }

        uint64_t _calln_opcode_offsets_count = 0;
        if (!stream->read_elem(&_calln_opcode_offsets_count))
            WO_LOAD_BIN_FAILED("Failed to restore calln offset count.");
        for (uint64_t i = 0; i < _calln_opcode_offsets_count; ++i)
        {
            uint64_t offset = 0;
            if (!stream->read_elem(&offset))
                WO_LOAD_BIN_FAILED("Failed to restore calln offset.");
            result->meta_data_for_jit._calln_opcode_offsets_for_jit.push_back((size_t)offset);
        }

        uint64_t _mkclos_opcode_offsets_count = 0;
        if (!stream->read_elem(&_mkclos_opcode_offsets_count))
            WO_LOAD_BIN_FAILED("Failed to restore mkclos offset count.");
        for (uint64_t i = 0; i < _mkclos_opcode_offsets_count; ++i)
        {
            uint64_t offset = 0;
            if (!stream->read_elem(&offset))
                WO_LOAD_BIN_FAILED("Failed to restore mkclos offset.");
            result->meta_data_for_jit._mkclos_opcode_offsets_for_jit.push_back((size_t)offset);
        }

        // 6.1 Constant string buffer
        uint64_t string_buffer_size_with_padding;
        if (!stream->read_elem(&string_buffer_size_with_padding))
            WO_LOAD_BIN_FAILED("Failed to restore string buffer size.");

        std::vector<char> string_pool_buffer((size_t)string_buffer_size_with_padding, 0);
        if (!stream->read_buffer(string_pool_buffer.data(), (size_t)string_buffer_size_with_padding))
            WO_LOAD_BIN_FAILED("Failed to restore string buffer.");

        auto restore_string_from_buffer =
            [&string_pool_buffer](const string_buffer_index& string_index, std::string* out_str)->bool
            {
                if (string_index.index + string_index.size > string_pool_buffer.size())
                    return false;
                *out_str = std::string(string_pool_buffer.data() + string_index.index, string_index.size);
                return true;
            };

        std::string constant_string;
        for (auto& [store_value, string_index] : constant_string_index_for_update)
        {
            wo_assert(store_value->type == wo::value::valuetype::string_type);

            if (!restore_string_from_buffer(string_index, &constant_string))
                WO_LOAD_BIN_FAILED("Failed to restore string from string buffer.");

            store_value->set_gcunit<wo::value::valuetype::string_type>(
                string_t::gc_new<gcbase::gctype::no_gc>(constant_string));
        }

        for (auto& extern_native_function : extern_native_functions)
        {
            std::string script_path, library_name, function_name;
            if (!restore_string_from_buffer(extern_native_function.script_path_idx, &script_path))
                WO_LOAD_BIN_FAILED("Failed to restore string from string buffer.");
            if (!restore_string_from_buffer(extern_native_function.library_name_idx, &library_name))
                WO_LOAD_BIN_FAILED("Failed to restore string from string buffer.");
            if (!restore_string_from_buffer(extern_native_function.function_name_idx, &function_name))
                WO_LOAD_BIN_FAILED("Failed to restore string from string buffer.");

            wo_native_func_t func = nullptr;

            if (library_name == "")
                func = rslib_extern_symbols::get_global_symbol(function_name.c_str());
            else
                func = result->loaded_libraries.try_load_func_from_in(script_path.c_str(), library_name.c_str(), function_name.c_str());

            if (func == nullptr)
            {
                WO_LOAD_BIN_FAILED("Failed to restore native function, might be changed?");
            }

            auto& extern_native_function_info = result->extern_native_functions[(intptr_t)func];
            extern_native_function_info.function_name = function_name;

            if (library_name == "")
                extern_native_function_info.library_name = std::optional(library_name);
            else
                extern_native_function_info.library_name = std::nullopt;

            extern_native_function_info.script_name = script_path;
            extern_native_function_info.constant_offset_in_binary = extern_native_function.constant_offsets;
            extern_native_function_info.caller_offset_in_ir = extern_native_function.ir_command_offsets;

            for (auto constant_offset : extern_native_function.constant_offsets)
            {
                wo_assert(preserved_memory[constant_offset].type == wo::value::valuetype::handle_type);
                preserved_memory[constant_offset].set_handle((wo_handle_t)func);
            }
            for (auto ir_code_offset : extern_native_function.ir_command_offsets)
            {
                // Add 1 for skip `calln` command.
                uint64_t funcaddr = (uint64_t)func;
                byte_t* funcaddr_for_write = std::launder(reinterpret_cast<byte_t*>(&funcaddr));

                code_buf[ir_code_offset + 1 + 0] = funcaddr_for_write[0];
                code_buf[ir_code_offset + 1 + 1] = funcaddr_for_write[1];
                code_buf[ir_code_offset + 1 + 2] = funcaddr_for_write[2];
                code_buf[ir_code_offset + 1 + 3] = funcaddr_for_write[3];
                code_buf[ir_code_offset + 1 + 4] = funcaddr_for_write[4];
                code_buf[ir_code_offset + 1 + 5] = funcaddr_for_write[5];
                code_buf[ir_code_offset + 1 + 6] = funcaddr_for_write[6];
                code_buf[ir_code_offset + 1 + 7] = funcaddr_for_write[7];
            }
        }

        for (auto& extern_script_function : extern_script_functions)
        {
            std::string function_name;
            if (!restore_string_from_buffer(extern_script_function.function_name, &function_name))
                WO_LOAD_BIN_FAILED("Failed to restore string from string buffer.");

            wo_assert(result->extern_script_functions.find(function_name) == result->extern_script_functions.end());
            result->extern_script_functions[function_name] = extern_script_function.ir_offset;
        }
        char magic_head_of_pdi[8] = {};

        auto _padding_size = ((stream->readed_size + (8 - 1)) / 8) * 8 - stream->readed_size;
        stream->read_buffer(magic_head_of_pdi, _padding_size);

        // 7.1 Debug information
        // 7.1.0 Magic head, "nopdisup" or "pdisuped"
        stream->read_buffer(magic_head_of_pdi, 8);

        if (memcmp(magic_head_of_pdi, "pdisuped", 8) == 0)
        {
            shared_pointer<program_debug_data_info> pdb = new program_debug_data_info;

            // 7.1.1 Restoring pdi's A data(_general_src_data_buf_a), it stores srcs' location informations.
            // And used for getting ip(not runtime ip) by src location informs.
            char _useless_pad[4];

            uint32_t _general_src_data_buf_a_size;
            if (!stream->read_elem(&_general_src_data_buf_a_size))
                WO_LOAD_BIN_FAILED("Failed to restore program debug informations record A count.");
            pdb->_general_src_data_buf_a.reserve(_general_src_data_buf_a_size);

            for (uint32_t ai = 0; ai < _general_src_data_buf_a_size; ++ai)
            {
                uint32_t _filename_length;
                if (!stream->read_elem(&_filename_length))
                    WO_LOAD_BIN_FAILED("Failed to restore program debug informations record A's filename length.");

                std::vector<char> _filename_string(_filename_length + 1, 0);
                if (!stream->read_buffer(_filename_string.data(), _filename_length))
                    WO_LOAD_BIN_FAILED("Failed to restore program debug informations record A's filename.");

                auto padding_len = ((_filename_length + (4 - 1)) / 4) * 4 - _filename_length;

                if (!stream->read_buffer(_useless_pad, padding_len))
                    WO_LOAD_BIN_FAILED("Failed to restore program debug informations record A's filename padding.");

                std::wstring filename = str_to_wstr(_filename_string.data());
                auto& locations = pdb->_general_src_data_buf_a[filename];
                uint32_t _locations_count;
                if (!stream->read_elem(&_locations_count))
                    WO_LOAD_BIN_FAILED("Failed to restore program debug informations record A's location count.");

                for (uint32_t ali = 0; ali < _locations_count; ++ali)
                {
                    program_debug_data_info::location loc;
                    loc.source_file = filename;

                    uint32_t data;

                    if (!stream->read_elem(&data))
                        WO_LOAD_BIN_FAILED("Failed to restore program debug informations record A's location ip.");
                    loc.ip = data;

                    if (!stream->read_elem(&data))
                        WO_LOAD_BIN_FAILED("Failed to restore program debug informations record A's location begin_row_no.");
                    loc.begin_row_no = data;

                    if (!stream->read_elem(&data))
                        WO_LOAD_BIN_FAILED("Failed to restore program debug informations record A's location begin_col_no.");
                    loc.begin_col_no = data;

                    if (!stream->read_elem(&data))
                        WO_LOAD_BIN_FAILED("Failed to restore program debug informations record A's location end_row_no.");
                    loc.end_row_no = data;

                    if (!stream->read_elem(&data))
                        WO_LOAD_BIN_FAILED("Failed to restore program debug informations record A's location end_col_no.");
                    loc.end_col_no = data;

                    if (!stream->read_elem(&data))
                        WO_LOAD_BIN_FAILED("Failed to restore program debug informations record A's location unbreakable.");
                    loc.unbreakable = (data != 0);

                    locations.push_back(loc);
                }
            }

            // 7.1.2 Restoring pdi's B data(_general_src_data_buf_b), it stores srcs' location informations.
            // It used for getting src location informs by ip(not runtime ip).
            uint32_t _general_src_data_buf_b_size;
            if (!stream->read_elem(&_general_src_data_buf_b_size))
                WO_LOAD_BIN_FAILED("Failed to restore program debug informations record B count.");
            for (uint32_t bi = 0; bi < _general_src_data_buf_b_size; ++bi)
            {
                uint32_t ip;
                if (!stream->read_elem(&ip))
                    WO_LOAD_BIN_FAILED("Failed to restore program debug informations record B's ip.");

                program_debug_data_info::location loc;

                uint32_t _filename_length;
                if (!stream->read_elem(&_filename_length))
                    WO_LOAD_BIN_FAILED("Failed to restore program debug informations record B's location filename length.");

                std::vector<char> _filename_string(_filename_length + 1, 0);
                if (!stream->read_buffer(_filename_string.data(), _filename_length))
                    WO_LOAD_BIN_FAILED("Failed to restore program debug informations record B's location filename.");

                auto padding_len = ((_filename_length + (4 - 1)) / 4) * 4 - _filename_length;

                if (!stream->read_buffer(_useless_pad, padding_len))
                    WO_LOAD_BIN_FAILED("Failed to restore program debug informations record B's location filename padding.");

                loc.source_file = str_to_wstr(_filename_string.data());

                uint32_t data;

                if (!stream->read_elem(&data))
                    WO_LOAD_BIN_FAILED("Failed to restore program debug informations record B's location ip.");
                loc.ip = data;

                if (!stream->read_elem(&data))
                    WO_LOAD_BIN_FAILED("Failed to restore program debug informations record B's location begin_row_no.");
                loc.begin_row_no = data;

                if (!stream->read_elem(&data))
                    WO_LOAD_BIN_FAILED("Failed to restore program debug informations record B's location begin_col_no.");
                loc.begin_col_no = data;

                if (!stream->read_elem(&data))
                    WO_LOAD_BIN_FAILED("Failed to restore program debug informations record B's location end_row_no.");
                loc.end_row_no = data;

                if (!stream->read_elem(&data))
                    WO_LOAD_BIN_FAILED("Failed to restore program debug informations record B's location end_col_no.");
                loc.end_col_no = data;

                if (!stream->read_elem(&data))
                    WO_LOAD_BIN_FAILED("Failed to restore program debug informations record B's location unbreakable.");
                loc.unbreakable = (data != 0);

                pdb->_general_src_data_buf_b[ip] = loc;
            }

            // 7.1.3 Restoring pdi's C data(_function_ip_data_buf), it stores functions' location and variables informs.
            uint32_t _function_ip_data_buf_size;
            if (!stream->read_elem(&_function_ip_data_buf_size))
                WO_LOAD_BIN_FAILED("Failed to restore program debug informations record B count.");
            pdb->_function_ip_data_buf.reserve(_function_ip_data_buf_size);
            for (uint32_t ci = 0; ci < _function_ip_data_buf_size; ++ci)
            {
                uint32_t _funcname_length;
                if (!stream->read_elem(&_funcname_length))
                    WO_LOAD_BIN_FAILED("Failed to restore program debug informations record C's function name length.");

                std::vector<char> _funcname_string(_funcname_length + 1, 0);
                if (!stream->read_buffer(_funcname_string.data(), _funcname_length))
                    WO_LOAD_BIN_FAILED("Failed to restore program debug informations record C's function name.");

                auto padding_len = ((_funcname_length + (4 - 1)) / 4) * 4 - _funcname_length;

                if (!stream->read_buffer(_useless_pad, padding_len))
                    WO_LOAD_BIN_FAILED("Failed to restore program debug informations record C's function name padding.");

                auto& function_inform = pdb->_function_ip_data_buf[_funcname_string.data()];

                uint32_t data;
                int32_t sdata;

                if (!stream->read_elem(&data))
                    WO_LOAD_BIN_FAILED("Failed to restore program debug informations record C's function ir_begin.");
                function_inform.ir_begin = data;

                if (!stream->read_elem(&data))
                    WO_LOAD_BIN_FAILED("Failed to restore program debug informations record C's function ir_end.");
                function_inform.ir_end = data;

                uint32_t variable_count;
                if (!stream->read_elem(&variable_count))
                    WO_LOAD_BIN_FAILED("Failed to restore program debug informations record C's function variable count.");
                for (uint32_t cvi = 0; cvi < variable_count; ++cvi)
                {
                    uint32_t _varname_length;
                    if (!stream->read_elem(&_varname_length))
                        WO_LOAD_BIN_FAILED("Failed to restore program debug informations record C's variable name length.");

                    std::vector<char> _varname_string(_varname_length + 1, 0);
                    if (!stream->read_buffer(_varname_string.data(), _varname_length))
                        WO_LOAD_BIN_FAILED("Failed to restore program debug informations record C's variable name.");

                    auto padding_len = ((_varname_length + (4 - 1)) / 4) * 4 - _varname_length;

                    if (!stream->read_buffer(_useless_pad, padding_len))
                        WO_LOAD_BIN_FAILED("Failed to restore program debug informations record C's variable name padding.");

                    auto& variable_info = function_inform.variables[_varname_string.data()];
                    uint32_t varcount_with_current_name;
                    if (!stream->read_elem(&varcount_with_current_name))
                        WO_LOAD_BIN_FAILED("Failed to restore program debug informations record C's variable count.");
                    for (uint32_t cvci = 0; cvci < varcount_with_current_name; ++cvci)
                    {
                        program_debug_data_info::function_symbol_infor::variable_symbol_infor varsym;
                        varsym.name = _varname_string.data();

                        if (!stream->read_elem(&data))
                            WO_LOAD_BIN_FAILED("Failed to restore program debug informations record C's define_place ir_begin.");
                        varsym.define_place = data;

                        if (!stream->read_elem(&sdata))
                            WO_LOAD_BIN_FAILED("Failed to restore program debug informations record C's define_place bp_offset.");
                        varsym.bp_offset = sdata;

                        variable_info.push_back(varsym);
                    }

                }
            }

            // 7.1.4 Restoring pdi's D data(pdd_rt_code_byte_offset_to_ir), it stores the relationship between
            // ip and runtime ip.
            uint32_t _pdd_rt_code_byte_offset_to_ir_size;
            if (!stream->read_elem(&_pdd_rt_code_byte_offset_to_ir_size))
                WO_LOAD_BIN_FAILED("Failed to restore program debug informations record D count.");
            for (uint32_t di = 0; di < _pdd_rt_code_byte_offset_to_ir_size; ++di)
            {
                uint32_t rtir, ir;
                if (!stream->read_elem(&rtir))
                    WO_LOAD_BIN_FAILED("Failed to restore program debug informations record D's rtir.");
                if (!stream->read_elem(&ir))
                    WO_LOAD_BIN_FAILED("Failed to restore program debug informations record D's ir.");

                pdb->pdd_rt_code_byte_offset_to_ir[rtir] = ir;
            }
            pdb->runtime_codes_base = result->rt_codes;
            pdb->runtime_codes_length = result->rt_code_len;

            result->program_debug_info = pdb;
        }
        else if (memcmp(magic_head_of_pdi, "nopdisup", 8) != 0)
            WO_LOAD_BIN_FAILED("Bad head of program debug informations.");

        return result;
#undef WO_LOAD_BIN_FAILED
    }

    std::optional<shared_pointer<runtime_env>> runtime_env::load_create_env_from_binary(
        wo_string_t virtual_file,
        const void* bytestream,
        size_t streamsz,
        wo_string_t* out_reason,
        bool* out_is_binary)
    {
        std::string buffer_to_store_data_from_file_or_mem;
        if (bytestream == nullptr)
        {
            std::wstring real_read_file;
            wo::check_and_read_virtual_source<false>(
                &buffer_to_store_data_from_file_or_mem,
                &real_read_file,
                wo::str_to_wstr(virtual_file),
                std::nullopt);
        }
        else
            buffer_to_store_data_from_file_or_mem = std::string((const char*)bytestream, streamsz);

        binary_source_stream buf(buffer_to_store_data_from_file_or_mem.data(), buffer_to_store_data_from_file_or_mem.size());
        return _create_from_stream(&buf, out_reason, out_is_binary);
    }

    bool runtime_env::try_find_script_func(const std::string& name, wo_integer_t* out_script_func)
    {
        auto fnd = extern_script_functions.find(name);
        if (fnd != extern_script_functions.end())
        {
            *out_script_func = fnd->second;
            return true;
        }
        return false;
    }
    bool runtime_env::try_find_jit_func(wo_integer_t out_script_func, wo_handle_t* out_jit_func)
    {
        auto fnd = meta_data_for_jit._jit_code_holder.find((size_t)out_script_func);
        if (fnd != meta_data_for_jit._jit_code_holder.end())
        {
            *out_jit_func = (wo_handle_t)reinterpret_cast<intptr_t>(*fnd->second);
            return true;
        }
        return false;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    shared_pointer<runtime_env> ir_compiler::finalize()
    {
        // 1. Generate constant & global & register & runtime_stack memory buffer
        size_t constant_value_count = constant_record_list.size();

        size_t global_value_count = 0;

        for (auto* global_opnum : global_record_list)
        {
            wo_assert(global_opnum->offset + constant_value_count + 1
                < INT32_MAX && global_opnum->offset >= 0);
            global_opnum->real_offset_const_glb = (int32_t)(global_opnum->offset + constant_value_count + 1);

            if (((size_t)global_opnum->offset + 1) > global_value_count)
                global_value_count = (size_t)global_opnum->offset + 1;
        }

        size_t real_register_count = 64;     // t0-t15 r0-r15 (32) special reg (32)

        size_t preserve_memory_size =
            constant_value_count
            + 1
            + global_value_count;

        value* preserved_memory = (value*)malloc(preserve_memory_size * sizeof(wo::value));

        cxx_vec_t<byte_t> generated_runtime_code_buf; // It will be put to 16 byte allign mem place.

        std::map<std::string, cxx_vec_t<size_t>> jmp_record_table;
        std::map<std::string, cxx_vec_t<size_t>> jmp_record_table_for_immtag;
        std::map<std::string, uint32_t> tag_offset_vector_table;

        wo_assert(preserved_memory, "Alloc memory fail.");

        memset(preserved_memory, 0, preserve_memory_size * sizeof(wo::value));
        //  // Fill constant
        for (auto constant_record : constant_record_list)
        {
            size_t constant_index = (size_t)constant_record->constant_index.value();
            wo_assert(constant_index < constant_value_count,
                "Constant index out of range.");

            constant_record->apply_to_constant_instance(
                &preserved_memory[constant_index]);

            if (auto* addr_tagimm_rsfunc = dynamic_cast<opnum::tagimm_rsfunc*>(constant_record))
            {
                jmp_record_table_for_immtag[addr_tagimm_rsfunc->name].push_back(
                    constant_index);
            }
        }

        // 2. Generate code
        shared_pointer<runtime_env> env = new runtime_env();

        for (size_t ip = 0; ip < ir_command_buffer.size(); ++ip)
        {
#define WO_OPCODE(...) wo_macro_overload(WO_OPCODE,__VA_ARGS__)
#define WO_OPCODE_1(OPCODE) (instruct(instruct::opcode::OPCODE, WO_IR.dr()).opcode_dr)
#define WO_OPCODE_2(OPCODE,DR) (instruct(instruct::opcode::OPCODE, 0b000000##DR).opcode_dr)

#define WO_OPCODE_EXT0(...) wo_macro_overload(WO_OPCODE_EXT0,__VA_ARGS__)
#define WO_OPCODE_EXT0_1(OPCODE) (instruct((instruct::opcode)instruct::extern_opcode_page_0::OPCODE, WO_IR.dr()).opcode_dr)
#define WO_OPCODE_EXT0_2(OPCODE,DR) (instruct((instruct::opcode)instruct::extern_opcode_page_0::OPCODE, 0b000000##DR).opcode_dr)


#define WO_OPCODE_EXT1(...) wo_macro_overload(WO_OPCODE_EXT1,__VA_ARGS__)
#define WO_OPCODE_EXT1_1(OPCODE) (instruct((instruct::opcode)instruct::extern_opcode_page_1::OPCODE, WO_IR.dr()).opcode_dr)
#define WO_OPCODE_EXT1_2(OPCODE,DR) (instruct((instruct::opcode)instruct::extern_opcode_page_1::OPCODE, 0b000000##DR).opcode_dr)

#define WO_OPCODE_EXT2(...) wo_macro_overload(WO_OPCODE_EXT2,__VA_ARGS__)
#define WO_OPCODE_EXT2_1(OPCODE) (instruct((instruct::opcode)instruct::extern_opcode_page_2::OPCODE, WO_IR.dr()).opcode_dr)
#define WO_OPCODE_EXT2_2(OPCODE,DR) (instruct((instruct::opcode)instruct::extern_opcode_page_2::OPCODE, 0b000000##DR).opcode_dr)

#define WO_OPCODE_EXT3(...) wo_macro_overload(WO_OPCODE_EXT3,__VA_ARGS__)
#define WO_OPCODE_EXT3_1(OPCODE) (instruct((instruct::opcode)instruct::extern_opcode_page_3::OPCODE, WO_IR.dr()).opcode_dr)
#define WO_OPCODE_EXT3_2(OPCODE,DR) (instruct((instruct::opcode)instruct::extern_opcode_page_3::OPCODE, 0b000000##DR).opcode_dr)

            auto& WO_IR = ir_command_buffer[ip];
            pdb_info->pdd_rt_code_byte_offset_to_ir[generated_runtime_code_buf.size()] = ip;

            if (auto fnd = tag_irbuffer_offset.find(ip); fnd != tag_irbuffer_offset.end())
            {
                for (auto& tag_name : fnd->second)
                {
                    wo_assert(tag_offset_vector_table.find(tag_name) == tag_offset_vector_table.end()
                        , "The tag point to different place.");

                    tag_offset_vector_table[tag_name] = (uint32_t)generated_runtime_code_buf.size();
                }
            }

            if (WO_IR.param)
            {
                auto& WO_IR_PARAM = WO_IR.param.value();
                switch (WO_IR_PARAM.m_type)
                {
                case ir_param::OPCODE:
                    generated_runtime_code_buf.push_back(
                        (uint8_t)instruct(WO_IR_PARAM.m_instruct, WO_IR_PARAM.m_dr).opcode_dr);
                    break;
                case ir_param::type::OPNUM:
                    WO_IR_PARAM.m_opnum->generate_opnum_to_buffer(generated_runtime_code_buf);
                    break;
                case ir_param::IMM_TAG:
                {
                    jmp_record_table[dynamic_cast<opnum::tag*>(WO_IR_PARAM.m_opnum)->name]
                        .push_back(generated_runtime_code_buf.size());
                    generated_runtime_code_buf.push_back(0x00);
                    generated_runtime_code_buf.push_back(0x00);
                    generated_runtime_code_buf.push_back(0x00);
                    generated_runtime_code_buf.push_back(0x00);
                    break;
                }
                case ir_param::type::IMM_U8:
                {
                    byte_t* readptr = (byte_t*)&WO_IR_PARAM.m_immu8;
                    generated_runtime_code_buf.push_back(readptr[0]);
                    break;
                }
                case ir_param::type::IMM_U16:
                {
                    byte_t* readptr = (byte_t*)&WO_IR_PARAM.m_immu16;
                    generated_runtime_code_buf.push_back(readptr[0]);
                    generated_runtime_code_buf.push_back(readptr[1]);
                    break;
                }
                case ir_param::type::IMM_U32:
                {
                    byte_t* readptr = (byte_t*)&WO_IR_PARAM.m_immu32;
                    generated_runtime_code_buf.push_back(readptr[0]);
                    generated_runtime_code_buf.push_back(readptr[1]);
                    generated_runtime_code_buf.push_back(readptr[2]);
                    generated_runtime_code_buf.push_back(readptr[3]);
                    break;
                }
                case ir_param::type::IMM_U64:
                {
                    byte_t* readptr = (byte_t*)&WO_IR_PARAM.m_immu64;
                    generated_runtime_code_buf.push_back(readptr[0]);
                    generated_runtime_code_buf.push_back(readptr[1]);
                    generated_runtime_code_buf.push_back(readptr[2]);
                    generated_runtime_code_buf.push_back(readptr[3]);
                    generated_runtime_code_buf.push_back(readptr[4]);
                    generated_runtime_code_buf.push_back(readptr[5]);
                    generated_runtime_code_buf.push_back(readptr[6]);
                    generated_runtime_code_buf.push_back(readptr[7]);
                    break;
                }
                }
            }
            else
            {
                switch (WO_IR.opcode)
                {
                case instruct::opcode::nop:
                    generated_runtime_code_buf.push_back(WO_OPCODE(nop));
                    break;
                case instruct::opcode::mov:
                    generated_runtime_code_buf.push_back(WO_OPCODE(mov));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);
                    break;
                case instruct::opcode::movcast:
                    generated_runtime_code_buf.push_back(WO_OPCODE(movcast));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);
                    generated_runtime_code_buf.push_back((byte_t)WO_IR.opinteger1);
                    break;
                case instruct::opcode::typeas:

                    if (WO_IR.opinteger2 == 0)
                    {
                        generated_runtime_code_buf.push_back(WO_OPCODE(typeas));
                        WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                        generated_runtime_code_buf.push_back((byte_t)WO_IR.opinteger1);
                    }
                    else
                    {
                        generated_runtime_code_buf.push_back(WO_OPCODE(typeas) | 0b01);
                        WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                        generated_runtime_code_buf.push_back((byte_t)WO_IR.opinteger1);
                    }
                    break;
                case instruct::opcode::psh:
                    if (nullptr == WO_IR.op1)
                    {
                        if (WO_IR.opinteger1 == 0)
                            break;

                        generated_runtime_code_buf.push_back(WO_OPCODE(psh, 00));

                        wo_assert(WO_IR.opinteger1 > 0 && WO_IR.opinteger1 <= UINT16_MAX
                            , "Invalid count to reserve in stack.");

                        uint16_t opushort = (uint16_t)WO_IR.opinteger1;
                        byte_t* readptr = (byte_t*)&opushort;
                        generated_runtime_code_buf.push_back(readptr[0]);
                        generated_runtime_code_buf.push_back(readptr[1]);
                    }
                    else
                    {
                        generated_runtime_code_buf.push_back(WO_OPCODE(psh) | 0b01);
                        WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    }
                    break;
                case instruct::opcode::pop:
                    if (nullptr == WO_IR.op1)
                    {
                        if (WO_IR.opinteger1 == 0)
                            break;

                        generated_runtime_code_buf.push_back(WO_OPCODE(pop, 00));

                        wo_assert(WO_IR.opinteger1 > 0 && WO_IR.opinteger1 <= UINT16_MAX
                            , "Invalid count to pop from stack.");

                        uint16_t opushort = (uint16_t)WO_IR.opinteger1;
                        byte_t* readptr = (byte_t*)&opushort;

                        generated_runtime_code_buf.push_back(readptr[0]);
                        generated_runtime_code_buf.push_back(readptr[1]);
                    }
                    else
                    {
                        generated_runtime_code_buf.push_back(WO_OPCODE(pop) | 0b01);
                        WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    }
                    break;
                case instruct::opcode::lds:
                    generated_runtime_code_buf.push_back(WO_OPCODE(lds));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);
                    break;
                case instruct::opcode::sts:
                    generated_runtime_code_buf.push_back(WO_OPCODE(sts));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);
                    break;
                case instruct::opcode::addi:
                    generated_runtime_code_buf.push_back(WO_OPCODE(addi));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);
                    break;
                case instruct::opcode::subi:
                    generated_runtime_code_buf.push_back(WO_OPCODE(subi));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);
                    break;
                case instruct::opcode::muli:
                    generated_runtime_code_buf.push_back(WO_OPCODE(muli));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);
                    break;
                case instruct::opcode::divi:
                    generated_runtime_code_buf.push_back(WO_OPCODE(divi));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);
                    break;
                case instruct::opcode::modi:
                    generated_runtime_code_buf.push_back(WO_OPCODE(modi));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);
                    break;
                case instruct::opcode::equr:
                    generated_runtime_code_buf.push_back(WO_OPCODE(equr));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);

                    break;
                case instruct::opcode::nequr:
                    generated_runtime_code_buf.push_back(WO_OPCODE(nequr));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);

                    break;
                case instruct::opcode::equs:
                    generated_runtime_code_buf.push_back(WO_OPCODE(equs));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);

                    break;
                case instruct::opcode::nequs:
                    generated_runtime_code_buf.push_back(WO_OPCODE(nequs));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);

                    break;
                case instruct::opcode::addr:
                    generated_runtime_code_buf.push_back(WO_OPCODE(addr));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);
                    break;
                case instruct::opcode::subr:
                    generated_runtime_code_buf.push_back(WO_OPCODE(subr));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);
                    break;
                case instruct::opcode::mulr:
                    generated_runtime_code_buf.push_back(WO_OPCODE(mulr));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);
                    break;
                case instruct::opcode::divr:
                    generated_runtime_code_buf.push_back(WO_OPCODE(divr));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);
                    break;
                case instruct::opcode::modr:
                    generated_runtime_code_buf.push_back(WO_OPCODE(modr));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);
                    break;
                case instruct::opcode::addh:
                    generated_runtime_code_buf.push_back(WO_OPCODE(addh));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);
                    break;
                case instruct::opcode::subh:
                    generated_runtime_code_buf.push_back(WO_OPCODE(subh));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);
                    break;
                case instruct::opcode::adds:
                    generated_runtime_code_buf.push_back(WO_OPCODE(adds));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);
                    break;

                case instruct::opcode::equb:
                    generated_runtime_code_buf.push_back(WO_OPCODE(equb));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);
                    break;
                case instruct::opcode::nequb:
                    generated_runtime_code_buf.push_back(WO_OPCODE(nequb));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);
                    break;
                case instruct::opcode::land:
                    generated_runtime_code_buf.push_back(WO_OPCODE(land));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);
                    break;
                case instruct::opcode::lor:
                    generated_runtime_code_buf.push_back(WO_OPCODE(lor));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);
                    break;
                case instruct::opcode::gti:
                    generated_runtime_code_buf.push_back(WO_OPCODE(gti));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);
                    break;
                case instruct::opcode::lti:
                    generated_runtime_code_buf.push_back(WO_OPCODE(lti));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);
                    break;
                case instruct::opcode::egti:
                    generated_runtime_code_buf.push_back(WO_OPCODE(egti));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);
                    break;
                case instruct::opcode::elti:
                    generated_runtime_code_buf.push_back(WO_OPCODE(elti));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);
                    break;

                case instruct::opcode::gtr:
                    generated_runtime_code_buf.push_back(WO_OPCODE(gtr));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);
                    break;
                case instruct::opcode::ltr:
                    generated_runtime_code_buf.push_back(WO_OPCODE(ltr));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);
                    break;
                case instruct::opcode::egtr:
                    generated_runtime_code_buf.push_back(WO_OPCODE(egtr));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);
                    break;
                case instruct::opcode::eltr:
                    generated_runtime_code_buf.push_back(WO_OPCODE(eltr));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);
                    break;

                case instruct::opcode::gtx:
                    generated_runtime_code_buf.push_back(WO_OPCODE(gtx));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);
                    break;
                case instruct::opcode::ltx:
                    generated_runtime_code_buf.push_back(WO_OPCODE(ltx));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);
                    break;
                case instruct::opcode::egtx:
                    generated_runtime_code_buf.push_back(WO_OPCODE(egtx));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);
                    break;
                case instruct::opcode::eltx:
                    generated_runtime_code_buf.push_back(WO_OPCODE(eltx));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);
                    break;
                case instruct::opcode::mkstruct:
                {
                    generated_runtime_code_buf.push_back(WO_OPCODE(mkstruct));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);

                    uint16_t size = (uint16_t)(WO_IR.opinteger1);
                    byte_t* readptr = (byte_t*)&size;
                    generated_runtime_code_buf.push_back(readptr[0]);
                    generated_runtime_code_buf.push_back(readptr[1]);
                    break;
                }
                case instruct::opcode::idstruct:
                {
                    generated_runtime_code_buf.push_back(WO_OPCODE(idstruct));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);

                    uint16_t size = (uint16_t)(WO_IR.opinteger1);
                    byte_t* readptr = (byte_t*)&size;
                    generated_runtime_code_buf.push_back(readptr[0]);
                    generated_runtime_code_buf.push_back(readptr[1]);
                    break;
                }
                case instruct::opcode::mkarr:
                {
                    generated_runtime_code_buf.push_back(WO_OPCODE(mkarr));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);

                    uint16_t size = (uint16_t)(WO_IR.opinteger1);
                    byte_t* readptr = (byte_t*)&size;
                    generated_runtime_code_buf.push_back(readptr[0]);
                    generated_runtime_code_buf.push_back(readptr[1]);
                    break;
                }
                case instruct::opcode::mkmap:
                {
                    generated_runtime_code_buf.push_back(WO_OPCODE(mkmap));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);

                    uint16_t size = (uint16_t)(WO_IR.opinteger1);
                    byte_t* readptr = (byte_t*)&size;
                    generated_runtime_code_buf.push_back(readptr[0]);
                    generated_runtime_code_buf.push_back(readptr[1]);
                    break;
                }
                case instruct::opcode::idarr:
                    generated_runtime_code_buf.push_back(WO_OPCODE(idarr));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);
                    break;
                case instruct::opcode::iddict:
                    generated_runtime_code_buf.push_back(WO_OPCODE(iddict));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);
                    break;
                case instruct::opcode::idstr:
                    generated_runtime_code_buf.push_back(WO_OPCODE(idstr));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);
                    break;
                case instruct::opcode::siddict:
                    generated_runtime_code_buf.push_back(WO_OPCODE(siddict));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);
                    opnum::reg((uint8_t)WO_IR.opinteger1).generate_opnum_to_buffer(generated_runtime_code_buf);
                    break;
                case instruct::opcode::sidmap:
                    generated_runtime_code_buf.push_back(WO_OPCODE(sidmap));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);
                    opnum::reg((uint8_t)WO_IR.opinteger1).generate_opnum_to_buffer(generated_runtime_code_buf);
                    break;
                case instruct::opcode::sidarr:
                    generated_runtime_code_buf.push_back(WO_OPCODE(sidarr));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);
                    opnum::reg((uint8_t)WO_IR.opinteger1).generate_opnum_to_buffer(generated_runtime_code_buf);
                    break;
                case instruct::opcode::sidstruct:
                {
                    generated_runtime_code_buf.push_back(WO_OPCODE(sidstruct));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);

                    uint16_t size = (uint16_t)(WO_IR.opinteger1);
                    byte_t* readptr = (byte_t*)&size;
                    generated_runtime_code_buf.push_back(readptr[0]);
                    generated_runtime_code_buf.push_back(readptr[1]);
                    break;
                }
                case instruct::opcode::jt:
                    generated_runtime_code_buf.push_back(WO_OPCODE(jt));

                    wo_assert(dynamic_cast<const opnum::tag*>(WO_IR.op1) != nullptr, "Operator num should be a tag.");

                    jmp_record_table[dynamic_cast<const opnum::tag*>(WO_IR.op1)->name]
                        .push_back(generated_runtime_code_buf.size());

                    generated_runtime_code_buf.push_back(0x00);
                    generated_runtime_code_buf.push_back(0x00);
                    generated_runtime_code_buf.push_back(0x00);
                    generated_runtime_code_buf.push_back(0x00);

                    break;
                case instruct::opcode::jf:
                    generated_runtime_code_buf.push_back(WO_OPCODE(jf));

                    wo_assert(dynamic_cast<const opnum::tag*>(WO_IR.op1) != nullptr, "Operator num should be a tag.");

                    jmp_record_table[dynamic_cast<const opnum::tag*>(WO_IR.op1)->name]
                        .push_back(generated_runtime_code_buf.size());

                    generated_runtime_code_buf.push_back(0x00);
                    generated_runtime_code_buf.push_back(0x00);
                    generated_runtime_code_buf.push_back(0x00);
                    generated_runtime_code_buf.push_back(0x00);
                    break;

                case instruct::opcode::jmp:
                    generated_runtime_code_buf.push_back(WO_OPCODE(jmp, 00));

                    wo_assert(dynamic_cast<const opnum::tag*>(WO_IR.op1) != nullptr, "Operator num should be a tag.");

                    jmp_record_table[dynamic_cast<const opnum::tag*>(WO_IR.op1)->name]
                        .push_back(generated_runtime_code_buf.size());

                    generated_runtime_code_buf.push_back(0x00);
                    generated_runtime_code_buf.push_back(0x00);
                    generated_runtime_code_buf.push_back(0x00);
                    generated_runtime_code_buf.push_back(0x00);
                    break;

                case instruct::opcode::call:
                    generated_runtime_code_buf.push_back(WO_OPCODE(call));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    break;
                case instruct::opcode::calln:
                    if (WO_IR.op2)
                    {
                        env->meta_data_for_jit._calln_opcode_offsets_for_jit.push_back(generated_runtime_code_buf.size());

                        wo_assert(dynamic_cast<const opnum::tag*>(WO_IR.op2) != nullptr, "Operator num should be a tag.");

                        generated_runtime_code_buf.push_back(WO_OPCODE(calln, 00));

                        jmp_record_table[dynamic_cast<const opnum::tag*>(WO_IR.op2)->name]
                            .push_back(generated_runtime_code_buf.size());

                        generated_runtime_code_buf.push_back(0x00);
                        generated_runtime_code_buf.push_back(0x00);
                        generated_runtime_code_buf.push_back(0x00);
                        generated_runtime_code_buf.push_back(0x00);

                        // reserve...
                        generated_runtime_code_buf.push_back(0x00);
                        generated_runtime_code_buf.push_back(0x00);
                        generated_runtime_code_buf.push_back(0x00);
                        generated_runtime_code_buf.push_back(0x00);
                    }
                    else if (WO_IR.op1)
                    {
                        uint64_t addr = (uint64_t)(intptr_t)(WO_IR.op1);

                        wo_assert(!extern_native_functions[(intptr_t)addr].function_name.empty());
                        extern_native_functions[(intptr_t)addr].caller_offset_in_ir
                            .push_back(generated_runtime_code_buf.size());

                        if (WO_IR.opinteger1)
                            generated_runtime_code_buf.push_back(WO_OPCODE(calln, 11));
                        else
                            generated_runtime_code_buf.push_back(WO_OPCODE(calln, 01));

                        byte_t* readptr = (byte_t*)&addr;
                        generated_runtime_code_buf.push_back(readptr[0]);
                        generated_runtime_code_buf.push_back(readptr[1]);
                        generated_runtime_code_buf.push_back(readptr[2]);
                        generated_runtime_code_buf.push_back(readptr[3]);
                        generated_runtime_code_buf.push_back(readptr[4]);
                        generated_runtime_code_buf.push_back(readptr[5]);
                        generated_runtime_code_buf.push_back(readptr[6]);
                        generated_runtime_code_buf.push_back(readptr[7]);

                    }
                    else
                    {
                        env->meta_data_for_jit._calln_opcode_offsets_for_jit.push_back(generated_runtime_code_buf.size());

                        generated_runtime_code_buf.push_back(WO_OPCODE(calln, 00));

                        byte_t* readptr = (byte_t*)&WO_IR.opinteger1;
                        generated_runtime_code_buf.push_back(readptr[0]);
                        generated_runtime_code_buf.push_back(readptr[1]);
                        generated_runtime_code_buf.push_back(readptr[2]);
                        generated_runtime_code_buf.push_back(readptr[3]);

                        // reserve...
                        generated_runtime_code_buf.push_back(0x00);
                        generated_runtime_code_buf.push_back(0x00);
                        generated_runtime_code_buf.push_back(0x00);
                        generated_runtime_code_buf.push_back(0x00);
                    }
                    break;
                case instruct::opcode::ret:
                    if (WO_IR.opinteger1)
                    {
                        // ret pop n
                        generated_runtime_code_buf.push_back(WO_OPCODE(ret, 10));

                        uint16_t pop_count = (uint16_t)WO_IR.opinteger1;
                        byte_t* readptr = (byte_t*)&pop_count;
                        generated_runtime_code_buf.push_back(readptr[0]);
                        generated_runtime_code_buf.push_back(readptr[1]);
                    }
                    else
                        generated_runtime_code_buf.push_back(WO_OPCODE(ret, 00));
                    break;
                case instruct::opcode::jnequb:
                {
                    wo_assert(dynamic_cast<const opnum::tag*>(WO_IR.op2) != nullptr, "Operator num should be a tag.");

                    generated_runtime_code_buf.push_back(WO_OPCODE(jnequb));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);

                    // Write jmp
                    jmp_record_table[dynamic_cast<const opnum::tag*>(WO_IR.op2)->name]
                        .push_back(generated_runtime_code_buf.size());
                    generated_runtime_code_buf.push_back(0x00);
                    generated_runtime_code_buf.push_back(0x00);
                    generated_runtime_code_buf.push_back(0x00);
                    generated_runtime_code_buf.push_back(0x00);

                    break;
                }
                case instruct::mkunion:
                {
                    generated_runtime_code_buf.push_back(WO_OPCODE(mkunion));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                    WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);

                    uint16_t id = (uint16_t)WO_IR.opinteger1;
                    byte_t* readptr = (byte_t*)&id;
                    generated_runtime_code_buf.push_back(readptr[0]);
                    generated_runtime_code_buf.push_back(readptr[1]);
                    break;
                }
                case instruct::mkclos:
                {
                    env->meta_data_for_jit._mkclos_opcode_offsets_for_jit.push_back(generated_runtime_code_buf.size());

                    generated_runtime_code_buf.push_back(WO_OPCODE(mkclos, 00));

                    uint16_t capture_count = (uint16_t)WO_IR.opinteger1;
                    byte_t* readptr = (byte_t*)&capture_count;
                    generated_runtime_code_buf.push_back(readptr[0]);
                    generated_runtime_code_buf.push_back(readptr[1]);

                    jmp_record_table[dynamic_cast<const opnum::tag*>(WO_IR.op1)->name]
                        .push_back(generated_runtime_code_buf.size());
                    generated_runtime_code_buf.push_back(0x00);
                    generated_runtime_code_buf.push_back(0x00);
                    generated_runtime_code_buf.push_back(0x00);
                    generated_runtime_code_buf.push_back(0x00);

                    // reserve...
                    generated_runtime_code_buf.push_back(0x00);
                    generated_runtime_code_buf.push_back(0x00);
                    generated_runtime_code_buf.push_back(0x00);
                    generated_runtime_code_buf.push_back(0x00);
                    break;
                }
                case instruct::unpackargs:
                {
                    generated_runtime_code_buf.push_back(WO_OPCODE(unpackargs));
                    WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);

                    byte_t* readptr = (byte_t*)&WO_IR.opinteger1;
                    generated_runtime_code_buf.push_back(readptr[0]);
                    generated_runtime_code_buf.push_back(readptr[1]);
                    generated_runtime_code_buf.push_back(readptr[2]);
                    generated_runtime_code_buf.push_back(readptr[3]);

                    break;
                }
                case instruct::opcode::ext:

                    switch (WO_IR.ext_page_id)
                    {
                    case 0:
                    {
                        generated_runtime_code_buf.push_back(WO_OPCODE(ext, 00));
                        switch (WO_IR.ext_opcode_p0)
                        {
                        case instruct::extern_opcode_page_0::packargs:
                        {
                            generated_runtime_code_buf.push_back(WO_OPCODE_EXT0(packargs));

                            WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);

                            uint16_t func_argc = (uint16_t)WO_IR.opinteger1;
                            byte_t* readptr = (byte_t*)&func_argc;
                            generated_runtime_code_buf.push_back(readptr[0]);
                            generated_runtime_code_buf.push_back(readptr[1]);

                            uint16_t skip_count = (uint16_t)WO_IR.opinteger2;
                            readptr = (byte_t*)&skip_count;
                            generated_runtime_code_buf.push_back(readptr[0]);
                            generated_runtime_code_buf.push_back(readptr[1]);

                            break;
                        }
                        case instruct::extern_opcode_page_0::panic:
                        {
                            generated_runtime_code_buf.push_back(WO_OPCODE_EXT0(panic));
                            WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                            break;
                        }
                        case instruct::extern_opcode_page_0::cdivilr:
                        {
                            generated_runtime_code_buf.push_back(WO_OPCODE_EXT0(cdivilr));
                            WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                            WO_IR.op2->generate_opnum_to_buffer(generated_runtime_code_buf);
                            break;
                        }
                        case instruct::extern_opcode_page_0::cdivil:
                        {
                            generated_runtime_code_buf.push_back(WO_OPCODE_EXT0(cdivil));
                            WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                            break;
                        }
                        case instruct::extern_opcode_page_0::cdivirz:
                        {
                            generated_runtime_code_buf.push_back(WO_OPCODE_EXT0(cdivirz));
                            WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                            break;
                        }
                        case instruct::extern_opcode_page_0::cdivir:
                        {
                            generated_runtime_code_buf.push_back(WO_OPCODE_EXT0(cdivir));
                            WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                            break;
                        }
                        case instruct::extern_opcode_page_0::popn:
                        {
                            generated_runtime_code_buf.push_back(WO_OPCODE_EXT0(popn));
                            WO_IR.op1->generate_opnum_to_buffer(generated_runtime_code_buf);
                            break;
                        }
                        default:
                            wo_error("Unknown instruct.");
                            break;
                        }
                        break;
                    }
                    //case 1:
                    //{
                    //    generated_runtime_code_buf.push_back(WO_OPCODE(ext, 01));
                    //    switch (WO_IR.ext_opcode_p1)
                    //    {
                    //    default:
                    //        wo_error("Unknown instruct.");
                    //        break;
                    //    }
                    //    break;
                    //}
                    //case 2:
                    //{
                    //    generated_runtime_code_buf.push_back(WO_OPCODE(ext, 10));
                    //    switch (WO_IR.ext_opcode_p2)
                    //    {
                    //    default:
                    //        wo_error("Unknown instruct.");
                    //        break;
                    //    }
                    //    break;
                    //}
                    case 3:
                    {
                        generated_runtime_code_buf.push_back(WO_OPCODE(ext, 11));
                        switch (WO_IR.ext_opcode_p3)
                        {
                        case instruct::extern_opcode_page_3::funcbegin:
                            generated_runtime_code_buf.push_back(WO_OPCODE_EXT3(funcbegin));
                            env->meta_data_for_jit._functions_offsets_for_jit.push_back(generated_runtime_code_buf.size());
                            break;
                        case instruct::extern_opcode_page_3::funcend:
                            generated_runtime_code_buf.push_back(WO_OPCODE_EXT3(funcend));
                            break;
                        default:
                            wo_error("Unknown instruct.");
                            break;
                        }
                        break;
                    }
                    default:
                        wo_error("Unknown extern-opcode-page.");
                        break;
                    }

                    break;
                case instruct::opcode::abrt:
                    if (WO_IR.opinteger1)
                        generated_runtime_code_buf.push_back(WO_OPCODE(abrt, 10));
                    else
                        generated_runtime_code_buf.push_back(WO_OPCODE(abrt, 00));
                    break;

                default:
                    wo_error("Unknown instruct.");
                }
            }
        }

#undef WO_OPCODE_2
#undef WO_OPCODE_1
#undef WO_OPCODE
#undef WO_IR
        // 3. Generate tag

        for (auto& [tag, offsets] : jmp_record_table)
        {
            uint32_t offset_val = tag_offset_vector_table[tag];
            for (auto offset : offsets)
            {
                memcpy(
                    generated_runtime_code_buf.data() + offset,
                    &offset_val,
                    sizeof(offset_val));
            }
        }

        for (auto& [tag, imm_value_offsets] : jmp_record_table_for_immtag)
        {
            uint32_t offset_val = tag_offset_vector_table[tag];
            for (auto imm_value_offset : imm_value_offsets)
            {
                env->meta_data_for_jit._functions_def_constant_idx_for_jit.push_back(imm_value_offset);
                wo_assert(preserved_memory[imm_value_offset].type == value::valuetype::integer_type);
                preserved_memory[imm_value_offset].integer = (wo_integer_t)offset_val;
            }
        }

        env->constant_and_global_storage = preserved_memory;

        env->constant_value_count = constant_value_count;
        env->constant_and_global_value_takeplace_count = preserve_memory_size;

        env->real_register_count = real_register_count;
        env->rt_code_len = generated_runtime_code_buf.size();
        byte_t* code_buf = (byte_t*)malloc(env->rt_code_len * sizeof(byte_t));

        pdb_info->runtime_codes_length = env->rt_code_len;

        wo_assert(code_buf, "Alloc memory fail.");
        memcpy(code_buf, generated_runtime_code_buf.data(), env->rt_code_len * sizeof(byte_t));

        for (auto& [extern_func_name, extern_func_offset] : extern_script_functions)
        {
            wo_assert(env->extern_script_functions.find(extern_func_name) == env->extern_script_functions.end());
            env->extern_script_functions[extern_func_name] = pdb_info->get_runtime_ip_by_ip(extern_func_offset);
        }
        env->rt_codes = pdb_info->runtime_codes_base = code_buf;

        env->extern_native_functions = extern_native_functions;
        env->loaded_libraries = loaded_libraries;

        if (wo::config::ENABLE_PDB_INFORMATIONS)
            env->program_debug_info = pdb_info;

        return env;
    }
}