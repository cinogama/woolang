#pragma once

#include "wo_basic_type.hpp"
#include "wo_lang_ast_builder.hpp"
#include "wo_compiler_ir.hpp"

#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <optional>
#include <typeindex>

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

        // NOTE: Used for variable to mark it has been defined in pass2.
        //       Used for type to mark it has been defined in pass1.
        bool has_been_completed_defined = false;
        bool is_constexpr = false;
        ast::identifier_decl decl = ast::identifier_decl::IMMUTABLE;
        bool is_captured_variable = false;
        bool is_argument = false;
        bool is_hkt_typing_symb = false;
        bool is_marked_as_used_variable = false;

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
        ast::ast_type* get_typedef()const noexcept
        {
            wo_assert(type == symbol_type::type_alias || type == symbol_type::typing);
            return type_informatiom;
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
        bool is_constexpr_or_immut_no_closure_func()
        {
            // If constexpr or immut binded with function define, 
            // No need generate code for it's stack space and init.
            return is_constexpr
                || (decl == wo::ast::identifier_decl::IMMUTABLE
                    && !is_argument
                    && !is_captured_variable
                    && type == lang_symbol::symbol_type::variable
                    && dynamic_cast<ast::ast_value_function_define*>(variable_value) != nullptr
                    // Only normal func (without capture vars) can use this way to optimize.
                    && dynamic_cast<ast::ast_value_function_define*>(variable_value)->capture_variables.empty())
                ;
        }
    };

    struct lang_scope
    {
        enum scope_type
        {
            namespace_scope,    // namespace xx{}
            function_scope,     // func xx(){}
            just_scope,         //{} if{} while{}
        };

        scope_type type = scope_type::namespace_scope;
        ast::ast_base* last_entry_ast = nullptr;
        lang_scope* belong_namespace = nullptr;
        lang_scope* parent_scope = nullptr;
        lang_scope* derivation_from_scope_for_function = nullptr;
        wo_pstring_t scope_namespace = nullptr;

        ast::ast_value_function_define* function_node = nullptr;

        size_t max_used_stack_size_in_func = 0; // only used in function_scope
        size_t used_stackvalue_index = 0; // only used in function_scope

        size_t this_block_used_stackvalue_count = 0;

        using template_type_map = std::map<wo_pstring_t, lang_symbol*>;
        std::vector<template_type_map> template_stack;

        std::unordered_map<wo_pstring_t, lang_symbol*> symbols;

        // Only used when this scope is a namespace.
        std::unordered_map<wo_pstring_t, lang_scope*> sub_namespaces;

        std::vector<ast::ast_using_namespace*> used_namespace;
        std::vector<lang_symbol*> in_function_symbols;

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
        bool belongs_to(const lang_scope* scope)const
        {
            wo_assert(scope != nullptr);

            if (this == scope || this->derivation_from_scope_for_function == scope)
                return true;

            if (parent_scope == nullptr)
                return false;
            return parent_scope->belongs_to(scope);
        }
    };

    class lang
    {
    public:
        struct dynamic_cast_pass_table
        {
            dynamic_cast_pass_table(const dynamic_cast_pass_table&) = delete;
            dynamic_cast_pass_table& operator=(const dynamic_cast_pass_table&) = delete;
            dynamic_cast_pass_table(dynamic_cast_pass_table&&) = delete;
            dynamic_cast_pass_table& operator=(dynamic_cast_pass_table&&) = delete;

            template<typename T>
            using typed_table_t = std::unordered_map<std::type_index, std::function<T>>;

            using pass_table_t = typed_table_t<void(lang*, ast::ast_base*)>;
            using finalize_table_t = typed_table_t<void(lang*, ast::ast_base*, ir_compiler*)>;
            using finalize_value_table_t = typed_table_t<opnum::opnumbase& (lang*, ast::ast_base*, ir_compiler*, bool)>;

            using ignore_debug_info_table_t = std::unordered_set<std::type_index>;

            pass_table_t m_pass_table[3];
            finalize_table_t m_finalize_table;
            finalize_value_table_t m_finalize_value_table;
            ignore_debug_info_table_t m_ignore_debug_info_table;

            dynamic_cast_pass_table() = default;

            template<typename T, size_t passid>
            void register_pass_table(void(lang::* pass_func)(T*))
            {
                wo_assure(m_pass_table[passid].insert(
                    std::make_pair(std::type_index(typeid(T)),
                        [pass_func](lang* langself, ast::ast_base* ast)
                        {
                            auto* node = dynamic_cast<T*>(ast);
                            wo_assert(node != nullptr);

                            (langself->*pass_func)(node);
                        })).second);
            }

            template<typename T>
            void register_finalize_table(void(lang::* pass_func)(T*, ir_compiler*))
            {
                wo_assure(m_finalize_table.insert(
                    std::make_pair(std::type_index(typeid(T)),
                        [pass_func](lang* langself, ast::ast_base* ast, ir_compiler* compiler)
                        {
                            auto* node = dynamic_cast<T*>(ast);
                            wo_assert(node != nullptr);

                            (langself->*pass_func)(node, compiler);
                        })).second);
            }

            template<typename T>
            void register_finalize_value_table(opnum::opnumbase& (lang::* pass_func)(T*, ir_compiler*, bool))
            {
                wo_assure(m_finalize_value_table.insert(
                    std::make_pair(std::type_index(typeid(T)),
                        [pass_func](lang* langself, ast::ast_base* ast, ir_compiler* compiler, bool get_pure_value)-> opnum::opnumbase&
                        {
                            auto* node = dynamic_cast<T*>(ast);
                            wo_assert(node != nullptr);

                            return (langself->*pass_func)(node, compiler, get_pure_value);
                        })).second);
            }

            template<typename T>
            void register_ignore_debug_info_table()
            {
                wo_assure(m_ignore_debug_info_table.insert(std::type_index(typeid(T))).second);
            }

            template<size_t passid>
            bool pass(lang* lang_self, ast::ast_base* ast)
            {
                if constexpr (passid > 1)
                    wo_assert(ast != nullptr);

                if (ast != nullptr)
                {
                    auto it = m_pass_table[passid].find(std::type_index(typeid(*ast)));
                    if (it != m_pass_table[passid].end())
                    {
                        it->second(lang_self, ast);
                        return true;
                    }
                }
                return false;
            }
            bool finalize(lang* lang_self, ast::ast_base* ast, ir_compiler* compiler)
            {
                if (ast != nullptr)
                {
                    auto it = m_finalize_table.find(std::type_index(typeid(*ast)));
                    if (it != m_finalize_table.end())
                    {
                        it->second(lang_self, ast, compiler);
                        return true;
                    }
                }
                return false;
            }
            opnum::opnumbase& finalize_value(lang* lang_self, ast::ast_base* ast, ir_compiler* compiler, bool get_pure_value)
            {
                wo_assert(ast != nullptr);
                const auto& f = m_finalize_value_table.at(std::type_index(typeid(*ast)));
                return f(lang_self, ast, compiler, get_pure_value);
            }
            bool ignore_debug_info(ast::ast_base* ast)
            {
                return m_ignore_debug_info_table.find(std::type_index(typeid(*ast))) != m_ignore_debug_info_table.end();
            }
        };

    private:
        inline static std::unique_ptr<dynamic_cast_pass_table> m_global_pass_table;
    public:
        static void init_global_pass_table();
        static void release_global_pass_table();
    private:
        lexer* lang_anylizer;
        std::vector<lang_scope*> lang_scopes_buffers;
        std::vector<lang_symbol*> lang_symbols; // only used for storing symbols to release
        std::vector<opnum::opnumbase*> generated_opnum_list_for_clean;
        std::forward_list<ast::ast_base*> generated_ast_nodes_buffers;
        std::unordered_set<lang_symbol*> traving_symbols;
        std::vector<lang_scope*> lang_scopes; // it is a stack like list;
        lang_scope* current_namespace = nullptr;
        lang_scope* current_function_in_pass2 = nullptr;

        std::map<uint32_t, ast::ast_type*> hashed_typing;

        using function_symb_map_t = std::unordered_map<std::wstring, ast::ast_extern_info*>;
        using library_symbs_map_t = std::unordered_map<std::wstring, function_symb_map_t>;
        library_symbs_map_t extern_symb_infos;

        ast::ast_value_function_define* now_function_in_final_anylize = nullptr;

        rslib_extern_symbols::extern_lib_set extern_libs;

        std::optional<uint32_t> get_typing_hash_after_pass1(ast::ast_type* typing);
        bool begin_template_scope(
            ast::ast_base* reporterr,
            lang_scope* belong_scope,
            const std::vector<wo_pstring_t>& template_defines_args,
            const std::vector<ast::ast_type*>& template_args);
        bool begin_template_scope(
            ast::ast_base* reporterr,
            lang_scope* belong_scope,
            ast::ast_defines* template_defines,
            const std::vector<ast::ast_type*>& template_args);

        void end_template_scope(lang_scope* scop);

        void temporary_entry_scope_in_pass1(lang_scope* scop);
        lang_scope* temporary_leave_scope_in_pass1();
    public:
        lang(lexer& lex);
        ~lang();
        ast::ast_type* generate_type_instance_by_templates(lang_symbol* symb, const std::vector<ast::ast_type*>& templates);
        void update_typeof_in_type(ast::ast_type* type, std::unordered_set<ast::ast_type*>& s);
        bool fully_update_type(ast::ast_type* type, bool in_pass_1, const std::vector<wo_pstring_t>& template_types, std::unordered_set<ast::ast_type*>& s);
        void fully_update_type(ast::ast_type* type, bool in_pass_1, const std::vector<wo_pstring_t>& template_types = {});

#define WO_PASS(NODETYPE) \
        void pass0_##NODETYPE (ast::NODETYPE* astnode);\
        void pass1_##NODETYPE (ast::NODETYPE* astnode);\
        void pass2_##NODETYPE (ast::NODETYPE* astnode)

#define WO_AST_PASS(NODETYPE) WO_PASS(NODETYPE);\
        void finalize_##NODETYPE (ast::NODETYPE* astnode, ir_compiler* compiler)

#define WO_VALUE_PASS(NODETYPE) WO_PASS(NODETYPE);\
        opnum::opnumbase& finalize_value_##NODETYPE (ast::NODETYPE* astnode, ir_compiler* compiler, bool get_pure_value)

        WO_AST_PASS(ast_list);
        WO_AST_PASS(ast_namespace);
        WO_AST_PASS(ast_varref_defines);
        WO_AST_PASS(ast_return);
        WO_AST_PASS(ast_sentence_block);
        WO_AST_PASS(ast_if);
        WO_AST_PASS(ast_while);
        WO_AST_PASS(ast_forloop);
        WO_AST_PASS(ast_mapping_pair);
        WO_AST_PASS(ast_using_namespace);
        WO_AST_PASS(ast_using_type_as);
        WO_AST_PASS(ast_foreach);
        WO_AST_PASS(ast_union_make_option_ob_to_cr_and_ret);
        WO_AST_PASS(ast_match);
        WO_AST_PASS(ast_match_union_case);
        WO_AST_PASS(ast_struct_member_define);
        WO_AST_PASS(ast_where_constraint);

        void finalize_ast_break(ast::ast_break* astnode, ir_compiler* compiler);
        void finalize_ast_continue(ast::ast_continue* astnode, ir_compiler* compiler);
        void finalize_ast_nop(ast::ast_nop* astnode, ir_compiler* compiler);

        WO_VALUE_PASS(ast_value_init);
        WO_VALUE_PASS(ast_value_binary);
        WO_VALUE_PASS(ast_value_mutable);
        WO_VALUE_PASS(ast_value_index);
        WO_VALUE_PASS(ast_value_assign);
        WO_VALUE_PASS(ast_value_logical_binary);
        WO_VALUE_PASS(ast_value_variable);
        WO_VALUE_PASS(ast_value_type_cast);
        WO_VALUE_PASS(ast_value_type_judge);
        WO_VALUE_PASS(ast_value_type_check);
        WO_VALUE_PASS(ast_value_function_define);
        WO_VALUE_PASS(ast_value_funccall);
        WO_VALUE_PASS(ast_value_array);
        WO_VALUE_PASS(ast_value_mapping);
        WO_VALUE_PASS(ast_value_indexed_variadic_args);
        WO_VALUE_PASS(ast_value_unary);
        WO_VALUE_PASS(ast_value_trib_expr);
        WO_VALUE_PASS(ast_value_typeid);
        WO_VALUE_PASS(ast_value_make_struct_instance);
        WO_VALUE_PASS(ast_value_make_tuple_instance);
        WO_VALUE_PASS(ast_value_packed_variadic_args);
        WO_VALUE_PASS(ast_fakevalue_unpacked_args);
        WO_VALUE_PASS(ast_value_takeplace);

#undef WO_AST_PASS
#undef WO_VALUE_PASS
#undef WO_PASS

        opnum::opnumbase& do_index_value_impl(
            ast::ast_value_index* a_value_index,
            ir_compiler* compiler,
            bool get_pure_value,
            bool get_pure_tmp_for_assign,
            opnum::opnumbase** out_index_from,
            opnum::opnumbase** out_index_key);

        void analyze_pattern_in_pass0(ast::ast_pattern_base* pattern, ast::ast_decl_attribute* attrib, ast::ast_value* initval);
        void analyze_pattern_in_pass1(ast::ast_pattern_base* pattern, ast::ast_decl_attribute* attrib, ast::ast_value* initval);
        void analyze_pattern_in_pass2(ast::ast_pattern_base* pattern, ast::ast_value* initval);
        void analyze_pattern_in_finalize(ast::ast_pattern_base* pattern, ast::ast_value* initval, bool in_pattern_expr, ir_compiler* compiler);
        void check_division(ast::ast_base* divop, ast::ast_value* left, ast::ast_value* right, opnum::opnumbase& left_opnum, opnum::opnumbase& right_opnum, ir_compiler* compiler);
        void collect_ast_nodes_for_pass1(ast::ast_base* ast_node);
        void analyze_pass0(ast::ast_base* ast_node);
        void analyze_pass1(ast::ast_base* ast_node, bool type_degradation = true);
        void _analyze_pass1(ast::ast_base* ast_node, bool type_degradation);
        lang_symbol* analyze_pass_template_reification_var(ast::ast_value_variable* origin_variable, std::vector<ast::ast_type*> template_args_types);
        ast::ast_value_function_define* analyze_pass_template_reification_func(ast::ast_value_function_define* origin_template_func_define, std::vector<ast::ast_type*> template_args_types);

        using judge_result_t = std::variant<ast::ast_type*, ast::ast_value_function_define*>;

        std::optional<judge_result_t> judge_auto_type_of_funcdef_with_type(
            lang_scope* located_scope,
            ast::ast_type* param,
            ast::ast_value* callaim,
            bool update,
            ast::ast_defines* template_defines,
            const std::vector<ast::ast_type*>* template_args);
        std::vector<ast::ast_type*> judge_auto_type_in_funccall(
            ast::ast_value_funccall* funccall,
            lang_scope* located_scope,
            bool update,
            ast::ast_defines* template_defines,
            const std::vector<ast::ast_type*>* template_args);

        bool has_step_in_step2 = false;

        void analyze_pass2(ast::ast_base* ast_node, bool type_degradation = true);
        void _analyze_pass2(ast::ast_base* ast_node, bool type_degradation);
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
        void mov_value_to_cr(opnum::opnumbase& op_num, ir_compiler* compiler);

        std::vector<ast::ast_value_function_define* > in_used_functions;

        opnum::opnumbase& get_new_global_variable();
        std::variant<opnum::opnumbase*, int16_t> get_opnum_by_symbol(ast::ast_base* error_prud, lang_symbol* symb, ir_compiler* compiler, bool get_pure_value = false);

        template<typename T>
        class raii_value_clear_guard
        {
            T m_clear_value;
            T* m_aim_value;
        public:
            raii_value_clear_guard(T* aim_value, T clear_value)
                :m_aim_value(aim_value), m_clear_value(clear_value)
            {
                *m_aim_value = m_clear_value;
            }
            ~raii_value_clear_guard()
            {
                *m_aim_value = m_clear_value;
            }

            void set_value(const T& new_value)
            {
                m_clear_value = new_value;
            }
        };

        bool _cr_modified = false;
        bool _last_value_also_stored_to_cr = false;
        std::optional<int16_t> _last_value_from_stack_offset_may_null = std::nullopt;

        raii_value_clear_guard<bool>* _current_also_cr_store_flag;
        raii_value_clear_guard<std::optional<int16_t>>* _current_stack_offset_store_flag;

        void set_last_value_also_store_to_cr()
        {
            set_cr_modified();
            _current_also_cr_store_flag->set_value(true);
        }
        void set_stack_offset_stored(int16_t offset)
        {
            _current_stack_offset_store_flag->set_value(offset);
        }
        void set_cr_modified()
        {
            _cr_modified = true;
        }

        opnum::opnumbase& analyze_value(ast::ast_value* value, ir_compiler* compiler, bool get_pure_value = false);
        opnum::opnumbase& analyze_value_and_get_cr_modified(ast::ast_value* value, ir_compiler* compiler, bool get_pure_value, bool* out_cr_modified)
        {
            bool old_state = _cr_modified;
            _cr_modified = false;

            auto& result = analyze_value(value, compiler, get_pure_value);

            *out_cr_modified = _cr_modified;

            _cr_modified = _cr_modified || old_state;
            return result;
        }
        opnum::opnumbase& auto_analyze_value(ast::ast_value* value, ir_compiler* compiler, bool get_pure_value = false);

        struct loop_label_info
        {
            wo_pstring_t current_loop_label;

            std::string current_loop_break_aim_tag;
            std::string current_loop_continue_aim_tag;
        };

        std::vector<loop_label_info> loop_stack_for_break_and_continue;

        void real_analyze_finalize(ast::ast_base* ast_node, ir_compiler* compiler);
        void analyze_finalize(ast::ast_base* ast_node, ir_compiler* compiler);
        lang_scope* begin_namespace(ast::ast_namespace* a_namespace);
        void end_namespace();
        lang_scope* begin_scope(ast::ast_base* block_beginer);
        void end_scope();
        lang_scope* begin_function(ast::ast_value_function_define* ast_value_funcdef);
        void end_function();
        lang_scope* now_scope() const;
        lang_scope* now_namespace() const;
        lang_scope* in_function() const;
        lang_scope* in_function_pass2() const;
        void _anylize_func_def_in_pass2(ast::ast_value_function_define* fdef);

        size_t global_symbol_index = 0;

        enum class template_style
        {
            NORMAL,
            IS_TEMPLATE_VARIABLE_DEFINE,
            IS_TEMPLATE_VARIABLE_IMPL
        };

        lang_symbol* define_variable_in_this_scope(
            ast::ast_base* errreporter,
            wo_pstring_t names,
            ast::ast_value* init_val,
            ast::ast_decl_attribute* attr,
            template_style is_template_value,
            ast::identifier_decl mutable_type,
            size_t captureindex = (size_t)-1);
        lang_symbol* define_type_in_this_scope(ast::ast_using_type_as* def, ast::ast_type* as_type, ast::ast_decl_attribute* attr);

        bool check_symbol_is_accessable(lang_symbol* symbol, lang_scope* current_scope, ast::ast_base* ast, bool give_error);

        lang_symbol* find_symbol_in_this_scope(ast::ast_symbolable_base* var_ident, wo_pstring_t ident_str, int target_type_mask, bool fuzzy_for_err_report);
        lang_symbol* find_type_in_this_scope(ast::ast_type* var_ident);

        // Only used for check symbol is exist?
        lang_symbol* find_value_symbol_in_this_scope(ast::ast_value_variable* var_ident);

        lang_symbol* find_value_in_this_scope(ast::ast_value_variable* var_ident);
        bool has_compile_error()const;

        bool record_error_for_constration(ast::ast_where_constraint_constration* c, std::function<void(void)> job);
        void report_error_for_constration(ast::ast_base* b, ast::ast_where_constraint_constration* c, const wchar_t* errmsg);
    public:
        static bool check_if_need_try_operation_overload_binary(ast::ast_type* left, ast::ast_type* right, lex_type op, ast::ast_type** out_type);
        static bool check_if_need_try_operation_overload_assign(ast::ast_type* left, ast::ast_type* right, lex_type op);
        static bool check_if_need_try_operation_overload_logical(ast::ast_type* left, ast::ast_type* right, lex_type op);
    };
}