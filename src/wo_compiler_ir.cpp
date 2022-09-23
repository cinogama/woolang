#define _CRT_SECURE_NO_WARNINGS

#include "wo_compiler_ir.hpp"
#include "wo_lang_ast_builder.hpp"

namespace wo
{
    void program_debug_data_info::generate_debug_info_at_funcbegin(ast::ast_value_function_define* ast_func, ir_compiler* compiler)
    {
        if (ast_func->argument_list->source_file)
        {
            auto& row_buff = _general_src_data_buf_a[*ast_func->argument_list->source_file][ast_func->argument_list->row_end_no];
            if (row_buff.find(ast_func->argument_list->col_end_no) == row_buff.end())
                row_buff[ast_func->argument_list->col_end_no] = SIZE_MAX;

            auto& old_ip = row_buff[ast_func->argument_list->col_end_no];
            if (compiler->get_now_ip() < old_ip)
                old_ip = compiler->get_now_ip();
        }
    }
    void program_debug_data_info::generate_debug_info_at_funcend(ast::ast_value_function_define* ast_func, ir_compiler* compiler)
    {
        if (ast_func->source_file)
        {
            auto& row_buff = _general_src_data_buf_a[*ast_func->source_file][ast_func->row_end_no];
            if (row_buff.find(ast_func->col_end_no) == row_buff.end())
                row_buff[ast_func->col_end_no] = SIZE_MAX;

            auto& old_ip = row_buff[ast_func->col_end_no];
            if (compiler->get_now_ip() < old_ip)
                old_ip = compiler->get_now_ip();
        }
    }
    void program_debug_data_info::generate_debug_info_at_astnode(grammar::ast_base* ast_node, ir_compiler* compiler)
    {
        // funcdef should not genrate val..
        if (dynamic_cast<ast::ast_value_function_define*>(ast_node)
            || dynamic_cast<ast::ast_list*>(ast_node)
            || dynamic_cast<ast::ast_namespace*>(ast_node)
            || dynamic_cast<ast::ast_sentence_block*>(ast_node)
            || dynamic_cast<ast::ast_if*>(ast_node)
            || dynamic_cast<ast::ast_while*>(ast_node)
            || dynamic_cast<ast::ast_forloop*>(ast_node)
            || dynamic_cast<ast::ast_foreach*>(ast_node)
            || dynamic_cast<ast::ast_match*>(ast_node))
            return;

        if (ast_node->source_file)
        {
            auto& row_buff = _general_src_data_buf_a[*ast_node->source_file][ast_node->row_end_no];
            if (row_buff.find(ast_node->col_end_no) == row_buff.end())
                row_buff[ast_node->col_end_no] = SIZE_MAX;

            auto& old_ip = row_buff[ast_node->col_end_no];
            if (compiler->get_now_ip() < old_ip)
                old_ip = compiler->get_now_ip();
        }
    }
    void program_debug_data_info::finalize_generate_debug_info()
    {
        for (auto& [filename, rowbuf] : _general_src_data_buf_a)
        {
            for (auto& [rowno, colbuf] : rowbuf)
            {
                for (auto& [colno, ipxx] : colbuf)
                {
                    // if (_general_src_data_buf_b.find(ipxx) == _general_src_data_buf_b.end())
                    _general_src_data_buf_b[ipxx] = location{ rowno , colno ,filename };
                }
            }
        }
    }
    const program_debug_data_info::location& program_debug_data_info::get_src_location_by_runtime_ip(const byte_t* rt_pos) const
    {
        const size_t FAIL_INDEX = SIZE_MAX;
        static program_debug_data_info::location     FAIL_LOC;

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
        for (auto& [rowid, linebuf] : fnd->second)
        {
            if (strict)
            {
                if (rowid == rowno)
                {
                    for (auto [colno, ip] : linebuf)
                        if (ip < result)
                            result = ip;
                    return result;
                }
            }
            else if (rowid >= rowno)
            {
                for (auto [colno, ip] : linebuf)
                    if (ip < result)
                        result = ip;
                return result;
            }
        }
        return FAIL_INDEX;
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
        generate_debug_info_at_funcbegin(funcdef, compiler);
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