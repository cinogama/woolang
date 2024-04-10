#include "wo_lang.hpp"

namespace wo
{
    using namespace ast;

#define WO_AST() astnode; wo_assert(astnode != nullptr)

#define WO_PASS0(NODETYPE) void lang::pass0_##NODETYPE (ast::NODETYPE* astnode)

    WO_PASS0(ast_list)
    {
        auto* a_list = WO_AST();

        auto* child = a_list->children;
        while (child != nullptr)
        {
            analyze_pass0(child);
            child = child->sibling;
        }
    }
    WO_PASS0(ast_namespace)
    {
        auto* a_namespace = WO_AST();

        begin_namespace(a_namespace);
        if (a_namespace->in_scope_sentence != nullptr)
        {
            a_namespace->add_child(a_namespace->in_scope_sentence);
            analyze_pass0(a_namespace->in_scope_sentence);
        }
        end_namespace();
    }
    WO_PASS0(ast_varref_defines)
    {
        auto* a_varref_defs = WO_AST();
        a_varref_defs->located_function = in_function();
        for (auto& varref : a_varref_defs->var_refs)
        {
            analyze_pattern_in_pass0(varref.pattern, a_varref_defs->declear_attribute, varref.init_val);
        }
    }
    WO_PASS0(ast_value_function_define)
    {
        auto* a_value_func_decl = WO_AST();

        // Only declear symbol in pass0
        a_value_func_decl->this_func_scope = begin_function(a_value_func_decl);
        end_function();
    }
    WO_PASS0(ast_using_type_as)
    {
        auto* a_using_type_as = WO_AST();

        if (a_using_type_as->type_symbol == nullptr)
        {
            auto* typing_symb = define_type_in_this_scope(
                a_using_type_as, a_using_type_as->old_type, a_using_type_as->declear_attribute);
            typing_symb->apply_template_setting(a_using_type_as);

            a_using_type_as->type_symbol = typing_symb;

            wo_assert(a_using_type_as->type_symbol->has_been_completed_defined == false);
        }
        analyze_pass0(a_using_type_as->namespace_decl);
    }

#undef WO_PASS0

    void lang::init_global_pass_table()
    {
        wo_assert(m_global_pass_table == nullptr);

        m_global_pass_table = std::make_unique<dynamic_cast_pass_table>();
#define WO_PASS(N, T) m_global_pass_table->register_pass_table<T, N>(&lang::pass##N##_##T)
#define WO_PASS0(T) WO_PASS(0, T)
#define WO_PASS1(T) WO_PASS(1, T)
#define WO_PASS2(T) WO_PASS(2, T)

#define WO_FINALIZE(T) m_global_pass_table->register_finalize_table(&lang::finalize_##T)
#define WO_FINALIZE_VALUE(T) m_global_pass_table->register_finalize_value_table(&lang::finalize_value_##T)

        WO_PASS0(ast_list);
        WO_PASS0(ast_namespace);
        WO_PASS0(ast_varref_defines);
        WO_PASS0(ast_value_function_define);
        WO_PASS0(ast_using_type_as);

        WO_PASS1(ast_namespace);
        WO_PASS1(ast_varref_defines);
        WO_PASS1(ast_value_binary);
        WO_PASS1(ast_value_mutable);
        WO_PASS1(ast_value_index);
        WO_PASS1(ast_value_assign);
        WO_PASS1(ast_value_logical_binary);
        WO_PASS1(ast_value_variable);
        WO_PASS1(ast_value_type_cast);
        WO_PASS1(ast_value_type_judge);
        WO_PASS1(ast_value_type_check);
        WO_PASS1(ast_value_function_define);
        WO_PASS1(ast_fakevalue_unpacked_args);
        WO_PASS1(ast_value_funccall);
        WO_PASS1(ast_value_array);
        WO_PASS1(ast_value_mapping);
        WO_PASS1(ast_value_indexed_variadic_args);
        WO_PASS1(ast_return);
        WO_PASS1(ast_sentence_block);
        WO_PASS1(ast_if);
        WO_PASS1(ast_while);
        WO_PASS1(ast_forloop);
        WO_PASS1(ast_value_unary);
        WO_PASS1(ast_mapping_pair);
        WO_PASS1(ast_using_namespace);
        WO_PASS1(ast_using_type_as);
        WO_PASS1(ast_foreach);
        WO_PASS1(ast_union_make_option_ob_to_cr_and_ret);
        WO_PASS1(ast_match);
        WO_PASS1(ast_match_union_case);
        WO_PASS1(ast_value_make_struct_instance);
        WO_PASS1(ast_value_make_tuple_instance);
        WO_PASS1(ast_struct_member_define);
        WO_PASS1(ast_where_constraint);
        WO_PASS1(ast_value_trib_expr);
        WO_PASS1(ast_value_typeid);

        WO_PASS2(ast_mapping_pair);
        WO_PASS2(ast_using_type_as);
        WO_PASS2(ast_return);
        WO_PASS2(ast_sentence_block);
        WO_PASS2(ast_if);
        WO_PASS2(ast_value_mutable);
        WO_PASS2(ast_while);
        WO_PASS2(ast_forloop);
        WO_PASS2(ast_foreach);
        WO_PASS2(ast_varref_defines);
        WO_PASS2(ast_union_make_option_ob_to_cr_and_ret);
        WO_PASS2(ast_match);
        WO_PASS2(ast_match_union_case);
        WO_PASS2(ast_struct_member_define);
        WO_PASS2(ast_where_constraint);
        WO_PASS2(ast_value_function_define);
        WO_PASS2(ast_value_assign);
        WO_PASS2(ast_value_type_cast);
        WO_PASS2(ast_value_type_judge);
        WO_PASS2(ast_value_type_check);
        WO_PASS2(ast_value_index);
        WO_PASS2(ast_value_indexed_variadic_args);
        WO_PASS2(ast_fakevalue_unpacked_args);
        WO_PASS2(ast_value_binary);
        WO_PASS2(ast_value_logical_binary);
        WO_PASS2(ast_value_array);
        WO_PASS2(ast_value_mapping);
        WO_PASS2(ast_value_make_tuple_instance);
        WO_PASS2(ast_value_make_struct_instance);
        WO_PASS2(ast_value_trib_expr);
        WO_PASS2(ast_value_variable);
        WO_PASS2(ast_value_unary);
        WO_PASS2(ast_value_funccall);
        WO_PASS2(ast_value_typeid);

        WO_FINALIZE(ast_varref_defines);
        WO_FINALIZE(ast_list);
        WO_FINALIZE(ast_if);
        WO_FINALIZE(ast_while);
        WO_FINALIZE(ast_forloop);
        WO_FINALIZE(ast_sentence_block);
        WO_FINALIZE(ast_return);
        WO_FINALIZE(ast_namespace);
        WO_FINALIZE(ast_using_namespace);
        WO_FINALIZE(ast_using_type_as);
        WO_FINALIZE(ast_nop);
        WO_FINALIZE(ast_foreach);
        WO_FINALIZE(ast_break);
        WO_FINALIZE(ast_continue);
        WO_FINALIZE(ast_union_make_option_ob_to_cr_and_ret);
        WO_FINALIZE(ast_match);
        WO_FINALIZE(ast_match_union_case);

        WO_FINALIZE_VALUE(ast_value_function_define);
        WO_FINALIZE_VALUE(ast_value_variable);
        WO_FINALIZE_VALUE(ast_value_binary);
        WO_FINALIZE_VALUE(ast_value_assign);
        WO_FINALIZE_VALUE(ast_value_mutable);
        WO_FINALIZE_VALUE(ast_value_type_cast);
        WO_FINALIZE_VALUE(ast_value_type_judge);
        WO_FINALIZE_VALUE(ast_value_type_check);
        WO_FINALIZE_VALUE(ast_value_funccall);
        WO_FINALIZE_VALUE(ast_value_logical_binary);
        WO_FINALIZE_VALUE(ast_value_array);
        WO_FINALIZE_VALUE(ast_value_mapping);
        WO_FINALIZE_VALUE(ast_value_index);
        WO_FINALIZE_VALUE(ast_value_packed_variadic_args);
        WO_FINALIZE_VALUE(ast_value_indexed_variadic_args);
        WO_FINALIZE_VALUE(ast_fakevalue_unpacked_args);
        WO_FINALIZE_VALUE(ast_value_unary);
        WO_FINALIZE_VALUE(ast_value_takeplace);
        WO_FINALIZE_VALUE(ast_value_make_struct_instance);
        WO_FINALIZE_VALUE(ast_value_make_tuple_instance);
        WO_FINALIZE_VALUE(ast_value_trib_expr);

#undef WO_PASS
#undef WO_PASS0
#undef WO_PASS1
#undef WO_PASS2
    }
    void lang::release_global_pass_table()
    {
        wo_assert(m_global_pass_table != nullptr);
        m_global_pass_table.reset();
    }
}