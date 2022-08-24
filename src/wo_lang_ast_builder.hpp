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

#include <any>
#include <type_traits>
#include <cmath>
#include <unordered_map>
#include <algorithm>

namespace wo
{
#define WO_REINSTANCE(ITEM) if(ITEM){(ITEM) = dynamic_cast<meta::origin_type<decltype(ITEM)>>((ITEM)->instance());}
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
            using ast_basic = wo::grammar::ast_base;
            using inputs_t = std::vector<std::any>;
            using builder_func_t = std::function<std::any(lexer&, const std::wstring&, inputs_t&)>;

            virtual ~astnode_builder() = default;
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                wo_test(false, "");
                return nullptr;
            }
        };

        inline static std::unordered_map<size_t, size_t> _registed_builder_function_id_list;
        inline static std::vector<astnode_builder::builder_func_t> _registed_builder_function;

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
            wo_test(idx != 0);

            return idx;
        }

        inline astnode_builder::builder_func_t get_builder(size_t idx)
        {
            wo_test(idx != 0);

            return _registed_builder_function[idx - 1];
        }
#endif
        /////////////////////////////////////////////////////////////////////////////////

        enum class identifier_decl
        {
            IMMUTABLE,
            MUTABLE,
            REF
        };

        struct ast_value;
        ast_value* dude_dump_ast_value(ast_value*);
        struct ast_type;
        ast_type* dude_dump_ast_type(ast_type*);

        struct ast_symbolable_base : virtual grammar::ast_base
        {
            std::vector<std::wstring> scope_namespaces;
            bool search_from_global_namespace = false;

            ast_type* searching_from_type = nullptr;

            lang_symbol* symbol = nullptr;
            lang_scope* searching_begin_namespace_in_pass2 = nullptr;

            std::string get_namespace_chain() const
            {
                return get_belong_namespace_path_with_lang_scope(searching_begin_namespace_in_pass2);
            }
            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // ast_base::instance(dumm);

                // Write self copy functions here..
                dumm->searching_from_type = dude_dump_ast_type(searching_from_type);
                dumm->symbol = nullptr;
                dumm->searching_begin_namespace_in_pass2 = nullptr;

                return dumm;
            }
        };

        struct ast_type : virtual public ast_symbolable_base
        {
            bool is_function_type = false;
            bool is_variadic_function_type = false;

            // if this type is function, following type information will describe the return type;
            std::wstring type_name;
            ast_type* complex_type = nullptr;

            value::valuetype value_type;
            bool is_pending_type = false;

            std::vector<ast_type*> argument_types;
            std::vector<ast_type*> template_arguments;

            std::vector<ast_type*> template_impl_naming_checking;

            struct struct_offset
            {
                ast_value* init_value_may_nil = nullptr;
                uint16_t offset = (uint16_t)0xFFFF;
                std::vector<size_t> union_used_template_index;
            };
            std::map<std::wstring, struct_offset> struct_member_index;
            ast_type* using_type_name = nullptr;
            ast_value* typefrom = nullptr;

            inline static const std::map<std::wstring, value::valuetype> name_type_pair =
            {
                {L"int", value::valuetype::integer_type},
                {L"handle", value::valuetype::handle_type},
                {L"real", value::valuetype::real_type},
                {L"string", value::valuetype::string_type},
                {L"map", value::valuetype::mapping_type},
                {L"array", value::valuetype::array_type},
                {L"gchandle", value::valuetype::gchandle_type},
                {L"nil", value::valuetype::invalid},

                // special type
                {L"union", value::valuetype::invalid},
                {L"struct", value::valuetype::invalid},
                {L"tuple", value::valuetype::invalid},

                {L"void", value::valuetype::invalid},
                {L"anything", value::valuetype::invalid}, // Top type.
                {L"pending", value::valuetype::invalid},
                {L"dynamic", value::valuetype::invalid},
            };

            static std::wstring get_name_from_type(value::valuetype _type)
            {
                if (_type == value::valuetype::invalid)
                    return L"nil";

                for (auto& [tname, vtype] : name_type_pair)
                {
                    if (vtype == _type)
                    {
                        return tname;
                    }
                }

                return L"pending";
            }

            static value::valuetype get_type_from_name(const std::wstring& name)
            {
                if (name_type_pair.find(name) != name_type_pair.end())
                    return name_type_pair.at(name);
                return value::valuetype::invalid;
            }

            static bool check_castable(ast_type* to, ast_type* from)
            {
                // MUST BE FORCE CAST HERE!

                if (to->is_pending() || (to->is_void() && !to->is_func()))
                    return false;

                if (to->is_bool())
                    return true;

                if (to->is_dynamic())
                    return true;

                if (from->is_dynamic())
                {
                    // Not allowed cast template type from dynamic
                    // In fact, cast func from dynamic is dangerous too...
                    if (to->is_complex_type())
                        return false;
                    return true;
                }

                if (to->accept_type(from, false))
                    return true;

                // Forbid 'nil' cast to any other value
                if (from->is_pending() || to->is_pending() || from->is_nil() || to->is_nil())
                    return false;

                if (from->accept_type(to, true))
                    return true; // ISSUE-16: using type is create a new type based on old-type, impl-cast from base-type & using-type is invalid.

                if (from->is_func() || to->is_func())
                    return false;

                if (to->value_type == value::valuetype::string_type)
                    return true;

                if (to->value_type == value::valuetype::integer_type
                    || to->value_type == value::valuetype::real_type
                    || to->value_type == value::valuetype::handle_type
                    || to->value_type == value::valuetype::string_type)
                {
                    if (from->value_type == value::valuetype::integer_type
                        || from->value_type == value::valuetype::real_type
                        || from->value_type == value::valuetype::handle_type
                        || from->value_type == value::valuetype::string_type)
                        return true;
                }

                if (from->value_type == value::valuetype::string_type)
                {
                    if (to->is_array() && ((!to->has_template()) || to->template_arguments[0]->is_dynamic()))
                        return true;
                    if (to->is_map() && ((!to->has_template()) || (
                        (to->template_arguments[0]->is_dynamic() || to->template_arguments[0]->is_string())
                        && to->template_arguments[1]->is_dynamic()
                        )))
                        return true;
                }
                return false;
            }

            static bool is_custom_type(const std::wstring& name)
            {
                if (name_type_pair.find(name) != name_type_pair.end())
                    return false;
                return true;
            }

            void set_type_with_name(const std::wstring& _type_name)
            {
                complex_type = nullptr;
                type_name = _type_name;
                value_type = get_type_from_name(_type_name);
                is_pending_type = _type_name == L"pending"; // reset state;

                using_type_name = nullptr;
                template_arguments.clear();

                if (value_type == value::valuetype::array_type)
                {
                    template_arguments.push_back(new ast_type(L"dynamic"));
                }
                else if (value_type == value::valuetype::mapping_type)
                {
                    template_arguments.push_back(new ast_type(L"dynamic"));
                    template_arguments.push_back(new ast_type(L"dynamic"));
                }
            }
            void set_type(const ast_type* _type)
            {
                *this = *_type;
            }
            void set_ret_type(const ast_type* _type)
            {
                wo_test(is_func());

                if (_type->is_func())
                {
                    type_name = L"complex";
                    complex_type = new ast_type(*_type);
                    is_pending_type = false; // reset state;
                }
                else
                {
                    // simplx
                    set_type_with_name(_type->type_name);
                    using_type_name = _type->using_type_name;
                    template_arguments = _type->template_arguments;
                    struct_member_index = _type->struct_member_index;
                    template_impl_naming_checking = _type->template_impl_naming_checking;
                }
            }

            ast_type(const std::wstring& _type_name)
            {
                set_type_with_name(_type_name);
                if (is_custom_type(_type_name))
                    is_pending_type = true;
            }
            ast_type(const value& _val)
            {
                type_name = get_name_from_type(_val.type);
                value_type = _val.type;
            }
            ast_type(ast_type* _val)
            {
                // complex_type
                type_name = L"complex";
                value_type = value::valuetype::invalid;

                complex_type = _val;
            }

            template<typename ... ArgTs>
            static ast_type* create_type_at(ast_base* base, ArgTs&&... args)
            {
                ast_type* ty = new ast_type(args...);
                ty->copy_source_info(base);
                return ty;
            }

            void set_as_function_type()
            {
                is_function_type = true;
            }
            ast_type* get_return_type() const
            {
                if (is_complex())
                    return new ast_type(*complex_type);

                auto* rett = new ast_type(type_name);
                rett->using_type_name = using_type_name;
                rett->template_arguments = template_arguments;
                rett->struct_member_index = struct_member_index;
                rett->template_impl_naming_checking = template_impl_naming_checking;
                return rett;
            }
            void append_function_argument_type(ast_type* arg_type)
            {
                argument_types.push_back(arg_type);
            }
            void set_as_variadic_arg_func()
            {
                is_variadic_function_type = true;
            }

            bool is_dynamic() const
            {
                return !is_func() && type_name == L"dynamic";
            }
            bool is_custom() const
            {
                if (has_template())
                {
                    for (auto arg_type : template_arguments)
                    {
                        if (arg_type->is_custom())
                            return true;
                    }
                }
                if (is_func())
                {
                    for (auto arg_type : argument_types)
                    {
                        if (arg_type->is_custom())
                            return true;
                    }
                }
                if (is_complex())
                    return complex_type->is_custom();
                else
                    return is_custom_type(type_name);
            }
            bool is_pure_pending() const
            {
                return !is_func() && type_name == L"pending" && typefrom == nullptr;
            }
            bool is_hkt() const;
            bool is_hkt_typing() const;
            bool is_pending() const
            {
                if (is_hkt_typing())
                    return false;

                if (has_template())
                {
                    for (auto arg_type : template_arguments)
                    {
                        if (arg_type->is_pending())
                            return true;
                    }
                }
                if (is_func())
                {
                    for (auto arg_type : argument_types)
                    {
                        if (arg_type->is_pending())
                            return true;
                    }
                }

                bool base_type_pending;
                if (is_complex())
                    base_type_pending = complex_type->is_pending();
                else
                    base_type_pending = type_name == L"pending";

                return is_pending_type || base_type_pending;
            }
            bool may_need_update() const
            {
                if (has_template())
                {
                    for (auto arg_type : template_arguments)
                    {
                        if (arg_type->may_need_update())
                            return true;
                    }
                }
                if (is_func())
                {
                    for (auto arg_type : argument_types)
                    {
                        if (arg_type->may_need_update())
                            return true;
                    }
                }

                bool base_type_pending;
                if (is_complex())
                    base_type_pending = complex_type->may_need_update();
                else
                    base_type_pending = type_name == L"pending";

                return is_pending_type || base_type_pending;
            }
            bool is_pending_function() const
            {
                if (is_func())
                {
                    return is_pending();
                }
                return false;
            }
            bool is_void() const
            {
                return type_name == L"void";
            }
            bool is_nil() const
            {
                return type_name == L"nil";
            }
            bool is_gc_type() const
            {
                return (uint8_t)value_type & (uint8_t)value::valuetype::need_gc;
            }
            bool is_like(const ast_type* another, const std::vector<std::wstring>& termplate_set, ast_type** out_para = nullptr, ast_type** out_args = nullptr)const
            {
                // Only used after pass1

                if (is_func() != another->is_func()
                    || is_variadic_function_type != another->is_variadic_function_type
                    || template_arguments.size() != template_arguments.size()
                    || argument_types.size() != another->argument_types.size())
                    return false;

                if ((using_type_name == nullptr && another->using_type_name)
                    || (using_type_name && another->using_type_name == nullptr))
                {
                    const ast_type* chtype = another->is_func() ? another->get_return_type() : another;
                    if (using_type_name && using_type_name->is_like(chtype, termplate_set))
                    {
                        if (out_para)*out_para = dynamic_cast<ast_type*>(using_type_name->instance());
                        if (out_args)*out_args = const_cast<ast_type*>(chtype);
                        return true;
                    }
                    chtype = is_func() ? get_return_type() : this;
                    if (another->using_type_name && chtype->is_like(another->using_type_name, termplate_set))
                    {
                        if (out_para)*out_para = const_cast<ast_type*>(chtype);
                        if (out_args)*out_args = dynamic_cast<ast_type*>(another->using_type_name->instance());
                        return true;
                    }

                    // TODO: If using A = ...;
                    //       The type: A or (A)=>... will have same using-type. need think about how to deal with it.
                    return false;
                }

                if (using_type_name)
                {
                    if (find_type_in_this_scope(using_type_name) != find_type_in_this_scope(another->using_type_name))
                        return false;
                }
                else
                {
                    if (std::find(termplate_set.begin(), termplate_set.end(), type_name) != termplate_set.end()
                        || std::find(termplate_set.begin(), termplate_set.end(), type_name) != termplate_set.end())
                    {
                        // Do nothing
                    }
                    else if (value_type != another->value_type
                        || type_name != another->type_name)
                        return false;
                }
                if (out_para)*out_para = const_cast<ast_type*>(this);
                if (out_args)*out_args = const_cast<ast_type*>(another);
                return true;

            }
            static lang_symbol* base_typedef_symbol(lang_symbol* symb);
            bool is_same(const ast_type* another, bool ignore_using_type) const;
            bool is_builtin_basic_type()
            {
                if (is_bool())
                    return true;
                if (is_custom() || using_type_name)
                    return false;
                return true;
            }
            bool accept_type(const ast_type* another, bool ignore_using_type) const
            {
                if (is_pending_function() || another->is_pending_function())
                    return false;

                if (is_pending() || another->is_pending())
                    return false;

                // Might HKT
                if (is_hkt_typing() && another->is_hkt_typing())
                {
                    if (base_typedef_symbol(symbol) == base_typedef_symbol(another->symbol))
                        return true;
                    return false;
                }

                if (another->is_anything())
                    return true; // top type, OK
                if (is_void())
                    return true; // button type, OK

                if (!ignore_using_type && (using_type_name || another->using_type_name))
                {
                    if (!using_type_name || !another->using_type_name)
                        return false;

                    if (find_type_in_this_scope(using_type_name) != find_type_in_this_scope(another->using_type_name))
                        return false;

                    if (using_type_name->template_arguments.size() != another->using_type_name->template_arguments.size())
                        return false;

                    for (size_t i = 0; i < using_type_name->template_arguments.size(); ++i)
                        if (!using_type_name->template_arguments[i]->accept_type(another->using_type_name->template_arguments[i], ignore_using_type))
                            return false;
                }
                if (has_template())
                {
                    if (template_arguments.size() != another->template_arguments.size())
                        return false;
                    for (size_t index = 0; index < template_arguments.size(); index++)
                    {
                        if (!template_arguments[index]->accept_type(another->template_arguments[index], ignore_using_type))
                            return false;
                    }
                }
                if (is_func())
                {
                    if (!another->is_func())
                        return false;

                    if (argument_types.size() != another->argument_types.size())
                        return false;
                    for (size_t index = 0; index < argument_types.size(); index++)
                    {
                        if (!argument_types[index]->accept_type(another->argument_types[index], ignore_using_type))
                            return false;
                    }
                    if (is_variadic_function_type != another->is_variadic_function_type)
                        return false;
                }
                else if (another->is_func())
                    return false;

                if (is_complex() && another->is_complex())
                    return complex_type->accept_type(another->complex_type, ignore_using_type);
                else if (!is_complex() && !another->is_complex())
                    return type_name == another->type_name;
                return false;
            }
            bool is_func() const
            {
                return is_function_type;
            }
            bool is_bool() const
            {
                return !is_func() &&
                    (type_name == L"bool" || (using_type_name && using_type_name->type_name == L"bool"));
            }
            bool is_union() const
            {
                return !is_func() &&
                    (type_name == L"union");
            }
            bool is_tuple() const
            {
                return !is_func() &&
                    (type_name == L"tuple");
            }
            bool is_anything() const
            {
                return !is_func() &&
                    (type_name == L"anything");
            }
            bool is_struct() const
            {
                return !is_func() &&
                    (type_name == L"struct");
            }
            bool has_template() const
            {
                return !template_arguments.empty();
            }
            bool is_complex() const
            {
                return complex_type;
            }
            bool is_string() const
            {
                return value_type == value::valuetype::string_type && !is_func();
            }
            bool is_array() const
            {
                return value_type == value::valuetype::array_type && !is_func();
            }
            bool is_complex_type() const
            {
                if (is_func() || using_type_name || is_union() || is_struct() || is_tuple())
                    return true;
                if (is_array() || is_map())
                {
                    for (auto* temp : template_arguments)
                    {
                        if (!temp->is_dynamic())
                            return true;
                    }
                    return false;
                }

                return has_template();
            }
            bool is_map() const
            {
                return value_type == value::valuetype::mapping_type && !is_func();
            }
            bool is_integer() const
            {
                return value_type == value::valuetype::integer_type && !is_func();
            }
            bool is_real() const
            {
                return value_type == value::valuetype::real_type && !is_func();
            }
            bool is_handle() const
            {
                return value_type == value::valuetype::handle_type && !is_func();
            }
            bool is_gchandle() const
            {
                return value_type == value::valuetype::gchandle_type && !is_func();
            }

            std::wstring get_type_name(bool ignore_using_type = true) const;

            void display(std::wostream& os = std::wcout, size_t lay = 0) const override
            {
                space(os, lay);
                os << L"<"
                    << ANSI_HIR
                    << L"type"
                    << ANSI_RST
                    << L" : "
                    << ANSI_HIM << get_type_name() << ANSI_RST << L" >" << std::endl;
            }

            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this, L"pending");
                if (!child_instance) *dumm = *this;
                ast_symbolable_base::instance(dumm);

                dumm->symbol = symbol;
                dumm->searching_begin_namespace_in_pass2 = searching_begin_namespace_in_pass2;

                // Write self copy functions here..
                dumm->typefrom = dude_dump_ast_value(dumm->typefrom);
                WO_REINSTANCE(dumm->complex_type);

                for (auto& argtype : dumm->argument_types)
                {
                    WO_REINSTANCE(argtype);
                }
                for (auto& argtype : dumm->template_arguments)
                {
                    WO_REINSTANCE(argtype);
                }
                WO_REINSTANCE(dumm->using_type_name);

                return dumm;
            }
        };

        struct ast_value : virtual public grammar::ast_base
        {
            // this type of ast node is used for stand a value or product a value.
            // liter functioncall variable and so on will belong this type of node.
            ast_type* value_type = nullptr;

            bool is_mark_as_using_ref = false;
            bool is_ref_ob_in_finalize = false;

            bool is_const_value = false;
            bool can_be_assign = false;

            bool is_constant = false;
            wo::value constant_value = {};

            ast_value& operator = (ast_value&&) = delete;
            ast_value& operator = (const ast_value&) = default;

            ~ast_value()
            {
                if (auto* gcunit = constant_value.get_gcunit_with_barrier())
                {
                    wo_assert(constant_value.type == value::valuetype::string_type);
                    wo_assert(gcunit->gc_type == gcbase::gctype::no_gc);

                    gcunit->~gcbase();
                    free64(gcunit);
                }
            }

            virtual wo::value& get_constant_value()
            {
                if (!this->is_constant)
                    wo_error("This value is not a constant.");

                return constant_value;
            };

            virtual void update_constant_value(lexer* lex)
            {
                wo_error("ast_value cannot update_constant_value.");
            }

            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // ast_symbolable_base::instance(dumm);

                // Write self copy functions here..
                if (constant_value.type == value::valuetype::string_type)
                    dumm->constant_value.set_string_nogc(constant_value.string->c_str());
                WO_REINSTANCE(dumm->value_type);

                return dumm;
            }
        };

        struct ast_value_takeplace : virtual public ast_value
        {
            bool as_ref = false;
            opnum::opnumbase* used_reg = nullptr;
            ast_value_takeplace()
            {
                value_type = new ast_type(L"pending");
            }
            void update_constant_value(lexer* lex) override
            {
                // do nothing..
            }
            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
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
            void update_constant_value(lexer* lex) override
            {
                // do nothing
            }

            static wo_handle_t wstr_to_handle(const std::wstring& str)
            {
                wo_handle_t base = 10;
                wo_handle_t sum = 0;

                const wchar_t* wstr = str.c_str();

                bool neg = false;
                if (wstr[0] == L'-')
                {
                    neg = true, ++wstr;
                }
                else if (wstr[0] == L'+')
                    ++wstr;

                if (wstr[0] == L'0')
                {
                    if (wstr[1] == 0)
                        return 0;
                    else if (lexer::lex_toupper(wstr[1]) == L'X')
                    {
                        base = 16;
                        wstr = wstr + 2;
                    }
                    else if (lexer::lex_toupper(wstr[1]) == L'B')
                    {
                        base = 2;
                        wstr = wstr + 2;
                    }
                    else
                    {
                        base = 8;
                        wstr++;
                    }
                }

                while (*wstr)
                {
                    if (*wstr == L'H' || *wstr == L'h')
                        break;
                    sum = base * sum + lexer::lex_hextonum(*wstr);
                    ++wstr;
                }
                if (neg)
                    return (wo_handle_t)(-(wo_integer_t)sum);
                return sum;
            }
            static wo_integer_t wstr_to_integer(const std::wstring& str)
            {
                wo_integer_t base = 10;
                wo_integer_t sum = 0;
                const wchar_t* wstr = str.c_str();

                bool neg = false;
                if (wstr[0] == L'-')
                {
                    neg = true, ++wstr;
                }
                else if (wstr[0] == L'+')
                    ++wstr;

                if (wstr[0] == L'0')
                {
                    if (wstr[1] == 0)
                        return 0;
                    else if (lexer::lex_toupper(wstr[1]) == L'X')
                    {
                        base = 16;
                        wstr = wstr + 2;
                    }
                    else if (lexer::lex_toupper(wstr[1]) == L'B')
                    {
                        base = 2;
                        wstr = wstr + 2;
                    }
                    else
                    {
                        base = 8;
                        wstr++;
                    }
                }

                while (*wstr)
                {
                    sum = base * sum + lexer::lex_hextonum(*wstr);
                    ++wstr;
                }
                if (neg)
                    return -sum;
                return sum;
            }
            static wo_real_t wstr_to_real(const std::wstring& str)
            {
                return std::stod(str);
            }

            ast_value_literal()
            {
                is_constant = true;
                constant_value.set_nil();
            }

            ast_value_literal(const token& te)
            {
                is_constant = true;

                switch (te.type)
                {
                case lex_type::l_literal_handle:
                    constant_value.set_handle(wstr_to_handle(te.identifier));
                    break;
                case lex_type::l_literal_integer:
                    constant_value.set_integer(wstr_to_integer(te.identifier));
                    break;
                case lex_type::l_literal_real:
                    constant_value.set_real(wstr_to_real(te.identifier));
                    break;
                case lex_type::l_literal_string:
                case lex_type::l_format_string:
                case lex_type::l_format_string_end:
                    constant_value.set_string_nogc(wstr_to_str(te.identifier).c_str());
                    break;
                case lex_type::l_nil:
                    constant_value.set_nil();
                    break;
                case lex_type::l_inf:
                    constant_value.set_real(INFINITY);
                    break;
                default:
                    wo_error("Unexcepted literal type.");
                    break;
                }

                value_type = new ast_type(constant_value);
            }

            void display(std::wostream& os = std::wcout, size_t lay = 0) const override
            {
                space(os, lay);
                os << L"< " << ANSI_HIC << wo_cast_string((wo_value)&constant_value) << ANSI_RST L" : " ANSI_HIG;

                switch (constant_value.type)
                {
                case value::valuetype::integer_type:
                    os << L"int";
                    break;
                case value::valuetype::real_type:
                    os << L"real";
                    break;
                case value::valuetype::handle_type:
                    os << L"handle";
                    break;
                case value::valuetype::string_type:
                    os << L"string";
                    break;
                case value::valuetype::mapping_type:
                    os << L"map";
                    break;
                case value::valuetype::gchandle_type:
                    os << L"gchandle";
                    break;
                case value::valuetype::array_type:
                    os << L"array";
                    break;
                case value::valuetype::invalid:
                    os << L"nil";
                    break;
                default:
                    break;
                }
                os << ANSI_RST << "(" << ANSI_HIR << value_type->get_type_name() << ANSI_RST << ")";

                os << ANSI_RST L" >" << std::endl;
            }

            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_value::instance(dumm);

                // Write self copy functions here..
                return dumm;
            }
        };

        struct ast_value_type_cast : public virtual ast_value
        {
            ast_value* _be_cast_value_node;
            ast_value_type_cast(ast_value* value, ast_type* type)
            {
                is_constant = false;
                _be_cast_value_node = value;
                value_type = type;
            }

            ast_value_type_cast() {}
            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_value::instance(dumm);

                // Write self copy functions here..
                WO_REINSTANCE(dumm->_be_cast_value_node);

                return dumm;
            }
            void display(std::wostream& os = std::wcout, size_t lay = 0) const override
            {
                space(os, lay);
                os << L"< " << ANSI_HIR << L"cast" << ANSI_RST << L" : >" << std::endl;
                _be_cast_value_node->display(os, lay + 1);

                space(os, lay);
                os << L"< " << ANSI_HIR << L"to "
                    << ANSI_HIM << value_type->get_type_name() << ANSI_RST;

                os << L" >" << std::endl;
            }

            void update_constant_value(lexer* lex) override
            {
                if (is_constant)
                    return;

                _be_cast_value_node->update_constant_value(lex);

                if (!_be_cast_value_node->value_type->is_pending() && !value_type->is_pending())
                {
                    if (_be_cast_value_node->is_constant)
                    {
                        // just cast the value!
                        value last_value = _be_cast_value_node->get_constant_value();

                        if (value_type->is_bool())
                            // Set bool (1 or 0)
                            constant_value.set_integer(last_value.integer ? 1 : 0);
                        else
                        {
                            value::valuetype aim_real_type = value_type->value_type;
                            if (value_type->is_dynamic())
                            {
                                aim_real_type = last_value.type;
                            }

                            switch (aim_real_type)
                            {
                            case value::valuetype::real_type:
                                if (last_value.is_nil())
                                    goto try_cast_nil_to_int_handle_real_str;
                                constant_value.set_real(wo_cast_real((wo_value)&last_value));
                                break;
                            case value::valuetype::integer_type:
                                if (last_value.is_nil())
                                    goto try_cast_nil_to_int_handle_real_str;
                                constant_value.set_integer(wo_cast_int((wo_value)&last_value));
                                break;
                            case value::valuetype::string_type:
                                if (last_value.is_nil())
                                    goto try_cast_nil_to_int_handle_real_str;
                                constant_value.set_string_nogc(wo_cast_string((wo_value)&last_value));
                                break;
                            case value::valuetype::handle_type:
                                if (last_value.is_nil())
                                    goto try_cast_nil_to_int_handle_real_str;
                                constant_value.set_handle(wo_cast_handle((wo_value)&last_value));
                                break;
                            case value::valuetype::mapping_type:
                                if (last_value.is_nil())
                                {
                                    constant_value.set_gcunit_with_barrier(value::valuetype::mapping_type);
                                    break;
                                }
                                if (last_value.type != value::valuetype::string_type)
                                    goto try_cast_nil_to_int_handle_real_str;
                                return; // cast it in runtime
                            case value::valuetype::gchandle_type:
                                if (last_value.is_nil())
                                {
                                    constant_value.set_gcunit_with_barrier(value::valuetype::gchandle_type);
                                    break;
                                }
                                goto try_cast_nil_to_int_handle_real_str;
                                break;
                            case value::valuetype::array_type:
                                if (last_value.is_nil())
                                {
                                    constant_value.set_gcunit_with_barrier(value::valuetype::array_type);
                                    break;
                                }
                                if (last_value.type != value::valuetype::string_type)
                                    goto try_cast_nil_to_int_handle_real_str;
                                return; // cast it in runtime
                            default:
                            try_cast_nil_to_int_handle_real_str:
                                if (value_type->is_dynamic() || (last_value.is_nil() && value_type->is_func()))
                                {
                                    constant_value.set_val(&last_value);
                                }
                                else
                                {
                                    is_constant = false;
                                }
                                break;
                            }
                        }
                        is_constant = true;
                    }
                }
            }
        };

        struct ast_value_type_judge : public virtual ast_value
        {
            ast_value* _be_cast_value_node;
            ast_value_type_judge(ast_value* value, ast_type* type)
            {
                _be_cast_value_node = value;
                value_type = type;
            }
            ast_value_type_judge() {}
            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_value::instance(dumm);

                // Write self copy functions here..
                WO_REINSTANCE(dumm->_be_cast_value_node);

                return dumm;
            }
            void display(std::wostream& os = std::wcout, size_t lay = 0) const override
            {
                space(os, lay);
                os << L"< " << ANSI_HIR << L"as" << ANSI_RST << L" : >" << std::endl;
                _be_cast_value_node->display(os, lay + 1);

                space(os, lay);
                os << L"< " << ANSI_HIR << L"to "
                    << ANSI_HIM << value_type->get_type_name() << ANSI_RST;

                os << L" >" << std::endl;
            }
            void update_constant_value(lexer* lex) override
            {
                // do nothing
                if (is_constant)
                    return;

                _be_cast_value_node->update_constant_value(lex);
                if (!_be_cast_value_node->value_type->is_pending() && _be_cast_value_node->is_constant)
                {
                    if (value_type->accept_type(_be_cast_value_node->value_type, false))
                    {
                        constant_value.set_val_compile_time(&_be_cast_value_node->get_constant_value());
                        is_constant = true;
                    }
                }
            }
        };

        struct ast_value_type_check : public virtual ast_value
        {
            ast_value* _be_check_value_node;
            ast_type* aim_type;
            bool check_pending = false;

            ast_value_type_check(ast_value* value, ast_type* type)
            {
                _be_check_value_node = value;
                aim_type = type;

                value_type = new ast_type(L"bool");
            }
            ast_value_type_check() {}
            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
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

                _be_check_value_node->update_constant_value(lex);

                if (!_be_check_value_node->value_type->is_pending() && (check_pending || !aim_type->is_pending()))
                {
                    auto result = aim_type->accept_type(_be_check_value_node->value_type, false);
                    if (result)
                    {
                        is_constant = true;
                        constant_value.set_integer(result);
                    }
                    if (_be_check_value_node->value_type->is_dynamic())
                    {
                        if (!aim_type->is_dynamic() && !check_pending)
                            return; // do nothing... give error in analyze_finalize
                    }
                    is_constant = true;
                    constant_value.set_integer(result);
                }
            }

            void display(std::wostream& os = std::wcout, size_t lay = 0) const override
            {
                space(os, lay);
                os << L"< " << ANSI_HIR << L"is" << ANSI_RST << L" : >" << std::endl;
                _be_check_value_node->display(os, lay + 1);

                space(os, lay);
                os << L"< " << ANSI_HIR << L"to "
                    << ANSI_HIM << aim_type->get_type_name() << ANSI_RST;

                os << L" >" << std::endl;
            }
        };

        struct ast_decl_attribute : virtual public grammar::ast_base
        {
            std::set<lex_type> attributes;
            void varify_attributes(lexer* lex) const
            {
                // 1) Check if public, protected or private at same time
                lex_type has_describe = +lex_type::l_error;
                for (auto att : attributes)
                {
                    if (att == +lex_type::l_public || att == +lex_type::l_private || att == +lex_type::l_protected)
                    {
                        if (has_describe != +lex_type::l_error)
                        {
                            lex->parser_error(0x0000, WO_ERR_CANNOT_DECL_PUB_PRI_PRO_SAME_TIME);
                            break;
                        }
                        has_describe = att;
                    }
                }
            }
            void add_attribute(lexer* lex, lex_type attr)
            {
                if (attributes.find(attr) == attributes.end())
                {
                    attributes.insert(attr);
                }
                else
                    lex->parser_error(0x0000, WO_ERR_REPEAT_ATTRIBUTE);
            }

            bool is_constant_attr() const
            {
                return attributes.find(+lex_type::l_const) != attributes.end();
            }

            bool is_static_attr() const
            {
                return attributes.find(+lex_type::l_static) != attributes.end();
            }

            bool is_private_attr() const
            {
                return attributes.find(+lex_type::l_private) != attributes.end();
            }

            bool is_protected_attr() const
            {
                return attributes.find(+lex_type::l_protected) != attributes.end();
            }

            bool is_extern_attr() const
            {
                return attributes.find(+lex_type::l_extern) != attributes.end();
            }

            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // ast_value::instance(dumm);

                // Write self copy functions here..

                return dumm;
            }
        };

        struct ast_defines : virtual public grammar::ast_base
        {
            ast_decl_attribute* declear_attribute = nullptr;

            bool template_reification_judge_by_funccall = false;// if template_reification_judge_by_funccall==true, pass2 will not checkxx
            bool is_template_define = false;
            bool is_template_reification = false; // if is_template_reification == true, symbol will not put to overset..
            lang_symbol* this_reification_lang_symbol = nullptr;
            std::vector<ast_type*> this_reification_template_args;
            std::vector<std::wstring> template_type_name_list;
            std::map<std::vector<uint32_t>, ast_defines*> template_typehashs_reification_instance_list;

            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // ast_value::instance(dumm);

                // Write self copy functions here..
                WO_REINSTANCE(dumm->declear_attribute);

                return dumm;
            }
        };

        struct ast_value_symbolable_base : virtual  ast_value, virtual ast_symbolable_base
        {
            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_value::instance(dumm);
                ast_symbolable_base::instance(dumm);
                // Write self copy functions here..

                return dumm;
            }

            void update_constant_value(lexer* lex) override
            {
                // do nothing;
            }
        };

        struct ast_value_variable : virtual ast_value_symbolable_base
        {
            std::wstring var_name;
            std::vector<ast_type*> template_reification_args;
            bool directed_function_call = false;
            bool is_auto_judge_function_overload = false;
            ast_value_variable(const std::wstring& _var_name)
            {
                var_name = _var_name;
                value_type = new ast_type(L"pending");
                can_be_assign = true;
            }
            ast_value_variable() {}
            void display(std::wostream& os = std::wcout, size_t lay = 0) const override
            {
                space(os, lay);
                os << L"< " << ANSI_HIY << L"variable: "
                    << ANSI_HIR;

                if (search_from_global_namespace)
                    os << "::";
                for (auto& nspx : scope_namespaces)
                {
                    os << nspx << "::";
                }
                os << var_name
                    << ANSI_RST
                    << L" : "
                    << ANSI_HIM << value_type->get_type_name() << ANSI_RST << L" >" << std::endl;
            }
            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_value_symbolable_base::instance(dumm);
                // Write self copy functions here..

                for (auto& tras : dumm->template_reification_args)
                {
                    WO_REINSTANCE(tras);
                }
                return dumm;
            }
            void update_constant_value(lexer* lex) override;
        };

        struct ast_list : virtual public grammar::ast_base
        {
            void append_at_head(grammar::ast_base* astnode)
            {
                // item LIST
                if (children)
                {
                    auto old_last = last;
                    auto old_child = children;

                    remove_allnode();
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
            void append_at_end(grammar::ast_base* astnode)
            {
                // LIST item
                add_child(astnode);
            }

            void display(std::wostream& os = std::wcout, size_t lay = 0) const override
            {
                space(os, lay);
                os << L"< " << ANSI_HIY << L"list" << ANSI_RST << L" >" << std::endl;
                space(os, lay);
                os << L"{" << std::endl;
                auto* mychild = children;
                while (mychild)
                {
                    mychild->display(os, lay + 1);

                    mychild = mychild->sibling;
                }
                space(os, lay);
                os << L"}" << std::endl;
            }
            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // ast_value_symbolable_base::instance(dumm);
                // Write self copy functions here..

                return dumm;
            }
        };

        struct ast_empty : virtual public grammar::ast_base
        {
            // used for stand fro l_empty
            // some passer will ignore this xx

            static bool is_empty(std::any& any)
            {
                if (grammar::ast_base* _node; cast_any_to<grammar::ast_base*>(any, _node))
                {
                    if (dynamic_cast<ast_empty*>(_node))
                    {
                        return true;
                    }
                }
                if (token _node = { lex_type::l_error }; cast_any_to<token>(any, _node))
                {
                    if (_node.type == +lex_type::l_empty)
                    {
                        return true;
                    }
                }

                return false;
            }
            void display(std::wostream& os = std::wcout, size_t lay = 0) const override
            {
                /*display nothing*/
            }
            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // ast_value_symbolable_base::instance(dumm);
                // Write self copy functions here..

                return dumm;
            }
        };

        struct ast_value_funccall;

        struct ast_value_binary : virtual public ast_value
        {
            // used for storing binary-operate;
            ast_value* left = nullptr;
            lex_type operate = +lex_type::l_error;
            ast_value* right = nullptr;

            ast_value_funccall* overrided_operation_call = nullptr;

            ast_value_binary()
            {
                value_type = new ast_type(L"pending");
            }

            void display(std::wostream& os = std::wcout, size_t lay = 0) const override
            {
                space(os, lay);
                os << L"< " << ANSI_HIY << L"bin-op: " << lexer::lex_is_operate_type(operate) << ANSI_RST << " >" << std::endl;
                space(os, lay);
                os << L"{" << std::endl;
                space(os, lay);
                os << L"left:" << std::endl;
                left->display(os, lay + 1);
                space(os, lay);
                os << L"right:" << std::endl;
                right->display(os, lay + 1);
                space(os, lay);
                os << L"}" << std::endl;
            }
            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_value::instance(dumm);
                // Write self copy functions here..

                WO_REINSTANCE(dumm->left);
                WO_REINSTANCE(dumm->right);

                return dumm;
            }

            template <typename T>
            static T binary_operate(lexer& lex, T left, T right, lex_type op_type, bool* out_result)
            {
                *out_result = true;
                switch (op_type)
                {
                case lex_type::l_add:
                    if constexpr (std::is_same<T, wo_string_t>::value)
                    {
                        thread_local static std::string _tmp_string_add_result;
                        _tmp_string_add_result = left;
                        _tmp_string_add_result += right;
                        return _tmp_string_add_result.c_str();
                    }
                    else
                        return left + right;
                case lex_type::l_sub:
                    if constexpr (!std::is_same<T, wo_string_t>::value)
                        return left - right;
                case lex_type::l_mul:
                    if constexpr (!std::is_same<T, wo_string_t>::value
                        && !std::is_same<T, wo_handle_t>::value)
                        return left * right;
                case lex_type::l_div:
                    if constexpr (!std::is_same<T, wo_string_t>::value
                        && !std::is_same<T, wo_handle_t>::value)
                        return left / right;
                case lex_type::l_mod:
                    if constexpr (!std::is_same<T, wo_string_t>::value
                        && !std::is_same<T, wo_handle_t>::value)
                    {
                        if constexpr (std::is_same<T, wo_real_t>::value)
                        {
                            return fmod(left, right);
                        }
                        else
                            return left % right;
                    }
                default:
                    *out_result = false;
                }

                return T{};
            }

            static ast_type* binary_upper_type(ast_type* left_v, ast_type* right_v)
            {
                if (left_v->is_anything())
                    return right_v;

                if (left_v->is_dynamic() || right_v->is_dynamic())
                {
                    return nullptr;
                }
                if (left_v->is_func() || right_v->is_func())
                {
                    return nullptr;
                }

                auto left_t = left_v->value_type;
                auto right_t = right_v->value_type;

                if (left_v->is_same(right_v, false))
                {
                    ast_type* type = new ast_type(L"pending");
                    type->set_type(left_v);
                    return type;
                }
                return nullptr;
            }

            static ast_type* binary_upper_type_with_operator(ast_type* left_v, ast_type* right_v, lex_type op)
            {
                if (left_v->is_custom() || right_v->is_custom())
                    return nullptr;
                if (left_v->is_func() || right_v->is_func())
                    return nullptr;
                if ((left_v->is_string() || right_v->is_string()) && op != +lex_type::l_add && op != +lex_type::l_add_assign)
                    return nullptr;
                if (left_v->is_map() || right_v->is_map())
                    return nullptr;
                if (left_v->is_array() || right_v->is_array())
                    return nullptr;
                if (left_v->is_anything() || right_v->is_anything())
                    return nullptr;
                if (left_v->is_bool() || right_v->is_bool())
                    return nullptr;
                if (left_v->is_tuple() || right_v->is_tuple())
                    return nullptr;
                if (left_v->is_struct() || right_v->is_struct())
                    return nullptr;
                if (left_v->is_union() || right_v->is_union())
                    return nullptr;

                return binary_upper_type(left_v, right_v);
            }

            void update_constant_value(lexer* lex) override
            {
                if (is_constant)
                    return;

                left->update_constant_value(lex);
                right->update_constant_value(lex);

                // if left/right is custom, donot calculate them 
                if (left->value_type->is_custom()
                    || left->value_type->using_type_name
                    || right->value_type->is_custom()
                    || right->value_type->using_type_name)
                    return;

                if (overrided_operation_call)
                    return;

                if (left->value_type->is_dynamic() || right->value_type->is_dynamic())
                    return;

                else if (left->value_type->is_pending() || right->value_type->is_pending())
                    return;

                if (nullptr == (value_type = binary_upper_type_with_operator(left->value_type, right->value_type, operate)))
                {
                    // Donot give error here.

                    // lex->lang_error(0x0000, this, WO_ERR_CANNOT_CALC_WITH_L_AND_R);
                    value_type = new ast_type(L"pending");

                    return;
                }

                if (left->is_constant && right->is_constant)
                {
                    is_constant = true;

                    value _left_val = left->get_constant_value();
                    value _right_val = right->get_constant_value();

                    switch (value_type->value_type)
                    {
                    case value::valuetype::integer_type:
                        constant_value.set_integer(
                            binary_operate(*lex,
                                wo_cast_int((wo_value)&_left_val),
                                wo_cast_int((wo_value)&_right_val),
                                operate, &is_constant));
                        break;
                    case value::valuetype::real_type:
                        constant_value.set_real(
                            binary_operate(*lex,
                                wo_cast_real((wo_value)&_left_val),
                                wo_cast_real((wo_value)&_right_val),
                                operate, &is_constant));
                        break;
                    case value::valuetype::handle_type:
                        constant_value.set_handle(
                            binary_operate(*lex,
                                wo_cast_handle((wo_value)&_left_val),
                                wo_cast_handle((wo_value)&_right_val),
                                operate, &is_constant));
                        break;
                    case value::valuetype::string_type:
                    {
                        std::string left_str = wo_cast_string((wo_value)&_left_val);
                        std::string right_str = wo_cast_string((wo_value)&_right_val);
                        const char* val = binary_operate(*lex,
                            (wo_string_t)left_str.c_str(),
                            (wo_string_t)right_str.c_str(),
                            operate, &is_constant);
                        if (is_constant)
                            constant_value.set_string_nogc(val);
                    }
                    break;
                    default:
                        is_constant = false;
                        return;
                    }
                }
            }
        };

        struct ast_namespace : virtual public grammar::ast_base
        {
            std::wstring scope_name;
            ast_list* in_scope_sentence;

            void display(std::wostream& os = std::wcout, size_t lay = 0) const override
            {
                space(os, lay);
                os << L"< " << ANSI_HIY << L"namespace: " << scope_name << ANSI_RST << " >" << std::endl;
                in_scope_sentence->display(os, lay + 0);
            }
            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
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

        struct ast_pattern_base : virtual public grammar::ast_base
        {
            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // ast_defines::instance(dumm);
                // Write self copy functions here..

                return dumm;
            }
        };

        struct ast_varref_defines : virtual public ast_defines
        {
            struct varref_define
            {
                ast_pattern_base* pattern;
                ast_value* init_val;
            };
            std::vector<varref_define> var_refs;
            void display(std::wostream& os = std::wcout, size_t lay = 0) const override
            {
                space(os, lay);
                os << L"< " << ANSI_HIY << L"defines" << ANSI_RST << " >" << std::endl;
                space(os, lay);
                os << L"{" << std::endl;
                for (auto& vr_define : var_refs)
                {
                    space(os, lay + 1);
                    os << "let " << std::endl;
                    vr_define.pattern->display(os, lay + 1);
                    os << L" = " << std::endl;
                    vr_define.init_val->display(os, lay + 1);
                }
                space(os, lay);
                os << L"}" << std::endl;
            }
            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_defines::instance(dumm);
                // Write self copy functions here..

                for (auto& varref : dumm->var_refs)
                {
                    WO_REINSTANCE(varref.pattern);
                    WO_REINSTANCE(varref.init_val);
                }

                return dumm;
            }
        };

        struct ast_value_arg_define : virtual ast_value_symbolable_base, virtual ast_defines
        {
            identifier_decl decl = identifier_decl::IMMUTABLE;
            std::wstring arg_name;

            void display(std::wostream& os = std::wcout, size_t lay = 0) const override
            {
                space(os, lay);
                os << L"< " << ANSI_HIY << (decl == identifier_decl::REF ? L"ref " : L"var ")
                    << arg_name << ANSI_HIM << L" (" << value_type->get_type_name() << L")" << ANSI_RST << " >" << std::endl;
            }
            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_value_symbolable_base::instance(dumm);
                ast_defines::instance(dumm);
                // Write self copy functions here..

                return dumm;
            }
        };
        struct ast_extern_info : virtual public grammar::ast_base
        {
            wo_extern_native_func_t externed_func = nullptr;

            std::wstring load_from_lib;
            std::wstring symbol_name;

            void display(std::wostream& os = std::wcout, size_t lay = 0) const override
            {
                space(os, lay);
                os << L"< " << ANSI_HIY << L"extern" << ANSI_RST << " >" << std::endl;
                space(os, lay);
                os << L"symbol: '" << symbol_name << "'" << std::endl;
                space(os, lay);
                os << L"from: '" << load_from_lib << "'" << std::endl;
            }

            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // ast_value::instance(dumm);
                // Write self copy functions here..
                return dumm;
            }
        };
        struct ast_value_function_define;
        struct ast_where_constraint : virtual public grammar::ast_base
        {
            ast_list* where_constraint_list;
            ast_value_function_define* binded_func_define;
            bool accept = true;

            std::vector<lexer::lex_error_msg> unmatched_constraint;

            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // Write self copy functions here..

                WO_REINSTANCE(dumm->where_constraint_list);
                dumm->binded_func_define = nullptr;
                dumm->accept = true;
                return dumm;
            }
        };
        struct ast_value_function_define : virtual ast_value_symbolable_base, virtual ast_defines
        {
            std::wstring function_name;
            ast_list* argument_list = nullptr;
            ast_list* in_function_sentence = nullptr;
            bool auto_adjust_return_type = false;
            bool has_return_value = false;
            bool ir_func_has_been_generated = false;
            std::string ir_func_signature_tag = "";
            lang_scope* this_func_scope = nullptr;
            ast_extern_info* externed_func_info = nullptr;
            bool is_different_arg_count_in_same_extern_symbol = false;
            std::vector<lang_symbol*> capture_variables;

            ast_where_constraint* where_constraint = nullptr;

            bool is_closure_function()const noexcept
            {
                return capture_variables.size();
            }

            const std::string& get_ir_func_signature_tag()
            {
                if (ir_func_signature_tag == "")
                {
                    //TODO : Change new function to generate signature.
                    auto spacename = get_namespace_chain();

                    ir_func_signature_tag =
                        spacename.empty() ? "func " : ("func " + spacename + "::");

                    if (function_name != L"")
                        ir_func_signature_tag += wstr_to_str(function_name);
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
            void display(std::wostream& os = std::wcout, size_t lay = 0) const override
            {
                space(os, lay);
                os << L"< " << ANSI_HIY << L"func "
                    << ANSI_HIR;

                if (search_from_global_namespace)
                    os << "::";
                for (auto& nspx : scope_namespaces)
                {
                    os << nspx << "::";
                }
                os << function_name
                    << ANSI_RST
                    << L" : "
                    << ANSI_HIM << value_type->get_return_type()->get_type_name() << ANSI_RST << L" >" << std::endl;
                argument_list->display(os, lay + 1);
                if (in_function_sentence)
                    in_function_sentence->display(os, lay + 1);
            }

            wo::value& get_constant_value() override
            {
                if (!this->is_constant)
                    wo_error("Not externed_func.");

                wo_assert(externed_func_info && externed_func_info->externed_func);

                constant_value.set_handle((wo_handle_t)externed_func_info->externed_func);
                return constant_value;
            };
            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));

                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_value_symbolable_base::instance(dumm);
                ast_defines::instance(dumm);
                // Write self copy functions here..

                WO_REINSTANCE(dumm->argument_list);
                WO_REINSTANCE(dumm->in_function_sentence);
                WO_REINSTANCE(dumm->where_constraint);
                WO_REINSTANCE(dumm->externed_func_info);
                dumm->this_func_scope = nullptr;
                dumm->capture_variables.clear();

                return dumm;
            }
        };

        struct ast_token : virtual grammar::ast_base
        {
            token tokens;

            ast_token(const token& tk)
                : tokens(tk)
            {
            }

            ast_token() :tokens({ +lex_type::l_error })
            {

            }

            void display(std::wostream& os = std::wcout, size_t lay = 0) const override
            {
                space(os, lay);
                os << L"< " << ANSI_HIY << tokens << ANSI_RST << L" >" << std::endl;
            }
            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // ast_defines::instance(dumm);
                 // Write self copy functions here..

                return dumm;
            }
        };

        struct ast_return : virtual public grammar::ast_base
        {
            ast_value* return_value = nullptr;
            ast_value_function_define* located_function = nullptr;
            void display(std::wostream& os = std::wcout, size_t lay = 0) const override
            {
                space(os, lay);
                os << L"< " << ANSI_HIY << "return" << ANSI_RST << L" >" << std::endl;
                if (return_value)
                    return_value->display(os, lay + 1);
            }
            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // ast_defines::instance(dumm);
                 // Write self copy functions here..

                WO_REINSTANCE(dumm->return_value);

                wo_assert(!located_function);

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

            void display(std::wostream& os = std::wcout, size_t lay = 0) const override
            {
                space(os, lay);
                os << L"< " << ANSI_HIY << "call" << ANSI_RST << L" >" << std::endl;
                if (auto* fdef = dynamic_cast<ast_value_function_define*>(called_func))
                {
                    space(os, lay + 1);
                    os << fdef->function_name << " (" << ANSI_HIM << fdef->value_type->get_type_name() << ANSI_RST << ")" << std::endl;
                }
                else
                    called_func->display(os, lay + 1);
                space(os, lay);
                os << L"< " << ANSI_HIY << "args:" << ANSI_RST << L" >" << std::endl;
                arguments->display(os, lay + 1);
            }

            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_value::instance(dumm);
                // Write self copy functions here..

                ast_value_function_define* called_func_def = dynamic_cast<ast_value_function_define*>(dumm->called_func);
                if (!called_func_def || called_func_def->function_name == L"")
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
            ast_value_array(ast_list* _items)
                : array_items(_items)
            {
                wo_test(array_items);
                value_type = new ast_type(L"array");
                value_type->template_arguments[0]->set_type_with_name(L"pending");
            }

            void display(std::wostream& os = std::wcout, size_t lay = 0) const override
            {
                space(os, lay);
                os << L"< " << ANSI_HIY << "array" << ANSI_RST << L" >" << std::endl;
                array_items->display(os, lay + 1);
            }
            ast_value_array() {}
            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
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

            ast_value_mapping(ast_list* _items)
                : mapping_pairs(_items)
            {
                wo_test(mapping_pairs);
                value_type = new ast_type(L"map");
                value_type->template_arguments[0]->set_type_with_name(L"pending");
                value_type->template_arguments[1]->set_type_with_name(L"pending");
            }

            void display(std::wostream& os = std::wcout, size_t lay = 0) const override
            {
                space(os, lay);
                os << L"< " << ANSI_HIY << "map" << ANSI_RST << L" >" << std::endl;
                mapping_pairs->display(os, lay + 1);
            }

            ast_value_mapping() {}
            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
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

        struct ast_sentence_block : virtual public grammar::ast_base
        {
            ast_list* sentence_list;

            ast_sentence_block(ast_list* sentences)
                : sentence_list(sentences)
            {
                wo_test(sentence_list);
            }

            void display(std::wostream& os = std::wcout, size_t lay = 0) const override
            {
                space(os, lay);
                os << "<block>" << std::endl;
                sentence_list->display(os, lay);
            }

            static ast_sentence_block* fast_parse_sentenceblock(grammar::ast_base* ast)
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
            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
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

        struct ast_if : virtual public grammar::ast_base
        {
            ast_value* judgement_value;
            ast_base* execute_if_true;
            ast_base* execute_else;

            bool is_constexpr_if = false;

            ast_if(ast_value* jdg, ast_base* exe_true, ast_base* exe_else)
                : judgement_value(jdg), execute_if_true(exe_true), execute_else(exe_else)
            {
                wo_test(judgement_value && execute_if_true);
            }
            void display(std::wostream& os = std::wcout, size_t lay = 0) const override
            {
                space(os, lay);
                os << "<if>" << std::endl;
                judgement_value->display(os, lay + 1);
                space(os, lay);
                os << "<then>" << std::endl;
                execute_if_true->display(os, lay + 1);
                if (execute_else)
                {
                    space(os, lay);
                    os << "<else>" << std::endl;
                    execute_else->display(os, lay + 1);
                }
                space(os, lay);
                os << "<endif>" << std::endl;
            }

            ast_if() {}
            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
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
        struct ast_except : virtual public grammar::ast_base
        {
            ast_base* execute_sentence;
            ast_except(ast_base* exec)
                :execute_sentence(exec)
            {
                wo_test(execute_sentence);
            }
            ast_except() {}
            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // ast_value::instance(dumm);
                // Write self copy functions here..

                WO_REINSTANCE(dumm->execute_sentence);

                return dumm;
            }
        };
        struct ast_while : virtual public grammar::ast_base
        {
            ast_value* judgement_value;
            ast_base* execute_sentence;

            ast_while(ast_value* jdg, ast_base* exec)
                : judgement_value(jdg), execute_sentence(exec)
            {
                wo_test(judgement_value && execute_sentence);
            }
            void display(std::wostream& os = std::wcout, size_t lay = 0) const override
            {
                space(os, lay);
                os << "<while>" << std::endl;
                judgement_value->display(os, lay + 1);
                space(os, lay);
                os << "<do>" << std::endl;
                execute_sentence->display(os, lay + 1);
                space(os, lay);
                os << "<end while>" << std::endl;
            }

            ast_while() {}
            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
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
            lex_type operate = +lex_type::l_error;
            ast_value* right = nullptr;

            ast_value_assign()
            {
                value_type = new ast_type(L"pending");
            }

            void display(std::wostream& os = std::wcout, size_t lay = 0) const override
            {
                space(os, lay);
                os << L"< " << ANSI_HIY << L"assign: " << lexer::lex_is_operate_type(operate) << ANSI_RST << " >" << std::endl;
                space(os, lay);
                os << L"{" << std::endl;
                space(os, lay);
                os << L"left:" << std::endl;
                left->display(os, lay + 1);
                space(os, lay);
                os << L"right:" << std::endl;
                right->display(os, lay + 1);
                space(os, lay);
                os << L"}" << std::endl;
            }

            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_value::instance(dumm);
                // Write self copy functions here..

                WO_REINSTANCE(dumm->left);
                WO_REINSTANCE(dumm->right);

                return dumm;
            }

            void update_constant_value(lexer* lex) override
            {
                // do nothing
            }
        };

        struct ast_value_logical_binary : virtual public ast_value
        {
            // used for storing binary-operate;
            ast_value* left = nullptr;
            lex_type operate = +lex_type::l_error;
            ast_value* right = nullptr;

            ast_value_funccall* overrided_operation_call = nullptr;

            ast_value_logical_binary()
            {
                value_type = new ast_type(L"bool");
            }

            void display(std::wostream& os = std::wcout, size_t lay = 0) const override
            {
                space(os, lay);
                os << L"< " << ANSI_HIY << L"logical: " << lexer::lex_is_operate_type(operate) << ANSI_RST << " >" << std::endl;
                space(os, lay);
                os << L"{" << std::endl;
                space(os, lay);
                os << L"left:" << std::endl;
                left->display(os, lay + 1);
                space(os, lay);
                os << L"right:" << std::endl;
                right->display(os, lay + 1);
                space(os, lay);
                os << L"}" << std::endl;
            }

            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_value::instance(dumm);
                // Write self copy functions here..

                WO_REINSTANCE(dumm->left);
                WO_REINSTANCE(dumm->right);

                return dumm;
            }

            void update_constant_value(lexer* lex) override
            {
                if (is_constant)
                    return;

                left->update_constant_value(lex);
                right->update_constant_value(lex);

                if (!left->value_type->is_same(right->value_type, false))
                    return;

                // if left/right is custom, donot calculate them 
                if (operate == +lex_type::l_land
                    || operate == +lex_type::l_lor)
                {
                    if (!left->value_type->is_bool() || !right->value_type->is_bool())
                        return;
                }
                else if (left->value_type->is_custom() || left->value_type->using_type_name
                    || right->value_type->is_custom() || right->value_type->using_type_name)
                    return;

                if (overrided_operation_call)
                    return;

                if (left->is_constant && right->is_constant)
                {
                    is_constant = true;
                    value_type = new ast_type(L"bool");

                    switch (operate)
                    {
                    case lex_type::l_land:
                        constant_value.set_integer(left->get_constant_value().handle && right->get_constant_value().handle);
                        break;
                    case lex_type::l_lor:
                        constant_value.set_integer(left->get_constant_value().handle || right->get_constant_value().handle);
                        break;
                    case lex_type::l_equal:
                    case lex_type::l_not_equal:
                        if (left->value_type->is_integer())
                            constant_value.set_integer(left->get_constant_value().integer == right->get_constant_value().integer);
                        else if (left->value_type->is_real())
                            constant_value.set_integer(left->get_constant_value().real == right->get_constant_value().real);
                        else if (left->value_type->is_string())
                            constant_value.set_integer(*left->get_constant_value().string == *right->get_constant_value().string);
                        else if (left->value_type->is_handle())
                            constant_value.set_integer(left->get_constant_value().handle == right->get_constant_value().handle);
                        else
                        {
                            is_constant = false;
                            return;
                        }
                        if (operate == +lex_type::l_not_equal)
                            constant_value.set_integer(!constant_value.integer);
                        break;

                    case lex_type::l_less:
                    case lex_type::l_larg_or_equal:
                        if (left->value_type->is_integer())
                            constant_value.set_integer(left->get_constant_value().integer < right->get_constant_value().integer);
                        else if (left->value_type->is_real())
                            constant_value.set_integer(left->get_constant_value().real < right->get_constant_value().real);
                        else if (left->value_type->is_string())
                            constant_value.set_integer(*left->get_constant_value().string < *right->get_constant_value().string);
                        else if (left->value_type->is_handle())
                            constant_value.set_integer(left->get_constant_value().handle < right->get_constant_value().handle);
                        else
                        {
                            is_constant = false;
                            return;
                        }


                        if (operate == +lex_type::l_larg_or_equal)
                            constant_value.set_integer(!constant_value.integer);
                        break;

                    case lex_type::l_larg:
                    case lex_type::l_less_or_equal:

                        if (left->value_type->is_integer())
                            constant_value.set_integer(left->get_constant_value().integer > right->get_constant_value().integer);
                        else if (left->value_type->is_real())
                            constant_value.set_integer(left->get_constant_value().real > right->get_constant_value().real);
                        else if (left->value_type->is_string())
                            constant_value.set_integer(*left->get_constant_value().string > *right->get_constant_value().string);
                        else if (left->value_type->is_handle())
                            constant_value.set_integer(left->get_constant_value().handle > right->get_constant_value().handle);
                        else
                        {
                            is_constant = false;
                            return;
                        }

                        if (operate == +lex_type::l_less_or_equal)
                            constant_value.set_integer(!constant_value.integer);
                        break;

                    default:
                        wo_error("Do not support this op.");
                    }
                }
            }
        };

        struct ast_value_index : virtual public ast_value
        {
            ast_value* from = nullptr;
            ast_value* index = nullptr;

            uint16_t struct_offset = 0xFFFF;

            ast_value_index()
            {
                value_type = new ast_type(L"pending");
                can_be_assign = true;
            }

            void display(std::wostream& os = std::wcout, size_t lay = 0) const override
            {
                space(os, lay);
                os << L"< " << ANSI_HIY << L"index" << ANSI_RST << " >" << std::endl;
                space(os, lay);
                os << L"{" << std::endl;
                space(os, lay);
                os << L"from:" << std::endl;
                from->display(os, lay + 1);
                space(os, lay);
                os << L"at:" << std::endl;
                index->display(os, lay + 1);
                space(os, lay);
                os << L"}" << std::endl;
            }

            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
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
                if (is_constant)
                    return;

                from->update_constant_value(lex);
                index->update_constant_value(lex);

                // if left/right is custom, donot calculate them 
                if (from->value_type->is_custom()
                    || from->value_type->using_type_name
                    || index->value_type->is_custom()
                    || index->value_type->using_type_name)
                    return;

                if (from->is_constant && index->is_constant)
                {
                    if (from->value_type->is_string())
                    {
                        is_constant = true;
                        value_type = new ast_type(L"string");

                        if (!index->value_type->is_integer() && !index->value_type->is_handle())
                        {
                            is_constant = false;
                            return;
                        }

                        size_t strlength = 0;
                        wo_string_t out_str = u8substr(from->get_constant_value().string->c_str(), index->get_constant_value().integer, 1, &strlength);

                        constant_value.set_string_nogc(
                            std::string(out_str, strlength).c_str());
                    }
                }
            }
        };

        struct ast_value_packed_variadic_args : virtual public ast_value
        {
            ast_value_packed_variadic_args()
            {
                value_type = new ast_type(L"array");
            }

            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_value::instance(dumm);
                // Write self copy functions here..
                return dumm;
            }

            void update_constant_value(lexer* lex) override
            {
                // DO NOTHING
            }
        };

        struct ast_value_indexed_variadic_args : virtual public ast_value
        {
            ast_value* argindex;
            ast_value_indexed_variadic_args(ast_value* arg_index)
                : argindex(arg_index)
            {
                wo_assert(argindex);
                value_type = new ast_type(L"dynamic");
                can_be_assign = false;
            }

            ast_value_indexed_variadic_args() {}

            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_value::instance(dumm);
                // Write self copy functions here..

                WO_REINSTANCE(dumm->argindex);

                return dumm;
            }

            void update_constant_value(lexer* lex) override
            {
                // DO NOTHING
            }
        };

        struct ast_fakevalue_unpacked_args : virtual public ast_value
        {
            constexpr static wo_integer_t UNPACK_ALL_ARGUMENT = 65535;
            ast_value* unpacked_pack;
            wo_integer_t expand_count = UNPACK_ALL_ARGUMENT;

            ast_fakevalue_unpacked_args(ast_value* pak, wo_integer_t _expand_count)
                : unpacked_pack(pak),
                expand_count(_expand_count)
            {
                wo_assert(unpacked_pack && _expand_count >= 0);
                value_type = new ast_type(L"dynamic");
            }

            ast_fakevalue_unpacked_args() {}
            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_value::instance(dumm);
                // Write self copy functions here..

                WO_REINSTANCE(dumm->unpacked_pack);

                return dumm;
            }

            void update_constant_value(lexer* lex) override
            {
                // DO NOTHING
            }
        };

        struct ast_value_unary : virtual public ast_value
        {
            lex_type operate = +lex_type::l_error;
            ast_value* val;

            ast_value_funccall* overrided_operation_call = nullptr;

            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
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
                if (is_constant)
                    return;

                val->update_constant_value(lex);
                if (val->is_constant)
                {
                    is_constant = true;

                    if (operate == +lex_type::l_sub)
                    {
                        value_type = val->value_type;
                        auto _rval = val->get_constant_value();
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
                            lex->lang_error(0x0000, this, WO_ERR_TYPE_CANNOT_NEGATIVE, val->value_type->get_type_name().c_str());
                            return;
                        }
                    }
                    else /*if(_token.type == +lex_type::l_lnot)*/
                    {
                        if (val->value_type->is_bool())
                        {
                            value_type = new ast_type(L"bool");
                            constant_value.set_integer(!val->get_constant_value().handle);
                        }
                        else
                            is_constant = false;
                    }
                }
            }
        };

        struct ast_mapping_pair : virtual public grammar::ast_base
        {
            ast_value* key;
            ast_value* val;
            ast_mapping_pair(ast_value* _k, ast_value* _v)
                : key(_k), val(_v)
            {
                wo_assert(key && val);
            }
            ast_mapping_pair() {}
            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
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

        struct ast_using_namespace : virtual public grammar::ast_base
        {
            bool from_global_namespace;
            std::vector<std::wstring> used_namespace_chain;

            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // ast_value::instance(dumm);
                // Write self copy functions here..

                return dumm;
            }
        };

        struct ast_enum_item : virtual public grammar::ast_base
        {
            std::wstring enum_ident;
            wo_integer_t enum_val;
            bool need_assign_val = true;

            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // ast_value::instance(dumm);
                // Write self copy functions here..

                return dumm;
            }
        };

        struct ast_enum_items_list : virtual public grammar::ast_base
        {
            wo_integer_t next_enum_val = 0;
            std::vector<ast_enum_item*> enum_items;

            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
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

        struct ast_check_type_with_naming_in_pass2 : virtual public grammar::ast_base
        {
            ast_type* template_type;
            ast_type* naming_const;

            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // ast_defines::instance(dumm);
                // Write self copy functions here..

                WO_REINSTANCE(dumm->template_type);
                WO_REINSTANCE(dumm->naming_const);

                return dumm;
            }
        };

        struct ast_using_type_as : virtual public ast_defines
        {
            std::wstring new_type_identifier;
            ast_type* old_type;

            std::map<std::wstring, ast::ast_value*> class_const_index_typing;
            std::map<std::wstring, std::vector<ast::ast_value_function_define*>> class_methods_list;

            ast_list* naming_check_list = nullptr;

            bool is_alias = false;

            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_defines::instance(dumm);
                // Write self copy functions here..

                WO_REINSTANCE(dumm->old_type);

                return dumm;
            }
        };

        struct ast_directed_values : virtual public grammar::ast_base
        {
            ast_value* from;
            ast_value* direct_val;

            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
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

        struct ast_nop : virtual public grammar::ast_base
        {
            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // Write self copy functions here..
                return dumm;
            }

        };

        struct ast_foreach : virtual public grammar::ast_base
        {
            std::vector<ast_value_takeplace*> foreach_patterns_vars_in_pass2;
            ast_value_variable* iterator_var;

            ast_varref_defines* used_iter_define; // Just used for taking place;;;
            ast_varref_defines* used_vawo_defines; // Just used for taking place;;;

            ast_value_funccall* iter_getting_funccall;  // Used for get iter's type. it will used for getting func symbol;;
            ast_value_funccall* iter_next_judge_expr;   // cannot make instance, will instance in pass2

            grammar::ast_base* execute_sentences;

            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // ast_defines::instance(dumm);
                // Write self copy functions here..

                for (auto& vptr : dumm->foreach_patterns_vars_in_pass2)
                {
                    WO_REINSTANCE(vptr);
                }
                WO_REINSTANCE(dumm->iterator_var);
                WO_REINSTANCE(dumm->used_iter_define);
                WO_REINSTANCE(dumm->used_vawo_defines);
                WO_REINSTANCE(dumm->iter_getting_funccall);
                WO_REINSTANCE(dumm->iter_next_judge_expr);
                WO_REINSTANCE(dumm->execute_sentences);
                return dumm;
            }
        };

        struct ast_forloop : virtual public grammar::ast_base
        {
            grammar::ast_base* pre_execute;
            ast_value* judgement_expr;
            ast_value* after_execute;
            grammar::ast_base* execute_sentences;

            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
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

        struct ast_break : virtual public grammar::ast_base
        {
            std::wstring label;

            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // ast_defines::instance(dumm);
                // Write self copy functions here..

                return dumm;
            }
        };

        struct ast_continue : virtual public grammar::ast_base
        {
            std::wstring label;

            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // ast_defines::instance(dumm);
                // Write self copy functions here..

                return dumm;
            }
        };

        struct ast_template_define_with_naming : virtual public grammar::ast_base
        {
            std::wstring template_ident;
            ast_type* naming_const;

            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // ast_defines::instance(dumm);
                // Write self copy functions here..

                WO_REINSTANCE(dumm->naming_const);

                return dumm;
            }
        };

        struct ast_union_item : virtual public grammar::ast_base
        {
            std::wstring identifier;
            ast_type* type_may_nil = nullptr;
            ast_type* gadt_out_type_may_nil = nullptr;

            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
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

        struct ast_union_make_option_ob_to_cr_and_ret : virtual public grammar::ast_base
        {
            uint16_t id;
            ast_value_variable* argument_may_nil;

            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
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

        struct ast_pattern_identifier : virtual public ast_pattern_base
        {
            identifier_decl decl = identifier_decl::IMMUTABLE;

            std::wstring identifier;
            lang_symbol* symbol = nullptr;
            ast_decl_attribute* attr = nullptr;

            std::vector<std::wstring> template_arguments;

            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
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

            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
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

            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
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
        struct ast_match_case_base : virtual public grammar::ast_base
        {
            ast_sentence_block* in_case_sentence;
            ast_match* in_match;
            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
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

            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_match_case_base::instance(dumm);
                // Write self copy functions here..

                WO_REINSTANCE(dumm->union_pattern);

                return dumm;
            }
        };


        struct ast_match : virtual public grammar::ast_base
        {
            ast_value* match_value;
            ast_list* cases;

            std::string match_end_tag_in_final_pass;
            lang_scope* match_scope_in_pass;
            bool has_using_namespace = false;

            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
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

        struct ast_struct_member_define : virtual public grammar::ast_base
        {
            std::wstring member_name;
            ast_value* member_val_or_type_tkplace;

            uint16_t member_offset;

            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // ast_defines::instance(dumm);
                // Write self copy functions here..

                WO_REINSTANCE(dumm->member_val_or_type_tkplace);

                return dumm;
            }
        };

        struct ast_value_make_struct_instance : virtual public ast_value
        {
            ast_list* struct_member_vals;
            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_value::instance(dumm);
                // Write self copy functions here..

                WO_REINSTANCE(dumm->struct_member_vals);

                return dumm;
            }

            void update_constant_value(lexer* lex) override
            {
                // DO NOTHING
            }
        };

        struct ast_value_make_tuple_instance : virtual public ast_value
        {
            ast_list* tuple_member_vals;
            ast_value_make_tuple_instance()
            {
                value_type = new ast_type(L"pending");
            }
            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_value::instance(dumm);
                // Write self copy functions here..

                WO_REINSTANCE(dumm->tuple_member_vals);

                return dumm;
            }

            void update_constant_value(lexer* lex) override
            {
                // DO NOTHING
            }
        };

        struct ast_value_trib_expr : virtual public ast_value
        {
            ast_value* judge_expr;
            ast_value* val_if_true;
            ast_value* val_or;

            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
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
                if (is_constant)
                    return;

                judge_expr->update_constant_value(lex);
                if (judge_expr->is_constant)
                {
                    if (judge_expr->get_constant_value().integer)
                    {
                        val_if_true->update_constant_value(lex);
                        if (val_if_true->is_constant)
                        {
                            is_constant = true;
                            constant_value.set_val(&val_if_true->get_constant_value());
                        }
                    }
                    else
                    {
                        val_or->update_constant_value(lex);
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

#define WO_NEED_TOKEN(ID) [&]() {             \
    token tk = {lex_type::l_error};           \
    if (!cast_any_to<token>(input[(ID)], tk)) \
        wo_error("Unexcepted token type.");   \
    return tk;                                \
}()
#define WO_NEED_AST(ID) [&]() {                     \
    ast_basic *nd = nullptr;                        \
    if (!cast_any_to<ast_basic *>(input[(ID)], nd)) \
        wo_error("Unexcepted ast-node type.");      \
    return nd;                                      \
}()

#define WO_IS_TOKEN(ID) [&]() {               \
    token tk = {lex_type::l_error};           \
    if (!cast_any_to<token>(input[(ID)], tk)) \
        return false;                         \
    return true;                              \
}()
#define WO_IS_AST(ID) [&]() {                       \
    ast_basic *nd = nullptr;                        \
    if (!cast_any_to<ast_basic *>(input[(ID)], nd)) \
        return false;                               \
    return true;                                    \
}()

        template <size_t pass_idx>
        struct pass_direct : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                wo_test(input.size() > pass_idx);
                return input[pass_idx];
            }
        };

        struct pass_typeof : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                auto* att = new ast_type(L"pending");
                att->typefrom = dynamic_cast<ast_value*>(WO_NEED_AST(2));

                wo_assert(att->typefrom);

                return (ast_basic*)att;
            }

        };

        struct pass_template_reification : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
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

        struct pass_decl_attrib_begin : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                auto att = new ast_decl_attribute;
                att->add_attribute(&lex, dynamic_cast<ast_token*>(WO_NEED_AST(0))->tokens.type);
                return (ast_basic*)att;
            }
        };

        struct pass_enum_item_create : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                ast_enum_item* item = new ast_enum_item;
                item->enum_ident = WO_NEED_TOKEN(0).identifier;
                if (input.size() == 3)
                {
                    item->need_assign_val = false;
                    auto fxxk = WO_NEED_TOKEN(2);
                    item->enum_val = ast_value_literal::wstr_to_integer(WO_NEED_TOKEN(2).identifier);
                }
                else if (input.size() == 4)
                {
                    item->need_assign_val = false;
                    auto fxxk = WO_NEED_TOKEN(3);
                    if (WO_NEED_TOKEN(2).type == +lex_type::l_add)
                        item->enum_val = ast_value_literal::wstr_to_integer(WO_NEED_TOKEN(3).identifier);
                    else if (WO_NEED_TOKEN(2).type == +lex_type::l_sub)
                        item->enum_val = -ast_value_literal::wstr_to_integer(WO_NEED_TOKEN(3).identifier);
                    else
                        wo_error("Enum item should be +/- integer");
                }
                return (ast_basic*)item;
            }
        };

        struct pass_enum_declear_begin : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
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
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
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

        struct pass_mark_value_as_ref : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                // MAY_REF_FACTOR_TYPE_CASTING -> 4
                ast_value* val = input.size() == 4 ? dynamic_cast<ast_value*>(WO_NEED_AST(2)) : dynamic_cast<ast_value*>(WO_NEED_AST(1));

                if (!val->can_be_assign)
                {
                    lex.parser_error(0x0000, WO_ERR_CANNOT_MAKE_UNASSABLE_ITEM_REF);
                }

                val->is_mark_as_using_ref = true;
                return (ast_basic*)val;
            }
        };

        struct pass_enum_finalize : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                wo_assert(input.size() == 6);
                ast_decl_attribute* union_arttribute = dynamic_cast<ast_decl_attribute*>(WO_NEED_AST(0));

                ast_list* bind_type_and_decl_list = new ast_list;

                ast_namespace* enum_scope = new ast_namespace;
                enum_scope->copy_source_info(union_arttribute);
                ast_list* decl_list = new ast_list;

                // TODO: Enum attribute should be apply here!
                //       WO_NEED_AST(0)

                enum_scope->scope_name = WO_NEED_TOKEN(2).identifier;

                auto* using_enum_as_int = new ast_using_type_as;
                using_enum_as_int->new_type_identifier = enum_scope->scope_name;
                using_enum_as_int->old_type = new ast_type(L"int");
                bind_type_and_decl_list->append_at_end(using_enum_as_int);

                enum_scope->in_scope_sentence = decl_list;
                ast_enum_items_list* enum_items = dynamic_cast<ast_enum_items_list*>(WO_NEED_AST(4));

                ast_varref_defines* vardefs = new ast_varref_defines;
                vardefs->declear_attribute = union_arttribute;
                wo_assert(vardefs->declear_attribute);

                for (auto& enumitem : enum_items->enum_items)
                {
                    ast_value_literal* const_val = new ast_value_literal(
                        token{ +lex_type::l_literal_integer, std::to_wstring(enumitem->enum_val) });

                    auto* define_enum_item = new ast_pattern_identifier;
                    define_enum_item->identifier = enumitem->enum_ident;
                    define_enum_item->attr = new ast_decl_attribute();
                    define_enum_item->attr->add_attribute(&lex, +lex_type::l_const);

                    vardefs->var_refs.push_back(
                        { define_enum_item, const_val });

                    // TODO: DATA TYPE SYSTEM..
                    const_val->value_type = new ast_type(enum_scope->scope_name);
                }

                decl_list->append_at_end(vardefs);
                bind_type_and_decl_list->append_at_end(enum_scope);
                return (ast_basic*)bind_type_and_decl_list;
            }
        };

        struct pass_append_attrib : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                auto att = dynamic_cast<ast_decl_attribute*>(WO_NEED_AST(0));
                att->add_attribute(&lex, dynamic_cast<ast_token*>(WO_NEED_AST(1))->tokens.type);
                return (ast_basic*)att;
            }
        };

        struct pass_unary_op : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                wo_test(input.size() == 2);

                token _token = WO_NEED_TOKEN(0);
                ast_value* right_v = dynamic_cast<ast_value*>(WO_NEED_AST(1));

                wo_test(right_v);
                wo_test(lexer::lex_is_operate_type(_token.type) && (_token.type == +lex_type::l_lnot || _token.type == +lex_type::l_sub));

                ast_value_unary* vbin = new ast_value_unary();
                vbin->operate = _token.type;
                vbin->val = right_v;

                vbin->update_constant_value(&lex);

                /*
                 // In ast build pass, all left value's type cannot judge, so it was useless..

                ast_type* result_type = new ast_type(L"pending");
                result_type->set_type(left_v->value_type);

                vbin->value_type = result_type;
                */
                return (grammar::ast_base*)vbin;
            }
        };

        struct pass_import_files : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                wo_test(input.size() == 2);
                std::wstring path;
                std::wstring filename;

                ast_token* importfilepaths = dynamic_cast<ast_token*>(
                    dynamic_cast<ast_list*>(WO_NEED_AST(1))->children);
                do
                {
                    path += filename = importfilepaths->tokens.identifier;
                    importfilepaths = dynamic_cast<ast_token*>(importfilepaths->sibling);
                    if (importfilepaths)
                        path += L"/";
                } while (importfilepaths);

                // path += L".wo";
                std::wstring srcfile, src_full_path;
                if (!wo::read_virtual_source(&srcfile, &src_full_path, path + L".wo", &lex))
                {
                    // import a.b; cannot open a/b.wo, trying a/b/b.wo
                    if (!wo::read_virtual_source(&srcfile, &src_full_path, path + L"/" + filename + L".wo", &lex))
                        return lex.parser_error(0x0000, WO_ERR_CANNOT_OPEN_FILE, path.c_str());
                }

                if (!lex.has_been_imported(src_full_path))
                {
                    lexer new_lex(srcfile, wstr_to_str(src_full_path));
                    new_lex.imported_file_list = lex.imported_file_list;
                    new_lex.used_macro_list = lex.used_macro_list;

                    auto* imported_ast = wo::get_wo_grammar()->gen(new_lex);

                    lex.used_macro_list = new_lex.used_macro_list;

                    lex.lex_error_list.insert(lex.lex_error_list.end(),
                        new_lex.lex_error_list.begin(),
                        new_lex.lex_error_list.end());

                    lex.imported_file_list = new_lex.imported_file_list;

                    if (imported_ast)
                    {
                        imported_ast->add_child(new ast_nop); // nop for debug info gen, avoid ip/cr confl..
                        return (ast_basic*)imported_ast;
                    }

                    return +lex_type::l_error;
                }
                return (ast_basic*)new ast_empty();
            }
        };

        struct pass_mapping_pair : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                wo_test(input.size() == 5);
                // { x , x }

                return (grammar::ast_base*)new ast_mapping_pair(
                    dynamic_cast<ast_value*>(WO_NEED_AST(1)),
                    dynamic_cast<ast_value*>(WO_NEED_AST(3)));
            }
        };

        struct pass_unpack_args : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                wo_test(input.size() == 2 || input.size() == 3);

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
                        lex.parser_error(0x0000, WO_ERR_UNPACK_ARG_LESS_THEN_ONE);

                    return (ast_basic*)new ast_fakevalue_unpacked_args(
                        dynamic_cast<ast_value*>(WO_NEED_AST(0)),
                        expand_count);
                }
            }
        };

        struct pass_pack_variadic_args : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                return (ast_basic*)new ast_value_packed_variadic_args;
            }
        };
        struct pass_extern : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                ast_extern_info* extern_symb = new ast_extern_info;
                if (input.size() == 4)
                {
                    extern_symb->symbol_name = WO_NEED_TOKEN(2).identifier;
                    extern_symb->externed_func =
                        rslib_extern_symbols::get_global_symbol(
                            wstr_to_str(extern_symb->symbol_name).c_str());

                    if (!extern_symb->externed_func)
                        lex.parser_error(0x0000, WO_ERR_CANNOT_FIND_EXT_SYM, extern_symb->symbol_name.c_str());
                }
                else if (input.size() == 6)
                {
                    // extern ( lib , symb )
                    extern_symb->load_from_lib = WO_NEED_TOKEN(2).identifier;
                    extern_symb->symbol_name = WO_NEED_TOKEN(4).identifier;
                    extern_symb->externed_func = nullptr;

                    // Load it in pass
                }
                else
                {
                    wo_error("error grammar..");
                }

                return (ast_basic*)extern_symb;
            }
        };
        struct pass_while : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                wo_test(input.size() == 5);
                return (grammar::ast_base*)new ast_while(dynamic_cast<ast_value*>(WO_NEED_AST(2)), WO_NEED_AST(4));
            }
        };
        struct pass_except : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                wo_test(input.size() == 2);
                return (grammar::ast_base*)new ast_except(WO_NEED_AST(1));
            }
        };
        struct pass_if : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                wo_test(input.size() == 6);
                if (ast_empty::is_empty(input[5]))
                    return (grammar::ast_base*)new ast_if(dynamic_cast<ast_value*>(WO_NEED_AST(2)), WO_NEED_AST(4), nullptr);
                else
                    return (grammar::ast_base*)new ast_if(dynamic_cast<ast_value*>(WO_NEED_AST(2)), WO_NEED_AST(4), WO_NEED_AST(5));
            }
        };

        template <size_t pass_idx>
        struct pass_sentence_block : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                wo_test(input.size() > pass_idx);
                return (grammar::ast_base*)ast_sentence_block::fast_parse_sentenceblock(WO_NEED_AST(pass_idx));
            }
        };

        struct pass_empty_sentence_block : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                return (grammar::ast_base*)ast_sentence_block::fast_parse_sentenceblock(new ast_empty);
            }
        };

        struct pass_map_builder : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                wo_test(input.size() == 3);
                return (ast_basic*)new ast_value_mapping(dynamic_cast<ast_list*>(WO_NEED_AST(1)));
            }
        };

        struct pass_array_builder : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                wo_test(input.size() == 3);
                return (ast_basic*)new ast_value_array(dynamic_cast<ast_list*>(WO_NEED_AST(1)));
            }
        };

        struct pass_function_define : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                auto* ast_func = new ast_value_function_define;
                ast_type* return_type = nullptr;
                ast_list* template_types = nullptr;

                if (input.size() == 10)
                {
                    // function with name..
                    ast_func->declear_attribute = dynamic_cast<ast_decl_attribute*>(WO_NEED_AST(0));
                    wo_assert(ast_func->declear_attribute);

                    ast_func->function_name = WO_NEED_TOKEN(2).identifier;

                    if (!ast_empty::is_empty(input[3]))
                        template_types = dynamic_cast<ast_list*>(WO_NEED_AST(3));

                    ast_func->argument_list = dynamic_cast<ast_list*>(WO_NEED_AST(5));

                    return_type = dynamic_cast<ast_type*>(WO_NEED_AST(7));
                    if (!ast_empty::is_empty(input[8]))
                    {
                        ast_func->where_constraint = dynamic_cast<ast_where_constraint*>(WO_NEED_AST(8));
                        wo_assert(ast_func->where_constraint);
                    }

                    ast_func->in_function_sentence = dynamic_cast<ast_sentence_block*>(WO_NEED_AST(9))->sentence_list;
                }
                else if (input.size() == 9)
                {
                    // anonymous function
                    ast_func->declear_attribute = dynamic_cast<ast_decl_attribute*>(WO_NEED_AST(0));
                    wo_assert(ast_func->declear_attribute);

                    if (!ast_empty::is_empty(input[2]))
                    {
                        wo_error("Not support now...");

                        template_types = dynamic_cast<ast_list*>(WO_NEED_AST(2));
                    }

                    ast_func->function_name = L""; // just get a fucking name
                    ast_func->argument_list = dynamic_cast<ast_list*>(WO_NEED_AST(4));

                    return_type = dynamic_cast<ast_type*>(WO_NEED_AST(6));
                    if (!ast_empty::is_empty(input[7]))
                    {
                        ast_func->where_constraint = dynamic_cast<ast_where_constraint*>(WO_NEED_AST(7));
                        wo_assert(ast_func->where_constraint);
                    }

                    ast_func->in_function_sentence = dynamic_cast<ast_sentence_block*>(WO_NEED_AST(8))->sentence_list;
                }
                else if (input.size() == 11)
                {
                    if (WO_IS_TOKEN(2) && WO_NEED_TOKEN(2).type == +lex_type::l_operator)
                    {
                        // operator function
                        ast_func->declear_attribute = dynamic_cast<ast_decl_attribute*>(WO_NEED_AST(0));
                        wo_assert(ast_func->declear_attribute);

                        ast_func->function_name = L"operator " + dynamic_cast<ast_token*>(WO_NEED_AST(3))->tokens.identifier;

                        if (!ast_empty::is_empty(input[4]))
                            template_types = dynamic_cast<ast_list*>(WO_NEED_AST(4));

                        ast_func->argument_list = dynamic_cast<ast_list*>(WO_NEED_AST(6));
                        // Check argument list, do not allowed operator overload function have 'ref' arguments
                        auto arguments = ast_func->argument_list->children;
                        while (arguments)
                        {
                            auto* argdef = dynamic_cast<ast_value_arg_define*>(arguments);
                            if (argdef->decl == identifier_decl::REF)
                                lex.lex_error(0x0000, WO_ERR_REF_ARG_IN_OPERATOR_OVERLOAD_FUNC,
                                    dynamic_cast<ast_token*>(WO_NEED_AST(3))->tokens.identifier.c_str());
                            arguments = arguments->sibling;
                        }
                        return_type = dynamic_cast<ast_type*>(WO_NEED_AST(8));
                        if (!ast_empty::is_empty(input[9]))
                        {
                            ast_func->where_constraint = dynamic_cast<ast_where_constraint*>(WO_NEED_AST(9));
                            wo_assert(ast_func->where_constraint);
                        }

                        ast_func->in_function_sentence = dynamic_cast<ast_sentence_block*>(WO_NEED_AST(10))->sentence_list;
                    }
                    else
                    {
                        // function with name.. export func
                        ast_func->declear_attribute = dynamic_cast<ast_decl_attribute*>(WO_NEED_AST(1));
                        wo_assert(ast_func->declear_attribute);

                        ast_func->function_name = WO_NEED_TOKEN(3).identifier;

                        if (!ast_empty::is_empty(input[4]))
                        {
                            template_types = dynamic_cast<ast_list*>(WO_NEED_AST(4));
                        }

                        ast_func->argument_list = dynamic_cast<ast_list*>(WO_NEED_AST(6));
                        ast_func->in_function_sentence = nullptr;

                        return_type = dynamic_cast<ast_type*>(WO_NEED_AST(8));
                        if (!ast_empty::is_empty(input[9]))
                        {
                            ast_func->where_constraint = dynamic_cast<ast_where_constraint*>(WO_NEED_AST(9));
                            wo_assert(ast_func->where_constraint);
                        }

                        ast_func->externed_func_info = dynamic_cast<ast_extern_info*>(WO_NEED_AST(0));
                        if (ast_func->externed_func_info->externed_func)
                            ast_func->is_constant = true;
                    }
                }
                else if (input.size() == 12)
                {
                    // export func
                    ast_func->declear_attribute = dynamic_cast<ast_decl_attribute*>(WO_NEED_AST(1));
                    wo_assert(ast_func->declear_attribute);

                    ast_func->function_name = L"operator " + dynamic_cast<ast_token*>(WO_NEED_AST(4))->tokens.identifier;

                    if (!ast_empty::is_empty(input[5]))
                    {
                        template_types = dynamic_cast<ast_list*>(WO_NEED_AST(5));
                    }

                    ast_func->argument_list = dynamic_cast<ast_list*>(WO_NEED_AST(7));
                    // Check argument list, do not allowed operator overload function have 'ref' arguments
                    auto arguments = ast_func->argument_list->children;
                    while (arguments)
                    {
                        auto* argdef = dynamic_cast<ast_value_arg_define*>(arguments);
                        if (argdef->decl == identifier_decl::REF)
                            lex.lex_error(0x0000, WO_ERR_REF_ARG_IN_OPERATOR_OVERLOAD_FUNC,
                                dynamic_cast<ast_token*>(WO_NEED_AST(4))->tokens.identifier.c_str());
                        arguments = arguments->sibling;
                    }

                    ast_func->in_function_sentence = nullptr;
                    return_type = dynamic_cast<ast_type*>(WO_NEED_AST(9));
                    if (!ast_empty::is_empty(input[10]))
                    {
                        ast_func->where_constraint = dynamic_cast<ast_where_constraint*>(WO_NEED_AST(10));
                        wo_assert(ast_func->where_constraint);
                    }

                    ast_func->externed_func_info = dynamic_cast<ast_extern_info*>(WO_NEED_AST(0));
                    if (ast_func->externed_func_info->externed_func)
                        ast_func->is_constant = true;
                }
                else if (input.size() == 7)
                {
                    // \ <> ARGS  = EXPR where;
                    // 0 1   2    3    4  5 
                    // anonymous function
                    ast_func->declear_attribute = new ast_decl_attribute();
                    wo_assert(ast_func->declear_attribute);

                    if (!ast_empty::is_empty(input[1]))
                    {
                        wo_error("Not support now...");
                        template_types = dynamic_cast<ast_list*>(WO_NEED_AST(1));
                    }

                    ast_func->function_name = L""; // just get a fucking name
                    ast_func->argument_list = dynamic_cast<ast_list*>(WO_NEED_AST(2));

                    return_type = nullptr;

                    ast_sentence_block* sentences = dynamic_cast<ast_sentence_block*>(WO_NEED_AST(4));
                    wo_assert(sentences);

                    ast_func->in_function_sentence = sentences->sentence_list;
                    wo_assert(ast_func->in_function_sentence);

                    if (ast_varref_defines* where_decls = dynamic_cast<ast_varref_defines*>(WO_NEED_AST(5)))
                    {
                        // Inverse where_decls
                        std::reverse(where_decls->var_refs.begin(), where_decls->var_refs.end());

                        sentences->sentence_list->append_at_head(where_decls);
                    }
                }
                else
                    wo_error("Unknown ast type.");
                // many things to do..

                if (return_type)
                {
                    ast_func->value_type = return_type;
                    ast_func->auto_adjust_return_type = false;
                }
                else
                {
                    ast_func->value_type = new ast_type(L"pending");
                    ast_func->auto_adjust_return_type = true;
                }

                if (ast_func->value_type->is_func())
                {
                    ast_func->value_type = new ast_type(ast_func->value_type); // complex type;;
                }

                ast_func->value_type->set_as_function_type();
                auto* argchild = ast_func->argument_list->children;
                while (argchild)
                {
                    if (auto* arg_node = dynamic_cast<ast_value_arg_define*>(argchild))
                    {
                        if (ast_func->value_type->is_variadic_function_type)
                            return lex.parser_error(0x0000, WO_ERR_ARG_DEFINE_AFTER_VARIADIC);

                        ast_func->value_type->append_function_argument_type(arg_node->value_type);
                    }
                    else if (dynamic_cast<ast_token*>(argchild))
                        ast_func->value_type->set_as_variadic_arg_func();
                    argchild = argchild->sibling;
                }

                ast_list* template_const_list = new ast_list;

                if (template_types)
                {
                    ast_func->is_template_define = true;
                    ast_template_define_with_naming* template_type = dynamic_cast<ast_template_define_with_naming*>(template_types->children);
                    wo_assert(template_type);

                    while (template_type)
                    {
                        ast_func->template_type_name_list.push_back(template_type->template_ident);

                        if (template_type->naming_const)
                        {
                            ast_check_type_with_naming_in_pass2* acheck = new ast_check_type_with_naming_in_pass2;
                            acheck->template_type = new ast_type(template_type->template_ident);
                            acheck->naming_const = new ast_type(L"pending");
                            acheck->naming_const->set_type(template_type->naming_const);
                            acheck->row_no = template_type->row_no;
                            acheck->col_no = template_type->col_no;
                            acheck->source_file = template_type->source_file;
                            template_const_list->append_at_end(acheck);
                        }

                        template_type = dynamic_cast<ast_template_define_with_naming*>(template_type->sibling);
                    }
                }

                if (ast_func->in_function_sentence)
                    ast_func->in_function_sentence->append_at_head(template_const_list);
                else
                    ast_func->in_function_sentence = template_const_list;

                // if ast_func->in_function_sentence == nullptr it means this function have no sentences...
                return (grammar::ast_base*)ast_func;
            }
        };

        struct pass_function_call : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                wo_test(input.size() == 4);

                auto* result = new ast_value_funccall;

                result->arguments = dynamic_cast<ast_list*>(WO_NEED_AST(2));

                if (ast_directed_values* adv = dynamic_cast<ast_directed_values*>(WO_NEED_AST(0)))
                {
                    result->called_func = adv->direct_val;
                    result->arguments->append_at_head(adv->from);
                    result->directed_value_from = adv->from;
                }
                else
                    result->called_func = dynamic_cast<ast_value*>(WO_NEED_AST(0));

                result->value_type = new ast_type(L"pending");
                //  result->called_func->value_type->get_return_type(); // just get pending..
                result->can_be_assign = true;
                return (ast_basic*)result;
            }
        };

        struct pass_directed_value_for_call : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                wo_test(input.size() == 3);

                auto* result = new ast_directed_values();
                auto* from = dynamic_cast<ast_value*>(WO_NEED_AST(0));
                auto* to = dynamic_cast<ast_value*>(WO_NEED_AST(2));
                result->from = from;
                result->direct_val = to;

                return (ast_basic*)result;
            }
        };

        struct pass_literal : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                wo_test(input.size() == 1);
                return (grammar::ast_base*)new ast_value_literal(WO_NEED_TOKEN(0));
            }
        };

        struct pass_namespace : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                wo_test(input.size() == 3);
                if (ast_empty::is_empty(input[2]))
                    return input[2];

                ast_namespace* last_namespace = nullptr;
                ast_namespace* output_namespace = nullptr;
                auto* space_name_list = WO_NEED_AST(1)->children;
                while (space_name_list)
                {
                    auto* name = dynamic_cast<ast_token*>(space_name_list);
                    wo_assert(name);

                    ast_namespace* result = new ast_namespace();
                    result->scope_name = name->tokens.identifier;
                    result->copy_source_info(name);

                    if (last_namespace)
                    {
                        last_namespace->in_scope_sentence = new ast_list;
                        last_namespace->in_scope_sentence->append_at_end(result);
                    }
                    else
                        output_namespace = result;

                    last_namespace = result;

                    space_name_list = space_name_list->sibling;
                }
                auto* list = dynamic_cast<ast_sentence_block*>(WO_NEED_AST(2));
                wo_test(list);
                last_namespace->in_scope_sentence = list->sentence_list;

                return (ast_basic*)output_namespace;
            }
        };

        struct pass_begin_varref_define : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                wo_test(input.size() == 4);
                ast_varref_defines* result = new ast_varref_defines;

                ast_value* init_val = dynamic_cast<ast_value*>(WO_NEED_AST(3));
                wo_test(init_val);

                auto* define_varref = dynamic_cast<ast_pattern_base*>(WO_NEED_AST(0));
                wo_assert(define_varref);
                if (auto* pattern_identifier = dynamic_cast<ast_pattern_identifier*>(define_varref))
                {
                    auto* template_def_item = WO_NEED_AST(1)->children;
                    while (template_def_item)
                    {
                        auto* template_def = dynamic_cast<ast_template_define_with_naming*>(template_def_item);
                        wo_assert(template_def);

                        pattern_identifier->template_arguments.push_back(template_def->template_ident);

                        template_def_item = template_def_item->sibling;
                    }
                }


                result->var_refs.push_back({ define_varref, init_val });

                return (ast_basic*)result;
            }
        };
        struct pass_add_varref_define : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                wo_test(input.size() == 6);
                ast_varref_defines* result = dynamic_cast<ast_varref_defines*>(WO_NEED_AST(0));

                ast_value* init_val = dynamic_cast<ast_value*>(WO_NEED_AST(5));
                wo_test(result && init_val);

                auto* define_varref = dynamic_cast<ast_pattern_base*>(WO_NEED_AST(2));
                wo_assert(define_varref);
                if (auto* pattern_identifier = dynamic_cast<ast_pattern_identifier*>(define_varref))
                {
                    auto* template_def_item = WO_NEED_AST(3)->children;
                    while (template_def_item)
                    {
                        auto* template_def = dynamic_cast<ast_template_define_with_naming*>(template_def_item);
                        wo_assert(template_def);

                        pattern_identifier->template_arguments.push_back(template_def->template_ident);

                        template_def_item = template_def_item->sibling;
                    }
                }
                result->var_refs.push_back({ define_varref, init_val });

                return (ast_basic*)result;
            }
        };
        struct pass_mark_as_var_define : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                wo_test(input.size() == 3);
                ast_varref_defines* result = dynamic_cast<ast_varref_defines*>(WO_NEED_AST(2));
                wo_test(result);

                result->declear_attribute = dynamic_cast<ast_decl_attribute*>(WO_NEED_AST(0));
                wo_assert(result->declear_attribute);

                return (ast_basic*)result;
            }
        };
        struct pass_trans_where_decl_in_lambda : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                wo_test(input.size() == 2);
                ast_varref_defines* result = dynamic_cast<ast_varref_defines*>(WO_NEED_AST(1));
                wo_test(result);

                result->declear_attribute = new ast_decl_attribute();

                return (ast_basic*)result;
            }
        };

        /*struct pass_type_decl :public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                wo_test(input.size() == 2);

                token tk = WO_NEED_TOKEN(1);

                if (tk.type == +lex_type::l_identifier)
                {
                    return (grammar::ast_base*)new ast_type(tk.identifier);
                }

                wo_error("Unexcepted token type.");
                return 0;
            }
        };*/

        struct pass_type_cast : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                wo_test(input.size() == 2);

                ast_value* value_node;
                ast_type* type_node;
                if ((value_node = dynamic_cast<ast_value*>(WO_NEED_AST(0))) && (type_node = dynamic_cast<ast_type*>(WO_NEED_AST(1))))
                {
                    if (type_node->is_array())
                        if (auto* array_builder = dynamic_cast<ast_value_array*>(value_node))
                        {
                            array_builder->value_type = type_node;
                            return (ast_basic*)array_builder;
                        }
                    if (type_node->is_map())
                        if (auto* map_builder = dynamic_cast<ast_value_mapping*>(value_node))
                        {
                            map_builder->value_type = type_node;
                            return (ast_basic*)map_builder;
                        }

                    ast_value_type_cast* typecast = new ast_value_type_cast(value_node, type_node);
                    typecast->update_constant_value(&lex);
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
                    lex.parser_error(0x0000, WO_ERR_CANNOT_AS_TYPE, value_node->value_type->get_type_name().c_str(), type_node->get_type_name().c_str());
                    return value_node;
                }
                return value_node;
            }

            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                wo_test(input.size() == 2);

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
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                wo_test(input.size() == 2);

                ast_value* value_node;
                ast_type* type_node;
                if ((value_node = dynamic_cast<ast_value*>(WO_NEED_AST(0))))
                {
                    if ((type_node = dynamic_cast<ast_type*>(WO_NEED_AST(1))))
                    {
                        ast_value_type_check* checking_node = new ast_value_type_check(value_node, type_node);
                        checking_node->update_constant_value(&lex);
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
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                wo_test(input.size() == 1);

                token tk = WO_NEED_TOKEN(0);

                wo_test(tk.type == +lex_type::l_identifier);
                return (grammar::ast_base*)new ast_value_variable(tk.identifier);
            }
        };

        struct pass_append_serching_namespace : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                wo_test(input.size() == 3);

                token tk = WO_NEED_TOKEN(1);
                ast_value_variable* result = dynamic_cast<ast_value_variable*>(WO_NEED_AST(2));

                wo_assert(tk.type == +lex_type::l_identifier && result);

                result->scope_namespaces.insert(result->scope_namespaces.begin(), tk.identifier);

                return (grammar::ast_base*)result;
            }
        };

        struct pass_using_namespace : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                ast_using_namespace* aunames = new ast_using_namespace();
                auto vs = dynamic_cast<ast_value_variable*>(WO_NEED_AST(2));
                wo_assert(vs);

                aunames->from_global_namespace = vs->search_from_global_namespace;

                for (auto& space : vs->scope_namespaces)
                    aunames->used_namespace_chain.push_back(space);
                aunames->used_namespace_chain.push_back(vs->var_name);

                return (grammar::ast_base*)aunames;
            }
        };

        struct pass_finalize_serching_namespace : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                wo_test(input.size() == 2);

                if (WO_IS_TOKEN(0))
                {
                    token tk = WO_NEED_TOKEN(0);
                    ast_value_variable* result = dynamic_cast<ast_value_variable*>(WO_NEED_AST(1));

                    wo_assert((tk.type == +lex_type::l_identifier || tk.type == +lex_type::l_empty) && result);
                    if (tk.type == +lex_type::l_identifier)
                    {
                        result->scope_namespaces.insert(result->scope_namespaces.begin(), tk.identifier);
                    }
                    else
                    {
                        result->search_from_global_namespace = true;
                    }
                    return (grammar::ast_base*)result;
                }
                else
                {
                    ast_type* findingfrom = dynamic_cast<ast_type*>(WO_NEED_AST(0));
                    ast_value_variable* result = dynamic_cast<ast_value_variable*>(WO_NEED_AST(1));

                    result->searching_from_type = findingfrom;
                    return (grammar::ast_base*)result;
                }

            }
        };

        struct pass_variable_in_namespace : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                wo_test(input.size() == 2);

                token tk = WO_NEED_TOKEN(1);

                wo_test(tk.type == +lex_type::l_identifier);

                return (grammar::ast_base*)new ast_value_variable(tk.identifier);
            }
        };

        template <size_t first_node>
        struct pass_create_list : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                wo_test(first_node < input.size());

                ast_list* result = new ast_list();
                if (ast_empty::is_empty(input[first_node]))
                    return (grammar::ast_base*)result;

                ast_basic* _node = WO_NEED_AST(first_node);

                result->append_at_end(_node);
                return (grammar::ast_base*)result;
            }
        };

        template <size_t from, size_t to_list>
        struct pass_append_list : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                wo_test(input.size() > std::max(from, to_list));

                ast_list* list = dynamic_cast<ast_list*>(WO_NEED_AST(to_list));
                if (list)
                {
                    if (ast_empty::is_empty(input[from]))
                        return (grammar::ast_base*)list;

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
                    return (grammar::ast_base*)list;
                }
                wo_error("Unexcepted token type, should be 'ast_list' or inherit from 'ast_list'.");
                return 0;
            }
        };

        struct pass_append_list_for_ref_tuple_maker : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                /*
                gm::te(gm::ttype::l_left_brackets), 0
                gm::te(gm::ttype::l_ref), 1
                gm::nt(L"RIGHT"), 2
                gm::te(gm::ttype::l_comma), 3
                gm::nt(L"APPEND_CONSTANT_TUPLE_ITEMS"), 4
                gm::te(gm::ttype::l_right_brackets)
                */
                wo_assert(input.size() == 6);
                ast_list* list = dynamic_cast<ast_list*>(WO_NEED_AST(4));
                if (list)
                {
                    ast_value* val = dynamic_cast<ast_value*>(WO_NEED_AST(2));
                    wo_assert(val);

                    val->is_mark_as_using_ref = true;
                    list->append_at_head(val);

                    return (grammar::ast_base*)list;
                }
                wo_error("Unexcepted token type, should be 'ast_list' or inherit from 'ast_list'.");
                return 0;
            }
        };

        struct pass_make_tuple : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                wo_assert(input.size() == 1);
                ast_value_make_tuple_instance* tuple = new ast_value_make_tuple_instance;

                if (!ast_empty::is_empty(input[0]))
                    tuple->tuple_member_vals = dynamic_cast<ast_list*>(WO_NEED_AST(0));
                else
                    tuple->tuple_member_vals = new ast_list();
                return (grammar::ast_base*)tuple;
            }

        };

        struct pass_empty : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                return (grammar::ast_base*)new ast_empty();
            }
        };

        struct pass_binary_op : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                wo_test(input.size() >= 3);

                ast_value* left_v = dynamic_cast<ast_value*>(WO_NEED_AST(0));
                ast_value* right_v = dynamic_cast<ast_value*>(WO_NEED_AST(2));
                wo_test(left_v && right_v);

                token _token = WO_NEED_TOKEN(1);
                wo_test(lexer::lex_is_operate_type(_token.type));

                // calc type upgrade


                ast_value_binary* vbin = new ast_value_binary();
                vbin->left = left_v;
                vbin->operate = _token.type;
                vbin->right = right_v;

                vbin->update_constant_value(&lex);
                // In ast build pass, all left value's type cannot judge, so it was useless..
                //vbin->value_type = result_type;

                return (grammar::ast_base*)vbin;
            }
        };

        struct pass_assign_op : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                wo_test(input.size() >= 3);

                ast_value* left_v = dynamic_cast<ast_value*>(WO_NEED_AST(0));
                ast_value* right_v = dynamic_cast<ast_value*>(WO_NEED_AST(2));
                wo_test(left_v && right_v);

                token _token = WO_NEED_TOKEN(1);
                wo_test(lexer::lex_is_operate_type(_token.type));

                if (left_v->is_constant)
                    return lex.parser_error(0x0000, WO_ERR_CANNOT_ASSIGN_TO_CONSTANT);

                ast_value_assign* vbin = new ast_value_assign();
                vbin->left = left_v;
                vbin->operate = _token.type;
                vbin->right = right_v;

                /*
                 // In ast build pass, all left value's type cannot judge, so it was useless..

                ast_type* result_type = new ast_type(L"pending");
                result_type->set_type(left_v->value_type);

                vbin->value_type = result_type;
                */
                return (grammar::ast_base*)vbin;
            }
        };

        struct pass_binary_logical_op : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                // TODO Do optmize, like pass_binary_op

                wo_test(input.size() >= 3);

                ast_value* left_v = dynamic_cast<ast_value*>(WO_NEED_AST(0));
                ast_value* right_v = dynamic_cast<ast_value*>(WO_NEED_AST(2));
                wo_test(left_v && right_v);

                token _token = WO_NEED_TOKEN(1);
                wo_test(lexer::lex_is_operate_type(_token.type));

                ast_value_logical_binary* vbin = new ast_value_logical_binary();
                vbin->left = left_v;
                vbin->operate = _token.type;
                vbin->right = right_v;

                /*
                 // In ast build pass, all left value's type cannot judge, so it was useless..

                ast_type* result_type = new ast_type(L"pending");
                result_type->set_type(left_v->value_type);

                vbin->value_type = result_type;
                */
                return (grammar::ast_base*)vbin;
            }
        };

        struct pass_index_op : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                wo_test(input.size() >= 3);

                ast_value* left_v = dynamic_cast<ast_value*>(WO_NEED_AST(0));
                token _token = WO_NEED_TOKEN(1);

                if (_token.type == +lex_type::l_index_begin)
                {
                    ast_value* right_v = dynamic_cast<ast_value*>(WO_NEED_AST(2));
                    wo_test(left_v && right_v);

                    if (dynamic_cast<ast_value_packed_variadic_args*>(left_v))
                    {
                        return (grammar::ast_base*)new ast_value_indexed_variadic_args(right_v);
                    }
                    else
                    {
                        ast_value_index* vbin = new ast_value_index();
                        vbin->from = left_v;
                        vbin->index = right_v;

                        vbin->update_constant_value(&lex);

                        return (grammar::ast_base*)vbin;
                    }
                }
                else if (_token.type == +lex_type::l_index_point)
                {
                    token right_tk = WO_NEED_TOKEN(2);
                    wo_test(left_v && right_tk.type == +lex_type::l_identifier);

                    ast_value_literal* const_result = new ast_value_literal();
                    const_result->value_type = new ast_type(L"string");
                    const_result->constant_value.set_string_nogc(
                        wstr_to_str(right_tk.identifier).c_str());

                    ast_value_index* vbin = new ast_value_index();
                    vbin->from = left_v;
                    vbin->index = const_result;

                    vbin->update_constant_value(&lex);

                    return (grammar::ast_base*)vbin;
                }

                wo_error("Unexcepted token type.");
                return lex.parser_error(0x0000, L"Unexcepted token type.");
            }
        };

        struct pass_build_function_type : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                wo_test(input.size() == 3);

                ast_type* result = nullptr;
                auto* complex_type = dynamic_cast<ast_type*>(WO_NEED_AST(2));

                wo_test(complex_type);

                if (complex_type->is_func())
                    result = new ast_type(complex_type);
                else
                    result = complex_type;

                result->set_as_function_type();
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
                        wo_test(child->sibling == nullptr && tktype && tktype->tokens.type == +lex_type::l_variadic_sign);
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
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                ast_type* result = nullptr;

                auto* scoping_type = dynamic_cast<ast_value_variable*>(WO_NEED_AST(0));
                wo_test(scoping_type);
                result = new ast_type(scoping_type->var_name);
                result->search_from_global_namespace = scoping_type->search_from_global_namespace;
                result->scope_namespaces = scoping_type->scope_namespaces;
                result->searching_from_type = scoping_type->searching_from_type;
                if (result->search_from_global_namespace || !result->scope_namespaces.empty())
                    result->is_pending_type = true;

                if (input.size() == 1 || ast_empty::is_empty(input[1]))
                {
                    return (ast_basic*)result;
                }
                else
                {
                    ast_list* template_arg_list = dynamic_cast<ast_list*>(WO_NEED_AST(1));

                    std::vector<ast_type*> template_args;
                    wo_test(template_arg_list);
                    ast_type* type = dynamic_cast<ast_type*>(template_arg_list->children);
                    while (type)
                    {
                        template_args.push_back(type);
                        type = dynamic_cast<ast_type*>(type->sibling);
                    }
                    result->template_arguments = template_args;
                    if (result->is_array())
                        if (result->template_arguments.size() != 1)
                            lex.parser_error(0x0000, WO_ERR_ARRAY_NEED_ONE_TEMPLATE_ARG);
                    if (result->is_map())
                        if (result->template_arguments.size() != 2)
                            lex.parser_error(0x0000, WO_ERR_MAP_NEED_TWO_TEMPLATE_ARG);

                    return (ast_basic*)result;
                }
            }
        };

        struct pass_using_type_as : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                // using xxx  = xxx
                ast_using_type_as* using_type = new ast_using_type_as;
                using_type->new_type_identifier = WO_NEED_TOKEN(2).identifier;
                using_type->old_type = dynamic_cast<ast_type*>(WO_NEED_AST(5));
                using_type->declear_attribute = dynamic_cast<ast_decl_attribute*>(WO_NEED_AST(0));
                using_type->is_alias = WO_NEED_TOKEN(1).type == +lex_type::l_alias;
                ast_list* template_const_list = new ast_list;

                if (!ast_empty::is_empty(input[3]))
                {
                    ast_list* template_defines = dynamic_cast<ast_list*>(WO_NEED_AST(3));
                    wo_test(template_defines);
                    using_type->is_template_define = true;

                    ast_template_define_with_naming* template_type = dynamic_cast<ast_template_define_with_naming*>(template_defines->children);
                    wo_test(template_type);
                    while (template_type)
                    {
                        using_type->template_type_name_list.push_back(template_type->template_ident);

                        if (template_type->naming_const)
                        {
                            ast_check_type_with_naming_in_pass2* acheck = new ast_check_type_with_naming_in_pass2;
                            acheck->template_type = new ast_type(template_type->template_ident);
                            acheck->naming_const = new ast_type(L"pending");
                            acheck->naming_const->set_type(template_type->naming_const);
                            acheck->row_no = template_type->row_no;
                            acheck->col_no = template_type->col_no;
                            acheck->source_file = template_type->source_file;
                            template_const_list->append_at_end(acheck);
                        }

                        template_type = dynamic_cast<ast_template_define_with_naming*>(template_type->sibling);
                    }
                }

                using_type->naming_check_list = template_const_list;

                return (ast_basic*)using_type;
            }
        };

        struct pass_token : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                return (grammar::ast_base*)new ast_token(WO_NEED_TOKEN(0));
            }
        };

        struct pass_return : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                ast_return* result = new ast_return();
                if (input.size() == 2)
                {
                    if (!ast_empty::is_empty(input[1]))
                    {
                        result->return_value = dynamic_cast<ast_value*>(WO_NEED_AST(1));
                    }
                }
                else if (input.size() == 3)
                {
                    result->return_value = dynamic_cast<ast_value*>(WO_NEED_AST(2));
                    result->return_value->is_mark_as_using_ref = true;
                }
                else
                {
                    wo_assert(input.size() == 1);
                    result->return_value = dynamic_cast<ast_value*>(WO_NEED_AST(0));
                }
                return (grammar::ast_base*)result;
            }
        };

        struct pass_func_argument : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                ast_value_arg_define* arg_def = new ast_value_arg_define;
                arg_def->declear_attribute = dynamic_cast<ast_decl_attribute*>(WO_NEED_AST(0));
                wo_assert(arg_def->declear_attribute);

                if (input.size() == 4)
                {
                    if (WO_NEED_TOKEN(1).type == +lex_type::l_ref)
                        arg_def->decl = identifier_decl::REF;
                    else
                    {
                        wo_assert(WO_NEED_TOKEN(1).type == +lex_type::l_mut);
                        arg_def->decl = identifier_decl::MUTABLE;
                    }

                    arg_def->arg_name = WO_NEED_TOKEN(2).identifier;
                    arg_def->value_type = dynamic_cast<ast_type*>(WO_NEED_AST(3));
                }
                else
                {
                    wo_assert(input.size() == 3);

                    arg_def->arg_name = WO_NEED_TOKEN(1).identifier;
                    arg_def->value_type = dynamic_cast<ast_type*>(WO_NEED_AST(2));
                }

                return (grammar::ast_base*)arg_def;
            }
        };

        struct pass_foreach : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                ast_foreach* afor = new ast_foreach;

                // for ( DECL_ATTRIBUTE var LIST : EXP ) SENT
                // 0   1        2        3   4   5  6  7   8

                // build EXP->iter() / var LIST;

                //{{{{
                    // var _iter = XXXXXX->iter();
                ast_value* be_iter_value = dynamic_cast<ast_value*>(WO_NEED_AST(6));

                ast_value_funccall* exp_dir_iter_call = new ast_value_funccall();
                exp_dir_iter_call->arguments = new ast_list();
                exp_dir_iter_call->value_type = new ast_type(L"pending");
                exp_dir_iter_call->called_func = new ast_value_variable(L"iter");
                exp_dir_iter_call->arguments->append_at_head(be_iter_value);
                exp_dir_iter_call->directed_value_from = be_iter_value;

                exp_dir_iter_call->called_func->source_file = be_iter_value->source_file;
                exp_dir_iter_call->called_func->row_no = be_iter_value->row_no;
                exp_dir_iter_call->called_func->col_no = be_iter_value->col_no;

                exp_dir_iter_call->source_file = be_iter_value->source_file;
                exp_dir_iter_call->row_no = be_iter_value->row_no;
                exp_dir_iter_call->col_no = be_iter_value->col_no;

                afor->iter_getting_funccall = exp_dir_iter_call;

                afor->used_iter_define = new ast_varref_defines;
                afor->used_iter_define->declear_attribute = new ast_decl_attribute;

                auto* afor_iter_define = new ast_pattern_identifier;
                afor_iter_define->identifier = L"_iter";
                afor_iter_define->attr = new ast_decl_attribute();

                afor->used_iter_define->var_refs.push_back({ afor_iter_define, exp_dir_iter_call });


                afor->used_vawo_defines = new ast_varref_defines;
                afor->used_vawo_defines->declear_attribute = new ast_decl_attribute;

                // var a= tkplace, b = tkplace...
                ast_pattern_base* a_var_defs = dynamic_cast<ast_pattern_base*>(dynamic_cast<ast_list*>(WO_NEED_AST(4))->children);
                while (a_var_defs)
                {
                    if (auto* a_identi = dynamic_cast<ast_pattern_identifier*>(a_var_defs))
                    {
                        if (a_identi->decl == identifier_decl::REF)
                            lex.lang_error(0x0000, a_identi, L"for-each 语句中的最外层模式不可以接收引用 'ref'.");
                    }

                    afor->used_vawo_defines->var_refs.push_back({ a_var_defs, new ast_value_takeplace() });
                    a_var_defs = dynamic_cast<ast_pattern_base*>(a_var_defs->sibling);
                }

                // in loop body..
                // {{{{
                //    while (_iter->next(..., a,b))
                // DONOT INSERT ARGUS NOW, TAKE PLACE IN PASS2..    
                // }}}}

                ast_value_funccall* iter_dir_next_call = new ast_value_funccall();
                iter_dir_next_call->arguments = new ast_list();
                iter_dir_next_call->value_type = new ast_type(L"pending");
                iter_dir_next_call->called_func = new ast_value_variable(L"next");
                iter_dir_next_call->directed_value_from = new ast_value_variable(L"_iter");


                iter_dir_next_call->called_func->source_file = be_iter_value->source_file;
                iter_dir_next_call->called_func->row_no = be_iter_value->row_no;
                iter_dir_next_call->called_func->col_no = be_iter_value->col_no;

                iter_dir_next_call->source_file = be_iter_value->source_file;
                iter_dir_next_call->row_no = be_iter_value->row_no;
                iter_dir_next_call->col_no = be_iter_value->col_no;

                afor->iter_next_judge_expr = iter_dir_next_call;
                afor->iterator_var = new ast_value_variable(L"_iter");
                afor->iterator_var->is_mark_as_using_ref = true;

                afor->execute_sentences = WO_NEED_AST(8);


                return (ast_basic*)afor;
            }
        };

        struct pass_forloop : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
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
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                auto* result = WO_NEED_AST(2);
                result->marking_label = WO_NEED_TOKEN(0).identifier;
                return (ast_basic*)result;
            }

        };

        struct pass_break : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                if (input.size() == 1)
                    return (ast_basic*)new ast_break;
                auto result = new ast_break;
                result->label = WO_NEED_TOKEN(1).identifier;
                return (ast_basic*)result;
            }
        };
        struct pass_continue : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                if (input.size() == 1)
                    return (ast_basic*)new ast_continue;
                auto result = new ast_continue;
                result->label = WO_NEED_TOKEN(1).identifier;
                return (ast_basic*)result;
            }
        };
        struct pass_template_decl : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                ast_template_define_with_naming* atn = new ast_template_define_with_naming;
                atn->template_ident = WO_NEED_TOKEN(0).identifier;
                if (ast_empty::is_empty(input[1]))
                    atn->naming_const = nullptr;
                else
                    atn->naming_const = dynamic_cast<ast_type*>(WO_NEED_AST(1));
                return (ast_basic*)atn;
            }
        };

        struct pass_format_string : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                //if(input.size() == )
                wo_assert(input.size() == 2 || input.size() == 3);
                if (input.size() == 2)
                {
                    auto* left = new ast_value_literal(WO_NEED_TOKEN(0));
                    auto* right = dynamic_cast<ast_value*>(WO_NEED_AST(1));

                    auto cast_right = new ast_value_type_cast(right, new ast_type(L"string"));

                    cast_right->row_no = left->row_no = right->row_no;
                    cast_right->col_no = left->col_no = right->col_no;
                    cast_right->source_file = left->source_file = right->source_file;

                    ast_value_binary* vbin = new ast_value_binary();
                    vbin->left = left;
                    vbin->operate = lex_type::l_add;
                    vbin->right = cast_right;
                    vbin->update_constant_value(&lex);
                    return (ast_basic*)vbin;
                }
                else
                {
                    auto* left = new ast_value_literal(WO_NEED_TOKEN(1));
                    auto* right = dynamic_cast<ast_value*>(WO_NEED_AST(2));

                    auto cast_right = new ast_value_type_cast(right, new ast_type(L"string"));

                    cast_right->row_no = left->row_no = right->row_no;
                    cast_right->col_no = left->col_no = right->col_no;
                    cast_right->source_file = left->source_file = right->source_file;

                    ast_value_binary* vbin = new ast_value_binary();
                    vbin->left = left;
                    vbin->operate = lex_type::l_add;
                    vbin->right = cast_right;
                    vbin->update_constant_value(&lex);

                    auto* origin_list = dynamic_cast<ast_value*>(WO_NEED_AST(0));

                    vbin->row_no = origin_list->row_no;
                    vbin->col_no = origin_list->col_no;
                    vbin->source_file = origin_list->source_file;

                    ast_value_binary* vbinresult = new ast_value_binary();
                    vbinresult->left = origin_list;
                    vbinresult->operate = lex_type::l_add;
                    vbinresult->right = vbin;
                    vbinresult->update_constant_value(&lex);
                    return (ast_basic*)vbinresult;
                }
            }
        };

        struct pass_finish_format_string : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                wo_assert(input.size() == 2);

                auto* left = dynamic_cast<ast_value*>(WO_NEED_AST(0));
                auto* right = new ast_value_literal(WO_NEED_TOKEN(1));

                ast_value_binary* vbin = new ast_value_binary();
                vbin->left = left;
                vbin->operate = lex_type::l_add;
                vbin->right = right;

                vbin->update_constant_value(&lex);
                return (ast_basic*)vbin;
            }
        };

        struct pass_union_item : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                ast_union_item* result = new ast_union_item;
                if (input.size() == 2)
                {
                    // identifier : type
                    // => const var identifier<A...> = func(){...}();
                    result->identifier = WO_NEED_TOKEN(0).identifier;
                    result->gadt_out_type_may_nil = dynamic_cast<ast_type*>(WO_NEED_AST(1));
                }
                else
                {
                    // identifier ( TYPE ) : type
                    // => func identifier<A...>(var v: TYPE){...}
                    wo_assert(input.size() == 5);
                    result->identifier = WO_NEED_TOKEN(0).identifier;
                    result->type_may_nil = dynamic_cast<ast_type*>(WO_NEED_AST(2));
                    result->gadt_out_type_may_nil = dynamic_cast<ast_type*>(WO_NEED_AST(4));
                    wo_assert(result->type_may_nil);
                }
                return (ast_basic*)result;
            }
        };
        struct pass_trib_expr : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
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
                const std::vector<std::wstring>& template_defines,
                std::set<std::wstring>& out_used_type)
            {
                // No need for checking using-type.
                if (type_decl->scope_namespaces.empty()
                    && !type_decl->search_from_global_namespace
                    && std::find(template_defines.begin(), template_defines.end(), type_decl->type_name)
                    != template_defines.end())
                    out_used_type.insert(type_decl->type_name);
                if (type_decl->is_complex())
                    find_used_template(type_decl->complex_type, template_defines, out_used_type);
                if (type_decl->is_func())
                    for (auto* argtype : type_decl->argument_types)
                        find_used_template(argtype, template_defines, out_used_type);
                if (type_decl->has_template())
                    for (auto* argtype : type_decl->template_arguments)
                        find_used_template(argtype, template_defines, out_used_type);
            }
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                wo_assert(input.size() == 7);
                // ATTRIBUTE union IDENTIFIER <TEMPLATE_DEF> { ITEMS }
                //    0                    2             3          5

                // Create a namespace 
                ast_decl_attribute* union_arttribute = dynamic_cast<ast_decl_attribute*>(WO_NEED_AST(0));

                ast_list* bind_using_type_namespace_result = new ast_list;

                ast_namespace* union_scope = new ast_namespace;
                union_scope->scope_name = WO_NEED_TOKEN(2).identifier;
                union_scope->copy_source_info(union_arttribute);
                union_scope->in_scope_sentence = new ast_list;

                // Get templates here(If have?)
                ast_template_define_with_naming* defined_template_args = nullptr;
                ast_list* defined_template_arg_lists = nullptr;

                if (!ast_empty::is_empty(input[3]))
                {
                    defined_template_arg_lists = dynamic_cast<ast_list*>(WO_NEED_AST(3));
                    defined_template_args = dynamic_cast<ast_template_define_with_naming*>(
                        defined_template_arg_lists->children);
                }

                // Then we decl a using-type here;
                ast_using_type_as* using_type = new ast_using_type_as;
                using_type->new_type_identifier = union_scope->scope_name;
                using_type->old_type = new ast_type(L"union");
                using_type->declear_attribute = union_arttribute;

                std::vector <std::wstring>& template_arg_defines = using_type->template_type_name_list;
                if (defined_template_args)
                {
                    ast_list* template_const_list = new ast_list;

                    using_type->is_template_define = true;

                    ast_template_define_with_naming* template_type = defined_template_args;
                    wo_test(template_type);
                    while (template_type)
                    {
                        using_type->template_type_name_list.push_back(template_type->template_ident);

                        if (template_type->naming_const)
                        {
                            ast_check_type_with_naming_in_pass2* acheck = new ast_check_type_with_naming_in_pass2;
                            acheck->template_type = new ast_type(template_type->template_ident);
                            acheck->naming_const = new ast_type(L"pending");
                            acheck->naming_const->set_type(template_type->naming_const);
                            acheck->row_no = template_type->row_no;
                            acheck->col_no = template_type->col_no;
                            acheck->source_file = template_type->source_file;
                            template_const_list->append_at_end(acheck);
                        }

                        template_type = dynamic_cast<ast_template_define_with_naming*>(template_type->sibling);
                    }
                    using_type->naming_check_list = template_const_list;
                }
                bind_using_type_namespace_result->append_at_head(using_type);

                // OK, We need decl union items/function here
                ast_union_item* items =
                    dynamic_cast<ast_union_item*>(dynamic_cast<ast_list*>(WO_NEED_AST(5))->children);

                uint16_t union_item_id = 0;

                while (items)
                {
                    std::set<std::wstring> used_template_args;

                    auto create_union_type = [&]() {
                        ast_type* union_type_with_template = new ast_type(using_type->new_type_identifier);
                        for (auto& ident : using_type->template_type_name_list)
                        {
                            // Current template-type has been used.
                            if (used_template_args.find(ident) != used_template_args.end())
                                union_type_with_template->template_arguments.push_back(new ast_type(ident));
                            else
                                // Not used template-type, 
                                union_type_with_template->template_arguments.push_back(new ast_type(L"anything"));
                        }
                        return union_type_with_template;
                    };

                    auto& member = using_type->old_type->struct_member_index[items->identifier];
                    member.offset = ++union_item_id;

                    if (items->gadt_out_type_may_nil)
                        find_used_template(items->gadt_out_type_may_nil, template_arg_defines, used_template_args);
                    if (items->type_may_nil)
                    {
                        find_used_template(items->type_may_nil, template_arg_defines, used_template_args);
                        // Generate func here!
                        ast_value_function_define* avfd_item_type_builder = new ast_value_function_define;
                        avfd_item_type_builder->function_name = items->identifier;
                        avfd_item_type_builder->argument_list = new ast_list;
                        avfd_item_type_builder->declear_attribute = union_arttribute;

                        ast_value_arg_define* argdef = new ast_value_arg_define;
                        argdef->arg_name = L"_val";
                        argdef->value_type = items->type_may_nil;
                        argdef->declear_attribute = new ast_decl_attribute;
                        avfd_item_type_builder->argument_list->add_child(argdef);
                        argdef->copy_source_info(items);

                        if (items->gadt_out_type_may_nil)
                            avfd_item_type_builder->value_type = items->gadt_out_type_may_nil;
                        else
                            avfd_item_type_builder->value_type = create_union_type();

                        avfd_item_type_builder->value_type->set_as_function_type();
                        avfd_item_type_builder->value_type->argument_types.push_back(dynamic_cast<ast_type*>(items->type_may_nil->instance()));

                        avfd_item_type_builder->auto_adjust_return_type = true;
                        avfd_item_type_builder->copy_source_info(items);
                        if (using_type->is_template_define)
                        {
                            size_t id = 0;
                            for (auto& templatedef : using_type->template_type_name_list)
                            {
                                if (used_template_args.find(templatedef) != used_template_args.end())
                                {
                                    avfd_item_type_builder->template_type_name_list.push_back(templatedef);
                                    member.union_used_template_index.push_back(id);
                                }
                                ++id;
                            }
                            if (!avfd_item_type_builder->template_type_name_list.empty())
                                avfd_item_type_builder->is_template_define = true;
                        }

                        avfd_item_type_builder->in_function_sentence = new ast_list;

                        ast_union_make_option_ob_to_cr_and_ret* result = new ast_union_make_option_ob_to_cr_and_ret();
                        result->copy_source_info(items);
                        result->argument_may_nil = new ast_value_variable(argdef->arg_name);
                        result->id = union_item_id;
                        // all done ~ fuck!

                        avfd_item_type_builder->in_function_sentence->append_at_end(result);
                        union_scope->in_scope_sentence->append_at_end(avfd_item_type_builder);
                    }
                    else
                    {
                        // Generate const here!
                        ast_value_function_define* avfd_item_type_builder = new ast_value_function_define;
                        avfd_item_type_builder->function_name = L""; // Is a lambda function!
                        avfd_item_type_builder->argument_list = new ast_list;
                        avfd_item_type_builder->declear_attribute = union_arttribute;
                        if (items->gadt_out_type_may_nil)
                            avfd_item_type_builder->value_type = items->gadt_out_type_may_nil;
                        else
                            avfd_item_type_builder->value_type = create_union_type();
                        avfd_item_type_builder->value_type->set_as_function_type();

                        avfd_item_type_builder->auto_adjust_return_type = true;
                        avfd_item_type_builder->copy_source_info(items);

                        avfd_item_type_builder->in_function_sentence = new ast_list;

                        ast_union_make_option_ob_to_cr_and_ret* result = new ast_union_make_option_ob_to_cr_and_ret();
                        result->copy_source_info(items);
                        result->id = union_item_id;
                        // all done ~ fuck!
                        avfd_item_type_builder->in_function_sentence->append_at_end(result);

                        ast_value_funccall* funccall = new ast_value_funccall();
                        funccall->copy_source_info(avfd_item_type_builder);
                        funccall->called_func = avfd_item_type_builder;
                        funccall->arguments = new ast_list();
                        funccall->value_type = new ast_type(L"pending");

                        ast_varref_defines* define_union_item = new ast_varref_defines();
                        define_union_item->copy_source_info(items);

                        auto* union_item_define = new ast_pattern_identifier;
                        union_item_define->identifier = items->identifier;
                        union_item_define->attr = new ast_decl_attribute();

                        if (using_type->is_template_define)
                        {
                            size_t id = 0;
                            for (auto& templatedef : using_type->template_type_name_list)
                            {
                                if (used_template_args.find(templatedef) != used_template_args.end())
                                {
                                    union_item_define->template_arguments.push_back(templatedef);
                                    member.union_used_template_index.push_back(id);
                                }
                                ++id;
                            }
                        }

                        define_union_item->var_refs.push_back({ union_item_define, funccall });
                        define_union_item->declear_attribute = new ast_decl_attribute();

                        union_scope->in_scope_sentence->append_at_end(define_union_item);
                    }
                    items = dynamic_cast<ast_union_item*>(items->sibling);
                }
                bind_using_type_namespace_result->append_at_end(union_scope);
                return (ast_basic*)bind_using_type_namespace_result;
            }
        };

        struct pass_identifier_pattern : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                auto* result = new ast_pattern_identifier;
                if (input.size() == 3)
                {
                    if (WO_NEED_TOKEN(0).type == +lex_type::l_ref)
                        result->decl = identifier_decl::REF;
                    else
                    {
                        wo_assert(WO_NEED_TOKEN(0).type == +lex_type::l_mut);
                        result->decl = identifier_decl::MUTABLE;
                    }

                    result->attr = dynamic_cast<ast_decl_attribute*>(WO_NEED_AST(1));
                    result->identifier = WO_NEED_TOKEN(2).identifier;
                }
                else
                {
                    result->attr = dynamic_cast<ast_decl_attribute*>(WO_NEED_AST(0));
                    result->identifier = WO_NEED_TOKEN(1).identifier;
                }
                return (ast_basic*)result;
            }
        };

        struct pass_tuple_pattern : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                auto* result = new ast_pattern_tuple;
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
                        if (ast_pattern_identifier* ipat = dynamic_cast<ast_pattern_identifier*>(child_pattern))
                            val_take_place->as_ref = (ipat->decl == identifier_decl::REF);
                        else
                            val_take_place->as_ref = true;

                        result->tuple_takeplaces.push_back(val_take_place);
                    }
                }
                return (ast_basic*)result;
            }
        };

        struct pass_union_pattern : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                // 1. CALLABLE_LEFT
                // 2. CALLABLE_LEFT ( PATTERN )
                auto* result = new ast_pattern_union_value;

                wo_assert(input.size() == 1 || input.size() == 4);

                result->union_expr = dynamic_cast<ast_value_variable*>(WO_NEED_AST(0));

                if (input.size() == 4)
                    result->pattern_arg_in_union_may_nil
                    = dynamic_cast<ast_pattern_base*>(WO_NEED_AST(2));
                return (ast_basic*)result;
            }
        };

        struct pass_match_case_for_union : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                // pattern_case? {sentence in list}
                wo_assert(input.size() == 3);

                auto* result = new ast_match_union_case;
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
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
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
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                auto* result = new ast_struct_member_define;
                result->member_name = WO_NEED_TOKEN(0).identifier;
                if (input.size() == 2)
                {
                    // identifier TYPE_DECLEAR
                    result->member_val_or_type_tkplace = new ast_value_takeplace;//  dynamic_cast<ast_list*>(WO_NEED_AST(5));
                    result->member_val_or_type_tkplace->value_type = dynamic_cast<ast_type*>(WO_NEED_AST(1));
                    wo_assert(result->member_val_or_type_tkplace->value_type);
                }
                else
                {
                    wo_assert(input.size() == 3);
                    // identifier = VALUE
                    result->member_val_or_type_tkplace = dynamic_cast<ast_value*>(WO_NEED_AST(2));
                    wo_assert(result->member_val_or_type_tkplace);
                }
                return (ast_basic*)result;
            }
        };

        struct pass_struct_type_define : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                wo_assert(input.size() == 4);
                // struct{ members }
                //   0   1     2   3 
                ast_type* struct_type = new ast_type(L"struct");
                uint16_t membid = 0;
                auto* members = WO_NEED_AST(2)->children;
                while (members)
                {
                    auto* member_pair = dynamic_cast<ast_struct_member_define*>(members);
                    wo_assert(member_pair);

                    struct_type->struct_member_index[member_pair->member_name].init_value_may_nil
                        = dynamic_cast<ast_value_takeplace*>(member_pair->member_val_or_type_tkplace);
                    struct_type->struct_member_index[member_pair->member_name].offset
                        = membid++;

                    wo_assert(struct_type->struct_member_index[member_pair->member_name].init_value_may_nil);

                    members = members->sibling;
                }

                return (ast_basic*)struct_type;
            }
        };

        struct pass_make_struct_instance : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                // STRUCT_TYPE { ITEMS }

                wo_assert(input.size() == 4);
                ast_value_make_struct_instance* value = new ast_value_make_struct_instance;
                value->value_type = dynamic_cast<ast_type*>(WO_NEED_AST(0));
                wo_assert(value->value_type);

                value->struct_member_vals = dynamic_cast<ast_list*>(WO_NEED_AST(2));
                wo_assert(value->struct_member_vals);

                return (ast_basic*)value;
            }
        };

        struct pass_build_tuple_type : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                // ( LIST )

                wo_assert(input.size() == 1);
                ast_type* tuple_type = new ast_type(L"tuple");

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
                            lex.parser_error(0x0000, L"元祖类型中不允许出现 '...'，继续");

                    }
                    wo_assert(!tuple_type->template_arguments.empty());
                }
                return (ast_basic*)tuple_type;
            }
        };

        struct pass_tuple_types_list : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
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
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                // where xxxx.... ,
                wo_assert(input.size() == 3);

                ast_where_constraint* result = new ast_where_constraint;
                result->where_constraint_list = dynamic_cast<ast_list*>(WO_NEED_AST(1));
                wo_assert(result->where_constraint_list);

                return (ast_basic*)result;
            }
        };

        /////////////////////////////////////////////////////////////////////////////////
#if 1
        inline void init_builder()
        {
            _registed_builder_function_id_list[meta::type_hash<pass_template_decl>] = _register_builder<pass_template_decl>();

            _registed_builder_function_id_list[meta::type_hash<pass_mark_label>] = _register_builder<pass_mark_label>();

            _registed_builder_function_id_list[meta::type_hash<pass_break>] = _register_builder<pass_break>();

            _registed_builder_function_id_list[meta::type_hash<pass_continue>] = _register_builder<pass_continue>();

            _registed_builder_function_id_list[meta::type_hash<pass_forloop>] = _register_builder<pass_forloop>();

            _registed_builder_function_id_list[meta::type_hash<pass_foreach>] = _register_builder<pass_foreach>();

            _registed_builder_function_id_list[meta::type_hash<pass_typeof>] = _register_builder<pass_typeof>();

            _registed_builder_function_id_list[meta::type_hash<pass_template_reification>] = _register_builder<pass_template_reification>();

            _registed_builder_function_id_list[meta::type_hash<pass_type_check>] = _register_builder<pass_type_check>();

            _registed_builder_function_id_list[meta::type_hash<pass_directed_value_for_call>] = _register_builder<pass_directed_value_for_call>();

            _registed_builder_function_id_list[meta::type_hash<pass_type_judgement>] = _register_builder<pass_type_judgement>();

            _registed_builder_function_id_list[meta::type_hash<pass_mark_value_as_ref>] = _register_builder<pass_mark_value_as_ref>();

            _registed_builder_function_id_list[meta::type_hash<pass_using_type_as>] = _register_builder<pass_using_type_as>();

            _registed_builder_function_id_list[meta::type_hash<pass_enum_item_create>] = _register_builder<pass_enum_item_create>();

            _registed_builder_function_id_list[meta::type_hash<pass_enum_declear_begin>] = _register_builder<pass_enum_declear_begin>();

            _registed_builder_function_id_list[meta::type_hash<pass_enum_declear_append>] = _register_builder<pass_enum_declear_append>();

            _registed_builder_function_id_list[meta::type_hash<pass_enum_finalize>] = _register_builder<pass_enum_finalize>();

            _registed_builder_function_id_list[meta::type_hash<pass_decl_attrib_begin>] = _register_builder<pass_decl_attrib_begin>();

            _registed_builder_function_id_list[meta::type_hash<pass_append_attrib>] = _register_builder<pass_append_attrib>();

            _registed_builder_function_id_list[meta::type_hash<pass_using_namespace>] = _register_builder<pass_using_namespace>();

            _registed_builder_function_id_list[meta::type_hash<pass_mapping_pair>] = _register_builder<pass_mapping_pair>();

            _registed_builder_function_id_list[meta::type_hash<pass_unary_op>] = _register_builder<pass_unary_op>();

            _registed_builder_function_id_list[meta::type_hash<pass_unpack_args>] = _register_builder<pass_unpack_args>();

            _registed_builder_function_id_list[meta::type_hash<pass_pack_variadic_args>] = _register_builder<pass_pack_variadic_args>();

            _registed_builder_function_id_list[meta::type_hash<pass_extern>] = _register_builder<pass_extern>();

            _registed_builder_function_id_list[meta::type_hash<pass_binary_logical_op>] = _register_builder<pass_binary_logical_op>();

            _registed_builder_function_id_list[meta::type_hash<pass_assign_op>] = _register_builder<pass_assign_op>();

            _registed_builder_function_id_list[meta::type_hash<pass_while>] = _register_builder<pass_while>();

            _registed_builder_function_id_list[meta::type_hash<pass_except>] = _register_builder<pass_except>();

            _registed_builder_function_id_list[meta::type_hash<pass_if>] = _register_builder<pass_if>();

            _registed_builder_function_id_list[meta::type_hash<pass_map_builder>] = _register_builder<pass_map_builder>();

            _registed_builder_function_id_list[meta::type_hash<pass_empty_sentence_block>] = _register_builder<pass_empty_sentence_block>();

            _registed_builder_function_id_list[meta::type_hash<pass_sentence_block<0>>] = _register_builder<pass_sentence_block<0>>();

            _registed_builder_function_id_list[meta::type_hash<pass_sentence_block<1>>] = _register_builder<pass_sentence_block<1>>();

            _registed_builder_function_id_list[meta::type_hash<pass_array_builder>] = _register_builder<pass_array_builder>();

            _registed_builder_function_id_list[meta::type_hash<pass_function_call>] = _register_builder<pass_function_call>();

            _registed_builder_function_id_list[meta::type_hash<pass_return>] = _register_builder<pass_return>();

            _registed_builder_function_id_list[meta::type_hash<pass_function_define>] = _register_builder<pass_function_define>();

            _registed_builder_function_id_list[meta::type_hash<pass_func_argument>] = _register_builder<pass_func_argument>();

            _registed_builder_function_id_list[meta::type_hash<pass_token>] = _register_builder<pass_token>();

            _registed_builder_function_id_list[meta::type_hash<pass_finalize_serching_namespace>] = _register_builder<pass_finalize_serching_namespace>();

            _registed_builder_function_id_list[meta::type_hash<pass_append_serching_namespace>] = _register_builder<pass_append_serching_namespace>();

            _registed_builder_function_id_list[meta::type_hash<pass_variable_in_namespace>] = _register_builder<pass_variable_in_namespace>();

            _registed_builder_function_id_list[meta::type_hash<pass_begin_varref_define>] = _register_builder<pass_begin_varref_define>();

            _registed_builder_function_id_list[meta::type_hash<pass_add_varref_define>] = _register_builder<pass_add_varref_define>();

            _registed_builder_function_id_list[meta::type_hash<pass_mark_as_var_define>] = _register_builder<pass_mark_as_var_define>();

            _registed_builder_function_id_list[meta::type_hash<pass_trans_where_decl_in_lambda>] = _register_builder<pass_trans_where_decl_in_lambda>();

            _registed_builder_function_id_list[meta::type_hash<pass_namespace>] = _register_builder<pass_namespace>();

            _registed_builder_function_id_list[meta::type_hash<pass_index_op>] = _register_builder<pass_index_op>();

            _registed_builder_function_id_list[meta::type_hash<pass_empty>] = _register_builder<pass_empty>();

            _registed_builder_function_id_list[meta::type_hash<pass_binary_op>] = _register_builder<pass_binary_op>();

            _registed_builder_function_id_list[meta::type_hash<pass_create_list<0>>] = _register_builder<pass_create_list<0>>();

            _registed_builder_function_id_list[meta::type_hash<pass_append_list<1, 0>>] = _register_builder<pass_append_list<1, 0>>();

            _registed_builder_function_id_list[meta::type_hash<pass_append_list<0, 1>>] = _register_builder<pass_append_list<0, 1>>();

            _registed_builder_function_id_list[meta::type_hash<pass_append_list<0, 2>>] = _register_builder<pass_append_list<0, 2>>();

            _registed_builder_function_id_list[meta::type_hash<pass_append_list<2, 0>>] = _register_builder<pass_append_list<2, 0>>();

            _registed_builder_function_id_list[meta::type_hash<pass_append_list<1, 2>>] = _register_builder<pass_append_list<1, 2>>();

            _registed_builder_function_id_list[meta::type_hash<pass_append_list<1, 3>>] = _register_builder<pass_append_list<1, 3>>();

            _registed_builder_function_id_list[meta::type_hash<pass_import_files>] = _register_builder<pass_import_files>();

            _registed_builder_function_id_list[meta::type_hash<pass_variable>] = _register_builder<pass_variable>();

            _registed_builder_function_id_list[meta::type_hash<pass_build_function_type>] = _register_builder<pass_build_function_type>();

            _registed_builder_function_id_list[meta::type_hash<pass_build_type_may_template>] = _register_builder<pass_build_type_may_template>();

            _registed_builder_function_id_list[meta::type_hash<pass_type_cast>] = _register_builder<pass_type_cast>();

            _registed_builder_function_id_list[meta::type_hash<pass_literal>] = _register_builder<pass_literal>();

            _registed_builder_function_id_list[meta::type_hash<pass_format_string>] = _register_builder<pass_format_string>();

            _registed_builder_function_id_list[meta::type_hash<pass_finish_format_string>] = _register_builder<pass_finish_format_string>();

            _registed_builder_function_id_list[meta::type_hash<pass_union_item>] = _register_builder<pass_union_item>();

            _registed_builder_function_id_list[meta::type_hash<pass_union_define>] = _register_builder<pass_union_define>();

            _registed_builder_function_id_list[meta::type_hash<pass_match>] = _register_builder<pass_match>();
            _registed_builder_function_id_list[meta::type_hash<pass_match_case_for_union>] = _register_builder<pass_match_case_for_union>();

            _registed_builder_function_id_list[meta::type_hash<pass_union_pattern>] = _register_builder<pass_union_pattern>();
            _registed_builder_function_id_list[meta::type_hash<pass_identifier_pattern>] = _register_builder<pass_identifier_pattern>();
            _registed_builder_function_id_list[meta::type_hash<pass_tuple_pattern>] = _register_builder<pass_tuple_pattern>();

            _registed_builder_function_id_list[meta::type_hash<pass_struct_member_def>] = _register_builder<pass_struct_member_def>();
            _registed_builder_function_id_list[meta::type_hash<pass_struct_type_define>] = _register_builder<pass_struct_type_define>();
            _registed_builder_function_id_list[meta::type_hash<pass_make_struct_instance>] = _register_builder<pass_make_struct_instance>();

            _registed_builder_function_id_list[meta::type_hash<pass_build_tuple_type>] = _register_builder<pass_build_tuple_type>();
            _registed_builder_function_id_list[meta::type_hash<pass_tuple_types_list>] = _register_builder<pass_tuple_types_list>();
            _registed_builder_function_id_list[meta::type_hash<pass_append_list_for_ref_tuple_maker>] = _register_builder<pass_append_list_for_ref_tuple_maker>();
            _registed_builder_function_id_list[meta::type_hash<pass_make_tuple>] = _register_builder<pass_make_tuple>();

            _registed_builder_function_id_list[meta::type_hash<pass_build_where_constraint>] = _register_builder<pass_build_where_constraint>();

            _registed_builder_function_id_list[meta::type_hash<pass_trib_expr>] = _register_builder<pass_trib_expr>();

            _registed_builder_function_id_list[meta::type_hash<pass_direct<0>>] = _register_builder<pass_direct<0>>();
            _registed_builder_function_id_list[meta::type_hash<pass_direct<1>>] = _register_builder<pass_direct<1>>();
            _registed_builder_function_id_list[meta::type_hash<pass_direct<2>>] = _register_builder<pass_direct<2>>();
            _registed_builder_function_id_list[meta::type_hash<pass_direct<3>>] = _register_builder<pass_direct<3>>();
            _registed_builder_function_id_list[meta::type_hash<pass_direct<4>>] = _register_builder<pass_direct<4>>();
            _registed_builder_function_id_list[meta::type_hash<pass_direct<5>>] = _register_builder<pass_direct<5>>();
        }
    }

    inline static grammar::rule operator>>(grammar::rule ost, size_t builder_index)
    {
        ost.first.builder_index = builder_index;
        return ost;
    }
#endif
#define WO_ASTBUILDER_INDEX(...) ast::index<__VA_ARGS__>()
}
