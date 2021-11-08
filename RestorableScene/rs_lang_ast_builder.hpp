#pragma once
#include "rs_compiler_parser.hpp"
#include "rs_meta.hpp"
#include "rs_basic_type.hpp"
#include "rs_env_locale.hpp"

#include <any>
#include <type_traits>

namespace rs
{
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

        template<typename T>
        size_t _register_builder()
        {
            static_assert(std::is_base_of<astnode_builder, T>::value);

            _registed_builder_function.push_back(T::build);

            return _registed_builder_function.size();

        }

        template<typename T>
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

        struct ast_type : public grammar::ast_base
        {
            std::wstring type_name;
            value::valuetype value_type;

            inline static const std::map<std::wstring, value::valuetype> name_type_pair =
            {
                {L"integer", value::valuetype::integer_type },
                {L"handle", value::valuetype::handle_type },
                {L"real", value::valuetype::real_type },
                {L"string", value::valuetype::string_type },
                {L"map", value::valuetype::mapping_type },
                {L"array", value::valuetype::array_type },
                {L"nil", value::valuetype::invalid },
            };

            static std::wstring get_name_from_type(value::valuetype _type)
            {
                for (auto& [tname, vtype] : name_type_pair)
                {
                    if (vtype == _type)
                    {
                        return tname;
                    }
                }

                return L"unknown";
            }

            static value::valuetype get_type_from_name(const std::wstring& name)
            {
                if (name_type_pair.find(name) != name_type_pair.end())
                    return name_type_pair.at(name);
                return value::valuetype::invalid;
            }

            ast_type(const std::wstring& _type_name)
            {
                type_name = _type_name;
                value_type = get_type_from_name(_type_name);
            }

            ast_type(const value& _val)
            {
                type_name = get_name_from_type(_val.type);
                value_type = _val.type;
            }

            void display(std::wostream& os = std::wcout, size_t lay = 0)const override
            {
                space(os, lay); os << L"<"
                    << ANSI_HIR
                    << L"type"
                    << ANSI_RST
                    << L" : "
                    << ANSI_HIM << type_name << ANSI_RST << L" >" << std::endl;
            }
        };

        struct ast_value : public grammar::ast_base
        {
            // this type of ast node is used for stand a value or product a value.
            // liter functioncall variable and so on will belong this type of node.
            ast_type* value_type = nullptr;
            bool is_constant = false;
            virtual rs::value get_constant_value() const
            {
                rs_error("Cannot get constant value from 'ast_value'.");

                value _v;
                _v.set_nil();
                return _v;
            };
        };

        struct ast_literal_value : public ast_value
        {
            value _constant_value;

            ~ast_literal_value()
            {
                if (_constant_value.is_gcunit())
                {
                    delete _constant_value.get_gcunit_with_barrier();
                }
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

            ast_literal_value()
            {
                is_constant = true;
                _constant_value.set_nil();
            }

            ast_literal_value(token& te)
            {
                is_constant = true;

                switch (te.type)
                {
                case lex_type::l_literal_handle:
                    _constant_value.set_handle(wstr_to_handle(te.identifier)); break;
                case lex_type::l_literal_integer:
                    _constant_value.set_integer(wstr_to_integer(te.identifier)); break;
                case lex_type::l_literal_real:
                    _constant_value.set_real(wstr_to_real(te.identifier)); break;
                case lex_type::l_literal_string:
                    _constant_value.set_string_nogc(wstr_to_str(te.identifier).c_str()); break;
                case lex_type::l_nil:
                    _constant_value.set_nil(); break;
                case lex_type::l_inf:
                    _constant_value.set_real(INFINITY); break;
                default:
                    rs_error("Unexcepted literal type.");
                    break;
                }

                value_type = new ast_type(_constant_value);
            }

            void display(std::wostream& os = std::wcout, size_t lay = 0)const override
            {
                space(os, lay); os << L"<" << ANSI_HIC << rs_cast_string((rs_value)&_constant_value) << ANSI_RST L" : " ANSI_HIG;

                switch (_constant_value.type)
                {
                case value::valuetype::integer_type:
                    os << L"integer"; break;
                case value::valuetype::real_type:
                    os << L"real"; break;
                case value::valuetype::handle_type:
                    os << L"handle"; break;
                case value::valuetype::string_type:
                    os << L"string"; break;
                case value::valuetype::mapping_type:
                    os << L"map"; break;
                case value::valuetype::array_type:
                    os << L"string"; break;
                case value::valuetype::invalid:
                    os << L"nil"; break;
                default:
                    break;
                }

                os << ANSI_RST L" >" << std::endl;
            }

            rs::value get_constant_value() const override
            {
                value _v;
                _v.set_val(&_constant_value);
                return _v;
            };
        };

        struct ast_type_cast_value : public ast_value
        {
            ast_value* _be_cast_value_node;
            ast_type_cast_value(ast_value* value, ast_type* type)
            {
                is_constant = false;
                _be_cast_value_node = value;
                value_type = type;
            }

            void display(std::wostream& os = std::wcout, size_t lay = 0)const override
            {
                space(os, lay); os << L"< " << ANSI_HIR << L"cast" << ANSI_RST << L" : >" << std::endl;
                _be_cast_value_node->display(os, lay + 1);

                space(os, lay); os << L"< " << ANSI_HIR << L" to "
                    << ANSI_HIM << value_type->type_name << ANSI_RST << L" >" << std::endl;
            }
        };

        struct ast_variable : ast_value
        {
            std::wstring var_name;

            ast_variable(const std::wstring& _var_name)
            {
                var_name = _var_name;
                value_type = new ast_type(L"nil");
            }

            void display(std::wostream& os = std::wcout, size_t lay = 0)const override
            {
                space(os, lay); os << L"< " << ANSI_HIY << L"variable: "
                    << ANSI_HIR
                    << var_name
                    << ANSI_RST
                    << L" : "
                    << ANSI_HIM << value_type->type_name << ANSI_RST << L" >" << std::endl;
            }
        };

        /////////////////////////////////////////////////////////////////////////////////

        template<size_t pass_idx>
        struct pass_direct :public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                rs_test(input.size() > pass_idx);
                return input[pass_idx];
            }
        };

        struct pass_literal :public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                rs_test(input.size() == 1);

                if (token tk = { lex_type::l_error }; cast_any_to<token>(input[0], tk))
                {
                    return (grammar::ast_base*)new ast_literal_value(tk);
                }

                rs_error("Unexcepted token type.");

                return (grammar::ast_base*)new grammar::ast_error(L"Unexcepted token type.");
            }
        };

        struct pass_type_decl :public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                rs_test(input.size() == 2);

                if (token tk = { lex_type::l_error }; cast_any_to<token>(input[1], tk))
                {
                    if (tk.type == +lex_type::l_identifier)
                    {
                        return (grammar::ast_base*)new ast_type(tk.identifier);
                    }
                }
                rs_error("Unexcepted token type.");

                return (ast_basic*)new grammar::ast_error(L"Unexcepted token type.");
            }
        };

        struct pass_type_cast :public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                rs_test(input.size() == 2);

                ast_basic* _value_node = nullptr;
                cast_any_to<ast_basic*>(input[0], _value_node);

                ast_basic* _type_node = nullptr;
                cast_any_to<ast_basic*>(input[1], _type_node);

                ast_value* value_node;
                ast_type* type_node;
                if ((value_node = dynamic_cast<ast_value*>(_value_node))
                    && (type_node = dynamic_cast<ast_type*>(_type_node)))
                {
                    if (value_node->is_constant)
                    {
                        // just cast the value!
                        value last_value = value_node->get_constant_value();
                        ast_literal_value* cast_result = new ast_literal_value;

                        switch (type_node->value_type)
                        {
                        case value::valuetype::real_type:
                            if (last_value.is_nil())
                                goto try_cast_nil_to_int_handle_real_str;
                            cast_result->_constant_value.set_real(rs_cast_real((rs_value)&last_value));
                            break;
                        case value::valuetype::integer_type:
                            if (last_value.is_nil())
                                goto try_cast_nil_to_int_handle_real_str;
                            cast_result->_constant_value.set_integer(rs_cast_integer((rs_value)&last_value));
                            break;
                        case value::valuetype::string_type:
                            if (last_value.is_nil())
                                goto try_cast_nil_to_int_handle_real_str;
                            cast_result->_constant_value.set_string_nogc(rs_cast_string((rs_value)&last_value));
                            break;
                        case value::valuetype::handle_type:
                            if (last_value.is_nil())
                                goto try_cast_nil_to_int_handle_real_str;
                            cast_result->_constant_value.set_handle(rs_cast_handle((rs_value)&last_value));
                            break;
                        case value::valuetype::mapping_type:
                            if (last_value.is_nil())
                            {
                                cast_result->_constant_value.set_gcunit_with_barrier(value::valuetype::mapping_type);
                                break;
                            }
                        case value::valuetype::array_type:
                            if (last_value.is_nil())
                            {
                                cast_result->_constant_value.set_gcunit_with_barrier(value::valuetype::array_type);
                                break;
                            }
                        default:
                        try_cast_nil_to_int_handle_real_str:
                            lex.lex_error(0x0000, L"Can not cast this value to '%s'.", type_node->type_name.c_str());
                            cast_result->_constant_value.set_nil();
                            break;
                        }
                        return (ast_basic*)cast_result;
                    }
                    else
                    {
                        return (ast_basic*)new ast_type_cast_value(value_node, type_node);
                    }
                }

                rs_error("Unexcepted token type.");

                return (ast_basic*)new grammar::ast_error(L"Unexcepted token type.");
            }
        };

        struct pass_variable :public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                rs_test(input.size() == 1);

                if (token tk = { lex_type::l_error }; cast_any_to<token>(input[0], tk))
                {
                    if (tk.type == +lex_type::l_identifier)
                        return (grammar::ast_base*)new ast_variable(tk.identifier);
                }

                rs_error("Unexcepted token type.");

                return (grammar::ast_base*)new grammar::ast_error(L"Unexcepted token type.");
            }
        };

        /////////////////////////////////////////////////////////////////////////////////
#if 1
        inline void init_builder()
        {
            _registed_builder_function_id_list[meta::type_hash<pass_variable>]
                = _register_builder<pass_variable>();

            _registed_builder_function_id_list[meta::type_hash<pass_type_decl>]
                = _register_builder<pass_type_decl>();

            _registed_builder_function_id_list[meta::type_hash<pass_type_cast>]
                = _register_builder<pass_type_cast>();

            _registed_builder_function_id_list[meta::type_hash<pass_literal>]
                = _register_builder<pass_literal>();

            _registed_builder_function_id_list[meta::type_hash<pass_direct<0>>]
                = _register_builder<pass_direct<0>>();
            _registed_builder_function_id_list[meta::type_hash<pass_direct<1>>]
                = _register_builder<pass_direct<1>>();
            _registed_builder_function_id_list[meta::type_hash<pass_direct<2>>]
                = _register_builder<pass_direct<2>>();
            _registed_builder_function_id_list[meta::type_hash<pass_direct<3>>]
                = _register_builder<pass_direct<3>>();
            _registed_builder_function_id_list[meta::type_hash<pass_direct<4>>]
                = _register_builder<pass_direct<4>>();
            _registed_builder_function_id_list[meta::type_hash<pass_direct<5>>]
                = _register_builder<pass_direct<5>>();

        }

    }

    inline static grammar::rule operator>>(grammar::rule ost, size_t builder_index)
    {
        ost.first.builder_index = builder_index;
        return ost;
    }
#endif
#define RS_ASTBUILDER_INDEX(T) ast::index<T>()
}
