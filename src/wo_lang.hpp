#pragma once

#include "wo_basic_type.hpp"
#include "wo_lang_ast.hpp"
#include "wo_compiler_ir.hpp"

#include <memory>
#include <unordered_map>
#include <set>
#include <variant>
#include <stack>
#include <tuple>

namespace wo
{

#ifndef WO_DISABLE_COMPILER
    struct lang_Namespace;
    struct lang_Scope;
    struct lang_Symbol;
  
    struct lang_TypeInstance
    {
        struct DeterminedType
        {
            enum base_type
            {
                VOID,
                NOTHING,
                DYNAMIC,
                NIL,
                INTEGER,
                REAL,
                HANDLE,
                BOOLEAN,
                STRING,
                GCHANDLE,
                DICTIONARY,
                MAPPING,
                ARRAY,
                VECTOR,
                TUPLE,
                FUNCTION,
                STRUCT,
                UNION,
            };
            struct ArrayOrVector
            {
                lang_TypeInstance* m_element_type;
            };
            struct DictionaryOrMapping
            {
                lang_TypeInstance* m_key_type;
                lang_TypeInstance* m_value_type;
            };
            struct Tuple
            {
                std::vector<lang_TypeInstance*> m_element_types;
            };
            struct Struct
            {
                struct StructMember
                {
                    ast::AstDeclareAttribue::accessc_attrib
                                            m_attrib;
                    wo_integer_t            m_offset;
                    lang_TypeInstance*      m_member_type;
                };
                std::unordered_map<wo_pstring_t, StructMember>
                    m_member_types;
            };
            struct Function
            {
                bool                            m_is_variadic;
                std::list<lang_TypeInstance*>   m_param_types;
                lang_TypeInstance*              m_return_type;
            };
            struct Union
            {
                struct UnionMember
                {
                    wo_integer_t               m_label;
                    std::optional<lang_TypeInstance*> m_item_type;
                };
                std::unordered_map<wo_pstring_t, UnionMember> m_union_label;
            };

            union ExternalTypeDescription
            {
                ArrayOrVector* m_array_or_vector;
                DictionaryOrMapping* m_dictionary_or_mapping;
                Tuple* m_tuple;
                Struct* m_struct;
                Function* m_function;
                Union* m_union;
            };

            base_type   m_base_type;
            ExternalTypeDescription m_external_type_description;

            DeterminedType(base_type type, const ExternalTypeDescription& desc);
            ~DeterminedType();

            DeterminedType(const DeterminedType&) = delete;
            DeterminedType& operator=(const DeterminedType&) = delete;

            DeterminedType(DeterminedType&&);
            DeterminedType& operator=(DeterminedType&&);
        };

        lang_Symbol* m_symbol;

        using DeterminedOrMutableType = 
            std::variant<std::optional<DeterminedType>, lang_TypeInstance*>;

        // NOTE: DeterminedType means immutable type, lang_TypeInstance* means mutable types.
        std::optional<std::list<lang_TypeInstance*>> m_instance_template_arguments;
        DeterminedOrMutableType m_determined_base_type_or_mutable;
        std::unordered_map<lang_TypeInstance*, bool> m_LANG_accepted_types;
        std::unordered_map<lang_TypeInstance*, bool> m_LANG_castfrom_types;
        std::unordered_set<lang_TypeInstance*> m_LANG_pending_depend_this;

        lang_TypeInstance(
            lang_Symbol* symbol,
            const std::optional<std::list<lang_TypeInstance*>>& template_arguments);

        lang_TypeInstance(const lang_TypeInstance&) = delete;
        lang_TypeInstance(lang_TypeInstance&&) = delete;
        lang_TypeInstance& operator=(const lang_TypeInstance&) = delete;
        lang_TypeInstance& operator=(lang_TypeInstance&&) = delete;

        bool is_immutable() const;
        bool is_mutable() const;
        std::optional<const DeterminedType*> get_determined_type() const;
        
        void determine_base_type_by_another_type(lang_TypeInstance* immutable_from_type);
        void _update_type_instance_depend_this(const DeterminedType& copy_type);
        void determine_base_type_copy(const DeterminedType& copy_type);
        void determine_base_type_move(DeterminedType&& move_type);
    };
    struct lang_AliasInstance
    {
        lang_Symbol* m_symbol;
        std::optional<lang_TypeInstance*> m_determined_type;

        lang_AliasInstance(lang_Symbol* symbol);
        lang_AliasInstance(const lang_AliasInstance&) = delete;
        lang_AliasInstance(lang_AliasInstance&&) = delete;
        lang_AliasInstance& operator=(const lang_AliasInstance&) = delete;
        lang_AliasInstance& operator=(lang_AliasInstance&&) = delete;
    };
    struct lang_ValueInstance
    {
        lang_Symbol* m_symbol;
        bool m_mutable;
        std::optional<wo::value> m_determined_constant;
        std::optional<lang_TypeInstance*> m_determined_type;

        void determined_value_instance(
            lang_TypeInstance* type,
            const std::optional<wo::value>& constant);

        lang_ValueInstance(bool mutable_, lang_Symbol* symbol);
        ~lang_ValueInstance();

        lang_ValueInstance(const lang_ValueInstance&) = delete;
        lang_ValueInstance(lang_ValueInstance&&) = delete;
        lang_ValueInstance& operator=(const lang_ValueInstance&) = delete;
        lang_ValueInstance& operator=(lang_ValueInstance&&) = delete;
    };

    struct lang_TemplateAstEvalStateBase
    {
        enum state
        {
            UNPROCESSED,
            EVALUATING,
            EVALUATED,
            FAILED,
        };
        state               m_state;
        ast::AstBase*       m_ast;
        lang_Symbol*        m_symbol;

        lang_TemplateAstEvalStateBase(lang_Symbol* symbol, ast::AstBase* ast);

        lang_TemplateAstEvalStateBase(const lang_TemplateAstEvalStateBase&) = delete;
        lang_TemplateAstEvalStateBase(lang_TemplateAstEvalStateBase&&) = delete;
        lang_TemplateAstEvalStateBase& operator=(const lang_TemplateAstEvalStateBase&) = delete;
        lang_TemplateAstEvalStateBase& operator=(lang_TemplateAstEvalStateBase&&) = delete;
    };
    struct lang_TemplateAstEvalStateValue : public lang_TemplateAstEvalStateBase
    {
        std::unique_ptr<lang_ValueInstance> m_value_instance;

        lang_TemplateAstEvalStateValue(lang_Symbol* symbol, ast::AstValueBase* ast);
    };
    struct lang_TemplateAstEvalStateType : public lang_TemplateAstEvalStateBase
    {
        std::unique_ptr<lang_TypeInstance> m_type_instance;

        lang_TemplateAstEvalStateType(
            lang_Symbol* symbol, 
            ast::AstTypeHolder* ast,
            const std::list<lang_TypeInstance*>& template_arguments);
    };
    struct lang_TemplateAstEvalStateAlias : public lang_TemplateAstEvalStateBase
    {
        std::unique_ptr<lang_AliasInstance> m_alias_instance;

        lang_TemplateAstEvalStateAlias(lang_Symbol* symbol, ast::AstTypeHolder* ast);
    };

    struct lang_Symbol
    {
        enum kind
        {
            VARIABLE,
            TYPE,
            ALIAS,
        };

        using TemplateArgumentListT = std::list<lang_TypeInstance*>;

        struct TemplateValuePrefab
        {
            lang_Symbol* m_symbol;
            bool m_mutable;
            std::list<wo_pstring_t> m_template_params;
            ast::AstValueBase* m_origin_value_ast;
            std::map<TemplateArgumentListT, std::unique_ptr<lang_TemplateAstEvalStateValue>>
                m_template_instances;

            TemplateValuePrefab(
                lang_Symbol* symbol,
                bool mutable_, 
                ast::AstValueBase* ast, 
                const std::list<wo_pstring_t>& template_params);

            lang_TemplateAstEvalStateValue* find_or_create_template_instance(
                const TemplateArgumentListT& template_args);

            lang_TemplateAstEvalStateValue* find_or_insert_te(
                const TemplateArgumentListT& template_args);

            TemplateValuePrefab(const TemplateValuePrefab&) = delete;
            TemplateValuePrefab(TemplateValuePrefab&&) = delete;
            TemplateValuePrefab& operator=(const TemplateValuePrefab&) = delete;
            TemplateValuePrefab& operator=(TemplateValuePrefab&&) = delete;

        };
        struct TemplateTypePrefab
        {
            lang_Symbol* m_symbol;
            std::list<wo_pstring_t> m_template_params;
            ast::AstTypeHolder* m_origin_value_ast;
            std::map<TemplateArgumentListT, std::unique_ptr<lang_TemplateAstEvalStateType>>
                m_template_instances;

            TemplateTypePrefab(
                lang_Symbol* symbol, 
                ast::AstTypeHolder* ast, 
                const std::list<wo_pstring_t>& template_params);

            lang_TemplateAstEvalStateType* find_or_create_template_instance(
                const TemplateArgumentListT& template_args);

            TemplateTypePrefab(const TemplateTypePrefab&) = delete;
            TemplateTypePrefab(TemplateTypePrefab&&) = delete;
            TemplateTypePrefab& operator=(const TemplateTypePrefab&) = delete;
            TemplateTypePrefab& operator=(TemplateTypePrefab&&) = delete;
        };
        struct TemplateAliasPrefab
        {
            lang_Symbol* m_symbol;
            std::list<wo_pstring_t> m_template_params;
            ast::AstTypeHolder* m_origin_value_ast;
            std::map<TemplateArgumentListT, std::unique_ptr<lang_TemplateAstEvalStateAlias>>
                m_template_instances;

            TemplateAliasPrefab(
                lang_Symbol* symbol, 
                ast::AstTypeHolder* ast,
                const std::list<wo_pstring_t>& template_params);

            lang_TemplateAstEvalStateAlias* find_or_create_template_instance(
                const TemplateArgumentListT& template_args);

            TemplateAliasPrefab(const TemplateAliasPrefab&) = delete;
            TemplateAliasPrefab(TemplateAliasPrefab&&) = delete;
            TemplateAliasPrefab& operator=(const TemplateAliasPrefab&) = delete;
            TemplateAliasPrefab& operator=(TemplateAliasPrefab&&) = delete;
        };

        kind                            m_symbol_kind;
        bool                            m_is_template;
        wo_pstring_t                    m_name;
        wo_pstring_t                    m_defined_source;
        std::optional<ast::AstDeclareAttribue*>
                                        m_declare_attribute;
        lang_Scope*                     m_belongs_to_scope;
        std::optional<ast::AstBase*>    m_symbol_declare_ast;
        bool                            m_is_builtin;

        union
        {
            TemplateTypePrefab* m_template_type_instances;
            TemplateValuePrefab* m_template_value_instances;
            TemplateAliasPrefab* m_template_alias_instances;
            lang_TypeInstance* m_type_instance;
            lang_ValueInstance* m_value_instance;
            lang_AliasInstance* m_alias_instance;
        };

        lang_Symbol(const lang_Symbol&) = delete;
        lang_Symbol(lang_Symbol&&) = delete;
        lang_Symbol& operator=(const lang_Symbol&) = delete;
        lang_Symbol& operator=(lang_Symbol&&) = delete;

        ~lang_Symbol();

        lang_Symbol(
            wo_pstring_t name,
            const std::optional<ast::AstDeclareAttribue*>& attr,
            std::optional<ast::AstBase*> symbol_declare_ast,
            wo_pstring_t src_location,
            lang_Scope* scope,
            kind kind,
            bool mutable_variable);
        lang_Symbol(
            wo_pstring_t name,
            const std::optional<ast::AstDeclareAttribue*>& attr,
            std::optional<ast::AstBase*> symbol_declare_ast,
            wo_pstring_t src_location,
            lang_Scope* scope,
            ast::AstValueBase* template_value_base,
            const std::list<wo_pstring_t>& template_params,
            bool mutable_variable);
        lang_Symbol(
            wo_pstring_t name,
            const std::optional<ast::AstDeclareAttribue*>& attr,
            std::optional<ast::AstBase*> symbol_declare_ast,
            wo_pstring_t src_location,
            lang_Scope* scope,
            ast::AstTypeHolder* template_type_base,
            const std::list<wo_pstring_t>& template_params,
            bool is_alias);
    };

    struct lang_Scope
    {
        std::unordered_map<wo_pstring_t, std::unique_ptr<lang_Symbol>>
            m_defined_symbols;
        std::list<std::unique_ptr<lang_Scope>>
            m_sub_scopes;

        std::optional<lang_Scope*> m_parent_scope;
        std::optional<ast::AstValueFunction*> m_function_instance;
        lang_Namespace* m_belongs_to_namespace;

        lang_Scope(const std::optional<lang_Scope*>& parent_scope, lang_Namespace* belongs);

        lang_Scope(const lang_Scope&) = delete;
        lang_Scope(lang_Scope&&) = delete;
        lang_Scope& operator=(const lang_Scope&) = delete;
        lang_Scope& operator=(lang_Scope&&) = delete;

        bool is_namespace_scope() const;
    };
    struct lang_Namespace
    {
        wo_pstring_t                            m_name;
        std::unordered_map<wo_pstring_t, std::unique_ptr<lang_Namespace>>
            m_sub_namespaces;
        std::unique_ptr<lang_Scope>             m_this_scope;
        std::optional<lang_Namespace*>          m_parent_namespace;

        lang_Namespace(wo_pstring_t name, const std::optional<lang_Namespace*>& parent_namespace);

        lang_Namespace(const lang_Scope&) = delete;
        lang_Namespace(lang_Scope&&) = delete;
        lang_Namespace& operator=(const lang_Scope&) = delete;
        lang_Namespace& operator=(lang_Scope&&) = delete;
    };

    struct LangContext
    {
        // Symbol & definition.
        enum pass_behavior
        {
            UNPROCESSED, // Not processed yet.

            OKAY,                   // Pop this ast node.
            HOLD,                   // Something need to be done.
            HOLD_BUT_CHILD_FAILED,  // Something need to be done, but child failed.
            FAILED,                 // Error occured.
        };
        struct AstNodeWithState
        {
            pass_behavior m_state;
            ast::AstBase* m_ast_node;

            AstNodeWithState(ast::AstBase* node);
            AstNodeWithState(pass_behavior state, ast::AstBase* node);

            AstNodeWithState(const AstNodeWithState&) = default;
            AstNodeWithState(AstNodeWithState&&) = default;
            AstNodeWithState& operator=(const AstNodeWithState&) = default;
            AstNodeWithState& operator=(AstNodeWithState&&) = default;
        };
        using PassProcessStackT = std::stack<AstNodeWithState>;
        using PassFunctionT = std::function<
            pass_behavior(LangContext*, lexer&, const AstNodeWithState&, PassProcessStackT&)>;

        class ProcessAstJobs
        {
            using NodeProcesserT = PassFunctionT;
            using TypeNodeProcesserMapping = std::unordered_map<ast::AstBase::node_type_t, NodeProcesserT>;

            TypeNodeProcesserMapping m_node_processer;

        public:
            template<typename T>
            void register_processer(
                ast::AstBase::node_type_t type,
                const std::function<pass_behavior(LangContext*, lexer&, T*, pass_behavior, PassProcessStackT&)> processer)
            {
                static_assert(std::is_base_of<ast::AstBase, T>::value);

                wo_assert(m_node_processer.find(type) == m_node_processer.end());
                m_node_processer.insert(std::make_pair(type,
                    [processer](LangContext* ctx, lexer& lex, const AstNodeWithState& node_state, PassProcessStackT& out_stack)
                    {
                        return processer(ctx, lex, static_cast<T*>(node_state.m_ast_node), node_state.m_state, out_stack);
                    }));
            }
            pass_behavior process_node(LangContext* ctx, lexer& lex, const AstNodeWithState& node_state, PassProcessStackT& out_stack);
        };

        struct OriginTypeHolder
        {
            struct OriginTypeChain
            {
                using TypeIndexChain = std::unordered_map<lang_TypeInstance*, std::unique_ptr<OriginTypeChain>>;
                
                TypeIndexChain                    m_chain;
                std::optional<lang_TypeInstance*> m_type_instance;

                OriginTypeChain* path(const std::list<lang_TypeInstance*>& type_path);
                ~OriginTypeChain();
            };
            struct UnionStructTypeIndexChain
            {
                using TypeIndexChain = std::unordered_map<lang_TypeInstance*, std::unique_ptr<UnionStructTypeIndexChain>>;
                using FieldNameIndexChain = std::unordered_map<wo_pstring_t, TypeIndexChain>;
                using AccessIndexChain = std::unordered_map<ast::AstDeclareAttribue::accessc_attrib, FieldNameIndexChain>;

                AccessIndexChain                  m_chain;
                std::optional<lang_TypeInstance*> m_type_instance;

                UnionStructTypeIndexChain* path(
                    const std::list<std::tuple<ast::AstDeclareAttribue::accessc_attrib, wo_pstring_t, lang_TypeInstance*>>& type_path);
                ~UnionStructTypeIndexChain();
            };

            OriginTypeChain m_array_chain;
            OriginTypeChain m_vector_chain;
            OriginTypeChain m_dictionary_chain;
            OriginTypeChain m_mapping_chain;
            OriginTypeChain m_tuple_chain;
            OriginTypeChain m_function_chain[2];
            UnionStructTypeIndexChain m_union_chain;
            UnionStructTypeIndexChain m_struct_chain;

            struct OriginNoTemplateSymbolAndInstance
            {
                lang_Symbol* m_symbol;
                lang_TypeInstance* m_type_instance;

                OriginNoTemplateSymbolAndInstance() = default;

                OriginNoTemplateSymbolAndInstance(const OriginNoTemplateSymbolAndInstance&) = delete;
                OriginNoTemplateSymbolAndInstance(OriginNoTemplateSymbolAndInstance&&) = delete;
                OriginNoTemplateSymbolAndInstance& operator=(const OriginNoTemplateSymbolAndInstance&) = delete;
                OriginNoTemplateSymbolAndInstance& operator=(OriginNoTemplateSymbolAndInstance&&) = delete;

            };
            OriginNoTemplateSymbolAndInstance   m_void;
            OriginNoTemplateSymbolAndInstance   m_nothing;
            OriginNoTemplateSymbolAndInstance   m_dynamic;

            OriginNoTemplateSymbolAndInstance   m_nil;
            OriginNoTemplateSymbolAndInstance   m_int;
            OriginNoTemplateSymbolAndInstance   m_real;
            OriginNoTemplateSymbolAndInstance   m_handle;
            OriginNoTemplateSymbolAndInstance   m_bool;
            OriginNoTemplateSymbolAndInstance   m_string;
            OriginNoTemplateSymbolAndInstance   m_gchandle;

            lang_TypeInstance* m_array_dynamic;
            lang_TypeInstance* m_dictionary_dynamic;

            lang_Symbol* m_dictionary;
            lang_Symbol* m_mapping;
            lang_Symbol* m_array;
            lang_Symbol* m_vector;
            lang_Symbol* m_tuple;
            lang_Symbol* m_function;
            lang_Symbol* m_struct;
            lang_Symbol* m_union;

            std::optional<lang_TypeInstance*> create_or_find_origin_type(lexer& lex, ast::AstTypeHolder* type_holder);

            lang_TypeInstance* create_dictionary_type(lang_TypeInstance* key_type, lang_TypeInstance* value_type);
            lang_TypeInstance* create_mapping_type(lang_TypeInstance* key_type, lang_TypeInstance* value_type);
            lang_TypeInstance* create_array_type(lang_TypeInstance* element_type);
            lang_TypeInstance* create_vector_type(lang_TypeInstance* element_type);
            lang_TypeInstance* create_tuple_type(const std::list<lang_TypeInstance*>& element_types);
            lang_TypeInstance* create_function_type(bool is_variadic, const std::list<lang_TypeInstance*>& param_types, lang_TypeInstance* return_type);
            lang_TypeInstance* create_struct_type(const std::list<std::tuple<ast::AstDeclareAttribue::accessc_attrib, wo_pstring_t, lang_TypeInstance*>>& member_types);
            lang_TypeInstance* create_union_type(const std::list<std::pair<wo_pstring_t, std::optional<lang_TypeInstance*>>>& member_types);

            OriginTypeHolder() = default;

            OriginTypeHolder(const OriginTypeHolder&) = delete;
            OriginTypeHolder(OriginTypeHolder&&) = delete;
            OriginTypeHolder& operator=(const OriginTypeHolder&) = delete;
            OriginTypeHolder& operator=(OriginTypeHolder&&) = delete;
        };
        OriginTypeHolder                m_origin_types;
        std::unique_ptr<lang_Namespace> m_root_namespace;
        std::stack<lang_Scope*>         m_scope_stack;
        std::unordered_map<lang_TypeInstance*, std::unique_ptr<lang_TypeInstance>> 
                                        m_mutable_type_instance_cache;

        // Used for print symbol & type names.
        std::unordered_map<lang_Symbol*, std::pair<std::wstring, std::string>>
                                        m_symbol_name_cache;
        std::unordered_map<lang_TypeInstance*, std::pair<std::wstring, std::string>>
                                        m_type_name_cache;

        static ProcessAstJobs* m_pass0_processers;
        static ProcessAstJobs* m_pass1_processers;

        LangContext();

        bool anylize_pass(lexer& lex, ast::AstBase* root, const PassFunctionT& pass_function);
        pass_behavior pass_0_process_scope_and_non_local_defination(
            lexer& lex, const AstNodeWithState& node_state, PassProcessStackT& out_stack);
        pass_behavior pass_1_process_basic_type_marking_and_constant_eval(
            lexer& lex, const AstNodeWithState& node_state, PassProcessStackT& out_stack);

        void pass_0_5_register_builtin_types();

        bool process(lexer& lex, ast::AstBase* root);

        //////////////////////////////////////

        static void init_lang_processers();
        static void shutdown_lang_processers();

        static void init_pass0();
        static void init_pass1();

        //////////////////////////////////////

#define WO_LANG_PROCESSER_NAME(AST, PASSNAME)\
        _##PASSNAME##_processer_##AST
#define WO_PASS_PROCESSER_IMPL(AST, PASSNAME)\
    LangContext::pass_behavior LangContext::WO_LANG_PROCESSER_NAME(AST, PASSNAME)(\
        lexer& lex, ast::AST* node, pass_behavior state, PassProcessStackT& out_stack)
#define WO_LANG_REGISTER_PROCESSER(AST, NODE_TYPE, PASSNAME)\
    m_##PASSNAME##_processers->register_processer<ast::AST>(\
        NODE_TYPE, &LangContext::WO_LANG_PROCESSER_NAME(AST, PASSNAME))
#define WO_EXCEPT_ERROR(V, VAL)\
    (V == HOLD_BUT_CHILD_FAILED\
        ? FAILED\
        : VAL)

#define WO_CONTINUE_PROCESS(NODE)\
    out_stack.push(NODE)
#define WO_CONTINUE_PROCESS_LIST(LIST)\
    continue_process_childs(LIST, out_stack)
#define WO_ALL_AST_LIST\
    WO_AST_MACRO(AstList);\
    WO_AST_MACRO(AstDeclareAttribue);\
    WO_AST_MACRO(AstIdentifier);\
    WO_AST_MACRO(AstStructFieldDefine);\
    WO_AST_MACRO(AstTypeHolder);\
    WO_AST_MACRO(AstValueBase);\
    WO_AST_MACRO(AstValueMarkAsMutable);\
    WO_AST_MACRO(AstValueMarkAsImmutable);\
    WO_AST_MACRO(AstValueLiteral);\
    WO_AST_MACRO(AstValueTypeid);\
    WO_AST_MACRO(AstValueTypeCast);\
    WO_AST_MACRO(AstValueTypeCheckIs);\
    WO_AST_MACRO(AstValueTypeCheckAs);\
    WO_AST_MACRO(AstValueVariable);\
    WO_AST_MACRO(AstWhereConstraints);\
    WO_AST_MACRO(AstValueFunctionCall);\
    WO_AST_MACRO(AstValueMayConsiderOperatorOverload);\
    WO_AST_MACRO(AstValueBinaryOperator);\
    WO_AST_MACRO(AstValueUnaryOperator);\
    WO_AST_MACRO(AstValueTribleOperator);\
    WO_AST_MACRO(AstFakeValueUnpack);\
    WO_AST_MACRO(AstValueVariadicArgumentsPack);\
    WO_AST_MACRO(AstValueIndex);\
    WO_AST_MACRO(AstPatternBase);\
    WO_AST_MACRO(AstPatternTakeplace);\
    WO_AST_MACRO(AstPatternSingle);\
    WO_AST_MACRO(AstPatternTuple);\
    WO_AST_MACRO(AstPatternUnion);\
    WO_AST_MACRO(AstPatternVariable);\
    WO_AST_MACRO(AstPatternIndex);\
    WO_AST_MACRO(AstVariableDefineItem);\
    WO_AST_MACRO(AstVariableDefines);\
    WO_AST_MACRO(AstFunctionParameterDeclare);\
    WO_AST_MACRO(AstValueFunction);\
    WO_AST_MACRO(AstValueArrayOrVec);\
    WO_AST_MACRO(AstKeyValuePair);\
    WO_AST_MACRO(AstValueDictOrMap);\
    WO_AST_MACRO(AstValueTuple);\
    WO_AST_MACRO(AstStructFieldValuePair);\
    WO_AST_MACRO(AstValueStruct);\
    WO_AST_MACRO(AstValueAssign);\
    WO_AST_MACRO(AstValuePackedArgs);\
    WO_AST_MACRO(AstNamespace);\
    WO_AST_MACRO(AstScope);\
    WO_AST_MACRO(AstMatchCase);\
    WO_AST_MACRO(AstMatch);\
    WO_AST_MACRO(AstIf);\
    WO_AST_MACRO(AstWhile);\
    WO_AST_MACRO(AstFor);\
    WO_AST_MACRO(AstForeach);\
    WO_AST_MACRO(AstBreak);\
    WO_AST_MACRO(AstContinue);\
    WO_AST_MACRO(AstReturn);\
    WO_AST_MACRO(AstLabeled);\
    WO_AST_MACRO(AstUsingTypeDeclare);\
    WO_AST_MACRO(AstAliasTypeDeclare);\
    WO_AST_MACRO(AstEnumItem);\
    WO_AST_MACRO(AstEnumDeclare);\
    WO_AST_MACRO(AstUnionItem);\
    WO_AST_MACRO(AstUnionDeclare);\
    WO_AST_MACRO(AstValueMakeUnion);\
    WO_AST_MACRO(AstUsingNamespace);\
    WO_AST_MACRO(AstToken);\
    WO_AST_MACRO(AstExternInformation)

#define WO_PASS_PROCESSER(AST, PASSNAME)\
    pass_behavior WO_LANG_PROCESSER_NAME(AST, PASSNAME)(\
        lexer&, ast::AST*, pass_behavior, PassProcessStackT&);

        WO_PASS_PROCESSER(AstList, pass0);
        WO_PASS_PROCESSER(AstScope, pass0);
        WO_PASS_PROCESSER(AstNamespace, pass0);
        WO_PASS_PROCESSER(AstVariableDefines, pass0);
        WO_PASS_PROCESSER(AstVariableDefineItem, pass0);
        WO_PASS_PROCESSER(AstAliasTypeDeclare, pass0);
        WO_PASS_PROCESSER(AstUsingTypeDeclare, pass0);
        WO_PASS_PROCESSER(AstEnumDeclare, pass0);
        WO_PASS_PROCESSER(AstUnionDeclare, pass0);

#define WO_AST_MACRO(AST)\
        WO_PASS_PROCESSER(AST, pass1)

        WO_ALL_AST_LIST;

#undef WO_AST_MACRO
#undef WO_PASS_PROCESSER

        //////////////////////////////////////

        template<typename T>
        void continue_process_childs(const std::list<T*>& ast_list, PassProcessStackT& out_stack)
        {
            static_assert(std::is_base_of<ast::AstBase, T>::value);
            auto r_end = ast_list.rend();
            for (auto it = ast_list.rbegin(); it != r_end; ++it)
                out_stack.push(AstNodeWithState(*it));
        }

        void begin_new_scope();
        void entry_spcify_scope(lang_Scope* scope);
        void end_last_scope();

        void begin_new_function(ast::AstValueFunction* func_instance);
        void end_last_function();

        bool begin_new_namespace(wo_pstring_t name);
        void entry_spcify_namespace(lang_Namespace* namespace_);
        void end_last_namespace();

        std::optional<lang_Symbol*>
            _search_symbol_from_current_scope(lexer& lex, ast::AstIdentifier* ident);
        std::optional<lang_Symbol*>
            find_symbol_in_current_scope(lexer& lex, ast::AstIdentifier* ident);

        template<typename ... ArgTs>
        std::optional<lang_Symbol*> define_symbol_in_current_scope(
            wo_pstring_t name, 
            const std::optional<ast::AstDeclareAttribue*>& attr,
            std::optional<ast::AstBase*> symbol_declare_ast,
            wo_pstring_t src_location,
            ArgTs&&...args)
        {
            auto& symbol_table = get_current_scope()->m_defined_symbols;
            if (symbol_table.find(name) != symbol_table.end())
                // Already defined.
                return std::nullopt;

            auto new_symbol = std::make_unique<lang_Symbol>(
                name, attr, symbol_declare_ast, src_location, std::forward<ArgTs>(args)...);
            auto* new_symbol_ptr = new_symbol.get();
            symbol_table.insert(std::make_pair(name, std::move(new_symbol)));

            return new_symbol_ptr;
        }

        lang_Scope* get_current_scope();
        std::optional<ast::AstValueFunction*> get_current_function();
        lang_Namespace* get_current_namespace();

        bool declare_pattern_symbol_pass0_1(
            lexer& lex,
            const std::optional<ast::AstDeclareAttribue*>& attribute,
            const std::optional<ast::AstBase*>& var_defines,
            ast::AstPatternBase* pattern,
            const std::optional<ast::AstValueBase*>& init_value);
        bool update_pattern_symbol_variable_type_pass1(
            lexer& lex, 
            ast::AstPatternBase* pattern, 
            const std::optional<ast::AstValueBase*>& init_value,
            // NOTE: If template pattern, init_value_type will not able to be determined.
            // So here is optional.
            const std::optional<lang_TypeInstance*>& init_value_type);

        lang_TypeInstance* mutable_type(lang_TypeInstance* origin_type);
        lang_TypeInstance* immutable_type(lang_TypeInstance* origin_type);

        void fast_create_template_type_alias_in_current_scope(
            lexer& lex, 
            wo_pstring_t source_location,
            const std::list<wo_pstring_t>& template_params,
            const std::list<lang_TypeInstance*>& template_args);

        //ast::AstValueFunction* begin_template_deduct_function(lang_Symbol* templating_symbol);
        //std::optional<lang_TemplateAstEvalStateValue*> begin_eval_template_ast_with_instance(
        //    lexer& lex,
        //    ast::AstBase* node,
        //    lang_Symbol* templating_symbol,
        //    ast::AstValueFunction* instance,
        //    const lang_Symbol::TemplateArgumentListT& template_arguments, 
        //    PassProcessStackT& out_stack);

        std::optional<lang_TemplateAstEvalStateBase*> begin_eval_template_ast(
            lexer& lex,
            ast::AstBase* node,
            lang_Symbol* templating_symbol,
            const lang_Symbol::TemplateArgumentListT& template_arguments, 
            PassProcessStackT& out_stack);

        void finish_eval_template_ast(
            lang_TemplateAstEvalStateBase* template_eval_instance);

        void failed_eval_template_ast(
            lang_TemplateAstEvalStateBase* template_eval_instance);

        //////////////////////////////////////
        bool is_type_accepted(
            lexer&lex, 
            ast::AstBase* node,
            lang_TypeInstance* accepter,
            lang_TypeInstance* provider);

        bool check_cast_able(
            lexer& lex,
            ast::AstBase* node,
            lang_TypeInstance* aimtype,
            lang_TypeInstance* srctype);

        std::wstring _get_scope_name(lang_Scope* scope);
        std::wstring _get_symbol_name(lang_Symbol* scope);
        std::wstring _get_type_name(lang_TypeInstance* scope);
        const wchar_t* get_symbol_name_w(lang_Symbol* symbol);
        const char* get_symbol_name(lang_Symbol* symbol);
        const wchar_t* get_type_name_w(lang_TypeInstance* type);
        const char* get_type_name(lang_TypeInstance* type);

        void template_type_deduction_extraction_with_complete_type(
            lexer& lex,
            ast::AstTypeHolder* accept_type_formal,
            lang_TypeInstance* applying_type_instance,
            const std::list<wo_pstring_t>& pending_template_params,
            std::unordered_map<wo_pstring_t, lang_TypeInstance*>* out_determined_template_arg_pair);

        void template_function_deduction_extraction_with_complete_type(
            lexer& lex, 
            ast::AstValueFunction* function_define,
            const std::list<std::optional<lang_TypeInstance*>>& argument_types,
            const std::optional< lang_TypeInstance*>& return_type,
            const std::list<wo_pstring_t>& pending_template_params,
            std::unordered_map<wo_pstring_t, lang_TypeInstance*>* out_determined_template_arg_pair
        );
        void LangContext::template_type_deduction_extraction_with_incomplete_type(
            lexer& lex,
            ast::AstTypeHolder* accept_type_formal,
            ast::AstTypeHolder* applying_type_formal,
            const std::list<wo_pstring_t>& pending_template_params,
            std::unordered_map<wo_pstring_t, ast::AstTypeHolder*>* out_determined_template_arg_pair);
        bool check_type_may_dependence_template_parameters(
            ast::AstTypeHolder* accept_type_formal,
            const std::list<wo_pstring_t>& pending_template_params);
    };
#endif
}