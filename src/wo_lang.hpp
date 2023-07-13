#pragma once

#include "wo_basic_type.hpp"
#include "wo_lang_ast_builder.hpp"
#include "wo_compiler_ir.hpp"

#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <optional>

namespace wo
{
    struct lang_symbol
    {
        enum symbol_type
        {
            type_alias = 0x01,
            typing = 0x02,

            variable = 0x04,
            function = 0x08,
        };
        symbol_type type;
        wo_pstring_t name;

        ast::ast_defines* define_node = nullptr;
        ast::ast_decl_attribute* attribute;
        lang_scope* defined_in_scope;
        bool define_in_function = false;
        bool static_symbol = false;
        bool has_been_defined_in_pass2 = false;
        bool is_constexpr = false;
        ast::identifier_decl decl = ast::identifier_decl::IMMUTABLE;
        bool is_captured_variable = false;
        bool is_argument = false;
        bool is_hkt_typing_symb = false;

        union
        {
            wo_integer_t stackvalue_index_in_funcs = -99999999;
            size_t global_index_in_lang;
            wo_integer_t captured_index;
        };

        union
        {
            ast::ast_value* variable_value;
            ast::ast_type* type_informatiom;
        };
        std::vector<ast::ast_check_type_with_naming_in_pass2*> naming_list;
        bool is_template_symbol = false;
        std::vector<wo_pstring_t> template_types;

        std::map<std::vector<uint32_t>, lang_symbol*> template_typehashs_reification_instance_symbol_list;
        std::map<std::vector<uint32_t>, ast::ast_type*> template_type_instances;

        void apply_template_setting(ast::ast_defines* defs)
        {
            is_template_symbol = defs->is_template_define;
            if (is_template_symbol)
            {
                template_types = defs->template_type_name_list;
            }
        }
        ast::ast_value_function_define* get_funcdef()const noexcept
        {
            wo_assert(type == symbol_type::function);
            auto* result = dynamic_cast<ast::ast_value_function_define*>(variable_value);
            wo_assert(result);

            return result;
        }
        bool is_type_decl() const noexcept
        {
            return type == symbol_type::type_alias || type == symbol_type::typing;
        }
        wo_pstring_t defined_source() const noexcept
        {
            if (is_type_decl())
                return type_informatiom->source_file;
            else
                return variable_value->source_file;
        }
    };

    struct lang_scope
    {
        bool stop_searching_in_last_scope_flag;

        enum scope_type
        {
            namespace_scope,    // namespace xx{}
            function_scope,     // func xx(){}
            just_scope,         //{} if{} while{}
        };

        scope_type type;
        grammar::ast_base* last_entry_ast;
        lang_scope* belong_namespace;
        lang_scope* parent_scope;
        wo_pstring_t scope_namespace = nullptr;
        std::unordered_map<wo_pstring_t, lang_symbol*> symbols;

        // Only used when this scope is a namespace.
        std::unordered_map<wo_pstring_t, lang_scope*> sub_namespaces;

        std::vector<ast::ast_using_namespace*> used_namespace;
        std::vector<lang_symbol*> in_function_symbols;

        ast::ast_value_function_define* function_node;

        size_t max_used_stack_size_in_func = 0; // only used in function_scope
        size_t used_stackvalue_index = 0; // only used in function_scope

        size_t this_block_used_stackvalue_count = 0;

        size_t assgin_stack_index(lang_symbol* in_func_variable)
        {
            wo_assert(type == scope_type::function_scope);
            in_function_symbols.push_back(in_func_variable);

            if (used_stackvalue_index + 1 > max_used_stack_size_in_func)
                max_used_stack_size_in_func = used_stackvalue_index + 1;
            return used_stackvalue_index++;
        }

        void reduce_function_used_stack_size_at(wo_integer_t canceled_stack_pos)
        {
            max_used_stack_size_in_func--;
            for (auto* infuncvars : in_function_symbols)
            {
                wo_assert(infuncvars->type == lang_symbol::symbol_type::variable);

                if (!infuncvars->static_symbol)
                {
                    if (infuncvars->stackvalue_index_in_funcs == canceled_stack_pos)
                        infuncvars->stackvalue_index_in_funcs = 0;
                    else if (infuncvars->stackvalue_index_in_funcs > canceled_stack_pos)
                        infuncvars->stackvalue_index_in_funcs--;
                }

            }
        }

    };

    class lang
    {
    private:
        using template_type_map = std::map<wo_pstring_t, lang_symbol*>;

        lexer* lang_anylizer;
        std::vector<lang_scope*> lang_scopes_buffers;
        std::vector<lang_symbol*> lang_symbols; // only used for storing symbols to release
        std::vector<opnum::opnumbase*> generated_opnum_list_for_clean;
        std::forward_list<grammar::ast_base*> generated_ast_nodes_buffers;
        std::unordered_set<grammar::ast_base*> traving_node;
        std::unordered_set<lang_symbol*> traving_symbols;
        std::vector<lang_scope*> lang_scopes; // it is a stack like list;
        lang_scope* now_namespace = nullptr;
        lang_scope* current_function_in_pass2 = nullptr;

        // Used for ignore impure type when get type of expr.
        bool skip_side_effect_check = false;

        std::map<uint32_t, ast::ast_type*> hashed_typing;

        std::map<wo_extern_native_func_t, std::vector<ast::ast_value_function_define*>> extern_symb_func_definee;
        ast::ast_value_function_define* now_function_in_final_anylize = nullptr;
        std::vector<template_type_map> template_stack;

        rslib_extern_symbols::extern_lib_set extern_libs;

        uint32_t get_typing_hash_after_pass1(ast::ast_type* typing);
        bool begin_template_scope(grammar::ast_base* reporterr, const std::vector<wo_pstring_t>& template_defines_args, const std::vector<ast::ast_type*>& template_args);
        bool begin_template_scope(grammar::ast_base* reporterr, ast::ast_defines* template_defines, const std::vector<ast::ast_type*>& template_args);

        void end_template_scope();

        void temporary_entry_scope_in_pass1(lang_scope* scop);
        lang_scope* temporary_leave_scope_in_pass1();
    public:
        lang(lexer& lex);
        ~lang();
        ast::ast_type* generate_type_instance_by_templates(lang_symbol* symb, const std::vector<ast::ast_type*>& templates);
        bool check_matching_naming(ast::ast_type* clstype, ast::ast_type* naming);
        bool fully_update_type(ast::ast_type* type, bool in_pass_1, const std::vector<wo_pstring_t>& template_types, std::unordered_set<ast::ast_type*>& s);
        void fully_update_type(ast::ast_type* type, bool in_pass_1, const std::vector<wo_pstring_t>& template_types = {});

        std::vector<bool> in_pass2 = { false };
        struct entry_pass
        {
            std::vector<bool>* _set;
            entry_pass(std::vector<bool>& state_set, bool inpass2)
                :_set(&state_set)
            {
                state_set.push_back(inpass2);
            }
            ~entry_pass()
            {
                _set->pop_back();
            }
        };

#define WO_PASS(NODETYPE) \
        bool pass1_##NODETYPE (ast::NODETYPE* astnode);\
        bool pass2_##NODETYPE (ast::NODETYPE* astnode)

        WO_PASS(ast_namespace);
        WO_PASS(ast_varref_defines);
        WO_PASS(ast_value_binary);
        WO_PASS(ast_value_mutable);
        WO_PASS(ast_value_index);
        WO_PASS(ast_value_assign);
        WO_PASS(ast_value_logical_binary);
        WO_PASS(ast_value_variable);
        WO_PASS(ast_value_type_cast);
        WO_PASS(ast_value_type_judge);
        WO_PASS(ast_value_type_check);
        WO_PASS(ast_value_function_define);
        WO_PASS(ast_fakevalue_unpacked_args);
        WO_PASS(ast_value_funccall);
        WO_PASS(ast_value_array);
        WO_PASS(ast_value_mapping);
        WO_PASS(ast_value_indexed_variadic_args);
        WO_PASS(ast_return);
        WO_PASS(ast_sentence_block);
        WO_PASS(ast_if);
        WO_PASS(ast_while);
        WO_PASS(ast_forloop);
        WO_PASS(ast_value_unary);
        WO_PASS(ast_mapping_pair);
        WO_PASS(ast_using_namespace);
        WO_PASS(ast_using_type_as);
        WO_PASS(ast_foreach);
        WO_PASS(ast_check_type_with_naming_in_pass2);
        WO_PASS(ast_union_make_option_ob_to_cr_and_ret);
        WO_PASS(ast_match);
        WO_PASS(ast_match_union_case);
        WO_PASS(ast_value_make_struct_instance);
        WO_PASS(ast_value_make_tuple_instance);
        WO_PASS(ast_struct_member_define);
        WO_PASS(ast_where_constraint);
        WO_PASS(ast_value_trib_expr);
        WO_PASS(ast_do_impure);
        WO_PASS(ast_value_typeid);
#undef WO_PASS
        void analyze_pattern_in_pass1(ast::ast_pattern_base* pattern, ast::ast_decl_attribute* attrib, ast::ast_value* initval);
        void analyze_pattern_in_pass2(ast::ast_pattern_base* pattern, ast::ast_value* initval);
        void analyze_pattern_in_finalize(ast::ast_pattern_base* pattern, ast::ast_value* initval, ir_compiler* compiler);
        void collect_ast_nodes_for_pass1(grammar::ast_base* ast_node);
        void analyze_pass1(grammar::ast_base* ast_node, bool type_degradation = true);
        lang_symbol* analyze_pass_template_reification(ast::ast_value_variable* origin_variable, std::vector<ast::ast_type*> template_args_types);
        ast::ast_value_function_define* analyze_pass_template_reification(ast::ast_value_function_define* origin_template_func_define, std::vector<ast::ast_type*> template_args_types);

        using judge_result_t = std::variant<ast::ast_type*, ast::ast_value_function_define*>;

        std::optional<judge_result_t> judge_auto_type_of_funcdef_with_type(
            grammar::ast_base* errreport,
            ast::ast_type* param,
            ast::ast_value* callaim,
            bool update,
            ast::ast_defines* template_defines,
            const std::vector<ast::ast_type*>* template_args);
        std::vector<ast::ast_type*> judge_auto_type_in_funccall(ast::ast_value_funccall* funccall, bool update, ast::ast_defines* template_defines, const std::vector<ast::ast_type*>* template_args);

        bool write_flag_complete_in_pass2 = true;
        bool has_step_in_step2 = false;

        void start_trying_pass2();
        void end_trying_pass2();
        void analyze_pass2(grammar::ast_base* ast_node, bool type_degradation=true);
        void clean_and_close_lang();

        ast::ast_type* analyze_template_derivation(
            wo_pstring_t temp_form,
            const std::vector<wo_pstring_t>& termplate_set,
            ast::ast_type* para, ast::ast_type* args);

        // register dict.... fxxk
        enum class RegisterUsingState : uint8_t
        {
            FREE,
            NORMAL,
            BLOCKING
        };
        std::vector<RegisterUsingState> assigned_tr_register_list = std::vector<RegisterUsingState>(
            opnum::reg::T_REGISTER_COUNT + opnum::reg::R_REGISTER_COUNT);   // will assign t register

        opnum::opnumbase& get_useable_register_for_pure_value(bool must_release = false);
        void _complete_using_register_for_pure_value(opnum::opnumbase& completed_reg);
        void _complete_using_all_register_for_pure_value();
        opnum::opnumbase& complete_using_register(opnum::opnumbase& completed_reg);
        void complete_using_all_register();
        bool is_reg(opnum::opnumbase& op_num);
        bool is_cr_reg(opnum::opnumbase& op_num);
        bool is_non_ref_tem_reg(opnum::opnumbase& op_num);
        bool is_temp_reg(opnum::opnumbase& op_num);
        opnum::opnumbase& mov_value_to_cr(opnum::opnumbase& op_num, ir_compiler* compiler);

        std::vector<ast::ast_value_function_define* > in_used_functions;

        opnum::opnumbase& get_new_global_variable();
        std::variant<opnum::opnumbase*, int16_t> get_opnum_by_symbol(grammar::ast_base* error_prud, lang_symbol* symb, ir_compiler* compiler, bool get_pure_value = false);

        bool _last_value_stored_to_cr = false;
        bool _last_value_from_stack = false;
        int16_t _last_stack_offset_to_write = 0;

        struct auto_cancel_value_store_to_cr
        {
            bool clear_sign = false;
            bool* aim_flag;
            auto_cancel_value_store_to_cr(bool& flag)
                :aim_flag(&flag)
            {

            }
            ~auto_cancel_value_store_to_cr()
            {
                *aim_flag = clear_sign;
            }

            void set_true()
            {
                clear_sign = true;
            }
            void set_false()
            {
                clear_sign = false;
            }
        };

        opnum::opnumbase& analyze_value(ast::ast_value* value, ir_compiler* compiler, bool get_pure_value = false);
        opnum::opnumbase& auto_analyze_value(ast::ast_value* value, ir_compiler* compiler, bool get_pure_value = false, bool force_value = false);

        struct loop_label_info
        {
            wo_pstring_t current_loop_label;

            std::string current_loop_break_aim_tag;
            std::string current_loop_continue_aim_tag;
        };

        std::vector<loop_label_info> loop_stack_for_break_and_continue;

        void real_analyze_finalize(grammar::ast_base* ast_node, ir_compiler* compiler);
        void analyze_finalize(grammar::ast_base* ast_node, ir_compiler* compiler);
        lang_scope* begin_namespace(ast::ast_namespace* a_namespace);
        void end_namespace();
        lang_scope* begin_scope(grammar::ast_base* block_beginer);
        void end_scope();
        lang_scope* begin_function(ast::ast_value_function_define* ast_value_funcdef);
        void end_function();
        lang_scope* now_scope() const;
        lang_scope* in_function() const;
        lang_scope* in_function_pass2() const;

        size_t global_symbol_index = 0;

        enum class template_style
        {
            NORMAL,
            IS_TEMPLATE_VARIABLE_DEFINE,
            IS_TEMPLATE_VARIABLE_IMPL
        };

        lang_symbol* define_variable_in_this_scope(
            grammar::ast_base* errreporter,
            wo_pstring_t names,
            ast::ast_value* init_val,
            ast::ast_decl_attribute* attr,
            template_style is_template_value,
            ast::identifier_decl mutable_type,
            size_t captureindex = (size_t)-1);
        lang_symbol* define_type_in_this_scope(ast::ast_using_type_as* def, ast::ast_type* as_type, ast::ast_decl_attribute* attr);

        bool check_symbol_is_accessable(ast::ast_defines* astdefine, lang_symbol* symbol, lang_scope* current_scope, grammar::ast_base* ast, bool give_error = true);
        bool check_symbol_is_accessable(lang_symbol* symbol, lang_scope* current_scope, grammar::ast_base* ast, bool give_error = true);

        lang_symbol* find_symbol_in_this_scope(ast::ast_symbolable_base* var_ident, wo_pstring_t ident_str, int target_type_mask, bool fuzzy_for_err_report);
        lang_symbol* find_type_in_this_scope(ast::ast_type* var_ident);

        // Only used for check symbol is exist?
        lang_symbol* find_value_symbol_in_this_scope(ast::ast_value_variable* var_ident);

        lang_symbol* find_value_in_this_scope(ast::ast_value_variable* var_ident);
        bool has_compile_error()const;
    };
}