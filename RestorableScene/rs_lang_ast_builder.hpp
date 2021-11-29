#pragma once
#include "rs_compiler_parser.hpp"
#include "rs_meta.hpp"
#include "rs_basic_type.hpp"
#include "rs_env_locale.hpp"

#include <any>
#include <type_traits>
#include <cmath>

namespace rs
{
    struct lang_symbol;
    struct lang_scope;

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

        struct ast_type : virtual public grammar::ast_base
        {
            bool is_function_type = false;
            bool is_variadic_function_type = false;

            // if this type is function, following type information will describe the return type;
            std::wstring type_name;
            ast_type* complex_type = nullptr;

            value::valuetype value_type;
            bool is_pending_type = false;

            std::vector<ast_type*> argument_types;



            inline static const std::map<std::wstring, value::valuetype> name_type_pair =
            {
                {L"int", value::valuetype::integer_type },
                {L"handle", value::valuetype::handle_type },
                {L"real", value::valuetype::real_type },
                {L"string", value::valuetype::string_type },
                {L"map", value::valuetype::mapping_type },
                {L"array", value::valuetype::array_type },
                {L"nil", value::valuetype::invalid },

                // special type
                {L"void", value::valuetype::invalid },
                {L"pending", value::valuetype::invalid },
                {L"dynamic", value::valuetype::invalid },
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
                if (from->is_dynamic() || to->is_dynamic())
                    return true;

                if (from->is_pending() || to->is_pending())
                    return false;

                if (from->is_same(to))
                    return true;

                if (from->is_func() || to->is_func())
                    return false;

                if (from->is_nil())
                {
                    if (to->value_type == value::valuetype::array_type
                        || to->value_type == value::valuetype::mapping_type
                        || to->is_func()
                        || to->is_nil())
                        return true;
                    return false;
                }
                if (to->is_nil())
                    return false;

                if (to->value_type == value::valuetype::array_type
                    || to->value_type == value::valuetype::mapping_type
                    || to->is_func())
                {
                    return to->is_same(from);
                }

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
                is_pending_type = false;// reset state;
                if (is_pending())
                {
                    is_pending_type = true;
                }
                else
                {
                    is_pending_type = false;
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
                    is_pending_type = false;// reset state;
                    if (is_pending())
                    {
                        is_pending_type = true;
                    }
                    else
                    {
                        is_pending_type = false;
                    }
                }
                else
                {
                    // simplx
                    set_type_with_name(_type->type_name);
                }
            }

            ast_type(const std::wstring& _type_name)
            {
                set_type_with_name(_type_name);
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

                return new ast_type(type_name);
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
            bool is_pending() const
            {
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
            bool is_same(const ast_type* another)const
            {
                rs_test(!is_pending() && !another->is_pending());
                if (is_func())
                {
                    if (argument_types.size() != another->argument_types.size())
                        return false;
                    for (size_t index = 0; index < argument_types.size(); index++)
                    {
                        if (!argument_types[index]->is_same(another->argument_types[index]))
                            return false;
                    }
                }
                if (is_complex() && another->is_complex())
                    return complex_type->is_same(another->complex_type);
                else if (!is_complex() && !another->is_complex())
                    return get_type_name() == another->get_type_name();
                return false;
            }
            bool is_func()const
            {
                return is_function_type;
            }
            bool is_complex()const
            {
                return complex_type;
            }

            std::wstring get_type_name()const
            {
                std::wstring result = (is_complex() ? complex_type->get_type_name() : type_name) + (is_pending() ? L"<pending>" : L"");
                if (is_function_type)
                {
                    result += L"(";
                    for (size_t index = 0; index < argument_types.size(); index++)
                    {
                        result += argument_types[index]->get_type_name();
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



            void display(std::wostream& os = std::wcout, size_t lay = 0)const override
            {
                space(os, lay); os << L"<"
                    << ANSI_HIR
                    << L"type"
                    << ANSI_RST
                    << L" : "
                    << ANSI_HIM << get_type_name() << ANSI_RST << L" >" << std::endl;
            }
        };

        struct ast_value : virtual public grammar::ast_base
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

        struct ast_value_literal : virtual public ast_value
        {
            value _constant_value;

            ~ast_value_literal()
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

            ast_value_literal()
            {
                is_constant = true;
                _constant_value.set_nil();
            }

            ast_value_literal(const token& te)
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
                space(os, lay); os << L"< " << ANSI_HIC << rs_cast_string((rs_value)&_constant_value) << ANSI_RST L" : " ANSI_HIG;

                switch (_constant_value.type)
                {
                case value::valuetype::integer_type:
                    os << L"int"; break;
                case value::valuetype::real_type:
                    os << L"real"; break;
                case value::valuetype::handle_type:
                    os << L"handle"; break;
                case value::valuetype::string_type:
                    os << L"string"; break;
                case value::valuetype::mapping_type:
                    os << L"map"; break;
                case value::valuetype::array_type:
                    os << L"array"; break;
                case value::valuetype::invalid:
                    os << L"nil"; break;
                default:
                    break;
                }
                os << ANSI_RST << "(" << ANSI_HIR << value_type->get_type_name() << ANSI_RST << ")";


                os << ANSI_RST L" >" << std::endl;
            }

            rs::value get_constant_value() const override
            {
                value _v;
                _v.set_val(&_constant_value);
                return _v;
            };
        };

        struct ast_value_type_cast : public ast_value
        {
            ast_value* _be_cast_value_node;
            ast_value_type_cast(ast_value* value, ast_type* type)
            {
                is_constant = false;
                _be_cast_value_node = value;
                value_type = type;
            }

            void display(std::wostream& os = std::wcout, size_t lay = 0)const override
            {
                space(os, lay); os << L"< " << ANSI_HIR << L"cast" << ANSI_RST << L" : >" << std::endl;
                _be_cast_value_node->display(os, lay + 1);

                space(os, lay); os << L"< " << ANSI_HIR << L"to "
                    << ANSI_HIM << value_type->get_type_name() << ANSI_RST;

                os << L" >" << std::endl;
            }
        };

        struct ast_value_symbolable_base : virtual ast_value
        {
            std::vector<std::wstring> scope_namespaces;
            bool search_from_global_namespace = false;

            lang_symbol* symbol = nullptr;
            lang_scope* searching_begin_namespace_in_pass2 = nullptr;
        };

        struct ast_value_variable : virtual ast_value_symbolable_base
        {
            std::wstring var_name;

            ast_value_variable(const std::wstring& _var_name)
            {
                var_name = _var_name;
                value_type = new ast_type(L"pending");
            }

            void display(std::wostream& os = std::wcout, size_t lay = 0)const override
            {
                space(os, lay); os << L"< " << ANSI_HIY << L"variable: "
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
        };

        struct ast_list : virtual public grammar::ast_base
        {
            void append_at_head(grammar::ast_base* astnode)
            {
                // item LIST
                if (children)
                {
                    auto bkup = children;
                    children = nullptr;
                    add_child(astnode);
                    astnode->sibling = bkup;
                }
                else
                    add_child(astnode);
            }
            void append_at_end(grammar::ast_base* astnode)
            {
                // LIST item
                add_child(astnode);
            }

            void display(std::wostream& os = std::wcout, size_t lay = 0)const override
            {
                space(os, lay); os << L"< " << ANSI_HIY << L"list" << ANSI_RST << L" >" << std::endl;
                space(os, lay); os << L"{" << std::endl;
                auto* mychild = children;
                while (mychild)
                {
                    mychild->display(os, lay + 1);

                    mychild = mychild->sibling;
                }
                space(os, lay); os << L"}" << std::endl;
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
            void display(std::wostream& os = std::wcout, size_t lay = 0)const override
            {
                /*display nothing*/
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

            void display(std::wostream& os = std::wcout, size_t lay = 0)const override
            {
                space(os, lay); os << L"< " << ANSI_HIY << L"bin-op: " << lexer::lex_is_operate_type(operate) << ANSI_RST << " >" << std::endl;
                space(os, lay); os << L"{" << std::endl;
                space(os, lay); os << L"left:" << std::endl;
                left->display(os, lay + 1);
                space(os, lay); os << L"right:" << std::endl;
                right->display(os, lay + 1);
                space(os, lay); os << L"}" << std::endl;
            }
        };

        struct ast_namespace : virtual public grammar::ast_base
        {
            std::wstring scope_name;
            ast_list* in_scope_sentence;

            void display(std::wostream& os = std::wcout, size_t lay = 0)const override
            {
                space(os, lay); os << L"< " << ANSI_HIY << L"namespace: " << scope_name << ANSI_RST << " >" << std::endl;
                in_scope_sentence->display(os, lay + 0);
            }
        };

        struct ast_varref_defines : virtual public grammar::ast_base
        {
            struct varref_define
            {
                bool is_ref;
                std::wstring ident_name;
                ast_value* init_val;
            };
            std::vector<varref_define> var_refs;
            void display(std::wostream& os = std::wcout, size_t lay = 0)const override
            {
                space(os, lay); os << L"< " << ANSI_HIY << L"defines" << ANSI_RST << " >" << std::endl;
                space(os, lay); os << L"{" << std::endl;
                for (auto& vr_define : var_refs)
                {
                    space(os, lay + 1); os << (vr_define.is_ref ? "ref " : "var ") << vr_define.ident_name << L" = " << std::endl;
                    vr_define.init_val->display(os, lay + 1);
                }
                space(os, lay); os << L"}" << std::endl;
            }
        };

        struct ast_value_arg_define : virtual ast_value_symbolable_base
        {
            bool is_ref = false;
            std::wstring arg_name;

            void display(std::wostream& os = std::wcout, size_t lay = 0)const override
            {
                space(os, lay); os << L"< " << ANSI_HIY << (is_ref ? L"ref " : L"var ")
                    << arg_name << ANSI_HIM << L" (" << value_type->get_type_name() << L")" << ANSI_RST << " >" << std::endl;
            }
        };

        struct ast_value_function_define : virtual ast_value_symbolable_base
        {
            std::wstring function_name;
            ast_list* argument_list;
            ast_list* in_function_sentence;
            bool auto_adjust_return_type = false;

            void display(std::wostream& os = std::wcout, size_t lay = 0)const override
            {
                space(os, lay); os << L"< " << ANSI_HIY << L"func "
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
        };

        struct ast_token : grammar::ast_base
        {
            token tokens;
            ast_token(const token& tk)
                :tokens(tk)
            {

            }
            void display(std::wostream& os = std::wcout, size_t lay = 0)const override
            {
                space(os, lay); os << L"< " << ANSI_HIY << tokens << ANSI_RST << L" >" << std::endl;
            }

        };

        struct ast_return : public grammar::ast_base
        {
            ast_value* return_value = nullptr;
            ast_value_function_define* located_function = nullptr;
            void display(std::wostream& os = std::wcout, size_t lay = 0)const override
            {
                space(os, lay); os << L"< " << ANSI_HIY << "return" << ANSI_RST << L" >" << std::endl;
                if (return_value)
                    return_value->display(os, lay + 1);
            }

        };

        struct ast_value_funccall : virtual public ast_value
        {
            ast_value* called_func;
            ast_list* arguments;

            void display(std::wostream& os = std::wcout, size_t lay = 0)const override
            {
                space(os, lay); os << L"< " << ANSI_HIY << "call" << ANSI_RST << L" >" << std::endl;
                if (auto* fdef = dynamic_cast<ast_value_function_define*>(called_func))
                {
                    space(os, lay + 1); os << fdef->function_name << " (" << ANSI_HIM << fdef->value_type->get_type_name() << ANSI_RST << ")" << std::endl;
                }
                else
                    called_func->display(os, lay + 1);
                space(os, lay); os << L"< " << ANSI_HIY << "args:" << ANSI_RST << L" >" << std::endl;
                arguments->display(os, lay + 1);
            }
        };

        struct ast_value_array : virtual public ast_value
        {
            ast_list* array_items;
            ast_value_array(ast_list* _items)
                :array_items(_items)
            {
                rs_test(array_items);
                value_type = new ast_type(L"array");
            }

            void display(std::wostream& os = std::wcout, size_t lay = 0)const override
            {
                space(os, lay); os << L"< " << ANSI_HIY << "array" << ANSI_RST << L" >" << std::endl;
                array_items->display(os, lay + 1);
            }
        };

        struct ast_value_mapping:virtual public ast_value
        {
            ast_list* mapping_pairs;

            ast_value_mapping(ast_list* _items)
                :mapping_pairs(_items)
            {
                rs_test(mapping_pairs);
                value_type = new ast_type(L"map");
            }

            void display(std::wostream& os = std::wcout, size_t lay = 0)const override
            {
                space(os, lay); os << L"< " << ANSI_HIY << "map" << ANSI_RST << L" >" << std::endl;
                mapping_pairs->display(os, lay + 1);
            }
        };

        /////////////////////////////////////////////////////////////////////////////////

#define RS_NEED_TOKEN(ID)[&](){token tk = { lex_type::l_error };if(!cast_any_to<token>(input[(ID)], tk)) rs_error("Unexcepted token type."); return tk;}()
#define RS_NEED_AST(ID)[&](){ast_basic* nd = nullptr;if(!cast_any_to<ast_basic*>(input[(ID)], nd)) rs_error("Unexcepted ast-node type."); return nd;}()

#define RS_IS_TOKEN(ID)[&](){token tk = { lex_type::l_error };if(!cast_any_to<token>(input[(ID)], tk))return false; return true;}()
#define RS_IS_AST(ID)[&](){ast_basic* nd = nullptr;if(!cast_any_to<ast_basic*>(input[(ID)], nd))return false; return true;}()

        template<size_t pass_idx>
        struct pass_direct :public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                rs_test(input.size() > pass_idx);
                return input[pass_idx];
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

        struct pass_function_define :public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                rs_test(input.size() == 7 || input.size() == 6);

                auto* ast_func = new ast_value_function_define;
                ast_type* return_type = nullptr;
                if (input.size() == 7)
                {
                    // function with name..
                    ast_func->function_name = RS_NEED_TOKEN(1).identifier;
                    ast_func->argument_list = dynamic_cast<ast_list*>(RS_NEED_AST(3));
                    ast_func->in_function_sentence = dynamic_cast<ast_list*>(RS_NEED_AST(6));
                    return_type = dynamic_cast<ast_type*>(RS_NEED_AST(5));

                }
                else
                {
                    // anonymous function
                    ast_func->function_name = L""; // just get a fucking name
                    ast_func->argument_list = dynamic_cast<ast_list*>(RS_NEED_AST(2));
                    ast_func->in_function_sentence = dynamic_cast<ast_list*>(RS_NEED_AST(5));
                    return_type = dynamic_cast<ast_type*>(RS_NEED_AST(4));
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
                            return lex.parser_error(0x0000, L"There should be no argument after '...'.");

                        ast_func->value_type->append_function_argument_type(arg_node->value_type);
                    }
                    else if (auto* arg_variadic = dynamic_cast<ast_token*>(argchild))
                        ast_func->value_type->set_as_variadic_arg_func();
                    argchild = argchild->sibling;
                }


                // if ast_func->in_function_sentence == nullptr it means this function have no sentences...
                return (grammar::ast_base*)ast_func;
            }
        };

        struct pass_function_call :public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                rs_test(input.size() == 4);

                auto* result = new ast_value_funccall;
                result->called_func = dynamic_cast<ast_value*>(RS_NEED_AST(0));
                result->arguments = dynamic_cast<ast_list*>(RS_NEED_AST(2));
                result->value_type = result->called_func->value_type->get_return_type(); // just get pending..

                return (ast_basic*)result;
            }
        };

        struct pass_literal :public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                rs_test(input.size() == 1);
                return (grammar::ast_base*)new ast_value_literal(RS_NEED_TOKEN(0));
            }
        };

        struct pass_namespace :public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                rs_test(input.size() == 3);
                if (ast_empty::is_empty(input[2]))
                    return input[2];

                ast_namespace* result = new ast_namespace();
                result->scope_name = RS_NEED_TOKEN(1).identifier;

                if (auto* list = dynamic_cast<ast_list*>(RS_NEED_AST(2)))
                {
                    result->in_scope_sentence = list;
                }
                else
                {
                    result->in_scope_sentence = new ast_list();
                    result->in_scope_sentence->append_at_end(RS_NEED_AST(2));
                }
                return (ast_basic*)result;
            }
        };

        struct pass_begin_varref_define :public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                rs_test(input.size() == 3);
                ast_varref_defines* result = new ast_varref_defines;

                ast_value* init_val = dynamic_cast<ast_value*>(RS_NEED_AST(2));
                rs_test(init_val);

                result->var_refs.push_back(
                    { false,RS_NEED_TOKEN(0).identifier, init_val }
                );

                return (ast_basic*)result;
            }
        };
        struct pass_add_varref_define :public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                rs_test(input.size() == 5);
                ast_varref_defines* result = dynamic_cast<ast_varref_defines*>(RS_NEED_AST(0));

                ast_value* init_val = dynamic_cast<ast_value*>(RS_NEED_AST(4));
                rs_test(result && init_val);

                result->var_refs.push_back(
                    { false, RS_NEED_TOKEN(2).identifier, init_val }
                );

                return (ast_basic*)result;
            }
        };
        struct pass_mark_as_ref_define :public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                rs_test(input.size() == 3);
                ast_varref_defines* result = dynamic_cast<ast_varref_defines*>(RS_NEED_AST(1));
                rs_test(result);

                for (auto& defines : result->var_refs)
                {
                    defines.is_ref = true;
                }

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

        struct pass_type_cast :public astnode_builder
        {
            static ast_value* do_cast(lexer& lex, ast_value* value_node, ast_type* type_node)
            {
                if (value_node->is_constant && !type_node->is_pending())
                {
                    // just cast the value!
                    value last_value = value_node->get_constant_value();
                    ast_value_literal* cast_result = new ast_value_literal;

                    value::valuetype aim_real_type = type_node->value_type;
                    if (type_node->is_dynamic())
                    {
                        aim_real_type = last_value.type;
                    }
                    else if (value_node->value_type->is_dynamic())
                    {
                        lex.lang_warning(0x0000, type_node, L"Overridden 'dynamic' attributes.");
                    }

                    switch (aim_real_type)
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
                        goto try_cast_nil_to_int_handle_real_str;
                        break;
                    case value::valuetype::array_type:
                        if (last_value.is_nil())
                        {
                            cast_result->_constant_value.set_gcunit_with_barrier(value::valuetype::array_type);
                            break;
                        }
                        goto try_cast_nil_to_int_handle_real_str;
                        break;
                    default:
                    try_cast_nil_to_int_handle_real_str:
                        if (type_node->is_dynamic() || (last_value.is_nil() && type_node->is_func()))
                        {

                        }
                        else
                        {
                            lex.lang_error(0x0000, type_node, L"Can not cast this value to '%s'.", type_node->get_type_name().c_str());
                            cast_result->_constant_value.set_nil();
                            cast_result->value_type = new ast_type(cast_result->_constant_value);
                            return cast_result;

                        }
                        break;
                    }

                    cast_result->value_type = type_node;
                    return cast_result;
                }
                else
                {
                    return new ast_value_type_cast(value_node, type_node);
                }
            }

            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                rs_test(input.size() == 2);

                ast_value* value_node;
                ast_type* type_node;
                if ((value_node = dynamic_cast<ast_value*>(RS_NEED_AST(0)))
                    && (type_node = dynamic_cast<ast_type*>(RS_NEED_AST(1))))
                {
                    return (ast_basic*)do_cast(lex, value_node, type_node);
                }

                rs_error("Unexcepted token type.");
                return 0;
            }
        };

        struct pass_variable :public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                rs_test(input.size() == 1);

                token tk = RS_NEED_TOKEN(0);

                rs_test(tk.type == +lex_type::l_identifier);
                return (grammar::ast_base*)new ast_value_variable(tk.identifier);
            }
        };

        struct pass_append_serching_namespace :public astnode_builder
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

        struct pass_finalize_serching_namespace :public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                rs_test(input.size() == 2);

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
        };

        struct pass_variable_in_namespace :public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                rs_test(input.size() == 2);

                token tk = RS_NEED_TOKEN(1);

                rs_test(tk.type == +lex_type::l_identifier);

                return (grammar::ast_base*)new ast_value_variable(tk.identifier);
            }
        };

        template<size_t first_node>
        struct pass_create_list :public astnode_builder
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

        template<size_t from, size_t to_list>
        struct pass_append_list :public astnode_builder
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

        struct pass_empty :public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                return (grammar::ast_base*)new ast_empty();
            }
        };

        struct pass_binary_op :public astnode_builder
        {
            template<typename T>
            static T binary_operate(lexer& lex, T left, T right, lex_type op_type)
            {
                if constexpr (std::is_same<T, rs_string_t>::value)
                    if (op_type != +lex_type::l_add)
                        lex.parser_error(0x0000, L"Unsupported string operations.");
                if constexpr (std::is_same<T, rs_handle_t>::value)
                    if (op_type == +lex_type::l_mul
                        || op_type == +lex_type::l_div
                        || op_type == +lex_type::l_mod
                        || op_type == +lex_type::l_mul_assign
                        || op_type == +lex_type::l_div_assign
                        || op_type == +lex_type::l_mod_assign)
                        lex.parser_error(0x0000, L"Unsupported handle operations.");

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
                case lex_type::l_assign:
                    lex.parser_error(0x0000, L"Can not assign to a constant.");
                    break;
                case lex_type::l_add_assign:
                    lex.parser_error(0x0000, L"Can not assign to a constant.");
                    break;
                case lex_type::l_sub_assign:
                    lex.parser_error(0x0000, L"Can not assign to a constant.");
                    break;
                case lex_type::l_mul_assign:
                    lex.parser_error(0x0000, L"Can not assign to a constant.");
                    break;
                case lex_type::l_div_assign:
                    lex.parser_error(0x0000, L"Can not assign to a constant.");
                    break;
                case lex_type::l_mod_assign:
                    lex.parser_error(0x0000, L"Can not assign to a constant.");
                    break;
                case lex_type::l_equal:
                    rs_error("Cannot calculate by this funcion");
                    break;
                case lex_type::l_not_equal:
                    rs_error("Cannot calculate by this funcion");
                    break;
                case lex_type::l_larg_or_equal:
                    rs_error("Cannot calculate by this funcion");
                    break;
                case lex_type::l_less_or_equal:
                    rs_error("Cannot calculate by this funcion");
                    break;
                case lex_type::l_less:
                    rs_error("Cannot calculate by this funcion");
                    break;
                case lex_type::l_larg:
                    rs_error("Cannot calculate by this funcion");
                    break;
                case lex_type::l_land:
                    rs_error("Cannot calculate by this funcion");
                    break;
                case lex_type::l_lor:
                    rs_error("Cannot calculate by this funcion");
                    break;
                default:
                    rs_error("Cannot calculate by this funcion");
                    break;
                }
                return T{};
            }

            static ast_type* binary_upper_type(ast_type* left_v, ast_type* right_v)
            {
                if (left_v->is_dynamic() || right_v->is_dynamic())
                {
                    return  new ast_type(L"dynamic");
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
                default:
                    return nullptr;
                    break;
                }
            }

            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                rs_test(input.size() >= 3);

                ast_value* left_v = dynamic_cast<ast_value*>(RS_NEED_AST(0));
                ast_value* right_v = dynamic_cast<ast_value*>(RS_NEED_AST(2));
                rs_test(left_v && right_v);

                token _token = RS_NEED_TOKEN(1);
                rs_test(lexer::lex_is_operate_type(_token.type));

                ast_type* result_type = nullptr;

                // calc type upgrade
                if (left_v->value_type->is_dynamic() || right_v->value_type->is_dynamic())
                {
                    result_type = new ast_type(L"dynamic");
                }
                else if (left_v->value_type->is_pending() || right_v->value_type->is_pending())
                {
                    result_type = new ast_type(L"pending");
                }
                else
                {
                    if (nullptr == (result_type = binary_upper_type(left_v->value_type, right_v->value_type)))
                        return lex.parser_error(0x0000, L"The value types on the left and right are incompatible.");
                }

                if (left_v->is_constant && right_v->is_constant)
                {
                    ast_value_literal* const_result = new ast_value_literal();
                    const_result->value_type = result_type;

                    value _left_val = left_v->get_constant_value();
                    value _right_val = right_v->get_constant_value();

                    switch (result_type->value_type)
                    {
                    case value::valuetype::integer_type:
                        const_result->_constant_value.set_integer(
                            binary_operate(lex,
                                rs_cast_integer((rs_value)&_left_val),
                                rs_cast_integer((rs_value)&_right_val),
                                _token.type
                            )
                        );
                        break;
                    case value::valuetype::real_type:
                        const_result->_constant_value.set_real(
                            binary_operate(lex,
                                rs_cast_real((rs_value)&_left_val),
                                rs_cast_real((rs_value)&_right_val),
                                _token.type
                            )
                        );
                        break;
                    case value::valuetype::handle_type:
                        const_result->_constant_value.set_handle(
                            binary_operate(lex,
                                rs_cast_handle((rs_value)&_left_val),
                                rs_cast_handle((rs_value)&_right_val),
                                _token.type
                            )
                        );
                        break;
                    case value::valuetype::string_type:
                    {
                        std::string left_str = rs_cast_string((rs_value)&_left_val);
                        std::string right_str = rs_cast_string((rs_value)&_right_val);
                        const_result->_constant_value.set_string_nogc(
                            binary_operate(lex,
                                (rs_string_t)left_str.c_str(),
                                (rs_string_t)right_str.c_str(),
                                _token.type
                            )
                        );
                    }
                    break;
                    default:
                        return lex.parser_error(0x0000, L"The value types on the left and right are incompatible.");
                        break;
                    }

                    return (grammar::ast_base*)const_result;
                }



                ast_value_binary* vbin = new ast_value_binary();
                vbin->left = left_v;
                vbin->operate = _token.type;
                vbin->right = right_v;
                return (grammar::ast_base*)vbin;
            }
        };

        struct pass_index_op :public astnode_builder
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

                    if (left_v->is_constant && right_v->is_constant)
                    {
                        if (left_v->value_type->value_type == value::valuetype::string_type)
                        {
                            if (right_v->value_type->value_type != value::valuetype::integer_type
                                && right_v->value_type->value_type != value::valuetype::handle_type)
                            {
                                return lex.parser_error(0x0000, L"Can not index string with this type of value.");
                            }

                            ast_value_literal* const_result = new ast_value_literal();
                            const_result->value_type = new ast_type(L"int");
                            const_result->_constant_value.set_integer(
                                (*left_v->get_constant_value().string)[right_v->get_constant_value().integer]
                            );

                            return (grammar::ast_base*)const_result;
                        }
                    }

                    ast_value_binary* vbin = new ast_value_binary();
                    vbin->left = left_v;
                    vbin->operate = lex_type::l_index_begin;
                    vbin->right = right_v;
                    return (grammar::ast_base*)vbin;
                }
                else if (_token.type == +lex_type::l_index_point)
                {
                    token right_tk = RS_NEED_TOKEN(2);
                    rs_test(left_v && right_tk.type == +lex_type::l_identifier);

                    ast_value_literal* const_result = new ast_value_literal();
                    const_result->value_type = new ast_type(L"string");
                    const_result->_constant_value.set_string_nogc(
                        wstr_to_str(right_tk.identifier).c_str()
                    );

                    ast_value_binary* vbin = new ast_value_binary();
                    vbin->left = left_v;
                    vbin->operate = lex_type::l_index_begin;
                    vbin->right = const_result;

                    rs_error("boomed! pls use ast_value_index but not binary");

                    return (grammar::ast_base*)vbin;
                }

                rs_error("Unexcepted token type.");
                return lex.parser_error(0x0000, L"Unexcepted token type.");
            }
        };

        struct pass_build_type :public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                ast_type* result = nullptr;

                if (RS_IS_TOKEN(0))
                {
                    result = new ast_type(RS_NEED_TOKEN(0).identifier);
                }
                else
                {
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

                }

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

        struct pass_token :public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                return (grammar::ast_base*)new ast_token(RS_NEED_TOKEN(0));
            }
        };

        struct pass_return :public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                ast_return* result = new ast_return();
                if (!ast_empty::is_empty(input[1]))
                {
                    result->return_value = dynamic_cast<ast_value*>(RS_NEED_AST(1));
                }
                return (grammar::ast_base*)result;
            }
        };

        struct pass_func_argument :public astnode_builder
        {
            static std::any build(lexer& lex, const std::wstring& name, inputs_t& input)
            {
                ast_value_arg_define* arg_def = new ast_value_arg_define;
                arg_def->is_ref = RS_NEED_TOKEN(0).type == +lex_type::l_ref;
                arg_def->arg_name = RS_NEED_TOKEN(1).identifier;
                if (input.size() == 3)
                    arg_def->value_type = dynamic_cast<ast_type*>(RS_NEED_AST(2));
                else
                    arg_def->value_type = new ast_type(L"dynamic");

                return (grammar::ast_base*)arg_def;
            }

        };

        /////////////////////////////////////////////////////////////////////////////////
#if 1
        inline void init_builder()
        {
            _registed_builder_function_id_list[meta::type_hash<pass_map_builder>]
                = _register_builder<pass_map_builder>();
           
            _registed_builder_function_id_list[meta::type_hash<pass_array_builder>]
                = _register_builder<pass_array_builder>();

            _registed_builder_function_id_list[meta::type_hash<pass_function_call>]
                = _register_builder<pass_function_call>();

            _registed_builder_function_id_list[meta::type_hash<pass_return>]
                = _register_builder<pass_return>();

            _registed_builder_function_id_list[meta::type_hash<pass_function_define>]
                = _register_builder<pass_function_define>();

            _registed_builder_function_id_list[meta::type_hash<pass_func_argument>]
                = _register_builder<pass_func_argument>();

            _registed_builder_function_id_list[meta::type_hash<pass_token>]
                = _register_builder<pass_token>();

            _registed_builder_function_id_list[meta::type_hash<pass_finalize_serching_namespace>]
                = _register_builder<pass_finalize_serching_namespace>();

            _registed_builder_function_id_list[meta::type_hash<pass_append_serching_namespace>]
                = _register_builder<pass_append_serching_namespace>();

            _registed_builder_function_id_list[meta::type_hash<pass_variable_in_namespace>]
                = _register_builder<pass_variable_in_namespace>();

            _registed_builder_function_id_list[meta::type_hash<pass_begin_varref_define>]
                = _register_builder<pass_begin_varref_define>();

            _registed_builder_function_id_list[meta::type_hash<pass_add_varref_define>]
                = _register_builder<pass_add_varref_define>();

            _registed_builder_function_id_list[meta::type_hash<pass_mark_as_ref_define>]
                = _register_builder<pass_mark_as_ref_define>();

            _registed_builder_function_id_list[meta::type_hash<pass_namespace>]
                = _register_builder<pass_namespace>();

            _registed_builder_function_id_list[meta::type_hash<pass_index_op>]
                = _register_builder<pass_index_op>();

            _registed_builder_function_id_list[meta::type_hash<pass_empty>]
                = _register_builder<pass_empty>();

            _registed_builder_function_id_list[meta::type_hash<pass_binary_op>]
                = _register_builder<pass_binary_op>();

            _registed_builder_function_id_list[meta::type_hash<pass_create_list<0>>]
                = _register_builder<pass_create_list<0>>();

            _registed_builder_function_id_list[meta::type_hash<pass_append_list<1, 0>>]
                = _register_builder<pass_append_list<1, 0>>();

            _registed_builder_function_id_list[meta::type_hash<pass_append_list<2, 0>>]
                = _register_builder<pass_append_list<2, 0>>();


            _registed_builder_function_id_list[meta::type_hash<pass_variable>]
                = _register_builder<pass_variable>();

            _registed_builder_function_id_list[meta::type_hash<pass_build_type>]
                = _register_builder<pass_build_type>();

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
#define RS_ASTBUILDER_INDEX(...) ast::index<##__VA_ARGS__##>()
}
