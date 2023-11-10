#include "wo_compiler_ir.hpp"
#include "wo_lang_ast_builder.hpp"

namespace wo
{
    void program_debug_data_info::generate_debug_info_at_astnode(grammar::ast_base* ast_node, ir_compiler* compiler)
    {
        // funcdef should not genrate val..
        if (ast_node->source_file)
        {
            bool unbreakable = dynamic_cast<ast::ast_value_function_define*>(ast_node)
                || dynamic_cast<ast::ast_list*>(ast_node)
                || dynamic_cast<ast::ast_namespace*>(ast_node)
                || dynamic_cast<ast::ast_sentence_block*>(ast_node)
                || dynamic_cast<ast::ast_if*>(ast_node)
                || dynamic_cast<ast::ast_while*>(ast_node)
                || dynamic_cast<ast::ast_forloop*>(ast_node)
                || dynamic_cast<ast::ast_foreach*>(ast_node)
                || dynamic_cast<ast::ast_match*>(ast_node)
                ;

            auto& location_list_of_file = _general_src_data_buf_a[*ast_node->source_file];

            location loc = {
                compiler->get_now_ip(),
                ast_node->row_begin_no,
                ast_node->col_begin_no,
                ast_node->row_end_no,
                ast_node->col_end_no,
                *ast_node->source_file,
                unbreakable
            };

            location_list_of_file.push_back(loc);
            _general_src_data_buf_b[compiler->get_now_ip()] = loc;
        }
    }
    void program_debug_data_info::finalize_generate_debug_info()
    {
    }
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

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    runtime_env::~runtime_env()
    {
        free_jit(this);

        gcbase::unit_attrib cancel_nogc;
        cancel_nogc.m_gc_age = 0;
        cancel_nogc.m_marked = (uint8_t)gcbase::gcmarkcolor::full_mark;
        cancel_nogc.m_alloc_mask = 0;
        cancel_nogc.m_nogc = 0;

        for (size_t ci = 0; ci < constant_value_count; ++ci)
            if (constant_global_reg_rtstack[ci].is_gcunit())
            {
                wo_assert(constant_global_reg_rtstack[ci].type == wo::value::valuetype::string_type);

                gcbase::unit_attrib* attrib;
                constant_global_reg_rtstack[ci].get_gcunit_with_barrier(&attrib);
                attrib->m_attr = cancel_nogc.m_attr;
            }

        if (constant_global_reg_rtstack)
            free(constant_global_reg_rtstack);

        if (rt_codes)
            free((byte_t*)rt_codes);
    }

    std::tuple<void*, size_t> runtime_env::create_env_binary(bool savepdi) noexcept
    {
        std::vector<wo::byte_t> binary_buffer;
        auto write_buffer_to_buffer = [&binary_buffer](const void* written_data, size_t written_length, size_t allign) {
            const size_t write_begin_place = binary_buffer.size();

            wo_assert(write_begin_place % allign == 0);

            binary_buffer.resize(write_begin_place + written_length);

            memcpy(binary_buffer.data() + write_begin_place, written_data, written_length);
            return binary_buffer.size();
        };

        auto write_binary_to_buffer = [&write_buffer_to_buffer](const auto& d, size_t size_for_assert) {
            const wo::byte_t* written_data = std::launder(reinterpret_cast<const wo::byte_t*>(&d));
            const size_t written_length = sizeof(d);

            wo_assert(written_length == size_for_assert);

            return write_buffer_to_buffer(written_data, written_length, sizeof(d) > 8 ? 8 : sizeof(d));
        };

        std::vector<const char*> constant_string_pool;

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
        size_t string_buffer_size = 0;
        write_binary_to_buffer(
            (uint64_t)this->constant_value_count, 8);
        for (size_t ci = 0; ci < this->constant_value_count; ++ci)
        {
            auto& constant_value = this->constant_global_reg_rtstack[ci];
            wo_assert(constant_value.type == wo::value::valuetype::integer_type
                || constant_value.type == wo::value::valuetype::real_type
                || constant_value.type == wo::value::valuetype::handle_type
                || constant_value.type == wo::value::valuetype::bool_type
                || constant_value.type == wo::value::valuetype::string_type
                || constant_value.type == wo::value::valuetype::invalid);

            write_binary_to_buffer((uint64_t)constant_value.type_space, 8);

            if (constant_value.type == wo::value::valuetype::string_type)
            {
                // Record for string
                write_binary_to_buffer((uint32_t)string_buffer_size, 4);
                constant_string_pool.push_back(constant_value.string->c_str());
                write_binary_to_buffer((uint32_t)constant_value.string->size(), 4);

                string_buffer_size += constant_value.string->size();
            }
            else
                // Record for value
                write_binary_to_buffer((uint64_t)constant_value.value_space, 8);

            if (constant_value.type == wo::value::valuetype::handle_type)
            {
                // Check if constant_value is function address from native?
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

        // 4.1 Extern native function information
        //  4.1.1 Extern function libname & symbname & used constant offset / code offset
        write_binary_to_buffer((uint64_t)this->extern_native_functions.size(), 8);
        for (auto& [funcptr, extfuncloc] : this->extern_native_functions)
        {
            // 4.1.1.1 extern script name
            write_binary_to_buffer((uint32_t)string_buffer_size, 4);
            constant_string_pool.push_back(extfuncloc.script_name.c_str());
            write_binary_to_buffer((uint32_t)extfuncloc.script_name.size(), 4);
            string_buffer_size += extfuncloc.script_name.size();

            // 4.1.1.2 extern library name
            write_binary_to_buffer((uint32_t)string_buffer_size, 4);
            constant_string_pool.push_back(extfuncloc.library_name.c_str());
            write_binary_to_buffer((uint32_t)extfuncloc.library_name.size(), 4);
            string_buffer_size += extfuncloc.library_name.size();

            // 4.1.1.3 extern function name
            write_binary_to_buffer((uint32_t)string_buffer_size, 4);
            constant_string_pool.push_back(extfuncloc.function_name.c_str());
            write_binary_to_buffer((uint32_t)extfuncloc.function_name.size(), 4);
            string_buffer_size += extfuncloc.function_name.size();

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
            // 4.2.1.1 extern function name
            write_binary_to_buffer((uint32_t)string_buffer_size, 4);
            constant_string_pool.push_back(funcname.c_str());
            write_binary_to_buffer((uint32_t)funcname.size(), 4);
            string_buffer_size += funcname.size();

            write_binary_to_buffer((uint64_t)offset, 8);
        }

        // 5.1 JIT Informations
        write_binary_to_buffer((uint64_t)_functions_offsets_for_jit.size(), 8);
        for (size_t function_offset : _functions_offsets_for_jit)
        {
            write_binary_to_buffer((uint64_t)function_offset, 8);
        }

        write_binary_to_buffer((uint64_t)_functions_def_constant_idx_for_jit.size(), 8);
        for (size_t function_constant_offset : _functions_def_constant_idx_for_jit)
        {
            write_binary_to_buffer((uint64_t)function_constant_offset, 8);
        }

        write_binary_to_buffer((uint64_t)_calln_opcode_offsets_for_jit.size(), 8);
        for (size_t calln_offset : _calln_opcode_offsets_for_jit)
        {
            write_binary_to_buffer((uint64_t)calln_offset, 8);
        }

        write_binary_to_buffer((uint64_t)_mkclos_opcode_offsets_for_jit.size(), 8);
        for (size_t mkclos_offset : _mkclos_opcode_offsets_for_jit)
        {
            write_binary_to_buffer((uint64_t)mkclos_offset, 8);
        }

        // 6.1 Constant string buffer
        size_t padding_length_for_constant_string_buf = (8ull - (string_buffer_size % 8ull)) % 8ull;

        write_binary_to_buffer(
            (uint64_t)(string_buffer_size + padding_length_for_constant_string_buf), 8);

        for (size_t si = 0; si < constant_string_pool.size(); ++si)
            write_buffer_to_buffer(constant_string_pool[si], strlen(constant_string_pool[si]), 1);

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
                write_binary_to_buffer((uint32_t)func_symb_info.in_stack_reg_count, 4);

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

    shared_pointer<runtime_env> runtime_env::_create_from_stream(binary_source_stream* stream, size_t stackcount, wo_string_t* out_reason, bool* out_is_binary)
    {
        *out_is_binary = true;

#define WO_LOAD_BIN_FAILED(reason) do{*out_reason = reason; return nullptr;}while(0)
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
        result->constant_and_global_value_takeplace_count = (size_t)(constant_value_count + global_value_count);
        result->constant_value_count = (size_t)constant_value_count;

        result->runtime_stack_count = stackcount;

        size_t preserve_memory_size =
            result->constant_and_global_value_takeplace_count
            + (size_t)register_count
            + result->runtime_stack_count;

        value* preserved_memory = (value*)malloc(preserve_memory_size * sizeof(value));
        memset(preserved_memory, 0, preserve_memory_size * sizeof(value));

        result->constant_global_reg_rtstack = preserved_memory;
        result->reg_begin = result->constant_global_reg_rtstack
            + result->constant_and_global_value_takeplace_count;

        result->stack_begin = result->constant_global_reg_rtstack + (preserve_memory_size - 1);

        struct string_buffer_index
        {
            uint32_t index;
            uint32_t size;
        };
        std::map<uint64_t, string_buffer_index> constant_string_index_for_update;

        for (uint64_t ci = 0; ci < constant_value_count; ++ci)
        {
            uint64_t constant_type_scope, constant_value_scope;
            if (!stream->read_elem(&constant_type_scope))
                WO_LOAD_BIN_FAILED("Failed to restore constant type.");

            preserved_memory[ci].type_space = constant_type_scope;
            if (preserved_memory[ci].type == wo::value::valuetype::string_type)
            {
                uint32_t constant_string_pool_loc, constant_string_pool_size;
                if (!stream->read_elem(&constant_string_pool_loc) || !stream->read_elem(&constant_string_pool_size))
                    WO_LOAD_BIN_FAILED("Failed to restore constant string.");

                auto& loc = constant_string_index_for_update[ci];
                loc.index = constant_string_pool_loc;
                loc.size = constant_string_pool_size;
            }
            else
            {
                if (!stream->read_elem(&constant_value_scope))
                    WO_LOAD_BIN_FAILED("Failed to restore constant value.");
                preserved_memory[ci].value_space = constant_value_scope;
            }
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
            result->_functions_offsets_for_jit.push_back((size_t)offset);
        }

        uint64_t _functions_constant_offsets_count = 0;
        if (!stream->read_elem(&_functions_constant_offsets_count))
            WO_LOAD_BIN_FAILED("Failed to restore functions constant offset count.");
        for (uint64_t i = 0; i < _functions_constant_offsets_count; ++i)
        {
            uint64_t offset = 0;
            if (!stream->read_elem(&offset))
                WO_LOAD_BIN_FAILED("Failed to restore functions offset.");
            result->_functions_def_constant_idx_for_jit.push_back((size_t)offset);
        }

        uint64_t _calln_opcode_offsets_count = 0;
        if (!stream->read_elem(&_calln_opcode_offsets_count))
            WO_LOAD_BIN_FAILED("Failed to restore calln offset count.");
        for (uint64_t i = 0; i < _calln_opcode_offsets_count; ++i)
        {
            uint64_t offset = 0;
            if (!stream->read_elem(&offset))
                WO_LOAD_BIN_FAILED("Failed to restore calln offset.");
            result->_calln_opcode_offsets_for_jit.push_back((size_t)offset);
        }

        uint64_t _mkclos_opcode_offsets_count = 0;
        if (!stream->read_elem(&_mkclos_opcode_offsets_count))
            WO_LOAD_BIN_FAILED("Failed to restore mkclos offset count.");
        for (uint64_t i = 0; i < _mkclos_opcode_offsets_count; ++i)
        {
            uint64_t offset = 0;
            if (!stream->read_elem(&offset))
                WO_LOAD_BIN_FAILED("Failed to restore mkclos offset.");
            result->_mkclos_opcode_offsets_for_jit.push_back((size_t)offset);
        }

        // 6.1 Constant string buffer
        uint64_t string_buffer_size_with_padding;
        if (!stream->read_elem(&string_buffer_size_with_padding))
            WO_LOAD_BIN_FAILED("Failed to restore string buffer size.");

        auto string_buffer_begin_offset = stream->readed_offset();

        auto restore_string_from_buffer = [stream, string_buffer_begin_offset](const string_buffer_index& string_index, std::string* out_str)->bool {
            auto current_string_begin_idx = stream->readed_offset() - string_buffer_begin_offset;
            if (string_index.index != current_string_begin_idx)
                return false;

            std::vector<char> tmp_string_buffer(string_index.size + 1, 0);
            stream->read_buffer(tmp_string_buffer.data(), string_index.size);

            *out_str = tmp_string_buffer.data();
            return true;
        };

        std::string constant_string;
        for (auto& [constant_offset, string_index] : constant_string_index_for_update)
        {
            wo_assert(preserved_memory[constant_offset].type == wo::value::valuetype::string_type);

            if (!restore_string_from_buffer(string_index, &constant_string))
                WO_LOAD_BIN_FAILED("Failed to restore string from string buffer.");

            preserved_memory[constant_offset].set_gcunit<wo::value::valuetype::string_type>(
                string_t::gc_new<gcbase::gctype::no_gc>(
                    constant_string.c_str()));
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

            wo_native_func func = nullptr;

            if (library_name == "")
                func = rslib_extern_symbols::get_global_symbol(function_name.c_str());
            else
                func = result->loaded_libs.try_load_func_from_in(script_path.c_str(), library_name.c_str(), function_name.c_str());

            if (func == nullptr)
                WO_LOAD_BIN_FAILED("Failed to restore native function, might be changed?");

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

                if (!stream->read_elem(&data))
                    WO_LOAD_BIN_FAILED("Failed to restore program debug informations record C's function in_stack_reg_count.");
                function_inform.in_stack_reg_count = data;

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

    shared_pointer<runtime_env> runtime_env::load_create_env_from_binary(
        wo_string_t virtual_file,
        const void* bytestream,
        size_t streamsz,
        size_t stack_count,
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
        return _create_from_stream(&buf, stack_count, out_reason, out_is_binary);
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    shared_pointer<runtime_env> ir_compiler::finalize(size_t stacksz)
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
        size_t runtime_stack_count = 1024;  // by default

        size_t preserve_memory_size =
            constant_value_count
            + 1
            + global_value_count
            + 1
            + real_register_count
            + runtime_stack_count;

        value* preserved_memory = (value*)malloc(preserve_memory_size * sizeof(value));

        cxx_vec_t<byte_t> generated_runtime_code_buf; // It will be put to 16 byte allign mem place.

        std::map<std::string, cxx_vec_t<size_t>> jmp_record_table;
        std::map<std::string, cxx_vec_t<size_t>> jmp_record_table_for_immtag;
        std::map<std::string, uint32_t> tag_offset_vector_table;

        wo_assert(preserved_memory, "Alloc memory fail.");

        memset(preserved_memory, 0, preserve_memory_size * sizeof(value));
        //  // Fill constant
        for (auto constant_record : constant_record_list)
        {
            wo_assert((size_t)constant_record->constant_index < constant_value_count,
                "Constant index out of range.");

            constant_record->apply(&preserved_memory[constant_record->constant_index]);

            if (auto* addr_tagimm_rsfunc = dynamic_cast<opnum::tagimm_rsfunc*>(constant_record))
            {
                jmp_record_table_for_immtag[addr_tagimm_rsfunc->name].push_back(
                    constant_record->constant_index);
            }
        }

        // 2. Generate code
        shared_pointer<runtime_env> env = new runtime_env();

        for (size_t ip = 0; ip < ir_command_buffer.size(); ip++)
        {

#define WO_IR ir_command_buffer[ip]
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

            cxx_vec_t<byte_t> temp_this_command_code_buf; // one command will be store here tempery for coding allign

            switch (WO_IR.opcode)
            {
            case instruct::opcode::nop:
                temp_this_command_code_buf.push_back(WO_OPCODE(nop));
                break;
            case instruct::opcode::mov:
                temp_this_command_code_buf.push_back(WO_OPCODE(mov));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                break;
            case instruct::opcode::movcast:
                temp_this_command_code_buf.push_back(WO_OPCODE(movcast));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                temp_this_command_code_buf.push_back((byte_t)WO_IR.opinteger);
                break;
            case instruct::opcode::typeas:

                if (WO_IR.ext_page_id)
                {
                    temp_this_command_code_buf.push_back(WO_OPCODE(typeas) | 0b01);
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    temp_this_command_code_buf.push_back((byte_t)WO_IR.opinteger);
                }
                else
                {
                    temp_this_command_code_buf.push_back(WO_OPCODE(typeas));
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                    temp_this_command_code_buf.push_back((byte_t)WO_IR.opinteger);
                }
                break;
            case instruct::opcode::psh:
                if (nullptr == WO_IR.op1)
                {
                    if (WO_IR.opinteger == 0)
                        break;

                    temp_this_command_code_buf.push_back(WO_OPCODE(psh, 00));

                    wo_assert(WO_IR.opinteger > 0 && WO_IR.opinteger <= UINT16_MAX
                        , "Invalid count to reserve in stack.");

                    uint16_t opushort = (uint16_t)WO_IR.opinteger;
                    byte_t* readptr = (byte_t*)&opushort;
                    temp_this_command_code_buf.push_back(readptr[0]);
                    temp_this_command_code_buf.push_back(readptr[1]);
                }
                else
                {
                    temp_this_command_code_buf.push_back(WO_OPCODE(psh) | 0b01);
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                }
                break;
            case instruct::opcode::pop:
                if (nullptr == WO_IR.op1)
                {
                    if (WO_IR.opinteger == 0)
                        break;

                    temp_this_command_code_buf.push_back(WO_OPCODE(pop, 00));

                    wo_assert(WO_IR.opinteger > 0 && WO_IR.opinteger <= UINT16_MAX
                        , "Invalid count to pop from stack.");

                    uint16_t opushort = (uint16_t)WO_IR.opinteger;
                    byte_t* readptr = (byte_t*)&opushort;

                    temp_this_command_code_buf.push_back(readptr[0]);
                    temp_this_command_code_buf.push_back(readptr[1]);
                }
                else
                {
                    temp_this_command_code_buf.push_back(WO_OPCODE(pop) | 0b01);
                    WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                }
                break;
            case instruct::opcode::lds:
                temp_this_command_code_buf.push_back(WO_OPCODE(lds));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                break;
            case instruct::opcode::sts:
                temp_this_command_code_buf.push_back(WO_OPCODE(sts));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                break;
            case instruct::opcode::addi:
                temp_this_command_code_buf.push_back(WO_OPCODE(addi));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                break;
            case instruct::opcode::subi:
                temp_this_command_code_buf.push_back(WO_OPCODE(subi));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                break;
            case instruct::opcode::muli:
                temp_this_command_code_buf.push_back(WO_OPCODE(muli));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                break;
            case instruct::opcode::divi:
                temp_this_command_code_buf.push_back(WO_OPCODE(divi));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                break;
            case instruct::opcode::modi:
                temp_this_command_code_buf.push_back(WO_OPCODE(modi));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                break;
            case instruct::opcode::equr:
                temp_this_command_code_buf.push_back(WO_OPCODE(equr));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);

                break;
            case instruct::opcode::nequr:
                temp_this_command_code_buf.push_back(WO_OPCODE(nequr));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);

                break;
            case instruct::opcode::equs:
                temp_this_command_code_buf.push_back(WO_OPCODE(equs));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);

                break;
            case instruct::opcode::nequs:
                temp_this_command_code_buf.push_back(WO_OPCODE(nequs));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);

                break;
            case instruct::opcode::addr:
                temp_this_command_code_buf.push_back(WO_OPCODE(addr));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                break;
            case instruct::opcode::subr:
                temp_this_command_code_buf.push_back(WO_OPCODE(subr));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                break;
            case instruct::opcode::mulr:
                temp_this_command_code_buf.push_back(WO_OPCODE(mulr));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                break;
            case instruct::opcode::divr:
                temp_this_command_code_buf.push_back(WO_OPCODE(divr));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                break;
            case instruct::opcode::modr:
                temp_this_command_code_buf.push_back(WO_OPCODE(modr));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                break;
            case instruct::opcode::addh:
                temp_this_command_code_buf.push_back(WO_OPCODE(addh));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                break;
            case instruct::opcode::subh:
                temp_this_command_code_buf.push_back(WO_OPCODE(subh));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                break;
            case instruct::opcode::adds:
                temp_this_command_code_buf.push_back(WO_OPCODE(adds));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                break;

            case instruct::opcode::equb:
                temp_this_command_code_buf.push_back(WO_OPCODE(equb));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                break;
            case instruct::opcode::nequb:
                temp_this_command_code_buf.push_back(WO_OPCODE(nequb));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                break;
            case instruct::opcode::land:
                temp_this_command_code_buf.push_back(WO_OPCODE(land));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                break;
            case instruct::opcode::lor:
                temp_this_command_code_buf.push_back(WO_OPCODE(lor));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                break;
            case instruct::opcode::gti:
                temp_this_command_code_buf.push_back(WO_OPCODE(gti));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                break;
            case instruct::opcode::lti:
                temp_this_command_code_buf.push_back(WO_OPCODE(lti));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                break;
            case instruct::opcode::egti:
                temp_this_command_code_buf.push_back(WO_OPCODE(egti));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                break;
            case instruct::opcode::elti:
                temp_this_command_code_buf.push_back(WO_OPCODE(elti));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                break;

            case instruct::opcode::gtr:
                temp_this_command_code_buf.push_back(WO_OPCODE(gtr));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                break;
            case instruct::opcode::ltr:
                temp_this_command_code_buf.push_back(WO_OPCODE(ltr));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                break;
            case instruct::opcode::egtr:
                temp_this_command_code_buf.push_back(WO_OPCODE(egtr));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                break;
            case instruct::opcode::eltr:
                temp_this_command_code_buf.push_back(WO_OPCODE(eltr));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                break;

            case instruct::opcode::gtx:
                temp_this_command_code_buf.push_back(WO_OPCODE(gtx));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                break;
            case instruct::opcode::ltx:
                temp_this_command_code_buf.push_back(WO_OPCODE(ltx));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                break;
            case instruct::opcode::egtx:
                temp_this_command_code_buf.push_back(WO_OPCODE(egtx));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                break;
            case instruct::opcode::eltx:
                temp_this_command_code_buf.push_back(WO_OPCODE(eltx));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                break;
            case instruct::opcode::mkstruct:
            {
                temp_this_command_code_buf.push_back(WO_OPCODE(mkstruct));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);

                uint16_t size = (uint16_t)(WO_IR.opinteger);
                byte_t* readptr = (byte_t*)&size;
                temp_this_command_code_buf.push_back(readptr[0]);
                temp_this_command_code_buf.push_back(readptr[1]);
                break;
            }
            case instruct::opcode::idstruct:
            {
                temp_this_command_code_buf.push_back(WO_OPCODE(idstruct));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);

                uint16_t size = (uint16_t)(WO_IR.opinteger);
                byte_t* readptr = (byte_t*)&size;
                temp_this_command_code_buf.push_back(readptr[0]);
                temp_this_command_code_buf.push_back(readptr[1]);
                break;
            }
            case instruct::opcode::mkarr:
            {
                temp_this_command_code_buf.push_back(WO_OPCODE(mkarr));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);

                uint16_t size = (uint16_t)(WO_IR.opinteger);
                byte_t* readptr = (byte_t*)&size;
                temp_this_command_code_buf.push_back(readptr[0]);
                temp_this_command_code_buf.push_back(readptr[1]);
                break;
            }
            case instruct::opcode::mkmap:
            {
                temp_this_command_code_buf.push_back(WO_OPCODE(mkmap));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);

                uint16_t size = (uint16_t)(WO_IR.opinteger);
                byte_t* readptr = (byte_t*)&size;
                temp_this_command_code_buf.push_back(readptr[0]);
                temp_this_command_code_buf.push_back(readptr[1]);
                break;
            }
            case instruct::opcode::idarr:
                temp_this_command_code_buf.push_back(WO_OPCODE(idarr));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                break;
            case instruct::opcode::iddict:
                temp_this_command_code_buf.push_back(WO_OPCODE(iddict));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                break;
            case instruct::opcode::idstr:
                temp_this_command_code_buf.push_back(WO_OPCODE(idstr));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                break;
            case instruct::opcode::siddict:
                temp_this_command_code_buf.push_back(WO_OPCODE(siddict));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                opnum::reg((uint8_t)WO_IR.opinteger).generate_opnum_to_buffer(temp_this_command_code_buf);
                break;
            case instruct::opcode::sidmap:
                temp_this_command_code_buf.push_back(WO_OPCODE(sidmap));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                opnum::reg((uint8_t)WO_IR.opinteger).generate_opnum_to_buffer(temp_this_command_code_buf);
                break;
            case instruct::opcode::sidarr:
                temp_this_command_code_buf.push_back(WO_OPCODE(sidarr));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                opnum::reg((uint8_t)WO_IR.opinteger).generate_opnum_to_buffer(temp_this_command_code_buf);
                break;
            case instruct::opcode::sidstruct:
            {
                temp_this_command_code_buf.push_back(WO_OPCODE(sidstruct));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);

                uint16_t size = (uint16_t)(WO_IR.opinteger);
                byte_t* readptr = (byte_t*)&size;
                temp_this_command_code_buf.push_back(readptr[0]);
                temp_this_command_code_buf.push_back(readptr[1]);
                break;
            }
            case instruct::opcode::jt:
                temp_this_command_code_buf.push_back(WO_OPCODE(jt));

                wo_assert(dynamic_cast<opnum::tag*>(WO_IR.op1) != nullptr, "Operator num should be a tag.");

                jmp_record_table[dynamic_cast<opnum::tag*>(WO_IR.op1)->name]
                    .push_back(generated_runtime_code_buf.size() + 1);

                temp_this_command_code_buf.push_back(0x00);
                temp_this_command_code_buf.push_back(0x00);
                temp_this_command_code_buf.push_back(0x00);
                temp_this_command_code_buf.push_back(0x00);


                break;
            case instruct::opcode::jf:
                temp_this_command_code_buf.push_back(WO_OPCODE(jf));

                wo_assert(dynamic_cast<opnum::tag*>(WO_IR.op1) != nullptr, "Operator num should be a tag.");

                jmp_record_table[dynamic_cast<opnum::tag*>(WO_IR.op1)->name]
                    .push_back(generated_runtime_code_buf.size() + 1);

                temp_this_command_code_buf.push_back(0x00);
                temp_this_command_code_buf.push_back(0x00);
                temp_this_command_code_buf.push_back(0x00);
                temp_this_command_code_buf.push_back(0x00);
                break;

            case instruct::opcode::jmp:
                temp_this_command_code_buf.push_back(WO_OPCODE(jmp, 00));

                wo_assert(dynamic_cast<opnum::tag*>(WO_IR.op1) != nullptr, "Operator num should be a tag.");

                jmp_record_table[dynamic_cast<opnum::tag*>(WO_IR.op1)->name]
                    .push_back(generated_runtime_code_buf.size() + 1);

                temp_this_command_code_buf.push_back(0x00);
                temp_this_command_code_buf.push_back(0x00);
                temp_this_command_code_buf.push_back(0x00);
                temp_this_command_code_buf.push_back(0x00);
                break;

            case instruct::opcode::call:
                temp_this_command_code_buf.push_back(WO_OPCODE(call));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                break;
            case instruct::opcode::calln:
                if (WO_IR.op2)
                {
                    env->_calln_opcode_offsets_for_jit.push_back(generated_runtime_code_buf.size());

                    wo_assert(dynamic_cast<opnum::tag*>(WO_IR.op2) != nullptr, "Operator num should be a tag.");

                    temp_this_command_code_buf.push_back(WO_OPCODE(calln, 00));

                    jmp_record_table[dynamic_cast<opnum::tag*>(WO_IR.op2)->name]
                        .push_back(generated_runtime_code_buf.size() + 1);

                    temp_this_command_code_buf.push_back(0x00);
                    temp_this_command_code_buf.push_back(0x00);
                    temp_this_command_code_buf.push_back(0x00);
                    temp_this_command_code_buf.push_back(0x00);

                    // reserve...
                    temp_this_command_code_buf.push_back(0x00);
                    temp_this_command_code_buf.push_back(0x00);
                    temp_this_command_code_buf.push_back(0x00);
                    temp_this_command_code_buf.push_back(0x00);
                }
                else if (WO_IR.op1)
                {
                    if(WO_IR.opinteger)
                        temp_this_command_code_buf.push_back(WO_OPCODE(calln, 11));
                    else
                        temp_this_command_code_buf.push_back(WO_OPCODE(calln, 01));

                    uint64_t addr = (uint64_t)(WO_IR.op1);

                    wo_assert(!extern_native_functions[(intptr_t)addr].function_name.empty());
                    extern_native_functions[(intptr_t)addr].caller_offset_in_ir
                        .push_back(generated_runtime_code_buf.size());

                    byte_t* readptr = (byte_t*)&addr;
                    temp_this_command_code_buf.push_back(readptr[0]);
                    temp_this_command_code_buf.push_back(readptr[1]);
                    temp_this_command_code_buf.push_back(readptr[2]);
                    temp_this_command_code_buf.push_back(readptr[3]);
                    temp_this_command_code_buf.push_back(readptr[4]);
                    temp_this_command_code_buf.push_back(readptr[5]);
                    temp_this_command_code_buf.push_back(readptr[6]);
                    temp_this_command_code_buf.push_back(readptr[7]);

                }
                else
                {
                    env->_calln_opcode_offsets_for_jit.push_back(generated_runtime_code_buf.size());

                    temp_this_command_code_buf.push_back(WO_OPCODE(calln, 00));

                    byte_t* readptr = (byte_t*)&WO_IR.opinteger;
                    temp_this_command_code_buf.push_back(readptr[0]);
                    temp_this_command_code_buf.push_back(readptr[1]);
                    temp_this_command_code_buf.push_back(readptr[2]);
                    temp_this_command_code_buf.push_back(readptr[3]);

                    // reserve...
                    temp_this_command_code_buf.push_back(0x00);
                    temp_this_command_code_buf.push_back(0x00);
                    temp_this_command_code_buf.push_back(0x00);
                    temp_this_command_code_buf.push_back(0x00);
                }
                break;
            case instruct::opcode::ret:
                if (WO_IR.opinteger)
                {
                    // ret pop n
                    temp_this_command_code_buf.push_back(WO_OPCODE(ret, 10));

                    uint16_t pop_count = (uint16_t)WO_IR.opinteger;
                    byte_t* readptr = (byte_t*)&pop_count;
                    temp_this_command_code_buf.push_back(readptr[0]);
                    temp_this_command_code_buf.push_back(readptr[1]);
                }
                else
                    temp_this_command_code_buf.push_back(WO_OPCODE(ret, 00));
                break;
            case instruct::opcode::jnequb:
            {
                wo_assert(dynamic_cast<opnum::tag*>(WO_IR.op2) != nullptr, "Operator num should be a tag.");

                temp_this_command_code_buf.push_back(WO_OPCODE(jnequb));
                size_t opcodelen = WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                
                // Write jmp
                jmp_record_table[dynamic_cast<opnum::tag*>(WO_IR.op2)->name]
                    .push_back(generated_runtime_code_buf.size() + 1 + opcodelen);
                temp_this_command_code_buf.push_back(0x00);
                temp_this_command_code_buf.push_back(0x00);
                temp_this_command_code_buf.push_back(0x00);
                temp_this_command_code_buf.push_back(0x00);

                break;
            }
            case instruct::mkunion:
            {
                temp_this_command_code_buf.push_back(WO_OPCODE(mkunion));
                WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);

                uint16_t id = (uint16_t)WO_IR.opinteger;
                byte_t* readptr = (byte_t*)&id;
                temp_this_command_code_buf.push_back(readptr[0]);
                temp_this_command_code_buf.push_back(readptr[1]);
                break;
            }
            case instruct::mkclos:
            {
                env->_mkclos_opcode_offsets_for_jit.push_back(generated_runtime_code_buf.size());

                temp_this_command_code_buf.push_back(WO_OPCODE(mkclos, 00));

                uint16_t capture_count = (uint16_t)WO_IR.opinteger;
                byte_t* readptr = (byte_t*)&capture_count;
                temp_this_command_code_buf.push_back(readptr[0]);
                temp_this_command_code_buf.push_back(readptr[1]);

                jmp_record_table[dynamic_cast<opnum::tag*>(WO_IR.op1)->name]
                    .push_back(generated_runtime_code_buf.size() + 1 + 2);
                temp_this_command_code_buf.push_back(0x00);
                temp_this_command_code_buf.push_back(0x00);
                temp_this_command_code_buf.push_back(0x00);
                temp_this_command_code_buf.push_back(0x00);

                // reserve...
                temp_this_command_code_buf.push_back(0x00);
                temp_this_command_code_buf.push_back(0x00);
                temp_this_command_code_buf.push_back(0x00);
                temp_this_command_code_buf.push_back(0x00);
                break;
            }
            case instruct::opcode::ext:

                switch (WO_IR.ext_page_id)
                {
                case 0:
                {
                    temp_this_command_code_buf.push_back(WO_OPCODE(ext, 00));
                    switch (WO_IR.ext_opcode_p0)
                    {
                    case instruct::extern_opcode_page_0::packargs:
                    {
                        temp_this_command_code_buf.push_back(WO_OPCODE_EXT0(packargs));

                        uint16_t skip_count = (uint16_t)WO_IR.opinteger;
                        byte_t* readptr = (byte_t*)&skip_count;
                        temp_this_command_code_buf.push_back(readptr[0]);
                        temp_this_command_code_buf.push_back(readptr[1]);

                        WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                        WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                        break;
                    }
                    case instruct::extern_opcode_page_0::unpackargs:
                        temp_this_command_code_buf.push_back(WO_OPCODE_EXT0(unpackargs));
                        WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
                        WO_IR.op2->generate_opnum_to_buffer(temp_this_command_code_buf);
                        break;
                    case instruct::extern_opcode_page_0::panic:
                    {
                        temp_this_command_code_buf.push_back(WO_OPCODE_EXT0(panic));
                        WO_IR.op1->generate_opnum_to_buffer(temp_this_command_code_buf);
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
                //    temp_this_command_code_buf.push_back(WO_OPCODE(ext, 01));
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
                //    temp_this_command_code_buf.push_back(WO_OPCODE(ext, 10));
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
                    temp_this_command_code_buf.push_back(WO_OPCODE(ext, 11));
                    switch (WO_IR.ext_opcode_p3)
                    {
                    case instruct::extern_opcode_page_3::funcbegin:
                        temp_this_command_code_buf.push_back(WO_OPCODE_EXT3(funcbegin));
                        env->_functions_offsets_for_jit.push_back(
                            temp_this_command_code_buf.size()
                            + generated_runtime_code_buf.size());
                        break;
                    case instruct::extern_opcode_page_3::funcend:
                        temp_this_command_code_buf.push_back(WO_OPCODE_EXT3(funcend));
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
                if (WO_IR.opinteger)
                    temp_this_command_code_buf.push_back(WO_OPCODE(abrt, 10));
                else
                    temp_this_command_code_buf.push_back(WO_OPCODE(abrt, 00));
                break;

            default:
                wo_error("Unknown instruct.");
            }

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


            generated_runtime_code_buf.insert(
                generated_runtime_code_buf.end(),
                temp_this_command_code_buf.begin(),
                temp_this_command_code_buf.end());

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
                *(uint32_t*)(generated_runtime_code_buf.data() + offset) = offset_val;
            }
        }

        for (auto& [tag, imm_value_offsets] : jmp_record_table_for_immtag)
        {
            uint32_t offset_val = tag_offset_vector_table[tag];
            for (auto imm_value_offset : imm_value_offsets)
            {
                env->_functions_def_constant_idx_for_jit.push_back(imm_value_offset);
                wo_assert(preserved_memory[imm_value_offset].type == value::valuetype::integer_type);
                preserved_memory[imm_value_offset].integer= (wo_integer_t)offset_val;
            }
        }

        env->constant_global_reg_rtstack = preserved_memory;

        env->constant_value_count = constant_value_count;
        env->constant_and_global_value_takeplace_count =
            constant_value_count
            + 1
            + global_value_count
            + 1;

        env->reg_begin = env->constant_global_reg_rtstack
            + env->constant_and_global_value_takeplace_count;

        env->stack_begin = env->constant_global_reg_rtstack + (preserve_memory_size - 1);
        env->real_register_count = real_register_count;
        env->runtime_stack_count = runtime_stack_count;
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
        env->loaded_libs = loaded_libs;

        if (wo::config::ENABLE_PDB_INFORMATIONS)
            env->program_debug_info = pdb_info;

        return env;
    }
}
