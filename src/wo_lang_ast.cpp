#include "wo_lang_ast.hpp"

#include <algorithm>

namespace wo
{
#ifndef WO_DISABLE_COMPILER
    namespace ast
    {
        AstDeclareAttribue::AstDeclareAttribue()
            : AstBase(AST_DECLARE_ATTRIBUTE)
            , m_lifecycle(std::nullopt)
            , m_access(std::nullopt)
            , m_external(std::nullopt)
        {
        }
        AstDeclareAttribue::AstDeclareAttribue(const AstDeclareAttribue& attrib)
            : AstBase(AST_DECLARE_ATTRIBUTE)
            , m_lifecycle(attrib.m_lifecycle)
            , m_access(attrib.m_access)
            , m_external(attrib.m_external)
        {
        }
        bool AstDeclareAttribue::modify_attrib(lexer& lex, AstToken* attrib_token)
        {
            switch (attrib_token->m_token.type)
            {
            case lex_type::l_static:
                if (m_lifecycle)
                {
                    lex.lang_error(lexer::errorlevel::error, attrib_token, WO_ERR_REPEAT_ATTRIBUTE);
                    return false;
                }
                m_lifecycle = lifecycle_attrib::STATIC;
                break;
            case lex_type::l_extern:
                if (m_external)
                {
                    lex.lang_error(lexer::errorlevel::error, attrib_token, WO_ERR_REPEAT_ATTRIBUTE);
                    return false;
                }
                m_external = external_attrib::EXTERNAL;
                break;
            case lex_type::l_public:
            case lex_type::l_private:
            case lex_type::l_protected:
                if (m_access)
                {
                    lex.lang_error(lexer::errorlevel::error, attrib_token, WO_ERR_REPEAT_ATTRIBUTE);
                    return false;
                }
                switch (attrib_token->m_token.type)
                {
                case lex_type::l_public:
                    m_access = accessc_attrib::PUBLIC; break;
                case lex_type::l_private:
                    m_access = accessc_attrib::PRIVATE; break;
                case lex_type::l_protected:
                    m_access = accessc_attrib::PROTECTED; break;
                default:
                    abort(); //WTF?
                }
                break;
            default:
                wo_error("Unknown attribute.");
            }
            return true;
        }
        AstBase* AstDeclareAttribue::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstDeclareAttribue* new_instance = exist_instance
                ? static_cast<AstDeclareAttribue*>(exist_instance.value())
                : new AstDeclareAttribue()
                ;
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstIdentifier::AstIdentifier(wo_pstring_t identifier)
            : AstBase(AST_IDENTIFIER)
            , m_formal(FROM_CURRENT)
            , m_from_type(std::nullopt)
            , m_scope({})
            , m_name(identifier)
            , m_template_arguments(std::nullopt)
            , m_LANG_determined_symbol(std::nullopt)
            //, m_LANG_determined_searching_from_scope(std::nullopt)
        {

        }
        AstIdentifier::AstIdentifier(
            wo_pstring_t identifier,
            const std::optional<std::list<AstTypeHolder*>>& template_arguments)
            : AstBase(AST_IDENTIFIER)
            , m_formal(FROM_CURRENT)
            , m_from_type(std::nullopt)
            , m_scope{}
            , m_name(identifier)
            , m_template_arguments(template_arguments)
            , m_LANG_determined_symbol(std::nullopt)
            //, m_LANG_determined_searching_from_scope(std::nullopt)
        {
        }
        AstIdentifier::AstIdentifier(
            wo_pstring_t identifier,
            const std::optional<std::list<AstTypeHolder*>>& template_arguments,
            const std::list<wo_pstring_t>& scopes,
            bool from_global)
            : AstBase(AST_IDENTIFIER)
            , m_formal(from_global ? FROM_GLOBAL : FROM_CURRENT)
            , m_from_type(std::nullopt)
            , m_scope(scopes)
            , m_name(identifier)
            , m_template_arguments(template_arguments)
            , m_LANG_determined_symbol(std::nullopt)
            //, m_LANG_determined_searching_from_scope(std::nullopt)
        {

        }
        AstIdentifier::AstIdentifier(
            wo_pstring_t identifier,
            const std::optional<std::list<AstTypeHolder*>>& template_arguments,
            const std::list<wo_pstring_t>& scopes,
            AstTypeHolder* from_type)
            : AstBase(AST_IDENTIFIER)
            , m_formal(FROM_TYPE)
            , m_from_type(from_type)
            , m_scope(scopes)
            , m_name(identifier)
            , m_template_arguments(template_arguments)
            , m_LANG_determined_symbol(std::nullopt)
            //, m_LANG_determined_searching_from_scope(std::nullopt)
        {

        }

        AstIdentifier::AstIdentifier(const AstIdentifier& identifer)
            : AstBase(AST_IDENTIFIER)
            , m_formal(identifer.m_formal)
            , m_from_type(identifer.m_from_type)
            , m_scope(identifer.m_scope)
            , m_name(identifer.m_name)
            , m_template_arguments(identifer.m_template_arguments)
            , m_LANG_determined_symbol(std::nullopt)
            //, m_LANG_determined_searching_from_scope(std::nullopt)
        {
        }

        AstBase* AstIdentifier::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstIdentifier* new_instance = exist_instance
                ? static_cast<AstIdentifier*>(exist_instance.value())
                : new AstIdentifier(*this)
                ;
            if (new_instance->m_from_type)
                out_continues.push_back(AstBase::make_holder(&new_instance->m_from_type.value()));
            if (new_instance->m_template_arguments)
                for (auto& arg : new_instance->m_template_arguments.value())
                    out_continues.push_back(AstBase::make_holder(&arg));
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstStructFieldDefine::AstStructFieldDefine(
            const std::optional<AstDeclareAttribue::accessc_attrib>& attrib,
            wo_pstring_t name,
            AstTypeHolder* type)
            : AstBase(AST_STRUCT_FIELD_DEFINE)
            , m_name(name)
            , m_type(type)
            , m_attribute(attrib)
        {
        }
        AstBase* AstStructFieldDefine::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstStructFieldDefine* new_instance = exist_instance
                ? static_cast<AstStructFieldDefine*>(exist_instance.value())
                : new AstStructFieldDefine(m_attribute, m_name, m_type)
                ;
            out_continues.push_back(AstBase::make_holder(&new_instance->m_type));
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstTypeHolder::AstTypeHolder(AstIdentifier* ident)
            : AstBase(AST_TYPE_HOLDER)
            , m_mutable_mark(NONE)
            , m_formal(IDENTIFIER)
            , m_LANG_template_evalating_state(std::nullopt)
            , m_LANG_determined_type(std::nullopt)
            , m_LANG_trying_advancing_type_judgement(false)
        {
            new (&m_typeform.m_identifier) AstIdentifier*(ident);
        }
        AstTypeHolder::AstTypeHolder(AstValueBase* expr)
            : AstBase(AST_TYPE_HOLDER)
            , m_mutable_mark(NONE)
            , m_formal(TYPEOF)
            , m_LANG_template_evalating_state(std::nullopt)
            , m_LANG_determined_type(std::nullopt)
            , m_LANG_trying_advancing_type_judgement(false)
        {
            new (&m_typeform.m_typefrom) AstValueBase*(expr);
        }
        AstTypeHolder::AstTypeHolder(const FunctionType& functype)
            : AstBase(AST_TYPE_HOLDER)
            , m_mutable_mark(NONE)
            , m_formal(FUNCTION)
            , m_LANG_template_evalating_state(std::nullopt)
            , m_LANG_determined_type(std::nullopt)
            , m_LANG_trying_advancing_type_judgement(false)
        {
            new (&m_typeform.m_function) FunctionType(functype);
        }
        AstTypeHolder::AstTypeHolder(const StructureType& structtype)
            : AstBase(AST_TYPE_HOLDER)
            , m_mutable_mark(NONE)
            , m_formal(STRUCTURE)
            , m_LANG_template_evalating_state(std::nullopt)
            , m_LANG_determined_type(std::nullopt)
            , m_LANG_trying_advancing_type_judgement(false)
        {
            new (&m_typeform.m_structure) StructureType(structtype);
        }
        AstTypeHolder::AstTypeHolder(const TupleType& tupletype)
            : AstBase(AST_TYPE_HOLDER)
            , m_mutable_mark(NONE)
            , m_formal(TUPLE)
            , m_LANG_template_evalating_state(std::nullopt)
            , m_LANG_determined_type(std::nullopt)
            , m_LANG_trying_advancing_type_judgement(false)
        {
            new (&m_typeform.m_tuple) TupleType(tupletype);
        }
        AstTypeHolder::AstTypeHolder(const UnionType& uniontype)
            : AstBase(AST_TYPE_HOLDER)
            , m_mutable_mark(NONE)
            , m_formal(UNION)
            , m_LANG_template_evalating_state(std::nullopt)
            , m_LANG_determined_type(std::nullopt)
            , m_LANG_trying_advancing_type_judgement(false)
        {
            new (&m_typeform.m_union) UnionType(uniontype);
        }
        AstTypeHolder::~AstTypeHolder()
        {
            switch (m_formal)
            {
            case IDENTIFIER:
            case TYPEOF:
                // Pointer type no need to invoke destructor.
                break;
            case FUNCTION:
                m_typeform.m_function.~FunctionType();
                break;
            case STRUCTURE:
                m_typeform.m_structure.~StructureType();
                break;
            case TUPLE:
                m_typeform.m_tuple.~TupleType();
                break;
            case UNION:
                m_typeform.m_union.~UnionType();
                break;
            default:
                wo_error("Unknown type form.");
            }
        }

        bool AstTypeHolder::_check_if_has_typeof() const
        {
            switch (m_formal)
            {
            case IDENTIFIER:
                if (m_typeform.m_identifier->m_formal == AstIdentifier::identifier_formal::FROM_TYPE)
                    return true;
                if (m_typeform.m_identifier->m_template_arguments)
                    for (auto& template_argument : m_typeform.m_identifier->m_template_arguments.value())
                    {
                        if (template_argument->_check_if_has_typeof())
                            return true;
                    }
                return false;
            case TYPEOF:
                return true;
            case FUNCTION:
                for (auto& param : m_typeform.m_function.m_parameters)
                {
                    if (param->_check_if_has_typeof())
                        return true;
                }
                return m_typeform.m_function.m_return_type->_check_if_has_typeof();
            case STRUCTURE:
                for (auto& field : m_typeform.m_structure.m_fields)
                {
                    if (field->m_type->_check_if_has_typeof())
                        return true;
                }
                return false;
            case TUPLE:
                for (auto& field : m_typeform.m_tuple.m_fields)
                {
                    if (field->_check_if_has_typeof())
                        return true;
                }
                return false;
            case UNION:
                return false;
            default:
                wo_error("Unknown type form.");
                return false;
            }
        }
        void AstTypeHolder::_check_if_template_exist_in(
            const std::list<wo_pstring_t>& template_params, std::vector<bool>& out_contain_flags) const
        {
            switch (m_formal)
            {
            case IDENTIFIER:
                if (m_typeform.m_identifier->m_formal == AstIdentifier::identifier_formal::FROM_TYPE)
                {
                    m_typeform.m_identifier->m_from_type.value()->_check_if_template_exist_in(template_params, out_contain_flags);
                    return;
                }
                else if (m_typeform.m_identifier->m_formal == AstIdentifier::identifier_formal::FROM_CURRENT)
                {
                    if (m_typeform.m_identifier->m_scope.empty())
                    {
                        size_t offset = 0;
                        for (auto& template_ : template_params)
                        {
                            if (m_typeform.m_identifier->m_name == template_)
                                out_contain_flags[offset] = true;
                            ++offset;
                        }
                    }
                }
                if (m_typeform.m_identifier->m_template_arguments)
                    for (auto& template_argument : m_typeform.m_identifier->m_template_arguments.value())
                        template_argument->_check_if_template_exist_in(template_params, out_contain_flags);
                break;
            case TYPEOF:
                // We are not able to check template in typefrom.
                break;
            case FUNCTION:
                for (auto& param : m_typeform.m_function.m_parameters)
                    param->_check_if_template_exist_in(template_params, out_contain_flags);
                m_typeform.m_function.m_return_type->_check_if_template_exist_in(template_params, out_contain_flags);
                break;
            case STRUCTURE:
                for (auto& field : m_typeform.m_structure.m_fields)
                    field->m_type->_check_if_template_exist_in(template_params, out_contain_flags);
                break;
            case TUPLE:
                for (auto& field : m_typeform.m_tuple.m_fields)
                    field->_check_if_template_exist_in(template_params, out_contain_flags);
                break;
            default:
                wo_error("Unknown type form.");
                break;
            }
        }
        AstBase* AstTypeHolder::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstTypeHolder* new_instance = exist_instance
                ? static_cast<AstTypeHolder*>(exist_instance.value())
                : nullptr;

            switch (m_formal)
            {
            case IDENTIFIER:
                if (new_instance == nullptr)new_instance = new AstTypeHolder(m_typeform.m_identifier);
                out_continues.push_back(AstBase::make_holder(&new_instance->m_typeform.m_identifier));
                break;
            case TYPEOF:
                if (new_instance == nullptr)new_instance = new AstTypeHolder(m_typeform.m_typefrom);
                out_continues.push_back(AstBase::make_holder(&new_instance->m_typeform.m_typefrom));
                break;
            case FUNCTION:
                if (new_instance == nullptr)new_instance = new AstTypeHolder(m_typeform.m_function);
                for (auto& param : new_instance->m_typeform.m_function.m_parameters)
                    out_continues.push_back(AstBase::make_holder(&param));
                out_continues.push_back(AstBase::make_holder(&new_instance->m_typeform.m_function.m_return_type));
                break;
            case STRUCTURE:
                if (new_instance == nullptr)new_instance = new AstTypeHolder(m_typeform.m_structure);
                for (auto& field : new_instance->m_typeform.m_structure.m_fields)
                    out_continues.push_back(AstBase::make_holder(&field));
                break;
            case TUPLE:
                if (new_instance == nullptr)new_instance = new AstTypeHolder(m_typeform.m_tuple);
                for (auto& field : new_instance->m_typeform.m_tuple.m_fields)
                    out_continues.push_back(AstBase::make_holder(&field));
                break;
            case UNION:
                if (new_instance == nullptr)new_instance = new AstTypeHolder(m_typeform.m_union);
                for (auto& field : new_instance->m_typeform.m_union.m_fields)
                    if (field.m_item)
                        out_continues.push_back(AstBase::make_holder(&field.m_item.value()));
                break;
            default:
                wo_error("Unknown type form.");
            }

            new_instance->m_mutable_mark = m_mutable_mark;

            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstValueBase::AstValueBase(AstBase::node_type_t nodetype)
            : AstBase(nodetype)
            , m_LANG_determined_type(std::nullopt)
            , m_evaled_const_value(std::nullopt)
        {
        }
        AstValueBase::~AstValueBase()
        {
            if (m_evaled_const_value && m_evaled_const_value.value().is_gcunit())
                delete m_evaled_const_value.value().gcunit;
        }
        void AstValueBase::decide_final_constant_value(const wo::value& val)
        {
            wo_assert(!m_evaled_const_value);
           
            m_evaled_const_value = wo::value();
            m_evaled_const_value->set_val_compile_time(&val);

        }
        void AstValueBase::decide_final_constant_value(const std::string& cstr)
        {
            wo_assert(!m_evaled_const_value);

            m_evaled_const_value = wo::value();
            m_evaled_const_value->set_string_nogc(cstr);
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

        AstValueLiteral::AstValueLiteral()
            : AstValueBase(AST_VALUE_LITERAL)
        {
        }
        AstBase* AstValueLiteral::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstValueLiteral* new_instance = exist_instance
                ? static_cast<AstValueLiteral*>(exist_instance.value())
                : new AstValueLiteral()
                ;
            AstValueBase::make_dup(new_instance, out_continues);
            new_instance->m_evaled_const_value = wo::value();
            new_instance->m_evaled_const_value.value().set_val_compile_time(
                &m_evaled_const_value.value());
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

        AstValueVariable::AstValueVariable(AstIdentifier* identifier)
            : AstValueBase(AST_VALUE_VARIABLE)
            , m_identifier(identifier)
            , m_LANG_template_evalating_state(std::nullopt)
            , m_LANG_variable_instance(std::nullopt)
            , m_LANG_trying_advancing_type_judgement(false)
        {

        }
        AstBase* AstValueVariable::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstValueVariable* new_instance = exist_instance
                ? static_cast<AstValueVariable*>(exist_instance.value())
                : new AstValueVariable(m_identifier)
                ;
            AstValueBase::make_dup(new_instance, out_continues);
            out_continues.push_back(AstBase::make_holder(&new_instance->m_identifier));
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

        AstValueVariadicArgumentsPack::AstValueVariadicArgumentsPack()
            : AstValueBase(AST_VALUE_VARIADIC_ARGUMENTS_PACK)
        {
        }
        AstBase* AstValueVariadicArgumentsPack::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstValueVariadicArgumentsPack* new_instance = exist_instance
                ? static_cast<AstValueVariadicArgumentsPack*>(exist_instance.value())
                : new AstValueVariadicArgumentsPack()
                ;
            AstValueBase::make_dup(new_instance, out_continues);
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
            bool is_mutable,
            wo_pstring_t name,
            const std::optional<std::list<wo_pstring_t>>& template_parameters)
            : AstPatternBase(AST_PATTERN_SINGLE)
            , m_is_mutable(is_mutable)
            , m_name(name)
            , m_template_parameters(template_parameters)
            , m_LANG_declared_symbol(std::nullopt)
        {
        }
        AstBase* AstPatternSingle::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstPatternSingle* new_instance = exist_instance
                ? static_cast<AstPatternSingle*>(exist_instance.value())
                : new AstPatternSingle(m_is_mutable, m_name, m_template_parameters)
                ;
            AstPatternBase::make_dup(new_instance, out_continues);
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstPatternTuple::AstPatternTuple(
            const std::list<AstPatternBase*>& fields)
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

        AstPatternUnion::AstPatternUnion(wo_pstring_t tag, std::optional<AstPatternBase*> field)
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
            if (new_instance->m_field)
                out_continues.push_back(AstBase::make_holder(&new_instance->m_field.value()));
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstValueIndex::AstValueIndex(AstValueBase* container, AstValueBase* index)
            : AstValueBase(AST_VALUE_INDEX)
            , m_container(container)
            , m_index(index)
            , m_LANG_fast_index_for_struct(std::nullopt)
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

        AstPatternVariable::AstPatternVariable(AstValueVariable* variable)
            : AstPatternBase(AST_PATTERN_VARIABLE)
            , m_variable(variable)
        {
        }
        AstBase* AstPatternVariable::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstPatternVariable* new_instance = exist_instance
                ? static_cast<AstPatternVariable*>(exist_instance.value())
                : new AstPatternVariable(m_variable)
                ;
            AstPatternBase::make_dup(new_instance, out_continues);
            out_continues.push_back(AstBase::make_holder(&new_instance->m_variable));
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

        AstVariableDefineItem::AstVariableDefineItem(const AstVariableDefineItem& item)
            : AstBase(AST_VARIABLE_DEFINE_ITEM)
            , m_pattern(item.m_pattern)
            , m_init_value(item.m_init_value)
            , m_LANG_declare_attribute(std::nullopt)
        {
        }
        AstVariableDefineItem::AstVariableDefineItem(
            AstPatternBase* pattern,
            AstValueBase* init_value)
            : AstBase(AST_VARIABLE_DEFINE_ITEM)
            , m_pattern(pattern)
            , m_init_value(init_value)
            , m_LANG_declare_attribute(std::nullopt)
        {
            if (pattern->node_type == AST_PATTERN_SINGLE && init_value->node_type == AST_VALUE_FUNCTION)
            {
                AstPatternSingle* single_pattern = static_cast<AstPatternSingle*>(pattern);
                AstValueFunction* func_value = static_cast<AstValueFunction*>(init_value);

                if (func_value->m_pending_param_type_mark_template)
                {
                    if (!single_pattern->m_template_parameters)
                        single_pattern->m_template_parameters = std::list<wo_pstring_t>{};

                    for (auto*& p : func_value->m_pending_param_type_mark_template.value())
                        single_pattern->m_template_parameters.value().push_back(p);
                }
            }
        }
        AstBase* AstVariableDefineItem::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstVariableDefineItem* new_instance = exist_instance
                ? static_cast<AstVariableDefineItem*>(exist_instance.value())
                : new AstVariableDefineItem(*this)
                ;
            out_continues.push_back(AstBase::make_holder(&new_instance->m_pattern));
            out_continues.push_back(AstBase::make_holder(&new_instance->m_init_value));
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstVariableDefines::AstVariableDefines(const AstVariableDefines& item)
            : AstBase(AST_VARIABLE_DEFINES)
            , m_definitions(item.m_definitions)
            , m_attribute(item.m_attribute)
        {
        }
        AstVariableDefines::AstVariableDefines(const std::optional<AstDeclareAttribue*>& attribute)
            : AstBase(AST_VARIABLE_DEFINES)
            , m_definitions{}
            , m_attribute(attribute)
        {
        }
        AstBase* AstVariableDefines::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstVariableDefines* new_instance = exist_instance
                ? static_cast<AstVariableDefines*>(exist_instance.value())
                : new AstVariableDefines(*this)
                ;
            new_instance->m_definitions = m_definitions;
            for (auto*& def : new_instance->m_definitions)
                out_continues.push_back(AstBase::make_holder(&def));

            if (m_attribute)
                out_continues.push_back(AstBase::make_holder(&new_instance->m_attribute.value()));

            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstFunctionParameterDeclare::AstFunctionParameterDeclare(AstPatternBase* m_pattern, std::optional<AstTypeHolder*> m_type)
            : AstBase(AST_FUNCTION_PARAMETER_DECLARE)
            , m_pattern(m_pattern)
            , m_type(m_type)
        {
        }
        AstBase* AstFunctionParameterDeclare::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstFunctionParameterDeclare* new_instance = exist_instance
                ? static_cast<AstFunctionParameterDeclare*>(exist_instance.value())
                : new AstFunctionParameterDeclare(m_pattern, m_type)
                ;
            out_continues.push_back(AstBase::make_holder(&new_instance->m_pattern));
            if (m_type)
                out_continues.push_back(AstBase::make_holder(&new_instance->m_type.value()));
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstValueFunction::AstValueFunction(
            const std::list<AstFunctionParameterDeclare*>& parameters,
            bool is_variadic,
            const std::optional<std::list<wo_pstring_t>>& defined_function_template_only_for_lambda,
            const std::optional<AstTypeHolder*>& marked_return_type,
            const std::optional<AstWhereConstraints*>& where_constraints,
            AstBase* body)
            : AstValueBase(AST_VALUE_FUNCTION)
            , m_parameters(parameters)
            , m_is_variadic(is_variadic)
            , m_marked_return_type(marked_return_type)
            , m_where_constraints(where_constraints)
            , m_pending_param_type_mark_template(defined_function_template_only_for_lambda)
            , m_body(body)
            , m_LANG_determined_return_type(std::nullopt)
            , m_LANG_hold_state(UNPROCESSED)
            , m_LANG_value_instance_to_update(std::nullopt)
        {
            for (auto* param_define : parameters)
            {
                if (!param_define->m_type)
                {
                    if (!m_pending_param_type_mark_template)
                        m_pending_param_type_mark_template = std::list<wo_pstring_t>{};

                    auto& pending_param_type_mark_template = 
                        m_pending_param_type_mark_template.value();

                    wo_pstring_t pending_template_name
                        = wstring_pool::get_pstr(L"*T" + std::to_wstring(pending_param_type_mark_template.size()));
                    pending_param_type_mark_template.push_back(pending_template_name);

                    AstIdentifier* template_identifier = new AstIdentifier(pending_template_name);
                    AstTypeHolder* template_type = new AstTypeHolder(template_identifier);
                    param_define->m_type = template_type;

                    template_identifier->source_location = param_define->source_location;
                    template_type->source_location = param_define->source_location;
                }
            }
        }
        AstBase* AstValueFunction::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstValueFunction* new_instance = exist_instance
                ? static_cast<AstValueFunction*>(exist_instance.value())
                : new AstValueFunction(m_parameters, m_is_variadic, m_pending_param_type_mark_template, m_marked_return_type, m_where_constraints, m_body)
                ;

            for (auto& param : new_instance->m_parameters)
                out_continues.push_back(AstBase::make_holder(&param));
            if (m_marked_return_type)
                out_continues.push_back(AstBase::make_holder(&new_instance->m_marked_return_type.value()));
            if (m_where_constraints)
                out_continues.push_back(AstBase::make_holder(&new_instance->m_where_constraints.value()));
            out_continues.push_back(AstBase::make_holder(&new_instance->m_body));
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstValueArrayOrVec::AstValueArrayOrVec(const std::list<AstValueBase*>& elements, bool making_vec)
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

        AstValueDictOrMap::AstValueDictOrMap(const std::list<AstKeyValuePair*>& elements, bool making_map)
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

        AstValueTuple::AstValueTuple(const std::list<AstValueBase*>& elements)
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

        AstStructFieldValuePair::AstStructFieldValuePair(wo_pstring_t name, AstValueBase* value)
            : AstBase(AST_STRUCT_FIELD_VALUE_PAIR)
            , m_name(name)
            , m_value(value)
        {
        }
        AstBase* AstStructFieldValuePair::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstStructFieldValuePair* new_instance = exist_instance
                ? static_cast<AstStructFieldValuePair*>(exist_instance.value())
                : new AstStructFieldValuePair(m_name, m_value)
                ;
            out_continues.push_back(AstBase::make_holder(&new_instance->m_value));
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstValueStruct::AstValueStruct(
            const std::optional<AstTypeHolder*>& marked_struct_type,
            const std::list<AstStructFieldValuePair*>& fields)
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
            bool valued_assign,
            assign_type type,
            AstPatternBase* assign_place,
            AstValueBase* right)
            : AstValueMayConsiderOperatorOverload(AST_VALUE_ASSIGN)
            , m_valued_assign(valued_assign)
            , m_assign_type(type)
            , m_assign_place(assign_place)
            , m_right(right)
        {
        }
        AstBase* AstValueAssign::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstValueAssign* new_instance = exist_instance
                ? static_cast<AstValueAssign*>(exist_instance.value())
                : new AstValueAssign(m_valued_assign, m_assign_type, m_assign_place, m_right)
                ;
            AstValueMayConsiderOperatorOverload::make_dup(new_instance, out_continues);
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

        AstNamespace::AstNamespace(wo_pstring_t name, AstBase* body)
            : AstBase(AST_NAMESPACE)
            , m_name(name)
            , m_body(body)
            , m_LANG_determined_namespace(std::nullopt)
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
            , m_LANG_determined_scope(std::nullopt)
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

        AstMatchCase::AstMatchCase(AstPatternBase* pattern, AstBase* body)
            : AstBase(AST_MATCH_CASE)
            , m_pattern(pattern)
            , m_body(body)
        {
        }
        AstBase* AstMatchCase::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstMatchCase* new_instance = exist_instance
                ? static_cast<AstMatchCase*>(exist_instance.value())
                : new AstMatchCase(m_pattern, m_body)
                ;
            out_continues.push_back(AstBase::make_holder(&new_instance->m_pattern));
            out_continues.push_back(AstBase::make_holder(&new_instance->m_body));
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstMatch::AstMatch(AstValueBase* match_value, const std::list<AstMatchCase*>& cases)
            : AstBase(AST_MATCH)
            , m_matched_value(match_value)
            , m_cases(cases)
        {
        }
        AstBase* AstMatch::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstMatch* new_instance = exist_instance
                ? static_cast<AstMatch*>(exist_instance.value())
                : new AstMatch(m_matched_value, m_cases)
                ;
            out_continues.push_back(AstBase::make_holder(&new_instance->m_matched_value));
            for (auto& c : new_instance->m_cases)
                out_continues.push_back(AstBase::make_holder(&c));
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstIf::AstIf(AstValueBase* condition, AstBase* true_body, const std::optional<AstBase*>& false_body)
            : AstBase(AST_IF)
            , m_condition(condition)
            , m_true_body(true_body)
            , m_false_body(false_body)
        {
        }
        AstBase* AstIf::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstIf* new_instance = exist_instance
                ? static_cast<AstIf*>(exist_instance.value())
                : new AstIf(m_condition, m_true_body, m_false_body)
                ;
            out_continues.push_back(AstBase::make_holder(&new_instance->m_condition));
            out_continues.push_back(AstBase::make_holder(&new_instance->m_true_body));
            if (m_false_body)
                out_continues.push_back(AstBase::make_holder(&new_instance->m_false_body.value()));
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstWhile::AstWhile(AstValueBase* condition, AstBase* body)
            : AstBase(AST_WHILE)
            , m_condition(condition)
            , m_body(body)
        {
        }
        AstBase* AstWhile::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstWhile* new_instance = exist_instance
                ? static_cast<AstWhile*>(exist_instance.value())
                : new AstWhile(m_condition, m_body)
                ;
            out_continues.push_back(AstBase::make_holder(&new_instance->m_condition));
            out_continues.push_back(AstBase::make_holder(&new_instance->m_body));
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstFor::AstFor(
            std::optional<AstBase*> initial,
            std::optional<AstValueBase*> condition,
            std::optional<AstValueBase*> step,
            AstBase* body)
            : AstBase(AST_FOR)
            , m_initial(initial)
            , m_condition(condition)
            , m_step(step)
            , m_body(body)
        {
        }
        AstBase* AstFor::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstFor* new_instance = exist_instance
                ? static_cast<AstFor*>(exist_instance.value())
                : new AstFor(m_initial, m_condition, m_step, m_body)
                ;
            if (new_instance->m_initial)
                out_continues.push_back(AstBase::make_holder(&new_instance->m_initial.value()));
            if (new_instance->m_condition)
                out_continues.push_back(AstBase::make_holder(&new_instance->m_condition.value()));
            if (new_instance->m_step)
                out_continues.push_back(AstBase::make_holder(&new_instance->m_step.value()));
            out_continues.push_back(AstBase::make_holder(&new_instance->m_body));
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstForeach::AstForeach(AstScope* job)
            : AstBase(AST_FOREACH)
            , m_job(job)
        {
        }
        AstForeach::AstForeach(AstPatternBase* pattern, AstValueBase* container, AstBase* body)
            : AstBase(AST_FOREACH)
            , m_job(nullptr)
        {
            auto* job_list = new AstList();

            // let $_iter = std::iterator(container);
            auto* std_iterator_identifier = new AstIdentifier(WO_PSTR(iterator), std::nullopt, { WO_PSTR(std) }, true);
            auto* std_iterator_value = new AstValueVariable(std_iterator_identifier);
            auto* invoke_std_iterator = new AstValueFunctionCall(false, std_iterator_value, { container });
            auto* iterator_declear = new AstVariableDefines(std::nullopt);

            auto* iterator_pattern = new AstPatternSingle(false, WO_PSTR(_iter), std::nullopt);
            auto* iterator_define_item = new AstVariableDefineItem(iterator_pattern, invoke_std_iterator);
            iterator_declear->m_definitions.push_back(iterator_define_item);

            // for (;;) {
            //    match ($_iter.next()) {
            auto* iterator_identifier = new AstIdentifier(WO_PSTR(_iter), std::nullopt, {}, false);
            auto* iterator_value = new AstValueVariable(iterator_identifier);
            auto* next_identifier = new AstIdentifier(WO_PSTR(next), std::nullopt, {}, false);
            auto* next_value = new AstValueVariable(next_identifier);
            auto* invoke_next = new AstValueFunctionCall(true, next_value, { iterator_value });

            //        value(PPP)? BODY
            auto* match_cast_value_pattern = new AstPatternUnion(WO_PSTR(value), pattern);
            auto* match_case_value = new AstMatchCase(match_cast_value_pattern, body);
            //        _? break;
            auto* match_case_none_pattern = new AstPatternTakeplace();
            auto* match_case_none = new AstMatchCase(match_case_none_pattern, new AstBreak(std::nullopt));
            auto* match_body = new AstMatch(invoke_next, { match_case_value, match_case_none });
            //    }
            // }
            auto* for_body = new AstFor(std::nullopt, std::nullopt, std::nullopt, match_body);

            job_list->m_list.push_back(for_body);

            m_job = new AstScope(job_list);

            // Update source msg;
            std_iterator_identifier->source_location = container->source_location;
            std_iterator_value->source_location = container->source_location;
            invoke_std_iterator->source_location = container->source_location;
            iterator_define_item->source_location = container->source_location;
            iterator_declear->source_location = container->source_location;
            iterator_pattern->source_location = container->source_location;

            iterator_identifier->source_location = container->source_location;
            iterator_value->source_location = pattern->source_location;
            next_identifier->source_location = pattern->source_location;
            next_value->source_location = pattern->source_location;
            invoke_next->source_location = pattern->source_location;

            match_cast_value_pattern->source_location = pattern->source_location;
            match_case_value->source_location = pattern->source_location;

            match_case_none_pattern->source_location = pattern->source_location;
            match_case_none->source_location = pattern->source_location;

            match_body->source_location = body->source_location;
            for_body->source_location = body->source_location;
            job_list->source_location = body->source_location;
            m_job->source_location = body->source_location;
        }
        AstBase* AstForeach::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstForeach* new_instance = exist_instance
                ? static_cast<AstForeach*>(exist_instance.value())
                : new AstForeach(m_job)
                ;
            if (new_instance->m_job)
                out_continues.push_back(AstBase::make_holder(&new_instance->m_job));
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstBreak::AstBreak(std::optional<wo_pstring_t> label)
            : AstBase(AST_BREAK)
            , m_label(label)
        {
        }
        AstBase* AstBreak::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstBreak* new_instance = exist_instance
                ? static_cast<AstBreak*>(exist_instance.value())
                : new AstBreak(m_label)
                ;
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstContinue::AstContinue(std::optional<wo_pstring_t> label)
            : AstBase(AST_CONTINUE)
            , m_label(label)
        {
        }
        AstBase* AstContinue::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstContinue* new_instance = exist_instance
                ? static_cast<AstContinue*>(exist_instance.value())
                : new AstContinue(m_label)
                ;
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstReturn::AstReturn(const std::optional<AstValueBase*>& value)
            : AstBase(AST_RETURN)
            , m_value(value)
        {
        }
        AstBase* AstReturn::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstReturn* new_instance = exist_instance
                ? static_cast<AstReturn*>(exist_instance.value())
                : new AstReturn(m_value)
                ;
            if (new_instance->m_value)
                out_continues.push_back(AstBase::make_holder(&new_instance->m_value.value()));
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstLabeled::AstLabeled(wo_pstring_t label, AstBase* body)
            : AstBase(AST_LABELED)
            , m_label(label)
            , m_body(body)
        {
        }
        AstBase* AstLabeled::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstLabeled* new_instance = exist_instance
                ? static_cast<AstLabeled*>(exist_instance.value())
                : new AstLabeled(m_label, m_body)
                ;
            out_continues.push_back(AstBase::make_holder(&new_instance->m_body));
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstUsingTypeDeclare::AstUsingTypeDeclare(const AstUsingTypeDeclare& item)
            : AstBase(AST_USING_TYPE_DECLARE)
            , m_typename(item.m_typename)
            , m_template_parameters(item.m_template_parameters)
            , m_type(item.m_type)
            , m_in_type_namespace(item.m_in_type_namespace)
            , m_attribute(item.m_attribute)
            , m_LANG_declared_symbol(std::nullopt)
            , m_LANG_hold_state(LANG_hold_state::UNPROCESSED)
        {
        }
        AstUsingTypeDeclare::AstUsingTypeDeclare(
            const std::optional<AstDeclareAttribue*>& attrib,
            wo_pstring_t typename_,
            const std::optional<std::list<wo_pstring_t>>& template_parameters,
            AstTypeHolder* type,
            const std::optional<AstNamespace*>& in_type_namespace)
            : AstBase(AST_USING_TYPE_DECLARE)
            , m_typename(typename_)
            , m_template_parameters(template_parameters)
            , m_type(type)
            , m_in_type_namespace(in_type_namespace)
            , m_attribute(attrib)
            , m_LANG_declared_symbol(std::nullopt)
            , m_LANG_hold_state(LANG_hold_state::UNPROCESSED)
        {
        }
        AstBase* AstUsingTypeDeclare::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstUsingTypeDeclare* new_instance = exist_instance
                ? static_cast<AstUsingTypeDeclare*>(exist_instance.value())
                : new AstUsingTypeDeclare(*this)
                ;

            out_continues.push_back(AstBase::make_holder(&new_instance->m_type));
            if (m_in_type_namespace)
                out_continues.push_back(AstBase::make_holder(&new_instance->m_in_type_namespace.value()));
            if (m_attribute)
                out_continues.push_back(AstBase::make_holder(&new_instance->m_attribute.value()));
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstAliasTypeDeclare::AstAliasTypeDeclare(const AstAliasTypeDeclare& item)
            : AstBase(AST_ALIAS_TYPE_DECLARE)
            , m_typename(item.m_typename)
            , m_template_parameters(item.m_template_parameters)
            , m_type(item.m_type)
            , m_attribute(item.m_attribute)
            , m_LANG_declared_symbol(std::nullopt)
        {
        }
        AstAliasTypeDeclare::AstAliasTypeDeclare(
            const std::optional<AstDeclareAttribue*>& attrib,
            wo_pstring_t typename_,
            const std::optional<std::list<wo_pstring_t>>& template_parameters,
            AstTypeHolder* type)
            : AstBase(AST_ALIAS_TYPE_DECLARE)
            , m_typename(typename_)
            , m_template_parameters(template_parameters)
            , m_type(type)
            , m_attribute(attrib)
            , m_LANG_declared_symbol(std::nullopt)
        {
        }
        AstBase* AstAliasTypeDeclare::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstAliasTypeDeclare* new_instance = exist_instance
                ? static_cast<AstAliasTypeDeclare*>(exist_instance.value())
                : new AstAliasTypeDeclare(*this)
                ;

            out_continues.push_back(AstBase::make_holder(&new_instance->m_type));
            if (m_attribute)
                out_continues.push_back(AstBase::make_holder(&new_instance->m_attribute.value()));
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstEnumItem::AstEnumItem(wo_pstring_t name, std::optional<AstValueBase*> value)
            : AstBase(AST_ENUM_ITEM)
            , m_name(name)
            , m_value(value)
        {
        }
        AstBase* AstEnumItem::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstEnumItem* new_instance = exist_instance
                ? static_cast<AstEnumItem*>(exist_instance.value())
                : new AstEnumItem(m_name, m_value)
                ;
            if (new_instance->m_value)
                out_continues.push_back(AstBase::make_holder(&new_instance->m_value.value()));
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstEnumDeclare::AstEnumDeclare(const AstEnumDeclare& item)
            : AstBase(AST_ENUM_DECLARE)
            , m_enum_type_declare(item.m_enum_type_declare)
            , m_enum_body(item.m_enum_body)
        {
        }
        AstEnumDeclare::AstEnumDeclare(
            const std::optional<AstDeclareAttribue*>& attrib,
            wo_pstring_t enum_name,
            const std::list<AstEnumItem*>& enum_items)
            : AstBase(AST_ENUM_DECLARE)
            , m_enum_type_declare(nullptr)
            , m_enum_body(nullptr)
        {
            wo_assert(!enum_items.empty());

            auto* enum_base_type_identifier = new AstIdentifier(WO_PSTR(int), std::nullopt, {}, true);
            auto* enum_base_type = new AstTypeHolder(enum_base_type_identifier);
            auto* enum_type_declare = new AstUsingTypeDeclare(attrib, enum_name, std::nullopt, enum_base_type, std::nullopt);

            std::optional<AstDeclareAttribue*> enum_item_attrib = std::nullopt;
            if (attrib)
            {
                AstDeclareAttribue* enum_item_attrib_instance = new AstDeclareAttribue(*attrib.value());
                enum_item_attrib = enum_item_attrib_instance;

                enum_item_attrib_instance->source_location = attrib.value()->source_location;
            }

            auto* enum_item_definations = new AstVariableDefines(enum_item_attrib);

            std::optional<wo_pstring_t> last_enum_item_name = std::nullopt;
            for (auto& item : enum_items)
            {
                if (!item->m_value)
                {
                    // No initial value specified, use the previous value + 1.
                    AstValueBase* last_enum_item_value;
                    std::optional<AstIdentifier*> last_enum_identifier = std::nullopt;
                    if (last_enum_item_name)
                    {
                        auto* last_enum_identifier_instance = new AstIdentifier(last_enum_item_name.value());
                        last_enum_item_value = new AstValueVariable(last_enum_identifier_instance);

                        last_enum_identifier = last_enum_identifier_instance;
                    }
                    else
                    {
                        wo::value last_enum_item_value_value;
                        last_enum_item_value_value.set_integer(0);
                        last_enum_item_value = new AstValueLiteral();
                        last_enum_item_value->decide_final_constant_value(last_enum_item_value_value);
                    }

                    auto* int_type_identifier = new AstIdentifier(WO_PSTR(int), std::nullopt, {}, true);
                    auto* int_type = new AstTypeHolder(int_type_identifier);
                    auto* cast_last_enum_item_value_into_int = new AstValueTypeCast(int_type, last_enum_item_value);
                    wo::value one_literal_value;
                    one_literal_value.set_integer(1);
                    auto* one_literal = new AstValueLiteral();
                    one_literal->decide_final_constant_value(one_literal_value);
                    auto* created_this_item_value = new AstValueBinaryOperator(
                        AstValueBinaryOperator::operator_type::ADD, cast_last_enum_item_value_into_int, one_literal);

                    item->m_value = created_this_item_value;

                    // Update source msg;
                    last_enum_item_value->source_location = item->source_location;
                    if (last_enum_identifier)
                        last_enum_identifier.value()->source_location = item->source_location;
                    int_type_identifier->source_location = item->source_location;
                    int_type->source_location = item->source_location;
                    cast_last_enum_item_value_into_int->source_location = item->source_location;
                    one_literal->source_location = item->source_location;
                    created_this_item_value->source_location = item->source_location;
                }

                auto* enum_type_identifier = new AstIdentifier(enum_name);
                auto* enum_type = new AstTypeHolder(enum_type_identifier);
                auto* enum_item_value_cast = new AstValueTypeCast(enum_type, item->m_value.value());
                auto* enum_item_pattern = new AstPatternSingle(false, item->m_name, std::nullopt);
                auto* enum_item_define_item = new AstVariableDefineItem(enum_item_pattern, enum_item_value_cast);
                enum_item_definations->m_definitions.push_back(enum_item_define_item);

                // Update source msg;
                enum_type_identifier->source_location = item->source_location;
                enum_type->source_location = item->source_location;
                enum_item_value_cast->source_location = item->source_location;
                enum_item_pattern->source_location = item->source_location;
                enum_item_define_item->source_location = item->source_location;

                last_enum_item_name = item->m_name;
            }

            auto* enum_namespace = new AstNamespace(enum_name, enum_item_definations);

            m_enum_type_declare = enum_type_declare;
            m_enum_body = enum_namespace;

            // Update source msg;
            enum_base_type_identifier->source_location = enum_items.front()->source_location;
            enum_base_type->source_location = enum_base_type_identifier->source_location;
            enum_type_declare->source_location = enum_base_type_identifier->source_location;
            enum_item_definations->source_location = enum_base_type_identifier->source_location;
            enum_namespace->source_location = enum_base_type_identifier->source_location;
        }
        AstBase* AstEnumDeclare::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstEnumDeclare* new_instance = exist_instance
                ? static_cast<AstEnumDeclare*>(exist_instance.value())
                : new AstEnumDeclare(*this)
                ;
            if (new_instance->m_enum_type_declare)
                out_continues.push_back(AstBase::make_holder(&new_instance->m_enum_type_declare));
            if (new_instance->m_enum_body)
                out_continues.push_back(AstBase::make_holder(&new_instance->m_enum_body));
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstValueMakeUnion::AstValueMakeUnion(
            wo_integer_t index,
            const std::optional<AstValueBase*>& packed_value)
            : AstValueBase(AST_VALUE_MAKE_UNION)
            , m_index(index)
            , m_packed_value(packed_value)
        {
        }
        AstBase* AstValueMakeUnion::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstValueMakeUnion* new_instance = exist_instance
                ? static_cast<AstValueMakeUnion*>(exist_instance.value())
                : new AstValueMakeUnion(m_index, m_packed_value)
                ;
            if (new_instance->m_packed_value)
                out_continues.push_back(AstBase::make_holder(&new_instance->m_packed_value.value()));
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstUnionItem::AstUnionItem(wo_pstring_t name, std::optional<AstTypeHolder*> type)
            : AstBase(AST_UNION_ITEM)
            , m_label(name)
            , m_type(type)
        {
        }
        AstBase* AstUnionItem::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstUnionItem* new_instance = exist_instance
                ? static_cast<AstUnionItem*>(exist_instance.value())
                : new AstUnionItem(m_label, m_type)
                ;
            if (new_instance->m_type)
                out_continues.push_back(AstBase::make_holder(&new_instance->m_type.value()));
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstUnionDeclare::AstUnionDeclare(
            const AstUnionDeclare& another)
            : AstBase(AST_UNION_DECLARE)
            //, m_union_type_name(another.m_union_type_name)
            //, m_template_parameters(another.m_template_parameters)
            , m_union_type_declare(another.m_union_type_declare)
            //, m_union_items(another.m_union_items)
        {
        }
        AstUnionDeclare::AstUnionDeclare(
            const std::optional<AstDeclareAttribue*>& attrib,
            wo_pstring_t union_type_name,
            const std::optional<std::list<wo_pstring_t>>& template_parameters,
            const std::list<AstUnionItem*>& union_items)
            : AstBase(AST_UNION_DECLARE)
            //, m_union_type_name(union_type_name)
            //, m_template_parameters(template_parameters)
            , m_union_type_declare(nullptr)
            //, m_union_items(nullptr)
        {
            wo_assert(!union_items.empty());
            auto* union_item_or_creator_declare = new AstVariableDefines(attrib);

            AstTypeHolder::UnionType union_type_info;

            wo_integer_t current_item_index = 0;
            for (auto& item : union_items)
            {
                union_type_info.m_fields.push_back(
                    AstTypeHolder::UnionType::UnionField{
                        item->m_label,
                        item->m_type
                            ? std::optional(static_cast<AstTypeHolder*>(item->m_type.value()->clone()))
                            : std::nullopt
                    });

                AstTypeHolder* union_type;

                if (template_parameters)
                {
                    std::list<AstTypeHolder*> template_arguments(template_parameters.value().size());
                    for (auto& template_argument : template_arguments)
                    {
                        auto* template_argument_identifier = new AstIdentifier(WO_PSTR(nothing), std::nullopt, {}, true);
                        template_argument = new AstTypeHolder(template_argument_identifier);

                        template_argument_identifier->source_location = item->source_location;
                        template_argument->source_location = item->source_location;
                    }

                    auto* union_type_identifier = new AstIdentifier(union_type_name, template_arguments);
                    union_type = new AstTypeHolder(union_type_identifier);

                    union_type_identifier->source_location = item->source_location;
                }
                else
                {
                    auto* union_type_identifier = new AstIdentifier(union_type_name);
                    union_type = new AstTypeHolder(union_type_identifier);

                    union_type_identifier->source_location = item->source_location;
                }

                union_type->source_location = item->source_location;

                if (item->m_type)
                {
                    // let CREATOR<..> = func(e: ...){return $MAKE_UNION;}
                    auto* creator_param_type = item->m_type.value();

                    // Declare a function to construct the union item.
                    // 1) Count used template parameters.
                    std::list<wo_pstring_t> used_template_parameters;

                    if (template_parameters)
                    {
                        wo_assert(union_type->m_typeform.m_identifier->m_template_arguments);

                        bool creator_param_has_typeof = creator_param_type->_check_if_has_typeof();

                        // NOTE: We cannot check template param has been used or not in from typeof.
                        //  So we treat all template param as used if `creator_param_has_typeof`
                        std::vector<bool> used_template_parameters_flags(template_parameters.value().size(), creator_param_has_typeof);

                        if (!creator_param_has_typeof)
                            creator_param_type->_check_if_template_exist_in(template_parameters.value(), used_template_parameters_flags);

                        auto updating_union_type_template_argument_iter = 
                            union_type->m_typeform.m_identifier->m_template_arguments.value().begin();
                        auto template_parameters_iter = template_parameters.value().begin();

                        for (bool template_param_this_place_has_been_used : used_template_parameters_flags)
                        {
                            if (template_param_this_place_has_been_used)
                            {
                                // Used, update the template argument.
                                auto* new_template_identifier = new AstIdentifier(*template_parameters_iter);
                                auto* new_template = new AstTypeHolder(new_template_identifier);

                                *updating_union_type_template_argument_iter = new_template;

                                used_template_parameters.push_back(*template_parameters_iter);

                                // Update source msg;
                                new_template_identifier->source_location = item->source_location;
                                new_template->source_location = item->source_location;
                            }

                            ++updating_union_type_template_argument_iter;
                            ++template_parameters_iter;
                        }
                    }

                    auto* union_creator_value_identifier = new AstIdentifier(WO_PSTR(_val));
                    auto* union_creator_value = new AstValueVariable(union_creator_value_identifier);
                    auto* union_creator_make_union = new AstValueMakeUnion(current_item_index, union_creator_value);
                    auto* union_creator_return = new AstReturn(union_creator_make_union);

                    auto* union_creator_param_decl = new AstFunctionParameterDeclare(
                        new AstPatternSingle(false, WO_PSTR(_val), std::nullopt),
                        creator_param_type);
                    auto* union_creator_function = new AstValueFunction(
                        { union_creator_param_decl },
                        false,
                        std::nullopt,
                        union_type,
                        std::nullopt,
                        union_creator_return);

                    auto* union_creator_pattern = new AstPatternSingle(
                        false, item->m_label, used_template_parameters.empty() ? std::nullopt : std::optional(used_template_parameters));
                    auto* union_creator_decl_item = new AstVariableDefineItem(union_creator_pattern, union_creator_function);

                    union_item_or_creator_declare->m_definitions.push_back(union_creator_decl_item);

                    union_creator_value_identifier->source_location = item->source_location;
                    union_creator_value->source_location = item->source_location;
                    union_creator_make_union->source_location = item->source_location;
                    union_creator_return->source_location = item->source_location;
                    union_creator_param_decl->source_location = item->source_location;
                    union_creator_function->source_location = item->source_location;
                    union_creator_pattern->source_location = item->source_location;
                    union_creator_decl_item->source_location = item->source_location;
                }
                else
                {
                    // let ITEM = $MAKE_UNION: UNION_TYPE;
                    auto* union_item_pattern = new AstPatternSingle(false, item->m_label, std::nullopt);
                    auto* union_item_maker = new AstValueMakeUnion(current_item_index, std::nullopt);
                    auto* union_item_type_cast = new AstValueTypeCast(union_type, union_item_maker);
                    auto* union_item_decl_item = new AstVariableDefineItem(union_item_pattern, union_item_type_cast);
                    union_item_or_creator_declare->m_definitions.push_back(union_item_decl_item);

                    union_item_pattern->source_location = item->source_location;
                    union_item_maker->source_location = item->source_location;
                    union_item_type_cast->source_location = item->source_location;
                    union_item_decl_item->source_location = item->source_location;
                }

                ++current_item_index;
            }

            auto* union_namespace = new AstNamespace(union_type_name, union_item_or_creator_declare);
            AstTypeHolder* using_declare_union_type = new AstTypeHolder(union_type_info);
            AstUsingTypeDeclare* using_type_declare = new AstUsingTypeDeclare(
                attrib, union_type_name, template_parameters, using_declare_union_type, union_namespace);

            m_union_type_declare = using_type_declare;

            // Update source msg;
            union_item_or_creator_declare->source_location = union_items.front()->source_location;
            union_namespace->source_location = union_items.front()->source_location;
            using_declare_union_type->source_location = union_items.front()->source_location;
            using_type_declare->source_location = union_items.front()->source_location;
        }
        AstBase* AstUnionDeclare::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstUnionDeclare* new_instance = exist_instance
                ? static_cast<AstUnionDeclare*>(exist_instance.value())
                : new AstUnionDeclare(*this)
                ;
            out_continues.push_back(AstBase::make_holder(&new_instance->m_union_type_declare));
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstUsingNamespace::AstUsingNamespace(const std::list<wo_pstring_t>& using_namespace)
            : AstBase(AST_USING_NAMESPACE)
            , m_using_namespace(using_namespace)
        {
        }
        AstBase* AstUsingNamespace::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstUsingNamespace* new_instance = exist_instance
                ? static_cast<AstUsingNamespace*>(exist_instance.value())
                : new AstUsingNamespace(m_using_namespace)
                ;
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstToken::AstToken(const token& token)
            : AstBase(AST_TOKEN)
            , m_token(token)
        {
        }
        AstBase* AstToken::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstToken* new_instance = exist_instance
                ? static_cast<AstToken*>(exist_instance.value())
                : new AstToken(m_token)
                ;
            return new_instance;
        }

        ////////////////////////////////////////////////////////

        AstExternInformation::AstExternInformation(const AstExternInformation& item)
            : AstBase(AST_EXTERN_INFORMATION)
            , m_extern_symbol(item.m_extern_symbol)
            , m_extern_from_library(item.m_extern_from_library)
            , m_attribute_flags(item.m_attribute_flags)
        {
        }
        AstExternInformation::AstExternInformation(
            wo_pstring_t extern_symbol,
            const std::optional<wo_pstring_t>& extern_from_library,
            uint32_t attribute_flags)
            : AstBase(AST_EXTERN_INFORMATION)
            , m_extern_symbol(extern_symbol)
            , m_extern_from_library(extern_from_library)
            , m_attribute_flags(attribute_flags)
        {
        }
        AstBase* AstExternInformation::make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const
        {
            AstExternInformation* new_instance = exist_instance
                ? static_cast<AstExternInformation*>(exist_instance.value())
                : new AstExternInformation(*this)
                ;
            return new_instance;
        }

        ////////////////////////////////////////////////////////
    }
#endif
}