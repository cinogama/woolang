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

    struct hash_type_holder_for_origin_cache
    {
        using AstTypeHolderPT = ast::AstTypeHolder*;

        template<typename T>
        static size_t _hash_ptr(T* ptr)
        {
            size_t result = (size_t)(uintptr_t)ptr;
            result = result >> 4 | result << (sizeof(size_t) * 8 - 4);
            return result;
        }
        static size_t _hash_rawtype(const AstTypeHolderPT& node)
        {
            return _hash_ptr(node->m_LANG_determined_type.value());
        }
        static size_t _hash(const AstTypeHolderPT& node)
        {
            switch (node->m_formal)
            {
            case ast::AstTypeHolder::IDENTIFIER:
            {
                size_t hash_result = _hash_ptr(node->m_identifier->m_LANG_determined_symbol.value());

                if (node->m_identifier->m_template_arguments)
                {
                    for (auto* template_argument : node->m_identifier->m_template_arguments.value())
                        hash_result ^= _hash_rawtype(template_argument);
                }
                return hash_result | ast::AstTypeHolder::IDENTIFIER;
            }
            case ast::AstTypeHolder::FUNCTION:
            {
                size_t hash_result = _hash_rawtype(node->m_function.m_return_type);

                for (auto* param_type : node->m_function.m_parameters)
                    hash_result ^= _hash_rawtype(param_type);

                if (node->m_function.m_is_variadic)
                    hash_result = ~hash_result;

                return hash_result | ast::AstTypeHolder::FUNCTION;
            }
            case ast::AstTypeHolder::STRUCTURE:
            {
                size_t hash_result = 0;

                for (auto& member : node->m_structure.m_fields)
                {
                    hash_result ^= _hash_ptr(member->m_name);
                    hash_result ^= _hash_rawtype(member->m_type);
                    hash_result ^= member->m_attribute
                        ? member->m_attribute.value()
                        : ast::AstDeclareAttribue::accessc_attrib::PRIVATE;
                }

                return hash_result | ast::AstTypeHolder::STRUCTURE;
            }
            case ast::AstTypeHolder::TUPLE:
            {
                size_t hash_result = 0;

                for (auto* member : node->m_tuple.m_fields)
                    hash_result ^= _hash_rawtype(member);

                return hash_result | ast::AstTypeHolder::TUPLE;
            }
            case ast::AstTypeHolder::UNION:
            {
                size_t hash_result = 0;

                for (auto& member : node->m_union.m_fields)
                {
                    hash_result ^= _hash_ptr(member.m_label);
                    if (member.m_item)
                        hash_result ^= _hash_rawtype(member.m_item.value());
                }

                return hash_result | ast::AstTypeHolder::UNION;
            }
            default:
                wo_error("Unknown type holder type.");
                return 0;
            }
        }

        bool operator()(const AstTypeHolderPT& node) const
        {
            return _hash(node);
        }
    };

    struct equal_to_type_holder_for_origin_cache
    {
        using AstTypeHolderPT = ast::AstTypeHolder*;

        static size_t _equal_to_rawtype(const AstTypeHolderPT& lhs, const AstTypeHolderPT& rhs)
        {
            return lhs->m_LANG_determined_type.value() == rhs->m_LANG_determined_type.value();
        }
        static size_t _equal_to(const AstTypeHolderPT& lhs, const AstTypeHolderPT& rhs)
        {
            if (lhs->m_formal != rhs->m_formal)
                return false;

            switch (lhs->m_formal)
            {
            case ast::AstTypeHolder::IDENTIFIER:
            {
                if (lhs->m_identifier->m_LANG_determined_symbol.value() 
                    != rhs->m_identifier->m_LANG_determined_symbol.value())
                    return false;

                if (lhs->m_identifier->m_template_arguments.has_value()
                    != rhs->m_identifier->m_template_arguments.has_value())
                    return false;

                if (lhs->m_identifier->m_template_arguments)
                {
                    auto& lhs_template_args = lhs->m_identifier->m_template_arguments.value();
                    auto& rhs_template_args = rhs->m_identifier->m_template_arguments.value();

                    if (lhs_template_args.size() != rhs_template_args.size())
                        return false;

                    auto lhs_iter = lhs_template_args.begin();
                    auto rhs_iter = rhs_template_args.begin();
                    auto lhs_end = lhs_template_args.end();

                    for (; lhs_iter != lhs_end; ++lhs_iter, ++rhs_iter)
                    {
                        if (!_equal_to_rawtype(*lhs_iter, *rhs_iter))
                            return false;
                    }
                    
                }

                return true;
            }
            case ast::AstTypeHolder::FUNCTION:
            {
                if (lhs->m_function.m_is_variadic != rhs->m_function.m_is_variadic)
                    return false;

                if (lhs->m_function.m_parameters.size() != rhs->m_function.m_parameters.size())
                    return false;

                if (!_equal_to_rawtype(lhs->m_function.m_return_type, rhs->m_function.m_return_type))
                    return false;

                auto lhs_iter = lhs->m_function.m_parameters.begin();
                auto rhs_iter = rhs->m_function.m_parameters.begin();
                auto lhs_end = lhs->m_function.m_parameters.end();

                for (; lhs_iter != lhs_end; ++lhs_iter, ++rhs_iter)
                {
                    if (!_equal_to_rawtype(*lhs_iter, *rhs_iter))
                        return false;
                }

                return true;
            }
            case ast::AstTypeHolder::STRUCTURE:
            {
                if (lhs->m_structure.m_fields.size() != rhs->m_structure.m_fields.size())
                    return false;

                auto lhs_iter = lhs->m_structure.m_fields.begin();
                auto rhs_iter = rhs->m_structure.m_fields.begin();
                auto lhs_end = lhs->m_structure.m_fields.end();

                for (; lhs_iter != lhs_end; ++lhs_iter, ++rhs_iter)
                {
                    if ((*lhs_iter)->m_name != (*rhs_iter)->m_name)
                        return false;

                    if (!_equal_to_rawtype((*lhs_iter)->m_type, (*rhs_iter)->m_type))
                        return false;

                    if ((*lhs_iter)->m_attribute != (*rhs_iter)->m_attribute)
                        return false;
                }
            }
            case ast::AstTypeHolder::TUPLE:
            {
                if (lhs->m_tuple.m_fields.size() != rhs->m_tuple.m_fields.size())
                    return false;

                auto lhs_iter = lhs->m_tuple.m_fields.begin();
                auto rhs_iter = rhs->m_tuple.m_fields.begin();
                auto lhs_end = lhs->m_tuple.m_fields.end();

                for (; lhs_iter != lhs_end; ++lhs_iter, ++rhs_iter)
                {
                    if (!_equal_to_rawtype(*lhs_iter, *rhs_iter))
                        return false;
                }

                return true;
            }
            case ast::AstTypeHolder::UNION:
            {
                if (lhs->m_union.m_fields.size() != rhs->m_union.m_fields.size())
                    return false;

                auto lhs_iter = lhs->m_union.m_fields.begin();
                auto rhs_iter = rhs->m_union.m_fields.begin();
                auto lhs_end = lhs->m_union.m_fields.end();

                for (; lhs_iter != lhs_end; ++lhs_iter, ++rhs_iter)
                {
                    if ((*lhs_iter).m_label != (*rhs_iter).m_label)
                        return false;

                    if ((*lhs_iter).m_item.has_value() != (*rhs_iter).m_item.has_value())
                        return false;

                    if ((*lhs_iter).m_item.has_value())
                    {
                        if (!_equal_to_rawtype((*lhs_iter).m_item.value(), (*rhs_iter).m_item.value()))
                            return false;
                    }
                }

                return true;
            }
            default:
                wo_error("Unknown type holder type.");
                return false;
            }
        }

        bool operator()(const AstTypeHolderPT& lhs, const AstTypeHolderPT& rhs) const
        {
            return _equal_to(lhs, rhs);
        }
    };

    struct lang_Namespace;
    struct lang_Scope;
    struct lang_Symbol;
    struct lang_TypeInstance;

    struct lang_TypeInstance
    {
        struct DeterminedType
        {
            enum base_type
            {
                VOID,
                NOTHING,
                NIL,
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
                lang_TypeInstance* m_element_type;
            };
            struct DictionaryOrMapping
            {
                lang_TypeInstance* m_key_type;
                lang_TypeInstance* m_value_type;
            };
            struct Tuple
            {
                std::list<lang_TypeInstance*> m_element_types;
            };
            struct Struct
            {
                struct StructMember
                {
                    wo_integer_t              m_offset;
                    lang_TypeInstance* m_member_type;
                };
                std::unordered_map<wo_pstring_t, StructMember>
                    m_member_types;
            };
            struct Function
            {
                bool                            m_is_variadic;
                std::vector<lang_TypeInstance*> m_param_types;
                lang_TypeInstance* m_return_type;
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

        using DeterminedOrMutableType = std::variant<DeterminedType, lang_TypeInstance*>;

        // NOTE: DeterminedType means immutable type, lang_TypeInstance* means mutable types.
        std::optional<DeterminedOrMutableType> m_determined_type;

        lang_TypeInstance(lang_Symbol* symbol);
        lang_TypeInstance(const lang_TypeInstance&) = delete;
        lang_TypeInstance(lang_TypeInstance&&) = delete;
        lang_TypeInstance& operator=(const lang_TypeInstance&) = delete;
        lang_TypeInstance& operator=(lang_TypeInstance&&) = delete;

        bool is_immutable() const;
        bool is_mutable() const;
        const DeterminedType* get_determined_type() const;
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

        lang_ValueInstance(bool mutable_, lang_Symbol* symbol);
        ~lang_ValueInstance();

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
            bool m_mutable;
            std::list<wo_pstring_t> m_template_params;
            ast::AstValueBase* m_origin_value_ast;
            std::map<TemplateArgumentSetT, std::unique_ptr<lang_ValueInstance*>>
                m_template_instances;

            TemplateValuePrefab(
                bool mutable_, 
                ast::AstValueBase* ast, 
                const std::list<wo_pstring_t>& template_params);

            TemplateValuePrefab(const TemplateValuePrefab&) = delete;
            TemplateValuePrefab(TemplateValuePrefab&&) = delete;
            TemplateValuePrefab& operator=(const TemplateValuePrefab&) = delete;
            TemplateValuePrefab& operator=(TemplateValuePrefab&&) = delete;

        };
        struct TemplateTypePrefab
        {
            std::list<wo_pstring_t> m_template_params;
            ast::AstTypeHolder* m_origin_value_ast;
            std::map<TemplateArgumentSetT, std::unique_ptr<lang_TypeInstance*>>
                m_template_instances;

            TemplateTypePrefab(ast::AstTypeHolder* ast, const std::list<wo_pstring_t>& template_params);

            TemplateTypePrefab(const TemplateTypePrefab&) = delete;
            TemplateTypePrefab(TemplateTypePrefab&&) = delete;
            TemplateTypePrefab& operator=(const TemplateTypePrefab&) = delete;
            TemplateTypePrefab& operator=(TemplateTypePrefab&&) = delete;
        };
        struct TemplateAliasPrefab
        {
            std::list<wo_pstring_t> m_template_params;
            ast::AstTypeHolder* m_origin_value_ast;
            std::map<TemplateArgumentSetT, std::unique_ptr<lang_AliasInstance*>>
                m_template_instances;

            TemplateAliasPrefab(ast::AstTypeHolder* ast, const std::list<wo_pstring_t>& template_params);
            TemplateAliasPrefab(const TemplateAliasPrefab&) = delete;
            TemplateAliasPrefab(TemplateAliasPrefab&&) = delete;
            TemplateAliasPrefab& operator=(const TemplateAliasPrefab&) = delete;
            TemplateAliasPrefab& operator=(TemplateAliasPrefab&&) = delete;
        };

        kind                            m_symbol_kind;
        bool                            m_is_template;
        wo_pstring_t                    m_defined_source;
        std::optional<ast::AstDeclareAttribue*>
                                        m_declare_attribute;
        lang_Scope*                     m_belongs_to_scope;
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
            const std::optional<ast::AstDeclareAttribue*>& attr,
            wo_pstring_t src_location,
            lang_Scope* scope,
            kind kind,
            bool mutable_variable);
        lang_Symbol(
            const std::optional<ast::AstDeclareAttribue*>& attr,
            wo_pstring_t src_location,
            lang_Scope* scope,
            ast::AstValueBase* template_value_base,
            const std::list<wo_pstring_t>& template_params,
            bool mutable_variable);
        lang_Symbol(
            const std::optional<ast::AstDeclareAttribue*>& attr,
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

        struct OriginTypeHolder
        {
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
            OriginNoTemplateSymbolAndInstance   m_nil;
            OriginNoTemplateSymbolAndInstance   m_int;
            OriginNoTemplateSymbolAndInstance   m_real;
            OriginNoTemplateSymbolAndInstance   m_handle;
            OriginNoTemplateSymbolAndInstance   m_bool;
            OriginNoTemplateSymbolAndInstance   m_string;
            lang_Symbol* m_dictionary;
            lang_Symbol* m_mapping;
            lang_Symbol* m_array;
            lang_Symbol* m_vector;
            lang_Symbol* m_tuple;
            lang_Symbol* m_function;
            lang_Symbol* m_struct;
            lang_Symbol* m_union;

            using OriginTypeInstanceMapping = 
                std::unordered_map<
                ast::AstTypeHolder*, 
                lang_TypeInstance*,
                hash_type_holder_for_origin_cache,
                equal_to_type_holder_for_origin_cache>;

            OriginTypeInstanceMapping m_origin_cached_types;
            std::optional<lang_TypeInstance*> create_or_find_origin_type(lexer& lex, ast::AstTypeHolder* type_holder);

            std::optional<lang_TypeInstance*> create_dictionary_type(lexer& lex, ast::AstTypeHolder* type_holder);
            std::optional<lang_TypeInstance*> create_mapping_type(lexer& lex, ast::AstTypeHolder* type_holder);
            std::optional<lang_TypeInstance*> create_array_type(lexer& lex, ast::AstTypeHolder* type_holder);
            std::optional<lang_TypeInstance*> create_vector_type(lexer& lex, ast::AstTypeHolder* type_holder);
            std::optional<lang_TypeInstance*> create_tuple_type(lexer& lex, ast::AstTypeHolder* type_holder);
            std::optional<lang_TypeInstance*> create_function_type(lexer& lex, ast::AstTypeHolder* type_holder);
            std::optional<lang_TypeInstance*> create_struct_type(lexer& lex, ast::AstTypeHolder* type_holder);
            std::optional<lang_TypeInstance*> create_union_type(lexer& lex, ast::AstTypeHolder* type_holder);

            OriginTypeHolder();
            ~OriginTypeHolder();
        };
        OriginTypeHolder                m_origin_types;
        std::unique_ptr<lang_Namespace> m_root_namespace;
        std::stack<lang_Scope*>         m_scope_stack;
        std::unordered_map<lang_TypeInstance*, std::unique_ptr<lang_TypeInstance>> 
                                        m_mutable_type_instance_cache;

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
    (V == HOLD_BUT_CHILD_FAILED ? FAILED : VAL)
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
        void end_last_scope();
        bool begin_new_namespace(wo_pstring_t name);
        void end_last_namespace();

        std::optional<lang_Symbol*>
            _search_symbol_from_current_scope(lexer& lex, ast::AstIdentifier* ident);
        std::optional<lang_Symbol*>
            find_symbol_in_current_scope(lexer& lex, ast::AstIdentifier* ident);

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

        bool declare_pattern_symbol_pass0_1(
            lexer& lex,
            const std::optional<ast::AstDeclareAttribue*>& decl_attrib,
            ast::AstPatternBase* pattern,
            const std::optional<ast::AstValueBase*>& init_value);
        bool update_pattern_symbol_variable_type_pass1(
            lexer& lex, 
            ast::AstPatternBase* pattern, 
            const std::optional<ast::AstValueBase*>& init_value,
            lang_TypeInstance* init_value_type);

        lang_TypeInstance* mutable_type(lang_TypeInstance* origin_type);
        lang_TypeInstance* immutable_type(lang_TypeInstance* origin_type);
        //////////////////////////////////////
    };
#endif
}