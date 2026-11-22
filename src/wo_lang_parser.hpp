#pragma once

#include <cstddef>
#include <initializer_list>
#include <vector>

#include "wo_ast_base.hpp"
#include "wo_lang_ast_builder.hpp"

#ifndef WO_DISABLE_COMPILER
namespace wo
{
namespace parser
{
    struct source_info
    {
        size_t row_no;
        size_t col_no;
    };

    /* Semantic value carried by the byacc parser stack.
     *
     * This deliberately stays a trivial aggregate: byacc's C skeleton copies
     * stack entries with memcpy/memset, which is only well-defined for
     * trivially copyable types. */
    struct value
    {
        bool is_ast;
        bool has_token;
        ast::AstBase* ast;
        lex_type tok_type;
        wo_pstring_t tok_identifier;
        source_info begin;
        source_info end;

        /* The start of the last token that belongs to this value.  The old
         * LR(1) driver read this from the lookahead token's pre-begin; byacc
         * may perform default reductions without reading a lookahead, so the
         * value carries it forward instead. */
        source_info last_token_begin;

        ::wo::produce to_produce() const
        {
            if (is_ast)
                return ::wo::produce(ast);
            return ::wo::produce(::wo::token{tok_type, tok_identifier});
        }
    };

    struct context
    {
        lexer* lex = nullptr;
        bool* out_is_incomplete = nullptr;

        bool has_error_node = false;
        bool at_eof = false;
        bool lookahead_valid = false;
        lexer::peeked_token_t lookahead{};

        value result{};
        bool result_valid = false;

        std::vector<::wo::produce> reduce_inputs;
    };

    template<typename BuilderT>
    inline size_t builder_id()
    {
        static const size_t id = ::wo::ast::index<BuilderT>();
        return id;
    }

    value reduce(size_t builder_index, context& ctx, std::initializer_list<value> rhs);
    value make_empty_token(context& ctx);

    ast::AstBase* parse_program(lexer& lex, bool* out_is_incomplete);
}

    /* Kept for the existing wo_init / wo_shutdown call sites. */
    void init_woolang_parser();
    void shutdown_woolang_parser();
}
#endif
