#include "wo_lang_ast_builder.hpp"

namespace wo
{
#ifndef WO_DISABLE_COMPILER
    namespace ast
    {
        Identifier::Identifier(wo_pstring_t identifier)
            : m_formal(FROM_CURRENT)
            , m_from_type(std::nullopt)
            , m_scope({})
            , m_name(identifier)
            , m_template_arguments({})
            , m_symbol(std::nullopt)
        {

        }
        Identifier::Identifier(wo_pstring_t identifier, const std::list<AstTypeHolder*>& template_arguments)
            : m_formal(FROM_CURRENT)
            , m_from_type(std::nullopt)
            , m_scope({})
            , m_name(identifier)
            , m_template_arguments(template_arguments)
            , m_symbol(std::nullopt)
        {
        }
        Identifier::Identifier(wo_pstring_t identifier, const std::list<AstTypeHolder*>& template_arguments, const std::list<wo_pstring_t>& scopes, bool from_global)
            : m_formal(from_global ? FROM_GLOBAL : FROM_CURRENT)
            , m_from_type(std::nullopt)
            , m_scope(scopes)
            , m_name(identifier)
            , m_template_arguments(template_arguments)
            , m_symbol(std::nullopt)
        {

        }
        Identifier::Identifier(wo_pstring_t identifier, const std::list<AstTypeHolder*>& template_arguments, const std::list<wo_pstring_t>& scopes, AstTypeHolder* from_type)
            : m_formal(FROM_TYPE)
            , m_from_type(from_type)
            , m_scope(scopes)
            , m_name(identifier)
            , m_template_arguments(template_arguments)
            , m_symbol(std::nullopt)
        {

        }

        Identifier::Identifier(const Identifier& identifer)
            : m_formal(identifer.m_formal)
            , m_from_type(identifer.m_from_type)
            , m_scope(identifer.m_scope)
            , m_name(identifer.m_name)
            , m_template_arguments(identifer.m_template_arguments)
            , m_symbol(std::nullopt)
        {

        }
        Identifier& Identifier::operator = (const Identifier& identifer)
        {
            m_formal = identifer.m_formal;
            m_from_type = identifer.m_from_type;
            m_scope = identifer.m_scope;
            m_name = identifer.m_name;
            m_template_arguments = identifer.m_template_arguments;
            m_symbol = std::nullopt;

            return *this;
        }

        void Identifier::make_dup(AstBase::ContinuesList& out_continues)
        {
            if (m_from_type)
                out_continues.push_back(AstBase::make_holder(&m_from_type.value()));
            for (auto& arg : m_template_arguments)
                out_continues.push_back(AstBase::make_holder(&arg));
        }

        ////////////////////////////////////////////////////////

        AstTypeHolder::AstTypeHolder(const Identifier& ident)
            : AstBase(AST_TYPE_HOLDER)
            , m_mutable_mark(NONE)
            , m_formal(IDENTIFIER)
            , m_identifier(ident)
            , m_type(std::nullopt)
        {

        }
        AstTypeHolder::AstTypeHolder(AstValueBase* expr)
            : AstBase(AST_TYPE_HOLDER)
            , m_mutable_mark(NONE)
            , m_formal(TYPEOF)
            , m_typefrom(expr)
            , m_type(std::nullopt)
        {

        }
        AstTypeHolder::AstTypeHolder(const FunctionType& functype)
            : AstBase(AST_TYPE_HOLDER)
            , m_mutable_mark(NONE)
            , m_formal(FUNCTION)
            , m_function(functype)
            , m_type(std::nullopt)
        {

        }
        AstTypeHolder::AstTypeHolder(const StructureType& structtype)
            : AstBase(AST_TYPE_HOLDER)
            , m_mutable_mark(NONE)
            , m_formal(STRUCTURE)
            , m_structure(structtype)
            , m_type(std::nullopt)
        {

        }
        AstTypeHolder::AstTypeHolder(const TupleType& tupletype)
            : AstBase(AST_TYPE_HOLDER)
            , m_mutable_mark(NONE)
            , m_formal(TUPLE)
            , m_tuple(tupletype)
            , m_type(std::nullopt)
        {

        }

        AstTypeHolder::~AstTypeHolder()
        {
            switch (m_formal)
            {
            case IDENTIFIER:
                m_identifier.~Identifier();
                break;
            case TYPEOF:
                break;
            case FUNCTION:
                m_function.~FunctionType();
                break;
            case STRUCTURE:
                m_structure.~StructureType();
                break;
            case TUPLE:
                m_tuple.~TupleType();
                break;
            default:
                wo_error("Unknown type form.");
            }

        }

        AstBase* AstTypeHolder::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstTypeHolder* new_instance = exist_instance? static_cast<AstTypeHolder*>(exist_instance.value()): nullptr;

            switch(m_formal)
            {
            case IDENTIFIER:
                if (new_instance == nullptr)new_instance = new AstTypeHolder(m_identifier);
                new_instance->m_identifier.make_dup(out_continues);
                break;
            case TYPEOF:
                if (new_instance == nullptr)new_instance = new AstTypeHolder(m_typefrom);
                out_continues.push_back(AstBase::make_holder(&new_instance->m_typefrom));
                break;
            case FUNCTION:
                if (new_instance == nullptr)new_instance = new AstTypeHolder(m_function);
                for (auto& param : new_instance->m_function.m_parameters)
                    out_continues.push_back(AstBase::make_holder(&param));
                out_continues.push_back(AstBase::make_holder(&new_instance->m_function.m_return_type));
                break;
            case STRUCTURE:
                if (new_instance == nullptr)new_instance = new AstTypeHolder(m_structure);
                for (auto& field : new_instance->m_structure.m_fields)
                    out_continues.push_back(AstBase::make_holder(&field.second));
                break;
            case TUPLE:
                if (new_instance == nullptr)new_instance = new AstTypeHolder(m_tuple);
                for (auto& field : new_instance->m_tuple.m_fields)
                    out_continues.push_back(AstBase::make_holder(&field));
                break;
            default:
                wo_error("Unknown type form.");
            }
            
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstValueBase::AstValueBase(AstBase::node_type_t nodetype)
            : AstBase(nodetype)
            , m_determined_type(std::nullopt)
            , m_evaled_const_value(std::nullopt)
        {
        }

        AstBase* AstValueBase::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            // AstValueBase is abstract class, so it must be exist_instance.
            AstValueBase* new_instance = static_cast<AstValueBase*>(exist_instance.value());
            // new_instance->m_determined_type = std::nullopt;
            // new_instance->m_evaled_const_value = std::nullopt;
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstValueMarkAsMutable::AstValueMarkAsMutable(AstValueBase* m_marked_value)
            : AstValueBase(AST_VALUE_MARK_AS_MUTABLE)
            , m_marked_value(m_marked_value)
        {
        }

        AstBase* AstValueMarkAsMutable::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstValueMarkAsMutable* new_instance = exist_instance
                ? static_cast<AstValueMarkAsMutable*>(exist_instance.value())
                : new AstValueMarkAsMutable(m_marked_value)
                ;
            AstValueBase::make_dup(new_instance, out_continues);
            out_continues.push_back(AstBase::make_holder(&new_instance->m_marked_value));
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstValueMarkAsImmutable::AstValueMarkAsImmutable(AstValueBase* m_marked_value)
            : AstValueBase(AST_VALUE_MARK_AS_IMMUTABLE)
            , m_marked_value(m_marked_value)
        {
        }

        AstBase* AstValueMarkAsImmutable::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstValueMarkAsImmutable* new_instance = exist_instance
                ? static_cast<AstValueMarkAsImmutable*>(exist_instance.value())
                : new AstValueMarkAsImmutable(m_marked_value)
                ;
            AstValueBase::make_dup(new_instance, out_continues);
            out_continues.push_back(AstBase::make_holder(&new_instance->m_marked_value));
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstValueLiteral::AstValueLiteral(const token& literal_token)
            : AstValueBase(AST_VALUE_LITERAL)
            , m_literal_token(literal_token)
        {

        }
        AstBase* AstValueLiteral::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstValueLiteral* new_instance = exist_instance
                ? static_cast<AstValueLiteral*>(exist_instance.value())
                : new AstValueLiteral(m_literal_token)
                ;
            AstValueBase::make_dup(new_instance, out_continues);
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstValueTypeid::AstValueTypeid(AstTypeHolder* id_type)
            : AstValueBase(AST_VALUE_TYPEID)
            , m_id_type(id_type)
        {

        }
        AstBase* AstValueTypeid::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstValueTypeid* new_instance = exist_instance
                ? static_cast<AstValueTypeid*>(exist_instance.value())
                : new AstValueTypeid(m_id_type)
                ;
            AstValueBase::make_dup(new_instance, out_continues);
            out_continues.push_back(AstBase::make_holder(&new_instance->m_id_type));
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstValueTypeCast::AstValueTypeCast(AstTypeHolder* cast_type, AstValueBase* cast_value)
            : AstValueBase(AST_VALUE_TYPE_CAST)
            , m_cast_type(cast_type)
            , m_cast_value(cast_value)
        {

        }
        AstBase* AstValueTypeCast::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstValueTypeCast* new_instance = exist_instance
                ? static_cast<AstValueTypeCast*>(exist_instance.value())
                : new AstValueTypeCast(m_cast_type, m_cast_value)
                ;
            AstValueBase::make_dup(new_instance, out_continues);
            out_continues.push_back(AstBase::make_holder(&new_instance->m_cast_type));
            out_continues.push_back(AstBase::make_holder(&new_instance->m_cast_value));
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstValueTypeCheckIs::AstValueTypeCheckIs(AstTypeHolder* check_type, AstValueBase* check_value)
            : AstValueBase(AST_VALUE_TYPE_CHECK_IS)
            , m_check_type(check_type)
            , m_check_value(check_value)
        {

        }
        AstBase* AstValueTypeCheckIs::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstValueTypeCheckIs* new_instance = exist_instance
                ? static_cast<AstValueTypeCheckIs*>(exist_instance.value())
                : new AstValueTypeCheckIs(m_check_type, m_check_value)
                ;
            AstValueBase::make_dup(new_instance, out_continues);
            out_continues.push_back(AstBase::make_holder(&new_instance->m_check_type));
            out_continues.push_back(AstBase::make_holder(&new_instance->m_check_value));
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstValueTypeCheckAs::AstValueTypeCheckAs(AstTypeHolder* check_type, AstValueBase* check_value)
            : AstValueBase(AST_VALUE_TYPE_CHECK_AS)
            , m_check_type(check_type)
            , m_check_value(check_value)
        {

        }
        AstBase* AstValueTypeCheckAs::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstValueTypeCheckAs* new_instance = exist_instance
                ? static_cast<AstValueTypeCheckAs*>(exist_instance.value())
                : new AstValueTypeCheckAs(m_check_type, m_check_value)
                ;
            AstValueBase::make_dup(new_instance, out_continues);
            out_continues.push_back(AstBase::make_holder(&new_instance->m_check_type));
            out_continues.push_back(AstBase::make_holder(&new_instance->m_check_value));
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstValueVariable::AstValueVariable(const Identifier& identifier)
            : AstValueBase(AST_VALUE_VARIABLE)
            , m_identifier(identifier)
        {

        }
        AstBase* AstValueVariable::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstValueVariable* new_instance = exist_instance
                ? static_cast<AstValueVariable*>(exist_instance.value())
                : new AstValueVariable(m_identifier)
                ;
            AstValueBase::make_dup(new_instance, out_continues);
            new_instance->m_identifier.make_dup(out_continues);
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstWhereConstraints::AstWhereConstraints(const std::list<AstValueBase*>& constraints)
            : AstBase(AST_WHERE_CONSTRAINTS)
            , m_constraints(constraints)
        {
        }
        AstBase* AstWhereConstraints::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstWhereConstraints* new_instance = exist_instance
                ? static_cast<AstWhereConstraints*>(exist_instance.value())
                : new AstWhereConstraints(m_constraints)
                ;
            for (auto& constraint : new_instance->m_constraints)
                out_continues.push_back(AstBase::make_holder(&constraint));
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstValueFunctionCall::AstValueFunctionCall(bool direct, AstValueBase* function, const std::list<AstValueBase*>& arguments)
            : AstValueBase(AST_VALUE_FUNCTION_CALL)
            , m_is_direct_call(direct)
            , m_function(function)
            , m_arguments(arguments)
        {

        }
        AstBase* AstValueFunctionCall::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstValueFunctionCall* new_instance = exist_instance
                ? static_cast<AstValueFunctionCall*>(exist_instance.value())
                : new AstValueFunctionCall(m_is_direct_call, m_function, m_arguments)
                ;
            AstValueBase::make_dup(new_instance, out_continues);
            out_continues.push_back(AstBase::make_holder(&new_instance->m_function));
            for (auto& arg : new_instance->m_arguments)
                out_continues.push_back(AstBase::make_holder(&arg));
            return new_instance;
        }
        ////////////////////////////////////////////////////////
 
        AstValueMayConsiderOperatorOverload::AstValueMayConsiderOperatorOverload(AstBase::node_type_t nodetype)
            : AstValueBase(nodetype)
            , m_overload_call(std::nullopt)
        {
        }
        AstBase* AstValueMayConsiderOperatorOverload::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstValueMayConsiderOperatorOverload* new_instance = exist_instance
                ? static_cast<AstValueMayConsiderOperatorOverload*>(exist_instance.value())
                : new AstValueMayConsiderOperatorOverload(node_type)
                ;
            AstValueBase::make_dup(new_instance, out_continues);
            if (m_overload_call)
                out_continues.push_back(AstBase::make_holder(&new_instance->m_overload_call.value()));
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstValueBinaryOperator::AstValueBinaryOperator(
            operator_type op, AstValueBase* left, AstValueBase* right)
            : AstValueMayConsiderOperatorOverload(AST_VALUE_BINARY_OPERATOR)
            , m_operator(op)
            , m_left(left)
            , m_right(right)
        {
        }
        AstBase* AstValueBinaryOperator::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstValueBinaryOperator* new_instance = exist_instance
                ? static_cast<AstValueBinaryOperator*>(exist_instance.value())
                : new AstValueBinaryOperator(m_operator, m_left, m_right)
                ;
            AstValueMayConsiderOperatorOverload::make_dup(new_instance, out_continues);
            out_continues.push_back(AstBase::make_holder(&new_instance->m_left));
            out_continues.push_back(AstBase::make_holder(&new_instance->m_right));
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstValueUnaryOperator::AstValueUnaryOperator(operator_type op, AstValueBase* operand)
            : AstValueBase(AST_VALUE_UNARY_OPERATOR)
            , m_operator(op)
            , m_operand(operand)
        {
        }
        AstBase* AstValueUnaryOperator::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstValueUnaryOperator* new_instance = exist_instance
                ? static_cast<AstValueUnaryOperator*>(exist_instance.value())
                : new AstValueUnaryOperator(m_operator, m_operand)
                ;
            AstValueBase::make_dup(new_instance, out_continues);
            out_continues.push_back(AstBase::make_holder(&new_instance->m_operand));
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstValueTribleOperator::AstValueTribleOperator(AstValueBase* condition, AstValueBase* true_value, AstValueBase* false_value)
            : AstValueBase(AST_VALUE_TRIBLE_OPERATOR)
            , m_condition(condition)
            , m_true_value(true_value)
            , m_false_value(false_value)
        {
        }
        AstBase* AstValueTribleOperator::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstValueTribleOperator* new_instance = exist_instance
                ? static_cast<AstValueTribleOperator*>(exist_instance.value())
                : new AstValueTribleOperator(m_condition, m_true_value, m_false_value)
                ;
            AstValueBase::make_dup(new_instance, out_continues);
            out_continues.push_back(AstBase::make_holder(&new_instance->m_condition));
            out_continues.push_back(AstBase::make_holder(&new_instance->m_true_value));
            out_continues.push_back(AstBase::make_holder(&new_instance->m_false_value));
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstFakeValueUnpack::AstFakeValueUnpack(AstValueBase* unpack_value)
            : AstValueBase(AST_FAKE_VALUE_UNPACK)
            , m_unpack_value(unpack_value)
        {
        }
        AstBase* AstFakeValueUnpack::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstFakeValueUnpack* new_instance = exist_instance
                ? static_cast<AstFakeValueUnpack*>(exist_instance.value())
                : new AstFakeValueUnpack(m_unpack_value)
                ;
            AstValueBase::make_dup(new_instance, out_continues);
            out_continues.push_back(AstBase::make_holder(&new_instance->m_unpack_value));
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstPatternBase::AstPatternBase(AstBase::node_type_t nodetype)
            : AstBase(nodetype)
        {
        }
        AstBase* AstPatternBase::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstPatternBase* new_instance = exist_instance
                ? static_cast<AstPatternBase*>(exist_instance.value())
                : new AstPatternBase(node_type)
                ;
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstPatternTakeplace::AstPatternTakeplace()
            : AstPatternBase(AST_PATTERN_TAKEPLACE)
        {
        }
        AstBase* AstPatternTakeplace::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstPatternTakeplace* new_instance = exist_instance
                ? static_cast<AstPatternTakeplace*>(exist_instance.value())
                : new AstPatternTakeplace()
                ;
            AstPatternBase::make_dup(new_instance, out_continues);
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstPatternSingle::AstPatternSingle(
            wo_pstring_t name,
            const std::optional<std::vector<wo_pstring_t>> template_parameters)
            : AstPatternBase(AST_PATTERN_SINGLE)
            , m_name(name)
            , m_template_parameters(template_parameters)
            , m_declared_symbol(std::nullopt)
        {
        }
        AstBase* AstPatternSingle::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstPatternSingle* new_instance = exist_instance
                ? static_cast<AstPatternSingle*>(exist_instance.value())
                : new AstPatternSingle(m_name, m_template_parameters)
                ;
            AstPatternBase::make_dup(new_instance, out_continues);
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstPatternTuple::AstPatternTuple(
            const std::vector<AstPatternBase*>& fields)
            : AstPatternBase(AST_PATTERN_TUPLE)
            , m_fields(fields)
        {
        }
        AstBase* AstPatternTuple::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstPatternTuple* new_instance = exist_instance
                ? static_cast<AstPatternTuple*>(exist_instance.value())
                : new AstPatternTuple(m_fields)
                ;
            AstPatternBase::make_dup(new_instance, out_continues);
            for (auto& field : new_instance->m_fields)
                out_continues.push_back(AstBase::make_holder(&field));
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstPatternUnion::AstPatternUnion(wo_pstring_t tag, AstPatternBase* field)
            : AstPatternBase(AST_PATTERN_UNION)
            , m_tag(tag)
            , m_field(field)
        {
        }
        AstBase* AstPatternUnion::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstPatternUnion* new_instance = exist_instance
                ? static_cast<AstPatternUnion*>(exist_instance.value())
                : new AstPatternUnion(m_tag, m_field)
                ;
            AstPatternBase::make_dup(new_instance, out_continues);
            out_continues.push_back(AstBase::make_holder(&new_instance->m_field));
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstValueIndex::AstValueIndex(AstValueBase* container, AstValueBase* index)
            : AstValueBase(AST_VALUE_INDEX)
            , m_container(container)
            , m_index(index)
        {
        }
        AstBase* AstValueIndex::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstValueIndex* new_instance = exist_instance
                ? static_cast<AstValueIndex*>(exist_instance.value())
                : new AstValueIndex(m_container, m_index)
                ;
            AstValueBase::make_dup(new_instance, out_continues);
            out_continues.push_back(AstBase::make_holder(&new_instance->m_container));
            out_continues.push_back(AstBase::make_holder(&new_instance->m_index));
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstPatternIndex::AstPatternIndex(AstValueIndex* index)
            : AstPatternBase(AST_PATTERN_INDEX)
            , m_index(index)
        {
        }
        AstBase* AstPatternIndex::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstPatternIndex* new_instance = exist_instance
                ? static_cast<AstPatternIndex*>(exist_instance.value())
                : new AstPatternIndex(m_index)
                ;
            AstPatternBase::make_dup(new_instance, out_continues);
            out_continues.push_back(AstBase::make_holder(&new_instance->m_index));
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstVariableDefines::AstVariableDefines()
            : AstBase(AST_VARIABLE_DEFINES)
            , m_definitions({})
        {
        }
        AstBase* AstVariableDefines::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstVariableDefines* new_instance = exist_instance
                ? static_cast<AstVariableDefines*>(exist_instance.value())
                : new AstVariableDefines()
                ;
            new_instance->m_definitions = m_definitions;
            for (auto& def : new_instance->m_definitions)
            {
                out_continues.push_back(AstBase::make_holder(&def.m_pattern));
                out_continues.push_back(AstBase::make_holder(&def.m_init_value));
            }
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstValueFunction::AstValueFunction(
            const std::vector<parameter_define>& parameters,
            bool is_variadic,
            const std::optional<AstTypeHolder*>& marked_return_type,
            const std::optional<AstWhereConstraints*>& where_constraints,
            AstValueBase* body)
            : AstValueBase(AST_VALUE_FUNCTION)
            , m_parameters(parameters)
            , m_is_variadic(is_variadic)
            , m_marked_return_type(marked_return_type)
            , m_where_constraints(where_constraints)
            , m_body(body)
        {
        }
        AstBase* AstValueFunction::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstValueFunction* new_instance = exist_instance
                ? static_cast<AstValueFunction*>(exist_instance.value())
                : new AstValueFunction(m_parameters, m_is_variadic, m_marked_return_type, m_where_constraints, m_body)
                ;
            for (auto& param : new_instance->m_parameters)
            {
                out_continues.push_back(AstBase::make_holder(&param.m_pattern));
                if (param.m_type)
                    out_continues.push_back(AstBase::make_holder(&param.m_type.value()));
            }
            if (m_marked_return_type)
                out_continues.push_back(AstBase::make_holder(&new_instance->m_marked_return_type.value()));
            if (m_where_constraints)
                out_continues.push_back(AstBase::make_holder(&new_instance->m_where_constraints.value()));
            out_continues.push_back(AstBase::make_holder(&new_instance->m_body));
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstValueArrayOrVec::AstValueArrayOrVec(const std::vector<AstValueBase*>& elements, bool making_vec)
            : AstValueBase(AST_VALUE_ARRAY_OR_VEC)
            , m_making_vec(making_vec)
            , m_elements(elements)
        {
        }
        AstBase* AstValueArrayOrVec::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstValueArrayOrVec* new_instance = exist_instance
                ? static_cast<AstValueArrayOrVec*>(exist_instance.value())
                : new AstValueArrayOrVec(m_elements, m_making_vec)
                ;
            AstValueBase::make_dup(new_instance, out_continues);
            for (auto& elem : new_instance->m_elements)
                out_continues.push_back(AstBase::make_holder(&elem));
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstKeyValuePair::AstKeyValuePair(AstValueBase* key, AstValueBase* value)
            : AstBase(AST_KEY_VALUE_PAIR)
            , m_key(key)
            , m_value(value)
        {
        }
        AstBase* AstKeyValuePair::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstKeyValuePair* new_instance = exist_instance
                ? static_cast<AstKeyValuePair*>(exist_instance.value())
                : new AstKeyValuePair(m_key, m_value)
                ;
            out_continues.push_back(AstBase::make_holder(&new_instance->m_key));
            out_continues.push_back(AstBase::make_holder(&new_instance->m_value));
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstValueDictOrMap::AstValueDictOrMap(const std::vector<AstKeyValuePair*>& elements, bool making_map)
            : AstValueBase(AST_VALUE_DICT_OR_MAP)
            , m_making_map(making_map)
            , m_elements(elements)
        {
        }
        AstBase* AstValueDictOrMap::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstValueDictOrMap* new_instance = exist_instance
                ? static_cast<AstValueDictOrMap*>(exist_instance.value())
                : new AstValueDictOrMap(m_elements, m_making_map)
                ;
            AstValueBase::make_dup(new_instance, out_continues);
            for (auto& elem : new_instance->m_elements)
                out_continues.push_back(AstBase::make_holder(&elem));

            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstValueTuple::AstValueTuple(const std::vector<AstValueBase*>& elements)
            : AstValueBase(AST_VALUE_TUPLE)
            , m_elements(elements)
        {
        }
        AstBase* AstValueTuple::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstValueTuple* new_instance = exist_instance
                ? static_cast<AstValueTuple*>(exist_instance.value())
                : new AstValueTuple(m_elements)
                ;
            AstValueBase::make_dup(new_instance, out_continues);
            for (auto& elem : new_instance->m_elements)
                out_continues.push_back(AstBase::make_holder(&elem));
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstFieldValuePair::AstFieldValuePair(wo_pstring_t name, AstValueBase* value)
            : AstBase(AST_FIELD_VALUE_PAIR)
            , m_name(name)
            , m_value(value)
        {
        }
        AstBase* AstFieldValuePair::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstFieldValuePair* new_instance = exist_instance
                ? static_cast<AstFieldValuePair*>(exist_instance.value())
                : new AstFieldValuePair(m_name, m_value)
                ;
            out_continues.push_back(AstBase::make_holder(&new_instance->m_value));
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstValueStruct::AstValueStruct(
            const std::optional<AstTypeHolder*>& marked_struct_type,
            const std::vector<AstFieldValuePair*>& fields)
            : AstValueBase(AST_VALUE_STRUCT)
            , m_marked_struct_type(marked_struct_type)
            , m_fields(fields)
        {
        }
        AstBase* AstValueStruct::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstValueStruct* new_instance = exist_instance
                ? static_cast<AstValueStruct*>(exist_instance.value())
                : new AstValueStruct(m_marked_struct_type, m_fields)
                ;
            AstValueBase::make_dup(new_instance, out_continues);
            if (m_marked_struct_type)
                out_continues.push_back(AstBase::make_holder(&new_instance->m_marked_struct_type.value()));
            for (auto& field : new_instance->m_fields)
                out_continues.push_back(AstBase::make_holder(&field));
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstValueAssign::AstValueAssign(
            assign_type type, AstPatternBase* assign_place, AstValueBase* right)
            : AstValueBase(AST_VALUE_ASSIGN)
            , m_assign_type(type)
            , m_assign_place(assign_place)
            , m_right(right)
        {
        }
        AstBase* AstValueAssign::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstValueAssign* new_instance = exist_instance
                ? static_cast<AstValueAssign*>(exist_instance.value())
                : new AstValueAssign(m_assign_type, m_assign_place, m_right)
                ;
            AstValueBase::make_dup(new_instance, out_continues);
            out_continues.push_back(AstBase::make_holder(&new_instance->m_assign_place));
            out_continues.push_back(AstBase::make_holder(&new_instance->m_right));
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstValuePackedArgs::AstValuePackedArgs()
            : AstValueBase(AST_VALUE_PACKED_ARGS)
        {
        }
        AstBase* AstValuePackedArgs::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstValuePackedArgs* new_instance = exist_instance
                ? static_cast<AstValuePackedArgs*>(exist_instance.value())
                : new AstValuePackedArgs()
                ;
            AstValueBase::make_dup(new_instance, out_continues);
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstValueIndexPackedArgs::AstValueIndexPackedArgs(wo_size_t m_index)
            : AstValueBase(AST_VALUE_INDEX_PACKED_ARGS)
            , m_index(m_index)
        {
        }
        AstBase* AstValueIndexPackedArgs::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstValueIndexPackedArgs* new_instance = exist_instance
                ? static_cast<AstValueIndexPackedArgs*>(exist_instance.value())
                : new AstValueIndexPackedArgs(m_index)
                ;
            AstValueBase::make_dup(new_instance, out_continues);
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstNamespace::AstNamespace(wo_pstring_t name, AstBase* body)
            : AstBase(AST_NAMESPACE)
            , m_name(name)
            , m_body(body)
        {
        }
        AstBase* AstNamespace::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstNamespace* new_instance = exist_instance
                ? static_cast<AstNamespace*>(exist_instance.value())
                : new AstNamespace(m_name, m_body)
                ;
            out_continues.push_back(AstBase::make_holder(&new_instance->m_body));
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstScope::AstScope(AstBase* body)
            : AstBase(AST_SCOPE)
            , m_body(body)
        {
        }
        AstBase* AstScope::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstScope* new_instance = exist_instance
                ? static_cast<AstScope*>(exist_instance.value())
                : new AstScope(m_body)
                ;
            out_continues.push_back(AstBase::make_holder(&new_instance->m_body));
            return new_instance;
        }

        ////////////////////////////////////////////////////////
       
       

        ////////////////////////////////////////////////////////
       
        ////////////////////////////////////////////////////////
        
        ////////////////////////////////////////////////////////
      
        ////////////////////////////////////////////////////////




        void init_builder()
        {

        }
    }

    grammar::rule operator>>(grammar::rule ost, size_t builder_index)
    {
        ost.first.builder_index = builder_index;
        return ost;
    }
#endif
}