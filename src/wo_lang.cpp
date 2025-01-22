#include "wo_lang.hpp"

#include <unordered_set>
#include <algorithm>

namespace wo
{
#ifndef WO_DISABLE_COMPILER
    LangContext::ProcessAstJobs* LangContext::m_pass0_processers;
    LangContext::ProcessAstJobs* LangContext::m_pass1_processers;
    LangContext::ProcessAstJobs* LangContext::m_passir_A_processers;
    LangContext::ProcessAstJobs* LangContext::m_passir_B_processers;

    void LangContext::init_lang_processers()
    {
        m_pass0_processers = new ProcessAstJobs();
        m_pass1_processers = new ProcessAstJobs();
        m_passir_A_processers = new ProcessAstJobs();
        m_passir_B_processers = new ProcessAstJobs();

        init_pass0();
        init_pass1();
        init_passir();
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
        wo_pstring_t name,
        const std::optional<ast::AstDeclareAttribue*>& attr,
        std::optional<ast::AstBase*> symbol_declare_ast,
        wo_pstring_t src_location,
        lang_Scope* scope,
        kind kind,
        bool mutable_variable)
        : m_symbol_kind(kind)
        , m_is_template(false)
        , m_is_global(false)
        , m_name(name)
        , m_defined_source(src_location)
        , m_declare_attribute(attr)
        , m_belongs_to_scope(scope)
        , m_symbol_declare_ast(symbol_declare_ast)
        , m_is_builtin(false)
        , m_symbol_edge(0)
        , m_has_been_used(false)
    {
        switch (kind)
        {
        case VARIABLE:
            m_value_instance = new lang_ValueInstance(
                mutable_variable, this, std::nullopt);
            break;
        case TYPE:
            m_type_instance = new lang_TypeInstance(this, std::nullopt);
            break;
        case ALIAS:
            m_alias_instance = new lang_AliasInstance(this);
            break;
        default:
            wo_error("Unexpected symbol kind.");
        }
    }
    lang_Symbol::lang_Symbol(
        wo_pstring_t name,
        const std::optional<ast::AstDeclareAttribue*>& attr,
        std::optional<ast::AstBase*> symbol_declare_ast,
        wo_pstring_t src_location,
        lang_Scope* scope,
        ast::AstValueBase* template_value_base,
        const std::list<wo_pstring_t>& template_params,
        bool mutable_variable)
        : m_symbol_kind(VARIABLE)
        , m_is_template(true)
        , m_is_global(false)
        , m_name(name)
        , m_defined_source(src_location)
        , m_declare_attribute(attr)
        , m_belongs_to_scope(scope)
        , m_symbol_declare_ast(symbol_declare_ast)
        , m_is_builtin(false)
        , m_symbol_edge(0)
        , m_has_been_used(false)
    {
        m_template_value_instances = new TemplateValuePrefab(
            this, mutable_variable, template_value_base, template_params);
    }
    lang_Symbol::lang_Symbol(
        wo_pstring_t name,
        const std::optional<ast::AstDeclareAttribue*>& attr,
        std::optional<ast::AstBase*> symbol_declare_ast,
        wo_pstring_t src_location,
        lang_Scope* scope,
        ast::AstTypeHolder* template_type_base,
        const std::list<wo_pstring_t>& template_params,
        bool is_alias)
        : m_is_template(true)
        , m_is_global(false)
        , m_name(name)
        , m_defined_source(src_location)
        , m_declare_attribute(attr)
        , m_belongs_to_scope(scope)
        , m_symbol_declare_ast(symbol_declare_ast)
        , m_is_builtin(false)
        , m_symbol_edge(0)
        , m_has_been_used(false)
    {
        if (is_alias)
        {
            m_symbol_kind = ALIAS;
            m_template_alias_instances = new TemplateAliasPrefab(
                this, template_type_base, template_params);
        }
        else
        {
            m_symbol_kind = TYPE;
            m_template_type_instances = new TemplateTypePrefab(
                this, template_type_base, template_params);
        }
    }

    //////////////////////////////////////

    lang_Symbol::TemplateValuePrefab::TemplateValuePrefab(
        lang_Symbol* symbol,
        bool mutable_,
        ast::AstValueBase* ast,
        const std::list<wo_pstring_t>& template_params)
        : m_symbol(symbol), m_mutable(mutable_), m_origin_value_ast(ast), m_template_params(template_params)
    {
    }

    lang_TemplateAstEvalStateValue* lang_Symbol::TemplateValuePrefab::find_or_create_template_instance(
        const TemplateArgumentListT& template_args)
    {
        auto fnd = m_template_instances.find(template_args);
        if (fnd != m_template_instances.end())
            return fnd->second.get();

        ast::AstValueBase* template_instance = static_cast<ast::AstValueBase*>(
            m_origin_value_ast->clone());
        auto new_instance = std::make_unique<lang_TemplateAstEvalStateValue>(
            m_symbol, template_instance, template_args);

        auto* result = new_instance.get();

        m_template_instances.insert(
            std::make_pair(template_args, std::move(new_instance)));

        return result;
    }

    //////////////////////////////////////

    lang_Symbol::TemplateTypePrefab::TemplateTypePrefab(
        lang_Symbol* symbol,
        ast::AstTypeHolder* ast,
        const std::list<wo_pstring_t>& template_params)
        : m_symbol(symbol), m_origin_value_ast(ast), m_template_params(template_params)
    {
    }
    lang_TemplateAstEvalStateType* lang_Symbol::TemplateTypePrefab::find_or_create_template_instance(
        const TemplateArgumentListT& template_args)
    {
        auto fnd = m_template_instances.find(template_args);
        if (fnd != m_template_instances.end())
            return fnd->second.get();

        ast::AstTypeHolder* template_instance = static_cast<ast::AstTypeHolder*>(
            m_origin_value_ast->clone());
        auto new_instance = std::make_unique<lang_TemplateAstEvalStateType>(
            m_symbol, template_instance, template_args);

        auto* result = new_instance.get();

        m_template_instances.insert(
            std::make_pair(template_args, std::move(new_instance)));

        return result;
    }

    //////////////////////////////////////

    lang_Symbol::TemplateAliasPrefab::TemplateAliasPrefab(
        lang_Symbol* symbol,
        ast::AstTypeHolder* ast,
        const std::list<wo_pstring_t>& template_params)
        : m_symbol(symbol), m_origin_value_ast(ast), m_template_params(template_params)
    {
    }
    lang_TemplateAstEvalStateAlias* lang_Symbol::TemplateAliasPrefab::find_or_create_template_instance(
        const TemplateArgumentListT& template_args)
    {
        auto fnd = m_template_instances.find(template_args);
        if (fnd != m_template_instances.end())
            return fnd->second.get();

        ast::AstTypeHolder* template_instance = static_cast<ast::AstTypeHolder*>(
            m_origin_value_ast->clone());
        auto new_instance = std::make_unique<lang_TemplateAstEvalStateAlias>(
            m_symbol, template_instance);

        auto* result = new_instance.get();

        m_template_instances.insert(
            std::make_pair(template_args, std::move(new_instance)));

        return result;
    }

    //////////////////////////////////////

    lang_TypeInstance::lang_TypeInstance(
        lang_Symbol* symbol,
        const std::optional<std::list<lang_TypeInstance*>>& template_arguments)
        : m_symbol(symbol), m_determined_base_type_or_mutable(std::nullopt), m_instance_template_arguments(template_arguments)
    {
    }

    bool lang_TypeInstance::is_immutable() const
    {
        return nullptr != std::get_if<std::optional<DeterminedType>>(
            &m_determined_base_type_or_mutable);
    }
    bool lang_TypeInstance::is_mutable() const
    {
        return !is_immutable();
    }
    std::optional<const lang_TypeInstance::DeterminedType*> lang_TypeInstance::get_determined_type() const
    {
        const std::optional<DeterminedType>* dtype = std::get_if<std::optional<DeterminedType>>(
            &m_determined_base_type_or_mutable);

        if (dtype != nullptr)
        {
            if (dtype->has_value())
                return &dtype->value();
            return std::nullopt;
        }

        return std::get<lang_TypeInstance*>(m_determined_base_type_or_mutable)
            ->get_determined_type();
    }

    void lang_TypeInstance::determine_base_type_by_another_type(lang_TypeInstance* immutable_from_type)
    {
        wo_assert(immutable_from_type->is_immutable());

        auto determined_type_may_null = immutable_from_type->get_determined_type();
        if (determined_type_may_null)
            determine_base_type_copy(*determined_type_may_null.value());
        else
        {
            // from_type it not decided, delay this type instance.
            if (this != immutable_from_type)
                immutable_from_type->m_LANG_pending_depend_this.insert(this);
        }
    }
    void lang_TypeInstance::_update_type_instance_depend_this(const DeterminedType& copy_type)
    {
        std::stack<lang_TypeInstance*> pending_stack;
        for (auto* depended : m_LANG_pending_depend_this)
            pending_stack.push(depended);

        m_LANG_pending_depend_this.clear();

        while (!pending_stack.empty())
        {
            lang_TypeInstance* current = pending_stack.top();
            pending_stack.pop();

            current->determine_base_type_copy(copy_type);

            for (auto* depended : current->m_LANG_pending_depend_this)
                pending_stack.push(depended);

            current->m_LANG_pending_depend_this.clear();
        }
    }
    void lang_TypeInstance::determine_base_type_copy(const DeterminedType& copy_type)
    {
        wo_assert(std::get<std::optional<DeterminedType>>(
            m_determined_base_type_or_mutable)
            .has_value() == false);

        DeterminedType::ExternalTypeDescription extern_desc;
        switch (copy_type.m_base_type)
        {
        case DeterminedType::base_type::ARRAY:
        case DeterminedType::base_type::VECTOR:
            extern_desc.m_array_or_vector = new DeterminedType::ArrayOrVector(
                *copy_type.m_external_type_description.m_array_or_vector);
            break;
        case DeterminedType::base_type::DICTIONARY:
        case DeterminedType::base_type::MAPPING:
            extern_desc.m_dictionary_or_mapping = new DeterminedType::DictionaryOrMapping(
                *copy_type.m_external_type_description.m_dictionary_or_mapping);
            break;
        case DeterminedType::base_type::TUPLE:
            extern_desc.m_tuple = new DeterminedType::Tuple(
                *copy_type.m_external_type_description.m_tuple);
            break;
        case DeterminedType::base_type::FUNCTION:
            extern_desc.m_function = new DeterminedType::Function(
                *copy_type.m_external_type_description.m_function);
            break;
        case DeterminedType::base_type::STRUCT:
            extern_desc.m_struct = new DeterminedType::Struct(
                *copy_type.m_external_type_description.m_struct);
            break;
        case DeterminedType::base_type::UNION:
            extern_desc.m_union = new DeterminedType::Union(
                *copy_type.m_external_type_description.m_union);
            break;
        default:
            // Nothing to do.
            break;
        }

        _update_type_instance_depend_this(copy_type);

        m_determined_base_type_or_mutable =
            std::move(DeterminedType(copy_type.m_base_type, extern_desc));
    }
    void lang_TypeInstance::determine_base_type_move(DeterminedType&& move_type)
    {
        wo_assert(std::get<std::optional<DeterminedType>>(
            m_determined_base_type_or_mutable)
            .has_value() == false);

        _update_type_instance_depend_this(move_type);

        m_determined_base_type_or_mutable = std::move(move_type);
    }

    //////////////////////////////////////

    lang_TemplateAstEvalStateBase::lang_TemplateAstEvalStateBase(lang_Symbol* symbol, ast::AstBase* ast)
        : m_state(UNPROCESSED), m_ast(ast), m_symbol(symbol)
    {
    }

    //////////////////////////////////////

    lang_TemplateAstEvalStateValue::lang_TemplateAstEvalStateValue(
        lang_Symbol* symbol,
        ast::AstValueBase* ast,
        const std::list<lang_TypeInstance*>& template_arguments)
        : lang_TemplateAstEvalStateBase(symbol, ast)
    {
        m_value_instance = std::make_unique<lang_ValueInstance>(
            symbol->m_template_value_instances->m_mutable, symbol, template_arguments);

        if (!symbol->m_template_value_instances->m_mutable && ast->node_type == ast::AstBase::AST_VALUE_FUNCTION)
            m_value_instance->try_determine_function(static_cast<ast::AstValueFunction*>(ast));
    }

    //////////////////////////////////////

    lang_TemplateAstEvalStateType::lang_TemplateAstEvalStateType(
        lang_Symbol* symbol, ast::AstTypeHolder* ast, const std::list<lang_TypeInstance*>& template_arguments)
        : lang_TemplateAstEvalStateBase(symbol, ast)
    {
        m_type_instance = std::make_unique<lang_TypeInstance>(
            symbol, std::optional(template_arguments));
    }

    //////////////////////////////////////

    lang_TemplateAstEvalStateAlias::lang_TemplateAstEvalStateAlias(lang_Symbol* symbol, ast::AstTypeHolder* ast)
        : lang_TemplateAstEvalStateBase(symbol, ast)
    {
        m_alias_instance = std::make_unique<lang_AliasInstance>(symbol);
    }

    //////////////////////////////////////

    lang_AliasInstance::lang_AliasInstance(lang_Symbol* symbol)
        : m_symbol(symbol), m_determined_type(std::nullopt)
    {
    }

    //////////////////////////////////////

    void lang_ValueInstance::try_determine_function(ast::AstValueFunction* func)
    {
        wo_assert(!m_mutable);
        m_determined_constant_or_function = func;
    }
    void lang_ValueInstance::try_determine_const_value(ast::AstValueBase* init_val)
    {
        wo_assert(!m_mutable);
        if (init_val->m_evaled_const_value.has_value())
        {
            wo::value new_constant;
            new_constant.set_val_compile_time(&init_val->m_evaled_const_value.value());

            m_determined_constant_or_function = new_constant;
        }
        else if (init_val->node_type == ast::AstBase::AST_VALUE_VARIABLE)
        {
            ast::AstValueVariable* variable_value = static_cast<ast::AstValueVariable*>(init_val);
            lang_ValueInstance* variable_instance = variable_value->m_LANG_variable_instance.value();

            if (variable_instance->m_determined_constant_or_function.has_value())
            {
                ast::AstValueFunction** function_instance =
                    std::get_if< ast::AstValueFunction*>(
                        &variable_instance->m_determined_constant_or_function.value());

                if (function_instance != nullptr)
                {
                    m_determined_constant_or_function = *function_instance;
                    m_IR_normal_function = *function_instance;
                }
            }
        }
    }

    bool lang_ValueInstance::IR_need_storage() const
    {
        if (m_determined_constant_or_function.has_value())
        {
            wo_assert(!m_mutable);

            auto& determined_constant_or_function =
                m_determined_constant_or_function.value();

            ast::AstValueFunction* const* function_instance =
                std::get_if<ast::AstValueFunction*>(&determined_constant_or_function);

            if (function_instance != nullptr)
            {
                if (!(*function_instance)->m_LANG_captured_context.m_captured_variables.empty())
                    // Function with captured variables, need storage.
                    return true;
            }

            // Is constant or normal function, no need storage.
            return false;
        }

        return true;
    }

    lang_ValueInstance::lang_ValueInstance(
        bool mutable_,
        lang_Symbol* symbol,
        const std::optional<std::list<lang_TypeInstance*>>& template_arguments)
        : m_symbol(symbol), m_mutable(mutable_), m_determined_constant_or_function(std::nullopt), m_determined_type(std::nullopt), m_instance_template_arguments(template_arguments)
    {
    }
    lang_ValueInstance::~lang_ValueInstance()
    {
        if (m_determined_constant_or_function.has_value())
        {
            wo::value* determined_value = std::get_if<wo::value>(
                &m_determined_constant_or_function.value());

            if (determined_value != nullptr)
            {
                if (determined_value->is_gcunit())
                    delete determined_value->gcunit;
            }
        }
    }

    //////////////////////////////////////

    lang_TypeInstance::DeterminedType::DeterminedType(
        base_type type, const ExternalTypeDescription& desc)
        : m_base_type(type), m_external_type_description(desc)
    {
    }
    lang_TypeInstance::DeterminedType::DeterminedType(DeterminedType&& item)
        : m_base_type(item.m_base_type), m_external_type_description(item.m_external_type_description)
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
        case base_type::UNION:
            delete m_external_type_description.m_union;
            break;
        }
    }

    //////////////////////////////////////

    lang_Scope::lang_Scope(const std::optional<lang_Scope*>& parent_scope, lang_Namespace* belongs)
        : m_defined_symbols{}, m_sub_scopes{}, m_parent_scope(parent_scope), m_belongs_to_namespace(belongs), m_visibility_from_edge_for_template_check(0)
    {
    }

    bool lang_Scope::is_namespace_scope() const
    {
        return m_belongs_to_namespace->m_this_scope.get() == this;
    }

    //////////////////////////////////////

    lang_Namespace::lang_Namespace(wo_pstring_t name, const std::optional<lang_Namespace*>& parent_namespace)
        : m_name(name), m_parent_namespace(parent_namespace)
    {
        m_this_scope = std::make_unique<lang_Scope>(
            m_parent_namespace
            ? std::optional(m_parent_namespace.value()->m_this_scope.get())
            : std::nullopt,
            this);
    }

    //////////////////////////////////////

    LangContext::AstNodeWithState::AstNodeWithState(ast::AstBase* node)
        : m_state(UNPROCESSED), m_ast_node(node)
#ifndef NDEBUG
        ,
        m_debug_scope_layer_count(0), m_debug_entry_scope(nullptr)
#endif
    {
    }
    LangContext::AstNodeWithState::AstNodeWithState(
        pass_behavior state, ast::AstBase* node)
        : m_state(state), m_ast_node(node)
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

    bool LangContext::ProcessAstJobs::check_has_processer(ast::AstBase::node_type_t nodetype)
    {
        return m_node_processer.find(nodetype) != m_node_processer.end();
    }

    //////////////////////////////////////

    LangContext::OriginTypeHolder::OriginTypeChain*
        LangContext::OriginTypeHolder::OriginTypeChain::path(const std::list<lang_TypeInstance*>& type_path)
    {
        OriginTypeChain* current_chain = this;
        for (auto* type : type_path)
        {
            auto fnd = current_chain->m_chain.find(type);
            if (fnd == current_chain->m_chain.end())
            {
                auto new_chain_node = std::make_unique<OriginTypeChain>();
                auto* next_chain_node_p = new_chain_node.get();

                current_chain->m_chain.insert(std::make_pair(type, std::move(new_chain_node)));

                current_chain = next_chain_node_p;
            }
            else
                current_chain = fnd->second.get();
        }
        return current_chain;
    }
    LangContext::OriginTypeHolder::OriginTypeChain::~OriginTypeChain()
    {
        if (m_type_instance)
            delete m_type_instance.value();
    }

    //////////////////////////////////////

    LangContext::OriginTypeHolder::UnionStructTypeIndexChain*
        LangContext::OriginTypeHolder::UnionStructTypeIndexChain::path(
            const std::list<std::tuple<ast::AstDeclareAttribue::accessc_attrib, wo_pstring_t, lang_TypeInstance*>>& type_path)
    {
        UnionStructTypeIndexChain* current_chain = this;
        for (auto& [attrib, field, type_may_null] : type_path)
        {
            auto& field_chain = current_chain->m_chain[attrib][field];
            auto fnd = field_chain.find(type_may_null);
            if (fnd == field_chain.end())
            {
                auto new_chain_node = std::make_unique<UnionStructTypeIndexChain>();
                auto* next_chain_node_p = new_chain_node.get();

                field_chain.insert(std::make_pair(type_may_null, std::move(new_chain_node)));

                current_chain = next_chain_node_p;
            }
            else
                current_chain = fnd->second.get();
        }
        return current_chain;
    }
    LangContext::OriginTypeHolder::UnionStructTypeIndexChain::~UnionStructTypeIndexChain()
    {
        if (m_type_instance)
            delete m_type_instance.value();
    }

    //////////////////////////////////////

    lang_TypeInstance* LangContext::OriginTypeHolder::create_dictionary_type(lang_TypeInstance* key_type, lang_TypeInstance* value_type)
    {
        OriginTypeChain* chain_node = m_dictionary_chain.path({ key_type, value_type });
        if (!chain_node->m_type_instance)
        {
            lang_TypeInstance::DeterminedType::ExternalTypeDescription desc;

            desc.m_dictionary_or_mapping = new lang_TypeInstance::DeterminedType::DictionaryOrMapping();

            desc.m_dictionary_or_mapping->m_key_type = key_type;
            desc.m_dictionary_or_mapping->m_value_type = value_type;

            lang_TypeInstance::DeterminedType determined_type_detail(
                lang_TypeInstance::DeterminedType::base_type::DICTIONARY, desc);

            lang_TypeInstance* new_type_instance = new lang_TypeInstance(m_dictionary, std::list{ key_type, value_type });
            new_type_instance->determine_base_type_move(std::move(determined_type_detail));

            chain_node->m_type_instance = new_type_instance;
        }
        return chain_node->m_type_instance.value();
    }
    lang_TypeInstance* LangContext::OriginTypeHolder::create_mapping_type(lang_TypeInstance* key_type, lang_TypeInstance* value_type)
    {
        OriginTypeChain* chain_node = m_mapping_chain.path({ key_type, value_type });
        if (!chain_node->m_type_instance)
        {
            lang_TypeInstance::DeterminedType::ExternalTypeDescription desc;

            desc.m_dictionary_or_mapping = new lang_TypeInstance::DeterminedType::DictionaryOrMapping();

            desc.m_dictionary_or_mapping->m_key_type = key_type;
            desc.m_dictionary_or_mapping->m_value_type = value_type;

            lang_TypeInstance::DeterminedType determined_type_detail(
                lang_TypeInstance::DeterminedType::base_type::MAPPING, desc);

            lang_TypeInstance* new_type_instance = new lang_TypeInstance(m_mapping, std::list{ key_type, value_type });
            new_type_instance->determine_base_type_move(std::move(determined_type_detail));

            chain_node->m_type_instance = new_type_instance;
        }
        return chain_node->m_type_instance.value();
    }
    lang_TypeInstance* LangContext::OriginTypeHolder::create_array_type(lang_TypeInstance* element_type)
    {
        OriginTypeChain* chain_node = m_array_chain.path({ element_type });
        if (!chain_node->m_type_instance)
        {
            lang_TypeInstance::DeterminedType::ExternalTypeDescription desc;

            desc.m_array_or_vector = new lang_TypeInstance::DeterminedType::ArrayOrVector();

            desc.m_array_or_vector->m_element_type = element_type;

            lang_TypeInstance::DeterminedType determined_type_detail(
                lang_TypeInstance::DeterminedType::base_type::ARRAY, desc);

            lang_TypeInstance* new_type_instance = new lang_TypeInstance(m_array, std::list{ element_type });
            new_type_instance->determine_base_type_move(std::move(determined_type_detail));

            chain_node->m_type_instance = new_type_instance;
        }
        return chain_node->m_type_instance.value();
    }
    lang_TypeInstance* LangContext::OriginTypeHolder::create_vector_type(lang_TypeInstance* element_type)
    {
        OriginTypeChain* chain_node = m_vector_chain.path({ element_type });
        if (!chain_node->m_type_instance)
        {
            lang_TypeInstance::DeterminedType::ExternalTypeDescription desc;

            desc.m_array_or_vector = new lang_TypeInstance::DeterminedType::ArrayOrVector();

            desc.m_array_or_vector->m_element_type = element_type;

            lang_TypeInstance::DeterminedType determined_type_detail(
                lang_TypeInstance::DeterminedType::base_type::VECTOR, desc);

            lang_TypeInstance* new_type_instance = new lang_TypeInstance(m_vector, std::list{ element_type });
            new_type_instance->determine_base_type_move(std::move(determined_type_detail));

            chain_node->m_type_instance = new_type_instance;
        }
        return chain_node->m_type_instance.value();
    }
    lang_TypeInstance* LangContext::OriginTypeHolder::create_tuple_type(const std::list<lang_TypeInstance*>& element_types)
    {
        OriginTypeChain* chain_node = m_tuple_chain.path(element_types);
        if (!chain_node->m_type_instance)
        {
            lang_TypeInstance::DeterminedType::ExternalTypeDescription desc;

            desc.m_tuple = new lang_TypeInstance::DeterminedType::Tuple();
            desc.m_tuple->m_element_types = std::vector<lang_TypeInstance*>(
                element_types.begin(), element_types.end());

            lang_TypeInstance::DeterminedType determined_type_detail(
                lang_TypeInstance::DeterminedType::base_type::TUPLE, desc);

            lang_TypeInstance* new_type_instance = new lang_TypeInstance(m_tuple, std::nullopt);
            new_type_instance->determine_base_type_move(std::move(determined_type_detail));

            chain_node->m_type_instance = new_type_instance;
        }
        return chain_node->m_type_instance.value();
    }
    lang_TypeInstance* LangContext::OriginTypeHolder::create_function_type(bool is_variadic, const std::list<lang_TypeInstance*>& param_types, lang_TypeInstance* return_type)
    {
        OriginTypeChain* chain_node = m_function_chain[is_variadic ? 1 : 0].path(param_types)->path({ return_type });
        if (!chain_node->m_type_instance)
        {
            lang_TypeInstance::DeterminedType::ExternalTypeDescription desc;

            desc.m_function = new lang_TypeInstance::DeterminedType::Function();
            desc.m_function->m_is_variadic = is_variadic;
            desc.m_function->m_param_types = param_types;
            desc.m_function->m_return_type = return_type;

            lang_TypeInstance::DeterminedType determined_type_detail(
                lang_TypeInstance::DeterminedType::base_type::FUNCTION, desc);

            lang_TypeInstance* new_type_instance = new lang_TypeInstance(m_function, std::nullopt);
            new_type_instance->determine_base_type_move(std::move(determined_type_detail));

            chain_node->m_type_instance = new_type_instance;
        }
        return chain_node->m_type_instance.value();
    }
    lang_TypeInstance* LangContext::OriginTypeHolder::create_struct_type(
        const std::list<std::tuple<ast::AstDeclareAttribue::accessc_attrib, wo_pstring_t, lang_TypeInstance*>>& member_types)
    {
        UnionStructTypeIndexChain* chain_node = m_struct_chain.path(member_types);
        if (!chain_node->m_type_instance)
        {
            lang_TypeInstance::DeterminedType::ExternalTypeDescription desc;

            desc.m_struct = new lang_TypeInstance::DeterminedType::Struct();

            wo_integer_t index = 0;
            for (auto& [access, field, type] : member_types)
            {
                auto& field_detail = desc.m_struct->m_member_types[field];
                field_detail.m_attrib = access;
                field_detail.m_member_type = type;
                field_detail.m_offset = index++;
            }

            lang_TypeInstance::DeterminedType determined_type_detail(
                lang_TypeInstance::DeterminedType::base_type::STRUCT, desc);

            lang_TypeInstance* new_type_instance = new lang_TypeInstance(m_struct, std::nullopt);
            new_type_instance->determine_base_type_move(std::move(determined_type_detail));

            chain_node->m_type_instance = new_type_instance;
        }
        return chain_node->m_type_instance.value();
    }
    lang_TypeInstance* LangContext::OriginTypeHolder::create_union_type(
        const std::list<std::pair<wo_pstring_t, std::optional<lang_TypeInstance*>>>& member_types)
    {
        std::list<std::tuple<ast::AstDeclareAttribue::accessc_attrib, wo_pstring_t, lang_TypeInstance*>>
            member_types_no_optional;
        for (auto& [field, type] : member_types)
            member_types_no_optional.push_back(
                std::make_tuple(ast::AstDeclareAttribue::accessc_attrib::PUBLIC, field, type ? type.value() : nullptr));

        UnionStructTypeIndexChain* chain_node = m_union_chain.path(member_types_no_optional);
        if (!chain_node->m_type_instance)
        {
            lang_TypeInstance::DeterminedType::ExternalTypeDescription desc;

            desc.m_union = new lang_TypeInstance::DeterminedType::Union();

            wo_integer_t index = 0;
            for (auto& [_access, field, type] : member_types_no_optional)
            {
                (void)_access;

                auto& field_detail = desc.m_union->m_union_label[field];
                field_detail.m_item_type = type == nullptr ? std::nullopt : std::optional(type);
                field_detail.m_label = index++;
            }

            lang_TypeInstance::DeterminedType determined_type_detail(
                lang_TypeInstance::DeterminedType::base_type::UNION, desc);

            lang_TypeInstance* new_type_instance = new lang_TypeInstance(m_union, std::nullopt);
            new_type_instance->determine_base_type_move(std::move(determined_type_detail));

            chain_node->m_type_instance = new_type_instance;
        }
        return chain_node->m_type_instance.value();
    }

    std::optional<lang_TypeInstance*> LangContext::OriginTypeHolder::create_or_find_origin_type(
        lexer& lex, LangContext* ctx, ast::AstTypeHolder* type_holder)
    {
        wo_assert(!type_holder->m_LANG_determined_type);

        switch (type_holder->m_formal)
        {
        case ast::AstTypeHolder::IDENTIFIER:
        {
            auto& identifier = type_holder->m_typeform.m_identifier;
            auto* symbol = identifier->m_LANG_determined_symbol.value();

            if (type_holder->m_LANG_refilling_template_target_symbol.has_value())
                symbol = type_holder->m_LANG_refilling_template_target_symbol.value();

            wo_assert(symbol->m_is_builtin);
            if (!identifier->m_template_arguments)
            {
                lex.lang_error(lexer::errorlevel::error, type_holder,
                    WO_ERR_EXPECTED_TEMPLATE_ARGUMENT,
                    ctx->get_symbol_name_w(symbol));

                return std::nullopt;
            }

            auto& template_arguments = identifier->m_template_arguments.value();

            if (symbol == m_dictionary)
            {
                if (template_arguments.size() != 2)
                {
                    lex.lang_error(lexer::errorlevel::error, type_holder,
                        WO_ERR_UNEXPECTED_TEMPLATE_COUNT,
                        (size_t)2);

                    return std::nullopt;
                }
                auto template_iter = template_arguments.begin();
                auto* key_type = (*(template_iter++))->m_LANG_determined_type.value();
                auto* value_type = (*template_iter)->m_LANG_determined_type.value();

                return create_dictionary_type(key_type, value_type);
            }
            else if (symbol == m_mapping)
            {
                if (template_arguments.size() != 2)
                {
                    lex.lang_error(lexer::errorlevel::error, type_holder,
                        WO_ERR_UNEXPECTED_TEMPLATE_COUNT,
                        (size_t)2);

                    return std::nullopt;
                }
                auto template_iter = template_arguments.begin();
                auto* key_type = (*(template_iter++))->m_LANG_determined_type.value();
                auto* value_type = (*template_iter)->m_LANG_determined_type.value();

                return create_mapping_type(key_type, value_type);
            }
            else if (symbol == m_array)
            {
                if (template_arguments.size() != 1)
                {
                    lex.lang_error(lexer::errorlevel::error, type_holder,
                        WO_ERR_UNEXPECTED_TEMPLATE_COUNT,
                        (size_t)1);

                    return std::nullopt;
                }
                auto* element_type = template_arguments.front()->m_LANG_determined_type.value();
                return create_array_type(element_type);
            }
            else if (symbol == m_vector)
            {
                if (template_arguments.size() != 1)
                {
                    lex.lang_error(lexer::errorlevel::error, type_holder,
                        WO_ERR_UNEXPECTED_TEMPLATE_COUNT,
                        (size_t)1);

                    return std::nullopt;
                }
                auto* element_type = template_arguments.front()->m_LANG_determined_type.value();
                return create_vector_type(element_type);
            }
            else
            {
                lex.lang_error(lexer::errorlevel::error, type_holder,
                    WO_ERR_CANNOT_USE_BUILTIN_TYPENAME_HERE,
                    ctx->get_symbol_name_w(symbol));

                return std::nullopt;
            }
        }
        case ast::AstTypeHolder::FUNCTION:
        {
            std::list<lang_TypeInstance*> param_types;
            for (auto* type_holder : type_holder->m_typeform.m_function.m_parameters)
                param_types.push_back(type_holder->m_LANG_determined_type.value());

            return create_function_type(
                type_holder->m_typeform.m_function.m_is_variadic,
                param_types,
                type_holder->m_typeform.m_function.m_return_type->m_LANG_determined_type.value());
        }
        case ast::AstTypeHolder::STRUCTURE:
        {
            std::list<std::tuple<ast::AstDeclareAttribue::accessc_attrib, wo_pstring_t, lang_TypeInstance*>> param_types;
            for (auto* field : type_holder->m_typeform.m_structure.m_fields)
                param_types.push_back(std::make_tuple(
                    field->m_attribute.value_or(ast::AstDeclareAttribue::accessc_attrib::PRIVATE),
                    field->m_name,
                    field->m_type->m_LANG_determined_type.value()));

            return create_struct_type(param_types);
        }
        case ast::AstTypeHolder::UNION:
        {
            std::list<std::pair<wo_pstring_t, std::optional<lang_TypeInstance*>>> member_types;
            for (auto& field : type_holder->m_typeform.m_union.m_fields)
            {
                member_types.push_back(std::make_pair(
                    field.m_label,
                    field.m_item
                    ? std::optional(field.m_item.value()->m_LANG_determined_type.value())
                    : std::nullopt));
            }
            return create_union_type(member_types);
        }
        case ast::AstTypeHolder::TUPLE:
        {
            std::list<lang_TypeInstance*> element_types;
            for (auto* type_holder : type_holder->m_typeform.m_tuple.m_fields)
                element_types.push_back(type_holder->m_LANG_determined_type.value());

            return create_tuple_type(element_types);
        }
        default:
            wo_error("Unexpected type holder formal.");
        }
        return std::nullopt;
    }

    //////////////////////////////////////

    LangContext::LangContext()
        : m_root_namespace(std::make_unique<lang_Namespace>(WO_PSTR(EMPTY), std::nullopt)), m_created_symbol_edge(0)
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

#ifndef NDEBUG
            if (top_state.m_debug_entry_scope == nullptr)
            {
                top_state.m_debug_scope_layer_count = m_scope_stack.size();
                top_state.m_debug_entry_scope = get_current_scope();

                top_state.m_debug_ir_eval_content =
                    m_ircontext.m_eval_result_storage_target.size()
                    + m_ircontext.m_evaled_result_storage.size();

                if (!m_ircontext.m_eval_result_storage_target.empty())
                {
                    switch (m_ircontext.m_eval_result_storage_target.top().m_request)
                    {
                    case BytecodeGenerateContext::EvalResult::PUSH_RESULT_AND_IGNORE_RESULT:
                    case BytecodeGenerateContext::EvalResult::EVAL_PURE_ACTION:
                    case BytecodeGenerateContext::EvalResult::IGNORE_RESULT:
                        --top_state.m_debug_ir_eval_content;
                        break;
                    }
                }
            }
#endif

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
#ifndef NDEBUG
                wo_assert(top_state.m_debug_entry_scope != nullptr);
                wo_assert(top_state.m_debug_scope_layer_count == m_scope_stack.size());
                wo_assert(top_state.m_debug_entry_scope == get_current_scope());
                wo_assert(top_state.m_debug_ir_eval_content ==
                    m_ircontext.m_eval_result_storage_target.size()
                    + m_ircontext.m_evaled_result_storage.size());
#endif
                break;
            default:
                wo_error("Unexpected pass behavior.");
            }
        }
        return true;
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
                    std::nullopt,
                    WO_PSTR(_),
                    get_current_scope(),
                    lang_Symbol::kind::TYPE,
                    false)
                    .value();

                out_sni->m_type_instance = out_sni->m_symbol->m_type_instance;
                out_sni->m_type_instance->determine_base_type_move(
                    std::move(lang_TypeInstance::DeterminedType(basic_type, {})));
            };
        create_builtin_non_template_symbol_and_instance(
            &m_origin_types.m_void, WO_PSTR(void), lang_TypeInstance::DeterminedType::VOID);
        create_builtin_non_template_symbol_and_instance(
            &m_origin_types.m_nothing, WO_PSTR(nothing), lang_TypeInstance::DeterminedType::NOTHING);
        create_builtin_non_template_symbol_and_instance(
            &m_origin_types.m_dynamic, WO_PSTR(dynamic), lang_TypeInstance::DeterminedType::DYNAMIC);
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
        create_builtin_non_template_symbol_and_instance(
            &m_origin_types.m_gchandle, WO_PSTR(gchandle), lang_TypeInstance::DeterminedType::GCHANDLE);
        create_builtin_non_template_symbol_and_instance(
            &m_origin_types.m_char, WO_PSTR(char), lang_TypeInstance::DeterminedType::INTEGER);

        // Declare array/vec/... type symbol.
        auto create_builtin_complex_symbol_and_instance = [this](
            lang_Symbol** out_symbol,
            wo_pstring_t name)
            {
                ast::AstDeclareAttribue* built_type_public_attrib = new ast::AstDeclareAttribue();
                built_type_public_attrib->m_access = ast::AstDeclareAttribue::PUBLIC;

                *out_symbol = define_symbol_in_current_scope(
                    name,
                    built_type_public_attrib,
                    std::nullopt,
                    WO_PSTR(_),
                    get_current_scope(),
                    lang_Symbol::kind::TYPE,
                    false)
                    .value();
                (*out_symbol)->m_is_builtin = true;
            };

        create_builtin_complex_symbol_and_instance(&m_origin_types.m_dictionary, WO_PSTR(dict));
        create_builtin_complex_symbol_and_instance(&m_origin_types.m_mapping, WO_PSTR(map));
        create_builtin_complex_symbol_and_instance(&m_origin_types.m_vector, WO_PSTR(vec));
        create_builtin_complex_symbol_and_instance(&m_origin_types.m_array, WO_PSTR(array));
        create_builtin_complex_symbol_and_instance(&m_origin_types.m_tuple, WO_PSTR(tuple));
        create_builtin_complex_symbol_and_instance(&m_origin_types.m_function, WO_PSTR(function));
        create_builtin_complex_symbol_and_instance(&m_origin_types.m_struct, WO_PSTR(struct));
        create_builtin_complex_symbol_and_instance(&m_origin_types.m_union, WO_PSTR(union));

        m_origin_types.m_array_dynamic =
            m_origin_types.create_array_type(m_origin_types.m_dynamic.m_type_instance);
        m_origin_types.m_dictionary_dynamic =
            m_origin_types.create_dictionary_type(m_origin_types.m_dynamic.m_type_instance, m_origin_types.m_dynamic.m_type_instance);
    }

    int32_t _assign_storage_for_instance(
        LangContext* lctx,
        const std::string& funcname,
        lang_ValueInstance* value_instance,
        int32_t offset)
    {
        bool alligned = false;
        if (value_instance->IR_need_storage())
        {
            if (!value_instance->m_IR_storage.has_value())
            {
                value_instance->m_IR_storage =
                    lang_ValueInstance::Storage{
                        lang_ValueInstance::Storage::STACKOFFSET,
                        offset,
                };

                alligned = true;
            }

            wo_assert(value_instance->m_IR_storage.value().m_type
                == lang_ValueInstance::Storage::STACKOFFSET);

            lctx->m_ircontext.c().pdb_info->add_func_variable(
                funcname,
                lctx->get_value_name_w(value_instance),
                value_instance->m_symbol->m_symbol_declare_ast.value()->source_location.begin_at.row,
                value_instance->m_IR_storage.value().m_index);
        }

        if (alligned)
            return offset - 1;

        // No need for storage.
        return offset;
    }
    bool _assign_storage_for_local_variable_instance(
        lexer& lex,
        LangContext* lctx,
        const std::string& funcname,
        lang_Scope* scope,
        int32_t stack_assign_offset,
        int32_t* out_max_stack_count)
    {
        int32_t next_assign_offset = stack_assign_offset;

        const auto it_sub_scope_end = scope->m_sub_scopes.end();
        auto next_it_sub_scope = scope->m_sub_scopes.begin();
        std::optional<size_t> next_it_sub_scope_head_edge =
            next_it_sub_scope == it_sub_scope_end ?
            std::nullopt :
            std::optional((*next_it_sub_scope)->m_visibility_from_edge_for_template_check);

        std::vector<lang_Symbol*> local_symbols_in_this_scope(
            scope->m_defined_symbols.size());

        lang_Symbol** applying_symbol_place = local_symbols_in_this_scope.data();
        for (auto& [_useless, symbol] : scope->m_defined_symbols)
        {
            (void)_useless;
            *(applying_symbol_place++) = symbol.get();
        }

        std::sort(
            local_symbols_in_this_scope.begin(),
            local_symbols_in_this_scope.end(),
            [](lang_Symbol* a, lang_Symbol* b)
            {
                return a->m_symbol_edge < b->m_symbol_edge;
            });

        bool donot_have_unused_variable = true;

        // Ok, let's rock!.
        for (auto* symbol : local_symbols_in_this_scope)
        {
            if (next_it_sub_scope_head_edge.has_value()
                && symbol->m_symbol_edge > next_it_sub_scope_head_edge.value())
            {
                auto* next_it_sub_scope_instance = next_it_sub_scope->get();

                if (!next_it_sub_scope_instance->m_function_instance.has_value())
                    donot_have_unused_variable =
                    _assign_storage_for_local_variable_instance(
                        lex, lctx, funcname, next_it_sub_scope_instance, next_assign_offset, out_max_stack_count)
                    && donot_have_unused_variable;

                if (++next_it_sub_scope != it_sub_scope_end)
                    next_it_sub_scope_head_edge = (*next_it_sub_scope)->m_visibility_from_edge_for_template_check;
                else
                    next_it_sub_scope_head_edge = std::nullopt;
            }

            if (symbol->m_symbol_kind != lang_Symbol::kind::VARIABLE)
                continue;

            bool is_static_storage = (symbol->m_declare_attribute.has_value()
                && symbol->m_declare_attribute.value()->m_lifecycle.has_value()
                && symbol->m_declare_attribute.value()->m_lifecycle.value()
                == ast::AstDeclareAttribue::lifecycle_attrib::STATIC);

            // Symbol defined in function local cannot be global.
            wo_assert(!symbol->m_is_global);

            if (symbol->m_is_template)
            {
                for (auto& [_useless, template_instance] : symbol->m_template_value_instances->m_template_instances)
                {
                    (void)_useless;
                    if (template_instance->m_state == lang_TemplateAstEvalStateValue::FAILED)
                        continue; // Skip failed template instance.

                    wo_assert(template_instance->m_state == lang_TemplateAstEvalStateValue::EVALUATED);
                    if (is_static_storage)
                        lctx->update_allocate_global_instance_storage_passir(
                            template_instance->m_value_instance.get());
                    else
                        next_assign_offset =
                        _assign_storage_for_instance(
                            lctx,
                            funcname,
                            template_instance->m_value_instance.get(),
                            next_assign_offset);
                }
            }
            else
            {
                if (is_static_storage)
                    lctx->update_allocate_global_instance_storage_passir(
                        symbol->m_value_instance);
                else
                    next_assign_offset =
                    _assign_storage_for_instance(
                        lctx,
                        funcname,
                        symbol->m_value_instance,
                        next_assign_offset);
            }

            if (!symbol->m_has_been_used)
            {
                lex.lang_error(lexer::errorlevel::error,
                    symbol->m_symbol_declare_ast.value(),
                    WO_ERR_UNSED_VARIABLE,
                    lctx->get_symbol_name_w(symbol));

                donot_have_unused_variable = false;
            }
        }

        // Ok, finish sub scopes;
        while (next_it_sub_scope != it_sub_scope_end)
        {
            auto* next_it_sub_scope_instance = next_it_sub_scope->get();

            if (!next_it_sub_scope_instance->m_function_instance.has_value())
                donot_have_unused_variable =
                _assign_storage_for_local_variable_instance(
                    lex, lctx, funcname, (*next_it_sub_scope).get(), next_assign_offset, out_max_stack_count)
                && donot_have_unused_variable;

            ++next_it_sub_scope;
        }

        *out_max_stack_count = std::max(
            *out_max_stack_count, -next_assign_offset);

        return donot_have_unused_variable;
    }

    std::string get_anonymous_function_name(LangContext* lctx, ast::AstValueFunction* anonymous_func)
    {
        if (anonymous_func->m_LANG_value_instance_to_update.has_value())
        {
            lang_ValueInstance* value_instance = anonymous_func->m_LANG_value_instance_to_update.value();
            std::string name = lctx->get_value_name(value_instance);

            lang_Scope* function_located_scope =
                anonymous_func->m_LANG_function_scope.value()->m_parent_scope.value();

            if (!function_located_scope->is_namespace_scope())
            {
                char local_name[32];
                sprintf(local_name, "[local_%p]", function_located_scope);

                auto function = lctx->get_scope_located_function(function_located_scope);
                if (function.has_value())
                    name =
                    get_anonymous_function_name(lctx, function.value())
                    + "::"
                    + local_name
                    + "::"
                    + name;
            }

            return name;
        }

        std::string result;

        lang_Scope* function_located_scope =
            anonymous_func->m_LANG_function_scope.value()->m_parent_scope.value();

        auto function = lctx->get_scope_located_function(function_located_scope);
        if (function.has_value())
            result += get_anonymous_function_name(lctx, function.value()) + "::";

        char anonymous_name[32];
        sprintf(anonymous_name, "[anonymous_%p]", anonymous_func);
        result += anonymous_name;

        return result;
    }

    bool LangContext::process(lexer& lex, ast::AstBase* root)
    {
        pass_0_5_register_builtin_types();

        if (!anylize_pass(lex, root, &LangContext::pass_0_process_scope_and_non_local_defination))
            return false;

        if (!anylize_pass(lex, root, &LangContext::pass_1_process_basic_type_marking_and_constant_eval))
            return false;

        // Final process, generate bytecode.

        // Do something for prepare.
        // final.1 Finalize global codes.
        size_t global_block_begin_place = m_ircontext.c().get_now_ip();
        auto global_reserving_ip = m_ircontext.c().reserved_stackvalue();

        if (!anylize_pass(lex, root, &LangContext::pass_final_A_process_bytecode_generation))
            return false;

        // All temporary registers should be released.
        wo_assert(m_ircontext.m_inused_temporary_registers.empty());
        wo_assert(m_ircontext.m_evaled_result_storage.empty());
        wo_assert(m_ircontext.m_eval_result_storage_target.empty());
        wo_assert(m_ircontext.m_loop_content_stack.empty());

        auto used_tmp_regs = m_ircontext.c().update_all_temp_regist_to_stack(
            &m_ircontext, global_block_begin_place);
        m_ircontext.c().reserved_stackvalue(
            global_reserving_ip,
            used_tmp_regs); // set reserved size

        m_ircontext.c().jmp(opnum::tag("#woolang_program_end"));

        // final.2 Finalize function codes.
        bool donot_have_unused_local_variable = true;
        for (;;)
        {
            auto functions_to_finalize =
                std::move(m_ircontext.m_being_used_function_instance);
            m_ircontext.m_being_used_function_instance.clear();

            bool has_function_to_be_eval = false;
            for (ast::AstValueFunction* eval_function : functions_to_finalize)
            {
                if (!m_ircontext.m_processed_function_instance.insert(
                    eval_function).second)
                    // Already processed.
                    continue;

                has_function_to_be_eval = true;

                m_ircontext.c().ext_funcbegin();

                // Bind function label.
                m_ircontext.c().tag(IR_function_label(eval_function));

                // Trying to register extern symbol.
                std::string eval_fucntion_name = get_anonymous_function_name(this, eval_function);
                m_ircontext.c().pdb_info->generate_func_begin(
                    eval_fucntion_name,
                    eval_function,
                    &m_ircontext.c());

                if (eval_function->m_LANG_value_instance_to_update.has_value())
                {
                    lang_ValueInstance* value_instance = eval_function->m_LANG_value_instance_to_update.value();
                    lang_Symbol* symbol = value_instance->m_symbol;

                    if (symbol->m_declare_attribute.has_value())
                    {
                        ast::AstDeclareAttribue* declare_attribute = symbol->m_declare_attribute.value();
                        if (declare_attribute->m_external.has_value()
                            && declare_attribute->m_external.value() == ast::AstDeclareAttribue::external_attrib::EXTERNAL)
                        {
                            m_ircontext.c().record_extern_script_function(
                                eval_fucntion_name);
                        }
                    }
                }

                size_t this_function_block_begin_place = m_ircontext.c().get_now_ip();
                auto this_function_reserving_ip = m_ircontext.c().reserved_stackvalue();

                /////////////////////////////////

                // 0. Addressing for simple parameter settings
                int32_t argument_place = 2;
                for (auto& [_useless, captured_variable_instance] :
                    eval_function->m_LANG_captured_context.m_captured_variables)
                {
                    captured_variable_instance.m_instance->m_IR_storage =
                        lang_ValueInstance::Storage{
                             lang_ValueInstance::Storage::STACKOFFSET,
                             argument_place,
                    };

                    m_ircontext.c().pdb_info->add_func_variable(
                        eval_fucntion_name,
                        get_value_name_w(captured_variable_instance.m_instance.get()),
                        eval_function->source_location.begin_at.row,
                        argument_place);

                    ++argument_place;
                }
                const auto no_captured_arguement_place = argument_place;
                for (auto* param_decls : eval_function->m_parameters)
                {
                    if (param_decls->m_pattern->node_type == ast::AstBase::AST_PATTERN_SINGLE)
                    {
                        ast::AstPatternSingle* pattern_single =
                            static_cast<ast::AstPatternSingle*>(param_decls->m_pattern);

                        if (!pattern_single->m_is_mutable)
                        {
                            // Immutable pattern single argument.
                            lang_Symbol* symbol = pattern_single->m_LANG_declared_symbol.value();

                            wo_assert(!symbol->m_is_template && symbol->m_symbol_kind == lang_Symbol::kind::VARIABLE);
                            symbol->m_value_instance->m_IR_storage =
                                lang_ValueInstance::Storage{
                                     lang_ValueInstance::Storage::STACKOFFSET,
                                     argument_place,
                            };

                            m_ircontext.c().pdb_info->add_func_variable(
                                eval_fucntion_name,
                                get_value_name_w(symbol->m_value_instance),
                                pattern_single->source_location.begin_at.row,
                                argument_place);
                        }
                    }
                    ++argument_place;
                }
                // 1. Addressing for local variable settings
                lang_Scope* function_scope = eval_function->m_LANG_function_scope.value();

                int32_t local_storage_size = 0;

                bool this_function_dont_have_unused_local_variable =
                    _assign_storage_for_local_variable_instance(
                        lex, this, eval_fucntion_name, eval_function->m_LANG_function_scope.value(), 0, &local_storage_size);

                if (!this_function_dont_have_unused_local_variable)
                {
                    lex.lang_error(lexer::errorlevel::infom, eval_function,
                        WO_INFO_IN_FUNCTION_NAMED,
                        str_to_wstr(eval_fucntion_name).c_str());
                }

                donot_have_unused_local_variable =
                    this_function_dont_have_unused_local_variable
                    && donot_have_unused_local_variable;

                // 2. Assign value arguments.
                argument_place = no_captured_arguement_place;
                for (auto* param_decls : eval_function->m_parameters)
                {
                    if (param_decls->m_pattern->node_type != ast::AstBase::AST_PATTERN_SINGLE
                        || static_cast<ast::AstPatternSingle*>(param_decls->m_pattern)->m_is_mutable)
                    {
                        // Need assign value.
                        opnum::opnumbase* argument_opnum;
                        if (argument_place <= 63)
                            argument_opnum = m_ircontext.opnum_stack_offset(argument_place);
                        else
                        {
                            argument_opnum = m_ircontext.borrow_opnum_temporary_register(
                                WO_BORROW_TEMPORARY_FROM(nullptr)
                            );
                            m_ircontext.c().lds(
                                *(opnum::opnumbase*)argument_opnum,
                                *(opnum::opnumbase*)m_ircontext.opnum_imm_int(argument_place));
                        }

                        update_pattern_storage_and_code_gen_passir(
                            lex, param_decls->m_pattern, argument_opnum, std::nullopt);

                        m_ircontext.try_return_opnum_temporary_register(argument_opnum);
                    }
                    ++argument_place;
                }

                // 3. If function is variadic, move tc to tp;
                if (eval_function->m_is_variadic)
                {
                    m_ircontext.c().psh(
                        *(opnum::opnumbase*)m_ircontext.opnum_spreg(opnum::reg::spreg::tp));
                    m_ircontext.c().mov(
                        *(opnum::opnumbase*)m_ircontext.opnum_spreg(opnum::reg::spreg::tp),
                        *(opnum::opnumbase*)m_ircontext.opnum_spreg(opnum::reg::spreg::tc));
                }

                // 4. Generate for function body.
                if (!anylize_pass(
                    lex,
                    eval_function->m_body,
                    &LangContext::pass_final_A_process_bytecode_generation))
                    return false;

                /////////////////////////////////

                if (immutable_type(eval_function->m_LANG_determined_return_type.value())
                    != m_origin_types.m_void.m_type_instance)
                {
                    m_ircontext.c().ext_panic(
                        opnum::imm_str("The function should have returned `"
                            + std::string(get_type_name(eval_function->m_LANG_determined_return_type.value()))
                            + "`, but ended without providing a return value"));
                }

                if (eval_function->m_is_variadic)
                {
                    m_ircontext.c().tag(IR_function_label(eval_function) + "_ret");
                    m_ircontext.c().pop(
                        *(opnum::opnumbase*)m_ircontext.opnum_spreg(opnum::reg::spreg::tp));
                }

                if (eval_function->m_LANG_captured_context.m_captured_variables.empty())
                    m_ircontext.c().ret();
                else
                    m_ircontext.c().ret(
                        (uint16_t)eval_function->m_LANG_captured_context.m_captured_variables.size());

                auto this_function_used_tmp_regs =
                    m_ircontext.c().update_all_temp_regist_to_stack(
                        &m_ircontext, this_function_block_begin_place);
                m_ircontext.c().reserved_stackvalue(
                    this_function_reserving_ip,
                    this_function_used_tmp_regs + local_storage_size); // set reserved size

                m_ircontext.c().pdb_info->generate_func_end(
                    eval_fucntion_name, this_function_used_tmp_regs, &m_ircontext.c());

                m_ircontext.c().pdb_info->update_func_variable(
                    eval_fucntion_name, -(wo_integer_t)this_function_used_tmp_regs);

                m_ircontext.c().ext_funcend();

                wo_assert(m_ircontext.m_inused_temporary_registers.empty());
                wo_assert(m_ircontext.m_evaled_result_storage.empty());
                wo_assert(m_ircontext.m_eval_result_storage_target.empty());
                wo_assert(m_ircontext.m_loop_content_stack.empty());
            }

            if (!has_function_to_be_eval)
                break;
        }

        m_ircontext.c().tag("#woolang_program_end");
        m_ircontext.c().end();

        m_ircontext.c().loaded_libs = m_ircontext.m_extern_libs;

        return donot_have_unused_local_variable;
    }

    ////////////////////////

    void LangContext::begin_new_function(ast::AstValueFunction* func_instance)
    {
        begin_new_scope();
        get_current_scope()->m_function_instance = func_instance;
    }
    void LangContext::end_last_function()
    {
        wo_assert(get_current_scope()->m_function_instance);
        end_last_scope();
    }
    void LangContext::begin_new_scope()
    {
        lang_Scope* current_scope = get_current_scope();
        auto new_scope = std::make_unique<lang_Scope>(
            current_scope, get_current_scope()->m_belongs_to_namespace);

        new_scope->m_visibility_from_edge_for_template_check =
            m_created_symbol_edge;

        entry_spcify_scope(new_scope.get());

        current_scope->m_sub_scopes.emplace_back(std::move(new_scope));
    }
    void LangContext::entry_spcify_scope(lang_Scope* scope)
    {
        m_scope_stack.push(scope);
    }
    void LangContext::end_last_scope()
    {
        m_scope_stack.pop();
        wo_assert(!m_scope_stack.empty());
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
                name, std::make_optional(get_current_namespace()));

            new_namespace->m_this_scope->m_visibility_from_edge_for_template_check =
                m_created_symbol_edge;

            auto* new_namespace_ptr = get_current_namespace()->m_sub_namespaces.insert(
                std::make_pair(name, std::move(new_namespace)))
                .first->second.get();

            m_scope_stack.push(new_namespace_ptr->m_this_scope.get());
        }
        else
        {
            m_scope_stack.push(fnd->second->m_this_scope.get());
        }
        return true;
    }
    void LangContext::entry_spcify_namespace(lang_Namespace* namespace_)
    {
        entry_spcify_scope(namespace_->m_this_scope.get());
    }
    void LangContext::end_last_namespace()
    {
        wo_assert(get_current_scope() ==
            get_current_namespace()->m_this_scope.get());

        end_last_scope();
    }
    lang_Scope* LangContext::get_current_scope()
    {
        return m_scope_stack.top();
    }
    lang_Namespace* LangContext::get_current_namespace()
    {
        return get_current_scope()->m_belongs_to_namespace;
    }
    std::optional<ast::AstValueFunction*> LangContext::get_scope_located_function(lang_Scope* scope)
    {
        std::optional<lang_Scope*> current_scope = scope;
        while (current_scope)
        {
            auto* current_scope_p = current_scope.value();
            current_scope = current_scope_p->m_parent_scope;

            if (current_scope_p->m_function_instance)
                return current_scope_p->m_function_instance;
        }
        return std::nullopt;
    }
    std::optional<ast::AstValueFunction*> LangContext::get_current_function()
    {
        return get_scope_located_function(get_current_scope());
    }
    std::optional<lang_Symbol*>
        LangContext::_search_symbol_from_current_scope(
            lexer& lex, ast::AstIdentifier* ident, const std::optional<bool*>& out_ambig)
    {
        wo_assert(!out_ambig.has_value() || *out_ambig.value() == false);
        wo_assert(ident->source_location.source_file != nullptr);

        std::unordered_set<lang_Namespace*> searching_namesapaces;
        std::unordered_set<lang_Symbol*> found_symbol;

        lang_Scope* current_scope = get_current_scope();

        // Collect used namespaces;
        for (lang_Scope* collecting_scope = current_scope;;)
        {
            if (collecting_scope->is_namespace_scope())
            {
                for (lang_Namespace* symbol_namespace = collecting_scope->m_belongs_to_namespace;;)
                {
                    auto fnd = symbol_namespace->m_declare_used_namespaces.find(
                        ident->source_location.source_file);

                    if (fnd != symbol_namespace->m_declare_used_namespaces.end())
                        searching_namesapaces.insert(fnd->second.begin(), fnd->second.end());

                    if (symbol_namespace->m_parent_namespace)
                        symbol_namespace = symbol_namespace->m_parent_namespace.value();
                    else
                        break;
                }
                break;
            }
            else
            {
                searching_namesapaces.insert(
                    collecting_scope->m_declare_used_namespaces.begin(),
                    collecting_scope->m_declare_used_namespaces.end());
                collecting_scope = collecting_scope->m_parent_scope.value();
            }
        }

        if (ident->m_scope.empty())
        {
            // Can see all the symbol defined in current scope;
            size_t visibility_edge_limit = m_created_symbol_edge;
            do
            {
                auto fnd = current_scope->m_defined_symbols.find(ident->m_name);
                if (fnd != current_scope->m_defined_symbols.end())
                {
                    if ((fnd->second->m_symbol_edge <= visibility_edge_limit || fnd->second->m_is_global)
                        && ident->m_find_type_only != (fnd->second->m_symbol_kind == lang_Symbol::kind::VARIABLE))
                    {
                        found_symbol.insert(fnd->second.get());
                        break;
                    }
                }

                if (current_scope->m_parent_scope)
                {
                    visibility_edge_limit = std::min(
                        visibility_edge_limit,
                        current_scope->m_visibility_from_edge_for_template_check);

                    current_scope = current_scope->m_parent_scope.value();
                }
                else
                    break;
            } while (true);
        }
        else
        {
            lang_Namespace* searching_namesapace = get_current_namespace();;
            for (;;)
            {
                lang_Namespace* symbol_namespace = searching_namesapace;
                for (auto* scope : ident->m_scope)
                {
                    auto fnd = symbol_namespace->m_sub_namespaces.find(scope);
                    if (fnd != symbol_namespace->m_sub_namespaces.end())
                        symbol_namespace = fnd->second.get();
                    else
                        goto _label_try_upper_namespace;
                }

                // Ok, found the namespace.
                if (auto fnd = symbol_namespace->m_this_scope->m_defined_symbols.find(ident->m_name);
                    fnd != symbol_namespace->m_this_scope->m_defined_symbols.end()
                    && ident->m_find_type_only != (fnd->second->m_symbol_kind == lang_Symbol::kind::VARIABLE))
                {
                    found_symbol.insert(fnd->second.get());
                    break; // Break for continue outside loop
                }

            _label_try_upper_namespace:
                if (searching_namesapace->m_parent_namespace)
                    searching_namesapace = searching_namesapace->m_parent_namespace.value();
                else
                    break; // Break for continue outside loop
            }
        }

        for (auto* searching_namesapace : searching_namesapaces)
        {
            bool scope_located = true;
            for (auto* scope : ident->m_scope)
            {
                auto fnd = searching_namesapace->m_sub_namespaces.find(scope);
                if (fnd != searching_namesapace->m_sub_namespaces.end())
                    searching_namesapace = fnd->second.get();
                else
                {
                    scope_located = false;
                    break;
                }
            }

            if (scope_located)
            {
                // Ok, found the namespace.
                if (auto fnd = searching_namesapace->m_this_scope->m_defined_symbols.find(ident->m_name);
                    fnd != searching_namesapace->m_this_scope->m_defined_symbols.end()
                    && ident->m_find_type_only != (fnd->second->m_symbol_kind == lang_Symbol::kind::VARIABLE))
                    found_symbol.insert(fnd->second.get());
            }
        }

        if (found_symbol.empty())
            return std::nullopt;

        lang_Symbol* result = *found_symbol.begin();
        if (found_symbol.size() > 1)
        {
            if (!out_ambig.has_value())
                return std::nullopt;

            lex.lang_error(lexer::errorlevel::error, ident,
                WO_ERR_AMBIGUOUS_TARGET_NAMED,
                ident->m_name->c_str(),
                get_symbol_name_w(result));

            for (auto* symbol : found_symbol)
            {
                if (symbol->m_symbol_declare_ast.has_value())
                    lex.lang_error(lexer::errorlevel::infom,
                        symbol->m_symbol_declare_ast.value(),
                        WO_INFO_MAYBE_NAMED_DEFINED_HERE,
                        get_symbol_name_w(symbol));
                else
                    lex.lang_error(lexer::errorlevel::infom,
                        ident,
                        WO_INFO_MAYBE_NAMED_DEFINED_IN_COMPILER,
                        get_symbol_name_w(symbol));
            }
            *out_ambig.value() = true;
        }
        return result;
    }
    std::optional<lang_Symbol*>
        LangContext::find_symbol_in_current_scope(
            lexer& lex, ast::AstIdentifier* ident, const std::optional<bool*>& out_ambig)
    {
        if (out_ambig.has_value())
            *out_ambig.value() = false;

        if (ident->m_LANG_determined_symbol)
            return ident->m_LANG_determined_symbol;

        lang_Namespace* search_begin_namespace;
        switch (ident->m_formal)
        {
        case ast::AstIdentifier::FROM_GLOBAL:
            search_begin_namespace = m_root_namespace.get();
            break;
        case ast::AstIdentifier::FROM_TYPE:
        {
            lang_TypeInstance* type_instance;

            if (ast::AstTypeHolder** from_type = std::get_if<ast::AstTypeHolder*>(&ident->m_from_type.value());
                from_type != nullptr)
            {
                auto determined_type = (*from_type)->m_LANG_determined_type;
                if (!determined_type)
                {
                    lex.lang_error(lexer::errorlevel::error, *from_type, WO_ERR_UNKNOWN_TYPE);
                    return std::nullopt;
                }
                type_instance = determined_type.value();
            }
            else
                type_instance = std::get<lang_TypeInstance*>(ident->m_from_type.value());

            lang_Scope* search_from_scope = type_instance->m_symbol->m_belongs_to_scope;
            if (!type_instance->m_symbol->m_belongs_to_scope->is_namespace_scope())
                return std::nullopt;

            search_begin_namespace =
                type_instance->m_symbol->m_belongs_to_scope->m_belongs_to_namespace;

            auto fnd = search_begin_namespace->m_sub_namespaces.find(type_instance->m_symbol->m_name);
            if (fnd == search_begin_namespace->m_sub_namespaces.end())
                return std::nullopt; // Namespace not defined.

            search_begin_namespace = fnd->second.get();
            break;
        }
        case ast::AstIdentifier::FROM_CURRENT:
        {
            auto found_symbol = _search_symbol_from_current_scope(lex, ident, out_ambig);
            if (found_symbol)
                ident->m_LANG_determined_symbol = found_symbol.value();
            return found_symbol;
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

            if (ident->m_find_type_only != (symbol->m_symbol_kind == lang_Symbol::kind::VARIABLE))
                ident->m_LANG_determined_symbol = symbol;

            return symbol;
        }
        return std::nullopt;
    }

    lang_TypeInstance* LangContext::mutable_type(lang_TypeInstance* origin_type)
    {
        if (origin_type->is_immutable())
        {
            // Is immutable type.
            auto fnd = m_mutable_type_instance_cache.find(origin_type);
            if (fnd != m_mutable_type_instance_cache.end())
                return fnd->second.get();

            auto mutable_type_instance = std::make_unique<lang_TypeInstance>(
                origin_type->m_symbol, std::nullopt);
            mutable_type_instance->m_determined_base_type_or_mutable = origin_type;

            auto* result = mutable_type_instance.get();

            m_mutable_type_instance_cache.insert(
                std::make_pair(origin_type, std::move(mutable_type_instance)));

            return result;
        }
        return origin_type;
    }
    lang_TypeInstance* LangContext::immutable_type(lang_TypeInstance* origin_type)
    {
        lang_TypeInstance** immutable_type =
            std::get_if<lang_TypeInstance*>(&origin_type->m_determined_base_type_or_mutable);

        if (immutable_type != nullptr)
            return *immutable_type;

        return origin_type;
    }

    void LangContext::fast_create_one_template_type_alias_in_current_scope(
        wo_pstring_t source_location,
        wo_pstring_t template_param,
        lang_TypeInstance* template_arg)
    {
        lang_Symbol* symbol = define_symbol_in_current_scope(
            template_param,
            std::nullopt,
            std::nullopt,
            source_location,
            get_current_scope(),
            lang_Symbol::kind::ALIAS,
            false).value();

        symbol->m_alias_instance->m_determined_type = template_arg;
    }
    void LangContext::fast_create_template_type_alias_in_current_scope(
        wo_pstring_t source_location,
        const std::list<wo_pstring_t>& template_params,
        const std::list<lang_TypeInstance*>& template_args)
    {
        wo_assert(template_params.size() == template_args.size());

        auto params_iter = template_params.begin();
        auto args_iter = template_args.begin();
        auto params_end = template_params.end();

        for (; params_iter != params_end; ++params_iter, ++args_iter)
        {
            fast_create_one_template_type_alias_in_current_scope(
                source_location,
                *params_iter,
                *args_iter);
        }
    }
    std::wstring LangContext::_get_scope_name(lang_Scope* scope)
    {
        std::wstring result = {};
        auto* belong_namesapce = scope->m_belongs_to_namespace;

        if (belong_namesapce->m_this_scope.get() == scope)
        {
            for (;;)
            {
                auto* space_name = belong_namesapce->m_name;

                if (belong_namesapce->m_parent_namespace)
                    belong_namesapce = belong_namesapce->m_parent_namespace.value();
                else
                    break;

                result = *space_name + L"::" + result;
            }
        }
        return result;
    }
    std::wstring LangContext::_get_symbol_name(lang_Symbol* symbol)
    {
        return _get_scope_name(symbol->m_belongs_to_scope) + *symbol->m_name;
    }
    std::wstring LangContext::_get_type_name(lang_TypeInstance* type)
    {
        std::wstring result_type_name;

        if (type->is_mutable())
            result_type_name += L"mut ";

        auto* immutable_type_instance = immutable_type(type);

        if (immutable_type_instance->m_symbol->m_is_builtin)
        {
            auto* base_determined_type = immutable_type_instance->get_determined_type().value();
            switch (base_determined_type->m_base_type)
            {
            case lang_TypeInstance::DeterminedType::ARRAY:
                result_type_name += L"array";
                break;
            case lang_TypeInstance::DeterminedType::VECTOR:
                result_type_name += L"vec";
                break;
            case lang_TypeInstance::DeterminedType::DICTIONARY:
                result_type_name += L"dict";
                break;
            case lang_TypeInstance::DeterminedType::MAPPING:
                result_type_name += L"map";
                break;
            case lang_TypeInstance::DeterminedType::TUPLE:
            {
                result_type_name += L"(";
                auto& element_types = base_determined_type->m_external_type_description.m_tuple->m_element_types;
                bool first = true;
                for (auto* element_type : element_types)
                {
                    if (!first)
                        result_type_name += L", ";

                    result_type_name += get_type_name_w(element_type);
                    first = false;
                }
                result_type_name += L")";
                break;
            }
            case lang_TypeInstance::DeterminedType::FUNCTION:
            {
                result_type_name += L"(";
                auto& param_types = base_determined_type->m_external_type_description.m_function->m_param_types;
                bool first = true;
                for (auto* param_type : param_types)
                {
                    if (!first)
                        result_type_name += L", ";

                    result_type_name += get_type_name_w(param_type);
                    first = false;
                }
                if (base_determined_type->m_external_type_description.m_function->m_is_variadic)
                {
                    if (!first)
                        result_type_name += L", ";
                    result_type_name += L"...";
                }
                result_type_name += L")=> ";
                result_type_name += get_type_name_w(
                    base_determined_type->m_external_type_description.m_function->m_return_type);
                break;
            }
            case lang_TypeInstance::DeterminedType::STRUCT:
            {
                result_type_name += L"struct{";
                auto& member_types = base_determined_type->m_external_type_description.m_struct->m_member_types;
                bool first = true;
                for (auto& [name, field] : member_types)
                {
                    if (!first)
                        result_type_name += L", ";

                    result_type_name += *name;
                    result_type_name += L": ";
                    result_type_name += get_type_name_w(field.m_member_type);
                    first = false;
                }
                result_type_name += L"}";
                break;
            }
            case lang_TypeInstance::DeterminedType::UNION:
            {
                result_type_name += L"union{";
                auto& member_types = base_determined_type->m_external_type_description.m_union->m_union_label;
                bool first = true;
                for (auto& [name, field] : member_types)
                {
                    if (!first)
                        result_type_name += L", ";

                    result_type_name += *name;
                    if (field.m_item_type)
                    {
                        result_type_name += L"(";
                        result_type_name += get_type_name_w(field.m_item_type.value());
                        result_type_name += L")";
                    }
                    first = false;
                }
                result_type_name += L"}";
                break;
            }
            default:
                wo_error("Unexpected builtin type.");
            }
        }
        else
        {
            // Get symbol name directly.
            result_type_name += get_symbol_name_w(immutable_type_instance->m_symbol);
        }

        if (immutable_type_instance->m_instance_template_arguments)
        {
            result_type_name += L"<";
            auto& template_args = immutable_type_instance->m_instance_template_arguments.value();
            bool first = true;
            for (auto* template_arg : template_args)
            {
                if (!first)
                    result_type_name += L", ";
                result_type_name += get_type_name_w(template_arg);
                first = false;
            }
            result_type_name += L">";
        }

        return result_type_name;
    }
    std::wstring LangContext::_get_value_name(lang_ValueInstance* scope)
    {
        std::wstring result_value_name = get_symbol_name_w(scope->m_symbol);

        if (scope->m_instance_template_arguments)
        {
            result_value_name += L"<";
            auto& template_args = scope->m_instance_template_arguments.value();
            bool first = true;
            for (auto* template_arg : template_args)
            {
                if (!first)
                    result_value_name += L", ";
                result_value_name += get_type_name_w(template_arg);
                first = false;
            }
            result_value_name += L">";
        }

        return result_value_name;
    }

    const wchar_t* LangContext::get_symbol_name_w(lang_Symbol* symbol)
    {
        auto fnd = m_symbol_name_cache.find(symbol);
        if (fnd != m_symbol_name_cache.end())
            return fnd->second.first.c_str();

        std::wstring result = _get_symbol_name(symbol);
        return m_symbol_name_cache.insert(
            std::make_pair(symbol, std::make_pair(result, wstrn_to_str(result))))
            .first
            ->second.first.c_str();
    }
    const char* LangContext::get_symbol_name(lang_Symbol* symbol)
    {
        auto fnd = m_symbol_name_cache.find(symbol);
        if (fnd != m_symbol_name_cache.end())
            return fnd->second.second.c_str();

        std::wstring result = _get_symbol_name(symbol);
        return m_symbol_name_cache.insert(
            std::make_pair(symbol, std::make_pair(result, wstrn_to_str(result))))
            .first
            ->second.second.c_str();
    }
    const wchar_t* LangContext::get_type_name_w(lang_TypeInstance* type)
    {
        auto fnd = m_type_name_cache.find(type);
        if (fnd != m_type_name_cache.end())
            return fnd->second.first.c_str();

        m_type_name_cache[type] = std::make_pair(
            *type->m_symbol->m_name + L"...", "...");

        std::wstring result = _get_type_name(type);

        m_type_name_cache.erase(type);
        return m_type_name_cache.insert(
            std::make_pair(type, std::make_pair(result, wstrn_to_str(result))))
            .first
            ->second.first.c_str();
    }
    const char* LangContext::get_type_name(lang_TypeInstance* type)
    {
        auto fnd = m_type_name_cache.find(type);
        if (fnd != m_type_name_cache.end())
            return fnd->second.second.c_str();

        m_type_name_cache[type] = std::make_pair(
            *type->m_symbol->m_name + L"...", "...");

        std::wstring result = _get_type_name(type);

        m_type_name_cache.erase(type);
        return m_type_name_cache.insert(
            std::make_pair(type, std::make_pair(result, wstrn_to_str(result))))
            .first
            ->second.second.c_str();
    }
    const wchar_t* LangContext::get_value_name_w(lang_ValueInstance* val)
    {
        auto fnd = m_value_name_cache.find(val);
        if (fnd != m_value_name_cache.end())
            return fnd->second.first.c_str();

        std::wstring result = _get_value_name(val);
        return m_value_name_cache.insert(
            std::make_pair(val, std::make_pair(result, wstrn_to_str(result))))
            .first
            ->second.first.c_str();
    }
    const char* LangContext::get_value_name(lang_ValueInstance* val)
    {
        auto fnd = m_value_name_cache.find(val);
        if (fnd != m_value_name_cache.end())
            return fnd->second.second.c_str();

        std::wstring result = _get_value_name(val);
        return m_value_name_cache.insert(
            std::make_pair(val, std::make_pair(result, wstrn_to_str(result))))
            .first
            ->second.second.c_str();
    }
    void LangContext::append_using_namespace_for_current_scope(
        const std::unordered_set<lang_Namespace*>& using_namespaces, wo_pstring_t source)
    {
        lang_Scope* current_scope = get_current_scope();
        if (current_scope->is_namespace_scope())
        {
            auto& used_namespaces =
                current_scope->m_belongs_to_namespace->m_declare_used_namespaces[
                    source];

            used_namespaces.insert(using_namespaces.begin(), using_namespaces.end());
        }
        else
        {
            current_scope->m_declare_used_namespaces.insert(
                using_namespaces.begin(), using_namespaces.end());
        }
    }
    void LangContext::using_namespace_declare_for_current_scope(
        ast::AstUsingNamespace* using_namespace)
    {
        wo_assert(using_namespace->source_location.source_file != nullptr);

        std::unordered_set<lang_Namespace*> using_namespaces;

        lang_Namespace* current_namespace = get_current_namespace();
        for (;;)
        {
            // Find sub namespace;
            bool found = true;
            lang_Namespace* sub_namespace = current_namespace;
            for (wo_pstring_t name : using_namespace->m_using_namespace)
            {
                auto fnd = sub_namespace->m_sub_namespaces.find(name);
                if (fnd == sub_namespace->m_sub_namespaces.end())
                {
                    found = false;
                    break;
                }
                else
                    sub_namespace = fnd->second.get();
            }

            if (found)
                using_namespaces.insert(sub_namespace);

            if (current_namespace->m_parent_namespace.has_value())
                current_namespace = current_namespace->m_parent_namespace.value();
            else
                break;
        }

        append_using_namespace_for_current_scope(
            using_namespaces, using_namespace->source_location.source_file);
    }

    lang_ValueInstance* LangContext::check_and_update_captured_varibale_in_current_scope(
        ast::AstValueVariable* ref_from_variable,
        lang_ValueInstance* variable_instance)
    {
        lang_Scope* const current_scope = get_current_scope();
        lang_Scope* const variable_defined_scope =
            variable_instance->m_symbol->m_belongs_to_scope;

        std::optional<ast::AstValueFunction*> current_functiopn_may_null =
            get_scope_located_function(current_scope);

        if (!current_functiopn_may_null.has_value())
            // Not in a function;
            return variable_instance;

        std::optional<ast::AstValueFunction*> variable_defined_in_function_may_null =
            get_scope_located_function(variable_defined_scope);

        if (!variable_defined_in_function_may_null.has_value()
            || (variable_instance->m_symbol->m_declare_attribute.has_value()
                && variable_instance->m_symbol->m_declare_attribute.value()->m_lifecycle.has_value()
                && variable_instance->m_symbol->m_declare_attribute.value()->m_lifecycle.value() == ast::AstDeclareAttribue::lifecycle_attrib::STATIC))
        {
            // Variable cannot be global.
            return variable_instance;
        }

        ast::AstValueFunction* current_function = current_functiopn_may_null.value();
        ast::AstValueFunction* varible_defined_in_function =
            variable_defined_in_function_may_null.value();

        if (current_function != varible_defined_in_function)
        {
            // Might need capture?;
            bool need_capture = false;

            if (variable_instance->m_determined_constant_or_function.has_value())
            {
                ast::AstValueFunction** captured_function =
                    std::get_if<ast::AstValueFunction*>(&variable_instance->m_determined_constant_or_function.value());

                need_capture = false;
                if (captured_function != nullptr)
                {
                    if ((*captured_function)->m_LANG_captured_context.m_finished)
                    {
                        // Function is outside of the current function, check it's capture list.
                        if (!(*captured_function)->m_LANG_captured_context.m_captured_variables.empty())
                            // Is a closure, need capture.
                            need_capture = true;
                    }
                    else
                    {
                        // Recursive function referenced or template function depend each other.
                        // Woolang 1.14.1: Not allowed capture variable in recursive function.

                        // Treat it as normal function, but we need to mark the referenced function as self-referenced.
                        // If it has capture list, compile error will raised in pass1.
                        (*captured_function)->m_LANG_captured_context.m_self_referenced = true;
                    }
                }
            }
            else
                // Mutable, or not constant, need capture.
                need_capture = true;

            if (need_capture)
            {
                // Get function chain, create captured chain one by one;
                std::list<ast::AstValueFunction*> function_chain;

                lang_Scope* this_scope = current_scope;
                for (;;)
                {
                    if (this_scope->m_function_instance.has_value())
                    {
                        ast::AstValueFunction* this_function = this_scope->m_function_instance.value();

                        if (this_function == varible_defined_in_function)
                            // Found the function.
                            break;

                        wo_assert(!this_function->m_LANG_captured_context.m_finished);
                        function_chain.push_front(this_function);
                    }

                    // Loop will exit before it reach root scope.
                    this_scope = this_scope->m_parent_scope.value();
                }

                lang_ValueInstance* this_value_instance = variable_instance;
                for (auto* func : function_chain)
                {
                    this_value_instance = func->m_LANG_captured_context.find_or_create_captured_instance(
                        this_value_instance, ref_from_variable);
                }

                // OK
                return this_value_instance;
            }
        }
        return variable_instance;
    }
    bool BytecodeGenerateContext::ignore_eval_result() noexcept
    {
        auto& top_eval_state = m_eval_result_storage_target.top();
        return top_eval_state.m_request == EvalResult::Request::IGNORE_RESULT;
    }
    bool BytecodeGenerateContext::apply_eval_result(
        const std::function<bool(EvalResult&)>& bind_func) noexcept
    {
        auto& top_eval_state = m_eval_result_storage_target.top();

        bool eval_result = true;
        if (!ignore_eval_result())
        {
            eval_result = bind_func(top_eval_state);
            if (eval_result)
            {
                switch (top_eval_state.m_request)
                {
                case EvalResult::Request::EVAL_PURE_ACTION:
                    // Pure action, do nothing.
                    break;
                case EvalResult::Request::PUSH_RESULT_AND_IGNORE_RESULT:
                {
                    opnum::opnumbase* result_opnum = top_eval_state.m_result.value();

                    try_return_opnum_temporary_register(result_opnum);

                    c().psh(*result_opnum);
                    break;
                }
                default:
                    m_evaled_result_storage.emplace(std::move(top_eval_state));
                }
            }
        }
        m_eval_result_storage_target.pop();
        return eval_result;
    }

    void BytecodeGenerateContext::failed_eval_result() noexcept
    {
        m_eval_result_storage_target.pop();
    }
    void BytecodeGenerateContext::eval_action()
    {
        m_eval_result_storage_target.push(EvalResult{
            EvalResult::Request::EVAL_PURE_ACTION,
            std::nullopt,
            });
    }
    void BytecodeGenerateContext::eval_for_upper()
    {
        wo_assert(!m_eval_result_storage_target.empty());
    }
    void BytecodeGenerateContext::eval_keep()
    {
        m_eval_result_storage_target.push(EvalResult{
            EvalResult::Request::GET_RESULT_OPNUM_AND_KEEP,
            std::nullopt,
            });
    }
    void BytecodeGenerateContext::eval_push()
    {
        m_eval_result_storage_target.push(EvalResult{
            EvalResult::Request::PUSH_RESULT_AND_IGNORE_RESULT,
            std::nullopt,
            });
    }
    void BytecodeGenerateContext::eval()
    {
        m_eval_result_storage_target.push(EvalResult{
            EvalResult::Request::GET_RESULT_OPNUM_ONLY,
            std::nullopt,
            });
    }
    void BytecodeGenerateContext::eval_to(opnum::opnumbase* target)
    {
        m_eval_result_storage_target.push(EvalResult{
            EvalResult::Request::ASSIGN_TO_SPECIFIED_OPNUM,
            target,
            });
    }
    void BytecodeGenerateContext::eval_ignore()
    {
        m_eval_result_storage_target.push(EvalResult{
            EvalResult::Request::IGNORE_RESULT,
            std::nullopt,
            });
    }
    void BytecodeGenerateContext::eval_to_if_not_ignore(opnum::opnumbase* target)
    {
        if (!ignore_eval_result())
            eval_to(target);
        else
            eval_ignore();
    }
    void BytecodeGenerateContext::eval_sth_if_not_ignore(void(BytecodeGenerateContext::* method)())
    {
        if (!ignore_eval_result())
        {
#ifndef NDEBUG
            size_t current_request_count = m_eval_result_storage_target.size();
#endif
            (this->*method)();
#ifndef NDEBUG
            // NOTE: Cannot `eval_for_upper` in eval_sth_if_not_ignore.
            //  You can invoke `eval_for_upper` directly, it has same check effect.
            wo_assert(m_eval_result_storage_target.size() == current_request_count + 1);
#endif
        }
        else
            eval_ignore();
    }

    opnum::opnumbase* BytecodeGenerateContext::get_eval_result()
    {
        auto& result = m_evaled_result_storage.top();
        auto* result_opnum = result.m_result.value();

        wo_assert(
            result.m_request == EvalResult::Request::ASSIGN_TO_SPECIFIED_OPNUM
            || result.m_request == EvalResult::Request::GET_RESULT_OPNUM_ONLY
            || result.m_request == EvalResult::Request::GET_RESULT_OPNUM_AND_KEEP);

        if (result.m_request == EvalResult::Request::GET_RESULT_OPNUM_ONLY)
            try_return_opnum_temporary_register(result_opnum);

        m_evaled_result_storage.pop();
        return result_opnum;
    }
    opnum::global* BytecodeGenerateContext::opnum_global(int32_t offset) noexcept
    {
        auto fnd = m_opnum_cache_global.find(offset);
        if (fnd != m_opnum_cache_global.end())
            return fnd->second.get();

        return m_opnum_cache_global.insert(
            std::make_pair(offset, std::make_unique<opnum::global>(offset)))
            .first->second.get();
    }
    opnum::immbase* BytecodeGenerateContext::opnum_imm_int(wo_integer_t value) noexcept
    {
        auto fnd = m_opnum_cache_imm_int.find(value);
        if (fnd != m_opnum_cache_imm_int.end())
            return fnd->second.get();

        return m_opnum_cache_imm_int.insert(
            std::make_pair(value, std::make_unique<opnum::imm<wo_integer_t>>(value)))
            .first->second.get();
    }
    opnum::immbase* BytecodeGenerateContext::opnum_imm_real(wo_real_t value) noexcept
    {
        auto fnd = m_opnum_cache_imm_real.find(value);
        if (fnd != m_opnum_cache_imm_real.end())
            return fnd->second.get();

        return m_opnum_cache_imm_real.insert(
            std::make_pair(value, std::make_unique<opnum::imm<wo_real_t>>(value)))
            .first->second.get();
    }
    opnum::immbase* BytecodeGenerateContext::opnum_imm_handle(wo_handle_t value) noexcept
    {
        auto fnd = m_opnum_cache_imm_handle.find(value);
        if (fnd != m_opnum_cache_imm_handle.end())
            return fnd->second.get();

        return m_opnum_cache_imm_handle.insert(
            std::make_pair(value, std::make_unique<opnum::imm_hdl>(value)))
            .first->second.get();
    }
    opnum::immbase* BytecodeGenerateContext::opnum_imm_string(const std::string& value) noexcept
    {
        auto fnd = m_opnum_cache_imm_string.find(value);
        if (fnd != m_opnum_cache_imm_string.end())
            return fnd->second.get();

        return m_opnum_cache_imm_string.insert(
            std::make_pair(value, std::make_unique<opnum::imm_str>(value)))
            .first->second.get();
    }
    opnum::immbase* BytecodeGenerateContext::opnum_imm_bool(bool value) noexcept
    {
        if (value)
            return m_opnum_cache_imm_true.get();
        else
            return m_opnum_cache_imm_false.get();
    }
    opnum::opnumbase* BytecodeGenerateContext::opnum_imm_value(const wo::value& val)
    {
        switch (val.type)
        {
        case wo::value::valuetype::invalid:
            return opnum_spreg(opnum::reg::spreg::ni);
        case wo::value::valuetype::integer_type:
            return opnum_imm_int(val.integer);
        case wo::value::valuetype::real_type:
            return opnum_imm_real(val.real);
        case wo::value::valuetype::handle_type:
            return opnum_imm_handle(val.handle);
        case wo::value::valuetype::bool_type:
            return opnum_imm_bool(val.integer ? true : false);
        case wo::value::valuetype::string_type:
            return opnum_imm_string(*val.string);
        default:
            wo_error("Unexpected value type");
            return nullptr;
        }
    }
    opnum::tagimm_rsfunc* BytecodeGenerateContext::opnum_imm_rsfunc(const std::string& value) noexcept
    {
        auto fnd = m_opnum_cache_imm_rsfunc.find(value);
        if (fnd != m_opnum_cache_imm_rsfunc.end())
            return fnd->second.get();

        return m_opnum_cache_imm_rsfunc.insert(
            std::make_pair(value, std::make_unique<opnum::tagimm_rsfunc>(value)))
            .first->second.get();
    }
    opnum::tag* BytecodeGenerateContext::opnum_tag(const std::string& value) noexcept
    {
        auto fnd = m_opnum_cache_tag.find(value);
        if (fnd != m_opnum_cache_tag.end())
            return fnd->second.get();

        return m_opnum_cache_tag.insert(
            std::make_pair(value, std::make_unique<opnum::tag>(value)))
            .first->second.get();
    }
    opnum::reg* BytecodeGenerateContext::opnum_spreg(opnum::reg::spreg value) noexcept
    {
        uint8_t regid = static_cast<uint8_t>(value);
        auto fnd = m_opnum_cache_reg_and_stack_offset.find(regid);
        if (fnd != m_opnum_cache_reg_and_stack_offset.end())
            return fnd->second.get();

        return m_opnum_cache_reg_and_stack_offset.insert(
            std::make_pair(regid, std::make_unique<opnum::reg>(value)))
            .first->second.get();
    }
    opnum::reg* BytecodeGenerateContext::opnum_stack_offset(int8_t value) noexcept
    {
        uint8_t regid = opnum::reg::bp_offset(value);
        auto fnd = m_opnum_cache_reg_and_stack_offset.find(regid);
        if (fnd != m_opnum_cache_reg_and_stack_offset.end())
            return fnd->second.get();

        return m_opnum_cache_reg_and_stack_offset.insert(
            std::make_pair(regid, std::make_unique<opnum::reg>(regid)))
            .first->second.get();
    }
    opnum::temporary* BytecodeGenerateContext::opnum_temporary(uint32_t id) noexcept
    {
        auto fnd = m_opnum_cache_temporarys.find(id);
        if (fnd != m_opnum_cache_temporarys.end())
            return fnd->second.get();

        return m_opnum_cache_temporarys.insert(
            std::make_pair(id, std::make_unique<opnum::temporary>(id)))
            .first->second.get();
    }

    ir_compiler& BytecodeGenerateContext::c() noexcept
    {
        return m_compiler;
    }

    BytecodeGenerateContext::BytecodeGenerateContext() noexcept
        : m_opnum_cache_imm_true(std::make_unique<opnum::imm<bool>>(true))
        , m_opnum_cache_imm_false(std::make_unique<opnum::imm<bool>>(false))
        , m_global_storage_allocating(0)
    {
    }
    opnum::temporary* BytecodeGenerateContext::borrow_opnum_temporary_register(
#ifndef NDEBUG
        ast::AstBase* borrow_from, size_t lineno
#endif
    ) noexcept
    {
        for (uint32_t i = 0; i < UINT32_MAX; ++i)
        {
            if (m_inused_temporary_registers.find(i) == m_inused_temporary_registers.end())
            {
#ifdef NDEBUG
                m_inused_temporary_registers.insert(i);
#else
                m_inused_temporary_registers.insert(std::make_pair(i, DebugBorrowRecord{ borrow_from , lineno }));
#endif
                return opnum_temporary(i);
            }
        }
        wo_error("Temporary register exhausted.");
    }
    void BytecodeGenerateContext::keep_opnum_temporary_register(
        opnum::temporary* reg
#ifndef NDEBUG
        , ast::AstBase* borrow_from, size_t lineno
#endif
    ) noexcept
    {
        wo_assert(m_inused_temporary_registers.find(reg->m_id) == m_inused_temporary_registers.end());
#ifdef NDEBUG
        m_inused_temporary_registers.insert(reg->m_id);
#else
        m_inused_temporary_registers.insert(std::make_pair(reg->m_id, DebugBorrowRecord{ borrow_from , lineno }));
#endif
    }
    void BytecodeGenerateContext::return_opnum_temporary_register(opnum::temporary* reg) noexcept
    {
        wo_assert(m_inused_temporary_registers.find(reg->m_id) != m_inused_temporary_registers.end());
        m_inused_temporary_registers.erase(reg->m_id);
    }
    void BytecodeGenerateContext::try_keep_opnum_temporary_register(
        opnum::opnumbase* opnum_may_reg
#ifndef NDEBUG
        , ast::AstBase* borrow_from, size_t lineno
#endif    
    ) noexcept
    {
        auto* reg = dynamic_cast<opnum::temporary*>(opnum_may_reg);
        if (reg != nullptr)
        {
            keep_opnum_temporary_register(reg
#ifndef NDEBUG
                , borrow_from, lineno
#endif    
            );
        }
    }
    void BytecodeGenerateContext::try_return_opnum_temporary_register(
        opnum::opnumbase* opnum_may_reg) noexcept
    {
        auto* reg = dynamic_cast<opnum::temporary*>(opnum_may_reg);
        if (reg != nullptr)
        {
            return_opnum_temporary_register(reg);
        }
    }

    opnum::opnumbase* BytecodeGenerateContext::get_storage_place(
        const lang_ValueInstance::Storage& storage)
    {
        switch (storage.m_type)
        {
        case lang_ValueInstance::Storage::GLOBAL:
            return opnum_global(storage.m_index);
        case lang_ValueInstance::Storage::STACKOFFSET:
            wo_assert(storage.m_index >= -64 && storage.m_index <= 63);
            return opnum_stack_offset(storage.m_index);
        default:
            wo_error("Unexpected storage kind");
            return nullptr;
        }
    }
    const std::optional<opnum::opnumbase*>&
        BytecodeGenerateContext::EvalResult::get_assign_target() noexcept
    {
        return m_result;
    }

    void BytecodeGenerateContext::EvalResult::set_result(
        BytecodeGenerateContext& ctx, opnum::opnumbase* result) noexcept
    {
        wo_assert(m_request == Request::GET_RESULT_OPNUM_ONLY
            || m_request == Request::GET_RESULT_OPNUM_AND_KEEP
            || m_request == Request::PUSH_RESULT_AND_IGNORE_RESULT);

        if (m_request == Request::GET_RESULT_OPNUM_AND_KEEP)
        {
            auto* reg = dynamic_cast<opnum::reg*>(result);
            if (reg != nullptr
                && reg->id >= opnum::reg::spreg::cr
                && reg->id <= opnum::reg::spreg::last_special_register
                && reg->id != opnum::reg::spreg::ni)
            {
                auto* borrowed_reg = ctx.borrow_opnum_temporary_register(
                    WO_BORROW_TEMPORARY_FROM(nullptr)
                );

                ctx.c().mov(
                    *(opnum::opnumbase*)borrowed_reg,
                    *(opnum::opnumbase*)result);

                m_result = borrowed_reg;
                return;
            }
        }
        m_result = result;
    }
#endif
}
