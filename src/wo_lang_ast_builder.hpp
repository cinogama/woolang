#pragma once
#include "wo_compiler_parser.hpp"
#include "wo_meta.hpp"
#include "wo_basic_type.hpp"
#include "wo_env_locale.hpp"
#include "wo_lang_functions_for_ast.hpp"
#include "wo_lang_extern_symbol_loader.hpp"
#include "wo_source_file_manager.hpp"
#include "wo_utf8.hpp"
#include "wo_memory.hpp"
#include "wo_const_string_pool.hpp"
#include "wo_crc_64.hpp"

#include <type_traits>
#include <cmath>
#include <unordered_map>
#include <algorithm>

namespace wo
{
#define WO_REINSTANCE(ITEM) do {if(ITEM){(ITEM) = dynamic_cast<meta::origin_type<decltype(ITEM)>>((ITEM)->instance());}}while(0)
    namespace opnum
    {
        struct opnumbase;
    }

    grammar* get_wo_grammar(void);
    namespace ast
    {
#if 1
        struct astnode_builder
        {
            using ast_basic = wo::ast::ast_base;
            using inputs_t = std::vector<grammar::produce>;
            using builder_func_t = std::function<grammar::produce(lexer&, inputs_t&)>;

            virtual ~astnode_builder() = default;
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                wo_assert(false, "");
                return nullptr;
            }
        };

        inline std::unordered_map<size_t, size_t> _registed_builder_function_id_list;
        inline std::vector<astnode_builder::builder_func_t> _registed_builder_function;

        template <typename T>
        size_t _register_builder()
        {
            static_assert(std::is_base_of<astnode_builder, T>::value);
            _registed_builder_function.push_back(T::build);
            return _registed_builder_function.size();
        }

        template <typename T>
        size_t index()
        {
            size_t idx = _registed_builder_function_id_list[meta::type_hash<T>];
            wo_assert(idx != 0);
            return idx;
        }

        inline astnode_builder::builder_func_t get_builder(size_t idx)
        {
            wo_assert(idx != 0);
            return _registed_builder_function[idx - 1];
        }
#endif
        /////////////////////////////////////////////////////////////////////////////////

        enum class identifier_decl
        {
            IMMUTABLE,
            MUTABLE,
        };

        struct ast_value;
        ast_value* dude_dump_ast_value(ast_value*);
        struct ast_type;
        ast_type* dude_dump_ast_type(ast_type*);

        struct ast_symbolable_base : virtual ast::ast_base
        {
            std::vector<wo_pstring_t> scope_namespaces;
            bool search_from_global_namespace = false;

            ast_type* searching_from_type = nullptr;

            lang_symbol* symbol = nullptr;

            lang_scope* searching_begin_namespace_in_pass2 = nullptr;

            std::wstring get_namespace_chain() const;
            std::string get_full_namespace_chain_after_pass1() const;
            ast::ast_base* instance(ast_base* child_instance = nullptr) const override;
        };

        struct ast_decl_attribute;

        struct ast_type : virtual public ast_symbolable_base
        {
            bool is_variadic_function_type = false;

            bool is_mutable_type = false;
            bool is_force_immutable_type = false;

            bool is_non_update_custom_type = false;

            // if this type is function, following type information will describe the return type;
            wo_pstring_t type_name = nullptr;
            ast_type* function_ret_type = nullptr;

            value::valuetype value_type;

            std::vector<ast_type*> argument_types;
            std::vector<ast_type*> template_arguments;

            struct struct_offset
            {
                ast_type* member_type = nullptr;
                uint16_t            offset = (uint16_t)0xFFFF;

                ast_decl_attribute* member_decl_attribute = nullptr;
                std::vector<size_t> union_used_template_index;
            };

            using struct_member_infos_t = std::unordered_map<wo_pstring_t, struct_offset>;
            struct_member_infos_t struct_member_index;

            ast_type* using_type_name = nullptr;
            ast_value* typefrom = nullptr;

            inline static const std::unordered_map<wo_pstring_t, value::valuetype> name_type_pair =
            {
                {WO_PSTR(int), value::valuetype::integer_type},
                {WO_PSTR(handle), value::valuetype::handle_type},
                {WO_PSTR(real), value::valuetype::real_type},
                {WO_PSTR(string), value::valuetype::string_type},
                {WO_PSTR(dict), value::valuetype::dict_type},
                {WO_PSTR(array), value::valuetype::array_type},
                {WO_PSTR(map), value::valuetype::dict_type},
                {WO_PSTR(vec), value::valuetype::array_type},
                {WO_PSTR(gchandle), value::valuetype::gchandle_type},
                {WO_PSTR(bool), value::valuetype::bool_type},
                {WO_PSTR(nil), value::valuetype::invalid},

                // special type
                {WO_PSTR(union), value::valuetype::invalid},
                {WO_PSTR(struct), value::valuetype::invalid},
                {WO_PSTR(tuple), value::valuetype::invalid},

                {WO_PSTR(function), value::valuetype::invalid},

                {WO_PSTR(void), value::valuetype::invalid},
                {WO_PSTR(nothing), value::valuetype::invalid}, // Buttom type.
                {WO_PSTR(pending), value::valuetype::invalid},
                {WO_PSTR(dynamic), value::valuetype::invalid},
            };

            static wo_pstring_t get_name_from_type(value::valuetype _type);
            static value::valuetype get_type_from_name(wo_pstring_t name);
            static bool check_castable(ast_type* to, ast_type* from);
            static bool is_custom_type(wo_pstring_t name);
            void set_type_with_name(wo_pstring_t _type_name);
            void set_type(const ast_type* _type);
            void set_type_with_constant_value(const value& _val);
            void set_ret_type(const ast_type* _type);
            ast_type(wo_pstring_t _type_name);
            ast_type* get_return_type() const;
            void append_function_argument_type(ast_type* arg_type);
            void set_as_variadic_arg_func();

            void set_is_mutable(bool is_mutable);
            void set_is_force_immutable();

            bool has_typeof() const;

            bool is_mutable() const;
            bool is_force_immutable() const;

            bool is_dynamic() const;
            bool has_custom() const;
            bool is_custom() const;
            bool is_pure_pending() const;
            bool is_hkt() const;
            bool is_hkt_typing() const;
            bool is_pending() const;
            bool may_need_update() const;
            bool is_pending_function() const;
            bool is_void() const;
            bool is_nil() const;
            bool is_gc_type() const;
            static lang_symbol* base_typedef_symbol(lang_symbol* symb);
            bool is_like(const ast_type* another, const std::vector<wo_pstring_t>& termplate_set, ast_type** out_para = nullptr, ast_type** out_args = nullptr)const;
            bool is_same(const ast_type* another, bool ignore_prefix) const;
            bool accept_type(const ast_type* another, bool ignore_using_type, bool ignore_prefix) const;
            bool set_mix_types(ast_type* another, bool ignore_mutable);

            bool is_builtin_basic_type();
            bool is_bool() const;
            bool is_char() const;
            bool is_builtin_using_type() const;
            bool is_union() const;
            bool is_tuple() const;
            bool is_nothing() const;
            bool is_struct() const;
            bool has_template() const;
            bool is_function() const;
            bool is_string() const;
            bool is_waiting_create_template_for_auto() const;
            bool is_pure_base_type() const;
            bool is_array() const;
            bool is_dict() const;
            bool is_vec() const;
            bool is_map() const;
            bool is_integer() const;
            bool is_real() const;
            bool is_handle() const;
            bool is_gchandle() const;
            std::wstring get_type_name(bool ignore_using_type = true, bool ignore_mut = false) const;
            ast::ast_base* instance_impl(ast_base* child_instance, bool clone_raw_struct_member) const;
            ast::ast_base* instance(ast_base* child_instance = nullptr) const override;
        };

        struct ast_value : virtual public ast::ast_base
        {
            // this type of ast node is used for stand a value or product a value.
            // liter functioncall variable and so on will belong this type of node.
            ast_type* value_type = nullptr;

            bool can_be_assign = false;
            bool is_evaling_constant = false;

            bool is_constant = false;
            wo::value constant_value = {};

            ast_value& operator = (ast_value&&) = delete;
            ast_value& operator = (const ast_value&) = default;

            ast_value(const ast_value&) = delete;
            ast_value(ast_value&&) = delete;

            ~ast_value();
        protected:
            ast_value(ast_type* type);
        public:
            wo::value& get_constant_value();
        public:
            void eval_constant_value(lexer* lex)
            {
                if (is_constant || is_evaling_constant)
                    return;

                is_evaling_constant = true;
                update_constant_value(lex);
                is_evaling_constant = false;

            }
            ast::ast_base* instance(ast_base* child_instance = nullptr) const override;
        private:
            virtual void update_constant_value(lexer* lex);
        };

        struct ast_value_mutable : virtual public ast_value
        {
            ast_value* val = nullptr;
            lex_type mark_type = lex_type::l_error;

            ast_value_mutable()
                : ast_value(new ast_type(WO_PSTR(pending)))
            {
            }

            void update_constant_value(lexer* lex) override
            {
                if (is_constant)
                    return;

                val->eval_constant_value(lex);

                if (val->is_constant)
                {
                    is_constant = true;
                    constant_value.set_val_compile_time(&val->constant_value);
                }
            }

            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_value::instance(dumm);

                // Write self copy functions here..
                WO_REINSTANCE(dumm->val);

                return dumm;
            }
        };

        struct ast_value_takeplace : virtual public ast_value
        {
            opnum::opnumbase* used_reg = nullptr;
            ast_value_takeplace()
                : ast_value(new ast_type(WO_PSTR(pending)))
            {
            }
            void update_constant_value(lexer* lex) override
            {
                // do nothing..
            }
            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_value::instance(dumm);

                // Write self copy functions here..

                return dumm;
            }
        };

        struct ast_value_literal : virtual public ast_value
        {
            void update_constant_value(lexer* lex) override;
            static wo_handle_t wstr_to_handle(const std::wstring& str);
            static wo_integer_t wstr_to_integer(const std::wstring& str);
            static wo_real_t wstr_to_real(const std::wstring& str);
            ast_value_literal();
            ast_value_literal(const token& te);
            ast::ast_base* instance(ast_base* child_instance = nullptr) const override;
        };

        struct ast_value_typeid : virtual public ast_value
        {
            ast_type* type;
            ast_value_typeid() : ast_value(new ast_type(WO_PSTR(int)))
            {

            }
            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_value::instance(dumm);

                // Write self copy functions here..
                WO_REINSTANCE(dumm->type);

                return dumm;
            }
            void update_constant_value(lexer* lex) override
            {
                // DO NOTHING, IT WILL BE UPDATE IN PASS2.
            }
        };

        struct ast_value_type_cast : public virtual ast_value
        {
            ast_value* _be_cast_value_node;

            ast_value_type_cast(ast_value* value, ast_type* target_type);
            ast_value_type_cast();
            ast::ast_base* instance(ast_base* child_instance = nullptr) const override;
            void update_constant_value(lexer* lex) override;
        };

        struct ast_value_type_judge : public virtual ast_value
        {
            ast_value* _be_cast_value_node;

            ast_value_type_judge(ast_value* value, ast_type* type);
            ast_value_type_judge();
            ast::ast_base* instance(ast_base* child_instance = nullptr) const override;
            void update_constant_value(lexer* lex) override;
        };

        struct ast_value_type_check : public virtual ast_value
        {
            ast_value* _be_check_value_node;
            ast_type* aim_type;

            ast_value_type_check(ast_value* value, ast_type* type)
                : ast_value(new ast_type(WO_PSTR(bool)))
            {
                _be_check_value_node = value;
                aim_type = type;
            }
            ast_value_type_check() : ast_value(new ast_type(WO_PSTR(bool))) {}
            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_value::instance(dumm);

                // Write self copy functions here..
                WO_REINSTANCE(dumm->_be_check_value_node);
                WO_REINSTANCE(dumm->aim_type);

                return dumm;
            }

            void update_constant_value(lexer* lex) override
            {
                if (is_constant)
                    return;

                _be_check_value_node->eval_constant_value(lex);

                if (!_be_check_value_node->value_type->is_pending() && !aim_type->is_pending())
                {
                    auto result = aim_type->accept_type(_be_check_value_node->value_type, false, true);
                    if (result)
                    {
                        is_constant = true;
                        constant_value.set_bool(result);
                    }
                    if (_be_check_value_node->value_type->is_dynamic())
                    {
                        if (!aim_type->is_dynamic())
                            return; // do nothing... give error in analyze_finalize
                    }
                    is_constant = true;
                    constant_value.set_bool(result);
                }
            }
        };

        struct ast_decl_attribute : virtual public ast::ast_base
        {
            std::set<lex_type> attributes;
            void varify_attributes(lexer* lex) const;
            void add_attribute(lexer* lex, lex_type attr);
            bool is_static_attr() const;
            bool is_private_attr() const;
            bool is_protected_attr() const;
            bool is_public_attr() const;
            bool is_extern_attr() const;
            ast::ast_base* instance(ast_base* child_instance = nullptr) const override;
        };

        struct ast_defines : virtual public ast::ast_base
        {
            ast_decl_attribute* declear_attribute = nullptr;

            bool template_reification_judge_by_funccall = false;// if template_reification_judge_by_funccall==true, pass2 will not checkxx
            bool is_template_define = false;
            bool is_template_reification = false; // if is_template_reification == true, symbol will not put to overset..
            ast_defines* reification_defines = nullptr;

            lang_symbol* this_reification_lang_symbol = nullptr;
            std::vector<ast_type*> this_reification_template_args;
            std::vector<wo_pstring_t> template_type_name_list;
            std::map<std::vector<uint32_t>, ast_defines*> template_typehashs_reification_instance_list;

            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // ast_value::instance(dumm);

                // Write self copy functions here..
                WO_REINSTANCE(dumm->declear_attribute);

                // Donot re-instance reification_defines.
                // dumm->reification_defines = reification_defines;

                return dumm;
            }
        };

        struct ast_value_variable : virtual ast_symbolable_base, virtual ast_value
        {
            wo_pstring_t var_name = nullptr;
            std::vector<ast_type*> template_reification_args;
            bool directed_function_call = false;
            bool is_this_value_used_for_function_call = false;

            std::wstring get_full_variable_name()
            {
                return get_namespace_chain() + *var_name;
            }

            ast_value_variable(wo_pstring_t _var_name, ast_type* type = new ast_type(WO_PSTR(pending)))
                : ast_value(type)
            {
                var_name = _var_name;
            }
            ast_value_variable(ast_type* type = new ast_type(WO_PSTR(pending)))
                : ast_value(type) {}
            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_value::instance(dumm);
                ast_symbolable_base::instance(dumm);
                // Write self copy functions here..

                for (auto& tras : dumm->template_reification_args)
                {
                    WO_REINSTANCE(tras);
                }
                return dumm;
            }
            void update_constant_value(lexer* lex) override;
        };

        struct ast_list : virtual public ast::ast_base
        {
            void append_at_head(ast::ast_base* astnode)
            {
                // item LIST
                if (children)
                {
                    auto old_last = last;
                    auto old_child = children;

                    remove_all_childs();
                    add_child(astnode);

                    astnode->sibling = old_child;
                    last = old_last;

                    auto* node = old_child;
                    while (node)
                    {
                        node->parent = this;
                        node = node->sibling;
                    }
                }
                else
                    add_child(astnode);
            }
            void append_at_end(ast::ast_base* astnode)
            {
                // LIST item
                add_child(astnode);
            }

            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // ast_value_symbolable_base::instance(dumm);
                // Write self copy functions here..

                return dumm;
            }
        };

        using ast_empty = grammar::ast_empty;

        struct ast_value_funccall;

        struct ast_value_binary : virtual public ast_value
        {
            // used for storing binary-operate;
            ast_value* left = nullptr;
            lex_type operate = lex_type::l_error;
            ast_value* right = nullptr;

            ast_value_funccall* overrided_operation_call = nullptr;

            template <typename T>
            static T binary_operate(lexer& lex, T left, T right, lex_type op_type, bool* out_result)
            {
                *out_result = true;
                switch (op_type)
                {
                case lex_type::l_add:
                    return left + right;
                case lex_type::l_sub:
                    if constexpr (!std::is_same<T, wo::string_t>::value)
                        return left - right;
                case lex_type::l_mul:
                    if constexpr (!std::is_same<T, wo::string_t>::value
                        && !std::is_same<T, wo_handle_t>::value)
                        return left * right;
                case lex_type::l_div:
                    if constexpr (!std::is_same<T, wo::string_t>::value
                        && !std::is_same<T, wo_handle_t>::value)
                    {
                        if constexpr (std::is_same<T, wo_integer_t>::value)
                        {
                            if (right == 0)
                            {
                                *out_result = false;
                                return T{};
                            }
                            if (right == -1 && left == INT64_MIN)
                            {
                                *out_result = false;
                                return T{};
                            }
                        }
                        return left / right;
                    }
                case lex_type::l_mod:
                    if constexpr (!std::is_same<T, wo::string_t>::value
                        && !std::is_same<T, wo_handle_t>::value)
                    {
                        if constexpr (std::is_same<T, wo_real_t>::value)
                        {
                            return fmod(left, right);
                        }
                        else
                        {
                            if constexpr (std::is_same<T, wo_integer_t>::value)
                            {
                                if (right == 0)
                                {
                                    *out_result = false;
                                    return T{};
                                }
                                if (right == -1 && left == INT64_MIN)
                                {
                                    *out_result = false;
                                    return T{};
                                }
                            }
                            return left % right;
                        }
                    }
                default:
                    *out_result = false;
                }

                return T{};
            }

            ast_value_binary();
            ast::ast_base* instance(ast_base* child_instance = nullptr) const override;

            static ast_type* binary_upper_type(ast_type* left_v, ast_type* right_v);
            static ast_type* binary_upper_type_with_operator(ast_type* left_v, ast_type* right_v, lex_type op);
            void update_constant_value(lexer* lex) override;
        };

        struct ast_namespace : virtual public ast::ast_base
        {
            wo_pstring_t scope_name = nullptr;
            ast_list* in_scope_sentence = nullptr;

            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // ast_value::instance(dumm);
                // Write self copy functions here..

                WO_REINSTANCE(dumm->in_scope_sentence);

                return dumm;
            }
        };

        struct ast_pattern_base : virtual public ast::ast_base
        {
            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // ast_defines::instance(dumm);
                // Write self copy functions here..

                return dumm;
            }
        };

        struct ast_value_init;

        struct ast_varref_defines : virtual public ast_defines
        {
            lang_scope* located_function = nullptr;
            struct varref_define
            {
                ast_pattern_base* pattern;
                ast_value_init* init_val;
            };
            std::vector<varref_define> var_refs;

            ast::ast_base* instance(ast_base* child_instance = nullptr) const override;
        };

        struct ast_value_arg_define : virtual ast_value, virtual ast_symbolable_base, virtual ast_defines
        {
            identifier_decl decl = identifier_decl::IMMUTABLE;
            wo_pstring_t arg_name = nullptr;

            ast_value_arg_define(ast_type* type = new ast_type(WO_PSTR(pending)))
                : ast_value(type)
            {}

            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;

                ast_defines::instance(dumm);
                ast_symbolable_base::instance(dumm);
                ast_value::instance(dumm);
                // Write self copy functions here..

                return dumm;
            }
            void update_constant_value(lexer* lex) override
            {
                // do nothing
            }
        };
        struct ast_extern_info : virtual public ast::ast_base
        {
            wo_extern_native_func_t externed_func = nullptr;

            bool is_slow_leaving_call = false;
            bool is_repeat_check_ignored = false;

            std::optional<std::wstring> library_name;
            std::wstring symbol_name;

            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                wo_error("Duplicating ast_extern_info is not allowed.");
                return nullptr;
            }
        };
        struct ast_value_function_define;
        struct ast_where_constraint : virtual public ast::ast_base
        {
            ast_list* where_constraint_list;
            bool accept = true;

            std::vector<lexer::lex_error_msg> unmatched_constraint;

            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // Write self copy functions here..

                WO_REINSTANCE(dumm->where_constraint_list);
                dumm->accept = true;
                return dumm;
            }
        };
        struct ast_where_constraint_constration : virtual ast_base
        {
            ast_where_constraint* where_constraint = nullptr;

            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));

                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;

                WO_REINSTANCE(dumm->where_constraint);

                return dumm;
            }
        };
        struct ast_value_init : virtual ast_where_constraint_constration, virtual ast_value
        {
            ast_value* init_value;

            ast_value_init()
                : ast_value(new ast_type(WO_PSTR(pending)))
            {
            }

            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));

                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_value::instance(dumm);
                ast_where_constraint_constration::instance(dumm);

                WO_REINSTANCE(dumm->init_value);

                return dumm;
            }

            void update_constant_value(lexer* lex) override
            {
                if (init_value != nullptr)
                {
                    init_value->eval_constant_value(lex);
                    if (init_value->is_constant)
                    {
                        is_constant = true;
                        constant_value.set_val_compile_time(&init_value->constant_value);
                    }
                }
            }
        };
        struct ast_value_function_define :
            virtual ast_value,
            virtual ast_symbolable_base,
            virtual ast_defines,
            virtual ast_where_constraint_constration
        {
            wo_pstring_t function_name = nullptr;
            ast_list* argument_list = nullptr;
            ast_list* in_function_sentence = nullptr;
            bool auto_adjust_return_type = false;
            bool delay_adjust_return_type = false;

            // has_return_value used for check if sentences in function has 
            // 'return'. will be reset to false at begining of pass2 for 
            // some cases like conditional compilation.
            bool has_return_value = false;
            bool ir_func_has_been_generated = false;

            std::string ir_func_signature_tag = "";
            lang_scope* this_func_scope = nullptr;
            ast_extern_info* externed_func_info = nullptr;
            std::vector<lang_symbol*> capture_variables;

            ast_value_function_define(ast_type* type = new ast_type(WO_PSTR(pending)))
                : ast_value(type)
            {
                type->set_ret_type(new ast_type(WO_PSTR(pending)));
            }

            bool is_closure_function()const noexcept
            {
                return capture_variables.size();
            }

            const std::string& get_ir_func_signature_tag()
            {
                if (ir_func_signature_tag == "")
                {
                    //TODO : Change new function to generate signature.
                    auto spacename = get_full_namespace_chain_after_pass1();

                    ir_func_signature_tag =
                        spacename.empty() ? "func " : ("func " + spacename + "::");

                    if (function_name != nullptr)
                        ir_func_signature_tag += wstr_to_str(*function_name);
                    else
                        ir_func_signature_tag += std::to_string((uint64_t)this);

                    if (is_template_reification)
                    {
                        ir_func_signature_tag += "<";

                        for (auto idx = this_reification_template_args.begin()
                            ; idx != this_reification_template_args.end(); idx++)
                        {
                            ir_func_signature_tag += wstr_to_str((*idx)->get_type_name(false));
                            if (idx + 1 != this_reification_template_args.end())
                                ir_func_signature_tag += ",";
                        }

                        ir_func_signature_tag += ">";
                    }

                    ir_func_signature_tag += "(...) : " + wstr_to_str(value_type->get_type_name(false));
                }
                return ir_func_signature_tag;
            }
            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));

                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_where_constraint_constration::instance(dumm);
                ast_defines::instance(dumm);
                ast_symbolable_base::instance(dumm);
                ast_value::instance(dumm);
                // Write self copy functions here..

                WO_REINSTANCE(dumm->argument_list);
                WO_REINSTANCE(dumm->in_function_sentence);

                // ISSUE 1.13: externed_func_info should not be re-instance here.
                //              just copy it.
                // WO_REINSTANCE(dumm->externed_func_info);

                // NOTE: Reset `this_func_scope`, template function instance will create 
                // new scope to store itselfs variable & other symbols.
                dumm->this_func_scope = nullptr;

                dumm->capture_variables.clear();

                return dumm;
            }
            void update_constant_value(lexer* lex) override
            {
                // do nothing
            }
        };

        struct ast_token : virtual ast::ast_base
        {
            token tokens;

            ast_token(const token& tk)
                : tokens(tk)
            {
            }

            ast_token() :tokens({ lex_type::l_error })
            {

            }

            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // ast_defines::instance(dumm);
                 // Write self copy functions here..

                return dumm;
            }
        };

        struct ast_return : virtual public ast::ast_base
        {
            ast_value* return_value = nullptr;
            ast_value_function_define* located_function = nullptr;

            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // Write self copy functions here..
                WO_REINSTANCE(dumm->return_value);
                dumm->located_function = nullptr;

                return dumm;
            }
        };

        struct ast_value_funccall : virtual public ast_value
        {
            ast_value* called_func;
            ast_list* arguments;

            ast_value* directed_value_from = nullptr;
            ast_value_variable* callee_symbol_in_type_namespace = nullptr;

            // Will be setting in pass1, do more check and modify in apply..
            bool try_invoke_operator_override_function = false;

            ast_value_funccall()
                : ast_value(new ast_type(WO_PSTR(pending)))
            {

            }

            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_value::instance(dumm);
                // Write self copy functions here..

                ast_value_function_define* called_func_def = dynamic_cast<ast_value_function_define*>(dumm->called_func);
                if (!called_func_def || called_func_def->function_name == nullptr)
                {
                    WO_REINSTANCE(dumm->called_func);
                }
                WO_REINSTANCE(dumm->arguments);
                WO_REINSTANCE(dumm->directed_value_from);
                WO_REINSTANCE(dumm->callee_symbol_in_type_namespace);

                return dumm;
            }

            void update_constant_value(lexer* lex) override
            {
                // TODO: MAYBE THERE IS CONSTEXPR FUNC?
                // do nothing
            }
        };

        struct ast_value_array : virtual public ast_value
        {
            ast_list* array_items;
            bool is_mutable_vector;
            ast_value_array(ast_list* _items, bool is_mutable)
                : ast_value(is_mutable ? new ast_type(WO_PSTR(vec)) : new ast_type(WO_PSTR(array)))
                , array_items(_items)
                , is_mutable_vector(is_mutable)
            {
                wo_assert(array_items != nullptr && value_type->template_arguments.empty());
                value_type->template_arguments.push_back(new ast_type(WO_PSTR(pending)));
            }

            ast_value_array() 
                : ast_value(new ast_type(WO_PSTR(pending))) 
                , array_items(nullptr)
                , is_mutable_vector(false)
            {
            }
            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_value::instance(dumm);
                // Write self copy functions here..

                WO_REINSTANCE(dumm->array_items);

                return dumm;
            }

            void update_constant_value(lexer* lex) override
            {
                // do nothing
            }
        };

        struct ast_value_mapping : virtual public ast_value
        {
            ast_list* mapping_pairs;
            bool is_mutable_map;
            ast_value_mapping(ast_list* _items, bool is_mutable)
                : ast_value(is_mutable ? new ast_type(WO_PSTR(map)) : new ast_type(WO_PSTR(dict)))
                , mapping_pairs(_items)
                , is_mutable_map(is_mutable)
            {
                wo_assert(mapping_pairs != nullptr && value_type->template_arguments.empty());
                value_type->template_arguments.push_back(new ast_type(WO_PSTR(pending)));
                value_type->template_arguments.push_back(new ast_type(WO_PSTR(pending)));
            }

            ast_value_mapping()
                : ast_value(new ast_type(WO_PSTR(pending)))
                , mapping_pairs(nullptr)
                , is_mutable_map(false)
            {
            }
            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_value::instance(dumm);
                // Write self copy functions here..

                WO_REINSTANCE(dumm->mapping_pairs);

                return dumm;
            }

            void update_constant_value(lexer* lex) override
            {
                // do nothing
            }
        };

        struct ast_sentence_block : virtual public ast::ast_base
        {
            ast_list* sentence_list;

            ast_sentence_block(ast_list* sentences)
                : sentence_list(sentences)
            {
                wo_assert(sentence_list);
            }

            static ast_sentence_block* fast_parse_sentenceblock(ast::ast_base* ast)
            {
                if (auto r = dynamic_cast<ast_sentence_block*>(ast))
                    return r;

                ast_list* list = nullptr;
                if (nullptr == dynamic_cast<ast_empty*>(ast))
                {
                    if (auto* lst = dynamic_cast<ast_list*>(ast))
                    {
                        list = lst;
                    }
                    else
                    {
                        list = new ast_list;
                        list->append_at_end(ast);
                    }
                }
                else
                {
                    list = new ast_list;
                    // emplace nothing..
                }
                ast_sentence_block* result = new ast_sentence_block(list);

                return result;
            }

            ast_sentence_block() {}
            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // ast_value::instance(dumm);
                // Write self copy functions here..

                WO_REINSTANCE(dumm->sentence_list);

                return dumm;
            }
        };

        struct ast_if : virtual public ast::ast_base
        {
            ast_value* judgement_value;
            ast_base* execute_if_true;
            ast_base* execute_else;

            bool is_constexpr_if = false;

            ast_if(ast_value* jdg, ast_base* exe_true, ast_base* exe_else)
                : judgement_value(jdg), execute_if_true(exe_true), execute_else(exe_else)
            {
                wo_assert(judgement_value && execute_if_true);
            }

            ast_if() {}
            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // ast_value::instance(dumm);
                // Write self copy functions here..

                WO_REINSTANCE(dumm->judgement_value);
                WO_REINSTANCE(dumm->execute_if_true);
                WO_REINSTANCE(dumm->execute_else);

                return dumm;
            }
        };

        struct ast_while : virtual public ast::ast_base
        {
            ast_value* judgement_value;
            ast_base* execute_sentence;

            ast_while(ast_value* jdg, ast_base* exec)
                : judgement_value(jdg), execute_sentence(exec)
            {
                wo_assert(nullptr != execute_sentence);
            }

            ast_while() {}
            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // ast_value::instance(dumm);
                // Write self copy functions here..

                WO_REINSTANCE(dumm->judgement_value);
                WO_REINSTANCE(dumm->execute_sentence);

                return dumm;
            }
        };

        struct ast_value_assign : virtual public ast_value
        {
            // used for storing binary-operate;
            ast_value* left = nullptr;
            lex_type operate = lex_type::l_error;
            ast_value* right = nullptr;

            bool is_value_assgin = false;

            ast_value_funccall* overrided_operation_call = nullptr;

            ast_value_assign() : ast_value(new ast_type(WO_PSTR(void)))
            {
            }

            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_value::instance(dumm);
                // Write self copy functions here..

                WO_REINSTANCE(dumm->left);
                WO_REINSTANCE(dumm->right);
                WO_REINSTANCE(dumm->overrided_operation_call);

                return dumm;
            }

            void update_constant_value(lexer*) override
            {
                // do nothing
            }
        };

        struct ast_value_logical_binary : virtual public ast_value
        {
            // used for storing binary-operate;
            ast_value* left = nullptr;
            lex_type operate = lex_type::l_error;
            ast_value* right = nullptr;

            ast_value_funccall* overrided_operation_call = nullptr;

            ast_value_logical_binary();
            ast::ast_base* instance(ast_base* child_instance = nullptr) const override;
            void update_constant_value(lexer* lex) override;
        };

        struct ast_value_index : virtual public ast_value
        {
            ast_value* from = nullptr;
            ast_value* index = nullptr;

            uint16_t struct_offset = 0xFFFF;

            ast_value_index() :ast_value(new ast_type(WO_PSTR(pending)))
            {
            }

            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_value::instance(dumm);
                // Write self copy functions here..

                WO_REINSTANCE(dumm->from);
                WO_REINSTANCE(dumm->index);

                return dumm;
            }

            void update_constant_value(lexer* lex) override
            {
                from->eval_constant_value(lex);
                index->eval_constant_value(lex);

                // if left/right is custom, donot calculate them 
                if (!from->value_type->is_builtin_basic_type()
                    || !index->value_type->is_builtin_basic_type())
                    return;

                if (from->is_constant && index->is_constant)
                {
                    if (from->value_type->is_string())
                    {
                        is_constant = true;
                        value_type->set_type_with_name(WO_PSTR(char));

                        if (!index->value_type->is_integer() && !index->value_type->is_handle())
                        {
                            is_constant = false;
                            return;
                        }

                        wchar_t out_str = u8strnidx(
                            from->get_constant_value().string->c_str(),
                            from->get_constant_value().string->size(),
                            (size_t)index->get_constant_value().integer);
                        if (out_str == 0 && u8strnlen(
                            from->get_constant_value().string->c_str(),
                            from->get_constant_value().string->size())
                            <= (size_t)index->get_constant_value().integer)
                            lex->lang_error(lexer::errorlevel::error, index, WO_ERR_INDEX_OUT_OF_RANGE);

                        constant_value.set_integer((wo_integer_t)(wo_handle_t)out_str);
                    }
                }
            }
        };

        struct ast_value_packed_variadic_args : virtual public ast_value
        {
            ast_value_packed_variadic_args() :ast_value(new ast_type(WO_PSTR(array)))
            {
                value_type->template_arguments.push_back(new ast_type(WO_PSTR(dynamic)));
            }

            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_value::instance(dumm);
                // Write self copy functions here..
                return dumm;
            }

            void update_constant_value(lexer*) override
            {
                // DO NOTHING
            }
        };

        struct ast_value_indexed_variadic_args : virtual public ast_value
        {
            ast_value* argindex;
            ast_value_indexed_variadic_args(ast_value* arg_index)
                : ast_value(new ast_type(WO_PSTR(dynamic)))
                , argindex(arg_index)
            {
                wo_assert(argindex);
            }

            ast_value_indexed_variadic_args() :ast_value(new ast_type(WO_PSTR(pending))) {}

            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_value::instance(dumm);
                // Write self copy functions here..

                WO_REINSTANCE(dumm->argindex);

                return dumm;
            }

            void update_constant_value(lexer*) override
            {
                // DO NOTHING
            }
        };

        struct ast_fakevalue_unpacked_args : virtual public ast_value
        {
            constexpr static int32_t UNPACK_ALL_ARGUMENT = INT32_MAX;

            ast_value* unpacked_pack = nullptr;
            int32_t expand_count = UNPACK_ALL_ARGUMENT;

            ast_fakevalue_unpacked_args(ast_value* pak, int32_t _expand_count)
                : ast_value(new ast_type(WO_PSTR(dynamic)))
                , unpacked_pack(pak)
                , expand_count(_expand_count)
            {
                wo_assert(unpacked_pack && _expand_count >= 0);
            }

            ast_fakevalue_unpacked_args() : ast_value(new ast_type(WO_PSTR(pending))) {}
            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_value::instance(dumm);
                // Write self copy functions here..

                WO_REINSTANCE(dumm->unpacked_pack);

                return dumm;
            }

            void update_constant_value(lexer*) override
            {
                // DO NOTHING
            }
        };

        struct ast_value_unary : virtual public ast_value
        {
            lex_type operate = lex_type::l_error;
            ast_value* val;

            ast_value_unary()
                :ast_value(new ast_type(WO_PSTR(pending)))
            {

            }

            ast_value_funccall* overrided_operation_call = nullptr;

            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_value::instance(dumm);
                // Write self copy functions here..

                WO_REINSTANCE(dumm->val);

                return dumm;
            }

            void update_constant_value(lexer* lex) override
            {
                val->eval_constant_value(lex);
                if (val->is_constant)
                {
                    is_constant = true;

                    if (operate == lex_type::l_sub)
                    {
                        value_type = val->value_type;
                        const auto& _rval = val->get_constant_value();
                        if (_rval.type == value::valuetype::integer_type)
                        {
                            constant_value.set_integer(-_rval.integer);
                        }
                        else if (_rval.type == value::valuetype::real_type)
                        {
                            constant_value.set_real(-_rval.real);
                        }
                        else
                        {
                            lex->lang_error(lexer::errorlevel::error, this, WO_ERR_TYPE_CANNOT_NEGATIVE, val->value_type->get_type_name().c_str());
                            return;
                        }
                    }
                    else /*if(_token.type == lex_type::l_lnot)*/
                    {
                        if (val->value_type->is_bool())
                        {
                            value_type->set_type_with_name(WO_PSTR(bool));
                            constant_value.set_bool(val->get_constant_value().integer == false);
                        }
                        else
                            is_constant = false;
                    }
                }
            }
        };

        struct ast_mapping_pair : virtual public ast::ast_base
        {
            ast_value* key;
            ast_value* val;
            ast_mapping_pair(ast_value* _k, ast_value* _v)
                : key(_k), val(_v)
            {
                wo_assert(key && val);
            }
            ast_mapping_pair() {}
            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // ast_value::instance(dumm);
                // Write self copy functions here..

                WO_REINSTANCE(dumm->key);
                WO_REINSTANCE(dumm->val);

                return dumm;
            }
        };

        struct ast_using_namespace : virtual public ast::ast_base
        {
            bool from_global_namespace;
            std::vector<wo_pstring_t> used_namespace_chain;

            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // ast_value::instance(dumm);
                // Write self copy functions here..

                return dumm;
            }
        };

        struct ast_enum_item : virtual public ast::ast_base
        {
            wo_pstring_t enum_ident = nullptr;
            wo_integer_t enum_val;
            bool need_assign_val = true;

            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // ast_value::instance(dumm);
                // Write self copy functions here..

                return dumm;
            }
        };

        struct ast_enum_items_list : virtual public ast::ast_base
        {
            wo_integer_t next_enum_val = 0;
            std::vector<ast_enum_item*> enum_items;

            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // ast_value::instance(dumm);
                // Write self copy functions here..

                for (auto& enumitem : dumm->enum_items)
                {
                    WO_REINSTANCE(enumitem);
                }

                return dumm;
            }
        };

        struct ast_using_type_as : virtual public ast_defines
        {
            ast_namespace* namespace_decl = nullptr;

            wo_pstring_t new_type_identifier = nullptr;
            ast_type* old_type = nullptr;

            lang_symbol* type_symbol = nullptr;

            std::map<std::wstring, ast::ast_value*> class_const_index_typing;
            std::map<std::wstring, std::vector<ast::ast_value_function_define*>> class_methods_list;

            bool is_alias = false;

            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_defines::instance(dumm);
                // Write self copy functions here..

                WO_REINSTANCE(dumm->old_type);
                // Donot deep-copy `namespace_decl`, it's not needed.

                // Clear type_symbol;
                dumm->type_symbol = nullptr;

                return dumm;
            }
        };

        struct ast_directed_values : virtual public ast::ast_base
        {
            ast_value* from;
            ast_value* direct_val;

            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // ast_defines::instance(dumm);
                // Write self copy functions here..

                WO_REINSTANCE(dumm->from);
                WO_REINSTANCE(dumm->direct_val);

                return dumm;
            }
        };

        struct ast_nop : virtual public ast::ast_base
        {
            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // Write self copy functions here..
                return dumm;
            }

        };

        struct ast_foreach : virtual public ast::ast_base
        {
            ast_varref_defines* used_iter_define; // Just used for taking place;;;
            ast_while* loop_sentences;

            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // ast_defines::instance(dumm);
                // Write self copy functions here..

                WO_REINSTANCE(dumm->used_iter_define);
                WO_REINSTANCE(dumm->loop_sentences);

                return dumm;
            }
        };

        struct ast_forloop : virtual public ast::ast_base
        {
            ast::ast_base* pre_execute;
            ast_value* judgement_expr;
            ast_value* after_execute;
            ast::ast_base* execute_sentences;

            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // ast_defines::instance(dumm);
                // Write self copy functions here..

                WO_REINSTANCE(dumm->pre_execute);
                WO_REINSTANCE(dumm->judgement_expr);
                WO_REINSTANCE(dumm->after_execute);
                WO_REINSTANCE(dumm->execute_sentences);
                return dumm;
            }
        };

        struct ast_break : virtual public ast::ast_base
        {
            wo_pstring_t label = nullptr;

            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // ast_defines::instance(dumm);
                // Write self copy functions here..

                return dumm;
            }
        };

        struct ast_continue : virtual public ast::ast_base
        {
            wo_pstring_t label = nullptr;

            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // ast_defines::instance(dumm);
                // Write self copy functions here..

                return dumm;
            }
        };

        struct ast_template_define : virtual public ast::ast_base
        {
            wo_pstring_t template_ident = nullptr;
            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // ast_defines::instance(dumm);
                // Write self copy functions here..

                return dumm;
            }
        };

        struct ast_union_item : virtual public ast::ast_base
        {
            wo_pstring_t identifier = nullptr;
            ast_type* type_may_nil = nullptr;
            ast_type* gadt_out_type_may_nil = nullptr;

            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // ast_defines::instance(dumm);
                // Write self copy functions here..

                WO_REINSTANCE(dumm->type_may_nil);
                WO_REINSTANCE(dumm->gadt_out_type_may_nil);
                return dumm;
            }
        };

        struct ast_union_make_option_ob_to_cr_and_ret : virtual public ast::ast_base
        {
            uint16_t id;
            ast_value_variable* argument_may_nil;

            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // ast_defines::instance(dumm);
                // Write self copy functions here..

                WO_REINSTANCE(dumm->argument_may_nil);

                return dumm;
            }
        };
        struct ast_pattern_takeplace : virtual public ast_pattern_base
        {
            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_pattern_base::instance(dumm);
                // Write self copy functions here..

                return dumm;
            }
        };
        struct ast_pattern_identifier : virtual public ast_pattern_base
        {
            identifier_decl decl = identifier_decl::IMMUTABLE;

            wo_pstring_t identifier = nullptr;
            lang_symbol* symbol = nullptr;
            ast_decl_attribute* attr = nullptr;

            std::vector<wo_pstring_t> template_arguments;

            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_pattern_base::instance(dumm);
                // Write self copy functions here..

                WO_REINSTANCE(dumm->attr);
                dumm->symbol = nullptr;

                return dumm;
            }
        };
        struct ast_pattern_tuple : virtual public ast_pattern_base
        {
            std::vector<ast_pattern_base*> tuple_patterns;
            std::vector<ast_value_takeplace*> tuple_takeplaces;

            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_pattern_base::instance(dumm);
                // Write self copy functions here..

                for (auto& pattern_in_tuple : dumm->tuple_patterns)
                {
                    WO_REINSTANCE(pattern_in_tuple);
                }
                for (auto& takeplaces : dumm->tuple_takeplaces)
                {
                    WO_REINSTANCE(takeplaces);
                }

                return dumm;
            }
        };
        struct ast_pattern_union_value : virtual public ast_pattern_base
        {
            // TMP IMPL!
            ast_value_variable* union_expr = nullptr;
            ast_pattern_base* pattern_arg_in_union_may_nil = nullptr;

            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_pattern_base::instance(dumm);
                // Write self copy functions here..

                WO_REINSTANCE(dumm->union_expr);
                WO_REINSTANCE(dumm->pattern_arg_in_union_may_nil);

                return dumm;
            }
        };

        struct ast_match;
        struct ast_match_case_base : virtual public ast::ast_base
        {
            ast_sentence_block* in_case_sentence;
            ast_match* in_match;
            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // ast_defines::instance(dumm);
                // Write self copy functions here..

                WO_REINSTANCE(dumm->in_case_sentence);

                return dumm;
            }
        };

        struct ast_match_union_case : virtual public ast_match_case_base
        {
            ast_pattern_union_value* union_pattern;
            ast_value_takeplace* take_place_value_may_nil;

            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_match_case_base::instance(dumm);
                // Write self copy functions here..

                WO_REINSTANCE(dumm->union_pattern);
                WO_REINSTANCE(dumm->take_place_value_may_nil);

                return dumm;
            }
        };


        struct ast_match : virtual public ast::ast_base
        {
            ast_value* match_value;
            ast_list* cases;

            std::string match_end_tag_in_final_pass;
            lang_scope* match_scope_in_pass;
            bool has_using_namespace = false;

            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // ast_defines::instance(dumm);
                // Write self copy functions here..

                WO_REINSTANCE(dumm->match_value);
                WO_REINSTANCE(dumm->cases);

                return dumm;
            }
        };

        struct ast_struct_member_define : virtual public ast::ast_base
        {
            wo_pstring_t member_name = nullptr;
            ast_decl_attribute* member_attribute = nullptr;
            bool is_value_pair;
            union
            {
                ast_type* member_type;
                ast_value* member_value_pair;
            };

            uint16_t member_offset;

            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // ast_defines::instance(dumm);
                // Write self copy functions here..

                if (is_value_pair)
                    WO_REINSTANCE(dumm->member_value_pair);
                else
                    WO_REINSTANCE(dumm->member_type);

                WO_REINSTANCE(dumm->member_attribute);

                return dumm;
            }
        };

        struct ast_value_make_struct_instance : virtual public ast_value
        {
            ast_list* struct_member_vals;
            ast_type* target_built_types;
            bool build_pure_struct;

            ast_value_make_struct_instance() : ast_value(new ast_type(WO_PSTR(pending))) {}

            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_value::instance(dumm);
                // Write self copy functions here..

                WO_REINSTANCE(dumm->struct_member_vals);
                WO_REINSTANCE(dumm->target_built_types);

                return dumm;
            }

            void update_constant_value(lexer*) override
            {
                // DO NOTHING
            }
        };

        struct ast_value_make_tuple_instance : virtual public ast_value
        {
            ast_list* tuple_member_vals;
            ast_value_make_tuple_instance() : ast_value(new ast_type(WO_PSTR(tuple)))
            {
            }
            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_value::instance(dumm);
                // Write self copy functions here..

                WO_REINSTANCE(dumm->tuple_member_vals);

                return dumm;
            }

            void update_constant_value(lexer*) override
            {
                // DO NOTHING
            }
        };

        struct ast_value_trib_expr : virtual public ast_value
        {
            ast_value* judge_expr;
            ast_value* val_if_true;
            ast_value* val_or;

            ast_value_trib_expr() : ast_value(new ast_type(WO_PSTR(pending))) {}

            ast::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_value::instance(dumm);
                // Write self copy functions here..

                WO_REINSTANCE(dumm->judge_expr);
                WO_REINSTANCE(dumm->val_if_true);
                WO_REINSTANCE(dumm->val_or);

                return dumm;
            }

            void update_constant_value(lexer* lex) override
            {
                judge_expr->eval_constant_value(lex);
                if (judge_expr->is_constant)
                {
                    if (judge_expr->get_constant_value().integer)
                    {
                        val_if_true->eval_constant_value(lex);
                        if (val_if_true->is_constant)
                        {
                            is_constant = true;
                            constant_value.set_val_compile_time(&val_if_true->get_constant_value());
                        }
                    }
                    else
                    {
                        val_or->eval_constant_value(lex);
                        if (val_or->is_constant)
                        {
                            is_constant = true;
                            constant_value.set_val_compile_time(&val_or->get_constant_value());
                        }
                    }
                }
            }
        };
        /////////////////////////////////////////////////////////////////////////////////

#define WO_NEED_TOKEN(ID) [&]()->const token& { \
    if (!input[(ID)].is_token())                \
        wo_error("Unexcepted token type.");     \
    return input[(ID)].read_token();            \
}()

#define WO_NEED_AST(ID) [&]() {                 \
    if (!input[(ID)].is_ast())                  \
        wo_error("Unexcepted ast-node type.");  \
    return input[(ID)].read_ast();              \
}()

#define WO_IS_TOKEN(ID) [&]() {                 \
    return input[(ID)].is_token();              \
}()
#define WO_IS_AST(ID) [&]() {                   \
    return input[(ID)].is_ast();                \
}()

        template <size_t pass_idx>
        struct pass_direct : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                wo_assert(input.size() > pass_idx);
                return input[pass_idx];
            }
        };

        struct pass_typeof : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                auto* att = new ast_type(WO_PSTR(pending));
                att->typefrom = dynamic_cast<ast_value*>(WO_NEED_AST(2));

                wo_assert(att->typefrom);

                return (ast_basic*)att;
            }

        };

        struct pass_build_mutable_type : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                auto* type = dynamic_cast<ast_type*>(WO_NEED_AST(1));

                wo_assert(WO_NEED_TOKEN(0).type == lex_type::l_mut || WO_NEED_TOKEN(0).type == lex_type::l_immut);

                if (WO_NEED_TOKEN(0).type == lex_type::l_mut)
                    type->set_is_mutable(true);
                else
                    type->set_is_force_immutable();

                return (ast_basic*)type;
            }
        };

        struct pass_template_reification : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                auto att = dynamic_cast<ast_value_variable*>(WO_NEED_AST(0));
                if (!ast_empty::is_empty(input[1]))
                {
                    auto tsalist = dynamic_cast<ast_list*>(WO_NEED_AST(1));
                    auto tsa = dynamic_cast<ast_type*>(tsalist->children);
                    while (tsa)
                    {
                        att->template_reification_args.push_back(tsa);
                        tsa = dynamic_cast<ast_type*>(tsa->sibling);
                    }
                }
                return (ast_basic*)att;
            }
        };

        struct pass_decl_attrib_check : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                auto* attr = dynamic_cast<ast_decl_attribute*>(WO_NEED_AST(0));
                attr->varify_attributes(&lex);

                return (ast_basic*)attr;
            }
        };

        struct pass_decl_attrib_begin : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                auto att = new ast_decl_attribute;
                if (ast_empty::is_empty(input[0]) == false)
                    att->add_attribute(&lex, dynamic_cast<ast_token*>(WO_NEED_AST(0))->tokens.type);

                return (ast_basic*)att;
            }
        };

        struct pass_enum_item_create : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                ast_enum_item* item = new ast_enum_item;
                item->enum_ident = wstring_pool::get_pstr(WO_NEED_TOKEN(0).identifier);
                if (input.size() == 3)
                {
                    item->need_assign_val = false;
                    item->enum_val = ast_value_literal::wstr_to_integer(WO_NEED_TOKEN(2).identifier);
                }
                else if (input.size() == 4)
                {
                    item->need_assign_val = false;
                    if (WO_NEED_TOKEN(2).type == lex_type::l_add)
                        item->enum_val = ast_value_literal::wstr_to_integer(WO_NEED_TOKEN(3).identifier);
                    else if (WO_NEED_TOKEN(2).type == lex_type::l_sub)
                        item->enum_val = -ast_value_literal::wstr_to_integer(WO_NEED_TOKEN(3).identifier);
                    else
                        wo_error("Enum item should be +/- integer");
                }
                return (ast_basic*)item;
            }
        };

        struct pass_enum_declear_begin : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                ast_enum_items_list* items = new ast_enum_items_list;
                auto* enum_item = dynamic_cast<ast_enum_item*>(WO_NEED_AST(0));
                items->enum_items.push_back(enum_item);
                if (enum_item->need_assign_val)
                {
                    enum_item->enum_val = items->next_enum_val;
                    enum_item->need_assign_val = false;
                }
                items->next_enum_val = enum_item->enum_val + 1;
                return (ast_basic*)items;
            }
        };

        struct pass_enum_declear_append : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                ast_enum_items_list* items = dynamic_cast<ast_enum_items_list*>(WO_NEED_AST(0));
                auto* enum_item = dynamic_cast<ast_enum_item*>(WO_NEED_AST(2));
                items->enum_items.push_back(enum_item);
                if (enum_item->need_assign_val)
                {
                    enum_item->enum_val = items->next_enum_val;
                    enum_item->need_assign_val = false;
                }
                items->next_enum_val = enum_item->enum_val + 1;
                return (ast_basic*)items;
            }
        };

        struct pass_mark_value_as_mut : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                // MAY_REF_FACTOR_TYPE_CASTING -> 4
                wo_assert(input.size() == 2);

                ast_value_mutable* result = new ast_value_mutable;
                result->val = dynamic_cast<ast_value*>(WO_NEED_AST(1));

                wo_assert(
                    WO_NEED_TOKEN(0).type == lex_type::l_mut ||
                    WO_NEED_TOKEN(0).type == lex_type::l_immut);

                result->mark_type = WO_NEED_TOKEN(0).type;
                return (ast_basic*)result;
            }
        };

        struct pass_enum_finalize : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                wo_assert(input.size() == 6);
                ast_decl_attribute* union_arttribute = dynamic_cast<ast_decl_attribute*>(WO_NEED_AST(0));

                ast_list* bind_type_and_decl_list = new ast_list;

                ast_namespace* enum_scope = new ast_namespace;
                enum_scope->copy_source_info(union_arttribute);
                ast_list* decl_list = new ast_list;

                // TODO: Enum attribute should be apply here!
                //       WO_NEED_AST(0)

                enum_scope->scope_name = wstring_pool::get_pstr(WO_NEED_TOKEN(2).identifier);

                auto* using_enum_as_int = new ast_using_type_as;
                using_enum_as_int->new_type_identifier = enum_scope->scope_name;
                using_enum_as_int->old_type = new ast_type(WO_PSTR(int));
                bind_type_and_decl_list->append_at_end(using_enum_as_int);
                using_enum_as_int->copy_source_info(union_arttribute);
                using_enum_as_int->old_type->copy_source_info(union_arttribute);

                enum_scope->in_scope_sentence = decl_list;
                ast_enum_items_list* enum_items = dynamic_cast<ast_enum_items_list*>(WO_NEED_AST(4));

                ast_varref_defines* vardefs = new ast_varref_defines;
                vardefs->copy_source_info(enum_items);
                vardefs->declear_attribute = union_arttribute;
                wo_assert(vardefs->declear_attribute);

                for (auto& enumitem : enum_items->enum_items)
                {
                    ast_value_literal* const_val = new ast_value_literal(
                        token{ lex_type::l_literal_integer, std::to_wstring(enumitem->enum_val) });
                    const_val->copy_source_info(enumitem);

                    ast_value_init* init_val_box = new ast_value_init();
                    init_val_box->init_value = const_val;
                    init_val_box->copy_source_info(enumitem);

                    auto* define_enum_item = new ast_pattern_identifier;
                    define_enum_item->identifier = enumitem->enum_ident;
                    define_enum_item->attr = new ast_decl_attribute();
                    define_enum_item->copy_source_info(enumitem);

                    vardefs->var_refs.push_back(
                        { define_enum_item, init_val_box });

                    // TODO: DATA TYPE SYSTEM..
                    const_val->value_type->set_type_with_name(enum_scope->scope_name);
                }

                decl_list->append_at_end(vardefs);
                bind_type_and_decl_list->append_at_end(enum_scope);
                return (ast_basic*)bind_type_and_decl_list;
            }
        };

        struct pass_append_attrib : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                auto att = dynamic_cast<ast_decl_attribute*>(WO_NEED_AST(0));
                att->add_attribute(&lex, dynamic_cast<ast_token*>(WO_NEED_AST(1))->tokens.type);
                return (ast_basic*)att;
            }
        };

        struct pass_unary_op : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                wo_assert(input.size() == 2);

                token _token = WO_NEED_TOKEN(0);
                ast_value* right_v = dynamic_cast<ast_value*>(WO_NEED_AST(1));

                wo_assert(right_v);
                wo_assert(lexer::lex_is_operate_type(_token.type) && (_token.type == lex_type::l_lnot || _token.type == lex_type::l_sub));

                ast_value_unary* vbin = new ast_value_unary();
                vbin->operate = _token.type;
                vbin->val = right_v;

                vbin->eval_constant_value(&lex);

                return (ast::ast_base*)vbin;
            }
        };

        struct pass_import_files : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input);
        };

        struct pass_mapping_pair : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                wo_assert(input.size() == 3);
                // [x] = x

                // Check 
                ast_value_array* key = dynamic_cast<ast_value_array*>(WO_NEED_AST(0));
                wo_assert(key);

                if (key->array_items->children == nullptr || key->array_items->children->sibling != nullptr)
                    return token{ lex.parser_error(lexer::errorlevel::error, WO_ERR_INVALID_KEY_EXPR) };

                ast_value* keyval = dynamic_cast<ast_value*>(key->array_items->children);
                wo_assert(keyval);

                return (ast::ast_base*)new ast_mapping_pair(
                    dynamic_cast<ast_value*>(keyval),
                    dynamic_cast<ast_value*>(WO_NEED_AST(2)));
            }
        };

        struct pass_unpack_args : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                wo_assert(input.size() == 2 || input.size() == 3);

                if (input.size() == 2)
                {
                    return (ast_basic*)new ast_fakevalue_unpacked_args(
                        dynamic_cast<ast_value*>(WO_NEED_AST(0)),
                        ast_fakevalue_unpacked_args::UNPACK_ALL_ARGUMENT);
                }
                else
                {
                    auto expand_count = ast_value_literal::wstr_to_integer(WO_NEED_TOKEN(2).identifier);

                    if (!expand_count)
                        lex.parser_error(lexer::errorlevel::error, WO_ERR_UNPACK_ARG_LESS_THEN_ONE);

                    return (ast_basic*)new ast_fakevalue_unpacked_args(
                        dynamic_cast<ast_value*>(WO_NEED_AST(0)),
                        (int32_t)expand_count);
                }
            }
        };

        struct pass_pack_variadic_args : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                return (ast_basic*)new ast_value_packed_variadic_args;
            }
        };
        struct pass_extern : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                ast_extern_info* extern_symb = new ast_extern_info;
                ast_list* extern_attribs = nullptr;
                if (input.size() == 5)
                {
                    extern_symb->library_name = std::nullopt;
                    extern_symb->symbol_name = WO_NEED_TOKEN(2).identifier;
                    extern_symb->externed_func = nullptr;

                    if (!ast_empty::is_empty(input[3]))
                    {
                        extern_attribs = dynamic_cast<ast_list*>(WO_NEED_AST(3));
                        wo_assert(extern_attribs != nullptr);
                    }
                }
                else
                {
                    wo_assert(input.size() == 7);

                    // extern ( lib , symb )
                    extern_symb->library_name = std::optional(WO_NEED_TOKEN(2).identifier);
                    extern_symb->symbol_name = WO_NEED_TOKEN(4).identifier;
                    extern_symb->externed_func = nullptr;

                    // Load it in pass
                    if (!ast_empty::is_empty(input[5]))
                    {
                        extern_attribs = dynamic_cast<ast_list*>(WO_NEED_AST(5));
                        wo_assert(extern_attribs != nullptr);
                    }
                }

                if (extern_attribs != nullptr)
                {
                    ast_token* attrib = dynamic_cast<ast_token*>(extern_attribs->children);
                    while (attrib)
                    {
                        wo_assert(attrib->tokens.type == lex_type::l_identifier);

                        if (attrib->tokens.identifier == L"slow")
                            extern_symb->is_slow_leaving_call = true;
                        else if (attrib->tokens.identifier == L"fast")
                            extern_symb->is_slow_leaving_call = false;
                        else if (attrib->tokens.identifier == L"repeat")
                            extern_symb->is_repeat_check_ignored = true;
                        else
                            lex.lang_error(lexer::errorlevel::error, attrib,
                                WO_ERR_UNKNOWN_EXTERN_ATTRIB, attrib->tokens.identifier.c_str());

                        attrib = dynamic_cast<ast_token*>(attrib->sibling);
                    }

                }

                return (ast_basic*)extern_symb;
            }
        };
        struct pass_while : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                wo_assert(input.size() == 5);
                return (ast::ast_base*)new ast_while(dynamic_cast<ast_value*>(WO_NEED_AST(2)), WO_NEED_AST(4));
            }
        };

        struct pass_if : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                wo_assert(input.size() == 6);
                if (ast_empty::is_empty(input[5]))
                    return (ast::ast_base*)new ast_if(dynamic_cast<ast_value*>(WO_NEED_AST(2)), WO_NEED_AST(4), nullptr);
                else
                    return (ast::ast_base*)new ast_if(dynamic_cast<ast_value*>(WO_NEED_AST(2)), WO_NEED_AST(4), WO_NEED_AST(5));
            }
        };

        template <size_t pass_idx>
        struct pass_sentence_block : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                wo_assert(input.size() > pass_idx);
                return (ast::ast_base*)ast_sentence_block::fast_parse_sentenceblock(WO_NEED_AST(pass_idx));
            }
        };

        struct pass_empty_sentence_block : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                return (ast::ast_base*)ast_sentence_block::fast_parse_sentenceblock(new ast_empty);
            }
        };

        struct pass_map_builder : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                if (input.size() == 3)
                    return (ast_basic*)new ast_value_mapping(dynamic_cast<ast_list*>(WO_NEED_AST(1)), false);
                else
                {
                    wo_assert(input.size() == 4);
                    return (ast_basic*)new ast_value_mapping(dynamic_cast<ast_list*>(WO_NEED_AST(1)), true);
                }
            }
        };

        struct pass_array_builder : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                if (input.size() == 3)
                    return (ast_basic*)new ast_value_array(dynamic_cast<ast_list*>(WO_NEED_AST(1)), false);
                else
                {
                    wo_assert(input.size() == 4);
                    return (ast_basic*)new ast_value_array(dynamic_cast<ast_list*>(WO_NEED_AST(1)), true);
                }
            }
        };

        struct pass_function_define : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input);
        };

        struct pass_function_call : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                auto* result = new ast_value_funccall;

                wo_assert(input.size() == 2);

                result->arguments = dynamic_cast<ast_list*>(WO_NEED_AST(1));

                if (ast_directed_values* adv = dynamic_cast<ast_directed_values*>(WO_NEED_AST(0)))
                {
                    result->called_func = adv->direct_val;
                    result->arguments->append_at_head(adv->from);
                    result->directed_value_from = adv->from;
                }
                else
                    result->called_func = dynamic_cast<ast_value*>(WO_NEED_AST(0));
                return (ast_basic*)result;
            }
        };

        struct pass_function_inv_call : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                wo_assert(input.size() == 3);

                auto* result = dynamic_cast<ast_value_funccall*>(WO_NEED_AST(0));

                auto* from = dynamic_cast<ast_value*>(WO_NEED_AST(2));

                if (result->directed_value_from == nullptr)
                {
                    result->directed_value_from = from;
                    result->arguments->append_at_head(from);
                }
                else
                {
                    // Insert `from` after first argument
                    result->arguments->remove_child(result->directed_value_from);
                    result->arguments->append_at_head(from);
                    result->arguments->append_at_head(result->directed_value_from);
                }

                return (ast_basic*)result;
            }
        };

        struct pass_function_inv_call2 : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                auto* result = new ast_value_funccall;

                wo_assert(input.size() == 3);

                result->arguments = new ast_list();
                result->called_func = dynamic_cast<ast_value*>(WO_NEED_AST(0));

                auto* from = dynamic_cast<ast_value*>(WO_NEED_AST(2));

                result->arguments->append_at_head(from);
                result->directed_value_from = from;

                return (ast_basic*)result;
            }
        };

        struct pass_directed_value_for_call : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                wo_assert(input.size() == 3);

                auto* result = new ast_directed_values();
                auto* from = dynamic_cast<ast_value*>(WO_NEED_AST(0));
                auto* to = dynamic_cast<ast_value*>(WO_NEED_AST(2));

                if (WO_NEED_TOKEN(1).type == lex_type::l_direct)
                {
                    result->from = from;
                    result->direct_val = to;
                }
                else
                {
                    wo_assert(WO_NEED_TOKEN(1).type == lex_type::l_inv_direct);
                    result->from = to;
                    result->direct_val = from;
                }

                return (ast_basic*)result;
            }
        };

        struct pass_literal : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                wo_assert(input.size() == 1);
                return (ast::ast_base*)new ast_value_literal(WO_NEED_TOKEN(0));
            }
        };

        struct pass_typeid : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                // typeid :< TYPE >
                wo_assert(input.size() == 4);
                auto* typeid_expr = new ast_value_typeid;
                typeid_expr->type = dynamic_cast<ast_type*>(WO_NEED_AST(2));
                wo_assert(typeid_expr->type != nullptr);

                return (ast::ast_base*)typeid_expr;
            }
        };

        struct pass_namespace : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input);
        };

        struct pass_begin_varref_define : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                wo_assert(input.size() == 4);
                ast_varref_defines* result = new ast_varref_defines;

                ast_value* init_val = dynamic_cast<ast_value*>(WO_NEED_AST(3));
                wo_assert(init_val);

                ast_value_init* init_val_box = new ast_value_init();
                init_val_box->init_value = init_val;
                init_val_box->copy_source_info(init_val);

                auto* define_varref = dynamic_cast<ast_pattern_base*>(WO_NEED_AST(0));
                wo_assert(define_varref);
                if (auto* pattern_identifier = dynamic_cast<ast_pattern_identifier*>(define_varref))
                {
                    auto* template_def_item = WO_NEED_AST(1)->children;
                    while (template_def_item)
                    {
                        auto* template_def = dynamic_cast<ast_template_define*>(template_def_item);
                        wo_assert(template_def);

                        pattern_identifier->template_arguments.push_back(template_def->template_ident);

                        template_def_item = template_def_item->sibling;
                    }
                }
                else if (!ast_empty::is_empty(input[1]))
                {
                    lex.lang_error(wo::lexer::errorlevel::error, WO_NEED_AST(1), WO_ERR_DECL_TEMPLATE_PATTERN_IS_NOT_ALLOWED);
                }

                result->var_refs.push_back({ define_varref, init_val_box });

                return (ast_basic*)result;
            }
        };
        struct pass_add_varref_define : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                wo_assert(input.size() == 6);
                ast_varref_defines* result = dynamic_cast<ast_varref_defines*>(WO_NEED_AST(0));

                ast_value* init_val = dynamic_cast<ast_value*>(WO_NEED_AST(5));
                wo_assert(result && init_val);

                ast_value_init* init_val_box = new ast_value_init();
                init_val_box->init_value = init_val;
                init_val_box->copy_source_info(init_val);

                auto* define_varref = dynamic_cast<ast_pattern_base*>(WO_NEED_AST(2));
                wo_assert(define_varref);
                if (auto* pattern_identifier = dynamic_cast<ast_pattern_identifier*>(define_varref))
                {
                    auto* template_def_item = WO_NEED_AST(3)->children;
                    while (template_def_item)
                    {
                        auto* template_def = dynamic_cast<ast_template_define*>(template_def_item);
                        wo_assert(template_def);

                        pattern_identifier->template_arguments.push_back(template_def->template_ident);

                        template_def_item = template_def_item->sibling;
                    }
                }
                else if (!ast_empty::is_empty(input[3]))
                {
                    lex.lang_error(wo::lexer::errorlevel::error, WO_NEED_AST(3), WO_ERR_DECL_TEMPLATE_PATTERN_IS_NOT_ALLOWED);
                }
                result->var_refs.push_back({ define_varref, init_val_box });

                return (ast_basic*)result;
            }
        };
        struct pass_mark_as_var_define : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                wo_assert(input.size() == 3);
                ast_varref_defines* result = dynamic_cast<ast_varref_defines*>(WO_NEED_AST(2));
                wo_assert(result);

                result->declear_attribute = dynamic_cast<ast_decl_attribute*>(WO_NEED_AST(0));
                wo_assert(result->declear_attribute);

                return (ast_basic*)result;
            }
        };
        struct pass_trans_where_decl_in_lambda : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                wo_assert(input.size() == 2);
                ast_varref_defines* result = dynamic_cast<ast_varref_defines*>(WO_NEED_AST(1));
                wo_assert(result);

                result->declear_attribute = new ast_decl_attribute();

                return (ast_basic*)result;
            }
        };

        /*struct pass_type_decl :public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                wo_assert(input.size() == 2);

                token tk = WO_NEED_TOKEN(1);

                if (tk.type == lex_type::l_identifier)
                {
                    return (ast::ast_base*)new ast_type(tk.identifier);
                }

                wo_error("Unexcepted token type.");
                return 0;
            }
        };*/

        struct pass_type_cast : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                wo_assert(input.size() == 2);

                ast_value* value_node;
                ast_type* type_node;
                if ((value_node = dynamic_cast<ast_value*>(WO_NEED_AST(0))) && (type_node = dynamic_cast<ast_type*>(WO_NEED_AST(1))))
                {
                    ast_value_type_cast* typecast = new ast_value_type_cast(value_node, type_node);
                    typecast->eval_constant_value(&lex);
                    return (ast_basic*)typecast;
                }

                wo_error("Unexcepted token type.");
                return 0;
            }
        };

        struct pass_type_judgement : public astnode_builder
        {
            static ast_value* do_judge(lexer& lex, ast_value* value_node, ast_type* type_node)
            {
                if (value_node->value_type->is_pending()
                    || value_node->value_type->is_dynamic()
                    || type_node->is_pending())
                {
                    return new ast_value_type_judge(value_node, type_node);
                }
                else if (!value_node->value_type->is_same(type_node, false))
                {
                    lex.parser_error(lexer::errorlevel::error, WO_ERR_CANNOT_AS_TYPE,
                        value_node->value_type->get_type_name().c_str(), type_node->get_type_name().c_str());
                    return value_node;
                }
                return value_node;
            }

            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                wo_assert(input.size() == 2);

                ast_value* value_node;
                ast_type* type_node;
                if ((value_node = dynamic_cast<ast_value*>(WO_NEED_AST(0))))
                {
                    if ((type_node = dynamic_cast<ast_type*>(WO_NEED_AST(1))))
                        return (ast_basic*)do_judge(lex, value_node, type_node);
                    return (ast_basic*)value_node;
                }

                wo_error("Unexcepted token type.");
                return 0;
            }
        };

        struct pass_type_check : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                wo_assert(input.size() == 2);

                ast_value* value_node;
                ast_type* type_node;
                if ((value_node = dynamic_cast<ast_value*>(WO_NEED_AST(0))))
                {
                    if ((type_node = dynamic_cast<ast_type*>(WO_NEED_AST(1))))
                    {
                        ast_value_type_check* checking_node = new ast_value_type_check(value_node, type_node);
                        checking_node->eval_constant_value(&lex);
                        return (ast_basic*)checking_node;
                    }
                    return (ast_basic*)value_node;
                }

                wo_error("Unexcepted token type.");
                return 0;
            }
        };

        struct pass_variable : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                wo_assert(input.size() == 1);

                token tk = WO_NEED_TOKEN(0);

                wo_assert(tk.type == lex_type::l_identifier);
                return (ast::ast_base*)new ast_value_variable(wstring_pool::get_pstr(tk.identifier));
            }
        };

        struct pass_append_serching_namespace : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                wo_assert(input.size() == 3);

                token tk = WO_NEED_TOKEN(1);
                ast_value_variable* result = dynamic_cast<ast_value_variable*>(WO_NEED_AST(2));

                wo_assert(tk.type == lex_type::l_identifier && result);

                result->scope_namespaces.insert(result->scope_namespaces.begin(), wstring_pool::get_pstr(tk.identifier));

                return (ast::ast_base*)result;
            }
        };

        struct pass_using_namespace : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                ast_using_namespace* aunames = new ast_using_namespace();
                auto vs = dynamic_cast<ast_value_variable*>(WO_NEED_AST(2));
                wo_assert(vs);

                aunames->from_global_namespace = vs->search_from_global_namespace;

                if (vs->var_name == WO_PSTR(unsafe))
                    return token{ lex.lang_error(lexer::errorlevel::error, vs, WO_ERR_USING_UNSAFE_NAMESPACE) };

                for (auto& space : vs->scope_namespaces)
                {
                    if (space == WO_PSTR(unsafe))
                        return token{ lex.lang_error(lexer::errorlevel::error, vs, WO_ERR_USING_UNSAFE_NAMESPACE) };
                    aunames->used_namespace_chain.push_back(space);
                }
                aunames->used_namespace_chain.push_back(vs->var_name);

                return (ast::ast_base*)aunames;
            }
        };

        struct pass_finalize_serching_namespace : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                wo_assert(input.size() == 2);

                if (WO_IS_TOKEN(0))
                {
                    token tk = WO_NEED_TOKEN(0);
                    ast_value_variable* result = dynamic_cast<ast_value_variable*>(WO_NEED_AST(1));

                    wo_assert((tk.type == lex_type::l_identifier || tk.type == lex_type::l_empty) && result);
                    if (tk.type == lex_type::l_identifier)
                    {
                        result->scope_namespaces.insert(result->scope_namespaces.begin(), wstring_pool::get_pstr(tk.identifier));
                    }
                    else
                    {
                        result->search_from_global_namespace = true;
                    }
                    return (ast::ast_base*)result;
                }
                else
                {
                    ast_type* findingfrom = dynamic_cast<ast_type*>(WO_NEED_AST(0));
                    ast_value_variable* result = dynamic_cast<ast_value_variable*>(WO_NEED_AST(1));

                    result->searching_from_type = findingfrom;
                    return (ast::ast_base*)result;
                }

            }
        };

        struct pass_variable_in_namespace : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                wo_assert(input.size() == 2);

                token tk = WO_NEED_TOKEN(1);

                wo_assert(tk.type == lex_type::l_identifier);

                return (ast::ast_base*)new ast_value_variable(wstring_pool::get_pstr(tk.identifier));
            }
        };

        template <size_t first_node>
        struct pass_create_list : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                wo_assert(first_node < input.size());

                ast_list* result = new ast_list();
                if (ast_empty::is_empty(input[first_node]))
                    return (ast::ast_base*)result;

                ast_basic* _node = WO_NEED_AST(first_node);

                result->append_at_end(_node);
                return (ast::ast_base*)result;
            }
        };

        template <size_t from, size_t to_list>
        struct pass_append_list : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                wo_assert(input.size() > std::max(from, to_list));

                ast_list* list = dynamic_cast<ast_list*>(WO_NEED_AST(to_list));
                if (list)
                {
                    if (ast_empty::is_empty(input[from]))
                        return (ast::ast_base*)list;

                    if (from < to_list)
                    {
                        list->append_at_head(WO_NEED_AST(from));
                    }
                    else if (from > to_list)
                    {
                        list->append_at_end(WO_NEED_AST(from));
                    }
                    else
                    {
                        wo_error("You cannot add list to itself.");
                    }
                    return (ast::ast_base*)list;
                }
                wo_error("Unexcepted token type, should be 'ast_list' or inherit from 'ast_list'.");
                return 0;
            }
        };

        struct pass_make_tuple : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                wo_assert(input.size() == 1);
                ast_value_make_tuple_instance* tuple = new ast_value_make_tuple_instance;

                if (!ast_empty::is_empty(input[0]))
                {
                    tuple->tuple_member_vals = dynamic_cast<ast_list*>(WO_NEED_AST(0));

                    auto* elem = tuple->tuple_member_vals->children;
                    while (elem != nullptr)
                    {
                        tuple->value_type->template_arguments.push_back(new ast_type(WO_PSTR(pending)));
                        elem = elem->sibling;
                    }
                }
                else
                    tuple->tuple_member_vals = new ast_list();
                return (ast::ast_base*)tuple;
            }

        };

        struct pass_empty : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                return (ast::ast_base*)new ast_empty();
            }
        };

        struct pass_binary_op : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                wo_assert(input.size() >= 3);

                ast_value* left_v = dynamic_cast<ast_value*>(WO_NEED_AST(0));
                ast_value* right_v = dynamic_cast<ast_value*>(WO_NEED_AST(2));
                wo_assert(left_v && right_v);

                token _token = WO_NEED_TOKEN(1);
                wo_assert(lexer::lex_is_operate_type(_token.type));

                // calc type upgrade


                ast_value_binary* vbin = new ast_value_binary();
                vbin->left = left_v;
                vbin->operate = _token.type;
                vbin->right = right_v;

                vbin->eval_constant_value(&lex);
                // In ast build pass, all left value's type cannot judge, so it was useless..
                //vbin->value_type = result_type;

                return (ast::ast_base*)vbin;
            }
        };

        struct pass_assign_op : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                wo_assert(input.size() == 3);

                ast_value* left_v = dynamic_cast<ast_value*>(WO_NEED_AST(0));
                ast_value* right_v = dynamic_cast<ast_value*>(WO_NEED_AST(2));
                wo_assert(left_v && right_v);

                token _token = WO_NEED_TOKEN(1);
                wo_assert(lexer::lex_is_operate_type(_token.type));

                if (left_v->is_constant)
                    return token{ lex.parser_error(lexer::errorlevel::error, WO_ERR_CANNOT_ASSIGN_TO_CONSTANT) };

                ast_value_assign* vbin = new ast_value_assign();
                vbin->left = left_v;
                vbin->operate = _token.type;
                vbin->right = right_v;

                if (_token.type == lex_type::l_value_assign
                    || _token.type == lex_type::l_value_add_assign
                    || _token.type == lex_type::l_value_sub_assign
                    || _token.type == lex_type::l_value_mul_assign
                    || _token.type == lex_type::l_value_div_assign
                    || _token.type == lex_type::l_value_mod_assign)
                {
                    vbin->is_value_assgin = true;
                    vbin->value_type = new ast_type(WO_PSTR(pending));
                }
                else
                    vbin->is_value_assgin = false;

                return (ast::ast_base*)vbin;
            }
        };

        struct pass_binary_logical_op : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                // TODO Do optmize, like pass_binary_op

                wo_assert(input.size() >= 3);

                ast_value* left_v = dynamic_cast<ast_value*>(WO_NEED_AST(0));
                ast_value* right_v = dynamic_cast<ast_value*>(WO_NEED_AST(2));
                wo_assert(left_v && right_v);

                token _token = WO_NEED_TOKEN(1);
                wo_assert(lexer::lex_is_operate_type(_token.type));

                ast_value_logical_binary* vbin = new ast_value_logical_binary();
                vbin->left = left_v;
                vbin->operate = _token.type;
                vbin->right = right_v;

                return (ast::ast_base*)vbin;
            }
        };

        struct pass_index_op : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                wo_assert(input.size() >= 3);

                ast_value* left_v = dynamic_cast<ast_value*>(WO_NEED_AST(0));
                token _token = WO_NEED_TOKEN(1);

                if (_token.type == lex_type::l_index_begin)
                {
                    ast_value* right_v = dynamic_cast<ast_value*>(WO_NEED_AST(2));
                    wo_assert(left_v && right_v);

                    if (dynamic_cast<ast_value_packed_variadic_args*>(left_v))
                    {
                        return (ast::ast_base*)new ast_value_indexed_variadic_args(right_v);
                    }
                    else
                    {
                        ast_value_index* vbin = new ast_value_index();
                        vbin->from = left_v;
                        vbin->index = right_v;

                        vbin->eval_constant_value(&lex);

                        return (ast::ast_base*)vbin;
                    }
                }
                else if (_token.type == lex_type::l_index_point)
                {
                    ast_token* right_tk = dynamic_cast<ast_token*>(WO_NEED_AST(2));
                    wo_assert(right_tk != nullptr && left_v &&
                        (right_tk->tokens.type == lex_type::l_identifier ||
                            right_tk->tokens.type == lex_type::l_literal_integer));

                    ast_value_literal* const_result = new ast_value_literal(right_tk->tokens);
                    const_result->copy_source_info(right_tk);

                    ast_value_index* vbin = new ast_value_index();
                    vbin->from = left_v;
                    vbin->index = const_result;

                    vbin->eval_constant_value(&lex);

                    return (ast::ast_base*)vbin;
                }

                wo_error("Unexcepted token type.");
                return token{ lex.parser_error(lexer::errorlevel::error, L"Unexcepted token type.") };
            }
        };

        struct pass_build_function_type : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                wo_assert(input.size() == 3);

                ast_type* result = nullptr;
                auto* function_ret_type = dynamic_cast<ast_type*>(WO_NEED_AST(2));

                wo_assert(function_ret_type != nullptr);

                result = new ast_type(WO_PSTR(pending));
                result->set_ret_type(function_ret_type);

                auto* arg_list = dynamic_cast<ast_list*>(WO_NEED_AST(0));
                auto* child = arg_list->children;
                while (child)
                {
                    if (auto* type = dynamic_cast<ast_type*>(child))
                    {
                        result->append_function_argument_type(type);
                    }
                    else
                    {
                        auto* tktype = dynamic_cast<ast_token*>(child);
                        wo_assert(child->sibling == nullptr && tktype && tktype->tokens.type == lex_type::l_variadic_sign);
                        //must be last elem..

                        result->set_as_variadic_arg_func();
                    }

                    child = child->sibling;
                }

                return (ast_basic*)result;

            }
        };
        struct pass_build_type_may_template : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                ast_type* result = nullptr;

                auto* scoping_type = dynamic_cast<ast_value_variable*>(WO_NEED_AST(0));
                wo_assert(scoping_type);
                result = new ast_type(scoping_type->var_name);
                result->search_from_global_namespace = scoping_type->search_from_global_namespace;
                result->scope_namespaces = scoping_type->scope_namespaces;
                result->searching_from_type = scoping_type->searching_from_type;
                if (result->search_from_global_namespace || !result->scope_namespaces.empty())
                    result->is_non_update_custom_type = true;

                if (input.size() == 1 || ast_empty::is_empty(input[1]))
                {
                    return (ast_basic*)result;
                }
                else
                {
                    ast_list* template_arg_list = dynamic_cast<ast_list*>(WO_NEED_AST(1));

                    std::vector<ast_type*> template_args;
                    wo_assert(template_arg_list);
                    ast_type* type = dynamic_cast<ast_type*>(template_arg_list->children);
                    while (type)
                    {
                        template_args.push_back(type);
                        type = dynamic_cast<ast_type*>(type->sibling);
                    }
                    result->template_arguments = template_args;
                    if (result->is_array() || result->is_vec())
                        if (result->template_arguments.size() != 1)
                            lex.parser_error(lexer::errorlevel::error, WO_ERR_TYPE_NEED_N_TEMPLATE_ARG,
                                result->type_name->c_str(), 1);
                    if (result->is_dict() || result->is_map())
                        if (result->template_arguments.size() != 2)
                            lex.parser_error(lexer::errorlevel::error, WO_ERR_TYPE_NEED_N_TEMPLATE_ARG,
                                result->type_name->c_str(), 2);

                    return (ast_basic*)result;
                }
            }
        };

        struct pass_build_nil_type : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                ast_type* result = new ast_type(WO_PSTR(nil));
                return (ast_basic*)result;
            }
        };

        struct pass_using_type_as : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input);
        };

        struct pass_token : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                return (ast::ast_base*)new ast_token(WO_NEED_TOKEN(0));
            }
        };

        struct pass_return : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                ast_return* result = new ast_return();
                if (input.size() == 3)
                {
                    if (!ast_empty::is_empty(input[1]))
                        result->return_value = dynamic_cast<ast_value*>(WO_NEED_AST(1));
                }
                else if (input.size() == 1)
                {
                    result->return_value = dynamic_cast<ast_value*>(WO_NEED_AST(0));
                }
                else
                    wo_error("Unexpected return format.");

                return (ast::ast_base*)result;
            }
        };

        struct pass_func_argument : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                ast_value_arg_define* arg_def = new ast_value_arg_define;
                arg_def->declear_attribute = dynamic_cast<ast_decl_attribute*>(WO_NEED_AST(0));
                wo_assert(arg_def->declear_attribute);

                if (input.size() == 4)
                {
                    wo_assert(WO_NEED_TOKEN(1).type == lex_type::l_mut);
                    arg_def->decl = identifier_decl::MUTABLE;

                    arg_def->arg_name = wstring_pool::get_pstr(WO_NEED_TOKEN(2).identifier);
                    if (ast_empty::is_empty(input[3]))
                        arg_def->value_type->set_type_with_name(WO_PSTR(auto));
                    else
                        arg_def->value_type = dynamic_cast<ast_type*>(WO_NEED_AST(3));
                }
                else
                {
                    wo_assert(input.size() == 3);

                    arg_def->arg_name = wstring_pool::get_pstr(WO_NEED_TOKEN(1).identifier);
                    if (ast_empty::is_empty(input[2]))
                        arg_def->value_type->set_type_with_name(WO_PSTR(auto));
                    else
                        arg_def->value_type = dynamic_cast<ast_type*>(WO_NEED_AST(2));
                }

                return (ast::ast_base*)arg_def;
            }
        };

        struct pass_foreach : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input);
        };

        struct pass_forloop : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                // ast_forloop
                // 1. for ( VARREF_DEFINE EXECUTE ; EXECUTE ) SENTENCES
                //     0  1        2        3     4    5    6     7 
                // 2. for ( EXECUTE ; EXECUTE ; EXECUTE ) SENTENCES
                //     0  1    2    3    4    5    6    7    8

                ast_forloop* result = new ast_forloop;
                if (input.size() == 8)
                {
                    result->pre_execute = WO_NEED_AST(2);
                    result->judgement_expr = dynamic_cast<ast_value*>(WO_NEED_AST(3));
                    result->after_execute = dynamic_cast<ast_value*>(WO_NEED_AST(5));
                    result->execute_sentences = WO_NEED_AST(7);
                }
                else
                {
                    if (ast_empty::is_empty(input[2]))
                        result->pre_execute = nullptr;
                    else
                        result->pre_execute = WO_NEED_AST(2);
                    result->judgement_expr = dynamic_cast<ast_value*>(WO_NEED_AST(4));
                    result->after_execute = dynamic_cast<ast_value*>(WO_NEED_AST(6));
                    result->execute_sentences = WO_NEED_AST(8);
                }

                return (ast_basic*)result;
            }

        };

        struct pass_mark_label : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                auto* result = WO_NEED_AST(2);
                result->marking_label = wo::wstring_pool::get_pstr(WO_NEED_TOKEN(0).identifier);
                return (ast_basic*)result;
            }

        };

        struct pass_break : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                if (input.size() == 2)
                    return (ast_basic*)new ast_break;
                auto result = new ast_break;
                result->label = wo::wstring_pool::get_pstr(WO_NEED_TOKEN(1).identifier);
                return (ast_basic*)result;
            }
        };
        struct pass_continue : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                if (input.size() == 2)
                    return (ast_basic*)new ast_continue;
                auto result = new ast_continue;
                result->label = wo::wstring_pool::get_pstr(WO_NEED_TOKEN(1).identifier);
                return (ast_basic*)result;
            }
        };
        struct pass_template_decl : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                ast_template_define* atn = new ast_template_define;
                atn->template_ident = wstring_pool::get_pstr(WO_NEED_TOKEN(0).identifier);
                return (ast_basic*)atn;
            }
        };

        struct pass_format_string : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input);
        };

        struct pass_finish_format_string : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                wo_assert(input.size() == 3);

                auto* begin = new ast_value_literal(WO_NEED_TOKEN(0));
                auto* middle = dynamic_cast<ast_value*>(WO_NEED_AST(1));
                auto* end = new ast_value_literal(WO_NEED_TOKEN(2));

                begin->copy_source_info(middle);
                end->copy_source_info(middle);

                ast_value_binary* lmbin = new ast_value_binary();
                lmbin->left = begin;
                lmbin->operate = lex_type::l_add;
                lmbin->right = middle;

                lmbin->eval_constant_value(&lex);
                lmbin->copy_source_info(middle);

                ast_value_binary* lmebin = new ast_value_binary();
                lmebin->left = lmbin;
                lmebin->operate = lex_type::l_add;
                lmebin->right = end;

                lmebin->eval_constant_value(&lex);
                lmebin->copy_source_info(middle);

                return (ast_basic*)lmebin;
            }
        };

        struct pass_union_item : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                ast_union_item* result = new ast_union_item;
                if (input.size() == 2)
                {
                    // identifier : type
                    // => const var identifier<A...> = func(){...}();
                    result->identifier = wstring_pool::get_pstr(WO_NEED_TOKEN(0).identifier);
                    result->gadt_out_type_may_nil = dynamic_cast<ast_type*>(WO_NEED_AST(1));
                }
                else
                {
                    // identifier ( TYPE ) : type
                    // => func identifier<A...>(var v: TYPE){...}
                    wo_assert(input.size() == 5);
                    result->identifier = wstring_pool::get_pstr(WO_NEED_TOKEN(0).identifier);
                    result->type_may_nil = dynamic_cast<ast_type*>(WO_NEED_AST(2));
                    result->gadt_out_type_may_nil = dynamic_cast<ast_type*>(WO_NEED_AST(4));
                    wo_assert(result->type_may_nil);
                }
                return (ast_basic*)result;
            }
        };
        struct pass_trib_expr : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                ast_value_trib_expr* expr = new ast_value_trib_expr;

                wo_assert(input.size() == 5);
                expr->judge_expr = dynamic_cast<ast_value*>(WO_NEED_AST(0));
                expr->val_if_true = dynamic_cast<ast_value*>(WO_NEED_AST(2));
                expr->val_or = dynamic_cast<ast_value*>(WO_NEED_AST(4));

                return (ast_basic*)expr;
            }

        };
        struct pass_union_define : public astnode_builder
        {
            static void find_used_template(
                ast_type* type_decl,
                const std::vector<wo_pstring_t>& template_defines,
                std::set<wo_pstring_t>& out_used_type);
            static grammar::produce build(lexer& lex, inputs_t& input);
        };

        struct pass_identifier_pattern : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                ast_pattern_base* result = nullptr;
                if (input.size() == 3)
                {
                    if (WO_NEED_TOKEN(2).identifier == L"_")
                        result = new ast_pattern_takeplace();
                    else
                    {
                        auto* result_identifier = new ast_pattern_identifier;

                        wo_assert(WO_NEED_TOKEN(0).type == lex_type::l_mut);
                        result_identifier->decl = identifier_decl::MUTABLE;

                        result_identifier->attr = dynamic_cast<ast_decl_attribute*>(WO_NEED_AST(1));
                        result_identifier->identifier = wstring_pool::get_pstr(WO_NEED_TOKEN(2).identifier);
                        result = result_identifier;
                    }
                }
                else
                {
                    if (WO_NEED_TOKEN(1).identifier == L"_")
                        result = new ast_pattern_takeplace();
                    else
                    {
                        auto* result_identifier = new ast_pattern_identifier;

                        result_identifier->attr = dynamic_cast<ast_decl_attribute*>(WO_NEED_AST(0));
                        result_identifier->identifier = wstring_pool::get_pstr(WO_NEED_TOKEN(1).identifier);
                        result = result_identifier;
                    }
                }

                wo_assert(result != nullptr);
                return (ast_basic*)result;
            }
        };

        struct pass_tuple_pattern : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                auto* result = new ast_pattern_tuple;
                wo_assert(input.size() == 3);

                if (!ast_empty::is_empty(input[1]))
                {
                    auto* subpattern = WO_NEED_AST(1)->children;
                    while (subpattern)
                    {
                        auto* child_pattern = dynamic_cast<ast_pattern_base*>(subpattern);
                        subpattern = subpattern->sibling;

                        wo_assert(child_pattern);
                        result->tuple_patterns.push_back(child_pattern);

                        ast_value_takeplace* val_take_place = new ast_value_takeplace;
                        val_take_place->copy_source_info(child_pattern);

                        result->tuple_takeplaces.push_back(val_take_place);
                    }
                }
                return (ast_basic*)result;
            }
        };

        struct pass_union_pattern : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                // 1. CALLABLE_LEFT
                // 2. CALLABLE_LEFT ( PATTERN )
                auto* result = new ast_pattern_union_value;

                wo_assert(input.size() == 1 || input.size() == 4);

                result->union_expr = dynamic_cast<ast_value_variable*>(WO_NEED_AST(0));
                if (result->union_expr->var_name == WO_PSTR(_)
                    && result->union_expr->scope_namespaces.empty()
                    && !result->union_expr->search_from_global_namespace)
                    result->union_expr = nullptr;

                if (input.size() == 4)
                {
                    result->pattern_arg_in_union_may_nil
                        = dynamic_cast<ast_pattern_base*>(WO_NEED_AST(2));

                    if (result->union_expr == nullptr)
                        lex.lang_error(lexer::errorlevel::error, result->pattern_arg_in_union_may_nil,
                            WO_ERR_NO_AGR_FOR_DEFAULT_PATTERN);
                }
                return (ast_basic*)result;
            }
        };

        struct pass_match_case_for_union : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                // pattern_case? {sentence in list}
                wo_assert(input.size() == 3);

                auto* result = new ast_match_union_case;
                result->take_place_value_may_nil = nullptr;
                result->union_pattern = dynamic_cast<ast_pattern_union_value*>(WO_NEED_AST(0));
                wo_assert(result->union_pattern);

                auto* scope = WO_NEED_AST(2);
                result->in_case_sentence = dynamic_cast<ast_sentence_block*>(scope);
                wo_assert(result->in_case_sentence);

                return (ast_basic*)result;
            }
        };

        struct pass_match : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                // match ( value ){ case... }
                wo_assert(input.size() == 7);

                auto* result = new ast_match;
                result->match_value = dynamic_cast<ast_value*>(WO_NEED_AST(2));
                result->cases = dynamic_cast<ast_list*>(WO_NEED_AST(5));
                return (ast_basic*)result;
            }
        };

        struct pass_struct_member_def : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                auto* result = new ast_struct_member_define;
                result->is_value_pair = false;

                result->member_attribute = new ast_decl_attribute;

                if (input.size() == 3)
                {
                    // ACCESS_MODIFIER identifier TYPE_DECLEAR
                    result->member_attribute->add_attribute(&lex, dynamic_cast<ast_token*>(WO_NEED_AST(0))->tokens.type);
                    result->member_name = wstring_pool::get_pstr(WO_NEED_TOKEN(1).identifier);
                    result->member_type = dynamic_cast<ast_type*>(WO_NEED_AST(2));
                }
                else
                {
                    wo_assert(input.size() == 2);
                    result->member_name = wstring_pool::get_pstr(WO_NEED_TOKEN(0).identifier);
                    result->member_type = dynamic_cast<ast_type*>(WO_NEED_AST(1));
                }
                wo_assert(result->member_type != nullptr);

                return (ast_basic*)result;
            }
        };

        struct pass_struct_member_init_pair : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                auto* result = new ast_struct_member_define;

                wo_assert(input.size() == 3);
                // identifier = VALUE
                result->member_name = wstring_pool::get_pstr(WO_NEED_TOKEN(0).identifier);
                result->member_value_pair = dynamic_cast<ast_value*>(WO_NEED_AST(2));
                result->is_value_pair = true;

                wo_assert(result->member_value_pair != nullptr);

                return (ast_basic*)result;
            }
        };

        struct pass_struct_type_define : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                wo_assert(input.size() == 4);
                // struct{ members }
                //   0   1     2   3
                ast_type* struct_type = new ast_type(WO_PSTR(struct));
                uint16_t membid = 0;

                // Duplicated member name check
                std::unordered_set<wo_pstring_t> member_names;

                auto* members = WO_NEED_AST(2)->children;
                while (members)
                {
                    auto* member_pair = dynamic_cast<ast_struct_member_define*>(members);
                    wo_assert(member_pair);

                    if (member_names.insert(member_pair->member_name).second == false)
                        lex.lang_error(wo::lexer::errorlevel::error, member_pair,
                            WO_ERR_REPEAT_MEMBER_NAME,
                            member_pair->member_name->c_str());

                    struct_type->struct_member_index[member_pair->member_name].member_decl_attribute
                        = member_pair->member_attribute;
                    struct_type->struct_member_index[member_pair->member_name].member_type
                        = member_pair->member_type;
                    struct_type->struct_member_index[member_pair->member_name].offset
                        = membid++;

                    wo_assert(struct_type->struct_member_index[member_pair->member_name].member_type);

                    members = members->sibling;
                }

                return (ast_basic*)struct_type;
            }
        };

        struct pass_make_struct_instance : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                // STRUCT_TYPE { ITEMS }

                wo_assert(input.size() == 4);
                ast_value_make_struct_instance* value = new ast_value_make_struct_instance();

                value->struct_member_vals = dynamic_cast<ast_list*>(WO_NEED_AST(2));

                // Duplicated member name check
                std::unordered_set<wo_pstring_t> member_names;
                auto* member_iter = dynamic_cast<ast_struct_member_define*>(value->struct_member_vals->children);
                while (member_iter)
                {
                    if (member_names.insert(member_iter->member_name).second == false)
                        lex.lang_error(wo::lexer::errorlevel::error, member_iter,
                            WO_ERR_REPEAT_MEMBER_NAME,
                            member_iter->member_name->c_str());

                    member_iter = dynamic_cast<ast_struct_member_define*>(member_iter->sibling);
                }

                if (WO_IS_TOKEN(0))
                {
                    value->target_built_types = new ast_type(WO_PSTR(struct));
                    uint16_t member_idx = 0;
                    auto* member_iter = dynamic_cast<ast_struct_member_define*>(value->struct_member_vals->children);
                    while (member_iter)
                    {
                        auto& member = value->target_built_types->struct_member_index[member_iter->member_name];

                        // Anonymous structure's member donot contain attribute.
                        wo_assert(member_iter->member_attribute == nullptr);
                        member.member_decl_attribute = member_iter->member_attribute;
                        member.member_type = new ast_type(WO_PSTR(pending));
                        member.offset = member_idx++;

                        member_iter = dynamic_cast<ast_struct_member_define*>(member_iter->sibling);
                    }
                    value->build_pure_struct = true;
                }
                else
                {
                    value->target_built_types = dynamic_cast<ast_type*>(WO_NEED_AST(0));
                    value->build_pure_struct = false;
                }

                wo_assert(value->value_type);
                wo_assert(value->target_built_types);

                wo_assert(value->struct_member_vals);

                return (ast_basic*)value;
            }
        };

        struct pass_build_tuple_type : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                // ( LIST )

                wo_assert(input.size() == 1);
                ast_type* tuple_type = new ast_type(WO_PSTR(tuple));

                if (!ast_empty::is_empty(input[0]))
                {
                    auto* type_ptr = WO_NEED_AST(0)->children;
                    while (type_ptr)
                    {
                        ast_type* type = dynamic_cast<ast_type*>(type_ptr);
                        if (type)
                        {
                            type_ptr = type_ptr->sibling;
                            tuple_type->template_arguments.push_back(type);
                        }
                        else
                        {
                            lex.parser_error(lexer::errorlevel::error, WO_ERR_FAILED_TO_CREATE_TUPLE_WITH_VAARG);
                            break;
                        }

                    }
                }
                return (ast_basic*)tuple_type;
            }
        };

        struct pass_tuple_types_list : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                //( LIST , ...)

                if (input.size() == 3)
                    return WO_NEED_AST(1);

                wo_assert(input.size() == 5);
                if (ast_empty::is_empty(input[3]))
                    return WO_NEED_AST(1);

                dynamic_cast<ast_list*>(WO_NEED_AST(1))->append_at_end(WO_NEED_AST(3));

                return WO_NEED_AST(1);
            }
        };

        struct pass_build_where_constraint : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                // where xxxx.... ,
                wo_assert(input.size() == 3);

                ast_where_constraint* result = new ast_where_constraint;
                result->where_constraint_list = dynamic_cast<ast_list*>(WO_NEED_AST(1));
                wo_assert(result->where_constraint_list);

                return (ast_basic*)result;
            }
        };

        struct pass_build_bind_map_monad : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                // a >> \n = [x * 2];
                // b ]] \n = x * 2;
                wo_assert(input.size() == 3);

                ast_value_funccall* result = new ast_value_funccall();
                result->arguments = new ast_list();
                result->directed_value_from = dynamic_cast<ast_value*>(WO_NEED_AST(0));
                result->arguments->append_at_head(result->directed_value_from);

                auto* bind_map_func = dynamic_cast<ast_value*>(WO_NEED_AST(2));
                wo_assert(bind_map_func != nullptr);
                result->arguments->append_at_end(bind_map_func);

                if (WO_NEED_TOKEN(1).type == lex_type::l_bind_monad)
                    result->called_func = new ast_value_variable(WO_PSTR(bind));
                else
                {
                    wo_assert(WO_NEED_TOKEN(1).type == lex_type::l_map_monad);
                    result->called_func = new ast_value_variable(WO_PSTR(map));
                }
                result->called_func->copy_source_info(result->directed_value_from);

                return (ast_basic*)result;
            }
        };

        struct pass_macro_failed : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                wo_assert(WO_NEED_TOKEN(0).type == lex_type::l_macro);
                return token{ lex.parser_error(lexer::errorlevel::error, WO_ERR_UNKNOWN_MACRO_NAMED, WO_NEED_TOKEN(0).identifier.c_str()) };
            }
        };

        struct pass_do_expr_as_sentence : public astnode_builder
        {
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                wo_assert(WO_NEED_TOKEN(0).type == lex_type::l_do);

                ast_value_type_cast* result = new ast_value_type_cast(dynamic_cast<ast_value*>(WO_NEED_AST(1)), new ast_type(WO_PSTR(void)));
                return (ast_basic*)result;
            }
        };

        /////////////////////////////////////////////////////////////////////////////////
#if 1
        void init_builder();
    }

    grammar::rule operator>>(grammar::rule ost, size_t builder_index);
#endif
#define WO_ASTBUILDER_INDEX(...) ast::index<__VA_ARGS__>()
}
