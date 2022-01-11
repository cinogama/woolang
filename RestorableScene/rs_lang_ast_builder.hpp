#pragma once
#include "rs_compiler_parser.hpp"
#include "rs_meta.hpp"
#include "rs_basic_type.hpp"
#include "rs_env_locale.hpp"
#include "rs_lang_functions_for_ast.hpp"
#include "rs_lang_extern_symbol_loader.hpp"
#include "rs_source_file_manager.hpp"
#include "rs_utf8.hpp"

#include <any>
#include <type_traits>
#include <cmath>

namespace rs
{
#define RS_REINSTANCE(ITEM) if(ITEM){(ITEM) = dynamic_cast<meta::origin_type<decltype(ITEM)>>((ITEM)->instance());}

    grammar* get_rs_grammar(void);
    namespace ast
    {
#if 1
        struct astnode_builder
        {
            using ast_basic = rs::grammar::ast_base;
            using inputs_t = std::vector<std::any>;
            using builder_func_t = std::function<std::any(lexer&, const std::wstring&, inputs_t&)>;

            virtual ~astnode_builder() = default;
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                rs_test(false, "");
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
            rs_test(idx != 0);

            return idx;
        }

        inline astnode_builder::builder_func_t get_builder(size_t idx)
        {
            rs_test(idx != 0);

            return _registed_builder_function[idx - 1];
        }
#endif
        /////////////////////////////////////////////////////////////////////////////////

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

            std::map<std::wstring, ast_value*> class_member_index;

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
                {L"void", value::valuetype::invalid},
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

            static bool check_castable(ast_type* to, ast_type* from, bool force)
            {
                if (to->is_pending() || to->is_void())
                    return false;

                if (to->is_dynamic())
                    return true;
                if (from->is_dynamic())
                {
                    // Not allowed cast template type from dynamic
                    // In fact, cast func from dynamic is dangerous too...

                    if (!to->is_func() && to->has_template())
                    {
                        for (auto ta_type : to->template_arguments)
                            if (!ta_type->is_dynamic())
                                return false;
                        return true;
                    }
                    return true;
                }


                if (from->is_pending() || to->is_pending())
                    return false;

                if (from->is_same(to, force))
                    return true;

                if (from->is_same(to))
                {
                    // TODO: MAY NEED USING CHAIN CHECK? HERE JUST CHECK IF A USING-TYPE TO NATIVE-TYPE
                    if (from->using_type_name && !to->using_type_name)
                        return true;
                }

                if (from->is_nil())
                {
                    if (to->value_type == value::valuetype::array_type
                        || to->value_type == value::valuetype::mapping_type
                        || to->value_type == value::valuetype::gchandle_type
                        || to->is_func()
                        || to->is_nil())
                        return true;
                    return false;
                }

                if (to->is_nil())
                    return false;

                if (from->is_func() || to->is_func())
                    return false;

                if (force)
                {
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
                }
                else
                {
                    if ((to->value_type == value::valuetype::integer_type && from->value_type == value::valuetype::real_type)
                        || (to->value_type == value::valuetype::real_type && from->value_type == value::valuetype::integer_type))
                    {
                        return true;
                    }
                }

                if (from->is_array() && to->is_array())
                    if (to->template_arguments[0]->is_dynamic())
                        return true;

                if (from->is_map() && to->is_map())
                {
                    if (to->template_arguments[0]->is_dynamic())
                    {
                        if (to->template_arguments[1]->is_dynamic()
                            || to->template_arguments[1]->is_same(from->template_arguments[1], false))
                            return true;

                    }
                    else if (to->template_arguments[1]->is_dynamic())
                    {
                        if (to->template_arguments[0]->is_same(from->template_arguments[0], false))
                            return true;
                    }
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
                rs_test(is_func());

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
                    class_member_index = _type->class_member_index;
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

            void set_as_function_type()
            {
                is_function_type = true;
            }
            ast_type* get_return_type()
            {
                if (is_complex())
                    return new ast_type(*complex_type);

                auto* rett = new ast_type(type_name);
                rett->using_type_name = using_type_name;
                rett->template_arguments = template_arguments;
                rett->class_member_index = class_member_index;
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
            bool is_pending() const
            {
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
                    if (using_type_name && using_type_name->is_like(another, termplate_set))
                    {
                        if (out_para)*out_para = dynamic_cast<ast_type*>(using_type_name->instance());
                        if (out_args)*out_args = const_cast<ast_type*>(another);
                        return true;
                    }
                    if (another->using_type_name && is_like(another->using_type_name, termplate_set))
                    {
                        if (out_para)*out_para = const_cast<ast_type*>(this);
                        if (out_args)*out_args = dynamic_cast<ast_type*>(another->using_type_name->instance());
                        return true;
                    }
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
            bool is_same(const ast_type* another, bool ignore_using_type = true) const
            {
                if (is_pending_function() || another->is_pending_function())
                    return false;

                rs_test(!is_pending() && !another->is_pending());

                if (!ignore_using_type && (using_type_name || another->using_type_name))
                {
                    if (!using_type_name || !another->using_type_name)
                        return false;

                    if (find_type_in_this_scope(using_type_name) != find_type_in_this_scope(another->using_type_name))
                        return false;
                }
                if (has_template())
                {
                    if (template_arguments.size() != another->template_arguments.size())
                        return false;
                    for (size_t index = 0; index < template_arguments.size(); index++)
                    {
                        if (!template_arguments[index]->is_same(another->template_arguments[index]))
                            return false;
                    }
                }
                if (is_func())
                {
                    if (argument_types.size() != another->argument_types.size())
                        return false;
                    for (size_t index = 0; index < argument_types.size(); index++)
                    {
                        if (!argument_types[index]->is_same(another->argument_types[index]))
                            return false;
                    }
                    if (is_variadic_function_type != another->is_variadic_function_type)
                        return false;
                }
                if (is_complex() && another->is_complex())
                    return complex_type->is_same(another->complex_type);
                else if (!is_complex() && !another->is_complex())
                    return get_type_name(ignore_using_type) == another->get_type_name(ignore_using_type);
                return false;
            }
            bool is_func() const
            {
                return is_function_type;
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

            std::wstring get_type_name(bool ignore_using_type = true) const
            {
                std::wstring result;

                if (!ignore_using_type && using_type_name)
                {
                    auto namespacechain = (search_from_global_namespace ? L"::" : L"") +
                        rs::str_to_wstr(get_belong_namespace_path_with_lang_scope(using_type_name->symbol));
                    result = (namespacechain.empty() ? L"" : namespacechain + L"::")
                        + using_type_name->get_type_name(ignore_using_type);
                }
                else
                {
                    result = (is_complex() ? complex_type->get_type_name(ignore_using_type) : type_name) /*+ (is_pending() ? L" !pending" : L"")*/;
                    if (has_template())
                    {
                        result += L"<";
                        for (size_t index = 0; index < template_arguments.size(); index++)
                        {
                            result += template_arguments[index]->get_type_name(ignore_using_type);
                            if (index + 1 != template_arguments.size())
                                result += L", ";
                        }
                        result += L">";
                    }
                }
                if (is_function_type)
                {
                    result += L"(";
                    for (size_t index = 0; index < argument_types.size(); index++)
                    {
                        result += argument_types[index]->get_type_name(ignore_using_type);
                        if (index + 1 != argument_types.size() || is_variadic_function_type)
                            result += L", ";
                    }

                    if (is_variadic_function_type)
                    {
                        result += L"...";
                    }
                    result += L")";
                }
                return result;
            }

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

                // Write self copy functions here..
                dumm->typefrom = dude_dump_ast_value(dumm->typefrom);
                RS_REINSTANCE(dumm->complex_type);
                std::vector<ast_type*> argument_types;
                std::vector<ast_type*> template_arguments;
                for (auto& argtype : dumm->argument_types)
                {
                    RS_REINSTANCE(argtype);
                }
                for (auto& argtype : dumm->template_arguments)
                {
                    RS_REINSTANCE(argtype);
                }
                RS_REINSTANCE(dumm->using_type_name);

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
            bool can_be_assign = false;

            bool is_constant = false;
            rs::value constant_value;

            virtual rs::value& get_constant_value()
            {
                if (!this->is_constant)
                    rs_error("This value is not a constant.");

                return constant_value;
            };

            virtual void update_constant_value(lexer* lex)
            {
                rs_error("ast_value cannot update_constant_value.");
            }

            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // ast_symbolable_base::instance(dumm);

                // Write self copy functions here..
                RS_REINSTANCE(dumm->value_type);

                return dumm;
            }
        };

        struct ast_value_takeplace : virtual public ast_value
        {
            ast_value_takeplace()
            {
                value_type = new ast_type(L"pending");
            }
            void update_constant_value(lexer* lex) override
            {
                // do nothing..
            }
        };

        struct ast_value_literal : virtual public ast_value
        {
            ~ast_value_literal()
            {
                if (constant_value.is_gcunit())
                {
                    delete constant_value.get_gcunit_with_barrier();
                }
            }
            void update_constant_value(lexer* lex) override
            {
                // do nothing
            }

            static rs_handle_t wstr_to_handle(const std::wstring& str)
            {
                rs_handle_t base = 10;
                rs_handle_t sum = 0;
                const wchar_t* wstr = str.c_str();

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
                return sum;
            }
            static rs_integer_t wstr_to_integer(const std::wstring& str)
            {
                rs_integer_t base = 10;
                rs_integer_t sum = 0;
                const wchar_t* wstr = str.c_str();

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
                return sum;
            }
            static rs_real_t wstr_to_real(const std::wstring& str)
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
                    constant_value.set_string_nogc(wstr_to_str(te.identifier).c_str());
                    break;
                case lex_type::l_nil:
                    constant_value.set_nil();
                    break;
                case lex_type::l_inf:
                    constant_value.set_real(INFINITY);
                    break;
                default:
                    rs_error("Unexcepted literal type.");
                    break;
                }

                value_type = new ast_type(constant_value);
            }

            void display(std::wostream& os = std::wcout, size_t lay = 0) const override
            {
                space(os, lay);
                os << L"< " << ANSI_HIC << rs_cast_string((rs_value)&constant_value) << ANSI_RST L" : " ANSI_HIG;

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
                if (constant_value.type == value::valuetype::string_type)
                    dumm->constant_value.set_string_nogc(constant_value.string->c_str());

                return dumm;
            }
        };

        struct ast_value_type_cast : public virtual ast_value
        {
            ast_value* _be_cast_value_node;
            bool implicit;
            ast_value_type_cast(ast_value* value, ast_type* type, bool _implicit)
            {
                is_constant = false;
                _be_cast_value_node = value;
                value_type = type;
                implicit = _implicit;
            }

            ast_value_type_cast() {}
            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_value::instance(dumm);

                // Write self copy functions here..
                RS_REINSTANCE(dumm->_be_cast_value_node);

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

                        value::valuetype aim_real_type = value_type->value_type;
                        if (value_type->is_dynamic())
                        {
                            aim_real_type = last_value.type;
                        }
                        else if (_be_cast_value_node->value_type->is_dynamic())
                        {
                            lex->lang_warning(0x0000, value_type, RS_WARN_OVERRIDDEN_DYNAMIC_TYPE);
                        }

                        switch (aim_real_type)
                        {
                        case value::valuetype::real_type:
                            if (last_value.is_nil() || (implicit && last_value.type != aim_real_type && last_value.type != value::valuetype::integer_type))
                                goto try_cast_nil_to_int_handle_real_str;
                            constant_value.set_real(rs_cast_real((rs_value)&last_value));
                            break;
                        case value::valuetype::integer_type:
                            if (last_value.is_nil() || (implicit && last_value.type != aim_real_type && last_value.type != value::valuetype::real_type))
                                goto try_cast_nil_to_int_handle_real_str;
                            constant_value.set_integer(rs_cast_int((rs_value)&last_value));
                            break;
                        case value::valuetype::string_type:
                            if (last_value.is_nil() || (implicit && last_value.type != aim_real_type))
                                goto try_cast_nil_to_int_handle_real_str;
                            constant_value.set_string_nogc(rs_cast_string((rs_value)&last_value));
                            break;
                        case value::valuetype::handle_type:
                            if (last_value.is_nil() || (implicit && last_value.type != aim_real_type))
                                goto try_cast_nil_to_int_handle_real_str;
                            constant_value.set_handle(rs_cast_handle((rs_value)&last_value));
                            break;
                        case value::valuetype::mapping_type:
                            if (last_value.is_nil())
                            {
                                constant_value.set_gcunit_with_barrier(value::valuetype::mapping_type);
                                break;
                            }
                            goto try_cast_nil_to_int_handle_real_str;
                            break;
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
                            goto try_cast_nil_to_int_handle_real_str;
                            break;
                        default:
                        try_cast_nil_to_int_handle_real_str:
                            if (value_type->is_dynamic() || (last_value.is_nil() && value_type->is_func()))
                            {
                                constant_value.set_val(&last_value);
                            }
                            else
                            {
                                if (implicit)
                                    lex->lang_error(0x0000, this, RS_ERR_CANNOT_IMPLCAST_TYPE_TO_TYPE,
                                        ast_type::get_name_from_type(_be_cast_value_node->value_type->value_type).c_str(),
                                        value_type->get_type_name().c_str());
                                else
                                    lex->lang_error(0x0000, this, RS_ERR_CANNOT_CAST_TYPE_TO_TYPE,
                                        ast_type::get_name_from_type(_be_cast_value_node->value_type->value_type).c_str(),
                                        value_type->get_type_name().c_str());

                                constant_value.set_nil();
                                value_type = new ast_type(constant_value);
                            }
                            break;
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
                RS_REINSTANCE(dumm->_be_cast_value_node);

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
            }
        };

        struct ast_value_type_check : public virtual ast_value
        {
            ast_value* _be_check_value_node;
            ast_type* aim_type;

            ast_value_type_check(ast_value* value, ast_type* type)
            {
                _be_check_value_node = value;
                aim_type = type;

                value_type = new ast_type(L"int");
            }
            ast_value_type_check() {}
            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_value::instance(dumm);

                // Write self copy functions here..
                RS_REINSTANCE(dumm->_be_check_value_node);
                RS_REINSTANCE(dumm->aim_type);

                return dumm;
            }

            void update_constant_value(lexer* lex) override
            {
                if (is_constant)
                    return;

                _be_check_value_node->update_constant_value(lex);

                if (!_be_check_value_node->value_type->is_pending() && !aim_type->is_pending())
                {
                    auto result = _be_check_value_node->value_type->is_same(aim_type, false);

                    if (result)
                    {
                        is_constant = true;
                        constant_value.set_integer(result);
                    }
                    if (_be_check_value_node->value_type->is_dynamic())
                    {
                        if (!aim_type->is_dynamic())
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
                            lex->parser_error(0x0000, RS_ERR_CANNOT_DECL_PUB_PRI_PRO_SAME_TIME);
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
                    lex->parser_warning(0x0000, RS_WARN_REPEAT_ATTRIBUTE);
            }

            bool is_constant_attr() const
            {
                return attributes.find(+lex_type::l_const) != attributes.end();
            }

            bool is_static_attr() const
            {
                return attributes.find(+lex_type::l_static) != attributes.end();
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
                RS_REINSTANCE(dumm->declear_attribute);

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
                    RS_REINSTANCE(tras);
                }

                return dumm;
            }
            void update_constant_value(lexer* lex) override
            {
                // TODO: constant variable here..

                // do nothing;
            }
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

        struct ast_value_binary : virtual public ast_value
        {
            // used for storing binary-operate;
            ast_value* left = nullptr;
            lex_type operate = +lex_type::l_error;
            ast_value* right = nullptr;

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

                RS_REINSTANCE(dumm->left);
                RS_REINSTANCE(dumm->right);

                return dumm;
            }

            template <typename T>
            static T binary_operate(lexer& lex, T left, T right, lex_type op_type)
            {
                if constexpr (std::is_same<T, rs_string_t>::value)
                    if (op_type != +lex_type::l_add)
                        lex.parser_error(0x0000, RS_ERR_CANNOT_CALC_STR_WITH_THIS_OP);
                if constexpr (std::is_same<T, rs_handle_t>::value)
                    if (op_type == +lex_type::l_mul || op_type == +lex_type::l_div || op_type == +lex_type::l_mod || op_type == +lex_type::l_mul_assign || op_type == +lex_type::l_div_assign || op_type == +lex_type::l_mod_assign)
                        lex.parser_error(0x0000, RS_ERR_CANNOT_CALC_HANDLE_WITH_THIS_OP);

                switch (op_type)
                {
                case lex_type::l_add:
                    if constexpr (std::is_same<T, rs_string_t>::value)
                    {
                        thread_local static std::string _tmp_string_add_result;
                        _tmp_string_add_result = left;
                        _tmp_string_add_result += right;
                        return _tmp_string_add_result.c_str();
                    }
                    else
                        return left + right;
                case lex_type::l_sub:
                    if constexpr (!std::is_same<T, rs_string_t>::value)
                        return left - right;
                case lex_type::l_mul:
                    if constexpr (!std::is_same<T, rs_string_t>::value)
                        return left * right;
                case lex_type::l_div:
                    if constexpr (!std::is_same<T, rs_string_t>::value)
                        return left / right;
                case lex_type::l_mod:
                    if constexpr (!std::is_same<T, rs_string_t>::value)
                    {
                        if constexpr (std::is_same<T, rs_real_t>::value)
                        {
                            return fmod(left, right);
                        }
                        else
                            return left % right;
                    }
                default:
                    rs_error("Grammar error: cannot calculate by this funcion");
                    break;
                }
                return T{};
            }

            static ast_type* binary_upper_type(ast_type* left_v, ast_type* right_v)
            {
                if (left_v->is_dynamic() || right_v->is_dynamic())
                {
                    return new ast_type(L"dynamic");
                }
                if (left_v->is_func() || right_v->is_func())
                {
                    return nullptr;
                }

                auto left_t = left_v->value_type;
                auto right_t = right_v->value_type;

                switch (left_t)
                {
                case value::valuetype::integer_type:
                {
                    switch (right_t)
                    {
                    case value::valuetype::integer_type:
                        return new ast_type(L"int");
                        break;
                    case value::valuetype::real_type:
                        return new ast_type(L"real");
                        break;
                    case value::valuetype::handle_type:
                        return new ast_type(L"handle");
                        break;
                    default:
                        return nullptr;
                        break;
                    }
                    break;
                }
                case value::valuetype::real_type:
                {
                    switch (right_t)
                    {
                    case value::valuetype::integer_type:
                        return new ast_type(L"real");
                        break;
                    case value::valuetype::real_type:
                        return new ast_type(L"real");
                        break;
                    default:
                        return nullptr;
                        break;
                    }
                    break;
                }
                case value::valuetype::handle_type:
                {
                    switch (right_t)
                    {
                    case value::valuetype::integer_type:
                        return new ast_type(L"handle");
                        break;
                    case value::valuetype::handle_type:
                        return new ast_type(L"handle");
                        break;
                    default:
                        return nullptr;
                        break;
                    }
                    break;
                }
                case value::valuetype::string_type:
                {
                    switch (right_t)
                    {
                    case value::valuetype::string_type:
                        return new ast_type(L"string");
                        break;
                    default:
                        return nullptr;
                        break;
                    }
                    break;
                }
                case value::valuetype::array_type:
                {
                    switch (right_t)
                    {
                    case value::valuetype::array_type:
                        return new ast_type(L"array");
                        break;
                    default:
                        return nullptr;
                        break;
                    }
                    break;
                }
                case value::valuetype::mapping_type:
                {
                    switch (right_t)
                    {
                    case value::valuetype::array_type:
                        return new ast_type(L"map");
                        break;
                    default:
                        return nullptr;
                        break;
                    }
                    break;
                }
                default:
                    return nullptr;
                    break;
                }
            }

            void update_constant_value(lexer* lex) override
            {
                if (is_constant)
                    return;

                left->update_constant_value(lex);
                right->update_constant_value(lex);
                if (left->value_type->is_dynamic() || right->value_type->is_dynamic())
                {
                    value_type = new ast_type(L"dynamic");
                }
                else if (left->value_type->is_pending() || right->value_type->is_pending())
                {
                    value_type = new ast_type(L"pending");
                }
                else
                {
                    if (nullptr == (value_type = binary_upper_type(left->value_type, right->value_type)))
                    {
                        lex->lang_error(0x0000, this, RS_ERR_CANNOT_CALC_WITH_L_AND_R);
                        value_type = new ast_type(L"pending");
                        return;
                    }
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
                                rs_cast_int((rs_value)&_left_val),
                                rs_cast_int((rs_value)&_right_val),
                                operate));
                        break;
                    case value::valuetype::real_type:
                        constant_value.set_real(
                            binary_operate(*lex,
                                rs_cast_real((rs_value)&_left_val),
                                rs_cast_real((rs_value)&_right_val),
                                operate));
                        break;
                    case value::valuetype::handle_type:
                        constant_value.set_handle(
                            binary_operate(*lex,
                                rs_cast_handle((rs_value)&_left_val),
                                rs_cast_handle((rs_value)&_right_val),
                                operate));
                        break;
                    case value::valuetype::string_type:
                    {
                        std::string left_str = rs_cast_string((rs_value)&_left_val);
                        std::string right_str = rs_cast_string((rs_value)&_right_val);
                        constant_value.set_string_nogc(
                            binary_operate(*lex,
                                (rs_string_t)left_str.c_str(),
                                (rs_string_t)right_str.c_str(),
                                operate));
                    }
                    break;
                    default:
                        lex->lang_error(0x0000, this, RS_ERR_CANNOT_CALC_WITH_L_AND_R);
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

                RS_REINSTANCE(dumm->in_scope_sentence);

                return dumm;
            }
        };

        struct ast_varref_defines : virtual public ast_defines
        {
            struct varref_define
            {
                bool is_ref;
                std::wstring ident_name;
                ast_value* init_val;
                lang_symbol* symbol = nullptr;
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
                    os << (vr_define.is_ref ? "ref " : "var ") << vr_define.ident_name << L" = " << std::endl;
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
                    RS_REINSTANCE(varref.init_val);
                }

                return dumm;
            }
        };

        struct ast_value_arg_define : virtual ast_value_symbolable_base, virtual ast_defines
        {
            bool is_ref = false;
            std::wstring arg_name;

            void display(std::wostream& os = std::wcout, size_t lay = 0) const override
            {
                space(os, lay);
                os << L"< " << ANSI_HIY << (is_ref ? L"ref " : L"var ")
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
            rs_extern_native_func_t externed_func = nullptr;

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

            rs::value& get_constant_value() override
            {
                if (!this->is_constant)
                    rs_error("Not externed_func.");

                rs_assert(externed_func_info && externed_func_info->externed_func);

                constant_value.set_handle((rs_handle_t)externed_func_info->externed_func);
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

                RS_REINSTANCE(dumm->argument_list);
                RS_REINSTANCE(dumm->in_function_sentence);

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

                RS_REINSTANCE(dumm->return_value);

                rs_assert(!located_function);

                return dumm;
            }
        };

        struct ast_value_funccall : virtual public ast_value
        {
            ast_value* called_func;
            ast_list* arguments;

            ast_value* directed_value_from = nullptr;
            ast_value_variable* callee_symbol_in_type_namespace = nullptr;

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

                RS_REINSTANCE(dumm->called_func);
                RS_REINSTANCE(dumm->arguments);
                RS_REINSTANCE(dumm->directed_value_from);
                RS_REINSTANCE(dumm->callee_symbol_in_type_namespace);

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
                rs_test(array_items);
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

                RS_REINSTANCE(dumm->array_items);

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
                rs_test(mapping_pairs);
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

                RS_REINSTANCE(dumm->mapping_pairs);

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
                rs_test(sentence_list);
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

                RS_REINSTANCE(dumm->sentence_list);

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
                rs_test(judgement_value && execute_if_true);
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

                RS_REINSTANCE(dumm->judgement_value);
                RS_REINSTANCE(dumm->execute_if_true);
                RS_REINSTANCE(dumm->execute_else);

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
                rs_test(judgement_value && execute_sentence);
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

                RS_REINSTANCE(dumm->judgement_value);
                RS_REINSTANCE(dumm->execute_sentence);

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

                RS_REINSTANCE(dumm->left);
                RS_REINSTANCE(dumm->right);

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

            ast_value_logical_binary()
            {
                value_type = new ast_type(L"int");
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

                RS_REINSTANCE(dumm->left);
                RS_REINSTANCE(dumm->right);

                return dumm;
            }

            void update_constant_value(lexer* lex) override
            {
                if (is_constant)
                    return;

                left->update_constant_value(lex);
                right->update_constant_value(lex);
                if (left->is_constant && right->is_constant)
                {
                    is_constant = true;
                    value_type = new ast_type(L"int");

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
                        {
                            if (right->value_type->is_integer())
                                constant_value.set_integer(left->get_constant_value().integer == right->get_constant_value().integer);
                            else if (right->value_type->is_real())
                                constant_value.set_integer(left->get_constant_value().integer == right->get_constant_value().real);
                            else
                                lex->lang_error(0x0000, this, RS_ERR_CANNOT_CALC_WITH_L_AND_R);
                        }
                        else if (left->value_type->is_real())
                        {
                            if (right->value_type->is_integer())
                                constant_value.set_integer(left->get_constant_value().real == right->get_constant_value().integer);
                            else if (right->value_type->is_real())
                                constant_value.set_integer(left->get_constant_value().real == right->get_constant_value().real);
                            else
                                lex->lang_error(0x0000, this, RS_ERR_CANNOT_CALC_WITH_L_AND_R);
                        }
                        else if (left->value_type->is_string())
                        {
                            if (right->value_type->is_string())
                                constant_value.set_integer(*left->get_constant_value().string == *right->get_constant_value().string);
                            else
                                lex->lang_error(0x0000, this, RS_ERR_CANNOT_CALC_WITH_L_AND_R);
                        }
                        else
                            constant_value.set_integer(left->get_constant_value().handle == right->get_constant_value().handle);

                        if (operate == +lex_type::l_not_equal)
                            constant_value.set_integer(!constant_value.integer);
                        break;

                    case lex_type::l_less:
                    case lex_type::l_larg_or_equal:
                        if (left->value_type->is_integer())
                        {
                            if (right->value_type->is_integer())
                                constant_value.set_integer(left->get_constant_value().integer < right->get_constant_value().integer);
                            else if (right->value_type->is_real())
                                constant_value.set_integer(left->get_constant_value().integer < right->get_constant_value().real);
                            else
                                lex->lang_error(0x0000, this, RS_ERR_CANNOT_CALC_WITH_L_AND_R);
                        }
                        else if (left->value_type->is_real())
                        {
                            if (right->value_type->is_integer())
                                constant_value.set_integer(left->get_constant_value().real < right->get_constant_value().integer);
                            else if (right->value_type->is_real())
                                constant_value.set_integer(left->get_constant_value().real < right->get_constant_value().real);
                            else
                                lex->lang_error(0x0000, this, RS_ERR_CANNOT_CALC_WITH_L_AND_R);
                        }
                        else if (left->value_type->is_string())
                        {
                            if (right->value_type->is_string())
                                constant_value.set_integer(*left->get_constant_value().string < *right->get_constant_value().string);
                            else
                                lex->lang_error(0x0000, this, RS_ERR_CANNOT_CALC_WITH_L_AND_R);
                        }
                        else if (left->value_type->is_handle())
                        {
                            if (right->value_type->is_handle())
                                constant_value.set_integer(left->get_constant_value().handle < right->get_constant_value().handle);
                            else
                                lex->lang_error(0x0000, this, RS_ERR_CANNOT_CALC_WITH_L_AND_R);
                        }
                        else
                            lex->lang_error(0x0000, this, RS_ERR_CANNOT_CALC_WITH_L_AND_R);


                        if (operate == +lex_type::l_larg_or_equal)
                            constant_value.set_integer(!constant_value.integer);
                        break;

                    case lex_type::l_larg:
                    case lex_type::l_less_or_equal:

                        if (left->value_type->is_integer())
                        {
                            if (right->value_type->is_integer())
                                constant_value.set_integer(left->get_constant_value().integer > right->get_constant_value().integer);
                            else if (right->value_type->is_real())
                                constant_value.set_integer(left->get_constant_value().integer > right->get_constant_value().real);
                            else
                                lex->lang_error(0x0000, this, RS_ERR_CANNOT_CALC_WITH_L_AND_R);
                        }
                        else if (left->value_type->is_real())
                        {
                            if (right->value_type->is_integer())
                                constant_value.set_integer(left->get_constant_value().real > right->get_constant_value().integer);
                            else if (right->value_type->is_real())
                                constant_value.set_integer(left->get_constant_value().real > right->get_constant_value().real);
                            else
                                lex->lang_error(0x0000, this, RS_ERR_CANNOT_CALC_WITH_L_AND_R);
                        }
                        else if (left->value_type->is_string())
                        {
                            if (right->value_type->is_string())
                                constant_value.set_integer(*left->get_constant_value().string > *right->get_constant_value().string);
                            else
                                lex->lang_error(0x0000, this, RS_ERR_CANNOT_CALC_WITH_L_AND_R);
                        }
                        else if (left->value_type->is_handle())
                        {
                            if (right->value_type->is_handle())
                                constant_value.set_integer(left->get_constant_value().handle > right->get_constant_value().handle);
                            else
                                lex->lang_error(0x0000, this, RS_ERR_CANNOT_CALC_WITH_L_AND_R);
                        }
                        else
                            lex->lang_error(0x0000, this, RS_ERR_CANNOT_CALC_WITH_L_AND_R);


                        if (operate == +lex_type::l_less_or_equal)
                            constant_value.set_integer(!constant_value.integer);
                        break;

                    default:
                        rs_error("Do not support this op.");
                    }
                }
            }
        };

        struct ast_value_index : virtual public ast_value
        {
            ast_value* from = nullptr;
            ast_value* index = nullptr;

            ast_value_index()
            {
                value_type = new ast_type(L"pending");
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

                RS_REINSTANCE(dumm->from);
                RS_REINSTANCE(dumm->index);

                return dumm;
            }

            void update_constant_value(lexer* lex) override
            {
                if (is_constant)
                    return;

                from->update_constant_value(lex);
                index->update_constant_value(lex);
                if (from->is_constant && index->is_constant)
                {
                    if (from->value_type->is_string())
                    {
                        is_constant = true;
                        value_type = new ast_type(L"string");

                        if (!index->value_type->is_integer() && !index->value_type->is_handle())
                        {
                            lex->lang_error(0x0000, this, RS_ERR_CANNOT_INDEX_STR_WITH_TYPE, value_type->get_type_name().c_str());
                            return;
                        }

                        size_t strlength = 0;
                        rs_string_t out_str = u8substr(from->get_constant_value().string->c_str(), index->get_constant_value().integer, 1, &strlength);

                        constant_value.set_string_nogc(
                            std::string(out_str, strlength).c_str());
                    }
                    else
                    {
                        // TODO: Index nil, report error in compile time?
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
                rs_assert(argindex);
                value_type = new ast_type(L"dynamic");
                can_be_assign = true;
            }

            ast_value_indexed_variadic_args() {}

            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_value::instance(dumm);
                // Write self copy functions here..

                RS_REINSTANCE(dumm->argindex);

                return dumm;
            }

            void update_constant_value(lexer* lex) override
            {
                // DO NOTHING
            }
        };

        struct ast_fakevalue_unpacked_args : virtual public ast_value
        {
            ast_value* unpacked_pack;
            rs_integer_t expand_count = 0;

            ast_fakevalue_unpacked_args(ast_value* pak, rs_integer_t _expand_count)
                : unpacked_pack(pak),
                expand_count(_expand_count)
            {
                rs_assert(unpacked_pack && _expand_count >= 0);
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

                RS_REINSTANCE(dumm->unpacked_pack);

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

            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_value::instance(dumm);
                // Write self copy functions here..

                RS_REINSTANCE(dumm->val);

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
                            lex->lang_error(0x0000, this, RS_ERR_TYPE_CANNOT_NEGATIVE, val->value_type->get_type_name().c_str());
                            return;
                        }
                    }
                    else /*if(_token.type == +lex_type::l_lnot)*/
                    {
                        value_type = new ast_type(L"int");
                        constant_value.set_integer(!val->get_constant_value().handle);
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
                rs_assert(key && val);
            }
            ast_mapping_pair() {}
            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                // ast_value::instance(dumm);
                // Write self copy functions here..

                RS_REINSTANCE(dumm->key);
                RS_REINSTANCE(dumm->val);

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
            rs_integer_t enum_val;
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
            rs_integer_t next_enum_val = 0;
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
                    RS_REINSTANCE(enumitem);
                }

                return dumm;
            }
        };

        struct ast_using_type_as : virtual public ast_defines
        {
            std::wstring new_type_identifier;
            ast_type* old_type;

            std::map<std::wstring, ast::ast_value*> class_const_index_typing;

            grammar::ast_base* instance(ast_base* child_instance = nullptr) const override
            {
                using astnode_type = decltype(MAKE_INSTANCE(this));
                auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
                if (!child_instance) *dumm = *this;
                ast_defines::instance(dumm);
                // Write self copy functions here..

                RS_REINSTANCE(dumm->old_type);

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

                RS_REINSTANCE(dumm->from);
                RS_REINSTANCE(dumm->direct_val);

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
                // ast_defines::instance(dumm);
                // Write self copy functions here..
                return dumm;
            }

        };

        struct ast_foreach : virtual public grammar::ast_base
        {
            std::vector<std::wstring> foreach_varname;
            std::vector<ast_value_variable*> foreach_var;
            ast_value_variable* iterator_var;

            ast_varref_defines* used_vars_defines; // Just used for taking place;;;

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

                for (auto& vptr : dumm->foreach_var)
                {
                    RS_REINSTANCE(vptr);
                }
                RS_REINSTANCE(dumm->iterator_var);
                RS_REINSTANCE(dumm->used_vars_defines);
                RS_REINSTANCE(dumm->iter_getting_funccall);
                RS_REINSTANCE(dumm->iter_next_judge_expr);
                RS_REINSTANCE(dumm->execute_sentences);
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

                RS_REINSTANCE(dumm->pre_execute);
                RS_REINSTANCE(dumm->judgement_expr);
                RS_REINSTANCE(dumm->after_execute);
                RS_REINSTANCE(dumm->execute_sentences);
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

        /////////////////////////////////////////////////////////////////////////////////

#define RS_NEED_TOKEN(ID) [&]() {             \
    token tk = {lex_type::l_error};           \
    if (!cast_any_to<token>(input[(ID)], tk)) \
        rs_error("Unexcepted token type.");   \
    return tk;                                \
}()
#define RS_NEED_AST(ID) [&]() {                     \
    ast_basic *nd = nullptr;                        \
    if (!cast_any_to<ast_basic *>(input[(ID)], nd)) \
        rs_error("Unexcepted ast-node type.");      \
    return nd;                                      \
}()

#define RS_IS_TOKEN(ID) [&]() {               \
    token tk = {lex_type::l_error};           \
    if (!cast_any_to<token>(input[(ID)], tk)) \
        return false;                         \
    return true;                              \
}()
#define RS_IS_AST(ID) [&]() {                       \
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
                rs_test(input.size() > pass_idx);
                return input[pass_idx];
            }
        };

        struct pass_class_define : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                // ATTRIB using/naming  IDENT <TEMPLIST> :NAMINGLIST {INSCOPE_SENTENCE}
                //   0          1        2        3           4              5              

                bool decl_naming = RS_NEED_TOKEN(1).type == +lex_type::l_naming;

                ast_using_type_as* using_type = new ast_using_type_as;
                using_type->new_type_identifier = RS_NEED_TOKEN(2).identifier;
                using_type->old_type = new ast_type(L"map");
                using_type->old_type->template_arguments[0] = new ast_type(L"string");
                using_type->declear_attribute = dynamic_cast<ast_decl_attribute*>(RS_NEED_AST(0));

                if (!ast_empty::is_empty(input[3]))
                {
                    ast_list* template_defines = dynamic_cast<ast_list*>(RS_NEED_AST(3));
                    rs_test(template_defines);
                    using_type->is_template_define = true;

                    ast_token* template_type = dynamic_cast<ast_token*>(template_defines->children);
                    rs_test(template_type);
                    while (template_type)
                    {
                        using_type->template_type_name_list.push_back(template_type->tokens.identifier);

                        template_type = dynamic_cast<ast_token*>(template_type->sibling);
                    }
                }

                // TYPE DECLS STORE IN 5

                bool has_custom_new_func = false;
                bool has_custom_create_func = false;

                ast_namespace* ast_cls_namespace = new ast_namespace;
                ast_cls_namespace->scope_name = using_type->new_type_identifier;

                std::vector<ast_varref_defines::varref_define*> init_mem_list;

                if (!ast_empty::is_empty(input[5]))
                {
                    ast_cls_namespace->in_scope_sentence = dynamic_cast<ast_list*>(RS_NEED_AST(5));
                    auto* ast_decl = ast_cls_namespace->in_scope_sentence->children;
                    while (ast_decl)
                    {
                        auto current_ast_node = ast_decl;
                        ast_decl = ast_decl->sibling;

                        if (auto* avfdef = dynamic_cast<ast_value_function_define*>(current_ast_node))
                        {
                            if (avfdef->function_name == L"create")
                                has_custom_create_func = true;
                            else if (avfdef->function_name == L"new"
                                && !avfdef->argument_list->children)
                                has_custom_new_func = true;
                        }
                        else
                        {
                            auto* varrefdef = dynamic_cast<ast_varref_defines*>(current_ast_node);
                            rs_assert(varrefdef);

                            for (auto& definfo : varrefdef->var_refs)
                                init_mem_list.push_back(&definfo);

                            // NOTE: Remove current varref def 
                            ast_cls_namespace->in_scope_sentence->remove_child(current_ast_node);
                        }
                    }
                }
                else
                    ast_cls_namespace->in_scope_sentence = new ast_list;

                if (!decl_naming && !has_custom_create_func)
                {
                    // MAKE FUNC DECL
                    ast_value_function_define* avfd_create = new ast_value_function_define;
                    avfd_create->function_name = L"create";
                    avfd_create->argument_list = new ast_list;
                    avfd_create->value_type = new ast_type(L"pending");
                    avfd_create->value_type->set_as_function_type();

                    avfd_create->auto_adjust_return_type = true;
                    avfd_create->declear_attribute = new ast_decl_attribute();

                    if (using_type->is_template_define)
                    {
                        avfd_create->is_template_define = true;
                        avfd_create->template_type_name_list = using_type->template_type_name_list;
                    }

                    avfd_create->in_function_sentence = new ast_list;

                    //return new()
                    ast_value_funccall* callnew = new ast_value_funccall();
                    ast_value_variable* called_funcname = new ast_value_variable(L"new");

                    for (auto& template_impl_args : avfd_create->template_type_name_list)
                    {
                        called_funcname->template_reification_args.push_back(new ast_type(template_impl_args));
                    }

                    callnew->called_func = called_funcname;
                    callnew->arguments = new ast_list();
                    callnew->value_type = new ast_type(L"pending");

                    ast_return* areturnsentence = new ast_return();
                    areturnsentence->return_value = callnew;

                    // all done ~ fuck!
                    avfd_create->in_function_sentence->append_at_end(areturnsentence);
                    ast_cls_namespace->in_scope_sentence->append_at_head(avfd_create);
                }

                if (!decl_naming && !has_custom_new_func)
                {
                    // MAKE FUNC DECL
                    ast_value_function_define* avfd_new = new ast_value_function_define;
                    avfd_new->function_name = L"new";
                    avfd_new->argument_list = new ast_list;
                    avfd_new->value_type = new ast_type(L"pending");
                    avfd_new->value_type->set_as_function_type();
                    avfd_new->auto_adjust_return_type = true;
                    avfd_new->declear_attribute = new ast_decl_attribute();

                    if (using_type->is_template_define)
                    {
                        avfd_new->is_template_define = true;
                        avfd_new->template_type_name_list = using_type->template_type_name_list;
                    }

                    avfd_new->in_function_sentence = new ast_list;


                    // return {...}:CLASS_TYPE_NAME
                    ast_list* in_instance_pairs = new ast_list();
                    for (auto map_instance_val : init_mem_list)
                    {
                        ast_value_literal* vpair_key = new ast_value_literal(
                            token{ +lex_type::l_literal_string, map_instance_val->ident_name });

                        ast_mapping_pair* vpair = new ast_mapping_pair(
                            vpair_key,
                            map_instance_val->init_val);

                        map_instance_val->init_val->is_mark_as_using_ref = map_instance_val->is_ref;

                        in_instance_pairs->append_at_end(vpair);

                    }
                    ast_value_mapping* mkinstance_value = new ast_value_mapping(in_instance_pairs);

                    ast_type* becasted_type = new ast_type(using_type->new_type_identifier);
                    for (auto& template_impl_args : avfd_new->template_type_name_list)
                    {
                        becasted_type->template_arguments.push_back(new ast_type(template_impl_args));
                    }

                    mkinstance_value->value_type = becasted_type;

                    ast_return* areturnsentence = new ast_return();
                    areturnsentence->return_value = mkinstance_value;

                    // all done ~ fuck!
                    avfd_new->in_function_sentence->append_at_end(areturnsentence);

                    ast_cls_namespace->in_scope_sentence->append_at_head(avfd_new);
                }

                for (auto* vdefinfo : init_mem_list)
                {
                    using_type->old_type->class_member_index[vdefinfo->ident_name]
                        = using_type->class_const_index_typing[vdefinfo->ident_name]
                        = vdefinfo->init_val;
                }

                if (!ast_empty::is_empty(input[4]))
                {
                    if (decl_naming)
                        lex.parser_error(0x0000, L"具名声明不可继承自其他具名，继续");
                    else
                    {
                        ast_type* depending_naming = dynamic_cast<ast_type*>(dynamic_cast<ast_list*>(RS_NEED_AST(4))->children);
                        while (depending_naming)
                        {
                            using_type->old_type->template_impl_naming_checking.push_back(depending_naming);
                            depending_naming = dynamic_cast<ast_type*>(depending_naming->sibling);
                        }
                    }
                }

                ast_list* type_decl_and_clas_body_pair = new ast_list;
                type_decl_and_clas_body_pair->append_at_end(using_type);
                type_decl_and_clas_body_pair->append_at_end(ast_cls_namespace);

                return (ast_basic*)type_decl_and_clas_body_pair;
            }
        };

        struct pass_typeof : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                auto* att = new ast_type(L"pending");
                att->typefrom = dynamic_cast<ast_value*>(RS_NEED_AST(2));

                rs_assert(att->typefrom);

                return (ast_basic*)att;
            }

        };

        struct pass_template_reification : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                auto att = dynamic_cast<ast_value_variable*>(RS_NEED_AST(0));
                if (!ast_empty::is_empty(input[1]))
                {
                    auto tsalist = dynamic_cast<ast_list*>(RS_NEED_AST(1));
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
                att->add_attribute(&lex, dynamic_cast<ast_token*>(RS_NEED_AST(0))->tokens.type);
                return (ast_basic*)att;
            }
        };

        struct pass_enum_item_create : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                ast_enum_item* item = new ast_enum_item;
                item->enum_ident = RS_NEED_TOKEN(0).identifier;
                if (input.size() == 3)
                {
                    item->need_assign_val = false;
                    auto fxxk = RS_NEED_TOKEN(2);
                    item->enum_val = ast_value_literal::wstr_to_integer(RS_NEED_TOKEN(2).identifier);
                }
                return (ast_basic*)item;
            }
        };

        struct pass_enum_declear_begin : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                ast_enum_items_list* items = new ast_enum_items_list;
                auto* enum_item = dynamic_cast<ast_enum_item*>(RS_NEED_AST(0));
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
                ast_enum_items_list* items = dynamic_cast<ast_enum_items_list*>(RS_NEED_AST(0));
                auto* enum_item = dynamic_cast<ast_enum_item*>(RS_NEED_AST(2));
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
                ast_value* val = input.size() == 4 ? dynamic_cast<ast_value*>(RS_NEED_AST(2)) : dynamic_cast<ast_value*>(RS_NEED_AST(1));

                if (!val->can_be_assign)
                {
                    lex.parser_error(0x0000, RS_ERR_CANNOT_MAKE_UNASSABLE_ITEM_REF);
                }

                val->is_mark_as_using_ref = true;
                return (ast_basic*)val;
            }
        };

        struct pass_enum_finalize : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                rs_assert(input.size() == 5);

                ast_list* bind_type_and_decl_list = new ast_list;

                ast_namespace* enum_scope = new ast_namespace;
                ast_list* decl_list = new ast_list;

                enum_scope->scope_name = RS_NEED_TOKEN(1).identifier;

                auto* using_enum_as_int = new ast_using_type_as;
                using_enum_as_int->new_type_identifier = enum_scope->scope_name;
                using_enum_as_int->old_type = new ast_type(L"int");
                bind_type_and_decl_list->append_at_end(using_enum_as_int);

                enum_scope->in_scope_sentence = decl_list;
                ast_enum_items_list* enum_items = dynamic_cast<ast_enum_items_list*>(RS_NEED_AST(3));

                ast_varref_defines* vardefs = new ast_varref_defines;
                vardefs->declear_attribute = new ast_decl_attribute;
                vardefs->declear_attribute->add_attribute(&lex, +lex_type::l_const);
                for (auto& enumitem : enum_items->enum_items)
                {
                    ast_value_literal* const_val = new ast_value_literal(
                        token{ +lex_type::l_literal_integer, std::to_wstring(enumitem->enum_val) });

                    vardefs->var_refs.push_back(
                        { false, enumitem->enum_ident, const_val });

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
                auto att = dynamic_cast<ast_decl_attribute*>(RS_NEED_AST(0));
                att->add_attribute(&lex, dynamic_cast<ast_token*>(RS_NEED_AST(1))->tokens.type);
                return (ast_basic*)att;
            }
        };

        struct pass_unary_op : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                rs_test(input.size() == 2);

                token _token = RS_NEED_TOKEN(0);
                ast_value* right_v = dynamic_cast<ast_value*>(RS_NEED_AST(1));

                rs_test(right_v);
                rs_test(lexer::lex_is_operate_type(_token.type) && (_token.type == +lex_type::l_lnot || _token.type == +lex_type::l_sub));

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
                rs_test(input.size() == 3);
                std::wstring path;
                std::wstring filename;
                auto* fx = RS_NEED_AST(1);

                ast_token* importfilepaths = dynamic_cast<ast_token*>(
                    dynamic_cast<ast_list*>(RS_NEED_AST(1))->children);
                do
                {
                    path += filename = importfilepaths->tokens.identifier;
                    importfilepaths = dynamic_cast<ast_token*>(importfilepaths->sibling);
                    if (importfilepaths)
                        path += L"/";
                } while (importfilepaths);

                // path += L".rsn";
                std::wstring srcfile, src_full_path;
                if (!rs::read_virtual_source(&srcfile, &src_full_path, path + L".rsn", &lex))
                {
                    // import a.b; cannot open a/b.rsn, trying a/b/b.rsn
                    if (!rs::read_virtual_source(&srcfile, &src_full_path, path + L"/" + filename + L".rsn", &lex))
                        return lex.parser_error(0x0000, RS_ERR_CANNOT_OPEN_FILE, path.c_str());
                }

                if (!lex.has_been_imported(src_full_path))
                {
                    lexer new_lex(srcfile, wstr_to_str(src_full_path));
                    new_lex.imported_file_list = lex.imported_file_list;

                    auto* imported_ast = rs::get_rs_grammar()->gen(new_lex);

                    lex.lex_error_list.insert(lex.lex_error_list.end(),
                        new_lex.lex_error_list.begin(),
                        new_lex.lex_error_list.end());

                    lex.lex_warn_list.insert(lex.lex_warn_list.end(),
                        new_lex.lex_warn_list.begin(),
                        new_lex.lex_warn_list.end());

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
                rs_test(input.size() == 5);
                // { x , x }

                return (grammar::ast_base*)new ast_mapping_pair(
                    dynamic_cast<ast_value*>(RS_NEED_AST(1)),
                    dynamic_cast<ast_value*>(RS_NEED_AST(3)));
            }
        };

        struct pass_unpack_args : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                rs_test(input.size() == 2 || input.size() == 3);

                if (input.size() == 2)
                {
                    return (ast_basic*)new ast_fakevalue_unpacked_args(
                        dynamic_cast<ast_value*>(RS_NEED_AST(0)),
                        0);
                }
                else
                {
                    auto expand_count = ast_value_literal::wstr_to_integer(RS_NEED_TOKEN(2).identifier);

                    if (!expand_count)
                        lex.parser_error(0x0000, RS_ERR_UNPACK_ARG_LESS_THEN_ONE);

                    return (ast_basic*)new ast_fakevalue_unpacked_args(
                        dynamic_cast<ast_value*>(RS_NEED_AST(0)),
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
                    extern_symb->symbol_name = RS_NEED_TOKEN(2).identifier;
                    extern_symb->externed_func =
                        rslib_extern_symbols::get_global_symbol(
                            wstr_to_str(extern_symb->symbol_name).c_str());

                    if (!extern_symb->externed_func)
                        lex.parser_error(0x0000, RS_ERR_CANNOT_FIND_EXT_SYM, extern_symb->symbol_name.c_str());
                }
                else if (input.size() == 6)
                {
                    // extern ( lib , symb )
                    extern_symb->load_from_lib = RS_NEED_TOKEN(2).identifier;
                    extern_symb->symbol_name = RS_NEED_TOKEN(4).identifier;
                    extern_symb->externed_func = nullptr;

                    // Load it in pass
                }
                else
                {
                    rs_error("error grammar..");
                }

                return (ast_basic*)extern_symb;
            }
        };
        struct pass_while : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                rs_test(input.size() == 5);
                return (grammar::ast_base*)new ast_while(dynamic_cast<ast_value*>(RS_NEED_AST(2)), RS_NEED_AST(4));
            }
        };
        struct pass_if : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                rs_test(input.size() == 6);
                if (ast_empty::is_empty(input[5]))
                    return (grammar::ast_base*)new ast_if(dynamic_cast<ast_value*>(RS_NEED_AST(2)), RS_NEED_AST(4), nullptr);
                else
                    return (grammar::ast_base*)new ast_if(dynamic_cast<ast_value*>(RS_NEED_AST(2)), RS_NEED_AST(4), RS_NEED_AST(5));
            }
        };

        template <size_t pass_idx>
        struct pass_sentence_block : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                rs_test(input.size() > pass_idx);
                return (grammar::ast_base*)ast_sentence_block::fast_parse_sentenceblock(RS_NEED_AST(pass_idx));
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
                rs_test(input.size() == 3);
                return (ast_basic*)new ast_value_mapping(dynamic_cast<ast_list*>(RS_NEED_AST(1)));
            }
        };

        struct pass_array_builder : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                rs_test(input.size() == 3);
                return (ast_basic*)new ast_value_array(dynamic_cast<ast_list*>(RS_NEED_AST(1)));
            }
        };

        struct pass_function_define : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                rs_test(input.size() == 8 || input.size() == 9 || input.size() == 10);

                auto* ast_func = new ast_value_function_define;
                ast_type* return_type = nullptr;
                ast_list* template_types = nullptr;

                if (input.size() == 9)
                {
                    // function with name..
                    ast_func->declear_attribute = dynamic_cast<ast_decl_attribute*>(RS_NEED_AST(0));
                    rs_assert(ast_func->declear_attribute);

                    ast_func->function_name = RS_NEED_TOKEN(2).identifier;

                    if (!ast_empty::is_empty(input[3]))
                        template_types = dynamic_cast<ast_list*>(RS_NEED_AST(3));

                    ast_func->argument_list = dynamic_cast<ast_list*>(RS_NEED_AST(5));
                    ast_func->in_function_sentence = dynamic_cast<ast_sentence_block*>(RS_NEED_AST(8))->sentence_list;
                    return_type = dynamic_cast<ast_type*>(RS_NEED_AST(7));
                }
                else if (input.size() == 8)
                {
                    // anonymous function
                    ast_func->declear_attribute = dynamic_cast<ast_decl_attribute*>(RS_NEED_AST(0));
                    rs_assert(ast_func->declear_attribute);

                    if (!ast_empty::is_empty(input[2]))
                    {
                        rs_error("Not support now...");

                        template_types = dynamic_cast<ast_list*>(RS_NEED_AST(2));
                    }

                    ast_func->function_name = L""; // just get a fucking name
                    ast_func->argument_list = dynamic_cast<ast_list*>(RS_NEED_AST(4));
                    ast_func->in_function_sentence = dynamic_cast<ast_sentence_block*>(RS_NEED_AST(7))->sentence_list;
                    return_type = dynamic_cast<ast_type*>(RS_NEED_AST(6));
                }
                else
                {
                    // function with name.. export func
                    ast_func->declear_attribute = dynamic_cast<ast_decl_attribute*>(RS_NEED_AST(1));
                    rs_assert(ast_func->declear_attribute);

                    ast_func->function_name = RS_NEED_TOKEN(3).identifier;

                    if (!ast_empty::is_empty(input[4]))
                    {
                        template_types = dynamic_cast<ast_list*>(RS_NEED_AST(4));
                    }

                    ast_func->argument_list = dynamic_cast<ast_list*>(RS_NEED_AST(6));
                    ast_func->in_function_sentence = nullptr;
                    return_type = dynamic_cast<ast_type*>(RS_NEED_AST(8));


                    ast_func->externed_func_info = dynamic_cast<ast_extern_info*>(RS_NEED_AST(0));
                    if (ast_func->externed_func_info->externed_func)
                        ast_func->is_constant = true;
                }
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
                            return lex.parser_error(0x0000, RS_ERR_ARG_DEFINE_AFTER_VARIADIC);

                        ast_func->value_type->append_function_argument_type(arg_node->value_type);
                    }
                    else if (auto* arg_variadic = dynamic_cast<ast_token*>(argchild))
                        ast_func->value_type->set_as_variadic_arg_func();
                    argchild = argchild->sibling;
                }

                if (template_types)
                {
                    ast_func->is_template_define = true;
                    ast_token* templatedef = dynamic_cast<ast_token*>(template_types->children);
                    rs_assert(templatedef);

                    while (templatedef)
                    {
                        ast_func->template_type_name_list.push_back(templatedef->tokens.identifier);

                        templatedef = dynamic_cast<ast_token*>(templatedef->sibling);
                    }
                }

                // if ast_func->in_function_sentence == nullptr it means this function have no sentences...
                return (grammar::ast_base*)ast_func;
            }
        };

        struct pass_function_call : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                rs_test(input.size() == 4);

                auto* result = new ast_value_funccall;

                result->arguments = dynamic_cast<ast_list*>(RS_NEED_AST(2));

                auto* callee = RS_NEED_AST(0);
                if (ast_directed_values* adv = dynamic_cast<ast_directed_values*>(RS_NEED_AST(0)))
                {
                    result->called_func = adv->direct_val;
                    result->arguments->append_at_head(adv->from);
                    result->directed_value_from = adv->from;
                }
                else
                    result->called_func = dynamic_cast<ast_value*>(RS_NEED_AST(0));

                result->value_type = result->called_func->value_type->get_return_type(); // just get pending..
                result->can_be_assign = true;
                return (ast_basic*)result;
            }
        };

        struct pass_directed_value_for_call : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                rs_test(input.size() == 3);

                auto* result = new ast_directed_values();
                auto* from = dynamic_cast<ast_value*>(RS_NEED_AST(0));
                auto* to = dynamic_cast<ast_value*>(RS_NEED_AST(2));
                result->from = from;
                result->direct_val = to;

                return (ast_basic*)result;
            }
        };

        struct pass_literal : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                rs_test(input.size() == 1);
                return (grammar::ast_base*)new ast_value_literal(RS_NEED_TOKEN(0));
            }
        };

        struct pass_namespace : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                rs_test(input.size() == 3);
                if (ast_empty::is_empty(input[2]))
                    return input[2];

                ast_namespace* result = new ast_namespace();
                result->scope_name = RS_NEED_TOKEN(1).identifier;

                auto* list = dynamic_cast<ast_sentence_block*>(RS_NEED_AST(2));
                rs_test(list);
                result->in_scope_sentence = list->sentence_list;

                return (ast_basic*)result;
            }
        };

        struct pass_begin_varref_define : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                rs_test(input.size() == 3);
                ast_varref_defines* result = new ast_varref_defines;

                ast_value* init_val = dynamic_cast<ast_value*>(RS_NEED_AST(2));
                rs_test(init_val);

                result->var_refs.push_back(
                    { false, RS_NEED_TOKEN(0).identifier, init_val });

                return (ast_basic*)result;
            }
        };
        struct pass_add_varref_define : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                rs_test(input.size() == 5);
                ast_varref_defines* result = dynamic_cast<ast_varref_defines*>(RS_NEED_AST(0));

                ast_value* init_val = dynamic_cast<ast_value*>(RS_NEED_AST(4));
                rs_test(result && init_val);

                result->var_refs.push_back(
                    { false, RS_NEED_TOKEN(2).identifier, init_val });

                return (ast_basic*)result;
            }
        };
        struct pass_mark_as_ref_define : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                rs_test(input.size() == 4);
                ast_varref_defines* result = dynamic_cast<ast_varref_defines*>(RS_NEED_AST(2));
                rs_test(result);

                result->declear_attribute = dynamic_cast<ast_decl_attribute*>(RS_NEED_AST(0));
                rs_assert(result->declear_attribute);

                for (auto& defines : result->var_refs)
                {
                    defines.is_ref = true;
                }

                return (ast_basic*)result;
            }
        };
        struct pass_mark_as_var_define : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                rs_test(input.size() == 4);
                ast_varref_defines* result = dynamic_cast<ast_varref_defines*>(RS_NEED_AST(2));
                rs_test(result);

                result->declear_attribute = dynamic_cast<ast_decl_attribute*>(RS_NEED_AST(0));
                rs_assert(result->declear_attribute);

                return (ast_basic*)result;
            }
        };

        /*struct pass_type_decl :public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                rs_test(input.size() == 2);

                token tk = RS_NEED_TOKEN(1);

                if (tk.type == +lex_type::l_identifier)
                {
                    return (grammar::ast_base*)new ast_type(tk.identifier);
                }

                rs_error("Unexcepted token type.");
                return 0;
            }
        };*/

        struct pass_type_cast : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                rs_test(input.size() == 2);

                ast_value* value_node;
                ast_type* type_node;
                if ((value_node = dynamic_cast<ast_value*>(RS_NEED_AST(0))) && (type_node = dynamic_cast<ast_type*>(RS_NEED_AST(1))))
                {

                    if (auto* array_builder = dynamic_cast<ast_value_array*>(value_node))
                    {
                        array_builder->value_type = type_node;
                        return (ast_basic*)array_builder;
                    }

                    if (auto* map_builder = dynamic_cast<ast_value_mapping*>(value_node))
                    {
                        map_builder->value_type = type_node;
                        return (ast_basic*)map_builder;
                    }

                    ast_value_type_cast* typecast = new ast_value_type_cast(value_node, type_node, false);
                    typecast->update_constant_value(&lex);
                    return (ast_basic*)typecast;
                }

                rs_error("Unexcepted token type.");
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
                else if (!value_node->value_type->is_same(type_node))
                {
                    lex.parser_error(0x0000, RS_ERR_CANNOT_AS_TYPE, value_node->value_type->get_type_name().c_str(), type_node->get_type_name().c_str());
                    return value_node;
                }
                return value_node;
            }

            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                rs_test(input.size() == 2);

                ast_value* value_node;
                ast_type* type_node;
                if (value_node = dynamic_cast<ast_value*>(RS_NEED_AST(0)))
                {
                    if (type_node = dynamic_cast<ast_type*>(RS_NEED_AST(1)))
                        return (ast_basic*)do_judge(lex, value_node, type_node);
                    return (ast_basic*)value_node;
                }

                rs_error("Unexcepted token type.");
                return 0;
            }
        };

        struct pass_type_check : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                rs_test(input.size() == 2);

                ast_value* value_node;
                ast_type* type_node;
                if (value_node = dynamic_cast<ast_value*>(RS_NEED_AST(0)))
                {
                    if (type_node = dynamic_cast<ast_type*>(RS_NEED_AST(1)))
                    {
                        ast_value_type_check* checking_node = new ast_value_type_check(value_node, type_node);
                        checking_node->update_constant_value(&lex);
                        return (ast_basic*)checking_node;
                    }
                    return (ast_basic*)value_node;
                }

                rs_error("Unexcepted token type.");
                return 0;
            }
        };

        struct pass_variable : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                rs_test(input.size() == 1);

                token tk = RS_NEED_TOKEN(0);

                rs_test(tk.type == +lex_type::l_identifier);
                return (grammar::ast_base*)new ast_value_variable(tk.identifier);
            }
        };

        struct pass_append_serching_namespace : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                rs_test(input.size() == 3);

                token tk = RS_NEED_TOKEN(1);
                ast_value_variable* result = dynamic_cast<ast_value_variable*>(RS_NEED_AST(2));

                rs_assert(tk.type == +lex_type::l_identifier && result);

                result->scope_namespaces.insert(result->scope_namespaces.begin(), tk.identifier);

                return (grammar::ast_base*)result;
            }
        };

        struct pass_using_namespace : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                ast_using_namespace* aunames = new ast_using_namespace();
                auto vs = dynamic_cast<ast_value_variable*>(RS_NEED_AST(2));
                rs_assert(vs);

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
                rs_test(input.size() == 2);

                if (RS_IS_TOKEN(0))
                {
                    token tk = RS_NEED_TOKEN(0);
                    ast_value_variable* result = dynamic_cast<ast_value_variable*>(RS_NEED_AST(1));

                    rs_assert((tk.type == +lex_type::l_identifier || tk.type == +lex_type::l_empty) && result);
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
                    ast_type* findingfrom = dynamic_cast<ast_type*>(RS_NEED_AST(0));
                    ast_value_variable* result = dynamic_cast<ast_value_variable*>(RS_NEED_AST(1));

                    result->searching_from_type = findingfrom;
                    return (grammar::ast_base*)result;
                }

            }
        };

        struct pass_variable_in_namespace : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                rs_test(input.size() == 2);

                token tk = RS_NEED_TOKEN(1);

                rs_test(tk.type == +lex_type::l_identifier);

                return (grammar::ast_base*)new ast_value_variable(tk.identifier);
            }
        };

        template <size_t first_node>
        struct pass_create_list : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                rs_test(first_node < input.size());

                ast_list* result = new ast_list();
                if (ast_empty::is_empty(input[first_node]))
                    return (grammar::ast_base*)result;

                ast_basic* _node = RS_NEED_AST(first_node);

                result->append_at_end(_node);
                return (grammar::ast_base*)result;
            }
        };

        template <size_t from, size_t to_list>
        struct pass_append_list : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                rs_test(input.size() > std::max(from, to_list));

                ast_list* list = dynamic_cast<ast_list*>(RS_NEED_AST(to_list));
                if (list)
                {
                    if (ast_empty::is_empty(input[from]))
                        return (grammar::ast_base*)list;

                    if (from < to_list)
                    {
                        list->append_at_head(RS_NEED_AST(from));
                    }
                    else if (from > to_list)
                    {
                        list->append_at_end(RS_NEED_AST(from));
                    }
                    else
                    {
                        rs_error("You cannot add list to itself.");
                    }
                    return (grammar::ast_base*)list;
                }
                rs_error("Unexcepted token type, should be 'ast_list' or inherit from 'ast_list'.");
                return 0;
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
                rs_test(input.size() >= 3);

                ast_value* left_v = dynamic_cast<ast_value*>(RS_NEED_AST(0));
                ast_value* right_v = dynamic_cast<ast_value*>(RS_NEED_AST(2));
                rs_test(left_v && right_v);

                token _token = RS_NEED_TOKEN(1);
                rs_test(lexer::lex_is_operate_type(_token.type));

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
                rs_test(input.size() >= 3);

                ast_value* left_v = dynamic_cast<ast_value*>(RS_NEED_AST(0));
                ast_value* right_v = dynamic_cast<ast_value*>(RS_NEED_AST(2));
                rs_test(left_v && right_v);

                token _token = RS_NEED_TOKEN(1);
                rs_test(lexer::lex_is_operate_type(_token.type));

                if (left_v->is_constant)
                    return lex.parser_error(0x0000, RS_ERR_CANNOT_ASSIGN_TO_CONSTANT);

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

                rs_test(input.size() >= 3);

                ast_value* left_v = dynamic_cast<ast_value*>(RS_NEED_AST(0));
                ast_value* right_v = dynamic_cast<ast_value*>(RS_NEED_AST(2));
                rs_test(left_v && right_v);

                token _token = RS_NEED_TOKEN(1);
                rs_test(lexer::lex_is_operate_type(_token.type));

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
                rs_test(input.size() >= 3);

                ast_value* left_v = dynamic_cast<ast_value*>(RS_NEED_AST(0));
                token _token = RS_NEED_TOKEN(1);

                if (_token.type == +lex_type::l_index_begin)
                {
                    ast_value* right_v = dynamic_cast<ast_value*>(RS_NEED_AST(2));
                    rs_test(left_v && right_v);

                    if (auto* apcked_varg = dynamic_cast<ast_value_packed_variadic_args*>(left_v))
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
                    token right_tk = RS_NEED_TOKEN(2);
                    rs_test(left_v && right_tk.type == +lex_type::l_identifier);

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

                rs_error("Unexcepted token type.");
                return lex.parser_error(0x0000, L"Unexcepted token type.");
            }
        };

        struct pass_build_complex_type : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                ast_type* result = nullptr;

                auto* complex_type = dynamic_cast<ast_type*>(RS_NEED_AST(0));

                rs_test(complex_type);

                if (complex_type->is_func())
                {
                    // complex type;
                    rs_test(complex_type && input.size() == 2 && !ast_empty::is_empty(input[1]));
                    result = new ast_type(complex_type);
                }
                else
                    result = complex_type;

                if (input.size() == 1 || ast_empty::is_empty(input[1]))
                {
                    return (ast_basic*)result;
                }
                else
                {
                    result->set_as_function_type();
                    auto* arg_list = dynamic_cast<ast_list*>(RS_NEED_AST(1));
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
                            rs_test(child->sibling == nullptr && tktype && tktype->tokens.type == +lex_type::l_variadic_sign);
                            //must be last elem..

                            result->set_as_variadic_arg_func();
                        }

                        child = child->sibling;
                    }
                    return (ast_basic*)result;
                }
            }
        };
        struct pass_build_type_may_template : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                ast_type* result = nullptr;

                auto* scoping_type = dynamic_cast<ast_value_variable*>(RS_NEED_AST(0));
                rs_test(scoping_type);
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
                    ast_list* template_arg_list = dynamic_cast<ast_list*>(RS_NEED_AST(1));

                    std::vector<ast_type*> template_args;
                    rs_test(template_arg_list);
                    ast_type* type = dynamic_cast<ast_type*>(template_arg_list->children);
                    while (type)
                    {
                        template_args.push_back(type);
                        type = dynamic_cast<ast_type*>(type->sibling);
                    }
                    result->template_arguments = template_args;
                    if (result->is_array())
                        if (result->template_arguments.size() != 1)
                            lex.parser_error(0x0000, RS_ERR_ARRAY_NEED_ONE_TEMPLATE_ARG);
                    if (result->is_map())
                        if (result->template_arguments.size() != 2)
                            lex.parser_error(0x0000, RS_ERR_MAP_NEED_TWO_TEMPLATE_ARG);

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
                using_type->new_type_identifier = RS_NEED_TOKEN(2).identifier;
                using_type->old_type = dynamic_cast<ast_type*>(RS_NEED_AST(5));
                using_type->declear_attribute = dynamic_cast<ast_decl_attribute*>(RS_NEED_AST(0));

                if (!ast_empty::is_empty(input[3]))
                {
                    ast_list* template_defines = dynamic_cast<ast_list*>(RS_NEED_AST(3));
                    rs_test(template_defines);
                    using_type->is_template_define = true;

                    ast_token* template_type = dynamic_cast<ast_token*>(template_defines->children);
                    rs_test(template_type);
                    while (template_type)
                    {
                        using_type->template_type_name_list.push_back(template_type->tokens.identifier);

                        template_type = dynamic_cast<ast_token*>(template_type->sibling);
                    }
                }

                return (ast_basic*)using_type;
            }
        };

        struct pass_token : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                return (grammar::ast_base*)new ast_token(RS_NEED_TOKEN(0));
            }
        };

        struct pass_return : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                ast_return* result = new ast_return();
                if (input.size() == 3)
                {
                    if (!ast_empty::is_empty(input[1]))
                    {
                        result->return_value = dynamic_cast<ast_value*>(RS_NEED_AST(1));
                    }
                }
                else // ==4
                {
                    result->return_value = dynamic_cast<ast_value*>(RS_NEED_AST(2));
                    result->return_value->is_mark_as_using_ref = true;
                }
                return (grammar::ast_base*)result;
            }
        };

        struct pass_func_argument : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                ast_value_arg_define* arg_def = new ast_value_arg_define;
                arg_def->declear_attribute = dynamic_cast<ast_decl_attribute*>(RS_NEED_AST(0));
                rs_assert(arg_def->declear_attribute);

                arg_def->is_ref = RS_NEED_TOKEN(1).type == +lex_type::l_ref;
                arg_def->arg_name = RS_NEED_TOKEN(2).identifier;
                if (input.size() == 4)
                    arg_def->value_type = dynamic_cast<ast_type*>(RS_NEED_AST(3));
                else
                    arg_def->value_type = new ast_type(L"dynamic");

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
                ast_varref_defines* used_variables = new ast_varref_defines();

                ast_value* be_iter_value = dynamic_cast<ast_value*>(RS_NEED_AST(6));

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

                afor->used_vars_defines = new ast_varref_defines;
                afor->used_vars_defines->declear_attribute = new ast_decl_attribute;

                afor->used_vars_defines->var_refs.push_back({ false, L"_iter", exp_dir_iter_call });
                //}}}}

                    // var a= tkplace, b = tkplace...
                ast_token* a_var_defs = dynamic_cast<ast_token*>(dynamic_cast<ast_list*>(RS_NEED_AST(4))->children);
                while (a_var_defs)
                {
                    ast_value_variable* foreachvar = new ast_value_variable(a_var_defs->tokens.identifier);
                    foreachvar->is_mark_as_using_ref = true;
                    afor->foreach_varname.push_back(a_var_defs->tokens.identifier);
                    afor->foreach_var.push_back(foreachvar);
                    afor->used_vars_defines->var_refs.push_back({ false, a_var_defs->tokens.identifier, new ast_value_takeplace() });

                    a_var_defs = dynamic_cast<ast_token*>(a_var_defs->sibling);
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

                afor->execute_sentences = RS_NEED_AST(8);


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
                    result->pre_execute = RS_NEED_AST(2);
                    result->judgement_expr = dynamic_cast<ast_value*>(RS_NEED_AST(3));
                    result->after_execute = dynamic_cast<ast_value*>(RS_NEED_AST(5));
                    result->execute_sentences = RS_NEED_AST(7);
                }
                else
                {
                    if (ast_empty::is_empty(input[2]))
                        result->pre_execute = nullptr;
                    else
                        result->pre_execute = RS_NEED_AST(2);
                    result->judgement_expr = dynamic_cast<ast_value*>(RS_NEED_AST(4));
                    result->after_execute = dynamic_cast<ast_value*>(RS_NEED_AST(6));
                    result->execute_sentences = RS_NEED_AST(8);
                }

                return (ast_basic*)result;
            }

        };

        struct pass_mark_label : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                auto* result = RS_NEED_AST(2);
                result->marking_label = RS_NEED_TOKEN(0).identifier;
                return (ast_basic*)result;
            }

        };

        struct pass_break : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                if (input.size() == 2)
                    return (ast_basic*)new ast_break;
                auto result = new ast_break;
                result->label = RS_NEED_TOKEN(1).identifier;
                return (ast_basic*)result;
            }
        };
        struct pass_continue : public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                if (input.size() == 2)
                    return (ast_basic*)new ast_continue;
                auto result = new ast_continue;
                result->label = RS_NEED_TOKEN(1).identifier;
                return (ast_basic*)result;
            }
        };

        /////////////////////////////////////////////////////////////////////////////////
#if 1
        inline void init_builder()
        {
            _registed_builder_function_id_list[meta::type_hash<pass_mark_label>] = _register_builder<pass_mark_label>();

            _registed_builder_function_id_list[meta::type_hash<pass_break>] = _register_builder<pass_break>();

            _registed_builder_function_id_list[meta::type_hash<pass_continue>] = _register_builder<pass_continue>();

            _registed_builder_function_id_list[meta::type_hash<pass_forloop>] = _register_builder<pass_forloop>();

            _registed_builder_function_id_list[meta::type_hash<pass_foreach>] = _register_builder<pass_foreach>();

            _registed_builder_function_id_list[meta::type_hash<pass_class_define>] = _register_builder<pass_class_define>();

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

            _registed_builder_function_id_list[meta::type_hash<pass_mark_as_ref_define>] = _register_builder<pass_mark_as_ref_define>();

            _registed_builder_function_id_list[meta::type_hash<pass_namespace>] = _register_builder<pass_namespace>();

            _registed_builder_function_id_list[meta::type_hash<pass_index_op>] = _register_builder<pass_index_op>();

            _registed_builder_function_id_list[meta::type_hash<pass_empty>] = _register_builder<pass_empty>();

            _registed_builder_function_id_list[meta::type_hash<pass_binary_op>] = _register_builder<pass_binary_op>();

            _registed_builder_function_id_list[meta::type_hash<pass_create_list<0>>] = _register_builder<pass_create_list<0>>();

            _registed_builder_function_id_list[meta::type_hash<pass_append_list<1, 0>>] = _register_builder<pass_append_list<1, 0>>();

            _registed_builder_function_id_list[meta::type_hash<pass_append_list<0, 1>>] = _register_builder<pass_append_list<0, 1>>();

            _registed_builder_function_id_list[meta::type_hash<pass_append_list<2, 0>>] = _register_builder<pass_append_list<2, 0>>();

            _registed_builder_function_id_list[meta::type_hash<pass_append_list<1, 2>>] = _register_builder<pass_append_list<1, 2>>();

            _registed_builder_function_id_list[meta::type_hash<pass_import_files>] = _register_builder<pass_import_files>();

            _registed_builder_function_id_list[meta::type_hash<pass_variable>] = _register_builder<pass_variable>();

            _registed_builder_function_id_list[meta::type_hash<pass_build_complex_type>] = _register_builder<pass_build_complex_type>();

            _registed_builder_function_id_list[meta::type_hash<pass_build_type_may_template>] = _register_builder<pass_build_type_may_template>();

            _registed_builder_function_id_list[meta::type_hash<pass_type_cast>] = _register_builder<pass_type_cast>();

            _registed_builder_function_id_list[meta::type_hash<pass_literal>] = _register_builder<pass_literal>();

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
#define RS_ASTBUILDER_INDEX(...) ast::index<__VA_ARGS__>()
}
