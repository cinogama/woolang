#include "wo_afx.hpp"

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
        delete m_passir_A_processers;
        delete m_passir_B_processers;
    }

    //////////////////////////////////////

    lang_Symbol::~lang_Symbol()
    {
        switch (m_symbol_kind)
        {
        case kind::VARIABLE:
            if (m_is_template)
                delete m_template_value_instances;
            else
                delete m_value_instance;
            break;
        case kind::TYPE:
            if (m_is_template)
                delete m_template_type_instances;
            else
                delete m_type_instance;
            break;
        case kind::ALIAS:
            if (m_is_template)
                delete m_template_alias_instances;
            else
                delete m_alias_instance;
            break;
        default:
            wo_error("Unexpected symbol kind.");
        }
    }
    lang_Symbol::lang_Symbol(
        wo_pstring_t name,
        const std::optional<ast::AstDeclareAttribue*>& attr,
        std::optional<ast::AstBase*> symbol_declare_ast,
        const std::optional<ast::AstBase::source_location_t>& location,
        lang_Scope* scope,
        kind kind,
        bool mutable_variable)
        : m_symbol_kind(kind)
        , m_is_template(false)
        , m_is_global(false)
        , m_name(name)
        , m_declare_attribute(attr)
        , m_belongs_to_scope(scope)
        , m_symbol_declare_ast(symbol_declare_ast)
        , m_symbol_declare_location(location)
        , m_is_builtin(false)
        , m_symbol_edge(0)
        , m_has_been_used(false)
    {
        switch (kind)
        {
        case kind::VARIABLE:
            m_value_instance = new lang_ValueInstance(
                mutable_variable, this, std::nullopt);
            break;
        case kind::TYPE:
            m_type_instance = new lang_TypeInstance(this, std::nullopt);
            break;
        case kind::ALIAS:
            m_alias_instance = new lang_AliasInstance(this, std::nullopt);
            break;
        default:
            wo_error("Unexpected symbol kind.");
        }
    }
    lang_Symbol::lang_Symbol(
        wo_pstring_t name,
        const std::optional<ast::AstDeclareAttribue*>& attr,
        std::optional<ast::AstBase*> symbol_declare_ast,
        const  std::optional<ast::AstBase::source_location_t>& location,
        lang_Scope* scope,
        ast::AstValueBase* template_value_base,
        const std::vector<ast::AstTemplateParam*>& template_params,
        bool mutable_variable)
        : m_symbol_kind(kind::VARIABLE)
        , m_is_template(true)
        , m_is_global(false)
        , m_name(name)
        , m_declare_attribute(attr)
        , m_belongs_to_scope(scope)
        , m_symbol_declare_ast(symbol_declare_ast)
        , m_symbol_declare_location(location)
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
        const std::optional<ast::AstBase::source_location_t>& location,
        lang_Scope* scope,
        ast::AstTypeHolder* template_type_base,
        const std::vector<ast::AstTemplateParam*>& template_params,
        bool is_alias)
        : m_is_template(true)
        , m_is_global(false)
        , m_name(name)
        , m_declare_attribute(attr)
        , m_belongs_to_scope(scope)
        , m_symbol_declare_ast(symbol_declare_ast)
        , m_symbol_declare_location(location)
        , m_is_builtin(false)
        , m_symbol_edge(0)
        , m_has_been_used(false)
    {
        if (is_alias)
        {
            m_symbol_kind = kind::ALIAS;
            m_template_alias_instances = new TemplateAliasPrefab(
                this, template_type_base, template_params);
        }
        else
        {
            m_symbol_kind = kind::TYPE;
            m_template_type_instances = new TemplateTypePrefab(
                this, template_type_base, template_params);
        }
    }

    bool lang_Symbol::is_declared_as_static() const
    {
        if (m_declare_attribute.has_value()
            && m_declare_attribute.value()->m_lifecycle.has_value()
            && m_declare_attribute.value()->m_lifecycle.value() ==
            ast::AstDeclareAttribue::lifecycle_attrib::STATIC)
        {
            return true;
        }
        return false;
    }

    //////////////////////////////////////

    lang_Symbol::TemplateValuePrefab::TemplateValuePrefab(
        lang_Symbol* symbol,
        bool mutable_,
        ast::AstValueBase* ast,
        const std::vector<ast::AstTemplateParam*>& template_params)
        : m_symbol(symbol)
        , m_mutable(mutable_)
        , m_template_params(template_params)
        , m_origin_value_ast(ast)
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
        const std::vector<ast::AstTemplateParam*>& template_params)
        : m_symbol(symbol)
        , m_template_params(template_params)
        , m_origin_value_ast(ast)
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
        const std::vector<ast::AstTemplateParam*>& template_params)
        : m_symbol(symbol)
        , m_template_params(template_params)
        , m_origin_value_ast(ast)
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
            m_symbol, template_instance, template_args);

        auto* result = new_instance.get();

        m_template_instances.insert(
            std::make_pair(template_args, std::move(new_instance)));

        return result;
    }

    //////////////////////////////////////

    lang_TypeInstance::lang_TypeInstance(
        lang_Symbol* symbol,
        const std::optional<std::vector<ast::AstIdentifier::TemplateArgumentInstance>>& template_arguments)
        : m_symbol(symbol)
        , m_determined_base_type_or_mutable(std::nullopt)
        , m_instance_template_arguments(template_arguments)
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
    bool lang_TypeInstance::is_need_to_box_in_IR(woort_BoxValueType* out_type) const
    {
        switch (get_determined_type().value()->m_base_type)
        {
        case DeterminedType::base_type::HANDLE:
        case DeterminedType::base_type::INTEGER:
            *out_type = WOORT_BOX_VALUE_TYPE_INT;
            return true;
        case DeterminedType::base_type::REAL:
            *out_type = WOORT_BOX_VALUE_TYPE_REAL;
            return true;
        case DeterminedType::base_type::BOOLEAN:
            *out_type = WOORT_BOX_VALUE_TYPE_BOOL;
            return true;
        default:
            return false;
        }
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
            DeterminedType(copy_type.m_base_type, extern_desc);
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
        : m_state(state::UNPROCESSED), m_ast(ast), m_symbol(symbol)
    {
    }

    //////////////////////////////////////

    lang_TemplateAstEvalStateValue::lang_TemplateAstEvalStateValue(
        lang_Symbol* symbol,
        ast::AstValueBase* duplicated_ast,
        const std::vector<ast::AstIdentifier::TemplateArgumentInstance>& template_arguments)
        : lang_TemplateAstEvalStateBase(symbol, duplicated_ast)
    {
        m_value_instance = std::make_unique<lang_ValueInstance>(
            symbol->m_template_value_instances->m_mutable, symbol, template_arguments);

        m_constant_template_argument_have_unfinished_function = false;
        for (auto& argument : template_arguments)
        {
            if (!argument.m_constant.has_value())
                continue;

            auto constant = argument.m_constant.value().value_try_function();
            if (constant.has_value() && !constant.value()->m_LANG_captured_context.m_finished)
            {
                m_constant_template_argument_have_unfinished_function = true;
                break;
            }
        }

        if (!symbol->m_template_value_instances->m_mutable
            && duplicated_ast->node_type == ast::AstBase::AST_VALUE_FUNCTION)
            // For recursive function, let compiler know this function instance.
            m_value_instance->try_determine_function_may_constant(
                static_cast<ast::AstValueFunction*>(duplicated_ast));
    }

    //////////////////////////////////////

    lang_TemplateAstEvalStateType::lang_TemplateAstEvalStateType(
        lang_Symbol* symbol, ast::AstTypeHolder* ast, const std::vector<ast::AstIdentifier::TemplateArgumentInstance>& template_arguments)
        : lang_TemplateAstEvalStateBase(symbol, ast)
    {
        m_type_instance = std::make_unique<lang_TypeInstance>(
            symbol, std::optional(template_arguments));
    }

    //////////////////////////////////////

    lang_TemplateAstEvalStateAlias::lang_TemplateAstEvalStateAlias(
        lang_Symbol* symbol,
        ast::AstTypeHolder* ast,
        const std::vector<ast::AstIdentifier::TemplateArgumentInstance>& template_arguments)
        : lang_TemplateAstEvalStateBase(symbol, ast)
    {
        m_alias_instance = std::make_unique<lang_AliasInstance>(symbol, template_arguments);
    }

    //////////////////////////////////////

    lang_AliasInstance::lang_AliasInstance(
        lang_Symbol* symbol,
        const std::optional<std::vector<ast::AstIdentifier::TemplateArgumentInstance>>& template_arguments)
        : m_symbol(symbol)
        , m_instance_template_arguments(template_arguments)
        , m_determined_type(std::nullopt)
    {
    }

    //////////////////////////////////////
    void lang_ValueInstance::try_determine_function_may_constant(
        ast::AstValueFunction* func)
    {
        wo_assert(!m_mutable);

        // ATTENTION: The func here might be unevaluated, under evaluation, or already evaluated.
        // 
        // 1) Unevaluated: This func is undergoing generic instantiation, freshly copied from the template.
        //  At this stage, we can temporarily treat it as a constant, although this is quite a hacky trick.
        //  After the generic instantiation completes, we can then check whether this func captures any 
        //  parameters. If it doesn't, all is well. Even if it does, either this constant isn't referenced 
        //  (non-recursive case), or it is referenced, but the compiler will report an error (recursive 
        //  functions are not allowed to capture) before issues arise during IR generation. Either way, 
        //  it's safe.
        // 2) Under evaluation: This is a recursive function. Similar to case 1: if it captures variables, 
        //  the compiler would have already reported an error during compilation. Treat it directly as a 
        //  constant.
        // 3) Already evaluated: Excellent! Simply check whether it captures any parameters.

        if (func->m_LANG_hold_state == ast::AstValueFunction::LANG_hold_state::UNPROCESSED
            || func->m_LANG_captured_context.m_captured_variables.empty())
        {
            m_determined_constant_or_function.emplace(func);
        }
    }
    void lang_ValueInstance::try_determine_const_value(ast::AstValueBase* init_val)
    {
        wo_assert(!m_mutable);

        if (init_val->m_evaled_const_value.has_value())
            set_const_value(init_val->m_evaled_const_value.value());
    }
    void lang_ValueInstance::set_const_value(const ast::ConstantValue& init_val)
    {
        m_determined_constant_or_function.emplace(init_val);
    }
    void lang_ValueInstance::check_and_reset_const_if_func_captured()
    {
        if (m_determined_constant_or_function.has_value())
        {
            auto function_instance =
                m_determined_constant_or_function.value().value_try_function();

            if (function_instance.has_value())
            {
                auto* func = function_instance.value();

                wo_assert(func->m_LANG_captured_context.m_finished);
                if (!func->m_LANG_captured_context.m_captured_variables.empty())
                {
                    // Has captured variable, cannot be constant.
                    // See `lang_ValueInstance::try_determine_function_may_constant`, we can reset
                    // constant here.
                    m_determined_constant_or_function.reset();
                }
            }
        }
    }



    bool lang_ValueInstance::IR_need_storage() const
    {
        if (m_determined_constant_or_function.has_value())
        {
            wo_assert(!m_mutable);

#if WO_ENABLE_RUNTIME_CHECK
            const auto function_instance =
                m_determined_constant_or_function.value().value_try_function();

            wo_assert(!function_instance.has_value()
                || function_instance.value()->m_LANG_captured_context.m_captured_variables.empty());
#endif
            return false;
        }

        return true;
    }

    lang_ValueInstance::Storage::Storage(woort_IRValue* stack_slot)
        : m_type(STACKOFFSET)
        , m_stack_slot(stack_slot)
    {
    }
    lang_ValueInstance::Storage::Storage(woort_IRStaticIndex static_index)
        : m_type(GLOBAL)
        , m_static_index(static_index)
    {
    }

    lang_ValueInstance::lang_ValueInstance(
        bool mutable_,
        lang_Symbol* symbol,
        const std::optional<std::vector<ast::AstIdentifier::TemplateArgumentInstance>>& template_arguments)
        : m_symbol(symbol)
        , m_mutable(mutable_)
        , m_instance_template_arguments(template_arguments)
        , m_determined_constant_or_function(std::nullopt)
        , m_determined_type(std::nullopt)
    {
    }
    lang_ValueInstance::~lang_ValueInstance()
    {
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
        default:
            // No need to free for other types.
            break;
        }
    }

    //////////////////////////////////////

    lang_Scope::lang_Scope(const std::optional<lang_Scope*>& parent_scope, lang_Namespace* belongs)
        : m_defined_symbols{}
        , m_sub_scopes{}
        , m_declare_used_namespaces{}
        , m_parent_scope(parent_scope)
        , m_scope_location(std::nullopt)
        , m_belongs_to_namespace(belongs)
        , m_visibility_from_edge_for_template_check(0)
        //
        , m_function_instance(std::nullopt)
        , m_scope_instance(std::nullopt)
        , m_labeled_instance(std::nullopt)
        , m_scope_type(ScopeType::NORMAL)
        , m_been_break(false)
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
        : m_state(UNPROCESSED)
        , m_ast_node(node)
        , m_srcloc_pushed(false)
#ifndef NDEBUG
        , m_debug_scope_layer_count(0)
        , m_debug_entry_scope(nullptr)
#endif
    {
        wo_assert(m_ast_node != nullptr);
    }
    LangContext::AstNodeWithState::AstNodeWithState(
        pass_behavior state, ast::AstBase* node)
        : m_state(state)
        , m_ast_node(node)
        , m_srcloc_pushed(false)
    {
        wo_assert(m_ast_node != nullptr);
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
        LangContext::OriginTypeHolder::OriginTypeChain::path(const std::vector<lang_TypeInstance*>& type_path)
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
            const std::vector<std::tuple<ast::AstDeclareAttribue::accessc_attrib, wo_pstring_t, lang_TypeInstance*>>& type_path)
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

            lang_TypeInstance* new_type_instance = new lang_TypeInstance(
                m_dictionary,
                std::vector{
                    ast::AstIdentifier::TemplateArgumentInstance(key_type),
                    ast::AstIdentifier::TemplateArgumentInstance(value_type) });
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

            lang_TypeInstance* new_type_instance = new lang_TypeInstance(
                m_mapping,
                std::vector{
                    ast::AstIdentifier::TemplateArgumentInstance(key_type),
                    ast::AstIdentifier::TemplateArgumentInstance(value_type) });
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

            lang_TypeInstance* new_type_instance = new lang_TypeInstance(
                m_array,
                std::vector{ ast::AstIdentifier::TemplateArgumentInstance(element_type) });
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

            lang_TypeInstance* new_type_instance = new lang_TypeInstance(
                m_vector, std::vector{ ast::AstIdentifier::TemplateArgumentInstance(element_type) });
            new_type_instance->determine_base_type_move(std::move(determined_type_detail));

            chain_node->m_type_instance = new_type_instance;
        }
        return chain_node->m_type_instance.value();
    }
    lang_TypeInstance* LangContext::OriginTypeHolder::create_tuple_type(const std::vector<lang_TypeInstance*>& element_types)
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
    lang_TypeInstance* LangContext::OriginTypeHolder::create_function_type(bool is_variadic, const std::vector<lang_TypeInstance*>& param_types, lang_TypeInstance* return_type)
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
        const std::vector<std::tuple<ast::AstDeclareAttribue::accessc_attrib, wo_pstring_t, lang_TypeInstance*>>& member_types)
    {
        UnionStructTypeIndexChain* chain_node = m_struct_chain.path(member_types);
        if (!chain_node->m_type_instance)
        {
            lang_TypeInstance::DeterminedType::ExternalTypeDescription desc;

            desc.m_struct = new lang_TypeInstance::DeterminedType::Struct();

            int64_t index = 0;
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
        const std::vector<std::pair<wo_pstring_t, std::optional<lang_TypeInstance*>>>& member_types)
    {
        std::vector<std::tuple<ast::AstDeclareAttribue::accessc_attrib, wo_pstring_t, lang_TypeInstance*>>
            member_types_no_optional;
        for (auto& [field, type] : member_types)
            member_types_no_optional.push_back(
                std::make_tuple(ast::AstDeclareAttribue::accessc_attrib::PUBLIC, field, type ? type.value() : nullptr));

        UnionStructTypeIndexChain* chain_node = m_union_chain.path(member_types_no_optional);
        if (!chain_node->m_type_instance)
        {
            lang_TypeInstance::DeterminedType::ExternalTypeDescription desc;

            desc.m_union = new lang_TypeInstance::DeterminedType::Union();

            int64_t index = 0;
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
                lex.record_lang_error(lexer::msglevel_t::error, type_holder,
                    WO_ERR_EXPECTED_TEMPLATE_ARGUMENT,
                    ctx->get_symbol_name(symbol));

                return std::nullopt;
            }

            auto& template_arguments = identifier->m_template_arguments.value();

            if (symbol == m_dictionary || symbol == m_mapping)
            {
                if (template_arguments.size() != 2)
                {
                    lex.record_lang_error(lexer::msglevel_t::error, type_holder,
                        WO_ERR_UNEXPECTED_TEMPLATE_COUNT,
                        (size_t)2,
                        template_arguments.size());

                    return std::nullopt;
                }
                auto template_iter = template_arguments.begin();
                auto& key_type_template = *(template_iter++);
                auto& val_type_template = *(template_iter);

                if (key_type_template->is_constant())
                {
                    lex.record_lang_error(
                        lexer::msglevel_t::error,
                        key_type_template,
                        WO_ERR_THIS_TEMPLATE_ARG_SHOULD_BE_TYPE);

                    return std::nullopt;
                }
                if (val_type_template->is_constant())
                {
                    lex.record_lang_error(
                        lexer::msglevel_t::error,
                        key_type_template,
                        WO_ERR_THIS_TEMPLATE_ARG_SHOULD_BE_TYPE);

                    return std::nullopt;
                }

                auto* key_type = key_type_template->get_type()->m_LANG_determined_type.value();
                auto* value_type = val_type_template->get_type()->m_LANG_determined_type.value();

                return symbol == m_dictionary
                    ? create_dictionary_type(key_type, value_type)
                    : create_mapping_type(key_type, value_type);
                ;
            }
            else if (symbol == m_array || symbol == m_vector)
            {
                if (template_arguments.size() != 1)
                {
                    lex.record_lang_error(lexer::msglevel_t::error, type_holder,
                        WO_ERR_UNEXPECTED_TEMPLATE_COUNT,
                        (size_t)1,
                        template_arguments.size());

                    return std::nullopt;
                }
                auto& element_type_template = template_arguments.front();

                if (element_type_template->is_constant())
                {
                    lex.record_lang_error(
                        lexer::msglevel_t::error,
                        element_type_template,
                        WO_ERR_THIS_TEMPLATE_ARG_SHOULD_BE_TYPE);

                    return std::nullopt;
                }

                auto* element_type = element_type_template->get_type()->m_LANG_determined_type.value();
                return symbol == m_array
                    ? create_array_type(element_type)
                    : create_vector_type(element_type)
                    ;
            }
            else
            {
                lex.record_lang_error(lexer::msglevel_t::error, type_holder,
                    WO_ERR_CANNOT_USE_BUILTIN_TYPENAME_HERE,
                    ctx->get_symbol_name(symbol));

                return std::nullopt;
            }
        }
        case ast::AstTypeHolder::FUNCTION:
        {
            std::vector<lang_TypeInstance*> param_types;
            for (auto* parameter_type_holder : type_holder->m_typeform.m_function.m_parameters)
                param_types.push_back(parameter_type_holder->m_LANG_determined_type.value());

            return create_function_type(
                type_holder->m_typeform.m_function.m_is_variadic,
                param_types,
                type_holder->m_typeform.m_function.m_return_type->m_LANG_determined_type.value());
        }
        case ast::AstTypeHolder::STRUCTURE:
        {
            std::vector<std::tuple<ast::AstDeclareAttribue::accessc_attrib, wo_pstring_t, lang_TypeInstance*>> param_types;
            for (auto* field : type_holder->m_typeform.m_structure.m_fields)
                param_types.push_back(std::make_tuple(
                    field->m_attribute.value_or(ast::AstDeclareAttribue::accessc_attrib::PRIVATE),
                    field->m_name,
                    field->m_type->m_LANG_determined_type.value()));

            return create_struct_type(param_types);
        }
        case ast::AstTypeHolder::UNION:
        {
            std::vector<std::pair<wo_pstring_t, std::optional<lang_TypeInstance*>>> member_types;
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
            std::vector<lang_TypeInstance*> element_types;
            for (auto* field_type_holder : type_holder->m_typeform.m_tuple.m_fields)
                element_types.push_back(field_type_holder->m_LANG_determined_type.value());

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
        lexer& lex, ast::AstBase* root, const PassFunctionT& pass_function, bool track_srcloc)
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
                top_state.m_lex_error_frame_count = lex.get_error_frame_count_for_debug();
                top_state.m_debug_scope_layer_count = m_scope_stack.size();
                top_state.m_debug_entry_scope = get_current_scope();

                top_state.m_debug_ir_eval_content =
                    m_ircontext.m_eval_result_storage_target.size()
                    + m_ircontext.m_evaled_result_storage.size();

                if (!m_ircontext.m_eval_result_storage_target.empty())
                {
                    switch (m_ircontext.m_eval_result_storage_target.top().m_request)
                    {
                    case BytecodeGenerateContext::EvalResult::Request::PUSH_RESULT_AND_IGNORE:
                    case BytecodeGenerateContext::EvalResult::Request::PUSH_BOXED_RESULT_AND_IGNORE:
                    case BytecodeGenerateContext::EvalResult::Request::IGNORE_RESULT:
                    case BytecodeGenerateContext::EvalResult::Request::EXECUTE_ACTION_BUT_IGNORE_RESULT:
                        --top_state.m_debug_ir_eval_content;
                        break;
                    default:
                        break;
                    }
                }
            }
#endif

            if (top_state.m_state == HOLD || top_state.m_state == HOLD_BUT_CHILD_FAILED)
                process_roots.pop();

            if (track_srcloc && top_state.m_state == UNPROCESSED)
            {
                auto& sl = top_state.m_ast_node->source_location;
                if (sl.source_file != nullptr)
                {
                    m_ircontext.c().push_srcloc(top_state.m_ast_node);
                    top_state.m_srcloc_pushed = true;
                }
            }

            auto process_state = pass_function(this, lex, top_state, process_stack);

            switch (process_state)
            {
            case HOLD:
                top_state.m_state = HOLD;
                process_roots.push(&top_state);
                break;
            case FAILED:
                if (process_roots.empty())
                {
                    if (top_state.m_srcloc_pushed)
                        m_ircontext.c().pop_srcloc();
                    return false;
                }
                process_roots.top()->m_state = HOLD_BUT_CHILD_FAILED;
                /* FALL THROUGH */
                [[fallthrough]];
            case OKAY:
#ifndef NDEBUG
                wo_assert(top_state.m_debug_entry_scope != nullptr);
                wo_assert(top_state.m_debug_scope_layer_count == m_scope_stack.size());
                wo_assert(top_state.m_lex_error_frame_count == lex.get_error_frame_count_for_debug());
                wo_assert(top_state.m_debug_entry_scope == get_current_scope());
                wo_assert(
                    process_state != OKAY
                    || top_state.m_debug_ir_eval_content ==
                    m_ircontext.m_eval_result_storage_target.size()
                    + m_ircontext.m_evaled_result_storage.size());
#endif
                if (top_state.m_srcloc_pushed)
                    m_ircontext.c().pop_srcloc();
                process_stack.pop();
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

                bool symbol_defined = define_symbol_in_current_scope(
                    &out_sni->m_symbol,
                    name,
                    built_type_public_attrib,
                    std::nullopt,
                    std::nullopt,
                    get_current_scope(),
                    lang_Symbol::kind::TYPE,
                    false);

                wo_assert(symbol_defined);
                (void)symbol_defined;

                out_sni->m_type_instance = out_sni->m_symbol->m_type_instance;
                out_sni->m_type_instance->determine_base_type_move(
                    lang_TypeInstance::DeterminedType(basic_type, {}));
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

                bool symbol_defined = define_symbol_in_current_scope(
                    out_symbol,
                    name,
                    built_type_public_attrib,
                    std::nullopt,
                    std::nullopt,
                    get_current_scope(),
                    lang_Symbol::kind::TYPE,
                    false);

                wo_assert(symbol_defined);
                (void)symbol_defined;

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

    compile_result LangContext::process(lexer& lex, ast::AstBase* root)
    {
        pass_0_5_register_builtin_types();

        if (!anylize_pass(lex, root, &LangContext::pass_0_process_scope_and_non_local_defination, false))
            return compile_result::PROCESS_FAILED;

        if (!anylize_pass(lex, root, &LangContext::pass_1_process_basic_type_marking_and_constant_eval, false))
            return compile_result::PROCESS_FAILED;

        // Final process, generate bytecode.
        // NOTE: After 1.15. we will create a entry function for init job.
        woort_IRFunction* entry_function = m_ircontext.c().push_function(0, 0);
        m_ircontext.c().set_entry_function(entry_function);

        // Entry function cannot be invoked twice, check it.
        woort_IRLabel* label_bad_entry = m_ircontext.c().new_label();
        woort_IRLabel* label_entry_body = m_ircontext.c().new_label();

        woort_IRStaticIndex entry_function_global_init_flag =
            m_ircontext.c().alloc_static();

        /*
        _entry:
            jifinited   _bad_entry if ALOAD init_flag != 0
            jfwd        _entry_body
        _bad_entry:
            panic       ""
        _entry_body:
            ...
            retvc       0
        */
        m_ircontext.c().jifinited(entry_function_global_init_flag, label_bad_entry);
        m_ircontext.c().jmp(label_entry_body);
        // Bad init:
        m_ircontext.c().bind(label_bad_entry);
        m_ircontext.c().panic(
            m_ircontext.c().load_imm_string(
                wo::wstring_pool::get_pstr("")));

        // Entry body:
        m_ircontext.c().bind(label_entry_body);
        m_ircontext.c().astore(
            entry_function_global_init_flag,
            m_ircontext.c().load_imm_int(2));

        if (!anylize_pass(lex, root, &LangContext::pass_final_A_process_bytecode_generation, true))
            return compile_result::PROCESS_FAILED_BUT_PASS_1_OK;

        m_ircontext.c().ret(m_ircontext.c().load_imm_int(0));

        m_ircontext.c().pop_function();

        for (;;)
        {
            auto functions_to_finalize =
                std::move(m_ircontext.m_being_used_function_instance);

            m_ircontext.m_being_used_function_instance.clear();
            if (functions_to_finalize.empty())
                break;

            for (ast::AstValueFunction* eval_function : functions_to_finalize)
            {
                if (!m_ircontext.m_processed_function_instance.insert(eval_function).second)
                    // Already processed.
                    continue;

                if (eval_function->m_LANG_extern_information.has_value())
                {
                    wo_assert(eval_function->m_LANG_captured_context.m_captured_variables.empty());
                    // Donot generate code for extern function.
                    continue;
                }

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
                            m_ircontext.c().register_extern_symbols(
                                get_function_name(eval_function), m_ircontext.c().imm_closure(eval_function));
                        }
                    }
                }

                // Script function, generate codes for it.
                eval_function->m_IR_function.emplace(
                    m_ircontext.c().push_function(
                        (uint32_t)eval_function->m_parameters.size() + (eval_function->m_is_variadic ? 1 : 0),
                        (uint32_t)eval_function->m_LANG_captured_context.m_captured_variables.size()));

                // Update captured variable / param's storage.
                uint32_t argument_place = 0;
                for (auto& [_useless, captured_variable_instance] :
                    eval_function->m_LANG_captured_context.m_captured_variables)
                {
                    captured_variable_instance.m_instance->m_IR_storage.emplace(
                        lang_ValueInstance::Storage(
                            const_cast<woort_IRValue*>(m_ircontext.c().captured(argument_place++))));
                }
                argument_place = eval_function->m_is_variadic ? 1 : 0;
                for (auto* param_decls : eval_function->m_parameters)
                {
                    if (param_decls->m_pattern->node_type == ast::AstBase::AST_PATTERN_SINGLE)
                    {
                        ast::AstPatternSingle* pattern_single =
                            static_cast<ast::AstPatternSingle*>(param_decls->m_pattern);

                        if (pattern_single->m_is_mutable)
                            goto _label_mutable_or_pattern_params;

                        // Immutable pattern single argument.
                        lang_Symbol* symbol = pattern_single->m_LANG_declared_symbol.value();

                        wo_assert(!symbol->m_is_template && symbol->m_symbol_kind == lang_Symbol::kind::VARIABLE);
                        symbol->m_value_instance->m_IR_storage.emplace(
                            lang_ValueInstance::Storage(
                                const_cast<woort_IRValue*>(m_ircontext.c().argument(argument_place))));
                    }
                    else
                    {
                    _label_mutable_or_pattern_params:
                        update_pattern_storage_and_code_gen_passir(
                            lex,
                            param_decls->m_pattern,
                            m_ircontext.c().argument(argument_place),
                            std::nullopt);
                    }

                    ++argument_place;
                }
                
                if (!anylize_pass(
                    lex,
                    eval_function->m_body,
                    &LangContext::pass_final_A_process_bytecode_generation,
                    true))
                {
                    return compile_result::PROCESS_FAILED_BUT_PASS_1_OK;
                }

                // Generate ret if function donot end with return.
                if (!eval_function->m_LANG_function_body_end_with_return_flag_for_IR)
                    m_ircontext.c().ret_void();

                m_ircontext.c().pop_function();
            }
        }

        return compile_result::PROCESS_OK;
    }

    ////////////////////////
    ast::AstScope::LANG_end_state LangContext::check_node_type_and_get_end_state(ast::AstBase* node)
    {
        ast::AstBase* current_node = node;
        for (;;)
        {
            switch (current_node->node_type)
            {
            case ast::AstBase::AST_BREAK:
                return ast::AstScope::LANG_end_state::END_WITH_BREAK;
            case ast::AstBase::AST_CONTINUE:
                return ast::AstScope::LANG_end_state::END_WITH_CONTINUE;
            case ast::AstBase::AST_RETURN:
                return ast::AstScope::LANG_end_state::END_WITH_RETURN;
            case ast::AstBase::AST_LABELED:
                current_node = static_cast<ast::AstLabeled*>(current_node)->m_body;
                continue;
            case ast::AstBase::AST_SCOPE:
                return static_cast<ast::AstScope*>(current_node)->m_LANG_end_state;
            case ast::AstBase::AST_IF:
                return static_cast<ast::AstIf*>(current_node)->m_LANG_end_state;
            case ast::AstBase::AST_MATCH:
                return static_cast<ast::AstMatch*>(current_node)->m_LANG_end_state;
            case ast::AstBase::AST_WHILE:
                return static_cast<ast::AstWhile*>(current_node)->m_LANG_end_state;
            case ast::AstBase::AST_FOR:
                return static_cast<ast::AstFor*>(current_node)->m_LANG_end_state;
                // Foreach always end with NORMAL.
                // case ast::AstBase::AST_FOREACH:
                //     return static_cast<ast::AstForeach*>(current_node)->m_forloop_body->m_LANG_end_state;
            default:
                return ast::AstScope::LANG_end_state::NORMAL;
            }
        }
    }
    std::optional<lang_Scope*> LangContext::get_loop_scope_by_label_may_nullopt(
        const std::optional<wo_pstring_t>& label)
    {
        auto* scope = get_current_scope();
        for (;;)
        {
            if (scope->m_function_instance.has_value())
                break;

            if (scope->m_scope_type == lang_Scope::ScopeType::LOOP)
            {
                if (!label.has_value()
                    || (scope->m_labeled_instance.has_value()
                        && scope->m_labeled_instance.value()->m_label == label))
                    return scope;
            }

            if (!scope->m_parent_scope.has_value())
                break;

            scope = scope->m_parent_scope.value();
        }
        return std::nullopt;
    }
    void LangContext::begin_new_function(ast::AstValueFunction* func_instance)
    {
        auto* scope = begin_new_scope(func_instance->m_body->source_location);
        scope->m_function_instance = func_instance;
    }
    void LangContext::end_last_function()
    {
        wo_assert(get_current_scope()->m_function_instance);
        end_last_scope();
    }
    lang_Scope* LangContext::begin_new_scope(
        const std::optional<ast::AstBase::source_location_t>& locations)
    {
        lang_Scope* current_scope = get_current_scope();
        auto new_scope = std::make_unique<lang_Scope>(
            current_scope, get_current_scope()->m_belongs_to_namespace);

        new_scope->m_visibility_from_edge_for_template_check =
            m_created_symbol_edge;
        new_scope->m_scope_location = locations;

        entry_spcify_scope(new_scope.get());

        return current_scope->m_sub_scopes.emplace_back(
            std::move(new_scope)).get();
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

            lex.record_lang_error(lexer::msglevel_t::error, ident,
                WO_ERR_AMBIGUOUS_TARGET_NAMED,
                ident->m_name->c_str(),
                get_symbol_name(result));

            for (auto* symbol : found_symbol)
            {
                if (symbol->m_symbol_declare_ast.has_value())
                    lex.record_lang_error(lexer::msglevel_t::infom,
                        symbol->m_symbol_declare_ast.value(),
                        WO_INFO_MAYBE_NAMED_DEFINED_HERE,
                        get_symbol_name(symbol));
                else
                    lex.record_lang_error(lexer::msglevel_t::infom,
                        ident,
                        WO_INFO_MAYBE_NAMED_DEFINED_IN_COMPILER,
                        get_symbol_name(symbol));
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
        case ast::AstIdentifier::identifier_formal::FROM_GLOBAL:
            search_begin_namespace = m_root_namespace.get();
            break;
        case ast::AstIdentifier::identifier_formal::FROM_TYPE:
        {
            lang_TypeInstance* type_instance;

            if (ast::AstTypeHolder** from_type = std::get_if<ast::AstTypeHolder*>(&ident->m_from_type.value());
                from_type != nullptr)
            {
                auto determined_type = (*from_type)->m_LANG_determined_type;
                if (!determined_type)
                {
                    lex.record_lang_error(lexer::msglevel_t::error, *from_type, WO_ERR_UNKNOWN_TYPE);
                    return std::nullopt;
                }
                type_instance = determined_type.value();
            }
            else
                type_instance = std::get<lang_TypeInstance*>(ident->m_from_type.value());

            lang_Scope* search_from_scope = type_instance->m_symbol->m_belongs_to_scope;
            if (!search_from_scope->is_namespace_scope())
                return std::nullopt;

            search_begin_namespace =
                search_from_scope->m_belongs_to_namespace;

            auto fnd = search_begin_namespace->m_sub_namespaces.find(type_instance->m_symbol->m_name);
            if (fnd == search_begin_namespace->m_sub_namespaces.end())
                return std::nullopt; // Namespace not defined.

            search_begin_namespace = fnd->second.get();
            break;
        }
        case ast::AstIdentifier::identifier_formal::FROM_CURRENT:
        {
            auto found_symbol = _search_symbol_from_current_scope(lex, ident, out_ambig);
            if (found_symbol)
                ident->m_LANG_determined_symbol = found_symbol.value();
            return found_symbol;
        }
        default:
            wo_error("Unexpected identifier_formal.");
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
            {
                ident->m_LANG_determined_symbol = symbol;
                return symbol;
            }
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

            // NOTE: Copy template arguments.
            mutable_type_instance->m_instance_template_arguments =
                origin_type->m_instance_template_arguments;

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

    bool LangContext::fast_create_one_template_type_alias_and_constant_in_current_scope(
        wo_pstring_t template_param,
        const ast::AstIdentifier::TemplateArgumentInstance& template_arg)
    {
        lang_Symbol* symbol;

        if (template_arg.m_constant.has_value())
        {
            bool symbol_defined = define_symbol_in_current_scope(
                &symbol,
                template_param,
                std::nullopt,
                std::nullopt,
                std::nullopt,
                get_current_scope(),
                lang_Symbol::kind::VARIABLE,
                false);

            if (!symbol_defined)
                return false;

            symbol->m_value_instance->set_const_value(
                template_arg.m_constant.value());
            symbol->m_value_instance->m_determined_type = template_arg.m_type;
        }
        else
        {
            bool symbol_defined = define_symbol_in_current_scope(
                &symbol,
                template_param,
                std::nullopt,
                std::nullopt,
                std::nullopt,
                get_current_scope(),
                lang_Symbol::kind::ALIAS,
                false);

            if (!symbol_defined)
                return false;

            symbol->m_alias_instance->m_determined_type = template_arg.m_type;
        }
        return true;
    }
    bool LangContext::fast_check_and_create_template_type_alias_and_constant_in_current_scope(
        lexer& lex,
        bool make_instance_for_variable,
        const std::vector<ast::AstTemplateParam*>& template_params,
        const std::vector<ast::AstIdentifier::TemplateArgumentInstance>& template_args,
        std::optional<ast::AstTemplateConstantTypeCheckInPass1*> template_checker)
    {
        wo_assert(template_params.size() == template_args.size());

        ast::AstTemplateConstantTypeCheckInPass1* template_checker_p_may_null = template_checker.value_or(nullptr);

        auto params_iter = template_params.begin();
        auto args_iter = template_args.begin();
        auto params_end = template_params.end();

        for (; params_iter != params_end; ++params_iter, ++args_iter)
        {
            auto* param = *params_iter;
            auto& argument = *args_iter;

            if (param->m_marked_type.has_value())
            {
                if (argument.m_constant.has_value())
                {
                    wo_assert(template_checker_p_may_null != nullptr);

                    if (make_instance_for_variable
                        && argument.m_type->m_symbol == m_origin_types.m_nothing.m_symbol)
                    {
                        // NOTE: To prevent instances of `nothing` from being extracted from 
                        //  generic parameters by the type extractor technique; 
                        //  thus rejecting all instances of Nothing as generic arguments for 
                        //  reified variables;
                        lex.record_lang_error(lexer::msglevel_t::error, param,
                            WO_ERR_THIS_TEMPLATE_ARG_SHOULD_NOT_BE_NOTHING);

                        return false;
                    }

                    template_checker_p_may_null->m_LANG_constant_check_pairs.emplace_back(
                        ast::AstTemplateConstantTypeCheckInPass1::CheckingPair{
                            static_cast<ast::AstTypeHolder*>(param->m_marked_type.value()->clone()),
                            argument.m_type,
                        });
                }
                else
                {
                    lex.record_lang_error(
                        lexer::msglevel_t::error,
                        param,
                        WO_ERR_THIS_TEMPLATE_ARG_SHOULD_BE_CONST);

                    return false;
                }
            }
            else
            {
                if (argument.m_constant.has_value())
                {
                    lex.record_lang_error(
                        lexer::msglevel_t::error,
                        param,
                        WO_ERR_THIS_TEMPLATE_ARG_SHOULD_BE_TYPE);

                    return false;
                }
                else
                    // No need to check.
                    ;
            }

            if (!fast_create_one_template_type_alias_and_constant_in_current_scope(
                param->m_param_name,
                *args_iter))
            {
                lex.record_lang_error(lexer::msglevel_t::error, param,
                    WO_ERR_REDEFINED,
                    param->m_param_name->c_str());

                return false;
            }
        }

        return true;
    }
    std::string LangContext::_get_scope_name(lang_Scope* scope)
    {
        std::string result;
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

                result = *space_name + "::" + result;
            }
        }
        return result;
    }
    std::string LangContext::_get_symbol_name(lang_Symbol* symbol)
    {
        return _get_scope_name(symbol->m_belongs_to_scope) + *symbol->m_name;
    }
    std::string LangContext::_get_type_name(lang_TypeInstance* type)
    {
        std::string result_type_name;

        if (type->is_mutable())
            result_type_name += "mut ";

        auto* immutable_type_instance = immutable_type(type);

        if (immutable_type_instance->m_symbol->m_is_builtin)
        {
            auto* base_determined_type = immutable_type_instance->get_determined_type().value();
            switch (base_determined_type->m_base_type)
            {
            case lang_TypeInstance::DeterminedType::ARRAY:
                result_type_name += "array";
                break;
            case lang_TypeInstance::DeterminedType::VECTOR:
                result_type_name += "vec";
                break;
            case lang_TypeInstance::DeterminedType::DICTIONARY:
                result_type_name += "dict";
                break;
            case lang_TypeInstance::DeterminedType::MAPPING:
                result_type_name += "map";
                break;
            case lang_TypeInstance::DeterminedType::TUPLE:
            {
                result_type_name += "(";
                auto& element_types = base_determined_type->m_external_type_description.m_tuple->m_element_types;
                bool first = true;
                for (auto* element_type : element_types)
                {
                    if (!first)
                        result_type_name += ", ";

                    result_type_name += get_type_name(element_type);
                    first = false;
                }
                result_type_name += ")";
                break;
            }
            case lang_TypeInstance::DeterminedType::FUNCTION:
            {
                result_type_name += "(";
                auto& param_types = base_determined_type->m_external_type_description.m_function->m_param_types;
                bool first = true;
                for (auto* param_type : param_types)
                {
                    if (!first)
                        result_type_name += ", ";

                    result_type_name += get_type_name(param_type);
                    first = false;
                }
                if (base_determined_type->m_external_type_description.m_function->m_is_variadic)
                {
                    if (!first)
                        result_type_name += ", ";
                    result_type_name += "...";
                }
                result_type_name += ")=> ";
                result_type_name += get_type_name(
                    base_determined_type->m_external_type_description.m_function->m_return_type);
                break;
            }
            case lang_TypeInstance::DeterminedType::STRUCT:
            {
                result_type_name += "struct{";
                auto& member_types = base_determined_type->m_external_type_description.m_struct->m_member_types;
                bool first = true;
                for (auto& [name, field] : member_types)
                {
                    if (!first)
                        result_type_name += ", ";

                    result_type_name += *name;
                    result_type_name += ": ";
                    result_type_name += get_type_name(field.m_member_type);
                    first = false;
                }
                result_type_name += "}";
                break;
            }
            case lang_TypeInstance::DeterminedType::UNION:
            {
                result_type_name += "union{";
                auto& member_types = base_determined_type->m_external_type_description.m_union->m_union_label;
                bool first = true;
                for (auto& [name, field] : member_types)
                {
                    if (!first)
                        result_type_name += ", ";

                    result_type_name += *name;
                    if (field.m_item_type)
                    {
                        result_type_name += "(";
                        result_type_name += get_type_name(field.m_item_type.value());
                        result_type_name += ")";
                    }
                    first = false;
                }
                result_type_name += "}";
                break;
            }
            default:
                wo_error("Unexpected builtin type.");
            }
        }
        else
        {
            // Get symbol name directly.
            result_type_name += get_symbol_name(immutable_type_instance->m_symbol);
        }

        if (immutable_type_instance->m_instance_template_arguments)
        {
            result_type_name += "<";
            auto& template_args = immutable_type_instance->m_instance_template_arguments.value();
            bool first = true;
            for (auto& template_arg : template_args)
            {
                if (!first)
                    result_type_name += ", ";

                if (template_arg.m_constant.has_value())
                    result_type_name += "{"
                    + get_constant_str(template_arg.m_constant.value())
                    + ": "
                    + get_type_name(template_arg.m_type)
                    + "}";
                else
                    result_type_name += get_type_name(template_arg.m_type);

                first = false;
            }
            result_type_name += ">";
        }

        return result_type_name;
    }
    std::string LangContext::_get_value_name(lang_ValueInstance* scope)
    {
        std::string result_value_name = get_symbol_name(scope->m_symbol);

        if (scope->m_instance_template_arguments)
        {
            result_value_name += "<";
            auto& template_args = scope->m_instance_template_arguments.value();
            bool first = true;
            for (auto& template_arg : template_args)
            {
                if (!first)
                    result_value_name += ", ";

                if (template_arg.m_constant.has_value())
                {
                    result_value_name += "{"
                        + get_constant_str(template_arg.m_constant.value())
                        + ": "
                        + get_type_name(template_arg.m_type)
                        + "}";
                }
                else
                    result_value_name += get_type_name(template_arg.m_type);

                first = false;
            }
            result_value_name += ">";
        }

        return result_value_name;
    }
    std::string LangContext::_get_function_name(ast::AstValueFunction* func)
    {
        if (func->m_LANG_value_instance_to_update.has_value())
        {
            lang_ValueInstance* value_instance = func->m_LANG_value_instance_to_update.value();
            std::string name = get_value_name(value_instance);

            lang_Scope* function_located_scope =
                func->m_LANG_function_scope.value()->m_parent_scope.value();

            if (!function_located_scope->is_namespace_scope())
            {
                char local_name[48];
                auto r = snprintf(local_name, 48, "[local_%p]", function_located_scope);
                wo_test(r >= 0 && r < 48, "Failed to generate function name.");

                auto function = get_scope_located_function(function_located_scope);
                if (function.has_value())
                    name = get_function_name(function.value()) + ("::" + (local_name + ("::" + name)));
            }
            return name;
        }

        std::string result;

        lang_Scope* function_located_scope =
            func->m_LANG_function_scope.value()->m_parent_scope.value();

        auto function = get_scope_located_function(function_located_scope);
        if (function.has_value())
            result += std::string(get_function_name(function.value())) + "::";

        char anonymous_name[48];
        auto r = snprintf(anonymous_name, 48, "<func %p>", func);
        wo_test(r >= 0 && r < 48, "Failed to generate function name.");

        wo_assert(func->source_location.source_file != nullptr);
        result += anonymous_name;

        return result;
    }
    const char* LangContext::get_symbol_name(lang_Symbol* symbol)
    {
        auto fnd = m_symbol_name_cache.find(symbol);
        if (fnd != m_symbol_name_cache.end())
            return fnd->second.c_str();

        return m_symbol_name_cache.insert(
            std::make_pair(symbol, _get_symbol_name(symbol)))
            .first->second.c_str();
    }
    const char* LangContext::get_type_name(lang_TypeInstance* type)
    {
        auto fnd = m_type_name_cache.find(type);
        if (fnd != m_type_name_cache.end())
            return fnd->second.c_str();

        (void)m_type_name_cache.insert(
            std::make_pair(type, *type->m_symbol->m_name + "..."));

        m_type_name_cache.erase(type);
        return m_type_name_cache.insert(
            std::make_pair(type, _get_type_name(type)))
            .first->second.c_str();
    }

    const char* LangContext::get_value_name(lang_ValueInstance* val)
    {
        auto fnd = m_value_name_cache.find(val);
        if (fnd != m_value_name_cache.end())
            return fnd->second.c_str();

        return m_value_name_cache.insert(
            std::make_pair(val, _get_value_name(val)))
            .first->second.c_str();
    }
    const char* LangContext::get_function_name(ast::AstValueFunction* func)
    {
        auto fnd = m_function_name_cache.find(func);
        if (fnd != m_function_name_cache.end())
            return fnd->second.c_str();

        return m_function_name_cache.insert(
            std::make_pair(func, _get_function_name(func)))
            .first->second.c_str();
    }
    std::string LangContext::get_constant_str(const ast::ConstantValue& val)
    {
        switch (val.m_type)
        {
        case ast::ConstantValue::Type::NIL:
            return "nil";
        case ast::ConstantValue::Type::BOOL:
            if (val.value_bool())
                return "true";
            return "false";
        case ast::ConstantValue::Type::INTEGER:
            return std::to_string(val.value_integer());
        case ast::ConstantValue::Type::HANDLE:
            return std::to_string(val.value_handle());
        case ast::ConstantValue::Type::REAL:
            return std::to_string(val.value_real());
        case ast::ConstantValue::Type::PSTRING:
        {
            wo_pstring_t pstring_constant = val.value_pstring();

            return wo::u8enstring(
                pstring_constant->data(),
                pstring_constant->size(),
                false);
        }
        case ast::ConstantValue::Type::STRUCT:
        {
            auto& struct_constant = val.value_struct();
            std::string result = "(";
            for (size_t idx = 0; idx < struct_constant.m_count; ++idx)
            {
                if (idx != 0)
                    result += ", ";

                result += get_constant_str(struct_constant.m_elements[idx]);
            }
            result += ")";
            return result;
        }
        case ast::ConstantValue::Type::FUNCTION:
            return get_function_name(val.value_function());
        default:
            return "<?>";
        }
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
                auto captured_function =
                    variable_instance->m_determined_constant_or_function.value().value_try_function();

                need_capture = false;
                if (captured_function.has_value())
                {
                    auto* captured_function_ptr = captured_function.value();

                    if (captured_function_ptr->m_LANG_captured_context.m_finished)
                    {
                        // Function is outside of the current function, check it's capture list.
                        if (!captured_function_ptr->m_LANG_captured_context.m_captured_variables.empty())
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

    void BytecodeGenerateContext::pop_eval_result()
    {
#if WO_ENABLE_RUNTIME_CHECK
        auto& result = m_evaled_result_storage.top();

        wo_assert(result.m_request != EvalResult::Request::GET_RESULT_FOR_READONLY
            && result.m_request != EvalResult::Request::GET_BOXED_RESULT_FOR_READONLY
            && result.m_request != EvalResult::Request::PUSH_RESULT_AND_IGNORE
            && result.m_request != EvalResult::Request::PUSH_BOXED_RESULT_AND_IGNORE
            && result.m_request != EvalResult::Request::IGNORE_RESULT);
#endif

        m_evaled_result_storage.pop();
    }

    const woort_IRValue* BytecodeGenerateContext::get_eval_result()
    {
        auto& result = m_evaled_result_storage.top();

        woort_IRValue* r;

        switch (result.m_result_type)
        {
        case EvalResult::ResultKind::ASSIGN_TO_STATIC:
        case EvalResult::ResultKind::RESULT_STATIC:
        {
            r = c().new_value();
            c().load(r, result.m_result_static);
            break;
        }
        case EvalResult::ResultKind::ASSIGN_TO_STACKSLOT:
        case EvalResult::ResultKind::RESULT_STACK_VARIABLE:
        case EvalResult::ResultKind::RESULT_STACK_TEMP:
            r = result.m_result_stack;
            break;
        default:
            abort();
        }

        m_evaled_result_storage.pop();
        return r;
    }
    bool BytecodeGenerateContext::is_eval_result_just_ignored() const noexcept
    {
        auto& top_eval_state = m_eval_result_storage_target.top();
        return top_eval_state.m_request == EvalResult::Request::IGNORE_RESULT;
    }
    bool BytecodeGenerateContext::upper_need_box() const noexcept
    {
        auto& top_eval_state = m_eval_result_storage_target.top();
        switch (top_eval_state.m_request)
        {
        case EvalResult::Request::ASSIGN_BOXED_TO_TARGET_AND_GET_TARGET:
        case EvalResult::Request::GET_BOXED_RESULT_FOR_READONLY:
        case EvalResult::Request::PUSH_BOXED_RESULT_AND_IGNORE:
            return true;
        default:
            break;
        }

        return false;
    }
    bool BytecodeGenerateContext::upper_need_get_result() const noexcept
    {
        auto& top_eval_state = m_eval_result_storage_target.top();
        switch (top_eval_state.m_request)
        {
        case EvalResult::Request::ASSIGN_TO_TARGET_AND_GET_TARGET:
        case EvalResult::Request::ASSIGN_BOXED_TO_TARGET_AND_GET_TARGET:
        case EvalResult::Request::GET_RESULT_FOR_READONLY:
        case EvalResult::Request::GET_BOXED_RESULT_FOR_READONLY:
            return true;
        default:
            break;
        }

        return false;
    }
    bool BytecodeGenerateContext::upper_need_assign() const noexcept
    {
        auto& top_eval_state = m_eval_result_storage_target.top();
        switch (top_eval_state.m_request)
        {
        case EvalResult::Request::ASSIGN_TO_TARGET_AND_GET_TARGET:
        case EvalResult::Request::ASSIGN_BOXED_TO_TARGET_AND_GET_TARGET:
            return true;
        default:
            break;
        }

        return false;
    }

    void BytecodeGenerateContext::apply_eval_result(
        const std::function<void(EvalResult&)>& bind_func) noexcept
    {
        auto& top_eval_state = m_eval_result_storage_target.top();

        if (!is_eval_result_just_ignored())
        {
            bool need_pop_pdi = false;
            if (top_eval_state.m_pdi_node.has_value())
            {
                need_pop_pdi = true;
                c().push_srcloc(top_eval_state.m_pdi_node.value());
            }

            bind_func(top_eval_state);
            switch (top_eval_state.m_request)
            {
            case EvalResult::Request::EXECUTE_ACTION_BUT_IGNORE_RESULT:
                // Pure action, do nothing.
            case EvalResult::Request::PUSH_RESULT_AND_IGNORE:
            case EvalResult::Request::PUSH_BOXED_RESULT_AND_IGNORE:
                // Already pushed when set_result_*, ignore the result.
                break;
            default:
                m_evaled_result_storage.emplace(std::move(top_eval_state));
            }

            if (need_pop_pdi)
                c().pop_srcloc();
        }
        m_eval_result_storage_target.pop();
    }

    void BytecodeGenerateContext::eval_to_assign(
        woort_IRValue* target, const std::optional<ast::AstBase*>& pdinode)
    {
        EvalResult r;
        r.m_request = EvalResult::Request::ASSIGN_TO_TARGET_AND_GET_TARGET;
        r.m_result_type = EvalResult::ResultKind::ASSIGN_TO_STACKSLOT;
        r.m_result_stack = target;
        r.m_pdi_node = pdinode;

        m_eval_result_storage_target.push(r);
    }
    void BytecodeGenerateContext::begin_eval_readonly()
    {
        EvalResult r;
        r.m_request = EvalResult::Request::GET_RESULT_FOR_READONLY;
        r.m_result_type = EvalResult::ResultKind::PENDING;
        r.m_pdi_node = std::nullopt;

        m_eval_result_storage_target.push(r);
    }
    void BytecodeGenerateContext::eval_to_push()
    {
        EvalResult r;
        r.m_request = EvalResult::Request::PUSH_RESULT_AND_IGNORE;
        r.m_result_type = EvalResult::ResultKind::PENDING;
        r.m_pdi_node = std::nullopt;

        m_eval_result_storage_target.push(r);
    }
    void BytecodeGenerateContext::eval_to_assign_box(
        woort_IRValue* target, const std::optional<ast::AstBase*>& pdinode)
    {
        EvalResult r;
        r.m_request = EvalResult::Request::ASSIGN_BOXED_TO_TARGET_AND_GET_TARGET;
        r.m_result_type = EvalResult::ResultKind::ASSIGN_TO_STACKSLOT;
        r.m_result_stack = target;
        r.m_pdi_node = pdinode;

        m_eval_result_storage_target.push(r);
    }
    void BytecodeGenerateContext::eval_to_assign_static(
        woort_IRStaticIndex target, const std::optional<ast::AstBase*>& pdinode)
    {
        EvalResult r;
        r.m_request = EvalResult::Request::ASSIGN_TO_TARGET_AND_GET_TARGET;
        r.m_result_type = EvalResult::ResultKind::ASSIGN_TO_STATIC;
        r.m_result_static = target;
        r.m_pdi_node = pdinode;

        m_eval_result_storage_target.push(r);
    }
    void BytecodeGenerateContext::eval_to_assign_box_static(
        woort_IRStaticIndex target, const std::optional<ast::AstBase*>& pdinode)
    {
        EvalResult r;
        r.m_request = EvalResult::Request::ASSIGN_BOXED_TO_TARGET_AND_GET_TARGET;
        r.m_result_type = EvalResult::ResultKind::ASSIGN_TO_STATIC;
        r.m_result_static = target;
        r.m_pdi_node = pdinode;

        m_eval_result_storage_target.push(r);
    }
    void BytecodeGenerateContext::begin_eval_readonly_box()
    {
        EvalResult r;
        r.m_request = EvalResult::Request::GET_BOXED_RESULT_FOR_READONLY;
        r.m_result_type = EvalResult::ResultKind::PENDING;
        r.m_pdi_node = std::nullopt;

        m_eval_result_storage_target.push(r);
    }
    void BytecodeGenerateContext::eval_to_push_box()
    {
        EvalResult r;
        r.m_request = EvalResult::Request::PUSH_BOXED_RESULT_AND_IGNORE;
        r.m_result_type = EvalResult::ResultKind::PENDING;
        r.m_pdi_node = std::nullopt;

        m_eval_result_storage_target.push(r);
    }
    void BytecodeGenerateContext::eval_and_ignore()
    {
        EvalResult r;
        r.m_request = EvalResult::Request::IGNORE_RESULT;
        r.m_result_type = EvalResult::ResultKind::PENDING;
        r.m_pdi_node = std::nullopt;

        m_eval_result_storage_target.push(r);
    }
    void BytecodeGenerateContext::eval_action_and_ignore()
    {
        EvalResult r;
        r.m_request = EvalResult::Request::EXECUTE_ACTION_BUT_IGNORE_RESULT;
        r.m_result_type = EvalResult::ResultKind::PENDING;
        r.m_pdi_node = std::nullopt;

        m_eval_result_storage_target.push(r);
    }

    void BytecodeGenerateContext::eval_for_upper()
    {
        wo_assert(!m_eval_result_storage_target.empty());

        // Make a duplicated eval request (For FAILED recover).
        m_eval_result_storage_target.push(m_eval_result_storage_target.top());
    }
    void BytecodeGenerateContext::eval_for_upper_box()
    {
        wo_assert(!m_eval_result_storage_target.empty());

        // Make a duplicated eval request (For FAILED recover).
        EvalResult dup_eval_request = m_eval_result_storage_target.top();
        switch (dup_eval_request.m_request)
        {
        case BytecodeGenerateContext::EvalResult::Request::ASSIGN_TO_TARGET_AND_GET_TARGET:
            dup_eval_request.m_request =
                BytecodeGenerateContext::EvalResult::Request::ASSIGN_BOXED_TO_TARGET_AND_GET_TARGET;
            break;
        case BytecodeGenerateContext::EvalResult::Request::GET_RESULT_FOR_READONLY:
            dup_eval_request.m_request =
                BytecodeGenerateContext::EvalResult::Request::GET_BOXED_RESULT_FOR_READONLY;
            break;
        case BytecodeGenerateContext::EvalResult::Request::PUSH_RESULT_AND_IGNORE:
            dup_eval_request.m_request =
                BytecodeGenerateContext::EvalResult::Request::PUSH_BOXED_RESULT_AND_IGNORE;
            break;
        case BytecodeGenerateContext::EvalResult::Request::IGNORE_RESULT:
        default:
            /* Donothing */
            break;
        }
        m_eval_result_storage_target.push(dup_eval_request);
    }
    void BytecodeGenerateContext::cleanup_for_eval_upper()
    {
        m_eval_result_storage_target.pop();
    }

    void BytecodeGenerateContext::eval_to_assign_if_not_ignore(
        woort_IRValue* target, const std::optional<ast::AstBase*>& pdinode)
    {
        if (is_eval_result_just_ignored())
            eval_and_ignore();
        else
            eval_to_assign(target, pdinode);
    }
    void BytecodeGenerateContext::do_eval_if_not_ignore(void(BytecodeGenerateContext::* method)())
    {
        if (is_eval_result_just_ignored())
            eval_and_ignore();
        else
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
    }

    void BytecodeGenerateContext::failed_eval_result() noexcept
    {
        m_eval_result_storage_target.pop();
    }

    std::optional<woort_CodeEnv*> BytecodeGenerateContext::finalize()
    {
        // Transfer loaded extern library handles to IRCompiler for CodeEnv binding
        auto handles = m_extern_libs.collect_handles();
        for (woort_Dylib* lib : handles)
            m_ir_compiler.add_extern_lib(lib);

        return c().commit();
    }

    BytecodeGenerateContext::BytecodeGenerateContext() noexcept
    {
    }

    std::optional<std::pair<std::optional<woort_BoxValueType>, std::variant<woort_IRValue*, woort_IRStaticIndex>>>
        BytecodeGenerateContext::EvalResult::get_assign_target(lang_TypeInstance* t) const noexcept
    {
        std::optional<woort_BoxValueType> need_to_box_as = std::nullopt;
        if (m_request == Request::ASSIGN_BOXED_TO_TARGET_AND_GET_TARGET)
        {
            woort_BoxValueType woort_box_target_type;

            if (t->is_need_to_box_in_IR(&woort_box_target_type))
                need_to_box_as.emplace(woort_box_target_type);
        }

        switch (m_result_type)
        {
        case ResultKind::ASSIGN_TO_STATIC:
            return std::make_pair(need_to_box_as, m_result_static);
        case ResultKind::ASSIGN_TO_STACKSLOT:
            return std::make_pair(need_to_box_as, m_result_stack);
        default:
            return std::nullopt;
        }
    }

    void BytecodeGenerateContext::EvalResult::set_result_stack_temp(
        BytecodeGenerateContext& ctx, const woort_IRValue* result, const lang_TypeInstance* type) noexcept
    {
        wo_assert(m_result_type == ResultKind::PENDING);

        switch (m_request)
        {
        case Request::GET_BOXED_RESULT_FOR_READONLY:
        {
            woort_BoxValueType box_type;
            if (type->is_need_to_box_in_IR(&box_type))
            {
                m_result_type = ResultKind::RESULT_STACK_TEMP;
                m_result_stack = ctx.c().new_value();

                ctx.c().boxdyn(m_result_stack, box_type, result);
                break;
            }
        }
        /* fallthrough */
        [[fallthrough]];
        case Request::GET_RESULT_FOR_READONLY:
            m_result_type = ResultKind::RESULT_STACK_TEMP;
            m_result_stack = const_cast<woort_IRValue*>(result);
            break;
        case Request::PUSH_BOXED_RESULT_AND_IGNORE:
        {
            woort_BoxValueType box_type;
            if (type->is_need_to_box_in_IR(&box_type))
            {
                ctx.c().pushboxdyn(box_type, result);
                break;
            }
        }
        /* fallthrough */
        [[fallthrough]];
        case Request::PUSH_RESULT_AND_IGNORE:
            ctx.c().pushchk(result);
            break;
        case Request::ASSIGN_TO_TARGET_AND_GET_TARGET:
        case Request::ASSIGN_BOXED_TO_TARGET_AND_GET_TARGET:
        default:
            abort();
        }
    }
    void BytecodeGenerateContext::EvalResult::set_result_stack_var(
        BytecodeGenerateContext& ctx, woort_IRValue* result, const lang_TypeInstance* type) noexcept
    {
        wo_assert(m_result_type == ResultKind::PENDING);

        switch (m_request)
        {
        case Request::GET_BOXED_RESULT_FOR_READONLY:
        {
            woort_BoxValueType box_type;
            if (type->is_need_to_box_in_IR(&box_type))
            {
                m_result_type = ResultKind::RESULT_STACK_TEMP;
                m_result_stack = ctx.c().new_value();

                ctx.c().boxdyn(m_result_stack, box_type, result);
                break;
            }
            /* fallthrough */
            [[fallthrough]]
            ;
        }
        case Request::GET_RESULT_FOR_READONLY:
            m_result_type = ResultKind::RESULT_STACK_VARIABLE;
            m_result_stack = result;
            break;
        case Request::PUSH_BOXED_RESULT_AND_IGNORE:
        {
            woort_BoxValueType box_type;
            if (type->is_need_to_box_in_IR(&box_type))
            {
                ctx.c().pushboxdyn(box_type, result);
                break;
            }
            /* fallthrough */
            [[fallthrough]]
            ;
        }
        case Request::PUSH_RESULT_AND_IGNORE:
            ctx.c().pushchk(result);
            break;
        case Request::ASSIGN_TO_TARGET_AND_GET_TARGET:
        case Request::ASSIGN_BOXED_TO_TARGET_AND_GET_TARGET:
        default:
            abort();
        }
    }
    void BytecodeGenerateContext::EvalResult::set_result_static(
        BytecodeGenerateContext& ctx, woort_IRStaticIndex result, const lang_TypeInstance* type) noexcept
    {
        wo_assert(m_result_type == ResultKind::PENDING);

        switch (m_request)
        {
        case Request::GET_BOXED_RESULT_FOR_READONLY:
        {
            woort_BoxValueType box_type;
            if (type->is_need_to_box_in_IR(&box_type))
            {
                m_result_type = ResultKind::RESULT_STACK_TEMP;
                m_result_stack = ctx.c().new_value();

                ctx.c().load(m_result_stack, result);
                ctx.c().boxdyn(m_result_stack, box_type, m_result_stack);

                break;
            }
        }
        /* fallthrough */
        [[fallthrough]];
        case Request::GET_RESULT_FOR_READONLY:
            m_result_type = ResultKind::RESULT_STACK_TEMP;
            m_result_stack = ctx.c().new_value();

            ctx.c().load(m_result_stack, result);
            break;
        case Request::PUSH_BOXED_RESULT_AND_IGNORE:
        {
            woort_BoxValueType box_type;
            if (type->is_need_to_box_in_IR(&box_type))
            {
                woort_IRValue* const temp = ctx.c().new_value();
                ctx.c().load(temp, result);
                ctx.c().pushboxdyn(box_type, temp);
                break;
            }
        }
        /* fallthrough */
        [[fallthrough]];
        case Request::PUSH_RESULT_AND_IGNORE:
            ctx.c().pushstaticchk(result);
            break;
        case Request::ASSIGN_TO_TARGET_AND_GET_TARGET:
        case Request::ASSIGN_BOXED_TO_TARGET_AND_GET_TARGET:
        default:
            abort();
        }
    }
    void BytecodeGenerateContext::EvalResult::set_result_struct_index(
        BytecodeGenerateContext& ctx,
        const woort_IRValue* structure,
        uint32_t index,
        const lang_TypeInstance* type) noexcept
    {
        wo_assert(m_result_type == ResultKind::PENDING);

        switch (m_request)
        {
        case Request::GET_BOXED_RESULT_FOR_READONLY:
        {
            woort_BoxValueType box_type;
            if (type->is_need_to_box_in_IR(&box_type))
            {
                m_result_type = ResultKind::RESULT_STACK_TEMP;
                m_result_stack = ctx.c().new_value();

                ctx.c().ldidxstruct(m_result_stack, structure, index);
                ctx.c().boxdyn(m_result_stack, box_type, m_result_stack);
                break;
            }
        }
        /* fallthrough */
        [[fallthrough]];
        case Request::GET_RESULT_FOR_READONLY:
            m_result_type = ResultKind::RESULT_STACK_TEMP;
            m_result_stack = ctx.c().new_value();

            ctx.c().ldidxstruct(m_result_stack, structure, index);
            break;
        case Request::PUSH_BOXED_RESULT_AND_IGNORE:
        {
            woort_BoxValueType box_type;
            if (type->is_need_to_box_in_IR(&box_type))
            {
                switch (box_type)
                {
                case WOORT_BOX_VALUE_TYPE_INT:
                    ctx.c().pushidxstboxi(structure, index);
                    break;
                case WOORT_BOX_VALUE_TYPE_REAL:
                    ctx.c().pushidxstboxr(structure, index);
                    break;
                case WOORT_BOX_VALUE_TYPE_BOOL:
                    ctx.c().pushidxstboxb(structure, index);
                    break;
                default:
                    wo_error("Unknown box_type for struct field.");
                    break;
                }
                break;
            }
        }
        /* fallthrough */
        [[fallthrough]];
        case Request::PUSH_RESULT_AND_IGNORE:
            ctx.c().pushidxstruct(structure, index);
            break;
        case Request::ASSIGN_TO_TARGET_AND_GET_TARGET:
        case Request::ASSIGN_BOXED_TO_TARGET_AND_GET_TARGET:
        default:
            abort();
        }
    }
    void BytecodeGenerateContext::EvalResult::set_result_const(
        BytecodeGenerateContext& ctx, const ast::ConstantValue& result) noexcept
    {
        wo_assert(m_result_type == ResultKind::PENDING);

        switch (m_request)
        {
        case Request::GET_BOXED_RESULT_FOR_READONLY:
            m_result_type = ResultKind::RESULT_STACK_TEMP;
            m_result_stack = const_cast<woort_IRValue*>(ctx.c().load_imm_box_const(result));
            break;
        case Request::GET_RESULT_FOR_READONLY:
            m_result_type = ResultKind::RESULT_STACK_TEMP;
            m_result_stack = const_cast<woort_IRValue*>(ctx.c().load_imm_const(result));
            break;
        case Request::PUSH_BOXED_RESULT_AND_IGNORE:
            ctx.c().pushchk(ctx.c().load_imm_box_const(result));
            break;
        case Request::PUSH_RESULT_AND_IGNORE:
            ctx.c().pushchk(ctx.c().load_imm_const(result));
            break;
        case Request::ASSIGN_TO_TARGET_AND_GET_TARGET:
        case Request::ASSIGN_BOXED_TO_TARGET_AND_GET_TARGET:
        default:
            abort();
        }
    }
    void BytecodeGenerateContext::EvalResult::set_result_const_idx_no_need_box(
        BytecodeGenerateContext& ctx,
        woort_IRConstantIndex result) noexcept
    {
        wo_assert(m_result_type == ResultKind::PENDING);

        switch (m_request)
        {
        case Request::GET_BOXED_RESULT_FOR_READONLY:
        case Request::GET_RESULT_FOR_READONLY:
            m_result_type = ResultKind::RESULT_STACK_TEMP;
            m_result_stack = const_cast<woort_IRValue*>(ctx.c().fetch_constant(result));
            break;
        case Request::PUSH_BOXED_RESULT_AND_IGNORE:
        case Request::PUSH_RESULT_AND_IGNORE:
            ctx.c().pushchk(ctx.c().fetch_constant(result));
            break;
        case Request::ASSIGN_TO_TARGET_AND_GET_TARGET:
        case Request::ASSIGN_BOXED_TO_TARGET_AND_GET_TARGET:
        default:
            abort();
        }
    }
    void BytecodeGenerateContext::EvalResult::set_result_junk(BytecodeGenerateContext& ctx) noexcept
    {
        wo_assert(m_result_type == ResultKind::PENDING);

        switch (m_request)
        {
        case Request::GET_BOXED_RESULT_FOR_READONLY:
        case Request::GET_RESULT_FOR_READONLY:
            m_result_type = ResultKind::RESULT_STACK_TEMP;
            m_result_stack = const_cast<woort_IRValue*>(ctx.c().load_imm_nil());
            break;
        case Request::PUSH_BOXED_RESULT_AND_IGNORE:
        case Request::PUSH_RESULT_AND_IGNORE:
            ctx.c().pushchk(ctx.c().load_imm_nil());
            break;
        case Request::ASSIGN_TO_TARGET_AND_GET_TARGET:
        case Request::ASSIGN_BOXED_TO_TARGET_AND_GET_TARGET:
        default:
            abort();
        }
    }
#endif
}
