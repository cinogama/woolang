#include "wo_afx.hpp"

#include "wo_lang_parser.hpp"

#ifndef WO_DISABLE_COMPILER

int yyparse(::wo::parser::context* ctx);
int yylex(::wo::parser::value* yylval, ::wo::parser::context* ctx);
void yyerror(::wo::parser::context* ctx, const char* s);

namespace wo
{
namespace parser
{
    value make_empty_token(context& ctx)
    {
        value result{};
        result.is_ast = false;
        result.has_token = false;
        result.ast = nullptr;
        result.tok_type = lex_type::l_empty;
        result.tok_identifier = nullptr;

        if (ctx.lookahead_valid)
        {
            result.begin = source_info{
                ctx.lookahead.m_token_begin[0],
                ctx.lookahead.m_token_begin[1] };
            result.end = source_info{
                ctx.lookahead.m_token_end[0],
                ctx.lookahead.m_token_end[1] };
            result.last_token_begin = result.begin;
        }

        return result;
    }

    value reduce(size_t builder_index, context& ctx, std::initializer_list<value> rhs)
    {
        wo_assert(ctx.lex != nullptr);

        if (ctx.has_error_node)
        {
            /* The old driver stopped invoking builders after an error token
             * had been reduced; keep that behaviour. */
            value result{};
            result.is_ast = false;
            result.has_token = false;
            result.ast = nullptr;
            result.tok_type = lex_type::l_error;
            result.tok_identifier = nullptr;

            if (ctx.lookahead_valid)
            {
                result.begin = source_info{
                    ctx.lookahead.m_token_end[0],
                    ctx.lookahead.m_token_end[1] };
            }
            result.end = result.begin;
            return result;
        }

        ctx.reduce_inputs.clear();
        ctx.reduce_inputs.reserve(rhs.size());
        for (const value& item : rhs)
            ctx.reduce_inputs.push_back(item.to_produce());

        auto builder = ::wo::ast::get_builder(builder_index);
        grammar::produce built = builder(*ctx.lex, ctx.reduce_inputs);

        value result{};
        result.is_ast = built.is_ast();
        result.has_token = false;
        if (result.is_ast)
        {
            result.ast = built.read_ast();
        }
        else
        {
            const ::wo::token& built_token = built.read_token();
            result.tok_type = built_token.type;
            result.tok_identifier = built_token.identifier;
        }

        if (rhs.size() != 0)
        {
            result.begin = rhs.begin()->begin;
            for (const value& item : rhs)
            {
                if (item.has_token)
                {
                    result.has_token = true;
                    result.last_token_begin = item.last_token_begin;
                }
            }
        }
        else if (ctx.lookahead_valid)
        {
            result.begin = source_info{
                ctx.lookahead.m_token_begin[0],
                ctx.lookahead.m_token_begin[1] };
        }

        if (!result.has_token)
            result.last_token_begin = result.begin;

        result.end = result.last_token_begin;

        if (result.is_ast)
        {
            ast::AstBase* node = result.ast;
            if (node->source_location.source_file == nullptr)
            {
                node->source_location.begin_at = ast::AstBase::location_t{
                    result.begin.row_no,
                    result.begin.col_no };
                node->source_location.end_at = ast::AstBase::location_t{
                    result.end.row_no,
                    result.end.col_no };
                node->source_location.source_file = ctx.lex->get_source_path();
                node->source_location.source_group = ctx.lex->get_source_group();
            }
        }
        else if (result.tok_type == lex_type::l_error)
        {
            ctx.has_error_node = true;
        }

        return result;
    }

    ast::AstBase* parse_program(lexer& lex, bool* out_is_incomplete)
    {
        context ctx;
        ctx.lex = &lex;
        ctx.out_is_incomplete = out_is_incomplete;

        if (yyparse(&ctx) != 0)
            return nullptr;

        if (!ctx.result_valid || lex.has_error())
            return nullptr;

        if (ctx.result.is_ast)
            return lex.merge_imported_ast_trees(ctx.result.ast);

        if (ctx.result.tok_type == lex_type::l_empty)
        {
            if (out_is_incomplete != nullptr)
                *out_is_incomplete = true;
            else
                (void)lex.record_parser_error(
                    lexer::msglevel_t::error,
                    diagnose::lexer::err_source_cannot_be_empty{});
        }

        return nullptr;
    }

}

    void init_woolang_grammar()
    {
        ::wo::ast::init_builder();
    }

    void shutdown_woolang_grammar()
    {
        ::wo::ast::shutdown_builder();
    }
}

int yylex(::wo::parser::value* yylval, ::wo::parser::context* ctx)
{
    using namespace wo;

    for (;;)
    {
        lexer::peeked_token_t* peeked = ctx->lex->peek(true);
        wo_assert(peeked != nullptr);

        if (peeked->m_lex_type == lex_type::l_error)
        {
            ctx->lex->move_forward(true);
            continue;
        }

        ctx->lookahead = *peeked;
        ctx->lookahead_valid = true;

        yylval->is_ast = false;
        yylval->has_token = true;
        yylval->ast = nullptr;
        yylval->tok_type = peeked->m_lex_type;
        yylval->tok_identifier = peeked->m_token_text;
        yylval->begin = parser::source_info{
            peeked->m_token_begin[0],
            peeked->m_token_begin[1] };
        yylval->end = parser::source_info{
            peeked->m_token_end[0],
            peeked->m_token_end[1] };
        yylval->last_token_begin = yylval->begin;

        if (peeked->m_lex_type == lex_type::l_eof)
        {
            ctx->at_eof = true;
            return 0;
        }

        int token_code = static_cast<int>(peeked->m_lex_type) + 300;
        ctx->lex->move_forward(true);
        return token_code;
    }
}

void yyerror(::wo::parser::context* ctx, const char* s)
{
    using namespace wo;

    (void)s;

    if (!ctx->lookahead_valid)
        return;

    const lexer::peeked_token_t& tk = ctx->lookahead;
    const char* text = (tk.m_token_text != nullptr)
        ? tk.m_token_text->c_str()
        : "";

    if (tk.m_lex_type == lex_type::l_eof)
    {
        if (ctx->out_is_incomplete != nullptr)
        {
            *ctx->out_is_incomplete = true;
            return;
        }

        ctx->lex->record_diagnose(
            lexer::msglevel_t::error,
            tk.m_token_begin[0],
            tk.m_token_begin[1],
            tk.m_token_end[0],
            tk.m_token_end[1],
            ctx->lex->get_source_path(),
            diagnose::lexer::err_unexpected_eof{});
        return;
    }

    if (tk.m_lex_type == lex_type::l_macro)
    {
        ctx->lex->record_diagnose(
            lexer::msglevel_t::error,
            tk.m_token_begin[0],
            tk.m_token_begin[1],
            tk.m_token_end[0],
            tk.m_token_end[1],
            ctx->lex->get_source_path(),
            diagnose::parser::err_unknown_macro_named{ text });
        return;
    }

    ctx->lex->record_diagnose(
        lexer::msglevel_t::error,
        tk.m_token_begin[0],
        tk.m_token_begin[1],
        tk.m_token_end[0],
        tk.m_token_end[1],
        ctx->lex->get_source_path(),
        diagnose::lexer::err_unexpected_token{ text });
}

#endif
