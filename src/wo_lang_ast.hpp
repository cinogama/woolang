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

    namespace opnum
    {
        struct opnumbase;
    }

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
            std::optional<std::variant<AstTypeHolder*, lang_TypeInstance*>>
                m_from_type;
            std::list<wo_pstring_t> m_scope;
            wo_pstring_t            m_name;
            std::optional<std::list<AstTypeHolder*>>
                m_template_arguments;

            std::optional<lang_Symbol*>
                m_LANG_determined_symbol;

            std::optional<std::list<lang_TypeInstance*>>
                m_LANG_determined_and_appended_template_arguments;

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

            bool m_IR_dynamic_need_runtime_check;

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
        struct AstValueFunctionCall_FakeAstArgumentDeductionContextA : public AstBase
        {
            enum LANG_hold_state
            {
                UNPROCESSED,
                HOLD_FOR_PREPARE,
                HOLD_FOR_EVAL_PARAM_TYPE,
                HOLD_FOR_DEDUCT_ARGUMENT,
                HOLD_FOR_REVERSE_DEDUCT,
                HOLD_TO_NEXT_ARGUMENT,
            };
            struct ArgumentMatch
            {
                AstValueBase* m_argument;
                AstTypeHolder* m_duped_param_type;
            };
            LANG_hold_state m_LANG_hold_state;
            lang_Scope* m_apply_template_argument_scope;

            std::list<ArgumentMatch> m_arguments_tobe_deduct;
            std::list<ArgumentMatch>::iterator m_current_argument;

            std::unordered_map<wo_pstring_t, lang_TypeInstance*> m_deduction_results;
            std::list<wo_pstring_t> m_undetermined_template_params;

            AstValueFunctionCall_FakeAstArgumentDeductionContextA(lang_Scope* scope);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override final;
        };
        struct AstValueFunctionCall_FakeAstArgumentDeductionContextB : public AstBase
        {
            enum LANG_hold_state
            {
                UNPROCESSED,
                HOLD_FOR_DEDUCE,
            };

            struct ArgumentMatch
            {
                AstValueBase* m_argument;
                lang_TypeInstance* m_param_type;
            };

            std::list<ArgumentMatch> m_arguments_tobe_deduct;
            std::list<ArgumentMatch>::iterator m_current_argument;

            LANG_hold_state m_LANG_hold_state;

            AstValueFunctionCall_FakeAstArgumentDeductionContextB();
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override final;
        };
        struct AstValueFunction;
        struct AstValueFunctionCall : public AstValueBase
        {
            enum LANG_hold_state
            {
                UNPROCESSED,
                HOLD_FOR_FIRST_ARGUMENT_EVAL,
                HOLD_FOR_FUNCTION_EVAL,
                HOLD_FOR_ARGUMENTS_EVAL,
                HOLD_BRANCH_A_TEMPLATE_ARGUMENT_DEDUCTION,
                HOLD_BRANCH_B_TEMPLATE_ARGUMENT_DEDUCTION,
            };

            bool m_is_direct_call;  // -> |> <|
            AstValueBase* m_function;
            std::list<AstValueBase*> m_arguments;

            LANG_hold_state m_LANG_hold_state;
            bool m_LANG_target_function_need_deduct_template_arguments;

            std::optional<
                std::variant<
                AstValueFunctionCall_FakeAstArgumentDeductionContextA*,
                AstValueFunctionCall_FakeAstArgumentDeductionContextB*>>
                m_LANG_branch_argument_deduction_context;
            bool m_LANG_invoking_variadic_function;
            bool m_LANG_has_runtime_full_unpackargs;
            size_t m_LANG_certenly_function_argument_count;

            std::optional<AstValueFunction*> m_IR_invoking_function_near;

            AstValueFunctionCall(bool direct, AstValueBase* function, const std::list<AstValueBase*>& arguments);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override final;
        };

        struct AstValueMayConsiderOperatorOverload : public AstValueBase
        {
            enum LANG_hold_state
            {
                UNPROCESSED,
                HOLD_FOR_OPNUM_EVAL,
                HOLD_FOR_OVERLOAD_FUNCTION_CALL_EVAL,

                IR_HOLD_FOR_INDEX_PATTERN_EVAL,
                IR_HOLD_TO_INDEX_PATTERN_OVERLOAD,
                IR_HOLD_TO_APPLY_ASSIGN,
            };

            std::optional<AstValueFunctionCall*> m_LANG_overload_call;
            LANG_hold_state m_LANG_hold_state;

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
            enum LANG_hold_state
            {
                UNPROCESSED,
                HOLD_FOR_COND_EVAL,
                HOLD_FOR_BRANCH_EVAL,

                IR_HOLD_FOR_COND_EVAL,
                IR_HOLD_FOR_BRANCH_A_EVAL,
                IR_HOLD_FOR_BRANCH_B_EVAL,
                IR_HOLD_FOR_BRANCH_CONST_EVAL,
            };

            AstValueBase* m_condition;
            AstValueBase* m_true_value;
            AstValueBase* m_false_value;

            LANG_hold_state m_LANG_hold_state;

            AstValueTribleOperator(AstValueBase* condition, AstValueBase* true_value, AstValueBase* false_value);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override final;
        };
        struct AstFakeValueUnpack : public AstValueBase
        {
            enum IR_unpack_method
            {
                SHOULD_NOT_UNPACK,
                UNPACK_FOR_FUNCTION_CALL,
                UNPACK_FOR_TUPLE
            };
            struct IR_unpack_requirement
            {
                size_t m_require_unpack_count;
                bool m_unpack_all;
            };

            AstValueBase* m_unpack_value;

            std::optional<IR_unpack_requirement> m_IR_need_to_be_unpack_count;
            IR_unpack_method m_IR_unpack_method;
           
            AstFakeValueUnpack(AstValueBase* unpack_value);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override final;
        };
        struct AstValueVariadicArgumentsPack : public AstValueBase
        {
            std::optional<AstValueFunction*> m_LANG_function_instance;

            AstValueVariadicArgumentsPack();
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override final;
        };
        struct AstValueIndex : public AstValueBase
        {
            AstValueBase* m_container;
            AstValueBase* m_index;

            bool m_LANG_result_is_mutable;

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
        struct AstExternInformation;
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
            struct LANG_capture_context
            {
                struct captured_variable_instance
                {
                    std::unique_ptr<lang_ValueInstance> m_instance;
                    std::list<ast::AstValueVariable*> m_referenced_variables;
                };

                bool m_finished; // This function's all capture list has been determined.
                // Recursive referenced?
                // NOTE: Woolang 1.14.1: Recursive function is not allowed if it has captured
                //      variable.
                bool m_self_referenced; 
                
                std::map<lang_ValueInstance*, captured_variable_instance>
                    m_captured_variables;

                lang_ValueInstance* find_or_create_captured_instance(
                    lang_ValueInstance* from_value, ast::AstValueVariable* variable);

                LANG_capture_context();

                LANG_capture_context(const LANG_capture_context&) = delete;
                LANG_capture_context(LANG_capture_context&&) = delete;
                LANG_capture_context& operator=(const LANG_capture_context&) = delete;
                LANG_capture_context& operator=(LANG_capture_context&&) = delete;
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
            bool m_LANG_in_template_reification_context;
            std::optional<std::list<lang_TypeInstance*>>
                m_LANG_determined_template_arguments;

            LANG_capture_context m_LANG_captured_context;
            std::optional<lang_Scope*> m_LANG_function_scope;

            std::optional<const wchar_t*> m_IR_marked_function_name;
            std::optional<AstExternInformation*> m_IR_extern_information;

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
            enum LANG_hold_state
            {
                UNPROCESSED,
                HOLD_FOR_EVAL_MEMBER_VALUE_BESIDE_TEMPLATE,
                HOLD_FOR_TEMPLATE_DEDUCTION,
                HOLD_FOR_ANYLIZE_ARGUMENTS_TEMAPLTE_INSTANCE,
                HOLD_TO_STRUCT_TYPE_CHECK,
                HOLD_FOR_FIELD_EVAL,
            };

            std::optional<AstTypeHolder*> m_marked_struct_type;
            std::list<AstStructFieldValuePair*> m_fields;

            LANG_hold_state m_LANG_hold_state;
            std::optional<
                std::variant<
                AstValueFunctionCall_FakeAstArgumentDeductionContextA*,
                AstValueFunctionCall_FakeAstArgumentDeductionContextB*>>
                m_LANG_branch_argument_deduction_context;

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
        struct AstMatch;
        struct AstMatchCase : public AstBase
        {
            AstPatternBase* m_pattern; // AstPatternTakeplace / AstPatternUnion
            AstBase* m_body;

            std::optional<lang_TypeInstance*> m_LANG_pattern_value_apply_type;
            std::optional<wo_integer_t> m_LANG_case_label_or_takeplace;

            std::optional<opnum::opnumbase*> m_IR_matching_struct_opnum;
            std::optional<AstMatch*> m_IR_match;

            AstMatchCase(AstPatternBase* pattern, AstBase* body);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstMatch : public AstBase
        {
            enum LANG_hold_state
            {
                UNPROCESSED,
                HOLD_FOR_EVAL_MATCHING_VALUE,
                HOLD_FOR_EVAL_CASES,
            };

            AstValueBase* m_matched_value;
            std::list<AstMatchCase*> m_cases;

            LANG_hold_state m_LANG_hold_state;

            std::optional<opnum::opnumbase*> m_IR_matching_struct_opnum;

            AstMatch(AstValueBase* match_value, const std::list<AstMatchCase*>& cases);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };

        struct AstIf : public AstBase
        {
            enum LANG_hold_state
            {
                UNPROCESSED,
                HOLD_FOR_CONDITION_EVAL,
                HOLD_FOR_BODY_EVAL,

                IR_HOLD_FOR_TRUE_BODY,
                IR_HOLD_FOR_FALSE_BODY,
            };

            AstValueBase* m_condition;
            AstBase* m_true_body;
            std::optional<AstBase*>
                m_false_body;

            LANG_hold_state m_LANG_hold_state;

            AstIf(AstValueBase* condition, AstBase* true_body, const std::optional<AstBase*>& false_body);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstLabeled;
        struct AstWhile : public AstBase
        {
            enum LANG_hold_state
            {
                UNPROCESSED,
                HOLD_FOR_CONDITION_EVAL,
                HOLD_FOR_BODY_EVAL,
            };

            AstValueBase* m_condition;
            AstBase* m_body;

            LANG_hold_state m_LANG_hold_state;

            std::optional<AstLabeled*> m_IR_binded_label;

            AstWhile(AstValueBase* condition, AstBase* body);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstFor : public AstBase
        {
            enum LANG_hold_state
            {
                UNPROCESSED,
                HOLD_FOR_INITIAL_EVAL,
                HOLD_FOR_CONDITION_EVAL,
                HOLD_FOR_STEP_EVAL,
                HOLD_FOR_BODY_EVAL,

                IR_HOLD_FOR_INIT_EVAL,
                IR_HOLD_FOR_BODY_EVAL,
                IR_HOLD_FOR_NEXT_EVAL,
                IR_HOLD_FOR_COND_EVAL,
            };

            std::optional<AstBase*>             m_initial;
            std::optional<AstValueBase*>        m_condition;
            std::optional<AstValueBase*>        m_step;
            AstBase* m_body;

            LANG_hold_state m_LANG_hold_state;

            std::optional<AstLabeled*> m_IR_binded_label;

            AstFor(
                std::optional<AstBase*> initial,
                std::optional<AstValueBase*> condition,
                std::optional<AstValueBase*> step,
                AstBase* body);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstForeach : public AstBase
        {
            /*
            for (let PPP : VVV)
            ====================>
            for (let _ITER = std::iterator(VVV);;)
            {
                match (_ITER->next)
                {
                value(PPP)? BODY;
                none? break;
                }
            }
            */

            AstFor* m_forloop_body;

        private:
            AstForeach(AstFor* forloop_body);
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
            enum LANG_hold_state
            {
                UNPROCESSED,
                HOLD_FOR_ENUM_TYPE_DECL,
                HOLD_FOR_ENUM_ITEMS_DECL,
            };

            AstUsingTypeDeclare* m_enum_type_declare;
            AstNamespace* m_enum_body;

            LANG_hold_state m_LANG_hold_state;

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
            wo_pstring_t                    m_extern_symbol;
            std::optional<wo_pstring_t>     m_extern_from_library;
            uint32_t                        m_attribute_flags;

            std::optional<wo_native_func_t> m_IR_externed_function;

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
        struct AstValueIROpnum : public AstValueBase
        {
            opnum::opnumbase* m_opnum;

            AstValueIROpnum(opnum::opnumbase* spec_opnum);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
    }

    grammar::rule operator >> (grammar::rule ost, size_t builder_index);
#endif
}
