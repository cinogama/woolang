#pragma once

#include <cstddef>
#include <initializer_list>
#include <optional>
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

        /* Source span of this value: `begin` is where its first RHS symbol
         * starts, `end` just past its last token (a token's own end for
         * terminals, forwarded from the RHS for nonterminals).  The old
         * LR(1) driver read a node's end from the lookahead token's
         * pre-begin, i.e. from the end of the token before it; byacc may
         * perform default reductions without reading a lookahead, so the
         * value carries it forward instead. */
        source_info begin;
        source_info end;

        ::wo::produce to_produce() const
        {
            if (is_ast)
                return ::wo::produce(ast);
            return ::wo::produce(::wo::token{tok_type, tok_identifier});
        }
    };

    struct context
    {
        lexer& lex;
        std::optional<bool*> out_is_incomplete;

        context(lexer& lex_instance, std::optional<bool*> out_is_incomplete_flag)
            : lex(lex_instance)
            , out_is_incomplete(out_is_incomplete_flag)
            , has_error_node(false)
            , at_eof(false)
            , lookahead_valid(false)
            , lookahead{}
            , result{}
            , result_valid(false)
        {
        }

        bool has_error_node;
        bool at_eof;
        bool lookahead_valid;
        lexer::peeked_token_t lookahead;

        value result;
        bool result_valid;

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

    /* yyparse is emitted by byacc into woolang.tab.inc, which
     * woolang.tab.cpp includes inside namespace wo::parser; yylex and
     * yyerror are defined in wo_lang_parser.cpp. */
    int yyparse(context* ctx);
    int yylex(value* yylval, context* ctx);
    void yyerror(context* ctx, const char* s);

    std::optional<ast::AstBase*> parse_program(
        lexer& lex,
        std::optional<bool*> out_is_incomplete = std::nullopt);
}

    /* Kept for the existing wo_init / wo_shutdown call sites. */
    void init_woolang_parser();
    void shutdown_woolang_parser();
}
#endif
