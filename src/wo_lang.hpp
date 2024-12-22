#pragma once

#include "wo_basic_type.hpp"
#include "wo_lang_ast.hpp"
#include "wo_compiler_ir.hpp"

#include <memory>
#include <unordered_map>
#include <set>
#include <variant>
#include <stack>

namespace wo
{
#ifndef WO_DISABLE_COMPILER
    struct lang_Namespace;
    struct lang_Scope;

    struct lang_TypeInstance
    {
        struct DeterminedType
        {
            enum base_type
            {
                INTEGER,
                REAL,
                HANDLE,
                BOOLEAN,
                STRING,
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
                lang_TypeInstance*              m_element_type;
            };
            struct DictionaryOrMapping
            {
                lang_TypeInstance*              m_key_type;
                lang_TypeInstance*              m_value_type;
            };
            struct Tuple
            {
                std::vector<lang_TypeInstance*> m_element_types;
            };
            struct Struct
            {
                std::unordered_map<wo_pstring_t, lang_TypeInstance*> 
                                                m_member_types;
            };
            struct Function
            {
                bool                            m_is_variadic;
                std::vector<lang_TypeInstance*> m_param_types;
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

            base_type m_base_type;
            ExternalTypeDescription m_external_type_description;

            DeterminedType(base_type type, const ExternalTypeDescription& desc);
            ~DeterminedType();

            DeterminedType(const DeterminedType&) = delete;
            DeterminedType(DeterminedType&&) = delete;
            DeterminedType& operator=(const DeterminedType&) = delete;
            DeterminedType& operator=(DeterminedType&&) = delete;
        };

        std::optional<DeterminedType> m_determined_type;

        lang_TypeInstance();
        lang_TypeInstance(const lang_TypeInstance&) = delete;
        lang_TypeInstance(lang_TypeInstance&&) = delete;
        lang_TypeInstance& operator=(const lang_TypeInstance&) = delete;
        lang_TypeInstance& operator=(lang_TypeInstance&&) = delete;
    };
    struct lang_AliasInstance
    {
        std::optional<lang_TypeInstance*> m_determined_type;

        lang_AliasInstance();
        lang_AliasInstance(const lang_AliasInstance&) = delete;
        lang_AliasInstance(lang_AliasInstance&&) = delete;
        lang_AliasInstance& operator=(const lang_AliasInstance&) = delete;
        lang_AliasInstance& operator=(lang_AliasInstance&&) = delete;
    };
    struct lang_ValueInstance
    {
        std::optional<ast::AstValueBase*> m_determined_constant;
        std::optional<lang_TypeInstance*> m_determined_type;

        lang_ValueInstance();
        lang_ValueInstance(const lang_ValueInstance&) = delete;
        lang_ValueInstance(lang_ValueInstance&&) = delete;
        lang_ValueInstance& operator=(const lang_ValueInstance&) = delete;
        lang_ValueInstance& operator=(lang_ValueInstance&&) = delete;
    };
    struct lang_Symbol
    {
        enum kind
        {
            VARIABLE,
            TYPE,
            ALIAS,
        };

        using TemplateArgumentSetT = std::set<lang_TypeInstance*>;

        struct TemplateValuePrefab
        {
            lang_Scope* m_belongs_to_scope;
            std::list<wo_pstring_t> m_template_params;
            ast::AstValueBase*  m_origin_value_ast;
            std::map<TemplateArgumentSetT, std::unique_ptr<lang_ValueInstance*>>
                                m_template_instances;

            TemplateValuePrefab(lang_Scope* scope, ast::AstValueBase* ast, const std::list<wo_pstring_t>& template_params);

            TemplateValuePrefab(const TemplateValuePrefab&) = delete;
            TemplateValuePrefab(TemplateValuePrefab&&) = delete;
            TemplateValuePrefab& operator=(const TemplateValuePrefab&) = delete;
            TemplateValuePrefab& operator=(TemplateValuePrefab&&) = delete;

        };
        struct TemplateTypePrefab
        {
            lang_Scope* m_belongs_to_scope;
            std::list<wo_pstring_t> m_template_params;
            ast::AstTypeHolder* m_origin_value_ast;
            std::map<TemplateArgumentSetT, std::unique_ptr<lang_TypeInstance*>>
                                m_template_instances;

            TemplateTypePrefab(lang_Scope* scope, ast::AstTypeHolder* ast, const std::list<wo_pstring_t>& template_params);

            TemplateTypePrefab(const TemplateTypePrefab&) = delete;
            TemplateTypePrefab(TemplateTypePrefab&&) = delete;
            TemplateTypePrefab& operator=(const TemplateTypePrefab&) = delete;
            TemplateTypePrefab& operator=(TemplateTypePrefab&&) = delete;
        };
        struct TemplateAliasPrefab
        {
            lang_Scope* m_belongs_to_scope;
            std::list<wo_pstring_t> m_template_params;
            ast::AstTypeHolder* m_origin_value_ast;
            std::map<TemplateArgumentSetT, std::unique_ptr<lang_AliasInstance*>>
                        m_template_instances;

            TemplateAliasPrefab(lang_Scope* scope, ast::AstTypeHolder* ast, const std::list<wo_pstring_t>& template_params);
            TemplateAliasPrefab(const TemplateAliasPrefab&) = delete;
            TemplateAliasPrefab(TemplateAliasPrefab&&) = delete;
            TemplateAliasPrefab& operator=(const TemplateAliasPrefab&) = delete;
            TemplateAliasPrefab& operator=(TemplateAliasPrefab&&) = delete;
        };

        kind                            m_symbol_kind;
        bool                            m_is_template;
        union
        {
            TemplateTypePrefab*         m_template_type_instances;
            TemplateValuePrefab*        m_template_value_instances;
            TemplateAliasPrefab*        m_template_alias_instances;
            lang_TypeInstance*          m_type_instance;
            lang_ValueInstance*         m_value_instance;
            lang_AliasInstance*         m_alias_instance;
        };

        lang_Symbol(const lang_Symbol&) = delete;
        lang_Symbol(lang_Symbol&&) = delete;
        lang_Symbol& operator=(const lang_Symbol&) = delete;
        lang_Symbol& operator=(lang_Symbol&&) = delete;

        ~lang_Symbol();

        lang_Symbol(kind kind);
        lang_Symbol(lang_Scope* scope, ast::AstValueBase* template_value_base, const std::list<wo_pstring_t>& template_params);
        lang_Symbol(lang_Scope* scope, ast::AstTypeHolder* template_type_base, const std::list<wo_pstring_t>& template_params, bool is_alias);
    };

    struct lang_Scope
    {
        std::unordered_map<wo_pstring_t, std::unique_ptr<lang_Symbol>> 
                        m_defined_symbols;
        std::list<std::unique_ptr<lang_Scope>>  
                        m_sub_scopes;

        lang_Namespace* m_belongs_to_namespace;

        lang_Scope(lang_Namespace* belongs);

        lang_Scope(const lang_Scope&) = delete;
        lang_Scope(lang_Scope&&) = delete;
        lang_Scope& operator=(const lang_Scope&) = delete;
        lang_Scope& operator=(lang_Scope&&) = delete;
    };
    struct lang_Namespace
    {
        std::unordered_map<wo_pstring_t, std::unique_ptr<lang_Namespace>> 
                                                m_sub_namespaces;
        std::unique_ptr<lang_Scope>             m_this_scope;
        std::optional<lang_Namespace*>          m_parent_namespace;

        lang_Namespace(const std::optional<lang_Namespace*>& parent_namespace);

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

        std::unique_ptr<lang_Namespace> m_root_namespace;
        std::stack<lang_Scope*>         m_scope_stack;

        static ProcessAstJobs* m_pass0_processers;

        LangContext();

        bool anylize_pass(lexer& lex, ast::AstBase* root, const PassFunctionT& pass_function);
        pass_behavior pass_0_process_scope_and_non_local_defination(
            lexer& lex, const AstNodeWithState& node_state, PassProcessStackT& out_stack);
        
        bool process(lexer& lex, ast::AstBase* root);

        //////////////////////////////////////

        static void init_lang_processers();
        static void shutdown_lang_processers();

        static void init_pass0();

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
    (V == HOLD_BUT_CHILD_FAILED ? FAILED : VAL)
#define WO_CONTINUE_PROCESS(NODE)\
    out_stack.push(NODE)
#define WO_CONTINUE_PROCESS_LIST(LIST)\
    continue_process_childs(LIST, out_stack)

#define WO_PASS_PROCESSER(AST, PASSNAME)\
    pass_behavior WO_LANG_PROCESSER_NAME(AST, PASSNAME)(\
        lexer&, ast::AST*, pass_behavior, PassProcessStackT&);

        WO_PASS_PROCESSER(AstList, pass0);
        WO_PASS_PROCESSER(AstScope, pass0);
        WO_PASS_PROCESSER(AstNamespace, pass0);
        WO_PASS_PROCESSER(AstVariableDefines, pass0);
        WO_PASS_PROCESSER(AstAliasTypeDeclare, pass0);
        WO_PASS_PROCESSER(AstUsingTypeDeclare, pass0);
        WO_PASS_PROCESSER(AstEnumDeclare, pass0);
        WO_PASS_PROCESSER(AstUnionDeclare, pass0);

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
        void end_last_scope();
        bool begin_new_namespace(wo_pstring_t name);
        void end_last_namespace();

        template<typename ... ArgTs>
        std::optional<lang_Symbol*> define_symbol_in_current_scope(wo_pstring_t name, ArgTs&&...args)
        {
            auto& symbol_table = get_current_scope()->m_defined_symbols;
            if (symbol_table.find(name) != symbol_table.end())
                // Already defined.
                return std::nullopt;

            auto new_symbol = std::make_unique<lang_Symbol>(std::forward<ArgTs>(args)...);
            auto* new_symbol_ptr = new_symbol.get();
            symbol_table.insert(std::make_pair(name, std::move(new_symbol)));

            return new_symbol_ptr;
        }

        lang_Scope* get_current_scope();
        lang_Namespace* get_current_namespace();

        bool declare_pattern_symbol(lexer& lex, ast::AstPatternBase* pattern, std::optional<ast::AstValueBase*> init_value);

        //////////////////////////////////////
    };
#endif
}