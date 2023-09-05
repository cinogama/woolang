#include "wo_lang_ast_builder.hpp"

namespace wo
{
    namespace ast
    {
        ast_value* dude_dump_ast_value(ast_value* dude)
        {
            if (dude)
                return dynamic_cast<ast_value*>(dude->instance());
            return nullptr;
        }
        ast_type* dude_dump_ast_type(ast_type* dude)
        {
            if (dude)
                return dynamic_cast<ast_type*>(dude->instance());
            return nullptr;
        }
        ////////////////////////////////////////////////////////////////////

        std::string ast_symbolable_base::get_namespace_chain() const
        {
            return get_belong_namespace_path_with_lang_scope(searching_begin_namespace_in_pass2);
        }
        grammar::ast_base* ast_symbolable_base::instance(ast_base* child_instance) const
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

        //////////////////////////////////////////

        ast_type::ast_type(wo_pstring_t _type_name)
        {
            set_type_with_name(_type_name);
        }
        wo_pstring_t ast_type::get_name_from_type(value::valuetype _type)
        {
            if (_type == value::valuetype::invalid)
                return WO_PSTR(nil);

            for (auto& [tname, vtype] : name_type_pair)
                if (vtype == _type)
                    return tname;

            return WO_PSTR(pending);
        }
        value::valuetype ast_type::get_type_from_name(wo_pstring_t name)
        {
            if (name_type_pair.find(name) != name_type_pair.end())
                return name_type_pair.at(name);
            return value::valuetype::invalid;
        }
        bool ast_type::check_castable(ast_type* to, ast_type* from)
        {
            // MUST BE FORCE CAST HERE!
            if (from->is_pending() || to->is_pending())
                return false;

            // Any type can cast to void;
            if (to->is_void())
                return true;

            // Cannot cast void to any other type.
            if (from->is_void())
                return false;

            if (to->is_dynamic())
                return true;

            if (from->is_dynamic())
            {
                // Not allowed cast template type from dynamic
                // In fact, cast func from dynamic is dangerous too...
                if (to->is_complex_type()
                    || to->is_nothing())
                    return false;
                return true;
            }

            // ISSUE-16: using type is create a new type based on old-type, impl-cast from base-type & using-type is invalid.
            if (to->accept_type(from, true, false))
                return true;

            if (to->value_type == value::valuetype::string_type
                && to->using_type_name == nullptr)
                return true;

            if (to->value_type == value::valuetype::integer_type
                || to->value_type == value::valuetype::real_type
                || to->value_type == value::valuetype::handle_type
                || to->value_type == value::valuetype::string_type
                || to->value_type == value::valuetype::bool_type)
            {
                if (from->value_type == value::valuetype::integer_type
                    || from->value_type == value::valuetype::real_type
                    || from->value_type == value::valuetype::handle_type
                    || from->value_type == value::valuetype::string_type)
                {
                    return to->using_type_name == nullptr
                        && from->using_type_name == nullptr;
                }
            }
            return false;
        }
        bool ast_type::is_custom_type(wo_pstring_t name)
        {
            if (name_type_pair.find(name) != name_type_pair.end())
                return false;
            return true;
        }
        void ast_type::set_type_with_name(wo_pstring_t _type_name)
        {
            complex_type = nullptr;
            type_name = _type_name;
            value_type = get_type_from_name(_type_name);
            using_type_name = nullptr;
            is_pending_type = _type_name == WO_PSTR(pending); // reset state;

            template_arguments.clear();

            if (is_custom_type(_type_name))
                is_pending_type = true;
        }
        void ast_type::set_type(const ast_type* _type)
        {
            // Skip!
            if (this == _type)
                return;

            *this = *_type;
            _type->instance_impl(this, false);
        }
        void ast_type::set_type_with_constant_value(const value& _val)
        {
            type_name = get_name_from_type(_val.type);
            value_type = _val.type;
            is_pending_type = false;
        }
        void ast_type::set_ret_type(const ast_type* _type)
        {
            wo_assert(is_func());

            type_name = WO_PSTR(complex);

            if (complex_type == nullptr)
                complex_type = new ast_type(WO_PSTR(pending));

            complex_type->set_type(_type);

            is_pending_type = false; // reset state;
            value_type = value::valuetype::invalid;
            template_arguments.clear();
        }

        void ast_type::set_as_function_type()
        {
            is_function_type = true;
        }
        ast_type* ast_type::get_return_type() const
        {
            wo_assert(is_complex());
            return complex_type;
        }
        void ast_type::append_function_argument_type(ast_type* arg_type)
        {
            argument_types.push_back(arg_type);
        }
        void ast_type::set_as_variadic_arg_func()
        {
            is_variadic_function_type = true;
        }
        bool ast_type::is_mutable() const
        {
            return is_mutable_type;
        }
        void ast_type::set_is_force_immutable()
        {
            set_is_mutable(false);
            is_force_immutable_type = true;
        }
        bool ast_type::is_force_immutable() const
        {
            return is_force_immutable_type;
        }
        bool ast_type::is_dynamic() const
        {
            return !is_func() && type_name == WO_PSTR(dynamic);
        }

        bool ast_type::is_custom() const
        {
            return is_custom_type(type_name) || (is_pending() && typefrom != nullptr);
        }

        bool ast_type::has_custom() const
        {
            if (has_template())
            {
                for (auto arg_type : template_arguments)
                {
                    if (arg_type->has_custom())
                        return true;
                }
            }
            // NOTE: Only anonymous structs need this check
            if (is_struct() && using_type_name == nullptr)
            {
                for (auto& [_, memberinfo] : struct_member_index)
                {
                    if (memberinfo.member_type->has_custom())
                        return true;
                }
            }
            if (is_func())
            {
                for (auto arg_type : argument_types)
                {
                    if (arg_type->has_custom())
                        return true;
                }
            }
            if (is_complex())
                return complex_type->has_custom();
            else
                return is_custom_type(type_name) || (is_pending() && typefrom != nullptr);
        }

        bool ast_type::is_pure_pending() const
        {
            return type_name == WO_PSTR(pending) && typefrom == nullptr;
        }
        bool ast_type::is_pending() const
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
            // NOTE: Only anonymous structs need this check
            if (is_struct() && using_type_name == nullptr)
            {
                for (auto& [_, memberinfo] : struct_member_index)
                {
                    if (memberinfo.member_type->is_pending())
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
                base_type_pending = type_name == WO_PSTR(pending);

            return is_pending_type || base_type_pending;
        }
        bool ast_type::may_need_update() const
        {
            if (has_template())
            {
                for (auto arg_type : template_arguments)
                {
                    if (arg_type->may_need_update())
                        return true;
                }
            }
            // NOTE: Only anonymous structs need this check
            if (is_struct() && using_type_name == nullptr)
            {
                for (auto& [_, memberinfo] : struct_member_index)
                {
                    if (memberinfo.member_type->may_need_update())
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
                base_type_pending = type_name == WO_PSTR(pending);

            return is_pending_type || base_type_pending;
        }
        bool ast_type::is_pending_function() const
        {
            if (is_func())
                return is_pending();
            return false;
        }
        bool ast_type::is_void() const
        {
            return type_name == WO_PSTR(void);
        }
        bool ast_type::is_nil() const
        {
            return type_name == WO_PSTR(nil);
        }
        bool ast_type::is_gc_type() const
        {
            return (uint8_t)value_type & (uint8_t)value::valuetype::need_gc;
        }
        bool ast_type::is_like(const ast_type* another, const std::vector<wo_pstring_t>& termplate_set, ast_type** out_para, ast_type** out_args)const
        {
            // Only used after pass1
            auto* this_origin_type = this->using_type_name ? this->using_type_name : this;
            auto* another_origin_type = another->using_type_name ? another->using_type_name : another;
            if (this_origin_type->is_func() != another_origin_type->is_func()
                || this_origin_type->is_variadic_function_type != another_origin_type->is_variadic_function_type
                || this_origin_type->template_arguments.size() != another_origin_type->template_arguments.size()
                || this_origin_type->argument_types.size() != another_origin_type->argument_types.size())
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
                else if (is_struct())
                {
                    wo_assert(another->is_struct());
                    if (struct_member_index.size() != another->struct_member_index.size())
                        return false;

                    for (auto& [memname, memberinfo] : struct_member_index)
                    {
                        auto fnd = another->struct_member_index.find(memname);
                        if (fnd == another->struct_member_index.end())
                            return false;

                        if (memberinfo.offset != fnd->second.offset)
                            return false;

                        if (!memberinfo.member_type->is_like(fnd->second.member_type, termplate_set))
                            return false;
                    }
                }
            }

            bool prefix_modified = false;
            if (is_mutable())
            {
                prefix_modified = true;
                if (!another->is_mutable())
                    return false;

                if (out_para)
                {
                    *out_para = dynamic_cast<ast_type*>(this->instance());
                    (**out_para).set_is_mutable(false);
                }
                if (out_args)
                {
                    *out_args = dynamic_cast<ast_type*>(another->instance());
                    (**out_args).set_is_mutable(false);
                }
            }

            if (!prefix_modified)
            {
                if (out_para)*out_para = const_cast<ast_type*>(this);
                if (out_args)*out_args = const_cast<ast_type*>(another);
            }
            return true;
        }
        bool ast_type::is_builtin_basic_type()
        {
            if (is_custom() || using_type_name)
                return false;
            return true;
        }

        bool ast_type::set_mix_types(ast_type* another, bool ignore_mutable)
        {
            if (is_pending() || another->is_pending())
                return false;

            if (is_same(another, ignore_mutable))
                return true;

            if (accept_type(another, false, ignore_mutable))
            {
                wo_assert(!another->accept_type(this, false, ignore_mutable));
                this->set_type(this);
                return true;
            }
            if (another->accept_type(this, false, ignore_mutable))
            {
                wo_assert(!accept_type(another, false, ignore_mutable));
                this->set_type(another);
                return true;
            }

            // 2. fast mix check failed, do full check

            if (is_pending_function() || another->is_pending_function())
                return false;

            if (is_pending() || another->is_pending())
                return false;

            if (another->is_nothing())
            {
                this->set_type(this);
                return true;
            }
            if (is_nothing())
            {
                this->set_type(another);
                return true;
            }

            if (is_mutable() != another->is_mutable())
                return false;

            // Might HKT
            if (is_hkt_typing() && another->is_hkt_typing())
            {
                if (base_typedef_symbol(symbol) == base_typedef_symbol(another->symbol))
                {
                    this->set_type(this);
                    return true;
                }
                return false;
            }

            if (is_func())
            {
                this->set_as_function_type();

                if (!another->is_func())
                    return false;

                if (argument_types.size() != another->argument_types.size())
                    return false;
                for (size_t index = 0; index < argument_types.size(); index++)
                {
                    // Woolang 1.12.6: Argument types of functions are no longer covariant but
                    //                  invariant during associating and receiving
                    if (!argument_types[index]->is_same(another->argument_types[index], true))
                        return false;
                }
                if (is_variadic_function_type != another->is_variadic_function_type)
                    return false;

                this->is_variadic_function_type = is_variadic_function_type;
            }
            else if (another->is_func())
                return false;

            if (is_complex() && another->is_complex())
            {
                if (!complex_type->set_mix_types(another->complex_type, false))
                    return false;
            }
            else if (!is_complex() && !another->is_complex())
            {
                if (type_name != another->type_name)
                    return false;

                wo_assert(!is_pending_type);
                this->type_name = type_name;
                this->value_type = value_type;
                this->struct_member_index = struct_member_index;
                this->typefrom = typefrom;
            }
            else
                return false;

            if (using_type_name || another->using_type_name)
            {
                if (!using_type_name || !another->using_type_name)
                    return false;

                if (find_type_in_this_scope(using_type_name) != find_type_in_this_scope(another->using_type_name))
                    return false;

                if (using_type_name->template_arguments.size() != another->using_type_name->template_arguments.size())
                    return false;

                for (size_t i = 0; i < using_type_name->template_arguments.size(); ++i)
                    if (!using_type_name->template_arguments[i]->set_mix_types(another->using_type_name->template_arguments[i], false))
                        return false;
            }
            else if (is_struct() && another->is_struct())
            {
                if (struct_member_index.size() != another->struct_member_index.size())
                    return false;

                for (auto& [memname, memberinfo] : struct_member_index)
                {
                    auto fnd = another->struct_member_index.find(memname);
                    if (fnd == another->struct_member_index.end())
                        return false;

                    if (memberinfo.offset != fnd->second.offset)
                        return false;

                    if (!memberinfo.member_type->set_mix_types(fnd->second.member_type, false))
                        return false;
                }
            }
            if (has_template())
            {
                if (template_arguments.size() != another->template_arguments.size())
                    return false;
                for (size_t index = 0; index < template_arguments.size(); index++)
                {
                    if (!template_arguments[index]->set_mix_types(another->template_arguments[index], false))
                        return false;
                }
            }
            this->is_pending_type = false;
            return true;
        }
        bool ast_type::is_func() const
        {
            return is_function_type;
        }
        bool ast_type::is_bool() const
        {
            return value_type == value::valuetype::bool_type;
        }
        bool ast_type::is_char() const
        {
            return type_name == WO_PSTR(char) || (using_type_name && using_type_name->type_name == WO_PSTR(char));
        }
        bool ast_type::is_builtin_using_type() const
        {
            return is_char();
        }
        bool ast_type::is_union() const
        {
            return type_name == WO_PSTR(union);
        }
        bool ast_type::is_tuple() const
        {
            return type_name == WO_PSTR(tuple);
        }
        bool ast_type::is_nothing() const
        {
            return type_name == WO_PSTR(nothing);
        }
        bool ast_type::is_struct() const
        {
            return type_name == WO_PSTR(struct);
        }
        bool ast_type::has_template() const
        {
            return !template_arguments.empty();
        }
        bool ast_type::is_complex() const
        {
            return complex_type;
        }
        bool ast_type::is_string() const
        {
            return value_type == value::valuetype::string_type;
        }
        bool ast_type::is_waiting_create_template_for_auto() const
        {
            return type_name == WO_PSTR(auto);
        }
        bool ast_type::is_complex_type() const
        {
            if (is_func() || using_type_name || is_union() || is_struct() || is_tuple() || is_vec() || is_map())
                return true;
            if (is_array() || is_dict())
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
        bool ast_type::is_array() const
        {
            return value_type == value::valuetype::array_type &&
                (type_name == WO_PSTR(array) || (using_type_name && using_type_name->type_name == WO_PSTR(array)));
        }
        bool ast_type::is_dict() const
        {
            return value_type == value::valuetype::dict_type &&
                (type_name == WO_PSTR(dict) || (using_type_name && using_type_name->type_name == WO_PSTR(dict)));
        }
        bool ast_type::is_vec() const
        {
            return value_type == value::valuetype::array_type &&
                (type_name == WO_PSTR(vec) || (using_type_name && using_type_name->type_name == WO_PSTR(vec)));
        }
        bool ast_type::is_map() const
        {
            return value_type == value::valuetype::dict_type &&
                (type_name == WO_PSTR(map) || (using_type_name && using_type_name->type_name == WO_PSTR(map)));
        }
        bool ast_type::is_integer() const
        {
            return value_type == value::valuetype::integer_type;
        }
        bool ast_type::is_real() const
        {
            return value_type == value::valuetype::real_type;
        }
        bool ast_type::is_handle() const
        {
            return value_type == value::valuetype::handle_type;
        }
        bool ast_type::is_gchandle() const
        {
            return value_type == value::valuetype::gchandle_type;
        }
        void ast_type::set_is_mutable(bool is_mutable)
        {
            if (is_mutable)
                is_force_immutable_type = false;

            is_mutable_type = is_mutable;
            if (using_type_name && using_type_name->is_mutable() != is_mutable)
                using_type_name->is_mutable_type = is_mutable;
        }
        grammar::ast_base* ast_type::instance_impl(ast_base* child_instance, bool clone_raw_struct_member) const
        {
            using astnode_type = decltype(MAKE_INSTANCE(this));
            auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this, WO_PSTR(pending));
            if (!child_instance) *dumm = *this;
            ast_symbolable_base::instance(dumm);

            // Write self copy functions here..
            dumm->symbol = symbol;
            dumm->searching_begin_namespace_in_pass2 = searching_begin_namespace_in_pass2;

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
            if (clone_raw_struct_member && (is_union() || is_struct()) && using_type_name == nullptr)
            {
                for (auto& [_, member_info] : dumm->struct_member_index)
                {
                    WO_REINSTANCE(member_info.member_type);
                }
            }

            WO_REINSTANCE(dumm->using_type_name);
            return dumm;
        }
        grammar::ast_base* ast_type::instance(ast_base* child_instance) const
        {
            return instance_impl(child_instance, true);
        }
        //////////////////////////////////////////

        ast_value::~ast_value()
        {
            if (auto* gcunit = constant_value.get_gcunit_with_barrier())
            {
                wo_assert(constant_value.type == value::valuetype::string_type);
                wo_assert(gcunit->gc_type == gcbase::gctype::no_gc);

                gcunit->~gcbase();
                free64(gcunit);
            }
        }
        ast_value::ast_value(ast_type* type)
            : value_type(type)
        {
            wo_assert(type != nullptr);
        }
        wo::value& ast_value::get_constant_value()
        {
            if (!this->is_constant)
                wo_error("This value is not a constant.");

            return constant_value;
        };
        void  ast_value::update_constant_value(lexer* lex)
        {
            wo_error("ast_value cannot update_constant_value.");
        }
        grammar::ast_base* ast_value::instance(ast_base* child_instance) const
        {
            wo_assert(child_instance != nullptr);
            // ast_value is abstract class, will not make instance here.

            auto* dumm = dynamic_cast<ast_value*>(child_instance);

            // Write self copy functions here..
            if (constant_value.type == value::valuetype::string_type)
                dumm->constant_value.set_string_nogc(constant_value.string->c_str());
            WO_REINSTANCE(dumm->value_type);

            return dumm;
        }

        //////////////////////////////////////////

        void ast_value_literal::update_constant_value(lexer* lex)
        {
            // do nothing
        }

        wo_handle_t ast_value_literal::wstr_to_handle(const std::wstring& str)
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
        wo_integer_t ast_value_literal::wstr_to_integer(const std::wstring& str)
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
        wo_real_t ast_value_literal::wstr_to_real(const std::wstring& str)
        {
            return (wo_real_t)std::stold(str);
        }

        ast_value_literal::ast_value_literal()
            : ast_value(new ast_type(WO_PSTR(nil)))
        {
            is_constant = true;
            constant_value.set_nil();
        }

        ast_value_literal::ast_value_literal(const token& te)
            : ast_value(new ast_type(WO_PSTR(pending)))
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
            case lex_type::l_identifier: // Special for xxx.index
                constant_value.set_string_nogc(wstr_to_str(te.identifier).c_str());
                break;
            case lex_type::l_literal_char:
                wo_assert(te.identifier.size() == 1);
                constant_value.set_integer((wo_integer_t)(wo_handle_t)te.identifier[0]);
                break;
            case lex_type::l_nil:
                constant_value.set_nil();
                break;
            case lex_type::l_true:
                constant_value.set_bool(true);
                break;
            case lex_type::l_false:
                constant_value.set_bool(false);
                break;
            default:
                wo_error("Unexcepted literal type.");
                break;
            }
            if (te.type == +lex_type::l_literal_char)
                value_type->set_type_with_name(WO_PSTR(char));
            else
                value_type->set_type_with_constant_value(constant_value);
        }

        grammar::ast_base* ast_value_literal::instance(ast_base* child_instance) const
        {
            using astnode_type = decltype(MAKE_INSTANCE(this));
            auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
            if (!child_instance) *dumm = *this;
            ast_value::instance(dumm);

            // Write self copy functions here..
            return dumm;
        }

        //////////////////////////////////////////

        ast_value_type_cast::ast_value_type_cast(ast_value* value, ast_type* target_type)
            : ast_value(target_type)
        {
            is_constant = false;
            _be_cast_value_node = value;
        }

        ast_value_type_cast::ast_value_type_cast() :ast_value(new ast_type(WO_PSTR(pending)))
        {
        }
        grammar::ast_base* ast_value_type_cast::instance(ast_base* child_instance) const
        {
            using astnode_type = decltype(MAKE_INSTANCE(this));
            auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
            if (!child_instance) *dumm = *this;
            ast_value::instance(dumm);

            // Write self copy functions here..
            WO_REINSTANCE(dumm->_be_cast_value_node);

            return dumm;
        }

        void ast_value_type_cast::update_constant_value(lexer* lex)
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

                    is_constant = true;

                    switch (aim_real_type)
                    {
                    case value::valuetype::real_type:
                        constant_value.set_real(wo_cast_real((wo_value)&last_value));
                        break;
                    case value::valuetype::integer_type:
                        constant_value.set_integer(wo_cast_int((wo_value)&last_value));
                        break;
                    case value::valuetype::string_type:
                        constant_value.set_string_nogc(wo_cast_string((wo_value)&last_value));
                        break;
                    case value::valuetype::handle_type:
                        constant_value.set_handle(wo_cast_handle((wo_value)&last_value));
                        break;
                    case value::valuetype::bool_type:
                        constant_value.set_bool(wo_cast_bool((wo_value)&last_value));
                        break;
                    default:
                        if (last_value.is_nil() && value_type->is_nil())
                            constant_value.set_val(&last_value);
                        else
                            is_constant = false;
                        break;
                    }
                    // end of match
                }
            }
        }

        //////////////////////////////////////////

        ast_value_type_judge::ast_value_type_judge(ast_value* value, ast_type* type)
            : ast_value(type)
        {
            _be_cast_value_node = value;
            value_type = type;
        }
        ast_value_type_judge::ast_value_type_judge() : ast_value(new ast_type(WO_PSTR(pending))) {}
        grammar::ast_base* ast_value_type_judge::instance(ast_base* child_instance) const
        {
            using astnode_type = decltype(MAKE_INSTANCE(this));
            auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
            if (!child_instance) *dumm = *this;
            ast_value::instance(dumm);

            // Write self copy functions here..
            WO_REINSTANCE(dumm->_be_cast_value_node);

            return dumm;
        }

        void ast_value_type_judge::update_constant_value(lexer* lex)
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

        //////////////////////////////////////////
        void ast_decl_attribute::varify_attributes(lexer* lex) const
        {
            // 1) Check if public, protected or private at same time
            lex_type has_describe = +lex_type::l_error;
            for (auto att : attributes)
            {
                if (att == +lex_type::l_public || att == +lex_type::l_private || att == +lex_type::l_protected)
                {
                    if (has_describe != +lex_type::l_error)
                    {
                        lex->parser_error(lexer::errorlevel::error, WO_ERR_CANNOT_DECL_PUB_PRI_PRO_SAME_TIME);
                        break;
                    }
                    has_describe = att;
                }
            }
        }
        void ast_decl_attribute::add_attribute(lexer* lex, lex_type attr)
        {
            if (attributes.find(attr) == attributes.end())
            {
                attributes.insert(attr);
            }
            else
                lex->parser_error(lexer::errorlevel::error, WO_ERR_REPEAT_ATTRIBUTE);
        }
        bool ast_decl_attribute::is_static_attr() const
        {
            return attributes.find(+lex_type::l_static) != attributes.end();
        }
        bool ast_decl_attribute::is_private_attr() const
        {
            return attributes.find(+lex_type::l_private) != attributes.end()
                || (!is_protected_attr() && !is_public_attr());
        }
        bool ast_decl_attribute::is_protected_attr() const
        {
            return attributes.find(+lex_type::l_protected) != attributes.end();
        }
        bool ast_decl_attribute::is_public_attr() const
        {
            return attributes.find(+lex_type::l_public) != attributes.end();
        }
        bool ast_decl_attribute::is_extern_attr() const
        {
            return attributes.find(+lex_type::l_extern) != attributes.end();
        }
        grammar::ast_base* ast_decl_attribute::instance(ast_base* child_instance) const
        {
            using astnode_type = decltype(MAKE_INSTANCE(this));
            auto* dumm = child_instance ? dynamic_cast<astnode_type>(child_instance) : MAKE_INSTANCE(this);
            if (!child_instance) *dumm = *this;
            // ast_value::instance(dumm);

            // Write self copy functions here..

            return dumm;
        }

        //////////////////////////////////////////

        ast_value_binary::ast_value_binary()
            : ast_value(new ast_type(WO_PSTR(pending)))
        {
        }

        grammar::ast_base* ast_value_binary::instance(ast_base* child_instance) const
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

        ast_type* ast_value_binary::binary_upper_type(ast_type* left_v, ast_type* right_v)
        {
            if (left_v->is_nothing())
                return right_v;
            if (right_v->is_nothing())
                return left_v;

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

            if (left_v->is_same(right_v, true))
                return left_v;

            return nullptr;
        }
        ast_type* ast_value_binary::binary_upper_type_with_operator(ast_type* left_v, ast_type* right_v, lex_type op)
        {
            if (left_v->is_custom() || right_v->is_custom())
                return nullptr;
            if (left_v->is_func() || right_v->is_func())
                return nullptr;
            if ((left_v->is_string() || right_v->is_string()) && op != +lex_type::l_add && op != +lex_type::l_add_assign)
                return nullptr;
            if (left_v->is_dict() || right_v->is_dict())
                return nullptr;
            if (left_v->is_array() || right_v->is_array())
                return nullptr;
            if (left_v->is_nothing() || right_v->is_nothing())
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
        void ast_value_binary::update_constant_value(lexer* lex)
        {
            if (is_constant)
                return;

            left->update_constant_value(lex);
            right->update_constant_value(lex);

            // if left/right is custom, donot calculate them 
            if (!left->value_type->is_builtin_basic_type()
                || !right->value_type->is_builtin_basic_type())
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

                // lex->lang_error(lexer::errorlevel::error, this, WO_ERR_CANNOT_CALC_WITH_L_AND_R);
                value_type = new ast_type(WO_PSTR(pending));

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

        //////////////////////////////////////////

        grammar::produce pass_import_files::build(lexer& lex, inputs_t& input)
        {
            wo_test(input.size() == 3);
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
            if (!wo::read_virtual_source(&srcfile, &src_full_path, path + L".wo", wo::wstr_to_str(*lex.source_file).c_str()))
            {
                // import a::b; cannot open a/b.wo, trying a/b/b.wo
                if (!wo::read_virtual_source(&srcfile, &src_full_path, path + L"/" + filename + L".wo", wo::wstr_to_str(*lex.source_file).c_str()))
                    return token{ lex.parser_error(lexer::errorlevel::error, WO_ERR_CANNOT_OPEN_FILE, path.c_str()) };
            }

            if (!lex.has_been_imported(wstring_pool::get_pstr(src_full_path))
                && !lex.has_been_imported(wo::crc_64(srcfile.c_str(), 0)))
            {
                lexer new_lex(srcfile, wstr_to_str(src_full_path));
                new_lex.imported_file_list = lex.imported_file_list;
                new_lex.imported_file_crc64_list = lex.imported_file_crc64_list;
                new_lex.used_macro_list = lex.used_macro_list;

                auto* imported_ast = wo::get_wo_grammar()->gen(new_lex);

                lex.used_macro_list = new_lex.used_macro_list;

                lex.lex_error_list.insert(lex.lex_error_list.end(),
                    new_lex.lex_error_list.begin(),
                    new_lex.lex_error_list.end());

                lex.imported_file_list = new_lex.imported_file_list;
                lex.imported_file_crc64_list = new_lex.imported_file_crc64_list;

                if (imported_ast)
                {
                    imported_ast->add_child(new ast_nop); // nop for debug info gen, avoid ip/cr confl..
                    return (ast_basic*)imported_ast;
                }

                return token{ +lex_type::l_error };
            }
            return (ast_basic*)new ast_empty();
        }

        grammar::produce pass_foreach::build(lexer& lex, inputs_t& input)
        {
            ast_foreach* afor = new ast_foreach;

            // for ( DECL_ATTRIBUTE let LIST : EXP ) SENT
            // 0   1        2        3   4   5  6  7   8

            // build EXP->iter() / var LIST;

            //{{{{
                // var _iter = XXXXXX->iter();
            ast_value* be_iter_value = dynamic_cast<ast_value*>(WO_NEED_AST(6));

            ast_value_funccall* exp_dir_iter_call = new ast_value_funccall();
            exp_dir_iter_call->arguments = new ast_list();
            exp_dir_iter_call->called_func = new ast_value_variable(WO_PSTR(iter));
            exp_dir_iter_call->arguments->append_at_head(be_iter_value);
            exp_dir_iter_call->directed_value_from = be_iter_value;
            exp_dir_iter_call->called_func->copy_source_info(be_iter_value);
            exp_dir_iter_call->copy_source_info(be_iter_value);

            afor->used_iter_define = new ast_varref_defines;
            afor->used_iter_define->declear_attribute = new ast_decl_attribute;

            auto* afor_iter_define = new ast_pattern_identifier;
            afor_iter_define->identifier = WO_PSTR(_iter);
            afor_iter_define->attr = new ast_decl_attribute();
            afor_iter_define->copy_source_info(be_iter_value);

            afor->used_iter_define->var_refs.push_back({ afor_iter_define, exp_dir_iter_call });
            afor->used_iter_define->copy_source_info(be_iter_value);

            // in loop body..
            // {{{{
            //    while (_iter->next(..., a,b))
            // DONOT INSERT ARGUS NOW, TAKE PLACE IN PASS2..    
            // }}}}

            ast_value_funccall* iter_dir_next_call = new ast_value_funccall();
            iter_dir_next_call->arguments = new ast_list();
            iter_dir_next_call->called_func = new ast_value_variable(WO_PSTR(next));
            iter_dir_next_call->directed_value_from = new ast_value_variable(WO_PSTR(_iter));
            iter_dir_next_call->arguments->append_at_head(iter_dir_next_call->directed_value_from);

            iter_dir_next_call->called_func->copy_source_info(be_iter_value);

            iter_dir_next_call->copy_source_info(be_iter_value);

            //WO_NEED_AST(8);

            ast_match* match_for_exec = new ast_match();
            match_for_exec->match_value = iter_dir_next_call;
            match_for_exec->cases = new ast_list;

            auto* for_decl_vars = dynamic_cast<ast_pattern_tuple*>(WO_NEED_AST(4));

            auto* pattern_none_case = new ast_match_union_case;
            {
                pattern_none_case->union_pattern = new ast_pattern_union_value;
                pattern_none_case->union_pattern->union_expr = new ast_value_variable(WO_PSTR(none));
                pattern_none_case->union_pattern->union_expr->copy_source_info(for_decl_vars);
                pattern_none_case->union_pattern->copy_source_info(for_decl_vars);
                auto* sentence_in_list = new ast_list;
                sentence_in_list->append_at_end(new ast_break());
                pattern_none_case->in_case_sentence = new ast_sentence_block(sentence_in_list);
                pattern_none_case->in_case_sentence->copy_source_info(for_decl_vars);
                pattern_none_case->copy_source_info(for_decl_vars);
            }
            pattern_none_case->copy_source_info(for_decl_vars);

            auto* pattern_value_case = new ast_match_union_case;
            {
                pattern_value_case->union_pattern = new ast_pattern_union_value;
                pattern_value_case->union_pattern->union_expr = new ast_value_variable(WO_PSTR(value));
                pattern_value_case->union_pattern->pattern_arg_in_union_may_nil = for_decl_vars;
                pattern_value_case->union_pattern->union_expr->copy_source_info(for_decl_vars);
                pattern_value_case->union_pattern->copy_source_info(for_decl_vars);
                auto* sentence_in_list = new ast_list;
                sentence_in_list->append_at_end(WO_NEED_AST(8));
                pattern_value_case->in_case_sentence = new ast_sentence_block(sentence_in_list);
                pattern_value_case->in_case_sentence->copy_source_info(for_decl_vars);
                pattern_value_case->copy_source_info(for_decl_vars);
            }

            match_for_exec->cases->append_at_end(pattern_value_case);
            match_for_exec->cases->append_at_end(pattern_none_case);

            match_for_exec->copy_source_info(be_iter_value);

            afor->loop_sentences = new ast_while(nullptr, match_for_exec);

            return (ast_basic*)afor;
        }

        grammar::produce pass_format_string::build(lexer& lex, inputs_t& input)
        {
            //if(input.size() == )
            wo_assert(input.size() == 2 || input.size() == 3);
            if (input.size() == 2)
            {
                auto* left = new ast_value_literal(WO_NEED_TOKEN(0));
                auto* right = dynamic_cast<ast_value*>(WO_NEED_AST(1));

                auto cast_right = new ast_value_type_cast(right, new ast_type(WO_PSTR(string)));

                left->copy_source_info(right);
                cast_right->copy_source_info(left);

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

                auto cast_right = new ast_value_type_cast(right, new ast_type(WO_PSTR(string)));

                left->copy_source_info(right);
                cast_right->copy_source_info(left);

                ast_value_binary* vbin = new ast_value_binary();
                vbin->left = left;
                vbin->operate = lex_type::l_add;
                vbin->right = cast_right;
                vbin->update_constant_value(&lex);

                auto* origin_list = dynamic_cast<ast_value*>(WO_NEED_AST(0));

                vbin->copy_source_info(origin_list);

                ast_value_binary* vbinresult = new ast_value_binary();
                vbinresult->left = origin_list;
                vbinresult->operate = lex_type::l_add;
                vbinresult->right = vbin;
                vbinresult->update_constant_value(&lex);
                return (ast_basic*)vbinresult;
            }
        }

        //////////////////////////////////////////

        void pass_union_define::find_used_template(
            ast_type* type_decl,
            const std::vector<wo_pstring_t>& template_defines,
            std::set<wo_pstring_t>& out_used_type)
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
        grammar::produce pass_union_define::build(lexer& lex, inputs_t& input)
        {
            wo_assert(input.size() == 7);
            // ATTRIBUTE union IDENTIFIER <TEMPLATE_DEF> { ITEMS }
            //    0                    2             3          5

            // Create a namespace 
            ast_decl_attribute* union_arttribute = dynamic_cast<ast_decl_attribute*>(WO_NEED_AST(0));

            ast_list* bind_using_type_namespace_result = new ast_list;

            ast_namespace* union_scope = new ast_namespace;
            union_scope->scope_name = wstring_pool::get_pstr(WO_NEED_TOKEN(2).identifier);
            union_scope->copy_source_info(union_arttribute);
            union_scope->in_scope_sentence = new ast_list;

            // Get templates here(If have?)
            ast_template_define* defined_template_args = nullptr;
            ast_list* defined_template_arg_lists = nullptr;

            if (!ast_empty::is_empty(input[3]))
            {
                defined_template_arg_lists = dynamic_cast<ast_list*>(WO_NEED_AST(3));
                defined_template_args = dynamic_cast<ast_template_define*>(
                    defined_template_arg_lists->children);
            }

            // Then we decl a using-type here;
            ast_using_type_as* using_type = new ast_using_type_as;
            using_type->new_type_identifier = union_scope->scope_name;
            using_type->old_type = new ast_type(WO_PSTR(union));
            using_type->declear_attribute = union_arttribute;
            using_type->copy_source_info(union_arttribute);
            using_type->old_type->copy_source_info(union_arttribute);

            std::vector<wo_pstring_t>& template_arg_defines = using_type->template_type_name_list;
            if (defined_template_args)
            {
                ast_list* template_const_list = new ast_list;

                using_type->is_template_define = true;

                ast_template_define* template_type = defined_template_args;
                wo_test(template_type);
                while (template_type)
                {
                    using_type->template_type_name_list.push_back(template_type->template_ident);
                    template_type = dynamic_cast<ast_template_define*>(template_type->sibling);
                }
            }
            bind_using_type_namespace_result->append_at_head(using_type);

            // OK, We need decl union items/function here
            ast_union_item* items =
                dynamic_cast<ast_union_item*>(dynamic_cast<ast_list*>(WO_NEED_AST(5))->children);

            uint16_t union_item_id = 0;

            while (items)
            {
                std::set<wo_pstring_t> used_template_args;

                auto create_union_type = [&]() {
                    ast_type* union_type_with_template = new ast_type(using_type->new_type_identifier);
                    for (auto& ident : using_type->template_type_name_list)
                    {
                        // Current template-type has been used.
                        if (used_template_args.find(ident) != used_template_args.end())
                            union_type_with_template->template_arguments.push_back(new ast_type(ident));
                        else
                            // Not used template-type, 
                            union_type_with_template->template_arguments.push_back(new ast_type(WO_PSTR(nothing)));
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
                    argdef->arg_name = WO_PSTR(_val);
                    argdef->value_type = items->type_may_nil;
                    argdef->declear_attribute = new ast_decl_attribute;
                    avfd_item_type_builder->argument_list->add_child(argdef);
                    argdef->copy_source_info(items);

                    auto* adt_type = create_union_type();
                    if (items->gadt_out_type_may_nil)
                    {
                        auto* gadt_type = items->gadt_out_type_may_nil;
                        bool conflict = false;

                        // TODO: Need verify gadt type is current union?
                        if (gadt_type->template_arguments.size() == template_arg_defines.size())
                        {
                            for (size_t i = 0; i < adt_type->template_arguments.size(); ++i)
                            {
                                if (!adt_type->template_arguments[i]->is_nothing())
                                {
                                    wo_assert(adt_type->template_arguments[i]->search_from_global_namespace == false
                                        && adt_type->template_arguments[i]->scope_namespaces.empty());

                                    if (adt_type->template_arguments[i]->type_name != gadt_type->template_arguments[i]->type_name
                                        || gadt_type->template_arguments[i]->search_from_global_namespace
                                        || !gadt_type->template_arguments[i]->scope_namespaces.empty())
                                    {
                                        conflict = true;
                                        break;
                                    }
                                }
                            }
                        }
                        else
                            conflict = true;

                        if (conflict)
                            lex.lang_error(lexer::errorlevel::error, items->gadt_out_type_may_nil, WO_ERR_INVALID_GADT_CONFLICT);

                    }
                    avfd_item_type_builder->value_type->set_ret_type(adt_type);

                    avfd_item_type_builder->value_type->argument_types.push_back(dynamic_cast<ast_type*>(items->type_may_nil->instance()));

                    avfd_item_type_builder->auto_adjust_return_type = true;
                    avfd_item_type_builder->copy_source_info(items);
                    avfd_item_type_builder->value_type->get_return_type()->copy_source_info(items);

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
                    avfd_item_type_builder->function_name = nullptr; // Is a lambda function!
                    avfd_item_type_builder->argument_list = new ast_list;
                    avfd_item_type_builder->declear_attribute = union_arttribute;

                    if (items->gadt_out_type_may_nil)
                        avfd_item_type_builder->value_type->set_ret_type(items->gadt_out_type_may_nil);
                    else
                        avfd_item_type_builder->value_type->set_ret_type(create_union_type());


                    avfd_item_type_builder->auto_adjust_return_type = true;
                    avfd_item_type_builder->copy_source_info(items);
                    avfd_item_type_builder->value_type->get_return_type()->copy_source_info(items);

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

                    ast_varref_defines* define_union_item = new ast_varref_defines();
                    define_union_item->copy_source_info(items);

                    auto* union_item_define = new ast_pattern_identifier;
                    union_item_define->identifier = items->identifier;
                    union_item_define->attr = union_arttribute;

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

        grammar::produce pass_namespace::build(lexer& lex, inputs_t& input)
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
                result->scope_name = wstring_pool::get_pstr(name->tokens.identifier);
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

        grammar::produce pass_function_define::build(lexer& lex, inputs_t& input)
        {
            auto* ast_func = new ast_value_function_define;
            ast_type* return_type = nullptr;
            ast_list* template_types = nullptr;

            if (input.size() == 10)
            {
                // function with name..
                ast_func->declear_attribute = dynamic_cast<ast_decl_attribute*>(WO_NEED_AST(0));
                wo_assert(ast_func->declear_attribute);

                ast_func->function_name = wstring_pool::get_pstr(WO_NEED_TOKEN(2).identifier);

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
                    template_types = dynamic_cast<ast_list*>(WO_NEED_AST(2));

                ast_func->function_name = nullptr; // just get a fucking name
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

                    ast_func->function_name = wstring_pool::get_pstr(L"operator " + dynamic_cast<ast_token*>(WO_NEED_AST(3))->tokens.identifier);

                    if (!ast_empty::is_empty(input[4]))
                        template_types = dynamic_cast<ast_list*>(WO_NEED_AST(4));

                    ast_func->argument_list = dynamic_cast<ast_list*>(WO_NEED_AST(6));

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

                    ast_func->function_name = wstring_pool::get_pstr(WO_NEED_TOKEN(3).identifier);

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

                ast_func->function_name = wstring_pool::get_pstr(L"operator " + dynamic_cast<ast_token*>(WO_NEED_AST(4))->tokens.identifier);

                if (!ast_empty::is_empty(input[5]))
                {
                    template_types = dynamic_cast<ast_list*>(WO_NEED_AST(5));
                }

                ast_func->argument_list = dynamic_cast<ast_list*>(WO_NEED_AST(7));
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
                    template_types = dynamic_cast<ast_list*>(WO_NEED_AST(1));

                ast_func->function_name = nullptr; // just get a fucking name
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
                // complex type;;
                ast_func->value_type->set_ret_type(return_type);
                ast_func->value_type->get_return_type()->copy_source_info(return_type);
                ast_func->auto_adjust_return_type = false;
            }
            else
            {
                ast_func->auto_adjust_return_type = true;
            }

            size_t auto_template_id = 0;
            auto* argchild = ast_func->argument_list->children;
            while (argchild)
            {
                if (auto* arg_node = dynamic_cast<ast_value_arg_define*>(argchild))
                {
                    if (arg_node->value_type->is_waiting_create_template_for_auto())
                    {
                        // Create a template for this fucking arguments...
                        std::wstring auto_template_name = L"auto_" + std::to_wstring(auto_template_id++) + L"_t";

                        ast_template_define* atn = new ast_template_define;
                        atn->template_ident = wstring_pool::get_pstr(auto_template_name);

                        // If no template arguments defined, create a new list~
                        if (template_types == nullptr)
                            template_types = new ast_list;

                        template_types->append_at_end(atn);
                        // Update typename at last:
                        arg_node->value_type->set_type_with_name(atn->template_ident);
                    }

                    if (ast_func->value_type->is_variadic_function_type)
                        return token{ lex.parser_error(lexer::errorlevel::error, WO_ERR_ARG_DEFINE_AFTER_VARIADIC) };

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
                ast_template_define* template_type = dynamic_cast<ast_template_define*>(template_types->children);
                wo_assert(template_type);

                while (template_type)
                {
                    ast_func->template_type_name_list.push_back(template_type->template_ident);
                    template_type = dynamic_cast<ast_template_define*>(template_type->sibling);
                }
            }

            if (ast_func->in_function_sentence)
                ast_func->in_function_sentence->append_at_head(template_const_list);
            else
                ast_func->in_function_sentence = template_const_list;

            // if ast_func->in_function_sentence == nullptr it means this function have no sentences...
            return (grammar::ast_base*)ast_func;
        }


        grammar::produce pass_using_type_as::build(lexer& lex, inputs_t& input)
        {
            // attrib using/alias xxx <>  = xxx ;/{...}
            ast_using_type_as* using_type = new ast_using_type_as;
            using_type->new_type_identifier = wstring_pool::get_pstr(WO_NEED_TOKEN(2).identifier);
            using_type->old_type = dynamic_cast<ast_type*>(WO_NEED_AST(5));
            using_type->declear_attribute = dynamic_cast<ast_decl_attribute*>(WO_NEED_AST(0));
            using_type->is_alias = WO_NEED_TOKEN(1).type == +lex_type::l_alias;
            ast_list* template_const_list = new ast_list;

            using_type->old_type = dynamic_cast<ast_type*>(WO_NEED_AST(5));

            if (!ast_empty::is_empty(input[3]))
            {
                ast_list* template_defines = dynamic_cast<ast_list*>(WO_NEED_AST(3));
                wo_test(template_defines);
                using_type->is_template_define = true;

                ast_template_define* template_type = dynamic_cast<ast_template_define*>(template_defines->children);
                wo_test(template_type);
                while (template_type)
                {
                    using_type->template_type_name_list.push_back(template_type->template_ident);
                    template_type = dynamic_cast<ast_template_define*>(template_type->sibling);
                }
            }

            if (!using_type->is_alias && WO_IS_AST(6) && !ast_empty::is_empty(input[6]))
            {
                ast_sentence_block* in_namespace = dynamic_cast<ast_sentence_block*>(WO_NEED_AST(6));
                wo_assert(in_namespace);

                ast_namespace* tname_scope = new ast_namespace;
                tname_scope->copy_source_info(in_namespace);
                tname_scope->scope_name = using_type->new_type_identifier;
                tname_scope->in_scope_sentence = in_namespace->sentence_list;

                auto result = new ast_list;
                result->append_at_end(using_type);
                result->append_at_end(tname_scope);

                return (ast_basic*)result;
            }

            return (ast_basic*)using_type;
        }

        //////////////////////////////////////////

        ast_value_logical_binary::ast_value_logical_binary() : ast_value(new ast_type(WO_PSTR(bool)))
        {
        }
        grammar::ast_base* ast_value_logical_binary::instance(ast_base* child_instance) const
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
        void ast_value_logical_binary::update_constant_value(lexer* lex)
        {
            if (is_constant)
                return;

            left->update_constant_value(lex);
            right->update_constant_value(lex);

            if (!left->value_type->is_same(right->value_type, true))
                return;

            // if left/right is custom, donot calculate them 
            if (operate == +lex_type::l_land
                || operate == +lex_type::l_lor)
            {
                if (!left->value_type->is_bool() || !right->value_type->is_bool())
                    return;
            }
            else if (!left->value_type->is_builtin_basic_type()
                || !right->value_type->is_builtin_basic_type())
                return;

            if (overrided_operation_call)
                return;

            if (left->is_constant && right->is_constant)
            {
                is_constant = true;
                value_type->set_type_with_name(WO_PSTR(bool));

                switch (operate)
                {
                case lex_type::l_land:
                    constant_value.set_bool(left->get_constant_value().handle && right->get_constant_value().handle);
                    break;
                case lex_type::l_lor:
                    constant_value.set_bool(left->get_constant_value().handle || right->get_constant_value().handle);
                    break;
                case lex_type::l_equal:
                case lex_type::l_not_equal:
                    if (left->value_type->is_integer() || left->value_type->is_bool())
                        constant_value.set_bool(left->get_constant_value().integer == right->get_constant_value().integer);
                    else if (left->value_type->is_real())
                        constant_value.set_bool(left->get_constant_value().real == right->get_constant_value().real);
                    else if (left->value_type->is_string())
                        constant_value.set_bool(*left->get_constant_value().string == *right->get_constant_value().string);
                    else if (left->value_type->is_handle())
                        constant_value.set_bool(left->get_constant_value().handle == right->get_constant_value().handle);
                    else
                    {
                        is_constant = false;
                        return;
                    }
                    if (operate == +lex_type::l_not_equal)
                        constant_value.set_bool(!constant_value.integer);
                    break;

                case lex_type::l_less:
                case lex_type::l_larg_or_equal:
                    if (left->value_type->is_integer())
                        constant_value.set_bool(left->get_constant_value().integer < right->get_constant_value().integer);
                    else if (left->value_type->is_real())
                        constant_value.set_bool(left->get_constant_value().real < right->get_constant_value().real);
                    else if (left->value_type->is_string())
                        constant_value.set_bool(*left->get_constant_value().string < *right->get_constant_value().string);
                    else if (left->value_type->is_handle())
                        constant_value.set_bool(left->get_constant_value().handle < right->get_constant_value().handle);
                    else
                    {
                        is_constant = false;
                        return;
                    }


                    if (operate == +lex_type::l_larg_or_equal)
                        constant_value.set_bool(!constant_value.integer);
                    break;

                case lex_type::l_larg:
                case lex_type::l_less_or_equal:

                    if (left->value_type->is_integer())
                        constant_value.set_bool(left->get_constant_value().integer > right->get_constant_value().integer);
                    else if (left->value_type->is_real())
                        constant_value.set_bool(left->get_constant_value().real > right->get_constant_value().real);
                    else if (left->value_type->is_string())
                        constant_value.set_bool(*left->get_constant_value().string > *right->get_constant_value().string);
                    else if (left->value_type->is_handle())
                        constant_value.set_bool(left->get_constant_value().handle > right->get_constant_value().handle);
                    else
                    {
                        is_constant = false;
                        return;
                    }

                    if (operate == +lex_type::l_less_or_equal)
                        constant_value.set_bool(!constant_value.integer);
                    break;

                default:
                    wo_error("Do not support this op.");
                }
            }
        }

        //////////////////////////////////////////

        //////////////////////////////////////////

        //////////////////////////////////////////

        //////////////////////////////////////////

        //////////////////////////////////////////

        //////////////////////////////////////////

        //////////////////////////////////////////

        //////////////////////////////////////////

        //////////////////////////////////////////

        //////////////////////////////////////////

        //////////////////////////////////////////

        //////////////////////////////////////////

        //////////////////////////////////////////

        //////////////////////////////////////////

        //////////////////////////////////////////

        //////////////////////////////////////////

        //////////////////////////////////////////

        //////////////////////////////////////////

        //////////////////////////////////////////

        //////////////////////////////////////////

        //////////////////////////////////////////

        //////////////////////////////////////////

        //////////////////////////////////////////

        //////////////////////////////////////////

        //////////////////////////////////////////

        //////////////////////////////////////////

        //////////////////////////////////////////

        //////////////////////////////////////////

        //////////////////////////////////////////

        //////////////////////////////////////////

        //////////////////////////////////////////

        //////////////////////////////////////////

        //////////////////////////////////////////

        //////////////////////////////////////////

        //////////////////////////////////////////

        void init_builder()
        {
            _registed_builder_function_id_list[meta::type_hash<pass_template_decl>] = _register_builder<pass_template_decl>();

            _registed_builder_function_id_list[meta::type_hash<pass_mark_label>] = _register_builder<pass_mark_label>();

            _registed_builder_function_id_list[meta::type_hash<pass_break>] = _register_builder<pass_break>();

            _registed_builder_function_id_list[meta::type_hash<pass_continue>] = _register_builder<pass_continue>();

            _registed_builder_function_id_list[meta::type_hash<pass_forloop>] = _register_builder<pass_forloop>();

            _registed_builder_function_id_list[meta::type_hash<pass_foreach>] = _register_builder<pass_foreach>();

            _registed_builder_function_id_list[meta::type_hash<pass_typeof>] = _register_builder<pass_typeof>();
            _registed_builder_function_id_list[meta::type_hash<pass_build_mutable_type>] = _register_builder<pass_build_mutable_type>();
            _registed_builder_function_id_list[meta::type_hash<pass_template_reification>] = _register_builder<pass_template_reification>();

            _registed_builder_function_id_list[meta::type_hash<pass_type_check>] = _register_builder<pass_type_check>();

            _registed_builder_function_id_list[meta::type_hash<pass_directed_value_for_call>] = _register_builder<pass_directed_value_for_call>();

            _registed_builder_function_id_list[meta::type_hash<pass_type_judgement>] = _register_builder<pass_type_judgement>();

            _registed_builder_function_id_list[meta::type_hash<pass_mark_value_as_mut>] = _register_builder<pass_mark_value_as_mut>();

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
            _registed_builder_function_id_list[meta::type_hash<pass_build_nil_type>] = _register_builder<pass_build_nil_type>();

            _registed_builder_function_id_list[meta::type_hash<pass_type_cast>] = _register_builder<pass_type_cast>();

            _registed_builder_function_id_list[meta::type_hash<pass_literal>] = _register_builder<pass_literal>();

            _registed_builder_function_id_list[meta::type_hash<pass_typeid>] = _register_builder<pass_typeid>();

            _registed_builder_function_id_list[meta::type_hash<pass_format_string>] = _register_builder<pass_format_string>();

            _registed_builder_function_id_list[meta::type_hash<pass_finish_format_string>] = _register_builder<pass_finish_format_string>();

            _registed_builder_function_id_list[meta::type_hash<pass_union_item>] = _register_builder<pass_union_item>();

            _registed_builder_function_id_list[meta::type_hash<pass_union_define>] = _register_builder<pass_union_define>();

            _registed_builder_function_id_list[meta::type_hash<pass_match>] = _register_builder<pass_match>();
            _registed_builder_function_id_list[meta::type_hash<pass_match_case_for_union>] = _register_builder<pass_match_case_for_union>();

            _registed_builder_function_id_list[meta::type_hash<pass_union_pattern>] = _register_builder<pass_union_pattern>();
            _registed_builder_function_id_list[meta::type_hash<pass_identifier_pattern>] = _register_builder<pass_identifier_pattern>();
            _registed_builder_function_id_list[meta::type_hash<pass_tuple_pattern<0>>] = _register_builder<pass_tuple_pattern<0>>();
            _registed_builder_function_id_list[meta::type_hash<pass_tuple_pattern<1>>] = _register_builder<pass_tuple_pattern<1>>();

            _registed_builder_function_id_list[meta::type_hash<pass_struct_member_def>] = _register_builder<pass_struct_member_def>();
            _registed_builder_function_id_list[meta::type_hash<pass_struct_member_init_pair>] = _register_builder<pass_struct_member_init_pair>();
            _registed_builder_function_id_list[meta::type_hash<pass_struct_type_define>] = _register_builder<pass_struct_type_define>();
            _registed_builder_function_id_list[meta::type_hash<pass_make_struct_instance>] = _register_builder<pass_make_struct_instance>();

            _registed_builder_function_id_list[meta::type_hash<pass_build_tuple_type>] = _register_builder<pass_build_tuple_type>();
            _registed_builder_function_id_list[meta::type_hash<pass_tuple_types_list>] = _register_builder<pass_tuple_types_list>();
            _registed_builder_function_id_list[meta::type_hash<pass_make_tuple>] = _register_builder<pass_make_tuple>();

            _registed_builder_function_id_list[meta::type_hash<pass_build_where_constraint>] = _register_builder<pass_build_where_constraint>();
            _registed_builder_function_id_list[meta::type_hash<pass_build_bind_map_monad>] = _register_builder<pass_build_bind_map_monad>();

            _registed_builder_function_id_list[meta::type_hash<pass_trib_expr>] = _register_builder<pass_trib_expr>();

            _registed_builder_function_id_list[meta::type_hash<pass_direct<0>>] = _register_builder<pass_direct<0>>();
            _registed_builder_function_id_list[meta::type_hash<pass_direct<1>>] = _register_builder<pass_direct<1>>();
            _registed_builder_function_id_list[meta::type_hash<pass_direct<2>>] = _register_builder<pass_direct<2>>();
            _registed_builder_function_id_list[meta::type_hash<pass_direct<3>>] = _register_builder<pass_direct<3>>();
            _registed_builder_function_id_list[meta::type_hash<pass_direct<4>>] = _register_builder<pass_direct<4>>();
            _registed_builder_function_id_list[meta::type_hash<pass_direct<5>>] = _register_builder<pass_direct<5>>();

            _registed_builder_function_id_list[meta::type_hash<pass_macro_failed>] = _register_builder<pass_macro_failed>();
            _registed_builder_function_id_list[meta::type_hash<pass_do_expr_as_sentence>] = _register_builder<pass_do_expr_as_sentence>();
        }

    }

    grammar::rule operator>>(grammar::rule ost, size_t builder_index)
    {
        ost.first.builder_index = builder_index;
        return ost;
    }
}