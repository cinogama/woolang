#include "wo_lang.hpp"

namespace wo
{
#ifndef WO_DISABLE_COMPILER
    LangContext::ProcessAstJobs* LangContext::m_pass0_processers;

    void LangContext::init_lang_processers()
    {
        m_pass0_processers = new ProcessAstJobs();
        init_pass0();
    }
    void LangContext::shutdown_lang_processers()
    {
        delete m_pass0_processers;
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
    lang_Symbol::lang_Symbol(kind kind)
        : m_symbol_kind(kind)
        , m_is_template(false)
    {
        switch (kind)
        {
        case VARIABLE:
            m_value_instance = new lang_ValueInstance();
            break;
        case TYPE:
            m_type_instance = new lang_TypeInstance();
            break;
        case ALIAS:
            m_alias_instance = new lang_AliasInstance();
            break;
        default:
            wo_error("Unexpected symbol kind.");
        }
    }
    lang_Symbol::lang_Symbol(lang_Scope* scope, ast::AstValueBase* template_value_base, const std::list<wo_pstring_t>& template_params)
        : m_symbol_kind(VARIABLE)
        , m_is_template(true)
    {
        m_template_value_instances = new TemplateValuePrefab(scope, template_value_base, template_params);
    }
    lang_Symbol::lang_Symbol(lang_Scope* scope, ast::AstTypeHolder* template_type_base, const std::list<wo_pstring_t>& template_params, bool is_alias)
        : m_is_template(true)
    {
        if (is_alias)
        {
            m_symbol_kind = ALIAS;
            m_template_alias_instances = new TemplateAliasPrefab(scope, template_type_base, template_params);
        }
        else
        {
            m_symbol_kind = TYPE;
            m_template_type_instances = new TemplateTypePrefab(scope, template_type_base, template_params);
        }
    }

    //////////////////////////////////////

    lang_Symbol::TemplateValuePrefab::TemplateValuePrefab(lang_Scope* scope, ast::AstValueBase* ast, const std::list<wo_pstring_t>& template_params)
        : m_belongs_to_scope(scope)
        , m_origin_value_ast(ast)
        , m_template_params(template_params)
    {
    }
    lang_Symbol::TemplateTypePrefab::TemplateTypePrefab(lang_Scope* scope, ast::AstTypeHolder* ast, const std::list<wo_pstring_t>& template_params)
        : m_belongs_to_scope(scope)
        , m_origin_value_ast(ast)
        , m_template_params(template_params)
    {
    }
    lang_Symbol::TemplateAliasPrefab::TemplateAliasPrefab(lang_Scope* scope, ast::AstTypeHolder* ast, const std::list<wo_pstring_t>& template_params)
        : m_belongs_to_scope(scope)
        , m_origin_value_ast(ast)
        , m_template_params(template_params)
    {
    }

    //////////////////////////////////////

    lang_TypeInstance::lang_TypeInstance()
        : m_determined_type(std::nullopt)
    {
    }
    lang_AliasInstance::lang_AliasInstance()
        : m_determined_type(std::nullopt)
    {
    }
    lang_ValueInstance::lang_ValueInstance()
        : m_determined_constant(std::nullopt)
        , m_determined_type(std::nullopt)
    {
    }

    //////////////////////////////////////

    lang_TypeInstance::DeterminedType::DeterminedType(base_type type, const ExternalTypeDescription& desc)
        : m_base_type(type)
        , m_external_type_description(desc)
    {
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

    lang_Scope::lang_Scope(lang_Namespace* belongs)
        : m_belongs_to_namespace(belongs)
    {
    }

    //////////////////////////////////////

    lang_Namespace::lang_Namespace(const std::optional<lang_Namespace*>& parent_namespace)
        : m_parent_namespace(parent_namespace)
    {
        m_this_scope = std::make_unique<lang_Scope>(this);
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

    bool LangContext::process(lexer& lex, ast::AstBase* root)
    {
        if (!anylize_pass(lex, root, &LangContext::pass_0_process_scope_and_non_local_defination))
            return false;

        return true;
    }

    void LangContext::begin_new_scope()
    {
        auto new_scope = std::make_unique<lang_Scope>(
            get_current_scope()->m_belongs_to_namespace);
        
        m_scope_stack.push(new_scope.get());

        get_current_scope()->m_sub_scopes.emplace_back(std::move(new_scope));
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
    bool LangContext::declare_pattern_symbol(
        lexer& lex, ast::AstPatternBase* pattern, std::optional<ast::AstValueBase*> init_value)
    {
        switch (pattern->node_type)
        {
        case ast::AstBase::AST_PATTERN_SINGLE:
        {
            ast::AstPatternSingle* single_pattern = static_cast<ast::AstPatternSingle*>(pattern);
            if (single_pattern->m_template_parameters)
            {
                wo_assert(init_value);
                single_pattern->m_LANG_declared_symbol = define_symbol_in_current_scope(
                    single_pattern->m_name, get_current_scope(), init_value.value(), single_pattern->m_template_parameters.value());
            }
            else
            {
                single_pattern->m_LANG_declared_symbol = define_symbol_in_current_scope(
                    single_pattern->m_name, lang_Symbol::kind::VARIABLE);
            }

            if (!single_pattern->m_LANG_declared_symbol)
            {
                lex.lang_error(lexer::errorlevel::error, single_pattern, 
                    WO_ERR_REDEFINED, 
                    single_pattern->m_name->c_str());

                return false;
            }

            return true;
            break;
        }
        case ast::AstBase::AST_PATTERN_TUPLE:
        {
            ast::AstPatternTuple* tuple_pattern = static_cast<ast::AstPatternTuple*>(pattern);
            bool success = true;
            for (auto& sub_pattern : tuple_pattern->m_fields)
                success = success && declare_pattern_symbol(lex, sub_pattern, std::nullopt);

            return success;
            break;
        }
        case ast::AstBase::AST_PATTERN_UNION:
        {
            ast::AstPatternUnion* union_pattern = static_cast<ast::AstPatternUnion*>(pattern);
            bool success = true;
            if (union_pattern->m_field)
                success = declare_pattern_symbol(lex, union_pattern->m_field.value(), std::nullopt);

            return success;
            break;
        }
        default:
            wo_error("Unexpected pattern type.");
        }
        return false;
    }
#endif
}
