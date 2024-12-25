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
        std::optional<ast::AstBase*> symbol_declare_ast,
        wo_pstring_t src_location,
        lang_Scope* scope,
        kind kind,
        bool mutable_variable)
        : m_symbol_kind(kind)
        , m_is_template(false)
        , m_defined_source(src_location)
        , m_declare_attribute(attr)
        , m_belongs_to_scope(scope)
        , m_symbol_declare_ast(symbol_declare_ast)
        , m_is_builtin(false)
    {
        switch (kind)
        {
        case VARIABLE:
            m_value_instance = new lang_ValueInstance(mutable_variable, this);
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
        std::optional<ast::AstBase*> symbol_declare_ast,
        wo_pstring_t src_location,
        lang_Scope* scope,
        ast::AstValueBase* template_value_base,
        const std::list<wo_pstring_t>& template_params,
        bool mutable_variable)
        : m_symbol_kind(VARIABLE)
        , m_is_template(true)
        , m_defined_source(src_location)
        , m_declare_attribute(attr)
        , m_belongs_to_scope(scope)
        , m_symbol_declare_ast(symbol_declare_ast)
        , m_is_builtin(false)
    {
        m_template_value_instances = new TemplateValuePrefab(
            this, mutable_variable, template_value_base, template_params);
    }
    lang_Symbol::lang_Symbol(
        const std::optional<ast::AstDeclareAttribue*>& attr,
        std::optional<ast::AstBase*> symbol_declare_ast,
        wo_pstring_t src_location,
        lang_Scope* scope,
        ast::AstTypeHolder* template_type_base,
        const std::list<wo_pstring_t>& template_params,
        bool is_alias)
        : m_is_template(true)
        , m_defined_source(src_location)
        , m_declare_attribute(attr)
        , m_belongs_to_scope(scope)
        , m_symbol_declare_ast(symbol_declare_ast)
        , m_is_builtin(false)
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
        : m_symbol(symbol)
        , m_mutable(mutable_)
        , m_origin_value_ast(ast)
        , m_template_params(template_params)
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
            m_symbol, template_instance);

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
        : m_symbol(symbol)
        , m_origin_value_ast(ast)
        , m_template_params(template_params)
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
            m_symbol, template_instance);

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
        : m_symbol(symbol)
        , m_origin_value_ast(ast)
        , m_template_params(template_params)
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

    lang_TypeInstance::lang_TypeInstance(lang_Symbol* symbol)
        : m_symbol(symbol)
        , m_determined_type(std::nullopt)
    {
    }

    bool lang_TypeInstance::is_immutable() const
    {
        return nullptr != std::get_if<lang_TypeInstance::DeterminedType>(&m_determined_type.value());
    }
    bool lang_TypeInstance::is_mutable() const
    {
        return !is_immutable();
    }
    const lang_TypeInstance::DeterminedType* lang_TypeInstance::get_determined_type() const
    {
        auto* dtype = std::get_if<lang_TypeInstance::DeterminedType>(&m_determined_type.value());

        if (dtype != nullptr)
            return dtype;

        return &std::get<lang_TypeInstance::DeterminedType>(
            std::get<lang_TypeInstance*>(m_determined_type.value())->m_determined_type.value());
    }
    void lang_TypeInstance::determine_base_type(const DeterminedType* from_determined)
    {
        wo_assert(!m_determined_type);

        DeterminedType::ExternalTypeDescription extern_desc;
        switch (from_determined->m_base_type)
        {
        case DeterminedType::base_type::ARRAY:
        case DeterminedType::base_type::VECTOR:
            extern_desc.m_array_or_vector = new DeterminedType::ArrayOrVector(
                *from_determined->m_external_type_description.m_array_or_vector);
            break;
        case DeterminedType::base_type::DICTIONARY:
        case DeterminedType::base_type::MAPPING:
            extern_desc.m_dictionary_or_mapping = new DeterminedType::DictionaryOrMapping(
                *from_determined->m_external_type_description.m_dictionary_or_mapping);
            break;
        case DeterminedType::base_type::TUPLE:
            extern_desc.m_tuple = new DeterminedType::Tuple(
                *from_determined->m_external_type_description.m_tuple);
            break;
        case DeterminedType::base_type::FUNCTION:
            extern_desc.m_function = new DeterminedType::Function(
                *from_determined->m_external_type_description.m_function);
            break;
        case DeterminedType::base_type::STRUCT:
            extern_desc.m_struct = new DeterminedType::Struct(
                *from_determined->m_external_type_description.m_struct);
            break;
        default:
            wo_error("Unexpected base type.");
        }

        m_determined_type = std::move(DeterminedType(
            from_determined->m_base_type, extern_desc));
    }

    //////////////////////////////////////

    lang_TemplateAstEvalStateBase::lang_TemplateAstEvalStateBase(lang_Symbol* symbol, ast::AstBase* ast)
        : m_state(UNPROCESSED)
        , m_ast(ast)
        , m_symbol(symbol)
    {
    }

    //////////////////////////////////////

    lang_TemplateAstEvalStateValue::lang_TemplateAstEvalStateValue(lang_Symbol* symbol, ast::AstValueBase* ast)
        : lang_TemplateAstEvalStateBase(symbol, ast)
        , m_value_instance(std::nullopt)
    {
    }

    //////////////////////////////////////

    lang_TemplateAstEvalStateType::lang_TemplateAstEvalStateType(lang_Symbol* symbol, ast::AstTypeHolder* ast)
        : lang_TemplateAstEvalStateBase(symbol, ast)
        , m_type_instance(std::nullopt)
    {

    }

    //////////////////////////////////////

    lang_TemplateAstEvalStateAlias::lang_TemplateAstEvalStateAlias(lang_Symbol* symbol, ast::AstTypeHolder* ast)
        : lang_TemplateAstEvalStateBase(symbol, ast)
        , m_alias_instance(std::nullopt)
    {
    }

    //////////////////////////////////////

    lang_AliasInstance::lang_AliasInstance(lang_Symbol* symbol)
        : m_symbol(symbol)
        , m_determined_type(std::nullopt)
    {
    }

    //////////////////////////////////////

    void lang_ValueInstance::determined_value_instance(
        lang_TypeInstance* type,
        const std::optional<wo::value>& constant)
    {
        m_determined_type = type;

        if (constant)
        {
            m_determined_constant = wo::value();
            m_determined_constant.value().set_val_compile_time(&constant.value());
        }
    }
    lang_ValueInstance::lang_ValueInstance(bool mutable_, lang_Symbol* symbol)
        : m_symbol(symbol)
        , m_mutable(mutable_)
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

    LangContext::OriginTypeHolder::OriginTypeHolder()
        : m_dictionary(nullptr)
        , m_mapping(nullptr)
        , m_array(nullptr)
        , m_vector(nullptr)
        , m_tuple(nullptr)
        , m_function(nullptr)
        , m_struct(nullptr)
        , m_union(nullptr)
    {
    }
    LangContext::OriginTypeHolder::~OriginTypeHolder()
    {
        for (auto& [_, item] : m_origin_cached_types)
            delete item;
    }

    std::optional<lang_TypeInstance*> LangContext::OriginTypeHolder::create_dictionary_type(
        lexer& lex, ast::AstTypeHolder* type_holder)
    {
        wo_assert(type_holder->m_formal == ast::AstTypeHolder::IDENTIFIER);
        if (type_holder->m_identifier->m_template_arguments)
        {
            auto& template_arguments_list = type_holder->m_identifier->m_template_arguments.value();
            if (template_arguments_list.size() == 2)
            {
                lang_TypeInstance::DeterminedType::ExternalTypeDescription desc;
                desc.m_dictionary_or_mapping = new lang_TypeInstance::DeterminedType::DictionaryOrMapping();

                auto iter = template_arguments_list.begin();
                desc.m_dictionary_or_mapping->m_key_type = (*(iter++))->m_LANG_determined_type.value();
                desc.m_dictionary_or_mapping->m_value_type = (*iter)->m_LANG_determined_type.value();

                lang_TypeInstance::DeterminedType determined_type_detail(
                    lang_TypeInstance::DeterminedType::base_type::DICTIONARY, desc);

                lang_TypeInstance* new_type_instance = new lang_TypeInstance(m_dictionary);
                new_type_instance->m_determined_type = std::move(determined_type_detail);

                return new_type_instance;
            }
        }
        lex.lang_error(lexer::errorlevel::error, type_holder,
            WO_ERR_UNEXPECTED_TEMPLATE_COUNT,
            (size_t)2);

        return std::nullopt;
    }
    std::optional<lang_TypeInstance*> LangContext::OriginTypeHolder::create_mapping_type(
        lexer& lex, ast::AstTypeHolder* type_holder)
    {
        wo_assert(type_holder->m_formal == ast::AstTypeHolder::IDENTIFIER);
        if (type_holder->m_identifier->m_template_arguments)
        {
            auto& template_arguments_list = type_holder->m_identifier->m_template_arguments.value();
            if (template_arguments_list.size() == 2)
            {
                lang_TypeInstance::DeterminedType::ExternalTypeDescription desc;
                desc.m_dictionary_or_mapping = new lang_TypeInstance::DeterminedType::DictionaryOrMapping();

                auto iter = template_arguments_list.begin();
                desc.m_dictionary_or_mapping->m_key_type = (*(iter++))->m_LANG_determined_type.value();
                desc.m_dictionary_or_mapping->m_value_type = (*iter)->m_LANG_determined_type.value();

                lang_TypeInstance::DeterminedType determined_type_detail(
                    lang_TypeInstance::DeterminedType::base_type::MAPPING, desc);

                lang_TypeInstance* new_type_instance = new lang_TypeInstance(m_mapping);
                new_type_instance->m_determined_type = std::move(determined_type_detail);

                return new_type_instance;
            }
        }
        lex.lang_error(lexer::errorlevel::error, type_holder,
            WO_ERR_UNEXPECTED_TEMPLATE_COUNT,
            (size_t)2);

        return std::nullopt;
    }
    std::optional<lang_TypeInstance*> LangContext::OriginTypeHolder::create_array_type(
        lexer& lex, ast::AstTypeHolder* type_holder)
    {
        wo_assert(type_holder->m_formal == ast::AstTypeHolder::IDENTIFIER);
        if (type_holder->m_identifier->m_template_arguments)
        {
            auto& template_arguments_list = type_holder->m_identifier->m_template_arguments.value();
            if (template_arguments_list.size() == 1)
            {
                lang_TypeInstance::DeterminedType::ExternalTypeDescription desc;
                desc.m_array_or_vector = new lang_TypeInstance::DeterminedType::ArrayOrVector();

                desc.m_array_or_vector->m_element_type = template_arguments_list.front()->m_LANG_determined_type.value();

                lang_TypeInstance::DeterminedType determined_type_detail(
                    lang_TypeInstance::DeterminedType::base_type::ARRAY, desc);

                lang_TypeInstance* new_type_instance = new lang_TypeInstance(m_array);
                new_type_instance->m_determined_type = std::move(determined_type_detail);

                return new_type_instance;
            }
        }
        lex.lang_error(lexer::errorlevel::error, type_holder,
            WO_ERR_UNEXPECTED_TEMPLATE_COUNT,
            (size_t)1);

        return std::nullopt;
    }
    std::optional<lang_TypeInstance*> LangContext::OriginTypeHolder::create_vector_type(
        lexer& lex, ast::AstTypeHolder* type_holder)
    {
        wo_assert(type_holder->m_formal == ast::AstTypeHolder::IDENTIFIER);
        if (type_holder->m_identifier->m_template_arguments)
        {
            auto& template_arguments_list = type_holder->m_identifier->m_template_arguments.value();
            if (template_arguments_list.size() == 1)
            {
                lang_TypeInstance::DeterminedType::ExternalTypeDescription desc;
                desc.m_array_or_vector = new lang_TypeInstance::DeterminedType::ArrayOrVector();

                desc.m_array_or_vector->m_element_type = template_arguments_list.front()->m_LANG_determined_type.value();

                lang_TypeInstance::DeterminedType determined_type_detail(
                    lang_TypeInstance::DeterminedType::base_type::VECTOR, desc);

                lang_TypeInstance* new_type_instance = new lang_TypeInstance(m_vector);
                new_type_instance->m_determined_type = std::move(determined_type_detail);

                return new_type_instance;
            }
        }
        lex.lang_error(lexer::errorlevel::error, type_holder,
            WO_ERR_UNEXPECTED_TEMPLATE_COUNT,
            (size_t)1);

        return std::nullopt;
    }
    std::optional<lang_TypeInstance*> LangContext::OriginTypeHolder::create_tuple_type(
        lexer& lex, ast::AstTypeHolder* type_holder)
    {
        wo_assert(type_holder->m_formal == ast::AstTypeHolder::TUPLE);

        lang_TypeInstance::DeterminedType::ExternalTypeDescription desc;
        desc.m_tuple = new lang_TypeInstance::DeterminedType::Tuple();

        for (auto& field : type_holder->m_tuple.m_fields)
            desc.m_tuple->m_element_types.push_back(field->m_LANG_determined_type.value());

        lang_TypeInstance::DeterminedType determined_type_detail(
            lang_TypeInstance::DeterminedType::base_type::TUPLE, desc);

        lang_TypeInstance* new_type_instance = new lang_TypeInstance(m_tuple);
        new_type_instance->m_determined_type = std::move(determined_type_detail);

        return new_type_instance;
    }
    std::optional<lang_TypeInstance*> LangContext::OriginTypeHolder::create_function_type(
        lexer& lex, ast::AstTypeHolder* type_holder)
    {
        wo_assert(type_holder->m_formal == ast::AstTypeHolder::FUNCTION);

        lang_TypeInstance::DeterminedType::ExternalTypeDescription desc;
        desc.m_function = new lang_TypeInstance::DeterminedType::Function();

        desc.m_function->m_is_variadic = type_holder->m_function.m_is_variadic;

        for (auto& param_type : type_holder->m_function.m_parameters)
            desc.m_function->m_param_types.push_back(param_type->m_LANG_determined_type.value());

        desc.m_function->m_return_type = type_holder->m_function.m_return_type->m_LANG_determined_type.value();

        lang_TypeInstance::DeterminedType determined_type_detail(
            lang_TypeInstance::DeterminedType::base_type::FUNCTION, desc);

        lang_TypeInstance* new_type_instance = new lang_TypeInstance(m_function);
        new_type_instance->m_determined_type = std::move(determined_type_detail);

        return new_type_instance;
    }
    std::optional<lang_TypeInstance*> LangContext::OriginTypeHolder::create_struct_type(
        lexer& lex, ast::AstTypeHolder* type_holder)
    {
        wo_assert(type_holder->m_formal == ast::AstTypeHolder::STRUCTURE);

        lang_TypeInstance::DeterminedType::ExternalTypeDescription desc;
        desc.m_struct = new lang_TypeInstance::DeterminedType::Struct();

        wo_integer_t field_offset = 0;
        for (auto& field : type_holder->m_structure.m_fields)
        {
            lang_TypeInstance::DeterminedType::Struct::StructMember new_field;
            new_field.m_offset = field_offset++;
            new_field.m_member_type = field->m_type->m_LANG_determined_type.value();

            desc.m_struct->m_member_types.insert(std::make_pair(field->m_name, new_field));
        }

        lang_TypeInstance::DeterminedType determined_type_detail(
            lang_TypeInstance::DeterminedType::base_type::STRUCT, desc);

        lang_TypeInstance* new_type_instance = new lang_TypeInstance(m_struct);
        new_type_instance->m_determined_type = std::move(determined_type_detail);

        return new_type_instance;
    }
    std::optional<lang_TypeInstance*> LangContext::OriginTypeHolder::create_union_type(
        lexer& lex, ast::AstTypeHolder* type_holder)
    {
        wo_assert(type_holder->m_formal == ast::AstTypeHolder::UNION);

        lang_TypeInstance::DeterminedType::ExternalTypeDescription desc;
        desc.m_union = new lang_TypeInstance::DeterminedType::Union();

        wo_integer_t field_offset = 0;
        for (auto& field : type_holder->m_union.m_fields)
        {
            lang_TypeInstance::DeterminedType::Union::UnionMember new_field;
            new_field.m_label = field_offset++;
            if (field.m_item)
                new_field.m_item_type = field.m_item.value()->m_LANG_determined_type.value();
            else
                new_field.m_item_type = std::nullopt;

            desc.m_union->m_union_label.insert(std::make_pair(field.m_label, new_field));
        }

        lang_TypeInstance::DeterminedType determined_type_detail(
            lang_TypeInstance::DeterminedType::base_type::UNION, desc);

        lang_TypeInstance* new_type_instance = new lang_TypeInstance(m_union);
        new_type_instance->m_determined_type = std::move(determined_type_detail);

        return new_type_instance;
    }

    std::optional<lang_TypeInstance*> LangContext::OriginTypeHolder::create_or_find_origin_type(
        lexer& lex, ast::AstTypeHolder* type_holder)
    {
        wo_assert(!type_holder->m_LANG_determined_type);

        auto fnd = m_origin_cached_types.find(type_holder);
        if (fnd != m_origin_cached_types.end())
            return fnd->second;

        switch (type_holder->m_formal)
        {
        case ast::AstTypeHolder::IDENTIFIER:
        {
            auto* symbol = type_holder->m_identifier->m_LANG_determined_symbol.value();

            wo_assert(symbol->m_is_builtin);
            if (symbol == m_dictionary)
            {
                auto new_type_instance = create_dictionary_type(lex, type_holder);
                if (new_type_instance)
                    m_origin_cached_types.insert(std::make_pair(type_holder, new_type_instance.value()));

                return new_type_instance;
            }
            else if (symbol == m_mapping)
            {
                auto new_type_instance = create_mapping_type(lex, type_holder);
                if (new_type_instance)
                    m_origin_cached_types.insert(std::make_pair(type_holder, new_type_instance.value()));

                return new_type_instance;
            }
            else if (symbol == m_array)
            {
                auto new_type_instance = create_array_type(lex, type_holder);
                if (new_type_instance)
                    m_origin_cached_types.insert(std::make_pair(type_holder, new_type_instance.value()));

                return new_type_instance;
            }
            else if (symbol == m_vector)
            {
                auto new_type_instance = create_vector_type(lex, type_holder);
                if (new_type_instance)
                    m_origin_cached_types.insert(std::make_pair(type_holder, new_type_instance.value()));

                return new_type_instance;
            }
            else
            {
                wo_error("Unexpected builtin type.");
            }
        }
        case ast::AstTypeHolder::FUNCTION:
        {
            auto new_type_instance = create_function_type(lex, type_holder);
            if (new_type_instance)
                m_origin_cached_types.insert(std::make_pair(type_holder, new_type_instance.value()));

            return new_type_instance;
        }
        case ast::AstTypeHolder::STRUCTURE:
        {
            auto new_type_instance = create_struct_type(lex, type_holder);
            if (new_type_instance)
                m_origin_cached_types.insert(std::make_pair(type_holder, new_type_instance.value()));

            return new_type_instance;
        }
        case ast::AstTypeHolder::UNION:
        {
            auto new_type_instance = create_union_type(lex, type_holder);
            if (new_type_instance)
                m_origin_cached_types.insert(std::make_pair(type_holder, new_type_instance.value()));

            return new_type_instance;
        }
        case ast::AstTypeHolder::TUPLE:
        {
            auto new_type_instance = create_tuple_type(lex, type_holder);
            if (new_type_instance)
                m_origin_cached_types.insert(std::make_pair(type_holder, new_type_instance.value()));

            return new_type_instance;
        }
        default:
            wo_error("Unexpected type holder formal.");
        }
        return std::nullopt;
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

            if (top_state.m_state == HOLD
                || top_state.m_state == HOLD_BUT_CHILD_FAILED)
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
        if (node_state.m_ast_node->finished_state != pass_behavior::UNPROCESSED)
        {
            if (node_state.m_ast_node->finished_state == pass_behavior::HOLD)
            {
                if (node_state.m_state != pass_behavior::HOLD
                    && node_state.m_state != pass_behavior::HOLD_BUT_CHILD_FAILED)
                {
                    // RECURSIVE EVALUATION.
                    lex.lang_error(lexer::errorlevel::error, node_state.m_ast_node,
                        WO_ERR_RECURSIVE_EVAL_PASS1);

                    return FAILED;
                }
                else
                {
                    // CONTINUE PROCESSING.
                }
            }
            else
                return (LangContext::pass_behavior)node_state.m_ast_node->finished_state;
        }
        auto result = m_pass1_processers->process_node(this, lex, node_state, out_stack);

        node_state.m_ast_node->finished_state = result;

        wo_assert(result != pass_behavior::UNPROCESSED);

        return result;
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
                    false).value();

                out_sni->m_type_instance = out_sni->m_symbol->m_type_instance;
                out_sni->m_type_instance->m_determined_type =
                    std::move(lang_TypeInstance::DeterminedType(basic_type, {}));
            };
        create_builtin_non_template_symbol_and_instance(
            &m_origin_types.m_void, WO_PSTR(void), lang_TypeInstance::DeterminedType::VOID);
        create_builtin_non_template_symbol_and_instance(
            &m_origin_types.m_nothing, WO_PSTR(nothing), lang_TypeInstance::DeterminedType::NOTHING);
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
                    false).value();
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
    void LangContext::entry_spcify_namespace(lang_Namespace* namespace_)
    {
        entry_spcify_scope(namespace_->m_this_scope.get());
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

    lang_TypeInstance* LangContext::mutable_type(lang_TypeInstance* origin_type)
    {
        if (origin_type->is_immutable())
        {
            // Is immutable type.
            auto fnd = m_mutable_type_instance_cache.find(origin_type);
            if (fnd != m_mutable_type_instance_cache.end())
                return fnd->second.get();

            auto mutable_type_instance = std::make_unique<lang_TypeInstance>(origin_type->m_symbol);
            mutable_type_instance->m_determined_type = origin_type;

            auto* result = mutable_type_instance.get();

            m_mutable_type_instance_cache.insert(
                std::make_pair(origin_type, std::move(mutable_type_instance)));

            return result;
        }
        return origin_type;
    }
    lang_TypeInstance* LangContext::immutable_type(lang_TypeInstance* origin_type)
    {
        lang_TypeInstance* immutable_type =
            *std::get_if<lang_TypeInstance*>(&origin_type->m_determined_type.value());

        if (nullptr != immutable_type)
        {
            // Is mutable type.
            return immutable_type;
        }
        return origin_type;
    }
    void LangContext::fast_create_template_type_alias_in_current_scope(
        lexer& lex,
        lang_Symbol* for_symbol,
        const std::list<wo_pstring_t>& template_params,
        const std::list<lang_TypeInstance*>& template_args)
    {
        wo_assert(for_symbol->m_is_template);
        wo_assert(template_params.size() == template_args.size());

        auto params_iter = template_params.begin();
        auto args_iter = template_args.begin();
        auto params_end = template_params.end();

        for (; params_iter != params_end; ++params_iter, ++args_iter)
        {
            lang_Symbol* symbol = define_symbol_in_current_scope(
                *params_iter,
                std::nullopt,
                std::nullopt,
                for_symbol->m_defined_source,
                get_current_scope(),
                lang_Symbol::kind::ALIAS,
                false).value();

            symbol->m_alias_instance->m_determined_type = *args_iter;
        }
    }
#endif
}
