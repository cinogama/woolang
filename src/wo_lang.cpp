#include "wo_lang.hpp"

namespace wo
{
    using namespace ast;
    void lang::check_function_where_constraint(ast::ast_base* ast, lexer* lang_anylizer, ast::ast_symbolable_base* func)
    {
        wo_assert(func != nullptr && ast != nullptr);
        auto* funcdef = dynamic_cast<ast::ast_value_function_define*>(func);
        if (funcdef == nullptr
            && func->symbol != nullptr
            && func->symbol->type == lang_symbol::symbol_type::function)
            // Why not get `funcdef` from symbol by this way if `func` is not a `ast_value_function_define`
            // ast_value_function_define's symbol will point to origin define, if `func` is an instance of 
            // template function define, the check will not-able to get correct error.
            auto* funcdef = func->symbol->get_funcdef();
        if (funcdef != nullptr
            && funcdef->where_constraint != nullptr
            && !funcdef->where_constraint->accept)
        {
            lang_anylizer->lang_error(lexer::errorlevel::error, ast, WO_ERR_FAILED_TO_INVOKE_BECAUSE);
            for (auto& error_info : funcdef->where_constraint->unmatched_constraint)
            {
                lang_anylizer->get_cur_error_frame().push_back(error_info);
            }
        }

    }

    namespace ast
    {
        bool ast_type::is_same(const ast_type* another, bool ignore_prefix) const
        {
            if (is_pending_function() || another->is_pending_function())
                return false;

            if (is_hkt() && another->is_hkt())
            {
                auto* ltsymb = symbol ? base_typedef_symbol(symbol) : nullptr;
                auto* rtsymb = another->symbol ? base_typedef_symbol(another->symbol) : nullptr;

                if (ltsymb && rtsymb && ltsymb == rtsymb)
                    return true;
                if (ltsymb == nullptr || rtsymb == nullptr)
                {
                    if (ltsymb && ltsymb->type == lang_symbol::symbol_type::type_alias)
                    {
                        wo_assert(another->value_type == wo::value::valuetype::dict_type
                            || another->value_type == wo::value::valuetype::array_type);
                        return ltsymb->type_informatiom->value_type == another->value_type
                            && ltsymb->type_informatiom->type_name == another->type_name;
                    }
                    if (rtsymb && rtsymb->type == lang_symbol::symbol_type::type_alias)
                    {
                        wo_assert(value_type == wo::value::valuetype::dict_type
                            || value_type == wo::value::valuetype::array_type);
                        return rtsymb->type_informatiom->value_type == value_type
                            && rtsymb->type_informatiom->type_name == type_name;
                    }
                    // both nullptr, check base type
                    wo_assert(another->value_type == wo::value::valuetype::dict_type
                        || another->value_type == wo::value::valuetype::array_type);
                    wo_assert(value_type == wo::value::valuetype::dict_type
                        || value_type == wo::value::valuetype::array_type);
                    return value_type == another->value_type
                        && type_name == another->type_name;
                }
                else if (ltsymb->type == lang_symbol::symbol_type::type_alias && rtsymb->type == lang_symbol::symbol_type::type_alias)
                {
                    // TODO: struct/pending type need check, struct!
                    return ltsymb->type_informatiom->value_type == rtsymb->type_informatiom->value_type;
                }
                return false;
            }

            if (is_pending() || another->is_pending())
                return false;

            if (!ignore_prefix)
            {
                if (is_mutable() != another->is_mutable())
                    return false;
            }

            if (is_function())
            {
                if (!another->is_function())
                    return false;

                if (argument_types.size() != another->argument_types.size())
                    return false;

                for (size_t index = 0; index < argument_types.size(); index++)
                {
                    if (!argument_types[index]->is_same(another->argument_types[index], true))
                        return false;
                }
                if (is_variadic_function_type != another->is_variadic_function_type)
                    return false;

                wo_assert(is_function() && another->is_function());
                if (!function_ret_type->is_same(another->function_ret_type, false))
                    return false;
            }
            else if (another->is_function())
                return false;

            if (type_name != another->type_name)
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
                    if (!using_type_name->template_arguments[i]->is_same(another->using_type_name->template_arguments[i], false))
                        return false;
            }

            if (template_arguments.size() != another->template_arguments.size())
                return false;
            for (size_t index = 0; index < template_arguments.size(); index++)
            {
                if (!template_arguments[index]->is_same(another->template_arguments[index], false))
                    return false;
            }

            // NOTE: Only anonymous structs need this check
            if (is_struct() && using_type_name == nullptr)
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

                    if (!memberinfo.member_type->is_same(fnd->second.member_type, false))
                        return false;
                }
            }
            return true;
        }

        bool ast_type::accept_type(const ast_type* another, bool ignore_using_type, bool ignore_prefix) const
        {
            if (is_pending_function() || another->is_pending_function())
                return false;

            if (is_hkt() && another->is_hkt())
            {
                auto* ltsymb = symbol ? base_typedef_symbol(symbol) : nullptr;
                auto* rtsymb = another->symbol ? base_typedef_symbol(another->symbol) : nullptr;

                if (ltsymb && rtsymb && ltsymb == rtsymb)
                    return true;
                if (ltsymb == nullptr || rtsymb == nullptr)
                {
                    if (ltsymb && ltsymb->type == lang_symbol::symbol_type::type_alias)
                    {
                        wo_assert(another->value_type == wo::value::valuetype::dict_type
                            || another->value_type == wo::value::valuetype::array_type);
                        return ltsymb->type_informatiom->value_type == another->value_type
                            && ltsymb->type_informatiom->type_name == another->type_name;
                    }
                    if (rtsymb && rtsymb->type == lang_symbol::symbol_type::type_alias)
                    {
                        wo_assert(value_type == wo::value::valuetype::dict_type
                            || value_type == wo::value::valuetype::array_type);
                        return rtsymb->type_informatiom->value_type == value_type
                            && rtsymb->type_informatiom->type_name == type_name;
                    }
                    // both nullptr, check base type
                    wo_assert(another->value_type == wo::value::valuetype::dict_type
                        || another->value_type == wo::value::valuetype::array_type);
                    wo_assert(value_type == wo::value::valuetype::dict_type
                        || value_type == wo::value::valuetype::array_type);
                    return value_type == another->value_type
                        && type_name == another->type_name;
                }
                else if (ltsymb->type == lang_symbol::symbol_type::type_alias && rtsymb->type == lang_symbol::symbol_type::type_alias)
                {
                    // TODO: struct/pending type need check, struct!
                    return ltsymb->type_informatiom->value_type == rtsymb->type_informatiom->value_type;
                }
                return false;
            }

            if (is_pending() || another->is_pending())
                return false;

            if (another->is_nothing())
                return true; // Buttom type, OK

            if (!ignore_prefix)
            {
                if (is_mutable() != another->is_mutable())
                    return false;
            }

            if (is_function())
            {
                if (!another->is_function())
                    return false;

                if (argument_types.size() != another->argument_types.size())
                    return false;
                for (size_t index = 0; index < argument_types.size(); index++)
                {
                    // Woolang 1.12.6: Argument types of functions are no longer covariant but
                    //                  invariant during associating and receiving
                    if (!argument_types[index]->is_same(another->argument_types[index], false))
                        return false;
                }
                if (is_variadic_function_type != another->is_variadic_function_type)
                    return false;

                wo_assert(is_function() && another->is_function());
                if (!function_ret_type->accept_type(another->function_ret_type, ignore_using_type, false))
                    return false;
            }
            else if (another->is_function())
                return false;

            if (type_name != another->type_name)
                return false;

            if (!ignore_using_type && (using_type_name || another->using_type_name))
            {
                if (!using_type_name || !another->using_type_name)
                    return false;

                if (find_type_in_this_scope(using_type_name) != find_type_in_this_scope(another->using_type_name))
                    return false;

                if (using_type_name->template_arguments.size() != another->using_type_name->template_arguments.size())
                    return false;

                for (size_t i = 0; i < using_type_name->template_arguments.size(); ++i)
                    if (!using_type_name->template_arguments[i]->accept_type(
                        another->using_type_name->template_arguments[i], ignore_using_type, false))
                        return false;
            }

            if (template_arguments.size() != another->template_arguments.size())
                return false;
            for (size_t index = 0; index < template_arguments.size(); index++)
            {
                if (!template_arguments[index]->accept_type(
                    another->template_arguments[index], ignore_using_type, false))
                    return false;
            }

            // NOTE: Only anonymous structs need this check
            if (is_struct() && (using_type_name == nullptr || another->using_type_name == nullptr))
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

                    if (!memberinfo.member_type->is_same(fnd->second.member_type, false))
                        return false;
                }
            }
            return true;
        }
        std::wstring ast_type::get_type_name(bool ignore_using_type, bool ignore_prefix) const
        {
            std::wstring result;

            if (!ignore_prefix)
            {
                if (is_mutable())
                    result += L"mut ";
            }

            if (!ignore_using_type && using_type_name)
            {
                result += using_type_name->get_type_name(ignore_using_type, true);
            }
            else
            {
                if (is_function())
                {
                    result += L"(";
                    for (size_t index = 0; index < argument_types.size(); index++)
                    {
                        result += argument_types[index]->get_type_name(ignore_using_type, false);
                        if (index + 1 != argument_types.size() || is_variadic_function_type)
                            result += L", ";
                    }

                    if (is_variadic_function_type)
                    {
                        result += L"...";
                    }
                    result += L")=>" + function_ret_type->get_type_name(ignore_using_type, false);
                }
                else
                {
                    if (is_hkt_typing() && symbol)
                    {
                        auto* base_symbol = base_typedef_symbol(symbol);
                        wo_assert(base_symbol && base_symbol->name != nullptr);
                        result += *base_symbol->name;
                    }
                    else if (type_name != WO_PSTR(tuple))
                    {
                        result += get_namespace_chain() + *type_name;
                    }

                    if (has_template() || type_name == WO_PSTR(tuple))
                    {
                        result += (type_name != WO_PSTR(tuple)) ? L"<" : L"(";
                        for (size_t index = 0; index < template_arguments.size(); index++)
                        {
                            result += template_arguments[index]->get_type_name(ignore_using_type, false);
                            if (index + 1 != template_arguments.size())
                                result += L", ";
                        }
                        result += (type_name != WO_PSTR(tuple)) ? L">" : L")";
                    }

                    if (is_struct())
                    {
                        result += L"{";
                        std::vector<const ast_type::struct_member_infos_t::value_type*> memberinfors;
                        memberinfors.reserve(struct_member_index.size());
                        for (auto& memberinfo : this->struct_member_index)
                        {
                            memberinfors.push_back(&memberinfo);
                        }
                        std::sort(memberinfors.begin(), memberinfors.end(),
                            [](const ast_type::struct_member_infos_t::value_type* a,
                                const ast_type::struct_member_infos_t::value_type* b)
                            {
                                return a->second.offset < b->second.offset;
                            });
                        bool first_member = true;
                        for (auto* memberinfo : memberinfors)
                        {
                            if (first_member)
                                first_member = false;
                            else
                                result += L",";

                            result += *memberinfo->first + L":" + memberinfo->second.member_type->get_type_name(false, false);
                        }
                        result += L"}";
                    }
                }
            }
            return result;
        }
        bool ast_type::is_hkt() const
        {
            if (is_function())
                return false;   // HKT cannot be return-type of function

            if (is_hkt_typing())
                return true;

            if (template_arguments.empty())
            {
                if (symbol && symbol->is_template_symbol && is_custom())
                    return true;
                else if (is_array() || is_dict() || is_vec() || is_map())
                    return true;
            }
            return false;
        }

        bool ast_type::is_hkt_typing() const
        {
            if (symbol)
                return symbol->is_hkt_typing_symb;
            return false;
        }

        lang_symbol* ast_type::base_typedef_symbol(lang_symbol* symb)
        {
            wo_assert(symb->type == lang_symbol::symbol_type::typing
                || symb->type == lang_symbol::symbol_type::type_alias);

            if (symb->type == lang_symbol::symbol_type::typing)
                return symb;
            else if (symb->type_informatiom->symbol)
                return base_typedef_symbol(symb->type_informatiom->symbol);
            else
                return symb;
        }

        void ast_value_variable::update_constant_value(lexer* lex)
        {
            if (is_constant)
                return;

            // TODO: constant variable here..
            if (symbol
                && !symbol->is_captured_variable
                && symbol->decl == identifier_decl::IMMUTABLE)
            {
                symbol->variable_value->eval_constant_value(lex);
                if (symbol->variable_value->is_constant)
                {
                    is_constant = true;
                    symbol->is_constexpr = true;
                    constant_value.set_val_compile_time(&symbol->variable_value->get_constant_value());
                }
            }
        }
    }

    //////////////////////////////////////////////////////////////////////////////////////////////

    uint32_t lang::get_typing_hash_after_pass1(ast::ast_type* typing)
    {
        uint64_t hashval = (uint64_t)typing->value_type;

        if (typing->is_hkt())
        {
            if (typing->value_type != wo::value::valuetype::array_type
                && typing->value_type != wo::value::valuetype::dict_type)
            {
                wo_assert(typing->symbol);
                auto* base_type_symb = ast::ast_type::base_typedef_symbol(typing->symbol);

                if (base_type_symb->type == lang_symbol::symbol_type::type_alias)
                {
                    if (base_type_symb->type_informatiom->using_type_name)
                        hashval = (uint64_t)base_type_symb->type_informatiom->using_type_name->symbol;
                    else
                        hashval = (uint64_t)base_type_symb->type_informatiom->value_type;
                }
                else
                    hashval = (uint64_t)base_type_symb;
            }
            else
            {
                hashval = (uint64_t)reinterpret_cast<intptr_t>(typing->type_name);
            }
        }

        ++hashval;

        if (typing->is_function())
            hashval += get_typing_hash_after_pass1(typing->function_ret_type);

        hashval *= hashval;

        if (lang_symbol* using_type_symb = typing->using_type_name ? find_type_in_this_scope(typing->using_type_name) : nullptr)
        {
            hashval <<= 1;
            hashval += (uint64_t)using_type_symb;
            hashval *= hashval;
        }

        if (!typing->is_hkt_typing() && typing->has_template())
        {
            for (auto& template_arg : typing->template_arguments)
            {
                hashval <<= 1;
                hashval += get_typing_hash_after_pass1(template_arg);
                hashval *= hashval;
            }
        }

        if (typing->is_function())
        {
            for (auto& arg : typing->argument_types)
            {
                hashval <<= 1;
                hashval += get_typing_hash_after_pass1(arg);
                hashval *= hashval;
            }
        }

        if (typing->is_struct() && typing->using_type_name == nullptr)
        {
            for (auto& [member_name, member_info] : typing->struct_member_index)
            {
                hashval ^= std::hash<std::wstring>()(*member_name);
                hashval *= member_info.offset;
                hashval += get_typing_hash_after_pass1(member_info.member_type);
            }
        }

        if (typing->is_mutable())
            hashval *= hashval;

        uint32_t hash32 = hashval & 0xFFFFFFFF;
        while (hashed_typing.find(hash32) != hashed_typing.end())
        {
            if (hashed_typing[hash32]->is_same(typing, false))
                return hash32;
            hash32++;
        }
        hashed_typing[hash32] = typing;

        return hash32;
    }
    bool lang::begin_template_scope(
        ast::ast_base* reporterr,
        const std::vector<wo_pstring_t>& template_defines_args,
        const std::vector<ast::ast_type*>& template_args)
    {
        wo_assert(reporterr);
        if (template_defines_args.size() != template_args.size())
        {
            lang_anylizer->lang_error(lexer::errorlevel::error, reporterr, WO_ERR_TEMPLATE_ARG_NOT_MATCH);
            return false;
        }

        template_stack.push_back(template_type_map());
        auto& current_template = template_stack.back();
        for (size_t index = 0; index < template_defines_args.size(); index++)
        {
            auto* applying_type = template_args[index];
            if (applying_type == nullptr)
                continue;

            lang_symbol* sym = new lang_symbol;
            sym->attribute = new ast::ast_decl_attribute();
            sym->type = lang_symbol::symbol_type::type_alias;
            sym->name = template_defines_args[index];
            sym->type_informatiom = new ast::ast_type(WO_PSTR(pending));
            sym->has_been_completed_defined = true;
            sym->type_informatiom->set_type(applying_type);

            sym->defined_in_scope = lang_scopes.back();

            if (sym->type_informatiom->is_hkt())
            {
                sym->is_hkt_typing_symb = true;
                sym->is_template_symbol = true;

                sym->type_informatiom->template_arguments.clear();

                // Update template setting info...
                if (sym->type_informatiom->is_array() || sym->type_informatiom->is_vec())
                {
                    sym->template_types = { WO_PSTR(VT) };
                    sym->type_informatiom->template_arguments.push_back(
                        new ast::ast_type(WO_PSTR(VT)));
                }
                else if (sym->type_informatiom->is_dict() || sym->type_informatiom->is_map())
                {
                    sym->template_types = { WO_PSTR(KT), WO_PSTR(VT) };
                    sym->type_informatiom->template_arguments.push_back(
                        new ast::ast_type(WO_PSTR(KT)));
                    sym->type_informatiom->template_arguments.push_back(
                        new ast::ast_type(WO_PSTR(VT)));
                }
                else
                {
                    wo_assert(sym->type_informatiom->symbol && sym->type_informatiom->symbol->is_template_symbol);

                    sym->template_types = sym->type_informatiom->symbol->template_types;
                    wo_assert(sym->type_informatiom->symbol);

                    for (auto& template_def_name : sym->type_informatiom->symbol->template_types)
                    {
                        sym->type_informatiom->template_arguments.push_back(
                            new ast::ast_type(template_def_name));
                    }
                }
            }

            lang_symbols.push_back(current_template[sym->name] = sym);
        }
        return true;
    }
    bool lang::begin_template_scope(ast::ast_base* reporterr, ast::ast_defines* template_defines, const std::vector<ast::ast_type*>& template_args)
    {
        return begin_template_scope(reporterr, template_defines->template_type_name_list, template_args);
    }
    void lang::end_template_scope()
    {
        template_stack.pop_back();
    }
    void lang::temporary_entry_scope_in_pass1(lang_scope* scop)
    {
        lang_scopes.push_back(scop);
    }
    lang_scope* lang::temporary_leave_scope_in_pass1()
    {
        auto* scop = lang_scopes.back();

        lang_scopes.pop_back();

        return scop;
    }

    lang::lang(lexer& lex) :
        lang_anylizer(&lex)
    {
        _this_thread_lang_context = this;
        ast::ast_namespace* global = new ast::ast_namespace;
        global->scope_name = wstring_pool::get_pstr(L"");
        global->source_file = wstring_pool::get_pstr(L"::");
        global->col_begin_no =
            global->col_end_no =
            global->row_begin_no =
            global->row_end_no = 1;
        begin_namespace(global);   // global namespace

        // Define 'char' as built-in type
        ast::ast_using_type_as* using_type_def_char = new ast::ast_using_type_as();
        using_type_def_char->new_type_identifier = WO_PSTR(char);
        using_type_def_char->old_type = new ast::ast_type(WO_PSTR(int));
        using_type_def_char->declear_attribute = new ast::ast_decl_attribute();
        using_type_def_char->declear_attribute->add_attribute(lang_anylizer, lex_type::l_public);
        define_type_in_this_scope(
            using_type_def_char,
            using_type_def_char->old_type,
            using_type_def_char->declear_attribute)->has_been_completed_defined = true;
    }
    lang::~lang()
    {
        _this_thread_lang_context = nullptr;
        clean_and_close_lang();
    }
    ast::ast_type* lang::generate_type_instance_by_templates(lang_symbol* symb, const std::vector<ast::ast_type*>& templates)
    {
        std::vector<uint32_t> hashs;
        for (auto& template_type : templates)
        {
            if (template_type->is_pending() && !template_type->is_hkt())
                return nullptr;

            hashs.push_back(get_typing_hash_after_pass1(template_type));
        }

        auto fnd = symb->template_type_instances.find(hashs);
        if (fnd == symb->template_type_instances.end())
        {
            auto& newtype = symb->template_type_instances[hashs];
            newtype = new ast::ast_type(WO_PSTR(pending));

            newtype->set_type(symb->type_informatiom);
            symb->type_informatiom->instance(newtype);

            if (symb->type_informatiom->source_file != nullptr)
                newtype->copy_source_info(symb->type_informatiom);

            temporary_entry_scope_in_pass1(symb->defined_in_scope);
            {
                if (begin_template_scope(symb->type_informatiom, symb->template_types, templates))
                {
                    std::unordered_set<ast::ast_type*> _repeat_guard;
                    update_typeof_in_type(newtype, _repeat_guard);

                    end_template_scope();
                }
            }
            temporary_leave_scope_in_pass1();
        }
        return symb->template_type_instances[hashs];
    }
    void lang::update_typeof_in_type(ast::ast_type* type, std::unordered_set<ast::ast_type*>& s)
    {
        if (s.find(type) != s.end())
            return;

        s.insert(type);

        // NOTE: Only work for update typefrom;
        if (type->typefrom != nullptr)
            analyze_pass1(type->typefrom);

        if (type->has_template())
            for (auto* t : type->template_arguments)
                update_typeof_in_type(t, s);

        if (type->is_struct() && type->using_type_name == nullptr)
        {
            for (auto& [_, memberinfo] : type->struct_member_index)
                update_typeof_in_type(memberinfo.member_type, s);
        }
        else if (type->is_function())
        {
            for (auto* t : type->argument_types)
                update_typeof_in_type(t, s);
            update_typeof_in_type(type->function_ret_type, s);
        }
    }
    bool lang::fully_update_type(ast::ast_type* type, bool in_pass_1, const std::vector<wo_pstring_t>& template_types, std::unordered_set<ast::ast_type*>& s)
    {
        if (s.find(type) != s.end())
            return true;

        s.insert(type);

        if (in_pass_1 && type->searching_begin_namespace_in_pass2 == nullptr)
        {
            type->searching_begin_namespace_in_pass2 = now_scope();
            if (type->source_file == nullptr)
                type->copy_source_info(type->searching_begin_namespace_in_pass2->last_entry_ast);
        }

        if (type->typefrom != nullptr)
        {
            bool is_mutable_typeof = type->is_mutable();
            bool is_force_immut_typeof = type->is_force_immutable();

            auto used_type_info = type->using_type_name;

            if (in_pass_1)
                analyze_pass1(type->typefrom, false);
            if (has_step_in_step2)
                analyze_pass2(type->typefrom, false);

            if (!type->typefrom->value_type->is_pending())
            {
                if (type->is_function())
                    type->set_ret_type(type->typefrom->value_type);
                else
                    type->set_type(type->typefrom->value_type);
            }

            if (used_type_info)
                type->using_type_name = used_type_info;
            if (is_mutable_typeof)
                type->set_is_mutable(true);
            if (is_force_immut_typeof)
                type->set_is_force_immutable();
        }

        if (type->using_type_name)
        {
            if (!type->using_type_name->symbol)
                type->using_type_name->symbol = find_type_in_this_scope(type->using_type_name);
        }

        // todo: begin_template_scope here~
        if (type->has_custom())
        {
            bool stop_update = false;
            if (type->is_function())
            {
                if (type->function_ret_type->has_custom() && !type->function_ret_type->is_hkt())
                {
                    if (fully_update_type(type->function_ret_type, in_pass_1, template_types, s))
                    {
                        if (type->function_ret_type->has_custom() && !type->function_ret_type->is_hkt())
                            stop_update = true;
                    }
                }
                for (auto& a_t : type->argument_types)
                {
                    if (a_t->has_custom() && !a_t->is_hkt())
                        if (fully_update_type(a_t, in_pass_1, template_types, s))
                        {
                            if (a_t->has_custom() && !a_t->is_hkt())
                                stop_update = true;
                        }
                }
            }

            for (auto* template_type : type->template_arguments)
            {
                if (template_type->has_custom() && !template_type->is_hkt())
                    if (fully_update_type(template_type, in_pass_1, template_types, s))
                        if (template_type->has_custom() && !template_type->is_hkt())
                            stop_update = true;
            }

            // ready for update..
            if (!stop_update && ast::ast_type::is_custom_type(type->type_name))
            {
                if (!type->scope_namespaces.empty() ||
                    type->search_from_global_namespace ||
                    template_types.end() == std::find(template_types.begin(), template_types.end(), type->type_name))
                {
                    lang_symbol* type_sym = find_type_in_this_scope(type);

                    if (nullptr == type_sym)
                        type_sym = type->symbol;

                    if (traving_symbols.find(type_sym) != traving_symbols.end())
                        return false;

                    struct traving_guard
                    {
                        lang* _lang;
                        lang_symbol* _tving_node;
                        traving_guard(lang* _lg, lang_symbol* ast_ndoe)
                            : _lang(_lg)
                            , _tving_node(ast_ndoe)
                        {
                            _lang->traving_symbols.insert(_tving_node);
                        }
                        ~traving_guard()
                        {
                            _lang->traving_symbols.erase(_tving_node);
                        }
                    };

                    if (type_sym
                        && type_sym->type != lang_symbol::symbol_type::typing
                        && type_sym->type != lang_symbol::symbol_type::type_alias)
                    {
                        lang_anylizer->lang_error(lexer::errorlevel::error, type, WO_ERR_IS_NOT_A_TYPE, type_sym->name->c_str());
                        type_sym = nullptr;
                    }

                    traving_guard g1(this, type_sym);

                    if (type_sym)
                    {
                        if (type_sym->has_been_completed_defined == false)
                            return false;

                        auto already_has_using_type_name = type->using_type_name;

                        auto type_has_mutable_mark = type->is_mutable();
                        bool is_force_immut_typeof = type->is_force_immutable();

                        bool using_template = false;
                        const auto using_template_args = type->template_arguments;

                        type->symbol = type_sym;
                        if (type_sym->template_types.size() != type->template_arguments.size())
                        {
                            // Template count is not match.
                            if (type->has_template())
                            {
                                if (in_pass_1 == false)
                                {
                                    // Error! if template_arguments.size() is 0, it will be 
                                    // high-ranked-templated type.
                                    if (type->template_arguments.size() > type_sym->template_types.size())
                                    {
                                        lang_anylizer->lang_error(lexer::errorlevel::error, type, WO_ERR_TOO_MANY_TEMPLATE_ARGS,
                                            type_sym->name->c_str());
                                    }
                                    else
                                    {
                                        wo_assert(type->template_arguments.size() < type_sym->template_types.size());
                                        lang_anylizer->lang_error(lexer::errorlevel::error, type, WO_ERR_TOO_FEW_TEMPLATE_ARGS,
                                            type_sym->name->c_str());
                                    }

                                    lang_anylizer->lang_error(lexer::errorlevel::infom, type, WO_INFO_THE_TYPE_IS_ALIAS_OF,
                                        type_sym->name->c_str(),
                                        type_sym->type_informatiom->get_type_name(false).c_str());
                                }

                            }
                        }
                        else
                        {
                            if (type->has_template())
                                using_template =
                                type_sym->define_node != nullptr ?
                                begin_template_scope(type, type_sym->define_node, type->template_arguments)
                                : (type_sym->type_informatiom->using_type_name
                                    ? begin_template_scope(type, type_sym->type_informatiom->using_type_name->symbol->define_node, type->template_arguments)
                                    : begin_template_scope(type, type_sym->template_types, type->template_arguments));

                            ast::ast_type* symboled_type = nullptr;

                            if (using_template)
                            {
                                // template arguments not anlyzed.
                                if (auto* template_instance_type =
                                    generate_type_instance_by_templates(type_sym, type->template_arguments))
                                    // Get template type instance.
                                    symboled_type = template_instance_type;
                                else
                                {
                                    // Failed to instance current template type, skip.
                                    end_template_scope();
                                    return false;
                                }
                            }
                            else
                                symboled_type = type_sym->type_informatiom;
                            //symboled_type->set_type(type_sym->type_informatiom);

                            wo_assert(symboled_type != nullptr);

                            // NOTE: The type here should have all the template parameters applied.
                            // We should not need give `template_types` here.
                            fully_update_type(symboled_type, in_pass_1, {}, s);

                            // NOTE: In old version, function's return type is not stores in complex.
                            //       But now, the type here cannot be a function.
                            wo_assert(type->is_function() == false);

                            // NOTE: Set type will make new instance of typefrom, but it will cause symbol loss.
                            //       To avoid dup-copy, we will let new type use typedef's typefrom directly.
                            do {
                                auto* defined_typefrom = symboled_type->typefrom;
                                symboled_type->typefrom = nullptr;
                                type->set_type(symboled_type);
                                type->typefrom = symboled_type->typefrom = defined_typefrom;
                            } while (false);

                            if (already_has_using_type_name)
                                type->using_type_name = already_has_using_type_name;
                            else if (type_sym->type != lang_symbol::symbol_type::type_alias)
                            {
                                auto* using_type = new ast::ast_type(type_sym->name);
                                using_type->template_arguments = type->template_arguments;

                                type->using_type_name = using_type;
                                type->using_type_name->template_arguments = using_template_args;

                                // Gen namespace chain
                                auto* inscopes = type_sym->defined_in_scope;
                                while (inscopes && inscopes->belong_namespace)
                                {
                                    if (inscopes->type == lang_scope::scope_type::namespace_scope)
                                    {
                                        type->using_type_name->scope_namespaces.insert(
                                            type->using_type_name->scope_namespaces.begin(),
                                            inscopes->scope_namespace);
                                    }
                                    inscopes = inscopes->belong_namespace;
                                }

                                type->using_type_name->symbol = type_sym;
                            }

                            if (type_has_mutable_mark)
                                // TODO; REPEATED MUT SIGN NEED REPORT ERROR?
                                type->set_is_mutable(true);
                            if (is_force_immut_typeof)
                                type->set_is_force_immutable();
                            if (using_template)
                                end_template_scope();
                        }// end template argu check & update
                    }
                }
            }
        }

        for (auto& [name, struct_info] : type->struct_member_index)
        {
            if (struct_info.member_type)
            {
                if (in_pass_1)
                    fully_update_type(struct_info.member_type, in_pass_1, template_types, s);
                if (has_step_in_step2)
                    fully_update_type(struct_info.member_type, in_pass_1, template_types, s);
            }
        }

        wo_test(!type->using_type_name || !type->using_type_name->using_type_name);
        return false;
    }
    void lang::fully_update_type(ast::ast_type* type, bool in_pass_1, const std::vector<wo_pstring_t>& template_types)
    {
        std::unordered_set<ast::ast_type*> us;
        wo_assert(type != nullptr);
        wo_assure(!fully_update_type(type, in_pass_1, template_types, us));
        wo_assert(type->using_type_name == nullptr || (type->is_mutable() == type->using_type_name->is_mutable()));
    }
    void lang::analyze_pattern_in_pass0(ast::ast_pattern_base* pattern, ast::ast_decl_attribute* attrib, ast::ast_value* initval)
    {
        using namespace ast;
        // Like analyze_pattern_in_pass1, but donot analyze initval.
        // Only declear the symbol.

        if (ast_pattern_takeplace* a_pattern_takeplace = dynamic_cast<ast_pattern_takeplace*>(pattern))
        {
        }
        else
        {
            if (ast_pattern_identifier* a_pattern_identifier = dynamic_cast<ast_pattern_identifier*>(pattern))
            {
                // Merge all attrib 
                a_pattern_identifier->attr->attributes.insert(attrib->attributes.begin(), attrib->attributes.end());

                if (a_pattern_identifier->template_arguments.empty())
                {
                    if (!a_pattern_identifier->symbol)
                    {
                        a_pattern_identifier->symbol = define_variable_in_this_scope(
                            a_pattern_identifier,
                            a_pattern_identifier->identifier,
                            initval,
                            a_pattern_identifier->attr,
                            template_style::NORMAL,
                            a_pattern_identifier->decl);
                    }
                }
                else
                {
                    // Template variable!!! we just define symbol here.
                    if (!a_pattern_identifier->symbol)
                    {
                        auto* symb = define_variable_in_this_scope(
                            a_pattern_identifier,
                            a_pattern_identifier->identifier,
                            initval,
                            a_pattern_identifier->attr,
                            template_style::IS_TEMPLATE_VARIABLE_DEFINE,
                            a_pattern_identifier->decl);
                        symb->is_template_symbol = true;
                        wo_assert(symb->template_types.empty());
                        symb->template_types = a_pattern_identifier->template_arguments;
                        a_pattern_identifier->symbol = symb;
                    }
                }
            }
            else if (ast_pattern_tuple* a_pattern_tuple = dynamic_cast<ast_pattern_tuple*>(pattern))
            {
                for (auto* take_place : a_pattern_tuple->tuple_takeplaces)
                    take_place->copy_source_info(pattern);

                for (size_t i = 0; i < a_pattern_tuple->tuple_takeplaces.size(); i++)
                    analyze_pattern_in_pass0(a_pattern_tuple->tuple_patterns[i], attrib, a_pattern_tuple->tuple_takeplaces[i]);
            }
            else
                lang_anylizer->lang_error(lexer::errorlevel::error, pattern, WO_ERR_UNEXPECT_PATTERN_MODE);
        }
    }
    void lang::analyze_pattern_in_pass1(ast::ast_pattern_base* pattern, ast::ast_decl_attribute* attrib, ast::ast_value* initval)
    {
        using namespace ast;

        if (ast_pattern_takeplace* a_pattern_takeplace = dynamic_cast<ast_pattern_takeplace*>(pattern))
        {
            analyze_pass1(initval);
        }
        else
        {
            if (ast_pattern_identifier* a_pattern_identifier = dynamic_cast<ast_pattern_identifier*>(pattern))
            {
                // Merge all attrib 
                a_pattern_identifier->attr->attributes.insert(attrib->attributes.begin(), attrib->attributes.end());

                if (a_pattern_identifier->template_arguments.empty())
                {
                    analyze_pass1(initval);

                    if (!a_pattern_identifier->symbol)
                    {
                        a_pattern_identifier->symbol = define_variable_in_this_scope(
                            a_pattern_identifier,
                            a_pattern_identifier->identifier,
                            initval,
                            a_pattern_identifier->attr,
                            template_style::NORMAL,
                            a_pattern_identifier->decl);
                    }
                }
                else
                {
                    // Template variable!!! we just define symbol here.
                    if (!a_pattern_identifier->symbol)
                    {
                        auto* symb = define_variable_in_this_scope(
                            a_pattern_identifier,
                            a_pattern_identifier->identifier,
                            initval,
                            a_pattern_identifier->attr,
                            template_style::IS_TEMPLATE_VARIABLE_DEFINE,
                            a_pattern_identifier->decl);
                        symb->is_template_symbol = true;
                        wo_assert(symb->template_types.empty());
                        symb->template_types = a_pattern_identifier->template_arguments;
                        a_pattern_identifier->symbol = symb;
                    }
                }
            }
            else if (ast_pattern_tuple* a_pattern_tuple = dynamic_cast<ast_pattern_tuple*>(pattern))
            {
                analyze_pass1(initval);
                for (auto* take_place : a_pattern_tuple->tuple_takeplaces)
                {
                    take_place->copy_source_info(pattern);
                }
                if (initval->value_type->is_tuple() && !initval->value_type->is_pending()
                    && initval->value_type->template_arguments.size() == a_pattern_tuple->tuple_takeplaces.size())
                {
                    for (size_t i = 0; i < a_pattern_tuple->tuple_takeplaces.size(); i++)
                    {
                        a_pattern_tuple->tuple_takeplaces[i]->value_type->set_type(initval->value_type->template_arguments[i]);
                    }
                }
                for (size_t i = 0; i < a_pattern_tuple->tuple_takeplaces.size(); i++)
                    analyze_pattern_in_pass1(a_pattern_tuple->tuple_patterns[i], attrib, a_pattern_tuple->tuple_takeplaces[i]);
            }
            else
                lang_anylizer->lang_error(lexer::errorlevel::error, pattern, WO_ERR_UNEXPECT_PATTERN_MODE);
        }
    }
    void lang::analyze_pattern_in_pass2(ast::ast_pattern_base* pattern, ast::ast_value* initval)
    {
        using namespace ast;

        if (ast_pattern_takeplace* a_pattern_takeplace = dynamic_cast<ast_pattern_takeplace*>(pattern))
        {
            analyze_pass2(initval);
        }
        else
        {
            if (ast_pattern_identifier* a_pattern_identifier = dynamic_cast<ast_pattern_identifier*>(pattern))
            {
                if (a_pattern_identifier->template_arguments.empty())
                {
                    analyze_pass2(initval);
                    a_pattern_identifier->symbol->has_been_completed_defined = true;
                }
                else
                {
                    a_pattern_identifier->symbol->has_been_completed_defined = true;
                    for (auto& [_, impl_symbol] : a_pattern_identifier->symbol->template_typehashs_reification_instance_symbol_list)
                    {
                        impl_symbol->has_been_completed_defined = true;
                    }
                }
            }
            else if (ast_pattern_tuple* a_pattern_tuple = dynamic_cast<ast_pattern_tuple*>(pattern))
            {
                analyze_pass2(initval);

                if (initval->value_type->is_tuple() && !initval->value_type->is_pending()
                    && initval->value_type->template_arguments.size() == a_pattern_tuple->tuple_takeplaces.size())
                {
                    for (size_t i = 0; i < a_pattern_tuple->tuple_takeplaces.size(); i++)
                    {
                        a_pattern_tuple->tuple_takeplaces[i]->value_type->set_type(initval->value_type->template_arguments[i]);
                    }
                    for (size_t i = 0; i < a_pattern_tuple->tuple_takeplaces.size(); i++)
                        analyze_pattern_in_pass2(a_pattern_tuple->tuple_patterns[i], a_pattern_tuple->tuple_takeplaces[i]);

                }
                else
                {
                    if (initval->value_type->is_pending())
                        lang_anylizer->lang_error(lexer::errorlevel::error, pattern, WO_ERR_UNMATCHED_PATTERN_TYPE_NOT_DECIDED);
                    else if (!initval->value_type->is_tuple())
                        lang_anylizer->lang_error(lexer::errorlevel::error, pattern, WO_ERR_UNMATCHED_PATTERN_TYPE_EXPECT_TUPLE,
                            initval->value_type->get_type_name(false).c_str());
                    else if (initval->value_type->template_arguments.size() != a_pattern_tuple->tuple_takeplaces.size())
                        lang_anylizer->lang_error(lexer::errorlevel::error, pattern, WO_ERR_UNMATCHED_PATTERN_TYPE_TUPLE_DNT_MATCH,
                            (int)a_pattern_tuple->tuple_takeplaces.size(),
                            (int)initval->value_type->template_arguments.size());
                }
            }
            else
                lang_anylizer->lang_error(lexer::errorlevel::error, pattern, WO_ERR_UNEXPECT_PATTERN_MODE);
        }
    }
    void lang::analyze_pattern_in_finalize(ast::ast_pattern_base* pattern, ast::ast_value* initval, bool in_pattern_expr, ir_compiler* compiler)
    {
        using namespace ast;
        using namespace opnum;

        if (ast_pattern_identifier* a_pattern_identifier = dynamic_cast<ast_pattern_identifier*>(pattern))
        {
            if (a_pattern_identifier->template_arguments.empty())
            {
                if (a_pattern_identifier->symbol->is_marked_as_used_variable == false
                    && a_pattern_identifier->symbol->define_in_function == true)
                {
                    auto* scope = a_pattern_identifier->symbol->defined_in_scope;
                    while (scope->type != wo::lang_scope::scope_type::function_scope)
                        scope = scope->parent_scope;

                    lang_anylizer->lang_error(lexer::errorlevel::error, pattern, WO_ERR_UNUSED_VARIABLE_DEFINE,
                        a_pattern_identifier->identifier->c_str(),
                        str_to_wstr(scope->function_node->get_ir_func_signature_tag()).c_str()
                    );
                }
                if (!a_pattern_identifier->symbol->is_constexpr_or_immut_no_closure_func())
                {
                    auto ref_ob = get_opnum_by_symbol(pattern, a_pattern_identifier->symbol, compiler);

                    if (auto** opnum = std::get_if<opnum::opnumbase*>(&ref_ob))
                    {
                        if (ast_value_takeplace* valtkpls = dynamic_cast<ast_value_takeplace*>(initval);
                            !valtkpls || valtkpls->used_reg)
                        {
                            compiler->mov(**opnum, analyze_value(initval, compiler));
                        }
                    }
                    else
                    {
                        auto stackoffset = std::get<int16_t>(ref_ob);
                        compiler->sts(analyze_value(initval, compiler), imm(stackoffset));
                    }
                }
            }
            else
            {
                // Template variable impl here, give init value to correct place.
                const auto& all_template_impl_variable_symbol
                    = a_pattern_identifier->symbol->template_typehashs_reification_instance_symbol_list;
                for (auto& [_, symbol] : all_template_impl_variable_symbol)
                {
                    if (!symbol->is_constexpr_or_immut_no_closure_func())
                    {
                        auto ref_ob = get_opnum_by_symbol(a_pattern_identifier, symbol, compiler);

                        if (auto** opnum = std::get_if<opnum::opnumbase*>(&ref_ob))
                        {
                            if (ast_value_takeplace* valtkpls = dynamic_cast<ast_value_takeplace*>(initval);
                                !valtkpls || valtkpls->used_reg)
                            {
                                compiler->mov(**opnum, analyze_value(symbol->variable_value, compiler));
                            }
                        }
                        else
                        {
                            auto stackoffset = std::get<int16_t>(ref_ob);
                            compiler->sts(analyze_value(initval, compiler), imm(stackoffset));
                        }
                    }
                } // end of for (auto& [_, symbol] : all_template_impl_variable_symbol)

            }
        }
        else if (ast_pattern_tuple* a_pattern_tuple = dynamic_cast<ast_pattern_tuple*>(pattern))
        {
            auto& struct_val = analyze_value(initval, compiler);
            auto& current_values = get_useable_register_for_pure_value();
            for (size_t i = 0; i < a_pattern_tuple->tuple_takeplaces.size(); i++)
            {
                // FOR OPTIMIZE, SKIP TAKEPLACE PATTERN
                if (dynamic_cast<ast_pattern_takeplace*>(a_pattern_tuple->tuple_patterns[i]) == nullptr)
                {
                    compiler->idstruct(current_values, struct_val, (uint16_t)i);
                    a_pattern_tuple->tuple_takeplaces[i]->used_reg = &current_values;

                    analyze_pattern_in_finalize(a_pattern_tuple->tuple_patterns[i], a_pattern_tuple->tuple_takeplaces[i], true, compiler);
                }
            }
            complete_using_register(current_values);
        }
        else if (ast_pattern_takeplace* a_pattern_takeplace = dynamic_cast<ast_pattern_takeplace*>(pattern))
        {
            // DO NOTHING
            if (!in_pattern_expr)
                analyze_value(initval, compiler);
        }
        else
            lang_anylizer->lang_error(lexer::errorlevel::error, pattern, WO_ERR_UNEXPECT_PATTERN_MODE);
    }

    void lang::check_division(ast::ast_base* divop, ast::ast_value* left, ast::ast_value* right, opnum::opnumbase& left_opnum, opnum::opnumbase& right_opnum, ir_compiler* compiler)
    {
        wo_assert(left->value_type->is_integer() == right->value_type->is_integer());
        if (left->value_type->is_integer())
        {
            std::optional<wo_integer_t> constant_right = std::nullopt;
            std::optional<wo_integer_t> constant_left = std::nullopt;

            if (right->is_constant)
            {
                auto& const_r_value = right->get_constant_value();
                wo_assert(const_r_value.type == wo::value::valuetype::integer_type);

                constant_right = std::optional(const_r_value.integer);
            }

            if (left->is_constant)
            {
                auto& const_l_value = left->get_constant_value();
                wo_assert(const_l_value.type == wo::value::valuetype::integer_type);

                constant_left = std::optional(const_l_value.integer);
            }

            if (constant_right.has_value())
            {
                auto const_rvalue = constant_right.value();
                if (const_rvalue == 0)
                    lang_anylizer->lang_error(wo::lexer::errorlevel::error, right, WO_ERR_CANNOT_DIV_ZERO);
                else if (const_rvalue == -1)
                {
                    if (constant_left.has_value())
                    {
                        if (constant_left.value() == INT64_MIN)
                            lang_anylizer->lang_error(wo::lexer::errorlevel::error, divop, WO_ERR_DIV_OVERFLOW);
                    }
                }
            }

            // Generate extra code for div checking.
            if (config::ENABLE_RUNTIME_CHECKING_INTEGER_DIVISION)
            {
                if (constant_right.has_value() == false)
                {
                    if (constant_left.has_value() == false)
                        compiler->ext_cdivilr(left_opnum, right_opnum);
                    else if (constant_left.value() == INT64_MIN)
                        compiler->ext_cdivir(right_opnum);
                    else
                        compiler->ext_cdivirz(right_opnum);
                }
                else if (constant_right.value() == -1)
                {
                    if (constant_left.has_value() == false)
                        compiler->ext_cdivil(left_opnum);
                }
            }
        }
    }

    void lang::collect_ast_nodes_for_pass1(ast::ast_base* ast_node)
    {
        std::vector<ast::ast_base*> _pass1_analyze_ast_nodes_list;
        std::stack<ast::ast_base*> _pending_ast_nodes;

        _pending_ast_nodes.push(ast_node);

        while (!_pending_ast_nodes.empty())
        {
            auto* cur_node = _pending_ast_nodes.top();
            _pending_ast_nodes.pop();

            if (_pass1_analyze_ast_nodes_list.end() !=
                std::find(_pass1_analyze_ast_nodes_list.begin(),
                    _pass1_analyze_ast_nodes_list.end(),
                    cur_node))
                // Cur node has been append to list. return!
                continue;

            _pass1_analyze_ast_nodes_list.push_back(cur_node);
        }
    }
    void lang::analyze_pass0(ast::ast_base* ast_node)
    {
        wo_assert(has_step_in_step2 == false);

        // Used for pre-define all global define, we need walk through all:
        //  ast_list
        //  ast_namespace
        //  ast_sentence_block
        // 
        // We will do pass1 job for following declare:
        //  ast_varref_defines
        //  ast_value_function_define
        //  ast_using_type_as

        this->m_global_pass_table->pass<0>(this, ast_node);
    }

    void lang::analyze_pass1(ast::ast_base* ast_node, bool type_degradation)
    {
        bool old_has_step_in_pass2 = has_step_in_step2;
        has_step_in_step2 = false;

        _analyze_pass1(ast_node, type_degradation);

        has_step_in_step2 = old_has_step_in_pass2;
    }
    void lang::_analyze_pass1(ast::ast_base* ast_node, bool type_degradation)
    {
        if (!ast_node)
            return;

        if (ast_node->completed_in_pass1)
            return;

        ast_node->completed_in_pass1 = true;

        using namespace ast;

        ast_node->located_scope = now_scope();

        if (ast_symbolable_base* a_symbol_ob = dynamic_cast<ast_symbolable_base*>(ast_node))
        {
            a_symbol_ob->searching_begin_namespace_in_pass2 = now_scope();
            if (a_symbol_ob->source_file == nullptr)
                a_symbol_ob->copy_source_info(a_symbol_ob->searching_begin_namespace_in_pass2->last_entry_ast);
        }
        if (ast_value* a_value = dynamic_cast<ast_value*>(ast_node))
        {
            a_value->value_type->copy_source_info(a_value);
        }

        ///////////////////////////////////////////////////////////////////////////////////////////

        if (false == this->m_global_pass_table->pass<1>(this, ast_node))
        {
            ast::ast_base* child = ast_node->children;
            while (child)
            {
                analyze_pass1(child);
                child = child->sibling;
            }
        }

        if (ast_value* a_val = dynamic_cast<ast_value*>(ast_node))
        {
            if (ast_defines* a_def = dynamic_cast<ast_defines*>(ast_node);
                a_def && a_def->is_template_define)
            {
                // Do nothing
            }
            else
            {
                wo_assert(a_val->value_type != nullptr);

                if (a_val->value_type->is_pending())
                {
                    if (!dynamic_cast<ast_value_function_define*>(a_val))
                        // ready for update..
                        fully_update_type(a_val->value_type, true);
                }
                if (!a_val->value_type->is_pending())
                {
                    if (a_val->value_type->is_mutable())
                    {
                        bool forced_mark_as_mutable = false;
                        if (dynamic_cast<ast_value_mutable*>(a_val) != nullptr
                            || dynamic_cast<ast_value_type_cast*>(a_val) != nullptr
                            || dynamic_cast<ast_value_type_judge*>(a_val) != nullptr)
                            forced_mark_as_mutable = true;

                        if (type_degradation && !forced_mark_as_mutable)
                        {
                            a_val->can_be_assign = true;
                            a_val->value_type->set_is_mutable(false);
                        }
                    }
                }
                a_val->eval_constant_value(lang_anylizer);
            }

        }
        wo_assert(ast_node->completed_in_pass1);
    }

    lang_symbol* lang::analyze_pass_template_reification(ast::ast_value_variable* origin_variable, std::vector<ast::ast_type*> template_args_types)
    {
        // NOTE: template_args_types will be modified in this function. donot use ref type.

        using namespace ast;
        std::vector<uint32_t> template_args_hashtypes;

        wo_assert(origin_variable->symbol != nullptr);
        if (origin_variable->symbol->type == lang_symbol::symbol_type::function)
        {
            if (origin_variable->symbol->get_funcdef()->is_template_define
                && origin_variable->symbol->get_funcdef()->template_type_name_list.size() == template_args_types.size())
            {
                // TODO: finding repeated template? goon
                ast_value_function_define* dumpped_template_func_define =
                    analyze_pass_template_reification(origin_variable->symbol->get_funcdef(), template_args_types);

                if (dumpped_template_func_define)
                    return dumpped_template_func_define->this_reification_lang_symbol;
                return nullptr;
            }
            else if (!template_args_types.empty())
            {
                lang_anylizer->lang_error(lexer::errorlevel::error, origin_variable, WO_ERR_NO_MATCHED_FUNC_TEMPLATE);
                return nullptr;
            }
            else
                // NOTE: Not specify template arguments, might want auto impl?
                // Give error in finalize step.
                return origin_variable->symbol;

        }
        else if (origin_variable->symbol->type == lang_symbol::symbol_type::variable)
        {
            // Is variable?
            for (auto temtype : template_args_types)
            {
                auto step_in_pass2 = has_step_in_step2;
                has_step_in_step2 = true;
                fully_update_type(temtype, true, origin_variable->symbol->template_types);
                has_step_in_step2 = step_in_pass2;

                template_args_hashtypes.push_back(get_typing_hash_after_pass1(temtype));
            }

            if (auto fnd = origin_variable->symbol->template_typehashs_reification_instance_symbol_list.find(template_args_hashtypes);
                fnd != origin_variable->symbol->template_typehashs_reification_instance_symbol_list.end())
            {
                return fnd->second;
            }

            ast_value* dumpped_template_init_value = dynamic_cast<ast_value*>(origin_variable->symbol->variable_value->instance());
            wo_assert(dumpped_template_init_value);

            lang_symbol* template_reification_symb = nullptr;

            temporary_entry_scope_in_pass1(origin_variable->symbol->defined_in_scope);
            if (begin_template_scope(origin_variable, origin_variable->symbol->template_types, template_args_types))
            {
                analyze_pass1(dumpped_template_init_value);

                auto step_in_pass2 = has_step_in_step2;
                has_step_in_step2 = false;

                template_reification_symb = define_variable_in_this_scope(
                    dumpped_template_init_value,
                    origin_variable->var_name,
                    dumpped_template_init_value,
                    origin_variable->symbol->attribute,
                    template_style::IS_TEMPLATE_VARIABLE_IMPL,
                    origin_variable->symbol->decl);
                end_template_scope();

                has_step_in_step2 = step_in_pass2;
            }
            temporary_leave_scope_in_pass1();
            origin_variable->symbol->template_typehashs_reification_instance_symbol_list[template_args_hashtypes] = template_reification_symb;
            return template_reification_symb;
        }
        else
        {
            lang_anylizer->lang_error(lexer::errorlevel::error, origin_variable, WO_ERR_NO_TEMPLATE_VARIABLE_OR_FUNCTION);
            return nullptr;
        }
    }

    ast::ast_value_function_define* lang::analyze_pass_template_reification(
        ast::ast_value_function_define* origin_template_func_define,
        std::vector<ast::ast_type*> template_args_types)
    {
        // NOTE: template_args_types will be modified in this function. donot use ref type.

        using namespace ast;

        std::vector<uint32_t> template_args_hashtypes;
        for (auto temtype : template_args_types)
        {
            auto step_in_pass2 = has_step_in_step2;
            has_step_in_step2 = true;

            fully_update_type(temtype, true, origin_template_func_define->template_type_name_list);

            has_step_in_step2 = step_in_pass2;

            if (temtype->is_pending() && !temtype->is_hkt())
            {
                lang_anylizer->lang_error(lexer::errorlevel::error, temtype, WO_ERR_UNKNOWN_TYPE, temtype->get_type_name(false).c_str());
                return nullptr;
            }
            template_args_hashtypes.push_back(get_typing_hash_after_pass1(temtype));
        }

        ast_value_function_define* dumpped_template_func_define = nullptr;

        if (auto fnd = origin_template_func_define->template_typehashs_reification_instance_list.find(template_args_hashtypes);
            fnd != origin_template_func_define->template_typehashs_reification_instance_list.end())
        {
            return  dynamic_cast<ast_value_function_define*>(fnd->second);
        }

        dumpped_template_func_define = dynamic_cast<ast_value_function_define*>(origin_template_func_define->instance());
        wo_assert(dumpped_template_func_define);

        // Reset compile state..
        dumpped_template_func_define->symbol = nullptr;
        dumpped_template_func_define->searching_begin_namespace_in_pass2 = nullptr;
        dumpped_template_func_define->completed_in_pass2 = false;
        dumpped_template_func_define->is_template_define = false;
        dumpped_template_func_define->is_template_reification = true;
        dumpped_template_func_define->this_reification_template_args = template_args_types;

        origin_template_func_define->template_typehashs_reification_instance_list[template_args_hashtypes] =
            dumpped_template_func_define;

        wo_assert(origin_template_func_define->symbol == nullptr
            || origin_template_func_define->symbol->defined_in_scope == origin_template_func_define->this_func_scope->parent_scope);

        temporary_entry_scope_in_pass1(origin_template_func_define->this_func_scope->parent_scope);
        if (begin_template_scope(dumpped_template_func_define, origin_template_func_define, template_args_types))
        {
            analyze_pass1(dumpped_template_func_define);

            auto step_in_pass2 = has_step_in_step2;
            has_step_in_step2 = false;

            end_template_scope();

            has_step_in_step2 = step_in_pass2;
        }
        temporary_leave_scope_in_pass1();

        lang_symbol* template_reification_symb = new lang_symbol;

        template_reification_symb->type = lang_symbol::symbol_type::function;
        template_reification_symb->name = dumpped_template_func_define->function_name;
        template_reification_symb->defined_in_scope = now_scope();
        template_reification_symb->attribute = dumpped_template_func_define->declear_attribute;
        template_reification_symb->variable_value = dumpped_template_func_define;

        dumpped_template_func_define->this_reification_lang_symbol = template_reification_symb;

        lang_symbols.push_back(template_reification_symb);

        return dumpped_template_func_define;
    }

    std::optional<lang::judge_result_t> lang::judge_auto_type_of_funcdef_with_type(
        ast::ast_base* errreport,
        lang_scope* located_scope,
        ast::ast_type* param,
        ast::ast_value* callaim,
        bool update,
        ast::ast_defines* template_defines,
        const std::vector<ast::ast_type*>* template_args)
    {
        wo_assert(located_scope != nullptr);
        if (!param->is_function())
            return std::nullopt;

        ast::ast_value_function_define* function_define = nullptr;

        ast::ast_value_mutable* marking_mutable = dynamic_cast<ast::ast_value_mutable*>(callaim);
        if (marking_mutable != nullptr)
        {
            callaim = marking_mutable->val;
        }

        if (auto* variable = dynamic_cast<ast::ast_symbolable_base*>(callaim))
        {
            if (variable->symbol != nullptr && variable->symbol->type == lang_symbol::symbol_type::function)
                function_define = variable->symbol->get_funcdef();
        }
        if (auto* funcdef = dynamic_cast<ast::ast_value_function_define*>(callaim))
            function_define = funcdef;

        if (function_define != nullptr && function_define->is_template_define
            && function_define->value_type->argument_types.size() == param->argument_types.size())
        {
            ast::ast_type* new_type = dynamic_cast<ast::ast_type*>(function_define->value_type->instance(nullptr));
            wo_assert(new_type->is_function());

            std::vector<ast::ast_type*> arg_func_template_args(function_define->template_type_name_list.size(), nullptr);

            for (size_t tempindex = 0; tempindex < function_define->template_type_name_list.size(); ++tempindex)
            {
                if (arg_func_template_args[tempindex] == nullptr)
                    for (size_t index = 0; index < new_type->argument_types.size(); ++index)
                    {
                        if (auto* pending_template_arg = analyze_template_derivation(
                            function_define->template_type_name_list[tempindex],
                            function_define->template_type_name_list,
                            new_type->argument_types[index],
                            param->argument_types[index]))
                        {
                            ast::ast_type* template_value_type = new ast::ast_type(WO_PSTR(pending));
                            template_value_type->set_type(pending_template_arg);
                            template_value_type->copy_source_info(function_define);

                            arg_func_template_args[tempindex] = template_value_type;
                        }
                    }
            }

            if (std::find(arg_func_template_args.begin(), arg_func_template_args.end(), nullptr) != arg_func_template_args.end())
                return std::nullopt;

            // Auto judge here...
            if (update)
            {
                if (!(template_defines && template_args) || begin_template_scope(errreport, template_defines, *template_args))
                {
                    temporary_entry_scope_in_pass1(located_scope);
                    {
                        for (auto* template_arg : arg_func_template_args)
                            fully_update_type(template_arg, true);
                    }
                    temporary_leave_scope_in_pass1();

                    for (auto* template_arg : arg_func_template_args)
                        fully_update_type(template_arg, false);

                    if (template_defines && template_args)
                        end_template_scope();

                    auto* reificated = analyze_pass_template_reification(function_define, arg_func_template_args);

                    if (reificated != nullptr)
                    {

                        if (marking_mutable != nullptr)
                        {
                            marking_mutable->val = reificated;
                            marking_mutable->completed_in_pass2 = false;
                            analyze_pass2(marking_mutable);
                        }
                        else
                        {
                            analyze_pass2(reificated);
                        }
                        return reificated;
                    }
                }
                return std::nullopt;
            }
            else
            {

                if (begin_template_scope(errreport, function_define, arg_func_template_args))
                {
                    temporary_entry_scope_in_pass1(located_scope);
                    {
                        fully_update_type(new_type, true);
                    }
                    temporary_leave_scope_in_pass1();

                    fully_update_type(new_type, false);

                    end_template_scope();
                }

                return new_type;
            }
        }

        // no need to judge, 
        return std::nullopt;
    }

    std::vector<ast::ast_type*> lang::judge_auto_type_in_funccall(
        ast::ast_value_funccall* funccall,
        lang_scope* located_scope,
        bool update,
        ast::ast_defines* template_defines,
        const std::vector<ast::ast_type*>* template_args)
    {
        using namespace ast;

        std::vector<ast_value*> args_might_be_nullptr_if_unpack;
        std::vector<ast::ast_type*> new_arguments_types_result;

        auto* arg = dynamic_cast<ast_value*>(funccall->arguments->children);
        while (arg)
        {
            if (auto* fake_unpack_value = dynamic_cast<ast_fakevalue_unpacked_args*>(arg))
            {
                if (fake_unpack_value->unpacked_pack->value_type->is_tuple())
                {
                    wo_assert(fake_unpack_value->expand_count >= 0);

                    size_t expand_count = std::min((size_t)fake_unpack_value->expand_count,
                        fake_unpack_value->unpacked_pack->value_type->template_arguments.size());

                    for (size_t i = 0; i < expand_count; ++i)
                    {
                        args_might_be_nullptr_if_unpack.push_back(nullptr);
                        new_arguments_types_result.push_back(nullptr);
                    }
                }
            }
            else
            {
                args_might_be_nullptr_if_unpack.push_back(arg);
                new_arguments_types_result.push_back(nullptr);
            }
            arg = dynamic_cast<ast_value*>(arg->sibling);
        }

        // If a function has been implized, this flag will be setting and function's
        //  argument list will be updated later.
        bool has_updated_arguments = false;

        for (size_t i = 0; i < args_might_be_nullptr_if_unpack.size() && i < funccall->called_func->value_type->argument_types.size(); ++i)
        {
            if (args_might_be_nullptr_if_unpack[i] == nullptr)
                continue;

            std::optional<judge_result_t> judge_result = judge_auto_type_of_funcdef_with_type(
                funccall, // Used for report error.
                located_scope,
                funccall->called_func->value_type->argument_types[i],
                args_might_be_nullptr_if_unpack[i], update, template_defines, template_args);

            if (judge_result.has_value())
            {
                if (auto** realized_func = std::get_if<ast::ast_value_function_define*>(&judge_result.value()))
                {
                    auto* pending_variable = dynamic_cast<ast::ast_value_variable*>(args_might_be_nullptr_if_unpack[i]);
                    if (pending_variable != nullptr)
                    {
                        pending_variable->value_type->set_type((*realized_func)->value_type);
                        pending_variable->symbol = (*realized_func)->this_reification_lang_symbol;
                        check_function_where_constraint(pending_variable, lang_anylizer, *realized_func);
                    }
                    else
                    {
                        auto& arg = args_might_be_nullptr_if_unpack[i];
                        if (dynamic_cast<ast::ast_value_mutable*>(arg) == nullptr)
                        {
                            wo_assert(dynamic_cast<ast::ast_value_function_define*>(arg) != nullptr);
                            arg = *realized_func;
                        }
                    }
                    has_updated_arguments = true;
                }
                else
                {
                    auto* updated_type = std::get<ast::ast_type*>(judge_result.value());
                    wo_assert(updated_type != nullptr);
                    new_arguments_types_result[i] = updated_type;
                }
            }
        }

        if (has_updated_arguments)
        {
            // Re-generate argument list for current function-call;
            funccall->arguments->remove_all_childs();
            for (auto* arg : args_might_be_nullptr_if_unpack)
            {
                arg->sibling = nullptr;
                funccall->arguments->append_at_end(arg);
            }
        }

        return new_arguments_types_result;
    }

    void lang::analyze_pass2(ast::ast_base* ast_node, bool type_degradation)
    {
        bool old_has_step_in_pass2 = has_step_in_step2;
        has_step_in_step2 = true;

        _analyze_pass2(ast_node, type_degradation);

        has_step_in_step2 = old_has_step_in_pass2;
    }

    void lang::_analyze_pass2(ast::ast_base* ast_node, bool type_degradation)
    {
        wo_assert(ast_node);

        if (ast_node->completed_in_pass2)
            return;

        wo_assert(ast_node->completed_in_pass1 == true);
        ast_node->completed_in_pass2 = true;

        using namespace ast;

        if (ast_value* a_value = dynamic_cast<ast_value*>(ast_node))
        {
            a_value->eval_constant_value(lang_anylizer);

            if (ast_defines* a_defines = dynamic_cast<ast_defines*>(a_value);
                a_defines && a_defines->is_template_define)
            {
                for (auto& [typehashs, astnode] : a_defines->template_typehashs_reification_instance_list)
                {
                    analyze_pass2(astnode);
                }
            }
            else
            {
                // TODO: REPORT THE REAL UNKNOWN TYPE HERE, EXAMPLE:
                //       'void(ERRTYPE, int)' should report 'ERRTYPE', not 'void' or 'void(ERRTYPE, int)'
                if (a_value->value_type->may_need_update())
                {
                    // ready for update..
                    fully_update_type(a_value->value_type, false);

                    if (dynamic_cast<ast_value_function_define*>(a_value) == nullptr
                        // ast_value_make_struct_instance might need to auto judge types.
                        && dynamic_cast<ast_value_make_struct_instance*>(a_value) == nullptr
                        && a_value->value_type->has_custom())
                    {
                        lang_anylizer->lang_error(lexer::errorlevel::error, a_value, WO_ERR_UNKNOWN_TYPE
                            , a_value->value_type->get_type_name().c_str());

                        auto fuzz_symbol = find_symbol_in_this_scope(
                            a_value->value_type, a_value->value_type->type_name,
                            lang_symbol::symbol_type::typing | lang_symbol::symbol_type::type_alias, true);
                        if (fuzz_symbol)
                        {
                            auto fuzz_symbol_full_name = str_to_wstr(get_belong_namespace_path_with_lang_scope(fuzz_symbol));
                            if (!fuzz_symbol_full_name.empty())
                                fuzz_symbol_full_name += L"::";
                            fuzz_symbol_full_name += *fuzz_symbol->name;
                            lang_anylizer->lang_error(lexer::errorlevel::infom,
                                fuzz_symbol->type_informatiom,
                                WO_INFO_IS_THIS_ONE,
                                fuzz_symbol_full_name.c_str());
                        }
                    }
                }

                this->m_global_pass_table->pass<2>(this, ast_node);
            }

            if (ast_defines* a_def = dynamic_cast<ast_defines*>(ast_node);
                a_def && a_def->is_template_define)
            {
                // Do nothing
            }
            else
            {
                a_value->eval_constant_value(lang_anylizer);

                // some expr may set 'bool'/'char'..., it cannot used directly. update it.
                if (a_value->value_type->is_builtin_using_type())
                    fully_update_type(a_value->value_type, false);

                if (!a_value->value_type->is_pending())
                {
                    if (a_value->value_type->is_mutable())
                    {
                        bool forced_mark_as_mutable = false;
                        if (dynamic_cast<ast_value_mutable*>(a_value) != nullptr
                            || dynamic_cast<ast_value_type_cast*>(a_value) != nullptr
                            || dynamic_cast<ast_value_type_judge*>(a_value) != nullptr)
                            forced_mark_as_mutable = true;

                        if (type_degradation && !forced_mark_as_mutable)
                        {
                            a_value->can_be_assign = true;
                            a_value->value_type->set_is_mutable(false);
                        }
                    }
                }
            }
        }
        else
        {
            this->m_global_pass_table->pass<2>(this, ast_node);
        }

        ast::ast_base* child = ast_node->children;
        while (child)
        {
            analyze_pass2(child);
            child = child->sibling;
        }

        wo_assert(ast_node->completed_in_pass2);
    }
    void lang::clean_and_close_lang()
    {
        for (auto& created_symbols : lang_symbols)
            delete created_symbols;
        for (auto* created_scopes : lang_scopes_buffers)
        {
            delete created_scopes;
        }
        for (auto* created_temp_opnum : generated_opnum_list_for_clean)
            delete created_temp_opnum;
        for (auto generated_ast_node : generated_ast_nodes_buffers)
            delete generated_ast_node;

        lang_symbols.clear();
        lang_scopes_buffers.clear();
        generated_opnum_list_for_clean.clear();
        generated_ast_nodes_buffers.clear();
    }

    ast::ast_type* lang::analyze_template_derivation(
        wo_pstring_t temp_form,
        const std::vector<wo_pstring_t>& termplate_set,
        ast::ast_type* para, ast::ast_type* args)
    {
        // Must match all formal
        if (!para->is_like(args, termplate_set, &para, &args))
        {
            if (!para->is_function()
                && !para->has_template()
                && para->scope_namespaces.empty()
                && !para->search_from_global_namespace
                && para->type_name == temp_form)
            {
                // do nothing..
            }
            else
                return nullptr;
        }
        if (para->is_mutable() && !args->is_mutable())
            return nullptr;

        if (para->is_function() && args->is_function())
        {
            if (auto* derivation_result = analyze_template_derivation(
                temp_form,
                termplate_set,
                para->function_ret_type,
                args->function_ret_type))
                return derivation_result;
        }

        if (para->is_struct() && para->using_type_name == nullptr
            && args->is_struct() && args->using_type_name == nullptr)
        {
            if (para->struct_member_index.size() == args->struct_member_index.size())
            {
                for (auto& [memname, memberinfo] : para->struct_member_index)
                {
                    auto fnd = args->struct_member_index.find(memname);
                    if (fnd == args->struct_member_index.end())
                        continue;

                    if (memberinfo.offset != fnd->second.offset)
                        continue;

                    if (auto* derivation_result = analyze_template_derivation(
                        temp_form,
                        termplate_set,
                        memberinfo.member_type,
                        fnd->second.member_type))
                        return derivation_result;
                }
            }
        }

        if (para->type_name == temp_form
            && para->scope_namespaces.empty()
            && !para->search_from_global_namespace)
        {
            ast::ast_type* picked_type = args;

            wo_assert(picked_type);
            if (!para->template_arguments.empty())
            {
                ast::ast_type* type_hkt = new ast::ast_type(WO_PSTR(pending));
                if (picked_type->using_type_name)
                    type_hkt->set_type(picked_type->using_type_name);
                else
                    type_hkt->set_type(picked_type);

                type_hkt->template_arguments.clear();
                return type_hkt;
            }

            if ((picked_type->is_mutable() && para->is_mutable()))
            {
                // mut T <<== mut InstanceT
                // Should get (immutable) InstanceT.

                ast::ast_type* immutable_type = new ast::ast_type(WO_PSTR(pending));
                immutable_type->set_type(picked_type);

                if (picked_type->is_mutable() && para->is_mutable())
                    immutable_type->set_is_mutable(false);

                return immutable_type;
            }

            return picked_type;
        }

        for (size_t index = 0;
            index < para->template_arguments.size()
            && index < args->template_arguments.size();
            index++)
        {
            if (auto* derivation_result = analyze_template_derivation(temp_form,
                termplate_set,
                para->template_arguments[index],
                args->template_arguments[index]))
                return derivation_result;
        }

        if (para->using_type_name && args->using_type_name)
        {
            if (auto* derivation_result =
                analyze_template_derivation(temp_form, termplate_set, para->using_type_name, args->using_type_name))
                return derivation_result;
        }

        for (size_t index = 0;
            index < para->argument_types.size()
            && index < args->argument_types.size();
            index++)
        {
            if (auto* derivation_result = analyze_template_derivation(temp_form,
                termplate_set,
                para->argument_types[index],
                args->argument_types[index]))
                return derivation_result;
        }

        return nullptr;
    }

    opnum::opnumbase& lang::get_useable_register_for_pure_value(bool must_release)
    {
        using namespace ast;
        using namespace opnum;
#define WO_NEW_OPNUM(...) (*generated_opnum_list_for_clean.emplace_back(new __VA_ARGS__))
        for (size_t i = 0; i < opnum::reg::T_REGISTER_COUNT + opnum::reg::R_REGISTER_COUNT; i++)
        {
            if (RegisterUsingState::FREE == assigned_tr_register_list[i])
            {
                assigned_tr_register_list[i] = must_release ? RegisterUsingState::BLOCKING : RegisterUsingState::NORMAL;
                return WO_NEW_OPNUM(reg(reg::t0 + (uint8_t)i));
            }
        }
        wo_error("cannot get a useable register..");
        return WO_NEW_OPNUM(reg(reg::cr));
    }
    void lang::_complete_using_register_for_pure_value(opnum::opnumbase& completed_reg)
    {
        using namespace ast;
        using namespace opnum;
        if (auto* reg_ptr = dynamic_cast<opnum::reg*>(&completed_reg);
            reg_ptr && reg_ptr->id >= 0 && reg_ptr->id < opnum::reg::T_REGISTER_COUNT + opnum::reg::R_REGISTER_COUNT)
        {
            assigned_tr_register_list[reg_ptr->id] = RegisterUsingState::FREE;
        }
    }
    void lang::_complete_using_all_register_for_pure_value()
    {
        for (size_t i = 0; i < opnum::reg::T_REGISTER_COUNT + opnum::reg::R_REGISTER_COUNT; i++)
        {
            if (assigned_tr_register_list[i] != RegisterUsingState::BLOCKING)
                assigned_tr_register_list[i] = RegisterUsingState::FREE;
        }
    }
    opnum::opnumbase& lang::complete_using_register(opnum::opnumbase& completed_reg)
    {
        _complete_using_register_for_pure_value(completed_reg);

        return completed_reg;
    }
    void lang::complete_using_all_register()
    {
        _complete_using_all_register_for_pure_value();
    }
    bool lang::is_reg(opnum::opnumbase& op_num)
    {
        using namespace opnum;
        if (auto* regist = dynamic_cast<reg*>(&op_num))
        {
            return true;
        }
        return false;
    }
    bool lang::is_cr_reg(opnum::opnumbase& op_num)
    {
        using namespace opnum;
        if (auto* regist = dynamic_cast<reg*>(&op_num))
        {
            if (regist->id == reg::cr)
                return true;
        }
        return false;
    }
    bool lang::is_non_ref_tem_reg(opnum::opnumbase& op_num)
    {
        using namespace opnum;
        if (auto* regist = dynamic_cast<reg*>(&op_num))
        {
            if (regist->id >= reg::t0 && regist->id <= reg::t15)
                return true;
        }
        return false;
    }
    bool lang::is_temp_reg(opnum::opnumbase& op_num)
    {
        using namespace opnum;
        if (auto* regist = dynamic_cast<reg*>(&op_num))
        {
            if (regist->id >= reg::t0 && regist->id <= reg::r15)
                return true;
        }
        return false;
    }
    opnum::opnumbase& lang::mov_value_to_cr(opnum::opnumbase& op_num, ir_compiler* compiler)
    {
        using namespace ast;
        using namespace opnum;
        if (_last_value_stored_to_cr)
            return op_num;

        if (auto* regist = dynamic_cast<reg*>(&op_num))
        {
            if (regist->id == reg::cr)
                return op_num;
        }
        compiler->mov(reg(reg::cr), op_num);
        return WO_NEW_OPNUM(reg(reg::cr));
    }

    opnum::opnumbase& lang::get_new_global_variable()
    {
        using namespace opnum;
        return WO_NEW_OPNUM(global((int32_t)global_symbol_index++));
    }
    std::variant<opnum::opnumbase*, int16_t> lang::get_opnum_by_symbol(
        ast::ast_base* error_prud,
        lang_symbol* symb,
        ir_compiler* compiler,
        bool get_pure_value)
    {
        using namespace opnum;

        wo_assert(symb != nullptr);

        if (symb->is_constexpr_or_immut_no_closure_func())
            return &analyze_value(symb->variable_value, compiler, get_pure_value);

        if (symb->type == lang_symbol::symbol_type::variable)
        {
            if (symb->static_symbol)
            {
                if (!get_pure_value)
                    return &WO_NEW_OPNUM(global((int32_t)symb->global_index_in_lang));
                else
                {
                    auto& loaded_pure_glb_opnum = get_useable_register_for_pure_value();
                    compiler->mov(loaded_pure_glb_opnum, global((int32_t)symb->global_index_in_lang));
                    return &loaded_pure_glb_opnum;
                }
            }
            else
            {
                wo_integer_t stackoffset = 0;

                if (symb->is_captured_variable)
                    stackoffset = -2 - symb->captured_index;
                else
                    stackoffset = symb->stackvalue_index_in_funcs;
                if (!get_pure_value)
                {
                    if (stackoffset <= 64 && stackoffset >= -63)
                        return &WO_NEW_OPNUM(reg(reg::bp_offset(-(int8_t)stackoffset)));
                    else
                    {
                        // Fuck GCC!
                        return (int16_t)-stackoffset;
                    }
                }
                else
                {
                    if (stackoffset <= 64 && stackoffset >= -63)
                    {
                        auto& loaded_pure_glb_opnum = get_useable_register_for_pure_value();
                        compiler->mov(loaded_pure_glb_opnum, reg(reg::bp_offset(-(int8_t)stackoffset)));
                        return &loaded_pure_glb_opnum;
                    }
                    else
                    {
                        auto& lds_aim = get_useable_register_for_pure_value();
                        compiler->lds(lds_aim, imm(-(int16_t)stackoffset));
                        return &lds_aim;
                    }
                }
            }
        }
        else
            return &analyze_value(symb->get_funcdef(), compiler, get_pure_value);
    }

    opnum::opnumbase& lang::analyze_value(ast::ast_value* value, ir_compiler* compiler, bool get_pure_value)
    {
        auto_cancel_value_store_to_cr last_value_stored_to_cr_flag(_last_value_stored_to_cr);
        auto_cancel_value_store_to_cr last_value_stored_to_stack_flag(_last_value_from_stack);
        using namespace ast;
        using namespace opnum;
        if (value->is_constant)
        {
            if (ast_value_trib_expr* a_value_trib_expr = dynamic_cast<ast_value_trib_expr*>(value))
                // Only generate expr if const-expr is a function call
                analyze_value(a_value_trib_expr->judge_expr, compiler, false);

            const auto& const_value = value->get_constant_value();
            switch (const_value.type)
            {
            case value::valuetype::bool_type:
                if (!get_pure_value)
                    return WO_NEW_OPNUM(imm((bool)(const_value.integer != 0)));
                else
                {
                    auto& treg = get_useable_register_for_pure_value();
                    compiler->mov(treg, imm((bool)(const_value.integer != 0)));
                    return treg;
                }
            case value::valuetype::integer_type:
                if (!get_pure_value)
                    return WO_NEW_OPNUM(imm(const_value.integer));
                else
                {
                    auto& treg = get_useable_register_for_pure_value();
                    compiler->mov(treg, imm(const_value.integer));
                    return treg;
                }
            case value::valuetype::real_type:
                if (!get_pure_value)
                    return WO_NEW_OPNUM(imm(const_value.real));
                else
                {
                    auto& treg = get_useable_register_for_pure_value();
                    compiler->mov(treg, imm(const_value.real));
                    return treg;
                }
            case value::valuetype::handle_type:
                if (!get_pure_value)
                    return WO_NEW_OPNUM(imm_hdl(const_value.handle));
                else
                {
                    auto& treg = get_useable_register_for_pure_value();
                    compiler->mov(treg, imm_hdl(const_value.handle));
                    return treg;
                }
            case value::valuetype::string_type:
                if (!get_pure_value)
                    return WO_NEW_OPNUM(imm_str(std::string(*const_value.string)));
                else
                {
                    auto& treg = get_useable_register_for_pure_value();
                    compiler->mov(treg, imm_str(std::string(*const_value.string)));
                    return treg;
                }
            case value::valuetype::invalid:  // for nil
            case value::valuetype::array_type:  // for nil
            case value::valuetype::dict_type:  // for nil
            case value::valuetype::gchandle_type:  // for nil
                if (!get_pure_value)
                    return WO_NEW_OPNUM(reg(reg::ni));
                else
                {
                    auto& treg = get_useable_register_for_pure_value();
                    compiler->mov(treg, reg(reg::ni));
                    return treg;
                }
            default:
                wo_error("error constant type..");
                break;
            }
        }
        else
        {
            compiler->pdb_info->generate_debug_info_at_astnode(value, compiler);
            return this->m_global_pass_table->finalize_value(this, value, compiler, get_pure_value);
        }
#undef WO_NEW_OPNUM
    }
    opnum::opnumbase& lang::auto_analyze_value(ast::ast_value* value, ir_compiler* compiler, bool get_pure_value)
    {
        auto& result = analyze_value(value, compiler, get_pure_value);
        complete_using_all_register();

        return result;
    }

    void lang::real_analyze_finalize(ast::ast_base* ast_node, ir_compiler* compiler)
    {
        wo_assert(ast_node->completed_in_pass2);

        compiler->pdb_info->generate_debug_info_at_astnode(ast_node, compiler);

        using namespace ast;
        using namespace opnum;

        if (auto* a_value = dynamic_cast<ast_value*>(ast_node))
        {
            if (auto* a_val_funcdef = dynamic_cast<ast_value_function_define*>(a_value);
                a_val_funcdef == nullptr || a_val_funcdef->function_name == nullptr)
            {
                // Woolang 1.10.2: The value is not void type, cannot be a sentence.
                if (!a_value->value_type->is_void() &&
                    !a_value->value_type->is_nothing())
                    lang_anylizer->lang_error(lexer::errorlevel::error, a_value, WO_ERR_NOT_ALLOW_IGNORE_VALUE,
                        a_value->value_type->get_type_name(false).c_str());
            }
            auto_analyze_value(a_value, compiler);
        }
        else
        {
            // 
            wo_assure(this->m_global_pass_table->finalize(this, ast_node, compiler));
        }
    }
    void lang::analyze_finalize(ast::ast_base* ast_node, ir_compiler* compiler)
    {
        // first, check each extern func
        for (auto& [_, symb_maps] : extern_symb_infos)
        {
            for (auto& [_, symb] : symb_maps)
            {
                wo_assert(symb != nullptr && symb->externed_func != nullptr);

                compiler->record_extern_native_function(
                    (intptr_t)symb->externed_func,
                    *symb->source_file,
                    symb->library_name,
                    symb->symbol_name);
            }
        }

        size_t public_block_begin = compiler->get_now_ip();
        auto res_ip = compiler->reserved_stackvalue(); // reserved..
        real_analyze_finalize(ast_node, compiler);
        auto used_tmp_regs = compiler->update_all_temp_regist_to_stack(public_block_begin);
        compiler->reserved_stackvalue(res_ip, used_tmp_regs); // set reserved size

        compiler->mov(opnum::reg(opnum::reg::spreg::cr), opnum::imm(0));
        compiler->jmp(opnum::tag("__rsir_rtcode_seg_function_define_end"));

        while (!in_used_functions.empty())
        {
            const auto tmp_build_func_list = in_used_functions;
            in_used_functions.clear();
            for (auto* funcdef : tmp_build_func_list)
            {
                // If current is template, the node will not be compile, just skip it.
                if (funcdef->is_template_define)
                    continue;

                wo_assert(funcdef->completed_in_pass2);

                size_t funcbegin_ip = compiler->get_now_ip();
                now_function_in_final_anylize = funcdef;

                compiler->ext_funcbegin();

                compiler->tag(funcdef->get_ir_func_signature_tag());
                if (funcdef->declear_attribute->is_extern_attr())
                {
                    // this function is externed, put it into extern-table and update the value in ir-compiler
                    auto&& spacename = funcdef->get_full_namespace_chain_after_pass1();
                    auto&& fname = (spacename.empty() ? "" : spacename + "::") + wstr_to_str(*funcdef->function_name);

                    compiler->record_extern_script_function(fname);
                }
                compiler->pdb_info->generate_func_begin(funcdef, compiler);

                auto res_ip = compiler->reserved_stackvalue();                      // reserved..

                // apply args.
                int arg_count = 0;
                auto arg_index = funcdef->argument_list->children;
                while (arg_index)
                {
                    if (auto* a_value_arg_define = dynamic_cast<ast::ast_value_arg_define*>(arg_index))
                    {
                        // Issue N221109: Reference will not support.
                        // All arguments will 'psh' to stack & no 'pshr' command in future.
                        if (a_value_arg_define->symbol != nullptr)
                        {
                            if (a_value_arg_define->symbol->is_marked_as_used_variable == false
                                && a_value_arg_define->symbol->define_in_function == true)
                            {
                                lang_anylizer->lang_error(lexer::errorlevel::error, a_value_arg_define, WO_ERR_UNUSED_VARIABLE_DEFINE,
                                    a_value_arg_define->arg_name->c_str(),
                                    str_to_wstr(funcdef->get_ir_func_signature_tag()).c_str());
                            }

                            funcdef->this_func_scope->
                                reduce_function_used_stack_size_at(a_value_arg_define->symbol->stackvalue_index_in_funcs);

                            wo_assert(0 == a_value_arg_define->symbol->stackvalue_index_in_funcs);
                            a_value_arg_define->symbol->stackvalue_index_in_funcs =
                                -2 - arg_count - (wo_integer_t)funcdef->capture_variables.size();
                        }

                        // NOTE: If argument's name is `_`, still need continue;
                    }
                    else // variadic
                        break;
                    arg_count++;
                    arg_index = arg_index->sibling;
                }

                for (auto& symb : funcdef->this_func_scope->in_function_symbols)
                {
                    if (symb->is_constexpr_or_immut_no_closure_func())
                        symb->is_constexpr = true;
                }

                funcdef->this_func_scope->in_function_symbols.erase(
                    std::remove_if(
                        funcdef->this_func_scope->in_function_symbols.begin(),
                        funcdef->this_func_scope->in_function_symbols.end(),
                        [](auto* symb) {return symb->is_constexpr; }),
                    funcdef->this_func_scope->in_function_symbols.end());

                real_analyze_finalize(funcdef->in_function_sentence, compiler);

                auto temp_reg_to_stack_count = compiler->update_all_temp_regist_to_stack(funcbegin_ip);
                auto reserved_stack_size =
                    funcdef->this_func_scope->max_used_stack_size_in_func
                    + temp_reg_to_stack_count;

                compiler->reserved_stackvalue(res_ip, (uint16_t)reserved_stack_size); // set reserved size
                compiler->tag(funcdef->get_ir_func_signature_tag() + "_do_ret");

                wo_assert(funcdef->value_type->is_function());
                if (!funcdef->value_type->function_ret_type->is_void())
                    compiler->ext_panic(opnum::imm_str("Function returned without valid value."));

                // do default return
                if (funcdef->is_closure_function())
                    compiler->ret((uint16_t)funcdef->capture_variables.size());
                else
                    compiler->ret();

                compiler->pdb_info->generate_func_end(funcdef, temp_reg_to_stack_count, compiler);
                compiler->ext_funcend();

                for (auto funcvar : funcdef->this_func_scope->in_function_symbols)
                    compiler->pdb_info->add_func_variable(funcdef, *funcvar->name, funcvar->variable_value->row_end_no, funcvar->stackvalue_index_in_funcs);
            }
        }
        compiler->tag("__rsir_rtcode_seg_function_define_end");
        compiler->loaded_libs = extern_libs;
        compiler->pdb_info->finalize_generate_debug_info();

        wo::ast::ast_base::exchange_this_thread_ast(generated_ast_nodes_buffers);
    }
    lang_scope* lang::begin_namespace(ast::ast_namespace* a_namespace)
    {
        wo_assert(a_namespace->source_file != nullptr);
        if (lang_scopes.size())
        {
            auto fnd = lang_scopes.back()->sub_namespaces.find(a_namespace->scope_name);
            if (fnd != lang_scopes.back()->sub_namespaces.end())
            {
                lang_scopes.push_back(fnd->second);
                current_namespace = lang_scopes.back();
                current_namespace->last_entry_ast = a_namespace;
                return current_namespace;
            }
        }

        lang_scope* scope = new lang_scope;
        lang_scopes_buffers.push_back(scope);

        scope->stop_searching_in_last_scope_flag = false;
        scope->type = lang_scope::scope_type::namespace_scope;
        scope->belong_namespace = current_namespace;
        scope->parent_scope = lang_scopes.empty() ? nullptr : lang_scopes.back();
        scope->scope_namespace = a_namespace->scope_name;

        if (lang_scopes.size())
            lang_scopes.back()->sub_namespaces[a_namespace->scope_name] = scope;

        lang_scopes.push_back(scope);
        current_namespace = lang_scopes.back();
        current_namespace->last_entry_ast = a_namespace;
        return current_namespace;
    }
    void lang::end_namespace()
    {
        wo_assert(lang_scopes.back()->type == lang_scope::scope_type::namespace_scope);

        current_namespace = lang_scopes.back()->belong_namespace;
        lang_scopes.pop_back();
    }
    lang_scope* lang::begin_scope(ast::ast_base* block_beginer)
    {
        lang_scope* scope = new lang_scope;
        lang_scopes_buffers.push_back(scope);

        scope->last_entry_ast = block_beginer;
        wo_assert(block_beginer->source_file != nullptr);
        scope->stop_searching_in_last_scope_flag = false;
        scope->type = lang_scope::scope_type::just_scope;
        scope->belong_namespace = current_namespace;
        scope->parent_scope = lang_scopes.empty() ? nullptr : lang_scopes.back();

        lang_scopes.push_back(scope);
        return scope;
    }
    void lang::end_scope()
    {
        wo_assert(lang_scopes.back()->type == lang_scope::scope_type::just_scope);

        auto scope = now_scope();
        if (auto* func = in_function())
        {
            func->used_stackvalue_index -= scope->this_block_used_stackvalue_count;
        }
        lang_scopes.pop_back();
    }
    lang_scope* lang::begin_function(ast::ast_value_function_define* ast_value_funcdef)
    {
        bool already_created_func_scope = ast_value_funcdef->this_func_scope != nullptr;
        lang_scope* scope =
            already_created_func_scope ? ast_value_funcdef->this_func_scope : new lang_scope;
        scope->last_entry_ast = ast_value_funcdef;
        wo_assert(ast_value_funcdef->source_file != nullptr);
        if (!already_created_func_scope)
        {
            lang_scopes_buffers.push_back(scope);

            scope->stop_searching_in_last_scope_flag = false;
            scope->type = lang_scope::scope_type::function_scope;
            scope->belong_namespace = current_namespace;
            scope->parent_scope = lang_scopes.empty() ? nullptr : lang_scopes.back();
            scope->function_node = ast_value_funcdef;

            if (ast_value_funcdef->function_name != nullptr && !ast_value_funcdef->is_template_reification)
            {
                // Not anymous function or template_reification , define func-symbol..
                auto* sym = define_variable_in_this_scope(
                    ast_value_funcdef,
                    ast_value_funcdef->function_name,
                    ast_value_funcdef,
                    ast_value_funcdef->declear_attribute,
                    template_style::NORMAL,
                    ast::identifier_decl::IMMUTABLE);
                ast_value_funcdef->symbol = sym;
            }
        }
        lang_scopes.push_back(scope);
        return scope;
    }
    void lang::end_function()
    {
        wo_assert(lang_scopes.back()->type == lang_scope::scope_type::function_scope);
        lang_scopes.pop_back();
    }
    lang_scope* lang::now_scope() const
    {
        wo_assert(!lang_scopes.empty());
        return lang_scopes.back();
    }
    lang_scope* lang::now_namespace() const
    {
        auto* current_scope = now_scope();
        if (current_scope->type != lang_scope::scope_type::namespace_scope)
            current_scope = current_scope->belong_namespace;

        wo_assert(current_scope->type == lang_scope::scope_type::namespace_scope);

        return current_scope;
    }
    lang_scope* lang::in_function() const
    {
        for (auto rindex = lang_scopes.rbegin(); rindex != lang_scopes.rend(); rindex++)
        {
            if ((*rindex)->type == lang_scope::scope_type::function_scope)
                return *rindex;
        }
        return nullptr;
    }
    lang_scope* lang::in_function_pass2() const
    {
        return current_function_in_pass2;
    }

    lang_symbol* lang::define_variable_in_this_scope(
        ast::ast_base* errreporter,
        wo_pstring_t names,
        ast::ast_value* init_val,
        ast::ast_decl_attribute* attr,
        template_style is_template_value,
        ast::identifier_decl mutable_type,
        size_t captureindex)
    {
        wo_assert(lang_scopes.size());

        if (is_template_value != template_style::IS_TEMPLATE_VARIABLE_IMPL && (lang_scopes.back()->symbols.find(names) != lang_scopes.back()->symbols.end()))
        {
            auto* last_found_symbol = lang_scopes.back()->symbols[names];

            lang_anylizer->lang_error(lexer::errorlevel::error, errreporter, WO_ERR_REDEFINED, names->c_str());
            lang_anylizer->lang_error(lexer::errorlevel::infom,
                (last_found_symbol->type == lang_symbol::symbol_type::typing || last_found_symbol->type == lang_symbol::symbol_type::type_alias)
                ? (ast::ast_base*)last_found_symbol->type_informatiom
                : (ast::ast_base*)last_found_symbol->variable_value
                , WO_INFO_ITEM_IS_DEFINED_HERE, names->c_str());

            return last_found_symbol;
        }

        if (auto* func_def = dynamic_cast<ast::ast_value_function_define*>(init_val);
            func_def != nullptr && func_def->function_name != nullptr)
        {
            wo_assert(template_style::NORMAL == is_template_value);

            lang_symbol* sym;

            sym = lang_scopes.back()->symbols[names] = new lang_symbol;
            sym->type = lang_symbol::symbol_type::function;
            sym->attribute = attr;
            sym->name = names;
            sym->defined_in_scope = lang_scopes.back();
            sym->variable_value = func_def;
            sym->is_template_symbol = func_def->is_template_define;
            sym->decl = mutable_type;

            lang_symbols.push_back(sym);

            return sym;
        }
        else
        {
            lang_symbol* sym = new lang_symbol;
            if (is_template_value != template_style::IS_TEMPLATE_VARIABLE_IMPL)
                lang_scopes.back()->symbols[names] = sym;

            sym->type = lang_symbol::symbol_type::variable;
            sym->attribute = attr;
            sym->name = names;
            sym->variable_value = init_val;
            sym->defined_in_scope = lang_scopes.back();
            sym->decl = mutable_type;

            auto* func = in_function();
            if (attr->is_extern_attr() && func)
                lang_anylizer->lang_error(lexer::errorlevel::error, attr, WO_ERR_CANNOT_EXPORT_SYMB_IN_FUNC);
            if (func && !sym->attribute->is_static_attr())
            {
                sym->define_in_function = true;
                sym->static_symbol = false;

                if (captureindex == (size_t)-1)
                {
                    if (sym->decl == ast::identifier_decl::MUTABLE || !init_val->is_constant)
                    {
                        if (is_template_value != template_style::IS_TEMPLATE_VARIABLE_DEFINE)
                        {
                            sym->stackvalue_index_in_funcs = func->assgin_stack_index(sym);
                            lang_scopes.back()->this_block_used_stackvalue_count++;
                        }
                    }
                    else
                        sym->is_constexpr = true;
                }
                else
                {
                    // Is capture symbol;
                    sym->is_captured_variable = true;
                    sym->captured_index = captureindex;
                }
            }
            else
            {
                wo_assert(captureindex == (size_t)-1);

                if (func)
                    sym->define_in_function = true;
                else
                    sym->define_in_function = false;

                sym->static_symbol = true;

                if (sym->decl == ast::identifier_decl::MUTABLE || !init_val->is_constant)
                {
                    if (is_template_value != template_style::IS_TEMPLATE_VARIABLE_DEFINE)
                        sym->global_index_in_lang = global_symbol_index++;
                }
                else
                    sym->is_constexpr = true;
            }
            lang_symbols.push_back(sym);
            return sym;
        }
    }
    lang_symbol* lang::define_type_in_this_scope(ast::ast_using_type_as* def, ast::ast_type* as_type, ast::ast_decl_attribute* attr)
    {
        wo_assert(lang_scopes.size());

        if (lang_scopes.back()->symbols.find(def->new_type_identifier) != lang_scopes.back()->symbols.end())
        {
            auto* last_found_symbol = lang_scopes.back()->symbols[def->new_type_identifier];

            lang_anylizer->lang_error(lexer::errorlevel::error, as_type, WO_ERR_REDEFINED, def->new_type_identifier->c_str());
            lang_anylizer->lang_error(lexer::errorlevel::infom,
                (last_found_symbol->type == lang_symbol::symbol_type::typing || last_found_symbol->type == lang_symbol::symbol_type::type_alias)
                ? (ast::ast_base*)last_found_symbol->type_informatiom
                : (ast::ast_base*)last_found_symbol->variable_value
                , WO_INFO_ITEM_IS_DEFINED_HERE, def->new_type_identifier->c_str());
            return last_found_symbol;
        }
        else
        {
            lang_symbol* sym = lang_scopes.back()->symbols[def->new_type_identifier] = new lang_symbol;
            sym->attribute = attr;
            if (def->is_alias)
                sym->type = lang_symbol::symbol_type::type_alias;
            else
                sym->type = lang_symbol::symbol_type::typing;
            sym->name = def->new_type_identifier;

            sym->type_informatiom = as_type;
            sym->defined_in_scope = lang_scopes.back();
            sym->define_node = def;

            lang_symbols.push_back(sym);
            return sym;
        }
    }

    bool lang::check_symbol_is_accessable(lang_symbol* symbol, lang_scope* current_scope, ast::ast_base* ast, bool give_error)
    {
        if (symbol->attribute)
        {
            if (symbol->attribute->is_protected_attr())
            {
                auto* symbol_defined_space = symbol->defined_in_scope;
                if (current_scope->belongs_to(symbol_defined_space) == false)
                {
                    if (give_error)
                        lang_anylizer->lang_error(lexer::errorlevel::error, ast, WO_ERR_CANNOT_REACH_PROTECTED_IN_OTHER_FUNC, symbol->name->c_str());
                    return false;
                }
                return true;
            }
            if (symbol->attribute->is_private_attr())
            {
                if (ast->source_file == symbol->defined_source())
                    return true;
                if (give_error)
                    lang_anylizer->lang_error(lexer::errorlevel::error, ast, WO_ERR_CANNOT_REACH_PRIVATE_IN_OTHER_FUNC, symbol->name->c_str(),
                        symbol->defined_source()->c_str());
                return false;
            }
        }
        return true;
    }

    template<typename T>
    double levenshtein_distance(const T& s, const T& t)
    {
        size_t n = s.size();
        size_t m = t.size();
        std::vector<size_t> d((n + 1) * (m + 1));

        auto df = [&d, n](size_t x, size_t y)->size_t& {return d[y * (n + 1) + x]; };

        if (n == m && n == 0)
            return 0.;
        if (n == 0)
            return (double)m / (double)std::max(n, m);
        if (m == 0)
            return (double)n / (double)std::max(n, m);

        for (size_t i = 0; i <= n; ++i)
            df(i, 0) = i;
        for (size_t j = 0; j <= m; ++j)
            df(0, j) = j;

        for (size_t i = 1; i <= n; i++)
        {
            for (size_t j = 1; j <= m; j++)
            {
                size_t cost = (t[j - 1] == s[i - 1]) ? 0 : 1;
                df(i, j) = std::min(
                    std::min(df(i - 1, j) + 1, df(i, j - 1) + 1),
                    df(i - 1, j - 1) + cost);
            }
        }

        return (double)df(n, m) / (double)std::max(n, m);
    }

    lang_symbol* lang::find_symbol_in_this_scope(ast::ast_symbolable_base* var_ident, wo_pstring_t ident_str, int target_type_mask, bool fuzzy_for_err_report)
    {
        wo_assert(lang_scopes.size());

        if (var_ident->symbol && !fuzzy_for_err_report)
            return var_ident->symbol;

        lang_symbol* fuzzy_nearest_symbol = nullptr;
        auto update_fuzzy_nearest_symbol =
            [&fuzzy_nearest_symbol, fuzzy_for_err_report, ident_str, target_type_mask]
        (lang_symbol* symbol) {
            if (fuzzy_for_err_report)
            {
                auto distance = levenshtein_distance(*symbol->name, *ident_str);
                if (distance <= 0.3 && (symbol->type & target_type_mask) != 0)
                {
                    if (fuzzy_nearest_symbol == nullptr
                        || distance < levenshtein_distance(*fuzzy_nearest_symbol->name, *ident_str))
                        fuzzy_nearest_symbol = symbol;
                }
            }
        };

        if (!var_ident->search_from_global_namespace && var_ident->scope_namespaces.empty())
            for (auto rind = template_stack.rbegin(); rind != template_stack.rend(); rind++)
            {
                if (fuzzy_for_err_report)
                {
                    for (auto& finding_symbol : *rind)
                        update_fuzzy_nearest_symbol(finding_symbol.second);
                }
                else if (auto fnd = rind->find(ident_str); fnd != rind->end())
                {
                    if ((fnd->second->type & target_type_mask) != 0)
                        return fnd->second;
                }
            }

        auto* searching_from_scope = var_ident->searching_begin_namespace_in_pass2;

        if (var_ident->searching_from_type)
        {
            fully_update_type(var_ident->searching_from_type, has_step_in_step2);

            if (!var_ident->searching_from_type->is_pending() || var_ident->searching_from_type->using_type_name)
            {
                auto* finding_from_type = var_ident->searching_from_type;
                if (var_ident->searching_from_type->using_type_name)
                    finding_from_type = var_ident->searching_from_type->using_type_name;

                if (finding_from_type->symbol)
                {
                    var_ident->searching_begin_namespace_in_pass2 =
                        finding_from_type->symbol->defined_in_scope;
                    wo_assert(var_ident->source_file != nullptr);
                }
                else
                    var_ident->search_from_global_namespace = true;

                var_ident->scope_namespaces.insert(var_ident->scope_namespaces.begin(),
                    finding_from_type->type_name);

                var_ident->searching_from_type = nullptr;
            }
            else
            {
                return nullptr;
            }
        }
        else if (!var_ident->scope_namespaces.empty())
        {
            if (!var_ident->search_from_global_namespace)
                for (auto rind = template_stack.rbegin(); rind != template_stack.rend(); rind++)
                {
                    if (auto fnd = rind->find(var_ident->scope_namespaces.front()); fnd != rind->end())
                    {
                        auto* fnd_template_type = fnd->second->type_informatiom;
                        if (fnd_template_type->using_type_name)
                            fnd_template_type = fnd_template_type->using_type_name;

                        if (fnd_template_type->symbol)
                        {
                            var_ident->searching_begin_namespace_in_pass2 = fnd_template_type->symbol->defined_in_scope;
                            wo_assert(var_ident->source_file != nullptr);
                        }
                        else
                            var_ident->search_from_global_namespace = true;

                        var_ident->scope_namespaces[0] = fnd_template_type->type_name;
                        break;
                    }
                }

        }

        auto* searching = var_ident->search_from_global_namespace ?
            lang_scopes.front()
            :
            (
                var_ident->searching_begin_namespace_in_pass2 ?
                var_ident->searching_begin_namespace_in_pass2
                :
                lang_scopes.back()
                );

        std::vector<lang_scope*> searching_namespace;
        searching_namespace.push_back(searching);
        // ATTENTION: IF SYMBOL WITH SAME NAME, IT MAY BE DUP HERE BECAUSE BY USING-NAMESPACE,
        //            WE NEED CHOOSE NEAREST SYMBOL
        //            So we should search in scope chain, if found, return it immediately.
        auto* _first_searching = searching;
        while (_first_searching)
        {
            size_t namespace_index = 0;

            auto* indet_finding_namespace = _first_searching;
            while (namespace_index < var_ident->scope_namespaces.size())
            {
                if (auto fnd = indet_finding_namespace->sub_namespaces.find(var_ident->scope_namespaces[namespace_index]);
                    fnd != indet_finding_namespace->sub_namespaces.end())
                {
                    namespace_index++;
                    indet_finding_namespace = fnd->second;
                }
                else
                    goto TRY_UPPER_SCOPE;
            }

            if (fuzzy_for_err_report)
            {
                for (auto& finding_symbol : indet_finding_namespace->symbols)
                    update_fuzzy_nearest_symbol(finding_symbol.second);
            }
            else if (auto fnd = indet_finding_namespace->symbols.find(ident_str);
                fnd != indet_finding_namespace->symbols.end())
            {
                if ((fnd->second->type & target_type_mask) != 0
                    && (fnd->second->type == lang_symbol::symbol_type::typing
                        || fnd->second->type == lang_symbol::symbol_type::type_alias
                        || check_symbol_is_accessable(fnd->second, searching_from_scope, var_ident, false)))
                    return var_ident->symbol = fnd->second;
            }

        TRY_UPPER_SCOPE:
            _first_searching = _first_searching->parent_scope;
        }

        // Not found in current scope, trying to find it in using-namespace/root namespace
        auto* _searching_in_all = searching;
        while (_searching_in_all)
        {
            for (auto* a_using_namespace : _searching_in_all->used_namespace)
            {
                wo_assert(a_using_namespace->source_file != nullptr);
                if (a_using_namespace->source_file != var_ident->source_file)
                    continue;

                if (!a_using_namespace->from_global_namespace)
                {
                    auto* finding_namespace = _searching_in_all;
                    while (finding_namespace)
                    {
                        auto* _deep_in_namespace = finding_namespace;
                        for (auto& nspace : a_using_namespace->used_namespace_chain)
                        {
                            if (auto fnd = _deep_in_namespace->sub_namespaces.find(nspace);
                                fnd != _deep_in_namespace->sub_namespaces.end())
                                _deep_in_namespace = fnd->second;
                            else
                            {
                                // fail
                                goto failed_in_this_namespace;
                            }
                        }
                        // ok!
                        searching_namespace.push_back(_deep_in_namespace);
                    failed_in_this_namespace:;
                        finding_namespace = finding_namespace->belong_namespace;
                    }
                }
                else
                {
                    auto* _deep_in_namespace = lang_scopes.front();
                    for (auto& nspace : a_using_namespace->used_namespace_chain)
                    {
                        if (auto fnd = _deep_in_namespace->sub_namespaces.find(nspace);
                            fnd != _deep_in_namespace->sub_namespaces.end())
                            _deep_in_namespace = fnd->second;
                        else
                        {
                            // fail
                            goto failed_in_this_namespace_from_global;
                        }
                    }
                    // ok!
                    searching_namespace.push_back(_deep_in_namespace);
                failed_in_this_namespace_from_global:;
                }
            }
            _searching_in_all = _searching_in_all->parent_scope;
        }
        std::set<lang_symbol*> searching_result;
        std::set<lang_scope*> searched_scopes;

        for (auto _searching : searching_namespace)
        {
            bool deepin_search = _searching == searching;
            while (_searching)
            {
                // search_in 
                if (var_ident->scope_namespaces.size())
                {
                    size_t namespace_index = 0;
                    if (_searching->type != lang_scope::scope_type::namespace_scope)
                        _searching = _searching->belong_namespace;

                    auto* stored_scope_for_next_try = _searching;

                    while (namespace_index < var_ident->scope_namespaces.size())
                    {
                        if (auto fnd = _searching->sub_namespaces.find(var_ident->scope_namespaces[namespace_index]);
                            fnd != _searching->sub_namespaces.end() && searched_scopes.find(fnd->second) == searched_scopes.end())
                        {
                            namespace_index++;
                            _searching = fnd->second;
                        }
                        else
                        {
                            _searching = stored_scope_for_next_try;
                            goto there_is_no_such_namespace;
                        }
                    }
                }

                searched_scopes.insert(_searching);
                if (fuzzy_for_err_report)
                {
                    for (auto& finding_symbol : _searching->symbols)
                        update_fuzzy_nearest_symbol(finding_symbol.second);
                }
                else if (auto fnd = _searching->symbols.find(ident_str);
                    fnd != _searching->symbols.end())
                {
                    if ((fnd->second->type & target_type_mask) != 0)
                        searching_result.insert(fnd->second);
                    goto next_searching_point;
                }

            there_is_no_such_namespace:
                if (deepin_search)
                    _searching = _searching->parent_scope;
                else
                    _searching = nullptr;
            }

        next_searching_point:;
        }

        if (fuzzy_for_err_report)
            return fuzzy_nearest_symbol;

        if (searching_result.empty())
            return var_ident->symbol = nullptr;

        // Result might have un-accessable type? remove them
        std::set<lang_symbol*> selecting_results;
        selecting_results.swap(searching_result);
        for (auto fnd_result : selecting_results)
            if (fnd_result->type == lang_symbol::symbol_type::typing
                || fnd_result->type == lang_symbol::symbol_type::type_alias
                || check_symbol_is_accessable(fnd_result, searching_from_scope, var_ident, false))
                searching_result.insert(fnd_result);

        if (searching_result.empty())
            return var_ident->symbol = nullptr;
        else if (searching_result.size() > 1)
        {
            // Donot report error if in pass1 to avoid a->>f; when `using option;/ using result;`
            if (has_step_in_step2)
            {
                std::wstring err_info = WO_ERR_SYMBOL_IS_AMBIGUOUS;
                size_t fnd_count = 0;
                for (auto fnd_result : searching_result)
                {
                    auto _full_namespace_ = wo::str_to_wstr(get_belong_namespace_path_with_lang_scope(fnd_result->defined_in_scope));
                    if (_full_namespace_ == L"")
                        err_info += WO_TERM_GLOBAL_NAMESPACE;
                    else
                        err_info += L"'" + _full_namespace_ + L"'";
                    fnd_count++;
                    if (fnd_count + 1 == searching_result.size())
                        err_info += L" " WO_TERM_AND L" ";
                    else
                        err_info += L", ";
                }

                lang_anylizer->lang_error(lexer::errorlevel::error, var_ident, err_info.c_str(), ident_str->c_str());
            }
            return var_ident->symbol = nullptr;
        }
        else
        {
            return var_ident->symbol = *searching_result.begin();
        }
    }
    lang_symbol* lang::find_type_in_this_scope(ast::ast_type* var_ident)
    {
        auto* result = find_symbol_in_this_scope(var_ident, var_ident->type_name,
            lang_symbol::symbol_type::type_alias | lang_symbol::symbol_type::typing, false);

        return result;
    }

    // Only used for check symbol is exist?
    lang_symbol* lang::find_value_symbol_in_this_scope(ast::ast_value_variable* var_ident)
    {
        return find_symbol_in_this_scope(var_ident, var_ident->var_name,
            lang_symbol::symbol_type::variable | lang_symbol::symbol_type::function, false);
    }

    lang_symbol* lang::find_value_in_this_scope(ast::ast_value_variable* var_ident)
    {
        auto* result = find_value_symbol_in_this_scope(var_ident);

        if (result)
        {
            auto symb_defined_in_func = result->defined_in_scope;
            while (symb_defined_in_func->parent_scope &&
                symb_defined_in_func->type != wo::lang_scope::scope_type::function_scope)
                symb_defined_in_func = symb_defined_in_func->parent_scope;

            auto* current_function = in_function();

            if (current_function &&
                result->define_in_function
                && !result->static_symbol
                && symb_defined_in_func != current_function
                && symb_defined_in_func->function_node != current_function->function_node)
            {
                if (result->is_template_symbol)
                    lang_anylizer->lang_error(lexer::errorlevel::error, var_ident, WO_ERR_CANNOT_CAPTURE_TEMPLATE_VAR,
                        result->name->c_str());

                // The variable is not static and define outside the function. ready to capture it!
                if (current_function->function_node->function_name != nullptr)
                    // Only anonymous can capture variablel;
                    lang_anylizer->lang_error(lexer::errorlevel::error, var_ident, WO_ERR_CANNOT_CAPTURE_IN_NAMED_FUNC,
                        result->name->c_str());

                if (result->decl == identifier_decl::IMMUTABLE)
                {
                    result->variable_value->eval_constant_value(lang_anylizer);
                    if (result->variable_value->is_constant)
                        // Woolang 1.10.4: Constant variable not need to capture.
                        return result;
                }

                auto* current_func_defined_in_function = current_function->parent_scope;
                while (current_func_defined_in_function->parent_scope &&
                    current_func_defined_in_function->type != wo::lang_scope::scope_type::function_scope)
                    current_func_defined_in_function = current_func_defined_in_function->parent_scope;

                std::vector<lang_scope*> need_capture_func_list;

                while (current_func_defined_in_function
                    && current_func_defined_in_function != symb_defined_in_func)
                {
                    need_capture_func_list.insert(need_capture_func_list.begin(), current_func_defined_in_function);

                    // goto last function
                    current_func_defined_in_function = current_func_defined_in_function->parent_scope;
                    while (current_func_defined_in_function->parent_scope &&
                        current_func_defined_in_function->type != wo::lang_scope::scope_type::function_scope)
                        current_func_defined_in_function = current_func_defined_in_function->parent_scope;
                }
                need_capture_func_list.push_back(current_function);

                // Add symbol to capture list.
                for (auto* cur_capture_func_scope : need_capture_func_list)
                {
                    auto& capture_list = cur_capture_func_scope->function_node->capture_variables;
                    wo_assert(std::find(capture_list.begin(), capture_list.end(), result) == capture_list.end()
                        || cur_capture_func_scope == cur_capture_func_scope);

                    if (std::find(capture_list.begin(), capture_list.end(), result) == capture_list.end())
                    {
                        result->is_marked_as_used_variable = true;
                        capture_list.push_back(result);
                        // Define a closure symbol instead of current one.
                        temporary_entry_scope_in_pass1(cur_capture_func_scope);
                        result = define_variable_in_this_scope(
                            result->variable_value,
                            result->name,
                            result->variable_value,
                            result->attribute,
                            template_style::NORMAL,
                            ast::identifier_decl::IMMUTABLE,
                            capture_list.size() - 1);
                        temporary_leave_scope_in_pass1();
                    }
                }
                var_ident->symbol = result;
            }
        }
        return result;
    }
    bool lang::has_compile_error()const
    {
        return lang_anylizer->has_error();
    }
}
