#pragma once
#include "wo_compiler_parser.hpp"
#include "wo_basic_type.hpp"

//#include "wo_meta.hpp"
//#include "wo_basic_type.hpp"
//#include "wo_env_locale.hpp"
//#include "wo_lang_extern_symbol_loader.hpp"
//#include "wo_source_file_manager.hpp"
//#include "wo_utf8.hpp"
//#include "wo_memory.hpp"
//#include "wo_const_string_pool.hpp"
//#include "wo_crc_64.hpp"

//#include <type_traits>
//#include <cmath>
//#include <unordered_map>
//#include <algorithm>

#include <optional>
#include <list>

namespace wo
{
#ifndef WO_DISABLE_COMPILER
    struct lang_TypeInstance;
    struct lang_ValueInstance;
    struct lang_Symbol;
    struct lang_Scope;
    struct lang_Namespace;
    struct lang_TemplateAstEvalStateBase;

    namespace ast
    {
        struct AstTypeHolder;
        struct AstValueBase;
        struct AstToken;

        using AstEmpty = grammar::AstEmpty;
        struct AstDeclareAttribue : public AstBase
        {
            enum lifecycle_attrib
            {
                STATIC,
            };
            enum accessc_attrib
            {
                PUBLIC,
                PROTECTED,
                PRIVATE,
            };
            enum external_attrib
            {
                EXTERNAL,
            };

            std::optional<lifecycle_attrib> m_lifecycle;
            std::optional<accessc_attrib>   m_access;
            std::optional<external_attrib>  m_external;

            bool modify_attrib(lexer& lex, AstToken* attrib_token);

            AstDeclareAttribue();
            AstDeclareAttribue(const AstDeclareAttribue&);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstIdentifier : public AstBase
        {
            enum identifier_formal
            {
                FROM_GLOBAL,
                FROM_CURRENT,
                FROM_TYPE,
            };
            identifier_formal       m_formal;
            std::optional<AstTypeHolder*>
                m_from_type;
            std::list<wo_pstring_t> m_scope;
            wo_pstring_t            m_name;
            std::optional<std::list<AstTypeHolder*>>
                m_template_arguments;

            std::optional<lang_Symbol*>
                m_LANG_determined_symbol;

            /*std::optional<lang_Scope*>
                                    m_LANG_determined_searching_from_scope;*/

            AstIdentifier(wo_pstring_t identifier);
            AstIdentifier(wo_pstring_t identifier, const std::optional<std::list<AstTypeHolder*>>& template_arguments);
            AstIdentifier(wo_pstring_t identifier, const std::optional<std::list<AstTypeHolder*>>& template_arguments, const std::list<wo_pstring_t>& scopes, bool from_global);
            AstIdentifier(wo_pstring_t identifier, const std::optional<std::list<AstTypeHolder*>>& template_arguments, const std::list<wo_pstring_t>& scopes, AstTypeHolder* from_type);

            AstIdentifier(const AstIdentifier& ident);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstStructFieldDefine : public AstBase
        {
            wo_pstring_t m_name;
            AstTypeHolder* m_type;
            std::optional<AstDeclareAttribue::accessc_attrib> m_attribute;

            AstStructFieldDefine(
                const std::optional<AstDeclareAttribue::accessc_attrib>& attrib,
                wo_pstring_t name,
                AstTypeHolder* type);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstTypeHolder : public AstBase
        {
            enum type_formal
            {
                IDENTIFIER,
                TYPEOF,
                FUNCTION,
                STRUCTURE,
                TUPLE,
                UNION,
            };
            enum mutable_mark
            {
                NONE,
                MARK_AS_MUTABLE,
                MARK_AS_IMMUTABLE,
            };

            struct FunctionType
            {
                bool m_is_variadic;
                std::list<AstTypeHolder*> m_parameters;
                AstTypeHolder* m_return_type;
            };
            struct StructureType
            {
                std::list<AstStructFieldDefine*>
                    m_fields;
            };
            struct TupleType
            {
                std::list<AstTypeHolder*>
                    m_fields;
            };
            struct UnionType
            {
                struct UnionField
                {
                    wo_pstring_t m_label;
                    std::optional<AstTypeHolder*> m_item;
                };
                std::list<UnionField> m_fields;
            };

            mutable_mark    m_mutable_mark;
            type_formal     m_formal;
            union TypeForm
            {
                AstIdentifier* m_identifier;
                AstValueBase* m_typefrom;
                FunctionType m_function;
                StructureType m_structure;
                TupleType m_tuple;
                UnionType m_union;

                TypeForm() {};
                ~TypeForm() {};
            };
            TypeForm        m_typeform;

            std::optional<lang_TemplateAstEvalStateBase*> m_LANG_template_evalating_state;
            std::optional<lang_TypeInstance*> m_LANG_determined_type;
            bool m_LANG_trying_advancing_type_judgement;

        public:
            AstTypeHolder(AstIdentifier* ident);
            AstTypeHolder(AstValueBase* expr);
            AstTypeHolder(const FunctionType& functype);
            AstTypeHolder(const StructureType& structtype);
            AstTypeHolder(const TupleType& tupletype);
            AstTypeHolder(const UnionType& tupletype);
            ~AstTypeHolder();

            bool _check_if_has_typeof() const;
            void _check_if_template_exist_in(const std::list<wo_pstring_t>& template_params, std::vector<bool>& out_contain_flags) const;
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstValueBase : public AstBase
        {
            std::optional<lang_TypeInstance*> m_LANG_determined_type;
            std::optional<wo::value> m_evaled_const_value;

            AstValueBase(AstBase::node_type_t nodetype);
            ~AstValueBase();

            // TBD: Not sure when and how to eval `type`.
            //using EvalTobeEvalConstList = std::list<AstValueBase*>;

            /*virtual void collect_eval_const_list(EvalTobeEvalConstList& out_vals) const = 0;
            virtual void eval_const_value() = 0;*/
            void decide_final_constant_value(const wo::value& val);
            void decide_final_constant_value(const std::string& cstr);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };

        struct AstValueMarkAsMutable : public AstValueBase
        {
            AstValueBase* m_marked_value;

            AstValueMarkAsMutable(AstValueBase* marking_value);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override final;
        };

        struct AstValueMarkAsImmutable : public AstValueBase
        {
            AstValueBase* m_marked_value;

            AstValueMarkAsImmutable(AstValueBase* marking_value);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override final;
        };

        struct AstValueLiteral : public AstValueBase
        {
            AstValueLiteral();
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override final;
        };
        struct AstValueTypeid : public AstValueBase
        {
            AstTypeHolder* m_id_type;

            AstValueTypeid(AstTypeHolder* id_type);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override final;
        };
        struct AstValueTypeCast : public AstValueBase
        {
            AstTypeHolder* m_cast_type;
            AstValueBase* m_cast_value;

            AstValueTypeCast(AstTypeHolder* cast_type, AstValueBase* cast_value);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override final;
        };
        struct AstValueTypeCheckIs : public AstValueBase
        {
            AstTypeHolder* m_check_type;
            AstValueBase* m_check_value;

            AstValueTypeCheckIs(AstTypeHolder* check_type, AstValueBase* check_value);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override final;
        };
        struct AstValueTypeCheckAs : public AstValueBase
        {
            AstTypeHolder* m_check_type;
            AstValueBase* m_check_value;

            AstValueTypeCheckAs(AstTypeHolder* check_type, AstValueBase* check_value);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override final;
        };
        struct AstValueVariable : public AstValueBase
        {
            AstIdentifier* m_identifier;

            std::optional<lang_TemplateAstEvalStateBase*> m_LANG_template_evalating_state;
            std::optional<lang_ValueInstance*> m_LANG_variable_instance;
            bool m_LANG_trying_advancing_type_judgement;

            AstValueVariable(AstIdentifier* variable_name);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override final;
        };
        struct AstWhereConstraints : public AstBase
        {
            std::list<AstValueBase*> m_constraints;

            AstWhereConstraints(const std::list<AstValueBase*>& constraints);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override final;
        };
        struct AstValueFunctionCall : public AstValueBase
        {
            bool m_is_direct_call;  // -> |> <|
            AstValueBase* m_function;
            std::list<AstValueBase*> m_arguments;

            AstValueFunctionCall(bool direct, AstValueBase* function, const std::list<AstValueBase*>& arguments);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override final;
        };

        struct AstValueMayConsiderOperatorOverload : public AstValueBase
        {
            std::optional<AstValueFunctionCall*> m_overload_call;

            AstValueMayConsiderOperatorOverload(AstBase::node_type_t nodetype);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstValueBinaryOperator : public AstValueMayConsiderOperatorOverload
        {
            enum operator_type
            {
                ADD,
                SUBSTRACT,
                MULTIPLY,
                DIVIDE,
                MODULO,

                LOGICAL_AND,
                LOGICAL_OR,
                GREATER,
                GREATER_EQUAL,
                LESS,
                LESS_EQUAL,
                EQUAL,
                NOT_EQUAL,
            };
            operator_type m_operator;
            AstValueBase* m_left;
            AstValueBase* m_right;

            AstValueBinaryOperator(operator_type op, AstValueBase* left, AstValueBase* right);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override final;
        };
        struct AstValueUnaryOperator : public AstValueBase
        {
            enum operator_type
            {
                NEGATIVE,
                LOGICAL_NOT,
            };
            operator_type m_operator;
            AstValueBase* m_operand;

            AstValueUnaryOperator(operator_type op, AstValueBase* operand);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override final;
        };
        struct AstValueTribleOperator : public AstValueBase
        {
            AstValueBase* m_condition;
            AstValueBase* m_true_value;
            AstValueBase* m_false_value;

            AstValueTribleOperator(AstValueBase* condition, AstValueBase* true_value, AstValueBase* false_value);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override final;
        };
        struct AstFakeValueUnpack : public AstValueBase
        {
            AstValueBase* m_unpack_value;

            AstFakeValueUnpack(AstValueBase* unpack_value);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override final;
        };
        struct AstValueVariadicArgumentsPack : public AstValueBase
        {
            AstValueVariadicArgumentsPack();
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override final;
        };
        struct AstValueIndex : public AstValueBase
        {
            AstValueBase* m_container;
            AstValueBase* m_index;

            std::optional<wo_integer_t> m_LANG_fast_index_for_struct;

            AstValueIndex(AstValueBase* container, AstValueBase* index);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstPatternBase : public AstBase
        {
            AstPatternBase(AstBase::node_type_t nodetype);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstPatternTakeplace : public AstPatternBase
        {
            AstPatternTakeplace();
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstPatternSingle : public AstPatternBase
        {
            bool m_is_mutable;
            wo_pstring_t m_name;
            std::optional<std::list<wo_pstring_t>> m_template_parameters;
            std::optional<lang_Symbol*> m_LANG_declared_symbol;

            AstPatternSingle(
                bool is_mutable,
                wo_pstring_t name,
                const std::optional<std::list<wo_pstring_t>>& template_parameters);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstPatternTuple : public AstPatternBase
        {
            std::list<AstPatternBase*> m_fields;

            AstPatternTuple(const std::list<AstPatternBase*>& fields);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstPatternUnion : public AstPatternBase
        {
            wo_pstring_t m_tag;
            std::optional<AstPatternBase*> m_field;

            AstPatternUnion(wo_pstring_t tag, std::optional<AstPatternBase*> field);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstPatternVariable : public AstPatternBase
        {
            AstValueVariable* m_variable;

            AstPatternVariable(AstValueVariable* variable);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstPatternIndex : public AstPatternBase
        {
            AstValueIndex* m_index;

            AstPatternIndex(AstValueIndex* index);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstVariableDefineItem : public AstBase
        {
            AstPatternBase* m_pattern;
            AstValueBase* m_init_value;

            std::optional<AstDeclareAttribue*> m_LANG_declare_attribute;
        private:
            AstVariableDefineItem(const AstVariableDefineItem&);
        public:
            AstVariableDefineItem(
                AstPatternBase* pattern,
                AstValueBase* init_value);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstVariableDefines : public AstBase
        {
            std::list<AstVariableDefineItem*> m_definitions;
            std::optional<AstDeclareAttribue*>
                m_attribute;

        private:
            AstVariableDefines(const AstVariableDefines&);
        public:
            AstVariableDefines(const std::optional<AstDeclareAttribue*>& attribute);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstFunctionParameterDeclare : public AstBase
        {
            AstPatternBase* m_pattern;
            std::optional<AstTypeHolder*> m_type;// Will be update in AstValueFunction pass and not NULLOPT.

            AstFunctionParameterDeclare(AstPatternBase* m_pattern, std::optional<AstTypeHolder*> m_type);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstValueFunction : public AstValueBase
        {
            enum LANG_hold_state
            {
                UNPROCESSED,
                HOLD_FOR_PARAMETER_EVAL,
                HOLD_FOR_RETURN_TYPE_EVAL,
                HOLD_FOR_EVAL_WHERE_CONSTRAINTS,
                HOLD_FOR_BODY_EVAL,
            };

            std::list<AstFunctionParameterDeclare*>
                m_parameters;
            bool                            m_is_variadic;
            std::optional<AstTypeHolder*>   m_marked_return_type;
            std::optional<AstWhereConstraints*>
                m_where_constraints;
            std::optional<std::list<wo_pstring_t>>
                m_pending_param_type_mark_template;
            AstBase* m_body;

            std::optional<lang_TypeInstance*>
                m_LANG_determined_return_type;
            LANG_hold_state                 m_LANG_hold_state;
            std::optional<lang_ValueInstance*>
                m_LANG_value_instance_to_update;

            AstValueFunction(
                const std::list<AstFunctionParameterDeclare*>& parameters,
                bool is_variadic,
                const std::optional<std::list<wo_pstring_t>>& defined_function_template_only_for_lambda,
                const std::optional<AstTypeHolder*>& marked_return_type,
                const std::optional<AstWhereConstraints*>& where_constraints,
                AstBase* body);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };

        struct AstValueArrayOrVec : public AstValueBase
        {
            bool m_making_vec;
            std::list<AstValueBase*> m_elements;

            AstValueArrayOrVec(const std::list<AstValueBase*>& elements, bool making_vec);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstKeyValuePair : public AstBase
        {
            AstValueBase* m_key;
            AstValueBase* m_value;

            AstKeyValuePair(AstValueBase* key, AstValueBase* value);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstValueDictOrMap : public AstValueBase
        {
            bool m_making_map;
            std::list<AstKeyValuePair*>
                m_elements;

            AstValueDictOrMap(const std::list<AstKeyValuePair*>& elements, bool making_map);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstValueTuple : public AstValueBase
        {
            std::list<AstValueBase*> m_elements;

            AstValueTuple(const std::list<AstValueBase*>& elements);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstStructFieldValuePair : public AstBase
        {
            wo_pstring_t m_name;
            AstValueBase* m_value;

            AstStructFieldValuePair(wo_pstring_t name, AstValueBase* value);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstValueStruct : public AstValueBase
        {
            std::optional<AstTypeHolder*> m_marked_struct_type;
            std::list<AstStructFieldValuePair*> m_fields;

            AstValueStruct(
                const std::optional<AstTypeHolder*>& marked_struct_type,
                const std::list<AstStructFieldValuePair*>& fields);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstValueAssign : public AstValueMayConsiderOperatorOverload
        {
            enum assign_type
            {
                ASSIGN,
                ADD_ASSIGN,
                SUBSTRACT_ASSIGN,
                MULTIPLY_ASSIGN,
                DIVIDE_ASSIGN,
                MODULO_ASSIGN,
            };

            bool m_valued_assign;
            assign_type m_assign_type;
            AstPatternBase* m_assign_place; // takeplace / variable / index
            AstValueBase* m_right;

            AstValueAssign(bool valued_assign, assign_type type, AstPatternBase* assign_place, AstValueBase* right);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstValuePackedArgs : public AstValueBase
        {
            AstValuePackedArgs();
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };

        ////////////////////////////////////////////////////////

        struct AstNamespace : public AstBase
        {
            wo_pstring_t    m_name;
            AstBase* m_body;

            std::optional<lang_Namespace*> m_LANG_determined_namespace;

            AstNamespace(wo_pstring_t name, AstBase* body);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstScope : public AstBase
        {
            AstBase* m_body;

            std::optional<lang_Scope*> m_LANG_determined_scope;

            AstScope(AstBase* body);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };

        struct AstMatchCase : public AstBase
        {
            AstPatternBase* m_pattern; // AstPatternTakeplace / AstPatternSingle / AstPatternUnion
            AstBase* m_body;

            AstMatchCase(AstPatternBase* pattern, AstBase* body);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstMatch : public AstBase
        {
            AstValueBase* m_matched_value;
            std::list<AstMatchCase*> m_cases;

            AstMatch(AstValueBase* match_value, const std::list<AstMatchCase*>& cases);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };

        struct AstIf : public AstBase
        {
            AstValueBase* m_condition;
            AstBase* m_true_body;
            std::optional<AstBase*>
                m_false_body;

            AstIf(AstValueBase* condition, AstBase* true_body, const std::optional<AstBase*>& false_body);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstWhile : public AstBase
        {
            AstValueBase* m_condition;
            AstBase* m_body;

            AstWhile(AstValueBase* condition, AstBase* body);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstFor : public AstBase
        {
            std::optional<AstBase*>             m_initial;
            std::optional<AstValueBase*>        m_condition;
            std::optional<AstValueBase*>        m_step;
            AstBase* m_body;

            AstFor(
                std::optional<AstBase*> initial,
                std::optional<AstValueBase*> condition,
                std::optional<AstValueBase*> step,
                AstBase* body);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstForeach : public AstBase
        {
            AstScope* m_job;
            /*
            for (let PPP : VVV)

            let _ITER = std::iterator(VVV);
            for (;;)
            {
                match (_ITER->next)
                {
                value(PPP)? BODY;
                none? break;
                }
            }
            */

        private:
            AstForeach(AstScope* job);
        public:
            AstForeach(AstPatternBase* pattern, AstValueBase* container, AstBase* body);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstBreak : public AstBase
        {
            std::optional<wo_pstring_t> m_label;

            AstBreak(std::optional<wo_pstring_t> label);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstContinue : public AstBase
        {
            std::optional<wo_pstring_t> m_label;

            AstContinue(std::optional<wo_pstring_t> label);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstReturn : public AstBase
        {
            std::optional<AstValueBase*> m_value;

            AstReturn(const std::optional<AstValueBase*>& value);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstLabeled : public AstBase
        {
            wo_pstring_t m_label;
            AstBase* m_body;

            AstLabeled(wo_pstring_t label, AstBase* body);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstUsingTypeDeclare : public AstBase
        {
            enum LANG_hold_state
            {
                UNPROCESSED,
                HOLD_FOR_BASE_TYPE_EVAL,
                HOLD_FOR_NAMESPACE_BODY,
            };

            wo_pstring_t            m_typename;
            std::optional<std::list<wo_pstring_t>>
                m_template_parameters;
            AstTypeHolder* m_type;
            std::optional<AstNamespace*>
                m_in_type_namespace;
            std::optional<AstDeclareAttribue*>
                m_attribute;
            std::optional<lang_Symbol*>
                m_LANG_declared_symbol;
            LANG_hold_state         m_LANG_hold_state;

        private:
            AstUsingTypeDeclare(const AstUsingTypeDeclare&);
        public:
            AstUsingTypeDeclare(
                const std::optional<AstDeclareAttribue*>& attrib,
                wo_pstring_t typename_,
                const std::optional<std::list<wo_pstring_t>>& template_parameters,
                AstTypeHolder* type,
                const std::optional<AstNamespace*>& in_type_namespace);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstAliasTypeDeclare : public AstBase
        {
            wo_pstring_t            m_typename;
            std::optional<std::list<wo_pstring_t>>
                m_template_parameters;
            AstTypeHolder* m_type;
            std::optional<AstDeclareAttribue*>
                m_attribute;
            std::optional<lang_Symbol*>
                m_LANG_declared_symbol;

        private:
            AstAliasTypeDeclare(const AstAliasTypeDeclare&);
        public:
            AstAliasTypeDeclare(
                const std::optional<AstDeclareAttribue*>& attrib,
                wo_pstring_t typename_,
                const std::optional<std::list<wo_pstring_t>>& template_parameters,
                AstTypeHolder* type);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstEnumItem : public AstBase
        {
            wo_pstring_t        m_name;
            std::optional<AstValueBase*>
                m_value; // Will update in AstEnumDeclare pass and not NULLOPT.

            AstEnumItem(wo_pstring_t name, std::optional<AstValueBase*> value);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstEnumDeclare : public AstBase
        {
            AstUsingTypeDeclare* m_enum_type_declare;
            AstNamespace* m_enum_body;

        private:
            AstEnumDeclare(const AstEnumDeclare&);
        public:
            AstEnumDeclare(
                const std::optional<AstDeclareAttribue*>& attrib,
                wo_pstring_t enum_name,
                const std::list<AstEnumItem*>& enum_items);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstUnionItem : public AstBase
        {
            wo_pstring_t                    m_label;
            std::optional<AstTypeHolder*>   m_type;

            AstUnionItem(wo_pstring_t name, std::optional<AstTypeHolder*> type);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstUnionDeclare : public AstBase
        {
            //wo_pstring_t            m_union_type_name;
            //std::optional<std::list<wo_pstring_t>>
            //                        m_template_parameters;
            AstUsingTypeDeclare* m_union_type_declare;

        private:
            AstUnionDeclare(const AstUnionDeclare&);
        public:
            AstUnionDeclare(
                const std::optional<AstDeclareAttribue*>& attrib,
                wo_pstring_t union_type_name,
                const std::optional<std::list<wo_pstring_t>>& template_parameters,
                const std::list<AstUnionItem*>& union_items);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstValueMakeUnion : public AstValueBase
        {
            // NOTE: This value will be marked as `nothing` type
            //  to fool the type checker.
            wo_integer_t m_index;
            std::optional<AstValueBase*> m_packed_value;

            AstValueMakeUnion(
                wo_integer_t index,
                const std::optional<AstValueBase*>& packed_value);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstUsingNamespace : public AstBase
        {
            std::list<wo_pstring_t> m_using_namespace;

            AstUsingNamespace(const std::list<wo_pstring_t>& using_namespace);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstToken : public AstBase
        {
            token m_token;

            AstToken(const token& token);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstExternInformation : public AstBase
        {
            wo_pstring_t m_extern_symbol;
            std::optional<wo_pstring_t> m_extern_from_library;
            uint32_t m_attribute_flags;

            enum extern_attribute : uint32_t
            {
                SLOW = 1 << 0,
                REPEATABLE = 1 << 1,
            };

        private:
            AstExternInformation(const AstExternInformation&);
        public:
            AstExternInformation(
                wo_pstring_t extern_symbol,
                const std::optional<wo_pstring_t>& extern_from_library,
                uint32_t attribute_flags);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
    }

    grammar::rule operator >> (grammar::rule ost, size_t builder_index);
#endif
}
