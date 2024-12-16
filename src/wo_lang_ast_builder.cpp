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
        {

        }
        Identifier::Identifier(wo_pstring_t identifier, const std::list<AstTypeHolder*>& template_arguments)
            : m_formal(FROM_CURRENT)
            , m_from_type(std::nullopt)
            , m_scope({})
            , m_name(identifier)
            , m_template_arguments(template_arguments)
        {
        }
        Identifier::Identifier(wo_pstring_t identifier, const std::list<AstTypeHolder*>& template_arguments, const std::list<wo_pstring_t>& scopes, bool from_global)
            : m_formal(from_global ? FROM_GLOBAL : FROM_CURRENT)
            , m_from_type(std::nullopt)
            , m_scope(scopes)
            , m_name(identifier)
            , m_template_arguments(template_arguments)
        {

        }
        Identifier::Identifier(wo_pstring_t identifier, const std::list<AstTypeHolder*>& template_arguments, const std::list<wo_pstring_t>& scopes, AstTypeHolder* from_type)
            : m_formal(FROM_TYPE)
            , m_from_type(from_type)
            , m_scope(scopes)
            , m_name(identifier)
            , m_template_arguments(template_arguments)
        {

        }

        Identifier::Identifier(const Identifier& identifer)
            : m_formal(identifer.m_formal)
            , m_from_type(identifer.m_from_type)
            , m_scope(identifer.m_scope)
            , m_name(identifer.m_name)
            , m_template_arguments(identifer.m_template_arguments)
            //, m_symbol(std::nullopt)
        {

        }
        Identifier& Identifier::operator = (const Identifier& identifer)
        {
            m_formal = identifer.m_formal;
            m_from_type = identifer.m_from_type;
            m_scope = identifer.m_scope;
            m_name = identifer.m_name;
            m_template_arguments = identifer.m_template_arguments;
            //m_symbol = std::nullopt;

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

            out_continues.push_back(AstBase::make_holder(&new_instance->m_marked_value));
            return new_instance;
        }

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