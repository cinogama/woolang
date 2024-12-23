#include "wo_lang.hpp"

namespace wo
{
#ifndef WO_DISABLE_COMPILER
    LangContext::ProcessAstJobs* LangContext::m_pass0_processers;
    LangContext::ProcessAstJobs* LangContext::m_pass1_processers;

    void LangContext::init_lang_processers()
    {
        m_pass0_processers = new ProcessAstJobs();
        m_pass1_processers = new ProcessAstJobs();

        init_pass0();
        init_pass1();
    }
    void LangContext::shutdown_lang_processers()
    {
        delete m_pass0_processers;
        delete m_pass1_processers;
    }

    //////////////////////////////////////

    lang_Symbol::~lang_Symbol()
    {
        switch (m_symbol_kind)
        {
        case VARIABLE:
            if (m_is_template)
                delete m_template_value_instances;
            else
                delete m_value_instance;
            break;
        case TYPE:
            if (m_is_template)
                delete m_template_type_instances;
            else
                delete m_type_instance;
            break;
        case ALIAS:
            if (m_is_template)
                delete m_template_alias_instances;
            else
                delete m_alias_instance;
            break;
        }
    }
    lang_Symbol::lang_Symbol(
        const std::optional<ast::AstDeclareAttribue*>& attr,
        wo_pstring_t src_location, 
        lang_Scope* scope,
        kind kind)
        : m_symbol_kind(kind)
        , m_is_template(false)
        , m_defined_source(src_location)
        , m_declare_attribute(attr)
        , m_belongs_to_scope(scope)
    {
        switch (kind)
        {
        case VARIABLE:
            m_value_instance = new lang_ValueInstance(this);
            break;
        case TYPE:
            m_type_instance = new lang_TypeInstance(this);
            break;
        case ALIAS:
            m_alias_instance = new lang_AliasInstance(this);
            break;
        default:
            wo_error("Unexpected symbol kind.");
        }
    }
    lang_Symbol::lang_Symbol(
        const std::optional<ast::AstDeclareAttribue*>& attr,
        wo_pstring_t src_location, 
        lang_Scope* scope, 
        ast::AstValueBase* template_value_base, 
        const std::list<wo_pstring_t>& template_params)
        : m_symbol_kind(VARIABLE)
        , m_is_template(true)
        , m_defined_source(src_location)
        , m_declare_attribute(attr)
        , m_belongs_to_scope(scope)
    {
        m_template_value_instances = new TemplateValuePrefab(template_value_base, template_params);
    }
    lang_Symbol::lang_Symbol(
        const std::optional<ast::AstDeclareAttribue*>& attr,
        wo_pstring_t src_location, 
        lang_Scope* scope, 
        ast::AstTypeHolder* template_type_base,
        const std::list<wo_pstring_t>& template_params, 
        bool is_alias)
        : m_is_template(true)
        , m_defined_source(src_location)
        , m_declare_attribute(attr)
        , m_belongs_to_scope(scope)
    {
        if (is_alias)
        {
            m_symbol_kind = ALIAS;
            m_template_alias_instances = new TemplateAliasPrefab(template_type_base, template_params);
        }
        else
        {
            m_symbol_kind = TYPE;
            m_template_type_instances = new TemplateTypePrefab(template_type_base, template_params);
        }
    }

    //////////////////////////////////////

    lang_Symbol::TemplateValuePrefab::TemplateValuePrefab(ast::AstValueBase* ast, const std::list<wo_pstring_t>& template_params)
        : m_origin_value_ast(ast)
        , m_template_params(template_params)
    {
    }
    lang_Symbol::TemplateTypePrefab::TemplateTypePrefab(ast::AstTypeHolder* ast, const std::list<wo_pstring_t>& template_params)
        :  m_origin_value_ast(ast)
        , m_template_params(template_params)
    {
    }
    lang_Symbol::TemplateAliasPrefab::TemplateAliasPrefab(ast::AstTypeHolder* ast, const std::list<wo_pstring_t>& template_params)
        : m_origin_value_ast(ast)
        , m_template_params(template_params)
    {
    }

    //////////////////////////////////////

    lang_TypeInstance::lang_TypeInstance(lang_Symbol* symbol)
        : m_symbol(symbol)
        , m_determined_type(std::nullopt)
    {
    }
    lang_AliasInstance::lang_AliasInstance(lang_Symbol* symbol)
        : m_symbol(symbol)
        , m_determined_type(std::nullopt)
    {
    }
    lang_ValueInstance::lang_ValueInstance(lang_Symbol* symbol)
        : m_symbol(symbol)
        , m_determined_constant(std::nullopt)
        , m_determined_type(std::nullopt)
    {
    }
    lang_ValueInstance::~lang_ValueInstance()
    {
        if (m_determined_constant && m_determined_constant.value().is_gcunit())
            delete m_determined_constant.value().gcunit;
    }

    //////////////////////////////////////

    lang_TypeInstance::DeterminedType::DeterminedType(
        base_type type, const ExternalTypeDescription& desc)
        : m_base_type(type)
        , m_external_type_description(desc)
    {
    }
    lang_TypeInstance::DeterminedType::DeterminedType(DeterminedType&& item)
        : m_base_type(item.m_base_type)
        , m_external_type_description(item.m_external_type_description)
    {
        item.m_base_type = NIL;
    }
    lang_TypeInstance::DeterminedType& lang_TypeInstance::DeterminedType::operator=(DeterminedType&& item)
    {
        m_base_type = item.m_base_type;
        m_external_type_description = item.m_external_type_description;
        item.m_base_type = NIL;
        return *this;
    }
    lang_TypeInstance::DeterminedType::~DeterminedType()
    {
        switch (m_base_type)
        {
        case base_type::ARRAY:
        case base_type::VECTOR:
            delete m_external_type_description.m_array_or_vector;
            break;
        case base_type::DICTIONARY:
        case base_type::MAPPING:
            delete m_external_type_description.m_dictionary_or_mapping;
            break;
        case base_type::TUPLE:
            delete m_external_type_description.m_tuple;
            break;
        case base_type::FUNCTION:
            delete m_external_type_description.m_function;
            break;
        case base_type::STRUCT:
            delete m_external_type_description.m_struct;
            break;
        }
    }

    //////////////////////////////////////

    lang_Scope::lang_Scope(const std::optional<lang_Scope*>& parent_scope, lang_Namespace* belongs)
        : m_defined_symbols{}
        , m_sub_scopes{}
        , m_parent_scope(parent_scope)
        , m_belongs_to_namespace(belongs)
    {
    }

    bool lang_Scope::is_namespace_scope() const
    {
        return m_belongs_to_namespace->m_this_scope.get() == this;
    }

    //////////////////////////////////////

    lang_Namespace::lang_Namespace(const std::optional<lang_Namespace*>& parent_namespace)
        : m_parent_namespace(parent_namespace)
    {
        m_this_scope = std::make_unique<lang_Scope>(
            m_parent_namespace 
                ? std::optional(m_parent_namespace.value()->m_this_scope.get())
                : std::nullopt,
            this);
    }

    //////////////////////////////////////

    LangContext::AstNodeWithState::AstNodeWithState(ast::AstBase* node)
        : m_state(UNPROCESSED) 
        , m_ast_node(node)
    {
    }
    LangContext::AstNodeWithState::AstNodeWithState(
        pass_behavior state, ast::AstBase* node)
        : m_state(state)
        , m_ast_node(node)
    {
    }

    //////////////////////////////////////

    LangContext::pass_behavior LangContext::ProcessAstJobs::process_node(
        LangContext* ctx, lexer& lex, const AstNodeWithState& node_state, PassProcessStackT& out_stack)
    {
        auto fnd = m_node_processer.find(node_state.m_ast_node->node_type);
        if (fnd == m_node_processer.end())
            return OKAY;

        return fnd->second(ctx, lex, node_state, out_stack);
    }

    //////////////////////////////////////

    LangContext::OriginTypeHolder::OriginNoTemplateSymbolAndInstance::OriginNoTemplateSymbolAndInstance()
        : m_symbol(nullptr)
        , m_type_instance(nullptr)
    {
    }
    LangContext::OriginTypeHolder::OriginNoTemplateSymbolAndInstance::~OriginNoTemplateSymbolAndInstance()
    {
        wo_assert(m_type_instance);
        delete m_type_instance;
    }

    //////////////////////////////////////

    LangContext::LangContext()
        : m_root_namespace(std::make_unique<lang_Namespace>(std::nullopt))
    {
        m_scope_stack.push(m_root_namespace->m_this_scope.get());
    }

    bool LangContext::anylize_pass(
        lexer& lex, ast::AstBase* root, const PassFunctionT& pass_function)
    {
        PassProcessStackT process_stack;
        std::stack<AstNodeWithState*> process_roots;

        process_stack.push(AstNodeWithState(root));

        while (!process_stack.empty())
        {
            AstNodeWithState& top_state = process_stack.top();

            if (top_state.m_state == HOLD || top_state.m_state == HOLD_BUT_CHILD_FAILED)
                process_roots.pop();

            auto process_state = pass_function(this, lex, top_state, process_stack);

            switch (process_state)
            {
            case HOLD:
                top_state.m_state = HOLD;
                process_roots.push(&top_state);
                break;
            case FAILED:
                if (process_roots.empty())
                    return false;
                process_roots.top()->m_state = HOLD_BUT_CHILD_FAILED;
                /* FALL THROUGH */
            [[fallthrough]];
            case OKAY:
                process_stack.pop();
                break;
            default:
                wo_error("Unexpected pass behavior.");
            }
        }
        return true;
    }

    LangContext::pass_behavior LangContext::pass_0_process_scope_and_non_local_defination(
        lexer& lex, const AstNodeWithState& node_state, PassProcessStackT& out_stack)
    {
        return m_pass0_processers->process_node(this, lex, node_state, out_stack);
    }
    LangContext::pass_behavior LangContext::pass_1_process_basic_type_marking_and_constant_eval(
        lexer& lex, const AstNodeWithState& node_state, PassProcessStackT& out_stack)
    {
        return m_pass1_processers->process_node(this, lex, node_state, out_stack);
    }

    void LangContext::pass_0_5_register_builtin_types()
    {
        wo_assert(get_current_scope() == get_current_namespace()->m_this_scope.get());
        
        // Register builtin types.
        auto create_builtin_non_template_symbol_and_instance = [this](
            OriginTypeHolder::OriginNoTemplateSymbolAndInstance* out_sni,
            wo_pstring_t name,
            lang_TypeInstance::DeterminedType::base_type basic_type)
            {
                ast::AstDeclareAttribue* built_type_public_attrib = new ast::AstDeclareAttribue();
                built_type_public_attrib->m_access = ast::AstDeclareAttribue::PUBLIC;

                out_sni->m_symbol = define_symbol_in_current_scope(
                    name,
                    built_type_public_attrib,
                    WO_PSTR(_),
                    get_current_scope(),
                    lang_Symbol::kind::ALIAS).value();

                out_sni->m_type_instance = new lang_TypeInstance(m_origin_types.m_int.m_symbol);
                out_sni->m_type_instance->m_determined_type =
                    std::move(lang_TypeInstance::DeterminedType(basic_type, {}));
            };

        create_builtin_non_template_symbol_and_instance(
            &m_origin_types.m_nil, WO_PSTR(nil), lang_TypeInstance::DeterminedType::NIL);
        create_builtin_non_template_symbol_and_instance(
            &m_origin_types.m_int, WO_PSTR(int), lang_TypeInstance::DeterminedType::INTEGER);
        create_builtin_non_template_symbol_and_instance(
            &m_origin_types.m_real, WO_PSTR(real), lang_TypeInstance::DeterminedType::REAL);
        create_builtin_non_template_symbol_and_instance(
            &m_origin_types.m_handle, WO_PSTR(handle), lang_TypeInstance::DeterminedType::HANDLE);
        create_builtin_non_template_symbol_and_instance(
            &m_origin_types.m_bool, WO_PSTR(bool), lang_TypeInstance::DeterminedType::BOOLEAN);
        create_builtin_non_template_symbol_and_instance(
            &m_origin_types.m_string, WO_PSTR(string), lang_TypeInstance::DeterminedType::STRING);        
    }

    bool LangContext::process(lexer& lex, ast::AstBase* root)
    {
        if (!anylize_pass(lex, root, &LangContext::pass_0_process_scope_and_non_local_defination))
            return false;

        pass_0_5_register_builtin_types();

        if (!anylize_pass(lex, root, &LangContext::pass_1_process_basic_type_marking_and_constant_eval))
            return false;

        return true;
    }

    ////////////////////////

    void LangContext::begin_new_scope()
    {
        lang_Scope* current_scope = get_current_scope();
        auto new_scope = std::make_unique<lang_Scope>(
            current_scope, get_current_scope()->m_belongs_to_namespace);
        
        m_scope_stack.push(new_scope.get());

        current_scope->m_sub_scopes.emplace_back(std::move(new_scope));
    }
    void LangContext::end_last_scope()
    {
        m_scope_stack.pop();
    }
    bool LangContext::begin_new_namespace(wo_pstring_t name)
    {
        if (get_current_namespace()->m_this_scope.get() != get_current_scope())
            // Woolang 1.14.1: Define namespace in non-namespace scope is not allowed.
            return false;

        auto* current_namespace = get_current_namespace();
        auto fnd = current_namespace->m_sub_namespaces.find(name);
        if (fnd == current_namespace->m_sub_namespaces.end())
        {
            auto new_namespace = std::make_unique<lang_Namespace>(
                std::make_optional(get_current_namespace()));

            auto new_namespace_ptr = new_namespace.get();

            get_current_namespace()->m_sub_namespaces.insert(
                std::make_pair(name, std::move(new_namespace)));

            m_scope_stack.push(new_namespace_ptr->m_this_scope.get());
        }
        else
        {
            m_scope_stack.push(fnd->second->m_this_scope.get());
        }
        return true;
    }
    void LangContext::end_last_namespace()
    {
        m_scope_stack.pop();
    }
    lang_Scope* LangContext::get_current_scope()
    {
        return m_scope_stack.top();
    }
    lang_Namespace* LangContext::get_current_namespace()
    {
        return get_current_scope()->m_belongs_to_namespace;
    }
    std::optional<lang_Symbol*>
        LangContext::_search_symbol_from_current_scope(lexer& lex, ast::AstIdentifier* ident)
    {
        lang_Scope* current_scope = get_current_scope();
        if (ident->m_scope.empty())
        {
            do
            {
                auto fnd = current_scope->m_defined_symbols.find(ident->m_name);
                if (fnd != current_scope->m_defined_symbols.end())
                    return fnd->second.get();

                if (current_scope->m_parent_scope)
                    current_scope = current_scope->m_parent_scope.value();
                else
                    return std::nullopt;
            } while (true);
        }

        lang_Namespace* search_begin_namespace = current_scope->m_belongs_to_namespace;
        do
        {
            lang_Namespace* symbol_namespace = search_begin_namespace;
            for (auto* scope : ident->m_scope)
            {
                auto fnd = symbol_namespace->m_sub_namespaces.find(scope);
                if (fnd != symbol_namespace->m_sub_namespaces.end())
                    symbol_namespace = fnd->second.get();
                else
                    goto _label_try_upper_namespace;
            }

            // Ok, found the namespace.
            do
            {
                auto fnd = symbol_namespace->m_this_scope->m_defined_symbols.find(ident->m_name);
                if (fnd != symbol_namespace->m_this_scope->m_defined_symbols.end())
                    return fnd->second.get();
            } while (0);
        _label_try_upper_namespace:
            if (search_begin_namespace->m_parent_namespace)
                search_begin_namespace = search_begin_namespace->m_parent_namespace.value();
            else
                return std::nullopt;

        } while (true);
    }
    std::optional<lang_Symbol*>
        LangContext::find_symbol_in_current_scope(lexer& lex, ast::AstIdentifier* ident)
    {
        wo_assert(! ident->m_LANG_determined_symbol);

        lang_Namespace* search_begin_namespace;
        switch (ident->m_formal)
        {
        case ast::AstIdentifier::FROM_GLOBAL:
            search_begin_namespace = m_root_namespace.get();
            break;
        case ast::AstIdentifier::FROM_TYPE:
        {
            ast::AstTypeHolder* from_type = ident->m_from_type.value();
            auto determined_type = from_type->m_LANG_determined_type;
            if (!determined_type)
            {
                lex.lang_error(lexer::errorlevel::error, from_type, WO_ERR_UNKNOWN_TYPE);
                return std::nullopt;
            }
            lang_TypeInstance* type_instance = determined_type.value();
            lang_Scope* search_from_scope = type_instance->m_symbol->m_belongs_to_scope;
            if (!type_instance->m_symbol->m_belongs_to_scope->is_namespace_scope())
                return std::nullopt;

            search_begin_namespace = 
                type_instance->m_symbol->m_belongs_to_scope->m_belongs_to_namespace;
            break;
        }
        case ast::AstIdentifier::FROM_CURRENT:
        {
            auto found_symbol = _search_symbol_from_current_scope(lex, ident);
            if (found_symbol)
                ident->m_LANG_determined_symbol = found_symbol.value();
            return found_symbol;
            break;
        }
        }

        for (auto* scope : ident->m_scope)
        {
            auto fnd = search_begin_namespace->m_sub_namespaces.find(scope);
            if (fnd != search_begin_namespace->m_sub_namespaces.end())
                search_begin_namespace = fnd->second.get();
            else
                return std::nullopt;
        }

        auto fnd = search_begin_namespace->m_this_scope->m_defined_symbols.find(ident->m_name);
        if (fnd != search_begin_namespace->m_this_scope->m_defined_symbols.end())
        {
            lang_Symbol* symbol = fnd->second.get();
            ident->m_LANG_determined_symbol = symbol;
            return symbol;
        }
        return std::nullopt;
    }

#endif
}
