#include "wo_afx.hpp"

namespace wo
{
    void cancel_nogc_mark_for_value(const value& val)
    {
        switch (val.m_type)
        {
        case value::valuetype::struct_type:
            if (val.m_structure != nullptr)
            {
                // Un-completed struct might be created when loading from binary.
                // unit might be nullptr.

                for (uint16_t idx = 0; idx < val.m_structure->m_count; ++idx)
                    cancel_nogc_mark_for_value(val.m_structure->m_values[idx]);
            }
            [[fallthrough]];
        case value::valuetype::string_type:
        {
            gc::unit_attrib cancel_nogc;
            cancel_nogc.m_gc_age = 0;
            cancel_nogc.m_marked = (uint8_t)gcbase::gcmarkcolor::full_mark;
            cancel_nogc.m_alloc_mask = 0;
            cancel_nogc.m_nogc = 0;

            gc::unit_attrib* attrib;

            // NOTE: Constant gcunit is nogc-unit, its safe here to `get_gcunit_and_attrib_ref`.
            auto* unit = val.get_gcunit_and_attrib_ref(&attrib);
            if (unit != nullptr)
            {
                // Un-completed string might be created when loading from binary.
                // unit might be nullptr.
                if (attrib->m_nogc != 0)
                    attrib->m_attr = cancel_nogc.m_attr;
            }
            break;
        }
        default:
            wo_assert(!val.is_gcunit());
            break;
        }
    }

    paged_env_mapping runtime_env::_paged_env_mapping_context;

    void runtime_env::register_envs(runtime_env* env) noexcept
    {
        std::lock_guard g1(_paged_env_mapping_context.m_paged_envs_mx);

        auto result = _paged_env_mapping_context.m_paged_envs.insert(
            std::make_pair(
                reinterpret_cast<intptr_t>(env->rt_codes),
                paged_env_min_context
                {
                    env,
                    env->rt_codes,
                    env->rt_codes + env->rt_code_len,
                    env->constant_and_global_storage,
                }
                ));
        (void)result;
        wo_assert(result.second);
    }
    bool runtime_env::fetch_is_far_addr(const irv2::ir* ip) noexcept
    {
        std::shared_lock g1(_paged_env_mapping_context.m_paged_envs_mx);
        auto fnd = _paged_env_mapping_context.m_paged_envs.upper_bound(reinterpret_cast<intptr_t>(ip));
        if (fnd == _paged_env_mapping_context.m_paged_envs.begin())
            return false;

        auto& far_context = (--fnd)->second;

        if (reinterpret_cast<const wo::byte_t*>(ip) >= far_context.m_runtime_code_end)
            return false;

        return true;
    }
    bool runtime_env::fetch_far_runtime_env(
        const irv2::ir* ip,
        runtime_env** out_env) noexcept
    {
        std::shared_lock g1(_paged_env_mapping_context.m_paged_envs_mx);
        auto fnd = _paged_env_mapping_context.m_paged_envs.upper_bound(reinterpret_cast<intptr_t>(ip));
        if (fnd == _paged_env_mapping_context.m_paged_envs.begin())
            return false;

        auto& far_context = (--fnd)->second;

        if (reinterpret_cast<const wo::byte_t*>(ip) >= far_context.m_runtime_code_end)
            return false;

        *out_env = far_context.m_runtime_env;

        return true;
    }
    bool runtime_env::resync_far_state(
        const irv2::ir* ip,
        const irv2::ir** out_runtime_code_begin,
        const irv2::ir** out_runtime_code_end,
        value** out_static_storage_edge) noexcept
    {
        std::shared_lock g1(_paged_env_mapping_context.m_paged_envs_mx);
        auto fnd = _paged_env_mapping_context.m_paged_envs.upper_bound(reinterpret_cast<intptr_t>(ip));
        if (fnd == _paged_env_mapping_context.m_paged_envs.begin())
            return false;

        auto& far_context = (--fnd)->second;

        if (reinterpret_cast<const wo::byte_t*>(ip) >= far_context.m_runtime_code_end)
            return false;

        *out_runtime_code_begin = reinterpret_cast<const irv2::ir*>(
            far_context.m_runtime_code_begin);
        *out_runtime_code_end = reinterpret_cast<const irv2::ir*>(
            far_context.m_runtime_code_end);
        *out_static_storage_edge = far_context.m_static_storage_edge;

        return true;

    }
    void runtime_env::unregister_envs(const runtime_env* env) noexcept
    {
        std::lock_guard g1(_paged_env_mapping_context.m_paged_envs_mx);

        _paged_env_mapping_context.m_paged_envs.erase(
            reinterpret_cast<intptr_t>(env->rt_codes));
    }

#ifndef WO_DISABLE_COMPILER
    namespace opnum
    {
        imm_extfunc::imm_extfunc(ast::AstValueFunction* func)
            : immbase(&func)
        {
            wo_assert(func->m_IR_extern_information.has_value());
        }

        tagimm_rsfunc::tagimm_rsfunc(ast::AstValueFunction* func)
            : tag(LangContext::IR_function_label(func))
            , immbase(&func)
        {
            wo_assert(!func->m_IR_extern_information.has_value());
        }
    }

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

            location loc = {
                compiler->get_now_ip(),
                ast_node->source_location.begin_at.row,
                ast_node->source_location.begin_at.column,
                ast_node->source_location.end_at.row,
                ast_node->source_location.end_at.column,
                *ast_node->source_location.source_file,
                unbreakable
            };

            auto& location_list_of_file =
                _general_src_data_buf_a[*ast_node->source_location.source_file];

            location_list_of_file.push_back(loc);
            _general_src_data_buf_b[compiler->get_now_ip()] = loc;
        }
    }
    void program_debug_data_info::finalize_generate_debug_info()
    {
    }
    void program_debug_data_info::generate_func_begin(
        const std::string& function_name, ast::AstBase* ast_node, ir_compiler* compiler)
    {
        _function_ip_data_buf[function_name].ir_begin = compiler->get_now_ip();
        generate_debug_info_at_astnode(ast_node, compiler);
    }
    void program_debug_data_info::generate_func_end(
        const std::string& function_name, ir_compiler* compiler)
    {
        _function_ip_data_buf[function_name].ir_end = compiler->get_now_ip();
    }
    void program_debug_data_info::add_func_variable(
        const std::string& function_name, const std::string& varname, size_t rowno, wo_integer_t loc)
    {
        _function_ip_data_buf[function_name].add_variable_define(varname, rowno, loc);
    }
    void program_debug_data_info::update_func_variable(
        const std::string& function_name, wo_integer_t offset)
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
                    auto stx_offset = imm_opnum_stx_offset->constant_value.value_integer();
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
                if (auto* op1tmp = dynamic_cast<const opnum::temporary*>(opnum1))
                {
                    stack_offset = -(int32_t)tr_regist_mapping[op1tmp->m_id];
                }
                else if (auto* op1reg = dynamic_cast<const opnum::reg*>(opnum1);
                    op1reg != nullptr && op1reg->is_bp_offset() && op1reg->get_bp_offset() <= 0)
                {
                    stack_offset = (int32_t)op1reg->get_bp_offset() - (int32_t)maxim_offset;
                }

                if (stack_offset.has_value())
                {
                    auto stack_offset_val = stack_offset.value();
                    if (stack_offset_val >= -64)
                        opnum1 = ctx->opnum_stack_offset(static_cast<int8_t>(stack_offset_val));
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

                if (auto* op2tmp = dynamic_cast<const opnum::temporary*>(opnum2))
                {
                    stack_offset = -(int32_t)tr_regist_mapping[op2tmp->m_id];
                }
                else if (auto* op2reg = dynamic_cast<const opnum::reg*>(opnum2);
                    op2reg != nullptr && op2reg->is_bp_offset() && op2reg->get_bp_offset() <= 0)
                {
                    stack_offset = (int32_t)op2reg->get_bp_offset() - (int32_t)maxim_offset;
                }

                if (stack_offset.has_value())
                {
                    auto stack_offset_val = stack_offset.value();
                    if (stack_offset_val >= -64)
                        opnum2 = ctx->opnum_stack_offset(static_cast<int8_t>(stack_offset_val));
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
                        opnum::reg op3(opnum::reg::bp_offset(static_cast<int8_t>(stack_offset_val)));
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
    std::vector<size_t> program_debug_data_info::get_ip_by_src_location(
        const std::string& src_name, size_t rowno, bool strict, bool ignore_unbreakable)const
    {
        auto fnd = _general_src_data_buf_a.find(src_name);
        if (fnd == _general_src_data_buf_a.end())
            return {};

        std::vector<size_t> result = {};
        for (auto& locinfo : fnd->second)
        {
            if (locinfo.unbreakable && ignore_unbreakable)
                // Skip unbreakable.
                continue;

            if (strict)
            {
                if (locinfo.begin_row_no == rowno)
                    result.push_back(locinfo.ip);
            }
            else if (locinfo.begin_row_no <= rowno
                && locinfo.end_row_no >= rowno)
                result.push_back(locinfo.ip);
        }
        return result;
    }
    size_t program_debug_data_info::get_ip_by_runtime_ip(const byte_t* rt_pos) const
    {
        const size_t FAIL_INDEX = SIZE_MAX;

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
            int result = snprintf(ptrr, sizeof(ptrr), "0x%p>", rt_pos);

            (void)result;
            wo_assert(result > 0 && result < sizeof(ptrr));
            return ptrr;

            }();
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    runtime_env::runtime_env()
        : rt_codes(nullptr)
        , rt_code_len(0)
        , constant_and_global_storage(nullptr)
        , constant_and_global_value_takeplace_count(0)
        , constant_value_count(0)
        , _running_on_vm_count(0)
        , _created_destructable_instance_count(0)
    {
    }
    runtime_env::~runtime_env()
    {
        unregister_envs(this);

        if (wo::config::ENABLE_JUST_IN_TIME)
            free_jit(this);

        if (constant_and_global_storage)
        {
            for (size_t ci = 0; ci < constant_value_count; ++ci)
                cancel_nogc_mark_for_value(constant_and_global_storage[ci]);

            free(constant_and_global_storage);
        }

        if (rt_codes)
            free(const_cast<byte_t*>(rt_codes));
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

        auto write_constant_str_to_buffer =
            [&write_binary_to_buffer, &constant_string_pool](const char* str, size_t len)
            {
                write_binary_to_buffer((uint32_t)constant_string_pool.insert(str, len), 4);
                write_binary_to_buffer((uint32_t)len, 4);
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

        // 3.1 Code data
        //  3.1.1 Code data length
        size_t padding_length_for_rt_coding = (8ull - (this->rt_code_len % 8ull)) % 8ull;
        write_binary_to_buffer((uint64_t)(this->rt_code_len + padding_length_for_rt_coding), 8);

        //  3.1.2 Code body
        write_buffer_to_buffer(this->rt_codes, this->rt_code_len, 1);
        write_buffer_to_buffer("\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC", padding_length_for_rt_coding, 1);

        // 3.2 Constant data
        write_binary_to_buffer(
            (uint64_t)this->constant_value_count, 8);

        std::unordered_map<gcbase*, size_t> gcunit_in_constant_indexs_cache;

        for (size_t ci = 0; ci < this->constant_value_count; ++ci)
        {
            auto& constant_value = this->constant_and_global_storage[ci];

            write_binary_to_buffer(static_cast<uint64_t>(constant_value.m_type), 8);
            switch (constant_value.m_type)
            {
            case wo::value::valuetype::string_type:
            {
                // Record for string
                auto result = gcunit_in_constant_indexs_cache.insert(
                    std::make_pair(constant_value.m_string, ci));

                wo_assert(result.second);
                (void)result;

                write_constant_str_to_buffer(
                    constant_value.m_string->c_str(), constant_value.m_string->size());
                break;
            }
            case wo::value::valuetype::struct_type:
            {
                auto result = gcunit_in_constant_indexs_cache.insert(
                    std::make_pair(constant_value.m_structure, ci));

                wo_assert(result.second);
                (void)result;

                // Record for struct
                write_binary_to_buffer((uint64_t)constant_value.m_structure->m_count, 8);
                for (uint16_t idx = 0; idx < constant_value.m_structure->m_count; ++idx)
                {
                    auto& struct_constant_elem = constant_value.m_structure->m_values[idx];

                    write_binary_to_buffer(static_cast<uint64_t>(struct_constant_elem.m_type), 8);
                    switch (struct_constant_elem.m_type)
                    {
                    case wo::value::valuetype::string_type:
                    case wo::value::valuetype::struct_type:
                        write_binary_to_buffer(
                            (uint64_t)gcunit_in_constant_indexs_cache.at(
                                struct_constant_elem.m_gcunit), 8);
                        break;
                    case wo::value::valuetype::script_func_type:
                    {
                        // Save offset instead script func address.
                        const ptrdiff_t diff =
                            reinterpret_cast<const wo::byte_t*>(struct_constant_elem.m_script_func) - rt_codes;
                        write_binary_to_buffer(static_cast<uint64_t>(diff), 8);
                        break;
                    }
                    case wo::value::valuetype::native_func_type:
                    case wo::value::valuetype::handle_type:
                    case wo::value::valuetype::integer_type:
                    case wo::value::valuetype::real_type:
                    case wo::value::valuetype::bool_type:
                    case wo::value::valuetype::invalid:
                        write_binary_to_buffer((uint64_t)struct_constant_elem.m_value_field, 8);
                        break;
                    default:
                        wo_error("Unknown value type.");
                        break;
                    }
                }
                break;
            }
            case wo::value::valuetype::script_func_type:
            {
                // Save offset instead script func address.
                const ptrdiff_t diff = reinterpret_cast<const wo::byte_t*>(constant_value.m_script_func) - rt_codes;
                write_binary_to_buffer(static_cast<uint64_t>(diff), 8);
                break;
            }
            case wo::value::valuetype::native_func_type:
            case wo::value::valuetype::handle_type:
            case wo::value::valuetype::integer_type:
            case wo::value::valuetype::real_type:
            case wo::value::valuetype::bool_type:
            case wo::value::valuetype::invalid:
                // Record for value
                write_binary_to_buffer((uint64_t)constant_value.m_value_field, 8);
                break;
            default:
                wo_error("Unknown value type.");
                break;
            }
        }

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

            write_binary_to_buffer((uint64_t)extfuncloc.offset_in_constant.size(), 8);
            write_binary_to_buffer((uint64_t)extfuncloc.offset_and_tuple_index_in_constant.size(), 8);
            write_binary_to_buffer((uint64_t)extfuncloc.caller_offset_in_ir.size(), 8);

            // 4.1.1.4 used function in constant index
            for (auto constant_index : extfuncloc.offset_in_constant)
                write_binary_to_buffer((uint32_t)constant_index, 4);

            // 4.1.1.5 used function in constant tuple
            for (auto& [constant_index, tuple_index] : extfuncloc.offset_and_tuple_index_in_constant)
            {
                write_binary_to_buffer((uint32_t)constant_index, 4);
                write_binary_to_buffer((uint32_t)tuple_index, 4);
            }

            // 4.1.1.6 used function in ir binary code
            for (auto constant_index : extfuncloc.caller_offset_in_ir)
                write_binary_to_buffer((uint32_t)constant_index, 4);

            wo_assert(binary_buffer.size() % 4 == 0);
            if (binary_buffer.size() % 8 != 0)
                // Make sure align.
                write_buffer_to_buffer("_pad", 4, 1);
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
        write_binary_to_buffer((uint64_t)meta_data_for_jit._functions_constant_idx_for_jit.size(), 8);
        write_binary_to_buffer((uint64_t)meta_data_for_jit._functions_constant_in_tuple_idx_for_jit.size(), 8);
        write_binary_to_buffer((uint64_t)meta_data_for_jit._calln_opcode_offsets_for_jit.size(), 8);
        write_binary_to_buffer((uint64_t)meta_data_for_jit._mkclos_opcode_offsets_for_jit.size(), 8);

        for (uint32_t function_offset : meta_data_for_jit._functions_offsets_for_jit)
            write_binary_to_buffer((uint32_t)function_offset, 4);
        for (uint32_t function_constant_offset : meta_data_for_jit._functions_constant_idx_for_jit)
            write_binary_to_buffer((uint32_t)function_constant_offset, 4);
        for (auto& [constant_offset, tuple_offset] : meta_data_for_jit._functions_constant_in_tuple_idx_for_jit)
        {
            write_binary_to_buffer((uint32_t)constant_offset, 4);
            write_binary_to_buffer((uint32_t)tuple_offset, 4);
        }
        for (uint32_t calln_offset : meta_data_for_jit._calln_opcode_offsets_for_jit)
            write_binary_to_buffer((uint32_t)calln_offset, 4);
        for (uint32_t mkclos_offset : meta_data_for_jit._mkclos_opcode_offsets_for_jit)
            write_binary_to_buffer((uint32_t)mkclos_offset, 4);

        wo_assert(binary_buffer.size() % 4 == 0);
        if (binary_buffer.size() % 8 != 0)
            // Make sure align.
            write_buffer_to_buffer("_pad", 4, 1);

        // 6.1 Constant string buffer
        auto& constant_string_buffer = constant_string_pool.get_pool();
        size_t padding_length_for_constant_string_buf = (8ull - (constant_string_buffer.size() % 8ull)) % 8ull;

        write_binary_to_buffer(
            (uint64_t)(constant_string_buffer.size() + padding_length_for_constant_string_buf), 8);
        write_buffer_to_buffer(constant_string_buffer.data(), constant_string_buffer.size(), 1);
        write_buffer_to_buffer("_padding", padding_length_for_constant_string_buf, 1);

        // 7.1 Debug information
        if (!savepdi || !this->program_debug_info.has_value())
            write_buffer_to_buffer("nopdisup", 8, 1);
        else
        {
            auto& pdi = this->program_debug_info.value();

            write_buffer_to_buffer("pdisuped", 8, 1);
            // 7.1.1 Saving pdi's A data(_general_src_data_buf_a), it stores srcs' location informations.
            // And used for getting ip(not runtime ip) by src location informs.
            write_binary_to_buffer((uint32_t)pdi->_general_src_data_buf_a.size(), 4);
            for (auto& [src_file_path, locations] : pdi->_general_src_data_buf_a)
            {
                write_binary_to_buffer((uint32_t)src_file_path.size(), 4);
                write_buffer_to_buffer(src_file_path.c_str(), src_file_path.size(), 1);
                write_buffer_to_buffer("_pad", (4ull - (src_file_path.size() % 4ull)) % 4ull, 1);

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
            write_binary_to_buffer((uint32_t)pdi->_general_src_data_buf_b.size(), 4);
            for (auto& [ip, loc] : pdi->_general_src_data_buf_b)
            {
                write_binary_to_buffer((uint32_t)ip, 4);

                write_binary_to_buffer((uint32_t)loc.source_file.size(), 4);
                write_buffer_to_buffer(loc.source_file.c_str(), loc.source_file.size(), 1);
                write_buffer_to_buffer("_pad", (4ull - (loc.source_file.size() % 4ull)) % 4ull, 1);

                write_binary_to_buffer((uint32_t)loc.ip, 4);
                write_binary_to_buffer((uint32_t)loc.begin_row_no, 4);
                write_binary_to_buffer((uint32_t)loc.begin_col_no, 4);
                write_binary_to_buffer((uint32_t)loc.end_row_no, 4);
                write_binary_to_buffer((uint32_t)loc.end_col_no, 4);
                write_binary_to_buffer((uint32_t)(loc.unbreakable ? 1 : 0), 4);
            }

            // 7.1.3 Saving pdi's C data(_function_ip_data_buf), it stores functions' location and variables informs.
            write_binary_to_buffer((uint32_t)pdi->_function_ip_data_buf.size(), 4);
            for (auto& [function_name, func_symb_info] : pdi->_function_ip_data_buf)
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
            write_binary_to_buffer((uint32_t)pdi->pdd_rt_code_byte_offset_to_ir.size(), 4);
            for (auto& [rtir, ir] : pdi->pdd_rt_code_byte_offset_to_ir)
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

        // 3.1 Code data
        //  3.1.1 Code data length
        uint64_t rt_code_with_padding_length;
        if (!stream->read_elem(&rt_code_with_padding_length))
            WO_LOAD_BIN_FAILED("Failed to restore code length.");

        //  3.1.2 Code body
        byte_t* code_buf = (byte_t*)malloc(
            (size_t)rt_code_with_padding_length * sizeof(byte_t));

        wo_assert(code_buf != nullptr);
        if (!stream->read_buffer(code_buf, (size_t)rt_code_with_padding_length * sizeof(byte_t)))
            WO_LOAD_BIN_FAILED("Failed to restore code.");

        // 3.2 Constant data
        uint64_t constant_value_count;
        if (!stream->read_elem(&constant_value_count))
            WO_LOAD_BIN_FAILED("Failed to restore constant count.");

        shared_pointer<runtime_env> created_env(new runtime_env);

        created_env->rt_codes = code_buf;
        created_env->rt_code_len = (size_t)rt_code_with_padding_length * sizeof(byte_t);
        created_env->constant_and_global_value_takeplace_count =
            (size_t)(constant_value_count + 1 + global_value_count + 1);
        created_env->constant_value_count = (size_t)constant_value_count;

        size_t preserve_memory_size =
            created_env->constant_and_global_value_takeplace_count;

        value* preserved_memory = (value*)calloc(preserve_memory_size, sizeof(wo::value));
        memset(preserved_memory, 0, preserve_memory_size * sizeof(wo::value));

        created_env->constant_and_global_storage = preserved_memory;

        struct string_buffer_index
        {
            uint32_t index;
            uint32_t size;

            bool operator < (const string_buffer_index& another) const
            {
                return index != another.index ? index < another.index : size < another.size;
            }
        };
        std::unordered_map<value*, string_buffer_index> constant_string_index_for_update;

        for (uint64_t ci = 0; ci < constant_value_count; ++ci)
        {
            auto& this_constant_value = preserved_memory[ci];

            uint64_t constant_type_scope;
            if (!stream->read_elem(&constant_type_scope))
                WO_LOAD_BIN_FAILED("Failed to restore constant value.");

            this_constant_value.m_type = static_cast<value::valuetype>(constant_type_scope);
            switch (this_constant_value.m_type)
            {
            case wo::value::valuetype::string_type:
            {
                uint32_t constant_string_pool_loc, constant_string_pool_size;
                if (!stream->read_elem(&constant_string_pool_loc)
                    || !stream->read_elem(&constant_string_pool_size))
                    WO_LOAD_BIN_FAILED("Failed to restore constant value.");

                auto result = constant_string_index_for_update.insert(
                    std::make_pair(
                        &this_constant_value,
                        string_buffer_index
                        {
                            constant_string_pool_loc,
                            constant_string_pool_size
                        }));

                wo_assert(result.second);
                (void)result;

                break;
            }
            case wo::value::valuetype::struct_type:
            {
                uint64_t constant_struct_size;
                if (!stream->read_elem(&constant_struct_size))
                    WO_LOAD_BIN_FAILED("Failed to restore constant value.");

                this_constant_value.set_struct_nogc(static_cast<uint16_t>(constant_struct_size));

                // Clear all field.
                memset(
                    this_constant_value.m_structure->m_values,
                    0,
                    sizeof(wo::value) * static_cast<size_t>(constant_struct_size));

                for (uint16_t idx = 0; idx < constant_struct_size; ++idx)
                {
                    auto& tuple_constant_elem = this_constant_value.m_structure->m_values[idx];

                    uint64_t constant_type_scope_in_struct;
                    if (!stream->read_elem(&constant_type_scope_in_struct))
                        WO_LOAD_BIN_FAILED("Failed to restore constant value.");

                    tuple_constant_elem.m_type = static_cast<value::valuetype>(constant_type_scope_in_struct);
                    switch (tuple_constant_elem.m_type)
                    {
                    case wo::value::valuetype::string_type:
                    {
                        uint64_t unit_ref_index;
                        if (!stream->read_elem(&unit_ref_index)
                            || unit_ref_index >= constant_value_count
                            || preserved_memory[unit_ref_index].m_type != value::valuetype::string_type)
                            WO_LOAD_BIN_FAILED("Failed to restore constant value.");

                        const auto& constant_string_loc =
                            constant_string_index_for_update.at(&preserved_memory[unit_ref_index]);

                        auto result = constant_string_index_for_update.insert(
                            std::make_pair(
                                &tuple_constant_elem,
                                string_buffer_index
                                {
                                    constant_string_loc.index,
                                    constant_string_loc.size
                                }));

                        wo_assert(result.second);
                        (void)result;
                        break;
                    }
                    case wo::value::valuetype::struct_type:
                    {
                        uint64_t unit_ref_index;
                        if (!stream->read_elem(&unit_ref_index)
                            || unit_ref_index >= constant_value_count
                            || preserved_memory[unit_ref_index].m_type != value::valuetype::struct_type)
                            WO_LOAD_BIN_FAILED("Failed to restore constant value.");

                        tuple_constant_elem.m_structure =
                            preserved_memory[unit_ref_index].m_structure;
                        break;
                    }
                    case wo::value::valuetype::script_func_type:
                    {
                        // Restore abs function address.
                        uint64_t diff;
                        if (!stream->read_elem(&diff))
                            WO_LOAD_BIN_FAILED("Failed to restore constant value.");

                        tuple_constant_elem.m_script_func =
                            reinterpret_cast<const irv2::ir*>(code_buf + diff);
                        break;
                    }
                    case wo::value::valuetype::native_func_type:
                    case wo::value::valuetype::integer_type:
                    case wo::value::valuetype::real_type:
                    case wo::value::valuetype::handle_type:
                    case wo::value::valuetype::bool_type:
                    case wo::value::valuetype::invalid:
                    {
                        uint64_t constant_value_scope;
                        if (!stream->read_elem(&constant_value_scope))
                            WO_LOAD_BIN_FAILED("Failed to restore constant value.");
                        tuple_constant_elem.m_value_field = constant_value_scope;
                        break;
                    }
                    default:
                        WO_LOAD_BIN_FAILED("Failed to restore constant value.");
                    }
                }
                break;
            }
            case wo::value::valuetype::script_func_type:
            {
                // Restore abs function address.
                uint64_t diff;
                if (!stream->read_elem(&diff))
                    WO_LOAD_BIN_FAILED("Failed to restore constant value.");

                this_constant_value.m_script_func =
                    reinterpret_cast<const irv2::ir*>(code_buf + diff);
                break;
            }
            case wo::value::valuetype::native_func_type:
            case wo::value::valuetype::integer_type:
            case wo::value::valuetype::real_type:
            case wo::value::valuetype::handle_type:
            case wo::value::valuetype::bool_type:
            case wo::value::valuetype::invalid:
            {
                uint64_t constant_value_scope;
                if (!stream->read_elem(&constant_value_scope))
                    WO_LOAD_BIN_FAILED("Failed to restore constant value.");
                this_constant_value.m_value_field = constant_value_scope;
                break;
            }
            default:
                WO_LOAD_BIN_FAILED("Failed to restore constant value.");
            }
        }

        struct extern_native_function
        {
            string_buffer_index script_path_idx;
            string_buffer_index library_name_idx;
            string_buffer_index function_name_idx;

            std::vector<uint32_t> constant_offsets;
            std::vector<std::pair<uint32_t, uint16_t>> constant_tuple_index_offsets;
            std::vector<uint32_t> ir_command_offsets;
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

            uint64_t used_constant_offset_count;
            uint64_t used_constant_in_tuple_offset_count;
            uint64_t used_ir_offset_count;
            if (!stream->read_elem(&used_constant_offset_count))
                WO_LOAD_BIN_FAILED("Failed to restore extern native function constant count.");
            if (!stream->read_elem(&used_constant_in_tuple_offset_count))
                WO_LOAD_BIN_FAILED("Failed to restore extern native function constant in tuple count.");
            if (!stream->read_elem(&used_ir_offset_count))
                WO_LOAD_BIN_FAILED("Failed to restore extern native function ir-offset count.");

            // 4.1.1.4 used function in constant index
            for (uint64_t ii = 0; ii < used_constant_offset_count; ++ii)
            {
                uint32_t constant_index;
                if (!stream->read_elem(&constant_index))
                    WO_LOAD_BIN_FAILED("Failed to restore extern native function constant index.");

                loading_function.constant_offsets.push_back(constant_index);
            }

            // 4.1.1.5 used function in constant tuple index
            for (uint64_t ii = 0; ii < used_constant_in_tuple_offset_count; ++ii)
            {
                uint32_t constant_index, elem_index;

                if (!stream->read_elem(&constant_index))
                    WO_LOAD_BIN_FAILED("Failed to restore extern native function constant tuple index.");
                if (!stream->read_elem(&elem_index))
                    WO_LOAD_BIN_FAILED("Failed to restore extern native function constant tuple elem index.");

                loading_function.constant_tuple_index_offsets.emplace_back(
                    std::make_pair(constant_index, static_cast<uint16_t>(elem_index)));
            }

            // 4.1.1.6 used function in ir binary code
            for (uint64_t ii = 0; ii < used_ir_offset_count; ++ii)
            {
                uint32_t ir_code_offset;
                if (!stream->read_elem(&ir_code_offset))
                    WO_LOAD_BIN_FAILED("Failed to restore extern native function ir-offset.");

                loading_function.ir_command_offsets.push_back(ir_code_offset);
            }

            extern_native_functions.emplace_back(std::move(loading_function));

            // Skip the gap.
            if ((used_constant_offset_count
                // + 2 * used_constant_in_tuple_offset_count
                + used_ir_offset_count) % 2 != 0)
            {
                uint32_t padding;
                if (!stream->read_elem(&padding))
                    WO_LOAD_BIN_FAILED("Failed to read padding space.");

                (void)padding;
            }
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
        uint64_t _functions_constant_offsets_count = 0;
        uint64_t _functions_constant_in_tuple_offsets_count = 0;
        uint64_t _calln_opcode_offsets_count = 0;
        uint64_t _mkclos_opcode_offsets_count = 0;

        if (!stream->read_elem(&_functions_offsets_count))
            WO_LOAD_BIN_FAILED("Failed to restore functions offset count.");
        if (!stream->read_elem(&_functions_constant_offsets_count))
            WO_LOAD_BIN_FAILED("Failed to restore functions constant offset count.");
        if (!stream->read_elem(&_functions_constant_in_tuple_offsets_count))
            WO_LOAD_BIN_FAILED("Failed to restore functions constant in tuple offset count.");
        if (!stream->read_elem(&_calln_opcode_offsets_count))
            WO_LOAD_BIN_FAILED("Failed to restore calln offset count.");
        if (!stream->read_elem(&_mkclos_opcode_offsets_count))
            WO_LOAD_BIN_FAILED("Failed to restore mkclos offset count.");

        for (uint64_t i = 0; i < _functions_offsets_count; ++i)
        {
            uint32_t offset = 0;
            if (!stream->read_elem(&offset))
                WO_LOAD_BIN_FAILED("Failed to restore functions offset.");
            created_env->meta_data_for_jit._functions_offsets_for_jit.push_back(offset);
        }
        for (uint64_t i = 0; i < _functions_constant_offsets_count; ++i)
        {
            uint32_t offset = 0;
            if (!stream->read_elem(&offset))
                WO_LOAD_BIN_FAILED("Failed to restore functions offset.");
            created_env->meta_data_for_jit._functions_constant_idx_for_jit.push_back(offset);
        }
        for (uint64_t i = 0; i < _functions_constant_in_tuple_offsets_count; ++i)
        {
            uint32_t offset = 0;
            uint32_t index = 0;
            if (!stream->read_elem(&offset))
                WO_LOAD_BIN_FAILED("Failed to restore functions in tuple offset.");
            if (!stream->read_elem(&index))
                WO_LOAD_BIN_FAILED("Failed to restore functions in tuple index.");
            created_env->meta_data_for_jit._functions_constant_in_tuple_idx_for_jit.emplace_back(
                std::make_pair(offset, (uint16_t)index));
        }
        for (uint64_t i = 0; i < _calln_opcode_offsets_count; ++i)
        {
            uint32_t offset = 0;
            if (!stream->read_elem(&offset))
                WO_LOAD_BIN_FAILED("Failed to restore calln offset.");
            created_env->meta_data_for_jit._calln_opcode_offsets_for_jit.push_back(offset);
        }
        for (uint64_t i = 0; i < _mkclos_opcode_offsets_count; ++i)
        {
            uint32_t offset = 0;
            if (!stream->read_elem(&offset))
                WO_LOAD_BIN_FAILED("Failed to restore mkclos offset.");
            created_env->meta_data_for_jit._mkclos_opcode_offsets_for_jit.push_back(offset);
        }

        // Skip the gap.
        if ((_functions_offsets_count
            + _functions_constant_offsets_count
            // + 2 * _functions_constant_in_tuple_offsets_count
            + _calln_opcode_offsets_count
            + _mkclos_opcode_offsets_count) % 2 != 0)
        {
            uint32_t padding;
            if (!stream->read_elem(&padding))
                WO_LOAD_BIN_FAILED("Failed to read padding space.");

            (void)padding;
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
        std::map<string_buffer_index, wo::string_t*> created_string_instances;
        for (auto& [store_value, string_index] : constant_string_index_for_update)
        {
            wo_assert(store_value->m_type == wo::value::valuetype::string_type);

            wo::string_t* string_instance;
            if (auto fnd = created_string_instances.find(string_index);
                fnd != created_string_instances.end())
            {
                string_instance = fnd->second;
            }
            else
            {
                if (!restore_string_from_buffer(string_index, &constant_string))
                    WO_LOAD_BIN_FAILED("Failed to restore string from string buffer.");

                string_instance = string_t::gc_new<gcbase::gctype::no_gc>(constant_string);
                (void)created_string_instances.insert(std::make_pair(string_index, string_instance));
            }
            wo_assert(string_instance != nullptr);

            store_value->set_gcunit<wo::value::valuetype::string_type>(
                string_instance);
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
                func = created_env->loaded_libraries.try_load_func_from_in(
                    script_path.c_str(), library_name.c_str(), function_name.c_str());

            if (func == nullptr)
            {
                WO_LOAD_BIN_FAILED("Failed to restore native function, might be changed?");
            }

            auto& extern_native_function_info = created_env->extern_native_functions[func];
            extern_native_function_info.function_name = function_name;

            if (library_name == "")
                extern_native_function_info.library_name = std::nullopt;
            else
                extern_native_function_info.library_name = std::optional(library_name);

            extern_native_function_info.script_name = script_path;
            extern_native_function_info.offset_in_constant = extern_native_function.constant_offsets;
            extern_native_function_info.offset_and_tuple_index_in_constant = extern_native_function.constant_tuple_index_offsets;
            extern_native_function_info.caller_offset_in_ir = extern_native_function.ir_command_offsets;

            for (auto constant_offset : extern_native_function.constant_offsets)
            {
                wo_assert(preserved_memory[constant_offset].m_type == wo::value::valuetype::native_func_type);
                preserved_memory[constant_offset].set_native_func(func);
            }
            for (auto& [constant_offset, tuple_index] : extern_native_function.constant_tuple_index_offsets)
            {
                wo_assert(preserved_memory[constant_offset].m_type == wo::value::valuetype::struct_type
                    && tuple_index < preserved_memory[constant_offset].m_structure->m_count
                    && preserved_memory[constant_offset].m_structure->m_values[tuple_index].m_type == wo::value::valuetype::handle_type);

                preserved_memory[constant_offset].m_structure->m_values[tuple_index].set_native_func(func);
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

            wo_assert(created_env->extern_script_functions.find(function_name) == created_env->extern_script_functions.end());
            created_env->extern_script_functions[function_name] = extern_script_function.ir_offset;
        }
        char magic_head_of_pdi[8] = {};

        auto _padding_size = ((stream->readed_size + (8 - 1)) / 8) * 8 - stream->readed_size;
        stream->read_buffer(magic_head_of_pdi, _padding_size);

        // 7.1 Debug information
        // 7.1.0 Magic head, "nopdisup" or "pdisuped"
        stream->read_buffer(magic_head_of_pdi, 8);

        if (memcmp(magic_head_of_pdi, "pdisuped", 8) == 0)
        {
            shared_pointer<program_debug_data_info> pdb(new program_debug_data_info);

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

                std::string filename = _filename_string.data();
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

                loc.source_file = _filename_string.data();

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

                    auto padding_len_var_name = ((_varname_length + (4 - 1)) / 4) * 4 - _varname_length;

                    if (!stream->read_buffer(_useless_pad, padding_len_var_name))
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
            pdb->runtime_codes_base = created_env->rt_codes;
            pdb->runtime_codes_length = created_env->rt_code_len;

            created_env->program_debug_info.emplace(pdb);
        }
        else if (memcmp(magic_head_of_pdi, "nopdisup", 8) != 0)
            WO_LOAD_BIN_FAILED("Bad head of program debug informations.");

        runtime_env::register_envs(created_env.get());
        return created_env;
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
            std::string real_read_file;
            wo::check_and_read_virtual_source(
                virtual_file,
                std::nullopt,
                &real_read_file,
                &buffer_to_store_data_from_file_or_mem);

            (void)real_read_file;
        }
        else
            buffer_to_store_data_from_file_or_mem = std::string((const char*)bytestream, streamsz);

        binary_source_stream buf(buffer_to_store_data_from_file_or_mem.data(), buffer_to_store_data_from_file_or_mem.size());
        return _create_from_stream(&buf, out_reason, out_is_binary);
    }

    bool runtime_env::try_find_script_func(const std::string& name, const byte_t** out_script_func)
    {
        auto fnd = extern_script_functions.find(name);
        if (fnd != extern_script_functions.end())
        {
            *out_script_func = rt_codes + fnd->second;
            return true;
        }
        return false;
    }
    bool runtime_env::try_find_jit_func(const byte_t* script_func, wo_native_func_t* out_jit_func)
    {
        const ptrdiff_t diff = script_func - rt_codes;
        if (diff > 0 && jit_code_holder.has_value())
        {
            auto& jit_holder = jit_code_holder.value();

            auto fnd = jit_holder.find(static_cast<size_t>(diff));
            if (fnd != jit_holder.end())
            {
                *out_jit_func = fnd->second;
                return true;
            }
        }
        return false;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    void IRBuilder::Label::apply_to_address(
        uint32_t* baseaddr, uint32_t ipoffset, ApplyAddress::Formal formal) noexcept
    {
        if (m_bound_ip_offset.has_value())
        {
            const uint32_t ip_offset = m_bound_ip_offset.value();
            switch (formal)
            {
            case ApplyAddress::Formal::U32:
            {
                baseaddr[ipoffset] = ip_offset;
                break;
            }
            case ApplyAddress::Formal::SHIFT8_U24:
            {
                wo_assert((ip_offset & static_cast<uint32_t>(0xff000000u)) == 0);
                baseaddr[ipoffset] =
                    (baseaddr[ipoffset] & static_cast<uint32_t>(0xff000000u))
                    | static_cast<uint32_t>(ip_offset);

                break;
            }
            default:
                wo_error("Unsupported apply address formal.");
                break;
            }
        }
        else
        {
            std::vector<ApplyAddress>* const pending_apply_address =
                m_pending_apply_address.has_value()
                ? &m_pending_apply_address.value()
                : &m_pending_apply_address.emplace();

            pending_apply_address->emplace_back(
                ApplyAddress{
                    formal,
                    ipoffset,
                });
        }
    }
    void IRBuilder::Label::bind_at_ip_offset(
        uint32_t* baseaddr, uint32_t ipoffset) noexcept
    {
        wo_assert(!m_bound_ip_offset.has_value());

        m_bound_ip_offset.emplace(ipoffset);
        if (m_pending_apply_address.has_value())
        {
            for (auto& apply_addr : m_pending_apply_address.value())
                apply_to_address(
                    baseaddr,
                    apply_addr.m_ipoffset,
                    apply_addr.m_formal);

            m_pending_apply_address.reset();
        }
    }

    IRBuilder::Constant::Constant(
        const IRBuilder* builder,
        size_t constant_index) noexcept
        : m_builder(builder)
        , m_constant_index(constant_index)
    {
    }

    value* IRBuilder::Constant::get_value() const noexcept
    {
        return m_builder->m_constant_storage
            + (m_builder->m_constant_storage_size - m_constant_index);
    }

    IRBuilder::IRBuilder()
        : m_code_holder(nullptr)
        , m_code_holder_capacity(0)
        , m_code_holder_size(0)
        , m_constant_storage(nullptr)
        , m_constant_storage_capacity(0)
        , m_constant_storage_size(0)
    {
    }
    IRBuilder::~IRBuilder()
    {
        if (m_code_holder != nullptr)
            free(m_code_holder);

        if (m_constant_storage != nullptr)
        {
            for (uint32_t i = 0; i < m_constant_storage_size; ++i)
                cancel_nogc_mark_for_value(m_constant_storage[i]);

            free(m_constant_storage);
        }

        for (auto* label : m_created_labels)
            delete label;
    }

    uint32_t IRBuilder::emit(uint32_t opcode) noexcept
    {
        if (m_code_holder_size >= m_code_holder_capacity)
        {
            wo_assert(m_code_holder_size == m_code_holder_capacity);

            if ((m_code_holder_capacity *= 2) == 0)
                m_code_holder_capacity = 128;

            m_code_holder = (uint32_t*)realloc(
                m_code_holder,
                m_code_holder_capacity * sizeof(uint32_t));

            wo_assert(m_code_holder != nullptr);
        }
        m_code_holder[m_code_holder_size] = opcode;
        return m_code_holder_size++;
    }

    IRBuilder::Label* IRBuilder::label() noexcept
    {
        Label* result = new Label();

        m_created_labels.push_back(result);
        return result;
    }
    IRBuilder::Label* IRBuilder::named_label(const char* name) noexcept
    {
        auto fnd = m_named_label.find(name);
        if (fnd != m_named_label.end())
            return fnd->second;

        Label* result = label();
        m_named_label.emplace(name, result);

        return result;
    }
    void IRBuilder::bind(Label* label) noexcept
    {
        label->bind_at_ip_offset(
            m_code_holder,
            m_code_holder_size);
    }

    IRBuilder::Constant IRBuilder::allocate_constant() noexcept
    {
        if (m_constant_storage_size >= m_constant_storage_capacity)
        {
            wo_assert(m_constant_storage_size == m_constant_storage_capacity);
            if (m_constant_storage_capacity == 0)
            {
                wo_assert(m_constant_storage == nullptr);
                m_constant_storage_capacity = 32;
            }

            auto* new_constant_storage = (value*)malloc(
                m_constant_storage_capacity * 2 * sizeof(value));
            wo_assert(new_constant_storage != nullptr);

            if (m_constant_storage != nullptr)
            {
                memcpy(
                    new_constant_storage + m_constant_storage_capacity,
                    m_constant_storage,
                    m_constant_storage_capacity * sizeof(value));

                free(m_constant_storage);
            }
            m_constant_storage = new_constant_storage;
            m_constant_storage_capacity *= 2;           
        }
        return Constant(this, ++m_constant_storage_size);
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define _WO_UI8(V)                                      \
    (static_cast<uint32_t>(V) & static_cast<uint32_t>(0xFFu))

#define _WO_UI16(V)                                      \
    (static_cast<uint32_t>(V) & static_cast<uint32_t>(0xFFFFu))

#define _WO_UI18(V)                                      \
    (static_cast<uint32_t>(V) & static_cast<uint32_t>(0x3FFFFu))

#define _WO_UI24(V)                                      \
    (static_cast<uint32_t>(V) & static_cast<uint32_t>(0xFFFFFFu))

#define _WO_UI26(V)                                      \
    (static_cast<uint32_t>(V) & static_cast<uint32_t>(0x3FFFFFFu))

#define _WO_UI32(V)                                      \
    (static_cast<uint32_t>(V))

#define _WO_EMIT_OP6_CMD(CMD, P26B)                     \
    ((static_cast<uint32_t>(WO_##CMD) << (32 - 6))             \
    | _WO_UI26(P26B))

#define _WO_EMIT_OP8_CMD(CMD, MODE, P24B)               \
    _WO_EMIT_OP6_CMD(                                   \
        CMD,                                            \
        (((static_cast<uint32_t>(MODE)                  \
            & static_cast<uint32_t>(0b11u)) << 24)      \
        | _WO_UI24(P24B)))

#define _WO_EMIT_EXT_I32(I32)                           \
    _WO_UI32(I32)

#define _WO_EMIT_EXT_U32(U32)                           \
    _WO_UI32(U32)

#define _WO_CMD26_I18_I8(I18, I8)                       \
    ((_WO_UI18(I18) << 8) | _WO_UI8(I8))

#define _WO_CMD24_I18_16(I8)                            \
    (_WO_UI8(I8) << 16)

#define _WO_CMD24_8_I8_U8(I8, U8)                     \
    ((_WO_UI8(I8) << 8) | _WO_UI8(U8))

#define _WO_CMD24_8_I16(I16)                            \
    _WO_UI16(I16)

#define _WO_CMD24_8_U16(U16)                            \
    _WO_UI16(U16)

#define _WO_CMD24_I8_16(I8)                             \
    (_WO_UI8(I8) << 16)

#define _WO_CMD24_I8_I8_8(I8A, I8B)                     \
    ((_WO_UI8(I8A) << 16) | (_WO_UI8(I8B) << 8))

#define _WO_CMD24_I8_I8_I8(I8A, I8B, I8C)               \
    ((_WO_UI8(I8A) << 16) | (_WO_UI8(I8B) << 8) | _WO_UI8(I8C))

#define _WO_CMD24_I8_I8_U8(I8A, I8B, U8)               \
    ((_WO_UI8(I8A) << 16) | (_WO_UI8(I8B) << 8) | _WO_UI8(U8))

#define _WO_CMD24_U8_16(U8)                             \
    (_WO_UI8(U8) << 16)

#define _WO_CMD24_I24(I24)                              \
    static_cast<uint32_t>(I24)

#define _WO_CMD24_U24(U24)                              \
    static_cast<uint32_t>(U24)

#define _WO_CMD24_24() 0

    template<typename IntegerT>
    size_t min_width(IntegerT n)
    {
        if (n == 0)
            return 1;

        if constexpr (std::is_signed_v<IntegerT>)
        {
            const int64_t v = n;

            size_t bits = 2;
            for (; bits < 64; ++bits)
            {
                const int64_t min = -(1LL << (bits - 1));
                const int64_t max = (1LL << (bits - 1)) - 1;
                if (v >= min && v <= max)
                    return bits;
            }
            return bits;
        }
        else
        {
            const uint64_t v = n;

            size_t bits = 1;
            for (; bits < 64; ++bits)
            {
                const uint64_t max = (1ULL << bits) - 1;
                if (v <= max)
                    return bits;
            }
            return bits;
        }
    }

    void IRBuilder::nop() noexcept
    {
        emit(_WO_EMIT_OP8_CMD(NOP, 0, 0));
    }
    void IRBuilder::end() noexcept
    {
        emit(_WO_EMIT_OP8_CMD(END, 0, 0));
    }
    void IRBuilder::load(cg_adrsing32 src_cg32, rs_adrsing8 dst_rs8) noexcept
    {
        if (min_width(src_cg32.m_val) <= 18)
            emit(_WO_EMIT_OP6_CMD(LOAD, _WO_CMD26_I18_I8(src_cg32.m_val, dst_rs8.m_val)));
        else
        {
            emit(_WO_EMIT_OP6_CMD(LOADEXT, _WO_CMD24_I18_16(dst_rs8.m_val)));
            emit(_WO_EMIT_EXT_I32(src_cg32.m_val));
        }
    }
    void IRBuilder::store(cg_adrsing32 dst_cg32, rs_adrsing8 src_rs8) noexcept
    {
        if (min_width(dst_cg32.m_val) <= 18)
            emit(_WO_EMIT_OP6_CMD(STORE, _WO_CMD26_I18_I8(dst_cg32.m_val, src_rs8.m_val)));
        else
        {
            emit(_WO_EMIT_OP6_CMD(STOREEXT, _WO_CMD24_I18_16(src_rs8.m_val)));
            emit(_WO_EMIT_EXT_I32(dst_cg32.m_val));
        }
    }
    void IRBuilder::loadext(s_adrsing<24> dst_s24, cg_adrsing32 src_cg32) noexcept
    {
        if (min_width(dst_s24.m_val) <= 16)
        {
            emit(_WO_EMIT_OP6_CMD(LOADEXT, _WO_CMD24_8_I16(dst_s24.m_val)));
            emit(_WO_EMIT_EXT_I32(src_cg32.m_val));
        }
        else
        {
            emit(_WO_EMIT_OP6_CMD(LOADEXT, _WO_CMD24_I24(dst_s24.m_val)));
            emit(_WO_EMIT_EXT_I32(src_cg32.m_val));
        }
    }
    void IRBuilder::storeext(s_adrsing<24> src_s24, cg_adrsing32 cg32) noexcept
    {
        if (min_width(src_s24.m_val) <= 16)
        {
            emit(_WO_EMIT_OP6_CMD(STOREEXT, _WO_CMD24_8_I16(src_s24.m_val)));
            emit(_WO_EMIT_EXT_I32(cg32.m_val));
        }
        else
        {
            emit(_WO_EMIT_OP6_CMD(STOREEXT, _WO_CMD24_I24(src_s24.m_val)));
            emit(_WO_EMIT_EXT_I32(cg32.m_val));
        }
    }
    void IRBuilder::push(fixed_unsigned<24> count_u24) noexcept
    {
        emit(_WO_EMIT_OP8_CMD(PUSH, 0, _WO_CMD24_U24(count_u24.m_val)));
    }
    void IRBuilder::push(rs_adrsing8 src_rs8) noexcept
    {
        emit(_WO_EMIT_OP8_CMD(PUSH, 1, _WO_CMD24_I8_16(src_rs8.m_val)));
    }
    void IRBuilder::push(cg_adrsing32 src_cg32) noexcept
    {
        if (min_width(src_cg32.m_val) <= 24)
        {
            emit(_WO_EMIT_OP8_CMD(PUSH, 2, _WO_CMD24_U24(src_cg32.m_val)));
        }
        else
        {
            emit(_WO_EMIT_OP8_CMD(PUSH, 3, 0));
            emit(_WO_EMIT_EXT_I32(src_cg32.m_val));
        }
    }
    void IRBuilder::pop(fixed_unsigned<24> count_u24) noexcept
    {
        emit(_WO_EMIT_OP8_CMD(POP, 0, _WO_CMD24_U24(count_u24.m_val)));
    }
    void IRBuilder::pop(rs_adrsing8 dst_rs8) noexcept
    {
        emit(_WO_EMIT_OP8_CMD(POP, 1, _WO_CMD24_I8_16(dst_rs8.m_val)));
    }
    void IRBuilder::pop(cg_adrsing32 dst_cg32) noexcept
    {
        if (min_width(dst_cg32.m_val) <= 24)
        {
            emit(_WO_EMIT_OP8_CMD(POP, 2, _WO_CMD24_U24(dst_cg32.m_val)));
        }
        else
        {
            emit(_WO_EMIT_OP8_CMD(POP, 3, 0));
            emit(_WO_EMIT_EXT_I32(dst_cg32.m_val));
        }
    }
    void IRBuilder::cast(
        rs_adrsing8 dst_rs8,
        rs_adrsing8 src_rs8,
        wo_type_t cast_to) noexcept
    {
        emit(_WO_EMIT_OP8_CMD(
            CAST, 0, _WO_CMD24_I8_I8_U8(
                dst_rs8.m_val, src_rs8.m_val, cast_to)));
    }
    void IRBuilder::castitors(
        rs_adrsing8 dst_rs8,
        rs_adrsing8 src_rs8) noexcept
    {
        emit(_WO_EMIT_OP8_CMD(
            CAST, 1, _WO_CMD24_I8_I8_8(
                dst_rs8.m_val, src_rs8.m_val)));
    }
    void IRBuilder::castrtoi(
        rs_adrsing8 dst_rs8,
        rs_adrsing8 src_rs8) noexcept
    {
        emit(_WO_EMIT_OP8_CMD(
            CAST, 2, _WO_CMD24_I8_I8_8(
                dst_rs8.m_val, src_rs8.m_val)));
    }
    void IRBuilder::typeis(
        rs_adrsing8 dst_rs8,
        rs_adrsing8 src_rs8,
        wo_type_t check_type) noexcept
    {
        emit(_WO_EMIT_OP8_CMD(
            TYPECHK, 0, _WO_CMD24_I8_I8_U8(
                dst_rs8.m_val, src_rs8.m_val, check_type)));
    }
    void IRBuilder::typeas(
        rs_adrsing8 src_rs8,
        wo_type_t as_type) noexcept
    {
        emit(_WO_EMIT_OP8_CMD(
            TYPECHK, 1, _WO_CMD24_8_I8_U8(
                src_rs8.m_val, as_type)));
    }
    void IRBuilder::addi(
        rs_adrsing8 dst_rs8,
        rs_adrsing8 src_rs8_a,
        rs_adrsing8 src_rs8_b) noexcept
    {
        if (dst_rs8.m_val == src_rs8_a.m_val)
        {
            emit(_WO_EMIT_OP8_CMD(
                OPCB, 0, _WO_CMD24_I8_I8_8(
                    dst_rs8.m_val, src_rs8_b.m_val)));
        }
        else if (dst_rs8.m_val == src_rs8_b.m_val)
        {
            emit(_WO_EMIT_OP8_CMD(
                OPCB, 0, _WO_CMD24_I8_I8_8(
                    dst_rs8.m_val, src_rs8_a.m_val)));
        }
        else
        {
            emit(_WO_EMIT_OP8_CMD(
                OPIA, 0, _WO_CMD24_I8_I8_I8(
                    dst_rs8.m_val, src_rs8_a.m_val, src_rs8_b.m_val)));
        }
    }
    void IRBuilder::subi(
        rs_adrsing8 dst_rs8,
        rs_adrsing8 src_rs8_a,
        rs_adrsing8 src_rs8_b) noexcept
    {
        if (dst_rs8.m_val == src_rs8_a.m_val)
        {
            emit(_WO_EMIT_OP8_CMD(
                OPCB, 1, _WO_CMD24_I8_I8_8(
                    dst_rs8.m_val, src_rs8_b.m_val)));
        }
        else
        {
            emit(_WO_EMIT_OP8_CMD(
                OPIA, 1, _WO_CMD24_I8_I8_I8(
                    dst_rs8.m_val, src_rs8_a.m_val, src_rs8_b.m_val)));
        }
    }
    void IRBuilder::muli(
        rs_adrsing8 dst_rs8,
        rs_adrsing8 src_rs8_a,
        rs_adrsing8 src_rs8_b) noexcept
    {
        if (dst_rs8.m_val == src_rs8_a.m_val)
        {
            emit(_WO_EMIT_OP8_CMD(
                OPCB, 2, _WO_CMD24_I8_I8_8(
                    dst_rs8.m_val, src_rs8_b.m_val)));
        }
        else if (dst_rs8.m_val == src_rs8_b.m_val)
        {
            emit(_WO_EMIT_OP8_CMD(
                OPCB, 2, _WO_CMD24_I8_I8_8(
                    dst_rs8.m_val, src_rs8_a.m_val)));
        }
        else
        {
            emit(_WO_EMIT_OP8_CMD(
                OPIA, 2, _WO_CMD24_I8_I8_I8(
                    dst_rs8.m_val, src_rs8_a.m_val, src_rs8_b.m_val)));
        }
    }
    void IRBuilder::divi(
        rs_adrsing8 dst_rs8,
        rs_adrsing8 src_rs8_a,
        rs_adrsing8 src_rs8_b) noexcept
    {
        if (dst_rs8.m_val == src_rs8_a.m_val)
        {
            emit(_WO_EMIT_OP8_CMD(
                OPCB, 3, _WO_CMD24_I8_I8_8(
                    dst_rs8.m_val, src_rs8_b.m_val)));
        }
        else
        {
            emit(_WO_EMIT_OP8_CMD(
                OPIA, 3, _WO_CMD24_I8_I8_I8(
                    dst_rs8.m_val, src_rs8_a.m_val, src_rs8_b.m_val)));
        }
    }
    void IRBuilder::modi(
        rs_adrsing8 dst_rs8,
        rs_adrsing8 src_rs8_a,
        rs_adrsing8 src_rs8_b) noexcept
    {
        if (dst_rs8.m_val == src_rs8_a.m_val)
        {
            emit(_WO_EMIT_OP8_CMD(
                OPCD, 2, _WO_CMD24_I8_I8_8(
                    dst_rs8.m_val, src_rs8_b.m_val)));
        }
        else
        {
            emit(_WO_EMIT_OP8_CMD(
                OPIB, 0, _WO_CMD24_I8_I8_I8(
                    dst_rs8.m_val, src_rs8_a.m_val, src_rs8_b.m_val)));
        }
    }
    void IRBuilder::negi(
        rs_adrsing8 dst_rs8,
        rs_adrsing8 src_rs8) noexcept
    {
        if (dst_rs8.m_val == src_rs8.m_val)
        {
            emit(_WO_EMIT_OP8_CMD(
                OPCE, 2, _WO_CMD24_I8_16(
                    dst_rs8.m_val)));
        }
        else
        {
            emit(_WO_EMIT_OP8_CMD(
                OPIB, 1, _WO_CMD24_I8_I8_8(
                    dst_rs8.m_val, src_rs8.m_val)));
        }
    }
    void IRBuilder::lti(
        rs_adrsing8 dst_rs8,
        rs_adrsing8 src_rs8_a,
        rs_adrsing8 src_rs8_b) noexcept
    {
        emit(_WO_EMIT_OP8_CMD(
            OPIB, 2, _WO_CMD24_I8_I8_I8(
                dst_rs8.m_val, src_rs8_a.m_val, src_rs8_b.m_val)));
    }
    void IRBuilder::gti(
        rs_adrsing8 dst_rs8,
        rs_adrsing8 src_rs8_a,
        rs_adrsing8 src_rs8_b) noexcept
    {
        emit(_WO_EMIT_OP8_CMD(
            OPIB, 3, _WO_CMD24_I8_I8_I8(
                dst_rs8.m_val, src_rs8_a.m_val, src_rs8_b.m_val)));
    }
    void IRBuilder::elti(
        rs_adrsing8 dst_rs8,
        rs_adrsing8 src_rs8_a,
        rs_adrsing8 src_rs8_b) noexcept
    {
        emit(_WO_EMIT_OP8_CMD(
            OPIC, 0, _WO_CMD24_I8_I8_I8(
                dst_rs8.m_val, src_rs8_a.m_val, src_rs8_b.m_val)));
    }
    void IRBuilder::egti(
        rs_adrsing8 dst_rs8,
        rs_adrsing8 src_rs8_a,
        rs_adrsing8 src_rs8_b) noexcept
    {
        emit(_WO_EMIT_OP8_CMD(
            OPIC, 1, _WO_CMD24_I8_I8_I8(
                dst_rs8.m_val, src_rs8_a.m_val, src_rs8_b.m_val)));
    }
    void IRBuilder::equb(
        rs_adrsing8 dst_rs8,
        rs_adrsing8 src_rs8_a,
        rs_adrsing8 src_rs8_b) noexcept
    {
        emit(_WO_EMIT_OP8_CMD(
            OPIC, 2, _WO_CMD24_I8_I8_I8(
                dst_rs8.m_val, src_rs8_a.m_val, src_rs8_b.m_val)));
    }
    void IRBuilder::nequb(
        rs_adrsing8 dst_rs8,
        rs_adrsing8 src_rs8_a,
        rs_adrsing8 src_rs8_b) noexcept
    {
        emit(_WO_EMIT_OP8_CMD(
            OPIC, 3, _WO_CMD24_I8_I8_I8(
                dst_rs8.m_val, src_rs8_a.m_val, src_rs8_b.m_val)));
    }

    void IRBuilder::addr(
        rs_adrsing8 dst_rs8,
        rs_adrsing8 src_rs8_a,
        rs_adrsing8 src_rs8_b) noexcept
    {
        if (dst_rs8.m_val == src_rs8_a.m_val)
        {
            emit(_WO_EMIT_OP8_CMD(
                OPCC, 0, _WO_CMD24_I8_I8_8(
                    dst_rs8.m_val, src_rs8_b.m_val)));
        }
        else if (dst_rs8.m_val == src_rs8_b.m_val)
        {
            emit(_WO_EMIT_OP8_CMD(
                OPCC, 0, _WO_CMD24_I8_I8_8(
                    dst_rs8.m_val, src_rs8_a.m_val)));
        }
        else
        {
            emit(_WO_EMIT_OP8_CMD(
                OPRA, 0, _WO_CMD24_I8_I8_I8(
                    dst_rs8.m_val, src_rs8_a.m_val, src_rs8_b.m_val)));
        }
    }
    void IRBuilder::subr(
        rs_adrsing8 dst_rs8,
        rs_adrsing8 src_rs8_a,
        rs_adrsing8 src_rs8_b) noexcept
    {
        if (dst_rs8.m_val == src_rs8_a.m_val)
        {
            emit(_WO_EMIT_OP8_CMD(
                OPCC, 1, _WO_CMD24_I8_I8_8(
                    dst_rs8.m_val, src_rs8_b.m_val)));
        }
        else
        {
            emit(_WO_EMIT_OP8_CMD(
                OPRA, 1, _WO_CMD24_I8_I8_I8(
                    dst_rs8.m_val, src_rs8_a.m_val, src_rs8_b.m_val)));
        }
    }
    void IRBuilder::mulr(
        rs_adrsing8 dst_rs8,
        rs_adrsing8 src_rs8_a,
        rs_adrsing8 src_rs8_b) noexcept
    {
        if (dst_rs8.m_val == src_rs8_a.m_val)
        {
            emit(_WO_EMIT_OP8_CMD(
                OPCC, 2, _WO_CMD24_I8_I8_8(
                    dst_rs8.m_val, src_rs8_b.m_val)));
        }
        else if (dst_rs8.m_val == src_rs8_b.m_val)
        {
            emit(_WO_EMIT_OP8_CMD(
                OPCC, 2, _WO_CMD24_I8_I8_8(
                    dst_rs8.m_val, src_rs8_a.m_val)));
        }
        else
        {
            emit(_WO_EMIT_OP8_CMD(
                OPRA, 2, _WO_CMD24_I8_I8_I8(
                    dst_rs8.m_val, src_rs8_a.m_val, src_rs8_b.m_val)));
        }
    }
    void IRBuilder::divr(
        rs_adrsing8 dst_rs8,
        rs_adrsing8 src_rs8_a,
        rs_adrsing8 src_rs8_b) noexcept
    {
        if (dst_rs8.m_val == src_rs8_a.m_val)
        {
            emit(_WO_EMIT_OP8_CMD(
                OPCC, 3, _WO_CMD24_I8_I8_8(
                    dst_rs8.m_val, src_rs8_b.m_val)));
        }
        else
        {
            emit(_WO_EMIT_OP8_CMD(
                OPRA, 3, _WO_CMD24_I8_I8_I8(
                    dst_rs8.m_val, src_rs8_a.m_val, src_rs8_b.m_val)));
        }
    }
    void IRBuilder::modr(
        rs_adrsing8 dst_rs8,
        rs_adrsing8 src_rs8_a,
        rs_adrsing8 src_rs8_b) noexcept
    {
        if (dst_rs8.m_val == src_rs8_a.m_val)
        {
            emit(_WO_EMIT_OP8_CMD(
                OPCD, 3, _WO_CMD24_I8_I8_8(
                    dst_rs8.m_val, src_rs8_b.m_val)));
        }
        else
        {
            emit(_WO_EMIT_OP8_CMD(
                OPRB, 0, _WO_CMD24_I8_I8_I8(
                    dst_rs8.m_val, src_rs8_a.m_val, src_rs8_b.m_val)));
        }
    }
    void IRBuilder::negr(
        rs_adrsing8 dst_rs8,
        rs_adrsing8 src_rs8) noexcept
    {
        if (dst_rs8.m_val == src_rs8.m_val)
        {
            emit(_WO_EMIT_OP8_CMD(
                OPCE, 3, _WO_CMD24_I8_16(
                    dst_rs8.m_val)));
        }
        else
        {
            emit(_WO_EMIT_OP8_CMD(
                OPRB, 1, _WO_CMD24_I8_I8_8(
                    dst_rs8.m_val, src_rs8.m_val)));
        }
    }
    void IRBuilder::ltr(
        rs_adrsing8 dst_rs8,
        rs_adrsing8 src_rs8_a,
        rs_adrsing8 src_rs8_b) noexcept
    {
        emit(_WO_EMIT_OP8_CMD(
            OPRB, 2, _WO_CMD24_I8_I8_I8(
                dst_rs8.m_val, src_rs8_a.m_val, src_rs8_b.m_val)));
    }
    void IRBuilder::gtr(
        rs_adrsing8 dst_rs8,
        rs_adrsing8 src_rs8_a,
        rs_adrsing8 src_rs8_b) noexcept
    {
        emit(_WO_EMIT_OP8_CMD(
            OPRB, 3, _WO_CMD24_I8_I8_I8(
                dst_rs8.m_val, src_rs8_a.m_val, src_rs8_b.m_val)));
    }
    void IRBuilder::eltr(
        rs_adrsing8 dst_rs8,
        rs_adrsing8 src_rs8_a,
        rs_adrsing8 src_rs8_b) noexcept
    {
        emit(_WO_EMIT_OP8_CMD(
            OPRC, 0, _WO_CMD24_I8_I8_I8(
                dst_rs8.m_val, src_rs8_a.m_val, src_rs8_b.m_val)));
    }
    void IRBuilder::egtr(
        rs_adrsing8 dst_rs8,
        rs_adrsing8 src_rs8_a,
        rs_adrsing8 src_rs8_b) noexcept
    {
        emit(_WO_EMIT_OP8_CMD(
            OPRC, 1, _WO_CMD24_I8_I8_I8(
                dst_rs8.m_val, src_rs8_a.m_val, src_rs8_b.m_val)));
    }
    void IRBuilder::equr(
        rs_adrsing8 dst_rs8,
        rs_adrsing8 src_rs8_a,
        rs_adrsing8 src_rs8_b) noexcept
    {
        emit(_WO_EMIT_OP8_CMD(
            OPRC, 2, _WO_CMD24_I8_I8_I8(
                dst_rs8.m_val, src_rs8_a.m_val, src_rs8_b.m_val)));
    }
    void IRBuilder::nequr(
        rs_adrsing8 dst_rs8,
        rs_adrsing8 src_rs8_a,
        rs_adrsing8 src_rs8_b) noexcept
    {
        emit(_WO_EMIT_OP8_CMD(
            OPRC, 3, _WO_CMD24_I8_I8_I8(
                dst_rs8.m_val, src_rs8_a.m_val, src_rs8_b.m_val)));
    }

    void IRBuilder::adds(
        rs_adrsing8 dst_rs8,
        rs_adrsing8 src_rs8_a,
        rs_adrsing8 src_rs8_b) noexcept
    {
        if (dst_rs8.m_val == src_rs8_a.m_val)
        {
            emit(_WO_EMIT_OP8_CMD(
                OPCD, 0, _WO_CMD24_I8_I8_8(
                    dst_rs8.m_val, src_rs8_b.m_val)));
        }
        else if (dst_rs8.m_val == src_rs8_b.m_val)
        {
            emit(_WO_EMIT_OP8_CMD(
                OPCD, 1, _WO_CMD24_I8_I8_8(
                    dst_rs8.m_val, src_rs8_a.m_val)));
        }
        else
        {
            emit(_WO_EMIT_OP8_CMD(
                OPSA, 0, _WO_CMD24_I8_I8_I8(
                    dst_rs8.m_val, src_rs8_a.m_val, src_rs8_b.m_val)));
        }
    }
    void IRBuilder::lts(
        rs_adrsing8 dst_rs8,
        rs_adrsing8 src_rs8_a,
        rs_adrsing8 src_rs8_b) noexcept
    {
        emit(_WO_EMIT_OP8_CMD(
            OPSA, 1, _WO_CMD24_I8_I8_I8(
                dst_rs8.m_val, src_rs8_a.m_val, src_rs8_b.m_val)));
    }
    void IRBuilder::gts(
        rs_adrsing8 dst_rs8,
        rs_adrsing8 src_rs8_a,
        rs_adrsing8 src_rs8_b) noexcept
    {
        emit(_WO_EMIT_OP8_CMD(
            OPSA, 2, _WO_CMD24_I8_I8_I8(
                dst_rs8.m_val, src_rs8_a.m_val, src_rs8_b.m_val)));
    }
    void IRBuilder::elts(
        rs_adrsing8 dst_rs8,
        rs_adrsing8 src_rs8_a,
        rs_adrsing8 src_rs8_b) noexcept
    {
        emit(_WO_EMIT_OP8_CMD(
            OPSA, 3, _WO_CMD24_I8_I8_I8(
                dst_rs8.m_val, src_rs8_a.m_val, src_rs8_b.m_val)));
    }
    void IRBuilder::egts(
        rs_adrsing8 dst_rs8,
        rs_adrsing8 src_rs8_a,
        rs_adrsing8 src_rs8_b) noexcept
    {
        emit(_WO_EMIT_OP8_CMD(
            OPSB, 0, _WO_CMD24_I8_I8_I8(
                dst_rs8.m_val, src_rs8_a.m_val, src_rs8_b.m_val)));
    }
    void IRBuilder::equs(
        rs_adrsing8 dst_rs8,
        rs_adrsing8 src_rs8_a,
        rs_adrsing8 src_rs8_b) noexcept
    {
        emit(_WO_EMIT_OP8_CMD(
            OPSB, 1, _WO_CMD24_I8_I8_I8(
                dst_rs8.m_val, src_rs8_a.m_val, src_rs8_b.m_val)));
    }
    void IRBuilder::nequs(
        rs_adrsing8 dst_rs8,
        rs_adrsing8 src_rs8_a,
        rs_adrsing8 src_rs8_b) noexcept
    {
        emit(_WO_EMIT_OP8_CMD(
            OPSB, 2, _WO_CMD24_I8_I8_I8(
                dst_rs8.m_val, src_rs8_a.m_val, src_rs8_b.m_val)));
    }

    void IRBuilder::cdivilr(
        rs_adrsing8 src_rs8_a,
        rs_adrsing8 src_rs8_b) noexcept
    {
        emit(_WO_EMIT_OP8_CMD(
            OPCA, 0, _WO_CMD24_I8_I8_8(
                src_rs8_a.m_val, src_rs8_b.m_val)));
    }
    void IRBuilder::cdivil(rs_adrsing8 src_rs8) noexcept
    {
        emit(_WO_EMIT_OP8_CMD(
            OPCA, 1, _WO_CMD24_I8_16(
                src_rs8.m_val)));
    }
    void IRBuilder::cdivir(rs_adrsing8 src_rs8) noexcept
    {
        emit(_WO_EMIT_OP8_CMD(
            OPCA, 2, _WO_CMD24_I8_16(
                src_rs8.m_val)));
    }
    void IRBuilder::cdivirz(rs_adrsing8 src_rs8) noexcept
    {
        emit(_WO_EMIT_OP8_CMD(
            OPCA, 3, _WO_CMD24_I8_16(
                src_rs8.m_val)));
    }

    void IRBuilder::land(
        rs_adrsing8 dst_rs8,
        rs_adrsing8 src_rs8_a,
        rs_adrsing8 src_rs8_b) noexcept
    {
        if (dst_rs8.m_val == src_rs8_a.m_val)
        {
            emit(_WO_EMIT_OP8_CMD(
                OPCE, 0, _WO_CMD24_I8_I8_8(
                    dst_rs8.m_val, src_rs8_b.m_val)));
        }
        else if (dst_rs8.m_val == src_rs8_b.m_val)
        {
            emit(_WO_EMIT_OP8_CMD(
                OPCE, 0, _WO_CMD24_I8_I8_8(
                    dst_rs8.m_val, src_rs8_a.m_val)));
        }
        else
        {
            emit(_WO_EMIT_OP8_CMD(
                OPLA, 0, _WO_CMD24_I8_I8_I8(
                    dst_rs8.m_val, src_rs8_a.m_val, src_rs8_b.m_val)));
        }
    }
    void IRBuilder::lor(
        rs_adrsing8 dst_rs8,
        rs_adrsing8 src_rs8_a,
        rs_adrsing8 src_rs8_b) noexcept
    {
        if (dst_rs8.m_val == src_rs8_a.m_val)
        {
            emit(_WO_EMIT_OP8_CMD(
                OPCE, 1, _WO_CMD24_I8_I8_8(
                    dst_rs8.m_val, src_rs8_b.m_val)));
        }
        else if (dst_rs8.m_val == src_rs8_b.m_val)
        {
            emit(_WO_EMIT_OP8_CMD(
                OPCE, 1, _WO_CMD24_I8_I8_8(
                    dst_rs8.m_val, src_rs8_a.m_val)));
        }
        else
        {
            emit(_WO_EMIT_OP8_CMD(
                OPLA, 1, _WO_CMD24_I8_I8_I8(
                    dst_rs8.m_val, src_rs8_a.m_val, src_rs8_b.m_val)));
        }
    }
    void IRBuilder::lnot(
        rs_adrsing8 dst_rs8,
        rs_adrsing8 src_rs8) noexcept
    {
        if (dst_rs8.m_val == src_rs8.m_val)
        {
            emit(_WO_EMIT_OP8_CMD(
                OPLA, 3, _WO_CMD24_I8_16(
                    dst_rs8.m_val)));
        }
        else
        {
            emit(_WO_EMIT_OP8_CMD(
                OPLA, 2, _WO_CMD24_I8_I8_8(
                    dst_rs8.m_val, src_rs8.m_val)));
        }
    }

    void IRBuilder::idstr(
        rs_adrsing8 dst_rs8,
        rs_adrsing8 cont_rs8,
        rs_adrsing8 idx_rs8) noexcept
    {
        emit(_WO_EMIT_OP8_CMD(
            IDX, 0, _WO_CMD24_I8_I8_I8(
                dst_rs8.m_val, cont_rs8.m_val, idx_rs8.m_val)));
    }
    void IRBuilder::idarr(
        rs_adrsing8 dst_rs8,
        rs_adrsing8 cont_rs8,
        rs_adrsing8 idx_rs8) noexcept
    {
        emit(_WO_EMIT_OP8_CMD(
            IDX, 1, _WO_CMD24_I8_I8_I8(
                dst_rs8.m_val, cont_rs8.m_val, idx_rs8.m_val)));
    }
    void IRBuilder::iddict(
        rs_adrsing8 dst_rs8,
        rs_adrsing8 cont_rs8,
        rs_adrsing8 idx_rs8) noexcept
    {
        emit(_WO_EMIT_OP8_CMD(
            IDX, 2, _WO_CMD24_I8_I8_I8(
                dst_rs8.m_val, cont_rs8.m_val, idx_rs8.m_val)));
    }
    void IRBuilder::idstruct(
        rs_adrsing8 dst_rs8,
        rs_adrsing8 cont_rs8,
        fixed_unsigned<32> idx_u32) noexcept
    {
        if (min_width(idx_u32.m_val) <= 8)
        {
            emit(_WO_EMIT_OP8_CMD(
                IDX, 3, _WO_CMD24_I8_I8_U8(
                    dst_rs8.m_val, cont_rs8.m_val, idx_u32.m_val)));
        }
        else
        {
            emit(_WO_EMIT_OP8_CMD(
                IDSTEXT, 0, _WO_CMD24_I8_I8_8(
                    dst_rs8.m_val, cont_rs8.m_val)));
            emit(_WO_EMIT_EXT_U32(idx_u32.m_val));
        }
    }

    void IRBuilder::sidarr(
        rs_adrsing8 src_rs8,
        rs_adrsing8 cont_rs8,
        rs_adrsing8 idx_rs8) noexcept
    {
        emit(_WO_EMIT_OP8_CMD(
            SIDX, 0, _WO_CMD24_I8_I8_I8(
                src_rs8.m_val, cont_rs8.m_val, idx_rs8.m_val)));
    }
    void IRBuilder::siddict(
        rs_adrsing8 src_rs8,
        rs_adrsing8 cont_rs8,
        rs_adrsing8 idx_rs8) noexcept
    {
        emit(_WO_EMIT_OP8_CMD(
            SIDX, 1, _WO_CMD24_I8_I8_I8(
                src_rs8.m_val, cont_rs8.m_val, idx_rs8.m_val)));
    }
    void IRBuilder::sidmap(
        rs_adrsing8 src_rs8,
        rs_adrsing8 cont_rs8,
        rs_adrsing8 idx_rs8) noexcept
    {
        emit(_WO_EMIT_OP8_CMD(
            SIDX, 2, _WO_CMD24_I8_I8_I8(
                src_rs8.m_val, cont_rs8.m_val, idx_rs8.m_val)));
    }
    void IRBuilder::sidstruct(
        rs_adrsing8 src_rs8,
        rs_adrsing8 cont_rs8,
        fixed_unsigned<32> idx_u32) noexcept
    {
        if (min_width(idx_u32.m_val) <= 8)
        {
            emit(_WO_EMIT_OP8_CMD(
                SIDX, 3, _WO_CMD24_I8_I8_U8(
                    src_rs8.m_val, cont_rs8.m_val, idx_u32.m_val)));
        }
        else
        {
            emit(_WO_EMIT_OP8_CMD(
                SIDSTEXT, 0, _WO_CMD24_I8_I8_8(
                    src_rs8.m_val, cont_rs8.m_val)));
            emit(_WO_EMIT_EXT_U32(idx_u32.m_val));
        }
    }

    void IRBuilder::jmp(Label* label) noexcept
    {
        if (label->m_bound_ip_offset.has_value())
        {
            // This label has been bound already.
            // Jump back, make a gc-checkpoint.

            label->apply_to_address(
                m_code_holder,
                emit(_WO_EMIT_OP8_CMD(
                    JMPGC, 0, _WO_CMD24_U24(0))),
                Label::ApplyAddress::Formal::SHIFT8_U24);
        }
        else
        {
            label->apply_to_address(
                m_code_holder,
                emit(_WO_EMIT_OP8_CMD(
                    JMP, 0, _WO_CMD24_U24(0))),
                Label::ApplyAddress::Formal::SHIFT8_U24);
        }
    }
    void IRBuilder::jmpf(Label* label) noexcept
    {
        if (label->m_bound_ip_offset.has_value())
        {
            // This label has been bound already.
            // Jump back, make a gc-checkpoint.

            label->apply_to_address(
                m_code_holder,
                emit(_WO_EMIT_OP8_CMD(
                    JMPGC, 2, _WO_CMD24_U24(0))),
                Label::ApplyAddress::Formal::SHIFT8_U24);
        }
        else
        {
            label->apply_to_address(
                m_code_holder,
                emit(_WO_EMIT_OP8_CMD(
                    JMP, 2, _WO_CMD24_U24(0))),
                Label::ApplyAddress::Formal::SHIFT8_U24);
        }
    }
    void IRBuilder::jmpt(Label* label) noexcept
    {
        if (label->m_bound_ip_offset.has_value())
        {
            // This label has been bound already.
            // Jump back, make a gc-checkpoint.

            label->apply_to_address(
                m_code_holder,
                emit(_WO_EMIT_OP8_CMD(
                    JMPGC, 3, _WO_CMD24_U24(0))),
                Label::ApplyAddress::Formal::SHIFT8_U24);
        }
        else
        {
            label->apply_to_address(
                m_code_holder,
                emit(_WO_EMIT_OP8_CMD(
                    JMP, 3, _WO_CMD24_U24(0))),
                Label::ApplyAddress::Formal::SHIFT8_U24);
        }
    }

    void IRBuilder::ret() noexcept
    {
        emit(_WO_EMIT_OP8_CMD(RET, 0, _WO_CMD24_24()));
    }
    void IRBuilder::retn(fixed_unsigned<16> count_u16) noexcept
    {
        const auto width = min_width(count_u16.m_val);
        if (width <= 8)
        {
            emit(_WO_EMIT_OP8_CMD(RET, 1,
                _WO_CMD24_U8_16(count_u16.m_val)));
        }
        else
        {
            emit(_WO_EMIT_OP8_CMD(RET, 2,
                _WO_CMD24_8_U16(count_u16.m_val)));
        }
    }

    void IRBuilder::calln(Label* label) noexcept
    {
        label->apply_to_address(
            m_code_holder,
            emit(_WO_EMIT_OP8_CMD(
                CALLN, 0, _WO_CMD24_U24(0))),
            Label::ApplyAddress::Formal::SHIFT8_U24);
        emit(_WO_EMIT_EXT_U32(0));
    }
    void IRBuilder::callnfp(wo_native_func_t extfunc) noexcept
    {
        const uint64_t fpaddr = static_cast<uint64_t>(
            reinterpret_cast<intptr_t>(
                reinterpret_cast<void*>(extfunc)));

        const uint32_t fpaddr_hi =
            static_cast<uint32_t>((fpaddr >> 32) & 0xFFFFFFFFu);
        const uint32_t fpaddr_lo =
            static_cast<uint32_t>(fpaddr & 0xFFFFFFFFu);

        emit(_WO_EMIT_OP8_CMD(CALLN, 3, _WO_CMD24_U24(fpaddr_hi)));
        emit(_WO_EMIT_EXT_U32(fpaddr_lo));
    }
    void IRBuilder::call(rs_adrsing8 src_rs8) noexcept
    {
        emit(_WO_EMIT_OP8_CMD(
            CALL, 0, _WO_CMD24_I8_16(src_rs8.m_val)));
    }

    void IRBuilder::panic(rs_adrsing8 src_rs8) noexcept
    {
        emit(_WO_EMIT_OP8_CMD(
            PANIC, 0, _WO_CMD24_I8_16(src_rs8.m_val)));
    }
    void IRBuilder::panic(cg_adrsing32 src_cg32) noexcept
    {
        if (min_width(src_cg32.m_val) <= 24)
        {
            emit(_WO_EMIT_OP8_CMD(
                PANIC, 1, _WO_CMD24_U24(src_cg32.m_val)));
        }
        else
        {
            emit(_WO_EMIT_OP8_CMD(PANIC, 2, 0));
            emit(_WO_EMIT_EXT_I32(src_cg32.m_val));
        }
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    shared_pointer<runtime_env> IRBuilder::finish()
    {
        shared_pointer<runtime_env> result(new runtime_env());

        result->rt_code_len =
            sizeof(uint32_t) * m_code_holder_size;
        result->rt_codes =
            reinterpret_cast<const byte_t*>(
                realloc(
                    m_code_holder,
                    sizeof(uint32_t) * m_code_holder_size));

        // TMP IMPL;
        result->constant_value_count =
            static_cast<size_t>(m_constant_storage_size);
        result->constant_and_global_value_takeplace_count =
            result->constant_value_count + 1 /* Always keep 0 for reserving */;
        result->constant_and_global_storage =
            reinterpret_cast<value*>(
                realloc(
                    m_constant_storage,
                    sizeof(value) * result->constant_and_global_value_takeplace_count));

        // Reset builder state.
        m_code_holder = nullptr;
        m_code_holder_capacity = 0;
        m_code_holder_size = 0;

        m_constant_storage = nullptr;
        m_constant_storage_capacity = 0;
        m_constant_storage_size = 0;

#ifndef NDEBUG
        for (auto* label : m_created_labels)
        {
            wo_assert(label->m_bound_ip_offset.has_value(),
                "All label must be bind.");
        }
#endif

        return result;
    }
}