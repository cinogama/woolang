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

    namespace ast
    {
        struct astnode_builder
        {
            using ast_basic = wo::ast::AstBase;
            using inputs_t = std::vector<grammar::produce>;
            using builder_func_t = std::function<grammar::produce(lexer&, inputs_t&)>;

            virtual ~astnode_builder() = default;
            static grammar::produce build(lexer& lex, inputs_t& input)
            {
                wo_assert(false, "");
                return nullptr;
            }
        };

        inline std::unordered_map<size_t, size_t> _registed_builder_function_id_list;
        inline std::vector<astnode_builder::builder_func_t> _registed_builder_function;

        template <typename T>
        size_t _register_builder()
        {
            static_assert(std::is_base_of<astnode_builder, T>::value);
            _registed_builder_function.push_back(T::build);
            return _registed_builder_function.size();
        }

        template <typename T>
        size_t index()
        {
            size_t idx = _registed_builder_function_id_list[meta::type_hash<T>];
            wo_assert(idx != 0);
            return idx;
        }

        inline astnode_builder::builder_func_t get_builder(size_t idx)
        {
            wo_assert(idx != 0);
            return _registed_builder_function[idx - 1];
        }
    }

    namespace ast
    {
        void init_builder();

        struct AstTypeHolder;
        struct AstValueBase;

        struct Identifier
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
            std::list<AstTypeHolder*>
                                    m_template_arguments;

            // std::optional<lang_symbol*>
            //                         m_symbol;
            Identifier(wo_pstring_t identifier);
            Identifier(wo_pstring_t identifier, const std::list<AstTypeHolder*>& template_arguments);
            Identifier(wo_pstring_t identifier, const std::list<AstTypeHolder*>& template_arguments, const std::list<wo_pstring_t>& scopes, bool from_global);
            Identifier(wo_pstring_t identifier, const std::list<AstTypeHolder*>& template_arguments, const std::list<wo_pstring_t>& scopes, AstTypeHolder* from_type);

            Identifier(const Identifier& identifer);
            Identifier& operator = (const Identifier& identifer);

            Identifier(Identifier&& identifer) = delete;
            Identifier& operator = (Identifier&& identifer) = delete;

            void make_dup(AstBase::ContinuesList& out_continues);
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
            };
            enum mutable_mark
            {
                NONE,
                MARK_AS_MUTABLE,
                MARK_AS_IMMUTABLE,
            };

            struct FunctionType
            {
                std::vector<AstTypeHolder*> m_parameters;
                AstTypeHolder* m_return_type;
            };
            struct StructureType
            {
                std::vector<std::pair<wo_pstring_t, AstTypeHolder*>>
                    m_fields;
            };
            struct TupleType
            {
                std::vector<AstTypeHolder*>
                    m_fields;
            };

            mutable_mark    m_mutable_mark;
            type_formal     m_formal;            
            union
            {
                Identifier m_identifier;
                AstValueBase* m_typefrom;
                FunctionType m_function;
                StructureType m_structure;
                TupleType m_tuple;
            };

            std::optional<lang_TypeInstance*> m_type;

            AstTypeHolder(const Identifier& ident);
            AstTypeHolder(AstValueBase* expr);
            AstTypeHolder(const FunctionType& functype);
            AstTypeHolder(const StructureType& structtype);
            AstTypeHolder(const TupleType& tupletype);
            ~AstTypeHolder();

            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        struct AstValueBase : public AstBase
        {
            std::optional<lang_TypeInstance*> m_determined_type;
            std::optional<wo::value> m_evaled_const_value;

            AstValueBase(AstBase::node_type_t nodetype);

            // TBD: Not sure when and how to eval `type`.
            //using EvalTobeEvalConstList = std::list<AstValueBase*>;

            /*virtual void collect_eval_const_list(EvalTobeEvalConstList& out_vals) const = 0;
            virtual void eval_const_value() = 0;*/
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };

        struct AstValueMarkAsMutable: public AstValueBase
        {
            AstValueBase* m_marked_value;

            AstValueMarkAsMutable(AstValueBase* marking_value);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
        
        struct AstValueMarkAsImmutable : public AstValueBase
        {
            AstValueBase* m_marked_value;

            AstValueMarkAsImmutable(AstValueBase* marking_value);
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override;
        };
    }

    grammar::rule operator >> (grammar::rule ost, size_t builder_index);
#endif
}
