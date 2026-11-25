#pragma once
/*
 * Base AST types shared by the generated (byacc) parser and the AST builders:
 * AstAllocator / AstBase / AstList, the semantic value `produce`, `token`,
 * and the shared empty node `AstEmpty`.
 *
 * The former hand-written LR(1) grammar/table machinery has been removed;
 * the active parser is generated from `src/woolang.y` (see wo_lang_parser.hpp).
 */

#include "wo_compiler_lexer.hpp"
#include "wo_const_string_pool.hpp"

#include <cstddef>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <new>
#include <optional>
#include <set>
#include <stack>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

namespace wo
{
#ifndef WO_DISABLE_COMPILER
    namespace ast
    {
        class AstAllocator
        {
            std::vector<char*> m_allocated_pages;
            std::vector<AstBase*> m_created_ast_nodes;
            size_t m_allocated_offset_in_page;

            AstAllocator(const AstAllocator&) = delete;
            AstAllocator(AstAllocator&&) = delete;
            AstAllocator& operator = (const AstAllocator&) = delete;
            AstAllocator& operator = (AstAllocator&&) = delete;

        public:
            AstAllocator();
            ~AstAllocator();
            void* allocate_space_for_ast_node(size_t sz);
            void* allocate_space_with_aligh_for_ast_node(size_t sz, std::align_val_t al);
            void swap(AstAllocator& another);
            const std::vector<AstBase*>& get_allocated_nodes_for_lspv2() const;
            bool empty() const;
        private:
            void allocate_new_page();
        };
        class AstBase
        {
        public:
            enum node_type_t : uint8_t
            {
                AST_BASE,
                AST_LIST,
                AST_EMPTY,

                AST_TOKEN,
                AST_IDENTIFIER,

                AST_TEMPLATE_PARAM,
                AST_TEMPLATE_ARGUMENT,

                AST_ENUM_ITEM,
                AST_ENUM_DECLARE,
                AST_UNION_ITEM,
                AST_UNION_DECLARE,

                AST_USING_NAMESPACE,

                AST_DECLARE_ATTRIBUTE,
                AST_STRUCT_FIELD_DEFINE,

                AST_TYPE_DECLARE,
                AST_ALIAS_TYPE_DECLARE,
                AST_ALIAS_VALUE_DECLARE,

                AST_WHERE_CONSTRAINTS,

                AST_TYPE_HOLDER,

                AST_PATTERN_begin,
                AST_PATTERN_TAKEPLACE = AST_PATTERN_begin,
                AST_PATTERN_SINGLE,
                AST_PATTERN_TUPLE,
                AST_PATTERN_UNION,  // Only used in match
                AST_PATTERN_VARIABLE, // Only used in assign
                AST_PATTERN_INDEX,  // Only used in assign
                AST_PATTERN_end,

                AST_VARIABLE_DEFINE_ITEM,
                AST_VARIABLE_DEFINES,
                AST_KEY_VALUE_PAIR,
                AST_STRUCT_FIELD_VALUE_PAIR,
                AST_FUNCTION_PARAMETER_DECLARE,

                AST_VALUE_begin,
                AST_VALUE_NOTHING = AST_VALUE_begin,
                AST_VALUE_MARK_AS_MUTABLE,
                AST_VALUE_MARK_AS_IMMUTABLE,
                AST_VALUE_LITERAL,
                AST_VALUE_TYPEID,
                AST_VALUE_TYPE_CAST,
                AST_VALUE_DO_AS_VOID,
                AST_VALUE_TYPE_CHECK_IS,
                AST_VALUE_TYPE_CHECK_ASSERT,
                AST_VALUE_VARIABLE,
                AST_VALUE_FUNCTION_CALL,
                AST_VALUE_BINARY_OPERATOR,
                AST_VALUE_UNARY_OPERATOR,
                AST_VALUE_TRIBLE_OPERATOR,
                AST_VALUE_INDEX,
                AST_VALUE_FUNCTION,
                AST_VALUE_ARRAY_OR_VEC,
                AST_VALUE_DICT_OR_MAP,
                AST_VALUE_TUPLE,
                AST_VALUE_STRUCT,
                AST_VALUE_ASSIGN,
                AST_VALUE_MAKE_UNION,
                AST_VALUE_VARIADIC_ARGUMENTS_PACK,
                AST_VALUE_IR_OPNUM,
                AST_FAKE_VALUE_UNPACK,
                AST_VALUE_end,

                AST_NAMESPACE,
                AST_SCOPE,

                AST_MATCH,
                AST_MATCH_CASE,
                AST_IF,
                AST_WHILE,
                AST_FOR,
                AST_FOREACH,
                AST_DEFER,

                AST_BREAK,
                AST_CONTINUE,
                AST_RETURN,
                AST_LABELED,

                AST_EXTERN_INFORMATION,

                AST_NOP,

                AST_ECHO_FOR_REPL,

                //////////////////////////////////////////////////////

                AST_VALUE_FUNCTION_CALL_FAKE_AST_ARGUMENT_DEDUCTION_CONTEXT_A,
                AST_VALUE_FUNCTION_CALL_FAKE_AST_ARGUMENT_DEDUCTION_CONTEXT_B,

                AST_TEMPLATE_CONSTANT_TYPE_CHECK_IN_PASS1,
            };
        private:
            inline thread_local static AstAllocator* node_allocator = nullptr;
        public:
            static void clean_this_thread_ast()
            {
                if (nullptr == node_allocator)
                    return;

                delete node_allocator;
                node_allocator = nullptr;
            }
            static bool exchange_this_thread_ast(AstAllocator& in_out_swap_allocator)
            {
                wo_assert(
                    in_out_swap_allocator.empty()
                    || nullptr == node_allocator
                    || node_allocator->empty());

                if (!node_allocator)
                    node_allocator = new AstAllocator();

                in_out_swap_allocator.swap(*node_allocator);
                return !in_out_swap_allocator.empty();
            }

        public:
            struct location_t
            {
                size_t row;
                size_t column;

                bool operator < (const location_t& another_loc) const
                {
                    if (row < another_loc.row)
                        return true;
                    else if (row == another_loc.row)
                        return column < another_loc.column;
                    else
                        return false;
                }
            };
            struct source_location_t
            {
                location_t begin_at;
                location_t end_at;
                wo_pstring_t source_file;
                // Stable logical source identity used by compiler-semantic
                // mechanisms that compare source files across declarations
                // (LOCAL access check, using-namespace map key, import
                // visibility). Equal to `source_file` for normal files; the
                // REPL overrides it with a session-stable token so every
                // eval line shares one semantic identity, while `source_file`
                // keeps the unique per-snippet VFS path for error rendering.
                wo_pstring_t source_group;
            };

            const node_type_t node_type;
            uint8_t           finished_state;
            bool              duplicated_node;
            source_location_t source_location;

            AstBase(node_type_t ty)
                : node_type(ty)
                , finished_state(0)
                , duplicated_node(false)
                , source_location{}
            {
            }
            AstBase(const AstBase&) = delete;
            AstBase(AstBase&&) = delete;
            AstBase& operator = (const AstBase&) = delete;
            AstBase& operator = (AstBase&&) = delete;

            virtual ~AstBase() = default;

            void* operator new(size_t sz)
            {
                if (!node_allocator)
                    node_allocator = new AstAllocator();

                return node_allocator->allocate_space_for_ast_node(sz);
            }
            void* operator new[](size_t) = delete;
            void* operator new(size_t sz, std::align_val_t al)
            {
                if (!node_allocator)
                    node_allocator = new AstAllocator();

                return node_allocator->allocate_space_with_aligh_for_ast_node(sz, al);
            }
            void* operator new[](size_t, std::align_val_t) = delete;
            void operator delete(void*) noexcept
            {
                // Do nothing.
            }
            void operator delete[](void*) = delete;
            void operator delete(void*, std::align_val_t) noexcept
            {
                // Do nothing.
            }
            void operator delete[](void*, std::align_val_t) = delete;
        public:
            class _AstSafeHolderBase
            {
            public:
                virtual ~_AstSafeHolderBase() = default;
                virtual const AstBase* get() = 0;
                virtual void set(AstBase* ast) = 0;
            };
            template<typename T>
            class SafeHolder : public _AstSafeHolderBase
            {
                static_assert(std::is_base_of<AstBase, T>::value);

                T** m_ptr;
            public:
                explicit SafeHolder(T** location)
                    : m_ptr(location)
                {
                }
                virtual const AstBase* get() override
                {
                    return *m_ptr;
                }
                virtual void set(AstBase* ast) override
                {
                    *m_ptr = static_cast<T*>(ast);
                }
            };

            template<typename T>
            static std::unique_ptr<SafeHolder<T>> make_holder(T** location)
            {
                wo_assert(location != nullptr && *location != nullptr);
                return std::make_unique<SafeHolder<T>>(location);
            }

            using ContinuesList = std::vector<std::unique_ptr<_AstSafeHolderBase>>;
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const = 0;

            AstBase* clone() const
            {
                ContinuesList continues;

                AstBase* result = make_dup(std::nullopt, continues);
                result->source_location = source_location;
                result->duplicated_node = true;

                while (!continues.empty())
                {
                    auto holder = std::move(continues.back());
                    continues.pop_back();

                    auto* origin_holder_ast = holder->get();
                    auto* duped_holder_ast = origin_holder_ast->make_dup(std::nullopt, continues);
                    duped_holder_ast->source_location = origin_holder_ast->source_location;
                    duped_holder_ast->duplicated_node = true;
                    holder->set(duped_holder_ast);
                }

                return result;
            }
        };
        class AstList : public AstBase
        {
        public:
            std::vector<AstBase*> m_list;
            AstList()
                : AstBase(AST_LIST)
            {
            }
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override
            {
                auto* new_instance = exist_instance ? static_cast<AstList*>(exist_instance.value()) : new AstList();

                new_instance->m_list = m_list;

                for (auto*& dup_child : new_instance->m_list)
                    out_continues.push_back(make_holder(&dup_child));

                return new_instance;
            }
        };
        class AstNop : public AstBase
        {
        public:
            AstNop()
                : AstBase(AST_NOP)
            {
            }
            virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override
            {
                (void)out_continues;

                auto* new_instance = exist_instance
                    ? static_cast<AstNop*>(exist_instance.value())
                    : new AstNop();

                return new_instance;
            }
        };
    }

    struct token
    {
        lex_type     type;
        wo_pstring_t identifier;
    };

    inline std::ostream& operator << (std::ostream& os, const token& tk)
    {
        os
            << "{ token: "
            << (lex_type_base_t)tk.type
            << "    , \""
            << (tk.identifier ? *tk.identifier : std::string{})
            << "\"";

        if (tk.type == lex_type::l_error)
            os << "(error)";

        os << " }";
        return os;
    }

    struct produce
    {
        std::variant<ast::AstBase*, token> m_token_or_ast;
        produce(ast::AstBase* ast)
        {
            m_token_or_ast = ast;
        }
        produce(const token& token)
        {
            m_token_or_ast = token;
        }

        produce() = default;
        produce(const produce&) = default;
        produce(produce&&) = default;
        produce& operator = (const produce&) = default;
        produce& operator = (produce&&) = default;

        bool is_ast() const
        {
            return std::holds_alternative<ast::AstBase*>(m_token_or_ast);
        }
        bool is_token() const
        {
            return std::holds_alternative<token>(m_token_or_ast);
        }
        const token& read_token() const
        {
            if (!is_token())
                wo_error("read_token() called on an ast node.");
            return std::get<token>(m_token_or_ast);
        }
        ast::AstBase* read_ast() const
        {
            if (!is_ast())
                wo_error("read_ast() called on a token.");

            return std::get<ast::AstBase*>(m_token_or_ast);
        }
        ast::AstBase* read_ast(ast::AstBase::node_type_t required) const
        {
            auto* ast_node = read_ast();
            if (required != ast_node->node_type)
                wo_error("read_ast() called on a wrong type of ast node.");

            return ast_node;
        }
        ast::AstBase* read_ast_value() const
        {
            auto* ast_node = read_ast();
            if (ast_node->node_type < ast::AstBase::AST_VALUE_begin || ast_node->node_type >= ast::AstBase::AST_VALUE_end)
                wo_error("read_ast_value() called on a wrong type of ast node.");

            return ast_node;
        }
        ast::AstBase* read_ast_pattern() const
        {
            auto* ast_node = read_ast();
            if (ast_node->node_type < ast::AstBase::AST_PATTERN_begin || ast_node->node_type >= ast::AstBase::AST_PATTERN_end)
                wo_error("read_ast_pattern() called on a wrong type of ast node.");

            return ast_node;
        }
    };

    struct AstEmpty : public ast::AstBase
    {
        /* Shared node for empty/optional constructs; list builders skip it. */

        AstEmpty() : AstBase(AST_EMPTY)
        {
        }

        static bool is_empty(const produce& p)
        {
            if (p.is_ast())
            {
                if (p.read_ast()->node_type == AST_EMPTY)
                    return true;
            }
            if (p.is_token())
            {
                if (p.read_token().type == lex_type::l_empty)
                    return true;
            }

            return false;
        }

        virtual AstBase* make_dup(std::optional<AstBase*> exist_instance, ContinuesList& out_continues) const override final
        {
            (void)exist_instance;
            (void)out_continues;

            // Immutable and no state to be modified.
            return const_cast<AstEmpty*>(this);
        }
    };

#endif
}
